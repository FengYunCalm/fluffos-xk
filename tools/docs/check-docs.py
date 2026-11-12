#!/usr/bin/env python3
"""Validate Markdown docs for stale links, paths, commit SHAs and report refs.

Checks performed:
  1. Explicit markdown links [text](path) and [text](path#anchor) whose target
     is a local file must exist (resolved against repo root or docs/).
  2. Inline code references that start with a known repo top-level dir
     (docs/, src/, tools/, testsuite/, .github/, third_party/, CMakeLists.txt)
     must exist.
  3. 40-hex commit SHA references must exist in git history.
  4. Report paths under docs/reports/ must exist.

Policy for frozen history: files under docs/archive/ are historical records of
state that no longer exists. Their Markdown links must still resolve (so the
archive stays navigable), but inline path references and commit SHAs are not
second-guessed -- an archived audit legitimately names files and upstream
commits this fork removed or never had. Live documents keep the strict checks.

Exit code 0 = all checks pass; 1 = violations found.

Usage:
  tools/docs/check-docs.py [--repo DIR] [--skip-git] [--dir docs]
"""

import argparse
import os
import re
import subprocess
import sys
import time

LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
INLINE_PATH_RE = re.compile(r"`([^`]+)`")
KNOWN_PREFIXES = (
    "docs/", "src/", "tools/", "testsuite/", ".github/", "third_party/",
    "cmake/", "compat/", "CMakeLists.txt", "Dockerfile", "SECURITY.md",
    "README", "CHANGELOG.md", "Testing", "CONTRIBUTING.md",
    "CODE_OF_CONDUCT.md", "NOTICE", "LICENSE", "Credits", "Copyright",
    "ChangeLog", "CMakePresets.json",
)
SHA_RE = re.compile(r"\b[0-9a-f]{40}\b")
IGNORED_DIRS = (
    "docs/efun",          # generated API docs with .html cross-refs
    "docs/lpc",           # generated language reference with .html cross-refs
    "docs/node_modules",  # vendored package READMEs are not project docs
)

# Known-nonlocal references in historical, evidence, and planning docs that
# refer to deleted reports, moved files, future artifacts, upstream-only paths,
# or SHAs unavailable in this partial checkout. They are tracked here
# deliberately: T17 keeps current implementation docs strict while historical
# evidence and explicit plans stay readable. New violations in current docs
# must NOT be added here; fix the reference instead.
KNOWN_STALE = {
    "docs/multicore-production-gate.md": {
        "docs/reports/multicore-mudlib-audit-2026-06-25.md",
    },
    "docs/archive/multicore/multicore-actor-vm-plan-2026-06.md": {
        "src/vm/internal/object_store.h",
        "src/vm/internal/stralloc.cc",
        "f80968b0868059d97e02da4062c3a233f1225bd8",
    },
    "docs/archive/multicore/multicore-refactor-execution-plan-2026-06.md": {
        "docs/multicore-actor-vm-plan.md",
        "src/vm/internal/stralloc.cc",
    },
    "docs/driver/adding_efuns.md": {
        "src/packages/mypkg",
    },
    # These historical audit references point to commits that are no longer
    # present in this fork's rewritten history.
    "docs/coding-standard.md": {
        "328aaf10635ef8ce636b2b6cebd549bb165e462d",
    },
    "docs/project-modernization-plan-2026-10.md": {
        "f1c242b1db54383d701838ee8aef512a0d923e81",
        "795eb371f1af522280738f454b8fc81afe231f1c",
        "328aaf10635ef8ce636b2b6cebd549bb165e462d",
    },
    # These are future fuzz corpus directories required by the design; Git
    # cannot represent their empty pre-populated state yet.
    "docs/recompile-object-special-design-2026-08.md": {
        "src/tests/fuzz/corpus/compile/",
        "src/tests/fuzz/corpus/restore/",
    },
    # The upstream absorption plan names an upstream-only backend module.
    "docs/upstream-absorption-plan-d07e7641-735bd31f.md": {
        "src/backend_libevent.cc",
    },
    # These harnesses and metrics are specified future artifacts, not present
    # production files in the current tree.
    "docs/upstream-sync-optimization-plan-2026-08.md": {
        "tools/upstream-sync-bench.sh",
        "tools/analyze-upstream-sync-bench.py",
        "tools/analyze-owner-scheduler-capacity.py",
        "tools/upstream-sync-owner-metrics.json",
    },
    # The upstream absorption plan preserves upstream-only paths and an
    # explicitly rejected generated docs page for auditability.
    "docs/upstream-absorption-plan-2026-10.md": {
        "tools/lpc-syntax",
        "docs/build.mdx",
        # Fixed upstream baseline/tag objects are intentionally not part of
        # this fork's Git history and are verified through git ls-remote.
        "795eb371f1af522280738f454b8fc81afe231f1c",
        "9c33f463727d714b70e74650813c5fac83c35c10",
    },
    # Evidence from a removed local build directory is retained for history.
    "docs/evidence/e3-v2-phase1-simul-efun-reload.md": {
        "testsuite/../build-sync/bin/driver etc/config.recompile -ftest",
    },
    # Artifacts named by the 2026-11 refactor plan. Each entry must be removed
    # from this set as the corresponding unit lands; the plan is the only
    # document allowed to reference not-yet-existing paths.
    "docs/refactor-findings-2026-11.md": {
        # Removed by U1a; the findings keep them as the deletion record.
        "src/outdated_define.txt",
        "testsuite/test.o",
        "testsuite/data/gateway_net_dead_destruct_user.o",
    },
    "docs/refactor-plan-2026-11.md": {
        # Removed by U1a; the plan keeps them as the deletion record.
        "src/outdated_define.txt",
        "testsuite/test.o",
        "testsuite/data/gateway_net_dead_destruct_user.o",
        "docs/add_missing_efuns.py",
        "docs/fix_md_header.py",
        "docs/fix_seealso.py",
        # Archived by U2a; the plan records the move.
        "docs/owner-status-contract.md",
        "docs/roadmap.md",
        "docs/architecture.md",
        "tools/docs/status-contract.py",
    },
}
IGNORED_FILES = set()


_ARCHIVE_DIR = os.path.join("docs", "archive")


def _archive_has(repo: str, basename: str) -> bool:
    """True when docs/archive/ holds a file that this basename names.

    Archived files were sometimes renamed with a date suffix while moving
    (REMEDIATION_PLAN.md -> REMEDIATION_PLAN-2026-10.md), so match on the
    extension-less stem as a prefix.
    """
    stem = os.path.splitext(basename)[0]
    if not stem:
        return False
    for root, _dirs, files in os.walk(os.path.join(repo, _ARCHIVE_DIR)):
        for name in files:
            if not name.endswith(".md"):
                continue
            if os.path.splitext(name)[0].startswith(stem):
                return True
    return False


def git_has_sha(repo: str, sha: str) -> bool:
    out = subprocess.run(
        ["git", "-C", repo, "cat-file", "-e", sha + "^{commit}"],
        capture_output=True,
        check=False,
    )
    return out.returncode == 0


def resolve(repo: str, ref: str, base_dir: str = ""):
    """Return the first existing candidate path, or None.

    Relative links are resolved against base_dir (the directory of the
    referencing file) first, then against the repo root and docs/.
    """
    ref = ref.strip()
    if not ref or ref.startswith(("http://", "https://", "mailto:", "#", "!")):
        return True  # external/anchor/ignored
    ref = ref.split("#", 1)[0].split("?", 1)[0]
    if not ref:
        return True
    if ref.startswith("$") or ref.startswith("<") or ref.startswith("{%"):
        return True  # template/placeholder
    if re.search(r"<[^>\n]+>", ref):
        return True  # embedded placeholder such as <candidate>
    if ref.endswith("*") or "*" in ref:
        return True  # glob pattern, not a literal path
    if ref.endswith(".html"):
        return True  # generated docs-site page, built at publish time
    # Strip commit annotations and C++/LPC symbol suffixes before resolving
    # the path.  Historical evidence uses forms such as ``file.cc:32::fn``
    # and ``CMakeLists.txt=deadbeef``.
    ref = re.sub(r"=[0-9a-f]{7,64}$", "", ref)
    if "::" in ref:
        ref = ref.split("::", 1)[0]
    # Strip line/range and symbol suffixes ("file:12", "file.cc:52-60",
    # "file.cc:function").
    ref = re.sub(r":\d+(?:-\d+)?(?:,\d+(?:-\d+)?)*$", "", ref)
    ref = re.sub(r":(?:[A-Za-z_][A-Za-z0-9_]*)(?:\(\))?$", "", ref)
    ref = re.sub(r"^(tools/[A-Za-z0-9_./-]+) .*$", r"\1", ref)

    # A compact reference such as path.{h,cc} denotes the two files in the
    # repository, rather than a literal brace-bearing filename.
    brace = re.search(r"\{([^{}]+)\}", ref)
    if brace:
        refs = [ref[:brace.start()] + part + ref[brace.end():]
                for part in brace.group(1).split(",")]
    else:
        refs = [ref]

    for resolved_ref in refs:
        candidates = []
        if not resolved_ref.startswith("/"):
            if base_dir:
                candidates.append(os.path.normpath(os.path.join(base_dir, resolved_ref)))
        candidates += [
            os.path.normpath(os.path.join(repo, resolved_ref)),
            os.path.normpath(os.path.join(repo, "docs", resolved_ref)),
        ]
        for c in candidates:
            if os.path.exists(c):
                return True
        # Generated docs-site pages ship as .html next to the .md source.
        for c in candidates:
            if c.endswith(".md") and os.path.exists(c[:-3] + ".html"):
                return True
    return False


def check_file(path: str, repo: str, skip_git: bool) -> list[str]:
    errors: list[str] = []
    rel = os.path.relpath(path, repo)
    if rel in IGNORED_FILES or rel.startswith(IGNORED_DIRS):
        return errors
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError as e:
        return [f"{rel}: unreadable: {e}"]

    stale = KNOWN_STALE.get(rel, set())
    # Frozen history: archived records describe a state that no longer exists,
    # so only their Markdown links are validated (see module docstring).
    frozen_history = rel.startswith("docs/archive/")
    for lineno, line in enumerate(text.splitlines(), 1):
        base_dir = os.path.dirname(os.path.abspath(path))
        for m in LINK_RE.finditer(line):
            ref = m.group(1)
            if ref in stale:
                continue
            if not resolve(repo, ref, base_dir):
                # Archived documents were written against the pre-archive tree.
                # Accept a link whose target basename still exists under
                # docs/archive/ (that is where the referenced file moved).
                target = ref.split("#", 1)[0].split("?", 1)[0].strip()
                basename = os.path.basename(target)
                if not (frozen_history and basename and _archive_has(repo, basename)):
                    errors.append(f"{rel}:{lineno}: missing link target: {ref!r}")
        if not frozen_history:
            for m in INLINE_PATH_RE.finditer(line):
                ref = m.group(1)
                if ref in stale:
                    continue
                if ref.startswith(KNOWN_PREFIXES) and not resolve(repo, ref, base_dir):
                    errors.append(f"{rel}:{lineno}: missing path reference: {ref!r}")
        if not skip_git and not frozen_history:
            for sha in set(SHA_RE.findall(line)):
                if sha in stale:
                    continue
                if not git_has_sha(repo, sha):
                    errors.append(f"{rel}:{lineno}: unknown commit SHA: {sha}")
    return errors


_DATED_PLAN_RE = re.compile(r"(?:plan|findings|design|audit|report)", re.IGNORECASE)
_DATED_YEAR_RE = re.compile(r"-20\d{2}-")
_STATUS_RE = re.compile(r"^>\s*状态")
_DRAFT_AGE_DAYS = 90


def check_root_plan_status(docs_dir: str, repo: str) -> list[str]:
    """Plan-like documents at the docs root must say what state they are in."""
    errors: list[str] = []
    for name in sorted(os.listdir(docs_dir)):
        path = os.path.join(docs_dir, name)
        if not name.endswith(".md") or not os.path.isfile(path):
            continue
        if not (_DATED_PLAN_RE.search(name) or _DATED_YEAR_RE.search(name)):
            continue
        try:
            head = "".join(open(path, encoding="utf-8", errors="replace").readlines()[:10])
        except OSError as e:
            errors.append(f"docs/{name}: unreadable: {e}")
            continue
        if not any(_STATUS_RE.match(line) for line in head.splitlines()):
            errors.append(f"docs/{name}: plan-like document needs a '> 状态：' header")
    return errors


_MAX_TEST_LINES = 3500
# Mudlib test files above the 1000-line goal, frozen at their current size. Splitting
# them needs the same helper extraction the C++ tests needed; until then the gate
# keeps them from growing. Lower each entry as its file gets split.
_LPC_TEST_SIZE_BASELINE = {
    "single/tests/efuns/owner_executor_contract.c": 1666,
    "single/tests/compiler/utf8.c": 1595,
}
_LPC_TEST_LINES = 1000


def check_lpc_test_size(repo: str) -> list[str]:
    """Mudlib test files may not grow past their frozen size; small ones stay under 1000."""
    errors: list[str] = []
    tests_dir = os.path.join(repo, "testsuite")
    if not os.path.isdir(tests_dir):
        return errors
    for root, _dirs, names in os.walk(tests_dir):
        for name in sorted(names):
            if not name.endswith((".c", ".lpc")):
                continue
            path = os.path.join(root, name)
            rel = os.path.relpath(path, tests_dir).replace(os.sep, "/")
            with open(path, encoding="utf-8", errors="replace") as handle:
                count = sum(1 for _ in handle)
            allowed = _LPC_TEST_SIZE_BASELINE.get(rel, _LPC_TEST_LINES)
            if count > allowed:
                errors.append(f"testsuite/{rel}: {count} lines exceeds {allowed}")
    return errors


def check_test_file_size(repo: str) -> list[str]:
    """A test file that grows past the cap is a file nobody can read in one go."""
    errors: list[str] = []
    tests_dir = os.path.join(repo, "src", "tests")
    if not os.path.isdir(tests_dir):
        return errors
    for name in sorted(os.listdir(tests_dir)):
        if not name.endswith(".cc"):
            continue
        path = os.path.join(tests_dir, name)
        with open(path, encoding="utf-8", errors="replace") as handle:
            count = sum(1 for _ in handle)
        if count > _MAX_TEST_LINES:
            errors.append(f"src/tests/{name}: {count} lines exceeds {_MAX_TEST_LINES}")
    return errors


def _draft_age_report(docs_dir: str, repo: str) -> None:
    """Report DRAFT documents untouched for a long time; never fails the check."""
    cutoff = time.time() - _DRAFT_AGE_DAYS * 86400
    for root, _dirs, files in os.walk(docs_dir):
        for name in sorted(files):
            if not name.endswith(".md"):
                continue
            path = os.path.join(root, name)
            head_lines = open(path, encoding="utf-8", errors="replace").readlines()[:10]
            if not any("DRAFT" in line and _STATUS_RE.match(line) for line in head_lines):
                continue
            rel = os.path.relpath(path, repo)
            out = subprocess.run(["git", "-C", repo, "log", "-1", "--format=%ct", "--", rel],
                                 capture_output=True, text=True, check=False)
            stamp = out.stdout.strip()
            if stamp.isdigit() and int(stamp) < cutoff:
                print(f"note: {rel}: DRAFT untouched for {_DRAFT_AGE_DAYS}+ days")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=".", help="repo root")
    parser.add_argument("--dir", default="docs", help="docs directory (relative to repo)")
    parser.add_argument("--skip-git", action="store_true", help="skip commit SHA checks")
    args = parser.parse_args()

    docs_dir = os.path.join(args.repo, args.dir)
    if not os.path.isdir(docs_dir):
        print(f"docs dir not found: {docs_dir}", file=sys.stderr)
        return 2

    all_errors: list[str] = []
    checked = 0
    for root, _dirs, files in os.walk(docs_dir):
        for fname in sorted(files):
            if fname.endswith(".md"):
                checked += 1
                all_errors.extend(check_file(os.path.join(root, fname), args.repo, args.skip_git))

    if all_errors:
        for e in all_errors[:100]:
            print(f"FAIL: {e}")
        print(f"check-docs: {len(all_errors)} issue(s) across {checked} markdown files")
        return 1
    root_errors = check_root_plan_status(docs_dir, args.repo)
    root_errors += check_test_file_size(args.repo)
    root_errors += check_lpc_test_size(args.repo)
    for e in root_errors:
        print(f"FAIL: {e}")
    if root_errors:
        print(f"check-docs: {len(root_errors)} root-document issue(s)")
        return 1
    if not args.skip_git:
        _draft_age_report(docs_dir, args.repo)
    print(f"check-docs: OK ({checked} markdown files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
