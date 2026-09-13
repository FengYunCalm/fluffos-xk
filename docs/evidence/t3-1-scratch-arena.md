---
layout: doc
title: T3.1 ScratchArena session scopes (evidence)
---
# T3.1 ScratchArena + ScratchArenaBinding

Phase T3.1 of the lpcshell prerequisite sequence
(`docs/lpcshell-prerequisite-plan-2026-08.md`): give the arena a session-shaped
scope model so a long-lived consumer can keep compile output readable across
cycles, without changing what the compiler itself does.

## What changed

`src/compiler/internal/compile_arena.{h,cc}`:

- The chunk pool is now a `ScratchArena` object. The compiler keeps using the
  process-wide instance through the unchanged free functions
  (`begin()`/`end()`/`alloc()`/`alloc_string()`/statistics), which is exactly
  the previous behaviour; a session creates its own arena.
- `ScratchArena::begin()` is "reset before use": it releases what the previous
  cycle left live and reports how many bytes that was
  (`cross_cycle_bytes()`). A session therefore allocates diagnostics, renders
  them after the cycle, and drops them when the next cycle starts - the
  lifetime shape lpcshell needs.
- `ScratchArenaBinding` is a nested scope. Destroying it releases every chunk
  allocated inside it and rewinds the bump cursor of the chunk it started in,
  so small allocations inside a binding really become reusable.
- Bindings are registered as marks in the arena (chunk + cursor + live-chain
  head) instead of raw pointers into binding objects, and each binding records
  the arena's cycle generation. Out-of-order destruction and bindings that span
  a cycle boundary are refused: they release nothing, increment
  `binding_order_violations()` / `binding_marks_abandoned()` (reported through
  `debug_message`) and leave the arena usable rather than handing out storage a
  live inner binding still owns.
- `~ScratchArena()` returns everything the arena malloc'd (the process-wide
  instance keeps its BSS base chunk); `reset()` additionally drops the retained
  pool and a session's base chunk.

## Gates

| Gate | Command | Result |
| --- | --- | --- |
| Unit coverage | `build-dev-debug/src/tests/compile_arena_tests` | 8/8 pass |
| Sanitizer | `ASAN_OPTIONS=detect_leaks=1 build-asan/src/tests/compile_arena_tests` | 8/8 pass, 0 ASan/LSan reports |
| Compiler regression (C++) | `build-dev-debug/src/tests/lpc_tests` | 466/466 pass |
| Compiler regression (LPC) | `cd testsuite && ../build-dev-debug/bin/driver etc/config.test -ftest` | 288 files, exit 0 |
| ASan compiler regression | `ASAN_OPTIONS=detect_leaks=1 build-asan/src/tests/lpc_tests` | 466/466 pass, no leaks |

The unit suite covers: session/compiler arena isolation, cross-cycle survival
plus reuse of the released storage, binding release of only its own storage
(with content intact before and after), bump-cursor rewind inside one chunk,
nested LIFO release, refused out-of-order destruction (arena intact, later
bindings still work, abandoned mark dropped at the cycle boundary), oversize
chunks not entering the retained pool, `reset()` idempotence and reusability,
and NUL termination plus aligned accounting of `alloc_string()`.

## Notes

- `lpc_tests` gained a new binary (`compile_arena_tests`) registered with
  `gtest_discover_tests`, so the standard `ctest` run picks it up.
- The compiler's observable behaviour is unchanged: `cycle_bytes()`,
  `peak_cycle_bytes()`, `chunk_mallocs()`, `reset_count()`,
  `retained_chunks()` and `retained_heap_bytes()` (the `mud_status()` and
  benchmark surface) keep their previous meaning and accounting units.
- Binding depth is unbounded (marks are a small vector); the discipline is
  ordering, not depth.
