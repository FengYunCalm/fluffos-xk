---
layout: doc
title: T3.3 diagnostic rendering stack (evidence)
---
# T3.3 diagnostic rendering stack (E4)

Phase T3.3 of the lpcshell prerequisite sequence: render the structured records
from T3.2 as text, including a source snippet and the macro expansion chain.

## What changed

`src/compiler/internal/diagnostic_render.{h,cc}` (new):

- `read_source_line()` - the memchr-based port of the upstream scan: 8K read
  buffer, no per-character `fgetc`, 512-byte line cap, leading `/` stripped
  (mudlib-relative path), missing file is not an error. The slow `fgetc` shape
  upstream measured (45.4M calls to serve 153 diagnostics) is not recreated.
- `render_diagnostic(diag, style)` with two styles, chosen per call:
  - `kTraditional` - `/file line N: [Warning: ]message` plus the snippet and
    caret, i.e. the shape `prepare_logs()` prints today, reconstructed from the
    record (byte-identical for the cases the golden test covers).
  - `kClang` - `file:line:col: error|warning: message`, snippet, caret, and one
    `note: expanded from macro 'X'` line per expansion step.
- No new rc slot or pragma: the style is a per-call choice and the driver's own
  output is unchanged, so the E4 §5 compatibility requirement ("default old
  format, new format opt-in") holds without consuming a public config number.
  A consumer that wants the structured shape asks for it.

`src/compiler/internal/lex.{h,cc}`:

- Completed macro expansions are remembered (name copied into the lexer's own
  buffer, file as an owned `std::string`, newest first, bounded at 8) because a
  macro body is re-lexed after its frame is popped: by the time the parser
  reports a problem inside the body, the frame stack is empty. The recorder
  attaches the entries from the diagnostic's own line, outermost first, so a
  record inside a macro carries a real expansion chain. Object-like macros take
  the early "no arguments" return path in `expand_define2()`, which is noted as
  well - a first cut missed exactly that case.

`src/tests/bench_diagnostic_render.cc` (new target `bench_diagnostic_render`):
per-case latency of the renderer (snippet and expansion shapes, both styles).

## Gates

| Gate | Command | Result |
| --- | --- | --- |
| Golden, 20 scenarios + byte shapes | `lpc_tests --gtest_filter=DriverTest.TestDiagnosticRendering*` | 2/2 pass: 20 compile scenarios (syntax, type, pragma, unused-local, argument-count, overlapping switch, macro object/function/nested/arity, missing include, end-of-file) each rendered in both styles with location line, severity, message, snippet equal to the file's line and caret at the recorded column; plus byte-exact traditional and clang strings for a syntax error and a macro-body error with its note |
| `read_source_line` | same suite | 1-based lines, line longer than the cap is truncated, missing file returns false, `/`-prefixed and nested paths resolve, end-of-file diagnostic renders location only |
| Full C++ regression | `build-dev-debug/src/tests/lpc_tests` | 469/469 pass |
| Full LPC regression | `cd testsuite && ../build-dev-debug/bin/driver etc/config.test -ftest` | 288 files, exit 0, `shutdown in 15 seconds` (same as before the change), `Unused local variable` count identical to the pre-change runs (19) |
| ASan | `ASAN_OPTIONS=detect_leaks=1 build-asan/src/tests/lpc_tests` | 469/469 pass, no reports, no leaks |
| ASan driver | `ASAN_OPTIONS=detect_leaks=1 build-asan/bin/driver etc/config.test -ftest` | 288 files, exit 0, 0 sanitizer reports |
| Render cost baseline | `build-dev-debug/src/tests/bench_diagnostic_render` | snippet/clang ~7.4-7.9 us/case, snippet/traditional ~7.5-7.9, expansion/clang ~5.2-5.9, expansion/traditional ~4.9-5.4 (20k iterations, 3 runs) |

## Notes

- ASan caught a use-after-free in the first cut of the expansion memory: the
  remembered frame kept a raw pointer to the compiled file's name, which is
  released when that file's compilation ends while the records remain readable.
  The entry now owns its strings (`std::string`), which is why the ASan row
  above is part of the gate rather than a formality.
- The traditional rendering reconstructs the driver's shape rather than
  replacing it: `prepare_logs()` still prints during the compile, and the
  snippet text of the structured form comes from the file on disk (a macro body
  diagnostic therefore shows the expansion site line, which is what makes the
  note chain readable).
- Snippet text for lines the renderer cannot read is omitted; the location line
  is always rendered.
