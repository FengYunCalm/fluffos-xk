#!/usr/bin/env python3
"""Run one isolated FluffOS test/tool workload with an auditable contract.

The runner never invokes a shell for the workload.  Every workload gets a
fresh copy of testsuite and src/www, a marked FLUFFOS_TEST_MUDLIB root, and a
separate log.  A failed run keeps its evidence directory; successful runs keep
logs and metadata as well so a zero exit code is not the only record.
"""

from __future__ import annotations

import argparse
import dataclasses
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from typing import Any, Iterable, Sequence


REPO_ROOT = Path(__file__).resolve().parents[2]
TESTSUITE_SOURCE = REPO_ROOT / "testsuite"
WWW_SOURCE = REPO_ROOT / "src" / "www"
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


@dataclasses.dataclass
class RunSummary:
    mode: str
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    selected: int = 0
    result_contract: str = ""
    records: list[dict[str, Any]] = dataclasses.field(default_factory=list)


class EvidenceSession:
    def __init__(self, requested_root: str | None) -> None:
        if requested_root:
            self.root = Path(requested_root).expanduser().resolve()
            self.root.mkdir(parents=True, exist_ok=False)
        else:
            self.root = Path(tempfile.mkdtemp(prefix="fluffos-targeted-"))
        self.counter = 0
        self.process_counter = 0
        self.records: list[dict[str, Any]] = []
        self.git_head = self._git_head()

    @staticmethod
    def _git_head() -> str:
        try:
            result = subprocess.run(
                ["git", "-c", "core.fsmonitor=false", "rev-parse", "HEAD"],
                cwd=REPO_ROOT,
                check=False,
                capture_output=True,
                text=True,
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
            "timed_out": result.timed_out,
            "duration_seconds": result.duration_seconds,
            "log": str(result.log_path),
            "source_git_head": self.git_head,
            "source_mudlib": str(source_root),
        }
        if binary is not None:
            record["binary"] = str(binary)
            record["binary_sha256"] = sha256_file(binary)
        self.records.append(record)
        (self.root / "records.json").write_text(
            json.dumps(self.records, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


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
            stat = path.lstat()
            if path.is_symlink() or not path.is_file():
                raise RunnerError(f"source tree contains a non-regular file: {path}")
            relative = path.relative_to(root).as_posix()
            result[relative] = ("file", stat.st_mode & 0o777, sha256_file(path))
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


def render_sandbox(session: EvidenceSession, config_name: str = "config.test") -> tuple[Path, Path]:
    root, mudlib = session.sandbox()
    copy_checked_tree(TESTSUITE_SOURCE, mudlib)
    copy_checked_tree(WWW_SOURCE, root / "src" / "www")
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
    environment = os.environ.copy()
    environment["FLUFFOS_TEST_MUDLIB"] = str(mudlib)
    return environment


def terminate_process(process: subprocess.Popen[str]) -> None:
    if os.name == "nt":
        # The process was placed in a fresh process group.  taskkill /T is the
        # standard Windows equivalent of killing that owned process tree.
        subprocess.run(
            ["taskkill", "/T", "/F", "/PID", str(process.pid)],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass


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
    command_list = [str(item) for item in command]
    session.process_counter += 1
    log_path = session.root / f"{session.process_counter:03d}-{label}.log"
    start = time.monotonic()
    timed_out = False
    try:
        process = subprocess.Popen(
            command_list,
            cwd=str(cwd),
            env=environment,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
            start_new_session=(os.name != "nt"),
            creationflags=(subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0),
        )
    except OSError as error:
        output = f"failed to start command: {error}\n"
        log_path.write_text(output, encoding="utf-8")
        result = ProcessResult(command_list, 127, False, time.monotonic() - start, output, log_path)
        session.record_process(result, source_root=cwd, binary=binary)
        return result

    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as error:
        timed_out = True
        terminate_process(process)
        output, _ = process.communicate()
        partial = error.output or ""
        if isinstance(partial, bytes):
            partial = partial.decode(errors="replace")
        output = partial + output
    log_path.write_text(output, encoding="utf-8")
    result = ProcessResult(
        command_list,
        process.returncode if process.returncode is not None else 124,
        timed_out,
        time.monotonic() - start,
        output,
        log_path,
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


def gtest_list_count(output: str) -> int:
    count = 0
    for line in output.splitlines():
        if re.match(r"^\s{2}(?!#)\S", line):
            count += 1
    return count


def gtest_result_counts(output: str) -> tuple[int, int, int, int]:
    def number(label: str) -> int:
        match = re.search(rf"\[\s*{label}\s*\]\s+(\d+)\s+tests?\.", output)
        return int(match.group(1)) if match else 0

    passed = number("PASSED")
    failed = number("FAILED")
    skipped = number("SKIPPED")
    run_count = len(re.findall(r"^\[ RUN      \]", output, flags=re.MULTILINE))
    return run_count, passed, failed, skipped


def reject_sanitizer_output(output: str) -> None:
    for signature in SANITIZER_SIGNATURES:
        if signature in output:
            raise RunnerError(f"sanitizer diagnostic in workload output: {signature}")


def run_binary(session: EvidenceSession, binary: Path, gtest_filter: str, timeout: int) -> RunSummary:
    root, mudlib = render_sandbox(session)
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
    selected = gtest_list_count(listing.output)
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
    if failed != 0 or skipped == run_count:
        raise RunnerError(f"GTest result contract failed; inspect {result.log_path}")
    return RunSummary(
        mode="binary",
        selected=selected,
        passed=passed,
        failed=failed,
        skipped=skipped,
        result_contract="gtest discovery non-empty; execution non-empty; no all-skipped result",
    )


def ctest_tests(ctest_dir: Path) -> list[str]:
    process = subprocess.run(
        ["ctest", "--test-dir", str(ctest_dir), "--show-only=json-v1"],
        cwd=REPO_ROOT,
        check=False,
        capture_output=True,
        text=True,
        errors="replace",
    )
    if process.returncode != 0:
        raise RunnerError(f"CTest JSON discovery failed: {process.stderr.strip()}")
    try:
        data = json.loads(process.stdout)
    except json.JSONDecodeError as error:
        raise RunnerError(f"CTest did not return json-v1 discovery output: {error}") from error
    tests = data.get("tests")
    if not isinstance(tests, list):
        raise RunnerError("CTest JSON discovery did not contain a tests array")
    names = [item.get("name") for item in tests if isinstance(item, dict)]
    if not names or any(not isinstance(name, str) or not name for name in names):
        raise RunnerError("CTest discovery returned zero or malformed test names")
    return names


def run_ctest(session: EvidenceSession, ctest_dir: Path, regex: str, timeout: int) -> RunSummary:
    try:
        matcher = re.compile(regex)
    except re.error as error:
        raise RunnerError(f"invalid CTest regex: {error}") from error
    names = [name for name in ctest_tests(ctest_dir) if matcher.search(name)]
    if not names:
        raise RunnerError(f"CTest regex selected zero tests: {regex!r}")

    summary = RunSummary(
        mode="ctest",
        selected=len(names),
        result_contract="json-v1 discovery non-empty; each selected CTest case run alone with parallel=1",
    )
    for index, name in enumerate(names, start=1):
        _root, mudlib = render_sandbox(session)
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
        if "Not Run" in result.output or "not run" in result.output.lower():
            summary.skipped += 1
        elif result.timed_out or result.returncode != 0:
            summary.failed += 1
            raise RunnerError(f"CTest case failed: {name}; inspect {result.log_path}")
        else:
            summary.passed += 1
    if summary.skipped == summary.selected:
        raise RunnerError("all selected CTest cases were skipped")
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
    if "Checks succeeded." not in result.output:
        raise RunnerError(f"LPC success marker missing; inspect {result.log_path}")
    if re.search(r"\bchecks?\s+failed\b", result.output, flags=re.IGNORECASE):
        raise RunnerError(f"LPC assertion failure marker found; inspect {result.log_path}")
    if "test skipped" in result.output.lower():
        raise RunnerError(f"LPC output reports a skipped test; inspect {result.log_path}")
    if case is not None and ("C> " + case) not in result.output:
        raise RunnerError(f"LPC case completion marker missing for {case}; inspect {result.log_path}")
    passed = len(re.findall(r"C> ", result.output))
    if passed == 0:
        passed = 1 if all_lpc else 0
    return RunSummary(
        mode="driver",
        selected=passed,
        passed=passed,
        result_contract=(
            "driver exit=0, Checks succeeded., no assertion-failure marker, "
            "no skip or sanitizer signature"
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
            "mode": summary.mode,
            "selected": summary.selected,
            "passed": summary.passed,
            "failed": summary.failed,
            "skipped": summary.skipped,
            "timeout_seconds": timeout,
            "result_contract": summary.result_contract,
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
    except RunnerError as error:
        evidence = locals().get("session")
        if isinstance(evidence, EvidenceSession):
            print(f"evidence_directory: {evidence.root}", file=sys.stderr)
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    except Exception as error:  # preserve unexpected failures as evidence-bearing failures
        evidence = locals().get("session")
        if isinstance(evidence, EvidenceSession):
            print(f"evidence_directory: {evidence.root}", file=sys.stderr)
        print(f"UNEXPECTED ERROR: {type(error).__name__}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
