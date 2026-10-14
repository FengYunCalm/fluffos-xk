#!/usr/bin/env python3
"""Offline regression tests for the manifest-driven SBOM generator."""

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
GENERATOR_PATH = ROOT / "tools/sbom-generate.py"


def load_generator():
    spec = importlib.util.spec_from_file_location("sbom_generate", GENERATOR_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


SBOM = load_generator()


class SbomGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name) / "fixture"
        self.source = self.root / "src/thirdparty/demo"
        self.source.mkdir(parents=True)
        (self.source / "demo.h").write_text("#define DEMO_VERSION 1\n", encoding="utf-8")
        (self.root / "third_party").mkdir()
        self.manifest_path = self.root / "third_party/manifest.yaml"
        self.output_path = self.root / "third_party/sbom.json"
        self.write_manifest(version="1.0")

    def tearDown(self):
        self.tempdir.cleanup()

    def manifest(self):
        return {
            "schema": SBOM.SCHEMA,
            "last_reviewed": "2026-10-11",
            "vendored": [
                {
                    "name": "demo",
                    "path": "src/thirdparty/demo",
                    "version": "1.0",
                    "source_url": "https://github.com/example/demo",
                    "source_revision": "unverified",
                    "source_revision_status": "unverified",
                    "source_tree_sha256": SBOM.source_tree_sha256(self.source),
                    "license": "MIT",
                    "patches": {
                        "status": "unverified",
                        "detail": "Fixture source revision is intentionally unavailable.",
                    },
                    "notes": "Fixture dependency.",
                }
            ],
        }

    def write_manifest(self, version=None):
        manifest = self.manifest()
        if version is not None:
            manifest["vendored"][0]["version"] = version
        self.manifest_path.write_text(
            yaml.safe_dump(manifest, sort_keys=False), encoding="utf-8"
        )

    def run_generator(self, *args):
        return subprocess.run(
            [
                sys.executable,
                "-B",
                str(GENERATOR_PATH),
                "--manifest",
                str(self.manifest_path),
                "--out",
                str(self.output_path),
                *args,
            ],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )

    def test_wrong_version_makes_committed_check_fail_without_overwrite(self):
        generated = self.run_generator()
        self.assertEqual(generated.returncode, 0, generated.stderr)
        original = self.output_path.read_bytes()
        self.write_manifest(version="2.0")

        checked = self.run_generator("--check")
        self.assertNotEqual(checked.returncode, 0)
        self.assertIn("stale", checked.stderr)
        self.assertEqual(original, self.output_path.read_bytes())

    def test_missing_github_owner_or_repository_does_not_fabricate_purl(self):
        dependency = self.manifest()["vendored"][0]
        dependency["source_url"] = "https://github.com/example"
        self.assertIsNone(SBOM.make_purl(dependency))

    def test_source_patch_drift_is_rejected(self):
        manifest = self.manifest()
        self.assertEqual(SBOM.validate_manifest(manifest, self.root), [])
        (self.source / "demo.h").write_text("changed\n", encoding="utf-8")
        errors = SBOM.validate_manifest(manifest, self.root)
        self.assertTrue(any("source drift" in error for error in errors), errors)

    def test_unknown_patch_status_is_not_treated_as_none(self):
        manifest = self.manifest()
        manifest["vendored"][0]["patches"] = "none"
        errors = SBOM.validate_manifest(manifest, self.root)
        self.assertTrue(any("patches must declare" in error for error in errors), errors)

    def test_committed_sbom_staleness_is_rejected(self):
        generated = self.run_generator()
        self.assertEqual(generated.returncode, 0, generated.stderr)
        original = self.output_path.read_bytes()
        data = json.loads(original)
        data["components"][0]["version"] = "tampered"
        self.output_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")

        checked = self.run_generator("--check")
        self.assertNotEqual(checked.returncode, 0)
        self.assertIn("stale", checked.stderr)

    def test_repeated_generation_is_byte_stable(self):
        first = self.run_generator("--validate")
        self.assertEqual(first.returncode, 0, first.stderr + first.stdout)
        first_bytes = self.output_path.read_bytes()
        second = self.run_generator("--validate")
        self.assertEqual(second.returncode, 0, second.stderr + second.stdout)
        self.assertEqual(first_bytes, self.output_path.read_bytes())


if __name__ == "__main__":
    unittest.main()
