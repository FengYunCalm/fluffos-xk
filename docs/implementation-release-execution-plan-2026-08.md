# FluffOS_XK 深度审计、上游吸收、实施与发布执行总方案

> **文档用途**：这是基于当前 checkout、历史审计上下文和 `docs/upstream-absorption-plan-d07e7641-735bd31f.md` 汇总后的统一执行方案，目标读者可以是能力较弱但能够执行命令、阅读源码和记录结果的模型。
>
> **当前状态**：本地实现、定向/全量验证和文档更新已完成；本地原子提交由本次任务收尾，不执行推送、发布或外部系统变更。Promise/`T_PROMISE`、stack-lvalue ABI 和 external-handle 仍按独立设计延期。
>
> **事实优先级**：当前工作区和可重跑验证结果 > 本文档中的历史证据记录 > 旧审计报告和上游提交说明。没有当前命令输出就不能写成“已通过”；没有读取 upstream patch 就不能写成“已兼容”。本轮逐 hunk 取证范围采用 `6cf257ce..735bd31f`（左边界不含 `6cf257ce`，共 37 个候选提交）；`6cf257ce` 本身只作为依赖升级基线另行记录，不能漏算也不能把左边界误当成已吸收。

---

## 0. 本次上下文复核结论

### 0.1 审计范围和方法

本轮只读审计覆盖四份历史方案/证据文档、当前 `src/` VM/LPC/compiler/package/net/base、`testsuite/`、CMake preset/target、生成规则、CI/release workflow、供应链脚本和 `docs/evidence/manifest.schema.json`。对 `6cf257ce..735bd31f` 的 37 个候选提交逐文件、逐 hunk 读取了本地 partial-clone 已取得的 patch；没有把 commit message、旧文档或 upstream 自报的测试数字当作当前通过证据。

审计结论分为：`confirmed`（当前源码/命令直接证实）、`failed`（可重现阻断）、`high-risk`（源码证据充分但需故障注入/压力/边界验证）、`external-required`（需 GitHub、registry、签名服务或生产环境）。所有 upstream 决策都绑定到本地调用者、生成链、fail-first 测试和可回滚提交，不允许批量 cherry-pick。

已只读复核指定的历史 pi 会话及其多次 compaction 摘要，并与当前工作区重新对照。结论如下：

1. **没有丢失核心任务目标或关键决策。** 原任务一直是：深度、多维度审计当前已落地代码，研究可吸收的 upstream 内容，并形成可直接执行的统一实施/发布方案。
2. **原先较短的方案遗漏了若干证据细节，已在本文恢复**，包括：
   - `build` 和 `build-portable-release` 的 `444/444` 结果属于 include 修复前的历史证据，不能替代修复后的当前结果；
   - `build`/portable 的布局与迁移定向测试曾通过，但 Debug/ASan 的布局测试曾失败；
   - `ObjectVariableBlockLifecycle`、`ClassifyLayoutRules` 在 ASan/Debug 中曾通过，而布局描述和 recompile transaction 路径仍失败；
   - `addr2line` 定位到 `walk_layout()`、`describe_recompile_layout()` 和对应测试行；backward-cpp 信号处理器会遮蔽部分原始 ASan 输出；
   - isolated testsuite 曾以四个不同 loopback 端口通过，`driver exit=0`、`lpc_assertions_ok=yes`、`bind_error=no`；该结果仍需在修复后重新取证；
   - 静态 release、action pin、fault injection、SBOM、manifest/schema 检查曾通过，但 GitHub live API 返回 `403`，SSH 远端查询也失败；
   - `third_party/cyclonedx.schema.json`、测试证书/私钥、release/rollback runbook、`Analyze (cpp)` 检查名不一致等供应链和发布细节；
   - upstream 37 个候选提交的逐提交分类、依赖关系和 Promise 延后原则。
3. **历史会话不能覆盖当前工作区事实。** 当前工作区只以本文件写入后的实际 `git status`、源码 diff 和新测试结果为准。
4. **本地 transaction/layout 阻断已完成修复并复验**：跨翻译单元 ABI 根因以最小 `base/std.h` include 修复，migration 前置合同、回滚、生成链和回归门禁均已纳入当前验证；剩余事项是外部发布门禁及明确延期的异步架构。

---

## 1. 固定基线与当前工作区

### 1.1 Git 基线

- 当前分支契约：`main...origin/main`。
- 当前 `HEAD`：`d07e7641fb8895a29e68de9af1a08677b75dd77b`。
- upstream 目标：`735bd31fcd07aa3ec55cd6b0dca2f664544682eb`。
- merge base：`277d0b1cbc4350844d9aff07a604dfa519650d9e`。
- `735bd31f` 的父提交：`7c808c8b`。
- 不得把 `master` 假设当作已存在的本地事实；当前本地可见主线是 `main`。

### 1.2 当前工作区改动

已执行并保存结果：

```text
git status --short --branch -> main...origin/main
git diff --check            -> 无输出
```

`cmake --preset dev-debug`、Debug 下 `driver/lpcc/lpc_tests` 构建和 ASan 下对应构建均通过；Debug 重点 GTest 单测 10 项中 9 项通过，`DriverTest.TestSimulEfunReloadCreateFailureRollback` 在 `recompile.cc:242` assertion 中止，ASan 复现同一 assertion。ASan `recompile_special_targets` 打印 `8 special reload checks ok` 后仍 SIGSEGV；具体发生阶段和原始栈尚未定位。完整 CTest、portable Release 全量、UBSan、TSan、完整 LPC suite、真实 libFuzzer、长期/容量、live GitHub/registry、签名和 provenance 尚未取得当前证据。

已知改动：

1. `src/vm/internal/recompile_layout.cc`
   - 在 `vm/internal/recompile_layout.h` 前增加 `#include "base/std.h"`。
   - 原因：`base/std.h` 提供构建配置宏 `DEBUGMALLOC_EXTENSIONS`；`program.h` 在该宏下向 `program_t` 增加 `extra_ref` 和 `extra_func_ref`。如果编译器翻译单元看到该宏而布局模块没有看到，两个翻译单元会使用不同的结构偏移，合法的 `program_t` 会被错误解释。
   - 该修复没有改变 layout digest 算法、inheritance 语义或 migration 语义。
2. `docs/upstream-absorption-plan-d07e7641-735bd31f.md`
   - 未跟踪的用户输入，必须保留，不覆盖、不删除。
3. 临时 layout 诊断文件
   - 已确认不具备稳定的正式测试契约并删除；不得未经判断提交临时文件。
4. `docs/implementation-release-execution-plan-2026-08.md`
   - 本统一方案文档。

临时 `fprintf()` 诊断已经移除。当前没有提交或推送，不得执行 `reset --hard`、批量清理或覆盖未知来源的未跟踪文件。

### 1.3 不可继承的历史假设

以下内容只能作为历史线索，必须重新验证：

- 旧 `build`/portable 的 `444/444`；
- 旧 ASan/TSan、capacity、mudlib、10-user/30-minute 和 `production_gate_ready=1` 结果；
- 旧分支为 `master` 的 workflow 结论；
- 旧 upstream diff 或未成功 hydrate 的 patch；
- 旧会话中任何没有落盘到当前工作区的“已修复”声明。

---

## 2. 现有审计结论与证据账本

每条结论都要标记为以下四类之一：

- **已确认**：当前源码或当前命令结果直接证明。
- **当前失败**：当前可重现的失败，必须先修复或明确豁免理由。
- **高可信风险**：源码显示风险，但还需故障注入、压力或边界测试证明。
- **外部/未验证**：需要 live GitHub、registry、受保护环境、生产规模或缺失的 upstream 对象。

### 2.1 已确认的本地能力

- 运行时已有 `VMContext`、owner metadata/mailbox、`OwnerExecutor`、`ObjectHandle`、冻结 payload、future store、snapshot、recompile transaction 和 compile arena。
- `OwnerExecutor::run()` 已有 `OwnerReleaseGuard`，并捕获 `std::bad_alloc`、`std::exception` 和未知异常；owner claim 清理和 task terminalization 已存在。
- `OwnerFutureStore` 已有终态数量上限、TTL、payload 字节配额、索引维护及 payload-free failed tombstone 查询能力；payload-bearing terminal record 仍要等 `take()`，这是生命周期合同，不是单纯实现细节。
- `f_sys_reload_tls()` 已有主线程限制、master 授权、索引转换前校验和 `external_port[5]` 数量边界；testsuite 覆盖非 TLS、WebSocket、TLS、越界和负索引路径，并有 WASM 条件保护。
- 已有 owner-runtime layering guard，stores 已从 `owner.cc` 的直接实现中分层。
- Docker 基础镜像、jemalloc 下载、workflow action 和 container 引用已有部分 pin/checksum/digest 约束。
- 本地不存在 native `T_PROMISE`、Promise VM、parked async frame、`acatch` 或 `external_create()`；现有 `src/packages/async/` 和 `external` 文件不能被误报为完整 Promise 实现。

### 2.2 已有但必须刷新证据的结果

- 静态 workflow 检查、action pin self-test、release fault injection、SBOM 生成/验证、manifest/schema 解析曾通过；SBOM 曾报告 30 个组件。
- `build` 和 `build-portable-release` 曾各有 `444/444` CTest 通过，约 13 秒；这些结果发生在当前 include 修复之前，必须重新构建和运行。
- `tools/testsuite/run-isolated.sh --driver build/bin/driver --mode audit --keep` 曾通过，四个端口分别为 `36627 41733 43831 45099`，driver 退出 0，LPC assertion 和 bind error 均为 0；端口和 kept workspace 只作为历史记录，不复制到新证据。
- `testsuite/single/tests/compiler/a2_grammar.lpc` 曾被确认与 `HEAD` 内容一致，先前失败归类为 stale driver/build artifact；当前仍应使用新 driver 重跑。
- `build` 和 portable 的布局 digest/migration 定向测试曾通过；Debug/ASan 的相同测试不能因此被视为通过。

### 2.3 当前失败或已确认阻断

1. **recompile transaction 断言**：

   ```text
   src/vm/internal/recompile.cc:242
   RecompilePrepared::commit_swap()
   Assertion 't.ob->variables.layout_id == program_layout_digest(new_prog)' failed
   ```

   在布局 ABI include 修复后，原来的 `walk_layout()` SIGSEGV 消失，但 `DriverTest.TestSimulEfunReloadCreateFailureRollback` 暴露出上述独立失败。当前源码给出一个可验证的最小根因线索：生产入口 `f_recompile_object()` 在 `commit_swap()` 前调用 `prepare_variable_migrations()`，而失败的 GTest 直接 `start_recompile_transaction()` 后调用 `commit_swap()`，没有满足 `recompile.h` 明确写出的前置条件；该测试生成的 staged source 还包含 `int g = boom();`，会改变变量布局。应先把测试调用顺序和 `migrations.size()==targets.size()` 不变量固定下来，再判断是否存在生产路径的独立故障；不能删除 assertion 或强制覆盖 `layout_id`。

2. **Debug/ASan 布局与 recompile 路径尚未绿**：
   - `TestProgramLayoutDigestMatchesDescriptor` 曾在 Debug/ASan 发生 SIGSEGV；
   - `TestLayoutDigestBijectionOverCorpus` 曾在 Debug 发生 SIGSEGV；
   - `TestRecompileMigrationAddRemovePreserves`、`TestMasterExactReloadMigrateThenInit` 和 simul-efun rollback 不能沿用 Release 结果；
   - `detect_leaks=0` 后仍能复现 assertion/崩溃，说明不是单纯 LeakSanitizer 报告。

3. **ASan 仍有泄漏/非泄漏问题**：完整 CTest 曾先报告约 11 个 simul-efun 泄漏，随后进入 assertion/崩溃；不能通过永久关闭 leak detection 掩盖问题。

4. **UBSan、TSan 和真实 `gateway_fuzz` 证据缺失**：仅构建过 `gateway_fuzz_smoke`，不能替代真实 libFuzzer target。

5. **分支契约不一致**：本地为 `main`，但 `.github/workflows/ci.yml`、Docker、CodeQL、release 和 `SECURITY.md` 仍含 `master` filter/`origin/master` 假设；`tools/release/common.sh` 还要求 `Analyze (cpp)`，该字面名称不在 workflow 源码中，必须用 live check-run 核实。

6. **live GitHub/registry 证据不可用**：历史尝试得到 HTTP 403，SSH 远端查询也失败；不能声称保护分支、required check、审批、artifact 下载或 registry mutation 已验证。

7. **签名与 provenance 缺失**：仓库/workflow 未发现 artifact signing、`cosign`、attestation 或 provenance signing。checksum、SBOM、manifest、OCI digest 只能证明部分完整性，不能证明来源真实性。

8. **upstream partial clone 阻断**：当前仓库仍是 `promisor=true`、`blob:none` 的 partial clone；此前出现 `fatal: could not fetch fbee17747bd5fd0b229e13d4da78bbc7912a0509 from promisor remote`。本轮已读取并导出 37 个候选 patch 到 `/tmp/fluffos-upstream-audit/`，但未成功 hydrate 的对象仍不能根据 ancestry 或文档猜 patch；patch 文件不是源码已吸收的证据。

9. **生产门禁缺失**：真实 300-player、生产规模容量、长期运行和受保护环境仍是 external-required。

### 2.4 高可信运行时风险

- `src/vm/internal/object_store.cc` 仍有 `global_live_object_bridge`、`global_object_records`、pointer bridge、ID scan 和 path scan；canonical owner-local storage 尚未建立，不能把 bridge 当作最终架构。
- `src/packages/gateway/gateway_session.cc` 的 `g_gateway_sessions` 仍可能被 flush 和 aggregate metrics 全量扫描；1 ms continuation、bounded drain 和 coalescing 只能降低单次阻塞，不能消除 O(session count) 成本。
- `src/comm.cc:967` 的 `add_vmessage()` 使用共享 static buffer `static char buf[LARGEST_PRINTABLE_STRING + 1]`，且 `vsnprintf()` 结果判断为 `result <= sizeof(buf)`；exact-size 边界和跨线程可达性必须用 fail-first test 证明。
- vendored Libevent `event_base_once()` 在已经取得 `th_base_lock` 后，如果 `event_add_nolock_()` 失败，释放 `eonce` 并直接返回，没有 `EVBASE_RELEASE_LOCK`；本地三个调用点忽略返回值，可能导致死锁、连接状态泄漏或未调度回调。
- Future terminal payload 保留至 `take()` 可能形成长期 payload retention；需测量 quota、TTL、reap、take 和 owner destruction 的组合行为。
- tracked `testsuite/etc/key.pem`、`cert.pem` 权限曾为 `0644`；虽文档声明 test-only，仍需确保 release exclusion、生成策略和扫描门禁。

---

## 3. 总体不可变规则

1. 以当前 checkout 为事实来源，不把旧报告或旧测试数字写成当前通过。
2. 先只读审计、复现和定位根因，再修改；不使用临时兼容代码掩盖未知原因。
3. 不 bulk merge、不批量 `cherry-pick` upstream；逐 commit、逐 hunk、逐行为适配。
4. upstream 生成文件只作为参考；必须改本地源文件/生成规则并通过本地生成器重新生成。
5. 同一 owner 内保持 LPC 串行语义；跨 owner 不共享可变 array/mapping/object 引用。
6. 保持 owner affinity、`VMContext` 隔离、destruct/epoch/stale task 检查和 main-thread required surface。
7. 每项行为修改先有 fail-first 测试，再有定向构建、匹配的 ASan/UBSan/TSan 或 fuzz 验证、evidence metadata 和回滚条件。
8. 不在本计划阶段实现 Promise；先定义本地 Promise value、EGC、调度、owner、park/resume、shutdown、recompile 和错误传播合同。
9. 不修改历史 evidence 文档；新结果放入新的 evidence manifest，记录源提交、工作区状态、构建 preset、命令、退出码、通过/失败数量、耗时、环境和限制。
10. 不覆盖、暂存、提交或清理未知的并行改动；不使用破坏性 Git 命令。
11. 发布前必须有 live GitHub、registry、签名、provenance、受保护环境和生产容量证据；本地静态脚本不替代这些证据。

---

## 4. 统一证据和停止门

### 4.1 Evidence manifest 最小字段

机器可验证的 benchmark/smoke/capacity envelope 必须严格通过 `docs/evidence/manifest.schema.json`；schema 的必填字段不是概念性摘要，而是：

```json
{
  "schema": "fluffos.evidence.manifest.v1",
  "run_id": "...",
  "commit_sha": "40-hex",
  "build_config_hash": "...",
  "compiler": {"name": "...", "version": "..."},
  "platform": {"os": "...", "arch": "...", "cpu_cores": 0},
  "workload_version": "...",
  "started_at": "ISO-8601 UTC",
  "ended_at": "ISO-8601 UTC",
  "command": "exact command",
  "cleanup": {"state": "clean|partial|failed|unknown"}
}
```

建议同时填 `evidence_kind`、`tested_sha`、`source_sha`、`raw_report`、`raw_sha256`，并在旁边的审计账本记录 `working_tree_status/diff_hash`、baseline/upstream target、build directory/preset、runtime config、exit code、pass/fail/skip、first failure、artifact hash、资源限制和外部依赖状态。`cleanup` 不是装饰字段：owner claim、队列、引用、临时文件、socket、子进程和 driver shutdown 未回到基线时不得标 `clean`。证据状态只能写 `confirmed`、`failed`、`unverified`、`external-required`；失败必须保留原始输出路径和最小复现命令。

### 4.2 每个原子修改的固定循环

```text
读取归属文档和调用者
  -> 建立最小复现 / fail-first test
  -> 修改最小范围
  -> 重新生成所需生成物
  -> 定向 Debug/Release 构建
  -> 匹配的 ASan/UBSan/TSan/fuzz
  -> 检查 diff、测试和 evidence
  -> 单独提交
  -> 需要时用 git revert 回滚
```

### 4.3 立即停止条件

出现以下任意情况，停止当前原子单元，保留失败证据：

- assertion、SIGSEGV、死锁、ASan/UBSan/TSan 错误或未解释泄漏；
- owner affinity、同 owner 串行、`VMContext` 隔离或 mutable cross-owner sharing 被破坏；
- object/program/event/future/Promise/child process 的失败路径无法证明安全；
- generated file 与 source/generator 不一致；
- benchmark 出现未解释回退；
- 测试出现非零 driver exit、bind error、LPC assertion 或不可复现结果；
- 需要覆盖/清理并行改动；
- live 外部验证不可用却有人要求宣称发布通过。

---

## 5. 阶段 A：先解决当前 recompile/layout 失败

### A-S0：冻结当前基线

只读执行：

```bash
git status --short --branch
git diff --check
df -h /
free -h
```

保存当前 source diff，确认没有把临时 layout 诊断文件当正式测试。不要先改 digest、inheritance、migration 或 rollback。

### A-S1：独立复现五条路径

在 `build-dev-debug` 中分别运行，禁止用组合 filter 让首个失败掩盖其余结果；下列七项是本根因直接相关的子集，当前重点 GTest 批次总计 10 项时仍须把另外三项名称和结果写入 manifest：

```text
DriverTest.TestProgramLayoutDigestMatchesDescriptor
DriverTest.TestLayoutDigestBijectionOverCorpus
DriverTest.TestRecompileMigrationAddRemovePreserves
DriverTest.TestMasterExactReloadMigrateThenInit
DriverTest.TestSimulEfunReloadCreateFailureRollback
```

同时补跑：

```text
DriverTest.TestObjectVariableBlockLifecycle
DriverTest.TestClassifyLayoutRules
```

分别记录每个测试的 exit code、断言/信号、栈和工作区。当前已确认 `gdb`、`lldb`、`valgrind`、`eu-stack` 不可用；`addr2line` 对现有运行时地址只能得到 `??:0`，因此必须保留 assertion 输出、构建符号和可复现命令，不能伪造栈帧。后续若安装 debugger，只在隔离环境中重跑，不改工作区内容。

### A-S2：沿事务真实顺序定位

只读检查以下函数和直接调用者：

```text
start_recompile_transaction()
prepare_variable_migrations()
RecompilePrepared::commit_swap()
RecompilePrepared::run_create_guarded()
RecompilePrepared::commit_finish()
RecompilePrepared::rollback()
simul_efuns_prepare()
simul_efuns_activate()
simul_efuns_rollback()
simul_efuns_finish()
program_layout_digest()
describe_recompile_layout()
obj_vars_init/move/swap/destroy()
```

建立以下时间线并逐项断言：

```text
old program pin
 -> target snapshot
 -> old layout capture
 -> new layout capture
 -> migration preparation
 -> simul dispatch prepare/activate
 -> commit_swap
 -> variable block publish
 -> __INIT/create
 -> commit_finish 或 rollback
```

临时诊断如果确实需要，只记录：target object、old/new program pointer、old/new layout digest、variable count/data、migration count/state、program generation、simul table state 和每个 swap/rollback 阶段。诊断完成后必须删除，不能以日志替代不变量。

当前失败的 GTest 最小调用链已确认存在缺口：`src/tests/test_lpc.cc` 的 `TestSimulEfunReloadCreateFailureRollback` 在 `start_recompile_transaction()` 后直接 `prep.commit_swap()`，没有调用 header 要求的 `prepare_variable_migrations(&prep)`；生产 `f_recompile_object()` 则有该调用。该测试 staged source 的 `int g = boom();` 使“旧 block 仍在、new program layout 已换”的断言路径可达。下一步必须先用一个满足前置条件的回归测试区分“测试 harness 误用”与“生产入口故障”，并把迁移准备是否可跳过变成可检查的不变量；禁止把 assertion 改成赋值。

特别检查：

- `prepare_variable_migrations()` 是否在所有需要 migration 的 target 上调用；
- `migrations.size()` 与 `targets.size()` 是否可以不一致；
- exact-layout target 是否也必须发布一个新 block；
- `obj_vars_move()` 前后 `data/count/layout_id` 是否正确；
- `commit_swap()` 断言检查的对象是否已经迁移或仍持有旧 block；
- simul-efun 的 dispatch activate 是否会触发目标对象变量状态改变；
- create 失败后旧 program、旧 table、ident orphan/live 状态和引用计数是否逐项恢复；
- staged program 的初始引用、commit pin、target refs 和 snapshot refs 的释放次数。

不要通过删除断言、强制重算 digest、无条件覆盖 `layout_id` 或关闭 sanitizer 来“修复”。

### A-S3：最小修复和验收

只有根因有证据后才修改；优先增加能在修复前失败、修复后通过的回归断言。当前 GTest harness 若确认只是违反 API 前置条件，应以最小测试修复/契约测试收口；若生产入口也能复现，才进入 transaction 实现修复。验收：

- 本节五个重点路径分别通过；当前已知重点 GTest 批次为 `9/10`，必须列出第 10 项失败/未运行项，不能用“多数通过”替代失败项；
- 无布局遍历 SIGSEGV、assertion 或未解释 sanitizer error；
- 成功 reload 的变量迁移值、layout digest、function dispatch 和 generation 正确；
- create/`__INIT` 失败恢复旧 program、旧 dispatch pointer、旧 ident 状态、旧变量数据/布局和引用计数；
- staged/new program、old program、migration payload 和 snapshot 无泄漏；
- Debug、portable Release、ASan 至少完成定向验证后，才允许进入全量测试。

### A-S4：C1 原子提交边界

`base/std.h` include 修复单独属于 C1，不能和 transaction 修复混合。C1 提交前必须：

- 保留完整 diff 仅包含该 include 和必要的回归测试；
- 重新构建受影响 Debug/Release/ASan 目标；
- 运行布局和 transaction 定向测试；
- `git diff --check`、文件列表、evidence manifest 通过。

如果 include 修复后的 transaction 仍失败，C1 只能记录“消除了 ABI 导致的布局误读，尚未解决 transaction”，不能宣称整体 recompile 修复。

---

## 6. 阶段 B：构建、sanitizer、fuzz 和 testsuite 门禁

### B-S0：Canonical build 矩阵

按项目规则构建前检查磁盘和内存；一次只运行一个构建/driver，不使用无限并行。

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug \
  --target driver lpcc lpc_tests ofile_tests gateway_fuzz_smoke --parallel 4

cmake --preset portable-release
cmake --build --preset portable-release \
  --target driver lpcc lpc_tests ofile_tests gateway_fuzz_smoke --parallel 4

cmake --preset asan
cmake --build --preset asan \
  --target driver lpcc lpc_tests ofile_tests gateway_fuzz_smoke --parallel 2

cmake --preset ubsan
cmake --build --preset ubsan \
  --target driver lpcc lpc_tests ofile_tests --parallel 4

cmake --preset tsan
cmake --build --preset tsan \
  --target driver lpcc lpc_tests ofile_tests --parallel 4
```

`gateway_fuzz_smoke` 和 `ofile_tests` 是否存在由当前 CMake 配置决定；若 GTest 未找到或 sanitizer preset 未注册某 target，必须把“target 未注册/未运行”记为 `unverified`，不能把构建命令失败改写成通过。

若 preset 不含某个 target，先读 `CMakePresets.json` 和 `src/tests/CMakeLists.txt`，不要猜 target 名称。构建目录必须与 preset 对应，不能把不同缓存混用；上面命令中 target 未注册时按 `unverified` 记录，不能静默跳过。

### B-S1：测试顺序

1. 先运行五个 recompile 定向测试。
2. 再运行 owner、future、ObjectHandle、gateway、TLS、compiler 和 object-store 定向测试。
3. 再运行完整 CTest：

```bash
ctest --test-dir <canonical-build> --output-on-failure
```

4. LPC testsuite 必须从 `testsuite/` 运行：

```bash
cd testsuite
../<canonical-build>/bin/driver etc/config.test -ftest
```

使用 isolated runner 时记录四个独立端口、driver exit、`lpc_assertions_ok`、`bind_error`、超时和 kept workspace；不得用旧端口结果代替新结果。

### B-S2：Sanitizer 规则

- ASan 必须分别观察 memory error 和 LeakSanitizer；`detect_leaks=0` 只用于隔离根因，不能作为最终门禁。
- UBSan 覆盖 compiler/program layout、file-info、recompile、owner、gateway 和 network error path。
- TSan 覆盖 OwnerExecutor、future store、gateway continuation、logging、event schedule 和 async/owner 交互。
- sanitizer 失败保留完整输出、first failure、构建 preset、环境变量和 binary hash；不降低阈值。

### B-S3：真实 fuzz

- `src/tests/CMakeLists.txt` 只在 Clang 且 `GATEWAY_FUZZ_LIBFUZZER=ON` 时生成真正的 `gateway_fuzz`；普通 preset 中的 `gateway_fuzz_smoke` 只有独立 `main()` 的 smoke target，不能冒充 libFuzzer。应使用独立的 Clang 构建目录配置 `-DGATEWAY_FUZZ_LIBFUZZER=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON`，构建并运行 `gateway_fuzz -runs=... <corpus>`，并确认 binary 含 fuzzer main。
- 记录 corpus、seed、duration、RSS、crash/timeout、sanitizer、artifact hash 和 cleanup；超时、OOM、空 corpus、只跑 smoke 或 sanitizer 被关闭都不是通过。
- compiler/preprocessor 变更还需 compiler fuzz 或等价的确定性边界语料；至少覆盖 lexer mirror differential、65536-line/file-info、long diagnostics、directive splice/comment/template 和 generated grammar reload。

---

## 7. 阶段 C：当前本地运行时风险的逐项处理

每一项均遵循“先 fail-first、再最小修复、再 sanitizer、再单独提交”。

### C-S1：Libevent `event_base_once()` 锁和错误路径

重点文件：

```text
src/thirdparty/libevent/event.c
src/comm.cc
src/net/ws_ascii.cc
src/net/ws_telnet.cc
src/tests/
```

先建立能注入 `event_add_nolock_()` 失败的 seam，验证失败前后：

- `th_base_lock` 一定释放；
- `eonce` 和 event payload 不泄漏；
- `interactive_t`、socket、websocket 状态不会留下半初始化对象；
- 每个调用者检查非零返回并执行安全清理或明确 fatal；
- tick/owner-main drain/gateway continuation 状态不会卡住。

通过后再决定修复是补 unlock、统一 cleanup、还是调用者安全失败；不得只改一个调用点。运行 Debug、ASan、TSan，并覆盖 `clear_tick_events()` 和 callback lifetime。

### C-S2：`add_vmessage()`

重点文件：`src/comm.cc` 及其调用者。

先测试：

- 两个并发调用不会互相覆盖；
- `vsnprintf()` 返回等于 buffer size 时不会越界或把未终止数据当完整字符串；
- 超长消息有明确截断合同；
- error/unwind 路径不泄漏或使用悬空 buffer；
- owner worker 不能触碰不安全的 main-thread shared state。

再根据现有 API 选择最小的线程安全/边界修复，不顺便重构日志系统。

### C-S3：object store bridge

重点文件：`src/vm/internal/object_store.cc`、`object_handle.h`、owner store/coordinator。

先画出 canonical owner-local record 的所有者、索引、epoch、destruct、stale handle 和 snapshot 生命周期。逐步替换：

1. 新对象创建写入 owner-local canonical record；
2. lookup、handle resolve、destruct 和 migration 走 canonical index；
3. 对 bridge 命中、ID scan、path scan、fallback 和 stale handle 计数；
4. 在所有直接调用者迁移后再删除 bridge；
5. 保留 rollback 开关或可 revert 的单独提交。

未满足 `global_record_bridge_retirement_ready` 和 `global_live_object_bridge_retirement_ready` 前，仍是 release blocker。

### C-S4：gateway scan

重点文件：`src/packages/gateway/gateway_session.cc`、`gateway.cc`。

先使用现有 harness 测量 session 数量从小到大时：flush 延迟、p95/p99、CPU、continuation 次数、backlog、普通 tick latency 和内存。只在有预算/验收指标时引入 owner 分桶、dirty set 或增量索引；保证 session register/unregister、destruct、shutdown 和 stale entry 清理。

### C-S5：future retention

覆盖：

- payload-free terminal 是否按 TTL/reap 清理；
- payload-bearing terminal 在 `take()` 前的 quota 记账；
- take/repeated take/expired take；
- owner destroy、shutdown、failed tombstone、index cleanup；
- 大 payload、许多 future、异常 completion 和 cross-owner snapshot。

先定义可接受 retention/eviction 合同，不得无证据改变用户可见语义。

### C-S6：TLS、async/external、诊断和配置

- `sys_reload_tls()` 补齐跨平台、权限失败、异常和真实 testsuite 入口验证。
- async/external 先审计 callback、child/socket EOF、reap、cancel、shutdown 和 owner destruction；不能把现有 async package 当 Promise。
- 诊断检查动态 format、warning 上限、source context、宏展开和 buffer boundary。
- 配置检查 slot ABI、默认值、invalid value、WASM/Windows/macOS 差异和文档一致性。

---

## 8. 阶段 D：upstream 逐提交、逐 hunk 吸收

### D-S0：upstream 取证前置

执行：

```bash
GIT_NO_LAZY_FETCH=1 git show --stat 735bd31f
GIT_NO_LAZY_FETCH=1 git diff 6cf257ce..735bd31f --check
```

如果 partial clone 缺 blob：

1. 先记录缺失对象和 transport 失败；
2. 通过可访问 HTTPS、archive 或其他合法 fetch 路径定向 hydrate；
3. 重新执行 `GIT_NO_LAZY_FETCH=1` 的读取验证；
4. 未成功 hydrate 的提交标为 `upstream-unverified`，不能从 commit message 推断实现。

不要修改全局 git config、URL rewrite 或远端；不执行 bulk merge/cherry-pick。

### D-S1：37 个提交的逐 hunk 吸收决策

这里的 37 个候选是 `git log 6cf257ce..735bd31f`，不含左边界 `6cf257ce`。左边界本身已检查：它只把 docs lock 中 `fast-uri 3.1.4→3.1.5`、`js-yaml 3.14.2→3.15.1` 及对应 integrity 改动作为依赖基线；若要吸收，也必须和后续 lockfile 变更合并后再做一次 `npm ci`/lockfile 检查。下面的“逐 hunk”列记录每个 patch 的实际代码/生成/测试 hunk 及本地门禁，不是只复述 commit message：

| 提交 | 逐 hunk 核对重点 | 分类 | 本地门禁/限制 |
|---|---|---|---|
| `990f7b12` | `interpret.cc` 的 `PUSH_LOCAL/F_TRANSFER_LOCAL/F_LOCAL/F_VOID_ASSIGN_LOCAL` 由 `fp-lval` 改为 `lval-fp`，补写入边界；patch 自己注明 default-argument wrong-value 仍未解释。 | 适配吸收 | 先用 malformed bytecode/默认参数 fail-first；Debug/ASan 证明读写都变为稳定错误，不能把它当作 default-arg 根因修复。 |
| `97ef8d3d` | compiler/grammar rule 将 block-expression 保存的 `current_type` 与 context 打包；`compile_max_num_locals` 给 `__INIT` 和 optimizer scratch 使用，并做 slot bound。 | 语义移植 | 只改本地 grammar source/rules；覆盖 `catch/time_expression`、`__INIT`、tree optimizer 和 generated grammar 一致性。 |
| `58bcc963` | `rule_expr_or_block_block()` 调 `pop_n_locals()`，global declaration 只结束名字作用域；新增 scope leak fixtures。 | 必须吸收 | 覆盖越界名称、同名重声明、for/foreach 固定 pop、global initializer 后参数 slot。 |
| `153ebe09` | 分离 `release_local_names()` 与新 frame 的 `free_all_local_names()`；修复 `reallocate_locals()` 两个并行 scratch 数组；`rule_block()` 负数 clamp。 | 必须吸收 | 多层 lambda、Bison error recovery、无 initializer local、ASan heap boundary；不能只验证正常语法。 |
| `bf8861c9` | `pop_n_locals()`/`clean_up_locals()` 防错误恢复 OOB；empty class `DCALLOC(0)` 改安全分配；全部动态 compiler diagnostic 传 `%s`。 | 拆分吸收 | scope/empty-class/tainted-format 三个独立 fail-first；检查本地诊断 buffer、warning cap 和 CodeQL 规则。 |
| `331b456e` | `render_diagnostic()` 200 字符窗口同步 caret/range/fixit；`yywarn()` 每文件 100 条并报告 suppression。 | 本地适配 | 长单行 34KB、range 左出窗口、warning cap、日志磁盘上限；不要复制后续已修正的 clamp。 |
| `3d24d7ae` | 所有 `CFG_INT(__MAX_LOCAL_VARIABLES__)` 改 `CONFIG_INT`，让 option 生效并设 255；随后 `ed328800` 改为 constant。 | 不单独吸收 | 不让旧配置的无效项突然生效；只采用 `ed328800` 最终 `kMaxLocalVariables` 方向。 |
| `9e11248f` | `function_context_t.entry_num_locals` 记录 high-water，禁止 default argument expression 声明 local。 | 语义移植 | 既测拒绝非法 declaration，也测常量/efun/合法 functional defaults；与 local slot 修复一起 gate。 |
| `1a0b118d` | `__TREE__` 补 scope/type restore；diagnostic range clamp；测试 config 改 255；含 `grammar.autogen.cc` 生成结果。 | 拆分吸收 | 以本地 `grammar.y`/generator 重生成，不手抄巨大 autogen diff；Debug `__TREE__` 和 range unit test 必须通过。 |
| `4853debc` | 只改 `tree_block_scope.lpc` 格式。 | 随附处理 | 不形成独立语义提交；随对应 compiler 行为测试做 LPC 编译/运行和 `git diff --check`，不虚构本地不存在的格式化门禁。 |
| `52de0005` | 五个 workflow 的 `pull_request.branches: [master]` 删除，push 仍保留。 | 独立配置 | 先决定 `main/master` 契约；静态 YAML/actionlint 不替代 live PR check-run。 |
| `17d9d4f1` | 只删除当前 branch 不存在的 `acatch` 评论，并明确 upstream branch 自身也有独立 context。 | 不吸收 | 当前没有 Promise/acatch；不把注释移植成虚假能力声明。 |
| `ed328800` | `kMaxLocalVariables=255`、废弃 rc slot 9、`__TREE__` 独立 context、`add_walltime_event()` 检查 `event_base_once()`、elision marker 后的 diagnostic clamp；含 grammar autogen。 | 拆分吸收 | 这是 `3d24d7ae` 的最终方向；event error contract 要和本地 Libevent lock/cleanup fail-first 一起处理。 |
| `dd2a3a14` | 五个 workflow 增加 workflow/ref concurrency，master 不 cancel。 | 独立配置 | 分支契约、release record 保留、PR supersede 和 required checks 做静态+live 双检。 |
| `1e74a758` | compiler `std::move`；DB tests 不再假设 handle 1，区分 string error/int handle，清理 sqlite 文件。 | 测试/小修适配 | 只吸收本地对应 hunk；DB backend 矩阵必须覆盖 SQLite 与 CI 的 MySQL-default/SQLite-compiled 形状。 |
| `858d5da9` | 223 files：`T_PROMISE`/refcount/mark/EGC、reaction/microtask drain、async grammar/parked frame/acatch、async_yield、callback efun promise bridge、call_out promise、cycle hooks、Libevent/wasm shutdown。 | 暂缓设计 | 本地没有 Promise substrate；先写 owner/VMContext/stack/error/shutdown/recompile 合同与 fail-first，不 cherry-pick。 |
| `a540f77d` | 16-bit file-info count 分块到 65535，decoder 加 end bound；明确留下 header `[0]/[1]` 16-bit 风险。 | 分阶段吸收 | 先覆盖 65535/65536/70003 行和 line-error translation；随后必须吸收 `a781b918` 的全 ABI，不能停在半格式。 |
| `de945701` | cycles walker 删除无效 `reaction_hint`，`schedule_drain()` 的 callback 两支 move。 | 暂缓设计 | 只有本地 Promise/cycle walker 字段和 ownership 完全对应后才适配；保留 Coverity regression。 |
| `e0ce7d26` | `promise_drain_yield.lpc` 记录 handler 实测 burned time，premise 不成立时明确记录而不误判。 | 暂缓设计 | Promise substrate 落地后用它作为 scheduler test；不能把历史“yield”数字当当前性能证据。 |
| `7af5c3ff` | trace start/end/walltime collect 去掉 `enabled()` gate，修复 buffer full 后永久 wedged。 | 适配吸收 | overflow→普通 trace recovery、dump thread、文件失败清理；跑 Debug/ASan/TSan，确认 callback 销毁。 |
| `9c673da7` | `replace_program_is_noop()` 比较 resolved function table、behavior flags、NOSAVE inherit tree；保留 last-call-wins，接受 `ob->prog` identity trade-off；含 Windows GTest link hunk。 | 高风险适配 | 先画本地 function/apply/cache/clone/reload contract，覆盖 diamond/private/nomask/global-init/clone；不复制 300 行实现。 |
| `f3ab999d` | `lexer_scan.h` 统一 char/string/template/comment classifier，peek 不消费；SC_DIRECTIVE 重写；删除 pulled-lines/strip plumbing；同步 `tools/lpc-syntax`。 | 语义移植 | 先读本地 Flex API；DFA differential 20k random、apostrophe/comment/template、`__LINE__`、ASan/UBSan；generated lexer 由本地规则生成。 |
| `134bebd7` | Debug foreach temporary counter 在 control frame、`do_catch`、`unwind_to_acatch_marker` 保存/恢复；promise queue mark 不重复标记 array item。 | 暂缓设计 | 本地没有 coroutine；未来必须绑定 `VMContext`/frame-local temporary state，不复制 upstream global counter。 |
| `e0d6cca2` | package generator 生成 disabled-only `packages_missing_efuns.autogen.h` sentinel；CMake output/dependency、undefined-efun diagnostic、GTest invariant。 | 本地适配 | 运行 generator 后检查 enabled duplicate exclusion、all-enabled empty sentinel、build-tree/source-tree 不混用。 |
| `c80ce56f` | docs lock `fast-uri 3.1.5→3.1.7` 与 integrity。 | 独立依赖 | `npm ci`、lockfile diff、license/vulnerability/SBOM；不得只改版本文字。 |
| `1da7a0b6` | docs lock browserslist 及 baseline/caniuse/electron/node-releases/update-db transitive versions/integrity。 | 独立依赖 | 同上，并检查 docs build、registry 可达性和 lockfile reproducibility。 |
| `24211e79` | EGC ASCII subrange cache、CR exclusion、负 length、strstr whole-needle bound、reverse/find/split fast path、wasm32 overflow。 | 适配吸收 | ASCII/CRLF/UTF-8/empty/multi-byte delimiter/trailing delimiters、8000-token correctness+benchmark、ASan/UBSan/wasm。 |
| `9bce345a` | typed arithmetic `op=` 只对 `call_other/evaluate` 的 TYPE_ANY/UNKNOWN 包 `to_int/to_float`，不包 mixed variable。 | 语义移植 | 四种 op、int/float array、dynamic call vs mixed variable、runtime type error；必须和 `11f23e20` 测试一起落地。 |
| `11f23e20` | 只增加 call_other/evaluate arithmetic `op=` 的四 operator regression coverage。 | 测试适配 | 依赖 `9bce345a` 行为；检查 `+=/-=/*=/=` 及 int/float exact result。 |
| `a781b918` | `lpc_file_info_t=int` 改 header/pairs、`program_t`、compiler producer、interpreter decoder、disassembler、lpcc consumer。 | ABI 适配 | 只能与 `a540f77d` 端到端同步；static assertion、65536 lines、switch/traceback/disassembler/ASan/UBSan。 |
| `faccd243` | A_INCLUDES 从 compile mem block 持久到 `program_t`，array efun first-seen/dedupe/nested/main omission，core spec/docs/sidebar/generated i18n。 | 设计后适配 | 先解决本地 `A_INCLUDES=16`/`NUMPAREAS=12` 和 program allocation/ownership；生成 docs/sidebar，不直接复制 upstream storage layout。 |
| `ac9f6191` | directive header/keyword/payload 三阶段；splice-first、slash hold、comment/string/template、dead-branch Flex longest-match、JS/EBNF/TextMate同步。 | 语义移植 | 必须建立在 `f3ab999d` classifier contract 上；跨 physical line、CRLF、`#define/#if`、raw newline、generated lexer/tooling 全检。 |
| `b1745c82` | shadowing 文档、三种 lookup order、efun override 从 declaration 起生效、VitePress index/sidebar。 | 独立文档 | 先用当前 driver 重验证示例和链接；不把 upstream 的 Docusaurus/i18n 产物直接带入本地文档站。 |
| `7bcd22eb` | stack `T_LVALUE_CODEPOINT/BYTE/RANGE` box、`F_MAKE_REF` keep-alive、PoppedLvalue unwind/free、plain local/global specialized stores、ops spec/disassembler。 | 暂缓设计 | 先定义 `svalue_t`/ref ABI、scratch ownership、nested ref、owner thread 和 legacy opcode contract；覆盖 error/unwind/Release leak。 |
| `b8dd5866` | external 1267-line process/handle implementation：spawn/reap generation、stdout/stderr/stdin EOF、cancel kill、Windows CreateProcess、classic API compatibility。 | 暂缓设计 | 本地 external 只有 callback `external_start`；先设计 child ownership/owner destroy/shutdown/reap/cancel/nonzero exit，再接 Promise。 |
| `7c808c8b` | Promise combinators reaction sharing/EGC marking、promise_cancel detach/consume-once、await foreach temporary relocation、shared constants。 | 暂缓设计 | 必须在 substrate 后；定义 multicast cancellation、cycle/abandon/foreach global state，不能仅移植 LPC tests。 |
| `735bd31f` | 文档及 `promise_reject_default.lpc`：bare reject truthy `PROMISE_REASON_NO_REASON`、direct vs indirect self-resolve。 | 暂缓文档/测试 | Promise 未实现前不添加用户 API 文档；最终必须用 shared header constants 和 status-based assertions，不能只比较 truthiness。 |

### D-S2：实施依赖图

```text
A-S0 基线、证据、benchmark
  |
  +--> A-S1 低风险硬化
  |      +--> 990f7b12 local-index
  |      +--> 24211e79 EGC
  |      +--> bf8861c9/331b456 diagnostics
  |      +--> 7af5c3ff tracing
  |      +--> local backend event lifecycle
  |
  +--> A-S2 compiler scope/slot/type
         +--> 97ef8d3d
         +--> 58bcc963
         +--> 153ebe09
         +--> bf8861c9 scope/empty-class hunks
         +--> 9e11248f
         +--> 1a0b118d
         +--> ed328800 final max-local semantics
         +--> 9bce345a -> 11f23e20

A-S2
  |
  +--> B-S1 file-info
  |      +--> a540f77d chunking/bounds
  |      +--> a781b918 integer-word ABI
  |
  +--> B-S2 include storage/include_list
  |
  +--> B-S3 preprocessor
         +--> f3ab999d lexical contract
         +--> ac9f6191 splice-first scanning

B-S1/B-S2
  |
  +--> C-S1 package diagnostics, DB, CI, docs/dependencies
  |
  +--> C-S2 replace_program, lvalue, Promise tracks
         +--> 858d5da9
                +--> de945701
                +--> 7c808c8b
                       +--> 134bebd7
                       +--> b8dd5866
                              +--> 735bd31f
```

这是假设的本地实施依赖，不是 upstream ancestry 依赖。每个节点都要有本地行为差异、fail-first 测试和回滚点。

---

## 9. 阶段 E：upstream 具体技术执行清单

### E-S1：compiler locals、scope、slot、type

本地重点：

```text
src/compiler/internal/compiler.cc
src/compiler/internal/compiler.h
src/compiler/internal/grammar.y
src/compiler/internal/grammar_rules.cc
src/compiler/internal/generate.cc
```

保持不变量：

```text
current_number_of_locals = 当前可见名称数
max_num_locals            = 当前 frame 的 slot high-water
locals_ptr                = 当前名称作用域起点
type_of_locals_ptr        = 当前类型 slot 起点
```

必须验证：block 结束释放名称但不降低 high-water；`clean_up_locals()` 只清 active window；`pop_n_locals()` 防负数；`reallocate_locals()` 同时扩容 locals/type；`__INIT` 使用最终 slot；catch、time expression、DEBUG `__TREE__` 恢复外层 type/context；默认参数中声明 local 必须报错。

测试语料至少包括：

```text
testsuite/single/tests/compiler/block_expr_decl_type.lpc
testsuite/single/tests/compiler/block_expr_scope.lpc
testsuite/single/tests/compiler/nested_lambda_locals.lpc
testsuite/single/tests/compiler/scope_error_recovery.lpc
testsuite/single/tests/compiler/empty_class_init.lpc
testsuite/single/tests/compiler/default_arg_decl.lpc
testsuite/single/tests/compiler/tree_block_scope.lpc
```

### E-S2：最大 local 变量数

不采用 `3d24d7ae` 的中间方案。沿 `ed328800` 最终方向审计：

- `kMaxLocalVariables = 255`；
- local index 与单字节 bytecode 编码一致；
- `__MAX_LOCAL_VARIABLES__` 改为保留的 `__RC_INT_9__`；
- 删除 rc、Config.example、配置文档和测试配置中的 active reader；
- 保留 slot 9 的 ABI 位置，不重排 runtime config；
- 旧配置行继续无效，不让原本的 64 限制突然生效。

200 locals 必须可运行，300 locals 必须编译失败；不能生成无法编码或静默截断的 slot。覆盖 `compile_file()`、lambda、`__INIT` 和 undefined-name path，运行 Debug/Release/ASan。

### E-S3：typed compound assignment

本地保持 `grammar.y`/`grammar_rules.cc` 路径，只处理：

- 静态未知 `call_other()`/`evaluate()`、`this_object()->`、function pointer call；
- typed int/float array element；
- `+=`、`-=`、`*=`、`/=`。

不顺便扩展到 `%=`、bitwise、所有 mixed 包装或无条件转换 `TYPE_ANY`。测试覆盖两个方向、四个 arithmetic operator、mixed legacy 行为和 function pointer。

### E-S4：EGC ASCII 路径

重点：`EGCIterator.h`、`strutils.cc/h`、array、相关 tests。验证：

- `slen < 0` 不走错误 ASCII 快速路径；
- ASCII 快速路径排除 CR，保持 CRLF UAX GB3；
- `reset()` 正确失效 ICU readiness、cursor、count；
- 已知 ASCII 子范围不重复扫描；
- `count()` cursor 在结束位置；
- 保留既有 off-end `previous()` 兼容；
- 保留本地 thread-local `BreakIteratorPool`。

覆盖 ASCII/UTF-8、CR/LF/CRLF、range、reset、`len == -1`、8000/50000 token explode，并做性能基线。

### E-S5：diagnostics 与 tracing

动态格式必须变为 literal format，例如 `yyerror("%s", buf)`，检查 compiler、grammar、trees、lex 等调用者。保留本地 120 字符 source context、warning 上限、1025-byte buffer 和左裁剪 column 语义。

Tracing 重点：`Tracer::start()`、`f_trace_start/end()`、walltime callback 无条件 collect pending buffer；空 filename 仍自行短路；`MAX_EVENTS` 边界不能溢出；overflow 后再次 start/end 不污染旧 trace 文件。运行 TSan 和 owner/async 交互测试。

### E-S6：file-info 两阶段 ABI

第一阶段 `a540f77d`：保留旧 16-bit table，单 count 最大 65535，零行文件保留 entry，decoder 有 end bound；覆盖 65535、65536、更大文件、include 边界、错误文件名/行号。

第二阶段 `a781b918`：同步所有 producer/decoder/consumer，格式固定为：

```text
file_info[0] = 总分配字节数
file_info[1] = lpc_file_info_t 单位的 line-info offset
之后为 (count, file_id) int-word pairs
```

检查 `program_t::file_info`、`save_file_info()`、`epilog()`、`prepare_cases()`、`translate_absolute_line()`、`find_line()`、`dump_line_numbers()`、所有 `reinterpret_cast` 和 decoder signature。加 `lpc_file_info_t` static assertion，覆盖 65536 行、大 file id、switch、compile error、traceback、disassembler、ASan/UBSan/Release。

### E-S7：`include_list()`

本地 `A_INCLUDES=16` 超过 `NUMPAREAS=12`，include 数据尚未持久保存在 `program_t`；不能直接复制 upstream。

先设计：编译阶段记录 first-seen include、嵌套 include、排除主源文件；`epilog()` 复制名称到 program 持久内存并计入内存统计；`program_t` 有 pointer/size；core spec/registration 加入 efun；返回标准化 LPC array；recompile/replace/destruct 后旧 program 安全。

测试直接、嵌套、重复、未打开 header、主源排除、空列表、recompile、destruct：

```text
testsuite/clone/include_list_a.h
testsuite/clone/include_list_b.h
testsuite/clone/include_list_nest.h
testsuite/clone/include_list_skipped.h
testsuite/clone/include_list_src.lpc
testsuite/single/tests/efuns/include_list.lpc
```

### E-S8：预处理器

将 `f3ab999d` 与 `ac9f6191` 统一到本地 `lex.cc/lex.h`，不复制 upstream lexer generation chain。必须处理：

- apostrophe/字符字面量不吞整个文件；
- string/character/backtick 内 comment marker 是数据；
- splice-first：先反斜杠换行，再 comment/directive；
- header/keyword/payload 间允许 comment/whitespace；
- raw newline 终止 directive；
- `extract_args()` 正确处理 quote、char、backtick、括号、escape、continuation。

覆盖 LF/CRLF、EOF continuation、字符串化、嵌套括号、宏错误位置、字符串中的 `/*`/`//`，运行 compiler fuzz、ASan/UBSan。

### E-S9：package、DB、CI、docs

- `src/tools/build_packages_genfiles.sh` 的清理命令必须引用 `$PACKAGES_SPEC_FULL`，不能删除字面量。
- 生成 build-tree `packages_missing_efuns.autogen.h`，只列 disabled package efun，排除 enabled package 同名 efun，加 sentinel 和 CMake dependency。
- DB tests 不假设 handle 为 1；每个 case 独立 open/use/close。
- `52de0005`、`dd2a3a14`、`c80ce56f`、`1da7a0b6`、`b1745c82` 分开处理；docs install/build 通过后再提交；VitePress sidebar 使用本地路径。

### E-S10：`replace_program()` no-op

先写 fail-first：same target 不产生 pending swap；不同 variable layout 仍迁移；function lookup table、private/nomask/inherit/diamond、last-call-wins、staged transaction、apply cache 计数正确。

本地重点：

```text
src/packages/core/replace_program.cc
src/vm/internal/recompile.cc
src/vm/internal/base/program.cc
```

no-op 必须考虑 program/inherit equivalence、layout、function table、access flags、pending swap 和 staged state；不能用 upstream 大段实现替换本地 migration。

### E-S11：stack-resident lvalue

本地已有：

```text
global_lvalue_byte
global_lvalue_codepoint_sv
global_lvalue_range_sv
```

在设计确定前不吸收 `7bcd22eb`。必须先定义 stack lvalue ownership、nested/chained 生命周期、string/buffer/range、`free_indexed_lvalue()` 错误 unwind、owner thread 隔离、`svalue_t` ABI、opcode 编码和 legacy bytecode 兼容。测试 specialized assignment、index alias、ref、nested range、catch/error、destruct、owner worker。

### E-S12：Promise/async/external（明确延期）

不能直接吸收 `858d5da9`、`de945701`、`7c808c8b`、`134bebd7`、`b8dd5866`、`735bd31f`。本地先写设计文档和 fail-first contract，不落地运行时，至少回答：

- Promise `svalue_t`、refcount、mark/sweep/EGC；
- pending/fulfilled/rejected 状态与 reaction queue；
- microtask drain budget、owner affinity、same-owner serialization；
- `VMContext` park/resume、stack/error context restore；
- shutdown、destruct、stale task、recompile interaction、cycle detection；
- combinator/cancel/await-foreach；
- external handle 的 child reaping、stdout/stderr EOF、nonzero exit、abort/cancel、invalid index、repeated start、owner destruction、socket/child leak；
- callback API 与未来 `external_create(index,args) -> handle` / `external_start(handle) -> promise` 的兼容。

实施顺序只能是：

```text
858d5da9 Promise substrate
 -> de945701 cycle walker
 -> 7c808c8b combinator/cancel/await foreach
 -> 134bebd7 local VMContext temporary unwind
 -> b8dd5866 external handle
 -> 735bd31f docs/rejection tests
```

`134bebd7` 不能复制假设全局 VM 状态的 unwind，必须保存/恢复本地 `VMExecutionState::stack_in_use_as_temporary`。`735bd31f` 的默认 rejection reason、文本、self-rejection 和 indirect self-resolution 只能在 Promise 真正存在后实现。

---

## 10. 阶段 F：性能、容量、安全和发布

### F-S1：性能基线

优先使用已有 harness，不重写 workload：

```text
tools/perf/official_compare/run_official_vs_xk.py
tools/perf/official_compare/portable_fluffos_loadtest.py
```

固定 `mixed`、`readonly`、`clone`、`dispatch`、ASCII explode 负载，记录 CPU、RSS、median、p95、样本数、异常样本、preset、配置和 source commit。没有 unchanged-harness baseline，不得宣称优化。

### F-S2：安全审计

检查：

- TLS 权限、边界、错误、证书加载、WASM/Windows/macOS；
- test key 是否被 release artifact、Docker context、SBOM、日志携带；
- `SECURITY.md` 的联系方式、支持分支、披露流程是否真实；
- Coverity 下载/校验、第三方 manifest、SBOM、license 和漏洞更新政策；
- 外部输入长度、format string、路径、socket、child process、artifact 下载和签名验证；
- 不在文档、evidence、日志中写入 token、密钥、账号或私有连接参数。

### F-S3：分支和 workflow 合同

先决定真实主分支是 `main` 还是 `master`，再统一：

```text
.github/workflows/ci.yml
.github/workflows/docker-publish.yml
.github/workflows/codeql-analysis.yml
.github/workflows/release.yml
SECURITY.md
tools/release/common.sh
tools/release/verify-release-inputs.sh
```

确认 trigger、permissions、required checks、check name、artifact 下载、target SHA reachability、tag-last、draft-first、image promotion、rollback 和 manual publication。静态 `check-workflows.py` 通过不等于 live GitHub check-run 通过。

### F-S4：签名、provenance 和 registry

设计并验证：

1. artifact signing（例如 cosign 或项目批准的等价方案）；
2. provenance/attestation 生成；
3. release 下载端独立验证签名、证书身份、source commit、builder、artifact digest；
4. registry digest 与 release manifest 绑定；
5. protected mutation 只在 dry-run、人工批准和所有 required checks 通过后执行；
6. partial write 之后不得移动/覆盖历史 tag，按 rollback runbook 发布新的 version candidate。

没有真实 registry/GitHub/保护环境访问时，状态必须写 `external-required`。

### F-S5：生产容量

`docs/multicore-production-gate.md` 的历史数据不能复用。外部环境必须提供真实 300-player pair、长时运行、owner queue/backpressure、gateway、future、object-store 和 restart/shutdown 数据；本地小 benchmark 只能作为 preflight。

---

## 11. 原子提交、回滚和交付顺序

### 11.1 推荐提交边界

```text
C1  program_t ABI/include 同步（当前 include 修复）
C2  recompile transaction invariant + regression tests
C3  Libevent once lock/error + all caller return handling
C4  add_vmessage boundary/thread safety
C5  object-store canonical owner-local migration（按子功能拆分）
C6  gateway scan/metrics（仅有测量和指标后）
C7  future retention policy/implementation
C8  upstream 990/97/58/153/bf/331/9e/1a/ed compiler locals
C9  upstream 242 EGC
C10 upstream 7af tracing
C11 upstream a540/a781 file-info ABI
C12 include storage/include_list
C13 f3/ac9 preprocessor
C14 package generator/DB/docs/CI independent changes
C15 replace_program no-op
C16 lvalue design/implementation（设计完成后）
C17 Promise substrate（单独架构项目）
C18 external handle/Promise continuation/docs
C19 release branch contract/evidence/signing/provenance
```

不要为了“减少提交数量”把不相关的 runtime、upstream、docs、release 和 Promise 混在一起。

### 11.2 每个提交前检查

```bash
git diff --name-only
git diff --check
git status --short
```

只暂存当前原子单元；确认没有构建产物、日志、key/cert、下游 mudlib、账号、密钥、未处理临时文件或并行改动。回滚只使用该任务提交的 `git revert`，不使用 `reset --hard`。

### 11.3 回滚触发条件

立即 revert 当前单元并保留 evidence：

- 新 assertion、崩溃、死锁、sanitizer/fuzz failure；
- layout digest、file-info、slot、opcode 或 legacy LPC 语义变化未解释；
- rollback 不能恢复 program/table/variables/ident/refcount；
- owner affinity 或 mutable sharing 规则破坏；
- testsuite bind/exit/assertion 失败；
- performance 超过预设回退阈值且没有解释；
- 生成链不再可重建；
- 发布脚本发生部分远端写入且无法验证 digest/signature/provenance。

---

## 12. 最终 release-ready 判定

只有全部满足才允许写 `release-ready`：

- 当前分支契约、远端 ref、workflow trigger 和 required check 名称已 live 验证；
- layout ABI 崩溃和 recompile transaction assertion 有当前回归证据；
- canonical Debug、portable Release、ASan、UBSan、TSan、LPC testsuite 和真实 fuzz 达到门槛；
- owner/future/gateway/object-store、TLS、event、compiler、file-info 和 generated-file 相关测试有当前 evidence；
- Libevent 锁/错误、`add_vmessage()`、object-store bridge 等 blocker 已修复或获得书面、可验证的 release gate 决策；
- artifact checksum、SBOM、manifest、OCI digest、签名和 provenance/attestation 相互匹配且可独立验证；
- test key 不进入发布物，安全披露联系方式和支持分支有效；
- 生产 300-player、长时容量、受保护环境和 registry mutation 由外部证据完成；
- Promise/async 尚未实现的内容被明确标为 deferred，不被文档或 release 信息误报为已实现；
- 工作区只含已审阅、已提交或明确保留的本任务文件，无临时诊断和未知并行改动。

---

## 13. 收尾与后续恢复最短路径

1. 提交前检查 `git status --short --branch`、`git diff --check`、完整 diff、生成物来源和临时 fuzz corpus 清理状态。
2. 保留当前本地证据：Debug/portable/ASan/UBSan/TSan CTest、完整 isolated LPC testsuite、`include_list()` 定向测试和真实 Clang libFuzzer 结果。
3. 将实现与测试作为原子本地提交落地，再用独立文档提交绑定最终验证对应的精确 commit SHA；不推送。
4. 后续若实施 Promise/`T_PROMISE`、stack-lvalue 或 external-handle，先完成独立 owner/VM/错误栈/生命周期设计和合同测试，不能从延期 upstream commit 直接 cherry-pick。
5. 只有在 GitHub required checks、registry、签名/provenance、生产容量和跨平台矩阵取得外部证据后，才重新评估 release-ready；本地通过不能替代这些门禁。

---

## 14. 配套 Prompt

按用途区分两份 Prompt：

- [`docs/deep-audit-upstream-plan-generation-prompt-2026-08.md`](deep-audit-upstream-plan-generation-prompt-2026-08.md)：重新审计当前代码、核对 upstream，并生成/更新本方案；默认不执行源码实现。
- [`docs/implementation-release-execution-prompt-2026-08.md`](implementation-release-execution-prompt-2026-08.md)：复制给新 coding model，按本方案一镜到底执行本地实现、验证、证据收集、原子提交和发布前门禁；发布、部署、registry mutation 和真实 release 仍受当前用户明确授权及外部证据约束。

第二份 Prompt 明确了阶段顺序、失败停止门、恢复规则、证据 manifest、upstream 逐提交吸收、deferred 项和最终交付格式；它不能把缺失的 live GitHub、签名、provenance 或生产容量证据伪装成本地通过。
