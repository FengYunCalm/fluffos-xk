# AUD-026 验收摘要

## 结论

AUD-026 已实施并完成本地 Linux/WSL2 验收。修复覆盖 `ScratchArena` 的对齐加法、Chunk 头部加法、统计加法、剩余空间判断和字符串终止字节加法。极端尺寸现在在 arena 状态改变前抛出 `std::bad_alloc`；`alloc(0) == nullptr` 保持不变。

本摘要不把本地结果扩展为默认 PCRE2 全包、Windows、真实下游 mudlib 或长期压力验证。

## 修复前证据

- `fail-first/fail-first-build-and-run.log.gz`：修前 Debug 构建与定向 runner 的原始输出（gzip 保持原始字节），发现并执行 1 个 `CompileArenaTest.AuditAllocationOverflowIsRejected`，测试失败。
- 原因与源码一致：未检查的 `size_t` 加法把大尺寸变为可接受的小请求，计量回绕，后续分配与前一块重叠。
- 修前未执行巨量内存写入。

## 实施内容

- `src/compiler/internal/compile_arena.cc`：新增统一 `checked_size_add`；校验对齐、Chunk 头部、统计和 `len + 1`；用 `need > size - used` 判断容量；oversize 校验早于惰性 base 创建。
- `src/compiler/internal/compile_arena.h`：记录溢出或分配失败抛 `std::bad_alloc` 且不改变 arena 状态的合同。
- `src/tests/test_compile_arena.cc`：覆盖零尺寸、对齐/头部邻界、`SIZE_MAX`、字符串终止字节、状态不变和后续小分配。

## 修后验证

每个变体均使用隔离的私有二进制副本和 targeted runner，发现数=执行数=通过数=1，失败=0，跳过=0；完整 `compile_arena_tests` 为 12/12：

- `debug-final-02/validation.log`：Debug 定向通过，完整套件 12/12。
- `asan-final-02/validation.log`：ASan 定向通过，完整套件 12/12。
- `ubsan-final-02/validation.log`：UBSan 定向通过，完整套件 12/12。
- `static-validation-02.log`：`git diff --check`、源码 100 列检查通过；当前环境没有 `clang-format`，未伪造格式化器结果。

三种构建均使用 `PACKAGE_PCRE=OFF`。未执行默认 PCRE2 全包构建。

## 证据边界

- 验证平台：当前 Linux/WSL2 工作区。
- 未验证：Windows、其他真实平台、默认 PCRE2 全包、真实下游 mudlib、长期压力和巨量实际分配。
- 未修改或启动既有 driver；未执行部署、发布或外部环境操作。
