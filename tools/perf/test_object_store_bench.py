#!/usr/bin/env python3
"""Check object-store benchmark failures and the existing default report."""

from __future__ import annotations

import argparse
import json
import sys

from measurement import checked_run, runner


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--evidence-dir", required=True)
    options = parser.parse_args()
    binary = runner.require_executable(options.binary)
    session = runner.EvidenceSession(options.evidence_dir)
    print(f"evidence_directory: {session.root}", flush=True)
    full_stdout = [
        sys.executable, "-c",
        'import os,sys; fd=os.open("/dev/full",os.O_WRONLY); os.dup2(fd,1); '
        'os.close(fd); os.execv(sys.argv[1],[sys.argv[1]])', str(binary),
    ]
    cases = [
        ("json-full", [str(binary), "--json", "/dev/full"],
         "failed to write benchmark json output"),
        ("stdout-full", full_stdout, "failed to write benchmark stdout"),
        ("operation-without-size", [str(binary), "--operation", "status"],
         "probe options require --objects"),
        ("rounds-without-size", [str(binary), "--rounds", "3"],
         "probe options require --objects"),
        ("zero-size", [str(binary), "--objects", "0"], "invalid --objects"),
        ("invalid-operation", [str(binary), "--objects", "1000", "--operation", "unknown"],
         "invalid --operation"),
    ]
    results = []
    for label, command, message in cases:
        _root, mudlib = runner.render_sandbox(session)
        result = runner.run_process(
            session, command, cwd=mudlib, environment=runner.command_environment(mudlib),
            timeout=90, label=label, binary=binary,
        )
        runner.reject_sanitizer_output(result.output)
        if (result.timed_out or result.failure_reason or result.returncode != 1
                or message not in result.output):
            raise runner.RunnerError(f"wrong failure result: {result.log_path}")
        results.append({"case": label, "verified": True})
    output = session.root / "legacy.json"
    _root, mudlib = runner.render_sandbox(session)
    checked_run(session, [str(binary), "--json", str(output)], mudlib,
                runner.command_environment(mudlib), "legacy-success", binary)
    report = json.loads(output.read_text())
    if (report["schema"] != "object_store_bench_v1"
            or report["metrics"]["object_count"] != 32
            or report["metrics"]["resolve_iterations"] != 32 * 256):
        raise runner.RunnerError("default benchmark report changed")
    results.append({"case": "legacy-success", "verified": True})
    (session.root / "checks.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"{len(results)}/{len(results)} checks passed")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (runner.RunnerError, OSError, ValueError, KeyError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
