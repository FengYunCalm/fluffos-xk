#!/usr/bin/env python3
"""Validate bench_compile schema 2 evidence; never repair incomplete reports."""

import argparse
import json
import math
from pathlib import Path


def validate_report(report: dict) -> None:
    def require(condition: bool, message: str) -> None:
        if not condition:
            raise ValueError(message)

    def finite(value: object) -> None:
        if isinstance(value, float):
            require(math.isfinite(value), "non-finite measurement")
        elif isinstance(value, dict):
            for item in value.values():
                finite(item)
        elif isinstance(value, list):
            for item in value:
                finite(item)

    def equal(actual: float, expected: float, name: str) -> None:
        require(type(actual) in (int, float), f"{name}: expected a number")
        require(math.isclose(actual, expected, rel_tol=1e-12, abs_tol=1e-15), name)

    finite(report)
    require(report["schema_version"] == 2, "unsupported schema")
    files, rounds = report["files"], report["rounds"]
    require(type(files) is int and files > 0, "empty corpus")
    require(type(rounds) is int and rounds > 0, "empty rounds")
    require(isinstance(report["corpus"], str) and bool(report["corpus"]), "missing corpus")
    for field in ("warmup_failures", "failures", "successful_compiles"):
        require(type(report[field]) is int and report[field] >= 0, f"invalid {field}")
    identities = report["corpus_files"]
    require(len(identities) == files, "missing file identity")
    require(len({item["path"] for item in identities}) == files, "duplicate file identity")
    for item in identities:
        require(isinstance(item["path"], str) and bool(item["path"]), "missing path")
        require(type(item["bytes"]) is int and item["bytes"] >= 0, "invalid file size")
        digest = item["sha256"]
        require(isinstance(digest, str) and len(digest) == 64
                and all(char in "0123456789abcdef" for char in digest), "invalid digest")
    require(len(report["warmup_success"]) == files, "incomplete warmup")
    require(all(value is True for value in report["warmup_success"]), "failed warmup")
    require(report["warmup_failures"] == 0 and report["failures"] == 0, "failed compilation")
    samples = report["round_samples"]
    require(len(samples) == rounds, "missing rounds")
    operations = []
    for sample in samples:
        require(type(sample["seconds"]) in (int, float) and sample["seconds"] > 0,
                "invalid round duration")
        require(len(sample["files"]) == files, "missing operation")
        for operation in sample["files"]:
            require(operation["success"] is True, "failed operation")
            require(type(operation["seconds"]) in (int, float) and operation["seconds"] > 0,
                    "invalid operation duration")
            operations.append(operation["seconds"])
        require(sum(item["seconds"] for item in sample["files"]) <= sample["seconds"] * (1 + 1e-12),
                "operations exceed round duration")
    require(report["successful_compiles"] == files * rounds, "incorrect success count")
    chronological = [sample["seconds"] for sample in samples]
    require(report["round_secs"] == sorted(chronological), "incorrect legacy round summary")
    last = [sample["seconds"] for sample in samples[-1]["files"]]
    require(report["per_file_secs_last_round"] == last, "incorrect last round")
    total = sum(chronological)
    equal(report["total_secs"], total, "total_secs")
    equal(report["throughput_files_per_s"], files * rounds / total, "throughput")
    window = max(1, files // 10)
    equal(report["degradation"], sum(last[-window:]) / sum(last[:window]),
          "legacy input distribution")
    lifecycle = report["lifecycle"]
    if rounds == 1:
        require(lifecycle is None, "one round cannot measure lifecycle change")
    else:
        window = max(1, rounds // 10)
        require(lifecycle["window_rounds"] == window, "invalid lifecycle window")
        equal(lifecycle["head_mean_secs"], sum(chronological[:window]) / window, "head mean")
        equal(lifecycle["tail_mean_secs"], sum(chronological[-window:]) / window, "tail mean")
        equal(lifecycle["ratio"], sum(chronological[-window:]) / sum(chronological[:window]),
              "lifecycle ratio must compare the same corpus across rounds")
    operations.sort()
    stats = report["operation_secs"]
    require(stats["sample_count"] == len(operations), "incorrect operation sample count")
    for name, fraction in (("median", 0.5), ("p95", 0.95), ("p99", 0.99)):
        equal(stats[name], operations[min(len(operations) - 1, int(len(operations) * fraction))],
              f"operation {name}")
    require(type(report["peak_rss_kb"]) is int and report["peak_rss_kb"] >= -1, "invalid RSS")
    arena = report["arena"]
    for key in ("warmup_chunk_mallocs", "final_chunk_mallocs", "delta", "retained_chunks",
                "retained_heap_bytes", "cycle_bytes", "peak_cycle_bytes", "resets"):
        require(type(arena[key]) is int and arena[key] >= 0, f"invalid arena {key}")
    require(arena["delta"] == arena["final_chunk_mallocs"] - arena["warmup_chunk_mallocs"],
            "incorrect arena allocation delta")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    try:
        report = json.loads(args.report.read_text(encoding="utf-8"))
        validate_report(report)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"invalid compile evidence: {error}")
        return 1
    print(f"valid compile evidence: {report['files']} files, {report['rounds']} rounds, "
          f"{report['operation_secs']['sample_count']} operation samples")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
