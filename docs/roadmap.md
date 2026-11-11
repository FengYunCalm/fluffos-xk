# 路线图与当前状态

> 本文是仓库**唯一**的"现在是什么状态、下一步做什么"来源。
> 历史计划与实施记录在 [`archive/plans/`](archive/plans/README.md)，只作历史证据，
> 不得据其判断当前代码行为。
> 维护规则：条目只有三种状态——`verified`（本节有可复现命令且已执行）、
> `unverified`（存在但未在本机验证）、`external-required`（需要本仓之外的平台或环境）。
> 任何"完成"声明必须同时给出命令与结果，否则只能是 `unverified`。

## 1 当前状态

基线：`main` 分支，本轮整理从 `1f3569f6` 开始，方案见
[2026-11 整理方案](refactor-plan-2026-11.md)，问题依据见
[2026-11 问题清单](refactor-findings-2026-11.md)。

| 项 | 状态 | 证据 / 说明 |
| --- | --- | --- |
| Debug 构建（driver/lpcc/lpcshell/lpc_tests/ofile_tests 等） | verified | `cmake --preset dev-debug -B build-refactor-2026-11` + `cmake --build ... --parallel 2` |
| CTest | verified | 598/598 通过（60s），基线记录见 `audit-evidence/refactor-2026-11/U0-ctest.log` |
| LPC testsuite（`-ftest`） | verified | 退出码 0，"Checks succeeded"，433 个测试文件 |
| 格式门禁（clang-format 18.1.8） | external-required | 本机未安装该版本；`tools/style/check-format.py` 明确报错退出，不用其他版本替代 |
| clang-tidy 门禁 | unverified | `src/.clang-tidy` 尚未接入 CI（本轮 U9c 处理） |
| ASan / UBSan | unverified | 本轮整理未运行；改动涉及 VM 热路径时按方案 U6/U7 验证矩阵执行 |
| TSan | external-required | 历史记录为本机工具链 `unexpected memory mapping` |
| Windows / macOS 构建 | external-required | 由 CI 矩阵覆盖，本机无平台 |
| 真实下游 mudlib（xiakexing） | unverified（每次发版前必验） | 本轮 U5d 用 `ownercheck full` 对比删键前后 |
| 上游位置 | `upstream/master` = `38a718f4`；merge-base `277d0b1c`（2026-01-18） | 落后 390 / 领先 796 |

## 2 待办

### 2.1 本轮整理未完成单元

以[方案](refactor-plan-2026-11.md)的执行记录表为准；未打勾的单元即未完成。
不要在本文件复制单元细节，避免第二份待办清单。

### 2.2 本轮不做、需要单独方案

| ID | 事项 | 为什么推迟 | 开始前需要 |
| --- | --- | --- | --- |
| R-01 | 对象生命周期状态收口（`O_*` 位 + `destructed_objects` + `ObjectRecord.destructed` + shard tombstone） | 三处描述同一事实，合并要改 object store 与 destruct 的写入顺序 | 设计文档 + sanitizer soak + 下游 smoke |
| R-02 | funptr 生命周期状态收口（`lifecycle_state` + 三条链表 + 事务 pin） | 与重编译事务、弱注册表、出生 journal 强耦合 | 事务失败注入用例集 |
| R-03 | parser 全局状态改为会话对象（14 个文件静态量） | 包级重构；先要有嵌套 parse 的失败复现 | 失败用例 + 上游/下游兼容评估 |
| R-04 | Worker 总线观测面缩减（`vm_worker.cc` ≈108 键） | 有真实下游消费者（`worker_bus_d`、`async_compute_d`），需逐键确认 | 下游使用清单 + 兼容策略 |
| R-05 | 上游 typed efun 绑定（`EfunArgs`/`Value`/`CallResult`，隐藏 `st_num_arg`/`sp`） | 架构迁移，与本地 owner/promise 路径交叉 | 迁移设计 + 全 efun 回归 |
| R-06 | 上游 hooks（function/efun/net/around 连接点） | 新扩展面，需与 owner admission 一起设计 | 设计文档 |
| R-07 | 上游 fused store（复合赋值/`++`/索引目标的融合写） | 与本地 managed lvalue/async 实现冲突 | 等价性分析 + 性能对照 |
| R-08 | WebSocket TLS 热重载 | 当前明确拒绝，属安全敏感变更 | 设计文档 + 真机握手验证 |
| R-09 | PCRE2 新 API（`pcre_config`/`pcre_info`/`pcre_convert` 等） | 本地 `.spec` 未公开这些 efun，不能把不存在接口伪装成覆盖 | API 决策 + 回归 |

### 2.3 上游待吸收

见[方案](refactor-plan-2026-11.md) U4 清单；其中 9 个提交仍需 `git show` 判定本地是否等价。

## 3 如何验证

最小验证集（按改动类型选择，细则见 `AGENTS.md` §5）：

```bash
# 构建（同一时刻只用一个构建目录；可用内存 < 3 GiB 时用 --parallel 2）
cmake --preset dev-debug -B build-refactor-2026-11
cmake --build build-refactor-2026-11 --target driver lpcc lpc_tests --parallel 2

# C++ 回归
ctest --test-dir build-refactor-2026-11 --output-on-failure

# LPC 回归（必须从 testsuite/ 运行）
cd testsuite && ../build-refactor-2026-11/bin/driver etc/config.test -ftest

# 文档与门禁
python3 tools/docs/check-docs.py
python3 tools/style/check-format.py --all-native   # 需要 clang-format 18.1.8
```

## 4 文档索引

| 文档 | 用途 |
| --- | --- |
| [architecture.md](architecture.md) | 模块地图、主调用链、不可破坏合同 |
| [coding-standard.md](coding-standard.md) | 代码规范（唯一风格来源） |
| [owner-status-contract.md](owner-status-contract.md) | LPC 可观测状态键的冻结契约 |
| [project-scope.md](project-scope.md) | 项目范围与默认交付 |
| [owner-multicore-api.md](owner-multicore-api.md) | owner 多核 API 合同 |
| [multicore-runtime-v4.md](multicore-runtime-v4.md) | 运行时硬化基线 |
| [multicore-production-gate.md](multicore-production-gate.md) | 运行时验收口径 |
| [recompile-object-v2-design-2026-08.md](recompile-object-v2-design-2026-08.md) | 重编译事务设计 |
| [archive/plans/](archive/plans/README.md) | 历史计划与实施记录（只读） |
