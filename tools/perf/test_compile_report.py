#!/usr/bin/env python3
"""Self-tests for compile reports; --benchmark also checks a real executable."""

import argparse
import copy
import hashlib
import importlib.util
import json
import pathlib
import subprocess
import sys
import unittest

sys.dont_write_bytecode = True
REPO = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "compile_report", REPO / "tools/perf/compile_report.py"
)
report_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report_module)
validate_report = report_module.validate_report


def sample_report(rounds=20):
    # Different file costs are not lifecycle degradation. Every round is identical.
    return {
        "schema_version": 2,
        "files": 2,
        "rounds": rounds,
        "corpus": "/private/corpus",
        "corpus_files": [
            {"path": 'a"quoted.c', "bytes": 1, "sha256": "a" * 64},
            {"path": "b.c", "bytes": 2, "sha256": "b" * 64},
        ],
        "warmup_success": [True, True],
        "warmup_failures": 0,
        "successful_compiles": rounds * 2,
        "failures": 0,
        "round_samples": [
            {"seconds": 102.0, "files": [
                {"seconds": 1.0, "success": True},
                {"seconds": 100.0, "success": True},
            ]}
            for _ in range(rounds)
        ],
        "total_secs": 102.0 * rounds,
        "throughput_files_per_s": 2 / 102.0,
        "round_secs": [102.0] * rounds,
        "per_file_secs_last_round": [1.0, 100.0],
        "degradation": 100.0,
        "operation_secs": {
            "sample_count": 2 * rounds, "median": 100.0, "p95": 100.0, "p99": 100.0,
        },
        "lifecycle": None if rounds == 1 else {
            "window_rounds": max(1, rounds // 10),
            "head_mean_secs": 102.0,
            "tail_mean_secs": 102.0,
            "ratio": 1.0,
        },
        "arena": {
            "warmup_chunk_mallocs": 2, "final_chunk_mallocs": 2, "delta": 0,
            "retained_chunks": 2, "retained_heap_bytes": 200,
            "cycle_bytes": 0, "peak_cycle_bytes": 100, "resets": 42,
        },
        "peak_rss_kb": 1000,
    }


class ReportValidationTest(unittest.TestCase):
    def test_heterogeneous_files_have_no_lifecycle_degradation(self):
        validate_report(sample_report())

    def test_one_round_is_not_a_lifecycle_comparison(self):
        validate_report(sample_report(rounds=1))

    def test_missing_sample_is_rejected(self):
        report = sample_report()
        report["round_samples"][1]["files"].pop()
        with self.assertRaises(ValueError):
            validate_report(report)

    def test_missing_fields_are_rejected(self):
        report = sample_report()
        del report["corpus_files"]
        with self.assertRaises(KeyError):
            validate_report(report)

    def test_nonfinite_samples_are_rejected(self):
        for value in (float("inf"), float("nan"), 0.0, -1.0, True):
            with self.subTest(value=value):
                report = sample_report()
                report["round_samples"][1]["files"][0]["seconds"] = value
                with self.assertRaises(ValueError):
                    validate_report(report)

    def test_false_success_and_warmup_failure_are_rejected(self):
        for field, value in (("failures", 1), ("successful_compiles", 39),
                             ("warmup_failures", 1), ("warmup_success", [False, True])):
            with self.subTest(field=field):
                report = sample_report()
                report[field] = value
                with self.assertRaises(ValueError):
                    validate_report(report)

    def test_wrong_summary_and_wrong_lifecycle_are_rejected(self):
        for field in ("total_secs", "throughput_files_per_s", "degradation"):
            with self.subTest(field=field):
                report = sample_report()
                report[field] *= 2
                with self.assertRaises(ValueError):
                    validate_report(report)
        report = sample_report()
        report["lifecycle"]["ratio"] = 100.0
        with self.assertRaises(ValueError):
            validate_report(report)

    def test_time_order_is_not_replaceable_with_sorted_rounds(self):
        report = sample_report(rounds=2)
        report["round_samples"][0]["seconds"] = 204.0
        report["round_secs"] = [102.0, 204.0]
        report["total_secs"] = 306.0
        report["throughput_files_per_s"] = 4 / 306.0
        report["lifecycle"].update(head_mean_secs=204.0, tail_mean_secs=102.0, ratio=0.5)
        validate_report(report)
        report["round_samples"].reverse()
        with self.assertRaises(ValueError):
            validate_report(report)

    def test_boolean_counts_and_duplicate_file_identity_are_rejected(self):
        for field in ("files", "rounds", "failures"):
            report = sample_report()
            report[field] = True
            with self.assertRaises(ValueError):
                validate_report(report)
        report = sample_report()
        report["corpus_files"][1] = copy.deepcopy(report["corpus_files"][0])
        with self.assertRaises(ValueError):
            validate_report(report)


class BenchmarkExecutableTest(unittest.TestCase):
    benchmark = None
    evidence_root = None

    def setUp(self):
        self.root = self.evidence_root / self._testMethodName
        self.root.mkdir()
        self.corpus = self.root / "corpus"
        self.corpus.mkdir()
        (self.corpus / "a.c").write_text("int f() { return 1; }\n", encoding="utf-8")
        (self.corpus / "b.c").write_text(
            "int f() { return 1; }\n"
            + "\n".join(f"int g{i}() {{ return {i}; }}" for i in range(100)) + "\n",
            encoding="utf-8",
        )

    def run_benchmark(self, *, rounds="3", output=None, shell_stdout=False):
        output = output if output is not None else self.root / "report.json"
        command = [str(self.benchmark), "--corpus", str(self.corpus),
                   "--rounds", rounds, "--json", str(output)]
        if shell_stdout:
            # Positional arguments, not interpolation; /dev/full deterministically rejects writes.
            command = ["/bin/sh", "-c", 'exec "$@" >/dev/full', "bench-stdout", *command]
        result = subprocess.run(
            ["/usr/bin/python3", str(REPO / "tools/testsuite/run-targeted.py"),
             "--tool", command[0], "--evidence-dir", str(self.root / "run"),
             "--timeout", "20", "--",
             *command[1:]],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False,
        )
        (self.root / "runner.log").write_bytes(result.stdout)
        records = json.loads((self.root / "run" / "records.json").read_text())
        self.assertEqual(len(records), 1, "runner must actually execute the benchmark")
        record = records[0]
        self.assertFalse(record["timed_out"])
        self.assertEqual(record["failure_reason"], "")
        self.assertEqual(record["returncode"], record["process_returncode"])
        self.assertEqual(record["inputs_before"], record["inputs_after"])
        self.assertEqual(record["binary_sha256_before"], record["binary_sha256"])
        self.assertEqual(result.returncode == 0, record["process_returncode"] == 0)
        return record["process_returncode"]

    def test_valid_report_preserves_samples(self):
        self.assertEqual(self.run_benchmark(rounds="20"), 0)
        report = json.loads((self.root / "report.json").read_text())
        validate_report(report)
        for identity in report["corpus_files"]:
            data = pathlib.Path(identity["path"]).read_bytes()
            self.assertEqual(identity["bytes"], len(data))
            self.assertEqual(identity["sha256"], hashlib.sha256(data).hexdigest())

    def test_json_escaping_and_single_round(self):
        quoted = self.root / 'corpus"quoted'
        self.corpus.rename(quoted)
        self.corpus = quoted
        (self.corpus / "a.c").rename(self.corpus / 'a"quoted.c')
        self.assertEqual(self.run_benchmark(rounds="1"), 0)
        validate_report(json.loads((self.root / "report.json").read_text()))

    def test_compile_failure_is_not_a_success_report(self):
        (self.corpus / "bad.c").write_text("int broken( {\n", encoding="utf-8")
        self.assertEqual(self.run_benchmark(), 1)
        report = json.loads((self.root / "report.json").read_text())
        self.assertEqual(report["warmup_failures"], 1)
        self.assertEqual(report["failures"], 3)
        self.assertEqual(report["successful_compiles"], 6)
        with self.assertRaises(ValueError):
            validate_report(report)

    def test_open_failure_is_not_success(self):
        self.assertEqual(self.run_benchmark(output=self.root), 2)

    @unittest.skipUnless(pathlib.Path("/dev/full").exists(), "requires /dev/full")
    def test_flush_or_close_failure_is_not_success(self):
        self.assertEqual(self.run_benchmark(output=pathlib.Path("/dev/full")), 2)

    @unittest.skipUnless(pathlib.Path("/dev/full").exists(), "requires /dev/full")
    def test_stdout_failure_is_not_success(self):
        self.assertEqual(self.run_benchmark(shell_stdout=True), 2)

    def test_invalid_round_count_is_not_silently_clamped(self):
        self.assertEqual(self.run_benchmark(rounds="not-a-number"), 2)

    def test_output_alias_does_not_overwrite_corpus(self):
        source = self.corpus / "a.c"
        original = source.read_bytes()
        self.assertEqual(self.run_benchmark(output=source), 2)
        self.assertEqual(source.read_bytes(), original)

    def test_hardlink_output_alias_is_rejected(self):
        source = self.corpus / "a.c"
        original = source.read_bytes()
        alias = self.root / "alias.json"
        alias.hardlink_to(source)
        self.assertEqual(self.run_benchmark(output=alias), 2)
        self.assertEqual(source.read_bytes(), original)

    def test_symlink_output_alias_is_rejected(self):
        source = self.corpus / "a.c"
        original = source.read_bytes()
        alias = self.root / "alias.json"
        alias.symlink_to(source)
        self.assertEqual(self.run_benchmark(output=alias), 2)
        self.assertEqual(source.read_bytes(), original)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmark", type=pathlib.Path)
    parser.add_argument("--evidence-root", type=pathlib.Path)
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ReportValidationTest)
    if args.benchmark:
        if not args.evidence_root:
            parser.error("--benchmark requires --evidence-root (must not already exist)")
        args.evidence_root.mkdir(mode=0o700)
        BenchmarkExecutableTest.benchmark = args.benchmark.resolve(strict=True)
        BenchmarkExecutableTest.evidence_root = args.evidence_root.resolve(strict=True)
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(BenchmarkExecutableTest))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(not result.wasSuccessful())
