#!/usr/bin/env python3
"""Measure real explode calls; optionally collect GCC classification loop counts."""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "explode_runner", ROOT / "tools/testsuite/run-targeted.py"
)
runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)


def checked_run(session, command, cwd, environment, label, binary=None):
    result = runner.run_process(
        session, command, cwd=cwd, environment=environment, timeout=180,
        label=label, binary=binary,
    )
    runner.reject_sanitizer_output(result.output)
    if (result.returncode != 0 or result.timed_out or result.failure_reason
            or "libgcov profiling error:" in result.output):
        raise runner.RunnerError(f"process failed: {result.log_path}")
    return result.output


def scanned_bytes(session, prefix: Path, build: Path, environment) -> int:
    header = ROOT / "src/base/internal/EGCIterator.h"
    lines = [i for i, line in enumerate(header.read_text().splitlines(), 1)
             if line.strip() == "acc |= c;"]
    if len(lines) != 1:
        raise runner.RunnerError("cannot identify the classification loop counter")
    gcov = runner.require_executable(shutil.which("gcov") or "gcov")
    notes_by_name = {
        str(path.resolve()).replace("/", "#").removesuffix(".gcno") + ".gcda": path
        for path in build.rglob("*.gcno")
    }
    total = 0
    found = False
    for data in sorted(prefix.rglob("*.gcda")):
        notes_source = notes_by_name.get(data.name)
        if notes_source is None:
            raise runner.RunnerError("coverage notes are outside the selected build")
        if b"EGCIterator.h" not in notes_source.read_bytes():
            continue
        notes = data.with_suffix(".gcno")
        shutil.copy2(notes_source, notes)
        output = checked_run(session, [str(gcov), "-j", "-t", str(notes)], prefix,
                             environment, "gcov")
        report = json.loads(output)
        for file in report["files"]:
            source = Path(report["current_working_directory"]) / file["file"]
            if source.resolve() != header:
                continue
            for line in file["lines"]:
                if line["line_number"] == lines[0]:
                    found = True
                    total += line["count"]
    if not found or total <= 0:
        raise runner.RunnerError("classification loop coverage is missing")
    return total


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--evidence-dir", required=True)
    parser.add_argument("--coverage-build", help="GCC --coverage build, not a timing baseline")
    options = parser.parse_args()
    binary = runner.require_executable(options.binary)
    build = Path(options.coverage_build).resolve() if options.coverage_build else None
    if build is not None and not binary.is_relative_to(build):
        raise runner.RunnerError("benchmark binary is outside the coverage build")
    session = runner.EvidenceSession(options.evidence_dir)
    print(f"evidence_directory: {session.root}", flush=True)
    results = []
    for kind in ("ascii", "unicode"):
        for count in (1000, 10000, 50000):
            _root, mudlib = runner.render_sandbox(session)
            environment = runner.command_environment(mudlib)
            command = [str(binary), kind, str(count)]
            prefix = Path(environment["TMPDIR"]) / "gcov"
            if build is not None:
                command.append("--coverage")
            output = checked_run(session, command, mudlib, environment, f"{kind}-{count}", binary)
            reports = [line.removeprefix("BENCH_EXPLODE_JSON=") for line in output.splitlines()
                       if line.startswith("BENCH_EXPLODE_JSON=")]
            if len(reports) != 1:
                raise runner.RunnerError("benchmark completion report is missing or repeated")
            report = json.loads(reports[0])
            rounds = 1 if build is not None else 5
            if (report["kind"] != kind or report["tokens"] != count or not report["verified"]
                    or report["coverage"] != (build is not None)
                    or len(report["seconds"]) != rounds
                    or not all(value > 0 for value in report["seconds"])):
                raise runner.RunnerError("benchmark report does not match the requested case")
            if build is not None:
                report["scanned_bytes"] = scanned_bytes(session, prefix, build, environment)
            results.append(report)
            print(f"{kind} {count}: verified", flush=True)
    (session.root / "report.json").write_text(json.dumps(results, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (runner.RunnerError, OSError, ValueError, KeyError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
