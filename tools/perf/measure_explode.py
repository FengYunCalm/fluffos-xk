#!/usr/bin/env python3
"""Measure real explode calls; optionally collect GCC classification loop counts."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from measurement import ROOT, checked_run, coverage_lines, runner


def scanned_bytes(session, prefix: Path, build: Path, environment) -> int:
    header = ROOT / "src/base/internal/EGCIterator.h"
    lines = [i for i, line in enumerate(header.read_text().splitlines(), 1)
             if line.strip() == "acc |= c;"]
    if len(lines) != 1:
        raise runner.RunnerError("cannot identify the classification loop counter")
    counts = coverage_lines(session, prefix, build, environment, header)
    total = counts.get(lines[0], 0)
    if total <= 0:
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
