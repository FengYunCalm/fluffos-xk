#!/usr/bin/env python3
"""Fail-first and non-mutation tests for tools/style/check-format.py."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


HERE = Path(__file__).resolve().parent
CHECKER = HERE / "check-format.py"
CONFIG = HERE.parent.parent / "src" / ".clang-format"


class TestFailure(Exception):
    pass


def run(
    command: list[str],
    cwd: Path,
    expected: int | None = None,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    process_env = os.environ.copy()
    if env:
        process_env.update(env)
    result = subprocess.run(
        command,
        cwd=cwd,
        env=process_env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if expected is not None and result.returncode != expected:
        raise TestFailure(
            f"unexpected exit {result.returncode}, expected {expected}: {' '.join(command)}\n"
            f"{result.stdout}"
        )
    return result


def write(path: Path, data: str | bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(data, str):
        path.write_text(data, encoding="utf-8", newline="")
    else:
        path.write_bytes(data)


def git(repo: Path, *args: str, expected: int = 0) -> str:
    return run(["git", *args], repo, expected).stdout


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare_repo(root: Path) -> None:
    (root / "tools/style").mkdir(parents=True, exist_ok=True)
    shutil.copy2(CHECKER, root / "tools/style/check-format.py")
    write(root / "src/.clang-format", CONFIG.read_bytes())
    git(root, "init", "-q")
    git(root, "config", "user.email", "style-test@example.invalid")
    git(root, "config", "user.name", "style-check-format")
    git(root, "config", "core.autocrlf", "false")


def commit_all(repo: Path, message: str) -> None:
    git(repo, "add", "--all")
    git(repo, "commit", "-qm", message)


def check(
    repo: Path,
    *args: str,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    return run([sys.executable, str(repo / "tools/style/check-format.py"), *args], repo, env=env)


def expect_pass(repo: Path, *args: str) -> str:
    result = check(repo, *args)
    if result.returncode != 0:
        raise TestFailure(f"expected pass for {args}, got {result.returncode}\\n{result.stdout}")
    return result.stdout


def expect_fail(repo: Path, *args: str) -> str:
    result = check(repo, *args)
    if result.returncode == 0:
        raise TestFailure(f"expected failure for {args}\n{result.stdout}")
    return result.stdout


def assert_contains(text: str, needle: str) -> None:
    if needle not in text:
        raise TestFailure(f"missing {needle!r} in output:\n{text}")


def test_paths_and_non_mutation(repo: Path) -> None:
    good = repo / "src/good.cc"
    write(good, "int add(int left, int right) {\n  return left + right;\n}\n")
    write(repo / "src/space name.cc", good.read_bytes())
    write(repo / "src/line\nname.cc", good.read_bytes())
    write(repo / "testsuite/not-native.c", good.read_bytes())
    outside = repo.parent / "outside.cc"
    write(outside, good.read_bytes())
    symlink = repo / "src/symlink.cc"
    symlink.symlink_to(good)
    commit_all(repo, "baseline")

    before_hash = sha256(good)
    before_stat = good.stat()
    output = expect_pass(repo, "--paths", "src/good.cc", "src/space name.cc", "src/line\nname.cc")
    assert_contains(output, "checked: 3")
    after_stat = good.stat()
    if sha256(good) != before_hash or after_stat.st_mtime_ns != before_stat.st_mtime_ns:
        raise TestFailure("--paths changed source bytes or mtime")

    bad = repo / "src/bad.cc"
    write(bad, "int bad(int left,int right){return left+right;}\n")
    before_bad = (sha256(bad), bad.stat().st_mtime_ns)
    output = expect_fail(repo, "--paths", "src/bad.cc")
    assert_contains(output, "replacement:")
    if (sha256(bad), bad.stat().st_mtime_ns) != before_bad:
        raise TestFailure("failed --paths check changed source bytes or mtime")

    expect_fail(repo, "--paths", "testsuite/not-native.c")
    expect_fail(repo, "--paths", os.fspath(outside))
    expect_fail(repo, "--paths", "src/symlink.cc")
    expect_fail(repo, "--paths", "src/missing.cc")
    unknown = check(repo, "--write")
    if unknown.returncode == 0:
        raise TestFailure("unsupported --write option was accepted")


def test_base_and_all_native(repo: Path) -> None:
    good = repo / "src/good.cc"
    other = repo / "src/other.cc"
    write(good, "int add(int left, int right) {\n  return left + right;\n}\n")
    write(other, good.read_bytes())
    commit_all(repo, "base-scope")
    base = git(repo, "rev-parse", "HEAD").strip()

    write(good, "int add(int left,int right){return left+right;}\n")
    write(repo / "src/untracked.cc", "int untracked(int x){return x+1;}\n")
    staged_before = git(repo, "diff", "--cached", "--binary")
    config_hash = sha256(repo / "src/.clang-format")
    output = expect_fail(repo, "--base", base)
    assert_contains(output, "untracked files are not part of --base")
    assert_contains(output, "format differences found: src/good.cc")
    if git(repo, "diff", "--cached", "--binary") != staged_before:
        raise TestFailure("--base changed the index")
    if sha256(repo / "src/.clang-format") != config_hash:
        raise TestFailure("--base changed the configuration")

    git(repo, "add", "src/good.cc")
    write(other, "int other(int x){return x+2;}\n")
    output = expect_fail(repo, "--base", base)
    assert_contains(output, "format differences found: src/good.cc")
    assert_contains(output, "format differences found: src/other.cc")

    write(good, "int add(int left, int right) {\n  return left + right;\n}\n")
    write(other, good.read_bytes())
    write(repo / "src/untracked.cc", good.read_bytes())
    git(repo, "add", "src/good.cc", "src/other.cc")
    commit_all(repo, "formatted")
    output = expect_pass(repo, "--all-native")
    assert_contains(output, "scope: all-native")
    assert_contains(output, "checked: 3")


def test_rename_delete_and_empty(root: Path) -> None:
    repo = root / "rename-delete"
    repo.mkdir()
    prepare_repo(repo)
    source = "int value(int input) {\n  return input + 1;\n}\n"
    write(repo / "src/old.cc", source)
    write(repo / "src/delete.cc", source)
    commit_all(repo, "rename-base")
    base = git(repo, "rev-parse", "HEAD").strip()
    (repo / "src/old.cc").rename(repo / "src/new.cc")
    write(repo / "src/new.cc", "int value(int input){return input+1;}\n")
    (repo / "src/delete.cc").unlink()
    git(repo, "add", "--all")
    output = expect_fail(repo, "--base", base)
    assert_contains(output, "format differences found: src/new.cc")
    assert_contains(output, "deleted: src/delete.cc")

    empty = root / "empty"
    empty.mkdir()
    prepare_repo(empty)
    write(empty / "README", "no native sources\n")
    commit_all(empty, "empty")
    output = expect_pass(empty, "--all-native")
    assert_contains(output, "result: not-applicable")


def test_formatter_failures(root: Path) -> None:
    repo = root / "formatter-failures"
    repo.mkdir()
    prepare_repo(repo)
    source = repo / "src/input.cc"
    write(source, "int value() { return 1; }\n")

    missing = check(
        repo,
        "--paths",
        "src/input.cc",
        env={"CLANG_FORMAT": os.fspath(repo / "missing-clang-format")},
    )
    if missing.returncode == 0:
        raise TestFailure("missing clang-format was accepted")
    assert_contains(missing.stdout, "clang-format is not executable")

    wrong_version = repo / "wrong-version"
    write(
        wrong_version,
        "#!/usr/bin/python3\nimport sys\n"
        "print('clang-format version 1.0.0' if '--version' in sys.argv else '')\n",
    )
    wrong_version.chmod(0o755)
    output = check(
        repo,
        "--paths",
        "src/input.cc",
        env={"CLANG_FORMAT": os.fspath(wrong_version)},
    )
    if output.returncode == 0:
        raise TestFailure("wrong clang-format version was accepted")
    assert_contains(output.stdout, "clang-format version mismatch")

    invalid_config = repo / "src/.clang-format"
    original_config = invalid_config.read_bytes()
    write(invalid_config, b"not a clang-format config\n")
    output = check(repo, "--paths", "src/input.cc")
    if output.returncode == 0:
        raise TestFailure("configuration without a version declaration was accepted")
    assert_contains(output.stdout, "must contain exactly one")
    write(invalid_config, original_config)

    write(invalid_config, b"\xff\xfe")
    output = check(repo, "--paths", "src/input.cc")
    if output.returncode == 0:
        raise TestFailure("non-UTF-8 configuration was accepted")
    assert_contains(output.stdout, "is not UTF-8")
    write(invalid_config, original_config)

    malformed = repo / "malformed-xml"
    write(
        malformed,
        "#!/usr/bin/python3\nimport sys\n"
        "print('clang-format version 18.1.8' if '--version' in sys.argv else 'not xml')\n",
    )
    malformed.chmod(0o755)
    output = check(
        repo,
        "--paths",
        "src/input.cc",
        env={"CLANG_FORMAT": os.fspath(malformed)},
    )
    if output.returncode == 0:
        raise TestFailure("malformed clang-format XML was accepted")
    assert_contains(output.stdout, "clang-format returned incomplete XML")

    incomplete = repo / "incomplete-xml"
    write(
        incomplete,
        "#!/usr/bin/python3\nimport sys\n"
        "print('clang-format version 18.1.8' if '--version' in sys.argv else "
        "'<?xml version=\"1.0\"?><replacements incomplete_format=\"true\"></replacements>')\n",
    )
    incomplete.chmod(0o755)
    output = check(
        repo,
        "--paths",
        "src/input.cc",
        env={"CLANG_FORMAT": os.fspath(incomplete)},
    )
    if output.returncode == 0:
        raise TestFailure("incomplete clang-format XML was accepted")
    assert_contains(output.stdout, "incomplete replacement XML")

    failed = repo / "failed-formatter"
    write(
        failed,
        "#!/usr/bin/python3\nimport sys\n"
        "print('clang-format version 18.1.8' if '--version' in sys.argv else '')\n"
        "sys.exit(1 if '--version' not in sys.argv else 0)\n",
    )
    failed.chmod(0o755)
    output = check(
        repo,
        "--paths",
        "src/input.cc",
        env={"CLANG_FORMAT": os.fspath(failed)},
    )
    if output.returncode == 0:
        raise TestFailure("failing clang-format process was accepted")
    assert_contains(output.stdout, "clang-format failed for src/input.cc")


def test_deleted_lines_map_to_current_context(root: Path) -> None:
    repo = root / "deleted-lines"
    repo.mkdir()
    prepare_repo(repo)
    source = repo / "src/deleted-lines.cc"
    write(
        source,
        "int obsolete() {\n  return 0;\n}\n"
        "int retained(int value){return value+1;}\n",
    )
    commit_all(repo, "deleted-lines-base")
    base = git(repo, "rev-parse", "HEAD").strip()
    write(source, "int retained(int value){return value+1;}\n")

    output = expect_fail(repo, "--base", base)
    assert_contains(output, "format differences found: src/deleted-lines.cc")
    assert_contains(output, "replacement: src/deleted-lines.cc")


def test_newline_path_in_base(root: Path) -> None:
    repo = root / "newline-base"
    repo.mkdir()
    prepare_repo(repo)
    path = repo / "src/line\nname.cc"
    write(path, "int value(int input) {\n  return input + 1;\n}\n")
    commit_all(repo, "newline-base")
    base = git(repo, "rev-parse", "HEAD").strip()
    write(path, "int value(int input){return input+1;}\n")
    output = expect_fail(repo, "--base", base)
    assert_contains(output, "format differences found: src/line\nname.cc")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="fluffos-style-check-") as directory:
        root = Path(directory)
        paths_repo = root / "paths"
        paths_repo.mkdir()
        prepare_repo(paths_repo)
        test_paths_and_non_mutation(paths_repo)

        base_repo = root / "base"
        base_repo.mkdir()
        prepare_repo(base_repo)
        test_base_and_all_native(base_repo)
        test_rename_delete_and_empty(root)
        test_formatter_failures(root)
        test_deleted_lines_map_to_current_context(root)
        test_newline_path_in_base(root)
    print("PASS: style checker self-test")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except TestFailure as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
