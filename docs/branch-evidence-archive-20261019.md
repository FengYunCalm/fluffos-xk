# FluffOS_XK branch/evidence archive (20261019)

This document records the branch and detached-worktree state before final cleanup. It is an evidence index, not a replacement for source history.

## Retention decision

- The local branch set and the `origin` branch set contained only `main`; no FluffOS branch was deleted.
- Upstream remote-tracking branches were not mutated.
- Detached audit worktrees are retained because they contain raw evidence; their provenance is recorded below.
- No running driver or test process was stopped.

- Main HEAD before archive: `fbc08a1674f52a19007ad5327b2e4b5066cf5575`

## Detached evidence worktree

- Path: `/home/mechrevo/projects/fluffos-xk/audit-evidence/aud027-current-01/old-source`
- HEAD: `4cbceff53556a8492e757ae29ad3840f783c040b`
- HEAD summary: 2026-10-15 09:08:27 +0800 fix: reject ScratchArena size overflows
- Tracked status before cleanup: ` M src/tests/test_lpc_compiler.cc`
- Total status entries including untracked: 1
- This worktree is retained as raw evidence; no branch ref is removed here.

### Recent history

```text
4cbceff5 2026-10-15 09:08:27 +0800 fix: reject ScratchArena size overflows
5240da42 2026-10-15 08:33:46 +0800 fix: rewind cold ScratchArena bindings
0fcf72fe 2026-10-15 08:11:09 +0800 docs: correct build database and test instructions
e5577f99 2026-10-15 08:03:24 +0800 docs: align formatting gate scope
ef065f1a 2026-10-15 07:59:24 +0800 fix: fail closed owner production gate evidence
b9c27349 2026-10-15 07:31:14 +0800 fix vendored dependency SBOM identity
6eb6accf 2026-10-15 07:17:51 +0800 fix cmake target installation on Windows
5b15e2d0 2026-10-15 07:03:59 +0800 fix vm function table upper bound
1a8262d5 2026-10-15 06:43:50 +0800 fix: preserve errors in discarded catch expressions
4be03de3 2026-10-15 05:25:26 +0800 fix compiler protected await contexts
0bd9ce1b 2026-10-15 04:55:41 +0800 fix(db): close backend resources on failure
63aedcad 2026-10-15 04:12:08 +0800 fix(build): validate database backend IDs
9802755c 2026-10-15 03:34:51 +0800 fix(config): reject truncated runtime lines safely
1250f41f 2026-10-15 02:11:52 +0800 Fix get_char UTF-8 and control-byte delivery
de215c82 2026-10-15 01:19:43 +0800 fix: preserve telnet text across input chunks
93e47eb4 2026-10-15 00:17:03 +0800 docs: reconcile remediation commit status
a0e9d1d5 2026-10-15 00:11:23 +0800 fix(runtime): defer operation signal work to safe points
7173f1b6 2026-10-14 22:58:00 +0800 fix reclaim promise-owned graphs
e39a101a 2026-10-14 13:15:21 +0800 fix portable evaluation deadlines
e8ceb350 2026-10-14 08:14:12 +0800 fix: redact noecho connection debug logs
```

### Diffstat against FluffOS main

```text
(none)
```

### Changed paths against FluffOS main

```text
(none)
```

## Detached evidence worktree

- Path: `/home/mechrevo/projects/fluffos-xk/build-modernization-evidence/audit-u02-baseline-src`
- HEAD: `34497c8b389e78b04b3322a41b7c201577724c98`
- HEAD summary: 2026-10-13 13:03:28 +0800 fix: preserve wide member_array indices
- Tracked status before cleanup: ` M src/comm.cc
 M src/comm.h
 M src/tests/test_lpc_gateway.cc`
- Total status entries including untracked: 4
- This worktree is retained as raw evidence; no branch ref is removed here.

### Recent history

```text
34497c8b 2026-10-13 13:03:28 +0800 fix: preserve wide member_array indices
43790802 2026-10-13 09:37:21 +0800 test: cover fortified fprintf fault injection
22ffd85f 2026-10-13 09:36:17 +0800 fix: guard empty input tail trimming
ead517ad 2026-10-13 08:50:23 +0800 test: persist targeted CI evidence
d60bf57f 2026-10-13 08:31:59 +0800 fix(testsuite): classify debug-only LPC coverage
69ce2a2f 2026-10-13 08:23:46 +0800 fix(testsuite): wait for asynchronous database cases
57bf8a02 2026-10-13 08:15:44 +0800 fix(testsuite): accept percent-encoded LPC case names
f3317359 2026-10-13 06:02:58 +0800 docs: expand fork audit findings and remediation plan
55c7360b 2026-10-12 14:51:42 +0800 docs: record fork audit findings and remediation plan
a4475b1a 2026-10-12 01:47:54 +0800 Finalize local modernization audit
5f5b5ad8 2026-10-11 22:35:28 +0800 refactor compiler grammar rule implementations
69eb4814 2026-10-11 21:58:14 +0800 fix compiler benchmark relative output paths
bf9a0236 2026-10-11 21:36:15 +0800 maint: refresh Alpine build and vendor records
31141ff7 2026-10-11 21:06:46 +0800 style: complete repository format gates
b52a4611 2026-10-11 21:04:19 +0800 fix: make libevent SSL fd callbacks type-safe
8960c6ac 2026-10-11 18:49:04 +0800 fix: limit UBSan null suppression to GCC
1e7988d7 2026-10-11 17:52:37 +0800 Fix UTF-8 socket text fragmentation
8be287ec 2026-10-11 13:33:50 +0800 refactor owner task identity construction
956b04ab 2026-10-11 12:51:51 +0800 refactor owner status serialization
20acfcc6 2026-10-11 12:23:27 +0800 optimize object store path index lifecycle
```

### Diffstat against FluffOS main

```text
(none)
```

### Changed paths against FluffOS main

```text
(none)
```
