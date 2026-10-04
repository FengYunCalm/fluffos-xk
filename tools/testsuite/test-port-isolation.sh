#!/usr/bin/env bash
# test-port-isolation.sh - port-0 isolation stress test (R2-F11).
#
# Verifies the driver's OS-assigned port contract under load:
#   1. 5 serial runs: every run binds four DISTINCT loopback ports and
#      exits cleanly with LPC assertions passing;
#   2. 20 concurrent runs: zero port conflicts (the OS cannot hand out the
#      same port twice while bound) and zero leftover processes, sandbox
#      directories, or lock files afterwards.
#
# Usage: tools/testsuite/test-port-isolation.sh --driver PATH [--quick]
#   --quick: 1 serial + 4 concurrent runs (for local iteration)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
RUNNER="$HERE/run-isolated.sh"
DRIVER=""
QUICK=0

while [ $# -gt 0 ]; do
  case "$1" in
    --driver) DRIVER="$2"; shift 2 ;;
    --quick) QUICK=1; shift ;;
    *) echo "usage: $0 --driver PATH [--quick]" >&2; exit 2 ;;
  esac
done
[ -n "$DRIVER" ] && [ -x "$DRIVER" ] || {
  echo "error: --driver must point to an executable driver" >&2
  exit 2
}

SERIAL_RUNS=$([ "$QUICK" -eq 1 ] && echo 1 || echo 5)
CONCURRENT_RUNS=$([ "$QUICK" -eq 1 ] && echo 4 || echo 20)

# Existing sandboxes and driver processes may belong to an earlier run or a
# different checkout. Snapshot them before this run so the final check only
# reports resources introduced by this invocation.
BASELINE_SANDBOXES="$(find "$ROOT/testsuite" -maxdepth 1 -type d -name '.run-isolated-??????' -print | sort)"
BASELINE_LOCKDIR=0
if [ -d "$ROOT/testsuite/.run-isolated.lockdir" ]; then
  BASELINE_LOCKDIR=1
fi
case "$DRIVER" in
  /*) DRIVER_ABS="$DRIVER" ;;
  *) DRIVER_ABS="$(cd "$(dirname "$DRIVER")" && pwd)/$(basename "$DRIVER")" ;;
esac
DRIVER_REAL="$(readlink -f "$DRIVER_ABS")"
DRIVER_NAME="$(basename "$DRIVER_ABS")"
list_matching_driver_pids() {
  local pid exe
  while read -r pid; do
    exe="$(readlink -f "/proc/$pid/exe" 2>/dev/null || true)"
    if [ "$exe" = "$DRIVER_REAL" ]; then
      printf '%s\n' "$pid"
    fi
  done < <(pgrep -x "$DRIVER_NAME" || true)
}
BASELINE_DRIVER_PIDS="$(list_matching_driver_pids | sort -n)"

FAIL=0
fail() {
  echo "FAIL: $1"
  FAIL=1
}

echo "== port isolation: $SERIAL_RUNS serial runs =="
for i in $(seq 1 "$SERIAL_RUNS"); do
  if ! bash "$RUNNER" --driver "$DRIVER" --mode audit >/tmp/portiso-serial-$i.log 2>&1; then
    fail "serial run $i failed (see /tmp/portiso-serial-$i.log)"
  fi
done

echo "== port isolation: $CONCURRENT_RUNS concurrent runs (ports-only) =="
PIDS=()
for i in $(seq 1 "$CONCURRENT_RUNS"); do
  # Ports-only mode: the LPC testsuite writes shared files under testsuite/
  # (single-instance design), so the concurrency stress targets the port-0
  # contract itself - unique OS-assigned loopback ports and zero leftovers.
  bash "$RUNNER" --driver "$DRIVER" --mode audit --ports-only >/tmp/portiso-conc-$i.log 2>&1 &
  PIDS+=("$!")
done
CONC_FAIL=0
for pid in "${PIDS[@]}"; do
  if ! wait "$pid"; then
    CONC_FAIL=1
  fi
done
if [ "$CONC_FAIL" -ne 0 ]; then
  fail "at least one concurrent run failed (logs /tmp/portiso-conc-*.log)"
fi

echo "== residual check =="
# Sandbox dirs and the mkdir fallback lockdir are transient; the flock
# lock FILE (.run-isolated.lock) is intentionally persistent (flock needs
# a stable inode), so it is not a leak.
CURRENT_SANDBOXES="$(find "$ROOT/testsuite" -maxdepth 1 -type d -name '.run-isolated-??????' -print | sort)"
NEW_SANDBOXES="$(comm -13 <(printf '%s\n' "$BASELINE_SANDBOXES") <(printf '%s\n' "$CURRENT_SANDBOXES") | sed '/^$/d')"
if [ -n "$NEW_SANDBOXES" ]; then
  fail "leftover sandbox entries introduced by this run: $NEW_SANDBOXES"
fi
if [ "$BASELINE_LOCKDIR" -eq 0 ] && [ -d "$ROOT/testsuite/.run-isolated.lockdir" ]; then
  fail "leftover fallback lock directory introduced by this run"
fi
# No leftover driver processes from this test run. Match the exact executable
# path and ignore processes that existed before this invocation, including
# drivers belonging to another checkout.
CURRENT_DRIVER_PIDS="$(list_matching_driver_pids | sort -n)"
NEW_DRIVER_PIDS="$(comm -13 <(printf '%s\n' "$BASELINE_DRIVER_PIDS") <(printf '%s\n' "$CURRENT_DRIVER_PIDS") | sed '/^$/d')"
if [ -n "$NEW_DRIVER_PIDS" ]; then
  fail "leftover driver processes introduced by this run: $NEW_DRIVER_PIDS"
fi

if [ "$FAIL" -ne 0 ]; then
  echo "port-isolation: FAILED"
  exit 1
fi
echo "port-isolation: OK ($SERIAL_RUNS serial + $CONCURRENT_RUNS concurrent, zero conflicts, zero leftovers)"
