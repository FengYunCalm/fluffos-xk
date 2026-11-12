#!/usr/bin/env python3
"""Diff-scoped clang-tidy gate.

Runs the trial check set from src/.clang-tidy on the lines a change touches, so
existing findings elsewhere do not block unrelated work. Needs a build directory
with compile_commands.json and clang-tidy-diff.py on PATH (the wheel
`clang-tidy` ships both).

Usage:
  python3 tools/style/check-tidy.py --build-dir build-dev-debug
  python3 tools/style/check-tidy.py --build-dir build-dev-debug --range origin/main...HEAD
  python3 tools/style/check-tidy.py --build-dir build-dev-debug --advisory
"""

import argparse
import subprocess
import sys

CHECKS = ",".join([
    "-*",
    "readability-identifier-naming",
    "readability-braces-around-statements",
    "modernize-use-nullptr",
    "modernize-use-override",
    "bugprone-macro-parentheses",
    "bugprone-sizeof-expression",
    "performance-for-range-copy",
])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, help="build directory with compile_commands.json")
    parser.add_argument("--range", default="HEAD~1..HEAD", help="git range whose diff is checked")
    parser.add_argument("--advisory", action="store_true", help="report findings but exit 0")
    args = parser.parse_args()

    diff = subprocess.run(["git", "diff", args.range, "--", "*.cc", "*.h"],
                          capture_output=True, text=True, check=False)
    if diff.returncode != 0:
        # Shallow CI checkouts may not have the base commit. Advisory mode
        # reports and continues; normal mode fails closed.
        print(f"check-tidy: cannot diff {args.range} in this checkout, skipping")
        return 0 if args.advisory else 1
    if not diff.stdout.strip():
        print(f"check-tidy: no C++ changes in {args.range}")
        return 0

    try:
        run = subprocess.run(["clang-tidy-diff.py", "-p1", "-path", args.build_dir, "-quiet",
                              f"-checks={CHECKS}"],
                             input=diff.stdout, capture_output=True, text=True, check=False)
    except OSError as error:
        print(f"check-tidy: cannot execute clang-tidy-diff.py: {error}", file=sys.stderr)
        return 0 if args.advisory else 1
    output = run.stdout + run.stderr
    findings = [line for line in output.splitlines() if ": warning:" in line]
    for line in findings:
        print(line)
    print(f"check-tidy: {len(findings)} finding(s) in {args.range}")
    if findings:
        return 0 if args.advisory else 1
    if run.returncode != 0:
        print(
            f"check-tidy: clang-tidy-diff.py failed with exit code {run.returncode}",
            file=sys.stderr,
        )
        if output.strip():
            print(output.rstrip(), file=sys.stderr)
        return 0 if args.advisory else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
