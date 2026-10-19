# FluffOS_XK 多核运行验收合同（自用）

> 本文描述运行时安全合同和本地验收口径，不是 GitHub Release、registry 或官方发布门禁。
> `production_gate_*` 等历史字段名保留用于运行时兼容；它们不表示公开发布状态。

## 证据新鲜度边界（2026-08-09 审计）

历史材料仍保留在仓库，但不再作为当前验收依据。2026-10-19 的原始 partial evidence 位于 `audit-evidence/xiakexing-main-dd99-current-driver-fbc08/`，记录的 FluffOS source commit 为 `fbc08a1674f52a19007ad5327b2e4b5066cf5575`，driver artifact 的 code commit 为 `b6068b8f6aff67a63ea30965017c8cb532571763`，Xiakexing commit 为 `dd99a9bb`。当前目录缺少 `final-audit/final-audit-envelope.json`、`final-audit/final-audit-record.json` 和 `final-audit/check-evidence.log`；因此不能把这些 partial evidence 绑定为当前生产验收，也不能声称 `tools/docs/check-evidence.py` 已通过。

## 当前结论

当前 driver 侧多核化执行面保留本 fork 的 runtime contract：owner-local lifecycle、OwnerExecutor callback task boundary、heartbeat、callout、async/file/db、DNS、socket read/write/close callback、gateway command execute 和 `socket_release` owner-safe release/acquire handshake。正常运行路径要求 `normal_path_main_fallback_count=0`；`off`、owner executor 不可用和 rollback/failure 场景只能作为显式兼容 fallback，不计入正常业务热路径。由于当前缺少完整 envelope，`production_gate_ready` 必须保持 `0`，状态为 `unverified`；公开 Release 状态也不因此改变。

截至 2026-10-19，现有原始材料记录了 core/owner sandbox、1/3 用户 smoke 的 `off`/`audit`/`enforced` 模式、10 用户约 30 分钟 `audit` 压测、static gate 和 `socket_release` 合同测试的通过结果。压力报告实际记录 5839/5839 条命令成功、0 failure、0 timeout、0 disconnect，static gate 记录 318 modules、2148 tests、0 reds，`socket-release.log` 记录 195 个测试全部通过。这些是 source/artifact 为旧 checkout 的 partial evidence，不是当前 main 的完整 production envelope；该结论不等同于公开 Release 的 `release-ready`。

> **证据边界**：当前目录中的 JSON 和日志只证明各自列出的检查。缺少绑定当前二进制、构建身份、测试 SHA、源 SHA、清理状态和原始日志摘要的完整 envelope 前，不得把这些 partial evidence 写入 current evidence 字段，也不得将 `production_gate_ready` 置为 `1`。

## 机器可读状态

`vm_owner_runtime_status()` 与 `vm_owner_thread_status()` 内的 `gateway_owner_task_contract` / `owner_executor_boundary_contract` 必须保持以下状态：

| 字段 | 当前值 | 含义 |
| --- | --- | --- |
| `mudlib_audit_required` | `1` | 真实下游 mudlib audit 是自用验收的硬前置 |
| `mudlib_cross_owner_hotspots_ready` | `0` | 当前没有绑定有效 production envelope |
| `mudlib_cross_owner_hotspots_blocker` | `current_evidence_not_bound_to_this_binary` | 当前 partial evidence 不能绑定当前二进制 |
| `mudlib_cross_owner_hotspots_evidence` | `` | 当前证据标识为空；partial evidence 见下文 |
| `mudlib_cross_owner_hotspots_historical_evidence` | `xkx_5513c8a12_multicore_mudlib_audit_2026_06_25_zero_delayed_object_payloads` | 可辨认但不能置 current-ready 的历史证据 |
| `production_gate_ready` | `0` | 缺少当前有效 envelope 时强制 fail-closed |
| `production_gate_status` | `unverified` | 当前 production gate 尚未由有效 envelope 证明 |
| `production_gate_blocker` | `current_evidence_not_bound_to_this_binary` | partial evidence 不能绑定当前二进制 |
| `production_gate_evidence_kind` | `historical` | 记录来自旧 source/artifact checkout |
| `production_gate_current_evidence_bound` | `0` | 没有证据绑定当前 main 和二进制 |
| `production_gate_required_users` | `1,3,10` | 恢复验收时必须覆盖的并发用户档位 |
| `production_gate_required_durations` | `smoke,30m` | 恢复验收时必须覆盖的时长档位 |
| `production_gate_pressure_evidence_ready` | `0` | 压力报告存在，但尚未通过当前 envelope 绑定 |
| `production_gate_pressure_evidence` | `` | 当前压力证据标识为空；partial report 见下文 |
| `production_gate_pressure_historical_evidence` | `xkx_audit_10_users_30m_2026_06_25_zero_timeouts_zero_gateway_errors` | 可辨认但不能置 current-ready 的历史证据 |
| `production_gate_required_modes` | `off,audit,enforced` | 必须覆盖的 driver 多核模式 |
| `production_gate_required_scenarios` | `login,create,move,chat,inventory,shop,quest,combat,skills,mail,reconnect,gateway_callback,socket_callback,heartbeat,callout` | 必须覆盖的真实 mudlib 场景 |
| `production_gate_evidence_schema` | `multicore_production_gate_evidence_v1` | 运行时验收证据 schema（历史字段名） |
| `production_gate_short_smoke_sufficient` | `0` | 单用户短 smoke 永远不能单独置 ready |
| `production_gate_minimum_ready_evidence` | `accepted_30m_pressure_scope_final_audit_and_socket_release_handshake` | 最小 ready 证据模型 |
| `production_gate_unclassified_hotspots_required_zero` | `1` | 未分类 cross-owner hotspot 必须为 0 |
| `production_gate_direct_cross_owner_writes_required_zero` | `1` | 直接 cross-owner 可变写必须为 0 |
| `production_gate_context_leaks_required_zero` | `1` | VMContext/eval/context leak 必须为 0 |
| `production_gate_future_backlog_required_zero` | `1` | future backlog 不允许持续增长 |
| `production_gate_same_owner_claim_conflict_required_zero` | `1` | same-owner claim conflict 必须为 0 |
| `production_gate_gateway_error_delta_required_zero` | `1` | gateway rejected/dropped/queue-full/write-error 增量必须为 0 |
| `production_gate_socket_release_policy` | `owner_safe_synchronous_release_acquire_handshake` | `socket_release` 保持同步返回语义，但 release/acquire 受 owner epoch guard 保护 |
| `production_gate_socket_release_handshake_ready` | `0` | 测试日志存在，但尚未绑定当前二进制 |
| `production_gate_socket_release_handshake_evidence` | `` | 当前 handshake 证据标识为空 |
| `production_gate_socket_release_handshake_historical_evidence` | `socket_release_owner_epoch_handshake_contract_v1` | 可辨认但不能置 current-ready 的历史证据 |
| `production_gate_report_schema` | `xkx_gateway_loadtest_report_v1` | loadtest JSON 报告 schema |
| `production_gate_report_required_fields` | `schema,run_id,mode,users_requested,duration_seconds,scenario,commands_ok,timeouts,gateway_metrics_delta,production_gate_observations` | 每个归档 JSON 必须包含的字段 |
| `registered_owner_task_domains_ready` | `1` | driver 已注册受控 owner domain task allowlist |
| `registered_owner_task_domain_count` | `18` | 当前受控 domain task 数量 |
| `domain_task_registry_mudlib_aligned` | `1` | driver domain task allowlist 与真实 mudlib 注册域一致 |
| `target_owner_message_executor_ready` | `1` | target-handle owner message 正常路径走 target owner mailbox/executor |
| `target_owner_message_main_fallback` | `0` | target-handle owner message 不走正常路径 main fallback |
| `normal_path_main_fallback_count` | `0` | 生产正常路径不允许隐式 main fallback |
| `normal_path_main_fallback_ready` | `1` | 正常路径 fallback 计数仍为 0 |
| `explicit_fallback_count` | runtime counter | 仅统计显式 owner main queue fallback 或兼容适配 |
| `service_shard_executor_ready` | `1` | service shard/domain task 可走 owner executor |
| `keyed_service_shard_ready` | `1` | service shard/domain task 由 owner/service key 分片 |
| `hot_path_service_owner_single_point` | `0` | 玩家热路径不存在 service-owner 单点瓶颈 |
| `production_perfect_contract_ready` | `1` | domain registry、target message、service shard 和 fallback 字段全部满足运行时合同 |
| `facade_only_runtime_claims` | `0` | 不允许仅声明 facade-ready 却没有 executor 路径 |

这些字段表达运行时验收状态和正常路径边界：业务命令、heartbeat、callout、async/DNS/socket callback 和 target-handle owner message 必须走 owner executor；main 线程只保留网络 IO、reply/prompt/telnet 适配、LPC 引用 cleanup、显式 fallback 和 documented main-required compatibility surface。

## 当前 smoke 与压力证据

当前证据根目录：`audit-evidence/xiakexing-main-dd99-current-driver-fbc08/`。

| 运行 | 结果 | 原始报告 |
| --- | --- | --- |
| core sandbox | PASS | `core-v7/evidence/sandbox.json` |
| owner sandbox | PASS | `owner-v7/evidence/sandbox.json` |
| 1 用户 smoke | `off`/`audit`/`enforced` 均 PASS | `production-matrix-v3-mode-{off,audit,enforced}/smoke-1.json` |
| 3 用户 smoke | `off`/`audit`/`enforced` 均 PASS | `production-matrix-v3-mode-{off,audit,enforced}/smoke-3.json` |
| 10 用户压力 | 请求 1800 秒，实际 1862.1 秒；5839/5839 命令成功，0 failure、0 timeout、0 disconnect，gateway error delta 全 0 | `production-matrix-v3-pressure/pressure-10u-30m.json` |
| static gate | PASS；318 modules，2148 tests，0 reds | `final-audit/static-gate.log`、`final-audit/static-gate.result` |
| socket release | PASS；195 个选中的 socket/gateway/release/transport 合同测试全部通过 | `final-audit/socket-release.log` |

`final-audit` 当前缺少 `final-audit-record.json`、`final-audit-envelope.json` 和 `check-evidence.log`。因此上述日志和 JSON 只能作为 partial evidence，不能证明当前 production gate ready。
## Loadtest 入口

仓库内自包含入口为 `tools/loadtest/xkx_gateway_loadtest.py`。最小 smoke 运行形式：

```bash
python3 tools/loadtest/xkx_gateway_loadtest.py --host <gateway-host> --port <gateway-port> --path <ws-path> --mode audit --users 1 --scenario smoke --fail-on-error
```

复核压力证据时显式设置 `--mode`、`--users`、`--duration`、`--ramp-up`、`--scenario`、`--metrics-url` 和 `--report-json`。`--mode` 只记录本次报告对应的 driver 模式，不负责启动或切换 driver；调用方必须先用匹配的 `off`、`audit` 或 `enforced` 配置启动真实链路。当前 loadtest 默认 `--command-timeout` 已提高到 5 秒，30 分钟复核仍建议显式传参记录本次预算。

该脚本输出的 JSON 顶层 schema 为 `xkx_gateway_loadtest_report_v1`，并包含兼容字段 `production_gate_observations.schema=multicore_production_gate_evidence_v1`。没有 `--metrics-url` 或指标缺失时，`production_gate_observations.metrics_available` 与 `gateway_error_delta_zero` 都必须为 `false`，该 run 只能作为功能 smoke，不能单独作为完整运行时验收证据。报告中的 `production_gate_observations.production_matrix_complete` 在单次入口内固定为 `false`；最终 ready 只能由当前接受的 30 分钟压力证据、final audit 零 blocker 和 `socket_release` owner-safe handshake 共同决定。

当前入口已通过 1 用户 `smoke` 场景，覆盖登录/建角和 `look`、`i`、`skills`、`score`、`map` 命令闭环；该结果只证明入口可用和最小路径未立即回归，不替代下游所需的实际负载复核。

## 当前阻塞项

真实 XiaKeXing mudlib final audit 的报告 `docs/reports/multicore-mudlib-audit-2026-06-25.md` 在当前 checkout 不存在，属历史证据；`external-required` 重跑前不得作为当前 ready 依据。当前 evidence 目录虽有 core/owner、smoke、压力、static gate 和 `socket_release` 原始材料，但缺少 `final-audit-record.json`、`final-audit-envelope.json` 和 checker 输出，不能完成当前身份、SHA、清理状态和原始摘要绑定。

当前仍有明确的验收 blocker：`current_evidence_not_bound_to_this_binary`。`socket_release` 仍保持 efun 同步返回语义；release 时捕获目标 owner epoch，acquire 时校验同一 owner epoch，成功、拒绝、stale 和 owner mismatch 都记录 `socket_release` task trace。现有合同测试日志不能替代当前 envelope。

## 验收矩阵

当前默认验收口径是受影响测试、smoke 和真实下游 mudlib 复核；如部署规模需要，再由下游单独安排压力测试。final audit 是技术审查，不是公开发布门禁；`socket_release` handshake 合同已补齐。

| 项目 | 用户数 | 时长 | 必须覆盖 |
| --- | --- | --- | --- |
| smoke 入口 | 1、3 | smoke | 登录、建角、基础命令、gateway callback 基线 |
| accepted pressure evidence | 10 | 30m | 需在当前二进制上重跑 audit 模式、owner executor、gateway metrics、timeout/fatal/panic 为 0 |
| final audit | 不适用 | 非压测 | 需绑定当前 checkout 的 cross-owner hotspot 分类、mutable write 清零、snapshot/message/future 迁移证明 |
| socket release handshake | 不适用 | 非压测 | 需绑定当前二进制的 release/acquire owner-safe handshake 证据 |

通过标准：

- 命令成功率满足下游 SLA，且 timeout、断线、panic、fatal error 为 0。
- owner executor trace 中同 owner 串行、不同 owner 可并行，没有 same-owner claim conflict。
- `thread_eval_stack_leak_detected`、VMContext leak、object store sync rejection 都为 0。
- future pending 不持续增长，stale/destructed/owner epoch mismatch 都明确 drop 或 fail。
- `normal_path_main_fallback_count=0`，target-handle owner message 通过 owner mailbox/executor 完成，不再依赖 owner main queue bridge。
- 受控 owner domain task allowlist 与真实 mudlib domain 注册保持一致，`domain_task_registry_mudlib_aligned=1`。
- service shard/domain task 按 owner/service key 分片，`hot_path_service_owner_single_point=0`；普通 legacy LPC 仍 default-closed。
- gateway metrics 没有 rejected、dropped、queue full 或 write error 增量。
- audit trace 中没有未分类 cross-owner write；enforced 模式没有静默同步 cross-owner fallback。

## 失败处理

- 压力证据、mudlib audit 证据或 socket release handshake evidence 失效后，必须将兼容字段 `production_gate_ready` 回退并重新运行相关 smoke、压力复核或 handshake 合同测试。
- 若 final audit 发现 mudlib 同步 cross-owner 调用，先迁 mudlib/API 模型，再重跑定向 smoke 或 audit 复核。
- 若失败来自 gateway 协议解析或大 payload，先修协议/客户端解析，再重跑相关 smoke。
- 若失败来自 owner executor context cleanup、future backlog 或 stale drop，先补 driver 合同测试，再重跑 C++、LPC 和真实 mudlib 定向验收。

## 下一步

当前 runtime contract 代码路径已有上述 partial evidence，但 production gate 仍为 `unverified`。后续必须在当前二进制上重新生成完整 record/envelope，并用 checker 校验身份、SHA、清理状态和原始日志摘要；在此之前不得把 partial evidence 写入 current evidence 字段或将 `production_gate_ready` 置为 `1`。公开 Release 仍不自动标记 `release-ready`；若后续要申请公开发布，必须另外满足项目级 fuzz、安全、性能、供应链、签名、provenance 和外部环境门禁。新增 executor callback 或受控 owner domain 类型时，必须先补 allowlist、payload policy、stale/drop cleanup、normal-path fallback 计数和合同测试。
