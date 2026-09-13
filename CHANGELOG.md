# Changelog

## Current self-use baseline (independent fork)

- Compiler: persist `DECL_NOSAVE` in the variable-type table when a global
  shadows an inherited name (#1381); restore no longer writes both lines into
  the inherited slot and leaves the child undefined.
- Compiler: run `end_new_file()` from a RAII guard so a compile that ends in
  `error()` still tears down the include stack (and its open streams/fds)
  instead of leaking them until the next compile; macro-expansion state
  (`nexpands` / `expand_depth`) is reset in the same teardown.
- External: check the CLOEXEC setup result on spawn/stdin pipes and fail the
  setup instead of silently leaving inherited descriptors.
- Tests: promise adoption propagation, the default rejection reason, and the
  destructed-object suspension refusal now have LPC-visible assertions.
- Docs: `docs/project-scope.md` documents how to verify a build from source
  now that no signed release artifact exists.
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
