#!/usr/bin/env python3
"""Read-only native C/C++ formatting checker for FluffOS_XK.

The checker never writes source files, the index, or a report. It compares the
formatter's replacement spans with the selected scope and prints every span
that the formatter proposes.
"""

from __future__ import annotations

import argparse
import difflib
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import Iterable, Sequence


FORMATTER_RE = re.compile(r"^#\s*Formatter:\s*clang-format\s+([0-9]+\.[0-9]+\.[0-9]+)\s*$", re.MULTILINE)
NATIVE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
EXCLUDED_PREFIXES = (
    "build",
    "build-",
    "build-modernization-evidence/",
    "docs/evidence/",
    "src/tests/fuzz/corpus/",
    "src/thirdparty/",
    "testsuite/",
)
GENERATED_SUFFIXES = (".autogen.cc", ".autogen.h", ".generated.cc", ".generated.h")


class CheckError(Exception):
    """A setup, scope, or formatter-input error."""


@dataclass(frozen=True)
class FileSnapshot:
    device: int
    inode: int
    size: int
    mtime_ns: int
    digest: str


@dataclass(frozen=True)
class LineRange:
    start: int
    end: int


@dataclass(frozen=True)
class Candidate:
    rel: str
    rel_bytes: bytes
    status: str
    base_rel_bytes: bytes | None = None


@dataclass(frozen=True)
class Replacement:
    offset: int
    length: int
    text: str


class Reporter:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.checked = 0
        self.excluded: dict[str, int] = {}
        self.not_applicable = 0

    def error(self, message: str) -> None:
        self.errors.append(message)
        print(f"error: {message}")

    def excluded_path(self, reason: str, rel: str) -> None:
        self.excluded[reason] = self.excluded.get(reason, 0) + 1
        print(f"excluded: {rel} ({reason})")

    def summary(self, scope: str) -> None:
        print(f"scope: {scope}")
        print(f"checked: {self.checked}")
        if self.excluded:
            details = ", ".join(
                f"{reason}={count}" for reason, count in sorted(self.excluded.items())
            )
            print(f"excluded-counts: {details}")
        if self.checked == 0 and not self.errors:
            self.not_applicable += 1
            print("result: not-applicable")


def _git_env() -> dict[str, str]:
    env = os.environ.copy()
    env["GIT_NO_LAZY_FETCH"] = "1"
    env["GIT_OPTIONAL_LOCKS"] = "0"
    env["GIT_TERMINAL_PROMPT"] = "0"
    return env


def _run_git(
    repo: Path,
    args: Sequence[str],
    input_bytes: bytes | None = None,
) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        ["git", *args],
        cwd=repo,
        env=_git_env(),
        input=input_bytes,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def _repo_root() -> Path:
    script_path = Path(__file__).absolute()
    candidate = script_path.parent.parent.parent
    result = _run_git(candidate, ["rev-parse", "--show-toplevel"])
    if result.returncode != 0:
        detail = result.stderr.decode(errors="replace").strip()
        raise CheckError(f"cannot locate repository from {candidate}: {detail}")
    return Path(os.fsdecode(result.stdout.rstrip(b"\n"))).resolve()


def _format_version(config: bytes) -> str:
    try:
        text = config.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise CheckError(f"{_config_path()} is not UTF-8: {exc}") from exc
    matches = FORMATTER_RE.findall(text)
    if len(matches) != 1:
        raise CheckError(
            f"{_config_path()} must contain exactly one '# Formatter: clang-format X.Y.Z' declaration"
        )
    return matches[0]


_REPO_ROOT: Path | None = None


def _config_path() -> Path:
    if _REPO_ROOT is None:
        raise CheckError("repository root is not initialized")
    return _REPO_ROOT / "src" / ".clang-format"


def _snapshot(path: Path) -> FileSnapshot:
    try:
        stat = path.stat()
        data = path.read_bytes()
    except OSError as exc:
        raise CheckError(f"cannot read {path}: {exc}") from exc
    return FileSnapshot(
        device=stat.st_dev,
        inode=stat.st_ino,
        size=stat.st_size,
        mtime_ns=stat.st_mtime_ns,
        digest=hashlib.sha256(data).hexdigest(),
    )


def _assert_unchanged(path: Path, before: FileSnapshot) -> None:
    after = _snapshot(path)
    if after != before:
        raise CheckError(f"file changed while checking: {path}")


def _relative_text(path_bytes: bytes) -> str:
    return os.fsdecode(path_bytes).replace(os.sep, "/")


def _path_from_rel(rel_bytes: bytes) -> Path:
    if _REPO_ROOT is None:
        raise CheckError("repository root is not initialized")
    rel = os.fsdecode(rel_bytes)
    path = _REPO_ROOT.joinpath(*rel.split("/"))
    return path


def _path_has_symlink_component(path: Path) -> bool:
    if _REPO_ROOT is None:
        return False
    try:
        relative = path.absolute().relative_to(_REPO_ROOT)
    except ValueError:
        return False
    current = _REPO_ROOT
    for part in relative.parts:
        current /= part
        if current.is_symlink():
            return True
    return False


def _explicit_path(raw: str) -> tuple[str, bytes, Path]:
    if _REPO_ROOT is None:
        raise CheckError("repository root is not initialized")
    supplied = Path(raw)
    candidate = supplied if supplied.is_absolute() else _REPO_ROOT / supplied
    candidate = Path(os.path.abspath(os.fspath(candidate)))
    if _path_has_symlink_component(candidate):
        raise CheckError(f"symbolic links are not checkable: {raw}")
    try:
        resolved = candidate.resolve(strict=False)
        rel = resolved.relative_to(_REPO_ROOT)
    except ValueError as exc:
        raise CheckError(f"path is outside the repository: {raw}") from exc
    rel_text = rel.as_posix()
    rel_bytes = os.fsencode(rel_text)
    return rel_text, rel_bytes, resolved


def _scope_reason(rel: str) -> str | None:
    if rel.startswith("src/thirdparty/"):
        return "third-party"
    if rel.startswith("testsuite/") or rel.endswith(".lpc"):
        return "LPC-or-testsuite"
    if rel.startswith("docs/evidence/"):
        return "raw-evidence"
    if rel.startswith("src/tests/fuzz/corpus/"):
        return "byte-fixture"
    if rel.startswith("build") or rel.startswith("build-modernization-evidence/"):
        return "build-output"
    if any(rel.endswith(suffix) for suffix in GENERATED_SUFFIXES):
        return "generated-output"
    if rel.endswith(".generated") or rel.endswith(".autogen"):
        return "generated-output"
    return None


def _classify(rel: str) -> tuple[bool, str | None]:
    reason = _scope_reason(rel)
    if reason is not None:
        return False, reason
    suffix = Path(rel).suffix.lower()
    if suffix not in NATIVE_EXTENSIONS:
        return False, "non-native-language"
    if not (rel.startswith("src/") or rel.startswith("compat/")):
        return False, "outside-native-root"
    return True, None


def _validate_explicit(rel: str, path: Path) -> None:
    applicable, reason = _classify(rel)
    if not applicable:
        raise CheckError(f"path is not an eligible hand-written native file: {rel} ({reason})")
    if not path.exists():
        raise CheckError(f"path does not exist: {rel}")
    if not path.is_file():
        raise CheckError(f"path is not a regular file: {rel}")
    if _path_has_symlink_component(path):
        raise CheckError(f"symbolic links are not checkable: {rel}")


def _find_formatter(expected: str, repo: Path) -> Path:
    value = os.environ.get("CLANG_FORMAT", "clang-format")
    candidate = Path(value)
    if not candidate.is_absolute():
        repo_candidate = repo / candidate
        if repo_candidate.exists():
            candidate = repo_candidate
        else:
            found = shutil.which(value)
            if found is None:
                raise CheckError(f"clang-format is unavailable (requested {expected})")
            candidate = Path(found)
    candidate = candidate.resolve()
    if not candidate.is_file() or not os.access(candidate, os.X_OK):
        raise CheckError(f"clang-format is not executable: {candidate}")
    result = subprocess.run(
        [os.fspath(candidate), "--version"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    output = (result.stdout + result.stderr).decode(errors="replace")
    match = re.search(r"clang-format version\s+([0-9]+\.[0-9]+\.[0-9]+)", output)
    if result.returncode != 0 or match is None:
        raise CheckError(f"cannot determine clang-format version from {candidate}: {output.strip()}")
    if match.group(1) != expected:
        raise CheckError(
            f"clang-format version mismatch: config={expected}, binary={match.group(1)} ({candidate})"
        )
    return candidate


def _read_blob(repo: Path, object_name: bytes) -> bytes:
    # The object name is an argv element rather than a line in cat-file's batch
    # protocol, so Git paths containing newlines remain unambiguous.
    result = subprocess.run(
        ["git", "cat-file", "blob", object_name],
        cwd=repo,
        env=_git_env(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.decode(errors="replace").strip()
        raise CheckError(f"cannot read base blob {os.fsdecode(object_name)}: {detail}")
    return result.stdout


def _base_commit(repo: Path, value: str) -> str:
    result = _run_git(repo, ["rev-parse", "--verify", "--end-of-options", f"{value}^{{commit}}"])
    if result.returncode != 0:
        detail = result.stderr.decode(errors="replace").strip()
        raise CheckError(f"base is not a local commit: {value}: {detail}")
    return result.stdout.decode("ascii").strip()


def _parse_diff_paths(data: bytes) -> list[tuple[str, bytes, bytes | None]]:
    fields = data.split(b"\0")
    result: list[tuple[str, bytes, bytes | None]] = []
    index = 0
    while index < len(fields) and fields[index]:
        status = fields[index].decode("ascii", errors="replace")
        index += 1
        if not status:
            break
        kind = status[0]
        if kind in {"C", "R"}:
            if index + 1 >= len(fields):
                raise CheckError("git diff returned an incomplete rename/copy record")
            old_path = fields[index]
            new_path = fields[index + 1]
            index += 2
            result.append((status, new_path, old_path))
        else:
            if index >= len(fields):
                raise CheckError("git diff returned an incomplete path record")
            path = fields[index]
            index += 1
            result.append((status, path, path if kind != "A" else None))
    return result


def _base_candidates(repo: Path, base: str, reporter: Reporter) -> list[Candidate]:
    commit = _base_commit(repo, base)
    result = _run_git(
        repo,
        [
            "diff",
            "--no-ext-diff",
            "--no-textconv",
            "--find-renames",
            "--name-status",
            "-z",
            commit,
            "--",
        ],
    )
    if result.returncode != 0:
        detail = result.stderr.decode(errors="replace").strip()
        raise CheckError(f"cannot enumerate base diff: {detail}")
    candidates: list[Candidate] = []
    for status, path_bytes, base_path_bytes in _parse_diff_paths(result.stdout):
        rel = _relative_text(path_bytes)
        applicable, reason = _classify(rel)
        if not applicable:
            reporter.excluded_path(reason or "out-of-scope", rel)
            continue
        path = _path_from_rel(path_bytes)
        if _path_has_symlink_component(path):
            reporter.excluded_path("symbolic-link", rel)
            continue
        candidates.append(Candidate(rel, path_bytes, status, base_path_bytes))
    print(f"base-commit: {commit}")
    print("note: untracked files are not part of --base; use --paths explicitly")
    return candidates


def _all_candidates(repo: Path, reporter: Reporter) -> list[Candidate]:
    result = _run_git(repo, ["ls-files", "-z", "--cached"])
    if result.returncode != 0:
        detail = result.stderr.decode(errors="replace").strip()
        raise CheckError(f"cannot enumerate tracked files: {detail}")
    candidates: list[Candidate] = []
    for path_bytes in result.stdout.split(b"\0"):
        if not path_bytes:
            continue
        rel = _relative_text(path_bytes)
        applicable, reason = _classify(rel)
        if not applicable:
            reporter.excluded_path(reason or "out-of-scope", rel)
            continue
        path = _path_from_rel(path_bytes)
        if _path_has_symlink_component(path):
            reporter.excluded_path("symbolic-link", rel)
            continue
        candidates.append(Candidate(rel, path_bytes, "tracked", path_bytes))
    return candidates


def _line_starts(data: bytes) -> list[int]:
    starts = [0]
    for index, byte in enumerate(data):
        if byte == 10:
            starts.append(index + 1)
    return starts


def _line_span(data: bytes, line_range: LineRange) -> tuple[int, int]:
    starts = _line_starts(data)
    start_index = max(0, min(line_range.start - 1, len(starts) - 1))
    end_index = max(0, min(line_range.end, len(starts) - 1))
    start = starts[start_index]
    end = starts[end_index] if end_index > start_index else len(data)
    if line_range.end >= len(starts):
        end = len(data)
    return start, end


def _changed_ranges(before: bytes, after: bytes) -> list[LineRange]:
    old_lines = before.splitlines(keepends=True)
    new_lines = after.splitlines(keepends=True)
    matcher = difflib.SequenceMatcher(a=old_lines, b=new_lines, autojunk=False)
    ranges: list[LineRange] = []
    for tag, _old_start, _old_end, new_start, new_end in matcher.get_opcodes():
        if tag == "equal":
            continue
        if new_start == new_end:
            if not new_lines:
                continue
            line = min(new_start, len(new_lines) - 1) + 1
            ranges.append(LineRange(line, line))
        else:
            ranges.append(LineRange(new_start + 1, new_end))
    merged: list[LineRange] = []
    for item in sorted(ranges, key=lambda value: (value.start, value.end)):
        if merged and item.start <= merged[-1].end + 1:
            merged[-1] = LineRange(merged[-1].start, max(merged[-1].end, item.end))
        else:
            merged.append(item)
    return merged


def _formatter_xml(
    formatter: Path,
    config: Path,
    rel: str,
    data: bytes,
    ranges: Sequence[LineRange] | None,
) -> list[Replacement]:
    args = [
        os.fspath(formatter),
        f"-style=file:{config}",
        "-output-replacements-xml",
        f"-assume-filename={rel}",
    ]
    if ranges is not None:
        for item in ranges:
            args.append(f"--lines={item.start}:{item.end}")
    result = subprocess.run(
        args,
        input=data,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.decode(errors="replace").strip()
        raise CheckError(f"clang-format failed for {rel}: {detail}")
    try:
        root = ET.fromstring(result.stdout)
    except ET.ParseError as exc:
        raise CheckError(f"clang-format returned incomplete XML for {rel}: {exc}") from exc
    if root.tag != "replacements" or root.attrib.get("incomplete_format") != "false":
        raise CheckError(f"clang-format returned incomplete replacement XML for {rel}")
    replacements: list[Replacement] = []
    for child in root:
        if child.tag != "replacement":
            raise CheckError(f"clang-format returned unknown XML element for {rel}: {child.tag}")
        try:
            offset = int(child.attrib["offset"])
            length = int(child.attrib["length"])
        except (KeyError, ValueError) as exc:
            raise CheckError(f"clang-format returned an invalid replacement span for {rel}") from exc
        if offset < 0 or length < 0 or offset + length > len(data):
            raise CheckError(f"clang-format returned an out-of-bounds replacement for {rel}")
        replacements.append(Replacement(offset, length, "".join(child.itertext())))
    return replacements


def _escaped(value: str) -> str:
    return value.encode("unicode_escape", errors="backslashreplace").decode("ascii")


def _report_replacements(
    reporter: Reporter,
    rel: str,
    data: bytes,
    replacements: Sequence[Replacement],
    allowed: Sequence[tuple[int, int]] | None,
) -> bool:
    if not replacements:
        return True
    for item in replacements:
        in_scope = allowed is None or any(
            start <= item.offset and item.offset + item.length <= end for start, end in allowed
        )
        scope_text = "in-scope" if in_scope else "out-of-scope"
        print(
            f"replacement: {rel} offset={item.offset} length={item.length} "
            f"end={item.offset + item.length} {scope_text} text={_escaped(item.text)}"
        )
    if allowed is not None:
        out_of_scope = [
            item
            for item in replacements
            if not any(start <= item.offset and item.offset + item.length <= end for start, end in allowed)
        ]
        if out_of_scope:
            reporter.error(f"formatter span crosses outside changed lines: {rel}")
    reporter.error(f"format differences found: {rel}")
    return False


def _check_candidate(
    candidate: Candidate,
    formatter: Path,
    config_path: Path,
    config_snapshot: FileSnapshot,
    reporter: Reporter,
    base_commit: str | None,
) -> None:
    path = _path_from_rel(candidate.rel_bytes)
    if candidate.status == "D":
        print(f"deleted: {candidate.rel} (no current bytes to format)")
        reporter.checked += 1
        return
    if not path.exists() or not path.is_file():
        reporter.error(f"native file is missing from the working tree: {candidate.rel}")
        return
    if _path_has_symlink_component(path):
        reporter.error(f"symbolic links are not checkable: {candidate.rel}")
        return
    before_snapshot = _snapshot(path)
    data = path.read_bytes()
    ranges: list[LineRange] | None = None
    allowed: list[tuple[int, int]] | None = None
    if base_commit is not None:
        if candidate.base_rel_bytes is None:
            base_data = b""
        else:
            base_spec = base_commit.encode("ascii") + b":" + candidate.base_rel_bytes
            base_data = _read_blob(_REPO_ROOT, base_spec)  # type: ignore[arg-type]
        ranges = _changed_ranges(base_data, data)
        if not ranges:
            print(f"not-applicable: {candidate.rel} (no current line changes)")
            _assert_unchanged(path, before_snapshot)
            return
        allowed = [_line_span(data, item) for item in ranges]
    try:
        replacements = _formatter_xml(formatter, config_path, candidate.rel, data, ranges)
        _report_replacements(reporter, candidate.rel, data, replacements, allowed)
        if not replacements:
            print(f"pass: {candidate.rel}")
    finally:
        _assert_unchanged(path, before_snapshot)
        _assert_unchanged(config_path, config_snapshot)
    if not replacements:
        reporter.checked += 1


def _make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--paths", nargs="+", metavar="PATH")
    group.add_argument("--base", metavar="COMMIT")
    group.add_argument("--all-native", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    global _REPO_ROOT
    parser = _make_parser()
    args = parser.parse_args(argv)
    reporter = Reporter()
    try:
        _REPO_ROOT = _repo_root()
        config_path = _config_path()
        config_snapshot = _snapshot(config_path)
        expected_version = _format_version(config_path.read_bytes())
        formatter = _find_formatter(expected_version, _REPO_ROOT)
        candidates: list[Candidate]
        base_commit: str | None = None
        scope: str
        if args.paths is not None:
            candidates = []
            for raw in args.paths:
                rel, rel_bytes, path = _explicit_path(raw)
                _validate_explicit(rel, path)
                candidates.append(Candidate(rel, rel_bytes, "explicit", None))
            scope = "paths"
        elif args.base is not None:
            candidates = _base_candidates(_REPO_ROOT, args.base, reporter)
            base_commit = _base_commit(_REPO_ROOT, args.base)
            scope = "base"
        else:
            candidates = _all_candidates(_REPO_ROOT, reporter)
            scope = "all-native"
        for candidate in candidates:
            _check_candidate(
                candidate,
                formatter,
                config_path,
                config_snapshot,
                reporter,
                base_commit,
            )
        _assert_unchanged(config_path, config_snapshot)
        reporter.summary(scope)
    except CheckError as exc:
        reporter.error(str(exc))
        return 2
    if reporter.errors:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
