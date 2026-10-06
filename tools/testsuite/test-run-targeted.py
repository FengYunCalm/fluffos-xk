#!/usr/bin/env python3
"""Self-tests for run-targeted.py's isolation and result contracts."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import select
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import textwrap
import time
import unittest
from unittest import mock


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
                from pathlib import Path

                def setting(name, default=""):
                    path = Path(__file__).with_name(name)
                    return path.read_text() if path.exists() else default

                if '--gtest_list_tests' in sys.argv:
                    if any(argument.endswith('Missing.*') for argument in sys.argv):
                        sys.exit(0)
                    print('FakeSuite.')
                    print('  Works')
                    sys.exit(0)
                if any(argument.startswith('-ftest') for argument in sys.argv):
                    if setting('driver-mode') == 'check-fail':
                        print('Check failed: synthetic assertion')
                    print('Checks succeeded.')
                    sys.exit(0)
                mode = setting('gtest-mode', 'pass')
                if mode == 'timeout':
                    pid_file = setting('child-pid-path')
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
            names = {"FAKE_DRIVER_MODE": "driver-mode", "FAKE_GTEST_MODE": "gtest-mode",
                     "FAKE_CHILD_PID": "child-pid-path"}
            for key, filename in names.items():
                if key in env:
                    (self.temp / filename).write_text(env[key], encoding="utf-8")
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


def load_runner():
    spec = importlib.util.spec_from_file_location("targeted_runner", RUNNER)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


runner = load_runner()


class ResultContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="fluffos-result-contract-")
        self.root = Path(self.temp.name)
        self.binary = self.root / "fake"
        self.binary.write_text("fixture\n", encoding="ascii")
        (self.root / "etc").mkdir()
        (self.root / "etc" / "test-scopes.tsv").write_text("# unit fixtures\n")
        self.session = runner.EvidenceSession(str(self.root / "evidence"))
        self.addCleanup(self.temp.cleanup)

    def result(self, output, returncode=0, timed_out=False):
        return runner.ProcessResult(
            [str(self.binary)], returncode, timed_out, 0.01, output, self.root / "output.log"
        )

    def test_gtest_uses_its_compiled_source_tree(self) -> None:
        source = self.root / "compiled-source"
        with mock.patch.object(runner, "find_build_roots", return_value=(self.root, source)), \
                mock.patch.object(runner, "render_sandbox", return_value=(self.root, self.root)) as render, \
                mock.patch.object(runner, "run_process", side_effect=[
                    self.result("Suite.\n  First\n"),
                    self.result("[ RUN      ] Suite.First\n[       OK ] Suite.First\n[  PASSED  ] 1 test.\n")
                ]):
            runner.run_binary(self.session, self.binary, "*", 10)
        render.assert_called_once_with(self.session, native_source=source / "src")

    def test_native_source_snapshot_uses_selected_tree(self) -> None:
        source = self.root / "compiled-source"
        source.mkdir()
        (source / "sentinel.cc").write_bytes(b"compiled input\r\n")
        sandbox, _mudlib = runner.render_sandbox(self.session, native_source=source)
        self.assertEqual((sandbox / "src" / "sentinel.cc").read_bytes(), b"compiled input\r\n")
        self.assertEqual(runner.source_manifest(source), runner.source_manifest(sandbox / "src"))

    def run_gtest(self, listing, output):
        with mock.patch.object(runner, "render_sandbox", return_value=(self.root, self.root)), \
                mock.patch.object(runner, "run_process", side_effect=[
                    self.result(listing), self.result(output)
                ]):
            return runner.run_binary(self.session, self.binary, "*", 10)

    def test_gtest_rejects_incomplete_duplicate_and_inconsistent_results(self) -> None:
        listing = "Suite.\n  First\n  Second\n"
        first = "[ RUN      ] Suite.First\n[       OK ] Suite.First (0 ms)\n"
        second = "[ RUN      ] Suite.Second\n[       OK ] Suite.Second (0 ms)\n"
        invalid = {
            "missing": first + "[  PASSED  ] 1 test.\n",
            "duplicate": first + first + "[  PASSED  ] 2 tests.\n",
            "wrong_identity": first + second.replace("Second", "Other") + "[  PASSED  ] 2 tests.\n",
            "wrong_count": first + second + "[  PASSED  ] 1 test.\n",
            "missing_completion": first + "[ RUN      ] Suite.Second\n[  PASSED  ] 2 tests.\n",
            "partial_skip": first + "[ RUN      ] Suite.Second\n"
                            "[  SKIPPED ] Suite.Second (0 ms)\n"
                            "[  PASSED  ] 1 test.\n[  SKIPPED ] 1 test.\n",
        }
        for name, output in invalid.items():
            with self.subTest(name=name), self.assertRaises(runner.RunnerError):
                self.run_gtest(listing, output)

    def test_gtest_classifies_disabled_before_execution(self) -> None:
        listing = "Suite.\n  First\n  DISABLED_Second\nDISABLED_Other.\n  Third\n"
        output = "[ RUN      ] Suite.First\n[       OK ] Suite.First (0 ms)\n[  PASSED  ] 1 test.\n"
        summary = self.run_gtest(listing, output)
        self.assertEqual(summary.selected, 1)
        self.assertEqual(summary.passed, 1)

    def test_gtest_rejects_discovery_sanitizer_diagnostic(self) -> None:
        with self.assertRaises(runner.RunnerError):
            self.run_gtest(
                "runtime error: synthetic discovery error\nSuite.\n  First\n",
                "[ RUN      ] Suite.First\n[       OK ] Suite.First\n[  PASSED  ] 1 test.\n",
            )

    def test_ctest_rejects_partial_skips_empty_and_failed_not_run(self) -> None:
        cases = (
            [self.result("1/1 Test #1: First ... ***Not Run\n", 8),
             self.result("1/1 Test #2: Second ... Passed\n")],
            [self.result("1/1 Test #1: First ... ***Skipped\n"),
             self.result("1/1 Test #2: Second ... Passed\n")],
            [self.result(""), self.result("")],
        )
        for results in cases:
            with self.subTest(outputs=[r.output for r in results]), \
                    mock.patch.object(runner, "ctest_tests", return_value=["First", "Second"]), \
                    mock.patch.object(runner, "render_sandbox", return_value=(self.root, self.root)), \
                    mock.patch.object(runner, "run_process", side_effect=results), \
                    self.assertRaises(runner.RunnerError):
                runner.run_ctest(self.session, self.root, ".*", 10)

    def test_lpc_completion_requires_matching_asserted_case(self) -> None:
        directory = self.root / "single" / "tests" / "efuns"
        directory.mkdir(parents=True)
        (directory / "example.lpc").write_text("void do_tests() { ASSERT(1); }\n")
        name = "/single/tests/efuns/example.lpc"
        good = f"C> {name}\nD> C {name} asserts=1\nChecks succeeded.\n"
        outputs = {
            "pass": good,
            "console_pass": "".join("]" + line for line in good.splitlines(keepends=True)),
            "duplicate": good + f"D> C {name} asserts=1\n",
            "unknown": good.replace("D> C " + name, "D> C /unknown"),
            "empty_case": good.replace("asserts=1", "asserts=0"),
            "late_failure": good + "T> fail late callback\n",
        }
        for mode, output in outputs.items():
            with self.subTest(mode=mode), \
                    mock.patch.object(runner, "render_sandbox", return_value=(self.root, self.root)), \
                    mock.patch.object(runner, "run_process", return_value=self.result(output)):
                if mode in ("pass", "console_pass"):
                    result = runner.run_driver(self.session, self.binary, None, True, 10, "config.test")
                    self.assertEqual(result.passed, 1)
                else:
                    with self.assertRaises(runner.RunnerError):
                        runner.run_driver(self.session, self.binary, None, True, 10, "config.test")

    def test_environment_excludes_credentials_and_ambient_gtest_selection(self) -> None:
        with mock.patch.dict(os.environ, {
            "UNRELATED_CREDENTIAL": "test-only-value", "GTEST_FILTER": "Nothing.*",
            "GTEST_TOTAL_SHARDS": "2", "LANG": "C.UTF-8",
        }, clear=True):
            environment = runner.command_environment(self.root)
        self.assertFalse("UNRELATED_CREDENTIAL" in environment)
        self.assertFalse("GTEST_FILTER" in environment)
        self.assertFalse("GTEST_TOTAL_SHARDS" in environment)
        self.assertEqual(environment["LANG"], "C.UTF-8")
        self.assertEqual(environment["FLUFFOS_TEST_MUDLIB"], str(self.root))

    def test_lpc_scope_selection_requires_declared_capabilities_and_exclusions(self) -> None:
        directory = self.root / "single" / "tests" / "efuns"
        directory.mkdir(parents=True)
        for name in ("good", "helper", "sqlite"):
            (directory / (name + ".c")).write_text("// parser fixture\n")
        (self.root / "etc" / "test-scopes.tsv").write_text(
            "/single/tests/efuns/helper\tfixture\t-\tHelper only.\n"
            "/single/tests/efuns/sqlite\truntime\tsqlite\tSQLite required.\n"
        )
        scopes = runner.lpc_scopes(self.root)
        discovered = runner.lpc_test_cases(self.root)
        output = (
            "T> cap sqlite=0\nC> /single/tests/efuns/good.c\n"
            "X> /single/tests/efuns/helper.c\tfixture\tHelper only.\n"
            "X> /single/tests/efuns/sqlite.c\tunavailable\tSQLite required.\n"
        )
        required, excluded, caps = runner.lpc_selection(discovered, scopes, output)
        self.assertEqual(required, {("C", "/single/tests/efuns/good.c")})
        self.assertEqual(len(excluded), 2)
        self.assertEqual(caps, {"sqlite": 0})
        for bad in (
            output.replace("T> cap sqlite=0\n", ""),
            output + "T> cap sqlite=0\n",
            output.replace("T> cap sqlite=0\n", "") + "T> cap sqlite=0\n",
            output.replace("Helper only.", "Unregistered waiver."),
            output.replace("X> /single/tests/efuns/helper.c\tfixture\tHelper only.\n", ""),
        ):
            with self.subTest(output=bad), self.assertRaises(runner.RunnerError):
                runner.lpc_selection(discovered, scopes, bad)

    def test_lpc_scope_manifest_rejects_duplicates_and_unknown_roles(self) -> None:
        directory = self.root / "single" / "tests" / "efuns"
        directory.mkdir(parents=True)
        (directory / "helper.c").write_text("// parser fixture\n")
        row = "/single/tests/efuns/helper\tfixture\t-\tHelper only.\n"
        for bad in (row + row, row.replace("fixture", "pass_without_assertions")):
            (self.root / "etc" / "test-scopes.tsv").write_text(bad)
            with self.subTest(row=bad), self.assertRaises(runner.RunnerError):
                runner.lpc_scopes(self.root)

    def test_lpc_rejects_success_without_completion(self) -> None:
        directory = self.root / "single" / "tests" / "efuns"
        directory.mkdir(parents=True)
        (directory / "example.c").write_text("// discovery fixture\n")
        for output in ("Checks succeeded.\n", "C> /single/tests/efuns/example\nChecks succeeded.\n"):
            with self.subTest(output=output), \
                    mock.patch.object(runner, "render_sandbox", return_value=(self.root, self.root)), \
                    mock.patch.object(runner, "run_process", return_value=self.result(output)), \
                    self.assertRaises(runner.RunnerError):
                runner.run_driver(self.session, self.binary, None, True, 10, "config.test")


class ProcessContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="fluffos-process-contract-"))
        self.session = runner.EvidenceSession(str(self.root / "evidence"))

    def run_python(self, source, timeout=2):
        return runner.run_process(
            self.session, [sys.executable, "-c", source], cwd=self.root,
            environment=os.environ.copy(), timeout=timeout, label="fixture",
            binary=Path(sys.executable),
        )

    def test_log_preserves_raw_bytes(self) -> None:
        result = self.run_python("import os; os.write(1, b'raw\\xff\\x00\\n')")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.log_path.read_bytes(), b"raw\xff\x00\n")

    def test_runtime_environment_boundary(self) -> None:
        result = runner.run_process(
            self.session,
            [sys.executable, "-c", "import os; print(os.getenv('FAKE_API_TOKEN'), "
             "os.getenv('GTEST_TOTAL_SHARDS'), os.getenv('LANG'), os.getenv('TZ'))"],
            cwd=self.root, timeout=2, label="environment-boundary",
            environment={"FAKE_API_TOKEN": "controlled-canary", "GTEST_TOTAL_SHARDS": "4",
                         "LANG": "C", "TZ": "UTC"},
        )
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.output, "None None C UTC\n")

    def test_materialized_input_identity(self) -> None:
        _root, mudlib = self.session.sandbox()
        (mudlib / "etc").mkdir(parents=True)
        data = b"raw\xff\r\n"
        (mudlib / "etc" / "config.test").write_bytes(data)
        digest = self.session.capture_launch(
            {"FLUFFOS_TEST_MUDLIB": str(mudlib), "FAKE_API_TOKEN": "controlled-canary"}
        )
        record = (self.session.root / f"inputs-{digest}.json").read_text()
        identity = json.loads(record)
        self.assertEqual(identity["materialized_mudlib"]["etc/config.test"][2],
                         hashlib.sha256(data).hexdigest())
        self.assertNotIn("controlled-canary", record)
        (mudlib / "etc" / "config.test").write_bytes(b"changed")
        self.assertNotEqual(digest, self.session.capture_launch(
            {"FLUFFOS_TEST_MUDLIB": str(mudlib)}))
        with self.assertRaises(runner.RunnerError):
            self.session.capture_launch({"FLUFFOS_TEST_MUDLIB": str(self.root)})

    def test_runtime_library_identity_changes_are_not_pass(self) -> None:
        library = self.root / "owned-library.fixture"
        library.write_bytes(b"before")
        with mock.patch.object(runner, "runtime_libraries", side_effect=lambda *_args: {
            "libraries": {str(library): runner.file_identity(library)}
        }):
            result = self.run_python(
                f"from pathlib import Path; Path({str(library)!r}).write_bytes(b'after')"
            )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotEqual(result.inputs_before, result.inputs_after)

    def test_output_budget_is_a_failure(self) -> None:
        with mock.patch.object(runner, "MAX_LOG_BYTES", 1024, create=True):
            result = self.run_python("import os; os.write(1, b'x' * 4096)")
        self.assertNotEqual(result.returncode, 0)
        self.assertLessEqual(result.log_path.stat().st_size, 1024)

    def test_binary_identity_changes_are_not_pass(self) -> None:
        executable = self.root / "changing-tool.py"
        executable.write_text(
            f"#!{sys.executable}\nfrom pathlib import Path\n"
            "Path(__file__).write_text('changed\\n')\nprint('ran original tool')\n"
        )
        executable.chmod(0o755)
        before = hashlib.sha256(executable.read_bytes()).hexdigest()
        result = runner.run_process(
            self.session, [str(executable)], cwd=self.root, environment={}, timeout=2,
            label="changing-binary", binary=executable,
        )
        self.assertNotEqual(result.returncode, 0)
        records = json.loads((self.session.root / "records.json").read_text())
        self.assertEqual(records[0]["binary_sha256_before"], before)
        self.assertNotEqual(records[0]["binary_sha256_before"], records[0]["binary_sha256"])

    def test_record_write_failure_preserves_process_log(self) -> None:
        (self.session.root / "records.json").mkdir()
        with self.assertRaises(OSError):
            self.run_python("print('record-write-fixture')")
        logs = list(self.session.root.glob("*.log"))
        self.assertEqual(len(logs), 1)
        self.assertEqual(logs[0].read_bytes(), b"record-write-fixture\n")

    @unittest.skipUnless(sys.platform.startswith("linux"), "requires Linux pidfd and subreaping")
    def test_exited_parent_with_pipe_holding_child_is_bounded(self) -> None:
        for detached in (False, True):
            with self.subTest(detached=detached):
                pid_file = self.root / f"child-{detached}.pid"
                child_source = (
                    "import os, signal, time; "
                    + ("os.setsid(); " if detached else "")
                    + "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                    + f"open({str(pid_file)!r}, 'w').write(str(os.getpid())); "
                    + "time.sleep(60)"
                )
                parent_source = (
                    "import subprocess, sys, time; "
                    f"subprocess.Popen([sys.executable, '-c', {child_source!r}]); "
                    "time.sleep(0.1)"
                )
                launcher = (
                    "import importlib.util, sys; from pathlib import Path; "
                    f"s=importlib.util.spec_from_file_location('r', {str(RUNNER)!r}); "
                    "m=importlib.util.module_from_spec(s); sys.modules['r']=m; s.loader.exec_module(m); "
                    f"e=m.EvidenceSession({str(self.root / f'bounded-{detached}')!r}); "
                    f"r=m.run_process(e,[sys.executable,'-c',{parent_source!r}],"
                    f"cwd=Path({str(self.root)!r}),environment={{}},timeout=1,label='inherited-pipe'); "
                    "assert r.returncode != 0; print('bounded cleanup')"
                )
                descriptor = None
                process = subprocess.Popen(
                    [sys.executable, "-c", launcher], text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                    env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
                )
                try:
                    deadline = time.monotonic() + 5
                    while not pid_file.is_file() and time.monotonic() < deadline:
                        time.sleep(0.01)
                    self.assertTrue(pid_file.is_file(), "fixture did not reach its child")
                    pid = int(pid_file.read_text())
                    try:
                        descriptor = os.pidfd_open(pid)
                        # A private, unique argv marker prevents signalling a reused PID.
                        if str(pid_file).encode() not in Path(f"/proc/{pid}/cmdline").read_bytes():
                            os.close(descriptor)
                            descriptor = None
                    except ProcessLookupError:
                        pass
                    except FileNotFoundError:
                        if descriptor is not None:
                            os.close(descriptor)
                            descriptor = None
                    output, errors = process.communicate(timeout=10)
                    self.assertEqual(process.returncode, 0, output + errors)
                    self.assertIn("bounded cleanup", output)
                    if descriptor is not None:
                        self.assertTrue(select.select([descriptor], [], [], 0)[0],
                                        "owned descendant survived cleanup")
                finally:
                    if descriptor is not None:
                        try:
                            signal.pidfd_send_signal(descriptor, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                        os.close(descriptor)
                    if process.poll() is None:
                        process.send_signal(signal.SIGINT)
                        try:
                            process.communicate(timeout=3)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.communicate(timeout=3)
                    process.stdout.close()
                    process.stderr.close()


def run_live_completion_tests(driver: Path, evidence_dir: str | None) -> None:
    session = runner.EvidenceSession(evidence_dir)
    cases = (
        ("/single/tests/efuns/package_trim", 30, "", "D> C /single/tests/efuns/package_trim"),
        ("/single/tests/efuns/async", 30, "", "D> C /single/tests/efuns/async"),
        ("/single/tests/efuns/async_promise", 30, "", "D> C /single/tests/efuns/async_promise"),
        ("/single/tests/efuns/socket_tls_server", 30, "", "D> C /single/tests/efuns/socket_tls_server"),
        ("/clone/completion_late_failure", 10, "LPC driver failed", "probe: late callback reached"),
        ("/clone/completion_never", 2, "LPC driver failed", "probe: waiting with registered token"),
        ("/clone/completion_duplicate", 10, "LPC driver failed", "unknown or repeated async completion"),
        ("/clone/completion_unknown", 10, "LPC driver failed", "unknown or repeated async completion"),
    )
    results = []
    for case, timeout, expected_error, marker in cases:
        error_text = ""
        try:
            runner.run_driver(session, driver, case, False, timeout, "config.test")
        except runner.RunnerError as error:
            error_text = str(error)
        record = session.records[-1]
        output = Path(record["log"]).read_text(encoding="utf-8", errors="replace")
        if ((expected_error and expected_error not in error_text)
                or (not expected_error and error_text) or marker not in output):
            raise AssertionError(f"completion contract failed: {case}; inspect {record['log']}")
        if case.endswith("completion_never") and not record["timed_out"]:
            raise AssertionError("missing completion did not reach its timeout")
        results.append({"case": case, "expected_rejection": bool(expected_error), "verified": True})
    report = {"verified": len(results), "failed": 0, "cases": results}
    (session.root / "live-completion.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    print(f"evidence_directory: {session.root}")


if __name__ == "__main__":
    if "--live-driver" in sys.argv:
        parser = argparse.ArgumentParser(description="Exercise the real driver completion protocol")
        parser.add_argument("--live-driver", required=True)
        parser.add_argument("--evidence-dir")
        options = parser.parse_args()
        run_live_completion_tests(runner.require_executable(options.live_driver), options.evidence_dir)
    else:
        unittest.main()
