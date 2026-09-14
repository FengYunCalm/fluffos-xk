# E2 fuzz harness evidence

- Date: 2026-09-14
- Source: `c5323633`
- Build: `build-fuzz-user`, `RelWithDebInfo`, `BUILD_FUZZERS=ON`, Clang 17.0.6 with AFL++ 4.09c
- Binaries: `build-fuzz-user/src/fuzz_compile`, `build-fuzz-user/src/fuzz_restore`

## Harness self-checks

Both harnesses ran from `testsuite/` against `etc/config.test` and the repository
corpora. Valid, mixed, and invalid sequences completed with the expected stable
`HARNESS_OK` summaries. Missing input was rejected with a non-zero exit code:

- `fuzz_compile`: valid `executions=3 chunks=3 success=3 diagnostic=0`; mixed
  `executions=4 chunks=4 success=3 diagnostic=1`; invalid
  `executions=3 chunks=3 success=0 diagnostic=3`; missing input exit `1`.
- `fuzz_restore`: valid `executions=3 chunks=3 success=2 diagnostic=0`; mixed
  `executions=3 chunks=3 success=2 diagnostic=1`; invalid
  `executions=3 chunks=3 success=1 diagnostic=2`; nested mapping regression
  `executions=2 chunks=2 success=1 diagnostic=1`; missing input exit `1`.

Raw stdout/stderr captures are stored beside this file as `e2-fuzz-*.stdout` and
`e2-fuzz-*.stderr`.

## Bounded AFL++ smoke

Each target ran independently for 60 seconds with `AFL_NO_UI=1`,
`AFL_NO_AFFINITY=1`, `AFL_SKIP_CPUFREQ=1`, and
`AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1` (the WSL2 `core_pattern` is managed by
an external utility). Both runs exited `0`, completed target dry-runs, and
reported no saved crashes or hangs.

| target | output | execs | stability | saved crashes | saved hangs |
| --- | --- | ---: | ---: | ---: | ---: |
| `fuzz_compile` | `build-fuzz-user/afl-compile-final-2` | 21,537 | 99.90% | 0 | 0 |
| `fuzz_restore` | `build-fuzz-user/afl-restore-final` | 43,413 | 100.00% | 0 | 0 |

The raw AFL console captures and copied `fuzzer_stats` files are
`e2-afl-compile.{stdout,stderr,fuzzer_stats}` and
`e2-afl-restore.{stdout,stderr,fuzzer_stats}`.
