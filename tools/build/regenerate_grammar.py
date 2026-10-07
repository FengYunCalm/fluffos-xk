#!/usr/bin/env python3
"""Check or regenerate Bison fallbacks; both modes write private temporary files."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

BISON_VERSION = "3.8.2"
GRAMMAR = "src/compiler/internal/grammar"
MAKE_FUNC = "src/tools/make_func"
OUTPUTS = (f"{GRAMMAR}.autogen.cc", f"{GRAMMAR}.autogen.h", f"{MAKE_FUNC}.autogen.cc")


def generate(root: Path, bison: str) -> dict[str, bytes]:
    version = subprocess.run([bison, "--version"], check=True, capture_output=True,
                             text=True).stdout.splitlines()[0]
    if version != f"bison (GNU Bison) {BISON_VERSION}":
        raise ValueError(f"Requires GNU Bison {BISON_VERSION}; found {version}")
    with tempfile.TemporaryDirectory(prefix="fluffos-grammar-") as directory:
        work = Path(directory)
        for stem in (GRAMMAR, MAKE_FUNC):
            destination = work / f"{stem}.y"
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes((root / f"{stem}.y").read_bytes())
        commands = (
            [bison, "-Wall", "--color=never", "-rall", "--warnings=none",
             f"--defines={GRAMMAR}.autogen.h", f"--output={GRAMMAR}.autogen.cc",
             f"{GRAMMAR}.y"],
            [bison, f"--output={MAKE_FUNC}.autogen.cc", f"{MAKE_FUNC}.y"],
        )
        for command in commands:
            subprocess.run(command, cwd=work, check=True, capture_output=True)
        return {name: (work / name).read_bytes() for name in OUTPUTS}


def update(root: Path, generated: dict[str, bytes], write: bool) -> list[str]:
    different = [name for name, data in generated.items()
                 if not (root / name).exists() or (root / name).read_bytes() != data]
    if write:
        for name in different:
            (root / name).write_bytes(generated[name])
    return different


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    parser.add_argument("--bison", default="bison")
    options = parser.parse_args()
    bison = shutil.which(options.bison)
    if bison is None:
        parser.error(f"Bison executable not found: {options.bison}")
    bison = str(Path(bison).resolve())
    root = Path(__file__).resolve().parents[2]
    try:
        different = update(root, generate(root, bison), options.write)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            sys.stderr.buffer.write(error.stderr if isinstance(error.stderr, bytes)
                                    else error.stderr.encode())
        return 1
    for name in different:
        print(f"{'Updated' if options.write else 'Out of date'}: {name}")
    if options.check and different:
        return 1
    print(f"Verified {len(OUTPUTS)} fallbacks with GNU Bison {BISON_VERSION}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
