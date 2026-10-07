#!/usr/bin/env python3
"""Exercise fallback reproducibility and drift detection using real Bison."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import regenerate_grammar as grammar


class GrammarTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.root = Path(__file__).resolve().parents[2]
        cls.bison = shutil.which("bison")
        if cls.bison is None:
            raise RuntimeError("GNU Bison 3.8.2 is required")
        cls.generated = grammar.generate(cls.root, cls.bison)

    def test_reproducible_and_current(self) -> None:
        self.assertEqual(self.generated, grammar.generate(self.root, self.bison))
        self.assertEqual([], grammar.update(self.root, self.generated, False))

    def test_check_does_not_write_and_write_repairs(self) -> None:
        with tempfile.TemporaryDirectory(prefix="fluffos-grammar-test-") as directory:
            root = Path(directory)
            for name in grammar.OUTPUTS:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"stale\n")
            self.assertEqual(list(grammar.OUTPUTS), grammar.update(root, self.generated, False))
            for name in grammar.OUTPUTS:
                self.assertEqual(b"stale\n", (root / name).read_bytes())
            self.assertEqual(list(grammar.OUTPUTS), grammar.update(root, self.generated, True))
            self.assertEqual([], grammar.update(root, self.generated, False))

    def test_missing_output_is_drift(self) -> None:
        with tempfile.TemporaryDirectory(prefix="fluffos-grammar-test-") as directory:
            self.assertEqual(list(grammar.OUTPUTS),
                             grammar.update(Path(directory), self.generated, False))

    def test_generator_failure_preserves_outputs(self) -> None:
        with tempfile.TemporaryDirectory(prefix="fluffos-grammar-test-") as directory:
            root = Path(directory)
            for stem in (grammar.GRAMMAR, grammar.MAKE_FUNC):
                path = root / f"{stem}.y"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("invalid grammar\n")
            for name in grammar.OUTPUTS:
                (root / name).write_bytes(b"original\n")
            with self.assertRaises(subprocess.CalledProcessError):
                grammar.generate(root, self.bison)
            for name in grammar.OUTPUTS:
                self.assertEqual(b"original\n", (root / name).read_bytes())

    def test_wrong_version_is_rejected(self) -> None:
        with patch.object(grammar.subprocess, "run") as run:
            run.return_value.stdout = "bison (GNU Bison) 3.7.6\n"
            with self.assertRaisesRegex(ValueError, "Requires GNU Bison 3.8.2"):
                grammar.generate(self.root, self.bison)
            run.assert_called_once()


if __name__ == "__main__":
    unittest.main(verbosity=2)
