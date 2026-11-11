---
layout: doc
title: T3.4 lpcshell interactive shell (evidence)
---
# T3.4 lpcshell: interactive compile-and-diagnose shell

Last phase of the lpcshell prerequisite sequence
(`docs/lpcshell-prerequisite-plan-2026-08.md` §1.4): the session shape that
consumes the arena (T3.1), the structured records (T3.2) and the renderer
(T3.3).

## What was built

`src/main_lpcshell.cc` (new binary `lpcshell`, built by the same presets as
`driver` and `lpcc`):

- `lpcshell <config> <file.c>...` compiles each file and exits 1 if any of them
  reported an error; `lpcshell <config>` runs the REPL.
- The REPL accumulates input until it is complete (bracket depth balanced, no
  unfinished string or block comment), so a function can be typed across lines.
  Preprocessor directives stay pending instead of being compiled on their own,
  so `#define` lines compile together with the program that uses them; a blank
  line submits them anyway.
- `#` words are shell commands only when they name one of them
  (`#help`, `#style clang|traditional`, `#diagnostics`, `#buffer`, `#arena`,
  `#reset`, `#quit`); a preprocessor directive is source text, and an unknown
  `#word` is reported without leaving the shell.
- Every evaluation calls `compiler_diag::clear()`, compiles, renders the records
  with the selected style and then releases the session arena - the
  "reset before use" shape T3.1 provides, so `#diagnostics` can replay the last
  compile and `#buffer` can show the text the session is holding. The buffer is
  also mirrored to the gitignored `tmp_eval_file.c` so the snippet renderer can
  quote the offending line, and the file is removed on exit.
- `compiler_set_text_log_suppressed(true)` keeps the driver's traditional text
  path out of the way while the shell renders the structured form, so each
  problem is reported once. The flag is a no-op for the driver, which never sets
  it.

## Gates

| Gate | Command | Result |
| --- | --- | --- |
| REPL contract | `lpc_tests --gtest_filter=DriverTest.TestLpcshellInteractiveContract` | pass: structured error for a bad line, `#diagnostics` replay in both styles (exactly three renders of the message, no duplicate from the driver), multi-line continuation compiles, error recovery compiles the next program, unknown command reported and the shell keeps running, exit code 0 |
| Batch mode | same suite | a compiling file exits 0 with no `error:` output; a broken file exits 1 with a diagnostic naming the host path and the position |
| Full C++ regression | `build-dev-debug/src/tests/lpc_tests` | 470/470 pass |
| Full LPC regression | `cd testsuite && ../build-dev-debug/bin/driver etc/config.test -ftest` | 288 files, exit 0, `shutdown in 15 seconds` |
| ASan | `ASAN_OPTIONS=detect_leaks=1 build-asan/src/tests/lpc_tests --gtest_filter=...` | 3/3 pass, no ASan/LSan reports; a direct `build-asan/bin/lpcshell` run with `detect_leaks=1` exits 0 with no report |
| TSan | `setarch x86_64 -R build-tsan/src/tests/lpc_tests --gtest_filter=...` | 3/3 pass, no ThreadSanitizer warnings (the TSan lpc_tests spawns the TSan lpcshell) |

## Limits

- The shell compiles, it does not evaluate: loading the program, running
  `create()` or calling functions is owned by the driver and is deliberately out
  of scope for this phase.
- A diagnostic reported inside spliced macro text has a position in the expanded
  buffer, so its caret can point past the end of the source line it quotes. The
  expansion note names the macro; the caret is not re-mapped.
- The expansion chain is exact when the reported position falls inside the
  spliced text; for errors whose position lands beyond the splice (the expanded
  buffer is longer than its macro body) only the innermost level is reported
  instead of a wrong intermediate step.
