#!/usr/bin/env bash
# Validate or explicitly format the testsuite LPC corpus with
# tools/lpc-syntax/format.mjs. Dependency-free: needs only node (>= 18).
#
#   testsuite/format.sh                         # read-only full-corpus check
#   testsuite/format.sh --check [paths...]      # read-only selected check
#   testsuite/format.sh --write path1 [path2]   # explicit, atomic per-file write
#
# The default is --check. A write requires --write and at least one explicit
# file. The corpus discovery and exclusion list live here; the Node engine
# validates every selected path, encoding, token-preserving result, idempotency
# and concurrent-change snapshot before it writes.
#
# EXCLUSIONS (do not remove): the two deliberately-malformed UTF-8 compiler
# fixtures are raw byte fixtures, not text -- formatting (or even reading them
# as UTF-8 and writing back) would corrupt them. testsuite/.gitattributes marks
# them binary for the same reason; if a similar raw-invalid-byte fixture is
# added later, add it BOTH there and here. See AGENTS.md section 7.
#
# The three EOF-lexerror fixtures (eof_in_string / eof_in_comment /
# bad_at_block) are deliberately-unterminated sources whose brokenness is the
# point -- the driver hard-errors on them at lex time, and the formatter refuses
# such input by design. If a new deliberately-unterminated fail fixture is
# added, add it here too.
#
# testsuite/tools/force_exdev_rename.c is real C, not LPC: an LD_PRELOAD shim
# built by the root CMakeLists.txt makes rename(2) fail with EXDEV. The find
# below picks up every *.c because legacy LPC sources use that extension, so a
# genuine C file has to be excluded by path.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

EXCLUDES=(
  "testsuite/single/tests/compiler/fail/bad_utf8_string.c"
  "testsuite/single/tests/compiler/fail/bad_utf8_arrayblock.c"
  "testsuite/single/tests/compiler/fail/eof_in_string.c"
  "testsuite/single/tests/compiler/fail/eof_in_comment.c"
  "testsuite/single/tests/compiler/fail/bad_at_block.c"
  "testsuite/tools/force_exdev_rename.c"
)

is_excluded() {
  local candidate=$1 excluded
  case "$candidate" in
    "$REPO_ROOT"/*) candidate=${candidate#"$REPO_ROOT"/} ;;
    ./*) candidate=${candidate#./} ;;
  esac
  for excluded in "${EXCLUDES[@]}"; do
    [[ "$candidate" == "$excluded" ]] && return 0
  done
  return 1
}

mode=--check
files=()
if (($# > 0)); then
  case "$1" in
    --check|--write)
      mode=$1
      shift
      ;;
    *)
      echo "ERROR: use --check or --write; writes require an explicit file list" >&2
      exit 2
      ;;
  esac
  if (($# > 0)); then
    files=("$@")
  elif [[ "$mode" == --write ]]; then
    echo "ERROR: --write requires at least one file" >&2
    exit 2
  fi
fi

if ((${#files[@]} == 0)); then
  if [[ "$mode" == --write ]]; then
    echo "ERROR: --write requires at least one file" >&2
    exit 2
  fi
  while IFS= read -r -d '' path; do
    if ! is_excluded "$path"; then
      files+=("$path")
    fi
  done < <(
    cd -- "$REPO_ROOT"
    find testsuite \( -name '*.lpc' -o -name '*.c' \) -type f -print0 | sort -z
  )
else
  for path in "${files[@]}"; do
    if is_excluded "$path"; then
      echo "ERROR: excluded fixture cannot be formatted: $path" >&2
      exit 2
    fi
  done
fi

if ((${#files[@]} == 0)); then
  echo "ERROR: no corpus files selected" >&2
  exit 2
fi

cd -- "$REPO_ROOT"
printf '%s\0' "${files[@]}" |
  node "${REPO_ROOT}/tools/lpc-syntax/bin/format-corpus.mjs" "$mode"
