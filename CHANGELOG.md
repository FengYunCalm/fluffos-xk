# Changelog

## Current self-use baseline (independent fork)

- VM/compiler: plain `x = y` on locals, globals, and parameters now emits
  specialized stores (`assign_local` / `assign_global` / `void_assign_global`)
  instead of pushing an lvalue; index/member/ref destinations keep `assign`,
  which now switches only for typed index lvalues.
- VM/compiler: `optimize_icode` now skips the `F_VOID_ASSIGN_LOCAL` operand
  byte, which it previously misread as the next instruction.
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
