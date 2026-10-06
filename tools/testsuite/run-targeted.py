#!/usr/bin/env python3
"""Run one isolated FluffOS test/tool workload with an auditable contract.

The runner never invokes a shell for the workload.  Every workload gets a
fresh testsuite copy (plus compiled native sources for C++ tests), a marked root, and a
separate log.  A failed run keeps its evidence directory; successful runs keep
logs and metadata as well so a zero exit code is not the only record.
"""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import ctypes
import dataclasses
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time
from typing import Any, Iterable, Iterator, Sequence


REPO_ROOT = Path(__file__).resolve().parents[2]
TESTSUITE_SOURCE = REPO_ROOT / "testsuite"
WWW_SOURCE = REPO_ROOT / "src" / "www"
MAX_LOG_BYTES = 64 * 1024 * 1024
TERMINATION_GRACE_SECONDS = 2
PIPE_DRAIN_SECONDS = 1
PR_SET_CHILD_SUBREAPER = 36
PR_GET_CHILD_SUBREAPER = 37
RUNTIME_ENV_KEYS = (
    "PATH", "LANG", "LC_ALL", "LC_CTYPE", "TZ", "ASAN_OPTIONS", "UBSAN_OPTIONS",
    "TSAN_OPTIONS", "LSAN_OPTIONS", "FLUFFOS_TEST_MUDLIB", "HOME", "TMPDIR", "TMP",
    "TEMP", "GTEST_COLOR", "PYTHONDONTWRITEBYTECODE",
)
IGNORE_PATTERNS = (".run-isolated-*", ".run-isolated.lock", ".run-isolated.lockdir", "log", "trace*.json")
SANITIZER_SIGNATURES = (
    "AddressSanitizer",
    "UndefinedBehaviorSanitizer",
    "ThreadSanitizer",
    "runtime error:",
)


class RunnerError(RuntimeError):
    """A usage, isolation, process, or result-contract failure."""


@dataclasses.dataclass
class ProcessResult:
    command: list[str]
    returncode: int
    timed_out: bool
    duration_seconds: float
    output: str
    log_path: Path
    failure_reason: str = ""
    binary_sha256_before: str = ""
    binary_sha256_after: str = ""
    inputs_before: str = ""
    inputs_after: str = ""
    launch_inputs: str = ""
    mudlib: str = ""
    process_returncode: int | None = None


@dataclasses.dataclass
class RunSummary:
    mode: str
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    selected: int = 0
    result_contract: str = ""
    excluded: list[str] = dataclasses.field(default_factory=list)
    exclusion_details: dict[str, dict[str, Any]] = dataclasses.field(default_factory=dict)
    capabilities: dict[str, int] = dataclasses.field(default_factory=dict)
    records: list[dict[str, Any]] = dataclasses.field(default_factory=list)


class EvidenceSession:
    def __init__(self, requested_root: str | None) -> None:
        if requested_root:
            self.root = Path(requested_root).expanduser().resolve()
            self.root.mkdir(mode=0o700, parents=True, exist_ok=False)
        else:
            self.root = Path(tempfile.mkdtemp(prefix="fluffos-targeted-"))
        self.counter = 0
        self.process_counter = 0
        self.records: list[dict[str, Any]] = []
        self.git_head = self._git_head()

    def capture_inputs(self, binary: Path | None, environment: dict[str, str]) -> str:
        data = {
            "harness_source": repository_manifest(REPO_ROOT),
            "runtime_libraries": runtime_libraries(binary, environment),
            "runner_python": file_identity(Path(sys.executable).resolve()),
        }
        roots = find_build_roots(binary.parent) if binary is not None else None
        if roots is not None:
            directory, source = roots
            build_files = {"CMakeCache.txt": file_identity(directory / "CMakeCache.txt")}
            commands = directory / "compile_commands.json"
            if commands.is_file():
                build_files["compile_commands.json"] = file_identity(commands)
            for name in ("build.ninja", "CMakeFiles/rules.ninja"):
                if (directory / name).is_file():
                    build_files[name] = file_identity(directory / name)
            for path in sorted((directory / "src").rglob("*")):
                if path.is_file() and (path.suffix in (".h", ".hpp", ".c", ".cc", ".spec", ".fullspec")
                                       or path.name in ("link.txt", "flags.make")):
                    build_files[path.relative_to(directory).as_posix()] = file_identity(path)
            data["build_root"] = str(directory)
            data["build_inputs"] = build_files
            data["compiled_source"] = repository_manifest(source)
        return self.store_inputs(data)

    def capture_launch(self, environment: dict[str, str]) -> str:
        name = environment.get("FLUFFOS_TEST_MUDLIB")
        if not name:
            return ""
        mudlib = Path(name).resolve()
        if not mudlib.is_relative_to(self.root):
            raise RunnerError("materialized mudlib is outside this evidence session")
        data = {"materialized_mudlib": source_manifest(mudlib),
                "environment": {key: value for key, value in environment.items()
                                if key in RUNTIME_ENV_KEYS}}
        native_source = mudlib.parent / "src"
        if native_source.is_dir():
            data["materialized_native_source"] = source_manifest(native_source)
        return self.store_inputs(data)

    def store_inputs(self, data: dict[str, Any]) -> str:
        encoded = (json.dumps(data, indent=2, sort_keys=True) + "\n").encode("utf-8")
        digest = hashlib.sha256(encoded).hexdigest()
        path = self.root / f"inputs-{digest}.json"
        if not path.exists():
            with path.open("xb") as output:
                output.write(encoded)
        elif sha256_file(path) != digest:
            raise RunnerError("existing input identity record changed")
        return digest

    @staticmethod
    def _git_head() -> str:
        try:
            result = subprocess.run(
                ["git", "-c", "core.fsmonitor=false", "rev-parse", "HEAD"],
                cwd=REPO_ROOT,
                check=False,
                capture_output=True,
                text=True,
                timeout=10,
                env={"PATH": os.environ.get("PATH", os.defpath), "GIT_OPTIONAL_LOCKS": "0",
                     "GIT_NO_LAZY_FETCH": "1"},
            )
        except OSError:
            return "unavailable"
        return result.stdout.strip() if result.returncode == 0 else "unavailable"

    def sandbox(self) -> tuple[Path, Path]:
        self.counter += 1
        root = self.root / f"sandbox-{self.counter:03d}"
        mudlib = root / "testsuite"
        return root, mudlib

    def record_process(self, result: ProcessResult, *, source_root: Path, binary: Path | None) -> None:
        record: dict[str, Any] = {
            "command": result.command,
            "cwd": str(source_root),
            "returncode": result.returncode,
            "process_returncode": result.process_returncode,
            "timed_out": result.timed_out,
            "failure_reason": result.failure_reason,
            "duration_seconds": result.duration_seconds,
            "log": str(result.log_path),
            "source_git_head": self.git_head,
            "source_mudlib": result.mudlib,
            "inputs_before": result.inputs_before,
            "inputs_after": result.inputs_after,
            "launch_inputs": result.launch_inputs,
        }
        if binary is not None:
            record["binary"] = str(binary)
            record["binary_sha256_before"] = result.binary_sha256_before
            record["binary_sha256"] = result.binary_sha256_after
        self.records.append(record)
        (self.root / "records.json").write_text(
            json.dumps(self.records, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )


def find_build_roots(start: Path) -> tuple[Path, Path] | None:
    for directory in (start, *start.parents):
        cache = directory / "CMakeCache.txt"
        if not cache.is_file():
            continue
        match = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$",
                          cache.read_text(), re.MULTILINE)
        if not match:
            raise RunnerError("build cache has no source directory identity")
        return directory, Path(match.group(1))
    return None


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def file_identity(path: Path) -> tuple[str, int, str]:
    metadata = path.lstat()
    mode = stat.S_IMODE(metadata.st_mode)
    if stat.S_ISLNK(metadata.st_mode):
        return "link", mode, os.readlink(path)
    if not stat.S_ISREG(metadata.st_mode):
        raise RunnerError(f"cannot fingerprint non-regular input: {path}")
    return "file", mode, sha256_file(path)


def runtime_libraries(binary: Path | None, environment: dict[str, str]) -> dict[str, Any]:
    if binary is None:
        return {"kind": "no-native-binary"}
    with binary.open("rb") as stream:
        if stream.read(4) != b"\x7fELF":
            return {"kind": "non-ELF; interpreter closure not measured"}
    probe = subprocess.run(
        ["ldd", str(binary)], env={**environment, "LC_ALL": "C"},
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10,
    )
    output = probe.stdout.decode("utf-8", errors="strict")
    if "not found" in output:
        raise RunnerError("unresolved runtime library")
    if "statically linked" in output or "not a dynamic executable" in output:
        return {"kind": "static-ELF"}
    if probe.returncode != 0:
        raise RunnerError("runtime library discovery failed")
    libraries = {}
    for line in output.splitlines():
        match = re.search(r"(?:=>\s*)?(/.+?)\s+\(0x[0-9a-f]+\)", line)
        if match:
            path = Path(match.group(1)).resolve()
            libraries[str(path)] = file_identity(path)
    if not libraries:
        raise RunnerError("dynamic ELF has no resolved library identity")
    return {"kind": "ELF-linker-closure", "libraries": libraries,
            "version_identity": "resolved file SHA-256; lazy dlopen is not attested"}


def repository_manifest(root: Path) -> dict[str, tuple[str, int, str]]:
    process = subprocess.run(
        ["git", "-c", "core.fsmonitor=false", "ls-files", "-z",
         "--cached", "--others", "--exclude-standard"],
        cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10,
        env={"PATH": os.environ.get("PATH", os.defpath), "GIT_OPTIONAL_LOCKS": "0",
             "GIT_NO_LAZY_FETCH": "1"},
    )
    if process.returncode != 0:
        raise RunnerError(f"cannot discover source identity: {root}")
    names = process.stdout.decode("utf-8", errors="surrogateescape").split("\0")[:-1]
    return {name: file_identity(root / name) for name in sorted(set(names))}


def ignored(name: str) -> bool:
    return any(fnmatch.fnmatch(name, pattern) for pattern in IGNORE_PATTERNS)


def source_manifest(root: Path) -> dict[str, tuple[str, int, str]]:
    """Hash a regular-file tree and reject links/special files."""
    if not root.is_dir():
        raise RunnerError(f"source tree is not a directory: {root}")
    result: dict[str, tuple[str, int, str]] = {}
    for current, dirs, files in os.walk(root, topdown=True, followlinks=False):
        current_path = Path(current)
        kept_dirs: list[str] = []
        for name in sorted(dirs):
            if ignored(name):
                continue
            path = current_path / name
            if path.is_symlink():
                raise RunnerError(f"source tree contains a symlink: {path}")
            if not path.is_dir():
                raise RunnerError(f"source tree contains a non-directory: {path}")
            kept_dirs.append(name)
        dirs[:] = kept_dirs
        for name in sorted(files):
            if ignored(name):
                continue
            path = current_path / name
            if path.is_symlink() or not path.is_file():
                raise RunnerError(f"source tree contains a non-regular file: {path}")
            relative = path.relative_to(root).as_posix()
            result[relative] = file_identity(path)
    return result


def copy_checked_tree(source: Path, destination: Path) -> None:
    before = source_manifest(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(
        source,
        destination,
        copy_function=shutil.copy2,
        ignore=lambda _directory, names: [name for name in names if ignored(name)],
    )
    after = source_manifest(source)
    if before != after:
        raise RunnerError(f"source tree changed while creating sandbox: {source}")


def render_sandbox(session: EvidenceSession, config_name: str = "config.test",
                   native_source: Path | None = None) -> tuple[Path, Path]:
    root, mudlib = session.sandbox()
    copy_checked_tree(TESTSUITE_SOURCE, mudlib)
    if native_source is None:
        copy_checked_tree(WWW_SOURCE, root / "src" / "www")
    else:
        copy_checked_tree(native_source, root / "src")
    (mudlib / ".fluffos-test-sandbox").write_text("version=1\n", encoding="ascii")
    (mudlib / "log").mkdir(exist_ok=True)

    if Path(config_name).name != config_name or not config_name.startswith("config."):
        raise RunnerError(f"invalid testsuite config name: {config_name!r}")
    config_path = mudlib / "etc" / config_name
    try:
        config = config_path.read_text(encoding="utf-8")
    except OSError as error:
        raise RunnerError(f"cannot read copied config: {config_path}: {error}") from error

    replacements = (
        (r"^mud ip : .*$", "mud ip : 127.0.0.1"),
        (r"^port number : .*$", "port number : 0"),
        (r"^external_port_2: websocket .*$", "external_port_2: websocket 0"),
        (r"^external_port_3: websocket .*$", "external_port_3: websocket 0"),
        (r"^external_port_4: telnet .*$", "external_port_4: telnet 0"),
        (r"^log directory : .*$", "log directory : /log"),
    )
    for pattern, replacement in replacements:
        config, count = re.subn(pattern, replacement, config, flags=re.MULTILINE)
        if count != 1:
            raise RunnerError(f"copied config replacement expected once: {pattern!r}, got {count}")
    (mudlib / "etc" / "config.test").write_text(config, encoding="utf-8")
    return root, mudlib


def command_environment(mudlib: Path) -> dict[str, str]:
    environment = {key: os.environ[key] for key in RUNTIME_ENV_KEYS if key in os.environ}
    home = mudlib / ".runner-home"
    temporary = mudlib / ".runner-tmp"
    home.mkdir(exist_ok=True)
    temporary.mkdir(exist_ok=True)
    environment.update(
        FLUFFOS_TEST_MUDLIB=str(mudlib), HOME=str(home), TMPDIR=str(temporary),
        TMP=str(temporary), TEMP=str(temporary), GTEST_COLOR="no",
        PYTHONDONTWRITEBYTECODE="1",
    )
    return environment


def direct_children() -> list[int]:
    path = Path(f"/proc/self/task/{os.getpid()}/children")
    return [int(pid) for pid in path.read_text().split()]


@contextmanager
def child_subreaper() -> Iterator[None]:
    # Exclusive ownership makes every newly adopted orphan part of this workload.
    if len(list(Path("/proc/self/task").iterdir())) != 1 or direct_children():
        raise RunnerError("process supervision requires a single-threaded, child-free runner")
    if signal.getsignal(signal.SIGCHLD) != signal.SIG_DFL:
        raise RunnerError("process supervision requires default SIGCHLD handling")
    library = ctypes.CDLL(None, use_errno=True)
    library.prctl.argtypes = [ctypes.c_int]
    library.prctl.restype = ctypes.c_int
    previous = ctypes.c_int()
    if library.prctl(PR_GET_CHILD_SUBREAPER, ctypes.byref(previous),
                     ctypes.c_ulong(0), ctypes.c_ulong(0), ctypes.c_ulong(0)) != 0:
        raise RunnerError(f"cannot inspect child-subreaper state: errno={ctypes.get_errno()}")
    if library.prctl(PR_SET_CHILD_SUBREAPER, ctypes.c_ulong(1),
                     ctypes.c_ulong(0), ctypes.c_ulong(0), ctypes.c_ulong(0)) != 0:
        raise RunnerError(f"cannot enable child subreaping: errno={ctypes.get_errno()}")
    try:
        yield
    finally:
        if library.prctl(PR_SET_CHILD_SUBREAPER, ctypes.c_ulong(previous.value),
                         ctypes.c_ulong(0), ctypes.c_ulong(0), ctypes.c_ulong(0)) != 0:
            raise RunnerError(f"cannot restore child-subreaper state: errno={ctypes.get_errno()}")


def adopted_children(leader: int) -> list[int]:
    alive = []
    for pid in direct_children():
        if pid == leader:
            continue
        reaped, _status = os.waitpid(pid, os.WNOHANG)
        if not reaped:
            alive.append(pid)
    return alive


def terminate_process(process: subprocess.Popen[bytes], *, force: bool = False) -> None:
    # Keep the leader unreaped until cleanup ends to reserve its process-group ID.
    try:
        os.killpg(process.pid, signal.SIGKILL if force else signal.SIGTERM)
    except ProcessLookupError:
        pass
    for pid in adopted_children(process.pid):
        descriptor = os.pidfd_open(pid)
        try:
            signal.pidfd_send_signal(descriptor, signal.SIGKILL if force else signal.SIGTERM)
        except ProcessLookupError:
            pass
        finally:
            os.close(descriptor)


def run_process(
    session: EvidenceSession,
    command: Sequence[str],
    *,
    cwd: Path,
    environment: dict[str, str],
    timeout: int,
    label: str,
    binary: Path | None = None,
) -> ProcessResult:
    if (not sys.platform.startswith("linux") or not hasattr(os, "WNOWAIT")
            or not hasattr(os, "pidfd_open") or not hasattr(signal, "pidfd_send_signal")):
        raise RunnerError("bounded process-tree supervision requires Linux /proc, waitid and pidfd")
    environment = {key: value for key, value in environment.items() if key in RUNTIME_ENV_KEYS}
    command_list = [str(item) for item in command]
    binary_before = sha256_file(binary) if binary is not None else ""
    inputs_before = session.capture_inputs(binary, environment)
    launch_inputs = session.capture_launch(environment)
    session.process_counter += 1
    log_path = session.root / f"{session.process_counter:03d}-{label}.log"
    start = time.monotonic()
    deadline = start + timeout
    timed_out = False
    failure_reason = ""
    output = bytearray()
    stop_time = None
    process = None
    returncode = 127
    with child_subreaper(), log_path.open("xb") as log, selectors.DefaultSelector() as selector:
        try:
            process = subprocess.Popen(
                command_list,
                cwd=str(cwd),
                env=environment,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0,
                start_new_session=True,
            )
        except OSError as error:
            failure_reason = f"failed to start command: {error}"
            output.extend((failure_reason + "\n").encode("utf-8"))
            log.write(output)
        else:
            try:
                os.set_blocking(process.stdout.fileno(), False)
                selector.register(process.stdout, selectors.EVENT_READ)
                while True:
                    now = time.monotonic()
                    exited = os.waitid(
                        os.P_PID, process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT
                    ) is not None
                    if exited:
                        children = adopted_children(process.pid)
                        if not children and not selector.get_map():
                            break
                        if children and not failure_reason:
                            failure_reason = "leader exited with live descendants"
                    if now >= deadline and not failure_reason:
                        timed_out = True
                        failure_reason = f"process or inherited output timed out after {timeout}s"
                    if failure_reason and stop_time is None:
                        stop_time = now
                        terminate_process(process)
                    if stop_time is not None:
                        if now >= stop_time + TERMINATION_GRACE_SECONDS:
                            terminate_process(process, force=True)
                        if now >= stop_time + TERMINATION_GRACE_SECONDS + PIPE_DRAIN_SECONDS:
                            break
                    for key, _events in selector.select(0.05):
                        data = os.read(key.fd, 65536)
                        if not data:
                            selector.unregister(key.fileobj)
                            continue
                        remaining = MAX_LOG_BYTES - len(output)
                        retained = data[:remaining]
                        log.write(retained)
                        output.extend(retained)
                        if len(data) > remaining and not failure_reason:
                            failure_reason = f"output exceeded {MAX_LOG_BYTES} byte budget"
            except KeyboardInterrupt:
                failure_reason = "workload interrupted"
            finally:
                # This also handles an interrupted selector or failed log write.
                terminate_process(process, force=True)
                process.stdout.close()
                cleanup_deadline = time.monotonic() + PIPE_DRAIN_SECONDS
                while True:
                    exited = os.waitid(
                        os.P_PID, process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT
                    ) is not None
                    children = adopted_children(process.pid)
                    if exited and not children:
                        break
                    terminate_process(process, force=True)
                    if time.monotonic() >= cleanup_deadline:
                        failure_reason = failure_reason or "owned process tree did not exit"
                        break
                    time.sleep(0.01)
                try:
                    returncode = process.wait(timeout=PIPE_DRAIN_SECONDS)
                except subprocess.TimeoutExpired:
                    failure_reason = failure_reason or "owned leader did not exit"
                    returncode = 124
    duration = time.monotonic() - start
    binary_after = sha256_file(binary) if binary is not None and binary.is_file() else ""
    inputs_after = session.capture_inputs(binary, environment)
    if binary_after != binary_before:
        failure_reason = failure_reason or "binary identity changed during execution"
    if inputs_after != inputs_before:
        failure_reason = failure_reason or "source/build/runtime inputs changed during execution"
    result = ProcessResult(
        command_list,
        returncode if returncode != 0 or not failure_reason else 1,
        timed_out,
        duration,
        output.decode("utf-8", errors="replace"),
        log_path,
        failure_reason,
        binary_before,
        binary_after,
        inputs_before,
        inputs_after,
        launch_inputs,
        environment.get("FLUFFOS_TEST_MUDLIB", ""),
        process.returncode if process is not None else None,
    )
    session.record_process(result, source_root=cwd, binary=binary)
    return result


def require_executable(path_string: str) -> Path:
    path = Path(path_string).expanduser()
    if not path.is_absolute():
        path = (Path.cwd() / path).resolve()
    else:
        path = path.resolve()
    if not path.is_file() or not os.access(path, os.X_OK):
        raise RunnerError(f"executable is missing or not executable: {path}")
    return path


def gtest_names(output: str) -> tuple[list[str], list[str]]:
    selected = []
    excluded = []
    suite = ""
    seen = set()
    for line in output.splitlines():
        name = line.split("#", 1)[0].strip()
        if not name:
            continue
        if not line.startswith(" ") and name.endswith(".") and " " not in name:
            suite = name
        elif line.startswith("  ") and suite and " " not in name:
            full_name = suite + name
            if full_name in seen:
                raise RunnerError(f"duplicate GTest discovery identity: {full_name}")
            seen.add(full_name)
            disabled = any(part.startswith("DISABLED_") for part in re.split(r"[./]", full_name))
            (excluded if disabled else selected).append(full_name)
    return selected, excluded


def gtest_result_counts(output: str) -> tuple[int, int, int, int]:
    def number(label: str) -> int:
        matches = re.findall(rf"^\[\s*{label}\s*\]\s+(\d+)\s+tests?\.$", output, re.MULTILINE)
        if len(matches) > 1:
            raise RunnerError(f"duplicate GTest {label} summary")
        return int(matches[0]) if matches else 0

    return (
        len(re.findall(r"^\[ RUN      \]", output, re.MULTILINE)),
        number("PASSED"), number("FAILED"), number("SKIPPED"),
    )


def reject_sanitizer_output(output: str) -> None:
    for signature in SANITIZER_SIGNATURES:
        if signature in output:
            raise RunnerError(f"sanitizer diagnostic in workload output: {signature}")


def run_binary(session: EvidenceSession, binary: Path, gtest_filter: str, timeout: int) -> RunSummary:
    roots = find_build_roots(binary.parent)
    source = roots[1] if roots is not None else REPO_ROOT
    _root, mudlib = render_sandbox(session, native_source=source / "src")
    environment = command_environment(mudlib)
    listing = run_process(
        session,
        [str(binary), "--gtest_list_tests", f"--gtest_filter={gtest_filter}"],
        cwd=mudlib,
        environment=environment,
        timeout=timeout,
        label="gtest-list",
        binary=binary,
    )
    if listing.timed_out:
        raise RunnerError(f"GTest discovery timed out after {timeout}s; inspect {listing.log_path}")
    if listing.returncode != 0:
        raise RunnerError(f"GTest discovery failed; inspect {listing.log_path}")
    reject_sanitizer_output(listing.output)
    expected, excluded = gtest_names(listing.output)
    selected = len(expected)
    if selected == 0:
        raise RunnerError(f"GTest filter selected zero tests: {gtest_filter!r}")

    result = run_process(
        session,
        [str(binary), f"--gtest_filter={gtest_filter}"],
        cwd=mudlib,
        environment=environment,
        timeout=timeout,
        label="gtest-run",
        binary=binary,
    )
    reject_sanitizer_output(result.output)
    run_count, passed, failed, skipped = gtest_result_counts(result.output)
    if result.timed_out:
        raise RunnerError(f"GTest execution timed out after {timeout}s; inspect {result.log_path}")
    if result.returncode != 0 or run_count == 0 or passed + failed + skipped == 0:
        raise RunnerError(f"GTest execution failed; inspect {result.log_path}")
    started = re.findall(r"^\[ RUN      \] (\S+)", result.output, re.MULTILINE)
    completed = re.findall(r"^\[       OK \] (\S+)", result.output, re.MULTILINE)
    if (failed or skipped or passed != selected or run_count != selected
            or len(set(started)) != selected or len(completed) != selected
            or set(started) != set(expected) or set(completed) != set(expected)):
        raise RunnerError(f"GTest result contract failed; inspect {result.log_path}")
    return RunSummary(
        mode="binary",
        selected=selected,
        passed=passed,
        failed=failed,
        skipped=skipped,
        excluded=excluded,
        result_contract=(
            "required discovery = unique RUN = unique OK = PASSED; no skips or failures"
        ),
    )


def ctest_tests(session: EvidenceSession, ctest_dir: Path, timeout: int) -> list[str]:
    _root, mudlib = render_sandbox(session)
    process = run_process(
        session, ["ctest", "--test-dir", str(ctest_dir), "--show-only=json-v1"],
        cwd=mudlib, environment=command_environment(mudlib), timeout=timeout,
        label="ctest-list",
    )
    reject_sanitizer_output(process.output)
    if process.timed_out or process.returncode != 0:
        raise RunnerError(f"CTest JSON discovery failed; inspect {process.log_path}")
    try:
        data = json.loads(process.output)
    except json.JSONDecodeError as error:
        raise RunnerError(f"CTest did not return json-v1 discovery output: {error}") from error
    tests = data.get("tests")
    if not isinstance(tests, list):
        raise RunnerError("CTest JSON discovery did not contain a tests array")
    names = [item.get("name") for item in tests if isinstance(item, dict)]
    if (not names or len(names) != len(tests)
            or any(not isinstance(name, str) or not name for name in names)
            or len(set(names)) != len(names)):
        raise RunnerError("CTest discovery returned zero, duplicate or malformed test names")
    return names

def run_ctest(session: EvidenceSession, ctest_dir: Path, regex: str, timeout: int) -> RunSummary:
    try:
        matcher = re.compile(regex)
    except re.error as error:
        raise RunnerError(f"invalid CTest regex: {error}") from error
    names = [name for name in ctest_tests(session, ctest_dir, timeout) if matcher.search(name)]
    if not names:
        raise RunnerError(f"CTest regex selected zero tests: {regex!r}")

    summary = RunSummary(
        mode="ctest",
        selected=len(names),
        result_contract="CTest scheduling results only; each selected case completed with no skips",
    )
    roots = find_build_roots(ctest_dir)
    source = roots[1] if roots is not None else REPO_ROOT
    for index, name in enumerate(names, start=1):
        _root, mudlib = render_sandbox(session, native_source=source / "src")
        environment = command_environment(mudlib)
        exact = "^" + re.escape(name) + "$"
        result = run_process(
            session,
            [
                "ctest",
                "--test-dir",
                str(ctest_dir),
                "--output-on-failure",
                "--parallel",
                "1",
                "-R",
                exact,
            ],
            cwd=REPO_ROOT,
            environment=environment,
            timeout=timeout,
            label=f"ctest-{index:03d}",
        )
        reject_sanitizer_output(result.output)
        if "No tests were found" in result.output:
            raise RunnerError(f"CTest exact selection became empty: {name}")
        if result.timed_out or result.returncode != 0:
            summary.failed += 1
            raise RunnerError(f"CTest case failed: {name}; inspect {result.log_path}")
        if re.search(r"not run|skipped", result.output, re.IGNORECASE):
            raise RunnerError(f"CTest case was skipped: {name}; inspect {result.log_path}")
        if not re.search(rf"1/1 Test #\d+: {re.escape(name)}\s+.*\bPassed\b", result.output):
            raise RunnerError(f"CTest completion missing: {name}; inspect {result.log_path}")
        summary.passed += 1
    return summary


def validate_case(case: str) -> str:
    if not re.fullmatch(r"/[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*", case):
        raise RunnerError(f"invalid LPC case path: {case!r}")
    return case


def case_exists(mudlib: Path, case: str) -> None:
    relative = Path(case.lstrip("/"))
    if any(part in ("", ".", "..") for part in relative.parts):
        raise RunnerError(f"invalid LPC case path: {case!r}")
    if not any((mudlib / (Path(str(relative) + extension))).is_file()
               for extension in (".c", ".lpc")):
        raise RunnerError(f"LPC case is not present in isolated mudlib: {case}")


def lpc_test_cases(mudlib: Path) -> set[tuple[str, str]]:
    def visit(directory: Path, category: str = "C") -> set[tuple[str, str]]:
        cases = set()
        for path in sorted(directory.iterdir()):
            if path.is_dir() and category == "C":
                child_category = {"fail": "A", "crasher": "B"}.get(path.name, "C")
                cases.update(visit(path, child_category))
            elif path.is_file() and path.suffix in (".c", ".lpc"):
                if path.suffix == ".c" and path.with_suffix(".lpc").exists():
                    continue
                cases.add((category, "/" + path.relative_to(mudlib).as_posix()))
        return cases

    return visit(mudlib / "single" / "tests")


def lpc_case_key(name: str) -> str:
    for suffix in (".lpc", ".c"):
        if name.endswith(suffix):
            return name[:-len(suffix)]
    return name


def lpc_scopes(mudlib: Path) -> dict[str, tuple[str, list[str], str]]:
    scopes = {}
    for line in (mudlib / "etc" / "test-scopes.tsv").read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 4:
            raise RunnerError("invalid test-scopes.tsv row")
        name, role, features, reason = fields
        validate_case(name)
        case_exists(mudlib, name)
        if name in scopes or role not in {"runtime", "fixture", "coverage_gap"} or not reason:
            raise RunnerError("duplicate or invalid test-scopes.tsv classification")
        scopes[name] = (role, [] if features == "-" else features.split(","), reason)
    return scopes


def lpc_selection(
    discovered: set[tuple[str, str]],
    scopes: dict[str, tuple[str, list[str], str]],
    output: str,
) -> tuple[set[tuple[str, str]], dict[str, dict[str, Any]], dict[str, int]]:
    cap_matches = list(re.finditer(r"^T> cap ([a-z_]+)=([01])$", output, re.MULTILINE))
    capabilities = {match[1]: int(match[2]) for match in cap_matches}
    if len(capabilities) != len(cap_matches):
        raise RunnerError("duplicate LPC capability declaration")
    first_start = re.search(r"^[ABC]> ", output, re.MULTILINE)
    if first_start and any(match.end() > first_start.start() for match in cap_matches):
        raise RunnerError("LPC capability declaration arrived after selection")
    required, excluded = set(), {}
    for kind, name in sorted(discovered):
        role, features, reason = scopes.get(lpc_case_key(name), ("runtime", [], ""))
        if any(feature not in capabilities for feature in features):
            raise RunnerError(f"missing LPC capability declaration for {name}")
        missing = [feature for feature in features if not capabilities[feature]]
        classification = role if role != "runtime" else ("unavailable" if missing else "")
        if kind == "C" and classification:
            excluded[name] = {"classification": classification, "reason": reason,
                              "missing_requirements": missing}
        else:
            required.add((kind, name))
    exclusions = re.findall(r"^X> (\S+)\t([^\t]+)\t(.+)$", output, re.MULTILINE)
    actual = {name: (kind, reason) for name, kind, reason in exclusions}
    expected = {name: (data["classification"], data["reason"])
                for name, data in excluded.items()}
    if len(actual) != len(exclusions) or actual != expected:
        raise RunnerError("LPC exclusion records do not match the predeclared source scopes")
    return required, excluded, capabilities


def run_driver(
    session: EvidenceSession,
    driver: Path,
    case: str | None,
    all_lpc: bool,
    timeout: int,
    config_name: str,
) -> RunSummary:
    _root, mudlib = render_sandbox(session, config_name)
    if case is not None:
        case = validate_case(case)
        case_exists(mudlib, case)
        flag = "-ftest:" + case
    else:
        flag = "-ftest"
    discovered = {("C", case)} if case is not None else lpc_test_cases(mudlib)
    scopes = lpc_scopes(mudlib)
    result = run_process(
        session,
        [str(driver), "etc/config.test", flag],
        cwd=mudlib,
        environment=command_environment(mudlib),
        timeout=timeout,
        label="driver-lpc",
        binary=driver,
    )
    reject_sanitizer_output(result.output)
    if result.timed_out or result.returncode != 0:
        raise RunnerError(f"LPC driver failed; inspect {result.log_path}")
    # comm.cc prefixes noninteractive write() payloads with a single ] on stderr.
    lpc_output = re.sub(r"^\]", "", result.output, flags=re.MULTILINE)
    if len(re.findall(r"^Checks succeeded\.$", lpc_output, re.MULTILINE)) != 1:
        raise RunnerError(f"LPC success marker missing or repeated; inspect {result.log_path}")
    if re.search(r"\bchecks?\s+failed\b", result.output, flags=re.IGNORECASE):
        raise RunnerError(f"LPC assertion failure marker found; inspect {result.log_path}")
    if "test skipped" in result.output.lower():
        raise RunnerError(f"LPC output reports a skipped test; inspect {result.log_path}")
    starts = re.findall(r"^([ABC])> (\S+)$", lpc_output, re.MULTILINE)
    completions = re.findall(r"^D> ([ABC]) (\S+) asserts=(\d+)$", lpc_output, re.MULTILINE)
    if not completions:
        raise RunnerError(f"LPC case completion records missing; inspect {result.log_path}")
    expected, excluded, capabilities = lpc_selection(discovered, scopes, lpc_output)
    completed = [(kind, name) for kind, name, _count in completions]
    if (not expected or set(starts) != expected or set(completed) != expected
            or len(starts) != len(expected) or len(completed) != len(expected)):
        raise RunnerError(f"LPC discovery/start/completion mismatch; inspect {result.log_path}")
    if any(kind == "C" and int(count) == 0 for kind, _name, count in completions):
        raise RunnerError(f"LPC ordinary case has no assertion coverage; inspect {result.log_path}")
    if re.search(r"^T> fail\b", lpc_output, re.MULTILINE):
        raise RunnerError(f"LPC completion barrier failed; inspect {result.log_path}")
    passed = len(completed)
    return RunSummary(
        mode="driver",
        selected=passed,
        passed=passed,
        excluded=sorted(excluded),
        exclusion_details=excluded,
        capabilities=capabilities,
        result_contract=(
            "required cases = unique starts = unique completions; C cases have assertions; "
            "driver exit=0 with success marker and no assertion, skip or sanitizer diagnostic"
        ),
    )


def run_tool(session: EvidenceSession, tool: Path, arguments: Sequence[str], timeout: int) -> RunSummary:
    _root, mudlib = render_sandbox(session)
    result = run_process(
        session,
        [str(tool), *arguments],
        cwd=mudlib,
        environment=command_environment(mudlib),
        timeout=timeout,
        label="tool",
        binary=tool,
    )
    reject_sanitizer_output(result.output)
    if result.timed_out or result.returncode != 0:
        raise RunnerError(f"tool failed; inspect {result.log_path}")
    return RunSummary(
        mode="tool",
        selected=1,
        passed=1,
        result_contract="execution-only; no benchmark improvement or fuzz coverage claim",
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--binary", help="GTest executable")
    modes.add_argument("--ctest-dir", help="configured CTest build directory")
    modes.add_argument("--driver", help="driver executable")
    modes.add_argument("--tool", help="bench/fuzz executable")
    parser.add_argument("--gtest-filter", help="required with --binary")
    parser.add_argument("--ctest-regex", help="required with --ctest-dir")
    parser.add_argument("--case", help="LPC object path, required with --driver unless --all-lpc")
    parser.add_argument("--all-lpc", action="store_true", help="run the complete LPC suite with --driver")
    parser.add_argument(
        "--config",
        default="config.test",
        help="testsuite/etc config.<name> template copied to config.test (driver mode only)",
    )
    parser.add_argument("--timeout", type=int, default=None, help="positive timeout in seconds (default 180)")
    parser.add_argument("--evidence-dir", help="new directory for logs and machine-readable records")
    parser.add_argument("tool_args", nargs=argparse.REMAINDER, help="arguments after -- for --tool")
    return parser


def validate_arguments(parser: argparse.ArgumentParser, args: argparse.Namespace) -> int:
    if not args.driver and args.config != "config.test":
        parser.error("--config is only valid with --driver")
    if args.timeout is not None and args.timeout <= 0:
        parser.error("--timeout must be a positive number of seconds")
    if args.binary:
        if not args.gtest_filter:
            parser.error("--binary requires --gtest-filter")
        if args.ctest_regex or args.case or args.all_lpc or args.tool_args:
            parser.error("--binary cannot be combined with CTest, driver, or tool arguments")
    elif args.ctest_dir:
        if not args.ctest_regex:
            parser.error("--ctest-dir requires --ctest-regex")
        if args.gtest_filter or args.case or args.all_lpc or args.tool_args:
            parser.error("--ctest-dir cannot be combined with GTest, driver, or tool arguments")
    elif args.driver:
        if args.gtest_filter or args.ctest_regex or bool(args.tool_args):
            parser.error("--driver cannot be combined with GTest, CTest, or tool arguments")
        if Path(args.config).name != args.config or not args.config.startswith("config."):
            parser.error("--config must be a testsuite/etc config.<name> file name")
        if (args.case is None) == (not args.all_lpc):
            parser.error("--driver requires exactly one of --case or --all-lpc")
        if args.all_lpc and args.timeout is None:
            parser.error("--all-lpc requires an explicit --timeout budget")
    elif args.tool:
        if args.gtest_filter or args.ctest_regex or args.case or args.all_lpc:
            parser.error("--tool cannot be combined with GTest, CTest, or driver selectors")
        if args.tool_args and args.tool_args[0] == "--":
            args.tool_args = args.tool_args[1:]
    return args.timeout if args.timeout is not None else 180


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    timeout = validate_arguments(parser, args)
    session = None
    try:
        session = EvidenceSession(args.evidence_dir)
        if args.binary:
            summary = run_binary(session, require_executable(args.binary), args.gtest_filter, timeout)
        elif args.ctest_dir:
            ctest_dir = Path(args.ctest_dir).expanduser().resolve()
            if not ctest_dir.is_dir():
                raise RunnerError(f"CTest directory is missing: {ctest_dir}")
            summary = run_ctest(session, ctest_dir, args.ctest_regex, timeout)
        elif args.driver:
            summary = run_driver(
                session,
                require_executable(args.driver),
                args.case,
                args.all_lpc,
                timeout,
                args.config,
            )
        else:
            summary = run_tool(session, require_executable(args.tool), args.tool_args, timeout)
        summary.records = session.records
        report = {
            "status": "passed",
            "mode": summary.mode,
            "selected": summary.selected,
            "passed": summary.passed,
            "failed": summary.failed,
            "skipped": summary.skipped,
            "timeout_seconds": timeout,
            "result_contract": summary.result_contract,
            "excluded_before_execution": summary.excluded,
            "exclusion_details": summary.exclusion_details,
            "capabilities": summary.capabilities,
            "evidence_directory": str(session.root),
            "source_git_head": session.git_head,
            "records": session.records,
        }
        (session.root / "summary.json").write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        print(json.dumps(report, indent=2, sort_keys=True))
        print(f"evidence_directory: {session.root}")
        return 0
    except Exception as error:
        if session is not None:
            failure = {"status": "failed", "error_type": type(error).__name__,
                       "error": str(error), "records": session.records}
            try:
                (session.root / "summary.json").write_text(
                    json.dumps(failure, indent=2, sort_keys=True) + "\n", encoding="utf-8"
                )
            except OSError as write_error:
                print(f"cannot persist failure summary: {write_error}", file=sys.stderr)
            print(f"evidence_directory: {session.root}", file=sys.stderr)
        print(f"ERROR: {type(error).__name__}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
