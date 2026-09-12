# 再次深度审计与上游吸收方案生成 Prompt

以下 Prompt 可直接交给另一个编码模型执行。**本次目标是重新审计、重新核对 upstream、重新生成方案；除非用户另行授权，不要直接修改源码、提交、推送或发布。**

```text
你是 FluffOS_XK 项目的高级代码审计、C++/LPC VM 架构、并发安全、供应链和发布工程负责人。请在当前仓库 checkout 中重新完成一次证据驱动的深度审计，并在现有方案基础上生成一份全新的、详细、可直接执行的实施与发布方案。

## 一、必须完成的三项任务

1. 再次深度、全面、多维度审计当前已经落地的代码。不要只检查已知问题，也不要只做 grep；必须阅读实现、直接调用者、数据结构、测试和构建规则。覆盖：
   - 总体架构、模块边界和 owner/service 分层；
   - VM、program/object、apply、master/simul_efun、recompile、layout、migration 和引用计数；
   - VMContext、OwnerExecutor、mailbox、future、ObjectHandle、object store、gateway 和 main-thread required surface；
   - owner affinity、同 owner 串行、跨 owner 数据传递、mutable array/mapping/object 禁止共享；
   - 并发、锁、事件循环、调度、背压、取消、异常、OOM 和 shutdown；
   - compiler、lexer、grammar、locals/scope/slot、lambda、__INIT、file-info ABI、include_list、preprocessor、lvalue、typed assignment 和生成文件；
   - 网络、TLS、WebSocket、socket close、外部进程、EOF、reap 和输入边界；
   - object/future/session/event/program 的生命周期、destruct、reload、rollback 和资源释放；
   - 安全、权限、密钥/证书、format string、整数/长度边界、路径、第三方依赖和 SBOM；
   - CMake presets、Debug/Release、ASan/UBSan/TSan、CTest、LPC testsuite、fuzz、性能、容量、CI/CD、release、registry、签名、provenance、文档和配置一致性。

2. 再次深度学习并核对 upstream 可吸收内容。完整阅读：
   - `docs/upstream-absorption-plan-d07e7641-735bd31f.md`
   - `docs/upstream-sync-evidence-2026-08.md`
   - `docs/upstream-sync-optimization-plan-2026-08.md`
   - `docs/implementation-release-execution-plan-2026-08.md`
   - 当前本地源码、测试、CMake、workflow 和生成规则。

   对现有方案列出的 upstream 从 `6cf257ce` 到 `735bd31f` 的全部 37 个候选提交逐一复核实际 patch/hunk、行为意图、本地已有实现、ABI/owner/VM 差异、依赖和风险。不能只读 commit message，不能用 ancestry 猜依赖；patch 无法取得时必须明确标为 `upstream-unverified`。

3. 在 `docs/implementation-release-execution-plan-2026-08.md` 基础上，结合第 1、2 项的最新结果，生成一份全新的详细可执行方案，并直接迭代更新该文件。必须合并事实、修正过时结论、保留仍有效的 upstream 分类和决策，不要另外创建会互相漂移的“最终方案”。

## 二、硬性约束

- 当前 checkout、当前源码、当前命令输出优先于旧报告、旧会话摘要和旧 benchmark。
- 先只读审计和复现，再提出实现；本次默认只写审计/方案文档，不未经授权修改源码。
- 修改文档前先执行 `git status --short --branch`、`git diff --check`，保护所有已有改动和未跟踪文件。
- 必须保留 `docs/upstream-absorption-plan-d07e7641-735bd31f.md`，不得覆盖、删除或静默改写。
- 禁止 bulk merge、bulk cherry-pick upstream；逐提交、逐 hunk、本地适配、独立验证、独立回滚。
- upstream 生成文件不得直接复制；必须改本地源文件/生成器/CMake，再用本地流程重新生成。
- 任何未来代码改动都必须有 fail-first 测试、调用者审计、定向构建、匹配的 sanitizer/fuzz、evidence metadata、停止门和回滚条件。
- 保持 owner affinity、同 owner LPC 串行、VMContext 隔离、destruct/epoch/stale-task 检查；禁止跨 owner 共享可变 LPC 引用。
- 不得用删除 assertion、关闭 LeakSanitizer、降低测试范围、忽略返回值或临时兼容层掩盖未知根因。
- Promise/async/external upstream 不能直接移植；先设计本地 Promise value/refcount/EGC、状态机、reaction/microtask、owner、park/resume、异常、shutdown、recompile 和 cycle 语义。
- 静态检查不等于 live GitHub、registry、签名、provenance、审批、保护分支或生产环境验证；缺证据必须写 `unverified` 或 `external-required`。
- 不泄露 token、账号、私钥、私有连接参数或会话标识。

## 三、审计方法

先建立审计矩阵。每个发现必须记录：

```text
维度
文件、符号和必要的行范围
实际行为与直接调用者
不变量/公共合同
证据来源、命令和结果
状态：已确认 / 当前失败 / 高可信风险 / 未验证 / 外部必需
严重性与影响面
根因置信度
最小修复或明确不修改的理由
fail-first 测试
构建与 sanitizer/fuzz
回滚条件
```

重点重新验证当前已知线索，而不是直接接受旧结论：

- `src/vm/internal/recompile.cc:242` 的 `layout_id` 断言；
- `recompile_layout.cc` 的 `base/std.h` include 修复及其 ABI 原因；
- Debug/ASan 的 layout、migration、master 和 simul-efun rollback；
- `event_base_once()` 锁释放和错误返回；
- `add_vmessage()` 的 static buffer 与 exact-size 边界；
- object-store global bridge、gateway O(session) 扫描、future terminal payload retention；
- `main`/`master` workflow 契约、`Analyze (cpp)` 检查名、GitHub 403/网络限制；
- artifact signing、cosign、attestation、provenance 是否存在；
- tracked test key/cert 是否会进入发布物；
- 当前 build、testsuite、sanitizer 和 fuzz 结果是否为 stale artifact。

对任何 bug 先做最小复现并追到根因，再写修复建议。对性能问题先使用现有 harness 测量，不凭感觉重构。

## 四、upstream 复核要求

为全部 37 个提交生成逐提交表，至少包含：

```text
commit
patch 是否实际取得
涉及本地文件/符号
上游意图
本地现状
本地 ABI/owner/VM 差异
分类：吸收 / 拆分 / 本地适配 / 暂缓 / 不吸收 / 仅文档
前置依赖
测试和验证命令
风险
回滚点
```

至少覆盖这些提交族：

- locals/scope/slot/type：`990f7b12`、`97ef8d3d`、`58bcc963`、`153ebe09`、`bf8861c9`、`331b456e`、`3d24d7ae`、`9e11248f`、`1a0b118d`、`ed328800`；
- CI/DB/docs：`52de0005`、`dd2a3a14`、`1e74a758`、`c80ce56f`、`1da7a0b6`、`b1745c82`；
- Promise/async/external：`858d5da9`、`de945701`、`e0ce7d26`、`134bebd7`、`b8dd5866`、`7c808c8b`、`735bd31f`；
- EGC/tracing/preprocessor/package：`24211e79`、`7af5c3ff`、`f3ab999d`、`ac9f6191`、`e0d6cca2`；
- file-info/include/assignment/replace/lvalue：`a540f77d`、`a781b918`、`faccd243`、`9bce345a`、`11f23e20`、`9c673da7`、`7bcd22eb`；
- 仅格式化：`4853debc`。

必须重新判断现有分类是否仍成立，但不得无证据推翻。特别保留这些约束：

- `17d9d4f1` 因本地没有 `acatch`，不能直接吸收；
- `3d24d7ae` 不单独吸收，采用最终 locals/config 方向；
- `a540f77d` 在 `a781b918` 前；file-info 必须同步所有 producer/decoder/consumer；
- `f3ab999d` 与 `ac9f6191` 统一为本地 lexer/preprocessor 语义；
- `7bcd22eb` 先解决本地 lvalue scratch、ABI 和 owner 生命周期；
- Promise 相关提交必须等待本地 substrate 和生命周期设计；
- 生成文件、lexer 生成链和 `replace_program()` 大段实现必须本地适配。

若 partial clone 缺少 blob：记录准确错误，尝试合法可访问的 fetch/archive 路径；失败则保留 `upstream-unverified`，不猜 patch，不修改全局 git 配置或远端。

## 五、统一方案的必备内容

更新后的 `docs/implementation-release-execution-plan-2026-08.md` 必须包含：

1. 当前基线、分支、HEAD、merge base、工作区差异、文档输入和事实优先级；
2. “已实现但需证据”“当前失败”“高可信风险”“未验证”“外部必需”的证据账本；
3. 覆盖上述全部维度的审计矩阵；
4. 37 个 upstream 提交的逐提交分类、依赖图、本地适配策略和不吸收项；
5. 明确阶段顺序、前置条件、停止门和禁止事项；
6. 每阶段的文件/符号范围、修改要点、fail-first 测试、精确构建命令和 sanitizer/fuzz 命令；
7. evidence manifest 的字段、artifact/hash、测试数量、退出码、耗时、环境和失败日志要求；
8. 原子提交边界，不能把 runtime、upstream、docs、release 和 Promise 混为一体；
9. 每个原子单元的回滚条件、失败分类和恢复方式；
10. branch contract、live GitHub、registry、签名、attestation、provenance、生产容量和 300-player 的外部门禁；
11. Promise、stack lvalue、生产容量等延期项目的先决条件和禁止提前实现的原因；
12. 给弱模型使用的“下一步一条一条执行清单”，不得要求模型猜测。

建议执行顺序：

```text
A0 重新冻结基线、工作区和 evidence
A1 解决 layout ABI/recompile transaction 当前失败
A2 刷新 Debug/Release/portable/ASan/UBSan/TSan/tests/fuzz 证据
A3 处理 Libevent、add_vmessage、owner/object-store、gateway、future 风险
B1 compiler locals/scope/type/diagnostics/tracing/EGC
B2 file-info 两阶段 ABI
B3 include storage/include_list 与 preprocessor
C1 package generator、DB、CI、docs、security/supply chain
C2 replace_program 与 stack-lvalue 设计/实现
D1 Promise substrate 本地设计
D2 Promise/external/cycle/combinator 分阶段实现
E1 branch/live checks/signing/attestation/provenance/release
E2 external capacity、300-player、长时运行和受保护环境
```

## 六、文档与最终回复要求

- 只更新方案文档，不把临时诊断、构建产物、密钥、日志或 session 信息写入方案。
- 保留历史 evidence，不覆盖历史报告；新结果使用新 manifest。
- 文档更新后运行 `git diff --check`，检查路径、命令、链接和状态是否一致。
- 没有运行的命令不能写成已通过；没有取得的 upstream patch 不能写成已研究完成。
- 如果本次只做审计和方案，必须明确写“未执行源码实现、提交、推送和发布”。

最终回复必须说明：

1. 实际审计了哪些模块和维度；
2. 当前确认的事实、失败、风险和外部阻塞；
3. 37 个 upstream 提交如何分类，哪些可吸收、哪些暂缓、哪些不吸收；
4. 更新了哪个文档；
5. 实际运行了哪些命令及通过/失败数字；
6. 哪些验证没有运行；
7. 下一条最高证据价值动作；
8. 不得声称 release-ready，除非所有本地和外部门禁都有当前证据。
```
