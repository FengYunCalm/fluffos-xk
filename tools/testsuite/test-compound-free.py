#!/usr/bin/env python3
"""Exercise compound-value release, including real process/thread termination."""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import sys
import unittest

spec = importlib.util.spec_from_file_location(
    "compound_free_runner", Path(__file__).with_name("run-targeted.py")
)
runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)

CASES = (
    "mixed", "inline", "overflow", "wide", "deep", "static-warm", "static-cold",
    "explicit-exit", "thread-late", "exception", "concurrent",
)


class CompoundFreeTest(unittest.TestCase):
    def __init__(self, mode: str, binary: Path, session, prefix: list[str], timeout: int):
        super().__init__("runTest")
        self.mode = mode
        self.binary = binary
        self.session = session
        self.prefix = prefix
        self.timeout = timeout

    def shortDescription(self) -> str:
        return self.mode

    def runTest(self) -> None:
        _root, mudlib = runner.render_sandbox(self.session)
        result = runner.run_process(
            self.session, [*self.prefix, str(self.binary), self.mode], cwd=mudlib,
            environment=runner.command_environment(mudlib), timeout=self.timeout,
            label=self.mode, binary=self.binary,
        )
        runner.reject_sanitizer_output(result.output)
        self.assertFalse(result.timed_out, result.log_path)
        self.assertEqual(result.failure_reason, "", result.log_path)
        self.assertEqual(result.returncode, 0, result.log_path)
        self.assertNotIn("COMPOUND_FREE_ERROR:", result.output, result.log_path)
        self.assertEqual(result.output.count(f"COMPOUND_FREE_OK:{self.mode}\n"), 1,
                         result.log_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--evidence-dir", required=True)
    parser.add_argument("--case", action="append", choices=CASES)
    parser.add_argument("--disable-aslr", action="store_true")
    parser.add_argument("--timeout", type=int, default=90)
    options = parser.parse_args()
    cases = options.case or list(CASES)
    if len(cases) != len(set(cases)):
        parser.error("duplicate --case")
    if options.timeout <= 0:
        parser.error("--timeout must be positive")
    binary = runner.require_executable(options.binary)
    session = runner.EvidenceSession(options.evidence_dir)
    prefix = ["setarch", "x86_64", "-R"] if options.disable_aslr else []
    print(f"evidence_directory: {session.root}", flush=True)
    suite = unittest.TestSuite(
        CompoundFreeTest(mode, binary, session, prefix, options.timeout) for mode in cases
    )
    discovered = suite.countTestCases()
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    passed = (result.wasSuccessful() and not result.skipped
              and result.testsRun == discovered and discovered > 0)
    (session.root / "test-summary.json").write_text(json.dumps({
        "passed": passed, "cases": cases, "discovered": discovered,
        "executed": result.testsRun, "failures": len(result.failures),
        "errors": len(result.errors), "skipped": len(result.skipped),
    }, indent=2) + "\n")
    sys.exit(0 if passed else 1)
