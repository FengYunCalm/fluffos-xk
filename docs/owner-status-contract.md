# LPC 可观测状态契约

> 本文是驱动对外暴露的 status mapping 键的**冻结清单**。
> 只有本表登记的键可以被 `add_mapping_*` 写入 owner 可观测面；新增键必须先改本表。
> 校验：`tools/docs/status-contract.py`（以 `--check` 模式运行；`--dump` 打印当前键）。
> 依据：下游 `xiakexing` `main`（`f6acdd30e`）逐键实测读取，`rewrite`（`661fdfc60`）
> 不消费任何 owner 状态。详见 [2026-11 问题清单](refactor-findings-2026-11.md) §5.4。

## 1 冻结键（51）

### 1.1 `vm_owner_thread_status()` — 19

| 键 | 含义 |
| --- | --- |
| `enabled` | owner 线程是否已启动（0/1） |
| `thread_count` | 当前 owner 线程数 |
| `max_owner_threads` | 允许的 owner 线程上限 |
| `thread_dispatched` | 在 owner 线程上派发的任务累计数 |
| `main_dispatched` | 主线程队列派发的任务累计数 |
| `thread_message_dispatched` | owner message 在 owner 线程派发的累计数 |
| `thread_context_bound` | 成功绑定 VMContext 的 owner 线程任务累计数 |
| `thread_object_store_isolated` | 拒绝在非主线程同步 object store 的累计数 |
| `thread_owner_bound` | 成功绑定 owner/epoch 的累计数 |
| `thread_owner_cleared` | owner 绑定被正确清除的累计数 |
| `thread_lpc_rejected` | 后台 LPC 执行被拒绝的累计数 |
| `thread_owner_state_guarded` | 单 owner 状态写守卫命中累计数 |
| `executor_safe_task_dispatched` | 经 owner executor 安全路径派发的累计数 |
| `executor_runnable_task_dispatched` | 经 runnable 路径派发的累计数 |
| `thread_lpc_probe_executed` | 受控 LPC probe 实际执行的累计数 |
| `thread_lpc_probe_guarded` | LPC probe 被守卫拒绝的累计数 |
| `thread_lpc_probe_failed` | LPC probe 执行失败的累计数 |
| `thread_lpc_canary_succeeded` | LPC canary 成功次数 |
| `thread_lpc_task_succeeded` | 已注册 LPC task 成功次数 |

### 1.2 `vm_owner_runtime_status()` — 13

| 键 | 含义 |
| --- | --- |
| `lpc_task_allowlist` | 允许在 owner 线程执行的 LPC 方法名数组 |
| `registered_owner_task_domains_ready` | 已注册 owner 任务域是否满足要求（0/1） |
| `registered_owner_task_domain_count` | 已注册 owner 任务域数量 |
| `gateway_command_execute_ready` | gateway 命令是否可经 executor 执行（0/1） |
| `target_owner_message_executor_ready` | 目标 owner message 是否可经 executor 投递（0/1） |
| `service_shard_executor_ready` | service shard executor 是否就绪（0/1） |
| `ordinary_lpc_default_closed` | 普通 LPC 后台执行默认关闭（恒为 1） |
| `normal_path_main_fallback_count` | 正常路径退回主线程的累计数 |
| `owner_quiesce_attempts` | 重编译静默尝试次数 |
| `owner_quiesce_success` | 静默成功次数 |
| `owner_quiesce_timeouts` | 静默超时次数（真实负载下超时排查依赖此值） |
| `owner_quiesce_admission_rejected` | 静默期间被拒绝的准入次数 |
| `owner_epoch` | 当前 owner epoch |

### 1.3 邮箱 / 调度 / 清理 — 7

| 键 | 来源 | 含义 |
| --- | --- | --- |
| `owner_queue_depth` | `vm_owner_mailbox_status()` | 该 owner 邮箱待处理任务数 |
| `queue_depth` | `vm_owner_mailbox_status()` | 同上（兼容键，下游两处都读） |
| `drained` | `vm_owner_drain()` | 本次 drain 取出并派发的任务数 |
| `purged` | `vm_owner_purge()` | 本次清除的任务数 |
| `remaining` | drain / purge / schedule | 处理后仍剩余的任务数 |
| `dispatched` | `vm_owner_schedule()` | 本次调度派发的任务数 |
| `tasks` | drain / purge / schedule | 任务记录数组 |

### 1.4 任务与事件记录 — 5

| 键 | 含义 |
| --- | --- |
| `task_id` | 任务 id |
| `owner_id` | 任务归属 owner |
| `task_type` | 任务类型（`command` / `lpc` / `owner_message` …） |
| `task_key` | 任务键（调用方自定义，用于断言身份） |
| `state` | 任务状态文本（见 §3 状态词汇） |

### 1.5 `vm_owner_status()` — 6

| 键 | 含义 |
| --- | --- |
| `object` | 目标对象文件名 |
| `owner_id` | 当前 owner id |
| `default_owner_id` | 默认 owner id（`legacy/main`） |
| `total_checks` | owner 校验总次数 |
| `mismatch_checks` | owner 不匹配次数 |
| `has_explicit_owner` | 是否显式设置过 owner（0/1） |

### 1.6 guard / trace — 2

| 键 | 来源 | 含义 |
| --- | --- | --- |
| `success` | `vm_owner_guard*()`、gateway output quiesce | 操作是否成功（0/1） |
| `events` | `vm_owner_trace()` / `vm_owner_access_trace()` / `vm_owner_message_trace()` / `vm_owner_commit_trace()` | 事件数组，元素含 §1.4 的字段 |

## 2 不在本契约内（本轮不动）

- **Worker 总线**（`vm_worker.cc`，≈108 键）：`worker_bus_d` / `async_compute_d` 在读，
  属功能合同，缩减需单独课题（roadmap R-04）。
- **`vm_object_handle()`** 返回的 handle 结构：`rewrite` 在用，保留。
- 测试专用 `*_for_test()` 钩子：不是 LPC 状态面。

## 3 状态词汇（trace / task state）

内部状态用枚举表达，对外仍输出下列字符串，二者由 `*_state_name()` 映射保持同步：

`accepted`、`accepted_dispatch_rejected`、`rejected`、`rejected_backpressure`、
`rejected_callback_not_allowlisted`、`dropped_callback_invalid_target`、
`completed`、`failed`、`cancelled`、`timed out`、
`thread_dispatched`、`thread_lpc_rejected`、`thread_owner_state_guarded`、
`thread_message_dispatched`。

新增状态词必须同时改本节与枚举映射。

## 4 2026-11 已移除

### 4.1 对象存储面（已完成）

`vm_object_store_status()` / `vm_object_store_owner_status()` 不再输出下列类别的键：

- **shard 合同表**（原嵌套 `vm_object_shard`，35 键）：`shard_kind`、`status_model`、
  `execution_model`、`directory_model`、`storage_model`、`*_source`、`*_ready`、
  `*_complete`、`*_blocker`、`*_consistent`、`contract_version`。
- **owner-local 生命周期合同**（11 键）：`owner_local_lifecycle_contract_version`、
  `owner_local_{lookup_resolve,create_canonical,move_canonical,destruct_canonical,
  deferred_destruct}_ready`、`owner_local_lifecycle_{ready,blocker}`、
  `global_index_physical_retirement_{ready,blocker}`。
- **迁移桥接状态**：`uses_global_object_table`、`global_index_bridge`、
  `global_{live_object,record}_bridge_{ready,source,consistent,retirement_ready}`、
  `owner_local_{to_global,global}_bridge_*`、`*_mismatch_record_total`、
  `owner_local_record_index_ready`、`owner_local_canonical_record_ready`。
- **嵌套重复副本**：`status_record` 与 `execution_shard` 两个子 mapping（其数字已在
  shard 映射上平铺）。

保留：记录/引用/路径索引计数、对象目录及其条目、四个索引一致性标志（由结构实算）、
`owner_local_store_{ready,complete}`（由 shard 状态实算）、迁移记录与 `migration_count`。

### 4.2 owner 状态面（部分完成）

`vm_owner_runtime_status()` 及其合同 mapping 不再输出：

- **生产门禁清单**（32 键）：`production_gate_*`。它是验收清单，属于
  `docs/multicore-production-gate.md`，不是运行时状态。
- **mudlib 审计证据字符串**（5 键）：`mudlib_*`。把某次历史审计的结论写成常量塞进状态。
- **代码自述**（12 键）：各类 `*_file`、`owner_cc_runtime_role`。文件名会改，状态键不会跟着改。
- **重构计划声明**：`implementation_state`、`next_refactor_target`、`executor_migration_state`、
  9 个 `*_dependency`。

保留：计数、容量、队列深度、延迟采样、owner/epoch、状态枚举、任务与未来记录、
readiness gate 表（它们描述的是真实准入条件，且有执行路径对照）。

### 4.3 其他文件

`src/packages/core/vm_owner.cc` 不在本契约范围内：它的 mapping 是公开 LPC efun 的
返回形状（`owner_safe` payload、失败 schema），不是状态观测面。

gateway 状态面（`gateway_status_internal()`，单函数 303 键）尚未收口：那里几乎全是
真实计数器与延迟采样，需要逐组判定，见 [roadmap](roadmap.md) R-14。

## 5 已知盲点

以下站点用变量作键，静态检查无法验证，属人工审查范围（检查器只扫 `owner.cc`、
`object_store.cc`、`vm_owner.cc` 三个文件）；

- `src/packages/gateway/gateway.cc` 端口统计（`add_mapping_pair(m, buf, …)`）
- `src/vm/internal/owner.cc` compute result 字段（`field.key.c_str()`）
- `src/vm/internal/base/mapping.cc` 通用 helper（`key.c_str()`）
- flag 名表（`levels[i].name`）

## 6 下游基线

用 `ownercheck full` 记录删键前后的通过/失败集合，写入本节：

- 删键前（baseline）：待记录（U5d）
- 删键后：待记录（U5d）
