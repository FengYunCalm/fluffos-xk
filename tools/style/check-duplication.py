#!/usr/bin/env python3
"""Duplicate-window ratchet.

Every window of WINDOW normalized lines is hashed; a file's count is how many of
its windows also appear elsewhere. Only per-file counts are compared, so splitting
a file is fine and copying a block is not. Refresh the baseline by hand:
`python3 tools/style/check-duplication.py --print-baseline`.
"""

import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BASELINE = os.path.join(ROOT, "tools", "style", "duplication-baseline.txt")
WINDOW = 20
COMMENT = re.compile(r"//[^\n]*|/\*[\s\S]*?\*/")


def normalized(path):
    text = COMMENT.sub("", open(path, encoding="utf-8", errors="ignore").read())
    return [(number, flat) for number, line in enumerate(text.split("\n"), 1)
            for flat in [re.sub(r"\s+", "", line)] if flat]


def counts():
    seen = {}
    for root, dirs, names in os.walk(os.path.join(ROOT, "src")):
        dirs[:] = [d for d in dirs if d != "thirdparty"]
        for name in names:
            if not name.endswith((".cc", ".h")) or ".autogen." in name:
                continue
            path = os.path.join(root, name)
            rel = os.path.relpath(path, ROOT)
            lines = normalized(path)
            for start in range(len(lines) - WINDOW + 1):
                body = "\n".join(flat for _, flat in lines[start:start + WINDOW])
                seen.setdefault(hashlib.md5(body.encode()).hexdigest(), []).append((rel, lines[start][0]))
    per_file, first = {}, {}
    for locations in seen.values():
        for rel, line in locations if len(locations) > 1 else ():
            per_file[rel] = per_file.get(rel, 0) + 1
            first[rel] = min(line, first.get(rel, line))
    return per_file, first


def main():
    per_file, first = counts()
    if "--print-baseline" in sys.argv:
        print("\n".join(f"{rel} {per_file[rel]}" for rel in sorted(per_file)))
        return 0
    baseline = {}
    if os.path.exists(BASELINE):
        baseline = {rel: int(count) for rel, _, count in
                    (line.partition(" ") for line in open(BASELINE, encoding="utf-8")) if count.strip()}
    over = [f"{rel}:{first[rel]}: {count} duplicate windows, baseline {baseline.get(rel, 0)}"
            for rel, count in sorted(per_file.items()) if count > baseline.get(rel, 0)]
    print("\n".join(over))
    print(f"duplication: {len(per_file)} files with duplicates, {len(over)} over baseline")
    return 1 if over else 0


if __name__ == "__main__":
    sys.exit(main())
