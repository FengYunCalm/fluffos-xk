# FluffOS_XK 实施与发布执行方案（最终审计版）

> **状态：历史发布审计仍为 `blocked`；当前架构批次已完成 A-D，不能写 `release-ready`。**
>
> 本文件同时记录历史发布审计的证据和后续架构批次的当前状态。未满足的运行时、供应链和生产门禁仍是明确的 `deferred` / `unverified` / `external-required` 事项，不得用局部测试替代。

## 1. 审计范围与基线

本次审计覆盖：

- 当前 checkout、分支、提交和工作区状态；
- recompile/layout 回归修复及其直接测试；
- Debug、ASan、UBSan 定向验证；
- GitHub Actions 矩阵、Evidence Gate、Docker、CodeQL 和 capacity artifact；
- 方案中的历史失败、未完成架构和外部发布门禁。

运行时源码验证锚点为 `f051d230a10be9b5d6e9f0f40cc69b6801f4d4c3`。该提交之前的本地原子提交为：

| 提交 | 已交付内容 |
|---|---|
| `e7044bf3` | 修复 `recompile_layout` 与 `program_t` 在 `DEBUGMALLOC_EXTENSIONS` 下的跨翻译单元布局 ABI 误读。 |
| `f1020aed` | 修复 gametick 延迟计算溢出，并收口 recompile transaction 的测试/迁移前置条件路径。 |
| `bbee8c84` | 增大 Windows Release CPU timing probe 的 LPC workload，避免粗粒度计时量化为零。 |
| `f051d230` | 修复 simul_efun rollback 测试错误固定使用 `snap.names[0]` 的索引假设，改用实际 `survivor_slot`。 |

历史审计基线为 `main == origin/main`、无冲突、无未跟踪任务文件；该基线已由后续 A-C 架构提交推进。历史审计的远端证据仍绑定 `f051d230`，当前架构状态见第 4.1 节。

## 2. 已完成的本地验证

- `build-dev-debug` 的 `lpc_tests` 定向重建成功。
- Debug simul_efun 回归集：`3/3` 通过，耗时 `0.11s`。
- ASan simul_efun 回归集：`3/3` 通过，耗时 `0.28s`；构建目标为 `lpc_tests`，使用 `ASAN_OPTIONS=detect_leaks=0` 仅用于该定向根因验证。
- UBSan simul_efun 回归集：`3/3` 通过，耗时 `0.14s`。
- 先前 `build-dev-debug` 全量 CTest 在 `bbee8c84` 为 `450/450` 通过；`f051d230` 只改变上述测试断言索引，当前提交的全量跨平台结果由 CI 确认。

失败根因已经闭合：Windows RelWithDebInfo 失败值为 `ihe->dn.simul_num == 11`、`snap.names[0].index == 33`；测试选择的有效 survivor 并不保证位于 snapshot 槽位 0。生产 recompile/layout 路径没有因此新增故障。

## 3. 当前远端证据

所有下列证据均绑定运行时源码提交 `f051d230a10be9b5d6e9f0f40cc69b6801f4d4c3`：

- GitHub CI run `34702766236`：15 个构建/测试矩阵 job 与 Evidence Gate 全部成功；包含 Windows RelWithDebInfo、完整 isolated LPC testsuite、ASan/UBSan/TSan 定向门禁和真实 Clang libFuzzer bounded run。
- Docker run `34702766223`：成功。
- CodeQL run `34702766232`：成功。
- Ubuntu GCC Debug 生成的 3 份 capacity envelope 已绑定该 SHA；下载对应 raw report 后，`check-evidence.py --mode gate` 返回 `OK (3 report(s))`。
- workflow/action-pin、文档链接、SBOM/schema、fault-injection 和 Evidence Gate 负例检查在上述 CI 中成功。

capacity artifact 的运行时构建配置为 GCC 13.3、Linux x86_64、4 核；报告 cleanup 状态为 clean。报告保留在 CI artifact，不复制进源码树，不把历史 `docs/evidence` 文本冒充 current envelope。

## 4. 历史审计封存的阻塞事实

下列记录描述历史审计截止点的状态，不能被定向测试或小 benchmark 改写为发布证据；后续本地架构提交的状态更新见第 4.1 节：

| 项目 | 状态 | 封存事实 |
|---|---|---|
| Promise / `T_PROMISE` / parked async frame | `deferred` | 本地没有完整 Promise value、EGC、reaction/microtask、owner affinity、park/resume、cancel、shutdown、recompile 和错误传播合同。 |
| stack-resident lvalue ABI | `deferred` | 栈槽、nested reference、error unwind、owner 隔离、`svalue_t` ABI、opcode 和 legacy bytecode 合同尚未形成可验证基座。 |
| external handle 与 canonical object-store migration | `deferred` | 现有 ObjectHandle、pointer/record bridge 和 owner-local fast path 不能证明单写 canonical store、stale 拒绝、迁移回滚和完整清理已经完成。 |
| Libevent、`add_vmessage`、gateway scan、future retention 等运行时风险 | `unverified` | 当前源码风险已有记录，但本批次没有为每项建立新的 fail-first、故障注入和匹配 sanitizer/压力证据。 |
| upstream partial-clone 对象 | `external-required` | 缺失对象不能由 commit message、patch 摘要或 ancestry 推断；未 hydrate 的 upstream 内容不作为本地实现证据。 |
| GitHub 分支保护和 required checks 合同 | `external-required` | live 查询确认 `main` 未启用 branch protection；静态 workflow 检查不能代替受保护环境和 required check-run 验证。 |
| artifact 签名、provenance、attestation 和 registry promotion | `external-required` | checksum、SBOM、manifest 和 OCI digest 不等于签名或来源真实性；本批次没有真实签名服务和受保护 registry mutation。 |
| 生产规模容量与长期运行 | `external-required` | 真实 300-player、长时 owner/gateway/future/object-store、restart/shutdown 和生产环境证据不存在。 |

这些记录是当前判定依据，不是本执行批次的开放清单。新的架构或外部门禁实施属于独立范围，并以独立合同、授权和证据边界为准。

## 4.1 后续独立架构实施记录（2026-09）

本节是对上方历史审计截止点的当前工作区更新。上方第 4 节中
Promise、stack-lvalue 和 external-handle 的 `deferred` 记录描述的是当时
尚未实施的状态；它们不再代表下列已经落地的本地代码，但不改变发布判定。

- A：`8fa24188` 完成 stack-resident indexed lvalue 的栈所有权、引用转移和
  异常清理。
- B1：`84c338f9` 完成 `T_PROMISE` substrate、引用/反应队列和微任务生命周期。
- B2：`46568b7c` 完成 async/await 的编译器、park/resume 和错误传播。
- B3：`6f45e262` 完成 callback async efun 的 Promise 形式。
- C：`722d8812` 完成 external handle：`external_create` / `external_run`、
  stdin/stdout/stderr、exit code、kill/close、generation/owner/epoch 绑定、
  POSIX `posix_spawn` 与 Win32 `CreateProcess` 路径，以及 Promise 取消、
  owner destruct 和 driver shutdown 清理；经典 `external_start` 的 fd/callback
  ABI 保留。Promise substrate 增加了一次性取消回调，用于把拥有外部工作的
  Promise 拒绝路由到子进程终止，而不让 worker 直接修改 LPC 引用。

C 单元的当前定向证据：Debug/ASan/UBSan/TSan `lpc_tests` 外部 Promise
回归集均为 `2/2`；testsuite 从 `testsuite/` 运行
`external_promise` 与 `external_start` 均 `Checks succeeded.`；构建目标为
`driver lpcc lpc_tests`，生成 efun 源已重建，`check-docs.py` 与
`git diff --check` 通过。TSan 在本机必须使用既有 WSL2 workaround：
`setarch x86_64 -R`。未在本机执行 Windows/macOS runtime，不能把本地
Linux 结果写成跨平台证据。

D：提交 `012ae728` 完成 object-store 收口。driver 的规范化 live-name
lookup 先路由到 owner shard 的 `local_records`、`local_objects` 和
`object_path_index`；本地 store 未启用或缺少记录时才回退到 `ObjectTable`
兼容索引。`ObjectTable` 的 children、insert/remove 和 legacy fallback 用途
保留，但不再作为 owner-local canonical source。新增 helper 不读取 worker
上的可变 `object_t` 生命周期字段，跨 owner handle 的拒绝合同不变；
`find_object`、`find_object2`、load/virtual collision 和 gateway name
validation 均已接入该路径。

D 的定向证据：Debug object-store `5/5`、name-lookup `1/1`、gateway `2/2`；
ASan object-store/name-lookup `6/6`、gateway `3/3`；UBSan 同为 `6/6` 和
`3/3`；TSan 同为 `6/6` 和 `3/3`，均通过。testsuite 从 `testsuite/` 运行
`owner_executor_contract` 返回 `Checks succeeded.`；`lpc_tests` 和
`driver` 定向构建成功，`check-docs.py` 报告 `1137 markdown files`，
`git diff --check` 通过。TSan 在本机必须使用既有 WSL2 workaround：
`setarch x86_64 -R`；未使用 workaround 的 CMake test discovery 仅失败于
已知 `unexpected memory mapping` 环境错误，随后使用 workaround 的构建和
运行均通过。

## 5. 最终判定

当前不能写 `release-ready`：运行时风险尚无逐项 current gate，且 branch protection、签名/provenance、registry 和生产容量仍为 `external-required`。A-D 的本地实现和定向测试不等于跨平台、生产或发布证据。

当前可以确认的结论是：

1. 历史审计中的 layout ABI/recompile transaction 和 Windows simul_efun 索引回归已修复并有对应证据；
2. A-D 已形成独立本地提交，A-C 与 D 的定向 Debug/ASan/UBSan/TSan、testsuite、生成链和文档检查证据见第 4.1 节；
3. D 的 owner-local canonical lookup、生命周期清理、失败路径和兼容 fallback 已通过本节列出的定向验证；
4. 未满足的跨平台、供应链、签名、registry 和生产容量条件不能由上述本地结果代替。

## 6. 当前执行边界

- A-D 已提交；当前没有开放的本地架构实现单元。
- 未完成内容继续标记为 `deferred`、`unverified` 或 `external-required`，不把计划文字当作完成证据。
- 方案状态保持 `blocked`；不得因本地测试通过而提升为 `release-ready`。
- 相关历史取证仍见 [`docs/upstream-sync-evidence-2026-08.md`](upstream-sync-evidence-2026-08.md)、[`docs/upstream-absorption-plan-d07e7641-735bd31f.md`](upstream-absorption-plan-d07e7641-735bd31f.md) 和 [`docs/evidence/manifest.schema.json`](evidence/manifest.schema.json)。
