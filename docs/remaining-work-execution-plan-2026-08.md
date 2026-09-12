# FluffOS_XK 剩余工作详细执行方案

## 1. 目标与状态

本方案承接实现提交 `e7044bf3` 与证据绑定提交 `bbfb6445`，只覆盖尚未完成的延期架构、当前证据补齐、外部发布门禁和生产规模验证。

本次执行启动时的本地状态：

- `main` 与 `origin/main` 同步，工作区干净；当前 `HEAD` 为 `c6aba24514267e3b8f5b4ca01db2ecf4847878e7`。
- 本地 canonical build 目录和 current evidence 尚未刷新；历史 449/449、LPC、isolated testsuite、include_list 和 Clang libFuzzer 数字不继承。
- `check-evidence.py` 在空 `docs/evidence` 报告集上 fail-closed；不能宣称 `release-ready`。
- 当前本地 GCC 为 13.3；Clang、Docker daemon、cosign/syft/trivy 和生产容量环境不可用，相关门禁保持 `unverified` 或 `external-required`。
- 只读 GitHub 查询可达，但 `main` 未启用 branch protection，且 `c6aba245` 的最新 CI 矩阵包含失败 job；这不能替代本次提交的 current evidence。
- Promise/`T_PROMISE`、stack-lvalue、external-handle 和完整 object-store migration 尚无兼容本地 VM 的实现基座。

最终只有在所有硬门禁拥有当前、可复现、机器可验证证据时才能标记 `release-ready`；否则使用 `blocked`、`unverified` 或 `external-required`。

## 2. 通用执行规则

1. 每阶段开始保存 `git status --short --branch`、`git log --oneline -5`、`df -h /` 和 `free -h`。
2. 先读目标模块、调用者、归属文档和现有测试，再写 fail-first 测试；不以兼容层掩盖未知根因。
3. 原子单元顺序固定为：fail-first → 最小实现 → 生成物重建 → 定向构建 → 定向测试 → sanitizer/fuzz → 文档/evidence → `git diff --check` → 原子提交。
4. 不覆盖、暂存、删除或清理无关改动；不使用 `reset --hard`、批量 cherry-pick 或强制推送。
5. 不删除 assertion、不降低门禁、不把 smoke 冒充压力证据。所有报告绑定精确 commit、构建配置、命令、环境和原始日志。
6. 本次用户授权提交和推送；发布、部署、registry mutation、真实 release 签名仍需另行授权。

## 3. A：恢复与证据基线

### A1. Checkout 审计

```bash
git status --short --branch
git log --oneline --decorate -5
git diff --check
git ls-files -u
df -h /
free -h
```

验收：没有未预期改动和冲突，磁盘/内存满足构建规则。发现并行改动立即停止并保留现场。

### A2. Evidence 合同

```bash
python3 tools/docs/check-evidence.py --reports-dir docs/evidence --schema docs/evidence/manifest.schema.json --repo .
python3 tools/docs/test-evidence-gate.py
```

空目录失败必须保留。后续每份 envelope 必须包含 schema、commit、时间、平台、构建配置、命令、退出码、摘要和原始日志位置；禁止伪造通过字段。

## 4. B：Promise 与 async

### B1. 设计前置

阅读 `src/include/type.h`、`src/vm/internal/base/svalue.h`、`src/vm/internal/base/interpret.cc`、`src/vm/context.h`、`src/packages/async/`、`docs/owner-multicore-api.md` 和 `docs/multicore-runtime-v4.md`，形成设计记录，明确：

- `T_PROMISE` 编码、引用计数、free/assign/compare/print/serialize；
- parked async frame 的 owner、epoch、program、栈、异常和 continuation；
- `acatch`、resolve/reject、取消、超时、destruct 和 stale task；
- 跨 owner payload 的不可变边界；
- interpreter、safe_apply、GC、recompile 和 object destruction 交互；
- ABI、program layout、调试输出和生成器影响。

若无法证明生命周期和 owner affinity，保持 `blocked`，不实现。

### B2. Fail-first 合同

先测试：Promise 不能静默当普通 svalue；continuation 不能携带跨 owner 可变引用；stale epoch/destruct/cancel/error 必须拒绝恢复；缺少 `acatch` 必须稳定报“不支持”。先运行最窄 Debug `lpc_tests` 和对应 LPC 测试，保存预期失败。

### B3. 实现顺序

按独立提交实施：纯 Promise value → resolve/reject 状态机 → owner mailbox/future payload → parked frame 恢复检查 → LPC 语法与 `acatch` → async 取消/超时 → recompile/destruct/GC 集成。每单元运行 Debug、ASan、UBSan；涉及线程时增加 TSan（必要时 `setarch x86_64 -R`）。任一生命周期失败即停止该单元。

## 5. C：stack-lvalue ABI

### C1. 盘点

检查 compiler 生成、icode、interpreter、reference、foreach、mapping lvalue 和所有 `T_LVALUE` 使用点，记录栈槽、容器内部槽、global/local、foreach reference、异常 unwind 和 async 保存期间的生命周期。禁止栈地址进入跨 owner 消息、Promise、长期 continuation 或可逃逸 function。

### C2. 测试与实现

先加入栈 lvalue 越界/unwind、compound assignment、foreach reference、mapping 删除重用、local index/stack depth 边界及 sanitizer 回归。随后按稳定句柄/槽索引 → compiler metadata → interpreter 访问 → 释放路径实施；每次 ABI 变化都重建受影响 grammar/efun/applies 生成物。验收为 Debug/Release/ASan/UBSan/TSan 全通过且无可变引用逃逸，否则 `blocked`。

## 6. D：external-handle 与 object-store

### D1. 生命周期合同

阅读 `src/vm/internal/object_store.cc`、对象引用/free/destruct、owner API、gateway/socket callback 和 recompile transaction，定义 handle 唯一性、owner、epoch、generation、destruct、resolve/retain/release、stale 拒绝、序列化边界，以及 store 迁移的切换/回滚/清理策略。

### D2. Fail-first 矩阵

覆盖销毁后 resolve、owner 迁移中 resolve、epoch 过期、重复 release、recompile rollback、replace_program 冲突、gateway delayed callback 和 socket release。非法路径必须稳定失败，不得 UAF、double free 或隐式 main fallback。

### D3. 迁移顺序

只读 resolve → owner/epoch 校验 → 保活与显式 release → mailbox/delayed callback → object-store 单写切换 → 清理旧路径。最终切换前保留安全回滚路径；切换后用故障注入验证。该阶段必须运行 ASan/TSan 和真实 LPC 回归。

## 7. E：当前本地证据

### E1. 构建测试矩阵

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug --target driver lpcc lpc_tests --parallel 4
ctest --test-dir build-dev-debug --output-on-failure

cmake --preset portable-release
cmake --build --preset portable-release --target driver lpcc lpc_tests --parallel 4
ctest --test-dir build-portable-release --output-on-failure

cmake --preset asan
cmake --build --preset asan --target driver lpcc lpc_tests --parallel 2
ctest --test-dir build-asan --output-on-failure

cmake --preset ubsan
cmake --build --preset ubsan --target driver lpcc lpc_tests --parallel 4
ctest --test-dir build-ubsan --output-on-failure

cmake --preset tsan
# TSan-instrumented generators also need the ASLR workaround on WSL2/Ubuntu.
setarch x86_64 -R cmake --build --preset tsan --target driver lpcc lpc_tests --parallel 2
setarch x86_64 -R ctest --test-dir build-tsan --output-on-failure
```

从 `testsuite/` 运行完整 `-ftest` 和 `tools/testsuite/run-isolated.sh`，记录 exit、assertions、bind error、端口和失败测试。真实 libFuzzer 必须与 smoke 分开、使用绝对 corpus/artifact 路径，并记录 coverage、run count、sanitizer 和 artifact。

### E2. 生成与静态门禁

从本地源和规则重建 package/efun/applies/grammar 生成物，比较各构建目录输出和 fallback grammar；然后执行：

```bash
python3 tools/docs/check-docs.py --repo .
python3 tools/docs/check-actions-pins.py --repo .
python3 tools/docs/check-workflows.py --repo .
python3 tools/docs/test-evidence-gate.py
```

每类结果写成符合 `docs/evidence/manifest.schema.json` 的当前 envelope，再运行 `check-evidence.py`。缺失、过期或环境不匹配的报告一律 `unverified`。

## 8. F：外部与生产门禁

在权限和安全环境确认后，依次执行并保存 URL/run id、commit、workflow/job、环境、参数、校验和失败日志：

1. GitHub Actions required checks、branch protection、environment gate；
2. Docker 构建、镜像扫描；registry push 仅在发布授权后执行；
3. SBOM、锁文件、漏洞扫描、provenance、attestation 和签名；
4. macOS、Windows、Ubuntu system-Clang 矩阵；
5. 真实 mudlib 的 `off`、`audit`、`enforced` 模式；
6. 生产规模 owner/gateway/socket/heartbeat/callout 和长期稳定性；
7. 失败注入、回滚、超时、断线和重连。

Docker daemon、GitHub API、签名服务或生产环境不可用时保持 `external-required`，不得以本地结果替代。

## 9. 最终判定、提交与推送

最终审计顺序：工作区干净且无冲突 → 源码/生成物来自当前 commit → 本地测试/sanitizer/fuzz/文档有当前证据 → evidence checker 通过 → 延期架构已完成或明确为 blocker → 外部/供应链/签名/跨平台/容量证据齐全 → 复核完整 diff、分支和远端。

状态只能是 `release-ready`、`blocked`、`unverified` 或 `external-required`；Promise/lvalue/external-handle 未完成时不得写 `release-ready`。

每个架构单元独立提交；evidence 文档在实现提交后绑定精确 SHA。推送前执行：

```bash
git diff origin/main..HEAD
git diff --check
git status --short --branch
git push origin main
git status --short --branch
git log --oneline --decorate -3
```

本次授权仅覆盖当前整个干净工作区的提交和推送；不执行 merge、release、deploy、registry 上传或真实签名。push 失败时保留原始错误，不强推、不改写历史。
