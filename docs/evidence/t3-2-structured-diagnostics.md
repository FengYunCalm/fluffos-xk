---
layout: doc
title: T3.2 structured compile diagnostics (evidence)
---
# T3.2 structured compile diagnostic records

Phase T3.2 of the lpcshell prerequisite sequence
(`docs/lpcshell-prerequisite-plan-2026-08.md`): keep a structured record of
every compile error/warning next to the rendered text the driver already
prints, so a consumer (interactive shell, tool, test) does not have to parse
log lines.

## What changed

`src/compiler/internal/diagnostic.{h,cc}` (new):

- `Diagnostic` carries severity, message, snippet position (file, line,
  column, context flag), ranges, fixits and macro `expansions` - the field set
  the rendering stack (T3.3) and the lpcshell session consume.
- Records live in a dedicated session arena
  (`ScratchArena(false, TAG_COMPILE_DIAGNOSTICS, "compile_diagnostics")` from
  T3.1). `begin_scope()` is "reset before use": opening the next compile scope
  releases the previous scope's records, so diagnostics stay readable after
  `compile_file()` returned - which is exactly what a session and the golden
  test need.
- `record()` is gated on an open compile scope. A report that has no lexer
  position (the runtime `smart_log()` path used by `apply.cc`) is not a compile
  diagnostic and is neither stored nor counted; the gate is exposed as
  `scope_active()` / `records_outside_scope()`.

`src/compiler/internal/compiler.cc`:

- `yyerror()`/`yywarn()` record the structured form (message, current file and
  line, lexer column, warning flag, `PRAGMA_ERROR_CONTEXT` state) before calling
  `smart_log()`, whose text output is unchanged.
- `compile_file()` opens/closes the diagnostic scope with the arena cycle
  (`DEFER`), so the `error()` exception path closes it too.
- `lex.cc` gained `current_line_start()` / `current_source_column()`, factored
  out of `prepare_logs()`'s context snippet, so the structured column and the
  traditional caret come from the same lexer position.

`src/base/internal/debugmalloc.h` / `src/packages/develop/checkmemory.cc`:

- New permanent-class tag `TAG_COMPILE_DIAGNOSTICS`. The records are
  driver-owned session state, not scratch: `check_memory()` whitelists the tag
  (same treatment as the pending `replace_program` queue) while
  `memory_summary()` still accounts the memory.

## Gates

| Gate | Command | Result |
| --- | --- | --- |
| Field golden | `lpc_tests --gtest_filter=DriverTest.TestCompileDiagnostics*` | pass: error severity/message/`diag_syntax_test`:2/column at the offending `;`, warning severity for `#pragma no_such_pragma`, records cleared by the next compile scope, runtime `smart_log()` not recorded |
| Full C++ regression | `build-dev-debug/src/tests/lpc_tests` | 467/467 pass |
| Full LPC regression | `cd testsuite && ../build-dev-debug/bin/driver etc/config.test -ftest` | 288 files, exit 0; the 288 per-file results are byte-identical to the pre-change run |
| ASan | `ASAN_OPTIONS=detect_leaks=1 build-asan/src/tests/lpc_tests` | 467/467 pass, no ASan/LSan reports |
| Arena unit suite | `ASAN_OPTIONS=detect_leaks=1 build-asan/src/tests/compile_arena_tests` | 8/8 pass |

## Notes

- The text path is deliberately untouched in this phase: `prepare_logs()` still
  produces the traditional `/file line N: message` + context output, and the
  full testsuite run confirms it. The format switch and the snippet/expansion
  rendering are T3.3.
- `records_outside_scope()` is a counter, not an error: it makes the
  compile-only contract observable if a future caller reports through
  `yyerror()` outside a parse.
- Message and file strings are arena copies, so a consumer must read records
  before the next compile scope opens (documented in the header).
