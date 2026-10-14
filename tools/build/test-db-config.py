#!/usr/bin/env python3
"""Exercise the database CMake ID contract in isolated configure directories."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Any


_CMAKE_TIMEOUT_SECONDS = 300
_DEPENDENCY_ERROR = re.compile(
    r"Could NOT find (?:MySQL|SQLite3|PostgreSQL)|(?:MySQL|SQLite3|PostgreSQL).*NOTFOUND",
    re.IGNORECASE,
)


_CASES: tuple[dict[str, Any], ...] = (
    {
        "name": "db-disabled",
        "expect": "success",
        "definitions": (
            "-DPACKAGE_DB=OFF",
            "-DPACKAGE_DB_MYSQL=0",
            "-DPACKAGE_DB_SQLITE=OFF",
            "-DPACKAGE_DB_POSTGRESQL=",
            "-DPACKAGE_DB_DEFAULT_DB=0",
        ),
    },
    {
        "name": "no-backend-enabled",
        "expect": "failure",
        "diagnostic": "no database backend is enabled",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=0",
            "-DPACKAGE_DB_SQLITE=OFF",
            "-DPACKAGE_DB_POSTGRESQL=",
            "-DPACKAGE_DB_DEFAULT_DB=0",
        ),
    },
    {
        "name": "mysql-only-default",
        "expect": "success",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=1",
            "-DPACKAGE_DB_SQLITE=",
            "-DPACKAGE_DB_POSTGRESQL=",
            "-DPACKAGE_DB_DEFAULT_DB=1",
        ),
    },
    {
        "name": "sqlite-only-zero-disabled",
        "expect": "success",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=0",
            "-DPACKAGE_DB_SQLITE=1",
            "-DPACKAGE_DB_POSTGRESQL=",
            "-DPACKAGE_DB_DEFAULT_DB=1",
        ),
    },
    {
        "name": "sqlite-only-off-disabled",
        "expect": "success",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=OFF",
            "-DPACKAGE_DB_SQLITE=1",
            "-DPACKAGE_DB_POSTGRESQL=",
            "-DPACKAGE_DB_DEFAULT_DB=1",
        ),
    },
    {
        "name": "all-distinct",
        "expect": "success",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=1",
            "-DPACKAGE_DB_SQLITE=2",
            "-DPACKAGE_DB_POSTGRESQL=3",
            "-DPACKAGE_DB_DEFAULT_DB=2",
        ),
    },
    {
        "name": "duplicate-mysql-sqlite",
        "expect": "failure",
        "diagnostic": "unique type IDs",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=1",
            "-DPACKAGE_DB_SQLITE=1",
            "-DPACKAGE_DB_POSTGRESQL=3",
            "-DPACKAGE_DB_DEFAULT_DB=1",
        ),
    },
    {
        "name": "duplicate-mysql-postgresql",
        "expect": "failure",
        "diagnostic": "unique type IDs",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=1",
            "-DPACKAGE_DB_SQLITE=2",
            "-DPACKAGE_DB_POSTGRESQL=1",
            "-DPACKAGE_DB_DEFAULT_DB=1",
        ),
    },
    {
        "name": "duplicate-sqlite-postgresql",
        "expect": "failure",
        "diagnostic": "unique type IDs",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=1",
            "-DPACKAGE_DB_SQLITE=2",
            "-DPACKAGE_DB_POSTGRESQL=2",
            "-DPACKAGE_DB_DEFAULT_DB=2",
        ),
    },
    {
        "name": "default-not-enabled",
        "expect": "failure",
        "diagnostic": "must match an enabled database backend",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=1",
            "-DPACKAGE_DB_SQLITE=2",
            "-DPACKAGE_DB_POSTGRESQL=3",
            "-DPACKAGE_DB_DEFAULT_DB=4",
        ),
    },
    {
        "name": "default-zero",
        "expect": "failure",
        "diagnostic": "PACKAGE_DB_DEFAULT_DB must be a positive decimal integer",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=1",
            "-DPACKAGE_DB_SQLITE=",
            "-DPACKAGE_DB_POSTGRESQL=",
            "-DPACKAGE_DB_DEFAULT_DB=0",
        ),
    },
    {
        "name": "non-decimal-backend",
        "expect": "failure",
        "diagnostic": "PACKAGE_DB_MYSQL must be a positive decimal integer",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=abc",
            "-DPACKAGE_DB_SQLITE=1",
            "-DPACKAGE_DB_POSTGRESQL=",
            "-DPACKAGE_DB_DEFAULT_DB=1",
        ),
    },
    {
        "name": "negative-backend",
        "expect": "failure",
        "diagnostic": "PACKAGE_DB_MYSQL must be a positive decimal integer",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=-1",
            "-DPACKAGE_DB_SQLITE=1",
            "-DPACKAGE_DB_POSTGRESQL=",
            "-DPACKAGE_DB_DEFAULT_DB=1",
        ),
    },
    {
        "name": "backend-out-of-range",
        "expect": "failure",
        "diagnostic": "PACKAGE_DB_MYSQL must be no greater than 2147483647",
        "definitions": (
            "-DPACKAGE_DB=ON",
            "-DPACKAGE_DB_MYSQL=2147483648",
            "-DPACKAGE_DB_SQLITE=1",
            "-DPACKAGE_DB_POSTGRESQL=",
            "-DPACKAGE_DB_DEFAULT_DB=1",
        ),
    },
)


def _tracked_manifest(source: Path) -> dict[str, tuple[int, int]]:
    result = subprocess.run(
        ["git", "-C", str(source), "ls-files", "-z"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    manifest: dict[str, tuple[int, int]] = {}
    for raw_path in result.stdout.split(b"\0"):
        if not raw_path:
            continue
        path = source / raw_path.decode()
        stat = path.stat()
        manifest[str(path.relative_to(source))] = (stat.st_size, stat.st_mtime_ns)
    return manifest


def _run_case(source: Path, work_dir: Path, case: dict[str, Any]) -> dict[str, Any]:
    case_dir = work_dir / str(case["name"])
    case_dir.mkdir()
    command = [
        "cmake",
        "-S",
        str(source),
        "-B",
        str(case_dir),
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DENABLE_LTO=OFF",
        "-DMARCH_NATIVE=OFF",
        "-DPACKAGE_PCRE=OFF",
        *case["definitions"],
    ]
    try:
        completed = subprocess.run(
            command,
            cwd=source,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=_CMAKE_TIMEOUT_SECONDS,
            check=False,
        )
        output = completed.stdout
        return_code = completed.returncode
        timed_out = False
    except subprocess.TimeoutExpired as error:
        output = (error.stdout or "") + "\nCMake configure timed out.\n"
        return_code = 124
        timed_out = True
    except OSError as error:
        output = f"Unable to execute CMake: {error}\n"
        return_code = 127
        timed_out = False

    log_path = case_dir / "configure.log"
    log_path.write_text(output, encoding="utf-8")
    expected = str(case["expect"])
    diagnostic = str(case.get("diagnostic", ""))
    if expected == "success":
        if return_code == 0:
            status = "passed"
        elif _DEPENDENCY_ERROR.search(output):
            status = "blocked-missing-dependency"
        else:
            status = "failed"
    elif return_code == 0:
        status = "failed-unexpected-success"
    elif diagnostic and diagnostic not in output:
        status = "failed-wrong-diagnostic"
    else:
        status = "passed"

    return {
        "name": case["name"],
        "expected": expected,
        "status": status,
        "returncode": return_code,
        "timed_out": timed_out,
        "diagnostic": diagnostic,
        "command": command,
        "log": str(log_path),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="repository source directory")
    parser.add_argument("--work-dir", required=True, type=Path, help="new output directory")
    options = parser.parse_args()

    source = options.source.expanduser().resolve()
    work_dir = options.work_dir.expanduser().resolve()
    if not source.is_dir() or not (source / "CMakeLists.txt").is_file():
        parser.error(f"--source is not a repository with CMakeLists.txt: {source}")
    if work_dir.exists():
        parser.error(f"--work-dir must not already exist: {work_dir}")
    work_dir.parent.mkdir(parents=True, exist_ok=True)
    work_dir.mkdir()

    before = _tracked_manifest(source)
    results = [_run_case(source, work_dir, case) for case in _CASES]
    after = _tracked_manifest(source)
    source_unchanged = before == after
    summary = {
        "source": str(source),
        "work_dir": str(work_dir),
        "source_unchanged": source_unchanged,
        "cases": results,
    }
    (work_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    for result in results:
        print(f"{result['name']}: {result['status']}", flush=True)
    if not source_unchanged:
        print("source: failed-source-modification", file=sys.stderr)
        return 1
    if any(result["status"] == "failed" or result["status"].startswith("failed-") for result in results):
        return 1
    if any(result["status"] == "blocked-missing-dependency" for result in results):
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
