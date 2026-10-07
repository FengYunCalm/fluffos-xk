#!/usr/bin/env python3
"""Check trace output and failures using real, supervised driver processes."""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import sys
import unittest

spec = importlib.util.spec_from_file_location(
    "trace_io_runner", Path(__file__).with_name("run-targeted.py")
)
runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)


class TraceIoTest(unittest.TestCase):
    probe: Path
    driver: Path
    session: runner.EvidenceSession
    prefix: list[str]

    def run_probe(self, mode: str) -> tuple[Path, str]:
        _root, mudlib = runner.render_sandbox(self.session)
        result = runner.run_process(
            self.session, [*self.prefix, str(self.probe), mode], cwd=mudlib,
            environment=runner.command_environment(mudlib), timeout=30,
            label=self._testMethodName, binary=self.probe,
        )
        runner.reject_sanitizer_output(result.output)
        self.assertFalse(result.timed_out, result.log_path)
        self.assertEqual(result.failure_reason, "", result.log_path)
        self.assertEqual(result.returncode, 0, result.log_path)
        self.assertNotIn("TRACE_OFF_MAIN_LOG", result.output, result.log_path)
        return mudlib, result.output.split("TRACE_PROBE_READY", 1)[1]

    def read_events(self, path: Path) -> list[dict]:
        events = json.loads(path.read_text())
        self.assertIsInstance(events, list)
        self.assertGreater(len(events), 0)
        for event in events:
            self.assertTrue({"pid", "tid", "ts", "dur", "ph", "cat", "name"} <= event.keys())
        return events

    def test_success(self) -> None:
        mudlib, _output = self.run_probe("normal")
        self.assertEqual([e["name"] for e in self.read_events(mudlib / "trace.json")],
                         ["before-stop"])

    def test_main_loop_drain(self) -> None:
        mudlib, output = self.run_probe("drain")
        self.assertIn("TRACE_DRAINED_BEFORE_EXIT", output)
        self.read_events(mudlib / "trace.json")

    def test_stop(self) -> None:
        mudlib, _output = self.run_probe("stop")
        self.assertEqual([e["name"] for e in self.read_events(mudlib / "trace.json")],
                         ["before-stop"])

    def test_open_failure(self) -> None:
        _mudlib, output = self.run_probe("open")
        self.assertRegex(output.lower(), r"error|failed")

    def test_write_failure(self) -> None:
        _mudlib, output = self.run_probe("full")
        self.assertRegex(output.lower(), r"error|failed")
        self.assertNotIn("successfully", output)

    def test_json_failure(self) -> None:
        _mudlib, output = self.run_probe("json")
        self.assertRegex(output.lower(), r"error|failed")

    def test_allocation_failure(self) -> None:
        _mudlib, output = self.run_probe("allocation")
        self.assertRegex(output.lower(), r"error|failed")

    def test_uncollected(self) -> None:
        _mudlib, output = self.run_probe("uncollected")
        self.assertIn("Uncollected profiling events: 1", output)

    def test_concurrent(self) -> None:
        mudlib, _output = self.run_probe("concurrent")
        for name in ["trace.json", *(f"trace-{i}.json" for i in range(4))]:
            self.read_events(mudlib / name)

    def test_live_driver(self) -> None:
        _root, mudlib = runner.render_sandbox(self.session)
        result = runner.run_process(
            self.session,
            [*self.prefix, str(self.driver), "etc/config.test",
             "-ftest:/single/tests/efuns/trace_io"],
            cwd=mudlib, environment=runner.command_environment(mudlib), timeout=30,
            label=self._testMethodName, binary=self.driver,
        )
        runner.reject_sanitizer_output(result.output)
        self.assertFalse(result.timed_out, result.log_path)
        self.assertEqual(result.failure_reason, "", result.log_path)
        self.assertEqual(result.returncode, 0, result.log_path)
        self.assertEqual(result.output.count("C> /single/tests/efuns/trace_io"), 1)
        self.assertEqual(result.output.count("D> C /single/tests/efuns/trace_io"), 1)
        self.assertRegex(result.output, r"D> C /single/tests/efuns/trace_io asserts=[1-9][0-9]*")
        self.assertIn("Checks succeeded.", result.output)
        self.assertNotIn("T> fail", result.output)
        self.read_events(mudlib / "log/trace-io.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True)
    parser.add_argument("--driver", required=True)
    parser.add_argument("--evidence-dir")
    parser.add_argument("--disable-aslr", action="store_true")
    options = parser.parse_args()
    TraceIoTest.probe = runner.require_executable(options.probe)
    TraceIoTest.driver = runner.require_executable(options.driver)
    TraceIoTest.session = runner.EvidenceSession(options.evidence_dir)
    TraceIoTest.prefix = ["setarch", "x86_64", "-R"] if options.disable_aslr else []
    print(f"evidence_directory: {TraceIoTest.session.root}", flush=True)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(TraceIoTest)
    discovered = suite.countTestCases()
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    passed = (result.wasSuccessful() and not result.skipped
              and result.testsRun == discovered and discovered > 0)
    (TraceIoTest.session.root / "test-summary.json").write_text(json.dumps({
        "passed": passed, "discovered": discovered, "executed": result.testsRun,
        "failures": len(result.failures), "errors": len(result.errors),
        "skipped": len(result.skipped),
    }, indent=2) + "\n")
    sys.exit(0 if passed else 1)
