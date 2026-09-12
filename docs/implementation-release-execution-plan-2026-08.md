# FluffOS_XK 实施与发布执行方案（最终审计版）

> **状态：本执行批次已收口；结论为 `blocked`，不是 `release-ready`。**
>
> 本文件已经从执行清单收束为事实账本。当前没有可直接执行的本地待办；尚未完成的架构、供应链和生产事项只作为明确的 `deferred` / `external-required` 阻塞记录保留。

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

审计前状态为 `main == origin/main`、无冲突、无未跟踪任务文件。最终文档提交只改变 Markdown，不改变运行时源码。

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

## 4. 明确封存的阻塞事实

下列项目没有在本批次完成，且不能被定向测试或小 benchmark 改写为“已完成”：

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

## 5. 最终判定

当前不能写 `release-ready`，原因是 Promise/stack-lvalue/external-handle 仍 deferred，运行时风险尚无逐项 current gate，且 branch protection、签名/provenance、registry 和生产容量仍 external-required。

本批次可以确认的结论只有：

1. layout ABI/recompile transaction 的当前回归已修复并复验；
2. Windows simul_efun 索引测试的非确定性假设已修复；
3. `f051d230` 的跨平台 CI、Docker、CodeQL 和 capacity artifact 已成功；
4. 工作区改动已按原子提交处理，没有临时诊断、构建产物、密钥或未知并行改动进入交付。

## 6. 收口声明

- 本文件不保留可直接执行的 TODO、恢复清单或“下一步”条目。
- 未完成内容均已转换为 `deferred`、`unverified` 或 `external-required` 的事实记录。
- 方案状态保持 `blocked`；不得因本地测试通过而提升为 `release-ready`。
- 相关历史取证仍见 [`docs/upstream-sync-evidence-2026-08.md`](upstream-sync-evidence-2026-08.md)、[`docs/upstream-absorption-plan-d07e7641-735bd31f.md`](upstream-absorption-plan-d07e7641-735bd31f.md) 和 [`docs/evidence/manifest.schema.json`](evidence/manifest.schema.json)。
