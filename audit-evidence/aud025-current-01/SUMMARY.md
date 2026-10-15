# AUD-025 验收摘要

## 结论

AUD-025 的生产修复和定向运行时验收已完成。冷 binding 在首次分配后销毁时会清零懒创建的 `base_`，并把 `current_` 恢复为可复用的 `base_`；live/retained chunk 链以及 `bytes_live`、`cycle_bytes` 统计保持一致。

本卡没有修改普通分配、嵌套 LIFO 拒绝、generation 检查或 oversize chunk 策略。

## 根因与修复

- 原实现只在 `mark.chunk != nullptr` 时回退游标。
- 冷 binding 的 mark 为 null；首次分配创建的 `base_` 不在 `live_` 链中。
- 析构后原实现把 `current_` 留为 null，且没有扣除 `base_->used`；下一次 `alloc()` 解引用空 `current_`，并留下错误计量。
- 修复在空起始 mark 且存在 `base_` 时清零 `base_->used`，累计释放量，并设置 `current_ = chunk ? chunk : base_`。
- `src/compiler/internal/compile_arena.h` 已补充冷启动 binding 生命周期合同。

## 回归覆盖

新增并实际执行生产入口的三个测试：

- `CompileArenaTest.AuditColdBindingRewindsBaseAndRemainsUsable`
- `CompileArenaTest.AuditColdBindingReleasesOverflowChunks`
- `CompileArenaTest.AuditColdBindingRewindsDuringException`

覆盖冷启动、live overflow 回收、异常析构和下一次分配/统计恢复。原有 nested、out-of-order、oversize、begin/reset 测试继续运行。

## Fail-first

`fail-first-exception/` 保留修复前旧实现的同一完整 `CompileArenaTest.AuditColdBinding*` filter：

- 发现 3 个用例。
- 运行先报告 `bytes_live == 128`、`cycle_bytes == 128` 未回退。
- 随后的下一次分配触发 SIGSEGV；直接运行退出码为 139。
- targeted runner 返回非零并保留失败日志。
- 构建二进制和私有运行副本 SHA-256 一致。

首次修后 Debug runner 因仓库中大量历史 `audit-evidence/` 未跟踪文件导致 runner 的 10 秒 Git manifest 扫描超时。没有把这次基础设施超时记为测试失败；使用只在 runner 子进程中生效的 Git wrapper 排除 runner 已明确不纳入身份哈希的 `audit-evidence/` 和 `evidence/` 目录后重跑，完成了有效 runner 验收。wrapper 未修改源码或 Git 配置。

## 定向结果

每个构建变体均使用私有、字节一致的二进制副本运行 targeted runner；发现数、执行数和通过数一致，无跳过：

| 变体 | 冷 binding filter | 完整 `compile_arena_tests` | 配置 |
| --- | ---: | ---: | --- |
| Debug | 3/3/3，0 skip | 11/11 | `PACKAGE_PCRE=OFF` |
| ASan | 3/3/3，0 skip | 11/11 | `PACKAGE_PCRE=OFF` |
| UBSan | 3/3/3，0 skip | 11/11 | `PACKAGE_PCRE=OFF` |

各变体的 targeted runner 和 CTest 原始日志位于对应的 `debug-final/`、`asan-final/`、`ubsan-final/` 子目录。ASan/UBSan 运行日志未出现 sanitizer 报告。

## 边界与未得证据

- 当前主机没有 PCRE2 开发文件；为构建不相关的 `compile_arena_tests` 目标，三种变体均显式使用 `PACKAGE_PCRE=OFF`。没有据此宣称默认 PCRE 全包构建通过。
- 仓库没有可用的 clang-format 18.1.8。`tools/style/check-format.py` 因缺少指定版本无法执行；`git diff --check` 已通过。该格式化器检查仍是环境缺口，不伪造为通过。
- 没有运行 driver、LPC testsuite、TSan 或真实外部平台测试；本卡只涉及 C++ ScratchArena，未将这些未执行项目计入验收。
