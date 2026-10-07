#!/usr/bin/env python3
"""Exercise lpcc IO failures in materialized mudlibs under the bounded supervisor."""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import unittest


def exec_lpcc(arguments: list[str]) -> None:
    mode, binary, *command = arguments
    if mode == "full-output":
        fd = os.open("/dev/full", os.O_WRONLY)
        os.dup2(fd, 1)
        os.close(fd)
    elif mode in {"bad-input", "batch-input"}:
        fd = os.open("." if mode == "bad-input" else "cli_io_list.txt", os.O_RDONLY)
        os.dup2(fd, 0)
        os.close(fd)
    elif mode != "normal":
        raise ValueError(f"unknown launch mode: {mode}")
    os.execv(binary, [binary, *command])


if __name__ == "__main__" and sys.argv[1:2] == ["--exec"]:
    try:
        exec_lpcc(sys.argv[2:])
    except OSError as error:
        print(f"lpcc test launcher: {error}", file=sys.stderr)
        sys.exit(125)

spec = importlib.util.spec_from_file_location(
    "lpcc_io_runner", Path(__file__).with_name("run-targeted.py")
)
runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)


class LpccIoTest(unittest.TestCase):
    binary: Path
    session: runner.EvidenceSession

    def run_lpcc(self, arguments: list[str], expected: int, mode: str = "normal") -> str:
        _root, mudlib = runner.render_sandbox(self.session)
        (mudlib / "cli_io_case.lpc").write_text("int answer() { return 42; }\n")
        (mudlib / "cli_io_bad.lpc").write_text("int broken(;\n")
        (mudlib / "cli_io_list.txt").write_text("/cli_io_case.lpc\n")
        result = runner.run_process(
            self.session,
            [sys.executable, str(Path(__file__).resolve()), "--exec", mode,
             str(self.binary), *arguments],
            cwd=mudlib,
            environment=runner.command_environment(mudlib),
            timeout=30,
            label=self._testMethodName,
            binary=self.binary,
        )
        runner.reject_sanitizer_output(result.output)
        self.assertFalse(result.timed_out, result.log_path)
        self.assertEqual(result.failure_reason, "", result.log_path)
        self.assertEqual(result.returncode, expected, result.log_path)
        return result.output

    def test_owner_audit_success(self) -> None:
        output = self.run_lpcc(
            ["--owner-audit", "--format=json", "etc/config.test", "cli_io_case.lpc"], 0
        )
        self.assertTrue(json.loads(output)["success"])

    def test_owner_audit_output_failure(self) -> None:
        self.run_lpcc(
            ["--owner-audit", "--format=json", "etc/config.test", "cli_io_case.lpc"],
            1, "full-output",
        )

    def test_owner_audit_missing_input(self) -> None:
        self.run_lpcc(
            ["--owner-audit", "--format=json", "etc/config.test", "cli_io_missing.lpc"], 1
        )

    def test_owner_audit_read_failure(self) -> None:
        self.run_lpcc(["--owner-audit", "--format=json", "etc/config.test", "."], 1)

    def test_batch_success(self) -> None:
        output = self.run_lpcc(["--batch", "etc/config.test", "/cli_io_case.lpc"], 0)
        self.assertIn("PASS /cli_io_case.lpc", output)

    def test_batch_output_failure(self) -> None:
        self.run_lpcc(["--batch", "etc/config.test", "/cli_io_case.lpc"], 1, "full-output")

    def test_batch_empty_input_remains_successful(self) -> None:
        self.run_lpcc(["--batch", "etc/config.test"], 0)

    def test_batch_stdin_success(self) -> None:
        output = self.run_lpcc(["--batch", "etc/config.test"], 0, "batch-input")
        self.assertIn("PASS /cli_io_case.lpc", output)

    def test_batch_input_failure(self) -> None:
        self.run_lpcc(["--batch", "etc/config.test"], 1, "bad-input")

    def test_disassembly_success(self) -> None:
        self.run_lpcc(["etc/config.test", "/cli_io_case.lpc"], 0)

    def test_disassembly_output_failure(self) -> None:
        self.run_lpcc(["etc/config.test", "/cli_io_case.lpc"], 1, "full-output")

    def test_compile_failure(self) -> None:
        self.run_lpcc(["etc/config.test", "/cli_io_bad.lpc"], 1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lpcc", required=True)
    parser.add_argument("--evidence-dir")
    options = parser.parse_args()
    LpccIoTest.binary = runner.require_executable(options.lpcc)
    LpccIoTest.session = runner.EvidenceSession(options.evidence_dir)
    print(f"evidence_directory: {LpccIoTest.session.root}", flush=True)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(LpccIoTest)
    discovered = suite.countTestCases()
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    passed = (result.wasSuccessful() and not result.skipped
              and result.testsRun == discovered and discovered > 0)
    (LpccIoTest.session.root / "test-summary.json").write_text(json.dumps({
        "passed": passed, "discovered": discovered, "executed": result.testsRun,
        "failures": len(result.failures), "errors": len(result.errors),
        "skipped": len(result.skipped),
    }, indent=2) + "\n")
    sys.exit(0 if passed else 1)
