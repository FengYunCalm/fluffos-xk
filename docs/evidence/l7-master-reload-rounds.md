---
layout: doc
title: L7 extension master-target reload rounds (evidence)
---
# L7 extension: master-target reload rounds

The L7 pressure contract covered leaf-object reloads (`recompile_stress.c`) and
three benches. The E3 v2 work made the master object reloadable, so the contract
gained rounds that target it.

## What changed

`testsuite/single/tests/efuns/recompile_stress.c`:

- Every round now also attempts `recompile_object(master())` and asserts the
  *stable rejection* `recompile_object target program is executing`. The test
  runs through master applies, so the master's own program is on the stack: this
  is the protection that keeps an executing target consistent, not the removed
  v1 gate. The header documents where the rounds that can run live.

`src/tests/test_lpc.cc` - `DriverTest.TestMasterReloadStressRounds`:

- 12 rounds of the full v2 transaction against `master_ob`
  (`compile_program_for_recompile` -> `start_recompile_transaction(Master)` ->
  `prepare_variable_migrations` -> `commit_swap` -> `run_create_guarded` ->
  `commit_finish`). After every round it asserts the swap published the new
  program, the rebuilt apply cache resolves `valid_recompile_object` for that
  program, and the standard master authorization apply still answers through the
  master (the apply every reload itself depends on). The original program is
  pinned before the loop and restored after it, the same way the single-reload
  test does.

## Gates

| Gate | Command | Result |
| --- | --- | --- |
| LPC stress contract | `cd testsuite && ../build-dev-debug/bin/driver etc/config.recompile -ftest:single/tests/efuns/recompile_stress` | `1000 recompiles ok, 3 master targets refused while executing, 0 failures, 4 workers`, exit 0 |
| LPC stress under ASan | `ASAN_OPTIONS=detect_leaks=1 ../build-asan/bin/driver etc/config.recompile -ftest:single/tests/efuns/recompile_stress` | same result, 0 ASan/LSan reports |
| Driver-side rounds | `lpc_tests --gtest_filter=DriverTest.TestMasterReload*` | 2/2 pass (single reload plus 12 stress rounds) |
| Full C++ regression | `build-dev-debug/src/tests/lpc_tests` | 471/471 pass |

## Notes

- The driver-side rounds are the counterpart of the LPC-side boundary
  assertion: only a caller that is not executing the target may reload it, and
  in the ftest process the master always is.
- The rounds deliberately recompile the same source, so the layout stays
  migratable and the assertion set stays about repetition rather than about
  layout changes (the layout-change paths have their own tests).
