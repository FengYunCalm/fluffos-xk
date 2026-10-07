#!/usr/bin/env python3
"""Verify global include configuration with real, isolated driver startup."""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import unittest

spec = importlib.util.spec_from_file_location(
    "global_include_runner", Path(__file__).with_name("run-targeted.py")
)
runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)


class GlobalIncludeTest(unittest.TestCase):
    driver: Path
    session: runner.EvidenceSession

    def run_include(self, value: str | None, expected: int | None) -> None:
        _root, mudlib = runner.render_sandbox(self.session)
        config_path = mudlib / "etc/config.test"
        config = config_path.read_text()
        settings = {
            "master file": "/single/u06_global_master",
            "simulated efun file": "/single/u06_global_simul",
            "global include file": value,
        }
        for key, setting in settings.items():
            replacement = "" if setting is None else f"{key} : {setting}"
            config, count = re.subn(rf"^{key} : .*?$", replacement, config, flags=re.MULTILINE)
            self.assertEqual(count, 1)
        config_path.write_text(config)
        (mudlib / "include/u06_global.h").write_text("#define U06_GLOBAL_VALUE 271\n")
        config_header = Path(__file__).resolve().parents[2] / "src/include/runtime_config.h"
        (mudlib / "include/u06_runtime_config.h").write_bytes(config_header.read_bytes())
        (mudlib / "single/u06_global_simul.lpc").write_text("int fixture() { return 1; }\n")
        (mudlib / "clone/u06_global_probe.lpc").write_text(
            "int value() {\n#ifdef U06_GLOBAL_VALUE\nreturn U06_GLOBAL_VALUE;\n"
            "#else\nreturn 0;\n#endif\n}\n"
        )
        (mudlib / "single/u06_global_master.lpc").write_text(
            '#include <u06_runtime_config.h>\n'
            'string get_root_uid() { return "Root"; }\n'
            'string get_bb_uid() { return "Backbone"; }\n'
            'string creator_file(string path) { return "Root"; }\n'
            'int valid_read(string path, mixed user, string operation) { return 1; }\n'
            'void flag(string expected) {\n'
            '  object probe = load_object("/clone/u06_global_probe");\n'
            '  if (probe->value() != to_int(expected)) { shutdown(1); return; }\n'
            '  if (expected == "0" && get_config(__GLOBAL_INCLUDE_FILE__) != "") {\n'
            '    shutdown(1); return;\n'
            '  }\n'
            '  write("GLOBAL_INCLUDE_OK:" + expected + "\\n");\n'
            '  shutdown(0);\n}\n'
        )
        result = runner.run_process(
            self.session,
            [str(self.driver), "etc/config.test", f"-f{expected or 0}"],
            cwd=mudlib, environment=runner.command_environment(mudlib), timeout=30,
            label=self._testMethodName, binary=self.driver,
        )
        runner.reject_sanitizer_output(result.output)
        self.assertFalse(result.timed_out, result.log_path)
        self.assertEqual(result.failure_reason, "", result.log_path)
        if expected is None:
            self.assertNotEqual(result.returncode, 0, result.log_path)
            self.assertNotIn("GLOBAL_INCLUDE_OK:", result.output, result.log_path)
            self.assertRegex(result.output, r"Cannot #include|Missing trailing .* in #include",
                             result.log_path)
        else:
            self.assertEqual(result.returncode, 0, result.log_path)
            self.assertEqual(result.output.count(f"GLOBAL_INCLUDE_OK:{expected}"), 1,
                             result.log_path)
            if expected == 0:
                self.assertNotIn("adding quotes", result.output, result.log_path)

    def test_absent(self) -> None:
        self.run_include(None, 0)

    def test_empty(self) -> None:
        self.run_include("", 0)

    def test_whitespace(self) -> None:
        self.run_include("   ", 0)

    def test_quoted(self) -> None:
        self.run_include('"/include/u06_global.h"', 271)

    def test_angle(self) -> None:
        self.run_include("<u06_global.h>", 271)

    def test_unquoted(self) -> None:
        self.run_include("/include/u06_global.h", 271)

    def test_missing_file(self) -> None:
        self.run_include("<u06_missing.h>", None)

    def test_unclosed_quote(self) -> None:
        self.run_include('"/include/u06_global.h', None)

    def test_unclosed_angle(self) -> None:
        self.run_include("<u06_global.h", None)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", required=True)
    parser.add_argument("--evidence-dir")
    options = parser.parse_args()
    GlobalIncludeTest.driver = runner.require_executable(options.driver)
    GlobalIncludeTest.session = runner.EvidenceSession(options.evidence_dir)
    print(f"evidence_directory: {GlobalIncludeTest.session.root}", flush=True)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(GlobalIncludeTest)
    discovered = suite.countTestCases()
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    passed = (result.wasSuccessful() and not result.skipped
              and result.testsRun == discovered and discovered > 0)
    (GlobalIncludeTest.session.root / "test-summary.json").write_text(json.dumps({
        "passed": passed, "discovered": discovered, "executed": result.testsRun,
        "failures": len(result.failures), "errors": len(result.errors),
        "skipped": len(result.skipped),
    }, indent=2) + "\n")
    sys.exit(0 if passed else 1)
