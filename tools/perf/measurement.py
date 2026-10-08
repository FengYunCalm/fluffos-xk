"""Shared supervised execution and GCC line coverage for runtime measurements."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "measurement_runner", ROOT / "tools/testsuite/run-targeted.py"
)
runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)


def checked_run(session, command, cwd, environment, label, binary=None, timeout=180):
    result = runner.run_process(
        session, command, cwd=cwd, environment=environment, timeout=timeout,
        label=label, binary=binary,
    )
    runner.reject_sanitizer_output(result.output)
    if (result.returncode != 0 or result.timed_out or result.failure_reason
            or "libgcov profiling error:" in result.output):
        raise runner.RunnerError(f"process failed: {result.log_path}")
    return result.output


def coverage_lines(session, prefix: Path, build: Path, environment, source: Path):
    gcov = runner.require_executable(shutil.which("gcov") or "gcov")
    notes_by_name = {
        str(path.resolve()).replace("/", "#").removesuffix(".gcno") + ".gcda": path
        for path in build.rglob("*.gcno")
    }
    counts = {}
    for data in sorted(prefix.rglob("*.gcda")):
        notes_source = notes_by_name.get(data.name)
        if notes_source is None:
            raise runner.RunnerError("coverage notes are outside the selected build")
        if source.name.encode() not in notes_source.read_bytes():
            continue
        notes = data.with_suffix(".gcno")
        shutil.copy2(notes_source, notes)
        output = checked_run(session, [str(gcov), "-j", "-t", str(notes)], prefix,
                             environment, "gcov")
        report = json.loads(output)
        for file in report["files"]:
            path = Path(report["current_working_directory"]) / file["file"]
            if path.resolve() != source:
                continue
            for line in file["lines"]:
                number = line["line_number"]
                counts[number] = counts.get(number, 0) + line["count"]
    if not counts:
        raise runner.RunnerError(f"source coverage is missing: {source}")
    return counts
