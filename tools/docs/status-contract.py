#!/usr/bin/env python3
"""Read-only status-key contract checker.

The driver exposes status mappings (`vm_owner_thread_status()`, `vm_owner_status()`,
...) whose key set is part of the LPC contract read by downstream mudlibs. This
script keeps the code and `docs/owner-status-contract.md` in sync.

Modes:
  --dump    print every literal key the code writes, sorted (for review)
  --check   require every documented key to be emitted; report extra source keys

Only literal keys are covered. The handful of sites that compute a key at
runtime are listed as known blind spots in the contract document.

`--check` verifies that every documented contract key is still emitted and
reports the count of extra literal source keys. `--dump` lists those scoped
literal keys for separate cleanup; extra keys are not a contract failure. CI
wiring is plan unit U9a.
"""

from __future__ import annotations

import argparse
import os
import re
import sys

CONTRACT_PATH = os.path.join("docs", "owner-status-contract.md")
# The contract covers the owner-observability surface only. Other add_mapping_*
# call sites in the tree build unrelated results (socket_status, variables dump,
# efun return values) and are deliberately out of scope. Extra keys in these two
# files are still outside the frozen downstream contract; --check intentionally
# does not treat them as contract keys.
SCOPED_FILES = (
    "src/vm/internal/owner.cc",
    "src/vm/internal/object_store.cc",
)
# src/packages/core/vm_owner.cc is deliberately out of scope: its mappings are the
# return shapes of public LPC efuns (owner_safe payloads, failure schemas), not the
# observability status surface.

# `add_mapping_pair(map, "key", ...)` and its siblings. `\s*` spans newlines so
# multi-line calls are recognized; comments are stripped first so a commented-out
# write never counts.
KEY_RE = re.compile(
    r'add_mapping_[a-z]+\(\s*[A-Za-z_][A-Za-z0-9_]*\s*,\s*"([a-z][a-z0-9_]*)"'
)
# Contract rows look like: | `queue_depth` | ... |
CONTRACT_ROW_RE = re.compile(r"^\|\s*`([a-z][a-z0-9_]*)`\s*\|", re.MULTILINE)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", " ", text)


def emitted_keys(repo: str) -> set[str]:
    keys: set[str] = set()
    for rel in SCOPED_FILES:
        path = os.path.join(repo, rel)
        if not os.path.exists(path):
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            keys.update(KEY_RE.findall(strip_comments(f.read())))
    return keys


def contract_keys(repo: str) -> set[str]:
    path = os.path.join(repo, CONTRACT_PATH)
    with open(path, encoding="utf-8", errors="replace") as f:
        return set(CONTRACT_ROW_RE.findall(f.read()))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=".", help="repository root")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--dump", action="store_true", help="print emitted keys")
    mode.add_argument("--check", action="store_true", help="check contracted keys and report extras")
    args = parser.parse_args()

    code = emitted_keys(args.repo)
    if args.dump:
        for key in sorted(code):
            print(key)
        return 0

    contract = contract_keys(args.repo)
    undocumented = sorted(code - contract)
    missing = sorted(contract - code)
    # The contract freezes the keys downstream may rely on. A key that is emitted
    # but not documented is not a failure -- these extra source keys are
    # reported as a count. A documented key that is no longer
    # emitted IS a failure: downstream code may still read it.
    for key in missing:
        print(f"FAIL: listed in {CONTRACT_PATH} but never emitted: {key}")
    print(
        f"status-contract: {len(undocumented)} undocumented, "
        f"{len(missing)} missing ({len(contract)} contracted)"
    )
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
