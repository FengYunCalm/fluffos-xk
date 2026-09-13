# Changelog

## Current self-use baseline (independent fork)

- Windows: use `bash cp -f` for `cmake --install` to avoid “Permission denied” when replacing binaries.
- Warning cleanup: targeted fixes in scratchpad, preprocessor, ed, telnet_ext, external, and socket_efuns.
- Third‑party warnings: suppress warnings and disable IPO where needed in libevent/crypt/libwebsockets; align filesystem CMake minimum version.
- Documentation: define the independent self-use fork scope in README/README_CN,
  `docs/project-scope.md`, and the repository guidance.
- Repository hygiene: remove obsolete public-release automation, promotion material,
  and release-only planning records.

## Upstream history

See the upstream `ChangeLog` file for historical FluffOS changes:
https://github.com/fluffos/fluffos
