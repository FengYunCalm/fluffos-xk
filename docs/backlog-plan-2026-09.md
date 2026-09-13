# Backlog plan / 待办事项执行方案（2026-09-13）

> 状态：**已执行**。本文档记录四项遗留事项的深度分析结论、执行的修复，以及
> 明确不做的事。执行方式：先定位后修改；每项修复都有可复现的验证数据。

## 1. 异步资源退出清理缺口

### 定位（LeakSanitizer 实测，当前代码）

| 部分 | 结论 | 证据 |
| --- | --- | --- |
| **async**（Request/buffer） | **已不存在**。旧证据 `docs/evidence/asan-async-lsan.txt`（2026-08-17，`complete_all_asyncio` 改造前）记录的 6 处泄漏在当前代码下**全部消失**：`complete_all_asyncio()` 已是循环等待，注释明确处理"回调提交新请求"的情形 | LSan 重跑 `async` 测试：0 泄漏 |
| **TLS/socket**（SSL_CTX） | **存在且已修复**。`lpc_socks_closeall()` 此前只调 `evutil_closesocket()` 关 fd，绕过了 `socket_close()` 的完整清理——TLS 上下文（`tls_server_init()` 创建，1616 字节 direct + 约 10KB 关联 OpenSSL 结构）、SSL 对象、libevent watch 全部泄漏 | LSan 重跑 `socket_tls_server`：修复前 1616B direct + 多个 indirect；修复后 **0 报告** |

### 修复

`lpc_socks_closeall()` 现在对每个活跃条目调用
`socket_close(fd, SC_FORCE | SC_FINAL_CLOSE)`（无回调），复用既有的
`SSL_CTX_free` / `event_free` / 缓冲区释放路径。`SC_FORCE` 跳过 owner 检查
（退出时无 `current_object`），`SC_FINAL_CLOSE` 允许 FLUSHING 条目。

### 伴随发现（TSan，非泄漏）

`async.cc` 的 `thread_func` 从工作线程调用 `add_walltime_event()` →
libevent `event_new()`，与主线程事件循环竞争 libevent 的调试标志
`event_debug_mode_too_late`（`event.c:267`）。危害极小（调试标志，不承载数
据），但调用模式确实违反 libevent 线程模型。**记录为已知限制**，修复需要改
变工作线程→事件循环的交接方式，风险高于收益，单独立项。

## 2. 上游 332 提交的选择性吸收

### 画像（277d0b1c..upstream/master，332 提交，0 merge）

- 主题：fix×63、compiler×25、docs×23、vm×16、thirdparty×13、ci×6、其余分散。
- 高频改动文件里 `lexer_utils.cc`(31)、`grammar_rules_*.cc`(23/22/13)、
  `lexer_rules_pp.*`(22/13) 是**上游 Flex 重构的产物，本库不存在**——这批
  提交结构性不适用（与 `docs/upstream-sync-evidence-2026-08.md` 的 #1210
  不适用清单一致）。

### 本轮核实与执行

| 上游提交 | 核实结果 | 动作 |
| --- | --- | --- |
| `7a471202` ops.cc compound-assign 不清 lvalue subtype | **本地未修**（0 处 `argp->subtype = 0`），`f_xor_eq` 连结果 subtype 也没清 | **已实施**：10 处修复 + 新测试 `compound_assign_undefined.lpc` |
| `990f7b12` vm 反向 local-index 守卫 | **本地已正确**（5 处 `lval - fp`，无反向写法） | 无需动作 |
| `d41f000f` #1394 Coverity 四项 | CLOEXEC **已吸收**；icode REVERSE_INULL 本地写法更保守**不适用**；cancelled-reason 本地无该功能**不适用** | 部分已吸收（前一批） |
| `070724d1` #1385 total_lines | **不适用**：本地 `simulate.cc` 在 load 后消费并清零，`compile_file` 不保存/恢复，无上游的归零 bug | 无需动作 |
| `de945701` async/await Coverity | **不适用**：涉及 `cycles.cc`，本库无此文件（#1276 产物） | 无需动作 |

### 后续吸收策略

后续按"先核实、后移植"执行，禁止直接 cherry-pick：每个候选先用
`git show <hash> -- src` 拿到 diff，再 `grep` 本地对应实现确认缺陷存在与
落点，最后按本地架构重写。优先级排序：

1. 本地同源文件的明确缺陷（ops/socket/efuns_main 等，冲突面小）；
2. 上游 fuzz/audit 批次（`b1fb96f3`、`bf8861c9`、`8b0aee8a`、`d9171788`）
   逐条对照 `// #1247` 矩阵，跳过已覆盖项；
3. Flex 重构产物与其依赖——结构性不适用，跳过。

## 3. pr-1237 / pr-1276 分支合并评估

| 维度 | pr-1237 | pr-1276 |
| --- | --- | --- |
| 领先 main | 108 提交 | 219 提交 |
| 规模 | 精确 diffstat 未能获取：分支引用的 blob 未在本地（partial clone），fetch 被网络阻断 | **3999 文件，+188,580 / -361,953** |
| 内容 | 上游历史为主（Flex 迁移、Emscripten、recompile_object） | 上游历史 + 循环引用运行时/orphan collector |
| 与本地冲突面 | 覆盖本地全部改动域（compiler/vm/net/CMake） | 同左，且含 docs 站迁移 |

**结论：不做分支合并。** 两者本质是"与上游对齐"的大工程，与自用 fork 的
演进方向冲突（本地保留旧 lexer、自建 Promise、自用定位）。#1276 中的
orphan collector 若有需要，按第 2 节的核实流程单独移植，不走分支合并。
`pr-1237` 的 blob 需在网络可达上游时执行一次 `git fetch upstream` 方能做
精确 diff——仅当决定评估其具体内容时再做。

## 4. 审计 P9（external_kill）——结论更正

**上一轮审计报告的判断有误**：`external_kill` 在本地**已完整存在**——
实现（`external.cc` `f_external_kill()`，与上游逐字一致）、spec
（`int external_kill(int);`）、文档（`docs/efun/external/external_kill.md`，
随 `722d8812` 引入）三者齐全。当时的审计子代理无法运行 git，把
`722d8812` 中 `docs/efun/external/index.md` 的行数变化误读为功能缺失。

P9 中剩余有效的只有两条代码质量建议（取消路径的顺序敏感、TLS 测试平台割
裂），记录于 CHANGELOG 与本文件，不再作为待办。

## 验收记录

| 验证 | 结果 |
| --- | --- |
| LSan `async` 定向（当前代码） | 0 泄漏 |
| LSan `socket_tls_server` 定向（修复后） | 0 泄漏（修复前 1616B direct + ~10KB indirect） |
| 完整 LPC 套件 | 见本次提交说明（0 失败） |
| C++ 测试 | 457/457 |
| ops subtype 新测试 | `compound_assign_undefined.lpc` 通过 |
