---
layout: doc
title: async and promises
---
# Async functions and promises

`async` functions use cooperative suspension without changing the execution
model of ordinary LPC code.

## Syntax

```lpc
async int read_value() {
    await async_yield();
    return 42;
}

async mixed load_value(promise<string> source) {
    mixed error = acatch {
        return await source;
    };
    return error;
}
```

- `async T function(...)` returns `promise<T>` to its caller. A bare `promise`
  uses an unconstrained payload.
- `await expression` is valid only in an async function. Awaiting a plain value
  continues immediately; awaiting a pending promise parks the current frame
  and resumes it from the event loop.
- `acatch expression` or `acatch { ... }` catches synchronous errors and
  rejected promises in the protected region. A synchronous `catch` cannot span
  an `await`; use `acatch` for an async boundary.

Promise settlement is delivered through the driver microtask queue. Promise
handlers therefore do not run synchronously inside `promise_resolve()` or
`promise_reject()`.

## Promise API

The core efuns are `promise_create()`, `promise_resolve()`,
`promise_reject()`, `promise_then()`, `promise_catch()`, `promise_status()`,
`promise_result()`, and `promisep()`. `promise_result()` may be called only
after settlement. `promise_status()` returns `0` for pending, `1` for
fulfilled, and `2` for rejected.

`async_yield()` returns a promise fulfilled on a later event-loop pass and is a
cooperative scheduling point. `async_info()` returns suspended coroutine
records; `async_info(1)` returns scheduler counters:

| counter | meaning |
| --- | --- |
| `suspended` | parked frames currently counted against `max suspended async functions` |
| `pending_deliveries` | reactions queued for the next drain pass |
| `drain_yields` | drain slices that handed the rest of the queue back to the event loop |
| `drain_arms_loop` | settles served by an event-loop arming instead of the tick queue |
| `queue_over_limit` | settles that queued while the backlog was already past `max pending promise deliveries` |
| `drain_eval_budget` | effective per-turn eval budget, in microseconds |

`promise_then()`, `promise_catch()`, and `async_yield()` refuse work once the
backlog reaches `max pending promise deliveries`. Internal settle sources (async
I/O completion, external process exit, adoption propagation) cannot refuse
without losing a settlement, so they keep queueing and increment
`queue_over_limit` instead; a driver that never crosses the limit leaves that
counter at zero.

The async I/O package also provides Promise companions for the callback efuns:
`async_read_promise()`, `async_write_promise()`, `async_getdir_promise()`, and
`async_db_exec_promise()` when database support is enabled. They start the same
background operations, fulfill with the callback result, and reject with the
same failure value (or database error string) instead of invoking a callback.

A parked frame is tied to its object's owner and program generation. Object
destruction, recompilation, or program replacement rejects the frame instead
of resuming stale bytecode. Ordinary legacy LPC remains synchronous and does
not become background work merely because async support is enabled.

## Suspension boundary

A frame cannot suspend while a transient reference or lvalue is pending on its
value stack. Store the value in a plain local first, then await or pass it to
another function. This keeps references valid when the frame is copied to the
parked coroutine record.
