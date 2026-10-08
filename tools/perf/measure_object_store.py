#!/usr/bin/env python3
"""Measure real object-store operations and their read-lock scopes."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import sys

from measurement import ROOT, checked_run, coverage_lines, runner

OPERATIONS = ("current", "missing", "stale", "path", "status")
COUNTS = (1000, 10000, 50000)


def store_work(session, prefix, build, environment):
    source = ROOT / "src/vm/internal/object_store.cc"
    lines = source.read_text().splitlines()
    counts = coverage_lines(session, prefix, build, environment, source)
    start = next(i for i, line in enumerate(lines)
                 if line.startswith("OwnerLocalBridgeSummary owner_local_bridge_summary_locked("))
    end = next(i for i in range(start + 1, len(lines))
               if lines[i].startswith("OwnerLocalLookupResult "))

    def counter(pattern, begin=0, limit=len(lines)):
        matches = [i + 1 for i in range(begin, limit) if re.fullmatch(pattern, lines[i].strip())]
        if len(matches) != 1 or matches[0] not in counts:
            raise runner.RunnerError(f"cannot locate executable counter: {pattern}")
        return counts[matches[0]]

    index_present = any("global_records_by_id.emplace(" in line for line in lines[start:end])
    return {
        "id_scan_comparisons": counter(r"if \(entry.second.object_id == object_id\) \{"),
        "summary_global_records": counter(r"auto\s*\*\s*object = entry.first;", start, end),
        "index_loop_present": index_present,
        "summary_index_records": counter(r"global_records_by_id.emplace\(.*", start, end)
        if index_present else 0,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--evidence-dir", required=True)
    parser.add_argument("--coverage-build", help="GCC coverage build, not a timing baseline")
    parser.add_argument("--objects", type=int, choices=COUNTS)
    parser.add_argument("--operation", choices=OPERATIONS)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--timeout", type=int, default=900, help="seconds per supervised probe")
    options = parser.parse_args()
    if options.rounds <= 0:
        parser.error("--rounds must be positive")
    if options.timeout <= 0:
        parser.error("--timeout must be positive")
    binary = runner.require_executable(options.binary)
    timer = runner.require_executable("/usr/bin/time")
    build = Path(options.coverage_build).resolve() if options.coverage_build else None
    if build is not None and not binary.is_relative_to(build):
        raise runner.RunnerError("benchmark binary is outside the coverage build")
    rounds = 1 if build else options.rounds
    objects = (options.objects,) if options.objects else COUNTS
    operations = (options.operation,) if options.operation else OPERATIONS
    session = runner.EvidenceSession(options.evidence_dir)
    print(f"evidence_directory: {session.root}", flush=True)
    reports = []
    for count in objects:
        for operation in operations:
            _root, mudlib = runner.render_sandbox(session)
            environment = runner.command_environment(mudlib)
            result_path = session.root / f"case-{len(reports) + 1:03}.json"
            command = [str(binary), "--objects", str(count), "--operation", operation,
                       "--rounds", str(rounds), "--json", str(result_path)]
            if build:
                command.append("--reset-coverage")
            resource_path = result_path.with_suffix(".resources.json")
            command = [str(timer), "-f", '{"peak_rss_kib":%M,"wall_seconds":%e}',
                       "-o", str(resource_path), *command]
            started = datetime.now(timezone.utc).isoformat()
            checked_run(session, command, mudlib, environment, f"{operation}-{count}", binary,
                        timeout=options.timeout)
            completed = datetime.now(timezone.utc).isoformat()
            resources = json.loads(resource_path.read_text())
            if (type(resources["peak_rss_kib"]) is not int or resources["peak_rss_kib"] <= 0
                    or not isinstance(resources["wall_seconds"], (int, float))
                    or resources["wall_seconds"] < 0):
                raise runner.RunnerError("invalid process resource measurement")
            report = json.loads(result_path.read_text())
            metrics = report["metrics"]
            if (report["schema"] != "object_store_bench_v1"
                    or report["runtime"]["probe_operation"] != operation
                    or metrics["object_count"] != count or metrics["probe_rounds"] != rounds
                    or metrics["global_record_total"] < count
                    or metrics["global_destructed_record_total"] != count // 10
                    or metrics["global_record_total"] != metrics["global_live_record_total"]
                    + metrics["global_destructed_record_total"]):
                raise runner.RunnerError("probe report does not match its workload")
            samples = []
            for i in range(rounds):
                sample = {key: metrics[f"{key}_{i}"] for key in
                          ("elapsed_ns", "read_lock_ns", "read_lock_count", "cpp_allocations")}
                if (not all(type(value) is int for value in sample.values())
                        or not 0 < sample["read_lock_ns"] <= sample["elapsed_ns"]
                        or sample["read_lock_count"] != 1 or sample["cpp_allocations"] < 0
                        or operation == "current" and sample["cpp_allocations"] != 0):
                    raise runner.RunnerError("invalid probe sample")
                samples.append(sample)
            result = {"objects": count, "operation": operation, "samples": samples,
                      "global_records": metrics["global_record_total"],
                      "owner_shards": metrics["owner_shards"], "coverage": build is not None,
                      "started_utc": started, "completed_utc": completed, "resources": resources}
            if build:
                result["work"] = store_work(session, Path(environment["TMPDIR"]) / "gcov",
                                            build, environment)
            reports.append(result)
            print(f"{operation} {count}: verified", flush=True)
    (session.root / "report.json").write_text(json.dumps(reports, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (runner.RunnerError, OSError, ValueError, KeyError, StopIteration) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
