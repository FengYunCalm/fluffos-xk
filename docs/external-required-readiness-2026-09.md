---
layout: doc
title: External-required readiness (P8)
---
# External-required readiness

Three items in the 2026-09 backlog need an environment this checkout cannot
provide. Each is prepared here to the point where running it is a decision, not
an engineering task: what to run, what to watch, how to roll back, and where the
result belongs.

Status vocabulary matches the project rule: `external-required` means the code
and the instructions are ready and the missing piece is an environment or an
authorization.

| Item | State | Blocked on |
| --- | --- | --- |
| Docker smoke workflow | ready to run | a machine with a working Docker daemon |
| Production capacity test | plan ready | a target environment plus authorization to generate load |
| Cross-platform CI matrix | jobs present and pinned | CI minutes; the Windows install step is a workaround |

## 1. Docker smoke

`.github/workflows/docker-smoke.yml` builds the image from the repository and
runs the driver inside it. It needs a Docker daemon; the WSL2 host used for this
work has no daemon reachable from the workspace, which is why the runs below
were never made here.

Run:

```bash
docker version                       # must succeed before anything else
gh workflow run docker-smoke.yml     # or: Actions -> docker-smoke -> Run workflow
gh run watch                          # follow the run
```

What to watch: image build completes, the container starts the driver with the
testsuite config, the smoke command runs and the container exits 0.

Rollback: nothing outside the image and the workflow run is touched; deleting the
run's image (`docker image rm <tag>`) removes the local artifact. No host state
is mutated.

Evidence to keep: the workflow run URL plus the job log, attached to
`docs/evidence/` as a text export when the run happens.

## 2. Production capacity test

No capacity runbook exists in the tree (an earlier one was removed), so this is
the procedure to execute against a real target, not a script to run here.

Prerequisites: a staging environment that mirrors production for the driver
build, config, mudlib and object count; an operator who can start and stop it;
authorization for the load.

Shape:

1. Baseline: the target's normal population, one hour of steady state. Record
   per-tick cost, owner queue depth, heartbeat drift and cleanup latencies.
2. 30-player smoke: confirm the metrics move proportionally and that no owner
   queue saturates or recompile quiescence times out.
3. 300-player run: only if the 30-player smoke holds, with the same metrics plus
   error logs checked for quiescence timeouts and rejected tasks.

What to watch: `owner_quiesce_*` counters and runtime status snapshots (they are
exposed as LPC status, so the collection is an LPC command, not a rebuild),
plus the driver log for `Owner quiesce` / `task rejected` lines.

Rollback: stop the load generator, restart the target with the previous driver
binary and config, and confirm the baseline metrics return. The run is
read-mostly but it writes player state if the target is a live world, so it
belongs on a staging copy.

Evidence to keep: the three metric snapshots, the log excerpt, and the exact
build (commit + config) under `docs/evidence/`.

## 3. Cross-platform CI matrix

The CI workflow already carries the three platform families the backlog asked
for, and the workflow contract gate pins them by name
(`tools/docs/check-workflows.py`, `EXPECTED_CI_CHECK_NAMES`):

- `macOS Debug`, `macOS RelWithDebInfo` (macos-14, Homebrew dependencies)
- `Windows Debug`, `Windows RelWithDebInfo` (windows-latest, MSYS2 shell,
  MSYS Makefiles)
- `Ubuntu Clang Debug` / `RelWithDebInfo` plus the ASan, UBSan and TSan entries

The open engineering detail is the Windows install step: on Windows the
binaries are installed by running `bash -lc "cp -f ..."` from CMake
(`src/CMakeLists.txt`, the `WIN32 AND BASH_EXECUTABLE` branch) instead of the
`install(TARGETS ...)` path used everywhere else, because the plain install path
did not produce runnable binaries. That branch depends on the MSYS2 bash being
reachable and on `cp` staying an MSYS2 tool; if the CI image changes, the
Windows job is where it shows.

Two things are true and worth stating plainly: the workaround is verified by the
Windows jobs themselves (that is the only place it runs), and the new
`lpcshell` binary is part of both install paths, so the matrix and the install
stay consistent.

Verification point: a green `Windows *` job in CI and `cmake --install` produced
a runnable `driver.exe` / `lpcshell.exe` in the install prefix. The local
equivalent checked here is `cmake --install build-dev-debug --prefix <dir>`
producing `bin/driver`, `bin/lpcc`, `bin/lpcshell`, `bin/symbol`, `bin/o2json`
and `bin/json2o`.

Rollback: none needed for matrix entries (CI-only), and the install change is a
CMake edit that reverts with the file.
