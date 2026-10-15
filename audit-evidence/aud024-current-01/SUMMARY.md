# AUD-024 current validation

- **Scope:** `docs/build.md` only. No source, CMake, generated artifact, or runtime change.
- **Contract cross-check:** `src/CMakeLists.txt` defines `PACKAGE_DB_*` values as backend type IDs and `PACKAGE_DB_DEFAULT_DB` as the selected enabled type ID. `src/packages/db/CMakeLists.txt` rejects non-positive IDs, duplicate enabled IDs, and a default ID that is not enabled.
- **Documentation result:** SQLite-only examples use `PACKAGE_DB_MYSQL=""`, `PACKAGE_DB_SQLITE=1`, and `PACKAGE_DB_DEFAULT_DB=1`. The multi-backend example uses MySQL `1`, SQLite `2`, and default `1`. The page no longer describes the IDs as SQLite API versions.
- **Command result:** Build examples use fixed ordinary `-j4`, sanitizer `-j2`, or the Windows `--parallel 1` cap. Build-to-testsuite blocks use `cd ../testsuite` before `../build/bin/driver`.
- **Validation:** `static-validation.log` records the source-contract scan, semantic assertions, `python3 -B tools/docs/check-docs.py --repo . --dir docs` (`1140` Markdown files), and `git diff --check -- docs/build.md`.
- **Not run:** configure, install, driver, C++/LPC, and sanitizer tests are not applicable to this documentation-only unit; database runtime validation remains covered by AUD-014.
