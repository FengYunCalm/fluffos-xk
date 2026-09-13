# FluffOS_XK Multicore Runtime

This page is the stable entry point for FluffOS_XK multicore runtime documentation.
The project is an independent fork maintained primarily for self-use; the
runtime contracts below are engineering contracts, not public release gates.
The historical v1 runtime note has been archived at
[`docs/archive/multicore/multicore-runtime-v1-2026-06.md`](archive/multicore/multicore-runtime-v1-2026-06.md).

## Current Status

FluffOS_XK multicore runtime work is baseline-complete for the current owner/service
executor model used by this fork. The current contract is not arbitrary background
execution for every legacy LPC method. Ordinary legacy LPC remains default-closed;
only explicit allowlist paths, same-owner execution, driver callback tasks,
frozen payloads, ObjectHandle routing, and owner/service shard contracts may enter
the owner executor.

The current runtime facts are recorded in these documents:

- [`multicore-runtime-v2.md`](multicore-runtime-v2.md): owner runtime v2 and production-perfect contract.
- [`multicore-production-gate.md`](multicore-production-gate.md): runtime contract fields, evidence model, and acceptance scope.
- [`owner-multicore-api.md`](owner-multicore-api.md): owner/snapshot API guide for downstream mudlibs.
- [`project-scope.md`](project-scope.md): self-use fork scope and local acceptance rules.

## Historical Plans

Older multicore plans and v1 notes are retained for technical history under
[`docs/archive/multicore/`](archive/multicore/README.md). They are not current execution
plans and must not be used to infer unfinished production work.
