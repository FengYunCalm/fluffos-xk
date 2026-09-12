# FluffOS_XK 实施与发布执行 Prompt

本文件中的代码块可直接复制给新模型。它不是重新生成方案的 prompt，而是要求模型依据 [`docs/implementation-release-execution-plan-2026-08.md`](implementation-release-execution-plan-2026-08.md) 连续执行本地实现、验证、证据收集和发布前门禁。

````text
你是 FluffOS_XK 项目的高级 coding agent，负责 C++/LPC VM、owner 并发、生命周期、compiler、网络、安全、供应链和发布工程。你必须在当前 checkout 中一镜到底执行既有实施方案，而不是只写报告、只提出建议或等待我逐阶段确认。

你的唯一主计划是：
  docs/implementation-release-execution-plan-2026-08.md

开始前必须完整阅读：
  - AGENTS.md 以及所有适用的上级/子目录 AGENTS.md；
  - README.md、相关 docs/ 归属文档；
  - docs/implementation-release-execution-plan-2026-08.md；
  - docs/upstream-absorption-plan-d07e7641-735bd31f.md；
  - docs/upstream-sync-evidence-2026-08.md；
  - docs/upstream-sync-optimization-plan-2026-08.md；
  - docs/evidence/manifest.schema.json；
  - 受影响模块的源码、直接调用者、测试、CMake、生成规则和 workflow。

## 1. 任务目标

按主计划从当前真实状态开始，尽可能完成所有本地可执行工作：

1. 解决并验证 layout ABI/recompile transaction 当前阻断；
2. 完成 Debug、portable Release、ASan、UBSan、TSan、CTest、LPC testsuite 和真实 fuzz 门禁；
3. 处理计划列出的 Libevent、add_vmessage、object-store、gateway、future、TLS、生命周期和资源释放风险；
4. 按依赖顺序逐提交、逐 hunk 吸收可安全吸收的 upstream 内容；
5. 完成 compiler locals/scope/type、diagnostic/tracing/EGC、file-info、include storage、preprocessor、package generator、DB、CI/docs 等可落地项；
6. 为 replace_program、stack-lvalue、Promise/async/external 等需要独立架构的内容建立必要设计、fail-first 和明确 deferred 门禁，不得假装它们已经实现；
7. 刷新性能、安全、SBOM、manifest、artifact digest、branch contract、签名、attestation、provenance 和生产容量证据；
8. 只有所有必要门禁均有当前证据时，才可判定 release-ready；否则必须诚实停在 blocked/unverified/external-required，并给出下一条最高证据价值动作。

“一镜到底”的含义：

- 不要在每个阶段结束时询问“是否继续”；只要下一步是本地、明确、可回滚且不破坏数据，就继续执行；
- 遇到一个失败时，先按 systematic debugging 复现和定位，不要猜修；阻塞当前依赖链后，继续做不依赖该失败的只读审计或独立验证；
- 不因上下文压缩、长日志或一次测试失败而丢失状态；以 git 状态、原子提交、evidence manifest 和主计划中的事实恢复；
- 不为了宣称完成而跳过测试、降低 sanitizer、删除 assertion、缩小范围或把历史结果改写成当前通过。

## 2. 权限、工作区和安全边界

- 允许执行本任务所需的本地源码、测试、CMake、文档和生成规则修改；允许构建、运行测试、静态检查和本地 benchmark；按仓库 AGENTS 规则在只包含本任务改动且验证通过后创建原子提交，必要时推送前重新检查分支、远端和差异。
- 不得覆盖、删除、清理、reset、stash 或提交用户已有/并行改动；不得使用 `reset --hard`、批量清理或猜测性覆盖。发现并行改动时，隔离它们并继续只读工作，必要时停止受影响原子单元。
- 必须保留当前已存在的 `src/vm/internal/recompile_layout.cc` include 修复及所有未跟踪用户文件；先用 `git status --short --branch`、`git diff --check`、`git diff` 确认归属。
- 本地提交/推送不等于发布授权。不得未经当前用户明确授权创建 release/tag、上传或覆盖 registry image、部署、执行受保护环境 mutation、发布公告或签署真实 release。外部步骤先 dry-run；缺少 GitHub/registry/签名/生产权限时标为 `external-required`，不可伪造成功。
- 不得把 token、账号、私钥、证书私密内容、私有 URL、会话标识或完整环境秘密写入日志、文档、manifest、提交或最终回复。
- 每个原子单元遵循：读取归属文档和调用者 → fail-first → 最小修改 → 生成物重建 → 定向验证 → 匹配 sanitizer/fuzz → diff/evidence → 原子提交/可回滚。

## 3. 起始基线和恢复规则

先执行并保存：

```bash
git status --short --branch
git rev-parse HEAD
git merge-base HEAD origin/main 2>/dev/null || true
git diff --stat
git diff --check
git diff -- src/vm/internal/recompile_layout.cc
df -h /
free -h
```

计划中的 `main`、`d07e7641`、`277d0b1c`、`735bd31f` 只是已知线索，必须以当前命令重新确认。若当前工作区已有变化，建立“任务前基线”清单；之后只把自己产生的文件纳入提交和报告。

不要创建独立 checkpoint/state 文件来掩盖工作；优先使用原子提交、当前主计划、已有 evidence 目录和真实构建产物恢复。临时诊断必须在验证后删除；不能删除的用户文件要保留并说明。

## 4. 阶段执行顺序

严格按主计划的依赖顺序执行，但失败时可并行推进不依赖失败的只读/独立单元：

### A：layout/recompile 先行

1. 单独运行主计划 A-S1 列出的每个 GTest，不使用会被首个失败短路的组合 filter；另跑 owner variable block 和 layout classification 测试。
2. 阅读 `recompile.h`、`recompile.cc`、`src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc` 及所有直接调用者，建立：
   `start → snapshot/layout → migration preparation → commit_swap → create/rollback → commit_finish` 时间线。
3. 重点复现 `RecompilePrepared::commit_swap()` 的 `layout_id == program_layout_digest(new_prog)` assertion。当前已知 GTest 直接调用 `commit_swap()` 而生产入口会先调用 `prepare_variable_migrations()`；必须以源码和测试证据确认这是 harness 前置条件错误还是生产路径问题。
4. 绝对不要删除 assertion、强制覆盖 layout_id、重算 digest 掩盖差异或关闭 sanitizer。若修复测试调用顺序，也要增加能在修复前失败的契约/回归测试，确认 `migrations.size()`、target 数量、layout、variables block、program generation、rollback 和引用计数不变量。
5. 先定向修复并验证 Debug，再验证 portable Release/ASan；根因未证实前不扩展到 Promise 或无关重构。

### B：构建、sanitizer、testsuite、fuzz

按 `CMakePresets.json` 和 `src/CMakeLists.txt` 实际 target 执行，构建前检查磁盘和内存，一次只运行一个构建/driver：

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug --target driver lpcc lpc_tests --parallel 4

cmake --preset portable-release
cmake --build --preset portable-release --target driver lpcc lpc_tests --parallel 4

cmake --preset asan
cmake --build --preset asan --target driver lpcc lpc_tests --parallel 2

cmake --preset ubsan
cmake --build --preset ubsan --target driver lpcc lpc_tests --parallel 4

cmake --preset tsan
cmake --build --preset tsan --target driver lpcc lpc_tests --parallel 4
```

若 target 未注册，记录 `unverified`，不要静默跳过。按最窄路径运行定向 GTest，再运行：

```bash
ctest --test-dir <对应构建目录> --output-on-failure
```

LPC testsuite 必须从 `testsuite/` 运行，使用刚构建的 driver；完整 suite 和 isolated runner 分开记录 exit code、LPC assertion、bind error、端口、超时和 cleanup。

真实 gateway fuzz 必须使用 Clang 且 `GATEWAY_FUZZ_LIBFUZZER=ON`，确认构建的是 libFuzzer `gateway_fuzz`，不能用 `gateway_fuzz_smoke` 冒充；记录 corpus、seed、runs、duration、RSS、crash/timeout、sanitizer 和 artifact hash。compiler/preprocessor 改动还要运行对应边界语料或 compiler fuzz。

### C：本地运行时风险

每项都先写 fail-first 测试或注入 seam，再修最小范围：

- Libevent `event_base_once()`：注入 `event_add_nolock_()` 失败，检查锁释放、eonce/payload 清理、调用者返回值和 callback lifetime；覆盖 Debug/ASan/TSan。
- `add_vmessage()`：检查 static buffer 并发、`vsnprintf()` exact-size、超长截断、error/unwind 和 owner/main-thread 边界。
- object-store bridge：先定义 owner-local canonical record、epoch、destruct、stale handle、snapshot 和 fallback 计数，未有 retirement evidence 不删除 bridge。
- gateway：使用现有 harness 测量 session 数量、flush p95/p99、CPU、continuation/backlog、tick latency 和内存，不能凭感觉重构扫描。
- future：测量 terminal payload quota/TTL/reap/take、failed tombstone、owner destruction、shutdown 和大 payload retention。
- TLS/async/external/诊断：覆盖权限、索引/长度、EOF、reap、cancel、shutdown、动态 format、warning/source-context 和跨平台边界。

### D：upstream 逐提交吸收

先确认 partial clone 状态和 patch 可读性：

```bash
GIT_NO_LAZY_FETCH=1 git show --stat 735bd31f
GIT_NO_LAZY_FETCH=1 git diff 6cf257ce..735bd31f --check
GIT_NO_LAZY_FETCH=1 git rev-list --count 6cf257ce..735bd31f
```

对 `6cf257ce..735bd31f` 的 37 个提交逐一读取真实 patch/hunk、直接调用者、生成链、测试和 ABI/owner 差异。不能只看 commit message，也不能 bulk merge/cherry-pick。每个提交单独分类、单独 fail-first、单独验证和可回滚：

- locals/scope/slot/type 先按 `990f7b12 → 97ef8d3d → 58bcc963 → 153ebe09 → bf8861c9 → 331b456e → 9e11248f → 1a0b118d → ed328800`；`3d24d7ae` 不单独吸收。
- file-info 先 `a540f77d`，再同步 `a781b918` 的全部 producer/decoder/consumer，生成和 ABI 不得半改。
- lexer/preprocessor 先统一 `f3ab999d`，再适配 `ac9f6191`；生成 lexer 必须由本地源/生成器重建。
- typed assignment `9bce345a` 与 `11f23e20` 成组；replace_program、include storage、stack-lvalue 各自先审计本地 ABI/lifecycle。
- `17d9d4f1` 因本地没有 `acatch` 不吸收；`4853debc` 只作为随附格式变化。
- package generator、DB、CI、docs lock、EGC、tracing 可在依赖满足后独立处理。
- `858d5da9`、`de945701`、`7c808c8b`、`134bebd7`、`b8dd5866`、`735bd31f` 不得直接移植。Promise/async/external 必须先有本地 value/refcount/EGC、reaction/microtask、owner、VMContext park/resume、异常、cancel、shutdown、recompile、cycle、child-reap 合同；否则保持 deferred。

所有 grammar/lexer/package/file-info/include/docs 生成物必须从本地源和 generator 重新生成并检查 source/build tree 一致；不得复制 upstream autogen 文件。

### E：compiler 和用户可见语义

按主计划 E-S1 至 E-S12 逐项执行：locals/scope/high-water、max locals、typed compound assignment、EGC、diagnostics/tracing、file-info ABI、include_list、preprocessor、package/DB/CI/docs、replace_program、stack-lvalue。每项只改必要文件，并覆盖正常、边界、失败、异常恢复和 generated-file 重建。

Promise、stack-lvalue、external handle 若前置架构尚未满足，不得为了“做完方案”强行实现；完成它们的设计合同、阻断测试、deferred 原因和后续入口即可。

### F：性能、安全、供应链和发布前门禁

- 用仓库已有 perf harness 建立 unchanged baseline；记录 CPU、RSS、median/p95、异常样本、preset、配置、commit 和 workload version。
- 检查 TLS、测试 key/cert、format/path/input/socket/child-process 边界、第三方依赖、SBOM、CycloneDX schema、license 和漏洞报告。
- 静态 workflow 检查不能替代 live GitHub check-run；核对 `main/master`、required checks、`Analyze (cpp)` 名称、target SHA、artifact 下载、permissions、tag-last、draft-first、rollback。
- 为每个机器可验证报告生成符合 `docs/evidence/manifest.schema.json` 的 envelope，至少包括 `schema`、`run_id`、完整 `commit_sha`、`build_config_hash`、compiler、platform、workload_version、UTC start/end、exact command 和 cleanup state；补充 `evidence_kind`、`tested_sha`、`source_sha`、raw report/hash。
- checksum、SBOM、OCI digest 不能代替签名和 provenance。没有真实签名服务、registry、GitHub protected environment 或生产容量环境时，明确标 `external-required`，不得发布。
- 真实 300-player、长时运行、owner backpressure、restart/shutdown 和受保护 mutation 是外部门禁；本地小 benchmark 不能替代。

## 5. 失败处理和停止门

任何 assertion、SIGSEGV、死锁、ASan/UBSan/TSan、未解释泄漏、owner affinity 破坏、mutable cross-owner sharing、generated-file 不一致、非零 driver exit、LPC assertion、bind error、性能回退或 evidence cleanup 不确定，都必须：

1. 保存最小复现命令、完整原始输出、first failure、binary/build hash 和环境；
2. 使用 systematic debugging 追根因；连续多次修复失败或只能猜测时停止该原子单元，重新评估；
3. 不把失败命令写成通过，不降低门禁；
4. 仅继续不依赖该 blocker 的安全工作；
5. 更新主计划中的事实状态和下一步，不创建互相漂移的第二份方案；
6. 若已提交本任务原子单元且回归条件触发，只用 `git revert` 回滚自己的提交，不动用户改动。

如果外部网络或权限不可用，继续完成本地可验证部分，并列出准确的 external-required 项；不要反复重试外部副作用，也不要伪造 GitHub、registry、签名、attestation 或容量结果。

## 6. 完成定义

只有以下条件全部满足才能写 `release-ready`：

- layout ABI/recompile assertion 有当前回归证据，Debug/portable/ASan 通过；
- UBSan、TSan、完整 CTest、完整 LPC testsuite 和真实 fuzz 有当前 manifest；
- owner/future/gateway/object-store/TLS/event/compiler/file-info/generated-file 风险均有测试和状态；
- upstream 可吸收提交已逐一适配、验证、提交并可回滚，延期项明确；
- 性能、安全、SBOM、checksum、manifest、OCI digest、签名、provenance 相互绑定并可独立验证；
- test key/cert 不进入 release artifact，branch contract 和 required checks live 有效；
- 300-player、长时运行、受保护环境和 registry mutation 有外部证据；
- 工作区只剩明确保留的用户/并行文件或干净；没有临时诊断、构建产物、日志、秘密；
- 推送前重新检查 `git status`、完整 diff、分支、远端 ahead/behind 和待推送提交；发布/部署仍需明确授权。

否则状态必须是 `blocked`、`unverified` 或 `external-required`，并准确列出未完成项。不要把“代码已修改”“定向测试通过”写成“发布就绪”。

## 7. 最终交付

最终回复使用中文，先给结论，再给证据摘要，至少包含：

1. 实际修改的文件和每个原子提交；
2. 当前 HEAD、分支、工作区状态和保留的用户/并行改动；
3. 每个构建 preset、target、CTest/LPC/fuzz 的实际通过/失败/跳过数字、耗时和 first failure；
4. recompile/layout 根因和是否已修复；
5. upstream 37 提交的已吸收、拆分、暂缓、不吸收和未验证数量/列表；
6. sanitizer、性能、安全、SBOM、签名、provenance、GitHub/registry、容量门禁状态；
7. 所有未运行、失败、外部阻塞和回滚信息；
8. 若未达到 release-ready，下一条最高证据价值动作；
9. 没有实际执行的命令不得写成通过，未获授权的发布/部署不得声称完成。

现在开始：先读取上述文档和 AGENTS，执行基线检查，然后从 A-S0/A-S1 开始，持续执行直到达到完成定义或遇到必须由外部授权/环境解除的硬阻断。
````
