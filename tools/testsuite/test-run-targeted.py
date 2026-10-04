#!/usr/bin/env python3
"""Self-tests for run-targeted.py's isolation and result contracts."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
RUNNER = REPO_ROOT / "tools" / "testsuite" / "run-targeted.py"
SOURCE_SENTINEL = REPO_ROOT / "testsuite" / "etc" / "config.test"


class TargetedRunnerTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = Path(tempfile.mkdtemp(prefix="fluffos-targeted-selftest-"))
        self.fake = self.temp / "fake-gtest.py"
        self.fake.write_text(
            textwrap.dedent(
                """\
                #!/usr/bin/env python3
                import os
                import subprocess
                import sys
                import time

                if '--gtest_list_tests' in sys.argv:
                    if any(argument.endswith('Missing.*') for argument in sys.argv):
                        sys.exit(0)
                    print('FakeSuite.')
                    print('  Works')
                    sys.exit(0)
                if any(argument.startswith('-ftest') for argument in sys.argv):
                    if os.environ.get('FAKE_DRIVER_MODE') == 'check-fail':
                        print('Check failed: synthetic assertion')
                    print('Checks succeeded.')
                    sys.exit(0)
                mode = os.environ.get('FAKE_GTEST_MODE', 'pass')
                if mode == 'timeout':
                    pid_file = os.environ['FAKE_CHILD_PID']
                    child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])
                    open(pid_file, 'w', encoding='ascii').write(str(child.pid))
                    time.sleep(60)
                print('[ RUN      ] FakeSuite.Works')
                if mode == 'skip':
                    print('[  SKIPPED ] FakeSuite.Works')
                    print('[  SKIPPED ] 1 test.')
                    sys.exit(0)
                if mode == 'fail':
                    print('[  FAILED  ] FakeSuite.Works')
                    print('[  FAILED  ] 1 test.')
                    sys.exit(7)
                print('[       OK ] FakeSuite.Works')
                print('[  PASSED  ] 1 test.')
                """
            ),
            encoding="utf-8",
        )
        self.fake.chmod(0o755)
        self.before_hash = hashlib.sha256(SOURCE_SENTINEL.read_bytes()).hexdigest()

    def tearDown(self) -> None:
        shutil.rmtree(self.temp, ignore_errors=True)

    def run_runner(self, *arguments: str, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        if env:
            environment.update(env)
        return subprocess.run(
            [sys.executable, str(RUNNER), *arguments],
            cwd=REPO_ROOT,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )

    @staticmethod
    def evidence_path(output: str) -> Path:
        match = re.search(r"evidence_directory: (.+)", output)
        if not match:
            raise AssertionError(output)
        return Path(match.group(1).strip())

    def test_invalid_cli_and_zero_filter_are_rejected(self) -> None:
        invalid = self.run_runner("--binary", str(self.fake))
        self.assertEqual(invalid.returncode, 2, invalid.stdout)

        zero = self.run_runner(
            "--binary", str(self.fake), "--gtest-filter", "Missing.*", "--timeout", "10"
        )
        self.assertEqual(zero.returncode, 1, zero.stdout)
        self.assertIn("selected zero tests", zero.stdout)

    def test_pass_and_repeated_runs_have_distinct_evidence_without_source_writes(self) -> None:
        first = self.run_runner(
            "--binary", str(self.fake), "--gtest-filter", "FakeSuite.Works", "--timeout", "10"
        )
        second = self.run_runner(
            "--binary", str(self.fake), "--gtest-filter", "FakeSuite.Works", "--timeout", "10"
        )
        self.assertEqual(first.returncode, 0, first.stdout)
        self.assertEqual(second.returncode, 0, second.stdout)
        first_evidence = self.evidence_path(first.stdout)
        second_evidence = self.evidence_path(second.stdout)
        self.assertNotEqual(first_evidence, second_evidence)
        self.assertTrue((first_evidence / "summary.json").is_file())
        self.assertTrue((second_evidence / "summary.json").is_file())
        self.assertEqual(
            hashlib.sha256(SOURCE_SENTINEL.read_bytes()).hexdigest(), self.before_hash
        )
        shutil.rmtree(first_evidence, ignore_errors=True)
        shutil.rmtree(second_evidence, ignore_errors=True)

    def test_lpc_assertion_failures_are_not_pass(self) -> None:
        result = self.run_runner(
            "--driver",
            str(self.fake),
            "--all-lpc",
            "--timeout",
            "10",
            env={"FAKE_DRIVER_MODE": "check-fail"},
        )
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("assertion failure marker", result.stdout)
        evidence = self.evidence_path(result.stdout)
        shutil.rmtree(evidence, ignore_errors=True)

    def test_nonzero_and_all_skipped_are_not_pass(self) -> None:
        for mode in ("fail", "skip"):
            result = self.run_runner(
                "--binary",
                str(self.fake),
                "--gtest-filter",
                "FakeSuite.Works",
                "--timeout",
                "10",
                env={"FAKE_GTEST_MODE": mode},
            )
            self.assertEqual(result.returncode, 1, result.stdout)
            evidence = self.evidence_path(result.stdout)
            shutil.rmtree(evidence, ignore_errors=True)

    @unittest.skipIf(os.name == "nt", "the Unix process-group assertion is platform-specific")
    def test_timeout_terminates_child_process_group(self) -> None:
        pid_file = self.temp / "child.pid"
        result = self.run_runner(
            "--binary",
            str(self.fake),
            "--gtest-filter",
            "FakeSuite.Works",
            "--timeout",
            "1",
            env={"FAKE_GTEST_MODE": "timeout", "FAKE_CHILD_PID": str(pid_file)},
        )
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("timed", result.stdout.lower())
        evidence = self.evidence_path(result.stdout)
        shutil.rmtree(evidence, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
