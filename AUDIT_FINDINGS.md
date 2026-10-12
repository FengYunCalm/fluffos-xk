# FluffOS_XK 审计发现

> 对象：用户自行改造的 **FluffOS_XK fork**，不是上游官方 FluffOS。
> 状态：静态审查与隔离动态验证的续审报告；**逐文件逐路径深审、完整 LPC 与原生跨平台验收尚未全部闭合**。
> 本轮只交付本文件与 [REMEDIATION_PLAN.md](REMEDIATION_PLAN.md)。列出的 69 项均未实施修复；续审已构建并运行本任务的隔离测试进程，未操作机器上既有 driver 或其构建目录。

## 1. 元信息与证据口径

| 项目 | 记录 |
| --- | --- |
| Fork / 分支 | FluffOS_XK / `main` |
| Fork URL | `https://github.com/<fork-owner>/fluffos-xk`；账户段脱敏，精确连接配置留在本地 `.git/config`，不复制账号或私有 SSH 参数 |
| 上游 URL / 本地引用 | https://github.com/fluffos/fluffos / `upstream/master` |
| 最初审计源码 Commit | `a4475b1afe998bb03a37bdb0596b64ab01042ed3` |
| 续审 Commit | `55c7360bdbd81ef717290c8cd3b233da42d67724`；相对最初基线只新增两份报告，产品源码相同 |
| 审计源码说明 | a4475b1a 2026-10-12 Finalize local modernization audit；后续证据按各记录的 source_git_head 与二进制摘要归属 |
| 当前本地上游快照 | `79548f86f66af19f61e03a0cca1aadc95f1e8973`（2026-10-07） |
| merge-base / 可证明的分叉基线 | `277d0b1cbc4350844d9aff07a604dfa519650d9e`（2026-01-18） |
| 基线最近 tag | `v2025.1205.0`；不是说基线提交恰好等于该 tag |
| 基线之后第一个 fork 独有提交 | 73d1b7bc 2026-03-27 Add read_json/write_json efuns；这是现有 DAG 的结果，不推断最初建仓/复制日期 |
| 源码基线取证开始 | 2026-10-12 02:38 UTC |
| 物理目录清点时间 | 2026-10-12T03:57:44.927365+00:00 |
| 本轮工作区基线 | 已跟踪未提交差异 0；暂存差异 0；非忽略未跟踪文件 0；本地 main 与 origin/main 同 SHA |
| 技术栈 | C++17 / C11（src/CMakeLists.txt:47）；根 CMake 最低 3.22；LPC、Bison、libevent、OpenSSL、ICU、PCRE、jemalloc、zlib、可选数据库后端 |
| 当前目标 | driver、lpcc、lpcshell、symbol、o2json、json2o；原生跨平台能力以实际配置/验证为准 |
| 审计执行 | 主代理交叉核验并汇总并行审读分片；最终波次 63/63 返回，没有虚构独立人工会签 |
| 审计方法 | 全文件清单/分段读取、A–P 检查点、直接调用链与上游对照；续审增加隔离构建、原生/LPC/Sanitizer 验证与失败调查 |
| 续审证据根 | `build-modernization-evidence/audit-deep-55c7360b-20261012T072825Z/`；持久本地证据，不把忽略目录或运行产物提交入库 |

**“确认”的含义：**正文共 69 项源码、配置或测试合同问题。动态事实只按 §10 的原始记录声明；未新增逐 ID 回归的项仍是源码确认，不能由通用套件通过补成已复现。P0 表示内存安全等高优先级，不表示已取得 RCE。未运行的性能、容量、修复后结果与工时估计不冒充实测。

**来源分类：**15 项上游继承、17 项 fork 引入、3 项 fork 未同步上游修复、34 项引入来源不确定。来源不确定不等于根因未核对。只有明确属于安全修复的上游差异才列入安全缺口；功能修复不伪称 CVE，缺少 commit hash 不等于代码未吸收。

最初在线查询受 403 等限制；续审已取得上游 `a2902952d74a53b5b031aa2d98633bec5fba6e55` 与本地快照之后的 6 个提交缓存。公开公告、依赖身份与本地 patch 的适用性尚未逐条闭合，不能据此宣称没有未修漏洞。未执行 fetch/merge，在线新提交不混入冻结的本地 366 个提交台账。

## 2. Fork 基线与差异报告

本节与附录 B–D 固定在最初 a4475b1a / 79548f86 的取证时点；后续只新增/修订两份报告，产品源码未变。续审身份和在线增量另见 §10，不将新增文档倒灌成旧差异或覆盖率。

### 2.1 Git 侦察结果

已读取 remote、branch、200 条带图 log、status、工作区 diff、暂存 diff、submodule status、tag，以及 merge-base 后提交与文件差异。远端配置名包括 origin、upstream 和一个名为上游 URL 的历史条目；本次没有修改这些配置。144 个本地 tag 仅用于识别历史，不等于发布验证。

- 当前默认 rename 检测：**2710 files changed, 583586 insertions(+), 187308 deletions(-)**。文件记录为新增 869、修改 955、删除 879、重命名 7。附录 B 完整列出 2710 条记录，重命名同时列旧/新路径。
- 为分模块复算使用 `--no-renames`：**2717 路径，新增 876、修改 955、删除 886；+583698 / -187420 行**。7 次纯移动在此口径分别计入新增/删除，不能混用两种分母。
- 基线之后可达的 fork 独有提交 **696** 个，附录 C 全列；上游可达而当前 HEAD 不可达的提交 **366** 个，附录 D 全列。后者**不等于 366 个未吸收补丁**。
- 没有已注册 gitlink 子模块，`git submodule status` 为空；`src/thirdparty` 是普通 vendored 文件，按文件审查，不假装已核对子模块 revision。
- 没有已跟踪未提交改动或非忽略未跟踪文件；忽略目录另见覆盖率，不能用“工作区干净”宣称它们不存在。

### 2.2 模块变化与架构集中点

下表采用上面的 **no-renames** 口径，完整分组在附录 E。文档历史证据有 345966 行新增，不能把总增行直接称为新增业务代码。第三方变化含大量样例/资料调整，也不能按改动文件数直接认定漏洞。

| 模块 | 变更路径 | +行 | -行 | 主要改造/审查重点 |
| --- | ---: | ---: | ---: | --- |
| `src/vm` | 81 | 25789 | 2340 | owner、执行器、Promise、对象句柄/重编译、线程上下文 |
| `src/compiler` | 33 | 6321 | 3707 | async/await、类型/文法、编码与诊断、源码拼写 |
| `src/packages` | 92 | 22863 | 3958 | efun、异步请求、数据库、socket、gateway、外部进程 |
| `src/net` | 13 | 783 | 376 | 协议、TLS/WebSocket、会话清理与编码 |
| `src/base` | 34 | 1437 | 623 | 文件/日志/编码、内存与平台支持 |
| `src/tests` | 41 | 35826 | 116 | C++ 回归、网络/owner/编译器测试及辅助目标 |
| `src/thirdparty` | 1329 | 68250 | 150084 | vendored 身份、局部补丁、编译配置和删减 |
| `docs/evidence` | 69 | 345966 | 0 | 历史运行材料；不作为本轮动态通过证据 |
| `tools/testsuite` | 12 | 3658 | 0 | 隔离、身份/完成屏障和进程监督 |
| `tools/lpc-syntax` | 17 | 8324 | 0 | 生成合同、tokenizer/formatter/linter |
| `testsuite/single` | 527 | 14703 | 3977 | LPC 行为/边界/失败用例 |
| `src/CMakeLists.txt` | 1 | 223 | 33 | 目标、输出路径、可选功能与 sanitizer |

改造没有自动授权普通 legacy LPC 在任意后台线程运行。owner/task admission、对象 epoch/destruct、冻结 payload、main-required efun、事务式 recompile 仍是必须保留的边界。本报告未建议删除它们或把下游 mudlib/账号/部署状态迁入驱动仓库。

## 3. 执行摘要

**当前不能给出全仓安全通过结论。** 共确认 69 项问题/缺口，均未实施修复；重点包括内存边界、网络关闭/续写、编译器和 parser 状态、原生循环预算，以及验收本身的失真。

- **P0 5 项：**AUD-001、AUD-029、AUD-032、AUD-062、AUD-066。优先处理内存安全，不夸大为已证明远程利用。
- **P1 22 项、P2 40 项、P3 2 项。** 详表分别给出触发前提、根因、来源与风险。
- Debug、ASan+UBSan、TSan 的原生套件各有 **629 项通过记录**；三配置完整 LPC 均未通过，不能相互替代。逐项 LPC 记录为 **415/419 通过、4 项失败**。
- 隔离 GCC 优化构建复现 file_io_tests **94/96、2 失败**，根因是 FORTIFY 的 __fprintf_chk 绕过现有测试注入，不是已经证明产品漏报写失败。
- 测试/证据缺口 AUD-037/038/041/042/044 与产品缺陷分列；零断言、未完成异步、skip 或缺失日志不能补记通过。
- 未把历史成绩、ready=1、分片返回数或扫描零命中当作当前安全证明。公开发布、签名、registry 推广等不作为本 fork 普通交付的额外门禁；范围仍以 [docs/project-scope.md](docs/project-scope.md) 为准。

## 4. 审计覆盖率与未覆盖清单

文件/字节/物理目录计数是最初 a4475b1a 的冻结台账，不是新增构建后的实时目录大小；续审工作与结果另列 §10。

| 层次 | 覆盖/分母 | 可证明的含义 | 不能推出的结论 |
| --- | --- | --- | --- |
| 物理文件/链接目录遍历 | 1720673/1720673，遍历错误 0 | 不跟随符号链接的文件计数、目录归属/类型；附录 F | 不能宣称忽略构建树逐行深审 |
| 当前跟踪文件枚举和逐字节分类 | 3882/3882，47525477 字节 | 全部当前跟踪路径被检查；附录 A | 不等于逐路径运行验收 |
| 文本通道完整分段读取 | 3843/3843，4509 段，1177913 行，44100899 字节 | 使用工具返回的 next offset 继续读取；不是只取每文件开头 | 不等于模型对每一行完成语义证明 |
| 严格 UTF-8 且无 NUL 的文本 | 3836/3836 | 都包含于上述读取集合 | 不等于所有 Unicode/语言输入已动态验证 |
| 原始字节文件 | 46/46 的尺寸、类型及 SHA-256 | 包括图片/字体/fuzz/raw UTF-8 fixture 和含非 UTF-8 的历史日志；附录 A.2 | 未解压执行、反汇编或验证二进制行为 |
| 关键证据交叉核验 | 69 项；新增 45 项与原 24 项统一编号 | 逐项给出源码/上下文、调用关系、证据边界与验证方法 | 问题数量不等于完成安全证明的文件数 |
| 续审动态记录 | 三配置各 629 原生通过；逐项 LPC 415/419；完整 LPC、CTest、优化 file_io 未通过 | 原始目录、失败原因及适用范围见 §10 | 不等于全量安全、原生跨平台、性能或容量验收通过 |

文本通道中的 7 个路径同时属于原始字节类：6 份历史日志及 `src/tests/fuzz/corpus/gateway/oversized_header`。它们可以被工具展示，但不能当作无损 Unicode 文本处理。另有 39 个原始字节文件只作元数据审查。因此 3843 与 46 有交集，不能相加当分母。

### 4.1 尚未完成的深度覆盖

以下明确属于未闭合范围，而不是“未发现问题即通过”：

1. VM **每条指令的全部操作数/栈/异常路径**、所有编译器语法/错误恢复分支、所有 efun 的逐元素和权限组合，没有完整逐项人工签字矩阵。
2. 2710 条 fork 文件差异已经完整列出，但没有为每个 hunk 提供独立语义论证。696 个提交不能只靠提交说明认定安全。
3. 本地快照的 366 个非祖先上游提交均已列出；在线另取得 6 个新增提交缓存（§10.3）。只有明确对照的残余修复获得归因，其余代码等价性和安全公告适用性仍待核验。
4. `src/thirdparty/**` 文本已读取；复杂第三方内部协议/密码学实现未做逐行安全证明。未用版本号代替 CVE 命中验证。
5. `build-*/**`、`build-modernization-evidence/**`、`Testing/**`、`docs` 下忽略依赖/构建物、`testsuite` 运行日志、`tools` 缓存、`.pi/**`、`.git/**` 只做物理元数据清点；它们是重复构建/历史/工具状态，未作为当前源码再次深审。具体计数见附录 F。
6. 没有证明 main-thread reclaim 与活动 owner claim 的并发隔离完全闭合；AUD-007 的实施必须先做控制屏障验证，不能未经证明跨 owner 写变量。
7. Windows/macOS/WASM 实机、真实部署的持续慢读/TLS 压力、外部数据库、性能/费用/容量与运行实例没有完整对应证据；已跑的隔离 Linux 用例不能代替这些验证，没有触碰既有 driver。

**覆盖结论：文件触达与基础读取完整；“全仓所有文件均已深度人工审计、无遗漏”这个更强目标尚未完成。** 附录 A 的 R、R-raw、M、E 是取证层次，不是安全通过标记。本报告提供已能证明的发现和可执行的后续工作，不伪造全量验收。

## 5. 项目全景

### 5.1 三层与依赖

```text
下游 LPC/mudlib、客户端（游戏数据不在本仓库）
  │ master apply / efun / input_to / owner message
  ▼
驱动层：compiler → program/bytecode → VM ↔ packages
          mainlib → backend ↔ comm/net ↔ owner admission/cleanup
  │ 标准库 / C API / 平台适配
  ▼
基础层：base、libevent、TLS/OpenSSL、ICU、zlib、数据库和 vendored 库
```

编译链是 `lex.cc/LexStream → grammar.y → parse tree/icode/generate → program`；当前路径为 `src/compiler/internal/`，并不存在任务背景所称的 `lexer.l`。解释器是 `src/vm/internal/base/interpret.cc`，不是 `src/vm/interpret.cc`。文法 fallback 由 Bison 3.8.2 生成；EBNF/JSON/TextMate 合同另由 `generate_ebnf` 生成。

功能上还可分成三个部件：**VM/编译器、网络服务器、efun 接口**。它们通过 main/backend 和包注册相连，不是三个彼此隔离的沙箱。

```text
网络服务器 ↔ main/backend ↔ VM/编译器
     ↑                         ↕
     └──── IO/系统库 ← efun/packages
```

### 5.2 事件与 VM 数据流

```text
网络可读 → transport/protocol → 会话输入缓冲 → 命令快照/owner 准入
       → VM apply/bytecode → efun/对象更新 → 输出队列 → 可写事件 drain
异步文件/DB worker → 完成请求 → backend 唤醒 → owner/main-thread 回调
heartbeat/callout/cleanup → tick callback → VM 或生命周期清理
recompile → 关闭相应准入/等待活动 claim → 暂存编译/迁移/发布 → 恢复
```

VM 调用通常经 apply/函数查询建立控制帧，解释器读取 opcode 并在各分支读写 sp/值槽或调用 efun；异常恢复上下文，await 则保持帧拥有数据后 park/resume。这里描述控制流，不表示每个分支的边界证明已完成。

解释器入口、函数表查询、类型分派、栈/控制栈和 Promise park/resume 是共享边界。网络回调跨入 LPC 后可以销毁对象，必须重新检查具体会话的生命。引用标记、释放复合值和 reclaim 的职责不同；修过其中一个不代表其他两个同步正确。

### 5.3 A–P 覆盖矩阵

| 维度 | 本轮核验对象/结论 | 发现 / 边界 |
| --- | --- | --- |
| A VM/字节码 | dispatch、函数索引、求值预算、catch/await、重编译关联路径；按真实 internal/base 路径读源码 | AUD-006/016/017/019；没有证明对外任意字节码加载入口，也没有完成每 opcode 的全部路径证明 |
| B 内存 | 会话 callback/free、复合引用、Promise mark/free/reclaim、数据库资源 | AUD-001/007/009/015/018/019；未运行 ASan/LSan |
| C 网络 | Telnet/WebSocket/TLS、输入模式、输出队列、连接清理、gateway/snoop 边界 | AUD-001/002/005/009/010/011/012；TLS 身份检查等现存约束不建议删除 |
| D 编译器 | lexer/grammar/IR、编译错误恢复、源码拼写/编码、await 上下文 | AUD-016/017；生成文件与源头都已纳入清单，未执行生成器 |
| E efun/FFI | packages 参数/权限、db/async/files/socket/external/bind 路径 | AUD-003/004/011/012/014/015；当前树未检出 valid_ffi/js_eval/jsbridge 定义，不臆造这套 API 的漏洞 |
| F 沙箱/运行时 | eval、master hooks、callout/heartbeat、配置边界 | AUD-006/007/013；exec 是会话相关 efun，不能与系统 exec 混同；WASM 外部能力边界未实机验收 |
| G 并发/事件 | worker 完成、owner 生命周期、请求身份、signal、tick/reclaim | AUD-002/003/004/007/008/015；不能以 TSan 历史日志代替当前竞争验证 |
| H 生命周期/热重载 | destruct/cleanup、会话替换、program/ref、epoch/quiescence、Promise | AUD-001/004/007/011/016/018；未提出“取消 owner 检查”式修复 |
| I 构建/依赖 | CMake/presets/options、平台宏、Windows install、SBOM/锁定 | AUD-006/013/014/020/021；没有重新测产物 PIE/canary/ASLR 或运行交叉构建 |
| J 测试/CI | C++/LPC/runner/CI、实际完成屏障、格式/生成器入口 | AUD-014/020 的门禁缺口及各问题定向回归；现有测试文件不是当前通过证据 |
| K 可观测性 | NOECHO 日志、信号诊断、状态字段、历史/当前证据区分 | AUD-005/008/013/022 |
| L 排版 | .editorconfig、src/.clang-format、clang-tidy、LPC 工具与 CI 范围 | AUD-023/024；未安装/运行固定 formatter，不用肉眼宣称全库排版通过 |
| M 注释 | 状态/输入/编译器约束、当前规范与冻结历史 | AUD-012/023；历史 Bugs 不作为现行合同，也不改写冻结历史 |
| N 架构 | compiler/VM/packages/net/base 依赖、调度/清理、共享 helper、资源生产 | AUD-002/003；以生产点预算和生命周期为边界，不新增笼统兼容层 |
| O 工程 | 规范/贡献/安全响应、依赖身份、证据工具、上游维护 | AUD-021/022/023；正式公开发布流程不属于默认必补项 |
| P fork 专项 | merge-base、完整差异/提交清单、关键本地扩展与上游残余 hunk | 6 项 fork 引入、3 项未等价同步；不能宣称其余所有 hunk 深审完成 |

### 5.4 原任务逐项追踪（109 项）

下表逐项对应原任务 A.1–P.10。列“待闭合/未验证”就是未完成的审计深度，不是通过。R 的完整读取不能替代这些更强结论；未证实的问题不另造漏洞 ID。

| 项 | 核验主题 | 实际证据/边界 | 已列问题或状态 | 文件/归属范围 |
| --- | --- | --- | --- | --- |
| A.1 | 逐 opcode 的类型/栈/越界/初始化 | interpret.cc 的全文已读，关键索引/预算已核验；每条指令全部分支没有完成签字 | AUD-006/019 | src/vm/internal/base/、src/compiler/internal/ |
| A.2 | 篡改字节码边界 | 未证明当前有对外加载任意字节码入口；人工破坏 program 内存的完整 fuzz 未执行 | 待闭合 | src/vm/internal/base/、src/compiler/internal/ |
| A.3 | svalue_t 类型混淆 | 值类型/Promise/解释器分派已读；所有联合体读写配对未形成完整证明 | AUD-018 | src/vm/internal/base/、src/compiler/internal/ |
| A.4 | sp/变参边界 | 调用帧、参数移动和直接索引入口已读；所有参数组合未动态验证 | AUD-019 | src/vm/internal/base/、src/compiler/internal/ |
| A.5 | 热重载旧 program/函数指针 | 读了重编译合同、program/frame 保持与 owner 准入；未做本轮强制并发重载 | 待动态验证 | src/vm/internal/base/、src/compiler/internal/ |
| A.6 | fork VM/类型/栈布局 | async park/frame 和 owner 改造纳入差异；未把历史测试当作当前证明 | AUD-006/016/018/019 | src/vm/internal/base/、src/compiler/internal/ |
| B.1 | 引用计数/环/清理 | 区分 mark、free_compound 和 reclaim；共享图重复访问与 Promise 覆盖有残余 | AUD-007/018 | src/vm/internal/base/、src/packages/core/reclaim.cc、src/packages/db/ |
| B.2 | 字符串引用/哈希并发 | 字符串/共享表源码纳入读取；所有线程交错及悬垂引用未穷尽 | 待闭合 | src/vm/internal/base/、src/packages/core/reclaim.cc、src/packages/db/ |
| B.3 | 分配释放/裸指针 | 追踪会话关闭和 DB 结果/失败连接所有权；未跑本轮 LSan | AUD-001/015 | src/vm/internal/base/、src/packages/core/reclaim.cc、src/packages/db/ |
| B.4 | 回调与迭代器失效 | LPC 回调可以销毁会话；关键 producer/cleanup 链已跟踪 | AUD-001 | src/vm/internal/base/、src/packages/core/reclaim.cc、src/packages/db/ |
| B.5 | fwrapv 与整数/UB | 编译选项、移位/索引路径已读；fwrapv 不证明指针/移位/数组均安全 | AUD-019；其余待组合验证 | src/vm/internal/base/、src/packages/core/reclaim.cc、src/packages/db/ |
| B.6 | fork 新内存路径 | Promise 新值类型与 owner/session 清理逐关键链对照 | AUD-018；未覆盖的 hunk 不算通过 | src/vm/internal/base/、src/packages/core/reclaim.cc、src/packages/db/ |
| C.1 | libevent 读写/释放 | 本地主要逻辑在 comm.cc；上游 transport_libevent.cc 的 hunk 已映射本地，不套用错误路径 | AUD-001/002/009/010 | src/comm.cc、src/net/、src/packages/gateway/ |
| C.2 | TLS 校验/版本/热更 | 证书/SNI/参数处理已读；配置矩阵和 TLS 热更竞态未本轮实测 | 待动态验证 | src/comm.cc、src/net/、src/packages/gateway/ |
| C.3 | WebSocket 握手/分片/关闭/端口 | 帧与会话路径已读，输出增长有缺口；未做所有 handshake/framing 组合 fuzz | AUD-002 | src/comm.cc、src/net/、src/packages/gateway/ |
| C.4 | Telnet IAC/边界/增长 | IAC 路径与文本转换/字符模式联查；不把 socket_text 修复等同 Telnet 修复 | AUD-011/012 | src/comm.cc、src/net/、src/packages/gateway/ |
| C.5 | 连接/input_to/snoop/断线 | 会话对象替换、input_to 隐藏输入、关闭后状态已跟踪；完整 snoop 权限组合未验 | AUD-001/005 | src/comm.cc、src/net/、src/packages/gateway/ |
| C.6 | nonblocking accept/洪泛 | 监听与事件接入源码已读；没有用未运行的洪泛实验声明容量或公平性 | 待动态验证 | src/comm.cc、src/net/、src/packages/gateway/ |
| C.7 | fork 网络安全默认 | 本地 owner/session 网络适配与上游差异纳入；保持 TLS 身份检查 | AUD-001/002/011/012 | src/comm.cc、src/net/、src/packages/gateway/ |
| D.1 | 文法歧义/递归/恶意源 | grammar 及 fallback 全部读取；新上下文丢失有证据，完整恶意语法 fuzz 未运行 | AUD-016 | src/compiler/internal/、tools/lpc-syntax/ |
| D.2 | lexer/长 token/转义 | 当前源头是 lex.cc，不是 lexer.l；完整 Unicode/极长 token 动态矩阵未运行 | 待动态验证 | src/compiler/internal/、tools/lpc-syntax/ |
| D.3 | 跳转/地址/局部槽位生成 | trees/icode/generate 已读；表达式可观察异常被丢弃 | AUD-017/019 | src/compiler/internal/、tools/lpc-syntax/ |
| D.4 | mixed/转换/类型检查 | 静态上下文与运行时 await/type 路径联查；所有 cast 组合尚未闭合 | AUD-016/017 | src/compiler/internal/、tools/lpc-syntax/ |
| D.5 | fork 文法/类型/codegen | 生成合同、async/await 和源码拼写改动纳入差异；不手改生成物 | AUD-016/017 | src/compiler/internal/、tools/lpc-syntax/ |
| E.1 | efun 参数/num_args/栈 | packages/spec 与关键调用者已读；未对所有 efun 各参数排列作独立签字 | AUD-003/004；其余待闭合 | src/packages/、docs/efun/、docs/apply/ |
| E.2 | FFI/valid_ffi/原生内存接口 | 当前树未检出任务所述 valid_ffi 定义；不能虚构对应 allocate/free 漏洞 | 不适用；未来新增需重新审查 | src/packages/、docs/efun/、docs/apply/ |
| E.3 | bind/valid_bind | 读取 bind 及权限入口；未做当前所有 caller/master 返回组合的运行验证 | 待动态验证 | src/packages/、docs/efun/、docs/apply/ |
| E.4 | SQL/异步数据共享 | 连接代际与 mutex 范围、结果/错误资源已跟踪；不把用户 SQL API 本身叫 SQL 注入 | AUD-004/014/015 | src/packages/、docs/efun/、docs/apply/ |
| E.5 | 文件路径/权限/边界 | valid_read/write 与文件/异步实现已读；全部路径规范化和符号链接组合未验证 | AUD-003；其余待闭合 | src/packages/、docs/efun/、docs/apply/ |
| E.6 | exec/shutdown 权限/override | 区分 LPC exec 与 OS process exec；信号 shutdown 非异步安全有证据 | AUD-008；efun 全组合待验证 | src/packages/、docs/efun/、docs/apply/ |
| E.7 | fork 新包/efun | 自定义 owner/gateway/Promise/external 纳入源码与 spec；不能视为自动继承原安全性 | AUD-003/004/018 | src/packages/、docs/efun/、docs/apply/ |
| F.1 | eval limit/成本 | Linux 与非 Linux 实现差异已直接确认；纯 LPC 忙循环须原生平台验证 | AUD-006 | src/backend.cc、src/vm/internal/、src/packages/core/ |
| F.2 | call_out/递归/destruct | callout 清理与复合值持有链已读；本轮未做全部饥饿/嵌套动态组合 | AUD-007/018 的根集合验证 | src/backend.cc、src/vm/internal/、src/packages/core/ |
| F.3 | heartbeat 稳定性/destruct | backend/heartbeat/object 生命周期路径已读；实时抖动及全 destruct 交错未实测 | 待动态验证 | src/backend.cc、src/vm/internal/、src/packages/core/ |
| F.4 | master hook 默认拒绝 | 读取关键安全 hook/caller；没有把不存在 hook 的默认处理一概说成拒绝 | 待完整逐 hook 矩阵 | src/backend.cc、src/vm/internal/、src/packages/core/ |
| F.5 | WASM/jsbridge/js_eval/CSP | 当前未检出 jsbridge/js_eval 定义；真实 browser/WASM 构建与宿主能力不宣称已验 | API 背景不适用；平台待验 | src/backend.cc、src/vm/internal/、src/packages/core/ |
| F.6 | fork 沙箱/master 扩展 | owner 主线程要求、危险能力边界与配置纳入核验，不建议删检查 | AUD-006/013/016 | src/backend.cc、src/vm/internal/、src/packages/core/ |
| G.1 | 事件循环阻塞 | 全局 reclaim 和资源生产可能拖垮主循环；未跑所有阻塞 IO 路径压力 | AUD-002/003/007 | src/backend.cc、src/mainlib.cc、src/packages/async/、src/vm/internal/ |
| G.2 | async DB 安全/耗尽 | 请求中仅保存整数 handle，worker 后查槽位；没有 bounded worker admission | AUD-003/004 | src/backend.cc、src/mainlib.cc、src/packages/async/、src/vm/internal/ |
| G.3 | signal 安全 | handler 调用日志/清理/join 不满足异步信号安全 | AUD-008 | src/backend.cc、src/mainlib.cc、src/packages/async/、src/vm/internal/ |
| G.4 | 定时器/IO 饥饿 | 事件调度与 cleanup 上限代码已读；本轮没有平台公平性/时延实测 | AUD-007；其余待动态验证 | src/backend.cc、src/mainlib.cc、src/packages/async/、src/vm/internal/ |
| G.5 | fork 锁/线程/异步 | owner executor、Promise 与 async 完成链已读；新增 join 放大旧 signal 风险 | AUD-003/004/008 | src/backend.cc、src/mainlib.cc、src/packages/async/、src/vm/internal/ |
| H.1 | recompile 身份/迁移/shadow | 合同与实现入口已读，未穷尽真实并发迁移/旧栈恢复组合 | 待动态验证 | src/vm/internal/、src/comm.cc、docs/recompile-object-v2-design-2026-08.md |
| H.2 | destruct 引用/交互 | 会话 callback 后仍读 ip；reclaim 漏 Promise 持有值 | AUD-001/007/018 | src/vm/internal/、src/comm.cc、docs/recompile-object-v2-design-2026-08.md |
| H.3 | reload 与 recompile | 接口及文档纳入读取；不混同两种操作的迁移/身份合同 | 待逐路径验证 | src/vm/internal/、src/comm.cc、docs/recompile-object-v2-design-2026-08.md |
| H.4 | clean_up 自毁/继承 | cleanup 调用链已读；所有自毁/继承回调组合未实测 | 待动态验证 | src/vm/internal/、src/comm.cc、docs/recompile-object-v2-design-2026-08.md |
| H.5 | shadow 深度/环/权限 | 相关对象/efun 路径已读；没有虚报全部 shadow 组合通过 | 待动态验证 | src/vm/internal/、src/comm.cc、docs/recompile-object-v2-design-2026-08.md |
| H.6 | fork 生命周期改造 | epoch、session 与异步 frame 改造有专门合同；旧路径仍可能未接 guard | AUD-001/004/018 | src/vm/internal/、src/comm.cc、docs/recompile-object-v2-design-2026-08.md |
| I.1 | CMake 选项/断言/UB | 已检查 option/preset 与 sanitizer 禁用项；Release assert 并非自动等于漏洞 | AUD-006/014 | CMakeLists.txt、CMakePresets.json、src/CMakeLists.txt、third_party/ |
| I.2 | 依赖锁定/CVE | 清单版本和 PURL 有直接失配；在线公告/实际系统库版本未完整获得 | AUD-021；CVE 范围 unknown | CMakeLists.txt、CMakePresets.json、src/CMakeLists.txt、third_party/ |
| I.3 | 交叉编译/WASM | 平台条件与安装规则已读；不把 Linux 模拟宏当作原生平台验证 | AUD-006/020 | CMakeLists.txt、CMakePresets.json、src/CMakeLists.txt、third_party/ |
| I.4 | 符号/PIE/ASLR/canary | 产物仅元数据清点，未对每个现有二进制做 readelf/平台等价检查 | 未动态/产物验收 | CMakeLists.txt、CMakePresets.json、src/CMakeLists.txt、third_party/ |
| I.5 | local_options 默认 | 配置模板与选项纳入全文件读取；合法性/默认值的完整组合未验 | AUD-013/014 | CMakeLists.txt、CMakePresets.json、src/CMakeLists.txt、third_party/ |
| I.6 | fork 构建/依赖默认 | 输出目录变化、DB 校验和 vendor 身份有残余问题 | AUD-014/020/021 | CMakeLists.txt、CMakePresets.json、src/CMakeLists.txt、third_party/ |
| J.1 | LPC 覆盖/错误/边界 | testsuite 及 scope/完成协议已读；没有把测试源数量当通过数量 | 各卡补失败与边界回归 | src/tests/、testsuite/、tools/testsuite/、.github/workflows/ |
| J.2 | C++ 覆盖 VM/编译/网络 | 读取注册与实际 case；缺口对应新 TestAudit*/DbIoTest | 各问题实施卡 | src/tests/、testsuite/、tools/testsuite/、.github/workflows/ |
| J.3 | CI 静态/Sanitizer/fuzz | 工作流与工具已读；缺能力/没运行不算通过，不绕开 runner | AUD-014/020/023 | src/tests/、testsuite/、tools/testsuite/、.github/workflows/ |
| J.4 | 断言是否能抓内存错 | 结构体内前读未必被 ASan 抓到，需要生产同源边界函数/证明 | AUD-001/009/015/019 | src/tests/、testsuite/、tools/testsuite/、.github/workflows/ |
| J.5 | fork case 与分支覆盖 | 定制代码已有大量测试，但本轮 24 项仍需针对性证据；不依赖历史 headline | AUD-022 与所有实施卡 | src/tests/、testsuite/、tools/testsuite/、.github/workflows/ |
| K.1 | 敏感日志 | get_user_command 在 NOECHO 路径打印原文 | AUD-005 | src/comm.cc、src/mainlib.cc、src/vm/internal/owner.cc |
| K.2 | 崩溃/诊断安全 | signal handler 直接调用非异步安全服务；未实测全部 fatal 路径泄漏 | AUD-008 | src/comm.cc、src/mainlib.cc、src/vm/internal/owner.cc |
| K.3 | 统计/指针/调试信息 | status 与证据消费联查；未完成所有统计输出逐字段权限核验 | AUD-022；指针披露全表待闭合 | src/comm.cc、src/mainlib.cc、src/vm/internal/owner.cc |
| K.4 | fork 日志/status | 源码常量 ready 与历史证据不能绑定当前运行版本 | AUD-005/022 | src/comm.cc、src/mainlib.cc、src/vm/internal/owner.cc |
| L.1 | format/缩进/括号/命名 | 权威规则与 CI 现行范围不一致；本轮未运行固定 formatter | AUD-023 | src/.clang-format、src/.clang-tidy、docs/coding-standard.md、tools/style/ |
| L.2 | 头文件/include/前向声明 | 已做基础读取；没有生成所有 include 的完整规范签字 | 待完整风格门禁 | src/.clang-format、src/.clang-tidy、docs/coding-standard.md、tools/style/ |
| L.3 | 现代 C++ 一致性 | 读取 constexpr/nullptr/enum/override 使用；不把兼容第三方 C 强制改 C++ | 规则应用范围已区分 | src/.clang-format、src/.clang-tidy、docs/coding-standard.md、tools/style/ |
| L.4 | C/C++ 内存与 cast | 配对错误按实质资源问题报告，不因 malloc 或 C cast 外观直接定罪 | AUD-015 | src/.clang-format、src/.clang-tidy、docs/coding-standard.md、tools/style/ |
| L.5 | 函数/复杂度/常量/全局 | 识别资源生产与图遍历的实质复杂度，不仅按长函数报警 | AUD-002/003/007 | src/.clang-format、src/.clang-tidy、docs/coding-standard.md、tools/style/ |
| L.6 | 复制/重复 | 保留生命周期不同的路径；不因文本相似合并 owner/session 语义 | 无独立已证实缺陷；继续定向审查 | src/.clang-format、src/.clang-tidy、docs/coding-standard.md、tools/style/ |
| L.7 | 命名/拼写 | 当前新代码规范与门禁范围已核对；不全库改名破坏接口 | AUD-023 | src/.clang-format、src/.clang-tidy、docs/coding-standard.md、tools/style/ |
| L.8 | fork 风格漂移 | 与本 fork 权威规范比较，不要求复制全部上游新风格 | AUD-023 | src/.clang-format、src/.clang-tidy、docs/coding-standard.md、tools/style/ |
| M.1 | 注释准确/过时 | 字符输入、风格阶段与构建说明存在漂移 | AUD-012/023/024 | docs/、src/、AGENTS.md |
| M.2 | 公共 API/算法/安全说明 | 文档/注释纳入读取；新增 budget/代际/遍历必须说明所有权和失败 | 对应实施卡文档字段 | docs/、src/、AGENTS.md |
| M.3 | 注释格式 | 不机械统一 // 与 /* */；遵守位置需要与项目规范 | 未跑全量格式工具 | docs/、src/、AGENTS.md |
| M.4 | 死注释/TODO/版权 | 基础扫描纳入；历史/第三方版权和冻结 Bugs 不因过时就删除 | 没有逐项技术债结案 | docs/、src/、AGENTS.md |
| M.5 | 注释与行为 | 控制键、ready 与当前 CI 口径联查 | AUD-012/022/023 | docs/、src/、AGENTS.md |
| M.6 | 参数/返回/异常注释 | 已读公共文档；未把所有函数缺注释都列缺陷 | 待逐公共合同完整核验 | docs/、src/、AGENTS.md |
| M.7 | 过度/不足注释 | 只要求非显然安全约束，不复述语句或复制 README | 修复时按归属更新 | docs/、src/、AGENTS.md |
| M.8 | API 文档生成 | VitePress/文法/语法合同相关文件已读取；未运行生成/网站构建 | 不宣称生成站点已通过 | docs/、src/、AGENTS.md |
| M.9 | fork 设计/风险说明 | owner/runtime/modernization 文档存在；部分历史快照不能证明当前状态 | AUD-022/023 | docs/、src/、AGENTS.md |
| N.1 | 分层/依赖/环 | 按 compiler/VM/packages/net/base 画直接依赖；未完成全仓 include SCC 证明 | 待完整依赖图核验 | src/{compiler,vm,packages,net,base}/、docs/owner-multicore-api.md |
| N.2 | 职责/抽象/耦合 | 资源预算和持有者应在生产点明确，不加无消费者通用层 | AUD-002/003 | src/{compiler,vm,packages,net,base}/、docs/owner-multicore-api.md |
| N.3 | 全局/单例/宏 | 区分刻意 VM 上下文与风险全局状态；信号处理是直接缺口 | AUD-008 | src/{compiler,vm,packages,net,base}/、docs/owner-multicore-api.md |
| N.4 | 扩展 efun/transport/platform | CMake/spec/adapter 入口已识别；新平台不可沿用不生效预算 | AUD-006/020 | src/{compiler,vm,packages,net,base}/、docs/owner-multicore-api.md |
| N.5 | 热重载/引用/事件一致性 | mark/free/reclaim 不等价；main/owner 收敛必须同一安全模型 | AUD-001/007/018 | src/{compiler,vm,packages,net,base}/、docs/owner-multicore-api.md |
| N.6 | 跨层补丁/绕过抽象 | 旧 ASCII 回调未享有 Telnet guard 的保护；不以合并代码代替所有权证明 | AUD-001/011 | src/{compiler,vm,packages,net,base}/、docs/owner-multicore-api.md |
| N.7 | 编译/运行配置 | 运行时预算走现有 rc 表，DB 类型校验修真实变量而非字面文本 | AUD-002/003/013/014 | src/{compiler,vm,packages,net,base}/、docs/owner-multicore-api.md |
| N.8 | 错误处理统一 | 配置 FATAL 与 native cleanup/error 生命周期失配需修；不统一成吞错 | AUD-013/014/015 | src/{compiler,vm,packages,net,base}/、docs/owner-multicore-api.md |
| N.9 | fork 边界/回移 | 保留 owner/Promise 合同，以小 hunk 回移通用修复；不整包覆盖 | §8 与上游同步计划 | src/{compiler,vm,packages,net,base}/、docs/owner-multicore-api.md |
| O.1 | 审查/分支/提交 | 读取 AGENTS/CONTRIBUTING/历史；本次不改远端保护策略 | 本地流程可核验；远端设置未验证 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| O.2 | CI 门禁组成 | 注册与工作流已读；源码有门禁不等于本轮门禁已运行 | AUD-014/020/023 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| O.3 | 测试金字塔/基准 | runner/单元/LPC/性能工具有边界；新根因不能用历史全量 headline 掩盖 | 逐卡回归与性能计划 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| O.4 | README/构建/变更/版本 | 权威文档均纳入清单，当前范围、安装与参数说明失配有证据 | AUD-020/022/023/024 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| O.5 | 依赖/可复建/安全响应 | SECURITY/manifest/SBOM 已读；不强推正式发布体系 | AUD-021 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| O.6 | 债务/模板/CODEOWNERS | 按自用 fork 需求区分可选流程与正确性，不因无模板自动报漏洞 | 主计划保留问题追踪 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| O.7 | Wall/Wextra/Werror | 构建 flags 定义已读；没有声称每条实际 compiler invocation 均启用所有警告 | 需按 compile_commands 实证 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| O.8 | 规范执行 | 当前 --all-native 已落地，文档仍残留试点说法 | AUD-023 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| O.9 | 版本/签名/分发 | 版本/tag 元数据已记录；不发布 release-ready 或未运行的产物签名证明 | 公开发布/分发不在授权 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| O.10 | fork 维护/同步/保护 | 独立 scope、CHANGELOG、modernization/absorption 记录存在；冻结历史不改写 | AUD-021/022/023；远端保护未核验 | AGENTS.md、CONTRIBUTING.md、CHANGELOG.md、SECURITY.md、.github/ |
| P.1 | 全部本地提交/未提交/私有补丁 | 696 个提交全列；起点已跟踪差异/暂存/非忽略未跟踪均为 0 | 附录 B/C；忽略目录另列 | Git 差异/提交附录、现行主计划、上游只读对象 |
| P.2 | 新增/修改/删除逐项 | 2710 条差异全列，当前文本全读；每 hunk 的语义深核尚未闭合 | 明确未完成范围 | Git 差异/提交附录、现行主计划、上游只读对象 |
| P.3 | 改动破坏原安全模型 | 关键 VM→efun→对象、网络→LPC→cleanup 已交叉核验 | 24 个发现；不作其余全安全保证 | Git 差异/提交附录、现行主计划、上游只读对象 |
| P.4 | 漏合并上游安全 | 残余代码与 b2ba/5f/858 来源对照；不是按 hash membership 定缺陷 | AUD-009/011/012；018 为参考 | Git 差异/提交附录、现行主计划、上游只读对象 |
| P.5 | 新增 efun/包/FFI | 真实 spec/package 与 owner/main 要求已读；不存在的 FFI 不强行造例 | AUD-003/004/018 | Git 差异/提交附录、现行主计划、上游只读对象 |
| P.6 | VM/compiler/net 新面 | async 上下文、Promise 根、输出/会话遗留交互有明确风险 | AUD-001/002/016/018 | Git 差异/提交附录、现行主计划、上游只读对象 |
| P.7 | 构建/依赖/local_options | DB 模板、Windows output/install、vendor/PURL 身份联查 | AUD-014/020/021 | Git 差异/提交附录、现行主计划、上游只读对象 |
| P.8 | fork 文档/测试/CI | 大量新增测试与合同不替代当前实际验证，ready 和范围说明需纠正 | AUD-022/023 | Git 差异/提交附录、现行主计划、上游只读对象 |
| P.9 | 私有补丁维护/回移/冲突 | 上游混合大提交包含不相关重构；本地 owner/async 不可被覆盖 | 只移植已证实残余 hunk | Git 差异/提交附录、现行主计划、上游只读对象 |
| P.10 | 上游同步清单/顺序 | 列两笔补丁来源和一笔设计参考；整包 cherry-pick/merge 不适合本轮 | 方案 §7 | Git 差异/提交附录、现行主计划、上游只读对象 |

## 6. 问题总表

状态均为静态发现、未修复；代码项没有本轮动态复现/利用证明。工作量是估值。

| ID | 级别 | 类别 | 来源 | 标题 | 位置 | 证据片段 | 影响 | 根因 | 验证方式 | 修复建议 | 关联文件 | 估计工时 | 状态 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AUD-001 | P0 | B C H P | 上游继承问题 | ASCII 输入回调销毁会话后继续访问 interactive_t | src/comm.cc:1332–1343；src/comm.cc:2043–2046；src/comm.cc:2079–2091 | `owner_bound_safe_apply(APPLY_PROCESS_INPUT, ip->ob, 1, ORIGIN_DRIVER, "interactive");` | get_user_data 随后读取已释放会话的 text_start/text_end，构成可达的释放后使用。未证明远程代码执行；默认仅启用其他端口时不等于已触发。 | ASCII 分行循环跨 LPC 回调保留裸 ip；现有延迟清理仅由 TelnetReceiveGuard 的深度计数保护。owner scope 只绑定执行上下文，不延长会话生命。 | 私有 bufferevent 夹具接入实际 LPC process_input：分别关闭当前连接、destruct 自身、关闭后建立另一会话；推进夹具 event_base 后检查旧会话只清理一次。 | 把既有接收期 guard 覆盖到所有直接同步回调的接收路径；回调后在持有 guard 时检查会话存活，失效立即返回。 | `src/comm.h`、`src/net/telnet.cc`、`src/tests/test_lpc_gateway.cc`、`testsuite/clone` | 8–16 小时 | 静态确认；未修复 |
| AUD-002 | P1 | C G N | 上游继承问题 | 交互输出队列没有字节预算和背压 | src/net/ws_ascii.cc:148–155；src/net/ws_telnet.cc:123–130；src/net/telnet.cc:97–105 | `evbuffer_add(pss->buffer, data, len);` | 排队字节可随输出累计，导致内存压力和服务不可用。没有测得本机耗尽阈值或吞吐下降数据。 | evbuffer_add/bufferevent_write 的实际入队点未检查单会话或全局累计字节；读端限额、单次 write 长度和 libwebsockets 帧校验不能限制待发送队列。 | 在私有 loopback/受控传输夹具暂停读端；检查精确边界、单块超限、累计超限、恢复读取、关闭和 TLS 返回值大于 payload 的场景。 | 在实际入队点统一预留并归还字节预算，超限关闭具体慢会话并计数，不阻塞主事件循环或静默截断。 | `src/comm.cc`、`src/net/websocket.cc`、`src/net/websocket.h`、`src/comm.h`、`src/base/internal/rc.cc`、`src/base/internal/rc.h`、`src/include/runtime_config.h`、`src/tests/test_lpc_gateway.cc`、`src/Config.example` | 24–40 小时 | 静态确认；未修复 |
| AUD-003 | P1 | E G N | 上游继承问题 | 异步文件/数据库执行没有工作线程和排队内存上界 | src/packages/async/async.cc:284–299；src/packages/async/async.cc:449–474 | `std::thread(thread_func).detach();` | 排队为空不代表工作线程空闲，因此可反复创建 detached thread；getdir 在输出数组截断前累计所有 dirent。可造成线程/内存资源耗尽。 | 缺少覆盖 queued、running、finished-undelivered 的统一准入预算；MAX_ARRAY_SIZE 只限制交付结果，没有约束生产时的分配。 | 用可控工作屏障提交超过上限的正常短任务，核对线程数、拒绝合同、回调唯一性、取消/关闭归还；大目录夹具必须受控并位于隔离根。 | 复用现有请求队列、唤醒和 owner completion，改为有界 worker 数、请求数及字节预算；getdir 在分配处限制目录项。 | `src/packages/async/async.h`、`src/base/internal/rc.cc`、`src/include/runtime_config.h`、`src/tests/test_lpc.cc`、`testsuite/etc/test-scopes.tsv`、`src/Config.example` | 24–40 小时 | 静态确认；未修复 |
| AUD-004 | P1 | E G H | 上游继承问题 | 异步数据库请求只保存可复用整数连接号 | src/packages/async/async.cc:402–418；src/packages/async/async.cc:554–564；src/packages/db/db.cc:639–662 | `db_t* db = find_db_conn(req->handle);` | 旧 SQL 可能在新连接执行，破坏连接身份、事务和数据边界；连接半初始化时还有并发访问风险。未证实可利用的内存破坏，不按 RCE 报告。 | Request 只有 handle，执行时再次查槽位；锁保证单次访问互斥，但不能证明两次查找是同一代连接。 | ELF test-only wrapper 在异步线程调用 db_lock_mutex 前暂停，主线程关闭并重建同号 SQLite 连接，再释放 worker；旧请求不得触达新库。 | 内部 generation+连接发布状态；在准入时锁内取代号，执行时锁内验证同一连接；两个异步 API 共用这条路径。 | `src/packages/db/db.h`、`src/packages/db/db.cc`、`src/packages/async/async.cc`、`src/tests/CMakeLists.txt`、`testsuite/single/tests/efuns/async.c` | 16–32 小时 | 静态确认；未修复 |
| AUD-005 | P1 | C K | 上游继承问题 | connections 调试日志记录 NOECHO 输入正文 | src/comm.cc:1573–1581 | `debug(connections, "get_user_command: user_command = (%s)\n", user_command);` | 密码或其他隐藏输入可能以明文进入日志。默认 debug_level=0 限制默认暴露，但不修复调试时泄漏。 | debug 输出位于 NOECHO 检查和模式清理之前，直接格式化 user_command。snoop 的 NOECHO 保护不覆盖日志。 | 用非真实凭据的固定测试标记分别走普通输入、NOECHO、字符模式与 gateway snapshot；捕获日志，隐藏标记不得出现。 | 在模式位清理之前决定脱敏；隐藏输入只输出固定标记或长度，保留非隐藏调试能力。 | `src/base/internal/log.cc`、`src/tests/test_lpc_gateway.cc`、`src/comm.h` | 4–8 小时 | 静态确认；未修复 |
| AUD-006 | P1 | A F I | 上游继承问题 | 非 Linux 构建没有执行求值时间限制 | src/vm/internal/eval_limit.cc:9–29；src/vm/internal/base/interpret.cc:2478–2487 | `outoftime = 0;` | set_eval 只把 outoftime 清零，get_eval 固定返回 max_eval_cost；求值循环没有另一套递减预算。长循环可持续占用执行线程。 | 计时实现只编译于 __linux__，其他平台只有警告而无等价保护。 | 真实 macOS 和 Windows 各运行有截止时间的正常计算、超过预算的有限计算、嵌套 catch、set_eval_limit 和不同 owner 互不污染测试。 | 保留 Linux 计时器；非 Linux 使用线程私有单调 deadline 和 VM 安全点检查，使 set_eval/get_eval 语义一致。 | `src/vm/internal/eval_limit.h`、`src/vm/internal/posix_timers.cc`、`src/vm/internal/base/promise.cc`、`.github/workflows/ci.yml`、`src/tests/lpc_vm_bench.cc` | 24–40 小时 | 静态确认；未修复 |
| AUD-007 | P1 | B F G H | 上游继承问题 | reclaim_objects 对共享或循环复合值重复遍历 | src/packages/core/reclaim.cc:24–50；src/packages/core/reclaim.cc:113–135 | `if (nested > MAX_RECURSION) {` | 仅用递归深度 25 不能限制宽图/共享图工作量；共享子图可按分支数重复展开，阻塞主线程。这里是源码复杂度结论，不是实测停顿时长。 | 递归访问没有按复合分配身份去重；后台 tick 的时间片只在整次 callback 返回后检查，不能打断一次 reclaim。 | 少量节点构成共享 DAG/环，在测试计数器中验证每个容器内容只扫描一次；同一已 destruct 对象的每条引用仍分别归零并减计数。 | 在一轮回收中采用显式工作表和 visited 集合；区分容器只扫描一次与每个引用槽必须处理。 | `src/packages/core/call_out.cc`、`src/backend.cc`、`src/vm/internal/base/svalue.cc`、`src/tests/test_lpc.cc` | 16–32 小时 | 静态确认；未修复 |
| AUD-008 | P1 | G K P | 上游继承问题 | 操作信号处理器直接执行 VM、日志和线程停止 | src/mainlib.cc:141–193；src/vm/internal/owner.cc:6287–6319 | `vm_owner_thread_stop();` | handler 调用非异步信号安全路径；SIGINT 的 owner stop 会加锁并 join，可能重入锁而死锁。没有对现有进程发送信号验证。 | 把正常线程的诊断/VM 调用/关闭工作放在异步中断上下文。fork 加入 owner 线程后扩大了继承设计的死锁风险。 | 仅对自建短生命周期 probe 在已知持锁屏障期间发送操作信号；验证 handler 返回、主线程安全点处理和限时退出，不能向现有 driver 发信号。 | handler 仅设置 lock-free pending 标志并使用经过审核的异步安全唤醒；VM/诊断/join 在正常执行安全点完成。 | `src/backend.cc`、`src/backend.h`、`src/vm/internal/base/interpret.cc`、`src/vm/internal/eval_limit.cc`、`src/tests/CMakeLists.txt` | 16–32 小时 | 静态确认；未修复 |
| AUD-009 | P2 | B C P | fork 未同步上游修复 | ASCII 空行在缓冲区起点前读一个字节 | src/comm.cc:1315–1327 | `if (*(nl - 1) == '\r') {` | CRLF 检测执行 *(nl - 1)，当 nl 在缓冲区起点时发生前读。尚未验证实际分配布局下的可利用性；不能仅凭一次前读宣称 RCE。 | 未先证明 nl > p 就读取分隔符前一字节。 | 私有 ASCII 接收夹具覆盖起始 LF、空行、连续 LF、CRLF、分片 CR/LF；检查字节访问边界和 callback 次数。 | 保持原始 LF 指针不变；仅在原始 LF 大于行首时检查前一个 CR。 | `src/tests/test_lpc_gateway.cc`、`src/comm.h` | 2–4 小时 | 静态确认；未修复 |
| AUD-010 | P2 | C | 上游继承问题 | ASCII CRLF 修剪后下一行游标退回分隔符 | src/comm.cc:1318–1343 | `p = nl + 1;` | text_start 按 LF 后位置推进，p 却按减一后的 nl+1 推进；二者不一致，后续命令可能变空或最后一行要等新数据才被处理。 | 同一个可变 nl 同时表示原始分隔符位置和修剪后的正文末尾。 | 比较同一正常多行命令在所有拆包方式下的回调序列；混合 LF/CRLF、空正文、末尾不完整行均需覆盖。 | 下一行始终使用原始 LF+1；正文修剪使用独立 line_end。 | `src/tests/test_lpc_gateway.cc` | 4–8 小时 | 静态确认；未修复 |
| AUD-011 | P2 | C E H | fork 未同步上游修复 | Telnet 按网络分块净化文本，破坏跨块编码序列 | src/net/telnet.cc:66–93 | `auto sanitized = u8_sanitize({transdata, translen});` | 尚未收齐的后缀被 u8_sanitize 提前替换；整包正确的文本拆包后发生数据损坏。不能假设网络分块等于字符边界。 | on_telnet_data 对每个独立块直接转换/净化，没有保留会话级未完成后缀；临时转换缓冲所有权也需在失败分支保全。 | 同一有效文本按每一个字节边界拆分、逐字节到达、非法中间字节、EOF 残片及关闭回调场景；非 UTF-8 session encoding 另测。 | 在具体 interactive_t 会话保存流式解码状态，只缓存不完整后缀；关闭/编码切换时按既有替换合同收口。 | `src/comm.h`、`src/comm.cc`、`src/base/internal/strutils.cc`、`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc_gateway.cc` | 16–24 小时 | 静态确认；未修复 |
| AUD-012 | P2 | C E M P | fork 未同步上游修复 | get_char 破坏控制字符，并按字节切断 UTF-8 字符 | src/comm.cc:1453–1475；src/comm.cc:1504–1530 | `*p = 0;` | BS/DEL 被交付为空字符串，特定配置下 ESC 被替换；static char tmp[2] 且每次只取一个字节，还会把合法 UTF-8 拆成无效单字节字符串。 | SINGLE_CHAR 的提取器仍采用单字节/特殊清零规则；ESC 的行模式净化条件没有排除字符模式。 | 原生接收夹具逐项检查 ESC、BS、DEL、普通字符、UTF-8 字符和跨分片序列；行模式必须维持现有编辑语义。 | 字符模式完整交付合法 UTF-8 序列和控制字节；不完整片段等待，行模式编辑/净化不改。 | `src/tests/test_lpc_gateway.cc`、`docs/efun/interactive/get_char.md`、`docs/archive/Bugs` | 4–8 小时 | 静态确认；未修复 |
| AUD-013 | P2 | F I K | 上游继承问题 | 超长配置行使后续配置静默停止解析 | src/base/internal/rc.cc:32–33；src/base/internal/rc.cc:229–254 | `while (buffer.getline(&tmp[0], sizeof(tmp), '\n')) {` | while(getline(...)) 在循环体前结束，体内的截断警告无法覆盖失败的那一行，后续配置没有加载。涉及权限/端口配置时可能违背管理员意图。 | 把 getline 的缓冲耗尽失败与正常 EOF 当成同一终止条件。 | 119/120/121 字节边界、有/无末尾换行、超长注释及后续合法配置；验证报错行号和不会悄悄继续启动。 | 保留有界行缓冲，显式区分 EOF、超长行和 I/O 失败；任何读取失败均停止配置初始化，不再悄悄忽略余下安全指令，也不打印整行配置值。 | `src/tests/test_lpc.cc`、`etc/config.test`、`docs/driver/config.md` | 4–8 小时 | 静态确认；未修复 |
| AUD-014 | P2 | E I J | 上游继承问题 | 数据库编号比较漏写变量展开，错误也未终止 CMake | src/packages/db/CMakeLists.txt:6–19；src/packages/db/CMakeLists.txt:36–43 | `message(FATAL "PACKAGE_DB_DEFAULT_DB is not valid!")` | 重复后端编号没有在配置时被拒绝，可能拖到 duplicate case 编译失败；无效默认编号仍通过配置。Windows 参数覆盖还会掩盖原意。 | 冲突检查把 ${PACKAGE_DB_*} 与字面量 "{PACKAGE_DB_*}" 比较；错误分支使用不存在的 message(FATAL ...) 模式，而不是 FATAL_ERROR。 | CMake 独立配置矩阵：有效编号、非相邻重复、无效默认、空/非数字、关闭全部后端和仅 SQLite；断言失败发生于 configure。 | 只收集启用后端，严格解析整数并比较完整去重长度，默认编号不合法直接 FATAL_ERROR；Windows 参数统一。 | `src/CMakeLists.txt`、`.github/workflows/ci.yml`、`src/packages/db/db.cc` | 4–8 小时 | 静态确认；未修复 |
| AUD-015 | P2 | B E G | 上游继承问题 | 数据库结果与失败连接的资源清理不完整 | src/packages/db/db.cc:1198–1214；src/packages/db/db.cc:1227–1238；src/packages/db/db.cc:842–852；src/packages/db/db.cc:370–379 | `c->postgres.res = 0;` | Postgres_cleanup 仅把 PGresult 指针置零，后续 execute 覆盖此前结果；失败连接和 MySQL 初始化资源也存在未完成释放路径。可持续泄漏资源。 | 驱动槽位释放与后端资源释放没有统一失败/关闭后置条件；部分连接失败分支只释放外层结构，结果所有权未逐项收口。 | 隔离 db_io_tests 包装 libpq/MySQL C API，核对每个结果/连接恰好释放一次；SQLite 失败路径加入防双关；真实外部 DB 仅作为额外授权集成。 | 清理所有 PGresult；所有后端失败连接在错误已复制后变成可重复关闭的空状态，槽位重用前零初始化。 | `src/packages/db/db.h`、`src/tests/CMakeLists.txt`、`src/tests/test_db_io.cc` | 8–16 小时 | 静态确认；未修复 |
| AUD-016 | P2 | A D H P | fork 引入问题 | 嵌套循环覆盖 catch/time_expression 的禁止挂起上下文 | src/compiler/internal/grammar.y:899–904；src/compiler/internal/grammar.y:2143–2151；src/vm/internal/base/promise.cc:1628–1644 | `context = LOOP_CONTEXT;` | 语法检查可能接受本应编译期拒绝的 await，随后运行时仍以受保护控制栈拒绝挂起；诊断阶段和结构语义不一致。运行时守卫仍在，不报已发生栈破坏。 | 将禁止挂起条件存放在可被普通流控覆盖的 SPECIAL_CONTEXT 位；foreach 等直接重置 context。 | 编译失败矩阵覆盖 protected-context × for/foreach/while/do/switch × await；合法 async 循环和匿名/嵌套函数边界另测。 | 禁止挂起状态独立于 break/continue/switch 流控状态，进入/退出受保护语法时可靠保存恢复；继续保留运行时硬守卫。 | `src/compiler/internal/grammar.autogen.cc`、`src/compiler/internal/grammar.autogen.h`、`src/compiler/internal/grammar_rules.h`、`src/tests/test_lpc_compiler.cc`、`testsuite/single/tests/compiler/fail` | 8–16 小时 | 静态确认；未修复 |
| AUD-017 | P2 | A D | 上游继承问题 | catch 表达式被丢弃值优化器删除潜在异常操作 | src/compiler/internal/grammar.y:3053–3057；src/compiler/internal/trees.cc:407–433 | `$$ = insert_pop_value($2);` | insert_pop_value 把部分运算节点降成仅求值子节点，异常操作本身可能消失，catch 观察到的错误行为改变。 | 把“结果不用”误当作“运算没有可观察行为”；抛错也是副作用。 | 用非常量操作数构造已知会抛错的算术、索引、转换，比较 catch(expr)、catch({expr;}) 与保存结果后执行的行为；先排除常量折叠影响。 | 结果丢弃时保留可能抛错/调用/分配的运算；只删除已证明无异常副作用的节点，或直接生成完整求值再 POP 的 IR。 | `src/compiler/internal/icode.cc`、`src/compiler/internal/generate.cc`、`src/tests/test_lpc_compiler.cc` | 8–16 小时 | 静态确认；未修复 |
| AUD-018 | P2 | B H P | fork 引入问题 | Promise 持有的已销毁对象引用不参与 reclaim_objects | src/packages/core/reclaim.cc:24–75 | `switch (v->type) {` | reclaim_objects 只处理既有复合类型，Promise 内部持有的已销毁对象引用不会被此路径及时清理。不是把正常 pending Promise 全部当成泄漏。 | fork 引入 T_PROMISE 后，引用标记已经扩展，但实际回收遍历没有同等扩展；mark_promise 与 reclaim 不可相互替代。 | Promise resolved/rejected/pending、共享链/环、reaction 绑定参数和 next Promise 各自持有 destruct 对象；检查引用计数与对象最终释放而不改 Promise 结算状态。 | 在 AUD-007 的统一复合遍历中增加 Promise 内容，按 mark_promise 的实际拥有字段逐项覆盖，并保持 owner 准入。 | `src/vm/internal/base/promise.h`、`src/vm/internal/base/promise.cc`、`src/vm/internal/base/svalue.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 静态确认；未修复 |
| AUD-019 | P2 | A B D | 上游继承问题 | 函数表查询允许等于函数总数的越界索引 | src/vm/internal/base/interpret.cc:1831–1845；src/compiler/internal/compiler.cc:2660–2667 | `if (findex < 0 &#124;&#124; findex > prog->last_inherited + prog->num_functions_defined) {` | 范围检查用 > 而非 >=，随后访问 function_flags[findex]。未找到公开 LPC 直接提供任意该索引的可靠入口，因此按内部边界缺陷，不报远程可利用漏洞。 | 把元素数量当成合法最大下标。函数标志数组按函数总数分配，没有证明额外哨兵。 | 手工构建最小合法 program 夹具，测试 -1、0、最后一个、count、count+1；另外覆盖 inherited/alias 的有效路径。 | 将上界改为半开区间，保持无效索引返回 {nullptr, 0} 的 pair 合同；检查 alias/inherited 跳转后同样有有效边界。 | `src/vm/internal/base/program.h`、`src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc` | 2–4 小时 | 静态确认；未修复 |
| AUD-020 | P2 | I J P | fork 引入问题 | Windows 安装复制旧路径且没有传播复制失败 | src/CMakeLists.txt:694–698；src/CMakeLists.txt:728–739 | `install(CODE "execute_process(COMMAND \"${BASH_EXECUTABLE}\" -lc \"cp -f '${CMAKE_CURRENT_BINARY_DIR}/driver.exe' '${CMAKE_INSTALL_PREFIX}/bin/driver.exe'\")")` | driver/lpcc/lpcshell 已改到构建根 bin，Bash 安装仍从旧 src 目录复制；execute_process 未检查结果，可能报告安装成功却漏掉程序或复制旧文件。symbol/o2json/json2o 不能误算为本次路径迁移目标。 | 输出目录修改只覆盖 driver/lpcc/lpcshell，Windows 的 Bash cp 特例未同步路径，也没有 RESULT_VARIABLE/失败传播。 | Windows 原生单配置和多配置各执行隔离前缀安装，核对目标 exe 的存在、SHA-256 和 build 中目标文件一致，不运行/覆盖已安装实例。 | 让 CMake install(TARGETS) 管理可执行目标，依赖复制使用目标生成表达式；不拼接旧 src 路径。 | `.github/workflows/ci.yml`、`cmake/helper.cmake` | 4–8 小时 | 静态确认；未修复 |
| AUD-021 | P2 | I O P | fork 引入问题 | 供应链清单与实际 vendored 源码身份不一致 | third_party/manifest.yaml:68–76；src/thirdparty/libwebsockets/CMakeLists.txt:500–507；tools/sbom-generate.py:30–43 | `version: 4.2.1` | manifest/SBOM 把 libwebsockets 记为 4.2.1，而源码声明 4.5.8；patches 信息与本地改动需重新核对，GitHub purl 还缺少可定位 owner 身份。安全扫描和版本归因不可靠。未据此宣称某个 CVE 命中。 | 自报版本/patch 元数据未与实际 source revision 绑定，purl 用展示 name 拼装；生成后 validate 不能证明提交中的原 SBOM 未漂移。 | 离线比较 manifest、实际源码版本、已知 source revision、补丁差异与生成 SBOM；检查各组件 purl owner/repo；修改输入后的只读 check 必须失败。 | 以 manifest 为身份单一来源，记录可核对的 commit/版本/补丁；按 source URL 构建 purl，增加不先覆写产物的 check。 | `third_party/sbom.json`、`third_party/cyclonedx.schema.json`、`tools/sbom-generate.py`、`.github/workflows/ci.yml`、`src/thirdparty/libevent` | 8–16 小时 | 静态确认；未修复 |
| AUD-022 | P2 | K O P | fork 引入问题 | owner 状态把历史验收写死为当前 ready | src/vm/internal/owner.cc:1602–1631；docs/multicore-production-gate.md:6–15；docs/multicore-production-gate.md:124–127 | `add_mapping_pair(map, "production_gate_ready", 1);` | 调用方可能把历史实现/测试记录误认成当前二进制的有效运行验收。它不是本轮测试通过的证据。 | 状态字段脱离当前 commit、构建配置及原始证据有效性；权威合同要求失效时回到 incomplete/blocked，但代码没有该判断。 | 无证据、历史证据、commit 不符、原始日志摘要损坏和当前有效 envelope；验证 API 状态不能仅凭常量报告 ready。 | 立即停止无条件 current-ready 声明；历史说明与当前可证明状态分离，只有绑定当前构建的真实验证结果才能置 ready。 | `tools/docs/check-evidence.py`、`docs/evidence/manifest.schema.json`、`src/tests/test_lpc_owner.cc` | 8–16 小时 | 静态确认；未修复 |
| AUD-023 | P3 | L M O P | fork 引入问题 | 权威排版说明仍描述已被 CI 扩大的 pilot 范围 | docs/coding-standard.md:29–29；.github/workflows/ci.yml:87–104 | `&#124; [CI][up-ci] &#124; 运行 LPC syntax 测试和 testsuite/format.sh --check &#124; 本地 U10f 已接入固定 pilot；完整 native/LPC 存量门禁仍由 U10g 按模块收口，不能把 pilot 写成全库通过 &#124;` | 文档写 pilot 子集，而 CI 已运行 --all-native，贡献者无法按权威说明复现检查范围。它不是新增运行时漏洞。 | 格式门禁扩大后，权威说明和过渡状态未同步。 | 逐字对照 check-format.py 的 --all-native 发现/排除规则、CI 调用与文档，确认 generated/thirdparty/raw-byte fixture 的边界一致。 | 仅更新实际归属文档的范围和命令，去除失效过渡语句；不再次全仓重排代码。 | `tools/style/check-format.py`、`src/.clang-tidy`、`docs/project-modernization-plan-2026-10.md` | 2–4 小时 | 静态确认；未修复 |
| AUD-024 | P3 | I M O | 上游继承问题 | 构建手册混淆数据库编号，并给出失配的测试/并行命令 | docs/build.md:285–288；docs/build.md:292–301；docs/build.md:312–318；src/CMakeLists.txt:19–23 | `- =2 enables SQLite3 API version 2 (recommended, includes newer features)` | 把后端 ID 误当 SQLite API 版本，会选择与其他后端/默认后端冲突的编号；测试块从 build 直接 cd testsuite 指向错误目录；不限并行的示例与项目 WSL 构建约束冲突。没有声称本轮实际发生 OOM。 | 上游遗留构建说明没有按真实 CMake 参数语义、当前目录和 fork 的资源约束同步校准。 | 静态对照 src/CMakeLists.txt、src/packages/db/CMakeLists.txt、docs/efun/db/db_connect.md 和 AGENTS §4.2；按命令序列推导 cwd；不实际 configure、安装或启动 driver。 | 把 SQLite 参数解释改为唯一后端编号；给出 SQLite-only 与多后端的完整有效编号/默认值示例；修正测试 cwd，并把该页不限并行示例按项目规则改为明确上限。 | `docs/build.md`、`docs/efun/db/db_connect.md`、`src/CMakeLists.txt`、`src/packages/db/CMakeLists.txt`、`AGENTS.md` | 2–4 小时 | 静态确认；未修复 |
| AUD-025 | P1 | B D H | fork 引入问题 | 冷启动 ScratchArenaBinding 留下空 current_ | src/compiler/internal/compile_arena.cc:174–208、220–240、285–324 | current_ = chunk; // chunk 为空而 base_ 已存在 | 合法内部调用顺序导致崩溃及 live/cycle 计量失真；未证实普通 LPC 直接控制该顺序。 | 冷 binding 记录空 chunk。首次分配建立 base_，回退仍把 current_ 置空，且漏减 base_ 使用量。下一次 alloc 因 base_ 非空而解引用空 current_。 | 退出冷 binding 后计量归零且可再次分配；内层回退不破坏外层数据；次序/代际错误仍拒绝。 | 空起始 mark 回退时归还新 base_ 的 used，并把 current_ 恢复到可复用 base_；保留 retained/live 链。 保持 generation 和严格 LIFO 拒绝逻辑；bytes_released、cycle_bytes、bytes_live 同步回退。 补冷/嵌套 binding、oversize、异常退出和下一次 begin/reset。 | `src/compiler/internal/compile_arena.cc`、`src/tests/test_compile_arena.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-026 | P2 | B D P | fork 引入问题 | ScratchArena 尺寸运算可回绕 | src/compiler/internal/compile_arena.cc:32–45、174–216 | need=align_up(size); if(used+need>capacity) | 内部分配合同缺口，不能据此宣称远程可利用。 | align_up、header+size、used+need、alloc_string(len+1) 都先作未检查的 size_t 加法，极端参数可变成小分配或重叠区间。 | 0、对齐和块边界正常；各溢出邻界拒绝，计量不变；后续小分配可用。 | 分配/游标变化前检查 len+1、对齐加数、header+size；非法尺寸抛 std::bad_alloc，不返回可用指针。 用 need > size-used 判断剩余空间；保留零尺寸返回空的合同。 只验证极值被拒绝和状态不变，不实际申请巨量内存；成功分配继续验证无重叠。 | `src/compiler/internal/compile_arena.cc`、`src/tests/test_compile_arena.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-027 | P1 | D G | 不确定 | 编译永久改变进程 locale | src/compiler/internal/compiler.cc:2215–2220 | 保存 setlocale(...,"C") 的返回值 | 后续数值转换、格式和并发线程的 locale 行为改变。 | setlocale(LC_ALL,"C") 返回修改后的值而不是原值，DEFER 恢复的仍是 C；切换本身也影响其他线程。 | 非 C locale 下 LPC 小数语义稳定，前后 locale 相同，观察线程不受影响；缺平台记 external-required。 | 移除进程全局 LC_ALL 切换，改为编译器内部线程局部 C-locale 作用域。 POSIX 用 newlocale/uselocale；Windows 用 CRT 线程局部模式；恢复先前线程 locale/模式，失败报告。 覆盖成功、错误和嵌套退出；并行观察线程不得看到 locale 改变。 | `src/compiler/internal/compiler.cc`、`src/tests/test_lpc_compiler.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-028 | P2 | B D | 不确定 | 内建预定义字符串构造无容量边界 | src/compiler/internal/lex.cc:3219–3225、3263–3276 | 固定数组 strcat；版本先 sprintf 到 1024 字节数组 | 元数据长度合同缺失；未证明玩家输入可达。 | 加引号值由 strcpy/strcat 拼入 MAXLINE，版本先 sprintf 到 1024 字节数组；这些内建构建元数据构造路径实际可达。 | 短值逐字保持；长构建元数据安全构造或按既有宏长度上界明确拒绝；版本不经过固定 sprintf 缓冲。 | 版本和加引号值改为单一动态字符串构造，保持内容不变，直接把 PROJECT_VERSION 传入加引号构造。 只改这条实际使用的构造路径；lpc_predefs 无生产写入者，不恢复 -D，不把旧列表的 sscanf 顺手纳入本单元。 让测试调用同一内部生产构造函数，验证短值、长值及邻界；不新增 LPC 接口。 | `src/compiler/internal/lex.cc`、`src/compiler/internal/lex.h`、`src/tests/test_lpc_compiler.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-029 | P0 | B D | 不确定 | 宏实参采集先越界再报错 | src/compiler/internal/lex.cc:4307–4406、4410–4413 | uisspace(*(out-1)); argv[++argc]=out; | 编译期边界前读和栈越界写，后续诊断不能撤销写入。 | extract_args 的空首参数读 out-1；逗号分支 argv[++argc] 可先写 argv[NARGS]，下一轮才到数量上限检查。调用者传栈数组。 | 合法输入不回归，超量先拒绝；ASan/UBSan 无越界，下一次正常编译成功。 | 收尾先确认 out>argv[argc]；空实参直接写终止符，不读前字节。 递增 argc 和写下一槽前检查槽边界，保留宏形参数量错误语义。 覆盖空首/中/末参数、嵌套括号/引号、数量边界及错误后再次编译。 | `src/compiler/internal/lex.cc`、`src/tests/test_lpc_compiler.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-030 | P1 | B D F P | fork 引入问题 | 字符候选绕过预处理指令长度上限 | src/compiler/internal/lex.cc:3783–3893、3987–4028 | char_candidate.push_back 不受 append 上限保护 | 编译期内存和 CPU 放大；受限输出数组不等于受限临时存储。 | append 限制 DEFMAX，十六/八进制候选却先无限追加 char_candidate。续行可在回放到受限输出前占用任意候选内存。 | 候选峰值受限；超长有确定诊断；边界内源码、位置映射及下一次编译一致。 | 每次候选追加前按已输出+待决候选总量检查 DEFMAX，保持单一追加规则。 超限锁存原超长诊断并终止该编译，不截断后当合法源；保留正确行号与下次编译状态。 覆盖十六/八进制、缺结束引号、LF/CRLF 续行和字符/注释歧义。 | `src/compiler/internal/lex.cc`、`src/tests/test_lpc_compiler.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-031 | P2 | D | 不确定 | case 常量移位漏掉低六位合同 | src/compiler/internal/grammar.y:1235–1242 | $$=$1<<$3; $$=$1>>$3; | 编译结果偏离 LPC 模 64 语义。 | constant 左右移直接使用 $3，未按运行时合同取低六位；负数或不小于 64 的计数触发 C++ UB/构建差异。 | -1/0/63/64/65、极值计数和符号边界结果一致，UBSan 无 shift 报告。 | 复用安全 LPC 移位实现，计数取低六位；左移用无符号位运算，不拒绝合同允许的计数。 同步 Bison fallback 和 generate_ebnf 输出，不手改生成物。 比对运行时、普通折叠、case 常量三条路径。 | `src/compiler/internal/grammar.y`、`src/tests/test_lpc_compiler.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-032 | P0 | B E | 不确定 | replace_string 跳表复制漏记目标长度 | src/packages/core/efuns_main.cc:2261–2318 | skip 只推进 dst2，后续用旧 dlen | 可写过按最大字符串长度分配的缓冲。 | 增长分支的 skip_table 复制推进 src/dst2，却不增加 dlen，也不检查目标剩余容量。后续检查使用偏小 dlen。 | 参考结果逐字节一致，越界在复制前拒绝；ASan/清理检查全部路径。 | skip、替换、单字节及尾段共用实际已写长度，所有路径复制前检查剩余容量。 保持超限失败返回、弹栈和释放合同，不以写后检查代替。 覆盖 first/last 范围、连续跳过、缩短/增长及容量邻界。 | `src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-033 | P1 | B C E H | 不确定 | TLS 重试把负返回值当发送偏移 | src/packages/sockets/socket_efuns.cc:1145–1226 | 重试保留 off<0，随后 w_off=off | 后续可从缓冲起点之前读取，队列和网络统计错误；未证实代码执行。 | SSL_write 的 WANT 等负返回被判可继续，off 却未归零，写入 w_off 与 len-off。 | 0<=w_off<=总长度、剩余长度正确；无前缀泄漏/重发/漏发，统计不负增。 | 可重试错误归一为消费 0 字节；只有正返回值推进偏移和统计。 按 WANT_READ/WANT_WRITE 等待正确事件，遵守同一数据重试要求。 注入 WANT、EINTR、部分成功、致命错误并检查生产队列。 | `src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-034 | P1 | B E | 不确定 | sscanf 的 %s%x 搜索越过终止符 | src/vm/internal/base/interpret.cc:5400–5414 | 非前缀尾部 0 仍 tmp+=2 后解引用 | 越界读取和不可信匹配结果。 | 尾部单个 0 无 x 前缀时仍 tmp+=2，循环随后读取终止符之后的位置。 | 空输入、尾部 0、完整/不完整前缀、合法十六进制和多转换；ASan 无越界。 | 后继字符存在才检查前缀；无前缀按仍在字符串内的步幅推进。 前缀完整且第三字节存在才检查该字节。 保留匹配计数、未匹配参数和其他转换语义。 | `src/vm/internal/base/interpret.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-035 | P1 | B C E H | 不确定 | TLS 延迟刷新前已释放并清空 SSL | src/packages/sockets/socket_efuns.cc:2031–2074 | SSL_free → ssl=null → STATE_FLUSHING | 向 SSL_write 传入空 SSL，可能崩溃；不是 UAF，也不是 comm.cc interactive 清理。 | socket_close 在判断 S_BLOCKED 延迟关闭前 SSL_free 并置空；刷新仍按 TLS 模式调用 SSL_write。 | 普通待发按合同完成后恰好释放一次；强制/致命关闭不续写、不传空 SSL。 | 先决定 flush/final；普通关闭需刷新时保留 SSL/CTX 到最终关闭。 最终路径一次释放 TLS/事件/队列，回调只通知一次并保留身份重查。 与 AUD-060 共核完整关闭状态机。 | `src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-036 | P1 | B C E H | 不确定 | TLS accept 早失败遗漏事件资源 | src/packages/sockets/socket_efuns.cc:748–783 | close(accept_fd); clear_socket(i,1); | 事件/上下文反复泄漏及旧事件关联；fd 已关闭，不能报 fd 泄漏。 | 事件和 ev_data 已分配，SSL_CTX 缺失/SSL_new 失败却只 close fd 再 clear_socket，资源指针被清掉。 | 重复失败后事件/上下文/fd 回基线；槽复用不接受旧回调；正常 accept 不回归。 | 早失败汇入完整新槽撤销，释放事件/上下文再清槽，fd 只关闭一次。 不交付半初始化槽，不绕过身份/回调约束。 注入两处失败，推进夹具 event_base 后检查资源。 | `src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-037 | P2 | I J | fork 引入问题 | runner 拒绝合法百分号文件名 | tools/testsuite/run-targeted.py:746–759 | /[A-Za-z0-9_.-]+ 白名单 | 用例不能通过定向入口执行，不是编译失败。 | validate_case 字符白名单排除 %，拒绝已有 warn_%s 和 warn_%s_a，driver 尚未启动。 | 两项真实执行且身份一致；越根启动前拒绝；少跑/异步门禁不放宽。 | 按组件和根归属校验；拒绝 NUL/控制字符、反斜杠、空组件与 ./..，允许语料合法字符。 确认实际 .c/.lpc 文件位于隔离 mudlib，继续 argv 传参、不经 shell。 补现有 %/Unicode 文件名与穿越/符号链接越根负例，再重跑两项。 | `tools/testsuite/run-targeted.py`、`tools/testsuite/test-run-targeted.py` | 4–8 小时 | 执行/测试缺口确认；未修复 |
| AUD-038 | P2 | E J | fork 引入问题 | SQLite 异步回归未接完成屏障 | testsuite/single/tests/efuns/async_db_exec_bound_args.lpc:17–44；testsuite/single/tests/efuns/db.c:93–108；testsuite/etc/test-scopes.tsv:10、36 | do_tests 返回时没有登记未完成 token | 回归覆盖和完成时序失真；单例超时后端根因未另证，不能猜成 DB 锁。 | 两项虽为 runtime，却未登记异步 token。do_tests 返回即完成；全量报 assertion after case completion，单例先结束后约 183 秒超时。 | 完成标记晚于最终回调，两项不超时；禁止回调/故意失败必须失败；仍挂起则另追 shutdown。 | 保持现有 runtime 角色及已正确声明的 async/db/sqlite 能力依赖，不改分类/门禁。 在 do_tests 返回前调用 /command/tests->begin_async() 并保存 token；最后一个回调完成断言及 db_close 后调用 complete_async(token)。 用真实 DB 回调推进，替代固定延时；保留 bound args 与拒绝 string callback 的既有回归。 | `testsuite/single/tests/efuns/async_db_exec_bound_args.lpc`、`testsuite/single/tests/efuns/db.c` | 8–16 小时 | 执行/测试缺口确认；未修复 |
| AUD-039 | P1 | E F H N P | fork 引入问题 | 无执行权限对象可占满外部句柄池 | src/packages/external/external.cc:152–166、1340–1357、1363–1388 | alloc_handle_id 先于权限检查 | 其他对象无法创建；不等于绕过权限启动进程。 | external_create 分配全局持久配额，权限仅在 external_run 检查；被拒绝执行的对象仍可长期占满槽。 | 拒绝对象重复 create 不影响授权对象；回调重入/销毁安全；创建后撤权仍不能 run。 | 复用 external_run/start 的 check_valid_socket("external", -1, current_object, "N/A", -1)，在 external_create 占用槽位以及建立参数 vector 前检查；不新增 efun、clone 入口或 master 操作名。 先保存 st_num_arg，权限回调返回后恢复；拒绝时抛既有风格的工厂调用错误，不预留槽。回调可销毁调用对象，授权后必须重新验证 owner 的存活与 epoch，不能沿用回调前的裸状态。 保留 kMaxHandles 上界与 run 时再次鉴权。测试拒绝对象重复申请、允许对象达到上限、释放后再申请，以及权限回调抛错/重入/销毁；此项改变原先明确允许的非特权分配政策，须随方案批准。 | `src/packages/external/external.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-040 | P2 | E P | fork 引入问题 | external_write 截断 stdin 却返回成功 | src/packages/external/external.cc:124–135、679–687、1524–1533 | append_capped(...); return 1; | 子进程收到残缺输入，LPC 却得到成功。 | queue_stdin 复用 append_capped 静默丢超容量字节，随后仍返回 1。 | 超限返回 0 且逻辑队列不变；成功字节完全一致，无丢弃/重复。 | 以当前 h->in_buf.size() 和 __MAX_STRING_LENGTH__ 计算剩余容量；flush_stdin 已通过 erase 移除已消费前缀，不另设冗余游标。 在追加/flush 改变状态前判断整次输入能否容纳；超限直接返回 0 且队列不变，能容纳才完整追加。保留 boolean 合同 1=本次全部接收、0=未接收，不改成字节数，也不改变 stdout/stderr 的截断策略。 保留 Created/Running、EOF/close、空输入行为；用有界管道模拟部分写与 EAGAIN，验证满额拒绝、随后重试、逐字节完整和清理。 | `src/packages/external/external.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-041 | P2 | I J | fork 引入问题 | 非 Debug 零断言用例未预分类 | testsuite/single/tests/compiler/constant_expr.c:1–39；testsuite/etc/test-scopes.tsv；.github/workflows/ci.yml | 条件体 + 默认 runtime | CI 分类缺口，不是缺语言模块；不能改门禁伪通过。 | constant_expr 的断言全受 __DEBUG__ 保护，优化配置没有对应的预执行能力排除，严格门禁因此拒绝零断言。 | Debug 有实际断言；优化配置预排除；其他运行用例故意零断言仍失败。 | 既有能力清单增加准确 debug 条件，为该测试声明依赖，不加假断言。 非 Debug 预列配置不支持并保留原因；Debug 必须真实执行。 同步能力生产者、分类校验、自测和 discovered/executed 对账。 | `testsuite/etc/test-scopes.tsv`、`testsuite/command/tests.c`、`tools/testsuite/run-targeted.py`、`tools/testsuite/test-run-targeted.py` | 4–8 小时 | 执行/测试缺口确认；未修复 |
| AUD-042 | P2 | I J K O | fork 引入问题 | CI 定向失败详细日志未持久归档 | .github/workflows/ci.yml；tools/testsuite/run-targeted.py:1000–1039；src/tests/CMakeLists.txt | 只剩临时路径，原日志未上传 | 根因核验被迫另行同型复跑；日志缺失不代表产品有错。 | 嵌套 runner 只打印 /tmp 证据路径，CI 未归档该目录完整失败日志；file_io 的具体断言无法从现有 CI 恢复。 | 已知失败可从 artifact 恢复断言与身份，job 仍失败；成功/中断可追踪且敏感文件不归档。 | 每次 runner/嵌套 CTest 显式使用工作区下唯一 evidence-dir。 always 上传受控 summary、有界完整日志和身份元数据；不上传凭据、环境或任意 /tmp。 保留失败码，用受控失败确认 artifact 可定位具体断言。 | `.github/workflows/ci.yml`、`src/tests/CMakeLists.txt`、`tools/testsuite/run-targeted.py`、`tools/testsuite/test-run-targeted.py` | 8–16 小时 | 执行/测试缺口确认；未修复 |
| AUD-043 | P2 | B D | 不确定 | 空输入尾裁剪形成起点之前的指针 | src/compiler/internal/lex.cc:handle_pragma；src/packages/core/add_action.cc:325–333 | start + strlen(start) - 1 | C++ 指针形成/比较超界 UB；未证明当前构建会崩溃。 | handle_pragma 的 strlen-1 与 user_parser 初始/末轮指针可越过起点。后者 p>=buff 会阻止解引用，不能报成已证实前读。 | 正常结果/诊断不变；边界夹具确认无非法指针；不伪造 sanitizer 可观察性。 | 两处用非负长度索引，仅 length>0 才访问 length-1。 保留 pragma 诊断和空命令返回 0，不扩大空白定义。 覆盖空串、全空格、单字符、普通尾空格和参数修改边界。 | `src/compiler/internal/lex.cc`、`src/packages/core/add_action.cc`、`src/tests/test_lpc_compiler.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-044 | P2 | I J | fork 引入问题 | FORTIFY 绕过 fprintf 故障注入 | src/tests/test_file_io.cc:308、602–642；src/symbol.cc；src/packages/mudlib_stats/mudlib_stats.cc:532；src/tests/CMakeLists.txt | 仅 --wrap=fprintf / __wrap_fprintf | 两个写失败回归失效并导致 CI 失败；不是已证实的产品输出丢失。 | GCC 优化/FORTIFY 把两处 fprintf 降成 __fprintf_chk，测试仅 wrap fprintf，故障钩子未执行。 | 同型优化配置 96/96 且钩子命中；Debug 不回归，正常 fortified 输出不被屏蔽。 | 保留 FORTIFY，在同一故障状态机接入 __fprintf_chk 包装；正常转发保留 flag/varargs。 CMake 仅在平台/编译器支持该符号时启用对应 wrap。 继续要求 hits>=1，验证正常写和两个 printf 变体的故障上报。 | `src/tests/test_file_io.cc`、`src/tests/CMakeLists.txt` | 4–8 小时 | 优化构建 94/96；2 失败；未修复 |
| AUD-045 | P2 | A E | 不确定 | member_array 下标窄化及逆向失败哨兵错误 | src/packages/core/efuns_main.cc:1092–1108、1243–1253 | i=(int)start；i=-1 后仍 size-i-1 | 返回错误位置，甚至把未找到报告成非负下标。 | 起点先从 LPC_INT 窄化为 int 再校验；逆向搜索把未找到的 -1 再映射成 size。 | 正反向未找到均 -1；实际命中下标正确；大起点不回绕成有效下标；负起点仍拒绝。 | 在 LPC_INT 中按容器规则验证起点，再作有证明的窄化；保留字符串越界报错与数组未找到语义。 只有命中时才进行反向下标映射，-1 始终保留。 覆盖普通、前缀及谓词组合和极值起点。 | `src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-046 | P1 | E F G | 不确定 | 原生骰子与编辑距离循环不响应预算 | src/packages/contrib/contrib.cc:2835–2853、2874–2910；src/vm/internal/base/interpret.cc:2478–2484 | 原生循环无预算轮询 | 事件循环被长时间占用；不把 WSL 结果推广为各平台计时器验收。 | roll_MdN 的 O(rolls) 与 string_difference 的 O(mn) 循环不轮询 outoftime，解释器只在回到 opcode 分派后处理超时。 | 正常结果一致；注入耗尽可预测中止且释放内存；真实隔离 driver 的有界输入验证响应时间，无负载外推。 | 把解释器现有超时处理提为同一内部检查函数，保留 max_eval_error/catch 不能吞掉超时的合同。 两循环以固定小批量轮询，预处理/初始化长循环也检查；不修改结果算法。 编辑距离工作表改为异常安全所有权，使预算 error 释放临时内存；接入 AUD-006 的平台计时合同。 | `src/packages/contrib/contrib.cc`、`src/vm/internal/base/interpret.cc`、`src/vm/internal/base/interpret.h`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-047 | P1 | B E H | 不确定 | get_dir 两遍扫描遇提前 EOF 解引用空目录项 | src/packages/core/file.cc:311–328、345–384 | for(...; i<count; de=readdir(...)) 直接用 de | 空指针解引用；第一遍也未区分 EOF 与错误。 | 第一遍计数后 rewinddir；第二遍只判断 i<count，不判断 readdir 是否为空。目录变化或读取错误可使其提前结束。 | 确定性注入提前 EOF/错误及匹配变化；无空槽/空解引用/泄漏，正常排序与限制不变。 | 每次 readdir 前清 errno，返回空后先区分正常 EOF 与错误，任何 d_name 访问前检查目录项。 正常提前 EOF 按实际填充 i 收缩数组并仅排序有效项；错误释放数组/关闭目录并返回既有失败值。 保留最大数组限制、通配符与排序；不用第一遍计数证明第二遍一定有项目。 | `src/packages/core/file.cc`、`src/tests/test_file_io.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-048 | P2 | B E K | 不确定 | get_dir 忽略元数据读取失败 | src/packages/core/file.cc:354–375 | large_file_stat(...); // assume it works | 输出看似有效但属于其他对象或虚构的大小/时间。 | 逐项 large_file_stat 返回值被忽略，失败后 encode_stat 使用之前目录/文件的 st；过长路径还伪造全零元数据。 | 第二项 stat 失败不会得到第一项元数据；不泄漏、不返回伪零；正常/目录/长路径边界覆盖。 | metadata 模式检查 stat 成功，不复用旧 st；路径不容纳也明确失败。 失败释放中间结果、关闭目录，按 get_dir 已有失败约定返回 0，不造元数据哨兵。 保持成功路径的 file_size/时间定义，文档说明元数据失败不返回部分伪结果。 | `src/packages/core/file.cc`、`src/tests/test_file_io.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-049 | P2 | E I | 不确定 | link 的 EXDEV 符号链接目标与返回值错误 | src/packages/core/file.cc:895–905、938–946 | symlink(from,to)；失败仍 return 0 | 返回成功但链接损坏，或根本没有创建。不是复制回退。 | 回退 symlink(from,to) 直接使用相对当前目录的 from，实际会相对目标目录解释；symlink 失败仍落到 return 0。 | 回退目标实际可读且与源一致；失败不成功；权限拒绝无副作用，既有目标不被覆盖。 | 仅 EXDEV 分支按目标父目录计算词法相对源目标，不改变权限回调原始输入和次数，不 canonicalize 跟随源链接。 symlink 失败走既有错误/失败合同，不能落到成功返回。 保留正常硬链接与目标存在处理；使用私有夹具注入 EXDEV。 F_LINK 的 symlink 失败分支返回 1，不能落到函数末尾的成功值 0；F_RENAME 分支不改。 | `src/packages/core/file.cc`、`src/tests/test_file_io.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-050 | P2 | E | 不确定 | sprintf 数组 %O 转换只作用于首元素 | src/packages/core/sprintf.cc:1075–1103、数组迭代尾部 | 首元素永久改写 finfo 类型 | 合法格式输出错误或抛错。 | 首元素把 finfo 从 INFO_T_LPC 改成 INFO_T_STRING，后续迭代不恢复原转换；后面的数值可报字符串类型错，字符串失去 %O 表示。 | 空/单/多元素、混合类型和嵌套值结果正确；ASan/引用检查无泄漏，后续调用不受影响。 | 保留原转换类型，每个元素重新执行 %O 转换，其他格式标志不丢。 每轮沿用已有 clean 释放，下一轮恢复原转换类型；保持列/表其他 flags 和缓冲寿命，不引入新的临时池。 比对逐元素 sprintf("%O") 拼接，并验证宽度、精度、列/表及错误后的状态。 | `src/packages/core/sprintf.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-051 | P1 | B E | 不确定 | parse_command 默认介词表可为空 | src/packages/ops/parse.cc:1154–1167 | parr=gPrepos_list; for(...parr->size) | 合法的缺省/未配置场景可崩溃。 | 未传有效显式表时使用可空 gPrepos_list，随即访问 parr->size。 | 缺省缺表返回未匹配不崩溃；有效单词/多词表正常。 | 介词表为空时按未匹配返回，设置 fail 和返回槽；不制造新默认词表。 空数组、含非字符串项和显式数组保持现有选择及交换规则。 验证 master 缺 apply/错误类型和后续可用调用。 | `src/packages/ops/parse.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-052 | P2 | B E H | 不确定 | living_parse 压缩候选却复用原下标缓存 | src/packages/ops/parse.cc:1050–1083；load_lpc_info/match_object | live[tix++]=obarr[obix]，缓存却按 obix | 错误匹配、漏匹配或把另一个对象的 ID 用到 living 对象。 | live 按 tix 压缩排列，gId_list/gPluid_list/gAdjid_list 仍按原 obarr 下标缓存；item_parse 用压缩下标查这些缓存。 | 冷/热缓存一致，混合数组不串对象；destruct 和非对象仍安全。 | living 临时数组用 allocate_array(obarr->size) 初始化为零，保留原数组长度。 仅把 living 对象复制到原 obix 槽位，不再压紧到 tix，也不缩短数组；复用 item_parse 跳过非对象的既有分支。 gId_list/gPluid_list/gAdjid_list 仍以原 obix 查找；对象引用计数按既有 assign/free 合同配对，不引入第二套缓存。 | `src/packages/ops/parse.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-053 | P2 | B E | 不确定 | 多词 ID 游标在调用者处多推进一词 | src/packages/ops/parse.cc:1119–1131、1294–1347 | 多词返回词后；调用者又 +1 | 跳过后一词，导致错误匹配/失败。 | find_string 单词命中留下当前词，多词命中却留下词后位置；single_parse/item_parse 都再加一。 | 只消费实际匹配词数；多词后下一词保留，单词行为不变。 | find_string 多词匹配成功后仅把输出参数 *cix_in 退回最后消费词；返回值 fpos 仍是匹配起点。 split 为空或零词时走不匹配分支；调用者继续按既有约定递增词游标。 核对 item_parse、adjectiv_parse、prepos_parse 对起点返回值与输出词游标的不同用法。 | `src/packages/ops/parse.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-054 | P2 | E | 不确定 | living_parse 回退成功仍保留失败标志 | src/packages/ops/parse.cc:1075–1105 | 回退 return 成功对象，但 fail 仍为 1 | 调用者丢弃正确回退结果。 | item_parse 失败置 fail=1；find_player/find_living 回退找到对象后返回值和游标成功，但未清 fail。 | 回退成功能继续匹配，失败仍失败；引用和游标无泄漏/多推进。 | 仅在回退实际找到并持有对象后设置 fail=0，再提交返回值及游标。 保留两种查找顺序和对象引用，不在未找到时清失败。 覆盖候选为空/非空、成功/失败以及相邻格式项。 | `src/packages/ops/parse.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-055 | P2 | E K | 不确定 | SQLite 执行失败没有更新错误文本 | src/packages/db/db.cc:1028–1053 | get_table 失败只 finalize/return | 调用方收到错误/陈旧诊断，妨碍处理和定位。 | prepare 失败会写 errormsg，sqlite3_get_table 执行失败却只清结果并返回 0，错误缓存为空或仍是上次错误。 | 实际约束失败有相应非空诊断，不带上次错误；句柄随后可正常执行，资源回基线。 | 执行失败时在清理前复制 sqlite3_errmsg 到同一有界错误缓存并终止。 清理 results 与状态一次，成功/下一次失败不能复用无关旧错误。 不改变同步/异步结果协议或重复执行策略。 | `src/packages/db/db.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-056 | P2 | B E H | 不确定 | query_multiple_short 缺回退对象时漏掉结果缓冲 | src/packages/dwlib/dwlib.cc:184–229 | 缺回退分支丢失 res | 重复调用泄漏原生字符串；不推断其他回调路径已经安全。 | res 已分配；no_dollars 下既无 command_giver 又无 /global/player 的分支 pop 参数并 push_undefined，却未交出或释放 res。 | 缺回退重复调用引用/分配回基线；成功输出不变，无双重释放。 | 让 res 在创建后由同一 RAII/作用域守卫持有，只有 push_malloced_string 真正移交时释放守卫。 缺回退对象与异常路径都释放；保留返回 undefined 和转换回调顺序。 覆盖有/无 command_giver、回退存在/缺失及回调错误。 | `src/packages/dwlib/dwlib.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-057 | P2 | B E H N | 不确定 | parse_refresh 清标志导致旧缓存不释放 | src/packages/parser/parser.cc:563–617、918–942 | 清 SETUP 后，释放函数又要求 SETUP | ID/复数/形容词数组和字符串引用泄漏。 | refresh 清 PI_SETUP、设 PI_REFRESH；remove_ids 却仅在 PI_SETUP 时释放。再次 refresh 还会抹掉 PI_REFRESH，旧数组随后被覆盖。 | 单次/多次刷新、错误及销毁后引用回基线；活动解析不悬垂，新 ID 下一次生效。 | remove_ids 按 PI_SETUP&#124;PI_REFRESH 识别仍拥有的缓存，释放后归零；不读从未初始化的字段。 重复 refresh 保留 PI_REFRESH，只有实际重建/清理完成才结束待刷新状态。 维持正在解析时延迟释放的约束，核对 remove_parser_info 和 interrogate_object。 | `src/packages/parser/parser.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-058 | P2 | A E | 不确定 | roll_MdN 把 64 位参数窄化成 int | src/packages/contrib/contrib.cc:2835–2853 | int rolls,sides,bonus = LPC_INT 实参 | 合法 64 位值变成负数/其他数，结果偏离合同。 | rolls/sides/bonus 使用 int，LPC_INT 实参在验证和运算前已截断；roll 本身已是 LPC_INT，不能误报累加器是 int。 | 小值不回归；大 bonus、受控 RNG 的大 sides 保持值；极大 rolls 被预算正常终止。 | 三种参数保持 LPC_INT，边界/正数判断在原宽度进行，检查 random_number 的参数宽度。 累加沿用项目 LPC 64 位整数溢出合同，避免引入 C++ UB。 与 AUD-046 共用有界循环，不能把窄化修复变成不受限长循环。 | `src/packages/contrib/contrib.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-059 | P2 | C E | 不确定 | socket_status 混用状态枚举和 flags 位图 | src/packages/sockets/socket_efuns.cc:2394–2402 | flags & STATE_FLUSHING | 状态 API 返回错误 owner；不据此推断权限提升。 | 用 flags & STATE_FLUSHING 判断刷新状态，实际应读取 state；无关位值可错误隐藏/暴露 owner 槽。 | 刷新时 owner 槽为 0，合法活动 socket 为相应对象，独立 flags 不改变结果。 | 改为 state != STATE_FLUSHING；保留 owner 存活检查和引用计数。 用枚举状态和独立 flags 组合验证，不靠当前数字巧合。 不改变返回数组形状和其他字段。 | `src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc` | 2–4 小时 | 源码确认；定向回归待实施 |
| AUD-060 | P1 | B C H | 不确定 | SC_FINAL_CLOSE 仍可进入延迟刷新 | src/packages/sockets/socket_efuns.cc:2038–2074、2449–2467 | FINAL 之后仍按 S_BLOCKED 进入 FLUSHING | 描述符、事件、队列可能未最终回收，TLS 生命周期与 AUD-035 交叉。 | FINAL 只跳过已有 FLUSHING 的前置拒绝；后续 S_BLOCKED 分支不检查 FINAL，强制清理可再次返回 FLUSHING。 | FINAL 返回后推进夹具事件即可资源归零，不续写/重复回调；普通关闭仍按合同排空。 | FINAL 必须绕过排空，回收全部资源；普通关闭才可进入 FLUSHING。 核对所有 FINAL 调用者、回调重入与槽 generation；不能只在调用者清 S_BLOCKED。 与 AUD-035 合并验证正常排空和强制丢弃两种不同合同。 | `src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-061 | P1 | E F N P | fork 引入问题 | parser 递归预算跨请求累计 | src/packages/parser/parser.cc:3171–3192、3300–3361 | static int parse_recurse_steps=0 | 正常解析逐渐失效，预算不是本次请求的资源边界。 | parse_recurse_steps 是函数 static，初始化一次且无重置。正常请求累计到 1000000 后，后续解析立即返回。 | 总历史工作量超过阈值不影响下一请求；单请求仍受原阈值限制，临时词/引用清理正确。 | 计数改为最外层本次解析入口的局部值，传到 parse_recurse 及所有递归调用。 同一请求所有规则/起点共享原阈值；不要每层/每规则重置；超限沿正常清理链退出。 验证正常重复请求、单次耗尽、错误后新请求及所有解析入口。 | `src/packages/parser/parser.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-062 | P0 | B E | 不确定 | PCRE2 合法前瞻捕获使替换长度下溢 | src/packages/pcre/pcre.cc:1235–1340；调用者 f_pcre_replace/f_pcre_replace_callback | tail=match_end-last_capture_end | 巨大复制长度和内存破坏；未运行可执行攻击样本。 | 捕获只验证在 subject 内；最终尾段却用 group 0 的 match_end 减去捕获的 last_end。前瞻捕获可超过整体匹配终点，差值下溢后进入 memcpy。 | 前瞻/后瞻、零宽、嵌套/可选捕获、空替换及上限，输出正确且 ASan 无越界。 | 用 subject 范围和唯一源游标拼装非重叠捕获替换：游标从 0 开始、最终尾段为 subject_length-cursor。 保留外层捕获优先的重叠规则；支持合法前瞻/后瞻，不通过拒绝合法模式绕过根因。 复核总输出长度与实际复制一致；两个 replace 入口共享同一实现。 | `src/packages/pcre/pcre.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-063 | P1 | B E | 不确定 | pcre_assoc 空匹配推进越尾且余量失真 | src/packages/pcre/pcre.cc:1015–1069 | tmp++，used 不变，下一轮 while(*tmp) | 越界读、错误匹配/UTF 错误；不是已具边界辅助函数的 match_all 路径。 | 空匹配直接 tmp++，未验证终点且未同步 used；下一轮可解引用尾后，并向 PCRE2 声明过长剩余 subject。也可能落到 UTF-8 续字节。 | 空串/空模式、开头/中间/末尾空匹配、UTF-8 和非 UTF 模式均有界；拼接还原输入，无重复丢字。 | 用唯一字节 offset 推导剩余长度，每次执行匹配使用真实 suffix 长度。 空匹配在终点停止，否则复用 UTF/非 UTF 模式相应的有界推进，保持字节拼接不遗漏。 保留模式优先级、tokens/default、嵌套返回数组和正常 suffix 锚点语义。 | `src/packages/pcre/pcre.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-064 | P2 | A B E | 不确定 | 运行时浮点转整数未检查可表示范围 | src/packages/core/efuns_main.cc:3132–3140；src/packages/core/sprintf.cc:1357–1366 | LPC_INT temp = floating_value | 结果依编译器/平台变化；未证明内存破坏。 | f__to_int 直接把 LPC_FLOAT 赋给 LPC_INT；非有限或超出整数可表示范围时是 C++ UB。 sprintf 的浮点 %d 分支先 floor 后直接赋整数，同样未校验。 | 零/负小数、上下界邻点、NaN/无穷都确定；正常值与旧输出一致，越界在 cast 前拒绝。 同时覆盖 sprintf %d 的既有 floor 语义和异常边界。 | 建立编译器与运行时共用的内部检查转换：有限且处于 [-2^63,2^63) 才截向零转换。 上界不能用四舍五入后的 double(INT64_MAX) 作包含边界；失败走 LPC 数值范围错误。 to_int 保持截向零；sprintf %d 保持先 floor 再转换。仅这些 float 路径复用检查，string/buffer 合同不改。 | `src/packages/core/efuns_main.cc`、`src/vm/internal/base/svalue.h`、`src/tests/test_lpc.cc`、`src/packages/core/sprintf.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-065 | P2 | A B D | 不确定 | 常量浮点转整数绕过运行时范围合同 | src/compiler/internal/compiler.cc:1825–1832、1912、2090 | 先设 NODE_NUMBER，再直接窄化 | 同一表达式常量和运行时结果/诊断不一致。 | promote_to_int 在 NODE_REAL 上直接窄化并先改节点类型，越界常量在编译器内发生同类 C++ UB。 | 常量/运行时边界语义一致，失败后下一次编译正常；含 float-cast-overflow 的专项插桩验证。 | 复用 AUD-064 的同一转换规则；确认可表示后才改节点 kind/type/value。 失败生成编译期范围诊断，不发布损坏 program，也不静默推迟规避验证。 核对赋值、显式转换及所有 promote_to_int 调用路径。 | `src/compiler/internal/compiler.cc`、`src/tests/test_lpc_compiler.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-066 | P0 | B E | 不确定 | replace_dollars 消费游标不一致导致负复制长度 | src/packages/dwlib/dwlib.cc:819–882 | currentold 前进而 i 不跳过；跨分配指针算尾长 | 负 len 转无符号大长度进入 strncpy；空替换结果错误。现有 replacement-length 修复未覆盖这些路径。 | 匹配后 currentold 前进但扫描 i 仍逐字递增，可重新匹配已消费范围得到负 len；尾检查还有跨分配指针相减。零长度产出还被误判未发生替换。 | 重叠/内嵌标记、空 key/空替换、多规则和边界；对照结果正确，ASan 无负长度/越界。 | 用源 offset、扫描 offset、输出长度统一所有计算；消费非空模式后跳到其末尾，禁止再扫已消费范围。 空模式保证扫描至少推进一个字节但保留未消费源字符；用独立 matched 标志区分零产出与未匹配。 复制前检查剩余源/目标容量，尾段只在同一字符串内计算；保持规则顺序和返回类型。 | `src/packages/dwlib/dwlib.cc`、`src/tests/test_lpc.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-067 | P1 | D F | 不确定 | include 宏展开直接递归且无预算 | src/compiler/internal/lex.cc:1181–1199 | return handle_include(def->exps) | 编译期栈耗尽/不可控展开；不提供可执行危险源。 | handle_include 在文件包含深度检查之前对宏值递归调用自身，未走普通宏展开预算；循环宏或长链可耗尽原生栈。 | 普通/间接链正确，循环/超限有界拒绝；之后正常编译成功，无原生栈增长。 | 改为迭代解析宏链，复用现有宏展开上限，不另设随意的小兼容阈值。 每一步计预算，循环/超限输出编译诊断；正常定界符后才进入原文件包含路径。 保留符号记录、源码行号和已验证的权限回调原始拼写。 | `src/compiler/internal/lex.cc`、`src/tests/test_lpc_compiler.cc` | 8–16 小时 | 源码确认；定向回归待实施 |
| AUD-068 | P2 | E | 不确定 | uncompress 注册 string 输入却无实现 | src/packages/compress/compress.spec:10；src/packages/compress/compress.cc:338–352 | 非 T_BUFFER 直接 undefined | 编译可通过而已声明重载始终不工作。 | 签名为 buffer uncompress(string&#124;buffer)，运行时却只接受 T_BUFFER，所有 string 在 inflate 前直接返回 undefined。 | 两种输入同字节得到同 buffer；嵌入 NUL、截断、空数据及解压上限，清理正确。 | string/buffer 都先取得借用字节区间与精确长度，共用现有 inflate 和输出上限。 字符串用 SVALUE_STRLEN，不用 strlen；输入保持存活直到解压结束，返回仍是 buffer。 保留非法压缩数据的 undefined 与输出超限错误，完善现有归属文档，不删注册重载。 | `src/packages/compress/compress.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |
| AUD-069 | P2 | E | 不确定 | sprintf 数组数值转换原地修改输入元素 | src/packages/core/sprintf.cc:1075–1081、1357–1369、1404–1405 | carg 指向数组元素，适配时直接改 type/u | 本应格式化的调用改变后续业务可见的数组类型和值；未把此现象扩张成已证跨 owner 写入。 | INFO_ARRAY 的 carg 指向调用方数组元素；%f/%d 的数字类型适配直接改 carg->type/u，从而修改共享输入数组。 | %@f/%@d 输出不变，输入及其别名的 tag/value 前后相同；中途范围/类型错误也不产生部分修改。 | 数值适配在当前迭代的局部 svalue副本上完成，格式化只读取原元素，禁止改写 carg 指向的输入。 沿用 AUD-064 的范围检查及原 floor/浮点规则；局部数字不增加引用所有权或堆分配。 验证别名数组、多元素、失败中途和后续调用；与 AUD-050 分别保留回归。 | `src/packages/core/sprintf.cc`、`src/tests/test_lpc.cc` | 4–8 小时 | 源码确认；定向回归待实施 |

## 7. 详细发现

证据摘录来自审计快照；修复前须核对当前行号/上下文。代码和文档都按原文引用，未运行的验证不算通过。

### AUD-001 · ASCII 输入回调销毁会话后继续访问 interactive_t

- **级别 / 类别 / 来源：**P0 / B C H P / 上游继承问题。
- **触发前提：**启用 ASCII 端口；process_input 同步关闭连接或销毁交互对象。
- **影响：**get_user_data 随后读取已释放会话的 text_start/text_end，构成可达的释放后使用。未证明远程代码执行；默认仅启用其他端口时不等于已触发。
- **根因：**ASCII 分行循环跨 LPC 回调保留裸 ip；现有延迟清理仅由 TelnetReceiveGuard 的深度计数保护。owner scope 只绑定执行上下文，不延长会话生命。
- **来源与确认边界：**merge-base 的 ASCII 回调后循环已有同型访问；fork 的 Telnet 延迟清理没有覆盖 ASCII。源码路径已确认，未运行 ASan 复现。
- **位置：**src/comm.cc:1332–1343；src/comm.cc:2043–2046；src/comm.cc:2079–2091。

`src/comm.cc:1332–1343`：

```text
          push_malloced_string(str);
          set_eval(max_eval_cost);
          owner_bound_safe_apply(APPLY_PROCESS_INPUT, ip->ob, 1, ORIGIN_DRIVER, "interactive");
        }

        if (ip->text_start == ip->text_end) {
          ip->text_start = ip->text_end = 0;
          break;
        }

        p = nl + 1;
      }
```

`src/comm.cc:2043–2046`：

```text
  const bool defer_cleanup = ip->telnet_receive_depth != 0;
  if (!defer_cleanup) {
    release_user_resources(ip);
  }
```

`src/comm.cc:2079–2091`：

```text
  user_del(ip);
  ob->interactive = nullptr;
  ip->ob = nullptr;
  if (defer_cleanup) {
    ip->cleanup_pending = true;
  } else {
    FREE(ip);
  }
  if (net_dead_guard) {
    free_object(&net_dead_guard, "remove_interactive net_dead");
  }
  free_object(&ob, "remove_interactive");
} /* remove_interactive() */
```

- **验证方式：**私有 bufferevent 夹具接入实际 LPC process_input：分别关闭当前连接、destruct 自身、关闭后建立另一会话；推进夹具 event_base 后检查旧会话只清理一次。
- **修复建议：**把既有接收期 guard 覆盖到所有直接同步回调的接收路径；回调后在持有 guard 时检查会话存活，失效立即返回。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不能用已释放 ip 做 IP_VALID 检查；不能误关替代会话；bufferevent 的最终关闭是延迟发生的。
- **关联文件：**`src/comm.h`、`src/net/telnet.cc`、`src/tests/test_lpc_gateway.cc`、`testsuite/clone`。
- **工时：**8–16 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-002 · 交互输出队列没有字节预算和背压

- **级别 / 类别 / 来源：**P1 / C G N / 上游继承问题。
- **触发前提：**慢读或停止读取的连接仍持续获得游戏输出；Telnet 与两种 WebSocket 输出入口均可到达。
- **影响：**排队字节可随输出累计，导致内存压力和服务不可用。没有测得本机耗尽阈值或吞吐下降数据。
- **根因：**evbuffer_add/bufferevent_write 的实际入队点未检查单会话或全局累计字节；读端限额、单次 write 长度和 libwebsockets 帧校验不能限制待发送队列。
- **来源与确认边界：**基线已有无上界输出追加；当前两个 WebSocket sink 与 Telnet sink 的直接调用链仍未提供队列预算。
- **位置：**src/net/ws_ascii.cc:148–155；src/net/ws_telnet.cc:123–130；src/net/telnet.cc:97–105。

`src/net/ws_ascii.cc:148–155`：

```text
void ws_ascii_send(struct lws* wsi, const char* data, size_t len) {
  DEBUG_CHECK(lws_get_protocol(wsi)->id != PROTOCOL_WS_ASCII, "wrong protocol!");
  auto pss = reinterpret_cast<WebSocketSession*>(lws_wsi_user(wsi));
  DEBUG_CHECK(pss == nullptr, "no session data!");

  evbuffer_add(pss->buffer, data, len);
  lws_callback_on_writable(wsi);
}
```

`src/net/ws_telnet.cc:123–130`：

```text
void ws_telnet_send(struct lws* wsi, const char* data, size_t len) {
  DEBUG_CHECK(lws_get_protocol(wsi)->id != PROTOCOL_WS_TELNET, "wrong protocol!");
  auto pss = reinterpret_cast<WebSocketSession*>(lws_wsi_user(wsi));
  DEBUG_CHECK(pss == nullptr, "no session data!");

  evbuffer_add(pss->buffer, data, len);
  lws_callback_on_writable(wsi);
}
```

`src/net/telnet.cc:97–105`：

```text
static inline void on_telnet_send(const char* buffer, unsigned long size, interactive_t* ip) {
  //no need to test if binary as only binary enables telnet
  if (ip->connection_type == PORT_TYPE_WEBSOCKET) {
    auto transdata = u8_convert_encoding(ip->trans, buffer, size);
    auto result = transdata.empty() ? std::string_view(buffer, size) : transdata;
    websocket_send_text(ip->lws, result.data(), result.size());
  } else
    bufferevent_write(ip->ev_buffer, buffer, size);
}
```

- **验证方式：**在私有 loopback/受控传输夹具暂停读端；检查精确边界、单块超限、累计超限、恢复读取、关闭和 TLS 返回值大于 payload 的场景。
- **修复建议：**在实际入队点统一预留并归还字节预算，超限关闭具体慢会话并计数，不阻塞主事件循环或静默截断。
- **依赖：**AUD-001。
- **风险与缓解：**新增资源限制会改变超额输出行为；默认值是方案建议而非现有合同或容量测量。编码、压缩、分片、关闭都必须归还正确字节数。
- **关联文件：**`src/comm.cc`、`src/net/websocket.cc`、`src/net/websocket.h`、`src/comm.h`、`src/base/internal/rc.cc`、`src/base/internal/rc.h`、`src/include/runtime_config.h`、`src/tests/test_lpc_gateway.cc`、`src/Config.example`。
- **工时：**24–40 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-003 · 异步文件/数据库执行没有工作线程和排队内存上界

- **级别 / 类别 / 来源：**P1 / E G N / 上游继承问题。
- **触发前提：**已有工作进入 current_works 后 reqs 再次为空，持续提交新的异步请求；或 getdir 扫描大量目录项。
- **影响：**排队为空不代表工作线程空闲，因此可反复创建 detached thread；getdir 在输出数组截断前累计所有 dirent。可造成线程/内存资源耗尽。
- **根因：**缺少覆盖 queued、running、finished-undelivered 的统一准入预算；MAX_ARRAY_SIZE 只限制交付结果，没有约束生产时的分配。
- **来源与确认边界：**基线已有按 reqs.empty() 起线程的模型；fork 增加 in-flight 记账但没有把它用作容量约束。
- **位置：**src/packages/async/async.cc:284–299；src/packages/async/async.cc:449–474。

`src/packages/async/async.cc:284–299`：

```text
}

void do_stuff(void* (*func)(struct Request*), struct Request* data) {
  std::lock_guard<std::mutex> const lock(reqs_lock);

  if (reqs.empty()) {
    std::thread(thread_func).detach();
  }

  auto* i = new Work;
  i->func = func;
  i->data = data;

  reqs.push_back(i);
}
```

`src/packages/async/async.cc:449–474`：

```text
    req->ret = 0;
    req->status = DONE;
    return nullptr;
  }
  /*
   * Count files
   */
  int i = 0;
  for (auto* de = readdir(dirp); de; de = readdir(dirp)) {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
      continue;
    // #1247 ASYNC-5: the buffer is used as an array of `struct dirent`, indexed
    // by i, so it must grow by sizeof(dirent) per entry -- growing by
    // sizeof(dirent*) (a pointer, ~8 bytes) undersized it and memcpy of the
    // ~280-byte dirent overflowed the heap once a directory held enough
    // entries.
    req->data.resize(static_cast<size_t>(i + 1) * sizeof(struct dirent));
    memcpy(&((dirent*)(req->data.data()))[i], de, sizeof(*de));
    i++;
  }

  closedir(dirp);

  req->ret = i;
  req->status = DONE;
  return nullptr;
```

- **验证方式：**用可控工作屏障提交超过上限的正常短任务，核对线程数、拒绝合同、回调唯一性、取消/关闭归还；大目录夹具必须受控并位于隔离根。
- **修复建议：**复用现有请求队列、唤醒和 owner completion，改为有界 worker 数、请求数及字节预算；getdir 在分配处限制目录项。
- **依赖：**AUD-004。
- **风险与缓解：**不得另建与 owner 调度并行的通用框架；拒绝路径不能遗失 Promise/回调和 live_requests 引用标记。
- **关联文件：**`src/packages/async/async.h`、`src/base/internal/rc.cc`、`src/include/runtime_config.h`、`src/tests/test_lpc.cc`、`testsuite/etc/test-scopes.tsv`、`src/Config.example`。
- **工时：**24–40 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-004 · 异步数据库请求只保存可复用整数连接号

- **级别 / 类别 / 来源：**P1 / E G H / 上游继承问题。
- **触发前提：**请求通过有效性检查后尚未执行，连接被关闭，随后同一槽位被新连接复用。
- **影响：**旧 SQL 可能在新连接执行，破坏连接身份、事务和数据边界；连接半初始化时还有并发访问风险。未证实可利用的内存破坏，不按 RCE 报告。
- **根因：**Request 只有 handle，执行时再次查槽位；锁保证单次访问互斥，但不能证明两次查找是同一代连接。
- **来源与确认边界：**基线已存在可复用槽位与 handle-only 请求；create_db_conn 的扩容持有 db_mutex，因此不把它误报为无锁 realloc UAF。
- **位置：**src/packages/async/async.cc:402–418；src/packages/async/async.cc:554–564；src/packages/db/db.cc:639–662。

`src/packages/async/async.cc:402–418`：

```text
  db_lock_mutex();
  DEFER {
    db_unlock_mutex();
  };
  // see add_db_exec
  db_t* db = find_db_conn(req->handle);
  int ret = -1;
  if (db && db->type->execute) {
    if (db->type->cleanup) {
      db->type->cleanup(&(db->c));
    }

    ret = db->type->execute(&(db->c), req->data.c_str());
    if (ret == -1) {
      if (db->type->error) {
        char* tmp = db->type->error(&(db->c));
        req->path = std::string(tmp);
```

`src/packages/async/async.cc:554–564`：

```text
#ifdef F_ASYNC_DB_EXEC
int add_db_exec(int handle, const char* sql, function_to_call_t* fun, array_t* bound_args) {
  auto* req = new Request();
  req->fun = fun;
  req->bound_args = bound_args;
  req->type = ADBEXEC;
  req->handle = handle;
  req->data = sql;
  bind_request_owner(req);
  return aio_db_exec(req);
}
```

`src/packages/db/db.cc:639–662`：

```text
  for (i = 0; i < db_conn_alloc; i++) {
    if (db_conn_list[i].flags & DB_FLAG_EMPTY) {
      db_conn_list[i].flags = 0;
      db_conn_list[i].type = &no_db;
      db_conn_used++;
      return i + 1;
    }
  }

  fatal("dbConnAlloc != dbConnUsed, but no empty slots");
}

db_t* find_db_conn(int handle) {
  if (handle < 1 || handle > db_conn_alloc || db_conn_list[handle - 1].flags & DB_FLAG_EMPTY) {
    return nullptr;
  }
  return &(db_conn_list[handle - 1]);
}

void free_db_conn(db_t* db) {
  DEBUG_CHECK(db->flags & DB_FLAG_EMPTY, "Freeing DB connection that is already freed\n");
  DEBUG_CHECK(!db_conn_used, "Freeing DB connection when dbConnUsed == 0\n");
  db_conn_used--;
  db->flags |= DB_FLAG_EMPTY;
```

- **验证方式：**ELF test-only wrapper 在异步线程调用 db_lock_mutex 前暂停，主线程关闭并重建同号 SQLite 连接，再释放 worker；旧请求不得触达新库。
- **修复建议：**内部 generation+连接发布状态；在准入时锁内取代号，执行时锁内验证同一连接；两个异步 API 共用这条路径。
- **依赖：**AUD-015。
- **风险与缓解：**不能改变公开整数 handle 语义；不能在执行期间把裸 db_t* 保存在可重分配槽数组之外；失败连接发布必须与 AUD-015 配合。
- **关联文件：**`src/packages/db/db.h`、`src/packages/db/db.cc`、`src/packages/async/async.cc`、`src/tests/CMakeLists.txt`、`testsuite/single/tests/efuns/async.c`。
- **工时：**16–32 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-005 · connections 调试日志记录 NOECHO 输入正文

- **级别 / 类别 / 来源：**P1 / C K / 上游继承问题。
- **触发前提：**开启 connections 调试位，输入经 input_to/get_char 的 NOECHO 路径处理。
- **影响：**密码或其他隐藏输入可能以明文进入日志。默认 debug_level=0 限制默认暴露，但不修复调试时泄漏。
- **根因：**debug 输出位于 NOECHO 检查和模式清理之前，直接格式化 user_command。snoop 的 NOECHO 保护不覆盖日志。
- **来源与确认边界：**基线有相同明文日志点；当前源码仍先记正文再判断 NOECHO。
- **位置：**src/comm.cc:1573–1581。

`src/comm.cc:1573–1581`：

```text

  /* got a command - return it and set command_giver */
  debug(connections, "get_user_command: user_command = (%s)\n", user_command);
  save_command_giver(ip->ob);

  if (ip->iflags & NOECHO) {
    /* must not enable echo before the user input is received */
    detach_user_noecho_localecho_restore_in_owner_frame(ip, command_giver);
  }
```

- **验证方式：**用非真实凭据的固定测试标记分别走普通输入、NOECHO、字符模式与 gateway snapshot；捕获日志，隐藏标记不得出现。
- **修复建议：**在模式位清理之前决定脱敏；隐藏输入只输出固定标记或长度，保留非隐藏调试能力。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不能用关闭整个 connections 日志掩盖问题；不能把真实密码写入回归日志；模式位清理后再检查会漏保护。
- **关联文件：**`src/base/internal/log.cc`、`src/tests/test_lpc_gateway.cc`、`src/comm.h`。
- **工时：**4–8 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-006 · 非 Linux 构建没有执行求值时间限制

- **级别 / 类别 / 来源：**P1 / A F I / 上游继承问题。
- **触发前提：**在项目 CI 声明支持的 macOS/Windows 运行长时间 LPC；正常逻辑缺陷也可触发，不要求上传任意字节码。
- **影响：**set_eval 只把 outoftime 清零，get_eval 固定返回 max_eval_cost；求值循环没有另一套递减预算。长循环可持续占用执行线程。
- **根因：**计时实现只编译于 __linux__，其他平台只有警告而无等价保护。
- **来源与确认边界：**基线存在同一平台分支；当前 CMake/CI 仍包含 macOS/Windows，不把它当作已删除平台。
- **位置：**src/vm/internal/eval_limit.cc:9–29；src/vm/internal/base/interpret.cc:2478–2487。

`src/vm/internal/eval_limit.cc:9–29`：

```text
void init_eval() {
#ifdef __linux__
  init_posix_timers();
#else
  debug_message("WARNING: Platform doesn't support eval limit!\n");
#endif
}

void set_eval(uint64_t etime) {
#ifdef __linux__
  posix_eval_timer_set(etime);
#endif
  outoftime = 0;
}

int64_t get_eval() {
#ifdef __linux__
  return posix_eval_timer_get();
#else
  return max_eval_cost;
#endif
```

`src/vm/internal/base/interpret.cc:2478–2487`：

```text
    if (outoftime) {
      debug_message("Eval interrupted: object %s cost limit reached, limit: %ld usec.\n",
                    current_object->obname, max_eval_cost);
      set_eval(max_eval_cost);
      vm_context_set_max_eval_error(vm_context(), 1);
      error("Too long evaluation. Execution aborted.\n");
    }
    /*
     * Execute current instruction. Note that all functions callable from
     * LPC must return a value. This does not apply to control
```

- **验证方式：**真实 macOS 和 Windows 各运行有截止时间的正常计算、超过预算的有限计算、嵌套 catch、set_eval_limit 和不同 owner 互不污染测试。
- **修复建议：**保留 Linux 计时器；非 Linux 使用线程私有单调 deadline 和 VM 安全点检查，使 set_eval/get_eval 语义一致。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**wall time 与线程 CPU 时间不同；不能把跨 await 的等待算成未定义的新预算；必须沿用现有挂起/恢复的 set_eval 调用语义并记录平台差异。 此项保证纯 LPC 运算在安全点响应预算，不承诺可中断任意阻塞 C/外部库调用。
- **关联文件：**`src/vm/internal/eval_limit.h`、`src/vm/internal/posix_timers.cc`、`src/vm/internal/base/promise.cc`、`.github/workflows/ci.yml`、`src/tests/lpc_vm_bench.cc`。
- **工时：**24–40 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-007 · reclaim_objects 对共享或循环复合值重复遍历

- **级别 / 类别 / 来源：**P1 / B F G H / 上游继承问题。
- **触发前提：**对象变量持有多次共享的 array/mapping/class/function 参数图，周期性或显式执行 reclaim_objects。
- **影响：**仅用递归深度 25 不能限制宽图/共享图工作量；共享子图可按分支数重复展开，阻塞主线程。这里是源码复杂度结论，不是实测停顿时长。
- **根因：**递归访问没有按复合分配身份去重；后台 tick 的时间片只在整次 callback 返回后检查，不能打断一次 reclaim。
- **来源与确认边界：**基线同型递归；fork 修复了 nested 计数平衡，但未解决节点重复扫描。
- **位置：**src/packages/core/reclaim.cc:24–50；src/packages/core/reclaim.cc:113–135。

`src/packages/core/reclaim.cc:24–50`：

```text
static void check_svalue(svalue_t* v) {
  int idx;

  nested++;
  if (nested > MAX_RECURSION) {
    // #1247 RECLAIM-1: keep nested balanced, or reclaim stays disabled for
    // the rest of the pass after one deep recursion.
    nested--;
    return;
  }
  switch (v->type) {
    case T_OBJECT:
      if (v->u.ob->flags & O_DESTRUCTED) {
        free_svalue(v, "reclaim_objects");
        *v = const0u;
        cleaned++;
      }
      break;
    case T_MAPPING:
      gc_mapping(v->u.map);
      break;
    case T_ARRAY:
    case T_CLASS:
      for (idx = 0; idx < v->u.arr->size; idx++) {
        check_svalue(&v->u.arr->item[idx]);
      }
      break;
```

`src/packages/core/reclaim.cc:113–135`：

```text
int reclaim_objects(bool is_auto) {
  if (is_auto) {
    add_gametick_event(time_to_next_gametick(std::chrono::seconds(30 + random_number(30))),
                       TickEvent::callback_type([] {
                         return reclaim_objects(true);
                       }));
  }
  int i;
  object_t* ob;

  reclaim_call_outs();

  cleaned = nested = 0;
  for (ob = obj_list; ob; ob = ob->next_all) {
    if (ob->prog) {
      for (i = 0; i < static_cast<int>(ob->variables.count); i++) {
        check_svalue(&ob->variables.data[i]);
      }
    }
  }

  return cleaned;
}
```

- **验证方式：**少量节点构成共享 DAG/环，在测试计数器中验证每个容器内容只扫描一次；同一已 destruct 对象的每条引用仍分别归零并减计数。
- **修复建议：**在一轮回收中采用显式工作表和 visited 集合；区分容器只扫描一次与每个引用槽必须处理。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不能把 visited 放到所有 svalue 上而漏掉重复对象引用；mapping 删除键时必须保持迭代器有效；owner 并行隔离必须先证明。
- **关联文件：**`src/packages/core/call_out.cc`、`src/backend.cc`、`src/vm/internal/base/svalue.cc`、`src/tests/test_lpc.cc`。
- **工时：**16–32 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-008 · 操作信号处理器直接执行 VM、日志和线程停止

- **级别 / 类别 / 来源：**P1 / G K P / 上游继承问题。
- **触发前提：**SIGINT/SIGUSR1/SIGUSR2 在分配器、日志、owner mutex 或 worker 生命周期操作中到达。
- **影响：**handler 调用非异步信号安全路径；SIGINT 的 owner stop 会加锁并 join，可能重入锁而死锁。没有对现有进程发送信号验证。
- **根因：**把正常线程的诊断/VM 调用/关闭工作放在异步中断上下文。fork 加入 owner 线程后扩大了继承设计的死锁风险。
- **来源与确认边界：**不安全日志/VM handler 设计来自基线；owner stop/join 是 fork 的放大因素。SIGTERM/SIGHUP 已用标志路径，不能与它们混淆。
- **位置：**src/mainlib.cc:141–193；src/vm/internal/owner.cc:6287–6319。

`src/mainlib.cc:141–193`：

```text
   ;
   }*/
}

/* send this signal when the machine is about to reboot.  The script
 which restarts the MUD should take an exit code of 1 to mean don't
 restart
 */
void sig_usr1(int /*sig*/) {
  push_constant_string("Host machine shutting down");
  push_undefined();
  push_undefined();
  apply_master_ob(APPLY_CRASH, 3);
  debug_message("Received SIGUSR1, calling exit(-1)\n");
  exit(-1);
}

/* Abort evaluation */
void sig_usr2(int /*sig*/) {
  debug_message("Received SIGUSR2, current eval aborted.\n");
  outoftime = 1;
}

/*
 * Actually, doing all this stuff from a signal is probably illegal
 * -Beek
 */
void attempt_shutdown(int sig) {
  using namespace backward;
  static StackTrace st;
  static Printer p;

  const char* msg = "Unkonwn signal!";
  switch (sig) {
    case SIGINT:
      msg = "SIGINT: Process interrupted";
      break;
  }

  // Reverse all traps
  signal(SIGINT, SIG_DFL);

  // Print backtrace
  st.load_here(64);
  p.object = true;
  p.color_mode = ColorMode::automatic;
  p.address = true;
  p.print(st, stderr);

  vm_owner_thread_stop();

  // Attempt to call crash()
  fatal(msg);
```

`src/vm/internal/owner.cc:6287–6319`：

```text
void vm_owner_thread_stop() {
  std::vector<std::thread> threads;
  {
    std::lock_guard<std::mutex> lock(owner_runtime_mutex);
    if (owner_thread_stopping) {
      return;
    }
    if (owner_threads.empty()) {
      return;
    }
    owner_thread_stopping = true;
    threads.swap(owner_threads);
  }
  owner_runtime_cv.notify_all();
  for (auto& thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  release_deferred_owner_targets_on_main();
  drain_owner_executor_callback_cleanups(0);
  {
    std::lock_guard<std::mutex> lock(owner_runtime_mutex);
    const auto released =
        owner_scheduler_state.release_all_active_owners(owner_task_executor_runnable);
    if (released > 0) {
      owner_executor_owner_releases.fetch_add(static_cast<uint64_t>(released),
                                              std::memory_order_relaxed);
    }
    owner_thread_stopping = false;
    owner_thread_stops.fetch_add(1, std::memory_order_relaxed);
  }
}
```

- **验证方式：**仅对自建短生命周期 probe 在已知持锁屏障期间发送操作信号；验证 handler 返回、主线程安全点处理和限时退出，不能向现有 driver 发信号。
- **修复建议：**handler 仅设置 lock-free pending 标志并使用经过审核的异步安全唤醒；VM/诊断/join 在正常执行安全点完成。
- **依赖：**AUD-006。
- **风险与缓解：**只把处理推到 backend 不足以打断正在计算的 LPC；不能破坏 Linux 定向求值计时器，也不能从 signal handler 调用会加锁的通用调度器。
- **关联文件：**`src/backend.cc`、`src/backend.h`、`src/vm/internal/base/interpret.cc`、`src/vm/internal/eval_limit.cc`、`src/tests/CMakeLists.txt`。
- **工时：**16–32 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-009 · ASCII 空行在缓冲区起点前读一个字节

- **级别 / 类别 / 来源：**P2 / B C P / fork 未同步上游修复。
- **触发前提：**ASCII 文本缓冲区以换行开始，memchr 返回的 nl 等于行首 p。
- **影响：**CRLF 检测执行 *(nl - 1)，当 nl 在缓冲区起点时发生前读。尚未验证实际分配布局下的可利用性；不能仅凭一次前读宣称 RCE。
- **根因：**未先证明 nl > p 就读取分隔符前一字节。
- **来源与确认边界：**本地上游快照 b2ba3f8f 在 src/net/transport_libevent.cc::get_user_data 添加 nl > ip->text 的 CR 前读保护；fork 对应逻辑仍在 src/comm.cc，未等价吸收。该提交其他修复有部分已等价实现，不能整包重复应用。
- **位置：**src/comm.cc:1315–1327。

`src/comm.cc:1315–1327`：

```text

      comm_append_input(ip, buf, num_bytes);

      p = ip->text + ip->text_start;
      while ((nl = reinterpret_cast<char*>(memchr(p, '\n', ip->text_end - ip->text_start)))) {
        ip->text_start = (nl + 1) - ip->text;

        *nl = 0;
        if (*(nl - 1) == '\r') {
          *--nl = 0;
        }

        if (!(ip->ob->flags & O_DESTRUCTED)) {
```

- **验证方式：**私有 ASCII 接收夹具覆盖起始 LF、空行、连续 LF、CRLF、分片 CR/LF；检查字节访问边界和 callback 次数。
- **修复建议：**保持原始 LF 指针不变；仅在原始 LF 大于行首时检查前一个 CR。
- **依赖：**AUD-001。
- **风险与缓解：**必须与 AUD-010 的游标修正协调，不能加一个判断却继续复用被减一的 nl。 ASan 对结构体成员内部前读可能不报警，不能只依赖 sanitizer 退出码。
- **关联文件：**`src/tests/test_lpc_gateway.cc`、`src/comm.h`。
- **工时：**2–4 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-010 · ASCII CRLF 修剪后下一行游标退回分隔符

- **级别 / 类别 / 来源：**P2 / C / 上游继承问题。
- **触发前提：**一次接收包含两行以上 CRLF 文本，或 CR/LF 恰跨分片。
- **影响：**text_start 按 LF 后位置推进，p 却按减一后的 nl+1 推进；二者不一致，后续命令可能变空或最后一行要等新数据才被处理。
- **根因：**同一个可变 nl 同时表示原始分隔符位置和修剪后的正文末尾。
- **来源与确认边界：**基线已有该游标算法；上游 COMM-1 的前读保护本身不等于修复这一独立 CRLF 游标问题。
- **位置：**src/comm.cc:1318–1343。

`src/comm.cc:1318–1343`：

```text
      p = ip->text + ip->text_start;
      while ((nl = reinterpret_cast<char*>(memchr(p, '\n', ip->text_end - ip->text_start)))) {
        ip->text_start = (nl + 1) - ip->text;

        *nl = 0;
        if (*(nl - 1) == '\r') {
          *--nl = 0;
        }

        if (!(ip->ob->flags & O_DESTRUCTED)) {
          char* str;

          str = new_string(nl - p, "PORT_ASCII");
          memcpy(str, p, nl - p + 1);
          push_malloced_string(str);
          set_eval(max_eval_cost);
          owner_bound_safe_apply(APPLY_PROCESS_INPUT, ip->ob, 1, ORIGIN_DRIVER, "interactive");
        }

        if (ip->text_start == ip->text_end) {
          ip->text_start = ip->text_end = 0;
          break;
        }

        p = nl + 1;
      }
```

- **验证方式：**比较同一正常多行命令在所有拆包方式下的回调序列；混合 LF/CRLF、空正文、末尾不完整行均需覆盖。
- **修复建议：**下一行始终使用原始 LF+1；正文修剪使用独立 line_end。
- **依赖：**AUD-001、AUD-009。
- **风险与缓解：**改变正文结束位置时要保留残片搬移逻辑，不能把未完整行当成完整命令。
- **关联文件：**`src/tests/test_lpc_gateway.cc`。
- **工时：**4–8 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-011 · Telnet 按网络分块净化文本，破坏跨块编码序列

- **级别 / 类别 / 来源：**P2 / C E H / fork 未同步上游修复。
- **触发前提：**一个 UTF-8 字符或启用 session encoding 后的多字节序列被 TCP/Telnet 回调拆开。
- **影响：**尚未收齐的后缀被 u8_sanitize 提前替换；整包正确的文本拆包后发生数据损坏。不能假设网络分块等于字符边界。
- **根因：**on_telnet_data 对每个独立块直接转换/净化，没有保留会话级未完成后缀；临时转换缓冲所有权也需在失败分支保全。
- **来源与确认边界：**上游 5f7be100 同时修复 Telnet 的逐块 UTF-8 净化；fork 的 socket_text 修复只覆盖 socket efun，当前 Telnet 输入仍未等价吸收。ICU 会话转码必须另外保持有状态合同。
- **位置：**src/net/telnet.cc:66–93。

`src/net/telnet.cc:66–93`：

```text
static inline void on_telnet_data(const char* buffer, unsigned long size, interactive_t* ip) {
  char* transdata = const_cast<char*>(buffer);
  auto translen = size;

  // Handle charset transcoding
  if (ip->trans) {
    UErrorCode error_code = U_ZERO_ERROR;
    translen = ucnv_toAlgorithmic(UConverterType::UCNV_UTF8, ip->trans, nullptr, 0, buffer, size,
                                  &error_code);
    if (error_code == U_BUFFER_OVERFLOW_ERROR) {
      error_code = U_ZERO_ERROR;
      transdata = (char*)DMALLOC(translen, TAG_TEMPORARY, "on_telnet_data: transcoding");
      auto written = ucnv_toAlgorithmic(UConverterType::UCNV_UTF8, ip->trans, transdata, translen,
                                        buffer, size, &error_code);
      DEBUG_CHECK(written != translen, "Bug: translation buffer size calculation error");
      if (U_FAILURE(error_code)) {
        debug_message("add_message: Translation failed!");
        transdata = const_cast<char*>(buffer);
        translen = size;
      };
    }
  }

  auto sanitized = u8_sanitize({transdata, translen});
  on_user_input(ip, sanitized.c_str(), sanitized.length());

  if (transdata != buffer) {
    FREE(transdata);
```

- **验证方式：**同一有效文本按每一个字节边界拆分、逐字节到达、非法中间字节、EOF 残片及关闭回调场景；非 UTF-8 session encoding 另测。
- **修复建议：**在具体 interactive_t 会话保存流式解码状态，只缓存不完整后缀；关闭/编码切换时按既有替换合同收口。
- **依赖：**AUD-001。
- **风险与缓解：**不能照搬 socket efun 的整个缓存，因为交互入口还有 ICU 转码、Telnet 控制序列和会话替换；EOF 回调可能关闭会话。
- **关联文件：**`src/comm.h`、`src/comm.cc`、`src/base/internal/strutils.cc`、`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc_gateway.cc`。
- **工时：**16–24 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-012 · get_char 破坏控制字符，并按字节切断 UTF-8 字符

- **级别 / 类别 / 来源：**P2 / C E M P / fork 未同步上游修复。
- **触发前提：**SINGLE_CHAR 模式收到 BS/DEL；或同时启用 no-ANSI 与 strip-before-process-input 时收到 ESC。
- **影响：**BS/DEL 被交付为空字符串，特定配置下 ESC 被替换；static char tmp[2] 且每次只取一个字节，还会把合法 UTF-8 拆成无效单字节字符串。
- **根因：**SINGLE_CHAR 的提取器仍采用单字节/特殊清零规则；ESC 的行模式净化条件没有排除字符模式。
- **来源与确认边界：**本地 upstream/master 的 5f7be100 提供相关修复；fork 已有部分接收阶段处理，但字符提取和 ESC 配置分支仍有差异。docs/archive/Bugs 是冻结历史，不应作为当前合同或直接改写。
- **位置：**src/comm.cc:1453–1475；src/comm.cc:1504–1530。

`src/comm.cc:1453–1475`：

```text
    if (ip->text_end == sizeof(ip->text) - 1) {
      // No more space
      break;
    }
    auto c = static_cast<unsigned char>(data[i]);
    switch (c) {
      case 0x08:  // BACKSPACE
      case 0x7f:  // DEL
        if (ip->iflags & SINGLE_CHAR) {
          ip->text[ip->text_end++] = c;
        } else {
          if (ip->text_end > 0) {
            ip->text_end--;
          }
        }
        break;
      case 0x1b:
        if (CONFIG_INT(__RC_NO_ANSI__) && CONFIG_INT(__RC_STRIP_BEFORE_PROCESS_INPUT__)) {
          ip->text[ip->text_end++] = ANSI_SUBSTITUTE;
          break;
        }
        // fallthrough
      default:
```

`src/comm.cc:1504–1530`：

```text
}

static char* first_cmd_in_buf(interactive_t* ip) {
  char* p;
  static char tmp[2];

  /* do standard input buffer cleanup */
  if (!clean_buf(ip)) {
    return nullptr;
  }

  p = ip->text + ip->text_start;

  /* if we're in single character mode, we've got input */
  if (ip->iflags & SINGLE_CHAR) {
    if (*p == 8 || *p == 127) {
      *p = 0;
    }
    tmp[0] = *p;
    ip->text[ip->text_start++] = 0;
    if (!clean_buf(ip)) {
      ip->iflags &= ~CMD_IN_BUF;
    }
    return tmp;
  }

  /* search for the newline */
```

- **验证方式：**原生接收夹具逐项检查 ESC、BS、DEL、普通字符、UTF-8 字符和跨分片序列；行模式必须维持现有编辑语义。
- **修复建议：**字符模式完整交付合法 UTF-8 序列和控制字节；不完整片段等待，行模式编辑/净化不改。
- **依赖：**AUD-011。
- **风险与缓解：**不能直接套用上游整段旧 comm.cc，fork 有额外 owner/gateway 输入路径；字符数和字节数语义须沿用既有 API。
- **关联文件：**`src/tests/test_lpc_gateway.cc`、`docs/efun/interactive/get_char.md`、`docs/archive/Bugs`。
- **工时：**4–8 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-013 · 超长配置行使后续配置静默停止解析

- **级别 / 类别 / 来源：**P2 / F I K / 上游继承问题。
- **触发前提：**配置文件某行达到固定 120 字节缓冲上限，流进入 failbit；合法后续配置仍存在。
- **影响：**while(getline(...)) 在循环体前结束，体内的截断警告无法覆盖失败的那一行，后续配置没有加载。涉及权限/端口配置时可能违背管理员意图。
- **根因：**把 getline 的缓冲耗尽失败与正常 EOF 当成同一终止条件。
- **来源与确认边界：**基线已有固定缓冲循环；当前完整读取和警告顺序未消除此 failbit 路径。
- **位置：**src/base/internal/rc.cc:32–33；src/base/internal/rc.cc:229–254。

`src/base/internal/rc.cc:32–33`：

```text
const int K_MAX_CONFIG_LINE_LENGTH = 120;
std::deque<std::string> config_lines;
```

`src/base/internal/rc.cc:229–254`：

```text

  char tmp[K_MAX_CONFIG_LINE_LENGTH];
  while (buffer.getline(&tmp[0], sizeof(tmp), '\n')) {
    if (strlen(tmp) == K_MAX_CONFIG_LINE_LENGTH - 1) {
      debug_message("*Warning: possible truncated config line: %s\n", tmp);
    }

    std::string v(tmp);

    // ignore anything after # in the line.
    auto pos = v.find_first_of('#');
    if (pos != std::string::npos) {
      v.erase(pos);
    }
    v = trim(v);
    if (v.empty()) {
      continue;
    }
    config_lines.push_back(v + "\n");
  }

  // OS environment allow-lists for get_os_env()/set_os_env() (package_contrib).
  // Names in the writable list are implicitly readable too; empty denies all.
  scan_config_line("allowed os environment variables : %[^\n]", tmp, kOptional);
  if (strlen(tmp) > 0) {
    CONFIG_STR(__OS_ENV_READABLE__) = alloc_cstring(tmp, "config file: aoev");
```

- **验证方式：**119/120/121 字节边界、有/无末尾换行、超长注释及后续合法配置；验证报错行号和不会悄悄继续启动。
- **修复建议：**保留有界行缓冲，显式区分 EOF、超长行和 I/O 失败；任何读取失败均停止配置初始化，不再悄悄忽略余下安全指令，也不打印整行配置值。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不能只换成无界 std::getline：后续 sscanf 的目标仍有固定数组。保留原可接受长度，不扩大未审计输入范围。
- **关联文件：**`src/tests/test_lpc.cc`、`etc/config.test`、`docs/driver/config.md`。
- **工时：**4–8 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-014 · 数据库编号比较漏写变量展开，错误也未终止 CMake

- **级别 / 类别 / 来源：**P2 / E I J / 上游继承问题。
- **触发前提：**任意两个启用后端编号相同，或默认编号无效；Windows workflow 的 SQLite 参数被公共命令覆盖。
- **影响：**重复后端编号没有在配置时被拒绝，可能拖到 duplicate case 编译失败；无效默认编号仍通过配置。Windows 参数覆盖还会掩盖原意。
- **根因：**冲突检查把 ${PACKAGE_DB_*} 与字面量 "{PACKAGE_DB_*}" 比较；错误分支使用不存在的 message(FATAL ...) 模式，而不是 FATAL_ERROR。
- **来源与确认边界：**数据库 CMake 逻辑来自基线；fork 的跨平台 workflow 继承/叠加了不一致参数。
- **位置：**src/packages/db/CMakeLists.txt:6–19；src/packages/db/CMakeLists.txt:36–43。

`src/packages/db/CMakeLists.txt:6–19`：

```text
    if (NOT "${PACKAGE_DB_DEFAULT_DB}" STREQUAL "${PACKAGE_DB_MYSQL}" AND
            NOT "${PACKAGE_DB_DEFAULT_DB}" STREQUAL "${PACKAGE_DB_SQLITE}" AND
            NOT "${PACKAGE_DB_DEFAULT_DB}" STREQUAL "${PACKAGE_DB_POSTGRESQL}"
            )
      message(FATAL "PACKAGE_DB_DEFAULT_DB is not valid!")
    endif()

    target_compile_definitions(package_db PUBLIC "DEFAULT_DB=${PACKAGE_DB_DEFAULT_DB}")

    if(NOT "${PACKAGE_DB_MYSQL}" STREQUAL "")
        if ("${PACKAGE_DB_MYSQL}" STREQUAL "{PACKAGE_DB_SQLITE}" OR
            "${PACKAGE_DB_MYSQL}" STREQUAL "{PACKAGE_DB_POSTGRESQL}")
          message(FATAL "Conflicting DB type!")
        endif()
```

`src/packages/db/CMakeLists.txt:36–43`：

```text
    if(NOT "${PACKAGE_DB_SQLITE}" STREQUAL "")
      if ("${PACKAGE_DB_SQLITE}" STREQUAL "{PACKAGE_DB_MYSQL}" OR
              "${PACKAGE_DB_SQLITE}" STREQUAL "{PACKAGE_DB_POSTGRESQL}")
        message(FATAL "Conflicting DB type!")
      endif()

      find_package(SQLite3 REQUIRED)
      message(STATUS "PACKAGE_DB USE_SQLITE3: ${PACKAGE_DB_SQLITE}, Found version ${SQLite3_VERSION}")
```

- **验证方式：**CMake 独立配置矩阵：有效编号、非相邻重复、无效默认、空/非数字、关闭全部后端和仅 SQLite；断言失败发生于 configure。
- **修复建议：**只收集启用后端，严格解析整数并比较完整去重长度，默认编号不合法直接 FATAL_ERROR；Windows 参数统一。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**既有有效编号保持原义；不能擅自重排已发布后端数字。配置错误不能降级为换一个后端继续构建。
- **关联文件：**`src/CMakeLists.txt`、`.github/workflows/ci.yml`、`src/packages/db/db.cc`。
- **工时：**4–8 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-015 · 数据库结果与失败连接的资源清理不完整

- **级别 / 类别 / 来源：**P2 / B E G / 上游继承问题。
- **触发前提：**连续执行 PostgreSQL 查询/事务命令，或 PostgreSQL/MySQL 建连失败。
- **影响：**Postgres_cleanup 仅把 PGresult 指针置零，后续 execute 覆盖此前结果；失败连接和 MySQL 初始化资源也存在未完成释放路径。可持续泄漏资源。
- **根因：**驱动槽位释放与后端资源释放没有统一失败/关闭后置条件；部分连接失败分支只释放外层结构，结果所有权未逐项收口。
- **来源与确认边界：**基线包含这些后端实现；当前 PQclear 不在对应成功结果替换/事务路径，f_db_connect 失败后仅 free_db_conn。
- **位置：**src/packages/db/db.cc:1198–1214；src/packages/db/db.cc:1227–1238；src/packages/db/db.cc:842–852；src/packages/db/db.cc:370–379。

`src/packages/db/db.cc:1198–1214`：

```text
static void Postgres_cleanup(dbconn_t* c) {
  c->postgres.res = 0;
}

static char* Postgres_errormsg(dbconn_t* c) {
  return string_copy(PQerrorMessage(c->postgres.conn), "postgresql_errormsg");
}

static int Postgres_close(dbconn_t* c) {
  PQclear(c->postgres.res);
  PQfinish(c->postgres.conn);
  return 1;
}

static int Postgres_execute(dbconn_t* c, const char* s) {
  c->postgres.res = PQexec(c->postgres.conn, s);
```

`src/packages/db/db.cc:1227–1238`：

```text
static int Postgres_connect(dbconn_t* c, const char* host, const char* database,
                            const char* username, const char* password) {
  const char* keywords[] = {"host", "dbname", "user", "password", nullptr};
  const char* values[] = {host, database, username, password, nullptr};

  c->postgres.conn = PQconnectdbParams(keywords, values, 0);

  if ((PQstatus(c->postgres.conn) != CONNECTION_OK)) {
    return 0;
  }
  return 1;
}
```

`src/packages/db/db.cc:842–852`：

```text
  tmp = mysql_init(tmp);
  *(c->mysql.errormsg) = 0;
  c->mysql.handle = mysql_real_connect(tmp, host, username, password, database, 0, nullptr, 0);
  // c->mysql.handle = mysql_connect(tmp, host, username, password);
  if (!c->mysql.handle) {
    strncpy(c->mysql.errormsg, mysql_error(tmp), sizeof(c->mysql.errormsg));
    c->mysql.errormsg[sizeof(c->mysql.errormsg) - 1] = 0;
    FREE(tmp);
    return 0;
  }
```

`src/packages/db/db.cc:370–379`：

```text
    if (db->type->error) {
      errormsg = db->type->error(&(db->c));
      push_malloced_string(errormsg);
    } else {
      push_number(0);
    }
    free_db_conn(db);
  } else {
    push_number(handle);
  }
```

- **验证方式：**隔离 db_io_tests 包装 libpq/MySQL C API，核对每个结果/连接恰好释放一次；SQLite 失败路径加入防双关；真实外部 DB 仅作为额外授权集成。
- **修复建议：**清理所有 PGresult；所有后端失败连接在错误已复制后变成可重复关闭的空状态，槽位重用前零初始化。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不能简单在所有失败路径再 close 一遍：SQLite 已关闭却未置空，MySQL 有手动分配结构；必须先使后端 close 幂等且失败后指针状态清楚。
- **关联文件：**`src/packages/db/db.h`、`src/tests/CMakeLists.txt`、`src/tests/test_db_io.cc`。
- **工时：**8–16 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-016 · 嵌套循环覆盖 catch/time_expression 的禁止挂起上下文

- **级别 / 类别 / 来源：**P2 / A D H P / fork 引入问题。
- **触发前提：**async 函数在 catch/time_expression 等受保护上下文内进入 foreach/其他会重写 context 的构造，再使用 await。
- **影响：**语法检查可能接受本应编译期拒绝的 await，随后运行时仍以受保护控制栈拒绝挂起；诊断阶段和结构语义不一致。运行时守卫仍在，不报已发生栈破坏。
- **根因：**将禁止挂起条件存放在可被普通流控覆盖的 SPECIAL_CONTEXT 位；foreach 等直接重置 context。
- **来源与确认边界：**fork 的 await 检查引入于其 async 改造，局部循环上下文未保留禁挂起约束；上游当前 NO_SUSPEND_CONTEXT 思路可参考，但不能覆盖 fork owner park 实现。
- **位置：**src/compiler/internal/grammar.y:899–904；src/compiler/internal/grammar.y:2143–2151；src/vm/internal/base/promise.cc:1628–1644。

`src/compiler/internal/grammar.y:899–904`：

```text
    {
      $<number>1 = context;
      context = LOOP_CONTEXT;
    }
  statement
    {
```

`src/compiler/internal/grammar.y:2143–2151`：

```text
    {
      if (!compiling_async_function) {
        yyerror("await is only allowed inside an async function.");
      }
      if ((context & SPECIAL_CONTEXT) && !(context & ASYNC_CATCH_CONTEXT)) {
        yyerror("await cannot suspend across a synchronous catch block.");
      }
      CREATE_UNARY_OP($$, F_AWAIT, promise_payload_type($2->type), $2);
    }
```

`src/vm/internal/base/promise.cc:1628–1644`：

```text
void coroutine_await_pending(promise_t* awaited) {
  /* locate the async entry frame; only acatch() markers may sit between
   * it and the current frame (anything else means a C++ frame would be
   * pinned across the suspension) */
  control_stack_t* async_frame = nullptr;
  for (control_stack_t* f = csp; f >= control_stack; f--) {
    if ((f->framekind & FRAME_MASK) == FRAME_FUNCTION && (f->framekind & FRAME_ASYNC)) {
      async_frame = f;
      break;
    }
    if (!((f->framekind & FRAME_MASK) == FRAME_CATCH && (f->framekind & FRAME_ASYNC))) {
      break;
    }
  }
  if (!async_frame || !g_coroutine_promise) {
    error("await: not directly inside an async function body.\n");
  }
```

- **验证方式：**编译失败矩阵覆盖 protected-context × for/foreach/while/do/switch × await；合法 async 循环和匿名/嵌套函数边界另测。
- **修复建议：**禁止挂起状态独立于 break/continue/switch 流控状态，进入/退出受保护语法时可靠保存恢复；继续保留运行时硬守卫。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**错误恢复路径必须恢复上下文；不能为让编译通过而删 coroutine_await_pending 的 protected-stack 检查。
- **关联文件：**`src/compiler/internal/grammar.autogen.cc`、`src/compiler/internal/grammar.autogen.h`、`src/compiler/internal/grammar_rules.h`、`src/tests/test_lpc_compiler.cc`、`testsuite/single/tests/compiler/fail`。
- **工时：**8–16 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-017 · catch 表达式被丢弃值优化器删除潜在异常操作

- **级别 / 类别 / 来源：**P2 / A D / 上游继承问题。
- **触发前提：**catch(expr) 的表达式结果不被使用，expr 含会抛错的算术、索引或转换，但没有被优化器识别的显式副作用。
- **影响：**insert_pop_value 把部分运算节点降成仅求值子节点，异常操作本身可能消失，catch 观察到的错误行为改变。
- **根因：**把“结果不用”误当作“运算没有可观察行为”；抛错也是副作用。
- **来源与确认边界：**merge-base 与当前 upstream/master 都有相同 expr_or_block/insert_pop_value 组合；未找到可直接回移的独立上游修复，不能把上游重构提交误称为修复。
- **位置：**src/compiler/internal/grammar.y:3053–3057；src/compiler/internal/trees.cc:407–433。

`src/compiler/internal/grammar.y:3053–3057`：

```text
  | '(' comma_expr ')'
    {
      $$ = insert_pop_value($2);
    }
;
```

`src/compiler/internal/trees.cc:407–433`：

```text
        case F_AND:
        case F_LSH:
        case F_RSH:
        case F_ADD:
        case F_SUBTRACT:
        case F_MULTIPLY:
        case F_DIVIDE:
        case F_MOD:
        case F_RE_RANGE:
        case F_NE_RANGE:
        case F_RINDEX:
        case F_INDEX:
          if ((expr->l.expr = insert_pop_value(expr->l.expr))) {
            if ((expr->r.expr = insert_pop_value(expr->r.expr))) {
              expr->kind = NODE_TWO_VALUES;
              return expr;
            } else {
              return expr->l.expr;
            }
          } else {
            return insert_pop_value(expr->r.expr);
          }
          break;
        case F_ASSIGN:
          if (IS_NODE(expr->r.expr, NODE_OPCODE_1, F_LOCAL_LVALUE)) {
            LPC_INT tmp = expr->r.expr->l.number;
            expr->kind = NODE_UNARY_OP_1;
```

- **验证方式：**用非常量操作数构造已知会抛错的算术、索引、转换，比较 catch(expr)、catch({expr;}) 与保存结果后执行的行为；先排除常量折叠影响。
- **修复建议：**结果丢弃时保留可能抛错/调用/分配的运算；只删除已证明无异常副作用的节点，或直接生成完整求值再 POP 的 IR。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不要把所有优化全部关闭；也不能仅改 catch(expr) 而让块语句的相同优化继续吞异常。
- **关联文件：**`src/compiler/internal/icode.cc`、`src/compiler/internal/generate.cc`、`src/tests/test_lpc_compiler.cc`。
- **工时：**8–16 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-018 · Promise 持有的已销毁对象引用不参与 reclaim_objects

- **级别 / 类别 / 来源：**P2 / B H P / fork 引入问题。
- **触发前提：**Promise 的已结算值、回调或后继链持有 destruct 对象，外部继续保留该 Promise。
- **影响：**reclaim_objects 只处理既有复合类型，Promise 内部持有的已销毁对象引用不会被此路径及时清理。不是把正常 pending Promise 全部当成泄漏。
- **根因：**fork 引入 T_PROMISE 后，引用标记已经扩展，但实际回收遍历没有同等扩展；mark_promise 与 reclaim 不可相互替代。
- **来源与确认边界：**基线尚无 fork 的当前 Promise 模型；上游 858d5da9 的实现可作覆盖参考，但它是大范围 async 引入，不是可直接整包 cherry-pick 的安全补丁。
- **位置：**src/packages/core/reclaim.cc:24–75。

`src/packages/core/reclaim.cc:24–75`：

```text
static void check_svalue(svalue_t* v) {
  int idx;

  nested++;
  if (nested > MAX_RECURSION) {
    // #1247 RECLAIM-1: keep nested balanced, or reclaim stays disabled for
    // the rest of the pass after one deep recursion.
    nested--;
    return;
  }
  switch (v->type) {
    case T_OBJECT:
      if (v->u.ob->flags & O_DESTRUCTED) {
        free_svalue(v, "reclaim_objects");
        *v = const0u;
        cleaned++;
      }
      break;
    case T_MAPPING:
      gc_mapping(v->u.map);
      break;
    case T_ARRAY:
    case T_CLASS:
      for (idx = 0; idx < v->u.arr->size; idx++) {
        check_svalue(&v->u.arr->item[idx]);
      }
      break;
    case T_FUNCTION: {
      svalue_t tmp;
      program_t* prog;

      if (v->u.fp->hdr.owner &&
          (v->u.fp->hdr.owner->flags &
           O_DESTRUCTED)) {  // #1247-equivalent RECLAIM-2 (funptr double-decrement)
        /* E3 P2: detach the owner only. The funptr keeps pinning its
         * creation-time program via local.prog's func_ref; dealloc_funp()
         * performs the single symmetric decrement. The old manual
         * owner->prog->func_ref-- here double-decremented once
         * dealloc_funp() switched to local.prog (v0.4 §10.3). */
        funptr_detach_owner(v->u.fp);
        cleaned++;
      }

      tmp.type = T_ARRAY;
      if ((tmp.u.arr = v->u.fp->hdr.args)) {
        check_svalue(&tmp);
      }
      break;
    }
  }
  nested--;
}
```

- **验证方式：**Promise resolved/rejected/pending、共享链/环、reaction 绑定参数和 next Promise 各自持有 destruct 对象；检查引用计数与对象最终释放而不改 Promise 结算状态。
- **修复建议：**在 AUD-007 的统一复合遍历中增加 Promise 内容，按 mark_promise 的实际拥有字段逐项覆盖，并保持 owner 准入。
- **依赖：**AUD-007。
- **风险与缓解：**不能在回收中执行 reaction 或改变 settled 状态；后继/共享链只扫描一次，但每条被清理引用都必须减计数。
- **关联文件：**`src/vm/internal/base/promise.h`、`src/vm/internal/base/promise.cc`、`src/vm/internal/base/svalue.cc`、`src/tests/test_lpc.cc`。
- **工时：**8–16 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-019 · 函数表查询允许等于函数总数的越界索引

- **级别 / 类别 / 来源：**P2 / A B D / 上游继承问题。
- **触发前提：**内部调用 get_function_at_index(prog,findex) 的 findex 等于 last_inherited+num_functions_defined。
- **影响：**范围检查用 > 而非 >=，随后访问 function_flags[findex]。未找到公开 LPC 直接提供任意该索引的可靠入口，因此按内部边界缺陷，不报远程可利用漏洞。
- **根因：**把元素数量当成合法最大下标。函数标志数组按函数总数分配，没有证明额外哨兵。
- **来源与确认边界：**基线已有此比较；当前 compiler 分配与解释器查询边界仍不一致。
- **位置：**src/vm/internal/base/interpret.cc:1831–1845；src/compiler/internal/compiler.cc:2660–2667。

`src/vm/internal/base/interpret.cc:1831–1845`：

```text
std::pair<program_t*, int> get_function_at_index(program_t* prog, int findex) {
  if (findex < 0 || findex > prog->last_inherited + prog->num_functions_defined) {
    return std::make_pair(nullptr, 0);
  }
  /* Walk up the inheritance tree to the real definition */
  if (prog->function_flags[findex] & FUNC_ALIAS) {
    findex = prog->function_flags[findex] & ~FUNC_ALIAS;
  }

  while (prog->function_flags[findex] & FUNC_INHERITED) {
    int low, high, mid;
    low = 0;
    high = prog->num_inherited - 1;

    while (high > low) {
```

`src/compiler/internal/compiler.cc:2660–2667`：

```text
  prog->function_flags = reinterpret_cast<unsigned short*>(p);
  if (prog_flags) {
    memcpy(p, prog_flags, (comp_last_inherited + num_func) * sizeof(unsigned short));
    FREE((char*)prog_flags);
  }
  p += align(sizeof(unsigned short) * (comp_last_inherited + num_func));

  if (mem_block[A_ARGUMENT_INDEX].current_size) {
```

- **验证方式：**手工构建最小合法 program 夹具，测试 -1、0、最后一个、count、count+1；另外覆盖 inherited/alias 的有效路径。
- **修复建议：**将上界改为半开区间，保持无效索引返回 {nullptr, 0} 的 pair 合同；检查 alias/inherited 跳转后同样有有效边界。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不能以更大的额外分配掩盖错误；不在没有攻击入口证据时扩展为任意字节码 RCE 声明。
- **关联文件：**`src/vm/internal/base/program.h`、`src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc`。
- **工时：**2–4 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-020 · Windows 安装复制旧路径且没有传播复制失败

- **级别 / 类别 / 来源：**P2 / I J P / fork 引入问题。
- **触发前提：**Windows 构建之后执行 cmake --install，尤其有旧构建文件或多配置生成器时。
- **影响：**driver/lpcc/lpcshell 已改到构建根 bin，Bash 安装仍从旧 src 目录复制；execute_process 未检查结果，可能报告安装成功却漏掉程序或复制旧文件。symbol/o2json/json2o 不能误算为本次路径迁移目标。
- **根因：**输出目录修改只覆盖 driver/lpcc/lpcshell，Windows 的 Bash cp 特例未同步路径，也没有 RESULT_VARIABLE/失败传播。
- **来源与确认边界：**基线使用 src 下目标输出；fork 新统一 bin 输出与旧 Windows 安装块并存。
- **位置：**src/CMakeLists.txt:694–698；src/CMakeLists.txt:728–739。

`src/CMakeLists.txt:694–698`：

```text
set_target_properties(driver lpcc lpcshell PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(symbol "main_symbol.cc")
target_link_libraries(symbol PUBLIC ${FLUFFOS_LINK})
```

`src/CMakeLists.txt:728–739`：

```text
if (WIN32 AND BASH_EXECUTABLE)
  install(CODE "execute_process(COMMAND \"${BASH_EXECUTABLE}\" -lc \"cp -f '${CMAKE_CURRENT_BINARY_DIR}/driver.exe' '${CMAKE_INSTALL_PREFIX}/bin/driver.exe'\")")
  install(CODE "execute_process(COMMAND \"${BASH_EXECUTABLE}\" -lc \"cp -f '${CMAKE_CURRENT_BINARY_DIR}/lpcc.exe' '${CMAKE_INSTALL_PREFIX}/bin/lpcc.exe'\")")
  install(CODE "execute_process(COMMAND \"${BASH_EXECUTABLE}\" -lc \"cp -f '${CMAKE_CURRENT_BINARY_DIR}/symbol.exe' '${CMAKE_INSTALL_PREFIX}/bin/symbol.exe'\")")
  install(CODE "execute_process(COMMAND \"${BASH_EXECUTABLE}\" -lc \"cp -f '${CMAKE_CURRENT_BINARY_DIR}/o2json.exe' '${CMAKE_INSTALL_PREFIX}/bin/o2json.exe'\")")
  install(CODE "execute_process(COMMAND \"${BASH_EXECUTABLE}\" -lc \"cp -f '${CMAKE_CURRENT_BINARY_DIR}/json2o.exe' '${CMAKE_INSTALL_PREFIX}/bin/json2o.exe'\")")
  install(CODE "execute_process(COMMAND \"${BASH_EXECUTABLE}\" -lc \"cp -f '${CMAKE_CURRENT_BINARY_DIR}/lpcshell.exe' '${CMAKE_INSTALL_PREFIX}/bin/lpcshell.exe'\")")
else()
  install(TARGETS driver lpcc lpcshell symbol o2json json2o
    RUNTIME DESTINATION bin)
endif()
```

- **验证方式：**Windows 原生单配置和多配置各执行隔离前缀安装，核对目标 exe 的存在、SHA-256 和 build 中目标文件一致，不运行/覆盖已安装实例。
- **修复建议：**让 CMake install(TARGETS) 管理可执行目标，依赖复制使用目标生成表达式；不拼接旧 src 路径。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不把 MinGW 的 DLL 依赖解析强行套给 MSVC；只有已支持的生成器才能声明通过。
- **关联文件：**`.github/workflows/ci.yml`、`cmake/helper.cmake`。
- **工时：**4–8 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-021 · 供应链清单与实际 vendored 源码身份不一致

- **级别 / 类别 / 来源：**P2 / I O P / fork 引入问题。
- **触发前提：**生成/扫描当前 SBOM 或据此决定依赖安全状态。
- **影响：**manifest/SBOM 把 libwebsockets 记为 4.2.1，而源码声明 4.5.8；patches 信息与本地改动需重新核对，GitHub purl 还缺少可定位 owner 身份。安全扫描和版本归因不可靠。未据此宣称某个 CVE 命中。
- **根因：**自报版本/patch 元数据未与实际 source revision 绑定，purl 用展示 name 拼装；生成后 validate 不能证明提交中的原 SBOM 未漂移。
- **来源与确认边界：**清单/生成工具由 fork 引入；当前 CMake 版本声明可直接对照。不能把“哈希一致”误当作“上游版本身份正确”。
- **位置：**third_party/manifest.yaml:68–76；src/thirdparty/libwebsockets/CMakeLists.txt:500–507；tools/sbom-generate.py:30–43。

`third_party/manifest.yaml:68–76`：

```text
- name: libwebsockets
  path: src/thirdparty/libwebsockets
  version: 4.2.1
  upstream: https://github.com/warmcat/libwebsockets
  license: MIT
  checksum: cbcecb4ed1b8bedf5a825049d3f90e7aa871627fdc1741bf68cd5429ad8141d0
  patches: multiple local build tweaks (LWS_WITH_* cache vars in src/CMakeLists.txt)
  notes: Version banner in libwebsockets.h.
- name: scope-guard
```

`src/thirdparty/libwebsockets/CMakeLists.txt:500–507`：

```text
set(CPACK_PACKAGE_VERSION_MAJOR "4")
set(CPACK_PACKAGE_VERSION_MINOR "5")
set(CPACK_PACKAGE_VERSION_PATCH_NUMBER "8")

set(CPACK_PACKAGE_VERSION_PATCH "${CPACK_PACKAGE_VERSION_PATCH_NUMBER}-${LWS_BUILD_HASH}")
set(CPACK_PACKAGE_RELEASE 1)

set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
```

`tools/sbom-generate.py:30–43`：

```text
def make_purl(dep):
    """Build a package URL for a dependency, or None when unknown."""
    name = str(dep.get("name", "")).strip()
    version = str(dep.get("version", "unknown")).strip()
    upstream = str(dep.get("upstream", ""))
    if not name or not version or version == "unknown":
        return None
    namespace = "github"
    if "gitlab.com" in upstream:
        namespace = "gitlab"
    if name in ("jemalloc",):
        namespace = "generic"
    return f"pkg:{namespace}/{name}@{version}"
```

- **验证方式：**离线比较 manifest、实际源码版本、已知 source revision、补丁差异与生成 SBOM；检查各组件 purl owner/repo；修改输入后的只读 check 必须失败。
- **修复建议：**以 manifest 为身份单一来源，记录可核对的 commit/版本/补丁；按 source URL 构建 purl，增加不先覆写产物的 check。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**此项不是授权升级全部依赖、发布 SBOM 或上传服务；无法证明精确 vendored 基线时写不确定，不编造 commit。
- **关联文件：**`third_party/sbom.json`、`third_party/cyclonedx.schema.json`、`tools/sbom-generate.py`、`.github/workflows/ci.yml`、`src/thirdparty/libevent`。
- **工时：**8–16 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-022 · owner 状态把历史验收写死为当前 ready

- **级别 / 类别 / 来源：**P2 / K O P / fork 引入问题。
- **触发前提：**查询 vm_owner 状态；当前源码、构建或证据已经变化，但硬编码 ready/零阻塞/占位摘要仍被返回。
- **影响：**调用方可能把历史实现/测试记录误认成当前二进制的有效运行验收。它不是本轮测试通过的证据。
- **根因：**状态字段脱离当前 commit、构建配置及原始证据有效性；权威合同要求失效时回到 incomplete/blocked，但代码没有该判断。
- **来源与确认边界：**相关 ready/evidence 常量由 fork 引入；当前 owner.cc 直接赋值，文档同时要求失效复位。
- **位置：**src/vm/internal/owner.cc:1602–1631；docs/multicore-production-gate.md:6–15；docs/multicore-production-gate.md:124–127。

`src/vm/internal/owner.cc:1602–1631`：

```text
      "xkx_5513c8a12_multicore_mudlib_audit_2026_06_25_zero_delayed_object_payloads");
  add_mapping_pair(map, "production_gate_ready", 1);
  add_mapping_string(map, "production_gate_blocker", "");
  add_mapping_string(map, "production_gate_required_users", "1,3,10");
  add_mapping_string(map, "production_gate_required_durations", "smoke,30m");
  add_mapping_pair(map, "production_gate_pressure_evidence_ready", 1);
  add_mapping_string(map, "production_gate_pressure_evidence",
                     "xkx_audit_10_users_30m_2026_06_25_zero_timeouts_zero_gateway_errors");
  add_mapping_string(map, "production_gate_required_modes", "off,audit,enforced");
  add_mapping_string(map, "production_gate_required_scenarios",
                     "login,create,move,chat,inventory,shop,quest,combat,skills,mail,reconnect,"
                     "gateway_callback,socket_callback,heartbeat,callout");
  add_mapping_string(map, "production_gate_evidence_schema",
                     "multicore_production_gate_evidence_v1");
  add_mapping_pair(map, "production_gate_evidence_required", 1);
  add_mapping_pair(map, "production_gate_short_smoke_sufficient", 0);
  add_mapping_string(map, "production_gate_minimum_ready_evidence",
                     "accepted_30m_pressure_scope_final_audit_and_socket_release_handshake");
  add_mapping_pair(map, "production_gate_unclassified_hotspots_required_zero", 1);
  add_mapping_pair(map, "production_gate_direct_cross_owner_writes_required_zero", 1);
  add_mapping_pair(map, "production_gate_context_leaks_required_zero", 1);
  add_mapping_pair(map, "production_gate_future_backlog_required_zero", 1);
  add_mapping_pair(map, "production_gate_same_owner_claim_conflict_required_zero", 1);
  add_mapping_pair(map, "production_gate_gateway_error_delta_required_zero", 1);
  add_mapping_string(map, "production_gate_socket_release_policy",
                     "owner_safe_synchronous_release_acquire_handshake");
  add_mapping_pair(map, "production_gate_socket_release_handshake_ready", 1);
  add_mapping_string(map, "production_gate_socket_release_handshake_evidence",
                     "socket_release_owner_epoch_handshake_contract_v1");
  add_mapping_string(map, "production_gate_report_schema", "xkx_gateway_loadtest_report_v1");
```

`docs/multicore-production-gate.md:6–15`：

```text
## 证据新鲜度边界（2026-08-09 审计）

本节中的 10 用户 30 分钟 audit 压测、XiaKeXing mudlib final audit 和 `socket_release` handshake 结论均为 **2026-06 历史证据**（`historical`），产生于旧 checkout；当前 checkout（`24bd5f4f`，2026-08-09）只验证了 CTest 392/392、ASan CTest 392/392 与短时 runtime smoke。`docs/reports/multicore-mudlib-audit-2026-06-25.md` 在当前 checkout **不存在**（链接漂移，已修正）。在重新运行对应压测/audit 并产出 `fluffos.evidence.manifest.v1` 报告前，历史结论不能单独作为当前自用验收的 ready 依据；机器可读 `production_gate_ready` 字段只能由当前证据检查脚本验证后置位。更大规模的外部压测不属于本独立 fork 的默认验收范围。

## 当前结论

当前 driver 侧多核化执行面已经满足本 fork 的 runtime contract：owner-local lifecycle、OwnerExecutor callback task boundary、heartbeat、callout、async/file/db、DNS、socket read/write/close callback、gateway command execute 和 `socket_release` owner-safe release/acquire handshake 都已经具备运行时合同。正常运行路径要求 `normal_path_main_fallback_count=0`；`off`、owner executor 不可用和 rollback/failure 场景只能作为显式兼容 fallback，不计入正常业务热路径。

> **证据边界**：下面“10 用户 30 分钟 audit 压测已满足”“final audit 已收口”“`production_gate_ready=1`”的表述依据的是 2026-06 历史证据（见上方“证据新鲜度边界”）；当前 checkout 尚未重新产生等效当前证据，机器可读 ready 字段应按 `tools/docs/check-evidence.py` 的当前证据要求重新验证后才可作为完整自用验收依据。
```

`docs/multicore-production-gate.md:124–127`：

```text
- 压力证据、mudlib audit 证据或 socket release handshake evidence 失效后，必须将兼容字段 `production_gate_ready` 回退并重新运行相关 smoke、压力复核或 handshake 合同测试。
- 若 final audit 发现 mudlib 同步 cross-owner 调用，先迁 mudlib/API 模型，再重跑定向 smoke 或 audit 复核。
- 若失败来自 gateway 协议解析或大 payload，先修协议/客户端解析，再重跑相关 smoke。
- 若失败来自 owner executor context cleanup、future backlog 或 stale drop，先补 driver 合同测试，再重跑 C++、LPC 和真实 mudlib 定向验收。
```

- **验证方式：**无证据、历史证据、commit 不符、原始日志摘要损坏和当前有效 envelope；验证 API 状态不能仅凭常量报告 ready。
- **修复建议：**立即停止无条件 current-ready 声明；历史说明与当前可证明状态分离，只有绑定当前构建的真实验证结果才能置 ready。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不能为让 ready 回到 1 伪造日志或加入可手工绕过的配置开关；不引入本项目不需要的正式发布认证体系。
- **关联文件：**`tools/docs/check-evidence.py`、`docs/evidence/manifest.schema.json`、`src/tests/test_lpc_owner.cc`。
- **工时：**8–16 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-023 · 权威排版说明仍描述已被 CI 扩大的 pilot 范围

- **级别 / 类别 / 来源：**P3 / L M O P / fork 引入问题。
- **触发前提：**维护者依据 coding-standard 判断应运行哪个格式门禁，或按旧说明评估 U10 完成范围。
- **影响：**文档写 pilot 子集，而 CI 已运行 --all-native，贡献者无法按权威说明复现检查范围。它不是新增运行时漏洞。
- **根因：**格式门禁扩大后，权威说明和过渡状态未同步。
- **来源与确认边界：**fork 的 U10 风格推广改变了 CI 范围，当前 coding-standard 仍有旧描述。
- **位置：**docs/coding-standard.md:29–29；.github/workflows/ci.yml:87–104。

`docs/coding-standard.md:29–29`：

```text
| [CI][up-ci] | 运行 LPC syntax 测试和 testsuite/format.sh --check | 本地 U10f 已接入固定 pilot；完整 native/LPC 存量门禁仍由 U10g 按模块收口，不能把 pilot 写成全库通过 |
```

`.github/workflows/ci.yml:87–104`：

```text
      # U10g has reviewed and written the module-scoped native backlog.
      # Keep this check read-only; CI must never rewrite the checkout.
      - name: Check all native sources
        env:
          CLANG_FORMAT: clang-format-18
        run: python3 tools/style/check-format.py --all-native

      - name: Run LPC syntax self-tests
        run: |
          node tools/lpc-syntax/test.mjs
          node tools/lpc-syntax/test-format-corpus.mjs

      # The full allowed corpus is read-only in CI; testsuite/format.sh owns
      # the explicit exclusion list for raw and intentionally invalid fixtures.
      - name: Check LPC formatter corpus
        run: bash testsuite/format.sh --check

  docs-check:
```

- **验证方式：**逐字对照 check-format.py 的 --all-native 发现/排除规则、CI 调用与文档，确认 generated/thirdparty/raw-byte fixture 的边界一致。
- **修复建议：**仅更新实际归属文档的范围和命令，去除失效过渡语句；不再次全仓重排代码。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**不能把排版检查扩大为未授权全仓命名重构；只改文档，不动第三方/生成文件。
- **关联文件：**`tools/style/check-format.py`、`src/.clang-tidy`、`docs/project-modernization-plan-2026-10.md`。
- **工时：**2–4 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-024 · 构建手册混淆数据库编号，并给出失配的测试/并行命令

- **级别 / 类别 / 来源：**P3 / I M O / 上游继承问题。
- **触发前提：**按构建手册选择 PACKAGE_DB_SQLITE=1/2，或顺序复制 Running Tests 中的 cwd 操作；在 WSL2 按页内 nproc 并行示例构建。
- **影响：**把后端 ID 误当 SQLite API 版本，会选择与其他后端/默认后端冲突的编号；测试块从 build 直接 cd testsuite 指向错误目录；不限并行的示例与项目 WSL 构建约束冲突。没有声称本轮实际发生 OOM。
- **根因：**上游遗留构建说明没有按真实 CMake 参数语义、当前目录和 fork 的资源约束同步校准。
- **来源与确认边界：**完整 base..HEAD 的 docs/build.md 差异未改这些遗留段；当前 CMake 明确将参数称作 DB Type，db_connect 文档也以编号解释，SQLite 段的 version 1/2 说法不成立。
- **位置：**docs/build.md:285–288；docs/build.md:292–301；docs/build.md:312–318；src/CMakeLists.txt:19–23。

`docs/build.md:285–288`：

```text
- `-DMARCH_NATIVE=OFF`: Ensures portability across different CPUs
- `-DPACKAGE_CRYPTO=OFF`: Crypto package is typically disabled on Windows
- `-DPACKAGE_DB_SQLITE=1`: Enable SQLite (set to 2 for version 2)
- `-DPACKAGE_DB_MYSQL=""`: Disable MySQL (or set to enable)
```

`docs/build.md:292–301`：

````text
```shell
# Unit tests
$ cd build
$ CTEST_OUTPUT_ON_FAILURE=1 make test

# LPC testsuite
$ cd testsuite
$ ../build/bin/driver etc/config.test -ftest
```
````

`docs/build.md:312–318`：

```text

Common package options:
- `PACKAGE_DB_SQLITE=1` or `=2`: Enable SQLite support
  - `=1` enables SQLite3 API version 1
  - `=2` enables SQLite3 API version 2 (recommended, includes newer features)
- `PACKAGE_DB_MYSQL=""`: Disable MySQL (or set to enable)
- `PACKAGE_CRYPTO=OFF`: Disable crypto package (typical on Windows)
```

`src/CMakeLists.txt:19–23`：

```text
option(PACKAGE_DB "db package" ON)
set(PACKAGE_DB_MYSQL "1" CACHE STRING "DB Type for Mysql")
set(PACKAGE_DB_POSTGRESQL "" CACHE STRING "DB Type for POSTGRESQL")
set(PACKAGE_DB_SQLITE "" CACHE STRING "DB Type for SQLITE")
set(PACKAGE_DB_DEFAULT_DB "1" CACHE STRING "Default DB Handle")
```

- **验证方式：**静态对照 src/CMakeLists.txt、src/packages/db/CMakeLists.txt、docs/efun/db/db_connect.md 和 AGENTS §4.2；按命令序列推导 cwd；不实际 configure、安装或启动 driver。
- **修复建议：**把 SQLite 参数解释改为唯一后端编号；给出 SQLite-only 与多后端的完整有效编号/默认值示例；修正测试 cwd，并把该页不限并行示例按项目规则改为明确上限。
- **依赖：**无其他问题修复前置；仍须满足公共准入。
- **风险与缓解：**只改文档，不升级 SQLite、不更改编号语义、不把构建示例变成实际部署；保留用户可搜索的参数名。
- **关联文件：**`docs/build.md`、`docs/efun/db/db_connect.md`、`src/CMakeLists.txt`、`src/packages/db/CMakeLists.txt`、`AGENTS.md`。
- **工时：**2–4 小时。
- **状态：**静态确认；代码项未做本轮动态复现；未修复。验收与执行步骤见修复方案同 ID。

### AUD-025 · 冷启动 ScratchArenaBinding 留下空 current_

- **级别 / 类别 / 来源：**P1 / B D H / fork 引入问题。
- **审读视角 / 性质：**C++ / 内存与编译器；产品或工具实现缺陷。
- **位置：**src/compiler/internal/compile_arena.cc:174–208、220–240、285–324。

**源码摘录：**`src/compiler/internal/compile_arena.cc:288–290`

```cpp
  arena_.binding_marks_.push_back(
      {arena_.current_, arena_.current_ ? arena_.current_->used : 0, arena_.live_});
  arena_.bindings_active_++;
```

**源码摘录：**`src/compiler/internal/compile_arena.cc:316–319`

```cpp
  ScratchArena::BindingMark const mark = arena_.binding_marks_[mark_index_];
  size_t const before = arena_.bytes_live_;
  arena_.rewind_to(mark.chunk, mark.used, mark.live_head);
  bytes_released_ = before - arena_.bytes_live_;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**arena 回退或溢出拒绝可能破坏嵌套 scope、代际和统计；保留 LIFO 合同，并验证拒绝后用量与原有绑定仍有效。
- **触发前提：**非静态 arena 首次分配发生在 binding 内；退出 binding 后再次分配。
- **影响：**合法内部调用顺序导致崩溃及 live/cycle 计量失真；未证实普通 LPC 直接控制该顺序。
- **根因：**冷 binding 记录空 chunk。首次分配建立 base_，回退仍把 current_ 置空，且漏减 base_ 使用量。下一次 alloc 因 base_ 非空而解引用空 current_。
- **现有上下文（控制流摘要，非逐字代码引用）：**current_ = chunk; // chunk 为空而 base_ 已存在。
- **证据与确认边界：**源码闭环；arena-debug-binding 与 arena-asan-ubsan-binding 的同源探针均退出 -11，日志保留。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**空起始 mark 回退时归还新 base_ 的 used，并把 current_ 恢复到可复用 base_；保留 retained/live 链。 保持 generation 和严格 LIFO 拒绝逻辑；bytes_released、cycle_bytes、bytes_live 同步回退。 补冷/嵌套 binding、oversize、异常退出和下一次 begin/reset。
- **验证方式：**退出冷 binding 后计量归零且可再次分配；内层回退不破坏外层数据；次序/代际错误仍拒绝。
- **关联源码/测试：**`src/compiler/internal/compile_arena.cc`、`src/tests/test_compile_arena.cc`。
- **文档归属：**`src/compiler/internal/compile_arena.h`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-025)。

### AUD-026 · ScratchArena 尺寸运算可回绕

- **级别 / 类别 / 来源：**P2 / B D P / fork 引入问题。
- **审读视角 / 性质：**C++ / 内存与编译器；产品或工具实现缺陷。
- **位置：**src/compiler/internal/compile_arena.cc:32–45、174–216。

**源码摘录：**`src/compiler/internal/compile_arena.cc:32–39`

```cpp
size_t align_up(size_t n) {
  constexpr size_t kAlign = alignof(max_align_t);
  return (n + kAlign - 1) & ~(kAlign - 1);
}

Chunk* new_chunk(size_t size, bool oversize, size_t* chunk_mallocs, int tag, const char* desc) {
  void* mem = DMALLOC(kChunkHeaderSize + size, tag, desc);
  (*chunk_mallocs)++;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**arena 回退或溢出拒绝可能破坏嵌套 scope、代际和统计；保留 LIFO 合同，并验证拒绝后用量与原有绑定仍有效。
- **触发前提：**内部 API 接收接近 SIZE_MAX 的尺寸；普通 LPC 到该极值的路径未证实。
- **影响：**内部分配合同缺口，不能据此宣称远程可利用。
- **根因：**align_up、header+size、used+need、alloc_string(len+1) 都先作未检查的 size_t 加法，极端参数可变成小分配或重叠区间。
- **现有上下文（控制流摘要，非逐字代码引用）：**need=align_up(size); if(used+need>capacity)。
- **证据与确认边界：**源码闭环；Debug/ASan-UBSan oversize 探针均以 86 报告非法长度未被拒绝；未做巨大内存写入。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**分配/游标变化前检查 len+1、对齐加数、header+size；非法尺寸抛 std::bad_alloc，不返回可用指针。 用 need > size-used 判断剩余空间；保留零尺寸返回空的合同。 只验证极值被拒绝和状态不变，不实际申请巨量内存；成功分配继续验证无重叠。
- **验证方式：**0、对齐和块边界正常；各溢出邻界拒绝，计量不变；后续小分配可用。
- **关联源码/测试：**`src/compiler/internal/compile_arena.cc`、`src/tests/test_compile_arena.cc`。
- **文档归属：**`src/compiler/internal/compile_arena.h`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-026)。

### AUD-027 · 编译永久改变进程 locale

- **级别 / 类别 / 来源：**P1 / D G / 不确定。
- **审读视角 / 性质：**LPC 编译器 / C++；产品或工具实现缺陷。
- **位置：**src/compiler/internal/compiler.cc:2215–2220。

**源码摘录：**`src/compiler/internal/compiler.cc:2215–2220`

```cpp
  {
    // make sure we use the C locale during parsing
    auto* current_locale = setlocale(LC_ALL, "C");
    DEFER {
      setlocale(LC_ALL, current_locale);
    };
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。
- **触发前提：**非 C locale 下编译，正常或异常退出。
- **影响：**后续数值转换、格式和并发线程的 locale 行为改变。
- **根因：**setlocale(LC_ALL,"C") 返回修改后的值而不是原值，DEFER 恢复的仍是 C；切换本身也影响其他线程。
- **现有上下文（控制流摘要，非逐字代码引用）：**保存 setlocale(...,"C") 的返回值。
- **证据与确认边界：**compiler-locale-debug 的同源 GTest 探针退出 1，保留具体 locale 前后断言；非仅推测。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**移除进程全局 LC_ALL 切换，改为编译器内部线程局部 C-locale 作用域。 POSIX 用 newlocale/uselocale；Windows 用 CRT 线程局部模式；恢复先前线程 locale/模式，失败报告。 覆盖成功、错误和嵌套退出；并行观察线程不得看到 locale 改变。
- **验证方式：**非 C locale 下 LPC 小数语义稳定，前后 locale 相同，观察线程不受影响；缺平台记 external-required。
- **关联源码/测试：**`src/compiler/internal/compiler.cc`、`src/tests/test_lpc_compiler.cc`。
- **文档归属：**`src/compiler/internal/compiler.cc`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-027)。

### AUD-028 · 内建预定义字符串构造无容量边界

- **级别 / 类别 / 来源：**P2 / B D / 不确定。
- **审读视角 / 性质：**LPC 编译器 / C++；产品或工具实现缺陷。
- **位置：**src/compiler/internal/lex.cc:3219–3225、3263–3276。

**源码摘录：**`src/compiler/internal/lex.cc:3219–3225`

```cpp
static void add_quoted_predefine(const char* def, const char* val) {
  char save_buf[MAXLINE];

  strcpy(save_buf, "\"");
  strcat(save_buf, val);
  strcat(save_buf, "\"");
  add_predefine(def, -1, save_buf);
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。
- **触发前提：**构建元数据超过固定容量。lpc_predefs 当前无生产写入者，旧分支不能报成 -D 可达。
- **影响：**元数据长度合同缺失；未证明玩家输入可达。
- **根因：**加引号值由 strcpy/strcat 拼入 MAXLINE，版本先 sprintf 到 1024 字节数组；这些内建构建元数据构造路径实际可达。
- **现有上下文（控制流摘要，非逐字代码引用）：**固定数组 strcat；版本先 sprintf 到 1024 字节数组。
- **证据与确认边界：**当前调用者及全仓 lpc_predefs 引用核对，撤回启动 -D 可达推断。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**版本和加引号值改为单一动态字符串构造，保持内容不变，直接把 PROJECT_VERSION 传入加引号构造。 只改这条实际使用的构造路径；lpc_predefs 无生产写入者，不恢复 -D，不把旧列表的 sscanf 顺手纳入本单元。 让测试调用同一内部生产构造函数，验证短值、长值及邻界；不新增 LPC 接口。
- **验证方式：**短值逐字保持；长构建元数据安全构造或按既有宏长度上界明确拒绝；版本不经过固定 sprintf 缓冲。
- **关联源码/测试：**`src/compiler/internal/lex.cc`、`src/compiler/internal/lex.h`、`src/tests/test_lpc_compiler.cc`。
- **文档归属：**`src/compiler/internal/lex.h`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-028)。

### AUD-029 · 宏实参采集先越界再报错

- **级别 / 类别 / 来源：**P0 / B D / 不确定。
- **审读视角 / 性质：**LPC 编译器 / C++；产品或工具实现缺陷。
- **位置：**src/compiler/internal/lex.cc:4307–4406、4410–4413。

**源码摘录：**`src/compiler/internal/lex.cc:4374–4384`

```cpp
    if (parcnt < 0 || (c == ',' && !parcnt && !dquote && !squote)) {
      /* strip off trailing whitespace char if there was one */
      if (uisspace(*(out - 1))) {
        *(out - 1) = 0;
      } else {
        *out++ = 0;
      }
      if (parcnt < 0) {
        return argc + 1;
      }
      argv[++argc] = out;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。
- **触发前提：**空首实参或超出 NARGS 的分隔数量；报告不提供可执行恶意源。
- **影响：**编译期边界前读和栈越界写，后续诊断不能撤销写入。
- **根因：**extract_args 的空首参数读 out-1；逗号分支 argv[++argc] 可先写 argv[NARGS]，下一轮才到数量上限检查。调用者传栈数组。
- **现有上下文（控制流摘要，非逐字代码引用）：**uisspace(*(out-1)); argv[++argc]=out;。
- **证据与确认边界：**调用者栈数组及两处访问顺序确认，未运行危险样本。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**收尾先确认 out>argv[argc]；空实参直接写终止符，不读前字节。 递增 argc 和写下一槽前检查槽边界，保留宏形参数量错误语义。 覆盖空首/中/末参数、嵌套括号/引号、数量边界及错误后再次编译。
- **验证方式：**合法输入不回归，超量先拒绝；ASan/UBSan 无越界，下一次正常编译成功。
- **关联源码/测试：**`src/compiler/internal/lex.cc`、`src/tests/test_lpc_compiler.cc`。
- **文档归属：**`src/compiler/internal/lex.cc`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-029)。

### AUD-030 · 字符候选绕过预处理指令长度上限

- **级别 / 类别 / 来源：**P1 / B D F P / fork 引入问题。
- **审读视角 / 性质：**LPC 编译器 / C++；产品或工具实现缺陷。
- **位置：**src/compiler/internal/lex.cc:3783–3893、3987–4028。

**源码摘录：**`src/compiler/internal/lex.cc:3847–3850`

```cpp
    if (char_state == directive_char_state_t::kOctal) {
      if (directive_is_octal(static_cast<unsigned char>(c))) {
        char_candidate.push_back(c);
        return;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。
- **触发前提：**跨续行的长字符候选，尚未进入回放。
- **影响：**编译期内存和 CPU 放大；受限输出数组不等于受限临时存储。
- **根因：**append 限制 DEFMAX，十六/八进制候选却先无限追加 char_candidate。续行可在回放到受限输出前占用任意候选内存。
- **现有上下文（控制流摘要，非逐字代码引用）：**char_candidate.push_back 不受 append 上限保护。
- **证据与确认边界：**待决 std::string 与受限 append 的分离路径确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**每次候选追加前按已输出+待决候选总量检查 DEFMAX，保持单一追加规则。 超限锁存原超长诊断并终止该编译，不截断后当合法源；保留正确行号与下次编译状态。 覆盖十六/八进制、缺结束引号、LF/CRLF 续行和字符/注释歧义。
- **验证方式：**候选峰值受限；超长有确定诊断；边界内源码、位置映射及下一次编译一致。
- **关联源码/测试：**`src/compiler/internal/lex.cc`、`src/tests/test_lpc_compiler.cc`。
- **文档归属：**`src/compiler/internal/lex.cc`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-030)。

### AUD-031 · case 常量移位漏掉低六位合同

- **级别 / 类别 / 来源：**P2 / D / 不确定。
- **审读视角 / 性质：**LPC 编译器 / C++；产品或工具实现缺陷。
- **位置：**src/compiler/internal/grammar.y:1235–1242。

**源码摘录：**`src/compiler/internal/grammar.y:1235–1242`

```cpp
  | constant L_LSH constant
    {
      $$ = $1 << $3;
    }
  | constant L_RSH constant
    {
      $$ = $1 >> $3;
    }
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。
- **触发前提：**case 等 constant 文法中的非常规移位计数。
- **影响：**编译结果偏离 LPC 模 64 语义。
- **根因：**constant 左右移直接使用 $3，未按运行时合同取低六位；负数或不小于 64 的计数触发 C++ UB/构建差异。
- **现有上下文（控制流摘要，非逐字代码引用）：**$$=$1<<$3; $$=$1>>$3;。
- **证据与确认边界：**文法动作和既有低六位合同直接比对。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**复用安全 LPC 移位实现，计数取低六位；左移用无符号位运算，不拒绝合同允许的计数。 同步 Bison fallback 和 generate_ebnf 输出，不手改生成物。 比对运行时、普通折叠、case 常量三条路径。
- **验证方式：**-1/0/63/64/65、极值计数和符号边界结果一致，UBSan 无 shift 报告。
- **关联源码/测试：**`src/compiler/internal/grammar.y`、`src/tests/test_lpc_compiler.cc`。
- **文档归属：**`src/compiler/internal/grammar.y`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-031)。

### AUD-032 · replace_string 跳表复制漏记目标长度

- **级别 / 类别 / 来源：**P0 / B E / 不确定。
- **审读视角 / 性质：**efun / VM / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/core/efuns_main.cc:2261–2318。

**源码摘录：**`src/packages/core/efuns_main.cc:2261–2268`

```cpp
    dst2 = dst1 = new_string(max_string_length, "f_replace_string: 2");

    if (plen > 1) {
      while (src < flimit) {
        if ((skip = skip_table[static_cast<unsigned char>(src[probe])])) {
          for (climit = dst2 + skip; dst2 < climit; *dst2++ = *src++) {
            ;
          }
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。
- **触发前提：**多字节模式、非匹配跳过和增长替换组合，输出接近上限。
- **影响：**可写过按最大字符串长度分配的缓冲。
- **根因：**增长分支的 skip_table 复制推进 src/dst2，却不增加 dlen，也不检查目标剩余容量。后续检查使用偏小 dlen。
- **现有上下文（控制流摘要，非逐字代码引用）：**skip 只推进 dst2，后续用旧 dlen。
- **证据与确认边界：**跳表分支、分配容量和后续检查链确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**skip、替换、单字节及尾段共用实际已写长度，所有路径复制前检查剩余容量。 保持超限失败返回、弹栈和释放合同，不以写后检查代替。 覆盖 first/last 范围、连续跳过、缩短/增长及容量邻界。
- **验证方式：**参考结果逐字节一致，越界在复制前拒绝；ASan/清理检查全部路径。
- **关联源码/测试：**`src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/strings/replace_string.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-032)。

### AUD-033 · TLS 重试把负返回值当发送偏移

- **级别 / 类别 / 来源：**P1 / B C E H / 不确定。
- **审读视角 / 性质：**网络 / TLS / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/sockets/socket_efuns.cc:1145–1226。

**源码摘录：**`src/packages/sockets/socket_efuns.cc:1153–1166`

```cpp
  if (off > 0) {
    should_continue = true;
  } else if (off <= 0) {
    if (is_tls) {
      auto e = SSL_get_error(lpc_socks[fd].ssl, off);
      switch (e) {
        case SSL_ERROR_WANT_READ:
          debug(sockets, "socket_write: SSL_ERROR_WANT_READ.\n");
          should_continue = true;
          break;
        case SSL_ERROR_WANT_WRITE:
          debug(sockets, "socket_write: SSL_ERROR_WANT_WRITE.\n");
          should_continue = true;
          break;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。
- **触发前提：**TLS 初次非阻塞写需要重试。
- **影响：**后续可从缓冲起点之前读取，队列和网络统计错误；未证实代码执行。
- **根因：**SSL_write 的 WANT 等负返回被判可继续，off 却未归零，写入 w_off 与 len-off。
- **现有上下文（控制流摘要，非逐字代码引用）：**重试保留 off<0，随后 w_off=off。
- **证据与确认边界：**SSL 返回分类到偏移/长度的数据流确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**可重试错误归一为消费 0 字节；只有正返回值推进偏移和统计。 按 WANT_READ/WANT_WRITE 等待正确事件，遵守同一数据重试要求。 注入 WANT、EINTR、部分成功、致命错误并检查生产队列。
- **验证方式：**0<=w_off<=总长度、剩余长度正确；无前缀泄漏/重发/漏发，统计不负增。
- **关联源码/测试：**`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/sockets/socket_write.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-033)。

### AUD-034 · sscanf 的 %s%x 搜索越过终止符

- **级别 / 类别 / 来源：**P1 / B E / 不确定。
- **审读视角 / 性质：**efun / VM / 生命周期；产品或工具实现缺陷。
- **位置：**src/vm/internal/base/interpret.cc:5400–5414。

**源码摘录：**`src/vm/internal/base/interpret.cc:5402–5413`

```cpp
        case 'x':
          do {
            while (*tmp && (*tmp != '0')) {
              tmp++;
            }
            if (*tmp == '0') {
              if ((tmp[1] == 'x' || tmp[1] == 'X') && uisxdigit(tmp[2])) {
                break;
              }
              tmp += 2;
            }
          } while (*tmp);
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。
- **触发前提：**%s 后跟 %x，剩余输入以单个 0 结束。
- **影响：**越界读取和不可信匹配结果。
- **根因：**尾部单个 0 无 x 前缀时仍 tmp+=2，循环随后读取终止符之后的位置。
- **现有上下文（控制流摘要，非逐字代码引用）：**非前缀尾部 0 仍 tmp+=2 后解引用。
- **证据与确认边界：**短路条件和下一轮读取位置确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**后继字符存在才检查前缀；无前缀按仍在字符串内的步幅推进。 前缀完整且第三字节存在才检查该字节。 保留匹配计数、未匹配参数和其他转换语义。
- **验证方式：**空输入、尾部 0、完整/不完整前缀、合法十六进制和多转换；ASan 无越界。
- **关联源码/测试：**`src/vm/internal/base/interpret.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/strings/sscanf.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-034)。

### AUD-035 · TLS 延迟刷新前已释放并清空 SSL

- **级别 / 类别 / 来源：**P1 / B C E H / 不确定。
- **审读视角 / 性质：**网络 / TLS / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/sockets/socket_efuns.cc:2031–2074。

**源码摘录：**`src/packages/sockets/socket_efuns.cc:2051–2056`

```cpp
  if (lpc_socks[fd].ssl != nullptr) {
    SSL_shutdown(lpc_socks[fd].ssl);
    // TODO: process write
    SSL_free(lpc_socks[fd].ssl);
    lpc_socks[fd].ssl = nullptr;
  }
```

**源码摘录：**`src/packages/sockets/socket_efuns.cc:2067–2074`

```cpp
  /* if we're linkdead, we'll never flush, so don't even try :-) */
  if ((lpc_socks[fd].flags & S_BLOCKED) && !(lpc_socks[fd].flags & S_LINKDEAD)) {
    /* Can't close now; we still have data to write.  Tell the mudlib
     * it is closed, but we really finish up later.
     */
    lpc_socks[fd].state = STATE_FLUSHING;
    return EESUCCESS;
  }
```

- **依赖：**AUD-060。
- **修复风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。
- **触发前提：**TLS 队列有待发数据时普通关闭。
- **影响：**向 SSL_write 传入空 SSL，可能崩溃；不是 UAF，也不是 comm.cc interactive 清理。
- **根因：**socket_close 在判断 S_BLOCKED 延迟关闭前 SSL_free 并置空；刷新仍按 TLS 模式调用 SSL_write。
- **现有上下文（控制流摘要，非逐字代码引用）：**SSL_free → ssl=null → STATE_FLUSHING。
- **证据与确认边界：**直接清空时序确认；纠正 UAF 和 comm.cc 归因。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**先决定 flush/final；普通关闭需刷新时保留 SSL/CTX 到最终关闭。 最终路径一次释放 TLS/事件/队列，回调只通知一次并保留身份重查。 与 AUD-060 共核完整关闭状态机。
- **验证方式：**普通待发按合同完成后恰好释放一次；强制/致命关闭不续写、不传空 SSL。
- **关联源码/测试：**`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/sockets/socket_close.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-035)。

### AUD-036 · TLS accept 早失败遗漏事件资源

- **级别 / 类别 / 来源：**P1 / B C E H / 不确定。
- **审读视角 / 性质：**网络 / TLS / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/sockets/socket_efuns.cc:748–783。

**源码摘录：**`src/packages/sockets/socket_efuns.cc:770–776`

```cpp
    // If parent socket has TLS enabled, initialize SSL for accepted connection
    if (lpc_socks[i].flags & S_TLS_SUPPORT) {
      if (lpc_socks[fd].ssl_ctx == nullptr) {
        debug(sockets, "socket_accept: parent socket has no SSL_CTX.\n");
        evutil_closesocket(accept_fd);
        clear_socket(i, 1);
        return EEACCEPT;
```

**源码摘录：**`src/packages/sockets/socket_efuns.cc:748–750`

```cpp
  if (i >= 0) {
    new_lpc_socket_event_listener(i, &lpc_socks[i], accept_fd);
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。
- **触发前提：**接受 TLS 连接后初始化失败。
- **影响：**事件/上下文反复泄漏及旧事件关联；fd 已关闭，不能报 fd 泄漏。
- **根因：**事件和 ev_data 已分配，SSL_CTX 缺失/SSL_new 失败却只 close fd 再 clear_socket，资源指针被清掉。
- **现有上下文（控制流摘要，非逐字代码引用）：**close(accept_fd); clear_socket(i,1);。
- **证据与确认边界：**分配/关闭/清槽顺序确认；撤回 fd 泄漏说法。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**早失败汇入完整新槽撤销，释放事件/上下文再清槽，fd 只关闭一次。 不交付半初始化槽，不绕过身份/回调约束。 注入两处失败，推进夹具 event_base 后检查资源。
- **验证方式：**重复失败后事件/上下文/fd 回基线；槽复用不接受旧回调；正常 accept 不回归。
- **关联源码/测试：**`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/sockets/socket_accept.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-036)。

### AUD-037 · runner 拒绝合法百分号文件名

- **级别 / 类别 / 来源：**P2 / I J / fork 引入问题。
- **审读视角 / 性质：**测试工具 / CI / 证据；测试或验收证据缺口，不冒称产品缺陷。
- **位置：**tools/testsuite/run-targeted.py:746–759。

**源码摘录：**`tools/testsuite/run-targeted.py:746–749`

```python
def validate_case(case: str) -> str:
    if not re.fullmatch(r"/[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*", case):
        raise RunnerError(f"invalid LPC case path: {case!r}")
    return case
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**不能通过放宽路径边界、降低证据身份或静默跳过解决门禁失败；负向自测必须继续拒绝路径逃逸、零断言、字节被替换和异步未完成。
- **触发前提：**选择两个现有格式串回归。
- **影响：**用例不能通过定向入口执行，不是编译失败。
- **根因：**validate_case 字符白名单排除 %，拒绝已有 warn_%s 和 warn_%s_a，driver 尚未启动。
- **现有上下文（控制流摘要，非逐字代码引用）：**/[A-Za-z0-9_.-]+ 白名单。
- **证据与确认边界：**lpc-individual-040/041 invalid LPC case path，各约 0.22 秒，driver 未运行。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**按组件和根归属校验；拒绝 NUL/控制字符、反斜杠、空组件与 ./..，允许语料合法字符。 确认实际 .c/.lpc 文件位于隔离 mudlib，继续 argv 传参、不经 shell。 补现有 %/Unicode 文件名与穿越/符号链接越根负例，再重跑两项。
- **验证方式：**两项真实执行且身份一致；越根启动前拒绝；少跑/异步门禁不放宽。
- **关联源码/测试：**`tools/testsuite/run-targeted.py`、`tools/testsuite/test-run-targeted.py`。
- **文档归属：**`tools/testsuite/README.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-037)。

### AUD-038 · SQLite 异步回归未接完成屏障

- **级别 / 类别 / 来源：**P2 / E J / fork 引入问题。
- **审读视角 / 性质：**LPC / 测试合同；测试或验收证据缺口，不冒称产品缺陷。
- **位置：**testsuite/single/tests/efuns/async_db_exec_bound_args.lpc:17–44；testsuite/single/tests/efuns/db.c:93–108；testsuite/etc/test-scopes.tsv:10、36。

**源码摘录：**`testsuite/single/tests/efuns/async_db_exec_bound_args.lpc:31–41`

```cpp
  // A trailing bound arg must survive to the callback, delivered after the
  // primary result, and not read stale/garbage stack memory.
  async_db_exec(conn, "insert into async_bound_tbl values('x');",
    (: bound_arg_cb :), "hello-bound-arg");

  call_out(function() {
    ASSERT_EQ(1, got_string_cb_error);
    ASSERT_EQ(1, got_bound_arg);
    db_close(conn);
  }, 1);
#else
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**分类和完成协议会改变统计；能力必须来自被测 driver，排除项单列且不计通过，真实异步回调完成前不能归还 token。
- **触发前提：**async/db/sqlite 配置运行两项。
- **影响：**回归覆盖和完成时序失真；单例超时后端根因未另证，不能猜成 DB 锁。
- **根因：**两项虽为 runtime，却未登记异步 token。do_tests 返回即完成；全量报 assertion after case completion，单例先结束后约 183 秒超时。
- **现有上下文（控制流摘要，非逐字代码引用）：**do_tests 返回时没有登记未完成 token。
- **证据与确认边界：**三配置全量都明确报该用例完成后断言；单例仅 2/18 断言先结束，各约 183 秒失败。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**保持现有 runtime 角色及已正确声明的 async/db/sqlite 能力依赖，不改分类/门禁。 在 do_tests 返回前调用 /command/tests->begin_async() 并保存 token；最后一个回调完成断言及 db_close 后调用 complete_async(token)。 用真实 DB 回调推进，替代固定延时；保留 bound args 与拒绝 string callback 的既有回归。
- **验证方式：**完成标记晚于最终回调，两项不超时；禁止回调/故意失败必须失败；仍挂起则另追 shutdown。
- **关联源码/测试：**`testsuite/single/tests/efuns/async_db_exec_bound_args.lpc`、`testsuite/single/tests/efuns/db.c`。
- **文档归属：**`tools/testsuite/README.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-038)。

### AUD-039 · 无执行权限对象可占满外部句柄池

- **级别 / 类别 / 来源：**P1 / E F H N P / fork 引入问题。
- **审读视角 / 性质：**外部进程 / 权限 / 资源；全局资源准入缺口；修复涉及原有非特权分配政策。
- **位置：**src/packages/external/external.cc:152–166、1340–1357、1363–1388。

**源码摘录：**`src/packages/external/external.cc:1340–1352`

```cpp
  svalue_t* arg = sp - num_arg + 1;

  /* Allocation is not privileged; valid_socket is checked at run. */
  int const cmd = validate_cmd_index(arg[0].u.number);
  std::vector<std::string> extra;
  parse_cmd_args(arg + 1, &extra);

  int const id = alloc_handle_id();
  auto* h = new ExternalHandle{};
  h->id = id;
  h->owner = current_object;
  h->owner_id = vm_owner_id(current_object);
  h->owner_epoch = vm_owner_epoch(current_object);
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**create 鉴权将收紧源码明确允许的非特权分配政策，须随方案批准；复用已有权限操作并验证回调重入/销毁与 arity 恢复，不改变 run 时检查。
- **触发前提：**可运行 LPC 但无 external 权限的对象重复 create，不 run/close。
- **影响：**其他对象无法创建；不等于绕过权限启动进程。
- **根因：**external_create 分配全局持久配额，权限仅在 external_run 检查；被拒绝执行的对象仍可长期占满槽。
- **现有上下文（控制流摘要，非逐字代码引用）：**alloc_handle_id 先于权限检查。
- **证据与确认边界：**配额与权限入口确认；此项为待批准的权限语义调整。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**复用 external_run/start 的 check_valid_socket("external", -1, current_object, "N/A", -1)，在 external_create 占用槽位以及建立参数 vector 前检查；不新增 efun、clone 入口或 master 操作名。 先保存 st_num_arg，权限回调返回后恢复；拒绝时抛既有风格的工厂调用错误，不预留槽。回调可销毁调用对象，授权后必须重新验证 owner 的存活与 epoch，不能沿用回调前的裸状态。 保留 kMaxHandles 上界与 run 时再次鉴权。测试拒绝对象重复申请、允许对象达到上限、释放后再申请，以及权限回调抛错/重入/销毁；此项改变原先明确允许的非特权分配政策，须随方案批准。
- **验证方式：**拒绝对象重复 create 不影响授权对象；回调重入/销毁安全；创建后撤权仍不能 run。
- **关联源码/测试：**`src/packages/external/external.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/external/external_create.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-039)。

### AUD-040 · external_write 截断 stdin 却返回成功

- **级别 / 类别 / 来源：**P2 / E P / fork 引入问题。
- **审读视角 / 性质：**外部进程 / 权限 / 资源；产品或工具实现缺陷。
- **位置：**src/packages/external/external.cc:124–135、679–687、1524–1533。

**源码摘录：**`src/packages/external/external.cc:1525–1533`

```cpp
void f_external_write() {
  int const num_arg = st_num_arg;
  svalue_t* arg = sp - num_arg + 1;
  int const id = static_cast<int>(arg[0].u.number);
  ExternalHandle* h = lookup_handle(id, /*require_owner=*/1);
  int const ok = queue_stdin(h, id, arg[1].u.string, SVALUE_STRLEN(arg + 1));
  pop_n_elems(num_arg);
  push_number(ok);
}
```

**源码摘录：**`src/packages/external/external.cc:679–698`

```cpp
int queue_stdin(ExternalHandle* h, int id, const char* data, size_t len) {
  if (h->in_closed || h->close_stdin_after_flush || h->state == HandleState::Done) {
    return 0;
  }
  append_capped(h->in_buf, data, len);
  if (h->state == HandleState::Running) {
    flush_stdin(h, id);
  }
  return 1;
}

#ifndef _WIN32
/* Shared by the classic callback form and the handle/promise form.
 * POSIX_SPAWN_USEVFORK (glibc) is the fast path: vfork/clone instead of
 * a full fork of the driver address space. Win32 has no posix_spawn;
 * see win32_create_process(). */
int posix_spawn_fast(pid_t* pid, const char* path, const posix_spawn_file_actions_t* file_actions,
                     char** argv, char** envp) {
  posix_spawnattr_t attr;
  if (posix_spawnattr_init(&attr) != 0) {
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**保留 boolean 返回值；超限整次拒绝且队列不变，不改成字节数、不改变 stdout/stderr 截断策略。
- **触发前提：**待发 stdin 加新输入超过上限。
- **影响：**子进程收到残缺输入，LPC 却得到成功。
- **根因：**queue_stdin 复用 append_capped 静默丢超容量字节，随后仍返回 1。
- **现有上下文（控制流摘要，非逐字代码引用）：**append_capped(...); return 1;。
- **证据与确认边界：**queue_stdin 唯一调用者及读写共用截断 helper 确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**以当前 h->in_buf.size() 和 __MAX_STRING_LENGTH__ 计算剩余容量；flush_stdin 已通过 erase 移除已消费前缀，不另设冗余游标。 在追加/flush 改变状态前判断整次输入能否容纳；超限直接返回 0 且队列不变，能容纳才完整追加。保留 boolean 合同 1=本次全部接收、0=未接收，不改成字节数，也不改变 stdout/stderr 的截断策略。 保留 Created/Running、EOF/close、空输入行为；用有界管道模拟部分写与 EAGAIN，验证满额拒绝、随后重试、逐字节完整和清理。
- **验证方式：**超限返回 0 且逻辑队列不变；成功字节完全一致，无丢弃/重复。
- **关联源码/测试：**`src/packages/external/external.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/external/external_write.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-040)。

### AUD-041 · 非 Debug 零断言用例未预分类

- **级别 / 类别 / 来源：**P2 / I J / fork 引入问题。
- **审读视角 / 性质：**LPC / 测试合同；测试或验收证据缺口，不冒称产品缺陷。
- **位置：**testsuite/single/tests/compiler/constant_expr.c:1–39；testsuite/etc/test-scopes.tsv；.github/workflows/ci.yml。

**源码摘录：**`testsuite/single/tests/compiler/constant_expr.c:1–8`

```cpp
void do_tests() {
  mixed code;

#ifdef __DEBUG__
  // array constants
  code = __TREE__ (sizeof(({ 1, 3, 3 })));
  ASSERT(same(code, ({ "number", 3 })));
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**分类和完成协议会改变统计；能力必须来自被测 driver，排除项单列且不计通过，真实异步回调完成前不能归还 token。
- **触发前提：**Release/RelWithDebInfo LPC 测试。
- **影响：**CI 分类缺口，不是缺语言模块；不能改门禁伪通过。
- **根因：**constant_expr 的断言全受 __DEBUG__ 保护，优化配置没有对应的预执行能力排除，严格门禁因此拒绝零断言。
- **现有上下文（控制流摘要，非逐字代码引用）：**条件体 + 默认 runtime。
- **证据与确认边界：**CI 原始 zero-assert runtime test: constant_expr.c 与源码条件一致。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**既有能力清单增加准确 debug 条件，为该测试声明依赖，不加假断言。 非 Debug 预列配置不支持并保留原因；Debug 必须真实执行。 同步能力生产者、分类校验、自测和 discovered/executed 对账。
- **验证方式：**Debug 有实际断言；优化配置预排除；其他运行用例故意零断言仍失败。
- **关联源码/测试：**`testsuite/etc/test-scopes.tsv`、`testsuite/command/tests.c`、`tools/testsuite/run-targeted.py`、`tools/testsuite/test-run-targeted.py`。
- **文档归属：**`tools/testsuite/README.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-041)。

### AUD-042 · CI 定向失败详细日志未持久归档

- **级别 / 类别 / 来源：**P2 / I J K O / fork 引入问题。
- **审读视角 / 性质：**测试工具 / CI / 证据；测试或验收证据缺口，不冒称产品缺陷。
- **位置：**.github/workflows/ci.yml；tools/testsuite/run-targeted.py:1000–1039；src/tests/CMakeLists.txt。

**源码摘录：**`tools/testsuite/run-targeted.py:647–651`

```python
    run_count, passed, failed, skipped = gtest_result_counts(result.output)
    if result.timed_out:
        raise RunnerError(f"GTest execution timed out after {timeout}s; inspect {result.log_path}")
    if result.returncode != 0 or run_count == 0 or passed + failed + skipped == 0:
        raise RunnerError(f"GTest execution failed; inspect {result.log_path}")
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**不能通过放宽路径边界、降低证据身份或静默跳过解决门禁失败；负向自测必须继续拒绝路径逃逸、零断言、字节被替换和异步未完成。
- **触发前提：**CI CTest/runner 子调用失败。
- **影响：**根因核验被迫另行同型复跑；日志缺失不代表产品有错。
- **根因：**嵌套 runner 只打印 /tmp 证据路径，CI 未归档该目录完整失败日志；file_io 的具体断言无法从现有 CI 恢复。
- **现有上下文（控制流摘要，非逐字代码引用）：**只剩临时路径，原日志未上传。
- **证据与确认边界：**CI run 38214721157 GCC RelWithDebInfo file_io 只剩临时路径；audit44 复跑得到两项具体失败。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**每次 runner/嵌套 CTest 显式使用工作区下唯一 evidence-dir。 always 上传受控 summary、有界完整日志和身份元数据；不上传凭据、环境或任意 /tmp。 保留失败码，用受控失败确认 artifact 可定位具体断言。
- **验证方式：**已知失败可从 artifact 恢复断言与身份，job 仍失败；成功/中断可追踪且敏感文件不归档。
- **关联源码/测试：**`.github/workflows/ci.yml`、`src/tests/CMakeLists.txt`、`tools/testsuite/run-targeted.py`、`tools/testsuite/test-run-targeted.py`。
- **文档归属：**`tools/testsuite/README.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-042)。

### AUD-043 · 空输入尾裁剪形成起点之前的指针

- **级别 / 类别 / 来源：**P2 / B D / 不确定。
- **审读视角 / 性质：**LPC 编译器 / C++；产品或工具实现缺陷。
- **位置：**src/compiler/internal/lex.cc:handle_pragma；src/packages/core/add_action.cc:325–333。

**源码摘录：**`src/compiler/internal/lex.cc:1723–1727`

```cpp
  // Ignore trailing whitespaces
  char* p = &str[strlen(str) - 1];
  while (iswspace(*p)) {
    *p-- = '\0';
  }
```

**源码摘录：**`src/packages/core/add_action.cc:325–333`

```cpp
  /* strip trailing spaces. */
  for (p = buff + strlen(buff) - 1; p >= buff; p--) {
    if (*p != ' ') {
      break;
    }
    *p = '\0';
  }
  if (buff[0] == '\0') {
    return 0;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。
- **触发前提：**空 pragma、空命令或全空格命令。
- **影响：**C++ 指针形成/比较超界 UB；未证明当前构建会崩溃。
- **根因：**handle_pragma 的 strlen-1 与 user_parser 初始/末轮指针可越过起点。后者 p>=buff 会阻止解引用，不能报成已证实前读。
- **现有上下文（控制流摘要，非逐字代码引用）：**start + strlen(start) - 1。
- **证据与确认边界：**循环保护条件确认，限定为指针 UB，不夸大物理越界读取。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**两处用非负长度索引，仅 length>0 才访问 length-1。 保留 pragma 诊断和空命令返回 0，不扩大空白定义。 覆盖空串、全空格、单字符、普通尾空格和参数修改边界。
- **验证方式：**正常结果/诊断不变；边界夹具确认无非法指针；不伪造 sanitizer 可观察性。
- **关联源码/测试：**`src/compiler/internal/lex.cc`、`src/packages/core/add_action.cc`、`src/tests/test_lpc_compiler.cc`。
- **文档归属：**`docs/efun/interactive/command.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-043)。

### AUD-044 · FORTIFY 绕过 fprintf 故障注入

- **级别 / 类别 / 来源：**P2 / I J / fork 引入问题。
- **审读视角 / 性质：**文件 IO / 错误传播；测试或验收证据缺口，不冒称产品缺陷。
- **位置：**src/tests/test_file_io.cc:308、602–642；src/symbol.cc；src/packages/mudlib_stats/mudlib_stats.cc:532；src/tests/CMakeLists.txt。

**源码摘录：**`src/tests/test_file_io.cc:308–317`

```cpp
int __wrap_fprintf(FILE* stream, const char* format, ...) {
  if (stream == fault.stream && reject_write(FaultPoint::kWrite)) {
    return -1;
  }
  va_list arguments;
  va_start(arguments, format);
  const int result = vfprintf(stream, format, arguments);
  va_end(arguments);
  return result;
}
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**不要伪造成功或丢掉首个系统错误；只操作夹具目录，保留权限检查及既有成功/失败返回值，覆盖 EOF、EIO、消失路径和跨设备失败。
- **触发前提：**GCC RelWithDebInfo、FORTIFY 启用。
- **影响：**两个写失败回归失效并导致 CI 失败；不是已证实的产品输出丢失。
- **根因：**GCC 优化/FORTIFY 把两处 fprintf 降成 __fprintf_chk，测试仅 wrap fprintf，故障钩子未执行。
- **现有上下文（控制流摘要，非逐字代码引用）：**仅 --wrap=fprintf / __wrap_fprintf。
- **证据与确认边界：**audit44：96 项中 94 通过、2 失败，fault.hits=0；禁止关闭 FORTIFY 或放宽断言。 隔离 GCC RelWithDebInfo 的原始记录见 §10；不得关闭 FORTIFY 使测试通过。
- **修复方向：**保留 FORTIFY，在同一故障状态机接入 __fprintf_chk 包装；正常转发保留 flag/varargs。 CMake 仅在平台/编译器支持该符号时启用对应 wrap。 继续要求 hits>=1，验证正常写和两个 printf 变体的故障上报。
- **验证方式：**同型优化配置 96/96 且钩子命中；Debug 不回归，正常 fortified 输出不被屏蔽。
- **关联源码/测试：**`src/tests/test_file_io.cc`、`src/tests/CMakeLists.txt`。
- **文档归属：**`tools/testsuite/README.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-044)。

### AUD-045 · member_array 下标窄化及逆向失败哨兵错误

- **级别 / 类别 / 来源：**P2 / A E / 不确定。
- **审读视角 / 性质：**efun / VM / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/core/efuns_main.cc:1092–1108、1243–1253。

**源码摘录：**`src/packages/core/efuns_main.cc:1105–1108`

```cpp
    i = (sp--)->u.number;
    if (i < 0) {
      bad_arg(3, F_MEMBER_ARRAY);
    }
```

**源码摘录：**`src/packages/core/efuns_main.cc:1119–1120`

```cpp
    if (i > SVALUE_STRLEN(sp)) {
      error("Index to start search from in member_array() is > string length.\n");
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。
- **触发前提：**大整数起点，或逆向空数组/未匹配/谓词全假。
- **影响：**返回错误位置，甚至把未找到报告成非负下标。
- **根因：**起点先从 LPC_INT 窄化为 int 再校验；逆向搜索把未找到的 -1 再映射成 size。
- **现有上下文（控制流摘要，非逐字代码引用）：**i=(int)start；i=-1 后仍 size-i-1。
- **证据与确认边界：**起点转换和退出后无条件映射均已确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**在 LPC_INT 中按容器规则验证起点，再作有证明的窄化；保留字符串越界报错与数组未找到语义。 只有命中时才进行反向下标映射，-1 始终保留。 覆盖普通、前缀及谓词组合和极值起点。
- **验证方式：**正反向未找到均 -1；实际命中下标正确；大起点不回绕成有效下标；负起点仍拒绝。
- **关联源码/测试：**`src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/arrays/member_array.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-045)。

### AUD-046 · 原生骰子与编辑距离循环不响应预算

- **级别 / 类别 / 来源：**P1 / E F G / 不确定。
- **审读视角 / 性质：**efun / VM / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/contrib/contrib.cc:2835–2853、2874–2910；src/vm/internal/base/interpret.cc:2478–2484。

**源码摘录：**`src/packages/contrib/contrib.cc:2844–2849`

```cpp
  if (rolls > 0 && sides > 0) {
    while (rolls--) {
      roll += 1 + random_number(sides);
    }
    roll += bonus;
  }
```

- **依赖：**AUD-006。
- **修复风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。
- **触发前提：**合法但成本高的参数，原生循环期间预算耗尽。
- **影响：**事件循环被长时间占用；不把 WSL 结果推广为各平台计时器验收。
- **根因：**roll_MdN 的 O(rolls) 与 string_difference 的 O(mn) 循环不轮询 outoftime，解释器只在回到 opcode 分派后处理超时。
- **现有上下文（控制流摘要，非逐字代码引用）：**原生循环无预算轮询。
- **证据与确认边界：**循环与解释器超时路径直接比对；跨平台部分仍受 AUD-006 约束。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**把解释器现有超时处理提为同一内部检查函数，保留 max_eval_error/catch 不能吞掉超时的合同。 两循环以固定小批量轮询，预处理/初始化长循环也检查；不修改结果算法。 编辑距离工作表改为异常安全所有权，使预算 error 释放临时内存；接入 AUD-006 的平台计时合同。
- **验证方式：**正常结果一致；注入耗尽可预测中止且释放内存；真实隔离 driver 的有界输入验证响应时间，无负载外推。
- **关联源码/测试：**`src/packages/contrib/contrib.cc`、`src/vm/internal/base/interpret.cc`、`src/vm/internal/base/interpret.h`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/contrib/roll_MdN.md`、`docs/efun/contrib/string_difference.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-046)。

### AUD-047 · get_dir 两遍扫描遇提前 EOF 解引用空目录项

- **级别 / 类别 / 来源：**P1 / B E H / 不确定。
- **审读视角 / 性质：**文件 IO / 错误传播；产品或工具实现缺陷。
- **位置：**src/packages/core/file.cc:311–328、345–384。

**源码摘录：**`src/packages/core/file.cc:345–352`

```cpp
  for (i = 0, de = readdir(dirp); i < count; de = readdir(dirp)) {
    namelen = strlen(de->d_name);
    if (!do_match && (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)) {
      continue;
    }
    if (do_match && !match_string(regexppath, de->d_name)) {
      continue;
    }
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**不要伪造成功或丢掉首个系统错误；只操作夹具目录，保留权限检查及既有成功/失败返回值，覆盖 EOF、EIO、消失路径和跨设备失败。
- **触发前提：**两遍之间项目消失、匹配集合缩小或 readdir 错误。
- **影响：**空指针解引用；第一遍也未区分 EOF 与错误。
- **根因：**第一遍计数后 rewinddir；第二遍只判断 i<count，不判断 readdir 是否为空。目录变化或读取错误可使其提前结束。
- **现有上下文（控制流摘要，非逐字代码引用）：**for(...; i<count; de=readdir(...)) 直接用 de。
- **证据与确认边界：**两轮循环条件确认；不需假定特定文件系统。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**每次 readdir 前清 errno，返回空后先区分正常 EOF 与错误，任何 d_name 访问前检查目录项。 正常提前 EOF 按实际填充 i 收缩数组并仅排序有效项；错误释放数组/关闭目录并返回既有失败值。 保留最大数组限制、通配符与排序；不用第一遍计数证明第二遍一定有项目。
- **验证方式：**确定性注入提前 EOF/错误及匹配变化；无空槽/空解引用/泄漏，正常排序与限制不变。
- **关联源码/测试：**`src/packages/core/file.cc`、`src/tests/test_file_io.cc`。
- **文档归属：**`docs/efun/filesystem/get_dir.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-047)。

### AUD-048 · get_dir 忽略元数据读取失败

- **级别 / 类别 / 来源：**P2 / B E K / 不确定。
- **审读视角 / 性质：**文件 IO / 错误传播；产品或工具实现缺陷。
- **位置：**src/packages/core/file.cc:354–375。

**源码摘录：**`src/packages/core/file.cc:361–369`

```cpp
      size_t const avail = sizeof(temppath) - (size_t)(endtemp - temppath);
      if (namelen < avail) {
        memcpy(endtemp, de->d_name, namelen + 1);
        large_file_stat(temppath, &st); /* We assume it works. */
      } else {
        memset(&st, 0, sizeof(st));
      }
    }
    encode_stat(&v->item[i], flags, de->d_name, &st);
```

- **依赖：**AUD-047。
- **修复风险与缓解：**不要伪造成功或丢掉首个系统错误；只操作夹具目录，保留权限检查及既有成功/失败返回值，覆盖 EOF、EIO、消失路径和跨设备失败。
- **触发前提：**目录项消失、权限/IO 错误，或组合路径无法容纳。
- **影响：**输出看似有效但属于其他对象或虚构的大小/时间。
- **根因：**逐项 large_file_stat 返回值被忽略，失败后 encode_stat 使用之前目录/文件的 st；过长路径还伪造全零元数据。
- **现有上下文（控制流摘要，非逐字代码引用）：**large_file_stat(...); // assume it works。
- **证据与确认边界：**st 在两轮和不同项目之间复用，错误与过长路径分支已核。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**metadata 模式检查 stat 成功，不复用旧 st；路径不容纳也明确失败。 失败释放中间结果、关闭目录，按 get_dir 已有失败约定返回 0，不造元数据哨兵。 保持成功路径的 file_size/时间定义，文档说明元数据失败不返回部分伪结果。
- **验证方式：**第二项 stat 失败不会得到第一项元数据；不泄漏、不返回伪零；正常/目录/长路径边界覆盖。
- **关联源码/测试：**`src/packages/core/file.cc`、`src/tests/test_file_io.cc`。
- **文档归属：**`docs/efun/filesystem/get_dir.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-048)。

### AUD-049 · link 的 EXDEV 符号链接目标与返回值错误

- **级别 / 类别 / 来源：**P2 / E I / 不确定。
- **审读视角 / 性质：**文件 IO / 错误传播；产品或工具实现缺陷。
- **位置：**src/packages/core/file.cc:895–905、938–946。

**源码摘录：**`src/packages/core/file.cc:938–945`

```cpp
#ifdef F_LINK
  else if (flag == F_LINK) {
    if (symlink(from, to) == 0) { /* symbolic link */
      return 0;
    }
  }
#endif
  return 0;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**不要伪造成功或丢掉首个系统错误；只操作夹具目录，保留权限检查及既有成功/失败返回值，覆盖 EOF、EIO、消失路径和跨设备失败。
- **触发前提：**跨设备 link，目标在另一目录，或 symlink 本身失败。
- **影响：**返回成功但链接损坏，或根本没有创建。不是复制回退。
- **根因：**回退 symlink(from,to) 直接使用相对当前目录的 from，实际会相对目标目录解释；symlink 失败仍落到 return 0。
- **现有上下文（控制流摘要，非逐字代码引用）：**symlink(from,to)；失败仍 return 0。
- **证据与确认边界：**硬链接失败分支及函数末尾返回直接确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**仅 EXDEV 分支按目标父目录计算词法相对源目标，不改变权限回调原始输入和次数，不 canonicalize 跟随源链接。 symlink 失败走既有错误/失败合同，不能落到成功返回。 保留正常硬链接与目标存在处理；使用私有夹具注入 EXDEV。 F_LINK 的 symlink 失败分支返回 1，不能落到函数末尾的成功值 0；F_RENAME 分支不改。
- **验证方式：**回退目标实际可读且与源一致；失败不成功；权限拒绝无副作用，既有目标不被覆盖。
- **关联源码/测试：**`src/packages/core/file.cc`、`src/tests/test_file_io.cc`。
- **文档归属：**`docs/efun/filesystem/link.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-049)。

### AUD-050 · sprintf 数组 %O 转换只作用于首元素

- **级别 / 类别 / 来源：**P2 / E / 不确定。
- **审读视角 / 性质：**efun / VM / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/core/sprintf.cc:1075–1103、数组迭代尾部。

**源码摘录：**`src/packages/core/sprintf.cc:1075–1086`

```cpp
      if (finfo & INFO_ARRAY) {
        if (carg->type != T_ARRAY) {
          SPRINTF_ERROR(ERR_ARRAY_EXPECTED);
        }
        if (carg->u.arr->size == 0) {
          last = fpos;
          fpos--; /* 'bout to get incremented */
          continue;
        }
        carg = (argv + sprintf_state->cur_arg)->u.arr->item;
        nelemno = 1; /* next element number */
      }
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。
- **触发前提：**%@O 处理多个元素。
- **影响：**合法格式输出错误或抛错。
- **根因：**首元素把 finfo 从 INFO_T_LPC 改成 INFO_T_STRING，后续迭代不恢复原转换；后面的数值可报字符串类型错，字符串失去 %O 表示。
- **现有上下文（控制流摘要，非逐字代码引用）：**首元素永久改写 finfo 类型。
- **证据与确认边界：**数组入口、%O 改写及后续类型判断确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**保留原转换类型，每个元素重新执行 %O 转换，其他格式标志不丢。 每轮沿用已有 clean 释放，下一轮恢复原转换类型；保持列/表其他 flags 和缓冲寿命，不引入新的临时池。 比对逐元素 sprintf("%O") 拼接，并验证宽度、精度、列/表及错误后的状态。
- **验证方式：**空/单/多元素、混合类型和嵌套值结果正确；ASan/引用检查无泄漏，后续调用不受影响。
- **关联源码/测试：**`src/packages/core/sprintf.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/strings/sprintf.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-050)。

### AUD-051 · parse_command 默认介词表可为空

- **级别 / 类别 / 来源：**P1 / B E / 不确定。
- **审读视角 / 性质：**LPC parser / 缓存与生命周期；产品或工具实现缺陷。
- **位置：**src/packages/ops/parse.cc:1154–1167。

**源码摘录：**`src/packages/ops/parse.cc:1154–1167`

```cpp
static svalue_t* prepos_parse(array_t* warr, int* cix_in, int* fail, svalue_t* prepos) {
  array_t *parr, *tarr;
  const char* tmp;
  int pix, tix;

  if ((!prepos) || (prepos->type != T_ARRAY)) {
    parr = gPrepos_list;
  } else {
    parr = prepos->u.arr;
  }

  for (pix = 0; pix < parr->size; pix++) {
    if (parr->item[pix].type != T_STRING) {
      continue;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。
- **触发前提：**master 未提供有效默认介词数组而解析 %p。
- **影响：**合法的缺省/未配置场景可崩溃。
- **根因：**未传有效显式表时使用可空 gPrepos_list，随即访问 parr->size。
- **现有上下文（控制流摘要，非逐字代码引用）：**parr=gPrepos_list; for(...parr->size)。
- **证据与确认边界：**可空默认表与无保护访问直接闭环。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**介词表为空时按未匹配返回，设置 fail 和返回槽；不制造新默认词表。 空数组、含非字符串项和显式数组保持现有选择及交换规则。 验证 master 缺 apply/错误类型和后续可用调用。
- **验证方式：**缺省缺表返回未匹配不崩溃；有效单词/多词表正常。
- **关联源码/测试：**`src/packages/ops/parse.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/parsing/parse_command.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-051)。

### AUD-052 · living_parse 压缩候选却复用原下标缓存

- **级别 / 类别 / 来源：**P2 / B E H / 不确定。
- **审读视角 / 性质：**LPC parser / 缓存与生命周期；产品或工具实现缺陷。
- **位置：**src/packages/ops/parse.cc:1050–1083；load_lpc_info/match_object。

**源码摘录：**`src/packages/ops/parse.cc:1069–1075`

```cpp
    if (obarr->item[obix].u.ob->flags & O_ENABLE_COMMANDS) {
      assign_svalue_no_free(&live->item[tix++], &obarr->item[obix]);
    }
  }

  if (tix) {
    pval = item_parse(live, warr, cix_in, fail);
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。
- **触发前提：**原数组混有非 living/非对象项，且前序格式项已预热缓存。
- **影响：**错误匹配、漏匹配或把另一个对象的 ID 用到 living 对象。
- **根因：**live 按 tix 压缩排列，gId_list/gPluid_list/gAdjid_list 仍按原 obarr 下标缓存；item_parse 用压缩下标查这些缓存。
- **现有上下文（控制流摘要，非逐字代码引用）：**live[tix++]=obarr[obix]，缓存却按 obix。
- **证据与确认边界：**压缩代码和全局缓存下标语义已核；不是已修复的非对象解引用。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**living 临时数组用 allocate_array(obarr->size) 初始化为零，保留原数组长度。 仅把 living 对象复制到原 obix 槽位，不再压紧到 tix，也不缩短数组；复用 item_parse 跳过非对象的既有分支。 gId_list/gPluid_list/gAdjid_list 仍以原 obix 查找；对象引用计数按既有 assign/free 合同配对，不引入第二套缓存。
- **验证方式：**冷/热缓存一致，混合数组不串对象；destruct 和非对象仍安全。
- **关联源码/测试：**`src/packages/ops/parse.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/parsing/parse_command.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-052)。

### AUD-053 · 多词 ID 游标在调用者处多推进一词

- **级别 / 类别 / 来源：**P2 / B E / 不确定。
- **审读视角 / 性质：**LPC parser / 缓存与生命周期；产品或工具实现缺陷。
- **位置：**src/packages/ops/parse.cc:1119–1131、1294–1347。

**源码摘录：**`src/packages/ops/parse.cc:1128–1132`

```cpp
    plur_flag = 0;
    if (match_object(obix, warr, &cix, &plur_flag)) {
      *cix_in = cix + 1;
      assign_svalue_no_free(&parse_ret, &obarr->item[obix]);
      return &parse_ret;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。
- **触发前提：**多词 ID 后还有格式字面量或参数。
- **影响：**跳过后一词，导致错误匹配/失败。
- **根因：**find_string 单词命中留下当前词，多词命中却留下词后位置；single_parse/item_parse 都再加一。
- **现有上下文（控制流摘要，非逐字代码引用）：**多词返回词后；调用者又 +1。
- **证据与确认边界：**find_string 两分支与调用者加一直接比对，修正了旧的相反归因。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**find_string 多词匹配成功后仅把输出参数 *cix_in 退回最后消费词；返回值 fpos 仍是匹配起点。 split 为空或零词时走不匹配分支；调用者继续按既有约定递增词游标。 核对 item_parse、adjectiv_parse、prepos_parse 对起点返回值与输出词游标的不同用法。
- **验证方式：**只消费实际匹配词数；多词后下一词保留，单词行为不变。
- **关联源码/测试：**`src/packages/ops/parse.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/parsing/parse_command.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-053)。

### AUD-054 · living_parse 回退成功仍保留失败标志

- **级别 / 类别 / 来源：**P2 / E / 不确定。
- **审读视角 / 性质：**LPC parser / 缓存与生命周期；产品或工具实现缺陷。
- **位置：**src/packages/ops/parse.cc:1075–1105。

**源码摘录：**`src/packages/ops/parse.cc:1097–1105`

```cpp
  if (ob) {
    parse_ret.type = T_OBJECT;
    parse_ret.u.ob = ob;
    add_ref(ob, "living_parse");
    (*cix_in)++;
    return &parse_ret;
  }
  *fail = 1;
  return nullptr;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。
- **触发前提：**候选 living 匹配失败，按名称回退成功。
- **影响：**调用者丢弃正确回退结果。
- **根因：**item_parse 失败置 fail=1；find_player/find_living 回退找到对象后返回值和游标成功，但未清 fail。
- **现有上下文（控制流摘要，非逐字代码引用）：**回退 return 成功对象，但 fail 仍为 1。
- **证据与确认边界：**item_parse 的失败状态与两个回退返回值闭环。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**仅在回退实际找到并持有对象后设置 fail=0，再提交返回值及游标。 保留两种查找顺序和对象引用，不在未找到时清失败。 覆盖候选为空/非空、成功/失败以及相邻格式项。
- **验证方式：**回退成功能继续匹配，失败仍失败；引用和游标无泄漏/多推进。
- **关联源码/测试：**`src/packages/ops/parse.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/parsing/parse_command.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-054)。

### AUD-055 · SQLite 执行失败没有更新错误文本

- **级别 / 类别 / 来源：**P2 / E K / 不确定。
- **审读视角 / 性质：**数据库 / 错误合同；产品或工具实现缺陷。
- **位置：**src/packages/db/db.cc:1028–1053。

**源码摘录：**`src/packages/db/db.cc:1047–1053`

```cpp
  if (c->SQLite3.results) {
    if (sqlite3_get_table(c->SQLite3.handle, s, &result, &c->SQLite3.nrows, &c->SQLite3.ncolumns,
                          NULL) != SQLITE_OK) {
      sqlite3_free_table(result);
      sqlite3_finalize(c->SQLite3.results);
      c->SQLite3.results = 0;
      return 0;
```

- **依赖：**AUD-038。
- **修复风险与缓解：**错误文本与句柄资源必须对应同一次操作；保留现有错误类型，不能把其他连接的诊断或旧 statement 当作当前结果。
- **触发前提：**prepare 成功但执行失败，例如受控约束失败。
- **影响：**调用方收到错误/陈旧诊断，妨碍处理和定位。
- **根因：**prepare 失败会写 errormsg，sqlite3_get_table 执行失败却只清结果并返回 0，错误缓存为空或仍是上次错误。
- **现有上下文（控制流摘要，非逐字代码引用）：**get_table 失败只 finalize/return。
- **证据与确认边界：**两种失败分支对 errormsg 的处理差异确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**执行失败时在清理前复制 sqlite3_errmsg 到同一有界错误缓存并终止。 清理 results 与状态一次，成功/下一次失败不能复用无关旧错误。 不改变同步/异步结果协议或重复执行策略。
- **验证方式：**实际约束失败有相应非空诊断，不带上次错误；句柄随后可正常执行，资源回基线。
- **关联源码/测试：**`src/packages/db/db.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/db/db_exec.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-055)。

### AUD-056 · query_multiple_short 缺回退对象时漏掉结果缓冲

- **级别 / 类别 / 来源：**P2 / B E H / 不确定。
- **审读视角 / 性质：**dwlib / 值语义；产品或工具实现缺陷。
- **位置：**src/packages/dwlib/dwlib.cc:184–229。

**源码摘录：**`src/packages/dwlib/dwlib.cc:208–227`

```cpp
    } else {
      /* We need to find /global/player. */
      /* Does this work? Seems not to. */
      ob = find_object("/global/player");

      if (ob) {
        push_malloced_string(res);
        v = apply("convert_message", ob, 1, ORIGIN_EFUN);

        /* Return the result! */
        if (v && v->type == T_STRING) {
          pop_n_elems(num_arg);
          share_and_push_string(v->u.string);
        } else {
          pop_n_elems(num_arg);
          push_undefined();
        }
      } else {
        pop_n_elems(num_arg);
        push_undefined();
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**格式化及替换不能更改调用者共享值或泄漏部分结果；保留输出语义，异常时清理，测试重复调用与别名输入。
- **触发前提：**可选 dwlib 包、该回退对象不存在。
- **影响：**重复调用泄漏原生字符串；不推断其他回调路径已经安全。
- **根因：**res 已分配；no_dollars 下既无 command_giver 又无 /global/player 的分支 pop 参数并 push_undefined，却未交出或释放 res。
- **现有上下文（控制流摘要，非逐字代码引用）：**缺回退分支丢失 res。
- **证据与确认边界：**分配点、两次移交与缺失释放分支已核；归属文档需新建。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**让 res 在创建后由同一 RAII/作用域守卫持有，只有 push_malloced_string 真正移交时释放守卫。 缺回退对象与异常路径都释放；保留返回 undefined 和转换回调顺序。 覆盖有/无 command_giver、回退存在/缺失及回调错误。
- **验证方式：**缺回退重复调用引用/分配回基线；成功输出不变，无双重释放。
- **关联源码/测试：**`src/packages/dwlib/dwlib.cc`、`src/tests/test_lpc.cc`。
- **文档归属（拟新增）：**`docs/efun/contrib/query_multiple_short.md`。当前没有此页；沿用同包 query_num 的 contrib 归属，在本卡实施时补充，不声称现有文档已经存在。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-056)。

### AUD-057 · parse_refresh 清标志导致旧缓存不释放

- **级别 / 类别 / 来源：**P2 / B E H N / 不确定。
- **审读视角 / 性质：**LPC parser / 缓存与生命周期；产品或工具实现缺陷。
- **位置：**src/packages/parser/parser.cc:563–617、918–942。

**源码摘录：**`src/packages/parser/parser.cc:611–616`

```cpp
  if (pi->flags & PI_SETUP) {
    pi->flags &= PI_VERB_HANDLER;
    pi->flags |= PI_REFRESH;
  } else {
    pi->flags &= PI_VERB_HANDLER;
  }
```

**源码摘录：**`src/packages/parser/parser.cc:563–567`

```cpp
static void remove_ids(parse_info_t* pinfo) {
  int i;

  if (pinfo->flags & PI_SETUP) {
    if (pinfo->num_ids) {
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。
- **触发前提：**已建立缓存的对象刷新、重复刷新或刷新后 destruct。
- **影响：**ID/复数/形容词数组和字符串引用泄漏。
- **根因：**refresh 清 PI_SETUP、设 PI_REFRESH；remove_ids 却仅在 PI_SETUP 时释放。再次 refresh 还会抹掉 PI_REFRESH，旧数组随后被覆盖。
- **现有上下文（控制流摘要，非逐字代码引用）：**清 SETUP 后，释放函数又要求 SETUP。
- **证据与确认边界：**flags 转移与清理条件确认；不是已否定的 PI_LIVING 不重置说法。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**remove_ids 按 PI_SETUP|PI_REFRESH 识别仍拥有的缓存，释放后归零；不读从未初始化的字段。 重复 refresh 保留 PI_REFRESH，只有实际重建/清理完成才结束待刷新状态。 维持正在解析时延迟释放的约束，核对 remove_parser_info 和 interrogate_object。
- **验证方式：**单次/多次刷新、错误及销毁后引用回基线；活动解析不悬垂，新 ID 下一次生效。
- **关联源码/测试：**`src/packages/parser/parser.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/parsing/parse_refresh.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-057)。

### AUD-058 · roll_MdN 把 64 位参数窄化成 int

- **级别 / 类别 / 来源：**P2 / A E / 不确定。
- **审读视角 / 性质：**efun / VM / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/contrib/contrib.cc:2835–2853。

**源码摘录：**`src/packages/contrib/contrib.cc:2835–2842`

```cpp
void f_roll_MdN() {
  int const num_arg = st_num_arg;
  int sides, rolls, bonus;
  LPC_INT roll = 0;

  rolls = (sp - 2)->u.number;
  sides = (sp - 1)->u.number;
  bonus = sp->u.number;
```

- **依赖：**AUD-046。
- **修复风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。
- **触发前提：**大于 int 范围的参数，尤其单次投掷或大 bonus。
- **影响：**合法 64 位值变成负数/其他数，结果偏离合同。
- **根因：**rolls/sides/bonus 使用 int，LPC_INT 实参在验证和运算前已截断；roll 本身已是 LPC_INT，不能误报累加器是 int。
- **现有上下文（控制流摘要，非逐字代码引用）：**int rolls,sides,bonus = LPC_INT 实参。
- **证据与确认边界：**三次实参窄化确认，累加器类型已交叉纠正。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**三种参数保持 LPC_INT，边界/正数判断在原宽度进行，检查 random_number 的参数宽度。 累加沿用项目 LPC 64 位整数溢出合同，避免引入 C++ UB。 与 AUD-046 共用有界循环，不能把窄化修复变成不受限长循环。
- **验证方式：**小值不回归；大 bonus、受控 RNG 的大 sides 保持值；极大 rolls 被预算正常终止。
- **关联源码/测试：**`src/packages/contrib/contrib.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/contrib/roll_MdN.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-058)。

### AUD-059 · socket_status 混用状态枚举和 flags 位图

- **级别 / 类别 / 来源：**P2 / C E / 不确定。
- **审读视角 / 性质：**网络 / TLS / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/sockets/socket_efuns.cc:2394–2402。

**源码摘录：**`src/packages/sockets/socket_efuns.cc:2394–2401`

```cpp
  if (!(lpc_socks[which].flags & STATE_FLUSHING) && lpc_socks[which].owner_ob &&
      !(lpc_socks[which].owner_ob->flags & O_DESTRUCTED)) {
    ret->item[5].type = T_OBJECT;
    ret->item[5].u.ob = lpc_socks[which].owner_ob;
    add_ref(lpc_socks[which].owner_ob, "socket_status");
  } else {
    ret->item[5] = const0u;
  }
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。
- **触发前提：**刷新状态或 flags 恰与枚举值重叠。
- **影响：**状态 API 返回错误 owner；不据此推断权限提升。
- **根因：**用 flags & STATE_FLUSHING 判断刷新状态，实际应读取 state；无关位值可错误隐藏/暴露 owner 槽。
- **现有上下文（控制流摘要，非逐字代码引用）：**flags & STATE_FLUSHING。
- **证据与确认边界：**两个字段的类型/用途与返回槽直接确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**改为 state != STATE_FLUSHING；保留 owner 存活检查和引用计数。 用枚举状态和独立 flags 组合验证，不靠当前数字巧合。 不改变返回数组形状和其他字段。
- **验证方式：**刷新时 owner 槽为 0，合法活动 socket 为相应对象，独立 flags 不改变结果。
- **关联源码/测试：**`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/sockets/socket_status.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**2–4 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-059)。

### AUD-060 · SC_FINAL_CLOSE 仍可进入延迟刷新

- **级别 / 类别 / 来源：**P1 / B C H / 不确定。
- **审读视角 / 性质：**网络 / TLS / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/sockets/socket_efuns.cc:2038–2074、2449–2467。

**源码摘录：**`src/packages/sockets/socket_efuns.cc:2067–2074`

```cpp
  /* if we're linkdead, we'll never flush, so don't even try :-) */
  if ((lpc_socks[fd].flags & S_BLOCKED) && !(lpc_socks[fd].flags & S_LINKDEAD)) {
    /* Can't close now; we still have data to write.  Tell the mudlib
     * it is closed, but we really finish up later.
     */
    lpc_socks[fd].state = STATE_FLUSHING;
    return EESUCCESS;
  }
```

**源码摘录：**`src/packages/sockets/socket_efuns.cc:2038–2040`

```cpp
  if (lpc_socks[fd].state == STATE_FLUSHING && !(flags & SC_FINAL_CLOSE)) {
    return EEBADF;
  }
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。
- **触发前提：**shutdown/destruct/fatal 路径要求最终关闭，队列仍 blocked 且未 linkdead。
- **影响：**描述符、事件、队列可能未最终回收，TLS 生命周期与 AUD-035 交叉。
- **根因：**FINAL 只跳过已有 FLUSHING 的前置拒绝；后续 S_BLOCKED 分支不检查 FINAL，强制清理可再次返回 FLUSHING。
- **现有上下文（控制流摘要，非逐字代码引用）：**FINAL 之后仍按 S_BLOCKED 进入 FLUSHING。
- **证据与确认边界：**中央判断与 shutdown 调用者意图不一致；应在风险发生路径修复。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**FINAL 必须绕过排空，回收全部资源；普通关闭才可进入 FLUSHING。 核对所有 FINAL 调用者、回调重入与槽 generation；不能只在调用者清 S_BLOCKED。 与 AUD-035 合并验证正常排空和强制丢弃两种不同合同。
- **验证方式：**FINAL 返回后推进夹具事件即可资源归零，不续写/重复回调；普通关闭仍按合同排空。
- **关联源码/测试：**`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/sockets/socket_close.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-060)。

### AUD-061 · parser 递归预算跨请求累计

- **级别 / 类别 / 来源：**P1 / E F N P / fork 引入问题。
- **审读视角 / 性质：**LPC parser / 缓存与生命周期；产品或工具实现缺陷。
- **位置：**src/packages/parser/parser.cc:3171–3192、3300–3361。

**源码摘录：**`src/packages/parser/parser.cc:3188–3192`

```cpp
  static int parse_recurse_steps = 0;
  constexpr int kMaxParseRecurseSteps = 1000000;
  if (++parse_recurse_steps > kMaxParseRecurseSteps) {
    return;
  }
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。
- **触发前提：**长时间服务或多次有分词分支的正常请求。
- **影响：**正常解析逐渐失效，预算不是本次请求的资源边界。
- **根因：**parse_recurse_steps 是函数 static，初始化一次且无重置。正常请求累计到 1000000 后，后续解析立即返回。
- **现有上下文（控制流摘要，非逐字代码引用）：**static int parse_recurse_steps=0。
- **证据与确认边界：**专用 fork 预算注释、static 计数及唯一入口调用确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。
- **修复方向：**计数改为最外层本次解析入口的局部值，传到 parse_recurse 及所有递归调用。 同一请求所有规则/起点共享原阈值；不要每层/每规则重置；超限沿正常清理链退出。 验证正常重复请求、单次耗尽、错误后新请求及所有解析入口。
- **验证方式：**总历史工作量超过阈值不影响下一请求；单请求仍受原阈值限制，临时词/引用清理正确。
- **关联源码/测试：**`src/packages/parser/parser.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/parsing/parse_sentence.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-061)。

### AUD-062 · PCRE2 合法前瞻捕获使替换长度下溢

- **级别 / 类别 / 来源：**P0 / B E / 不确定。
- **审读视角 / 性质：**正则 / 内存与字符串；产品或工具实现缺陷。
- **位置：**src/packages/pcre/pcre.cc:1235–1340；调用者 f_pcre_replace/f_pcre_replace_callback。

**源码摘录：**`src/packages/pcre/pcre.cc:1268–1280`

```cpp
  }

  size_t const matched_tail_length = static_cast<size_t>(match_end) - subject_pos;
  if (matched_tail_length > 0) {
    memcpy(ret + ret_pos, run->subject + subject_pos, matched_tail_length);
    ret_pos += matched_tail_length;
  }

  size_t const suffix_length = static_cast<size_t>(run->s_length) - match_end;
  if (suffix_length > 0) {
    memcpy(ret + ret_pos, run->subject + match_end, suffix_length);
    ret_pos += suffix_length;
  }
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**前瞻/后顾与零宽捕获本身是合法语法；不能一概拒绝以掩盖越界。按真实 subject 坐标重建输出，保留既有外层捕获优先的重叠处理规则。
- **触发前提：**合法零宽/短整体匹配含更长前瞻捕获。
- **影响：**巨大复制长度和内存破坏；未运行可执行攻击样本。
- **根因：**捕获只验证在 subject 内；最终尾段却用 group 0 的 match_end 减去捕获的 last_end。前瞻捕获可超过整体匹配终点，差值下溢后进入 memcpy。
- **现有上下文（控制流摘要，非逐字代码引用）：**tail=match_end-last_capture_end。
- **证据与确认边界：**PCRE2 捕获范围合同与本地尾段算术闭环。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**用 subject 范围和唯一源游标拼装非重叠捕获替换：游标从 0 开始、最终尾段为 subject_length-cursor。 保留外层捕获优先的重叠规则；支持合法前瞻/后瞻，不通过拒绝合法模式绕过根因。 复核总输出长度与实际复制一致；两个 replace 入口共享同一实现。
- **验证方式：**前瞻/后瞻、零宽、嵌套/可选捕获、空替换及上限，输出正确且 ASan 无越界。
- **关联源码/测试：**`src/packages/pcre/pcre.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/pcre/pcre_replace.md`、`docs/efun/pcre/pcre_replace_callback.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-062)。

### AUD-063 · pcre_assoc 空匹配推进越尾且余量失真

- **级别 / 类别 / 来源：**P1 / B E / 不确定。
- **审读视角 / 性质：**正则 / 内存与字符串；产品或工具实现缺陷。
- **位置：**src/packages/pcre/pcre.cc:1015–1069。

**源码摘录：**`src/packages/pcre/pcre.cc:1060–1071`

```cpp
        rmp->begin = rmpb_tmp;
        rmp->end = tmp = rmpe_tmp;
        used += tmpreg->ovector[1];
        rmp->tok_i = regindex;
        rmp->next = (struct RegMatch*)nullptr;
      } else {
        break;
      }

      if (rmp->begin == tmp && (!*++tmp)) {
        break;
      }
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**零宽匹配推进不能越过终点或拆开 UTF-8 码点；同步实际源游标与剩余字节数，保留既有 token/default 结果，不误改已单独处理空匹配的 match_all 路径。
- **触发前提：**空匹配位于末尾或多字节字符前。
- **影响：**越界读、错误匹配/UTF 错误；不是已具边界辅助函数的 match_all 路径。
- **根因：**空匹配直接 tmp++，未验证终点且未同步 used；下一轮可解引用尾后，并向 PCRE2 声明过长剩余 subject。也可能落到 UTF-8 续字节。
- **现有上下文（控制流摘要，非逐字代码引用）：**tmp++，used 不变，下一轮 while(*tmp)。
- **证据与确认边界：**空匹配分支与下一次长度赋值确认。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**用唯一字节 offset 推导剩余长度，每次执行匹配使用真实 suffix 长度。 空匹配在终点停止，否则复用 UTF/非 UTF 模式相应的有界推进，保持字节拼接不遗漏。 保留模式优先级、tokens/default、嵌套返回数组和正常 suffix 锚点语义。
- **验证方式：**空串/空模式、开头/中间/末尾空匹配、UTF-8 和非 UTF 模式均有界；拼接还原输入，无重复丢字。
- **关联源码/测试：**`src/packages/pcre/pcre.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/pcre/pcre_assoc.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-063)。

### AUD-064 · 运行时浮点转整数未检查可表示范围

- **级别 / 类别 / 来源：**P2 / A B E / 不确定。
- **审读视角 / 性质：**VM / 编译器数值语义；产品或工具实现缺陷。
- **位置：**src/packages/core/efuns_main.cc:3132–3140；src/packages/core/sprintf.cc:1357–1366。

**源码摘录：**`src/packages/core/efuns_main.cc:3132–3139`

```cpp
void f__to_int() {
  LPC_INT temp = 0;
  switch (sp->type) {
    case T_REAL:
      temp = sp->u.real;
      sp->type = T_NUMBER;
      sp->u.number = temp;
      break;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**越界浮点转换的失败语义必须与 VM/编译期一致；保留合法端点、截断方向和已有整数行为，不靠关闭 sanitizer 规避 UB。
- **触发前提：**运行时数值转换输入 NaN、无穷或越界有限浮点。
- **影响：**结果依编译器/平台变化；未证明内存破坏。
- **根因：**f__to_int 直接把 LPC_FLOAT 赋给 LPC_INT；非有限或超出整数可表示范围时是 C++ UB。 sprintf 的浮点 %d 分支先 floor 后直接赋整数，同样未校验。
- **现有上下文（控制流摘要，非逐字代码引用）：**LPC_INT temp = floating_value。
- **证据与确认边界：**原生转换直接确认；具体诊断属待批准的错误路径合同，不声称已修复。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**建立编译器与运行时共用的内部检查转换：有限且处于 [-2^63,2^63) 才截向零转换。 上界不能用四舍五入后的 double(INT64_MAX) 作包含边界；失败走 LPC 数值范围错误。 to_int 保持截向零；sprintf %d 保持先 floor 再转换。仅这些 float 路径复用检查，string/buffer 合同不改。
- **验证方式：**零/负小数、上下界邻点、NaN/无穷都确定；正常值与旧输出一致，越界在 cast 前拒绝。 同时覆盖 sprintf %d 的既有 floor 语义和异常边界。
- **关联源码/测试：**`src/packages/core/efuns_main.cc`、`src/vm/internal/base/svalue.h`、`src/tests/test_lpc.cc`、`src/packages/core/sprintf.cc`。
- **文档归属：**`docs/efun/floats/to_int.md`、`docs/efun/strings/sprintf.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-064)。

### AUD-065 · 常量浮点转整数绕过运行时范围合同

- **级别 / 类别 / 来源：**P2 / A B D / 不确定。
- **审读视角 / 性质：**VM / 编译器数值语义；产品或工具实现缺陷。
- **位置：**src/compiler/internal/compiler.cc:1825–1832、1912、2090。

**源码摘录：**`src/compiler/internal/compiler.cc:1825–1832`

```cpp
parse_node_t* promote_to_int(parse_node_t* node) {
  parse_node_t* expr;
  if (node->kind == NODE_REAL) {
    node->kind = NODE_NUMBER;
    decltype(node->v.number) tmp = node->v.real;
    node->v.number = tmp;
    return node;
  }
```

- **依赖：**AUD-064。
- **修复风险与缓解：**越界浮点转换的失败语义必须与 VM/编译期一致；保留合法端点、截断方向和已有整数行为，不靠关闭 sanitizer 规避 UB。
- **触发前提：**常量折叠/类型提升走 promote_to_int，值在 LPC_INT 范围外。
- **影响：**同一表达式常量和运行时结果/诊断不一致。
- **根因：**promote_to_int 在 NODE_REAL 上直接窄化并先改节点类型，越界常量在编译器内发生同类 C++ UB。
- **现有上下文（控制流摘要，非逐字代码引用）：**先设 NODE_NUMBER，再直接窄化。
- **证据与确认边界：**直接常量分支及非常量转为 to_int efun 的分歧已核。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**复用 AUD-064 的同一转换规则；确认可表示后才改节点 kind/type/value。 失败生成编译期范围诊断，不发布损坏 program，也不静默推迟规避验证。 核对赋值、显式转换及所有 promote_to_int 调用路径。
- **验证方式：**常量/运行时边界语义一致，失败后下一次编译正常；含 float-cast-overflow 的专项插桩验证。
- **关联源码/测试：**`src/compiler/internal/compiler.cc`、`src/tests/test_lpc_compiler.cc`。
- **文档归属：**`docs/efun/floats/to_int.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-065)。

### AUD-066 · replace_dollars 消费游标不一致导致负复制长度

- **级别 / 类别 / 来源：**P0 / B E / 不确定。
- **审读视角 / 性质：**dwlib / 值语义；产品或工具实现缺陷。
- **位置：**src/packages/dwlib/dwlib.cc:819–882。

**源码摘录：**`src/packages/dwlib/dwlib.cc:873–881`

```cpp
  if (currentnew + COUNTED_STRLEN(oldstr) - currentold + oldstr - newstr > max_string_length) {
    FREE_MSTR(newstr);
    error("string too long");
  }

  strcpy(currentnew, currentold);
  pop_2_elems();
  push_malloced_string(
      extend_string(newstr, currentnew - newstr + COUNTED_STRLEN(oldstr) - (currentold - oldstr)));
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**格式化及替换不能更改调用者共享值或泄漏部分结果；保留输出语义，异常时清理，测试重复调用与别名输入。
- **触发前提：**模式覆盖的源范围内含后续可匹配标记，或起点匹配替换为空。
- **影响：**负 len 转无符号大长度进入 strncpy；空替换结果错误。现有 replacement-length 修复未覆盖这些路径。
- **根因：**匹配后 currentold 前进但扫描 i 仍逐字递增，可重新匹配已消费范围得到负 len；尾检查还有跨分配指针相减。零长度产出还被误判未发生替换。
- **现有上下文（控制流摘要，非逐字代码引用）：**currentold 前进而 i 不跳过；跨分配指针算尾长。
- **证据与确认边界：**输入扫描、消费、尾算术及零产出分支直接确认；归属文档需新建。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**用源 offset、扫描 offset、输出长度统一所有计算；消费非空模式后跳到其末尾，禁止再扫已消费范围。 空模式保证扫描至少推进一个字节但保留未消费源字符；用独立 matched 标志区分零产出与未匹配。 复制前检查剩余源/目标容量，尾段只在同一字符串内计算；保持规则顺序和返回类型。
- **验证方式：**重叠/内嵌标记、空 key/空替换、多规则和边界；对照结果正确，ASan 无负长度/越界。
- **关联源码/测试：**`src/packages/dwlib/dwlib.cc`、`src/tests/test_lpc.cc`。
- **文档归属（拟新增）：**`docs/efun/contrib/replace_dollars.md`。当前没有此页；沿用同包 query_num 的 contrib 归属，在本卡实施时补充，不声称现有文档已经存在。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-066)。

### AUD-067 · include 宏展开直接递归且无预算

- **级别 / 类别 / 来源：**P1 / D F / 不确定。
- **审读视角 / 性质：**LPC 编译器 / C++；产品或工具实现缺陷。
- **位置：**src/compiler/internal/lex.cc:1181–1199。

**源码摘录：**`src/compiler/internal/lex.cc:1186–1200`

```cpp
  if (*name != '"' && *name != '<') {
    defn_t* d;

    if ((d = lookup_define(name)) && d->nargs == -1) {
      char* q;

      q = d->exps;
      while (uisspace(*q)) {
        q++;
      }
      handle_include(q, global);
    } else {
      include_error("Missing leading \" or < in #include", global);
    }
    return;
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。
- **触发前提：**include 参数为循环宏或足够长的间接宏链。
- **影响：**编译期栈耗尽/不可控展开；不提供可执行危险源。
- **根因：**handle_include 在文件包含深度检查之前对宏值递归调用自身，未走普通宏展开预算；循环宏或长链可耗尽原生栈。
- **现有上下文（控制流摘要，非逐字代码引用）：**return handle_include(def->exps)。
- **证据与确认边界：**递归发生在实际 include 深度计数之前，普通宏预算未覆盖该分支。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**改为迭代解析宏链，复用现有宏展开上限，不另设随意的小兼容阈值。 每一步计预算，循环/超限输出编译诊断；正常定界符后才进入原文件包含路径。 保留符号记录、源码行号和已验证的权限回调原始拼写。
- **验证方式：**普通/间接链正确，循环/超限有界拒绝；之后正常编译成功，无原生栈增长。
- **关联源码/测试：**`src/compiler/internal/lex.cc`、`src/tests/test_lpc_compiler.cc`。
- **文档归属：**`src/compiler/internal/lex.cc`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**8–16 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-067)。

### AUD-068 · uncompress 注册 string 输入却无实现

- **级别 / 类别 / 来源：**P2 / E / 不确定。
- **审读视角 / 性质：**压缩 / 类型与资源；产品或工具实现缺陷。
- **位置：**src/packages/compress/compress.spec:10；src/packages/compress/compress.cc:338–352。

**源码摘录：**`src/packages/compress/compress.cc:342–348`

```cpp
  if (sp->type == T_BUFFER) {
    buffer = sp->u.buf;
  } else {
    pop_n_elems(st_num_arg);
    push_undefined();
    return;
  }
```

**源码摘录：**`src/packages/compress/compress.spec:10`

```text
buffer uncompress(string | buffer);
```

- **依赖：**无新增前置依赖；遵守方案中的公共准入和可信验证前置。
- **修复风险与缓解：**保持 string/buffer 的精确字节长度和输入存活期；保留非法流、空输入与输出上限合同，不删注册重载或取消解压上限。
- **触发前提：**合法 string 重载，包括可表示的压缩字节串。
- **影响：**编译可通过而已声明重载始终不工作。
- **根因：**签名为 buffer uncompress(string|buffer)，运行时却只接受 T_BUFFER，所有 string 在 inflate 前直接返回 undefined。
- **现有上下文（控制流摘要，非逐字代码引用）：**非 T_BUFFER 直接 undefined。
- **证据与确认边界：**注册、实现和 compress 返回类型直接核对，未把文件压缩删除合同混入此项。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**string/buffer 都先取得借用字节区间与精确长度，共用现有 inflate 和输出上限。 字符串用 SVALUE_STRLEN，不用 strlen；输入保持存活直到解压结束，返回仍是 buffer。 保留非法压缩数据的 undefined 与输出超限错误，完善现有归属文档，不删注册重载。
- **验证方式：**两种输入同字节得到同 buffer；嵌入 NUL、截断、空数据及解压上限，清理正确。
- **关联源码/测试：**`src/packages/compress/compress.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/general/uncompress.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-068)。

### AUD-069 · sprintf 数组数值转换原地修改输入元素

- **级别 / 类别 / 来源：**P2 / E / 不确定。
- **审读视角 / 性质：**efun / VM / 生命周期；产品或工具实现缺陷。
- **位置：**src/packages/core/sprintf.cc:1075–1081、1357–1369、1404–1405。

**源码摘录：**`src/packages/core/sprintf.cc:1084–1085`

```cpp
        carg = (argv + sprintf_state->cur_arg)->u.arr->item;
        nelemno = 1; /* next element number */
```

**源码摘录：**`src/packages/core/sprintf.cc:1357–1366`

```cpp
          if ((cheat[i - 1] == 'f' && carg->type == T_NUMBER)) {
            // convert to T_REAL
            LPC_FLOAT temp = carg->u.number;
            carg->type = T_REAL;
            carg->u.real = temp;
          } else if ((cheat[i - 1] == 'd' && carg->type == T_REAL)) {
            // convert to T_NUMBER
            LPC_INT temp = floor(carg->u.real);
            carg->type = T_NUMBER;
            carg->u.number = temp;
```

- **依赖：**AUD-064。
- **修复风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。
- **触发前提：**数组格式化把整数作为浮点输出，或浮点作为整数输出。
- **影响：**本应格式化的调用改变后续业务可见的数组类型和值；未把此现象扩张成已证跨 owner 写入。
- **根因：**INFO_ARRAY 的 carg 指向调用方数组元素；%f/%d 的数字类型适配直接改 carg->type/u，从而修改共享输入数组。
- **现有上下文（控制流摘要，非逐字代码引用）：**carg 指向数组元素，适配时直接改 type/u。
- **证据与确认边界：**数组取址、原地适配及下一元素赋址三处直接闭环。 源码判断不等于已跑本项新增回归；通用套件通过也不补成此项 fail-first。根因已核对；尚无足够引入提交证据，不能按文件年代猜成上游或 fork。
- **修复方向：**数值适配在当前迭代的局部 svalue副本上完成，格式化只读取原元素，禁止改写 carg 指向的输入。 沿用 AUD-064 的范围检查及原 floor/浮点规则；局部数字不增加引用所有权或堆分配。 验证别名数组、多元素、失败中途和后续调用；与 AUD-050 分别保留回归。
- **验证方式：**%@f/%@d 输出不变，输入及其别名的 tag/value 前后相同；中途范围/类型错误也不产生部分修改。
- **关联源码/测试：**`src/packages/core/sprintf.cc`、`src/tests/test_lpc.cc`。
- **文档归属：**`docs/efun/strings/sprintf.md`。只在实际实施并改变相应行为后同步，不预填修复事实。
- **预估工作量：**4–8 小时；包含定向回归与必要文档，不含等待外部平台。
- **修复状态：**未实施；可执行任务卡见 [修复方案](REMEDIATION_PLAN.md#aud-069)。

## 8. 问题分布与上游未同步修复

### 8.1 分布

| 严重级别 | 数量 |
| --- | --- |
| P0 | 5 |
| P1 | 22 |
| P2 | 40 |
| P3 | 2 |

| 来源 | 数量 |
| --- | --- |
| 上游继承问题 | 15 |
| fork 引入问题 | 17 |
| fork 未同步上游修复 | 3 |
| 不确定来源 | 34 |

类别按 A–P 具体审计维度多标，不应相加后与 69 比较。

| 类别 | 涉及问题数 |
| --- | --- |
| A | 8 |
| B | 30 |
| C | 12 |
| D | 13 |
| E | 36 |
| F | 8 |
| G | 8 |
| H | 16 |
| I | 11 |
| J | 7 |
| K | 7 |
| L | 1 |
| M | 3 |
| N | 5 |
| O | 5 |
| P | 15 |

### 8.2 本地快照内已确认的残余上游修复

| 上游提交 | 上游路径/性质 | 当前残余 | 本地处理 |
| --- | --- | --- | --- |
| b2ba3f8f0031b2718448acfdbaccfe550e38931c（2026-10-03，#1333） | src/net/transport_libevent.cc 的 ASCII 分隔符前读保护；**内存安全修复** | AUD-009；本地对应路径仍是 src/comm.cc | 仅移植对应 hunk，与 AUD-001/010 合并验证；不重复吸收整个混合提交 |
| 5f7be1008f28ac400e7fa436b6e353656bb7db58（2026-07-11，#1245） | comm/Telnet UTF-8 后缀与 get_char 控制字符/整字符交付；**正确性/兼容修复**，不冒称 CVE | AUD-011/012 | 选择性适配现有 owner/session 生命周期；不连带引入整套 TUI mudlib |
| 858d5da9d068e69eb4fa287730937e6b0564690b（2026-08-27，#1347） | 大范围 async/await/Promise 引入；不是独立安全修复 | AUD-018 仅借鉴其复合值覆盖思路 | 本地补全拥有字段遍历；禁止覆盖 fork park/owner 实现或整包 cherry-pick |

**不列为“待同步漏洞”的反例：**本地已经有异步 getdir 分配按 sizeof(dirent) 增长、reclaim 递归计数回退、输入 callback 的非法 init 名称清理等等价修正；存在历史提交标题并不足以要求重复应用。Char-mode 大提交还含 NAWS/TUI 等内容，本报告不把整提交的所有 hunk 自动算作未同步。

续审已取得在线提交、安全工具及公告查询缓存，见 §10.3；并未完成全部组件/补丁的安全适用性核验。**unknown / external verification required 不等于 0 漏洞。** 不由版本字符串、空查询或成功构建推导“依赖全部安全”。

## 9. 交付自检

- 对照原任务 A–P 共 109 个子项核对：全文件台账、fork/上游差异、架构、问题证据、来源和修复方案均保留；不能把已记录检查点称为所有路径已证明安全。
- 两份文档各有 AUD-001–AUD-069 的唯一详细节；总表、级别、类别、来源和工作量按同一问题集合复算。机械核验通过：新增 55 段源码摘录与原文一致，88 段 shell 命令通过 bash -n，73 个本地文档链接（含 45 个 ID 锚点链接）有效，git diff --check 无错误。这不表示拟议补丁已编译或修复后测试已运行。
- 新增问题均列位置、上下文、触发、影响、根因、验证、依赖、风险与状态；来源不确定的 34 项不伪造引入提交。
- 附录 A–G 保留最初 a4475b1a 的原始台账，不把两份报告、新构建与后来的验证结果倒灌成历史覆盖。55c7360b 相对该源码基线仅新增这两份报告。
- 本轮交付只修改本文件与修复方案；没有修驱动/测试/配置、改变唯一主计划或操作既有 driver。代码行为未变，不提前更新 API/CHANGELOG；需要同步的权威文档已列入对应实施卡，现有 skill 约束仍适用。
- **审计剩余边界见 §10.5；69 项修复均未实施。** 文档交付不等于全量安全验收或所有审计目标闭合。

## 10. 续审执行记录、证据与未闭合项

### 10.1 身份、目录与计数

续审源码为 `55c7360bdbd81ef717290c8cd3b233da42d67724`，产品源码与最初 `a4475b1afe998bb03a37bdb0596b64ab01042ed3` 相同；两者只差两份报告。当前跟踪路径 3884，原始文件台账分母仍是 3882。以下 E 均指仓库内 `build-modernization-evidence/audit-deep-55c7360b-20261012T072825Z/`，不是 /tmp 唯一副本，也不是需要提交的运行产物。

- `source-reviews-final/summary.json`：63 个分片，returned=63、failed=0、not_started=0。它证明工作收回，不证明每个结论正确或每条执行路径覆盖；主代理另对照源码、调用者和合同。
- 中断/重复波次不累加：`source-reviews-resumed/summary.json` 的 17 返回、3 失败、43 未开始不是最终完成率；`extended-validation-summary.json` 未生成，不能补造总通过记录。
- LPC 最终逐项记录归 `lpc-resumed-summary.json`：planned=recorded=419、interrupted=false。原记录和续跑记录按用例身份去重，不把重跑次数当新覆盖。
- 该逐项 Debug driver SHA-256 为 `1dd9ec9fafce7302f86800d12ec5fa89b1826edfcf4a874fe3094c8054532adc`；其他 profile 使用各自 summary 的摘要，不能套用这个值。
- 构建使用独立目录和有限并行；没有改写运行实例的产物。验证进程与既有 driver 分离，失败日志原样保留。

### 10.2 实际运行结果

| 工作 | 实际结果 | 证据入口 / 解释 |
| --- | --- | --- |
| Debug 原生套件 | 509 lpc_tests + 2 ofile + 8 compile_arena + 14 object_store + 96 file_io = **629 通过** | `debug-validation-summary.json`、`gtest-debug-*/summary.json` |
| ASan+UBSan 原生套件 | 同五套件 **629 通过** | `sanitizer-summary.json` 与对应 gtest 记录；仅证明实际启用的 sanitizer 检查 |
| TSan 原生套件 | 同五套件 **629 通过** | 同上；本机按 setarch 禁用 ASLR 的受支持方式运行，不当作任意平台证明 |
| 三配置完整 LPC | **均未通过** | `lpc-debug-full/`、`lpc-asan-ubsan-full/`、`lpc-tsan-full/`；已出现 case 完成后的断言，见 AUD-038 |
| 419 项逐项 LPC | **415 通过，4 失败** | 两个 warn_%s 文件被 runner 拒绝（AUD-037）；async_db_exec_bound_args 与 db 各约 183 秒失败（AUD-038）；不把 wrapper 失败变成产品 Crash |
| CTest Debug | **未通过** | `ctest-debug/summary.json`；严格 wrapper 拒绝 TestLpcshellInteractiveContract 的 skip，不能与直接 GTest 记录混同 |
| GCC RelWithDebInfo 的 file_io_tests | **94/96 通过，2 失败** | `gtest-audit44-gcc-relwithdebinfo-file_io_tests/summary.json`；SymbolWriteFailureIsReported、StatWriteFailureIsReported 的注入未命中 |
| GCC 优化配置/构建 | configure 与 file_io_tests 构建成功 | `configure-audit44-gcc-relwithdebinfo/`、`build-audit44-gcc-relwithdebinfo/`；失败发生在测试，不是构建 |

GCC 优化测试二进制 SHA-256 为 `854efb715bfc716fe0818275fc39d49af486af6f8c67ea8b1062a0a464bd810d`。调用经 FORTIFY 改为 __fprintf_chk，现有 --wrap=fprintf 不能截获；修复应补齐注入，不关闭 FORTIFY。原始 CI 临时日志没有完整上传的问题独立记为 AUD-042。

其他 gateway、lpcc、trace、arena 与 locale 探针保留在 E 的同名目录。它们只支持各自输入/布局下的观察，不替代 69 项逐 ID 的修复前后回归、完整 fuzz 或真实负载性能结论。单例 SQLite 超时的所有退出期根因尚未单独证明；先修正已确认的完成协议，若仍挂起必须继续追踪，不能猜成数据库锁或直接加长超时。

### 10.3 上游与安全工具

在线比对缓存 `upstream-post-snapshot.json` 为 ahead_by=total_commits=6，终点是 `a2902952d74a53b5b031aa2d98633bec5fba6e55`。这是该次查询快照，不是永久“最新”声明。

| SHA / 提交 | 与本地的关系和处理 |
| --- | --- |
| [9d24db92b8e6](https://github.com/fluffos/fluffos/commit/9d24db92b8e693fcd40b5e57eaaa07864ee404f5) · docs: telnet_suboption receives the payload without the option byte (#1437) | 文档候选；核对 telnet_suboption 实际传参后再决定是否同步 |
| [df21db8e8c6c](https://github.com/fluffos/fluffos/commit/df21db8e8c6ccd196fdbb045b760d29d9afdbe44) · docs: fix broken diagnostics link in lpc/types/float (#1438) | 文档候选；只在本地仍有同型断链时处理 |
| [774d1783c419](https://github.com/fluffos/fluffos/commit/774d1783c4193ff620a8ef2692e9ef360b33a20c) · config: read whole lines; reject an over-long setting instead of dropping the rest (#1434) | 关联 AUD-013；核对有界整行读取、超限拒绝与 IO 错误合同 |
| [ffbd54a67d4f](https://github.com/fluffos/fluffos/commit/ffbd54a67d4f64f97656a2776f77da2a57d9d80d) · compiler: catch(expr) no longer prunes operations that can error (#1440) | 关联 AUD-017；核对 catch 中可观察异常，不能只补单个运算 |
| [c51f482b55ff](https://github.com/fluffos/fluffos/commit/c51f482b55ffdd6c2a6b260c07decdb186c70b26) · compiler: keep error-capable code in catch commas, blocks, refs, $() (#1442) | 关联 AUD-017；覆盖逗号、块、引用与 $() 的错误保留 |
| [a2902952d74a](https://github.com/fluffos/fluffos/commit/a2902952d74a53b5b031aa2d98633bec5fba6e55) · compiler: track "inside a catch body" with its own context bit (#1443) | 关联 AUD-016；核对 catch 上下文位与本 fork async/await 边界 |

这 6 项不是都属于安全补丁，也没有授权整包 cherry-pick/merge。§8 的本地已确认残余与此处在线候选分开。后续取候选 hunk 前仍须核对接口、owner、生成物及测试；原来源字段不因“找到类似标题”而改写。

`codeql_analyses.json` 记录了 55c7360b 的一次分析，results_count=3、rules_count=58；`codeql_alerts.json` 有 3 条 open：#31（libwebsockets 明文传输）、#30（efuns_main 弱算法）、#6（vendored crypt_des 弱算法）。它们是工具告警，不冒充本轮已经复现的 3 个新增漏洞。适用调用面、兼容用途及暴露策略仍须人工裁定。`upstream_advisories.json` 和 `dependency-advisories.json` 的查询缓存也不能替代真实组件身份和本地 patch 的适用性核验，尤其 AUD-021 的身份漂移尚未修复。

### 10.4 去重与结论校准

- 没有找到当前驱动向 -D 预定义列表写入的可达路径；不把相关无界扫描描述成已证实的启动参数漏洞。
- comm 清理先把 ssl 置空，不能把其后续写描述成访问原 SSL 对象的 UAF；socket efun 的 TLS 关闭/续写问题按自己的实际路径记录，不混淆两个模块。
- link 的 EXDEV 分支是 symlink 回退，不是复制；AUD-049 约束相对目标和失败返回 1。
- 优化构建的 constant_expr 受 __DEBUG__ 保护，属于测试能力预分类缺口；不是“编译器丢失功能”，也不能用假断言让它通过。
- FORTIFY 下两项写失败测试未命中钩子，属于测试注入缺口，不据此认定产品错误传播失败。
- parser 递归预算跨调用累计与 reclaim 的历史递归计数修复不是同一问题；不会重复吸收已经等价实现的上游补丁。

### 10.5 尚未闭合的验收边界

1. **逐路径和逐 hunk：**原任务要求的完整语义矩阵仍不足。63 分片、全文件读取和 69 项发现不等于 VM 每条指令/错误路径、全部 efun 组合或 2710 条差异的独立安全证明。
2. **测试闭合：**完整 LPC、严格 CTest 与 GCC 优化 file_io_tests 当前未通过。修正已确认的测试缺口后必须重跑；本轮没有修它们，也不补造通过。
3. **真实环境：**原生 macOS/Windows、WASM runtime、持续慢读/TLS 压力、外部 PostgreSQL/MySQL、真实部署性能和容量没有对应完整证据。隔离 Linux fixture 不能代替它们。
4. **来源与供应链：**34 项来源仍不确定；在线候选的全部 hunk、安全公告适用性和 3 条 CodeQL open 告警未全部闭合，不能发布“依赖/上游漏洞清零”。
5. **修复实施：**69 项均待实施批准；优先级、依赖、保留合同、正常/边界/失败测试和回退见修复方案。没有把发布、部署、现有 driver 或下游数据加入范围。

上述边界保留为明确未完成项，不因报告已落盘、原生套件通过或分片返回齐全而消失。


## 附录说明

A：当前全部跟踪路径（R=文本通道已完整读取；K=在正文作关键证据交叉核验；B=原始字节元数据，可能与 R 重叠）。R/K 均不是逐文件安全签字。
B：默认 rename 检测的完整差异；D 行是历史删除路径，不是当前漏读文件。
C/D：提交清单用于复核来源，提交标题不是安全证明；只保留日期/标题，不复制作者账号、邮件或会话元数据。
E：no-renames 模块统计。F：忽略目录/元数据边界。G：46 个原始字节文件元数据，不展示其内容。

**生成前补核：**AUD-003 沿用 `complete_all_asyncio()` 的嵌套回调 drain，并将最终 worker stop 放在其他生产者停止后。AUD-007 复用 `src/vm/internal/owner_runtime_coordinator.cc:143–207` 已有的 owner-wide quiescence；其 epoch 只保护冻结窗口，不是 object/program epoch。两项均尚未实施或动态验证，找到入口不表示当前路径已经安全。

## 附录 A. 全量文件台账

本台账锚定审计起点的 3,882 个 Git 跟踪文件；新增的两份交付报告不倒灌进覆盖率。非忽略未跟踪文件为 0。物理树的忽略/构建/运行目录只做了前文所述元数据清点，不冒充源文件深审。

- `R`：分段读取记录完整，仅证明基础读取，不证明文件内每条路径都深审通过。
- `R-raw`：同属已读取集合，但 UTF-8/NUL 有格式例外；原始字节身份另列于 A.2，不把替换显示当无损文本恢复。
- `E:AUD-xxx`：该文件有对应问题的精确引文；不是对该文件其余部分的安全背书。
- `M`：非文本文件只核对元数据和 SHA-256，未逆向、解码执行或对内容作语义签字。

完整台账较大，应下载原始 Markdown 或本地查看；托管页面的渲染截断不代表文件内容缺失。

### A.1 全部文件（文本 3,843；非文本 39）

| 路径 | 字节 | 行数 | 读取块数 | 取证状态 |
| --- | ---: | ---: | ---: | --- |
| `.coveralls.yml` | 71 | 3 | 1 | R |
| `.dockerignore` | 113 | 4 | 1 | R |
| `.editorconfig` | 485 | 31 | 1 | R |
| `.gitattributes` | 290 | 10 | 1 | R |
| `.github/workflows/ci.yml` | 20175 | 488 | 1 | R / E:AUD-023 |
| `.github/workflows/codeql-analysis.yml` | 1953 | 58 | 1 | R |
| `.github/workflows/coverity-scan.yml` | 2598 | 67 | 1 | R |
| `.github/workflows/docker-smoke.yml` | 1238 | 44 | 1 | R |
| `.github/workflows/gh-pages.yml` | 2287 | 68 | 1 | R |
| `.gitignore` | 454 | 29 | 1 | R |
| `.node-version` | 8 | 2 | 1 | R |
| `AGENTS.md` | 13051 | 216 | 1 | R |
| `CHANGELOG.md` | 3324 | 53 | 1 | R |
| `CMakeLists.txt` | 2673 | 84 | 1 | R |
| `CMakePresets.json` | 3066 | 107 | 1 | R |
| `CODE_OF_CONDUCT.md` | 4597 | 75 | 1 | R |
| `CONTRIBUTING.md` | 2467 | 55 | 1 | R |
| `ChangeLog` | 10521 | 232 | 1 | R |
| `Copyright` | 730 | 20 | 1 | R |
| `Credits` | 235 | 10 | 1 | R |
| `Dockerfile` | 1637 | 48 | 1 | R |
| `LICENSE` | 1055 | 15 | 1 | R |
| `NOTICE` | 1393 | 37 | 1 | R |
| `README.md` | 12230 | 259 | 1 | R |
| `README_CN.md` | 11622 | 249 | 1 | R |
| `SECURITY.md` | 2696 | 67 | 1 | R |
| `cmake/FindLibevent.cmake` | 3619 | 100 | 1 | R |
| `cmake/FindMySQL.cmake` | 30134 | 904 | 1 | R |
| `cmake/FindPCRE.cmake` | 3638 | 92 | 1 | R |
| `cmake/FindPostgreSQL.cmake` | 12583 | 311 | 1 | R |
| `cmake/FindSQLite3.cmake` | 1977 | 67 | 1 | R |
| `cmake/Findjemalloc.cmake` | 1699 | 51 | 1 | R |
| `cmake/helper.cmake` | 1419 | 44 | 1 | R |
| `compat/fixstatic` | 1690 | 51 | 1 | R |
| `compat/parser_error.c` | 1564 | 59 | 1 | R |
| `compat/simuls/apply.c` | 259 | 10 | 1 | R |
| `compat/simuls/break_string.c` | 621 | 27 | 1 | R |
| `compat/simuls/dump_socket_status.c` | 361 | 11 | 1 | R |
| `compat/simuls/parse_command.c` | 15956 | 676 | 1 | R |
| `compat/simuls/process_string.c` | 794 | 40 | 1 | R |
| `compat/simuls/tail.c` | 1380 | 61 | 1 | R |
| `docs/.gitignore` | 78 | 7 | 1 | R |
| `docs/.vitepress/config.mts` | 1453 | 54 | 1 | R |
| `docs/.vitepress/sidebar.ts` | 2001 | 71 | 1 | R |
| `docs/.vitepress/theme/index.ts` | 435 | 18 | 1 | R |
| `docs/.vitepress/theme/style.css` | 4286 | 140 | 1 | R |
| `docs/CNAME` | 16 | 1 | 1 | R |
| `docs/_layouts/default.html` | 5734 | 138 | 1 | R |
| `docs/add_missing_efuns.py` | 1408 | 66 | 1 | R |
| `docs/apply/index.md` | 2910 | 75 | 1 | R |
| `docs/apply/interactive/catch_tell.md` | 686 | 28 | 1 | R |
| `docs/apply/interactive/gmcp.md` | 482 | 24 | 1 | R |
| `docs/apply/interactive/gmcp_enable.md` | 319 | 23 | 1 | R |
| `docs/apply/interactive/index.md` | 739 | 26 | 1 | R |
| `docs/apply/interactive/logon.md` | 317 | 24 | 1 | R |
| `docs/apply/interactive/msdp.md` | 579 | 26 | 1 | R |
| `docs/apply/interactive/msdp_enable.md` | 328 | 23 | 1 | R |
| `docs/apply/interactive/msp_enable.md` | 303 | 22 | 1 | R |
| `docs/apply/interactive/mxp_enable.md` | 663 | 32 | 1 | R |
| `docs/apply/interactive/mxp_tag.md` | 810 | 39 | 1 | R |
| `docs/apply/interactive/net_dead.md` | 613 | 28 | 1 | R |
| `docs/apply/interactive/process_input.md` | 772 | 30 | 1 | R |
| `docs/apply/interactive/receive_ed.md` | 1465 | 55 | 1 | R |
| `docs/apply/interactive/receive_environ.md` | 556 | 23 | 1 | R |
| `docs/apply/interactive/receive_message.md` | 1036 | 41 | 1 | R |
| `docs/apply/interactive/receive_snoop.md` | 545 | 26 | 1 | R |
| `docs/apply/interactive/telnet_suboption.md` | 1091 | 35 | 1 | R |
| `docs/apply/interactive/terminal_colour_replace.md` | 1524 | 58 | 1 | R |
| `docs/apply/interactive/terminal_type.md` | 470 | 22 | 1 | R |
| `docs/apply/interactive/window_size.md` | 376 | 20 | 1 | R |
| `docs/apply/interactive/write_prompt.md` | 468 | 25 | 1 | R |
| `docs/apply/interactive/zmp.md` | 1357 | 57 | 1 | R |
| `docs/apply/master/author_file.md` | 818 | 31 | 1 | R |
| `docs/apply/master/compile_object.md` | 1154 | 31 | 1 | R |
| `docs/apply/master/connect.md` | 608 | 29 | 1 | R |
| `docs/apply/master/crash.md` | 722 | 30 | 1 | R |
| `docs/apply/master/creator_file.md` | 948 | 32 | 1 | R |
| `docs/apply/master/domain_file.md` | 841 | 35 | 1 | R |
| `docs/apply/master/epilog.md` | 954 | 35 | 1 | R |
| `docs/apply/master/error_handler.md` | 1164 | 47 | 1 | R |
| `docs/apply/master/flag.md` | 291 | 20 | 1 | R |
| `docs/apply/master/get_bb_uid.md` | 416 | 25 | 1 | R |
| `docs/apply/master/get_include_path.md` | 1221 | 37 | 1 | R |
| `docs/apply/master/get_mud_stats.md` | 381 | 23 | 1 | R |
| `docs/apply/master/get_root_uid.md` | 450 | 26 | 1 | R |
| `docs/apply/master/get_save_file_name.md` | 597 | 30 | 1 | R |
| `docs/apply/master/index.md` | 1290 | 40 | 1 | R |
| `docs/apply/master/log_error.md` | 693 | 32 | 1 | R |
| `docs/apply/master/make_path_absolute.md` | 385 | 24 | 1 | R |
| `docs/apply/master/object_name.md` | 449 | 25 | 1 | R |
| `docs/apply/master/parser_error_message.md` | 2220 | 80 | 1 | R |
| `docs/apply/master/preload.md` | 910 | 34 | 1 | R |
| `docs/apply/master/prepare_shutdown.md` | 939 | 33 | 1 | R |
| `docs/apply/master/privs_file.md` | 659 | 30 | 1 | R |
| `docs/apply/master/retrieve_ed_setup.md` | 465 | 26 | 1 | R |
| `docs/apply/master/save_ed_setup.md` | 492 | 25 | 1 | R |
| `docs/apply/master/valid_bind.md` | 483 | 22 | 1 | R |
| `docs/apply/master/valid_database.md` | 1173 | 36 | 1 | R |
| `docs/apply/master/valid_hide.md` | 747 | 29 | 1 | R |
| `docs/apply/master/valid_link.md` | 524 | 26 | 1 | R |
| `docs/apply/master/valid_object.md` | 738 | 29 | 1 | R |
| `docs/apply/master/valid_override.md` | 1745 | 61 | 1 | R |
| `docs/apply/master/valid_read.md` | 592 | 27 | 1 | R |
| `docs/apply/master/valid_save_binary.md` | 514 | 23 | 1 | R |
| `docs/apply/master/valid_seteuid.md` | 536 | 26 | 1 | R |
| `docs/apply/master/valid_shadow.md` | 684 | 28 | 1 | R |
| `docs/apply/master/valid_socket.md` | 1103 | 31 | 1 | R |
| `docs/apply/master/valid_write.md` | 603 | 27 | 1 | R |
| `docs/apply/object/__INIT.md` | 350 | 21 | 1 | R |
| `docs/apply/object/clean_up.md` | 1120 | 33 | 1 | R |
| `docs/apply/object/create.md` | 1379 | 41 | 1 | R |
| `docs/apply/object/heart_beat.md` | 343 | 23 | 1 | R |
| `docs/apply/object/id.md` | 443 | 25 | 1 | R |
| `docs/apply/object/index.md` | 346 | 16 | 1 | R |
| `docs/apply/object/init.md` | 987 | 39 | 1 | R |
| `docs/apply/object/is_living.md` | 699 | 28 | 1 | R |
| `docs/apply/object/move_or_destruct.md` | 1353 | 40 | 1 | R |
| `docs/apply/object/on_destruct.md` | 484 | 21 | 1 | R |
| `docs/apply/object/reset.md` | 767 | 30 | 1 | R |
| `docs/apply/object/virtual_start.md` | 325 | 24 | 1 | R |
| `docs/archive/Artistic.README` | 628 | 10 | 1 | R |
| `docs/archive/Bugs` | 9007 | 319 | 1 | R |
| `docs/archive/ChangeLog.alpha` | 248909 | 5202 | 6 | R |
| `docs/archive/ChangeLog.beta` | 98916 | 2073 | 3 | R |
| `docs/archive/ChangeLog.fluffos` | 7835 | 221 | 1 | R |
| `docs/archive/ChangeLog.fluffos-2.x` | 24710 | 543 | 1 | R |
| `docs/archive/ChangeLog.mudos` | 105149 | 2218 | 3 | R |
| `docs/archive/Credits.LPmud` | 990 | 32 | 1 | R |
| `docs/archive/Credits.MudOS` | 3692 | 100 | 1 | R |
| `docs/archive/Credits.docs` | 489 | 22 | 1 | R |
| `docs/archive/Email` | 1500 | 35 | 1 | R |
| `docs/archive/FTP` | 329 | 9 | 1 | R |
| `docs/archive/History.MudOS` | 11754 | 200 | 1 | R |
| `docs/archive/INSTALL` | 713 | 17 | 1 | R |
| `docs/archive/IT_CRASHED` | 4506 | 131 | 1 | R |
| `docs/archive/Ideas` | 7901 | 269 | 1 | R |
| `docs/archive/Install.MudOS` | 3598 | 75 | 1 | R |
| `docs/archive/Performance` | 2754 | 54 | 1 | R |
| `docs/archive/Platforms.MudOS` | 1382 | 38 | 1 | R |
| `docs/archive/README` | 2086 | 49 | 1 | R |
| `docs/archive/README.MudOS` | 3468 | 70 | 1 | R |
| `docs/archive/Running.MudOS` | 4531 | 88 | 1 | R |
| `docs/archive/convert.sh` | 190 | 10 | 1 | R |
| `docs/archive/defines/BSDMALLOC` | 306 | 7 | 1 | R |
| `docs/archive/defines/CHECK_MEMORY` | 296 | 4 | 1 | R |
| `docs/archive/defines/DEBUGMALLOC` | 165 | 3 | 1 | R |
| `docs/archive/defines/DEBUGMALLOC_EXTENSIONS` | 303 | 6 | 1 | R |
| `docs/archive/defines/DO_MSTATS` | 151 | 3 | 1 | R |
| `docs/archive/defines/HAS_STATUS_TYPE` | 250 | 7 | 1 | R |
| `docs/archive/defines/README-DEFINES` | 220 | 3 | 1 | R |
| `docs/archive/defines/SMALLOC` | 214 | 4 | 1 | R |
| `docs/archive/defines/SYSMALLOC` | 402 | 10 | 1 | R |
| `docs/archive/defines/WRAPPEDMALLOC` | 123 | 3 | 1 | R |
| `docs/archive/defines/lpc/HAS_CACHE_STATS` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_DEBUGMALLOC` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_DEBUG_LEVEL` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_EACH` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_ED` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_LIGHT` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_MATH` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_MATRIX` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_MUDLIB_ERROR_HANDLER` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_MUDLIB_STATS` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_OPCPROF` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_PRINTF` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_PRIVS` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_RUSAGE` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_SHADOWS` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/HAS_SOCKETS` | 0 | 1 | 1 | R |
| `docs/archive/defines/lpc/LPC3` | 16 | 2 | 1 | R |
| `docs/archive/defines/lpc/MUDOS` | 16 | 2 | 1 | R |
| `docs/archive/defines/lpc/MUD_NAME` | 77 | 2 | 1 | R |
| `docs/archive/defines/lpc/README` | 166 | 4 | 1 | R |
| `docs/archive/defines/lpc/SAVE_EXTENSION` | 134 | 3 | 1 | R |
| `docs/archive/defines/lpc/USE_EUID` | 16 | 2 | 1 | R |
| `docs/archive/defines/lpc/__ARCH__` | 91 | 3 | 1 | R |
| `docs/archive/defines/lpc/__DIR__` | 56 | 2 | 1 | R |
| `docs/archive/defines/lpc/__FILE__` | 31 | 3 | 1 | R |
| `docs/archive/defines/lpc/__PORT__` | 77 | 2 | 1 | R |
| `docs/archive/defines/lpc/__VERSION__` | 78 | 2 | 1 | R |
| `docs/archive/driver/done-lars` | 45999 | 1226 | 2 | R |
| `docs/archive/driver/done-mudos` | 43457 | 1002 | 2 | R |
| `docs/archive/man2ascii.sh` | 98 | 6 | 1 | R |
| `docs/archive/multicore/README.md` | 533 | 14 | 1 | R |
| `docs/archive/multicore/multicore-actor-vm-plan-2026-06.md` | 18098 | 440 | 1 | R |
| `docs/archive/multicore/multicore-refactor-execution-plan-2026-06.md` | 81057 | 400 | 1 | R |
| `docs/archive/multicore/multicore-runtime-v1-2026-06.md` | 46509 | 235 | 1 | R |
| `docs/archive/platforms/Alpha-OSF1` | 311 | 10 | 1 | R |
| `docs/archive/platforms/DEC-Ultrix` | 367 | 13 | 1 | R |
| `docs/archive/platforms/HP-UX` | 465 | 15 | 1 | R |
| `docs/archive/platforms/Irix` | 104 | 5 | 1 | R |
| `docs/archive/platforms/Linux` | 237 | 5 | 1 | R |
| `docs/archive/platforms/NeXT` | 97 | 6 | 1 | R |
| `docs/archive/platforms/RS6000` | 270 | 10 | 1 | R |
| `docs/backlog-full-plan-2026-09.md` | 21042 | 324 | 1 | R |
| `docs/backlog-plan-2026-09.md` | 7399 | 114 | 1 | R |
| `docs/bug.md` | 1893 | 69 | 1 | R |
| `docs/build.md` | 13288 | 467 | 1 | R / E:AUD-024 |
| `docs/build_v2017.md` | 2892 | 103 | 1 | R |
| `docs/cli/driver.md` | 1916 | 40 | 1 | R |
| `docs/cli/generate_keywords.md` | 2828 | 116 | 1 | R |
| `docs/cli/index.md` | 221 | 12 | 1 | R |
| `docs/cli/json2o.md` | 461 | 26 | 1 | R |
| `docs/cli/lpcc.md` | 53757 | 967 | 1 | R |
| `docs/cli/o2json.md` | 1328 | 57 | 1 | R |
| `docs/cli/portbind.md` | 2637 | 69 | 1 | R |
| `docs/cli/symbol.md` | 1329 | 48 | 1 | R |
| `docs/coding-standard.md` | 26496 | 320 | 1 | R / E:AUD-023 |
| `docs/concepts/general/MudOSdriver.md` | 1419 | 35 | 1 | R |
| `docs/concepts/general/async.md` | 3560 | 84 | 1 | R |
| `docs/concepts/general/global_include_file.md` | 345 | 12 | 1 | R |
| `docs/concepts/general/index.md` | 415 | 18 | 1 | R |
| `docs/concepts/general/lpc.md` | 2651 | 68 | 1 | R |
| `docs/concepts/general/message_doc.md` | 9108 | 219 | 1 | R |
| `docs/concepts/general/objects.md` | 761 | 21 | 1 | R |
| `docs/concepts/general/oop.md` | 1501 | 39 | 1 | R |
| `docs/concepts/general/preprocessor.md` | 8044 | 328 | 1 | R |
| `docs/concepts/general/simul_efun.md` | 1820 | 37 | 1 | R |
| `docs/concepts/general/socket_efuns.md` | 42582 | 790 | 1 | R |
| `docs/concepts/general/tls.md` | 11465 | 440 | 1 | R |
| `docs/concepts/general/tracing.md` | 10889 | 419 | 1 | R |
| `docs/concepts/general/websocket.md` | 12568 | 550 | 1 | R |
| `docs/concepts/index.md` | 501 | 18 | 1 | R |
| `docs/css/default.css` | 895 | 57 | 1 | R |
| `docs/css/rouge.css` | 2597 | 197 | 1 | R |
| `docs/driver/adding_efuns.md` | 2375 | 57 | 1 | R |
| `docs/driver/call_into_vm.md` | 1816 | 66 | 1 | R |
| `docs/driver/config.md` | 7167 | 238 | 1 | R |
| `docs/driver/index.md` | 222 | 11 | 1 | R |
| `docs/driver/malloc.md` | 1533 | 29 | 1 | R |
| `docs/driver/parse_tree.md` | 1955 | 131 | 1 | R |
| `docs/driver/stackmachine.md` | 4841 | 158 | 1 | R |
| `docs/e4-read-source-line-port-eval-2026-08.md` | 4990 | 79 | 1 | R |
| `docs/efun/arrays/allocate.md` | 813 | 33 | 1 | R |
| `docs/efun/arrays/arrayp.md` | 508 | 29 | 1 | R |
| `docs/efun/arrays/element_of.md` | 243 | 18 | 1 | R |
| `docs/efun/arrays/filter_array.md` | 971 | 31 | 1 | R |
| `docs/efun/arrays/index.md` | 342 | 15 | 1 | R |
| `docs/efun/arrays/map_array.md` | 908 | 33 | 1 | R |
| `docs/efun/arrays/member_array.md` | 1508 | 56 | 1 | R |
| `docs/efun/arrays/pointerp.md` | 522 | 29 | 1 | R |
| `docs/efun/arrays/shuffle.md` | 232 | 18 | 1 | R |
| `docs/efun/arrays/sort_array.md` | 1316 | 40 | 1 | R |
| `docs/efun/arrays/unique_array.md` | 1932 | 85 | 1 | R |
| `docs/efun/async/async_db_exec.md` | 784 | 39 | 1 | R |
| `docs/efun/async/async_getdir.md` | 992 | 37 | 1 | R |
| `docs/efun/async/async_read.md` | 747 | 33 | 1 | R |
| `docs/efun/async/async_write.md` | 788 | 35 | 1 | R |
| `docs/efun/async/index.md` | 173 | 9 | 1 | R |
| `docs/efun/buffers/allocate_buffer.md` | 494 | 28 | 1 | R |
| `docs/efun/buffers/buffer_transcode.md` | 465 | 25 | 1 | R |
| `docs/efun/buffers/bufferp.md` | 531 | 29 | 1 | R |
| `docs/efun/buffers/crc32.md` | 284 | 19 | 1 | R |
| `docs/efun/buffers/index.md` | 239 | 11 | 1 | R |
| `docs/efun/buffers/read_buffer.md` | 1056 | 41 | 1 | R |
| `docs/efun/buffers/write_buffer.md` | 807 | 36 | 1 | R |
| `docs/efun/calls/call_other.md` | 1543 | 51 | 1 | R |
| `docs/efun/calls/call_out.md` | 1811 | 51 | 1 | R |
| `docs/efun/calls/call_out_walltime.md` | 565 | 25 | 1 | R |
| `docs/efun/calls/call_out_walltime_background.md` | 885 | 28 | 1 | R |
| `docs/efun/calls/call_out_walltime_gateway.md` | 874 | 28 | 1 | R |
| `docs/efun/calls/call_stack.md` | 773 | 29 | 1 | R |
| `docs/efun/calls/catch.md` | 1346 | 57 | 1 | R |
| `docs/efun/calls/index.md` | 553 | 19 | 1 | R |
| `docs/efun/calls/origin.md` | 703 | 32 | 1 | R |
| `docs/efun/calls/previous_object.md` | 837 | 30 | 1 | R |
| `docs/efun/calls/query_shadowing.md` | 395 | 25 | 1 | R |
| `docs/efun/calls/remove_call_out.md` | 1132 | 37 | 1 | R |
| `docs/efun/calls/shadow.md` | 1894 | 50 | 1 | R |
| `docs/efun/calls/this_object.md` | 473 | 25 | 1 | R |
| `docs/efun/calls/throw.md` | 759 | 36 | 1 | R |
| `docs/efun/contrib/abs.md` | 413 | 25 | 1 | R |
| `docs/efun/contrib/assemble_class.md` | 1304 | 66 | 1 | R |
| `docs/efun/contrib/base_name.md` | 836 | 39 | 1 | R |
| `docs/efun/contrib/classes.md` | 793 | 46 | 1 | R |
| `docs/efun/contrib/compressedp.md` | 305 | 19 | 1 | R |
| `docs/efun/contrib/copy.md` | 473 | 29 | 1 | R |
| `docs/efun/contrib/debug_message.md` | 304 | 20 | 1 | R |
| `docs/efun/contrib/disassemble_class.md` | 952 | 52 | 1 | R |
| `docs/efun/contrib/element_of.md` | 242 | 18 | 1 | R |
| `docs/efun/contrib/event.md` | 1591 | 53 | 1 | R |
| `docs/efun/contrib/fetch_class_member.md` | 762 | 35 | 1 | R |
| `docs/efun/contrib/fetch_variable.md` | 948 | 40 | 1 | R |
| `docs/efun/contrib/file_length.md` | 395 | 26 | 1 | R |
| `docs/efun/contrib/function_owner.md` | 274 | 18 | 1 | R |
| `docs/efun/contrib/functions.md` | 1274 | 50 | 1 | R |
| `docs/efun/contrib/get_garbage.md` | 433 | 21 | 1 | R |
| `docs/efun/contrib/heart_beats.md` | 349 | 22 | 1 | R |
| `docs/efun/contrib/index.md` | 1865 | 56 | 1 | R |
| `docs/efun/contrib/is_daylight_savings_time.md` | 321 | 20 | 1 | R |
| `docs/efun/contrib/max.md` | 413 | 25 | 1 | R |
| `docs/efun/contrib/memory_summary.md` | 296 | 25 | 1 | R |
| `docs/efun/contrib/min.md` | 413 | 25 | 1 | R |
| `docs/efun/contrib/named_livings.md` | 372 | 23 | 1 | R |
| `docs/efun/contrib/network_stats.md` | 681 | 39 | 1 | R |
| `docs/efun/contrib/num_classes.md` | 282 | 22 | 1 | R |
| `docs/efun/contrib/pluralize.md` | 326 | 26 | 1 | R |
| `docs/efun/contrib/program_info.md` | 436 | 29 | 1 | R |
| `docs/efun/contrib/query_charmode.md` | 436 | 25 | 1 | R |
| `docs/efun/contrib/query_ip_port.md` | 312 | 20 | 1 | R |
| `docs/efun/contrib/query_notify_fail.md` | 260 | 19 | 1 | R |
| `docs/efun/contrib/query_num.md` | 1099 | 62 | 1 | R |
| `docs/efun/contrib/query_replaced_program.md` | 355 | 21 | 1 | R |
| `docs/efun/contrib/real_time.md` | 199 | 22 | 1 | R |
| `docs/efun/contrib/remove_charmode.md` | 473 | 26 | 1 | R |
| `docs/efun/contrib/remove_get_char.md` | 543 | 31 | 1 | R |
| `docs/efun/contrib/remove_interactive.md` | 559 | 26 | 1 | R |
| `docs/efun/contrib/remove_shadow.md` | 616 | 29 | 1 | R |
| `docs/efun/contrib/repeat_string.md` | 546 | 28 | 1 | R |
| `docs/efun/contrib/replaceable.md` | 552 | 28 | 1 | R |
| `docs/efun/contrib/restore_from_string.md` | 441 | 23 | 1 | R |
| `docs/efun/contrib/roll_MdN.md` | 608 | 33 | 1 | R |
| `docs/efun/contrib/send_nullbyte.md` | 263 | 18 | 1 | R |
| `docs/efun/contrib/set_prompt.md` | 166 | 12 | 1 | R |
| `docs/efun/contrib/shuffle.md` | 223 | 18 | 1 | R |
| `docs/efun/contrib/store_class_member.md` | 1074 | 61 | 1 | R |
| `docs/efun/contrib/store_variable.md` | 939 | 37 | 1 | R |
| `docs/efun/contrib/string_difference.md` | 255 | 18 | 1 | R |
| `docs/efun/contrib/terminal_colour.md` | 858 | 37 | 1 | R |
| `docs/efun/contrib/test_load.md` | 455 | 22 | 1 | R |
| `docs/efun/contrib/upper_case.md` | 453 | 27 | 1 | R |
| `docs/efun/contrib/variables.md` | 363 | 26 | 1 | R |
| `docs/efun/contrib/zonetime.md` | 293 | 22 | 1 | R |
| `docs/efun/crypto/hash.md` | 3562 | 118 | 1 | R |
| `docs/efun/crypto/index.md` | 54 | 6 | 1 | R |
| `docs/efun/db/db_close.md` | 349 | 25 | 1 | R |
| `docs/efun/db/db_commit.md` | 393 | 29 | 1 | R |
| `docs/efun/db/db_connect.md` | 1849 | 53 | 1 | R |
| `docs/efun/db/db_exec.md` | 477 | 28 | 1 | R |
| `docs/efun/db/db_fetch.md` | 990 | 51 | 1 | R |
| `docs/efun/db/db_rollback.md` | 396 | 29 | 1 | R |
| `docs/efun/db/db_status.md` | 310 | 23 | 1 | R |
| `docs/efun/db/index.md` | 238 | 12 | 1 | R |
| `docs/efun/ed/ed_cmd.md` | 533 | 26 | 1 | R |
| `docs/efun/ed/ed_start.md` | 642 | 28 | 1 | R |
| `docs/efun/ed/index.md` | 120 | 8 | 1 | R |
| `docs/efun/ed/query_ed_mode.md` | 570 | 29 | 1 | R |
| `docs/efun/external/external_close.md` | 551 | 24 | 1 | R |
| `docs/efun/external/external_close_stdin.md` | 615 | 26 | 1 | R |
| `docs/efun/external/external_create.md` | 1384 | 45 | 1 | R |
| `docs/efun/external/external_exit_code.md` | 601 | 25 | 1 | R |
| `docs/efun/external/external_kill.md` | 1356 | 44 | 1 | R |
| `docs/efun/external/external_run.md` | 1951 | 55 | 1 | R |
| `docs/efun/external/external_start.md` | 5216 | 164 | 1 | R |
| `docs/efun/external/external_stderr.md` | 383 | 22 | 1 | R |
| `docs/efun/external/external_stdout.md` | 636 | 26 | 1 | R |
| `docs/efun/external/external_write.md` | 1260 | 42 | 1 | R |
| `docs/efun/filesystem/cp.md` | 345 | 27 | 1 | R |
| `docs/efun/filesystem/file_size.md` | 430 | 25 | 1 | R |
| `docs/efun/filesystem/get_dir.md` | 1096 | 36 | 1 | R |
| `docs/efun/filesystem/index.md` | 424 | 20 | 1 | R |
| `docs/efun/filesystem/link.md` | 712 | 30 | 1 | R |
| `docs/efun/filesystem/mkdir.md` | 282 | 23 | 1 | R |
| `docs/efun/filesystem/read_bytes.md` | 825 | 31 | 1 | R |
| `docs/efun/filesystem/read_file.md` | 841 | 31 | 1 | R |
| `docs/efun/filesystem/read_json.md` | 766 | 31 | 1 | R |
| `docs/efun/filesystem/rename.md` | 366 | 28 | 1 | R |
| `docs/efun/filesystem/rm.md` | 257 | 23 | 1 | R |
| `docs/efun/filesystem/rmdir.md` | 307 | 27 | 1 | R |
| `docs/efun/filesystem/stat.md` | 675 | 30 | 1 | R |
| `docs/efun/filesystem/write_bytes.md` | 568 | 26 | 1 | R |
| `docs/efun/filesystem/write_file.md` | 944 | 34 | 1 | R |
| `docs/efun/filesystem/write_json.md` | 721 | 30 | 1 | R |
| `docs/efun/floats/acos.md` | 294 | 23 | 1 | R |
| `docs/efun/floats/asin.md` | 290 | 23 | 1 | R |
| `docs/efun/floats/atan.md` | 295 | 22 | 1 | R |
| `docs/efun/floats/ceil.md` | 313 | 24 | 1 | R |
| `docs/efun/floats/cos.md` | 285 | 23 | 1 | R |
| `docs/efun/floats/exp.md` | 226 | 23 | 1 | R |
| `docs/efun/floats/floatp.md` | 402 | 24 | 1 | R |
| `docs/efun/floats/floor.md` | 318 | 24 | 1 | R |
| `docs/efun/floats/index.md` | 334 | 20 | 1 | R |
| `docs/efun/floats/log.md` | 301 | 24 | 1 | R |
| `docs/efun/floats/pow.md` | 324 | 24 | 1 | R |
| `docs/efun/floats/round.md` | 237 | 22 | 1 | R |
| `docs/efun/floats/sin.md` | 281 | 23 | 1 | R |
| `docs/efun/floats/sqrt.md` | 325 | 24 | 1 | R |
| `docs/efun/floats/tan.md` | 287 | 23 | 1 | R |
| `docs/efun/floats/to_int.md` | 946 | 39 | 1 | R |
| `docs/efun/functions/bind.md` | 964 | 37 | 1 | R |
| `docs/efun/functions/defer.md` | 632 | 34 | 1 | R |
| `docs/efun/functions/evaluate.md` | 853 | 44 | 1 | R |
| `docs/efun/functions/functionp.md` | 1715 | 60 | 1 | R |
| `docs/efun/functions/index.md` | 137 | 9 | 1 | R |
| `docs/efun/general/act_mxp.md` | 139 | 19 | 1 | R |
| `docs/efun/general/angle.md` | 164 | 19 | 1 | R |
| `docs/efun/general/check_memory.md` | 164 | 19 | 1 | R |
| `docs/efun/general/classp.md` | 585 | 35 | 1 | R |
| `docs/efun/general/clear_debug_level.md` | 680 | 45 | 1 | R |
| `docs/efun/general/compress.md` | 160 | 19 | 1 | R |
| `docs/efun/general/compress_file.md` | 1139 | 33 | 1 | R |
| `docs/efun/general/destructed_objects.md` | 186 | 19 | 1 | R |
| `docs/efun/general/distance.md` | 176 | 19 | 1 | R |
| `docs/efun/general/dotprod.md` | 172 | 19 | 1 | R |
| `docs/efun/general/dump_jemalloc.md` | 163 | 19 | 1 | R |
| `docs/efun/general/dump_stralloc.md` | 171 | 19 | 1 | R |
| `docs/efun/general/dump_trace.md` | 154 | 19 | 1 | R |
| `docs/efun/general/explode_reversible.md` | 201 | 19 | 1 | R |
| `docs/efun/general/filter.md` | 2540 | 90 | 1 | R |
| `docs/efun/general/id_matrix.md` | 150 | 19 | 1 | R |
| `docs/efun/general/index.md` | 1643 | 52 | 1 | R |
| `docs/efun/general/json_encode_frozen.md` | 828 | 32 | 1 | R |
| `docs/efun/general/json_encode_frozen_or_zero.md` | 854 | 31 | 1 | R |
| `docs/efun/general/log10.md` | 143 | 19 | 1 | R |
| `docs/efun/general/log2.md` | 139 | 19 | 1 | R |
| `docs/efun/general/lookat_rotate.md` | 194 | 19 | 1 | R |
| `docs/efun/general/lookat_rotate2.md` | 219 | 19 | 1 | R |
| `docs/efun/general/map.md` | 888 | 37 | 1 | R |
| `docs/efun/general/next_bit.md` | 153 | 19 | 1 | R |
| `docs/efun/general/norm.md` | 143 | 19 | 1 | R |
| `docs/efun/general/nullp.md` | 596 | 31 | 1 | R |
| `docs/efun/general/parse_add_synonym.md` | 201 | 19 | 1 | R |
| `docs/efun/general/parse_dump.md` | 153 | 19 | 1 | R |
| `docs/efun/general/parse_my_rules.md` | 187 | 19 | 1 | R |
| `docs/efun/general/parse_remove.md` | 165 | 19 | 1 | R |
| `docs/efun/general/request_term_size.md` | 182 | 19 | 1 | R |
| `docs/efun/general/request_term_type.md` | 179 | 19 | 1 | R |
| `docs/efun/general/restore_variable.md` | 382 | 24 | 1 | R |
| `docs/efun/general/rotate_x.md` | 160 | 19 | 1 | R |
| `docs/efun/general/rotate_y.md` | 160 | 19 | 1 | R |
| `docs/efun/general/rotate_z.md` | 160 | 19 | 1 | R |
| `docs/efun/general/save_variable.md` | 349 | 24 | 1 | R |
| `docs/efun/general/scale.md` | 162 | 19 | 1 | R |
| `docs/efun/general/send_zmp.md` | 159 | 19 | 1 | R |
| `docs/efun/general/shallow_inherit_list.md` | 201 | 19 | 1 | R |
| `docs/efun/general/sizeof.md` | 453 | 26 | 1 | R |
| `docs/efun/general/start_request_term_type.md` | 203 | 19 | 1 | R |
| `docs/efun/general/translate.md` | 178 | 19 | 1 | R |
| `docs/efun/general/typeof.md` | 903 | 43 | 1 | R |
| `docs/efun/general/uncompress.md` | 168 | 19 | 1 | R |
| `docs/efun/general/uncompress_file.md` | 1417 | 37 | 1 | R |
| `docs/efun/general/undefinedp.md` | 767 | 32 | 1 | R |
| `docs/efun/index.md` | 16706 | 430 | 1 | R |
| `docs/efun/interactive/add_action.md` | 1539 | 46 | 1 | R |
| `docs/efun/interactive/command.md` | 745 | 29 | 1 | R |
| `docs/efun/interactive/commands.md` | 718 | 28 | 1 | R |
| `docs/efun/interactive/disable_commands.md` | 683 | 30 | 1 | R |
| `docs/efun/interactive/disable_wizard.md` | 358 | 24 | 1 | R |
| `docs/efun/interactive/ed.md` | 1806 | 53 | 1 | R |
| `docs/efun/interactive/enable_commands.md` | 1333 | 46 | 1 | R |
| `docs/efun/interactive/exec.md` | 1037 | 33 | 1 | R |
| `docs/efun/interactive/find_player.md` | 402 | 24 | 1 | R |
| `docs/efun/interactive/get_char.md` | 1743 | 51 | 1 | R |
| `docs/efun/interactive/has_gmcp.md` | 607 | 32 | 1 | R |
| `docs/efun/interactive/has_msdp.md` | 344 | 23 | 1 | R |
| `docs/efun/interactive/has_msp.md` | 328 | 23 | 1 | R |
| `docs/efun/interactive/has_mxp.md` | 317 | 23 | 1 | R |
| `docs/efun/interactive/has_zmp.md` | 316 | 23 | 1 | R |
| `docs/efun/interactive/in_edit.md` | 352 | 24 | 1 | R |
| `docs/efun/interactive/in_input.md` | 360 | 24 | 1 | R |
| `docs/efun/interactive/index.md` | 1573 | 54 | 1 | R |
| `docs/efun/interactive/input_to.md` | 1294 | 42 | 1 | R |
| `docs/efun/interactive/interactive.md` | 395 | 24 | 1 | R |
| `docs/efun/interactive/message.md` | 1394 | 46 | 1 | R |
| `docs/efun/interactive/notify_fail.md` | 1285 | 38 | 1 | R |
| `docs/efun/interactive/printf.md` | 4180 | 123 | 1 | R |
| `docs/efun/interactive/query_encoding.md` | 415 | 22 | 1 | R |
| `docs/efun/interactive/query_host_name.md` | 467 | 31 | 1 | R |
| `docs/efun/interactive/query_idle.md` | 341 | 24 | 1 | R |
| `docs/efun/interactive/query_ip_name.md` | 942 | 35 | 1 | R |
| `docs/efun/interactive/query_ip_number.md` | 399 | 24 | 1 | R |
| `docs/efun/interactive/query_snoop.md` | 408 | 24 | 1 | R |
| `docs/efun/interactive/query_snooping.md` | 409 | 24 | 1 | R |
| `docs/efun/interactive/receive.md` | 552 | 26 | 1 | R |
| `docs/efun/interactive/remove_action.md` | 632 | 34 | 1 | R |
| `docs/efun/interactive/resolve.md` | 950 | 34 | 1 | R |
| `docs/efun/interactive/say.md` | 956 | 34 | 1 | R |
| `docs/efun/interactive/send_gmcp.md` | 508 | 28 | 1 | R |
| `docs/efun/interactive/send_msdp_variable.md` | 621 | 28 | 1 | R |
| `docs/efun/interactive/set_encoding.md` | 1927 | 45 | 1 | R |
| `docs/efun/interactive/set_this_player.md` | 678 | 28 | 1 | R |
| `docs/efun/interactive/set_this_user.md` | 469 | 24 | 1 | R |
| `docs/efun/interactive/shout.md` | 478 | 26 | 1 | R |
| `docs/efun/interactive/snoop.md` | 678 | 27 | 1 | R |
| `docs/efun/interactive/telnet_ga.md` | 316 | 19 | 1 | R |
| `docs/efun/interactive/telnet_msp_oob.md` | 385 | 22 | 1 | R |
| `docs/efun/interactive/telnet_nop.md` | 293 | 19 | 1 | R |
| `docs/efun/interactive/this_interactive.md` | 674 | 29 | 1 | R |
| `docs/efun/interactive/this_player.md` | 1043 | 34 | 1 | R |
| `docs/efun/interactive/this_user.md` | 563 | 27 | 1 | R |
| `docs/efun/interactive/userp.md` | 289 | 22 | 1 | R |
| `docs/efun/interactive/users.md` | 326 | 23 | 1 | R |
| `docs/efun/interactive/write.md` | 518 | 26 | 1 | R |
| `docs/efun/internals/cache_stats.md` | 444 | 25 | 1 | R |
| `docs/efun/internals/debug_info.md` | 1788 | 72 | 1 | R |
| `docs/efun/internals/debug_levels.md` | 902 | 48 | 1 | R |
| `docs/efun/internals/debugmalloc.md` | 705 | 28 | 1 | R |
| `docs/efun/internals/dump_file_descriptors.md` | 1523 | 43 | 1 | R |
| `docs/efun/internals/dump_prog.md` | 619 | 28 | 1 | R |
| `docs/efun/internals/dump_socket_status.md` | 1909 | 49 | 1 | R |
| `docs/efun/internals/dumpallobj.md` | 561 | 27 | 1 | R |
| `docs/efun/internals/get_config.md` | 548 | 32 | 1 | R |
| `docs/efun/internals/index.md` | 857 | 29 | 1 | R |
| `docs/efun/internals/malloc_status.md` | 599 | 27 | 1 | R |
| `docs/efun/internals/memory_info.md` | 690 | 28 | 1 | R |
| `docs/efun/internals/moncontrol.md` | 567 | 27 | 1 | R |
| `docs/efun/internals/mud_status.md` | 547 | 26 | 1 | R |
| `docs/efun/internals/opcprof.md` | 549 | 26 | 1 | R |
| `docs/efun/internals/query_load_average.md` | 392 | 24 | 1 | R |
| `docs/efun/internals/refs.md` | 446 | 25 | 1 | R |
| `docs/efun/internals/rusage.md` | 1123 | 45 | 1 | R |
| `docs/efun/internals/set_config.md` | 800 | 37 | 1 | R |
| `docs/efun/internals/set_debug_level.md` | 1664 | 60 | 1 | R |
| `docs/efun/internals/set_malloc_mask.md` | 601 | 28 | 1 | R |
| `docs/efun/internals/swap.md` | 537 | 24 | 1 | R |
| `docs/efun/internals/time_expression.md` | 549 | 27 | 1 | R |
| `docs/efun/internals/trace.md` | 776 | 43 | 1 | R |
| `docs/efun/internals/traceprefix.md` | 373 | 24 | 1 | R |
| `docs/efun/mappings/allocate_mapping.md` | 2159 | 72 | 1 | R |
| `docs/efun/mappings/filter_mapping.md` | 840 | 32 | 1 | R |
| `docs/efun/mappings/index.md` | 322 | 14 | 1 | R |
| `docs/efun/mappings/keys.md` | 716 | 38 | 1 | R |
| `docs/efun/mappings/map_delete.md` | 884 | 46 | 1 | R |
| `docs/efun/mappings/map_mapping.md` | 842 | 33 | 1 | R |
| `docs/efun/mappings/mapp.md` | 370 | 24 | 1 | R |
| `docs/efun/mappings/match_path.md` | 567 | 26 | 1 | R |
| `docs/efun/mappings/unique_mapping.md` | 823 | 31 | 1 | R |
| `docs/efun/mappings/values.md` | 635 | 38 | 1 | R |
| `docs/efun/mudlib/author_stats.md` | 2026 | 52 | 1 | R |
| `docs/efun/mudlib/domain_stats.md` | 2445 | 60 | 1 | R |
| `docs/efun/mudlib/enable_wizard.md` | 704 | 32 | 1 | R |
| `docs/efun/mudlib/export_uid.md` | 425 | 25 | 1 | R |
| `docs/efun/mudlib/find_living.md` | 644 | 28 | 1 | R |
| `docs/efun/mudlib/geteuid.md` | 689 | 28 | 1 | R |
| `docs/efun/mudlib/getuid.md` | 459 | 25 | 1 | R |
| `docs/efun/mudlib/index.md` | 530 | 21 | 1 | R |
| `docs/efun/mudlib/living.md` | 351 | 24 | 1 | R |
| `docs/efun/mudlib/livings.md` | 377 | 24 | 1 | R |
| `docs/efun/mudlib/query_privs.md` | 519 | 27 | 1 | R |
| `docs/efun/mudlib/set_author.md` | 1427 | 40 | 1 | R |
| `docs/efun/mudlib/set_light.md` | 413 | 21 | 1 | R |
| `docs/efun/mudlib/set_living_name.md` | 420 | 24 | 1 | R |
| `docs/efun/mudlib/set_privs.md` | 376 | 25 | 1 | R |
| `docs/efun/mudlib/seteuid.md` | 605 | 30 | 1 | R |
| `docs/efun/mudlib/wizardp.md` | 329 | 24 | 1 | R |
| `docs/efun/numbers/index.md` | 145 | 9 | 1 | R |
| `docs/efun/numbers/intp.md` | 401 | 24 | 1 | R |
| `docs/efun/numbers/random.md` | 314 | 20 | 1 | R |
| `docs/efun/numbers/secure_random.md` | 474 | 22 | 1 | R |
| `docs/efun/numbers/to_float.md` | 343 | 24 | 1 | R |
| `docs/efun/objects/all_inventory.md` | 424 | 25 | 1 | R |
| `docs/efun/objects/children.md` | 731 | 33 | 1 | R |
| `docs/efun/objects/clone_object.md` | 748 | 31 | 1 | R |
| `docs/efun/objects/clonep.md` | 819 | 31 | 1 | R |
| `docs/efun/objects/deep_inventory.md` | 526 | 27 | 1 | R |
| `docs/efun/objects/destruct.md` | 598 | 28 | 1 | R |
| `docs/efun/objects/environment.md` | 354 | 24 | 1 | R |
| `docs/efun/objects/file_name.md` | 756 | 26 | 1 | R |
| `docs/efun/objects/find_object.md` | 635 | 30 | 1 | R |
| `docs/efun/objects/first_inventory.md` | 508 | 26 | 1 | R |
| `docs/efun/objects/index.md` | 1017 | 34 | 1 | R |
| `docs/efun/objects/load_object.md` | 560 | 29 | 1 | R |
| `docs/efun/objects/master.md` | 244 | 23 | 1 | R |
| `docs/efun/objects/move_object.md` | 365 | 23 | 1 | R |
| `docs/efun/objects/new.md` | 1361 | 53 | 1 | R |
| `docs/efun/objects/next_inventory.md` | 501 | 26 | 1 | R |
| `docs/efun/objects/objectp.md` | 379 | 24 | 1 | R |
| `docs/efun/objects/objects.md` | 882 | 34 | 1 | R |
| `docs/efun/objects/present.md` | 1134 | 41 | 1 | R |
| `docs/efun/objects/query_heart_beat.md` | 554 | 27 | 1 | R |
| `docs/efun/objects/query_notify_destruct.md` | 997 | 35 | 1 | R |
| `docs/efun/objects/recompile_object.md` | 3284 | 71 | 1 | R |
| `docs/efun/objects/reload_object.md` | 805 | 31 | 1 | R |
| `docs/efun/objects/restore_object.md` | 893 | 34 | 1 | R |
| `docs/efun/objects/save_object.md` | 1325 | 41 | 1 | R |
| `docs/efun/objects/set_heart_beat.md` | 737 | 29 | 1 | R |
| `docs/efun/objects/set_hide.md` | 656 | 28 | 1 | R |
| `docs/efun/objects/set_notify_destruct.md` | 916 | 32 | 1 | R |
| `docs/efun/objects/tell_object.md` | 462 | 25 | 1 | R |
| `docs/efun/objects/tell_room.md` | 447 | 21 | 1 | R |
| `docs/efun/objects/virtualp.md` | 541 | 27 | 1 | R |
| `docs/efun/parsing/index.md` | 487 | 17 | 1 | R |
| `docs/efun/parsing/parse_add_rule.md` | 1404 | 56 | 1 | R |
| `docs/efun/parsing/parse_add_synonym.md` | 557 | 31 | 1 | R |
| `docs/efun/parsing/parse_command.md` | 7832 | 190 | 1 | R |
| `docs/efun/parsing/parse_dump.md` | 678 | 30 | 1 | R |
| `docs/efun/parsing/parse_init.md` | 559 | 28 | 1 | R |
| `docs/efun/parsing/parse_my_rules.md` | 553 | 28 | 1 | R |
| `docs/efun/parsing/parse_refresh.md` | 999 | 38 | 1 | R |
| `docs/efun/parsing/parse_remove.md` | 574 | 28 | 1 | R |
| `docs/efun/parsing/parse_sentence.md` | 2549 | 75 | 1 | R |
| `docs/efun/parsing/process_string.md` | 1666 | 64 | 1 | R |
| `docs/efun/parsing/process_value.md` | 1306 | 52 | 1 | R |
| `docs/efun/parsing/query_verb.md` | 575 | 28 | 1 | R |
| `docs/efun/pcre/index.md` | 576 | 19 | 1 | R |
| `docs/efun/pcre/pcre_assoc.md` | 537 | 27 | 1 | R |
| `docs/efun/pcre/pcre_cache.md` | 314 | 22 | 1 | R |
| `docs/efun/pcre/pcre_extract.md` | 951 | 35 | 1 | R |
| `docs/efun/pcre/pcre_match.md` | 662 | 28 | 1 | R |
| `docs/efun/pcre/pcre_match_all.md` | 1107 | 44 | 1 | R |
| `docs/efun/pcre/pcre_replace.md` | 613 | 27 | 1 | R |
| `docs/efun/pcre/pcre_replace_callback.md` | 758 | 27 | 1 | R |
| `docs/efun/pcre/pcre_version.md` | 594 | 26 | 1 | R |
| `docs/efun/promises/promise_reject.md` | 2714 | 56 | 1 | R |
| `docs/efun/sockets/index.md` | 581 | 19 | 1 | R |
| `docs/efun/sockets/socket_accept.md` | 2767 | 87 | 1 | R |
| `docs/efun/sockets/socket_acquire.md` | 1604 | 45 | 1 | R |
| `docs/efun/sockets/socket_address.md` | 649 | 37 | 1 | R |
| `docs/efun/sockets/socket_bind.md` | 1008 | 52 | 1 | R |
| `docs/efun/sockets/socket_close.md` | 616 | 41 | 1 | R |
| `docs/efun/sockets/socket_connect.md` | 1920 | 80 | 1 | R |
| `docs/efun/sockets/socket_create.md` | 2815 | 85 | 1 | R |
| `docs/efun/sockets/socket_error.md` | 543 | 34 | 1 | R |
| `docs/efun/sockets/socket_get_option.md` | 4850 | 166 | 1 | R |
| `docs/efun/sockets/socket_listen.md` | 1743 | 67 | 1 | R |
| `docs/efun/sockets/socket_release.md` | 1826 | 47 | 1 | R |
| `docs/efun/sockets/socket_set_option.md` | 6504 | 225 | 1 | R |
| `docs/efun/sockets/socket_status.md` | 616 | 33 | 1 | R |
| `docs/efun/sockets/socket_write.md` | 1432 | 67 | 1 | R |
| `docs/efun/strings/capitalize.md` | 303 | 24 | 1 | R |
| `docs/efun/strings/clear_bit.md` | 358 | 24 | 1 | R |
| `docs/efun/strings/crypt.md` | 1004 | 47 | 1 | R |
| `docs/efun/strings/explode.md` | 509 | 29 | 1 | R |
| `docs/efun/strings/hash.md` | 2404 | 80 | 1 | R |
| `docs/efun/strings/implode.md` | 414 | 25 | 1 | R |
| `docs/efun/strings/index.md` | 737 | 31 | 1 | R |
| `docs/efun/strings/lower_case.md` | 334 | 24 | 1 | R |
| `docs/efun/strings/ltrim.md` | 482 | 27 | 1 | R |
| `docs/efun/strings/oldcrypt.md` | 1053 | 47 | 1 | R |
| `docs/efun/strings/reg_assoc.md` | 1707 | 56 | 1 | R |
| `docs/efun/strings/regexp.md` | 4296 | 90 | 1 | R |
| `docs/efun/strings/replace_string.md` | 1958 | 57 | 1 | R |
| `docs/efun/strings/rtrim.md` | 484 | 27 | 1 | R |
| `docs/efun/strings/set_bit.md` | 565 | 31 | 1 | R |
| `docs/efun/strings/sha1.md` | 508 | 30 | 1 | R |
| `docs/efun/strings/sprintf.md` | 4092 | 121 | 1 | R |
| `docs/efun/strings/sscanf.md` | 2971 | 96 | 1 | R |
| `docs/efun/strings/strcmp.md` | 695 | 28 | 1 | R |
| `docs/efun/strings/string_decode.md` | 276 | 18 | 1 | R |
| `docs/efun/strings/string_encode.md` | 301 | 19 | 1 | R |
| `docs/efun/strings/stringp.md` | 376 | 23 | 1 | R |
| `docs/efun/strings/strlen.md` | 554 | 29 | 1 | R |
| `docs/efun/strings/strsrch.md` | 836 | 32 | 1 | R |
| `docs/efun/strings/strwidth.md` | 568 | 29 | 1 | R |
| `docs/efun/strings/test_bit.md` | 280 | 23 | 1 | R |
| `docs/efun/strings/trim.md` | 621 | 29 | 1 | R |
| `docs/efun/system/all_previous_objects.md` | 523 | 26 | 1 | R |
| `docs/efun/system/call_out_info.md` | 476 | 25 | 1 | R |
| `docs/efun/system/ctime.md` | 435 | 26 | 1 | R |
| `docs/efun/system/deep_inherit_list.md` | 447 | 26 | 1 | R |
| `docs/efun/system/error.md` | 664 | 28 | 1 | R |
| `docs/efun/system/errorp.md` | 640 | 32 | 1 | R |
| `docs/efun/system/eval_cost.md` | 425 | 25 | 1 | R |
| `docs/efun/system/find_call_out.md` | 1403 | 50 | 1 | R |
| `docs/efun/system/flush_messages.md` | 473 | 22 | 1 | R |
| `docs/efun/system/function_exists.md` | 742 | 33 | 1 | R |
| `docs/efun/system/function_profile.md` | 1049 | 40 | 1 | R |
| `docs/efun/system/include_list.md` | 607 | 26 | 1 | R |
| `docs/efun/system/index.md` | 1106 | 36 | 1 | R |
| `docs/efun/system/inherit_list.md` | 554 | 27 | 1 | R |
| `docs/efun/system/inherits.md` | 510 | 27 | 1 | R |
| `docs/efun/system/localtime.md` | 1848 | 48 | 1 | R |
| `docs/efun/system/max_eval_cost.md` | 440 | 24 | 1 | R |
| `docs/efun/system/perf_counter_ns.md` | 363 | 22 | 1 | R |
| `docs/efun/system/reclaim_objects.md` | 683 | 28 | 1 | R |
| `docs/efun/system/replace_program.md` | 708 | 28 | 1 | R |
| `docs/efun/system/request_clean_up.md` | 652 | 28 | 1 | R |
| `docs/efun/system/reset_eval_cost.md` | 445 | 27 | 1 | R |
| `docs/efun/system/set_eval_limit.md` | 615 | 27 | 1 | R |
| `docs/efun/system/set_reset.md` | 494 | 27 | 1 | R |
| `docs/efun/system/shutdown.md` | 1034 | 33 | 1 | R |
| `docs/efun/system/strftime.md` | 323 | 24 | 1 | R |
| `docs/efun/system/strptime.md` | 332 | 24 | 1 | R |
| `docs/efun/system/sys_network_ports.md` | 516 | 29 | 1 | R |
| `docs/efun/system/sys_reload_tls.md` | 700 | 25 | 1 | R |
| `docs/efun/system/time.md` | 323 | 23 | 1 | R |
| `docs/efun/system/time_ns.md` | 352 | 23 | 1 | R |
| `docs/efun/system/trace_end.md` | 543 | 32 | 1 | R |
| `docs/efun/system/trace_start.md` | 773 | 36 | 1 | R |
| `docs/efun/system/uptime.md` | 367 | 25 | 1 | R |
| `docs/evidence/asan-async-lsan.txt` | 32535 | 430 | 1 | R |
| `docs/evidence/asan-ftest-a1-a2-2026-08-19.txt` | 1811307 | 52418 | 53 | R-raw |
| `docs/evidence/asan-ftest-final.txt` | 1752959 | 51489 | 52 | R-raw |
| `docs/evidence/asan-ftest-leaks-full.txt` | 1786058 | 51733 | 52 | R-raw |
| `docs/evidence/asan-osenv-lsan.txt` | 6961 | 209 | 1 | R |
| `docs/evidence/asan-ownercontract-lsan.txt` | 7012 | 209 | 1 | R |
| `docs/evidence/asan-recompile-final.txt` | 7014 | 209 | 1 | R |
| `docs/evidence/asan-sockettls-lsan.txt` | 625660 | 580 | 1 | R |
| `docs/evidence/e2-afl-compile-fuzzer_stats.txt` | 1260 | 44 | 1 | R |
| `docs/evidence/e2-afl-compile.stderr` | 0 | 1 | 1 | R |
| `docs/evidence/e2-afl-compile.stdout` | 4619 | 70 | 1 | R |
| `docs/evidence/e2-afl-restore-fuzzer_stats.txt` | 1261 | 44 | 1 | R |
| `docs/evidence/e2-afl-restore.stderr` | 0 | 1 | 1 | R |
| `docs/evidence/e2-afl-restore.stdout` | 6184 | 75 | 1 | R |
| `docs/evidence/e2-fuzz-compile-invalid.stderr` | 218 | 5 | 1 | R |
| `docs/evidence/e2-fuzz-compile-invalid.stdout` | 997 | 29 | 1 | R |
| `docs/evidence/e2-fuzz-compile-missing.stderr` | 67 | 2 | 1 | R |
| `docs/evidence/e2-fuzz-compile-missing.stdout` | 534 | 20 | 1 | R |
| `docs/evidence/e2-fuzz-compile-mixed.stderr` | 120 | 3 | 1 | R |
| `docs/evidence/e2-fuzz-compile-mixed.stdout` | 806 | 26 | 1 | R |
| `docs/evidence/e2-fuzz-compile-valid.stderr` | 71 | 2 | 1 | R |
| `docs/evidence/e2-fuzz-compile-valid.stdout` | 534 | 20 | 1 | R |
| `docs/evidence/e2-fuzz-restore-invalid.stderr` | 71 | 2 | 1 | R |
| `docs/evidence/e2-fuzz-restore-invalid.stdout` | 534 | 20 | 1 | R |
| `docs/evidence/e2-fuzz-restore-missing.stderr` | 72 | 2 | 1 | R |
| `docs/evidence/e2-fuzz-restore-missing.stdout` | 534 | 20 | 1 | R |
| `docs/evidence/e2-fuzz-restore-mixed.stderr` | 71 | 2 | 1 | R |
| `docs/evidence/e2-fuzz-restore-mixed.stdout` | 534 | 20 | 1 | R |
| `docs/evidence/e2-fuzz-restore-nested.stderr` | 71 | 2 | 1 | R |
| `docs/evidence/e2-fuzz-restore-nested.stdout` | 534 | 20 | 1 | R |
| `docs/evidence/e2-fuzz-restore-nested_mapping_error.stderr` | 71 | 2 | 1 | R |
| `docs/evidence/e2-fuzz-restore-nested_mapping_error.stdout` | 534 | 20 | 1 | R |
| `docs/evidence/e2-fuzz-restore-valid.stderr` | 71 | 2 | 1 | R |
| `docs/evidence/e2-fuzz-restore-valid.stdout` | 534 | 20 | 1 | R |
| `docs/evidence/e2-fuzzing.md` | 1885 | 41 | 1 | R |
| `docs/evidence/e3-v2-phase1-simul-efun-reload.md` | 4474 | 47 | 1 | R |
| `docs/evidence/ftest-final-2026-08-19.txt` | 1811409 | 52861 | 53 | R-raw |
| `docs/evidence/l2-ftest-buildsync.txt` | 1752759 | 51482 | 52 | R-raw |
| `docs/evidence/l2-lpc-tests-buildsync.txt` | 214742 | 790 | 1 | R |
| `docs/evidence/l2-lpc-tests-final.txt` | 214741 | 790 | 1 | R |
| `docs/evidence/l2-recompile-check.txt` | 7018 | 210 | 1 | R |
| `docs/evidence/l2-tools-bench-build.txt` | 8301 | 246 | 1 | R |
| `docs/evidence/l5-ctest.txt` | 94949 | 460 | 1 | R |
| `docs/evidence/l5-lpcc-smoke.txt` | 1963 | 61 | 1 | R |
| `docs/evidence/l5-ofile-tests.txt` | 4334 | 146 | 1 | R |
| `docs/evidence/l6-tsan-driver-owner.txt` | 6979 | 209 | 1 | R |
| `docs/evidence/l6-tsan-driver-recompile.txt` | 6981 | 209 | 1 | R |
| `docs/evidence/l6-tsan-full.txt` | 214809 | 790 | 1 | R |
| `docs/evidence/l6-tsan-owner-first.txt` | 37039 | 513 | 1 | R |
| `docs/evidence/l6-tsan-summary.md` | 2440 | 39 | 1 | R |
| `docs/evidence/l7-bench-lpc_vm_bench.txt` | 156 | 6 | 1 | R |
| `docs/evidence/l7-bench-object_store_bench.txt` | 168 | 6 | 1 | R |
| `docs/evidence/l7-bench-owner_runtime_bench.txt` | 122 | 6 | 1 | R |
| `docs/evidence/l7-ftest-regression.txt` | 1752798 | 51483 | 52 | R-raw |
| `docs/evidence/l7-master-reload-rounds.md` | 2560 | 50 | 1 | R |
| `docs/evidence/l7-stress-asan.txt` | 7075 | 210 | 1 | R |
| `docs/evidence/lpc-tests-asan-final-2026-08-19.txt` | 232565 | 790 | 1 | R |
| `docs/evidence/lpc-tests-final-2026-08-19.txt` | 217177 | 790 | 1 | R |
| `docs/evidence/lpc-tests-tsan-final-2026-08-19.txt` | 217251 | 790 | 1 | R |
| `docs/evidence/manifest.schema.json` | 4068 | 143 | 1 | R |
| `docs/evidence/s16-ws-wss-smoke.md` | 5138 | 105 | 1 | R |
| `docs/evidence/t3-1-scratch-arena.md` | 3569 | 67 | 1 | R |
| `docs/evidence/t3-2-structured-diagnostics.md` | 3664 | 70 | 1 | R |
| `docs/evidence/t3-3-diagnostic-rendering.md` | 4532 | 69 | 1 | R |
| `docs/evidence/t3-4-lpcshell.md` | 3775 | 62 | 1 | R |
| `docs/evidence/ubsan-enumerate.txt` | 214789 | 790 | 1 | R |
| `docs/evidence/ubsan-fnonull-probe.txt` | 916 | 26 | 1 | R |
| `docs/evidence/ubsan-o0-probe.txt` | 890 | 26 | 1 | R |
| `docs/evidence/ubsan-tlsmodel-exp.txt` | 971 | 27 | 1 | R |
| `docs/external-required-readiness-2026-09.md` | 4889 | 109 | 1 | R |
| `docs/fix_md_header.py` | 1226 | 46 | 1 | R |
| `docs/fix_seealso.py` | 520 | 23 | 1 | R |
| `docs/fluffos-xk-overview.md` | 7935 | 182 | 1 | R |
| `docs/gen_index.py` | 1456 | 54 | 1 | R |
| `docs/google07a1b3520fdbc61a.html` | 54 | 2 | 1 | R |
| `docs/index.md` | 5051 | 114 | 1 | R |
| `docs/keywords.json` | 94407 | 4944 | 5 | R |
| `docs/lpc-modern-runtime.md` | 8646 | 242 | 1 | R |
| `docs/lpc/constructs/for.md` | 871 | 39 | 1 | R |
| `docs/lpc/constructs/function.md` | 2555 | 121 | 1 | R |
| `docs/lpc/constructs/if.md` | 2492 | 104 | 1 | R |
| `docs/lpc/constructs/include.md` | 1146 | 34 | 1 | R |
| `docs/lpc/constructs/index.md` | 230 | 13 | 1 | R |
| `docs/lpc/constructs/inherit.md` | 3079 | 61 | 1 | R |
| `docs/lpc/constructs/prototypes.md` | 3470 | 105 | 1 | R |
| `docs/lpc/constructs/switch.md` | 922 | 49 | 1 | R |
| `docs/lpc/constructs/while.md` | 1271 | 50 | 1 | R |
| `docs/lpc/index.md` | 826 | 30 | 1 | R |
| `docs/lpc/preprocessor/README.md` | 378 | 13 | 1 | R |
| `docs/lpc/preprocessor/define.md` | 319 | 14 | 1 | R |
| `docs/lpc/preprocessor/include.md` | 455 | 16 | 1 | R |
| `docs/lpc/preprocessor/index.md` | 114 | 8 | 1 | R |
| `docs/lpc/source-files.md` | 1567 | 42 | 1 | R |
| `docs/lpc/types/array.md` | 873 | 44 | 1 | R |
| `docs/lpc/types/buffer.md` | 791 | 18 | 1 | R |
| `docs/lpc/types/classes.md` | 1062 | 50 | 1 | R |
| `docs/lpc/types/float.md` | 481 | 20 | 1 | R |
| `docs/lpc/types/function.md` | 8320 | 337 | 1 | R |
| `docs/lpc/types/general.md` | 6542 | 171 | 1 | R |
| `docs/lpc/types/index.md` | 273 | 14 | 1 | R |
| `docs/lpc/types/mappings.md` | 5528 | 191 | 1 | R |
| `docs/lpc/types/promise.md` | 3437 | 86 | 1 | R |
| `docs/lpc/types/strings.md` | 934 | 30 | 1 | R |
| `docs/lpc/types/substructures.md` | 12105 | 341 | 1 | R |
| `docs/lpcshell-prerequisite-plan-2026-08.md` | 5543 | 96 | 1 | R |
| `docs/multicore-production-gate.md` | 13511 | 132 | 1 | R / E:AUD-022 |
| `docs/multicore-runtime-v2.md` | 4241 | 47 | 1 | R |
| `docs/multicore-runtime-v4.md` | 12902 | 324 | 1 | R |
| `docs/multicore-runtime.md` | 1620 | 30 | 1 | R |
| `docs/optimization-master-plan-2026-08-18.md` | 35423 | 468 | 1 | R |
| `docs/owner-multicore-api.md` | 11338 | 305 | 1 | R |
| `docs/package-lock.json` | 92456 | 2687 | 3 | R |
| `docs/package.json` | 781 | 40 | 1 | R |
| `docs/project-knowledge-map.md` | 46801 | 505 | 1 | R |
| `docs/project-modernization-plan-2026-10.md` | 288226 | 415 | 1 | R |
| `docs/project-scope.md` | 4803 | 91 | 1 | R |
| `docs/recompile-followup-plan-2026-08.md` | 19106 | 299 | 1 | R |
| `docs/recompile-object-special-design-2026-08.md` | 69808 | 658 | 1 | R |
| `docs/recompile-object-v2-design-2026-08.md` | 11695 | 172 | 1 | R |
| `docs/reports/capacity-evidence-2026-08-11/lpc_vm_bench_capacity.json` | 5432 | 115 | 1 | R |
| `docs/reports/capacity-evidence-2026-08-11/lpc_vm_bench_raw.json` | 4263 | 88 | 1 | R |
| `docs/reports/capacity-evidence-2026-08-11/object_store_bench_capacity.json` | 5462 | 115 | 1 | R |
| `docs/reports/capacity-evidence-2026-08-11/object_store_bench_raw.json` | 4275 | 88 | 1 | R |
| `docs/reports/capacity-evidence-2026-08-11/owner_runtime_bench_capacity.json` | 13676 | 264 | 1 | R |
| `docs/reports/capacity-evidence-2026-08-11/owner_runtime_bench_raw.json` | 12045 | 237 | 1 | R |
| `docs/reports/owner-scheduler-capacity-2026-08-09.md` | 3687 | 73 | 1 | R |
| `docs/runbooks/gateway-security.md` | 4224 | 90 | 1 | R |
| `docs/runbooks/rollback.md` | 2223 | 42 | 1 | R |
| `docs/src/css/custom.css` | 1076 | 31 | 1 | R |
| `docs/stdlib/arrays/index.md` | 58 | 6 | 1 | R |
| `docs/stdlib/arrays/reduce.md` | 2083 | 74 | 1 | R |
| `docs/stdlib/db/database.md` | 9950 | 312 | 1 | R |
| `docs/stdlib/db/database_zh.md` | 9994 | 373 | 1 | R |
| `docs/stdlib/db/index.md` | 92 | 7 | 1 | R |
| `docs/stdlib/index.md` | 656 | 25 | 1 | R |
| `docs/stdlib/mappings/element_of_weighted.md` | 1040 | 37 | 1 | R |
| `docs/stdlib/mappings/index.md` | 86 | 6 | 1 | R |
| `docs/stdlib/numbers/array_sum.md` | 609 | 32 | 1 | R |
| `docs/stdlib/numbers/index.md` | 141 | 9 | 1 | R |
| `docs/stdlib/numbers/percent.md` | 557 | 32 | 1 | R |
| `docs/stdlib/numbers/percent_of.md` | 640 | 34 | 1 | R |
| `docs/stdlib/numbers/sum.md` | 554 | 31 | 1 | R |
| `docs/stdlib/objects/all_environment.md` | 597 | 32 | 1 | R |
| `docs/stdlib/objects/index.md` | 115 | 7 | 1 | R |
| `docs/stdlib/objects/present_clone.md` | 1957 | 51 | 1 | R |
| `docs/stdlib/string/base64decode.md` | 565 | 29 | 1 | R |
| `docs/stdlib/string/base64encode.md` | 482 | 29 | 1 | R |
| `docs/stdlib/string/bitmap_font.md` | 3530 | 105 | 1 | R |
| `docs/stdlib/string/break_string.md` | 3993 | 112 | 1 | R |
| `docs/stdlib/string/index.md` | 176 | 9 | 1 | R |
| `docs/tsan-ftest-report-2026-08-19.md` | 2154 | 42 | 1 | R |
| `docs/update_index.sh` | 699 | 37 | 1 | R |
| `docs/upstream-absorption-plan-2026-10.md` | 217332 | 470 | 1 | R |
| `docs/upstream-absorption-plan-d07e7641-735bd31f.md` | 28736 | 902 | 1 | R |
| `docs/upstream-sync-evidence-2026-08.md` | 54283 | 463 | 1 | R |
| `docs/upstream-sync-optimization-plan-2026-08.md` | 52260 | 889 | 1 | R |
| `docs/vendor-libwebsockets-manifest-2026-08.md` | 2386 | 40 | 1 | R |
| `docs/zh-CN/apply/index.md` | 2336 | 62 | 1 | R |
| `docs/zh-CN/apply/interactive/catch_tell.md` | 945 | 28 | 1 | R |
| `docs/zh-CN/apply/interactive/gmcp.md` | 350 | 17 | 1 | R |
| `docs/zh-CN/apply/interactive/index.md` | 453 | 17 | 1 | R |
| `docs/zh-CN/apply/interactive/logon.md` | 348 | 27 | 1 | R |
| `docs/zh-CN/apply/interactive/net_dead.md` | 422 | 26 | 1 | R |
| `docs/zh-CN/apply/interactive/process_input.md` | 806 | 28 | 1 | R |
| `docs/zh-CN/apply/interactive/receive_environ.md` | 467 | 22 | 1 | R |
| `docs/zh-CN/apply/interactive/receive_message.md` | 673 | 26 | 1 | R |
| `docs/zh-CN/apply/interactive/receive_snoop.md` | 620 | 28 | 1 | R |
| `docs/zh-CN/apply/interactive/telnet_suboption.md` | 1042 | 28 | 1 | R |
| `docs/zh-CN/apply/interactive/terminal_type.md` | 450 | 22 | 1 | R |
| `docs/zh-CN/apply/interactive/window_size.md` | 367 | 20 | 1 | R |
| `docs/zh-CN/apply/interactive/write_prompt.md` | 459 | 26 | 1 | R |
| `docs/zh-CN/apply/master/author_file.md` | 756 | 28 | 1 | R |
| `docs/zh-CN/apply/master/compile_object.md` | 959 | 22 | 1 | R |
| `docs/zh-CN/apply/master/connect.md` | 561 | 29 | 1 | R |
| `docs/zh-CN/apply/master/crash.md` | 661 | 26 | 1 | R |
| `docs/zh-CN/apply/master/creator_file.md` | 993 | 30 | 1 | R |
| `docs/zh-CN/apply/master/domain_file.md` | 720 | 32 | 1 | R |
| `docs/zh-CN/apply/master/epilog.md` | 880 | 29 | 1 | R |
| `docs/zh-CN/apply/master/error_handler.md` | 1159 | 52 | 1 | R |
| `docs/zh-CN/apply/master/flag.md` | 459 | 22 | 1 | R |
| `docs/zh-CN/apply/master/get_bb_uid.md` | 496 | 28 | 1 | R |
| `docs/zh-CN/apply/master/get_include_path.md` | 1161 | 35 | 1 | R |
| `docs/zh-CN/apply/master/get_mud_stats.md` | 460 | 25 | 1 | R |
| `docs/zh-CN/apply/master/get_root_uid.md` | 530 | 28 | 1 | R |
| `docs/zh-CN/apply/master/get_save_file_name.md` | 566 | 30 | 1 | R |
| `docs/zh-CN/apply/master/index.md` | 1226 | 39 | 1 | R |
| `docs/zh-CN/apply/master/log_error.md` | 672 | 30 | 1 | R |
| `docs/zh-CN/apply/master/make_path_absolute.md` | 385 | 24 | 1 | R |
| `docs/zh-CN/apply/master/object_name.md` | 444 | 26 | 1 | R |
| `docs/zh-CN/apply/master/preload.md` | 849 | 29 | 1 | R |
| `docs/zh-CN/apply/master/prepare_shutdown.md` | 883 | 30 | 1 | R |
| `docs/zh-CN/apply/master/privs_file.md` | 659 | 30 | 1 | R |
| `docs/zh-CN/apply/master/retrieve_ed_setup.md` | 465 | 26 | 1 | R |
| `docs/zh-CN/apply/master/save_ed_setup.md` | 492 | 25 | 1 | R |
| `docs/zh-CN/apply/master/valid_bind.md` | 463 | 26 | 1 | R |
| `docs/zh-CN/apply/master/valid_database.md` | 1176 | 28 | 1 | R |
| `docs/zh-CN/apply/master/valid_hide.md` | 704 | 26 | 1 | R |
| `docs/zh-CN/apply/master/valid_link.md` | 502 | 26 | 1 | R |
| `docs/zh-CN/apply/master/valid_object.md` | 806 | 30 | 1 | R |
| `docs/zh-CN/apply/master/valid_override.md` | 1521 | 51 | 1 | R |
| `docs/zh-CN/apply/master/valid_read.md` | 729 | 26 | 1 | R |
| `docs/zh-CN/apply/master/valid_seteuid.md` | 552 | 26 | 1 | R |
| `docs/zh-CN/apply/master/valid_shadow.md` | 645 | 26 | 1 | R |
| `docs/zh-CN/apply/master/valid_socket.md` | 903 | 22 | 1 | R |
| `docs/zh-CN/apply/master/valid_write.md` | 739 | 26 | 1 | R |
| `docs/zh-CN/apply/master/view_errors.md` | 470 | 25 | 1 | R |
| `docs/zh-CN/apply/object/clean_up.md` | 828 | 26 | 1 | R |
| `docs/zh-CN/apply/object/create.md` | 646 | 33 | 1 | R |
| `docs/zh-CN/apply/object/heart_beat.md` | 375 | 26 | 1 | R |
| `docs/zh-CN/apply/object/id.md` | 412 | 26 | 1 | R |
| `docs/zh-CN/apply/object/index.md` | 258 | 13 | 1 | R |
| `docs/zh-CN/apply/object/init.md` | 923 | 34 | 1 | R |
| `docs/zh-CN/apply/object/move_or_destruct.md` | 506 | 26 | 1 | R |
| `docs/zh-CN/apply/object/reset.md` | 734 | 30 | 1 | R |
| `docs/zh-CN/apply/object/virtual_start.md` | 308 | 22 | 1 | R |
| `docs/zh-CN/build/build.md` | 8593 | 233 | 1 | R |
| `docs/zh-CN/build/index.md` | 55 | 6 | 1 | R |
| `docs/zh-CN/efun/abs.md` | 341 | 25 | 1 | R |
| `docs/zh-CN/efun/acos.md` | 270 | 22 | 1 | R |
| `docs/zh-CN/efun/add_action.md` | 1570 | 35 | 1 | R |
| `docs/zh-CN/efun/all_inventory.md` | 479 | 26 | 1 | R |
| `docs/zh-CN/efun/all_previous_objects.md` | 499 | 26 | 1 | R |
| `docs/zh-CN/efun/allocate.md` | 803 | 34 | 1 | R |
| `docs/zh-CN/efun/allocate_buffer.md` | 525 | 30 | 1 | R |
| `docs/zh-CN/efun/allocate_mapping.md` | 1139 | 38 | 1 | R |
| `docs/zh-CN/efun/arrayp.md` | 491 | 31 | 1 | R |
| `docs/zh-CN/efun/asin.md` | 270 | 22 | 1 | R |
| `docs/zh-CN/efun/assemble_class.md` | 1226 | 64 | 1 | R |
| `docs/zh-CN/efun/atan.md` | 270 | 22 | 1 | R |
| `docs/zh-CN/efun/author_stats.md` | 1528 | 40 | 1 | R |
| `docs/zh-CN/efun/base_name.md` | 524 | 29 | 1 | R |
| `docs/zh-CN/efun/bind.md` | 920 | 35 | 1 | R |
| `docs/zh-CN/efun/buffer_transcode.md` | 377 | 22 | 1 | R |
| `docs/zh-CN/efun/bufferp.md` | 458 | 26 | 1 | R |
| `docs/zh-CN/efun/cache_stats.md` | 423 | 26 | 1 | R |
| `docs/zh-CN/efun/call_other.md` | 1450 | 45 | 1 | R |
| `docs/zh-CN/efun/call_out.md` | 637 | 28 | 1 | R |
| `docs/zh-CN/efun/call_out_info.md` | 429 | 26 | 1 | R |
| `docs/zh-CN/efun/capitalize.md` | 341 | 26 | 1 | R |
| `docs/zh-CN/efun/catch.md` | 704 | 30 | 1 | R |
| `docs/zh-CN/efun/ceil.md` | 367 | 26 | 1 | R |
| `docs/zh-CN/efun/children.md` | 672 | 32 | 1 | R |
| `docs/zh-CN/efun/classes.md` | 562 | 29 | 1 | R |
| `docs/zh-CN/efun/classp.md` | 409 | 26 | 1 | R |
| `docs/zh-CN/efun/clear_bit.md` | 391 | 26 | 1 | R |
| `docs/zh-CN/efun/clone_object.md` | 1078 | 29 | 1 | R |
| `docs/zh-CN/efun/clonep.md` | 791 | 30 | 1 | R |
| `docs/zh-CN/efun/command.md` | 466 | 26 | 1 | R |
| `docs/zh-CN/efun/commands.md` | 678 | 26 | 1 | R |
| `docs/zh-CN/efun/compressedp.md` | 243 | 19 | 1 | R |
| `docs/zh-CN/efun/copy.md` | 603 | 41 | 1 | R |
| `docs/zh-CN/efun/cos.md` | 261 | 22 | 1 | R |
| `docs/zh-CN/efun/cp.md` | 351 | 30 | 1 | R |
| `docs/zh-CN/efun/crc32.md` | 335 | 22 | 1 | R |
| `docs/zh-CN/efun/crypt.md` | 768 | 40 | 1 | R |
| `docs/zh-CN/efun/ctime.md` | 396 | 26 | 1 | R |
| `docs/zh-CN/efun/db_close.md` | 405 | 28 | 1 | R |
| `docs/zh-CN/efun/db_commit.md` | 423 | 32 | 1 | R |
| `docs/zh-CN/efun/db_connect.md` | 1840 | 51 | 1 | R |
| `docs/zh-CN/efun/db_exec.md` | 420 | 26 | 1 | R |
| `docs/zh-CN/efun/db_fetch.md` | 1012 | 51 | 1 | R |
| `docs/zh-CN/efun/db_rollback.md` | 430 | 32 | 1 | R |
| `docs/zh-CN/efun/db_status.md` | 369 | 26 | 1 | R |
| `docs/zh-CN/efun/debug_info.md` | 496 | 26 | 1 | R |
| `docs/zh-CN/efun/debug_message.md` | 342 | 22 | 1 | R |
| `docs/zh-CN/efun/deep_inherit_list.md` | 477 | 26 | 1 | R |
| `docs/zh-CN/efun/deep_inventory.md` | 531 | 26 | 1 | R |
| `docs/zh-CN/efun/defer.md` | 549 | 32 | 1 | R |
| `docs/zh-CN/efun/destruct.md` | 804 | 28 | 1 | R |
| `docs/zh-CN/efun/disable_commands.md` | 748 | 32 | 1 | R |
| `docs/zh-CN/efun/disable_wizard.md` | 371 | 27 | 1 | R |
| `docs/zh-CN/efun/disassemble_class.md` | 862 | 54 | 1 | R |
| `docs/zh-CN/efun/domain_stats.md` | 1528 | 40 | 1 | R |
| `docs/zh-CN/efun/dump_file_descriptors.md` | 1523 | 43 | 1 | R |
| `docs/zh-CN/efun/dump_prog.md` | 661 | 30 | 1 | R |
| `docs/zh-CN/efun/dumpallobj.md` | 513 | 26 | 1 | R |
| `docs/zh-CN/efun/ed.md` | 1522 | 39 | 1 | R |
| `docs/zh-CN/efun/ed_cmd.md` | 327 | 21 | 1 | R |
| `docs/zh-CN/efun/ed_start.md` | 642 | 28 | 1 | R |
| `docs/zh-CN/efun/element_of.md` | 322 | 24 | 1 | R |
| `docs/zh-CN/efun/enable_commands.md` | 1313 | 36 | 1 | R |
| `docs/zh-CN/efun/enable_wizard.md` | 792 | 32 | 1 | R |
| `docs/zh-CN/efun/environment.md` | 415 | 26 | 1 | R |
| `docs/zh-CN/efun/error.md` | 439 | 26 | 1 | R |
| `docs/zh-CN/efun/eval_cost.md` | 401 | 22 | 1 | R |
| `docs/zh-CN/efun/evaluate.md` | 365 | 22 | 1 | R |
| `docs/zh-CN/efun/event.md` | 471 | 23 | 1 | R |
| `docs/zh-CN/efun/exec.md` | 894 | 30 | 1 | R |
| `docs/zh-CN/efun/exp.md` | 269 | 22 | 1 | R |
| `docs/zh-CN/efun/explode.md` | 487 | 30 | 1 | R |
| `docs/zh-CN/efun/export_uid.md` | 499 | 26 | 1 | R |
| `docs/zh-CN/efun/fetch_class_member.md` | 682 | 36 | 1 | R |
| `docs/zh-CN/efun/fetch_variable.md` | 720 | 30 | 1 | R |
| `docs/zh-CN/efun/file_length.md` | 378 | 26 | 1 | R |
| `docs/zh-CN/efun/file_name.md` | 648 | 24 | 1 | R |
| `docs/zh-CN/efun/file_size.md` | 402 | 26 | 1 | R |
| `docs/zh-CN/efun/filter.md` | 714 | 27 | 1 | R |
| `docs/zh-CN/efun/filter_array.md` | 913 | 33 | 1 | R |
| `docs/zh-CN/efun/filter_mapping.md` | 873 | 29 | 1 | R |
| `docs/zh-CN/efun/find_call_out.md` | 427 | 26 | 1 | R |
| `docs/zh-CN/efun/find_living.md` | 618 | 26 | 1 | R |
| `docs/zh-CN/efun/find_object.md` | 654 | 29 | 1 | R |
| `docs/zh-CN/efun/find_player.md` | 438 | 26 | 1 | R |
| `docs/zh-CN/efun/first_inventory.md` | 551 | 26 | 1 | R |
| `docs/zh-CN/efun/floatp.md` | 411 | 26 | 1 | R |
| `docs/zh-CN/efun/floor.md` | 380 | 26 | 1 | R |
| `docs/zh-CN/efun/function_exists.md` | 970 | 31 | 1 | R |
| `docs/zh-CN/efun/function_owner.md` | 400 | 28 | 1 | R |
| `docs/zh-CN/efun/functionp.md` | 1999 | 79 | 1 | R |
| `docs/zh-CN/efun/functions.md` | 662 | 29 | 1 | R |
| `docs/zh-CN/efun/get_char.md` | 1033 | 34 | 1 | R |
| `docs/zh-CN/efun/get_config.md` | 4209 | 116 | 1 | R |
| `docs/zh-CN/efun/get_dir.md` | 953 | 32 | 1 | R |
| `docs/zh-CN/efun/get_garbage.md` | 434 | 22 | 1 | R |
| `docs/zh-CN/efun/geteuid.md` | 703 | 26 | 1 | R |
| `docs/zh-CN/efun/getuid.md` | 558 | 26 | 1 | R |
| `docs/zh-CN/efun/hash.md` | 1196 | 51 | 1 | R |
| `docs/zh-CN/efun/heart_beats.md` | 356 | 26 | 1 | R |
| `docs/zh-CN/efun/implode.md` | 474 | 26 | 1 | R |
| `docs/zh-CN/efun/in_edit.md` | 351 | 23 | 1 | R |
| `docs/zh-CN/efun/in_input.md` | 359 | 23 | 1 | R |
| `docs/zh-CN/efun/index.md` | 9651 | 310 | 1 | R |
| `docs/zh-CN/efun/inherit_list.md` | 466 | 26 | 1 | R |
| `docs/zh-CN/efun/inherits.md` | 427 | 26 | 1 | R |
| `docs/zh-CN/efun/input_to.md` | 1183 | 36 | 1 | R |
| `docs/zh-CN/efun/interactive.md` | 432 | 26 | 1 | R |
| `docs/zh-CN/efun/intp.md` | 413 | 26 | 1 | R |
| `docs/zh-CN/efun/is_daylight_savings_time.md` | 321 | 20 | 1 | R |
| `docs/zh-CN/efun/keys.md` | 625 | 37 | 1 | R |
| `docs/zh-CN/efun/link.md` | 713 | 28 | 1 | R |
| `docs/zh-CN/efun/living.md` | 436 | 26 | 1 | R |
| `docs/zh-CN/efun/livings.md` | 436 | 26 | 1 | R |
| `docs/zh-CN/efun/load_object.md` | 466 | 26 | 1 | R |
| `docs/zh-CN/efun/localtime.md` | 1771 | 59 | 1 | R |
| `docs/zh-CN/efun/log.md` | 257 | 22 | 1 | R |
| `docs/zh-CN/efun/lower_case.md` | 324 | 26 | 1 | R |
| `docs/zh-CN/efun/ltrim.md` | 480 | 25 | 1 | R |
| `docs/zh-CN/efun/malloc_status.md` | 599 | 27 | 1 | R |
| `docs/zh-CN/efun/map.md` | 796 | 27 | 1 | R |
| `docs/zh-CN/efun/map_array.md` | 744 | 31 | 1 | R |
| `docs/zh-CN/efun/map_delete.md` | 794 | 45 | 1 | R |
| `docs/zh-CN/efun/map_mapping.md` | 685 | 27 | 1 | R |
| `docs/zh-CN/efun/mapp.md` | 386 | 26 | 1 | R |
| `docs/zh-CN/efun/master.md` | 345 | 26 | 1 | R |
| `docs/zh-CN/efun/match_path.md` | 568 | 27 | 1 | R |
| `docs/zh-CN/efun/max.md` | 422 | 27 | 1 | R |
| `docs/zh-CN/efun/max_eval_cost.md` | 420 | 22 | 1 | R |
| `docs/zh-CN/efun/member_array.md` | 462 | 22 | 1 | R |
| `docs/zh-CN/efun/memory_info.md` | 588 | 26 | 1 | R |
| `docs/zh-CN/efun/memory_summary.md` | 376 | 26 | 1 | R |
| `docs/zh-CN/efun/message.md` | 1160 | 35 | 1 | R |
| `docs/zh-CN/efun/min.md` | 422 | 27 | 1 | R |
| `docs/zh-CN/efun/mkdir.md` | 305 | 26 | 1 | R |
| `docs/zh-CN/efun/move_object.md` | 411 | 26 | 1 | R |
| `docs/zh-CN/efun/mud_status.md` | 449 | 26 | 1 | R |
| `docs/zh-CN/efun/named_livings.md` | 414 | 26 | 1 | R |
| `docs/zh-CN/efun/network_stats.md` | 807 | 39 | 1 | R |
| `docs/zh-CN/efun/new.md` | 1041 | 51 | 1 | R |
| `docs/zh-CN/efun/next_inventory.md` | 618 | 28 | 1 | R |
| `docs/zh-CN/efun/notify_fail.md` | 1176 | 32 | 1 | R |
| `docs/zh-CN/efun/nullp.md` | 864 | 35 | 1 | R |
| `docs/zh-CN/efun/num_classes.md` | 357 | 26 | 1 | R |
| `docs/zh-CN/efun/objectp.md` | 388 | 26 | 1 | R |
| `docs/zh-CN/efun/objects.md` | 876 | 32 | 1 | R |
| `docs/zh-CN/efun/oldcrypt.md` | 679 | 38 | 1 | R |
| `docs/zh-CN/efun/opcprof.md` | 575 | 24 | 1 | R |
| `docs/zh-CN/efun/origin.md` | 864 | 38 | 1 | R |
| `docs/zh-CN/efun/parse_command.md` | 7832 | 190 | 1 | R |
| `docs/zh-CN/efun/pcre_assoc.md` | 505 | 24 | 1 | R |
| `docs/zh-CN/efun/pcre_cache.md` | 314 | 22 | 1 | R |
| `docs/zh-CN/efun/pcre_extract.md` | 853 | 30 | 1 | R |
| `docs/zh-CN/efun/pcre_match.md` | 646 | 27 | 1 | R |
| `docs/zh-CN/efun/pcre_match_all.md` | 784 | 28 | 1 | R |
| `docs/zh-CN/efun/pcre_replace.md` | 581 | 27 | 1 | R |
| `docs/zh-CN/efun/pcre_replace_callback.md` | 735 | 27 | 1 | R |
| `docs/zh-CN/efun/pcre_version.md` | 549 | 24 | 1 | R |
| `docs/zh-CN/efun/perf_counter_ns.md` | 379 | 22 | 1 | R |
| `docs/zh-CN/efun/pluralize.md` | 319 | 22 | 1 | R |
| `docs/zh-CN/efun/pointerp.md` | 495 | 31 | 1 | R |
| `docs/zh-CN/efun/pow.md` | 352 | 26 | 1 | R |
| `docs/zh-CN/efun/present.md` | 1121 | 35 | 1 | R |
| `docs/zh-CN/efun/previous_object.md` | 747 | 26 | 1 | R |
| `docs/zh-CN/efun/printf.md` | 3086 | 87 | 1 | R |
| `docs/zh-CN/efun/program_info.md` | 729 | 40 | 1 | R |
| `docs/zh-CN/efun/query_charmode.md` | 436 | 25 | 1 | R |
| `docs/zh-CN/efun/query_ed_mode.md` | 570 | 29 | 1 | R |
| `docs/zh-CN/efun/query_encoding.md` | 471 | 24 | 1 | R |
| `docs/zh-CN/efun/query_heart_beat.md` | 481 | 28 | 1 | R |
| `docs/zh-CN/efun/query_host_name.md` | 453 | 30 | 1 | R |
| `docs/zh-CN/efun/query_idle.md` | 319 | 26 | 1 | R |
| `docs/zh-CN/efun/query_ip_name.md` | 863 | 32 | 1 | R |
| `docs/zh-CN/efun/query_ip_number.md` | 401 | 26 | 1 | R |
| `docs/zh-CN/efun/query_ip_port.md` | 436 | 26 | 1 | R |
| `docs/zh-CN/efun/query_load_average.md` | 409 | 26 | 1 | R |
| `docs/zh-CN/efun/query_notify_fail.md` | 260 | 19 | 1 | R |
| `docs/zh-CN/efun/query_num.md` | 400 | 23 | 1 | R |
| `docs/zh-CN/efun/query_privs.md` | 519 | 27 | 1 | R |
| `docs/zh-CN/efun/query_replaced_program.md` | 462 | 22 | 1 | R |
| `docs/zh-CN/efun/query_shadowing.md` | 410 | 26 | 1 | R |
| `docs/zh-CN/efun/query_snoop.md` | 375 | 26 | 1 | R |
| `docs/zh-CN/efun/query_snooping.md` | 399 | 26 | 1 | R |
| `docs/zh-CN/efun/query_verb.md` | 450 | 26 | 1 | R |
| `docs/zh-CN/efun/random.md` | 309 | 26 | 1 | R |
| `docs/zh-CN/efun/read_buffer.md` | 987 | 35 | 1 | R |
| `docs/zh-CN/efun/read_bytes.md` | 759 | 28 | 1 | R |
| `docs/zh-CN/efun/read_file.md` | 743 | 30 | 1 | R |
| `docs/zh-CN/efun/real_time.md` | 464 | 26 | 1 | R |
| `docs/zh-CN/efun/receive.md` | 557 | 26 | 1 | R |
| `docs/zh-CN/efun/reclaim_objects.md` | 555 | 26 | 1 | R |
| `docs/zh-CN/efun/refs.md` | 446 | 25 | 1 | R |
| `docs/zh-CN/efun/reg_assoc.md` | 1671 | 55 | 1 | R |
| `docs/zh-CN/efun/regexp.md` | 4219 | 88 | 1 | R |
| `docs/zh-CN/efun/reload_object.md` | 679 | 26 | 1 | R |
| `docs/zh-CN/efun/remove_action.md` | 553 | 30 | 1 | R |
| `docs/zh-CN/efun/remove_call_out.md` | 570 | 29 | 1 | R |
| `docs/zh-CN/efun/remove_charmode.md` | 427 | 26 | 1 | R |
| `docs/zh-CN/efun/remove_get_char.md` | 504 | 31 | 1 | R |
| `docs/zh-CN/efun/remove_interactive.md` | 383 | 26 | 1 | R |
| `docs/zh-CN/efun/remove_shadow.md` | 510 | 28 | 1 | R |
| `docs/zh-CN/efun/rename.md` | 381 | 30 | 1 | R |
| `docs/zh-CN/efun/repeat_string.md` | 518 | 22 | 1 | R |
| `docs/zh-CN/efun/replace_program.md` | 767 | 28 | 1 | R |
| `docs/zh-CN/efun/replace_string.md` | 1610 | 41 | 1 | R |
| `docs/zh-CN/efun/replaceable.md` | 577 | 26 | 1 | R |
| `docs/zh-CN/efun/reset_eval_cost.md` | 386 | 22 | 1 | R |
| `docs/zh-CN/efun/resolve.md` | 950 | 34 | 1 | R |
| `docs/zh-CN/efun/restore_from_string.md` | 479 | 26 | 1 | R |
| `docs/zh-CN/efun/restore_object.md` | 599 | 28 | 1 | R |
| `docs/zh-CN/efun/restore_variable.md` | 395 | 26 | 1 | R |
| `docs/zh-CN/efun/rm.md` | 287 | 26 | 1 | R |
| `docs/zh-CN/efun/rmdir.md` | 315 | 30 | 1 | R |
| `docs/zh-CN/efun/roll_MdN.md` | 395 | 26 | 1 | R |
| `docs/zh-CN/efun/round.md` | 300 | 26 | 1 | R |
| `docs/zh-CN/efun/rtrim.md` | 480 | 25 | 1 | R |
| `docs/zh-CN/efun/rusage.md` | 832 | 39 | 1 | R |
| `docs/zh-CN/efun/save_object.md` | 784 | 30 | 1 | R |
| `docs/zh-CN/efun/save_variable.md` | 348 | 26 | 1 | R |
| `docs/zh-CN/efun/say.md` | 718 | 30 | 1 | R |
| `docs/zh-CN/efun/secure_random.md` | 445 | 24 | 1 | R |
| `docs/zh-CN/efun/send_nullbyte.md` | 263 | 18 | 1 | R |
| `docs/zh-CN/efun/set_author.md` | 1227 | 28 | 1 | R |
| `docs/zh-CN/efun/set_bit.md` | 556 | 28 | 1 | R |
| `docs/zh-CN/efun/set_debug_level.md` | 642 | 30 | 1 | R |
| `docs/zh-CN/efun/set_encoding.md` | 2245 | 52 | 1 | R |
| `docs/zh-CN/efun/set_eval_limit.md` | 578 | 22 | 1 | R |
| `docs/zh-CN/efun/set_heart_beat.md` | 599 | 26 | 1 | R |
| `docs/zh-CN/efun/set_hide.md` | 563 | 26 | 1 | R |
| `docs/zh-CN/efun/set_light.md` | 607 | 25 | 1 | R |
| `docs/zh-CN/efun/set_living_name.md` | 415 | 26 | 1 | R |
| `docs/zh-CN/efun/set_privs.md` | 376 | 25 | 1 | R |
| `docs/zh-CN/efun/set_reset.md` | 515 | 28 | 1 | R |
| `docs/zh-CN/efun/seteuid.md` | 659 | 30 | 1 | R |
| `docs/zh-CN/efun/sha1.md` | 399 | 25 | 1 | R |
| `docs/zh-CN/efun/shadow.md` | 2253 | 36 | 1 | R |
| `docs/zh-CN/efun/shout.md` | 372 | 26 | 1 | R |
| `docs/zh-CN/efun/shuffle.md` | 326 | 26 | 1 | R |
| `docs/zh-CN/efun/shutdown.md` | 746 | 28 | 1 | R |
| `docs/zh-CN/efun/sin.md` | 261 | 22 | 1 | R |
| `docs/zh-CN/efun/sizeof.md` | 434 | 26 | 1 | R |
| `docs/zh-CN/efun/snoop.md` | 623 | 28 | 1 | R |
| `docs/zh-CN/efun/socket_accept.md` | 2471 | 60 | 1 | R |
| `docs/zh-CN/efun/socket_acquire.md` | 1257 | 30 | 1 | R |
| `docs/zh-CN/efun/socket_address.md` | 565 | 30 | 1 | R |
| `docs/zh-CN/efun/socket_bind.md` | 1161 | 42 | 1 | R |
| `docs/zh-CN/efun/socket_close.md` | 605 | 36 | 1 | R |
| `docs/zh-CN/efun/socket_connect.md` | 1670 | 57 | 1 | R |
| `docs/zh-CN/efun/socket_create.md` | 5122 | 108 | 1 | R |
| `docs/zh-CN/efun/socket_error.md` | 458 | 30 | 1 | R |
| `docs/zh-CN/efun/socket_listen.md` | 1205 | 46 | 1 | R |
| `docs/zh-CN/efun/socket_release.md` | 1521 | 30 | 1 | R |
| `docs/zh-CN/efun/socket_status.md` | 616 | 33 | 1 | R |
| `docs/zh-CN/efun/socket_write.md` | 1348 | 48 | 1 | R |
| `docs/zh-CN/efun/sort_array.md` | 1103 | 32 | 1 | R |
| `docs/zh-CN/efun/sprintf.md` | 3053 | 86 | 1 | R |
| `docs/zh-CN/efun/sqrt.md` | 312 | 26 | 1 | R |
| `docs/zh-CN/efun/sscanf.md` | 1121 | 28 | 1 | R |
| `docs/zh-CN/efun/stat.md` | 559 | 30 | 1 | R |
| `docs/zh-CN/efun/store_class_member.md` | 1062 | 63 | 1 | R |
| `docs/zh-CN/efun/store_variable.md` | 740 | 30 | 1 | R |
| `docs/zh-CN/efun/strcmp.md` | 605 | 28 | 1 | R |
| `docs/zh-CN/efun/string_decode.md` | 359 | 22 | 1 | R |
| `docs/zh-CN/efun/string_difference.md` | 400 | 26 | 1 | R |
| `docs/zh-CN/efun/string_encode.md` | 388 | 22 | 1 | R |
| `docs/zh-CN/efun/stringp.md` | 400 | 26 | 1 | R |
| `docs/zh-CN/efun/strlen.md` | 291 | 26 | 1 | R |
| `docs/zh-CN/efun/strsrch.md` | 808 | 30 | 1 | R |
| `docs/zh-CN/efun/strwidth.md` | 529 | 25 | 1 | R |
| `docs/zh-CN/efun/tan.md` | 261 | 22 | 1 | R |
| `docs/zh-CN/efun/tell_object.md` | 459 | 26 | 1 | R |
| `docs/zh-CN/efun/tell_room.md` | 587 | 27 | 1 | R |
| `docs/zh-CN/efun/telnet_nop.md` | 313 | 23 | 1 | R |
| `docs/zh-CN/efun/terminal_colour.md` | 708 | 35 | 1 | R |
| `docs/zh-CN/efun/test_bit.md` | 325 | 26 | 1 | R |
| `docs/zh-CN/efun/test_load.md` | 305 | 22 | 1 | R |
| `docs/zh-CN/efun/this_interactive.md` | 443 | 26 | 1 | R |
| `docs/zh-CN/efun/this_object.md` | 322 | 26 | 1 | R |
| `docs/zh-CN/efun/this_player.md` | 671 | 27 | 1 | R |
| `docs/zh-CN/efun/this_user.md` | 669 | 27 | 1 | R |
| `docs/zh-CN/efun/throw.md` | 592 | 35 | 1 | R |
| `docs/zh-CN/efun/time.md` | 444 | 26 | 1 | R |
| `docs/zh-CN/efun/time_expression.md` | 511 | 22 | 1 | R |
| `docs/zh-CN/efun/to_float.md` | 355 | 26 | 1 | R |
| `docs/zh-CN/efun/to_int.md` | 500 | 28 | 1 | R |
| `docs/zh-CN/efun/trace_end.md` | 297 | 24 | 1 | R |
| `docs/zh-CN/efun/trace_start.md` | 546 | 27 | 1 | R |
| `docs/zh-CN/efun/trim.md` | 578 | 26 | 1 | R |
| `docs/zh-CN/efun/typeof.md` | 991 | 46 | 1 | R |
| `docs/zh-CN/efun/undefinedp.md` | 778 | 32 | 1 | R |
| `docs/zh-CN/efun/unique_array.md` | 1182 | 40 | 1 | R |
| `docs/zh-CN/efun/unique_mapping.md` | 641 | 29 | 1 | R |
| `docs/zh-CN/efun/upper_case.md` | 347 | 26 | 1 | R |
| `docs/zh-CN/efun/uptime.md` | 349 | 26 | 1 | R |
| `docs/zh-CN/efun/userp.md` | 331 | 26 | 1 | R |
| `docs/zh-CN/efun/users.md` | 355 | 26 | 1 | R |
| `docs/zh-CN/efun/values.md` | 584 | 39 | 1 | R |
| `docs/zh-CN/efun/variables.md` | 630 | 29 | 1 | R |
| `docs/zh-CN/efun/virtualp.md` | 538 | 26 | 1 | R |
| `docs/zh-CN/efun/wizardp.md` | 371 | 26 | 1 | R |
| `docs/zh-CN/efun/write.md` | 531 | 26 | 1 | R |
| `docs/zh-CN/efun/write_buffer.md` | 832 | 37 | 1 | R |
| `docs/zh-CN/efun/write_bytes.md` | 466 | 26 | 1 | R |
| `docs/zh-CN/efun/write_file.md` | 821 | 31 | 1 | R |
| `docs/zh-CN/efun/zonetime.md` | 431 | 26 | 1 | R |
| `docs/zh-CN/index.md` | 11185 | 313 | 1 | R |
| `etc/config.test` | 14123 | 392 | 1 | R |
| `fix_permission.sh` | 215 | 8 | 1 | R |
| `qodana.yaml` | 90 | 7 | 1 | R |
| `src/.clang-format` | 638 | 28 | 1 | R |
| `src/.clang-tidy` | 4944 | 93 | 1 | R |
| `src/CMakeLists.txt` | 27336 | 770 | 1 | R / E:AUD-020, AUD-024 |
| `src/Config.example` | 13019 | 346 | 1 | R |
| `src/backend.cc` | 32609 | 928 | 1 | R |
| `src/backend.h` | 4981 | 133 | 1 | R |
| `src/base/README` | 357 | 9 | 1 | R |
| `src/base/internal/EGCIterator.h` | 7104 | 230 | 1 | R |
| `src/base/internal/arch.h` | 2266 | 149 | 1 | R |
| `src/base/internal/debugmalloc.cc` | 3289 | 108 | 1 | R |
| `src/base/internal/debugmalloc.h` | 4566 | 109 | 1 | R |
| `src/base/internal/external_port.cc` | 153 | 11 | 1 | R |
| `src/base/internal/external_port.h` | 1539 | 76 | 1 | R |
| `src/base/internal/file.cc` | 1052 | 61 | 1 | R |
| `src/base/internal/file.h` | 78 | 7 | 1 | R |
| `src/base/internal/log.cc` | 2904 | 116 | 1 | R |
| `src/base/internal/log.h` | 2911 | 97 | 1 | R |
| `src/base/internal/lru_cache.h` | 3027 | 114 | 1 | R |
| `src/base/internal/md.cc` | 5950 | 228 | 1 | R |
| `src/base/internal/md.h` | 4924 | 133 | 1 | R |
| `src/base/internal/options_incl.h` | 492 | 16 | 1 | R |
| `src/base/internal/options_internal.h` | 5185 | 132 | 1 | R |
| `src/base/internal/outbuf.cc` | 2270 | 89 | 1 | R |
| `src/base/internal/outbuf.h` | 341 | 17 | 1 | R |
| `src/base/internal/port.cc` | 5949 | 207 | 1 | R |
| `src/base/internal/port.h` | 768 | 36 | 1 | R |
| `src/base/internal/rc.cc` | 16224 | 472 | 1 | R / E:AUD-013 |
| `src/base/internal/rc.h` | 697 | 31 | 1 | R |
| `src/base/internal/rusage.cc` | 3819 | 112 | 1 | R |
| `src/base/internal/rusage.h` | 948 | 54 | 1 | R |
| `src/base/internal/stats.cc` | 1064 | 50 | 1 | R |
| `src/base/internal/stats.h` | 1480 | 61 | 1 | R |
| `src/base/internal/stralloc.cc` | 16367 | 519 | 1 | R |
| `src/base/internal/stralloc.h` | 10473 | 251 | 1 | R |
| `src/base/internal/strput.cc` | 335 | 22 | 1 | R |
| `src/base/internal/strput.h` | 520 | 24 | 1 | R |
| `src/base/internal/strutils.cc` | 23553 | 767 | 1 | R |
| `src/base/internal/strutils.h` | 8584 | 243 | 1 | R |
| `src/base/internal/tracing.cc` | 9499 | 335 | 1 | R |
| `src/base/internal/tracing.h` | 4729 | 165 | 1 | R |
| `src/base/internal/vm_thread_local.h` | 334 | 15 | 1 | R |
| `src/base/package_api.h` | 1169 | 58 | 1 | R |
| `src/base/std.h` | 4886 | 184 | 1 | R |
| `src/comm.cc` | 76894 | 2487 | 3 | R / E:AUD-001, AUD-005, AUD-009, AUD-010, AUD-012 |
| `src/comm.h` | 3437 | 93 | 1 | R |
| `src/compiler/internal/LexStream.h` | 1173 | 64 | 1 | R |
| `src/compiler/internal/compile_arena.cc` | 9897 | 363 | 1 | R |
| `src/compiler/internal/compile_arena.h` | 8321 | 192 | 1 | R |
| `src/compiler/internal/compiler.cc` | 93255 | 3125 | 4 | R / E:AUD-019 |
| `src/compiler/internal/compiler.h` | 11663 | 317 | 1 | R |
| `src/compiler/internal/diagnostic.cc` | 3378 | 122 | 1 | R |
| `src/compiler/internal/diagnostic.h` | 4482 | 130 | 1 | R |
| `src/compiler/internal/diagnostic_render.cc` | 5492 | 168 | 1 | R |
| `src/compiler/internal/diagnostic_render.h` | 2403 | 57 | 1 | R |
| `src/compiler/internal/disassembler.cc` | 23042 | 795 | 1 | R |
| `src/compiler/internal/disassembler.h` | 219 | 8 | 1 | R |
| `src/compiler/internal/generate.cc` | 26027 | 912 | 1 | R |
| `src/compiler/internal/generate.h` | 817 | 25 | 1 | R |
| `src/compiler/internal/grammar.autogen.cc` | 270611 | 1930 | 2 | R |
| `src/compiler/internal/grammar.autogen.h` | 6108 | 160 | 1 | R |
| `src/compiler/internal/grammar.y` | 107958 | 3781 | 4 | R / E:AUD-016, AUD-017 |
| `src/compiler/internal/grammar_rules.h` | 656 | 15 | 1 | R |
| `src/compiler/internal/grammar_rules_classes.cc` | 2164 | 71 | 1 | R |
| `src/compiler/internal/grammar_rules_functions.cc` | 9232 | 226 | 1 | R |
| `src/compiler/internal/grammar_rules_program.cc` | 3012 | 99 | 1 | R |
| `src/compiler/internal/icode.cc` | 38192 | 1361 | 2 | R |
| `src/compiler/internal/icode.h` | 1170 | 43 | 1 | R |
| `src/compiler/internal/keyword.h` | 1081 | 32 | 1 | R |
| `src/compiler/internal/lex.cc` | 145421 | 5163 | 6 | R |
| `src/compiler/internal/lex.h` | 5873 | 185 | 1 | R |
| `src/compiler/internal/lpc_modern_profile.cc` | 6812 | 198 | 1 | R |
| `src/compiler/internal/lpc_modern_profile.h` | 1669 | 55 | 1 | R |
| `src/compiler/internal/lpc_source_encoding.cc` | 3408 | 112 | 1 | R |
| `src/compiler/internal/lpc_source_encoding.h` | 514 | 21 | 1 | R |
| `src/compiler/internal/scratchpad.cc` | 1832 | 63 | 1 | R |
| `src/compiler/internal/scratchpad.h` | 649 | 23 | 1 | R |
| `src/compiler/internal/trees.cc` | 14711 | 527 | 1 | R / E:AUD-017 |
| `src/compiler/internal/trees.h` | 8485 | 196 | 1 | R |
| `src/config.h.in` | 713 | 32 | 1 | R |
| `src/format_source.sh` | 351 | 10 | 1 | R |
| `src/include/README` | 61 | 2 | 1 | R |
| `src/include/function.h` | 340 | 18 | 1 | R |
| `src/include/localtime.h` | 243 | 16 | 1 | R |
| `src/include/opcodes_extra.h` | 41 | 4 | 1 | R |
| `src/include/origin.h` | 479 | 14 | 1 | R |
| `src/include/parser_error.h` | 274 | 15 | 1 | R |
| `src/include/pcre_flags.h` | 588 | 19 | 1 | R |
| `src/include/runtime_config.h` | 4788 | 131 | 1 | R |
| `src/include/socket.h` | 420 | 24 | 1 | R |
| `src/include/socket_err.h` | 2170 | 47 | 1 | R |
| `src/include/type.h` | 575 | 26 | 1 | R |
| `src/interactive.h` | 4234 | 106 | 1 | R |
| `src/local_options` | 2483 | 68 | 1 | R |
| `src/local_options.README` | 14992 | 379 | 1 | R |
| `src/main.cc` | 135 | 8 | 1 | R |
| `src/main_fuzz_compile.cc` | 7551 | 221 | 1 | R |
| `src/main_fuzz_restore.cc` | 4735 | 160 | 1 | R |
| `src/main_generate_keywords.cc` | 1448 | 58 | 1 | R |
| `src/main_json2o.cc` | 1622 | 72 | 1 | R |
| `src/main_lpcc.cc` | 6845 | 234 | 1 | R |
| `src/main_lpcshell.cc` | 12452 | 388 | 1 | R |
| `src/main_o2json.cc` | 1771 | 77 | 1 | R |
| `src/main_symbol.cc` | 1026 | 52 | 1 | R |
| `src/mainlib.cc` | 11763 | 487 | 1 | R / E:AUD-008 |
| `src/mainlib.h` | 209 | 7 | 1 | R |
| `src/net/msp.cc` | 954 | 36 | 1 | R |
| `src/net/msp.h` | 172 | 7 | 1 | R |
| `src/net/sys_telnet.h` | 10904 | 343 | 1 | R |
| `src/net/telnet.cc` | 25804 | 761 | 1 | R / E:AUD-002, AUD-011 |
| `src/net/telnet.h` | 1450 | 40 | 1 | R |
| `src/net/tls.cc` | 4381 | 159 | 1 | R |
| `src/net/tls.h` | 884 | 24 | 1 | R |
| `src/net/websocket.cc` | 16614 | 512 | 1 | R |
| `src/net/websocket.h` | 2454 | 64 | 1 | R |
| `src/net/ws_ascii.cc` | 5497 | 156 | 1 | R / E:AUD-002 |
| `src/net/ws_ascii.h` | 427 | 17 | 1 | R |
| `src/net/ws_telnet.cc` | 4169 | 131 | 1 | R / E:AUD-002 |
| `src/net/ws_telnet.h` | 434 | 17 | 1 | R |
| `src/ofile.cc` | 9719 | 354 | 1 | R |
| `src/ofile.h` | 449 | 23 | 1 | R |
| `src/outdated_define.txt` | 897 | 52 | 1 | R |
| `src/packages/CMakeLists.txt` | 1170 | 45 | 1 | R |
| `src/packages/README` | 772 | 14 | 1 | R |
| `src/packages/async/CMakeLists.txt` | 244 | 10 | 1 | R |
| `src/packages/async/async.cc` | 30931 | 1126 | 2 | R / E:AUD-003, AUD-004 |
| `src/packages/async/async.h` | 130 | 8 | 1 | R |
| `src/packages/async/async.spec` | 503 | 14 | 1 | R |
| `src/packages/compress/CMakeLists.txt` | 207 | 8 | 1 | R |
| `src/packages/compress/compress.cc` | 11810 | 422 | 1 | R |
| `src/packages/compress/compress.spec` | 288 | 11 | 1 | R |
| `src/packages/contrib/CMakeLists.txt` | 133 | 7 | 1 | R |
| `src/packages/contrib/contrib.cc` | 82420 | 3253 | 4 | R |
| `src/packages/contrib/contrib.spec` | 2029 | 69 | 1 | R |
| `src/packages/contrib/cycles.cc` | 13060 | 378 | 1 | R |
| `src/packages/core/CMakeLists.txt` | 180 | 8 | 1 | R |
| `src/packages/core/add_action.cc` | 22659 | 852 | 1 | R |
| `src/packages/core/add_action.h` | 1060 | 44 | 1 | R |
| `src/packages/core/call_out.cc` | 28537 | 940 | 1 | R |
| `src/packages/core/call_out.h` | 1444 | 63 | 1 | R |
| `src/packages/core/core.spec` | 14626 | 463 | 1 | R |
| `src/packages/core/crc32.cc` | 1144 | 47 | 1 | R |
| `src/packages/core/crc32.h` | 125 | 12 | 1 | R |
| `src/packages/core/crctab.h` | 5815 | 87 | 1 | R |
| `src/packages/core/custom_crypt.cc` | 14163 | 437 | 1 | R |
| `src/packages/core/custom_crypt.h` | 994 | 32 | 1 | R |
| `src/packages/core/debug.cc` | 454 | 28 | 1 | R |
| `src/packages/core/dns.cc` | 13721 | 439 | 1 | R |
| `src/packages/core/dns.h` | 350 | 14 | 1 | R |
| `src/packages/core/dumpstat.cc` | 4008 | 174 | 1 | R |
| `src/packages/core/dumpstat.h` | 129 | 11 | 1 | R |
| `src/packages/core/ed.cc` | 93442 | 3622 | 4 | R |
| `src/packages/core/ed.h` | 4259 | 161 | 1 | R |
| `src/packages/core/efuns_main.cc` | 99837 | 3937 | 4 | R |
| `src/packages/core/encoding.cc` | 5903 | 206 | 1 | R |
| `src/packages/core/file.cc` | 30110 | 1165 | 2 | R |
| `src/packages/core/file.h` | 762 | 30 | 1 | R |
| `src/packages/core/heartbeat.cc` | 10236 | 356 | 1 | R |
| `src/packages/core/heartbeat.h` | 606 | 30 | 1 | R |
| `src/packages/core/interactive.cc` | 1326 | 52 | 1 | R |
| `src/packages/core/json.cc` | 8664 | 323 | 1 | R |
| `src/packages/core/json.h` | 244 | 12 | 1 | R |
| `src/packages/core/mssp.cc` | 3029 | 92 | 1 | R |
| `src/packages/core/mssp.h` | 120 | 7 | 1 | R |
| `src/packages/core/outbuf.cc` | 391 | 18 | 1 | R |
| `src/packages/core/outbuf.h` | 117 | 7 | 1 | R |
| `src/packages/core/promises.cc` | 8198 | 229 | 1 | R |
| `src/packages/core/rc.cc` | 1213 | 57 | 1 | R |
| `src/packages/core/reclaim.cc` | 3653 | 136 | 1 | R / E:AUD-007, AUD-018 |
| `src/packages/core/reclaim.h` | 92 | 10 | 1 | R |
| `src/packages/core/regexp.cc` | 42407 | 1644 | 2 | R |
| `src/packages/core/regexp.h` | 918 | 38 | 1 | R |
| `src/packages/core/replace_program.cc` | 8295 | 239 | 1 | R |
| `src/packages/core/replace_program.h` | 318 | 17 | 1 | R |
| `src/packages/core/save.cc` | 2201 | 101 | 1 | R |
| `src/packages/core/sprintf.cc` | 46526 | 1419 | 2 | R |
| `src/packages/core/sprintf.h` | 188 | 8 | 1 | R |
| `src/packages/core/string.cc` | 1463 | 68 | 1 | R |
| `src/packages/core/sys.cc` | 3839 | 112 | 1 | R |
| `src/packages/core/telnet_ext.cc` | 6578 | 261 | 1 | R |
| `src/packages/core/time.cc` | 4577 | 189 | 1 | R |
| `src/packages/core/trace.cc` | 955 | 44 | 1 | R |
| `src/packages/core/vm_owner.cc` | 26052 | 807 | 1 | R |
| `src/packages/core/vm_worker.cc` | 29398 | 750 | 1 | R |
| `src/packages/crypto/CMakeLists.txt` | 284 | 9 | 1 | R |
| `src/packages/crypto/crypto.cc` | 6317 | 219 | 1 | R |
| `src/packages/crypto/crypto.spec` | 29 | 2 | 1 | R |
| `src/packages/db/CMakeLists.txt` | 2702 | 65 | 1 | R / E:AUD-014 |
| `src/packages/db/db.cc` | 34528 | 1282 | 2 | R / E:AUD-004, AUD-015 |
| `src/packages/db/db.h` | 1504 | 80 | 1 | R |
| `src/packages/db/db.spec` | 199 | 8 | 1 | R |
| `src/packages/develop/CMakeLists.txt` | 267 | 11 | 1 | R |
| `src/packages/develop/checkmemory.cc` | 36404 | 1100 | 2 | R |
| `src/packages/develop/develop.cc` | 9108 | 319 | 1 | R |
| `src/packages/develop/develop.spec` | 592 | 26 | 1 | R |
| `src/packages/develop/disassembler.cc` | 1389 | 71 | 1 | R |
| `src/packages/dwlib/CMakeLists.txt` | 103 | 6 | 1 | R |
| `src/packages/dwlib/dwlib.cc` | 22992 | 883 | 1 | R |
| `src/packages/dwlib/dwlib.spec` | 434 | 12 | 1 | R |
| `src/packages/external/CMakeLists.txt` | 341 | 12 | 1 | R |
| `src/packages/external/external.cc` | 44279 | 1547 | 2 | R |
| `src/packages/external/external.h` | 778 | 28 | 1 | R |
| `src/packages/external/external.spec` | 1167 | 24 | 1 | R |
| `src/packages/external/socketpair.cc` | 4878 | 142 | 1 | R |
| `src/packages/gateway/CMakeLists.txt` | 264 | 7 | 1 | R |
| `src/packages/gateway/gateway.cc` | 151917 | 2931 | 3 | R |
| `src/packages/gateway/gateway.h` | 34581 | 664 | 1 | R |
| `src/packages/gateway/gateway.spec` | 2515 | 42 | 1 | R |
| `src/packages/gateway/gateway_session.cc` | 298026 | 7117 | 8 | R |
| `src/packages/math/CMakeLists.txt` | 100 | 6 | 1 | R |
| `src/packages/math/math.cc` | 8143 | 379 | 1 | R |
| `src/packages/math/math.spec` | 546 | 20 | 1 | R |
| `src/packages/matrix/CMakeLists.txt` | 129 | 7 | 1 | R |
| `src/packages/matrix/matrix.cc` | 12021 | 542 | 1 | R |
| `src/packages/matrix/matrix.h` | 804 | 36 | 1 | R |
| `src/packages/matrix/matrix.spec` | 417 | 12 | 1 | R |
| `src/packages/mudlib_stats/CMakeLists.txt` | 153 | 7 | 1 | R |
| `src/packages/mudlib_stats/mudlib_stats.cc` | 17851 | 677 | 1 | R |
| `src/packages/mudlib_stats/mudlib_stats.h` | 1579 | 58 | 1 | R |
| `src/packages/mudlib_stats/mudlib_stats.spec` | 112 | 5 | 1 | R |
| `src/packages/ops/CMakeLists.txt` | 162 | 9 | 1 | R |
| `src/packages/ops/ops.cc` | 34609 | 1391 | 2 | R |
| `src/packages/ops/ops.h` | 661 | 34 | 1 | R |
| `src/packages/ops/ops.spec` | 2397 | 83 | 1 | R |
| `src/packages/ops/parse.cc` | 50317 | 1604 | 2 | R |
| `src/packages/ops/parse.h` | 209 | 10 | 1 | R |
| `src/packages/parser/CMakeLists.txt` | 129 | 7 | 1 | R |
| `src/packages/parser/parser.cc` | 104251 | 3833 | 4 | R |
| `src/packages/parser/parser.h` | 5391 | 232 | 1 | R |
| `src/packages/parser/parser.spec` | 307 | 10 | 1 | R |
| `src/packages/pcre/CMakeLists.txt` | 486 | 14 | 1 | R |
| `src/packages/pcre/pcre.cc` | 44924 | 1551 | 2 | R |
| `src/packages/pcre/pcre.h` | 1243 | 58 | 1 | R |
| `src/packages/pcre/pcre.spec` | 448 | 10 | 1 | R |
| `src/packages/sha1/CMakeLists.txt` | 99 | 5 | 1 | R |
| `src/packages/sha1/sha1.cc` | 3094 | 117 | 1 | R |
| `src/packages/sha1/sha1.spec` | 28 | 2 | 1 | R |
| `src/packages/sockets/CMakeLists.txt` | 168 | 8 | 1 | R |
| `src/packages/sockets/socket_efuns.cc` | 79913 | 2471 | 3 | R |
| `src/packages/sockets/socket_efuns.h` | 3472 | 135 | 1 | R |
| `src/packages/sockets/socket_text.h` | 1411 | 55 | 1 | R |
| `src/packages/sockets/sockets.cc` | 9701 | 388 | 1 | R |
| `src/packages/sockets/sockets.spec` | 969 | 27 | 1 | R |
| `src/packages/trim/CMakeLists.txt` | 99 | 5 | 1 | R |
| `src/packages/trim/trim.cc` | 1699 | 94 | 1 | R |
| `src/packages/trim/trim.spec` | 355 | 16 | 1 | R |
| `src/packages/uids/CMakeLists.txt` | 342 | 12 | 1 | R |
| `src/packages/uids/uids.cc` | 3404 | 167 | 1 | R |
| `src/packages/uids/uids.h` | 515 | 27 | 1 | R |
| `src/packages/uids/uids.spec` | 156 | 6 | 1 | R |
| `src/portbind.cc` | 3444 | 121 | 1 | R |
| `src/svalue_json.cc` | 0 | 1 | 1 | R |
| `src/symbol.cc` | 1277 | 70 | 1 | R |
| `src/symbol.h` | 450 | 21 | 1 | R |
| `src/tests/CMakeLists.txt` | 6808 | 142 | 1 | R |
| `src/tests/bench_compile.cc` | 14723 | 395 | 1 | R |
| `src/tests/bench_diagnostic_render.cc` | 3247 | 84 | 1 | R |
| `src/tests/bench_explode.cc` | 3635 | 110 | 1 | R |
| `src/tests/bench_scratchpad.cc` | 3991 | 126 | 1 | R |
| `src/tests/compound_free_probe.cc` | 12357 | 404 | 1 | R |
| `src/tests/fuzz/corpus/compile/invalid_sequence` | 98 | 6 | 1 | R |
| `src/tests/fuzz/corpus/compile/mixed_sequence` | 128 | 8 | 1 | R |
| `src/tests/fuzz/corpus/compile/valid_sequence` | 99 | 6 | 1 | R |
| `src/tests/fuzz/corpus/gateway/coalesced_valid` | 126 | — | — | M |
| `src/tests/fuzz/corpus/gateway/empty` | 0 | 1 | 1 | R |
| `src/tests/fuzz/corpus/gateway/invalid_json` | 25 | — | — | M |
| `src/tests/fuzz/corpus/gateway/oversized_header` | 36 | 1 | 1 | R-raw |
| `src/tests/fuzz/corpus/gateway/truncated_frame` | 31 | — | — | M |
| `src/tests/fuzz/corpus/gateway/valid_data_frame` | 63 | — | — | M |
| `src/tests/fuzz/corpus/gateway/valid_fragmented_a` | 31 | — | — | M |
| `src/tests/fuzz/corpus/gateway/valid_fragmented_b` | 32 | 1 | 1 | R |
| `src/tests/fuzz/corpus/restore/invalid_sequence` | 49 | 6 | 1 | R |
| `src/tests/fuzz/corpus/restore/mixed_sequence` | 44 | 6 | 1 | R |
| `src/tests/fuzz/corpus/restore/nested_mapping_error` | 19 | 4 | 1 | R |
| `src/tests/fuzz/corpus/restore/valid_sequence` | 42 | 6 | 1 | R |
| `src/tests/gateway_full_wire_bench.cc` | 34335 | 812 | 1 | R |
| `src/tests/gateway_fuzz.cc` | 15336 | 434 | 1 | R |
| `src/tests/gateway_fuzz_smoke_main.cc` | 813 | 23 | 1 | R |
| `src/tests/lpc_vm_bench.cc` | 21451 | 549 | 1 | R |
| `src/tests/object_store_bench.cc` | 21337 | 580 | 1 | R |
| `src/tests/owner_runtime_bench.cc` | 46859 | 1098 | 2 | R |
| `src/tests/test_compile_arena.cc` | 10300 | 291 | 1 | R |
| `src/tests/test_egc.cc` | 3881 | 110 | 1 | R |
| `src/tests/test_file_io.cc` | 45006 | 1519 | 2 | R |
| `src/tests/test_lpc.cc` | 41860 | 1026 | 2 | R |
| `src/tests/test_lpc_compiler.cc` | 96207 | 2320 | 3 | R |
| `src/tests/test_lpc_gateway.cc` | 327561 | 2955 | 3 | R |
| `src/tests/test_lpc_owner.cc` | 889640 | 11728 | 12 | R |
| `src/tests/test_lpc_support.h` | 14269 | 454 | 1 | R |
| `src/tests/test_mudlib.h` | 3079 | 97 | 1 | R |
| `src/tests/test_object_store_summary.cc` | 13317 | 365 | 1 | R |
| `src/tests/test_ofile.cc` | 3250 | 117 | 1 | R |
| `src/tests/test_socket_text.cc` | 1827 | 46 | 1 | R |
| `src/tests/trace_io_probe.cc` | 2974 | 106 | 1 | R |
| `src/tests/ws_smoke.py` | 12193 | 343 | 1 | R |
| `src/thirdparty/argparse/include/argparse/argparse.hpp` | 72547 | 2183 | 3 | R |
| `src/thirdparty/backward-cpp/.clang-format` | 23 | 3 | 1 | R |
| `src/thirdparty/backward-cpp/.gitignore` | 14 | 3 | 1 | R |
| `src/thirdparty/backward-cpp/.travis.yml` | 728 | 26 | 1 | R |
| `src/thirdparty/backward-cpp/BackwardConfig.cmake` | 10032 | 260 | 1 | R |
| `src/thirdparty/backward-cpp/CMakeLists.txt` | 4485 | 140 | 1 | R |
| `src/thirdparty/backward-cpp/LICENSE.txt` | 1095 | 22 | 1 | R |
| `src/thirdparty/backward-cpp/README.md` | 15155 | 441 | 1 | R |
| `src/thirdparty/backward-cpp/backward.cpp` | 1096 | 43 | 1 | R |
| `src/thirdparty/backward-cpp/backward.hpp` | 139492 | 4432 | 5 | R |
| `src/thirdparty/backward-cpp/builds.sh` | 1359 | 78 | 1 | R |
| `src/thirdparty/backward-cpp/conanfile.py` | 1509 | 46 | 1 | R |
| `src/thirdparty/backward-cpp/doc/nice.png` | 40904 | — | — | M |
| `src/thirdparty/backward-cpp/doc/pretty.png` | 105498 | — | — | M |
| `src/thirdparty/backward-cpp/doc/rude.png` | 6183 | — | — | M |
| `src/thirdparty/backward-cpp/test/_test_main.cpp` | 6293 | 242 | 1 | R |
| `src/thirdparty/backward-cpp/test/rectrace.cpp` | 2451 | 99 | 1 | R |
| `src/thirdparty/backward-cpp/test/select_signals.cpp` | 1782 | 52 | 1 | R |
| `src/thirdparty/backward-cpp/test/stacktrace.cpp` | 1719 | 58 | 1 | R |
| `src/thirdparty/backward-cpp/test/suicide.cpp` | 2504 | 90 | 1 | R |
| `src/thirdparty/backward-cpp/test/test.cpp` | 1976 | 64 | 1 | R |
| `src/thirdparty/backward-cpp/test/test.hpp` | 7226 | 184 | 1 | R |
| `src/thirdparty/backward-cpp/test_package/CMakeLists.txt` | 235 | 9 | 1 | R |
| `src/thirdparty/backward-cpp/test_package/conanfile.py` | 367 | 15 | 1 | R |
| `src/thirdparty/backward-cpp/test_package/main.cpp` | 894 | 47 | 1 | R |
| `src/thirdparty/crypt/CMakeLists.txt` | 828 | 40 | 1 | R |
| `src/thirdparty/crypt/README` | 25 | 2 | 1 | R |
| `src/thirdparty/crypt/crypt.c` | 478 | 14 | 1 | R |
| `src/thirdparty/crypt/crypt.h` | 780 | 28 | 1 | R |
| `src/thirdparty/crypt/crypt_blowfish.c` | 29435 | 799 | 1 | R |
| `src/thirdparty/crypt/crypt_des.c` | 34020 | 1017 | 2 | R |
| `src/thirdparty/crypt/crypt_des.h` | 347 | 16 | 1 | R |
| `src/thirdparty/crypt/crypt_md5.c` | 8121 | 286 | 1 | R |
| `src/thirdparty/crypt/crypt_r.c` | 863 | 26 | 1 | R |
| `src/thirdparty/crypt/crypt_sha256.c` | 8466 | 323 | 1 | R |
| `src/thirdparty/crypt/crypt_sha512.c` | 11194 | 372 | 1 | R |
| `src/thirdparty/crypt/encrypt.c` | 920 | 53 | 1 | R |
| `src/thirdparty/crypt/include/crypt.h` | 288 | 21 | 1 | R |
| `src/thirdparty/filesystem/.appveyor.yml` | 2842 | 86 | 1 | R |
| `src/thirdparty/filesystem/.ci/unix-build.sh` | 86 | 5 | 1 | R |
| `src/thirdparty/filesystem/.ci/unix-test.sh` | 149 | 8 | 1 | R |
| `src/thirdparty/filesystem/.cirrus.yml` | 1197 | 35 | 1 | R |
| `src/thirdparty/filesystem/.clang-format` | 576 | 26 | 1 | R |
| `src/thirdparty/filesystem/.clang-tidy` | 41 | 4 | 1 | R |
| `src/thirdparty/filesystem/.drone.yml` | 795 | 44 | 1 | R |
| `src/thirdparty/filesystem/.github/ISSUE_TEMPLATE/bug_report.md` | 359 | 18 | 1 | R |
| `src/thirdparty/filesystem/.github/ISSUE_TEMPLATE/feature_request.md` | 560 | 18 | 1 | R |
| `src/thirdparty/filesystem/.github/workflows/build_cmake.yml` | 6729 | 207 | 1 | R |
| `src/thirdparty/filesystem/.gitignore` | 50 | 8 | 1 | R |
| `src/thirdparty/filesystem/CMakeLists.txt` | 3150 | 77 | 1 | R |
| `src/thirdparty/filesystem/LICENSE` | 1086 | 20 | 1 | R |
| `src/thirdparty/filesystem/README.md` | 57107 | 968 | 1 | R |
| `src/thirdparty/filesystem/cmake/GhcHelper.cmake` | 3696 | 73 | 1 | R |
| `src/thirdparty/filesystem/cmake/config.cmake.in` | 140 | 6 | 1 | R |
| `src/thirdparty/filesystem/examples/CMakeLists.txt` | 584 | 17 | 1 | R |
| `src/thirdparty/filesystem/examples/dir.cpp` | 1898 | 61 | 1 | R |
| `src/thirdparty/filesystem/examples/du.cpp` | 1609 | 62 | 1 | R |
| `src/thirdparty/filesystem/include/ghc/filesystem.hpp` | 190421 | 6050 | 7 | R |
| `src/thirdparty/filesystem/include/ghc/fs_fwd.hpp` | 2111 | 39 | 1 | R |
| `src/thirdparty/filesystem/include/ghc/fs_impl.hpp` | 1978 | 36 | 1 | R |
| `src/thirdparty/filesystem/include/ghc/fs_std.hpp` | 2685 | 61 | 1 | R |
| `src/thirdparty/filesystem/include/ghc/fs_std_fwd.hpp` | 2950 | 64 | 1 | R |
| `src/thirdparty/filesystem/include/ghc/fs_std_impl.hpp` | 2464 | 47 | 1 | R |
| `src/thirdparty/filesystem/test/CMakeLists.txt` | 4218 | 92 | 1 | R |
| `src/thirdparty/filesystem/test/catch.hpp` | 657891 | 17960 | 18 | R |
| `src/thirdparty/filesystem/test/cmake/ParseAndAddCatchTests.cmake` | 13721 | 231 | 1 | R |
| `src/thirdparty/filesystem/test/exception.cpp` | 60 | 6 | 1 | R |
| `src/thirdparty/filesystem/test/filesystem_test.cpp` | 116146 | 2959 | 3 | R |
| `src/thirdparty/filesystem/test/fwd_test.cpp` | 408 | 8 | 1 | R |
| `src/thirdparty/filesystem/test/impl_test.cpp` | 419 | 9 | 1 | R |
| `src/thirdparty/filesystem/test/multi1.cpp` | 2046 | 43 | 1 | R |
| `src/thirdparty/filesystem/test/multi2.cpp` | 2018 | 41 | 1 | R |
| `src/thirdparty/fmt/.clang-format` | 231 | 9 | 1 | R |
| `src/thirdparty/fmt/CMakeLists.txt` | 16278 | 445 | 1 | R |
| `src/thirdparty/fmt/CONTRIBUTING.md` | 710 | 21 | 1 | R |
| `src/thirdparty/fmt/ChangeLog.rst` | 239120 | 5873 | 6 | R |
| `src/thirdparty/fmt/LICENSE.rst` | 1431 | 28 | 1 | R |
| `src/thirdparty/fmt/README.rst` | 20113 | 543 | 1 | R |
| `src/thirdparty/fmt/doc/CMakeLists.txt` | 918 | 27 | 1 | R |
| `src/thirdparty/fmt/doc/_static/bootstrap.min.js` | 35951 | 7 | 1 | R |
| `src/thirdparty/fmt/doc/_static/fonts/glyphicons-halflings-regular.eot` | 20335 | — | — | M |
| `src/thirdparty/fmt/doc/_static/fonts/glyphicons-halflings-regular.svg` | 62927 | 187 | 1 | R |
| `src/thirdparty/fmt/doc/_static/fonts/glyphicons-halflings-regular.ttf` | 41280 | — | — | M |
| `src/thirdparty/fmt/doc/_static/fonts/glyphicons-halflings-regular.woff` | 23320 | — | — | M |
| `src/thirdparty/fmt/doc/_templates/layout.html` | 5155 | 149 | 1 | R |
| `src/thirdparty/fmt/doc/_templates/search.html` | 1938 | 56 | 1 | R |
| `src/thirdparty/fmt/doc/api.rst` | 21390 | 672 | 1 | R |
| `src/thirdparty/fmt/doc/basic-bootstrap/README` | 141 | 3 | 1 | R |
| `src/thirdparty/fmt/doc/basic-bootstrap/layout.html` | 7616 | 209 | 1 | R |
| `src/thirdparty/fmt/doc/basic-bootstrap/theme.conf` | 24 | 3 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/alerts.less` | 1518 | 74 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/badges.less` | 1201 | 67 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/bootstrap.less` | 1121 | 51 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/breadcrumbs.less` | 594 | 27 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/button-groups.less` | 5659 | 244 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/buttons.less` | 3565 | 161 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/carousel.less` | 5405 | 270 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/close.less` | 764 | 35 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/code.less` | 1401 | 70 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/component-animations.less` | 666 | 34 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/dropdowns.less` | 4764 | 215 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/forms.less` | 14935 | 575 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/glyphicons.less` | 19803 | 306 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/grid.less` | 1390 | 85 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/input-groups.less` | 4215 | 167 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/jumbotron.less` | 993 | 51 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/labels.less` | 1079 | 65 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/list-group.less` | 3022 | 125 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/media.less` | 780 | 62 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins.less` | 1102 | 40 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/alerts.less` | 257 | 15 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/background-variant.less` | 139 | 9 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/border-radius.less` | 468 | 19 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/buttons.less` | 1080 | 53 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/center-block.less` | 120 | 8 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/clearfix.less` | 605 | 23 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/forms.less` | 2641 | 86 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/gradients.less` | 4388 | 60 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/grid-framework.less` | 2784 | 92 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/grid.less` | 3094 | 123 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/hide-text.less` | 579 | 22 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/image.less` | 1062 | 34 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/labels.less` | 161 | 13 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/list-group.less` | 533 | 30 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/nav-divider.less` | 232 | 11 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/nav-vertical-align.less` | 364 | 10 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/opacity.less` | 148 | 9 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/pagination.less` | 438 | 24 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/panels.less` | 537 | 25 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/progress-bar.less` | 191 | 11 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/reset-filter.less` | 248 | 9 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/resize.less` | 196 | 7 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/responsive-visibility.less` | 343 | 16 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/size.less` | 127 | 11 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/tab-focus.less` | 159 | 10 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/table-row.less` | 700 | 29 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/text-emphasis.less` | 116 | 9 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/text-overflow.less` | 162 | 9 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/mixins/vendor-prefixes.less` | 6650 | 228 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/modals.less` | 3565 | 151 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/navbar.less` | 14634 | 661 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/navs.less` | 4930 | 243 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/normalize.less` | 7650 | 428 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/pager.less` | 861 | 55 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/pagination.less` | 2001 | 89 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/panels.less` | 6151 | 266 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/popovers.less` | 3509 | 136 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/print.less` | 2133 | 108 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/progress-bars.less` | 1925 | 88 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/responsive-embed.less` | 546 | 36 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/responsive-utilities.less` | 4262 | 195 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/scaffolding.less` | 3122 | 162 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/tables.less` | 4612 | 235 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/theme.less` | 7812 | 274 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/thumbnails.less` | 753 | 37 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/tooltip.less` | 2937 | 103 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/type.less` | 5951 | 303 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/utilities.less` | 747 | 56 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/variables.less` | 27053 | 862 | 1 | R |
| `src/thirdparty/fmt/doc/bootstrap/wells.less` | 527 | 30 | 1 | R |
| `src/thirdparty/fmt/doc/build.py` | 4970 | 122 | 1 | R |
| `src/thirdparty/fmt/doc/conf.py` | 8122 | 257 | 1 | R |
| `src/thirdparty/fmt/doc/contents.rst` | 84 | 11 | 1 | R |
| `src/thirdparty/fmt/doc/fmt.less` | 1256 | 72 | 1 | R |
| `src/thirdparty/fmt/doc/html/_sources/api.rst.txt` | 21390 | 672 | 1 | R |
| `src/thirdparty/fmt/doc/html/_sources/contents.rst.txt` | 84 | 11 | 1 | R |
| `src/thirdparty/fmt/doc/html/_sources/index.rst.txt` | 5474 | 199 | 1 | R |
| `src/thirdparty/fmt/doc/html/_sources/syntax.rst.txt` | 34660 | 641 | 1 | R |
| `src/thirdparty/fmt/doc/html/_sources/usage.rst.txt` | 6621 | 213 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/basic.css` | 13647 | 856 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/bootstrap.min.js` | 35951 | 7 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/doctools.js` | 9416 | 317 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/documentation_options.js` | 356 | 12 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/file.png` | 286 | — | — | M |
| `src/thirdparty/fmt/doc/html/_static/fmt.css` | 117721 | 1 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/fonts/glyphicons-halflings-regular.eot` | 20335 | — | — | M |
| `src/thirdparty/fmt/doc/html/_static/fonts/glyphicons-halflings-regular.svg` | 62927 | 187 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/fonts/glyphicons-halflings-regular.ttf` | 41280 | — | — | M |
| `src/thirdparty/fmt/doc/html/_static/fonts/glyphicons-halflings-regular.woff` | 23320 | — | — | M |
| `src/thirdparty/fmt/doc/html/_static/jquery-3.5.1.js` | 287630 | 10873 | 11 | R |
| `src/thirdparty/fmt/doc/html/_static/jquery.js` | 89476 | 3 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/language_data.js` | 10847 | 298 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/minus.png` | 90 | — | — | M |
| `src/thirdparty/fmt/doc/html/_static/plus.png` | 90 | — | — | M |
| `src/thirdparty/fmt/doc/html/_static/pygments.css` | 4929 | 75 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/searchtools.js` | 16323 | 515 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/underscore-1.3.1.js` | 35168 | 1000 | 1 | R |
| `src/thirdparty/fmt/doc/html/_static/underscore.js` | 12140 | 32 | 1 | R |
| `src/thirdparty/fmt/doc/html/api.html` | 207871 | 419 | 1 | R |
| `src/thirdparty/fmt/doc/html/contents.html` | 8253 | 189 | 1 | R |
| `src/thirdparty/fmt/doc/html/genindex.html` | 15947 | 318 | 1 | R |
| `src/thirdparty/fmt/doc/html/index.html` | 19318 | 325 | 1 | R |
| `src/thirdparty/fmt/doc/html/objects.inv` | 5698 | — | — | M |
| `src/thirdparty/fmt/doc/html/search.html` | 4986 | 171 | 1 | R |
| `src/thirdparty/fmt/doc/html/searchindex.js` | 33573 | 1 | 1 | R |
| `src/thirdparty/fmt/doc/html/syntax.html` | 65425 | 700 | 1 | R |
| `src/thirdparty/fmt/doc/html/usage.html` | 23009 | 335 | 1 | R |
| `src/thirdparty/fmt/doc/index.rst` | 5474 | 199 | 1 | R |
| `src/thirdparty/fmt/doc/python-license.txt` | 14903 | 291 | 1 | R |
| `src/thirdparty/fmt/doc/syntax.rst` | 34660 | 641 | 1 | R |
| `src/thirdparty/fmt/doc/usage.rst` | 6621 | 213 | 1 | R |
| `src/thirdparty/fmt/include/fmt/args.h` | 7418 | 235 | 1 | R |
| `src/thirdparty/fmt/include/fmt/chrono.h` | 72610 | 2209 | 3 | R |
| `src/thirdparty/fmt/include/fmt/color.h` | 24267 | 633 | 1 | R |
| `src/thirdparty/fmt/include/fmt/compile.h` | 19004 | 535 | 1 | R |
| `src/thirdparty/fmt/include/fmt/core.h` | 98470 | 2925 | 3 | R |
| `src/thirdparty/fmt/include/fmt/format-inl.h` | 73122 | 1663 | 2 | R |
| `src/thirdparty/fmt/include/fmt/format.h` | 163533 | 4500 | 5 | R |
| `src/thirdparty/fmt/include/fmt/os.h` | 13040 | 452 | 1 | R |
| `src/thirdparty/fmt/include/fmt/ostream.h` | 6554 | 210 | 1 | R |
| `src/thirdparty/fmt/include/fmt/printf.h` | 20677 | 668 | 1 | R |
| `src/thirdparty/fmt/include/fmt/ranges.h` | 24258 | 736 | 1 | R |
| `src/thirdparty/fmt/include/fmt/std.h` | 13202 | 441 | 1 | R |
| `src/thirdparty/fmt/include/fmt/xchar.h` | 9895 | 259 | 1 | R |
| `src/thirdparty/fmt/src/fmt.cc` | 2568 | 111 | 1 | R |
| `src/thirdparty/fmt/src/format.cc` | 1333 | 44 | 1 | R |
| `src/thirdparty/fmt/src/os.cc` | 11533 | 399 | 1 | R |
| `src/thirdparty/fmt/support/Android.mk` | 299 | 16 | 1 | R |
| `src/thirdparty/fmt/support/AndroidManifest.xml` | 34 | 2 | 1 | R |
| `src/thirdparty/fmt/support/C++.sublime-syntax` | 72324 | 2062 | 3 | R |
| `src/thirdparty/fmt/support/README` | 85 | 5 | 1 | R |
| `src/thirdparty/fmt/support/Vagrantfile` | 602 | 21 | 1 | R |
| `src/thirdparty/fmt/support/bazel/.bazelversion` | 6 | 2 | 1 | R |
| `src/thirdparty/fmt/support/bazel/BUILD.bazel` | 697 | 29 | 1 | R |
| `src/thirdparty/fmt/support/bazel/README.md` | 2484 | 75 | 1 | R |
| `src/thirdparty/fmt/support/bazel/WORKSPACE.bazel` | 24 | 2 | 1 | R |
| `src/thirdparty/fmt/support/build-docs.py` | 2064 | 59 | 1 | R |
| `src/thirdparty/fmt/support/build.gradle` | 3977 | 133 | 1 | R |
| `src/thirdparty/fmt/support/cmake/FindSetEnv.cmake` | 299 | 8 | 1 | R |
| `src/thirdparty/fmt/support/cmake/JoinPaths.cmake` | 959 | 27 | 1 | R |
| `src/thirdparty/fmt/support/cmake/fmt-config.cmake.in` | 147 | 8 | 1 | R |
| `src/thirdparty/fmt/support/cmake/fmt.pc.in` | 264 | 12 | 1 | R |
| `src/thirdparty/fmt/support/compute-powers.py` | 1590 | 54 | 1 | R |
| `src/thirdparty/fmt/support/docopt.py` | 19784 | 582 | 1 | R |
| `src/thirdparty/fmt/support/manage.py` | 11495 | 304 | 1 | R |
| `src/thirdparty/fmt/support/printable.py` | 6109 | 202 | 1 | R |
| `src/thirdparty/fmt/support/rst2md.py` | 4072 | 167 | 1 | R |
| `src/thirdparty/fmt/support/rtd/conf.py` | 123 | 8 | 1 | R |
| `src/thirdparty/fmt/support/rtd/index.rst` | 112 | 3 | 1 | R |
| `src/thirdparty/fmt/support/rtd/theme/layout.html` | 483 | 18 | 1 | R |
| `src/thirdparty/fmt/support/rtd/theme/theme.conf` | 24 | 3 | 1 | R |
| `src/thirdparty/fmt/test/CMakeLists.txt` | 9289 | 258 | 1 | R |
| `src/thirdparty/fmt/test/add-subdirectory-test/CMakeLists.txt` | 595 | 18 | 1 | R |
| `src/thirdparty/fmt/test/add-subdirectory-test/main.cc` | 128 | 6 | 1 | R |
| `src/thirdparty/fmt/test/args-test.cc` | 5201 | 187 | 1 | R |
| `src/thirdparty/fmt/test/assert-test.cc` | 852 | 32 | 1 | R |
| `src/thirdparty/fmt/test/chrono-test.cc` | 40163 | 1004 | 2 | R |
| `src/thirdparty/fmt/test/color-test.cc` | 3278 | 73 | 1 | R |
| `src/thirdparty/fmt/test/compile-error-test/CMakeLists.txt` | 7533 | 242 | 1 | R |
| `src/thirdparty/fmt/test/compile-fp-test.cc` | 2822 | 64 | 1 | R |
| `src/thirdparty/fmt/test/compile-test.cc` | 13366 | 360 | 1 | R |
| `src/thirdparty/fmt/test/core-test.cc` | 27207 | 859 | 1 | R |
| `src/thirdparty/fmt/test/cuda-test/CMakeLists.txt` | 3364 | 74 | 1 | R |
| `src/thirdparty/fmt/test/cuda-test/cpp14.cc` | 418 | 12 | 1 | R |
| `src/thirdparty/fmt/test/cuda-test/cuda-cpp14.cu` | 905 | 29 | 1 | R |
| `src/thirdparty/fmt/test/detect-stdfs.cc` | 607 | 19 | 1 | R |
| `src/thirdparty/fmt/test/enforce-checks-test.cc` | 1746 | 66 | 1 | R |
| `src/thirdparty/fmt/test/find-package-test/CMakeLists.txt` | 590 | 18 | 1 | R |
| `src/thirdparty/fmt/test/find-package-test/main.cc` | 130 | 6 | 1 | R |
| `src/thirdparty/fmt/test/format-impl-test.cc` | 15011 | 484 | 1 | R |
| `src/thirdparty/fmt/test/format-test.cc` | 90027 | 2261 | 3 | R |
| `src/thirdparty/fmt/test/fuzzing/CMakeLists.txt` | 1225 | 31 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/README.md` | 789 | 26 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/build.sh` | 2113 | 91 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/chrono-duration.cc` | 3303 | 137 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/chrono-timepoint.cc` | 828 | 33 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/float.cc` | 1219 | 40 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/fuzzer-common.h` | 2419 | 78 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/main.cc` | 592 | 23 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/named-arg.cc` | 2266 | 103 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/one-arg.cc` | 2048 | 93 | 1 | R |
| `src/thirdparty/fmt/test/fuzzing/two-args.cc` | 2343 | 107 | 1 | R |
| `src/thirdparty/fmt/test/gtest-extra-test.cc` | 13918 | 416 | 1 | R |
| `src/thirdparty/fmt/test/gtest-extra.cc` | 1983 | 81 | 1 | R |
| `src/thirdparty/fmt/test/gtest-extra.h` | 7628 | 166 | 1 | R |
| `src/thirdparty/fmt/test/gtest/.clang-format` | 68 | 4 | 1 | R |
| `src/thirdparty/fmt/test/gtest/CMakeLists.txt` | 1291 | 33 | 1 | R |
| `src/thirdparty/fmt/test/gtest/gmock-gtest-all.cc` | 532560 | 14435 | 15 | R |
| `src/thirdparty/fmt/test/gtest/gmock/gmock.h` | 454528 | 11646 | 12 | R |
| `src/thirdparty/fmt/test/gtest/gtest/gtest-spi.h` | 10112 | 239 | 1 | R |
| `src/thirdparty/fmt/test/gtest/gtest/gtest.h` | 474089 | 12399 | 13 | R |
| `src/thirdparty/fmt/test/header-only-test.cc` | 253 | 12 | 1 | R |
| `src/thirdparty/fmt/test/mock-allocator.h` | 1546 | 65 | 1 | R |
| `src/thirdparty/fmt/test/module-test.cc` | 16599 | 521 | 1 | R |
| `src/thirdparty/fmt/test/noexception-test.cc` | 437 | 19 | 1 | R |
| `src/thirdparty/fmt/test/os-test.cc` | 13791 | 507 | 1 | R |
| `src/thirdparty/fmt/test/ostream-test.cc` | 8803 | 292 | 1 | R |
| `src/thirdparty/fmt/test/posix-mock-test.cc` | 11508 | 454 | 1 | R |
| `src/thirdparty/fmt/test/posix-mock.h` | 1556 | 76 | 1 | R |
| `src/thirdparty/fmt/test/printf-test.cc` | 19706 | 561 | 1 | R |
| `src/thirdparty/fmt/test/ranges-odr-test.cc` | 454 | 18 | 1 | R |
| `src/thirdparty/fmt/test/ranges-test.cc` | 16096 | 528 | 1 | R |
| `src/thirdparty/fmt/test/scan-test.cc` | 2806 | 117 | 1 | R |
| `src/thirdparty/fmt/test/scan.h` | 6982 | 245 | 1 | R |
| `src/thirdparty/fmt/test/static-export-test/CMakeLists.txt` | 813 | 31 | 1 | R |
| `src/thirdparty/fmt/test/static-export-test/library.cc` | 142 | 6 | 1 | R |
| `src/thirdparty/fmt/test/static-export-test/main.cc` | 114 | 7 | 1 | R |
| `src/thirdparty/fmt/test/std-test.cc` | 8156 | 239 | 1 | R |
| `src/thirdparty/fmt/test/test-assert.h` | 1113 | 40 | 1 | R |
| `src/thirdparty/fmt/test/test-main.cc` | 1210 | 44 | 1 | R |
| `src/thirdparty/fmt/test/unicode-test.cc` | 1314 | 49 | 1 | R |
| `src/thirdparty/fmt/test/util.cc` | 1152 | 47 | 1 | R |
| `src/thirdparty/fmt/test/util.h` | 2047 | 82 | 1 | R |
| `src/thirdparty/fmt/test/xchar-test.cc` | 20656 | 585 | 1 | R |
| `src/thirdparty/json/LICENSE.MIT` | 1076 | 22 | 1 | R |
| `src/thirdparty/json/include/nlohmann/adl_serializer.hpp` | 2279 | 56 | 1 | R |
| `src/thirdparty/json/include/nlohmann/byte_container_with_subtype.hpp` | 3533 | 104 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/abi_macros.hpp` | 3807 | 101 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/conversions/from_json.hpp` | 18880 | 498 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/conversions/to_chars.hpp` | 38503 | 1119 | 2 | R |
| `src/thirdparty/json/include/nlohmann/detail/conversions/to_json.hpp` | 15749 | 447 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/exceptions.hpp` | 9128 | 256 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/hash.hpp` | 4016 | 130 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/input/binary_reader.hpp` | 103174 | 3011 | 4 | R |
| `src/thirdparty/json/include/nlohmann/detail/input/input_adapters.hpp` | 17372 | 495 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/input/json_sax.hpp` | 21370 | 729 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/input/lexer.hpp` | 54544 | 1633 | 2 | R |
| `src/thirdparty/json/include/nlohmann/detail/input/parser.hpp` | 18722 | 508 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/input/position_t.hpp` | 958 | 38 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/iterators/internal_iterator.hpp` | 1071 | 36 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/iterators/iter_impl.hpp` | 23906 | 752 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/iterators/iteration_proxy.hpp` | 7962 | 243 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/iterators/iterator_traits.hpp` | 1757 | 62 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/iterators/json_reverse_iterator.hpp` | 3861 | 131 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/iterators/primitive_iterator.hpp` | 3227 | 133 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/json_pointer.hpp` | 37067 | 989 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/json_ref.hpp` | 1811 | 79 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/macro_scope.hpp` | 42861 | 469 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/macro_unscope.hpp` | 1220 | 45 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/meta/call_std/begin.hpp` | 453 | 18 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/meta/call_std/end.hpp` | 451 | 18 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/meta/cpp_future.hpp` | 5178 | 172 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/meta/detected.hpp` | 2109 | 71 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/meta/identity_tag.hpp` | 526 | 22 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/meta/is_sax.hpp` | 6960 | 160 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/meta/std_fs.hpp` | 766 | 30 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/meta/type_traits.hpp` | 28099 | 741 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/meta/void_t.hpp` | 597 | 25 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/output/binary_writer.hpp` | 69893 | 1839 | 2 | R |
| `src/thirdparty/json/include/nlohmann/detail/output/output_adapters.hpp` | 4067 | 148 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/output/serializer.hpp` | 39896 | 989 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/string_concat.hpp` | 6006 | 147 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/string_escape.hpp` | 2168 | 73 | 1 | R |
| `src/thirdparty/json/include/nlohmann/detail/value_t.hpp` | 4326 | 119 | 1 | R |
| `src/thirdparty/json/include/nlohmann/json.hpp` | 193862 | 5215 | 6 | R |
| `src/thirdparty/json/include/nlohmann/json_fwd.hpp` | 2469 | 75 | 1 | R |
| `src/thirdparty/json/include/nlohmann/ordered_map.hpp` | 11447 | 360 | 1 | R |
| `src/thirdparty/json/include/nlohmann/thirdparty/hedley/hedley.hpp` | 86068 | 2046 | 3 | R |
| `src/thirdparty/json/include/nlohmann/thirdparty/hedley/hedley_undef.hpp` | 5500 | 159 | 1 | R |
| `src/thirdparty/json/meson.build` | 629 | 25 | 1 | R |
| `src/thirdparty/json/single_include/nlohmann/json.hpp` | 907858 | 2737 | 3 | R |
| `src/thirdparty/libevent/.clang-format` | 1624 | 64 | 1 | R |
| `src/thirdparty/libevent/.exrc` | 304 | 16 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/abi.yml` | 2248 | 76 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/build.yml` | 32310 | 974 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/cifuzz.yml` | 817 | 37 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/coverage.yml` | 1278 | 53 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/doxygen.yml` | 1496 | 58 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/linux.yml` | 7786 | 228 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/macos.yml` | 6021 | 188 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/master.yml` | 4858 | 170 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/mingw.yml` | 4613 | 156 | 1 | R |
| `src/thirdparty/libevent/.github/workflows/windows.yml` | 8116 | 247 | 1 | R |
| `src/thirdparty/libevent/.gitignore` | 2058 | 174 | 1 | R |
| `src/thirdparty/libevent/.mailmap` | 249 | 8 | 1 | R |
| `src/thirdparty/libevent/.uncrustify` | 2675 | 56 | 1 | R |
| `src/thirdparty/libevent/.yamllint` | 192 | 12 | 1 | R |
| `src/thirdparty/libevent/CMakeLists.txt` | 51769 | 1748 | 2 | R |
| `src/thirdparty/libevent/CONTRIBUTING.md` | 875 | 39 | 1 | R |
| `src/thirdparty/libevent/CONTRIBUTORS.md` | 5914 | 357 | 1 | R |
| `src/thirdparty/libevent/ChangeLog` | 21755 | 321 | 1 | R |
| `src/thirdparty/libevent/ChangeLog-1.4` | 17369 | 232 | 1 | R |
| `src/thirdparty/libevent/ChangeLog-2.0` | 83347 | 894 | 1 | R |
| `src/thirdparty/libevent/ChangeLog-2.1` | 101174 | 860 | 1 | R |
| `src/thirdparty/libevent/Documentation/Building.md` | 6289 | 230 | 1 | R |
| `src/thirdparty/libevent/Doxyfile` | 10534 | 260 | 1 | R |
| `src/thirdparty/libevent/LICENSE` | 6751 | 152 | 1 | R |
| `src/thirdparty/libevent/Makefile.am` | 10236 | 369 | 1 | R |
| `src/thirdparty/libevent/PreLoad.cmake` | 267 | 8 | 1 | R |
| `src/thirdparty/libevent/README.md` | 3241 | 102 | 1 | R |
| `src/thirdparty/libevent/SECURITY.md` | 688 | 12 | 1 | R |
| `src/thirdparty/libevent/WIN32-Code/getopt.c` | 4230 | 150 | 1 | R |
| `src/thirdparty/libevent/WIN32-Code/getopt.h` | 688 | 34 | 1 | R |
| `src/thirdparty/libevent/WIN32-Code/getopt_long.c` | 6304 | 235 | 1 | R |
| `src/thirdparty/libevent/WIN32-Code/tree.h` | 22646 | 678 | 1 | R |
| `src/thirdparty/libevent/arc4random.c` | 12134 | 527 | 1 | R |
| `src/thirdparty/libevent/autogen.sh` | 429 | 23 | 1 | R |
| `src/thirdparty/libevent/buffer.c` | 83517 | 3507 | 4 | R |
| `src/thirdparty/libevent/buffer_iocp.c` | 8662 | 328 | 1 | R |
| `src/thirdparty/libevent/bufferevent-internal.h` | 19626 | 526 | 1 | R |
| `src/thirdparty/libevent/bufferevent.c` | 27436 | 1070 | 2 | R |
| `src/thirdparty/libevent/bufferevent_async.c` | 18675 | 707 | 1 | R |
| `src/thirdparty/libevent/bufferevent_filter.c` | 18014 | 626 | 1 | R |
| `src/thirdparty/libevent/bufferevent_mbedtls.c` | 10792 | 426 | 1 | R |
| `src/thirdparty/libevent/bufferevent_openssl.c` | 13571 | 538 | 1 | R |
| `src/thirdparty/libevent/bufferevent_pair.c` | 9797 | 366 | 1 | R |
| `src/thirdparty/libevent/bufferevent_ratelim.c` | 30252 | 1095 | 2 | R |
| `src/thirdparty/libevent/bufferevent_sock.c` | 18434 | 714 | 1 | R |
| `src/thirdparty/libevent/bufferevent_ssl.c` | 31700 | 1185 | 2 | R |
| `src/thirdparty/libevent/changelist-internal.h` | 4627 | 103 | 1 | R |
| `src/thirdparty/libevent/checkpatch.sh` | 7047 | 300 | 1 | R |
| `src/thirdparty/libevent/cmake/AddCompilerFlags.cmake` | 336 | 14 | 1 | R |
| `src/thirdparty/libevent/cmake/AddEventLibrary.cmake` | 6507 | 183 | 1 | R |
| `src/thirdparty/libevent/cmake/COPYING-CMAKE-SCRIPTS` | 1326 | 22 | 1 | R |
| `src/thirdparty/libevent/cmake/CheckConstExists.cmake` | 815 | 26 | 1 | R |
| `src/thirdparty/libevent/cmake/CheckFileOffsetBits.c` | 295 | 15 | 1 | R |
| `src/thirdparty/libevent/cmake/CheckFileOffsetBits.cmake` | 1975 | 44 | 1 | R |
| `src/thirdparty/libevent/cmake/CheckFunctionKeywords.cmake` | 545 | 15 | 1 | R |
| `src/thirdparty/libevent/cmake/CheckPrototypeDefinition.c.in` | 514 | 30 | 1 | R |
| `src/thirdparty/libevent/cmake/CheckPrototypeDefinition.cmake` | 3789 | 83 | 1 | R |
| `src/thirdparty/libevent/cmake/CheckWorkingKqueue.cmake` | 994 | 55 | 1 | R |
| `src/thirdparty/libevent/cmake/CodeCoverage.cmake` | 6220 | 161 | 1 | R |
| `src/thirdparty/libevent/cmake/Copyright.txt` | 2655 | 57 | 1 | R |
| `src/thirdparty/libevent/cmake/FindMbedTLS.cmake` | 6694 | 177 | 1 | R |
| `src/thirdparty/libevent/cmake/LibeventConfig.cmake.in` | 6075 | 148 | 1 | R |
| `src/thirdparty/libevent/cmake/LibeventConfigVersion.cmake.in` | 386 | 12 | 1 | R |
| `src/thirdparty/libevent/cmake/Macros.cmake` | 1217 | 37 | 1 | R |
| `src/thirdparty/libevent/cmake/RenameDoxygen.cmake` | 596 | 18 | 1 | R |
| `src/thirdparty/libevent/cmake/Uninstall.cmake.in` | 1094 | 24 | 1 | R |
| `src/thirdparty/libevent/cmake/UseDoxygen.cmake` | 4026 | 119 | 1 | R |
| `src/thirdparty/libevent/cmake/VersionViaGit.cmake` | 1980 | 67 | 1 | R |
| `src/thirdparty/libevent/compat/sys/queue.h` | 16674 | 489 | 1 | R |
| `src/thirdparty/libevent/configure.ac` | 26400 | 892 | 1 | R |
| `src/thirdparty/libevent/defer-internal.h` | 2829 | 74 | 1 | R |
| `src/thirdparty/libevent/devpoll.c` | 7889 | 312 | 1 | R |
| `src/thirdparty/libevent/doxygen.am` | 2648 | 81 | 1 | R |
| `src/thirdparty/libevent/epoll.c` | 16650 | 605 | 1 | R |
| `src/thirdparty/libevent/epoll_sub.c` | 2391 | 67 | 1 | R |
| `src/thirdparty/libevent/epolltable-internal.h` | 41360 | 1167 | 2 | R |
| `src/thirdparty/libevent/evbuffer-internal.h` | 12859 | 354 | 1 | R |
| `src/thirdparty/libevent/evconfig-private.h.cmake` | 1045 | 41 | 1 | R |
| `src/thirdparty/libevent/evconfig-private.h.in` | 1364 | 56 | 1 | R |
| `src/thirdparty/libevent/evdns.c` | 160041 | 5712 | 6 | R |
| `src/thirdparty/libevent/event-config.h.cmake` | 17249 | 529 | 1 | R |
| `src/thirdparty/libevent/event-internal.h` | 18994 | 538 | 1 | R |
| `src/thirdparty/libevent/event.c` | 106524 | 4094 | 5 | R |
| `src/thirdparty/libevent/event_iocp.c` | 7706 | 295 | 1 | R |
| `src/thirdparty/libevent/event_rpcgen.py` | 54938 | 1926 | 2 | R |
| `src/thirdparty/libevent/event_tagging.c` | 14730 | 610 | 1 | R |
| `src/thirdparty/libevent/evmap-internal.h` | 4893 | 118 | 1 | R |
| `src/thirdparty/libevent/evmap.c` | 29039 | 1064 | 2 | R |
| `src/thirdparty/libevent/evport.c` | 12005 | 452 | 1 | R |
| `src/thirdparty/libevent/evrpc-internal.h` | 5711 | 206 | 1 | R |
| `src/thirdparty/libevent/evrpc.c` | 29379 | 1172 | 2 | R |
| `src/thirdparty/libevent/evsignal-internal.h` | 3055 | 79 | 1 | R |
| `src/thirdparty/libevent/evthread-internal.h` | 14644 | 407 | 1 | R |
| `src/thirdparty/libevent/evthread.c` | 13725 | 510 | 1 | R |
| `src/thirdparty/libevent/evthread_pthread.c` | 5838 | 230 | 1 | R |
| `src/thirdparty/libevent/evthread_win32.c` | 8595 | 342 | 1 | R |
| `src/thirdparty/libevent/evutil.c` | 78692 | 3011 | 4 | R |
| `src/thirdparty/libevent/evutil_rand.c` | 5342 | 207 | 1 | R |
| `src/thirdparty/libevent/evutil_time.c` | 18286 | 622 | 1 | R |
| `src/thirdparty/libevent/extra/abi-check/README.md` | 1158 | 36 | 1 | R |
| `src/thirdparty/libevent/extra/abi-check/abi_check.sh` | 1647 | 54 | 1 | R |
| `src/thirdparty/libevent/extra/abi-check/libevent.json` | 465 | 13 | 1 | R |
| `src/thirdparty/libevent/extra/lsan.supp` | 85 | 4 | 1 | R |
| `src/thirdparty/libevent/extra/release/changelog.py` | 2686 | 82 | 1 | R |
| `src/thirdparty/libevent/extra/release/update-contributors.sh` | 775 | 25 | 1 | R |
| `src/thirdparty/libevent/extra/tsan.supp` | 122 | 4 | 1 | R |
| `src/thirdparty/libevent/ht-internal.h` | 29185 | 488 | 1 | R |
| `src/thirdparty/libevent/http-internal.h` | 6939 | 239 | 1 | R |
| `src/thirdparty/libevent/http.c` | 139221 | 5609 | 6 | R |
| `src/thirdparty/libevent/include/evdns.h` | 2019 | 46 | 1 | R |
| `src/thirdparty/libevent/include/event.h` | 2749 | 84 | 1 | R |
| `src/thirdparty/libevent/include/event2/buffer.h` | 39726 | 1099 | 2 | R |
| `src/thirdparty/libevent/include/event2/buffer_compat.h` | 4748 | 117 | 1 | R |
| `src/thirdparty/libevent/include/event2/bufferevent.h` | 36204 | 1062 | 2 | R |
| `src/thirdparty/libevent/include/event2/bufferevent_compat.h` | 4748 | 113 | 1 | R |
| `src/thirdparty/libevent/include/event2/bufferevent_ssl.h` | 9822 | 276 | 1 | R |
| `src/thirdparty/libevent/include/event2/bufferevent_struct.h` | 4136 | 117 | 1 | R |
| `src/thirdparty/libevent/include/event2/dns.h` | 30712 | 839 | 1 | R |
| `src/thirdparty/libevent/include/event2/dns_compat.h` | 12595 | 359 | 1 | R |
| `src/thirdparty/libevent/include/event2/dns_struct.h` | 2596 | 81 | 1 | R |
| `src/thirdparty/libevent/include/event2/event.h` | 63771 | 1705 | 2 | R |
| `src/thirdparty/libevent/include/event2/event_compat.h` | 7646 | 231 | 1 | R |
| `src/thirdparty/libevent/include/event2/event_struct.h` | 5029 | 183 | 1 | R |
| `src/thirdparty/libevent/include/event2/http.h` | 52659 | 1483 | 2 | R |
| `src/thirdparty/libevent/include/event2/http_compat.h` | 3272 | 95 | 1 | R |
| `src/thirdparty/libevent/include/event2/http_struct.h` | 4810 | 153 | 1 | R |
| `src/thirdparty/libevent/include/event2/keyvalq_struct.h` | 2603 | 81 | 1 | R |
| `src/thirdparty/libevent/include/event2/listener.h` | 8422 | 203 | 1 | R |
| `src/thirdparty/libevent/include/event2/rpc.h` | 21775 | 627 | 1 | R |
| `src/thirdparty/libevent/include/event2/rpc_compat.h` | 2358 | 62 | 1 | R |
| `src/thirdparty/libevent/include/event2/rpc_struct.h` | 3598 | 115 | 1 | R |
| `src/thirdparty/libevent/include/event2/tag.h` | 4921 | 147 | 1 | R |
| `src/thirdparty/libevent/include/event2/tag_compat.h` | 2148 | 50 | 1 | R |
| `src/thirdparty/libevent/include/event2/thread.h` | 10457 | 269 | 1 | R |
| `src/thirdparty/libevent/include/event2/util.h` | 30111 | 906 | 1 | R |
| `src/thirdparty/libevent/include/event2/visibility.h` | 2918 | 69 | 1 | R |
| `src/thirdparty/libevent/include/event2/watch.h` | 5430 | 137 | 1 | R |
| `src/thirdparty/libevent/include/event2/ws.h` | 1825 | 59 | 1 | R |
| `src/thirdparty/libevent/include/evhttp.h` | 2035 | 46 | 1 | R |
| `src/thirdparty/libevent/include/evrpc.h` | 2015 | 46 | 1 | R |
| `src/thirdparty/libevent/include/evutil.h` | 1782 | 40 | 1 | R |
| `src/thirdparty/libevent/include/include.am` | 1589 | 56 | 1 | R |
| `src/thirdparty/libevent/iocp-internal.h` | 7993 | 216 | 1 | R |
| `src/thirdparty/libevent/ipv6-internal.h` | 2514 | 84 | 1 | R |
| `src/thirdparty/libevent/kqueue-internal.h` | 1990 | 40 | 1 | R |
| `src/thirdparty/libevent/kqueue.c` | 15128 | 581 | 1 | R |
| `src/thirdparty/libevent/libevent.pc.in` | 336 | 17 | 1 | R |
| `src/thirdparty/libevent/libevent_core.pc.in` | 280 | 14 | 1 | R |
| `src/thirdparty/libevent/libevent_extra.pc.in` | 307 | 15 | 1 | R |
| `src/thirdparty/libevent/libevent_mbedtls.pc.in` | 377 | 15 | 1 | R |
| `src/thirdparty/libevent/libevent_openssl.pc.in` | 381 | 16 | 1 | R |
| `src/thirdparty/libevent/libevent_pthreads.pc.in` | 391 | 15 | 1 | R |
| `src/thirdparty/libevent/listener.c` | 22246 | 923 | 1 | R |
| `src/thirdparty/libevent/log-internal.h` | 3391 | 95 | 1 | R |
| `src/thirdparty/libevent/log.c` | 5465 | 245 | 1 | R |
| `src/thirdparty/libevent/m4/ac_backport_259_ssizet.m4` | 104 | 4 | 1 | R |
| `src/thirdparty/libevent/m4/ax_check_compile_flag.m4` | 2104 | 54 | 1 | R |
| `src/thirdparty/libevent/m4/ax_check_funcs_ex.m4` | 668 | 23 | 1 | R |
| `src/thirdparty/libevent/m4/ax_prog_doxygen.m4` | 20841 | 602 | 1 | R |
| `src/thirdparty/libevent/m4/ax_pthread.m4` | 22682 | 523 | 1 | R |
| `src/thirdparty/libevent/m4/libevent_mbedtls.m4` | 1163 | 38 | 1 | R |
| `src/thirdparty/libevent/m4/libevent_openssl.m4` | 3379 | 101 | 1 | R |
| `src/thirdparty/libevent/make-event-config.sed` | 731 | 28 | 1 | R |
| `src/thirdparty/libevent/make_epoll_table.py` | 1807 | 64 | 1 | R |
| `src/thirdparty/libevent/mbedtls-compat.h` | 706 | 31 | 1 | R |
| `src/thirdparty/libevent/minheap-internal.h` | 6947 | 189 | 1 | R |
| `src/thirdparty/libevent/mm-internal.h` | 3501 | 93 | 1 | R |
| `src/thirdparty/libevent/openssl-compat.h` | 1453 | 49 | 1 | R |
| `src/thirdparty/libevent/poll.c` | 8680 | 360 | 1 | R |
| `src/thirdparty/libevent/ratelim-internal.h` | 4092 | 106 | 1 | R |
| `src/thirdparty/libevent/sample/becat.c` | 14624 | 659 | 1 | R |
| `src/thirdparty/libevent/sample/dns-example.c` | 6854 | 270 | 1 | R |
| `src/thirdparty/libevent/sample/event-read-fifo.c` | 3421 | 163 | 1 | R |
| `src/thirdparty/libevent/sample/hello-world.c` | 3482 | 141 | 1 | R |
| `src/thirdparty/libevent/sample/hostcheck.c` | 6623 | 218 | 1 | R |
| `src/thirdparty/libevent/sample/hostcheck.h` | 1259 | 31 | 1 | R |
| `src/thirdparty/libevent/sample/http-connect.c` | 3479 | 132 | 1 | R |
| `src/thirdparty/libevent/sample/http-server.c` | 14040 | 593 | 1 | R |
| `src/thirdparty/libevent/sample/https-client.c` | 16645 | 672 | 1 | R |
| `src/thirdparty/libevent/sample/include.am` | 3021 | 81 | 1 | R |
| `src/thirdparty/libevent/sample/le-proxy.c` | 7755 | 319 | 1 | R |
| `src/thirdparty/libevent/sample/openssl_hostname_validation.c` | 7101 | 179 | 1 | R |
| `src/thirdparty/libevent/sample/openssl_hostname_validation.h` | 2251 | 57 | 1 | R |
| `src/thirdparty/libevent/sample/signal-test.c` | 1380 | 84 | 1 | R |
| `src/thirdparty/libevent/sample/ssl-client-mbedtls.c` | 8594 | 300 | 1 | R |
| `src/thirdparty/libevent/sample/time-test.c` | 2217 | 111 | 1 | R |
| `src/thirdparty/libevent/sample/watch-timing.c` | 11165 | 360 | 1 | R |
| `src/thirdparty/libevent/sample/ws-chat-server.c` | 5441 | 256 | 1 | R |
| `src/thirdparty/libevent/sample/ws-chat.html` | 2350 | 103 | 1 | R |
| `src/thirdparty/libevent/select.c` | 8728 | 348 | 1 | R |
| `src/thirdparty/libevent/sha1.c` | 9166 | 307 | 1 | R |
| `src/thirdparty/libevent/sha1.h` | 214 | 15 | 1 | R |
| `src/thirdparty/libevent/signal.c` | 13305 | 507 | 1 | R |
| `src/thirdparty/libevent/signalfd.c` | 6177 | 214 | 1 | R |
| `src/thirdparty/libevent/ssl-compat.h` | 3612 | 102 | 1 | R |
| `src/thirdparty/libevent/strlcpy-internal.h` | 418 | 25 | 1 | R |
| `src/thirdparty/libevent/strlcpy.c` | 2564 | 72 | 1 | R |
| `src/thirdparty/libevent/test-export/CMakeLists.txt` | 620 | 17 | 1 | R |
| `src/thirdparty/libevent/test-export/test-export.c` | 3053 | 168 | 1 | R |
| `src/thirdparty/libevent/test-export/test-export.py` | 6497 | 202 | 1 | R |
| `src/thirdparty/libevent/test/bench.c` | 5844 | 236 | 1 | R |
| `src/thirdparty/libevent/test/bench_cascade.c` | 4694 | 189 | 1 | R |
| `src/thirdparty/libevent/test/bench_http.c` | 5062 | 201 | 1 | R |
| `src/thirdparty/libevent/test/bench_httpclient.c` | 6036 | 241 | 1 | R |
| `src/thirdparty/libevent/test/check-dumpevents.py` | 1547 | 55 | 1 | R |
| `src/thirdparty/libevent/test/include.am` | 5592 | 190 | 1 | R |
| `src/thirdparty/libevent/test/print-winsock-errors.c` | 1913 | 87 | 1 | R |
| `src/thirdparty/libevent/test/regress.c` | 89480 | 3633 | 4 | R |
| `src/thirdparty/libevent/test/regress.h` | 5639 | 159 | 1 | R |
| `src/thirdparty/libevent/test/regress.rpc` | 465 | 26 | 1 | R |
| `src/thirdparty/libevent/test/regress_buffer.c` | 82315 | 2877 | 3 | R |
| `src/thirdparty/libevent/test/regress_bufferevent.c` | 41427 | 1501 | 2 | R |
| `src/thirdparty/libevent/test/regress_dns.c` | 91694 | 3096 | 4 | R |
| `src/thirdparty/libevent/test/regress_et.c` | 8217 | 271 | 1 | R |
| `src/thirdparty/libevent/test/regress_finalize.c` | 10039 | 396 | 1 | R |
| `src/thirdparty/libevent/test/regress_http.c` | 166813 | 6049 | 7 | R |
| `src/thirdparty/libevent/test/regress_http.h` | 389 | 12 | 1 | R |
| `src/thirdparty/libevent/test/regress_iocp.c` | 8993 | 353 | 1 | R |
| `src/thirdparty/libevent/test/regress_listener.c` | 13520 | 485 | 1 | R |
| `src/thirdparty/libevent/test/regress_main.c` | 14211 | 549 | 1 | R |
| `src/thirdparty/libevent/test/regress_mbedtls.c` | 9934 | 345 | 1 | R |
| `src/thirdparty/libevent/test/regress_minheap.c` | 2988 | 100 | 1 | R |
| `src/thirdparty/libevent/test/regress_openssl.c` | 8334 | 332 | 1 | R |
| `src/thirdparty/libevent/test/regress_rpc.c` | 20957 | 953 | 1 | R |
| `src/thirdparty/libevent/test/regress_ssl.c` | 26591 | 840 | 1 | R |
| `src/thirdparty/libevent/test/regress_testutils.c` | 9199 | 337 | 1 | R |
| `src/thirdparty/libevent/test/regress_testutils.h` | 3210 | 82 | 1 | R |
| `src/thirdparty/libevent/test/regress_thread.c` | 15196 | 594 | 1 | R |
| `src/thirdparty/libevent/test/regress_thread.h` | 2532 | 57 | 1 | R |
| `src/thirdparty/libevent/test/regress_util.c` | 49212 | 1891 | 2 | R |
| `src/thirdparty/libevent/test/regress_watch.c` | 7917 | 244 | 1 | R |
| `src/thirdparty/libevent/test/regress_ws.c` | 9240 | 379 | 1 | R |
| `src/thirdparty/libevent/test/regress_ws.h` | 159 | 8 | 1 | R |
| `src/thirdparty/libevent/test/regress_zlib.c` | 8801 | 349 | 1 | R |
| `src/thirdparty/libevent/test/rpcgen_wrapper.sh` | 1331 | 44 | 1 | R |
| `src/thirdparty/libevent/test/test-changelist.c` | 5907 | 225 | 1 | R |
| `src/thirdparty/libevent/test/test-closed.c` | 3335 | 112 | 1 | R |
| `src/thirdparty/libevent/test/test-dumpevents.c` | 5698 | 180 | 1 | R |
| `src/thirdparty/libevent/test/test-eof.c` | 3105 | 118 | 1 | R |
| `src/thirdparty/libevent/test/test-fdleak.c` | 7012 | 253 | 1 | R |
| `src/thirdparty/libevent/test/test-init.c` | 2148 | 66 | 1 | R |
| `src/thirdparty/libevent/test/test-ratelim.c` | 19267 | 689 | 1 | R |
| `src/thirdparty/libevent/test/test-ratelim.sh` | 1669 | 89 | 1 | R |
| `src/thirdparty/libevent/test/test-time.c` | 3224 | 124 | 1 | R |
| `src/thirdparty/libevent/test/test-weof.c` | 3074 | 115 | 1 | R |
| `src/thirdparty/libevent/test/test.sh` | 3791 | 189 | 1 | R |
| `src/thirdparty/libevent/test/tinytest.c` | 16138 | 639 | 1 | R |
| `src/thirdparty/libevent/test/tinytest.h` | 4475 | 104 | 1 | R |
| `src/thirdparty/libevent/test/tinytest_demo.c` | 8480 | 293 | 1 | R |
| `src/thirdparty/libevent/test/tinytest_local.h` | 170 | 13 | 1 | R |
| `src/thirdparty/libevent/test/tinytest_macros.h` | 7862 | 214 | 1 | R |
| `src/thirdparty/libevent/time-internal.h` | 3134 | 102 | 1 | R |
| `src/thirdparty/libevent/util-internal.h` | 17888 | 564 | 1 | R |
| `src/thirdparty/libevent/watch.c` | 2886 | 83 | 1 | R |
| `src/thirdparty/libevent/wepoll.c` | 68318 | 2222 | 3 | R |
| `src/thirdparty/libevent/wepoll.h` | 3442 | 112 | 1 | R |
| `src/thirdparty/libevent/whatsnew-2.0.txt` | 26344 | 610 | 1 | R |
| `src/thirdparty/libevent/whatsnew-2.1.txt` | 33387 | 769 | 1 | R |
| `src/thirdparty/libevent/whatsnew-2.2.txt` | 14904 | 421 | 1 | R |
| `src/thirdparty/libevent/win32select.c` | 10626 | 390 | 1 | R |
| `src/thirdparty/libevent/ws.c` | 10888 | 459 | 1 | R |
| `src/thirdparty/libtelnet/.github/workflows/ci.yml` | 1081 | 50 | 1 | R |
| `src/thirdparty/libtelnet/.gitignore` | 17 | 2 | 1 | R |
| `src/thirdparty/libtelnet/AUTHORS` | 154 | 5 | 1 | R |
| `src/thirdparty/libtelnet/CMakeLists.txt` | 1467 | 60 | 1 | R |
| `src/thirdparty/libtelnet/COPYING` | 492 | 12 | 1 | R |
| `src/thirdparty/libtelnet/NEWS` | 919 | 51 | 1 | R |
| `src/thirdparty/libtelnet/README.md` | 26077 | 701 | 1 | R |
| `src/thirdparty/libtelnet/doc/CMakeLists.txt` | 22 | 2 | 1 | R |
| `src/thirdparty/libtelnet/doc/man/CMakeLists.txt` | 45 | 2 | 1 | R |
| `src/thirdparty/libtelnet/doc/man/man1/CMakeLists.txt` | 115 | 4 | 1 | R |
| `src/thirdparty/libtelnet/doc/man/man1/telnet-chatd.1` | 471 | 16 | 1 | R |
| `src/thirdparty/libtelnet/doc/man/man1/telnet-client.1` | 492 | 16 | 1 | R |
| `src/thirdparty/libtelnet/doc/man/man1/telnet-proxy.1` | 907 | 18 | 1 | R |
| `src/thirdparty/libtelnet/doc/man/man3/CMakeLists.txt` | 1265 | 32 | 1 | R |
| `src/thirdparty/libtelnet/doc/man/man3/Doxyfile.in` | 354 | 16 | 1 | R |
| `src/thirdparty/libtelnet/libtelnet.c` | 44331 | 1670 | 2 | R |
| `src/thirdparty/libtelnet/libtelnet.h` | 21611 | 692 | 1 | R |
| `src/thirdparty/libtelnet/libtelnet.pc.in` | 235 | 9 | 1 | R |
| `src/thirdparty/libtelnet/test/CMakeLists.txt` | 301 | 7 | 1 | R |
| `src/thirdparty/libtelnet/test/environ01.input` | 145 | 13 | 1 | R |
| `src/thirdparty/libtelnet/test/environ01.txt` | 78 | 3 | 1 | R |
| `src/thirdparty/libtelnet/test/environ02.input` | 75 | 7 | 1 | R |
| `src/thirdparty/libtelnet/test/environ02.txt` | 57 | 2 | 1 | R |
| `src/thirdparty/libtelnet/test/environ03.input` | 77 | 7 | 1 | R |
| `src/thirdparty/libtelnet/test/environ03.txt` | 38 | 2 | 1 | R |
| `src/thirdparty/libtelnet/test/mssp01.input` | 336 | 32 | 1 | R |
| `src/thirdparty/libtelnet/test/mssp01.txt` | 100 | 5 | 1 | R |
| `src/thirdparty/libtelnet/test/rfc1143.input` | 274 | 16 | 1 | R |
| `src/thirdparty/libtelnet/test/rfc1143.txt` | 26 | 3 | 1 | R |
| `src/thirdparty/libtelnet/test/simple01.input` | 69 | 3 | 1 | R |
| `src/thirdparty/libtelnet/test/simple01.txt` | 45 | 2 | 1 | R |
| `src/thirdparty/libtelnet/test/simple02.input` | 44 | 3 | 1 | R |
| `src/thirdparty/libtelnet/test/simple02.txt` | 13 | 2 | 1 | R |
| `src/thirdparty/libtelnet/test/ttype01.input` | 148 | 9 | 1 | R |
| `src/thirdparty/libtelnet/test/ttype01.txt` | 49 | 5 | 1 | R |
| `src/thirdparty/libtelnet/test/zmp01.input` | 55 | 3 | 1 | R |
| `src/thirdparty/libtelnet/test/zmp01.txt` | 19 | 2 | 1 | R |
| `src/thirdparty/libtelnet/test/zmp02.input` | 85 | 3 | 1 | R |
| `src/thirdparty/libtelnet/test/zmp02.txt` | 19 | 2 | 1 | R |
| `src/thirdparty/libtelnet/test/zmp03.input` | 68 | 3 | 1 | R |
| `src/thirdparty/libtelnet/test/zmp03.txt` | 30 | 2 | 1 | R |
| `src/thirdparty/libtelnet/util/CMakeLists.txt` | 563 | 25 | 1 | R |
| `src/thirdparty/libtelnet/util/telnet-chatd.c` | 9033 | 374 | 1 | R |
| `src/thirdparty/libtelnet/util/telnet-client.c` | 6314 | 260 | 1 | R |
| `src/thirdparty/libtelnet/util/telnet-proxy.c` | 13413 | 512 | 1 | R |
| `src/thirdparty/libtelnet/util/telnet-test.c` | 9032 | 367 | 1 | R |
| `src/thirdparty/libwebsockets/.github/codeql.yml` | 804 | 36 | 1 | R |
| `src/thirdparty/libwebsockets/.github/workflows/codeql.yml` | 966 | 44 | 1 | R |
| `src/thirdparty/libwebsockets/.gitignore` | 802 | 68 | 1 | R |
| `src/thirdparty/libwebsockets/.mailmap` | 103 | 4 | 1 | R |
| `src/thirdparty/libwebsockets/.sai.json` | 14472 | 257 | 1 | R |
| `src/thirdparty/libwebsockets/CMakeLists-implied-options.txt` | 11526 | 452 | 1 | R |
| `src/thirdparty/libwebsockets/CMakeLists.txt` | 50732 | 1274 | 2 | R / E:AUD-021 |
| `src/thirdparty/libwebsockets/Kconfig` | 857 | 33 | 1 | R |
| `src/thirdparty/libwebsockets/LICENSE` | 19205 | 312 | 1 | R |
| `src/thirdparty/libwebsockets/Makefile.projbuild` | 54 | 2 | 1 | R |
| `src/thirdparty/libwebsockets/README.md` | 9262 | 172 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.async-dns.md` | 4396 | 101 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.build-android.md` | 2434 | 78 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.build-windows.md` | 5828 | 181 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.build.md` | 28680 | 799 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.captive-portal-detection.md` | 3677 | 89 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.cbor-cose.md` | 9805 | 273 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.cbor-lecp.md` | 12957 | 345 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.ci.md` | 911 | 30 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.cmake.md` | 2069 | 42 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.coding.md` | 53212 | 1338 | 2 | R |
| `src/thirdparty/libwebsockets/READMEs/README.content-security-policy.md` | 5950 | 149 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.contributing.md` | 1144 | 42 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.crypto-apis.md` | 7884 | 182 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.ctest.md` | 15867 | 340 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.debugging.md` | 2310 | 64 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.display-list.md` | 5501 | 149 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.esp32.md` | 1917 | 38 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.event-libs.md` | 3325 | 81 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.event-loops-intro.md` | 13464 | 286 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.fault-injection.md` | 19941 | 359 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.h2-long-poll.md` | 1812 | 56 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.html-parser.md` | 16516 | 305 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.http-cache.md` | 1376 | 41 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.http-fallback.md` | 6663 | 173 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.http_parser.md` | 935 | 34 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.jit-trust.md` | 17627 | 447 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.jpeg-decoder.md` | 2992 | 78 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.json-lejp.md` | 5738 | 137 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.jwt.md` | 7305 | 177 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.libressl.md` | 2749 | 67 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lifecycle.md` | 1650 | 44 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.logging.md` | 12285 | 289 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_backtrace.md` | 7449 | 182 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_cache.md` | 5489 | 161 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_conmon.md` | 1467 | 37 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_dll.md` | 3823 | 131 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_fixed3232.md` | 2666 | 88 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_map.md` | 3712 | 118 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_metrics.md` | 11232 | 246 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_ota.md` | 13090 | 303 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_plugins.md` | 3368 | 106 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_retry.md` | 3913 | 99 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_struct.md` | 1554 | 39 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_sul.md` | 4865 | 111 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lws_system.md` | 8924 | 244 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.lwsws.md` | 24883 | 701 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.plugin-acme.md` | 6081 | 181 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.plugin-sshd-base.md` | 8128 | 250 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.png-decoder.md` | 2336 | 57 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.porting.md` | 2144 | 61 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.problems.md` | 3033 | 67 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.release-policy.md` | 5795 | 127 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.routing.md` | 2087 | 52 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.secure-streams.md` | 868 | 27 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.stdin.md` | 3163 | 87 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.tcp_fastopen.md` | 895 | 33 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.test-apps.md` | 14872 | 449 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.tls-sessions.md` | 5256 | 122 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.udp.md` | 1789 | 51 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/README.unix-domain-reverse-proxy.md` | 4558 | 102 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/mainpage.md` | 1110 | 28 | 1 | R |
| `src/thirdparty/libwebsockets/READMEs/release-checklist` | 999 | 61 | 1 | R |
| `src/thirdparty/libwebsockets/SECURITY.md` | 287 | 7 | 1 | R |
| `src/thirdparty/libwebsockets/bug_report.md` | 2120 | 39 | 1 | R |
| `src/thirdparty/libwebsockets/changelog` | 35363 | 781 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/FindGit.cmake` | 6519 | 164 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/FindMiniz.cmake` | 867 | 36 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/FindOpenSSLbins.cmake` | 3636 | 103 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/LwsCheckRequirements.cmake` | 3747 | 130 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/UseRPMTools.cmake` | 5873 | 177 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/libwebsockets-config-version.cmake.in` | 386 | 12 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/libwebsockets-config.cmake.in` | 1260 | 40 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/lws_config.h.in` | 9926 | 273 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/lws_config_private.h.in` | 3179 | 113 | 1 | R |
| `src/thirdparty/libwebsockets/cmake/pico_sdk_import.cmake` | 2807 | 65 | 1 | R |
| `src/thirdparty/libwebsockets/component.mk` | 1659 | 46 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets.h` | 21685 | 835 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets.hxx` | 3517 | 149 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-adopt.h` | 10659 | 280 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-async-dns.h` | 4883 | 131 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-backtrace.h` | 10611 | 281 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-bb-i2c.h` | 2119 | 67 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-bb-spi.h` | 2276 | 65 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-button.h` | 3988 | 121 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-cache-ttl.h` | 13342 | 349 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-callbacks.h` | 36981 | 921 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-cgi.h` | 4347 | 131 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-client.h` | 19879 | 504 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-conmon.h` | 5643 | 156 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-context-vhost.h` | 61504 | 1449 | 2 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-cose.h` | 15637 | 512 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-dbus.h` | 3492 | 95 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-diskcache.h` | 8570 | 188 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-display.h` | 7351 | 225 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-dll2.h` | 9975 | 327 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-dlo.h` | 14211 | 525 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-dsh.h` | 6469 | 170 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-esp32-spi.h` | 2051 | 53 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-eventlib-exports.h` | 5136 | 154 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-fault-injection.h` | 8933 | 252 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-freertos.h` | 2751 | 88 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-fts.h` | 7089 | 216 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-genaes.h` | 5577 | 171 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-gencrypto.h` | 4357 | 138 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-genec.h` | 7152 | 212 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-genhash.h` | 6344 | 200 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-genrsa.h` | 9205 | 256 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-gpio.h` | 2072 | 61 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-html.h` | 18904 | 718 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-http.h` | 36573 | 1030 | 2 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-i2c.h` | 2061 | 55 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-ili9341-spi.h` | 2114 | 55 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-jose.h` | 7214 | 216 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-jpeg.h` | 4340 | 105 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-jrpc.h` | 7986 | 230 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-jwe.h` | 5907 | 165 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-jwk.h` | 7932 | 221 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-jws.h` | 21790 | 602 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-lecp.h` | 15623 | 540 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-led.h` | 4964 | 147 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-lejp.h` | 9811 | 333 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-logs.h` | 24740 | 807 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-lwsac.h` | 11226 | 312 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-map.h` | 6664 | 189 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-metrics.h` | 11543 | 330 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-misc.h` | 55194 | 1597 | 2 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-mqtt.h` | 14273 | 387 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-netdev.h` | 8541 | 284 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-network-helper.h` | 8851 | 262 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-optee.h` | 2587 | 78 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-ota.h` | 4261 | 123 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-protocols-plugins.h` | 14480 | 384 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-purify.h` | 3728 | 106 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-pwm.h` | 2441 | 68 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-retry.h` | 4398 | 96 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-ring.h` | 11869 | 307 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams-client.h` | 11922 | 363 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams-policy.h` | 12832 | 393 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams-serialization.h` | 20367 | 600 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams-transport-proxy.h` | 2059 | 48 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams.h` | 27265 | 743 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-service.h` | 7679 | 203 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-settings.h` | 4448 | 113 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-sha1-base64.h` | 3613 | 114 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-smd.h` | 8345 | 228 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-spa.h` | 6833 | 178 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-spd1656-spi.h` | 2164 | 56 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-spi.h` | 3314 | 94 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-ssd1306-i2c.h` | 2387 | 65 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-ssd1675b-spi.h` | 2159 | 56 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-state.h` | 3946 | 120 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-struct.h` | 12857 | 554 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-system.h` | 17283 | 466 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-test-sequencer.h` | 2527 | 62 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-threadpool.h` | 10272 | 281 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-timeout-timer.h` | 11809 | 314 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-tls-sessions.h` | 3076 | 82 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-tokenize.h` | 11416 | 296 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-uc8176-spi.h` | 2152 | 56 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-upng.h` | 5978 | 161 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-vfs.h` | 9683 | 285 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-write.h` | 9733 | 260 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-writeable.h` | 8900 | 247 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-ws-close.h` | 6056 | 126 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-ws-ext.h` | 7849 | 199 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-ws-state.h` | 3710 | 110 | 1 | R |
| `src/thirdparty/libwebsockets/include/libwebsockets/lws-x509.h` | 11496 | 294 | 1 | R |
| `src/thirdparty/libwebsockets/lgtm.yml` | 76 | 4 | 1 | R |
| `src/thirdparty/libwebsockets/lib/CMakeLists.txt` | 15233 | 471 | 1 | R |
| `src/thirdparty/libwebsockets/lib/README.md` | 675 | 17 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/CMakeLists.txt` | 2230 | 86 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/README.md` | 2816 | 59 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/adopt.c` | 25701 | 970 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/client/client.c` | 3484 | 123 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/client/conmon.c` | 4391 | 156 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/client/connect.c` | 15880 | 577 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/client/connect2.c` | 10119 | 396 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/client/connect3.c` | 20947 | 808 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/client/connect4.c` | 9396 | 339 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/client/sort-dns.c` | 21029 | 775 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/close.c` | 28875 | 1075 | 2 | R |
| `src/thirdparty/libwebsockets/lib/core-net/dummy-callback.c` | 24254 | 895 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/lws-dsh.c` | 16596 | 647 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/network.c` | 27193 | 1163 | 2 | R |
| `src/thirdparty/libwebsockets/lib/core-net/output.c` | 9914 | 386 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/pollfd.c` | 17575 | 666 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/private-lib-core-net.h` | 44185 | 1714 | 2 | R |
| `src/thirdparty/libwebsockets/lib/core-net/route.c` | 10989 | 407 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/service.c` | 22024 | 881 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/socks5-client.c` | 9949 | 380 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/sorted-usec-list.c` | 9547 | 394 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/state.c` | 3971 | 154 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/transport-mux-client.c` | 8673 | 338 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/transport-mux-common.c` | 20633 | 814 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/transport-mux-proxy.c` | 10448 | 405 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/vhost.c` | 49581 | 1983 | 2 | R |
| `src/thirdparty/libwebsockets/lib/core-net/wol.c` | 3622 | 100 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/wsi-timeout.c` | 8887 | 296 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core-net/wsi.c` | 38628 | 1718 | 2 | R |
| `src/thirdparty/libwebsockets/lib/core/CMakeLists.txt` | 1842 | 64 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core/alloc.c` | 5210 | 234 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core/buflist.c` | 9897 | 453 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core/context.c` | 66631 | 2482 | 3 | R |
| `src/thirdparty/libwebsockets/lib/core/libwebsockets.c` | 44277 | 2191 | 3 | R |
| `src/thirdparty/libwebsockets/lib/core/logs.c` | 13665 | 625 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core/lws_dll2.c` | 7830 | 335 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core/lws_map.c` | 5835 | 270 | 1 | R |
| `src/thirdparty/libwebsockets/lib/core/private-lib-core.h` | 30915 | 1242 | 2 | R |
| `src/thirdparty/libwebsockets/lib/core/vfs.c` | 3924 | 159 | 1 | R |
| `src/thirdparty/libwebsockets/lib/cose/CMakeLists.txt` | 1437 | 40 | 1 | R |
| `src/thirdparty/libwebsockets/lib/cose/cose_key.c` | 28505 | 1189 | 2 | R |
| `src/thirdparty/libwebsockets/lib/cose/cose_sign.c` | 13693 | 544 | 1 | R |
| `src/thirdparty/libwebsockets/lib/cose/cose_sign_alg.c` | 6923 | 272 | 1 | R |
| `src/thirdparty/libwebsockets/lib/cose/cose_validate.c` | 24149 | 1052 | 2 | R |
| `src/thirdparty/libwebsockets/lib/cose/cose_validate_alg.c` | 6992 | 275 | 1 | R |
| `src/thirdparty/libwebsockets/lib/cose/private-lib-cose.h` | 3798 | 149 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/CMakeLists.txt` | 718 | 34 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/README.md` | 2192 | 45 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/button/README.md` | 6517 | 157 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/button/lws-button.c` | 15044 | 533 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/display/README.md` | 1823 | 37 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/display/ili9341-spi.c` | 10366 | 384 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/display/lws-display.c` | 12371 | 444 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/display/spd1656-spi.c` | 11972 | 464 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/display/ssd1306-i2c.c` | 7138 | 279 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/display/ssd1675b-spi.c` | 14084 | 496 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/display/uc8176-spi.c` | 32769 | 1018 | 2 | R |
| `src/thirdparty/libwebsockets/lib/drivers/i2c/bitbang/lws-bb-i2c.c` | 3391 | 136 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/i2c/lws-i2c.c` | 1821 | 60 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/led/README.md` | 4848 | 156 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/led/led-gpio.c` | 3419 | 121 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/led/led-seq.c` | 5975 | 201 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/led/private-lib-drivers-led.h` | 1476 | 40 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/netdev/netdev.c` | 7291 | 273 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/netdev/wifi.c` | 7034 | 244 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/pwm/pwm.c` | 4827 | 157 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/settings/settings.c` | 1922 | 70 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/spi/bitbang/lws-bb-spi.c` | 3719 | 131 | 1 | R |
| `src/thirdparty/libwebsockets/lib/drivers/spi/lws-spi.c` | 2627 | 94 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/CMakeLists.txt` | 5075 | 148 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/README.md` | 6200 | 170 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/glib/CMakeLists.txt` | 2457 | 78 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/glib/glib.c` | 12978 | 517 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/glib/private-lib-event-libs-glib.h` | 1886 | 62 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/libev/CMakeLists.txt` | 2971 | 89 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/libev/libev.c` | 12487 | 469 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/libev/private-lib-event-libs-libev.h` | 2163 | 63 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/libevent/CMakeLists.txt` | 2451 | 73 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/libevent/libevent.c` | 13451 | 521 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/libevent/private-lib-event-libs-libevent.h` | 1757 | 50 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/libuv/CMakeLists.txt` | 3079 | 87 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/libuv/libuv.c` | 23784 | 953 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/libuv/private-lib-event-libs-libuv.h` | 2923 | 91 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/poll/CMakeLists.txt` | 1533 | 43 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/poll/poll.c` | 2103 | 63 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/poll/private-lib-event-libs-poll.h` | 1277 | 26 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/private-lib-event-libs.h` | 1273 | 28 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/sdevent/CMakeLists.txt` | 1131 | 45 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/sdevent/private-lib-event-libs-sdevent.h` | 94 | 4 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/sdevent/sdevent.c` | 10324 | 447 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/uloop/CMakeLists.txt` | 2394 | 73 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/uloop/private-lib-event-libs-uloop.h` | 1506 | 38 | 1 | R |
| `src/thirdparty/libwebsockets/lib/event-libs/uloop/uloop.c` | 8361 | 327 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/CMakeLists.txt` | 1618 | 48 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/README.md` | 2097 | 80 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwe/enc/aescbc.c` | 8960 | 272 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwe/enc/aesgcm.c` | 5310 | 174 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwe/enc/aeskw.c` | 5592 | 178 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwe/jwe-ecdh-es-aeskw.c` | 20840 | 617 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwe/jwe-rsa-aescbc.c` | 6196 | 197 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwe/jwe-rsa-aesgcm.c` | 5446 | 184 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwe/jwe.c` | 22265 | 795 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwe/private-lib-jose-jwe.h` | 2544 | 89 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwk/jose_key.c` | 17343 | 650 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jwk/jwk.c` | 7018 | 298 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jws/jose.c` | 16204 | 612 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/jws/jws.c` | 31520 | 1315 | 2 | R |
| `src/thirdparty/libwebsockets/lib/jose/jws/private-lib-jose-jws.h` | 1380 | 28 | 1 | R |
| `src/thirdparty/libwebsockets/lib/jose/private-lib-jose.h` | 2115 | 54 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/CMakeLists.txt` | 4349 | 203 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/backtrace.c` | 9653 | 393 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/base64-decode.c` | 8008 | 314 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/cache-ttl/file.c` | 24417 | 1006 | 2 | R |
| `src/thirdparty/libwebsockets/lib/misc/cache-ttl/heap.c` | 15683 | 609 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/cache-ttl/lws-cache-ttl.c` | 7212 | 301 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/cache-ttl/private-lib-misc-cache-ttl.h` | 3375 | 105 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/css-lextable-strings.txt` | 1636 | 121 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/css-lextable.h` | 62067 | 1338 | 2 | R |
| `src/thirdparty/libwebsockets/lib/misc/css-prop-constants.txt` | 1087 | 125 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/css-propconst-lextable.h` | 51122 | 1111 | 2 | R |
| `src/thirdparty/libwebsockets/lib/misc/daemonize.c` | 5495 | 237 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dir.c` | 11801 | 540 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/diskcache.c` | 12225 | 491 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-font-mcufont.c` | 13053 | 529 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-jpeg.c` | 5400 | 196 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-lhp.c` | 25119 | 983 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-png.c` | 5196 | 188 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-rect.c` | 7291 | 248 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-ss.c` | 8961 | 353 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-text.c` | 10001 | 414 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dlo/dlo.c` | 20731 | 901 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/dlo/private-lib-drivers-display-dlo.h` | 1556 | 63 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/fsmount.c` | 4261 | 157 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/fts/README.md` | 11280 | 316 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/fts/private-lib-misc-fts.h` | 404 | 24 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/fts/trie-fd.c` | 21876 | 1004 | 2 | R |
| `src/thirdparty/libwebsockets/lib/misc/fts/trie.c` | 32372 | 1373 | 2 | R |
| `src/thirdparty/libwebsockets/lib/misc/getifaddrs.c` | 6855 | 277 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/getifaddrs.h` | 2583 | 81 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/ieeehalfprecision.c` | 6746 | 229 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/jpeg.c` | 55274 | 2757 | 3 | R |
| `src/thirdparty/libwebsockets/lib/misc/jrpc/jrpc.c` | 9157 | 387 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/jrpc/private-lib-misc-jrpc.h` | 3341 | 89 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/lecp.c` | 36344 | 1710 | 2 | R |
| `src/thirdparty/libwebsockets/lib/misc/lejp.c` | 26076 | 1013 | 2 | R |
| `src/thirdparty/libwebsockets/lib/misc/lhp-ss.c` | 6246 | 242 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/lhp.c` | 53943 | 2157 | 3 | R |
| `src/thirdparty/libwebsockets/lib/misc/lws-ring.c` | 7541 | 299 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/lws-struct-lejp.c` | 20041 | 898 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/lws-struct-sqlite.c` | 19096 | 774 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/lwsac/README.md` | 6464 | 149 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/lwsac/cached-file.c` | 5697 | 219 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/lwsac/lwsac.c` | 8362 | 395 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/lwsac/lwsac.cxx` | 2329 | 81 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/lwsac/private-lib-misc-lwsac.h` | 2223 | 71 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/minilex.c` | 5452 | 289 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/peer-limits.c` | 8317 | 319 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/prng.c` | 2154 | 81 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/romfs.c` | 5749 | 242 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/romfs.h` | 2142 | 64 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/sha-1.c` | 9297 | 297 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/threadpool/README.md` | 8287 | 183 | 1 | R |
| `src/thirdparty/libwebsockets/lib/misc/threadpool/threadpool.c` | 30070 | 1217 | 2 | R |
| `src/thirdparty/libwebsockets/lib/misc/upng-gzip.c` | 24534 | 1039 | 2 | R |
| `src/thirdparty/libwebsockets/lib/misc/upng.c` | 15822 | 727 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/CMakeLists.txt` | 2281 | 67 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/drivers/gpio-esp32.c` | 2728 | 97 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/drivers/lws-plat-gpio.h` | 1231 | 26 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/drivers/netdev/wifi-esp32.c` | 13217 | 499 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/drivers/pwm-esp32.c` | 2980 | 96 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/drivers/settings-esp32.c` | 2418 | 76 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/drivers/spi-esp32.c` | 7596 | 298 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/esp32-lws_ota.c` | 4847 | 202 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-fds.c` | 2056 | 62 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-file.c` | 4835 | 228 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-init.c` | 3504 | 120 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-misc.c` | 2601 | 105 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-pipe.c` | 4203 | 142 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-resolv.c` | 2110 | 63 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-service.c` | 5473 | 213 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-sockets.c` | 9580 | 399 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/freertos/private-lib-plat-freertos.h` | 4186 | 144 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/optee/CMakeLists.txt` | 1769 | 50 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/optee/lws-plat-optee.c` | 5512 | 262 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/optee/network.c` | 6989 | 329 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/optee/private-lib-plat-optee.h` | 2111 | 52 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/CMakeLists.txt` | 3252 | 116 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/android/android-resolv.c` | 2035 | 60 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/private-lib-plat-unix.h` | 5700 | 220 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-caps.c` | 5805 | 251 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-fds.c` | 5628 | 267 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-file.c` | 4184 | 186 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-init.c` | 7911 | 276 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-misc.c` | 3400 | 143 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-pipe.c` | 3083 | 108 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-plugins.c` | 3226 | 131 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-resolv.c` | 2745 | 93 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-service.c` | 8013 | 282 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-sockets.c` | 16925 | 694 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-spawn.c` | 23716 | 935 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/unix/unix-systemd.c` | 2891 | 86 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/CMakeLists.txt` | 3332 | 104 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/private-lib-plat-windows.h` | 4772 | 184 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-fds.c` | 2465 | 80 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-file.c` | 4624 | 198 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-init.c` | 4425 | 173 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-misc.c` | 2913 | 123 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-pipe.c` | 4186 | 141 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-plugins.c` | 4366 | 182 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-resolv.c` | 2838 | 105 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-service.c` | 5496 | 194 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-sockets.c` | 16489 | 671 | 1 | R |
| `src/thirdparty/libwebsockets/lib/plat/windows/windows-spawn.c` | 16938 | 653 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/CMakeLists.txt` | 2560 | 94 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/README.md` | 6123 | 171 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/cgi/CMakeLists.txt` | 1540 | 43 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/cgi/cgi-server.c` | 31388 | 1176 | 2 | R |
| `src/thirdparty/libwebsockets/lib/roles/cgi/ops-cgi.c` | 5348 | 183 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/cgi/private-lib-roles-cgi.h` | 2657 | 94 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/dbus/CMakeLists.txt` | 2355 | 69 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/dbus/README.md` | 2830 | 84 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/dbus/dbus.c` | 15373 | 574 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/dbus/private-lib-roles-dbus.h` | 1671 | 47 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/h1/CMakeLists.txt` | 1514 | 42 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/h1/ops-h1.c` | 32002 | 1185 | 2 | R |
| `src/thirdparty/libwebsockets/lib/roles/h1/private-lib-roles-h1.h` | 1514 | 32 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/h2/CMakeLists.txt` | 1551 | 45 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/h2/hpack.c` | 41999 | 1504 | 2 | R |
| `src/thirdparty/libwebsockets/lib/roles/h2/http2.c` | 83879 | 3085 | 4 | R |
| `src/thirdparty/libwebsockets/lib/roles/h2/huftable.h` | 29952 | 531 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/h2/minihuf.c` | 11854 | 517 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/h2/ops-h2.c` | 42575 | 1528 | 2 | R |
| `src/thirdparty/libwebsockets/lib/roles/h2/private-lib-roles-h2.h` | 11440 | 390 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/CMakeLists.txt` | 2793 | 97 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/client/client-http.c` | 63270 | 2428 | 3 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/compression/README.md` | 798 | 18 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/compression/brotli/brotli.c` | 3385 | 125 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/compression/deflate/deflate.c` | 3155 | 114 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/compression/private-lib-roles-http-compression.h` | 2961 | 88 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/compression/stream.c` | 6687 | 228 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/cookie.c` | 15256 | 745 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/date.c` | 5319 | 226 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/header.c` | 17153 | 682 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/lextable-strings.h` | 3242 | 137 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/lextable.h` | 340443 | 6897 | 7 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/minilex.c` | 10631 | 505 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/parsers.c` | 41239 | 1726 | 2 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/private-lib-roles-http.h` | 9062 | 358 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/server/access-log.c` | 5914 | 210 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/server/fops-zip.c` | 17414 | 658 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/server/lejp-conf.c` | 31371 | 1241 | 2 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/server/lws-spa.c` | 14880 | 726 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/server/ranges.c` | 4960 | 215 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/server/rewrite.c` | 1022 | 56 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/http/server/server.c` | 91451 | 3561 | 4 | R |
| `src/thirdparty/libwebsockets/lib/roles/listen/CMakeLists.txt` | 1523 | 43 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/listen/ops-listen.c` | 6864 | 222 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/mqtt/CMakeLists.txt` | 1717 | 49 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/mqtt/client/client-mqtt-handshake.c` | 6063 | 208 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/mqtt/client/client-mqtt.c` | 10749 | 400 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/mqtt/mqtt.c` | 67691 | 2521 | 3 | R |
| `src/thirdparty/libwebsockets/lib/roles/mqtt/ops-mqtt.c` | 17784 | 654 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/mqtt/primitives.c` | 7181 | 324 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/mqtt/private-lib-roles-mqtt.h` | 12133 | 450 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/netlink/ops-netlink.c` | 20539 | 719 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/pipe/ops-pipe.c` | 5107 | 164 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/private-lib-roles.h` | 15018 | 440 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/raw-file/CMakeLists.txt` | 1524 | 40 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/raw-file/ops-raw-file.c` | 4418 | 128 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/raw-proxy/CMakeLists.txt` | 1530 | 43 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/raw-proxy/ops-raw-proxy.c` | 7468 | 247 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/raw-proxy/private-lib-roles-raw-proxy.h` | 1664 | 45 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/raw-skt/CMakeLists.txt` | 1610 | 47 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/raw-skt/ops-raw-skt.c` | 10611 | 399 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/ws/CMakeLists.txt` | 1917 | 62 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/ws/client-parser-ws.c` | 22492 | 833 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/ws/client-ws.c` | 16736 | 662 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/ws/ext/extension-permessage-deflate.c` | 16053 | 567 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/ws/ext/extension-permessage-deflate.h` | 2128 | 70 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/ws/ext/extension.c` | 8776 | 383 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/ws/ops-ws.c` | 61635 | 2277 | 3 | R |
| `src/thirdparty/libwebsockets/lib/roles/ws/private-lib-roles-ws.h` | 6140 | 210 | 1 | R |
| `src/thirdparty/libwebsockets/lib/roles/ws/server-ws.c` | 28675 | 1113 | 2 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/CMakeLists.txt` | 4396 | 155 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/README.md` | 32380 | 925 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/cpp/README.md` | 937 | 30 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/cpp/lss.cxx` | 3840 | 155 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/cpp/lssFile.cxx` | 3471 | 133 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/cpp/lssMsg.cxx` | 1946 | 61 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/plugins/ssp-h1url/h1url.c` | 813 | 41 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/policy-common.c` | 14977 | 604 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/policy-json.c` | 33627 | 1324 | 2 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/private-lib-secure-streams.h` | 18605 | 744 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/protocols/README.md` | 1377 | 39 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-h1.c` | 34353 | 1299 | 2 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-h2.c` | 6223 | 229 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-mqtt.c` | 31260 | 1131 | 2 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-raw.c` | 5719 | 203 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-ws.c` | 8530 | 286 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/secure-streams.c` | 53687 | 2198 | 3 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/CMakeLists.txt` | 575 | 28 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/README.md` | 4342 | 90 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/sspc-deserialize.c` | 24015 | 1026 | 2 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/sspc-transport-wsi.c` | 7538 | 289 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/sspc-transport.c` | 12497 | 516 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/sspc.c` | 20749 | 852 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/proxy/proxy-deserialize.c` | 20138 | 859 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/proxy/proxy-transport-wsi.c` | 7229 | 293 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/proxy/proxy-transport.c` | 10450 | 422 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/serialized/proxy/proxy.c` | 13528 | 493 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/system/auth-api.amazon.com/auth.c` | 7954 | 290 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/system/auth-sigv4/sign.c` | 14117 | 575 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/system/captive-portal-detect/captive-portal-detect.c` | 2927 | 99 | 1 | R |
| `src/thirdparty/libwebsockets/lib/secure-streams/system/fetch-policy/fetch-policy.c` | 4655 | 171 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/CMakeLists.txt` | 2414 | 87 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/README.md` | 2410 | 69 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/async-dns/async-dns-parse.c` | 18546 | 743 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/async-dns/async-dns.c` | 29404 | 1224 | 2 | R |
| `src/thirdparty/libwebsockets/lib/system/async-dns/private-lib-async-dns.h` | 4562 | 165 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/dhcpclient/dhcpc4.c` | 14833 | 536 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/dhcpclient/dhcpclient.c` | 3989 | 157 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/dhcpclient/private-lib-system-dhcpclient.h` | 3478 | 113 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/fault-injection/fault-injection.c` | 9843 | 448 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/fault-injection/private-lib-system-fault-injection.h` | 1385 | 36 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/metrics/CMakeLists.txt` | 137 | 11 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/metrics/metrics.c` | 22177 | 895 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/metrics/private-lib-system-metrics.h` | 4287 | 125 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/ntpclient/ntpclient.c` | 8969 | 311 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/ota/ota.c` | 19366 | 736 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/smd/CMakeLists.txt` | 92 | 9 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/smd/README.md` | 11256 | 283 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/smd/private-lib-system-smd.h` | 2752 | 96 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/smd/smd.c` | 21748 | 833 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/stdin.c` | 4699 | 176 | 1 | R |
| `src/thirdparty/libwebsockets/lib/system/system.c` | 5790 | 266 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/CMakeLists.txt` | 24826 | 595 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/lws-gencrypto-common.c` | 22038 | 696 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/lws-genec-common.c` | 3635 | 135 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/CMakeLists.txt` | 5098 | 136 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/lws-genaes.c` | 11730 | 448 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/lws-gencrypto.c` | 2095 | 68 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/lws-genec.c` | 14407 | 536 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/lws-genhash.c` | 8067 | 323 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/lws-genrsa.c` | 13584 | 565 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-client.c` | 15390 | 563 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-extensions.c` | 16526 | 515 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-server.c` | 19154 | 714 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-session.c` | 9787 | 373 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-ssl.c` | 9317 | 357 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-tls.c` | 1732 | 51 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-x509.c` | 15032 | 543 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/private-lib-tls-mbedtls.h` | 2299 | 66 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl3.h` | 1447 | 45 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_cert.h` | 1293 | 56 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_code.h` | 3543 | 126 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_dbg.h` | 7218 | 191 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_lib.h` | 802 | 31 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_methods.h` | 3312 | 126 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_pkey.h` | 2122 | 87 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_stack.h` | 938 | 53 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_types.h` | 6134 | 326 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_x509.h` | 2596 | 112 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/tls1.h` | 2089 | 59 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/x509_vfy.h` | 5960 | 112 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/openssl/ssl.h` | 38569 | 1868 | 2 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/platform/ssl_pm.h` | 1767 | 64 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/platform/ssl_port.h` | 1145 | 47 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_cert.c` | 1966 | 90 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_lib.c` | 23552 | 1315 | 2 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_methods.c` | 2752 | 91 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_pkey.c` | 5053 | 242 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_stack.c` | 1798 | 77 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_x509.c` | 6479 | 350 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/platform/ssl_pm.c` | 28794 | 1089 | 2 | R |
| `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/platform/ssl_port.c` | 955 | 30 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/lws-genaes.c` | 9843 | 414 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/lws-gencrypto.c` | 2397 | 90 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/lws-genec.c` | 17544 | 721 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/lws-genhash.c` | 5923 | 271 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/lws-genrsa.c` | 9755 | 414 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-client.c` | 30002 | 1098 | 2 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-server.c` | 28027 | 1091 | 2 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-session.c` | 12513 | 492 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-ssl.c` | 15656 | 606 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-tls.c` | 4884 | 187 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-x509.c` | 20718 | 868 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/openssl/private-lib-tls-openssl.h` | 3382 | 100 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/private-jit-trust.h` | 4835 | 150 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/private-lib-tls.h` | 7283 | 246 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/private-network.h` | 6205 | 209 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/tls-client.c` | 6935 | 239 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/tls-jit-trust.c` | 18484 | 715 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/tls-network.c` | 7355 | 280 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/tls-server.c` | 10739 | 375 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/tls-sessions.c` | 2022 | 66 | 1 | R |
| `src/thirdparty/libwebsockets/lib/tls/tls.c` | 14230 | 596 | 1 | R |
| `src/thirdparty/libwebsockets/libwebsockets.dox` | 115067 | 2614 | 3 | R |
| `src/thirdparty/libwebsockets/lwsws/CMakeLists.txt` | 2265 | 68 | 1 | R |
| `src/thirdparty/libwebsockets/lwsws/etc-logrotate.d-lwsws` | 194 | 11 | 1 | R |
| `src/thirdparty/libwebsockets/lwsws/etc-lwsws-conf-EXAMPLE` | 297 | 17 | 1 | R |
| `src/thirdparty/libwebsockets/lwsws/etc-lwsws-conf.d-localhost-EXAMPLE` | 1644 | 60 | 1 | R |
| `src/thirdparty/libwebsockets/lwsws/main.c` | 8202 | 368 | 1 | R |
| `src/thirdparty/libwebsockets/lwsws/usr-lib-systemd-system-lwsws.service` | 216 | 13 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/CMakeLists.txt` | 7952 | 251 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/acme-client/protocol_lws_acme_client.c` | 39856 | 1638 | 2 | R |
| `src/thirdparty/libwebsockets/plugins/deaddrop/README.md` | 2918 | 92 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/deaddrop/assets/BungeeTint-Regular.ttf` | 217004 | — | — | M |
| `src/thirdparty/libwebsockets/plugins/deaddrop/assets/deaddrop-title.png` | 14529 | — | — | M |
| `src/thirdparty/libwebsockets/plugins/deaddrop/assets/deaddrop.css` | 7544 | 146 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/deaddrop/assets/deaddrop.js` | 11504 | 446 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/deaddrop/assets/drop.svg` | 19663 | 103 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/deaddrop/assets/index.html` | 1620 | 59 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/deaddrop/protocol_lws_deaddrop.c` | 23355 | 970 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/lwsws-logo.png` | 5172 | — | — | M |
| `src/thirdparty/libwebsockets/plugins/protocol_client_loopback_test.c` | 4878 | 194 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/protocol_dumb_increment.c` | 3650 | 143 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/protocol_fulltext_demo.c` | 6886 | 288 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/protocol_lws_mirror.c` | 14066 | 506 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/protocol_lws_openmetrics_export.c` | 31580 | 1200 | 2 | R |
| `src/thirdparty/libwebsockets/plugins/protocol_lws_raw_test.c` | 7816 | 305 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/protocol_lws_sshd_demo.c` | 11604 | 481 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/protocol_lws_status.c` | 6742 | 271 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/protocol_post_demo.c` | 7550 | 324 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/protocol_urlarg.c` | 4152 | 149 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/raw-proxy/README.md` | 1809 | 67 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/raw-proxy/protocol_lws_raw_proxy.c` | 14937 | 587 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/server-status.css` | 4356 | 198 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/chacha.c` | 9889 | 369 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/ed25519.c` | 4245 | 222 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/fe25519.c` | 8355 | 339 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/fe25519.h` | 2332 | 69 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/ge25519.c` | 11431 | 322 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/ge25519.h` | 1384 | 44 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/ge25519_base.data` | 168560 | 267 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/poly1305.c` | 4718 | 173 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/sc25519.c` | 7908 | 308 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/sc25519.h` | 2856 | 79 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/smult_curve25519_ref.c` | 6878 | 266 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/include/lws-plugin-ssh.h` | 14468 | 374 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/include/lws-plugin-sshd-static-build-includes.h` | 562 | 19 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/include/lws-ssh.h` | 16077 | 617 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/kex-25519.c` | 14701 | 552 | 1 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/sshd.c` | 64985 | 2630 | 3 | R |
| `src/thirdparty/libwebsockets/plugins/ssh-base/telnet.c` | 6524 | 267 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/ahrefs-topsites.sh` | 220 | 7 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/attack.sh` | 21903 | 816 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/autobahn-test-client.sh` | 2487 | 102 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/autobahn-test-server.sh` | 2293 | 93 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/build-gcov.sh` | 143 | 7 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/client-ca/certindex.txt` | 0 | 1 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/client-ca/create-ca.sh` | 173 | 7 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/client-ca/create-client-cert.sh` | 458 | 23 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/client-ca/create-server-cert.sh` | 403 | 22 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/client-ca/serial` | 7 | 2 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/client-ca/tmp.cnf` | 2065 | 75 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/ctest-background-kill.sh` | 1116 | 68 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/ctest-background.sh` | 1030 | 42 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/dox-extra.css` | 60 | 6 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/esp32.mk` | 5160 | 132 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/gcov.sh` | 86 | 4 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/h2load-smp.sh` | 943 | 64 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/h2load.sh` | 936 | 64 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/h2spec.sh` | 760 | 42 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/mozilla-trust-gen.sh` | 6340 | 195 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/test-dbus-proxy.sh` | 466 | 35 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/travis_control.sh` | 1829 | 55 | 1 | R |
| `src/thirdparty/libwebsockets/scripts/travis_install.sh` | 1892 | 80 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/dirent/dirent-win32.h` | 26801 | 1161 | 2 | R |
| `src/thirdparty/libwebsockets/win32port/libwebsockets.nsi` | 4939 | 119 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/version.rc.in` | 1419 | 36 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/win32helpers/getopt.c` | 4442 | 154 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/win32helpers/getopt.h` | 670 | 34 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/win32helpers/getopt_long.c` | 6611 | 241 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/win32helpers/gettimeofday.c` | 775 | 39 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/win32helpers/gettimeofday.h` | 570 | 30 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/ZLib.vcxproj` | 18375 | 374 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/ZLib.vcxproj.filters` | 2903 | 95 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/adler32.c` | 5054 | 170 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/compress.c` | 2526 | 81 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/crc32.c` | 13663 | 441 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/crc32.h` | 30568 | 442 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/deflate.c` | 68351 | 1847 | 2 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/deflate.h` | 12678 | 343 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/gzguts.h` | 4697 | 133 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/gzlib.c` | 14071 | 538 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/gzread.c` | 20574 | 654 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/gzwrite.c` | 14602 | 532 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/infback.c` | 22622 | 633 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/inffast.c` | 13439 | 341 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/inffast.h` | 427 | 12 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/inffixed.h` | 6343 | 95 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/inflate.c` | 52741 | 1483 | 2 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/inflate.h` | 6399 | 123 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/inftrees.c` | 13745 | 329 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/inftrees.h` | 2928 | 63 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/trees.c` | 45248 | 1245 | 2 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/trees.h` | 8472 | 129 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/uncompr.c` | 2000 | 60 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/zconf.h` | 13423 | 430 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/zlib.h` | 79652 | 995 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/zutil.c` | 7319 | 319 | 1 | R |
| `src/thirdparty/libwebsockets/win32port/zlib/zutil.h` | 7179 | 275 | 1 | R |
| `src/thirdparty/scope_guard/scope_guard.hpp` | 13837 | 370 | 1 | R |
| `src/thirdparty/utf8_decoder_dfa/LICENSE` | 1087 | 8 | 1 | R |
| `src/thirdparty/utf8_decoder_dfa/decoder.h` | 1838 | 49 | 1 | R |
| `src/thirdparty/utfcpp/.circleci/config.yml` | 301 | 14 | 1 | R |
| `src/thirdparty/utfcpp/.gitignore` | 49 | 4 | 1 | R |
| `src/thirdparty/utfcpp/.gitmodules` | 88 | 4 | 1 | R |
| `src/thirdparty/utfcpp/CMakeLists.txt` | 1291 | 43 | 1 | R |
| `src/thirdparty/utfcpp/LICENSE` | 1338 | 24 | 1 | R |
| `src/thirdparty/utfcpp/README.md` | 55726 | 1504 | 2 | R |
| `src/thirdparty/utfcpp/samples/docsample.cpp` | 2123 | 65 | 1 | R |
| `src/thirdparty/utfcpp/source/utf8.h` | 1521 | 35 | 1 | R |
| `src/thirdparty/utfcpp/source/utf8/checked.h` | 12474 | 336 | 1 | R |
| `src/thirdparty/utfcpp/source/utf8/core.h` | 10928 | 339 | 1 | R |
| `src/thirdparty/utfcpp/source/utf8/cpp11.h` | 3306 | 104 | 1 | R |
| `src/thirdparty/utfcpp/source/utf8/cpp17.h` | 3298 | 104 | 1 | R |
| `src/thirdparty/utfcpp/source/utf8/unchecked.h` | 10989 | 275 | 1 | R |
| `src/thirdparty/utfcpp/tests/CMakeLists.txt` | 1734 | 48 | 1 | R |
| `src/thirdparty/utfcpp/tests/docker/Dockerfile` | 125 | 6 | 1 | R |
| `src/thirdparty/utfcpp/tests/negative.cpp` | 2143 | 60 | 1 | R |
| `src/thirdparty/utfcpp/tests/test_checked_api.cpp` | 5578 | 190 | 1 | R |
| `src/thirdparty/utfcpp/tests/test_checked_iterator.cpp` | 1032 | 33 | 1 | R |
| `src/thirdparty/utfcpp/tests/test_cpp11.cpp` | 3036 | 110 | 1 | R |
| `src/thirdparty/utfcpp/tests/test_cpp17.cpp` | 2611 | 89 | 1 | R |
| `src/thirdparty/utfcpp/tests/test_data/utf8_invalid.txt` | 20010 | — | — | M |
| `src/thirdparty/utfcpp/tests/test_unchecked_api.cpp` | 4626 | 162 | 1 | R |
| `src/thirdparty/utfcpp/tests/test_unchecked_iterator.cpp` | 1002 | 35 | 1 | R |
| `src/thirdparty/widecharwidth/.clang-format` | 389 | 17 | 1 | R |
| `src/thirdparty/widecharwidth/.gitignore` | 115 | 10 | 1 | R |
| `src/thirdparty/widecharwidth/.pylintrc` | 16680 | 553 | 1 | R |
| `src/thirdparty/widecharwidth/LICENSE` | 445 | 5 | 1 | R |
| `src/thirdparty/widecharwidth/Makefile` | 391 | 16 | 1 | R |
| `src/thirdparty/widecharwidth/README.md` | 1322 | 28 | 1 | R |
| `src/thirdparty/widecharwidth/generate.py` | 17094 | 570 | 1 | R |
| `src/thirdparty/widecharwidth/test.cpp` | 7691 | 333 | 1 | R |
| `src/thirdparty/widecharwidth/widechar_width.h` | 36140 | 1522 | 2 | R |
| `src/thirdparty/widecharwidth/widechar_width.js` | 34798 | 1429 | 2 | R |
| `src/tools/CMakeLists.txt` | 524 | 15 | 1 | R |
| `src/tools/build_applies.cc` | 3265 | 108 | 1 | R |
| `src/tools/build_packages_genfiles.sh` | 2593 | 89 | 1 | R |
| `src/tools/make_func.autogen.cc` | 60122 | 2008 | 3 | R |
| `src/tools/make_func.y` | 14733 | 622 | 1 | R |
| `src/tools/make_options_defs.cc` | 1554 | 61 | 1 | R |
| `src/tools/preprocessor.hpp` | 31757 | 1331 | 2 | R |
| `src/user.cc` | 1330 | 54 | 1 | R |
| `src/user.h` | 415 | 26 | 1 | R |
| `src/valgrind.supp` | 59 | 6 | 1 | R |
| `src/vm/README` | 125 | 4 | 1 | R |
| `src/vm/context.h` | 7366 | 223 | 1 | R |
| `src/vm/frozen_value.h` | 1098 | 33 | 1 | R |
| `src/vm/internal/applies` | 1440 | 85 | 1 | R |
| `src/vm/internal/apply.cc` | 18947 | 555 | 1 | R |
| `src/vm/internal/apply.h` | 1984 | 67 | 1 | R |
| `src/vm/internal/base/apply_cache.cc` | 8747 | 274 | 1 | R |
| `src/vm/internal/base/apply_cache.h` | 753 | 18 | 1 | R |
| `src/vm/internal/base/array.cc` | 54653 | 2169 | 3 | R |
| `src/vm/internal/base/array.h` | 2550 | 75 | 1 | R |
| `src/vm/internal/base/buffer.cc` | 2670 | 108 | 1 | R |
| `src/vm/internal/base/buffer.h` | 634 | 31 | 1 | R |
| `src/vm/internal/base/class.cc` | 2154 | 93 | 1 | R |
| `src/vm/internal/base/class.h` | 502 | 25 | 1 | R |
| `src/vm/internal/base/debug.cc` | 3906 | 126 | 1 | R |
| `src/vm/internal/base/debug.h` | 22 | 2 | 1 | R |
| `src/vm/internal/base/function.cc` | 14372 | 478 | 1 | R |
| `src/vm/internal/base/function.h` | 3179 | 104 | 1 | R |
| `src/vm/internal/base/interpret.cc` | 175316 | 5819 | 6 | R / E:AUD-006, AUD-019 |
| `src/vm/internal/base/interpret.h` | 9533 | 280 | 1 | R |
| `src/vm/internal/base/machine.h` | 2409 | 85 | 1 | R |
| `src/vm/internal/base/mapping.cc` | 37772 | 1354 | 2 | R |
| `src/vm/internal/base/mapping.h` | 3798 | 121 | 1 | R |
| `src/vm/internal/base/number.h` | 648 | 27 | 1 | R |
| `src/vm/internal/base/object.cc` | 64541 | 2405 | 3 | R |
| `src/vm/internal/base/object.h` | 9391 | 239 | 1 | R |
| `src/vm/internal/base/old_qsort_inc.h` | 1558 | 57 | 1 | R |
| `src/vm/internal/base/program.cc` | 11986 | 331 | 1 | R |
| `src/vm/internal/base/program.h` | 12300 | 303 | 1 | R |
| `src/vm/internal/base/promise.cc` | 94728 | 2146 | 3 | R / E:AUD-016 |
| `src/vm/internal/base/promise.h` | 12715 | 256 | 1 | R |
| `src/vm/internal/base/scoped_current_object_as_master.h` | 2221 | 58 | 1 | R |
| `src/vm/internal/base/svalue.cc` | 11299 | 386 | 1 | R |
| `src/vm/internal/base/svalue.h` | 13774 | 295 | 1 | R |
| `src/vm/internal/context.cc` | 12799 | 375 | 1 | R |
| `src/vm/internal/eval_limit.cc` | 525 | 31 | 1 | R / E:AUD-006 |
| `src/vm/internal/eval_limit.h` | 750 | 28 | 1 | R |
| `src/vm/internal/frozen_value.cc` | 8738 | 324 | 1 | R |
| `src/vm/internal/layout_digest.h` | 2108 | 54 | 1 | R |
| `src/vm/internal/lpc_vm_profile.cc` | 8396 | 240 | 1 | R |
| `src/vm/internal/lpc_vm_profile.h` | 3203 | 74 | 1 | R |
| `src/vm/internal/master.cc` | 7059 | 199 | 1 | R |
| `src/vm/internal/master.h` | 455 | 19 | 1 | R |
| `src/vm/internal/object_store.cc` | 129540 | 2724 | 3 | R |
| `src/vm/internal/otable.cc` | 4177 | 132 | 1 | R |
| `src/vm/internal/otable.h` | 827 | 41 | 1 | R |
| `src/vm/internal/otable_test.cc` | 1736 | 45 | 1 | R |
| `src/vm/internal/owner.cc` | 328821 | 1948 | 2 | R / E:AUD-008, AUD-022 |
| `src/vm/internal/owner_executor.cc` | 1140 | 34 | 1 | R |
| `src/vm/internal/owner_executor.h` | 876 | 29 | 1 | R |
| `src/vm/internal/owner_future_store.cc` | 27943 | 729 | 1 | R |
| `src/vm/internal/owner_future_store.h` | 9938 | 215 | 1 | R |
| `src/vm/internal/owner_runtime_coordinator.cc` | 6820 | 208 | 1 | R |
| `src/vm/internal/owner_runtime_coordinator.h` | 4946 | 130 | 1 | R |
| `src/vm/internal/owner_runtime_metrics.cc` | 392 | 11 | 1 | R |
| `src/vm/internal/owner_runtime_metrics.h` | 7603 | 133 | 1 | R |
| `src/vm/internal/owner_scheduler_state.cc` | 14462 | 445 | 1 | R |
| `src/vm/internal/owner_scheduler_state.h` | 7270 | 194 | 1 | R |
| `src/vm/internal/owner_service_registry.cc` | 5840 | 154 | 1 | R |
| `src/vm/internal/owner_service_registry.h` | 1216 | 37 | 1 | R |
| `src/vm/internal/owner_task_manifest.cc` | 13503 | 264 | 1 | R |
| `src/vm/internal/owner_task_manifest.h` | 2555 | 85 | 1 | R |
| `src/vm/internal/owner_trace_store.cc` | 7924 | 199 | 1 | R |
| `src/vm/internal/owner_trace_store.h` | 5760 | 180 | 1 | R |
| `src/vm/internal/posix_timers.cc` | 3529 | 140 | 1 | R |
| `src/vm/internal/posix_timers.h` | 186 | 11 | 1 | R |
| `src/vm/internal/recompile.cc` | 36011 | 914 | 1 | R |
| `src/vm/internal/recompile.h` | 13819 | 307 | 1 | R |
| `src/vm/internal/recompile_layout.cc` | 16190 | 435 | 1 | R |
| `src/vm/internal/recompile_layout.h` | 5136 | 128 | 1 | R |
| `src/vm/internal/simul_efun.cc` | 19899 | 535 | 1 | R |
| `src/vm/internal/simul_efun.h` | 4889 | 106 | 1 | R |
| `src/vm/internal/simulate.cc` | 75325 | 2566 | 3 | R |
| `src/vm/internal/simulate.h` | 2271 | 85 | 1 | R |
| `src/vm/internal/source_spelling.cc` | 2076 | 63 | 1 | R |
| `src/vm/internal/source_spelling.h` | 3027 | 61 | 1 | R |
| `src/vm/internal/trace.cc` | 10962 | 387 | 1 | R |
| `src/vm/internal/trace.h` | 280 | 9 | 1 | R |
| `src/vm/internal/vm.cc` | 6758 | 217 | 1 | R |
| `src/vm/internal/worker.cc` | 42141 | 1101 | 2 | R |
| `src/vm/object_handle.h` | 8304 | 192 | 1 | R |
| `src/vm/owner.h` | 11391 | 253 | 1 | R |
| `src/vm/vm.h` | 1328 | 59 | 1 | R |
| `src/vm/worker.h` | 5071 | 174 | 1 | R |
| `src/www/index.html` | 51400 | 1410 | 2 | R |
| `testsuite/.gitattributes` | 85 | 6 | 1 | R |
| `testsuite/.gitignore` | 274 | 23 | 1 | R |
| `testsuite/.vim/coc-settings.json` | 254 | 10 | 1 | R |
| `testsuite/2000.txt` | 72548 | 2019 | 3 | R |
| `testsuite/README` | 1841 | 30 | 1 | R |
| `testsuite/adm/daemons/gateway_d.c` | 655 | 23 | 1 | R |
| `testsuite/clone/README` | 0 | 1 | 1 | R |
| `testsuite/clone/a2_void_dots.lpc` | 196 | 5 | 1 | R |
| `testsuite/clone/async_info_names.lpc` | 231 | 15 | 1 | R |
| `testsuite/clone/async_inherit_base.lpc` | 139 | 6 | 1 | R |
| `testsuite/clone/async_inherit_park.lpc` | 168 | 9 | 1 | R |
| `testsuite/clone/async_inherit_park_child.lpc` | 216 | 11 | 1 | R |
| `testsuite/clone/bad_macro_params.lpc` | 41 | 3 | 1 | R |
| `testsuite/clone/clean_up_deadline.lpc` | 417 | 13 | 1 | R |
| `testsuite/clone/completion_duplicate.lpc` | 176 | 9 | 1 | R |
| `testsuite/clone/completion_late_failure.lpc` | 251 | 14 | 1 | R |
| `testsuite/clone/completion_never.lpc` | 119 | 6 | 1 | R |
| `testsuite/clone/completion_unknown.lpc` | 74 | 5 | 1 | R |
| `testsuite/clone/compress_file_io.lpc` | 187 | 8 | 1 | R |
| `testsuite/clone/defarg_child.c` | 273 | 7 | 1 | R |
| `testsuite/clone/defarg_fp.c` | 238 | 6 | 1 | R |
| `testsuite/clone/defarg_parent.c` | 93 | 3 | 1 | R |
| `testsuite/clone/dual_fail_only_lpc.lpc` | 331 | 6 | 1 | R |
| `testsuite/clone/dual_inherit_c_spelling.lpc` | 355 | 7 | 1 | R |
| `testsuite/clone/dual_only_c.c` | 169 | 4 | 1 | R |
| `testsuite/clone/dual_only_lpc.lpc` | 137 | 4 | 1 | R |
| `testsuite/clone/dual_pick_c.c` | 170 | 4 | 1 | R |
| `testsuite/clone/dual_pick_c.lpc` | 170 | 4 | 1 | R |
| `testsuite/clone/dual_pick_default.c` | 176 | 4 | 1 | R |
| `testsuite/clone/dual_pick_default.lpc` | 176 | 4 | 1 | R |
| `testsuite/clone/dual_pick_lpc.c` | 172 | 4 | 1 | R |
| `testsuite/clone/dual_pick_lpc.lpc` | 172 | 4 | 1 | R |
| `testsuite/clone/dual_retry_child.c` | 347 | 7 | 1 | R |
| `testsuite/clone/dual_retry_child.lpc` | 146 | 4 | 1 | R |
| `testsuite/clone/dual_retry_parent.lpc` | 165 | 4 | 1 | R |
| `testsuite/clone/dwlib_long_name.lpc` | 385 | 15 | 1 | R |
| `testsuite/clone/file_io_editor.lpc` | 198 | 13 | 1 | R |
| `testsuite/clone/file_io_state.lpc` | 814 | 46 | 1 | R |
| `testsuite/clone/gateway_exec_user.c` | 962 | 36 | 1 | R |
| `testsuite/clone/gateway_login_example.c` | 13575 | 405 | 1 | R |
| `testsuite/clone/gateway_login_exec_example.c` | 376 | 15 | 1 | R |
| `testsuite/clone/gateway_net_dead_destruct_user.c` | 666 | 25 | 1 | R |
| `testsuite/clone/include_list_a.h` | 92 | 4 | 1 | R |
| `testsuite/clone/include_list_b.h` | 79 | 3 | 1 | R |
| `testsuite/clone/include_list_nest.h` | 60 | 3 | 1 | R |
| `testsuite/clone/include_list_skipped.h` | 130 | 4 | 1 | R |
| `testsuite/clone/include_list_src.lpc` | 291 | 11 | 1 | R |
| `testsuite/clone/inh0.c` | 0 | 1 | 1 | R |
| `testsuite/clone/inh1.c` | 25 | 2 | 1 | R |
| `testsuite/clone/inh2.c` | 25 | 2 | 1 | R |
| `testsuite/clone/login.c` | 581 | 32 | 1 | R |
| `testsuite/clone/memory_holder.c` | 1982 | 88 | 1 | R |
| `testsuite/clone/mgip1.c` | 42 | 4 | 1 | R |
| `testsuite/clone/mgip2.c` | 42 | 4 | 1 | R |
| `testsuite/clone/mgip3.c` | 42 | 4 | 1 | R |
| `testsuite/clone/mgip4.c` | 42 | 4 | 1 | R |
| `testsuite/clone/move_object_item.lpc` | 153 | 4 | 1 | R |
| `testsuite/clone/move_object_reset_dest.lpc` | 311 | 10 | 1 | R |
| `testsuite/clone/priv_alias_grandleaf.lpc` | 34 | 2 | 1 | R |
| `testsuite/clone/priv_alias_leaf.lpc` | 35 | 2 | 1 | R |
| `testsuite/clone/priv_alias_leaf_own.lpc` | 66 | 4 | 1 | R |
| `testsuite/clone/priv_proto_deep.lpc` | 69 | 4 | 1 | R |
| `testsuite/clone/priv_proto_priv.lpc` | 78 | 4 | 1 | R |
| `testsuite/clone/priv_proto_pub.lpc` | 69 | 4 | 1 | R |
| `testsuite/clone/priv_same_ab.lpc` | 64 | 3 | 1 | R |
| `testsuite/clone/priv_same_ba.lpc` | 64 | 3 | 1 | R |
| `testsuite/clone/priv_same_child.lpc` | 95 | 5 | 1 | R |
| `testsuite/clone/reclaim_fp_helper.lpc` | 183 | 5 | 1 | R |
| `testsuite/clone/recompile_blueprint.c` | 1281 | 35 | 1 | R |
| `testsuite/clone/recompile_lifecycle_probe.c` | 1286 | 54 | 1 | R |
| `testsuite/clone/recompile_simul_compile_probe.c` | 212 | 5 | 1 | R |
| `testsuite/clone/recompile_unrelated_probe.c` | 258 | 7 | 1 | R |
| `testsuite/clone/reload_counter.lpc` | 277 | 7 | 1 | R |
| `testsuite/clone/reload_self_destruct.lpc` | 463 | 14 | 1 | R |
| `testsuite/clone/replaceable_1.c` | 100 | 8 | 1 | R |
| `testsuite/clone/replaceable_2.c` | 95 | 8 | 1 | R |
| `testsuite/clone/replaceable_inh.c` | 41 | 3 | 1 | R |
| `testsuite/clone/testob1.c` | 65 | 4 | 1 | R |
| `testsuite/clone/testob2.c` | 65 | 4 | 1 | R |
| `testsuite/clone/testob3.c` | 65 | 4 | 1 | R |
| `testsuite/clone/user.c` | 5457 | 262 | 1 | R |
| `testsuite/clone/websocket_session_user.lpc` | 2926 | 110 | 1 | R |
| `testsuite/command/README` | 135 | 3 | 1 | R |
| `testsuite/command/callouts.c` | 417 | 17 | 1 | R |
| `testsuite/command/codefor.c` | 563 | 24 | 1 | R |
| `testsuite/command/crasher.c` | 15608 | 486 | 1 | R |
| `testsuite/command/dest.c` | 613 | 37 | 1 | R |
| `testsuite/command/ed.c` | 185 | 12 | 1 | R |
| `testsuite/command/eval.c` | 336 | 12 | 1 | R |
| `testsuite/command/external_curl.c` | 952 | 49 | 1 | R |
| `testsuite/command/gmcp.c` | 381 | 20 | 1 | R |
| `testsuite/command/more.c` | 8188 | 313 | 1 | R |
| `testsuite/command/msdp.c` | 672 | 21 | 1 | R |
| `testsuite/command/mxp.c` | 220 | 13 | 1 | R |
| `testsuite/command/quit.c` | 247 | 14 | 1 | R |
| `testsuite/command/refs.c` | 127 | 8 | 1 | R |
| `testsuite/command/rm.c` | 121 | 8 | 1 | R |
| `testsuite/command/say.c` | 194 | 12 | 1 | R |
| `testsuite/command/shutdown.c` | 74 | 7 | 1 | R |
| `testsuite/command/speed.c` | 8832 | 270 | 1 | R |
| `testsuite/command/stress_memory.c` | 498 | 27 | 1 | R |
| `testsuite/command/stress_string.c` | 2551 | 93 | 1 | R |
| `testsuite/command/test_input_to.c` | 744 | 39 | 1 | R |
| `testsuite/command/test_tls.c` | 269 | 14 | 1 | R |
| `testsuite/command/tests.c` | 10891 | 405 | 1 | R |
| `testsuite/command/update.c` | 344 | 21 | 1 | R |
| `testsuite/command/who.c` | 430 | 18 | 1 | R |
| `testsuite/crlf_test_file.crlf` | 90 | 16 | 1 | R |
| `testsuite/crlf_test_file.lf` | 90 | 16 | 1 | R |
| `testsuite/data/README` | 77 | 2 | 1 | R |
| `testsuite/data/compress/zipbomb.zbin` | 1960 | — | — | M |
| `testsuite/data/gateway_net_dead_destruct_user.o` | 128 | 5 | 1 | R |
| `testsuite/data/sha1/README` | 94 | 4 | 1 | R |
| `testsuite/data/sha1/Readme.txt` | 1039 | 26 | 1 | R |
| `testsuite/data/sha1/byte-hashes.sha1` | 8592 | 205 | 1 | R |
| `testsuite/data/sha1/byte-messages.sha1` | 97475 | 177 | 1 | R |
| `testsuite/etc/cert.pem` | 847 | 16 | 1 | R |
| `testsuite/etc/config.recompile` | 14288 | 396 | 1 | R |
| `testsuite/etc/config.test` | 14385 | 398 | 1 | R |
| `testsuite/etc/efun_define.c` | 23 | 2 | 1 | R |
| `testsuite/etc/key.pem` | 288 | 7 | 1 | R |
| `testsuite/etc/motd` | 404 | 14 | 1 | R |
| `testsuite/etc/test-scopes.tsv` | 7978 | 83 | 1 | R |
| `testsuite/etc/vimrc` | 6429 | 181 | 1 | R |
| `testsuite/format.sh` | 3237 | 102 | 1 | R |
| `testsuite/include/README` | 98 | 3 | 1 | R |
| `testsuite/include/command.h` | 40 | 4 | 1 | R |
| `testsuite/include/config.h` | 227 | 10 | 1 | R |
| `testsuite/include/globals.h` | 702 | 36 | 1 | R |
| `testsuite/include/lpctypes.h` | 337 | 17 | 1 | R |
| `testsuite/include/m_gip1/m_gip_test.h` | 112 | 10 | 1 | R |
| `testsuite/include/m_gip2/m_gip_test.h` | 112 | 10 | 1 | R |
| `testsuite/include/m_gip_test.h` | 112 | 10 | 1 | R |
| `testsuite/include/tests.h` | 1009 | 27 | 1 | R |
| `testsuite/inherit/README` | 81 | 3 | 1 | R |
| `testsuite/inherit/base.c` | 712 | 31 | 1 | R |
| `testsuite/inherit/clean_up.c` | 65 | 5 | 1 | R |
| `testsuite/inherit/master/valid.c` | 4260 | 166 | 1 | R |
| `testsuite/inherit/nomask.c` | 41 | 4 | 1 | R |
| `testsuite/inherit/priv_alias_base.lpc` | 63 | 4 | 1 | R |
| `testsuite/inherit/priv_alias_mid.lpc` | 74 | 4 | 1 | R |
| `testsuite/inherit/priv_proto_body.lpc` | 61 | 4 | 1 | R |
| `testsuite/inherit/priv_proto_mid.lpc` | 36 | 2 | 1 | R |
| `testsuite/inherit/priv_same_a.lpc` | 121 | 6 | 1 | R |
| `testsuite/inherit/priv_same_b.lpc` | 121 | 6 | 1 | R |
| `testsuite/inherit/tests.c` | 125 | 10 | 1 | R |
| `testsuite/log/README` | 72 | 2 | 1 | R |
| `testsuite/single/README` | 193 | 4 | 1 | R |
| `testsuite/single/async_phase2_base.lpc` | 67 | 5 | 1 | R |
| `testsuite/single/async_phase2_probe.lpc` | 591 | 32 | 1 | R |
| `testsuite/single/inh.c` | 59 | 3 | 1 | R |
| `testsuite/single/master.c` | 11663 | 446 | 1 | R |
| `testsuite/single/on_destruct_bad.c` | 211 | 11 | 1 | R |
| `testsuite/single/on_destruct_good.c` | 453 | 23 | 1 | R |
| `testsuite/single/owner_singleton.c` | 16 | 2 | 1 | R |
| `testsuite/single/promise_destruct_probe.lpc` | 587 | 15 | 1 | R |
| `testsuite/single/recompile_probe.c` | 720 | 23 | 1 | R |
| `testsuite/single/simul_efun.c` | 7751 | 319 | 1 | R |
| `testsuite/single/tests/applies/on_destruct_test.c` | 874 | 21 | 1 | R |
| `testsuite/single/tests/compiler/.gitattributes` | 30 | 2 | 1 | R |
| `testsuite/single/tests/compiler/0default_args.c` | 3843 | 128 | 1 | R |
| `testsuite/single/tests/compiler/0default_args_1.c` | 74 | 5 | 1 | R |
| `testsuite/single/tests/compiler/64bit.c` | 4781 | 174 | 1 | R |
| `testsuite/single/tests/compiler/a2_grammar.lpc` | 1086 | 40 | 1 | R |
| `testsuite/single/tests/compiler/async_phase2_smoke.lpc` | 858 | 39 | 1 | R |
| `testsuite/single/tests/compiler/at_block.c` | 143 | 14 | 1 | R |
| `testsuite/single/tests/compiler/at_block_crlf.c` | 579 | 18 | 1 | R |
| `testsuite/single/tests/compiler/bad_macro_params.lpc` | 324 | 8 | 1 | R |
| `testsuite/single/tests/compiler/class.c` | 3405 | 115 | 1 | R |
| `testsuite/single/tests/compiler/conditional.c` | 4474 | 185 | 1 | R |
| `testsuite/single/tests/compiler/constant_expr.c` | 834 | 40 | 1 | R |
| `testsuite/single/tests/compiler/deep_definition_chain.lpc` | 1105 | 33 | 1 | R |
| `testsuite/single/tests/compiler/deep_expr_recursion.lpc` | 1030 | 29 | 1 | R |
| `testsuite/single/tests/compiler/deep_nesting_caps.c` | 1137 | 36 | 1 | R |
| `testsuite/single/tests/compiler/deep_statement_chain.lpc` | 881 | 30 | 1 | R |
| `testsuite/single/tests/compiler/default_args_error_unwind.c` | 3002 | 70 | 1 | R |
| `testsuite/single/tests/compiler/default_args_inherit.c` | 1500 | 31 | 1 | R |
| `testsuite/single/tests/compiler/fail/.gitattributes` | 77 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/arr_index.c` | 24 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_$var.c` | 65 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_at_block.c` | 46 | 5 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_break.c` | 24 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_continue.c` | 27 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_for_init.c` | 39 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_foreach.c` | 38 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_init.c` | 12 | 2 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_local_init.c` | 29 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_local_init2.c` | 41 | 6 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_param.c` | 28 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_param2.c` | 30 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_return.c` | 37 | 6 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_return2.c` | 39 | 6 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_string_index.c` | 81 | 2 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_type_init.c` | 24 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_utf8_arrayblock.c` | 22869 | — | — | M |
| `testsuite/single/tests/compiler/fail/bad_utf8_literial.c` | 116 | 7 | 1 | R |
| `testsuite/single/tests/compiler/fail/bad_utf8_string.c` | 22867 | — | — | M |
| `testsuite/single/tests/compiler/fail/class_redef.c` | 50 | 8 | 1 | R |
| `testsuite/single/tests/compiler/fail/div_zero.c` | 57 | 6 | 1 | R |
| `testsuite/single/tests/compiler/fail/div_zero3.c` | 32 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/eof_in_comment.c` | 3 | 2 | 1 | R |
| `testsuite/single/tests/compiler/fail/eof_in_string.c` | 24 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/global_modifier.c` | 117 | 15 | 1 | R |
| `testsuite/single/tests/compiler/fail/inherit_exact_ext.lpc` | 181 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/local_redef.c` | 29 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/local_redef2.c` | 43 | 7 | 1 | R |
| `testsuite/single/tests/compiler/fail/lvar_func.c` | 35 | 5 | 1 | R |
| `testsuite/single/tests/compiler/fail/lvar_func2.c` | 39 | 5 | 1 | R |
| `testsuite/single/tests/compiler/fail/missing_endif.c` | 6 | 2 | 1 | R |
| `testsuite/single/tests/compiler/fail/missing_package_efun.c` | 75 | 6 | 1 | R |
| `testsuite/single/tests/compiler/fail/missing_type.c` | 27 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/mixed_case.c` | 68 | 7 | 1 | R |
| `testsuite/single/tests/compiler/fail/mod_zero.c` | 57 | 6 | 1 | R |
| `testsuite/single/tests/compiler/fail/no_member.c` | 56 | 9 | 1 | R |
| `testsuite/single/tests/compiler/fail/no_ret_type.c` | 31 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/no_ret_type.crlf.c` | 34 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/no_such_class.c` | 33 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/predef.c` | 19 | 2 | 1 | R |
| `testsuite/single/tests/compiler/fail/redef_function.c` | 29 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/static_local.c` | 36 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/string_case.c` | 60 | 6 | 1 | R |
| `testsuite/single/tests/compiler/fail/two_default.c` | 60 | 7 | 1 | R |
| `testsuite/single/tests/compiler/fail/undef_class.c` | 13 | 2 | 1 | R |
| `testsuite/single/tests/compiler/fail/var_to_nonvar.c` | 61 | 8 | 1 | R |
| `testsuite/single/tests/compiler/fail/void_arg.c` | 20 | 2 | 1 | R |
| `testsuite/single/tests/compiler/fail/void_arg2.c` | 25 | 2 | 1 | R |
| `testsuite/single/tests/compiler/fail/void_for_var.c` | 39 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/void_global.c` | 8 | 2 | 1 | R |
| `testsuite/single/tests/compiler/fail/void_local.c` | 25 | 4 | 1 | R |
| `testsuite/single/tests/compiler/fail/void_member.c` | 24 | 4 | 1 | R |
| `testsuite/single/tests/compiler/firstclass_functions.c` | 4063 | 197 | 1 | R |
| `testsuite/single/tests/compiler/foreach_unwind.lpc` | 801 | 44 | 1 | R |
| `testsuite/single/tests/compiler/function.c` | 1875 | 89 | 1 | R |
| `testsuite/single/tests/compiler/function_callother.c` | 1249 | 40 | 1 | R |
| `testsuite/single/tests/compiler/function_inherit.c` | 1154 | 54 | 1 | R |
| `testsuite/single/tests/compiler/function_inherit_2.c` | 298 | 20 | 1 | R |
| `testsuite/single/tests/compiler/get_include_path.c` | 293 | 16 | 1 | R |
| `testsuite/single/tests/compiler/huge_file_line_table.lpc` | 1160 | 41 | 1 | R |
| `testsuite/single/tests/compiler/include_merge_limits.lpc` | 92 | 6 | 1 | R |
| `testsuite/single/tests/compiler/include_path_limits.lpc` | 86 | 6 | 1 | R |
| `testsuite/single/tests/compiler/include_paths.lpc` | 3868 | 133 | 1 | R |
| `testsuite/single/tests/compiler/literal.c` | 1234 | 54 | 1 | R |
| `testsuite/single/tests/compiler/long_local_name.lpc` | 1188 | 34 | 1 | R |
| `testsuite/single/tests/compiler/marco_crlf.c` | 155 | 16 | 1 | R |
| `testsuite/single/tests/compiler/mixed_declarations.c` | 5323 | 216 | 1 | R |
| `testsuite/single/tests/compiler/optimize.c` | 461 | 28 | 1 | R |
| `testsuite/single/tests/compiler/positive_compilation.lpc` | 385 | 12 | 1 | R |
| `testsuite/single/tests/compiler/preprocessor.lpc` | 3179 | 127 | 1 | R |
| `testsuite/single/tests/compiler/private_inherit_alias_chain.lpc` | 724 | 16 | 1 | R |
| `testsuite/single/tests/compiler/private_inherit_same_name.lpc` | 933 | 26 | 1 | R |
| `testsuite/single/tests/compiler/private_prototype_descendant.lpc` | 587 | 11 | 1 | R |
| `testsuite/single/tests/compiler/succeed.c` | 235 | 23 | 1 | R |
| `testsuite/single/tests/compiler/switch.c` | 9475 | 491 | 1 | R |
| `testsuite/single/tests/compiler/update_simulefun.c` | 231 | 13 | 1 | R |
| `testsuite/single/tests/compiler/utf8.c` | 79041 | 941 | 1 | R |
| `testsuite/single/tests/compiler/valid_override_file.lpc` | 2255 | 69 | 1 | R |
| `testsuite/single/tests/compiler/warn_%s.lpc` | 406 | 9 | 1 | R |
| `testsuite/single/tests/compiler/warn_%s_a.lpc` | 185 | 5 | 1 | R |
| `testsuite/single/tests/compiler/warn_b.lpc` | 185 | 5 | 1 | R |
| `testsuite/single/tests/crasher/1.c` | 43 | 4 | 1 | R |
| `testsuite/single/tests/crasher/1063-aux.c` | 17 | 2 | 1 | R |
| `testsuite/single/tests/crasher/1063.c` | 741 | 29 | 1 | R |
| `testsuite/single/tests/crasher/11.c` | 62 | 6 | 1 | R |
| `testsuite/single/tests/crasher/12.c` | 95 | 8 | 1 | R |
| `testsuite/single/tests/crasher/13.c` | 82 | 4 | 1 | R |
| `testsuite/single/tests/crasher/14.c` | 122 | 8 | 1 | R |
| `testsuite/single/tests/crasher/15.c` | 258 | 9 | 1 | R |
| `testsuite/single/tests/crasher/16.c` | 119 | 7 | 1 | R |
| `testsuite/single/tests/crasher/16b.c` | 20 | 2 | 1 | R |
| `testsuite/single/tests/crasher/17.c` | 109 | 5 | 1 | R |
| `testsuite/single/tests/crasher/17b.c` | 124 | 5 | 1 | R |
| `testsuite/single/tests/crasher/2.c` | 53 | 4 | 1 | R |
| `testsuite/single/tests/crasher/283.c` | 111 | 5 | 1 | R |
| `testsuite/single/tests/crasher/4.c` | 111 | 8 | 1 | R |
| `testsuite/single/tests/crasher/5.c` | 71 | 7 | 1 | R |
| `testsuite/single/tests/crasher/7.c` | 52 | 6 | 1 | R |
| `testsuite/single/tests/crasher/750.c` | 501 | 27 | 1 | R |
| `testsuite/single/tests/crasher/773.c` | 65 | 4 | 1 | R |
| `testsuite/single/tests/crasher/8.c` | 77 | 6 | 1 | R |
| `testsuite/single/tests/crasher/864.c` | 278 | 18 | 1 | R |
| `testsuite/single/tests/crasher/871.c` | 794 | 18 | 1 | R |
| `testsuite/single/tests/crasher/9.c` | 118 | 7 | 1 | R |
| `testsuite/single/tests/crasher/callout0.c` | 396 | 17 | 1 | R |
| `testsuite/single/tests/crasher/catch.c` | 10979 | 323 | 1 | R |
| `testsuite/single/tests/crasher/living_parse_nonobject.lpc` | 569 | 16 | 1 | R |
| `testsuite/single/tests/crasher/parser_handler_destruct.lpc` | 1028 | 40 | 1 | R |
| `testsuite/single/tests/crasher/reclaim_funptr_owner.lpc` | 871 | 24 | 1 | R |
| `testsuite/single/tests/crasher/replaceable_empty.lpc` | 525 | 15 | 1 | R |
| `testsuite/single/tests/crasher/rp_base.lpc` | 186 | 6 | 1 | R |
| `testsuite/single/tests/crasher/socket_long_host.lpc` | 581 | 22 | 1 | R |
| `testsuite/single/tests/crasher/terminal_colour_error_replace.lpc` | 661 | 18 | 1 | R |
| `testsuite/single/tests/efuns/README` | 0 | 1 | 1 | R |
| `testsuite/single/tests/efuns/a2_runtime.lpc` | 2050 | 51 | 1 | R |
| `testsuite/single/tests/efuns/abs.c` | 350 | 14 | 1 | R |
| `testsuite/single/tests/efuns/acatch_defer_catch_value.lpc` | 2249 | 88 | 1 | R |
| `testsuite/single/tests/efuns/acos.lpc` | 231 | 7 | 1 | R |
| `testsuite/single/tests/efuns/add_action.c` | 878 | 32 | 1 | R |
| `testsuite/single/tests/efuns/all_inventory.c` | 525 | 28 | 1 | R |
| `testsuite/single/tests/efuns/allocate.c` | 209 | 9 | 1 | R |
| `testsuite/single/tests/efuns/allocate.lpc` | 570 | 19 | 1 | R |
| `testsuite/single/tests/efuns/allocate_buffer.c` | 129 | 7 | 1 | R |
| `testsuite/single/tests/efuns/allocate_mapping.c` | 175 | 11 | 1 | R |
| `testsuite/single/tests/efuns/allocate_mapping_callback_throw.c` | 1004 | 35 | 1 | R |
| `testsuite/single/tests/efuns/angle.lpc` | 740 | 16 | 1 | R |
| `testsuite/single/tests/efuns/asin.lpc` | 231 | 7 | 1 | R |
| `testsuite/single/tests/efuns/assemble_class.lpc` | 233 | 8 | 1 | R |
| `testsuite/single/tests/efuns/async.c` | 2472 | 81 | 1 | R |
| `testsuite/single/tests/efuns/async_await.lpc` | 1068 | 43 | 1 | R |
| `testsuite/single/tests/efuns/async_db_exec_bound_args.lpc` | 1645 | 45 | 1 | R |
| `testsuite/single/tests/efuns/async_foreach.lpc` | 529 | 20 | 1 | R |
| `testsuite/single/tests/efuns/async_info_names.lpc` | 795 | 38 | 1 | R |
| `testsuite/single/tests/efuns/async_info_scheduler.lpc` | 1450 | 38 | 1 | R |
| `testsuite/single/tests/efuns/async_inherit.lpc` | 1282 | 49 | 1 | R |
| `testsuite/single/tests/efuns/async_inherit_park.lpc` | 797 | 35 | 1 | R |
| `testsuite/single/tests/efuns/async_nested_callbacks.lpc` | 3183 | 122 | 1 | R |
| `testsuite/single/tests/efuns/async_promise.c` | 1275 | 52 | 1 | R |
| `testsuite/single/tests/efuns/async_yield.lpc` | 771 | 38 | 1 | R |
| `testsuite/single/tests/efuns/async_yield_shutdown.lpc` | 402 | 20 | 1 | R |
| `testsuite/single/tests/efuns/atan.lpc` | 184 | 6 | 1 | R |
| `testsuite/single/tests/efuns/b3_bp.c` | 129 | 5 | 1 | R |
| `testsuite/single/tests/efuns/badshad.c` | 167 | 11 | 1 | R |
| `testsuite/single/tests/efuns/badshad2.c` | 171 | 11 | 1 | R |
| `testsuite/single/tests/efuns/base_name.lpc` | 233 | 6 | 1 | R |
| `testsuite/single/tests/efuns/bind.c` | 410 | 15 | 1 | R |
| `testsuite/single/tests/efuns/bind_destruct_owner.c` | 1073 | 32 | 1 | R |
| `testsuite/single/tests/efuns/break_cycles.c` | 5079 | 145 | 1 | R |
| `testsuite/single/tests/efuns/buffer_transcode.lpc` | 326 | 8 | 1 | R |
| `testsuite/single/tests/efuns/bufferp.lpc` | 128 | 6 | 1 | R |
| `testsuite/single/tests/efuns/cache_stats.c` | 84 | 6 | 1 | R |
| `testsuite/single/tests/efuns/call_other.c` | 637 | 27 | 1 | R |
| `testsuite/single/tests/efuns/call_out.c` | 1422 | 77 | 1 | R |
| `testsuite/single/tests/efuns/call_out_walltime.lpc` | 242 | 12 | 1 | R |
| `testsuite/single/tests/efuns/call_stack.c` | 739 | 26 | 1 | R |
| `testsuite/single/tests/efuns/call_stack.lpc` | 1214 | 38 | 1 | R |
| `testsuite/single/tests/efuns/call_stack_driver_frame.lpc` | 790 | 36 | 1 | R |
| `testsuite/single/tests/efuns/callout_memory_refs.lpc` | 1045 | 41 | 1 | R |
| `testsuite/single/tests/efuns/capitalize.c` | 596 | 33 | 1 | R |
| `testsuite/single/tests/efuns/ceil.lpc` | 109 | 6 | 1 | R |
| `testsuite/single/tests/efuns/children.c` | 204 | 12 | 1 | R |
| `testsuite/single/tests/efuns/classes.c` | 232 | 13 | 1 | R |
| `testsuite/single/tests/efuns/classp.lpc` | 182 | 10 | 1 | R |
| `testsuite/single/tests/efuns/clear_bit.c` | 300 | 15 | 1 | R |
| `testsuite/single/tests/efuns/clear_debug_level.lpc` | 221 | 10 | 1 | R |
| `testsuite/single/tests/efuns/clone_object.c` | 345 | 21 | 1 | R |
| `testsuite/single/tests/efuns/clonep.c` | 161 | 8 | 1 | R |
| `testsuite/single/tests/efuns/command.c` | 690 | 35 | 1 | R |
| `testsuite/single/tests/efuns/compress.lpc` | 357 | 11 | 1 | R |
| `testsuite/single/tests/efuns/compress_file.c` | 1184 | 42 | 1 | R |
| `testsuite/single/tests/efuns/compress_file_integrity.lpc` | 2047 | 67 | 1 | R |
| `testsuite/single/tests/efuns/compress_file_long_path.lpc` | 509 | 15 | 1 | R |
| `testsuite/single/tests/efuns/compressedp.lpc` | 157 | 7 | 1 | R |
| `testsuite/single/tests/efuns/copy.lpc` | 357 | 12 | 1 | R |
| `testsuite/single/tests/efuns/cos.lpc` | 228 | 7 | 1 | R |
| `testsuite/single/tests/efuns/cp.c` | 242 | 10 | 1 | R |
| `testsuite/single/tests/efuns/crc32.c` | 84 | 5 | 1 | R |
| `testsuite/single/tests/efuns/crypt.c` | 2922 | 87 | 1 | R |
| `testsuite/single/tests/efuns/ctime.c` | 220 | 10 | 1 | R |
| `testsuite/single/tests/efuns/db.c` | 2781 | 109 | 1 | R |
| `testsuite/single/tests/efuns/debug_info.lpc` | 440 | 14 | 1 | R |
| `testsuite/single/tests/efuns/debug_levels.lpc` | 52 | 4 | 1 | R |
| `testsuite/single/tests/efuns/debug_message.lpc` | 85 | 4 | 1 | R |
| `testsuite/single/tests/efuns/debugmalloc.lpc` | 528 | 20 | 1 | R |
| `testsuite/single/tests/efuns/deep_inherit_list.c` | 236 | 10 | 1 | R |
| `testsuite/single/tests/efuns/deep_inventory.c` | 454 | 23 | 1 | R |
| `testsuite/single/tests/efuns/defer.c` | 542 | 28 | 1 | R |
| `testsuite/single/tests/efuns/destruct.c` | 94 | 7 | 1 | R |
| `testsuite/single/tests/efuns/disable_commands.c` | 322 | 16 | 1 | R |
| `testsuite/single/tests/efuns/disable_commands2.c` | 1539 | 66 | 1 | R |
| `testsuite/single/tests/efuns/distance.lpc` | 221 | 6 | 1 | R |
| `testsuite/single/tests/efuns/dotprod.lpc` | 1100 | 22 | 1 | R |
| `testsuite/single/tests/efuns/dual_extension.lpc` | 2308 | 57 | 1 | R |
| `testsuite/single/tests/efuns/dump_file_descriptors.lpc` | 167 | 6 | 1 | R |
| `testsuite/single/tests/efuns/dump_jemalloc.lpc` | 60 | 4 | 1 | R |
| `testsuite/single/tests/efuns/dump_prog.c` | 326 | 9 | 1 | R |
| `testsuite/single/tests/efuns/dump_stralloc.lpc` | 1107 | 27 | 1 | R |
| `testsuite/single/tests/efuns/dumpallobj.c` | 500 | 20 | 1 | R |
| `testsuite/single/tests/efuns/dwlib_replace_objects.lpc` | 856 | 21 | 1 | R |
| `testsuite/single/tests/efuns/ed.c` | 327 | 14 | 1 | R |
| `testsuite/single/tests/efuns/element_of.lpc` | 253 | 10 | 1 | R |
| `testsuite/single/tests/efuns/enable_wizard.c` | 468 | 18 | 1 | R |
| `testsuite/single/tests/efuns/encoding.c` | 913 | 37 | 1 | R |
| `testsuite/single/tests/efuns/environment.c` | 319 | 18 | 1 | R |
| `testsuite/single/tests/efuns/error.c` | 106 | 5 | 1 | R |
| `testsuite/single/tests/efuns/error_handler_mapping_size.lpc` | 210 | 10 | 1 | R |
| `testsuite/single/tests/efuns/evaluate.lpc` | 209 | 9 | 1 | R |
| `testsuite/single/tests/efuns/event.lpc` | 355 | 16 | 1 | R |
| `testsuite/single/tests/efuns/exec.c` | 174 | 10 | 1 | R |
| `testsuite/single/tests/efuns/exp.lpc` | 181 | 6 | 1 | R |
| `testsuite/single/tests/efuns/explode.c` | 5860 | 166 | 1 | R |
| `testsuite/single/tests/efuns/external_promise.c` | 1543 | 49 | 1 | R |
| `testsuite/single/tests/efuns/external_start.lpc` | 301 | 12 | 1 | R |
| `testsuite/single/tests/efuns/fetch_class_member.lpc` | 223 | 9 | 1 | R |
| `testsuite/single/tests/efuns/fetch_variable.lpc` | 229 | 9 | 1 | R |
| `testsuite/single/tests/efuns/file_io_failure.lpc` | 831 | 22 | 1 | R |
| `testsuite/single/tests/efuns/file_length.lpc` | 269 | 9 | 1 | R |
| `testsuite/single/tests/efuns/file_name.c` | 62 | 4 | 1 | R |
| `testsuite/single/tests/efuns/file_size.c` | 439 | 17 | 1 | R |
| `testsuite/single/tests/efuns/filter.c` | 1775 | 70 | 1 | R |
| `testsuite/single/tests/efuns/find_call_out.c` | 185 | 8 | 1 | R |
| `testsuite/single/tests/efuns/find_cycles.c` | 1997 | 74 | 1 | R |
| `testsuite/single/tests/efuns/find_living.c` | 292 | 17 | 1 | R |
| `testsuite/single/tests/efuns/find_object.c` | 512 | 16 | 1 | R |
| `testsuite/single/tests/efuns/find_player.c` | 412 | 22 | 1 | R |
| `testsuite/single/tests/efuns/first_inventory.lpc` | 363 | 18 | 1 | R |
| `testsuite/single/tests/efuns/floatp.lpc` | 137 | 7 | 1 | R |
| `testsuite/single/tests/efuns/floor.lpc` | 112 | 6 | 1 | R |
| `testsuite/single/tests/efuns/function_exists.c` | 813 | 28 | 1 | R |
| `testsuite/single/tests/efuns/function_owner.lpc` | 80 | 4 | 1 | R |
| `testsuite/single/tests/efuns/function_profile.c` | 281 | 11 | 1 | R |
| `testsuite/single/tests/efuns/functionp.c` | 111 | 6 | 1 | R |
| `testsuite/single/tests/efuns/functions.c` | 1601 | 81 | 1 | R |
| `testsuite/single/tests/efuns/get_config.c` | 449 | 12 | 1 | R |
| `testsuite/single/tests/efuns/get_dir.c` | 354 | 13 | 1 | R |
| `testsuite/single/tests/efuns/get_os_env.lpc` | 827 | 25 | 1 | R |
| `testsuite/single/tests/efuns/geteuid.lpc` | 152 | 8 | 1 | R |
| `testsuite/single/tests/efuns/getuid.lpc` | 135 | 8 | 1 | R |
| `testsuite/single/tests/efuns/goodshad.c` | 199 | 13 | 1 | R |
| `testsuite/single/tests/efuns/has_cycle.c` | 2951 | 106 | 1 | R |
| `testsuite/single/tests/efuns/hash.c` | 2325 | 61 | 1 | R |
| `testsuite/single/tests/efuns/implode.c` | 506 | 13 | 1 | R |
| `testsuite/single/tests/efuns/in_edit.c` | 180 | 9 | 1 | R |
| `testsuite/single/tests/efuns/include_list.lpc` | 937 | 29 | 1 | R |
| `testsuite/single/tests/efuns/inherit_list.c` | 190 | 8 | 1 | R |
| `testsuite/single/tests/efuns/inherits.c` | 418 | 19 | 1 | R |
| `testsuite/single/tests/efuns/input_to.c` | 193 | 9 | 1 | R |
| `testsuite/single/tests/efuns/interactive.c` | 193 | 7 | 1 | R |
| `testsuite/single/tests/efuns/intp.c` | 200 | 10 | 1 | R |
| `testsuite/single/tests/efuns/is_daylight_savings_time.lpc` | 142 | 5 | 1 | R |
| `testsuite/single/tests/efuns/json_encode_frozen.c` | 1855 | 53 | 1 | R |
| `testsuite/single/tests/efuns/keys.c` | 162 | 11 | 1 | R |
| `testsuite/single/tests/efuns/light.c` | 141 | 12 | 1 | R |
| `testsuite/single/tests/efuns/link.lpc` | 424 | 14 | 1 | R |
| `testsuite/single/tests/efuns/living.c` | 143 | 8 | 1 | R |
| `testsuite/single/tests/efuns/livings.c` | 189 | 8 | 1 | R |
| `testsuite/single/tests/efuns/load_object_long_path.lpc` | 935 | 25 | 1 | R |
| `testsuite/single/tests/efuns/log.lpc` | 258 | 8 | 1 | R |
| `testsuite/single/tests/efuns/log10.lpc` | 196 | 7 | 1 | R |
| `testsuite/single/tests/efuns/log2.lpc` | 191 | 7 | 1 | R |
| `testsuite/single/tests/efuns/lower_case.c` | 360 | 20 | 1 | R |
| `testsuite/single/tests/efuns/ltrim.lpc` | 290 | 11 | 1 | R |
| `testsuite/single/tests/efuns/malloc_status.c` | 56 | 4 | 1 | R |
| `testsuite/single/tests/efuns/map.c` | 1652 | 55 | 1 | R |
| `testsuite/single/tests/efuns/map_delete.c` | 274 | 12 | 1 | R |
| `testsuite/single/tests/efuns/mapp.c` | 86 | 6 | 1 | R |
| `testsuite/single/tests/efuns/master.c` | 69 | 5 | 1 | R |
| `testsuite/single/tests/efuns/match_path.c` | 381 | 12 | 1 | R |
| `testsuite/single/tests/efuns/math_vector_results.lpc` | 1370 | 29 | 1 | R |
| `testsuite/single/tests/efuns/matrix.c` | 732 | 23 | 1 | R |
| `testsuite/single/tests/efuns/matrix_type_check.lpc` | 1870 | 45 | 1 | R |
| `testsuite/single/tests/efuns/max.lpc` | 232 | 8 | 1 | R |
| `testsuite/single/tests/efuns/member_array.c` | 976 | 23 | 1 | R |
| `testsuite/single/tests/efuns/memory_info.lpc` | 145 | 6 | 1 | R |
| `testsuite/single/tests/efuns/memory_summary.lpc` | 96 | 6 | 1 | R |
| `testsuite/single/tests/efuns/min.lpc` | 268 | 9 | 1 | R |
| `testsuite/single/tests/efuns/mkdir.c` | 255 | 12 | 1 | R |
| `testsuite/single/tests/efuns/modernization_compat.lpc` | 1857 | 58 | 1 | R |
| `testsuite/single/tests/efuns/move_object.c` | 318 | 12 | 1 | R |
| `testsuite/single/tests/efuns/mud_status.c` | 235 | 10 | 1 | R |
| `testsuite/single/tests/efuns/nested_array_free_recursion.lpc` | 1290 | 35 | 1 | R |
| `testsuite/single/tests/efuns/network_stats.lpc` | 95 | 6 | 1 | R |
| `testsuite/single/tests/efuns/new.lpc` | 229 | 11 | 1 | R |
| `testsuite/single/tests/efuns/next_bit.lpc` | 174 | 7 | 1 | R |
| `testsuite/single/tests/efuns/next_inventory.lpc` | 344 | 17 | 1 | R |
| `testsuite/single/tests/efuns/norm.lpc` | 182 | 6 | 1 | R |
| `testsuite/single/tests/efuns/nullp.c` | 106 | 8 | 1 | R |
| `testsuite/single/tests/efuns/num_classes.lpc` | 184 | 10 | 1 | R |
| `testsuite/single/tests/efuns/objectp.c` | 77 | 5 | 1 | R |
| `testsuite/single/tests/efuns/origin.c` | 501 | 25 | 1 | R |
| `testsuite/single/tests/efuns/os_env.c` | 983 | 25 | 1 | R |
| `testsuite/single/tests/efuns/owner_executor_contract.c` | 95665 | 940 | 1 | R |
| `testsuite/single/tests/efuns/owner_payload.c` | 8150 | 219 | 1 | R |
| `testsuite/single/tests/efuns/package_socket_external.lpc` | 2620 | 92 | 1 | R |
| `testsuite/single/tests/efuns/package_trim.c` | 1929 | 58 | 1 | R |
| `testsuite/single/tests/efuns/parse_utf8.lpc` | 1310 | 47 | 1 | R |
| `testsuite/single/tests/efuns/parser.c` | 801 | 44 | 1 | R |
| `testsuite/single/tests/efuns/parser_debug_output.c` | 511 | 35 | 1 | R |
| `testsuite/single/tests/efuns/parser_owner_probe.c` | 344 | 25 | 1 | R |
| `testsuite/single/tests/efuns/parser_reentry.c` | 1484 | 76 | 1 | R |
| `testsuite/single/tests/efuns/pcre.c` | 8650 | 276 | 1 | R |
| `testsuite/single/tests/efuns/pcre_cache.lpc` | 165 | 8 | 1 | R |
| `testsuite/single/tests/efuns/pcre_error_paths.lpc` | 769 | 24 | 1 | R |
| `testsuite/single/tests/efuns/pcre_replace.lpc` | 1147 | 25 | 1 | R |
| `testsuite/single/tests/efuns/pcre_replace_callback_zero.lpc` | 129 | 7 | 1 | R |
| `testsuite/single/tests/efuns/pcre_replace_type_checks.lpc` | 993 | 26 | 1 | R |
| `testsuite/single/tests/efuns/pcre_version.lpc` | 124 | 7 | 1 | R |
| `testsuite/single/tests/efuns/perf_counter_ns.lpc` | 115 | 7 | 1 | R |
| `testsuite/single/tests/efuns/pluralize.c` | 853 | 25 | 1 | R |
| `testsuite/single/tests/efuns/pointerp.c` | 73 | 5 | 1 | R |
| `testsuite/single/tests/efuns/pow.lpc` | 236 | 8 | 1 | R |
| `testsuite/single/tests/efuns/present.c` | 2730 | 88 | 1 | R |
| `testsuite/single/tests/efuns/present_destruct.c` | 1596 | 54 | 1 | R |
| `testsuite/single/tests/efuns/previous_object.c` | 146 | 8 | 1 | R |
| `testsuite/single/tests/efuns/program_info.lpc` | 139 | 7 | 1 | R |
| `testsuite/single/tests/efuns/promise_body_owned.lpc` | 2231 | 76 | 1 | R |
| `testsuite/single/tests/efuns/promise_contracts.lpc` | 2639 | 84 | 1 | R |
| `testsuite/single/tests/efuns/promise_memory_refs.lpc` | 554 | 23 | 1 | R |
| `testsuite/single/tests/efuns/promise_phase1.lpc` | 1724 | 61 | 1 | R |
| `testsuite/single/tests/efuns/promise_reject_default.lpc` | 1439 | 59 | 1 | R |
| `testsuite/single/tests/efuns/query_charmode.lpc` | 65 | 4 | 1 | R |
| `testsuite/single/tests/efuns/query_ed_mode.c` | 297 | 15 | 1 | R |
| `testsuite/single/tests/efuns/query_host_name.lpc` | 99 | 5 | 1 | R |
| `testsuite/single/tests/efuns/query_idle.c` | 86 | 5 | 1 | R |
| `testsuite/single/tests/efuns/query_ip_name.c` | 141 | 6 | 1 | R |
| `testsuite/single/tests/efuns/query_ip_number.c` | 141 | 6 | 1 | R |
| `testsuite/single/tests/efuns/query_ip_port.lpc` | 66 | 4 | 1 | R |
| `testsuite/single/tests/efuns/query_load_average.c` | 61 | 4 | 1 | R |
| `testsuite/single/tests/efuns/query_notify_destruct.lpc` | 115 | 5 | 1 | R |
| `testsuite/single/tests/efuns/query_notify_fail.lpc` | 140 | 6 | 1 | R |
| `testsuite/single/tests/efuns/query_num.lpc` | 169 | 7 | 1 | R |
| `testsuite/single/tests/efuns/query_privs.c` | 82 | 6 | 1 | R |
| `testsuite/single/tests/efuns/query_replaced_program.lpc` | 422 | 10 | 1 | R |
| `testsuite/single/tests/efuns/query_snoop.c` | 91 | 6 | 1 | R |
| `testsuite/single/tests/efuns/query_snooping.c` | 94 | 6 | 1 | R |
| `testsuite/single/tests/efuns/query_verb.c` | 244 | 17 | 1 | R |
| `testsuite/single/tests/efuns/random.c` | 113 | 7 | 1 | R |
| `testsuite/single/tests/efuns/read_buffer.c` | 892 | 49 | 1 | R |
| `testsuite/single/tests/efuns/read_bytes.c` | 808 | 21 | 1 | R |
| `testsuite/single/tests/efuns/read_file.c` | 1238 | 33 | 1 | R |
| `testsuite/single/tests/efuns/real_time.lpc` | 132 | 7 | 1 | R |
| `testsuite/single/tests/efuns/receive.c` | 80 | 5 | 1 | R |
| `testsuite/single/tests/efuns/reclaim_objects.lpc` | 110 | 6 | 1 | R |
| `testsuite/single/tests/efuns/recompile_object.c` | 7187 | 171 | 1 | R |
| `testsuite/single/tests/efuns/recompile_simul_one.c` | 472 | 17 | 1 | R |
| `testsuite/single/tests/efuns/recompile_special_targets.c` | 3599 | 86 | 1 | R |
| `testsuite/single/tests/efuns/recompile_stress.c` | 4121 | 108 | 1 | R |
| `testsuite/single/tests/efuns/refs.lpc` | 97 | 6 | 1 | R |
| `testsuite/single/tests/efuns/reg_assoc.c` | 678 | 23 | 1 | R |
| `testsuite/single/tests/efuns/regexp.c` | 887 | 33 | 1 | R |
| `testsuite/single/tests/efuns/reload_object_self_destruct.lpc` | 874 | 23 | 1 | R |
| `testsuite/single/tests/efuns/remove_action.c` | 773 | 33 | 1 | R |
| `testsuite/single/tests/efuns/remove_call_out.c` | 660 | 34 | 1 | R |
| `testsuite/single/tests/efuns/rename.c` | 613 | 19 | 1 | R |
| `testsuite/single/tests/efuns/repeat_string.lpc` | 499 | 13 | 1 | R |
| `testsuite/single/tests/efuns/replace_program.lpc` | 779 | 20 | 1 | R |
| `testsuite/single/tests/efuns/replace_string.c` | 4822 | 84 | 1 | R |
| `testsuite/single/tests/efuns/replaceable.c` | 380 | 18 | 1 | R |
| `testsuite/single/tests/efuns/request_clean_up.lpc` | 353 | 11 | 1 | R |
| `testsuite/single/tests/efuns/resolve.lpc` | 822 | 32 | 1 | R |
| `testsuite/single/tests/efuns/restore_from_string.lpc` | 366 | 21 | 1 | R |
| `testsuite/single/tests/efuns/restore_object.c` | 950 | 56 | 1 | R |
| `testsuite/single/tests/efuns/restore_variable.c` | 1208 | 44 | 1 | R |
| `testsuite/single/tests/efuns/restore_variable.lpc` | 2899 | 85 | 1 | R |
| `testsuite/single/tests/efuns/restore_variable_class.lpc` | 611 | 23 | 1 | R |
| `testsuite/single/tests/efuns/restore_variable_parser_desync.lpc` | 218 | 7 | 1 | R |
| `testsuite/single/tests/efuns/restore_variable_unterminated_escape.lpc` | 230 | 7 | 1 | R |
| `testsuite/single/tests/efuns/rm.c` | 143 | 7 | 1 | R |
| `testsuite/single/tests/efuns/rmdir.c` | 136 | 7 | 1 | R |
| `testsuite/single/tests/efuns/round.lpc` | 139 | 6 | 1 | R |
| `testsuite/single/tests/efuns/rp_a.c` | 129 | 5 | 1 | R |
| `testsuite/single/tests/efuns/rp_leaf.c` | 175 | 6 | 1 | R |
| `testsuite/single/tests/efuns/rp_mid.c` | 171 | 6 | 1 | R |
| `testsuite/single/tests/efuns/rtrim.lpc` | 290 | 11 | 1 | R |
| `testsuite/single/tests/efuns/rusage.c` | 152 | 7 | 1 | R |
| `testsuite/single/tests/efuns/save_file_integrity.lpc` | 489 | 17 | 1 | R |
| `testsuite/single/tests/efuns/save_object.c` | 1427 | 47 | 1 | R |
| `testsuite/single/tests/efuns/save_object_source.lpc` | 160 | 7 | 1 | R |
| `testsuite/single/tests/efuns/save_variable.c` | 723 | 37 | 1 | R |
| `testsuite/single/tests/efuns/say.c` | 707 | 33 | 1 | R |
| `testsuite/single/tests/efuns/secure_random.c` | 127 | 7 | 1 | R |
| `testsuite/single/tests/efuns/send_nullbyte.c` | 165 | 8 | 1 | R |
| `testsuite/single/tests/efuns/set_bit.c` | 276 | 15 | 1 | R |
| `testsuite/single/tests/efuns/set_bit_negative.lpc` | 1009 | 25 | 1 | R |
| `testsuite/single/tests/efuns/set_clean_up.c` | 1334 | 34 | 1 | R |
| `testsuite/single/tests/efuns/set_config.lpc` | 135 | 5 | 1 | R |
| `testsuite/single/tests/efuns/set_debug_level.c` | 273 | 14 | 1 | R |
| `testsuite/single/tests/efuns/set_eval_limit.lpc` | 149 | 7 | 1 | R |
| `testsuite/single/tests/efuns/set_heart_beat.c` | 2087 | 72 | 1 | R |
| `testsuite/single/tests/efuns/set_hide.c` | 354 | 17 | 1 | R |
| `testsuite/single/tests/efuns/set_light.c` | 533 | 41 | 1 | R |
| `testsuite/single/tests/efuns/set_living_name.c` | 432 | 20 | 1 | R |
| `testsuite/single/tests/efuns/set_notify_destruct.lpc` | 273 | 12 | 1 | R |
| `testsuite/single/tests/efuns/set_privs.c` | 200 | 9 | 1 | R |
| `testsuite/single/tests/efuns/sha1.c` | 2778 | 102 | 1 | R |
| `testsuite/single/tests/efuns/shadow.c` | 1032 | 51 | 1 | R |
| `testsuite/single/tests/efuns/shallow_inherit_list.lpc` | 249 | 6 | 1 | R |
| `testsuite/single/tests/efuns/shutdown.c` | 265 | 11 | 1 | R |
| `testsuite/single/tests/efuns/sin.lpc` | 255 | 8 | 1 | R |
| `testsuite/single/tests/efuns/sizeof.c` | 274 | 10 | 1 | R |
| `testsuite/single/tests/efuns/snoop.c` | 123 | 5 | 1 | R |
| `testsuite/single/tests/efuns/socket_accept.lpc` | 301 | 14 | 1 | R |
| `testsuite/single/tests/efuns/socket_acquire.lpc` | 173 | 9 | 1 | R |
| `testsuite/single/tests/efuns/socket_address.lpc` | 301 | 15 | 1 | R |
| `testsuite/single/tests/efuns/socket_binary_fragments.c` | 2489 | 105 | 1 | R |
| `testsuite/single/tests/efuns/socket_bind.lpc` | 291 | 15 | 1 | R |
| `testsuite/single/tests/efuns/socket_close.lpc` | 282 | 14 | 1 | R |
| `testsuite/single/tests/efuns/socket_connect.lpc` | 203 | 10 | 1 | R |
| `testsuite/single/tests/efuns/socket_create.lpc` | 334 | 15 | 1 | R |
| `testsuite/single/tests/efuns/socket_error.lpc` | 167 | 9 | 1 | R |
| `testsuite/single/tests/efuns/socket_get_option.lpc` | 3307 | 103 | 1 | R |
| `testsuite/single/tests/efuns/socket_listen.lpc` | 430 | 19 | 1 | R |
| `testsuite/single/tests/efuns/socket_release.lpc` | 569 | 22 | 1 | R |
| `testsuite/single/tests/efuns/socket_status.lpc` | 266 | 14 | 1 | R |
| `testsuite/single/tests/efuns/socket_text_close_reuse.c` | 3524 | 133 | 1 | R |
| `testsuite/single/tests/efuns/socket_text_eof.c` | 2414 | 99 | 1 | R |
| `testsuite/single/tests/efuns/socket_text_fragments.c` | 4133 | 170 | 1 | R |
| `testsuite/single/tests/efuns/socket_text_invalid.c` | 2389 | 99 | 1 | R |
| `testsuite/single/tests/efuns/socket_tls_server.c` | 3695 | 164 | 1 | R |
| `testsuite/single/tests/efuns/socket_tls_text_fragments.c` | 4741 | 182 | 1 | R |
| `testsuite/single/tests/efuns/socket_tls_verify_requires_nonempty_sni.lpc` | 1112 | 43 | 1 | R |
| `testsuite/single/tests/efuns/socket_write.lpc` | 256 | 13 | 1 | R |
| `testsuite/single/tests/efuns/sockets.c` | 852 | 43 | 1 | R |
| `testsuite/single/tests/efuns/sort_array.c` | 446 | 18 | 1 | R |
| `testsuite/single/tests/efuns/sprintf.c` | 9411 | 311 | 1 | R |
| `testsuite/single/tests/efuns/sprintf.lpc` | 11473 | 349 | 1 | R |
| `testsuite/single/tests/efuns/sprintf_column_object.lpc` | 1043 | 27 | 1 | R |
| `testsuite/single/tests/efuns/sqrt.lpc` | 227 | 7 | 1 | R |
| `testsuite/single/tests/efuns/sscanf.c` | 672 | 33 | 1 | R |
| `testsuite/single/tests/efuns/stat.lpc` | 1860 | 49 | 1 | R |
| `testsuite/single/tests/efuns/store_class_member.lpc` | 253 | 11 | 1 | R |
| `testsuite/single/tests/efuns/store_variable.c` | 771 | 38 | 1 | R |
| `testsuite/single/tests/efuns/strcmp.c` | 266 | 9 | 1 | R |
| `testsuite/single/tests/efuns/strftime.c` | 165 | 5 | 1 | R |
| `testsuite/single/tests/efuns/string_add_object.c` | 2060 | 61 | 1 | R |
| `testsuite/single/tests/efuns/string_ascii_cache.lpc` | 8027 | 192 | 1 | R |
| `testsuite/single/tests/efuns/string_decode.lpc` | 232 | 7 | 1 | R |
| `testsuite/single/tests/efuns/string_difference.lpc` | 252 | 8 | 1 | R |
| `testsuite/single/tests/efuns/string_encode.lpc` | 310 | 10 | 1 | R |
| `testsuite/single/tests/efuns/stringp.lpc` | 145 | 7 | 1 | R |
| `testsuite/single/tests/efuns/strptime.c` | 136 | 5 | 1 | R |
| `testsuite/single/tests/efuns/strsrch.c` | 1552 | 50 | 1 | R |
| `testsuite/single/tests/efuns/strsrch_reverse_egc.lpc` | 1127 | 24 | 1 | R |
| `testsuite/single/tests/efuns/strwidth.c` | 491 | 12 | 1 | R |
| `testsuite/single/tests/efuns/sys_network_ports.c` | 794 | 27 | 1 | R |
| `testsuite/single/tests/efuns/sys_reload_tls.c` | 954 | 41 | 1 | R |
| `testsuite/single/tests/efuns/sys_reload_tls.lpc` | 1251 | 48 | 1 | R |
| `testsuite/single/tests/efuns/talker.c` | 320 | 25 | 1 | R |
| `testsuite/single/tests/efuns/tan.lpc` | 192 | 6 | 1 | R |
| `testsuite/single/tests/efuns/terminal_colour.lpc` | 1439 | 35 | 1 | R |
| `testsuite/single/tests/efuns/test_bit.lpc` | 155 | 7 | 1 | R |
| `testsuite/single/tests/efuns/this_object.lpc` | 204 | 6 | 1 | R |
| `testsuite/single/tests/efuns/this_player.lpc` | 128 | 6 | 1 | R |
| `testsuite/single/tests/efuns/throw.lpc` | 202 | 7 | 1 | R |
| `testsuite/single/tests/efuns/time.c` | 112 | 10 | 1 | R |
| `testsuite/single/tests/efuns/time_ns.c` | 73 | 5 | 1 | R |
| `testsuite/single/tests/efuns/to_float.c` | 240 | 19 | 1 | R |
| `testsuite/single/tests/efuns/to_int.c` | 194 | 17 | 1 | R |
| `testsuite/single/tests/efuns/trace.c` | 44 | 4 | 1 | R |
| `testsuite/single/tests/efuns/trace_io.lpc` | 190 | 14 | 1 | R |
| `testsuite/single/tests/efuns/trace_overflow_recovery.lpc` | 1345 | 58 | 1 | R |
| `testsuite/single/tests/efuns/transcode.c` | 443 | 16 | 1 | R |
| `testsuite/single/tests/efuns/trim.lpc` | 543 | 16 | 1 | R |
| `testsuite/single/tests/efuns/typeof.lpc` | 398 | 13 | 1 | R |
| `testsuite/single/tests/efuns/uncompress.lpc` | 265 | 8 | 1 | R |
| `testsuite/single/tests/efuns/uncompress_file.lpc` | 382 | 13 | 1 | R |
| `testsuite/single/tests/efuns/uncompress_file_corrupt.lpc` | 760 | 29 | 1 | R |
| `testsuite/single/tests/efuns/uncompress_zipbomb.lpc` | 1644 | 37 | 1 | R |
| `testsuite/single/tests/efuns/undefinedp.lpc` | 238 | 10 | 1 | R |
| `testsuite/single/tests/efuns/unhandled_rejection_locus.lpc` | 2320 | 105 | 1 | R |
| `testsuite/single/tests/efuns/unique_mapping.c` | 1277 | 43 | 1 | R |
| `testsuite/single/tests/efuns/unloaded.c` | 17 | 2 | 1 | R |
| `testsuite/single/tests/efuns/upper_case.lpc` | 149 | 6 | 1 | R |
| `testsuite/single/tests/efuns/uptime.lpc` | 99 | 6 | 1 | R |
| `testsuite/single/tests/efuns/userp.lpc` | 102 | 5 | 1 | R |
| `testsuite/single/tests/efuns/users.c` | 124 | 6 | 1 | R |
| `testsuite/single/tests/efuns/values.c` | 164 | 11 | 1 | R |
| `testsuite/single/tests/efuns/virtual.c` | 461 | 19 | 1 | R |
| `testsuite/single/tests/efuns/virtualp.lpc` | 197 | 6 | 1 | R |
| `testsuite/single/tests/efuns/vowel.lpc` | 141 | 9 | 1 | R |
| `testsuite/single/tests/efuns/wizardp.lpc` | 60 | 4 | 1 | R |
| `testsuite/single/tests/efuns/worker_payload.c` | 1844 | 55 | 1 | R |
| `testsuite/single/tests/efuns/write_buffer.lpc` | 271 | 10 | 1 | R |
| `testsuite/single/tests/efuns/write_buffer_bounds.lpc` | 850 | 23 | 1 | R |
| `testsuite/single/tests/efuns/write_bytes.lpc` | 215 | 8 | 1 | R |
| `testsuite/single/tests/efuns/write_file.lpc` | 369 | 10 | 1 | R |
| `testsuite/single/tests/efuns/zonetime.lpc` | 147 | 7 | 1 | R |
| `testsuite/single/tests/operators/assign_specialized.lpc` | 2528 | 98 | 1 | R |
| `testsuite/single/tests/operators/buffer_range_assign.lpc` | 1278 | 46 | 1 | R |
| `testsuite/single/tests/operators/class.c` | 3148 | 105 | 1 | R |
| `testsuite/single/tests/operators/compound_assign_float.lpc` | 2029 | 116 | 1 | R |
| `testsuite/single/tests/operators/compound_assign_index_rhs.lpc` | 2079 | 55 | 1 | R |
| `testsuite/single/tests/operators/compound_assign_undefined.lpc` | 1831 | 83 | 1 | R |
| `testsuite/single/tests/operators/foreach.c` | 826 | 56 | 1 | R |
| `testsuite/single/tests/operators/foreach_large.lpc` | 1347 | 56 | 1 | R |
| `testsuite/single/tests/operators/foreach_ref_mapping.c` | 1170 | 37 | 1 | R |
| `testsuite/single/tests/operators/index_lvalue_alias.lpc` | 3840 | 174 | 1 | R |
| `testsuite/single/tests/operators/mapping.c` | 475 | 21 | 1 | R |
| `testsuite/single/tests/operators/range.c` | 502 | 18 | 1 | R |
| `testsuite/single/tests/operators/shift_mod64.lpc` | 1257 | 46 | 1 | R |
| `testsuite/single/tests/operators/string_index.c` | 3837 | 134 | 1 | R |
| `testsuite/single/tests/operators/switch.c` | 2465 | 134 | 1 | R |
| `testsuite/single/tests/operators/too_deep_recursion.c` | 255 | 17 | 1 | R |
| `testsuite/single/tests/std/base64.c` | 615 | 15 | 1 | R |
| `testsuite/single/tests/std/bitmap_font.c` | 373 | 14 | 1 | R |
| `testsuite/single/tests/std/break_string.c` | 1012 | 31 | 1 | R |
| `testsuite/single/tests/std/database.c` | 1991 | 55 | 1 | R |
| `testsuite/single/tests/std/element_of_weighted.c` | 346 | 18 | 1 | R |
| `testsuite/single/tests/std/highest.c` | 229 | 11 | 1 | R |
| `testsuite/single/tests/std/json.c` | 1127 | 45 | 1 | R |
| `testsuite/single/tests/std/lowest.c` | 224 | 11 | 1 | R |
| `testsuite/single/tests/std/number_string.c` | 939 | 28 | 1 | R |
| `testsuite/single/tests/std/percent.c` | 226 | 12 | 1 | R |
| `testsuite/single/tests/std/present_clone.c` | 1685 | 53 | 1 | R |
| `testsuite/single/tests/std/range.c` | 272 | 14 | 1 | R |
| `testsuite/single/tests/std/reduce.c` | 1290 | 53 | 1 | R |
| `testsuite/single/tests/std/sum.c` | 344 | 15 | 1 | R |
| `testsuite/single/tests/std/test.json` | 10239 | 402 | 1 | R |
| `testsuite/single/tests/std/test2.json` | 2322 | 79 | 1 | R |
| `testsuite/single/void.c` | 8913 | 307 | 1 | R |
| `testsuite/speed.py` | 5428 | 150 | 1 | R |
| `testsuite/speed_str.py` | 41900 | 1485 | 2 | R |
| `testsuite/std/all_environment.c` | 273 | 12 | 1 | R |
| `testsuite/std/base64.c` | 2442 | 114 | 1 | R |
| `testsuite/std/bitmap_font.c` | 2865 | 101 | 1 | R |
| `testsuite/std/break_string.c` | 2657 | 106 | 1 | R |
| `testsuite/std/break_string.h` | 263 | 12 | 1 | R |
| `testsuite/std/database.c` | 13435 | 757 | 1 | R |
| `testsuite/std/element_of_weighted.c` | 1247 | 40 | 1 | R |
| `testsuite/std/fonts/ASC12` | 3072 | — | — | M |
| `testsuite/std/fonts/ASC14` | 3584 | — | — | M |
| `testsuite/std/fonts/ASC16` | 4096 | — | — | M |
| `testsuite/std/fonts/HZK12` | 196272 | — | — | M |
| `testsuite/std/fonts/HZK14` | 228984 | — | — | M |
| `testsuite/std/fonts/HZK16C` | 261696 | — | — | M |
| `testsuite/std/fonts/HZK16F` | 261696 | — | — | M |
| `testsuite/std/fonts/HZK16H` | 261696 | — | — | M |
| `testsuite/std/fonts/HZK16K` | 261696 | — | — | M |
| `testsuite/std/fonts/HZK16L` | 261696 | — | — | M |
| `testsuite/std/fonts/HZK16S` | 261696 | — | — | M |
| `testsuite/std/fonts/HZK16V` | 261696 | — | — | M |
| `testsuite/std/fonts/HZK16X` | 261696 | — | — | M |
| `testsuite/std/fonts/HZK16Y` | 261696 | — | — | M |
| `testsuite/std/fonts/fonts.txt` | 259 | 8 | 1 | R |
| `testsuite/std/highest.c` | 724 | 28 | 1 | R |
| `testsuite/std/http.c` | 4094 | 129 | 1 | R |
| `testsuite/std/json.c` | 19410 | 658 | 1 | R |
| `testsuite/std/lowest.c` | 720 | 28 | 1 | R |
| `testsuite/std/number_string.c` | 1186 | 46 | 1 | R |
| `testsuite/std/percent.c` | 274 | 10 | 1 | R |
| `testsuite/std/present_clone.c` | 1331 | 47 | 1 | R |
| `testsuite/std/range.c` | 1195 | 39 | 1 | R |
| `testsuite/std/reduce.c` | 2158 | 62 | 1 | R |
| `testsuite/std/sum.c` | 773 | 38 | 1 | R |
| `testsuite/std/telnet.c` | 9820 | 317 | 1 | R |
| `testsuite/telnet_test.expect` | 570 | 45 | 1 | R |
| `testsuite/test.o` | 1086 | 6 | 1 | R |
| `testsuite/u/README` | 84 | 3 | 1 | R |
| `third_party/cyclonedx.schema.json` | 72334 | 1698 | 2 | R |
| `third_party/manifest.yaml` | 7337 | 189 | 1 | R / E:AUD-021 |
| `third_party/sbom.json` | 13111 | 556 | 1 | R |
| `tools/build/regenerate_grammar.py` | 3213 | 80 | 1 | R |
| `tools/build/test_generated_dependencies.py` | 5770 | 125 | 1 | R |
| `tools/build/test_regenerate_grammar.py` | 3009 | 69 | 1 | R |
| `tools/docs/check-actions-pins.py` | 10747 | 243 | 1 | R |
| `tools/docs/check-docs.py` | 9351 | 234 | 1 | R |
| `tools/docs/check-evidence.py` | 14555 | 351 | 1 | R |
| `tools/docs/check-workflows.py` | 19729 | 511 | 1 | R |
| `tools/docs/derive-cleanup.py` | 2794 | 79 | 1 | R |
| `tools/docs/test-evidence-gate.py` | 9694 | 237 | 1 | R |
| `tools/loadtest/xkx_gateway_loadtest.py` | 31842 | 814 | 1 | R |
| `tools/lpc-modern-runtime-stress.sh` | 4437 | 98 | 1 | R |
| `tools/lpc-syntax/.gitignore` | 14 | 2 | 1 | R |
| `tools/lpc-syntax/README.md` | 15614 | 166 | 1 | R |
| `tools/lpc-syntax/bin/format-corpus.mjs` | 7348 | 232 | 1 | R |
| `tools/lpc-syntax/docusaurus-plugin.cjs` | 1172 | 42 | 1 | R |
| `tools/lpc-syntax/format.mjs` | 94917 | 997 | 1 | R |
| `tools/lpc-syntax/generate_ebnf.py` | 22628 | 534 | 1 | R |
| `tools/lpc-syntax/grammar.ebnf` | 13549 | 425 | 1 | R |
| `tools/lpc-syntax/grammar_lexical.ebnf.in` | 5017 | 102 | 1 | R |
| `tools/lpc-syntax/highlight.mjs` | 1948 | 61 | 1 | R |
| `tools/lpc-syntax/lint.mjs` | 4027 | 126 | 1 | R |
| `tools/lpc-syntax/lpc-grammar.json` | 34815 | 2352 | 3 | R |
| `tools/lpc-syntax/lpc.tmLanguage.json` | 4675 | 186 | 1 | R |
| `tools/lpc-syntax/package.json` | 708 | 22 | 1 | R |
| `tools/lpc-syntax/prism-lpc.cjs` | 3582 | 125 | 1 | R |
| `tools/lpc-syntax/test-format-corpus.mjs` | 2873 | 83 | 1 | R |
| `tools/lpc-syntax/test.mjs` | 95814 | 864 | 1 | R |
| `tools/lpc-syntax/tokenizer.mjs` | 15460 | 402 | 1 | R |
| `tools/owner-runtime-v4-stress.sh` | 2702 | 74 | 1 | R |
| `tools/owner-scheduler-capacity.sh` | 6241 | 144 | 1 | R |
| `tools/perf/README.md` | 11759 | 188 | 1 | R |
| `tools/perf/compile_report.py` | 5898 | 116 | 1 | R |
| `tools/perf/corpus/s_arrays_00.c` | 2083 | 101 | 1 | R |
| `tools/perf/corpus/s_arrays_01.c` | 2083 | 101 | 1 | R |
| `tools/perf/corpus/s_arrays_02.c` | 2086 | 101 | 1 | R |
| `tools/perf/corpus/s_arrays_03.c` | 2085 | 101 | 1 | R |
| `tools/perf/corpus/s_arrays_04.c` | 2082 | 101 | 1 | R |
| `tools/perf/corpus/s_arrays_05.c` | 2089 | 101 | 1 | R |
| `tools/perf/corpus/s_arrays_06.c` | 2086 | 101 | 1 | R |
| `tools/perf/corpus/s_arrays_07.c` | 2091 | 101 | 1 | R |
| `tools/perf/corpus/s_arrays_08.c` | 2083 | 101 | 1 | R |
| `tools/perf/corpus/s_arrays_09.c` | 2086 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_00.c` | 1993 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_01.c` | 1991 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_02.c` | 1989 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_03.c` | 1990 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_04.c` | 1990 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_05.c` | 1989 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_06.c` | 1990 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_07.c` | 1988 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_08.c` | 1994 | 101 | 1 | R |
| `tools/perf/corpus/s_classes_09.c` | 1989 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_00.c` | 2301 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_01.c` | 2296 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_02.c` | 2297 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_03.c` | 2297 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_04.c` | 2300 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_05.c` | 2298 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_06.c` | 2300 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_07.c` | 2291 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_08.c` | 2290 | 101 | 1 | R |
| `tools/perf/corpus/s_functions_09.c` | 2299 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_00.c` | 1979 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_01.c` | 1974 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_02.c` | 1976 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_03.c` | 1982 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_04.c` | 1980 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_05.c` | 1983 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_06.c` | 1975 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_07.c` | 1976 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_08.c` | 1979 | 101 | 1 | R |
| `tools/perf/corpus/s_loops_09.c` | 1979 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_00.c` | 2258 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_01.c` | 2257 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_02.c` | 2255 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_03.c` | 2256 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_04.c` | 2260 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_05.c` | 2255 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_06.c` | 2256 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_07.c` | 2252 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_08.c` | 2248 | 101 | 1 | R |
| `tools/perf/corpus/s_macros_09.c` | 2249 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_00.c` | 2120 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_01.c` | 2131 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_02.c` | 2120 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_03.c` | 2132 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_04.c` | 2126 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_05.c` | 2123 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_06.c` | 2128 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_07.c` | 2123 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_08.c` | 2127 | 101 | 1 | R |
| `tools/perf/corpus/s_mappings_09.c` | 2128 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_00.c` | 2162 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_01.c` | 2167 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_02.c` | 2163 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_03.c` | 2164 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_04.c` | 2169 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_05.c` | 2162 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_06.c` | 2164 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_07.c` | 2167 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_08.c` | 2165 | 101 | 1 | R |
| `tools/perf/corpus/s_mixed_09.c` | 2168 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_00.c` | 2128 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_01.c` | 2127 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_02.c` | 2122 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_03.c` | 2125 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_04.c` | 2124 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_05.c` | 2121 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_06.c` | 2121 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_07.c` | 2126 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_08.c` | 2125 | 101 | 1 | R |
| `tools/perf/corpus/s_objects_09.c` | 2123 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_00.c` | 2452 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_01.c` | 2455 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_02.c` | 2457 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_03.c` | 2457 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_04.c` | 2454 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_05.c` | 2459 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_06.c` | 2457 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_07.c` | 2454 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_08.c` | 2460 | 101 | 1 | R |
| `tools/perf/corpus/s_strings_09.c` | 2457 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_00.c` | 2039 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_01.c` | 2038 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_02.c` | 2036 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_03.c` | 2035 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_04.c` | 2032 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_05.c` | 2033 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_06.c` | 2043 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_07.c` | 2040 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_08.c` | 2039 | 101 | 1 | R |
| `tools/perf/corpus/s_switch_09.c` | 2033 | 101 | 1 | R |
| `tools/perf/gen_compile_corpus.py` | 7451 | 250 | 1 | R |
| `tools/perf/measure_explode.py` | 3385 | 74 | 1 | R |
| `tools/perf/measure_object_store.py` | 8402 | 153 | 1 | R |
| `tools/perf/measurement.py` | 2325 | 60 | 1 | R |
| `tools/perf/official_compare/analyze_official_vs_xk.py` | 4348 | 104 | 1 | R |
| `tools/perf/official_compare/portable_fluffos_loadtest.py` | 12028 | 291 | 1 | R |
| `tools/perf/official_compare/portable_microbench.py` | 11424 | 356 | 1 | R |
| `tools/perf/official_compare/portable_mudlib/clone/login.c` | 316 | 18 | 1 | R |
| `tools/perf/official_compare/portable_mudlib/clone/user.c` | 673 | 45 | 1 | R |
| `tools/perf/official_compare/portable_mudlib/etc/config.template` | 986 | 40 | 1 | R |
| `tools/perf/official_compare/portable_mudlib/etc/motd` | 35 | 2 | 1 | R |
| `tools/perf/official_compare/portable_mudlib/single/master.c` | 2076 | 88 | 1 | R |
| `tools/perf/official_compare/portable_mudlib/single/simul_efun.c` | 17 | 2 | 1 | R |
| `tools/perf/official_compare/portable_mudlib/std/bench_clone.c` | 100 | 12 | 1 | R |
| `tools/perf/official_compare/portable_mudlib/std/bench_target.c` | 1975 | 116 | 1 | R |
| `tools/perf/official_compare/run_official_vs_xk.py` | 21175 | 555 | 1 | R |
| `tools/perf/official_compare/run_perf_truth_gate.py` | 17898 | 488 | 1 | R |
| `tools/perf/results/after/compile2-1.json` | 1789 | 23 | 1 | R |
| `tools/perf/results/after/compile2-2.json` | 1783 | 23 | 1 | R |
| `tools/perf/results/after/compile2-3.json` | 1776 | 23 | 1 | R |
| `tools/perf/results/after/compile2-4.json` | 1792 | 23 | 1 | R |
| `tools/perf/results/after/compile2-5.json` | 1793 | 23 | 1 | R |
| `tools/perf/results/before/compile2-1.json` | 1545 | 13 | 1 | R |
| `tools/perf/results/before/compile2-2.json` | 1540 | 13 | 1 | R |
| `tools/perf/results/before/compile2-3.json` | 1535 | 13 | 1 | R |
| `tools/perf/results/before/compile2-4.json` | 1548 | 13 | 1 | R |
| `tools/perf/results/before/compile2-5.json` | 1548 | 13 | 1 | R |
| `tools/perf/test_compile_report.py` | 11129 | 262 | 1 | R |
| `tools/perf/test_object_store_bench.py` | 4152 | 93 | 1 | R |
| `tools/perf/tests/test_official_compare_contract.py` | 14016 | 370 | 1 | R |
| `tools/sbom-generate.py` | 8319 | 221 | 1 | R / E:AUD-021 |
| `tools/style/check-format.py` | 21815 | 640 | 1 | R |
| `tools/style/test-check-format.py` | 13080 | 375 | 1 | R |
| `tools/testsuite/README.md` | 12327 | 184 | 1 | R |
| `tools/testsuite/portable-timeout.py` | 2781 | 98 | 1 | R |
| `tools/testsuite/run-isolated.sh` | 11966 | 364 | 1 | R |
| `tools/testsuite/run-targeted.py` | 44967 | 1041 | 2 | R |
| `tools/testsuite/test-compound-free.py` | 3377 | 84 | 1 | R |
| `tools/testsuite/test-global-include.py` | 5670 | 131 | 1 | R |
| `tools/testsuite/test-lpcc-io.py` | 5243 | 136 | 1 | R |
| `tools/testsuite/test-port-isolation.sh` | 4473 | 119 | 1 | R |
| `tools/testsuite/test-run-isolated-portability.py` | 5118 | 154 | 1 | R |
| `tools/testsuite/test-run-targeted.py` | 28863 | 586 | 1 | R |
| `tools/testsuite/test-trace-io.py` | 5798 | 134 | 1 | R |
| `tools/testsuite/test-websocket.py` | 26434 | 639 | 1 | R |
| `tools/wsl-cmake-build.sh` | 258 | 17 | 1 | R |

### A.2 非文本与格式例外的身份核对

下列 46 项包含 39 个非文本文件和 7 个原始字节例外。SHA-256 于 2026-10-12T05:57:49+00:00 只读重算；模式是本地文件权限，不是新的权限设置。行数/分块读取不用于声称二进制语义覆盖。

| 路径 | 类别 / MIME | 字节 | 模式 | SHA-256 | 边界 |
| --- | --- | ---: | --- | --- | --- |
| `docs/evidence/asan-ftest-a1-a2-2026-08-19.txt` | raw-text / `text/plain` | 1811307 | `0664` | `69bc0c9d281d8f002d22bc968772dcb59500cca67d3aea536199815d9c5a6551` | UTF-8 解码不完整；按原始字节保留 |
| `docs/evidence/asan-ftest-final.txt` | raw-text / `text/plain` | 1752959 | `0664` | `6a944a7ffb7db0c4c170c15c3ece0cdd5a851bfdf7d84599fdaad537fd3ddd78` | UTF-8 解码不完整；按原始字节保留 |
| `docs/evidence/asan-ftest-leaks-full.txt` | raw-text / `text/plain` | 1786058 | `0664` | `baf8ea00f1ec8ca47ebbcbac42e71c838a0521339cdf4807a19db71525370d75` | UTF-8 解码不完整；按原始字节保留 |
| `docs/evidence/ftest-final-2026-08-19.txt` | raw-text / `text/plain` | 1811409 | `0664` | `3a942635c2259225e89e8b6b64cbc18eacac00ca50f34922a66c0d9f93b739e3` | UTF-8 解码不完整；按原始字节保留 |
| `docs/evidence/l2-ftest-buildsync.txt` | raw-text / `text/plain` | 1752759 | `0664` | `5121aa4fdbdc08f2ffc0271889517ddc00de55e4de5ffb5bcd1e8d4413ffdb02` | UTF-8 解码不完整；按原始字节保留 |
| `docs/evidence/l7-ftest-regression.txt` | raw-text / `text/plain` | 1752798 | `0664` | `5b01d87bee8180887260e65d217dc78e62dd7569d6a22f8e01e3324a66a8159d` | UTF-8 解码不完整；按原始字节保留 |
| `src/tests/fuzz/corpus/gateway/coalesced_valid` | non-text / `application/octet-stream` | 126 | `0644` | `313d830a8b60164628e118ec5d8c84b60e207dbbab6b13142f13281affdb38a6` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/tests/fuzz/corpus/gateway/invalid_json` | non-text / `application/octet-stream` | 25 | `0644` | `1e8785503c90fc3d30aaa0ef02aee51e59c9bb793ebe788036ef55e2e5be056f` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/tests/fuzz/corpus/gateway/oversized_header` | raw-text / `text/plain` | 36 | `0644` | `44ceaca0476e8ab026f92789c0f15f0f38ab8c9b0187a4df9cf58379848f70ac` | 含 NUL；按原始字节保留 |
| `src/tests/fuzz/corpus/gateway/truncated_frame` | non-text / `application/octet-stream` | 31 | `0644` | `7e8c780630c29fe31517669dcc26318f05e1017ec5de0d7ef963a0a6dca852fa` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/tests/fuzz/corpus/gateway/valid_data_frame` | non-text / `application/octet-stream` | 63 | `0644` | `c0b01165cffd33200203d8f883722bd3f638fcaea2803b93d778b14722f4f5d5` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/tests/fuzz/corpus/gateway/valid_fragmented_a` | non-text / `application/octet-stream` | 31 | `0644` | `7e8c780630c29fe31517669dcc26318f05e1017ec5de0d7ef963a0a6dca852fa` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/backward-cpp/doc/nice.png` | non-text / `image/png` | 40904 | `0777` | `3d43d7e767460a67f3dfe061a581ecc1a4206caf7cd4a0f788f3a4ee39e51581` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/backward-cpp/doc/pretty.png` | non-text / `image/png` | 105498 | `0777` | `a3e1eb7c6b0f76d375b51806776f9a94dc387022940d00fab1574a9fc5f06a94` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/backward-cpp/doc/rude.png` | non-text / `image/png` | 6183 | `0777` | `b76f7855c8fddb392d9d5de5e2b298937b4439e0fd43e2038b29be7147af538e` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/_static/fonts/glyphicons-halflings-regular.eot` | non-text / `application/vnd.ms-fontobject` | 20335 | `0777` | `f495f34e4f177cf0115af995bbbfeb3fcabc88502876e76fc51a4ab439bc8431` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/_static/fonts/glyphicons-halflings-regular.ttf` | non-text / `font/sfnt` | 41280 | `0777` | `bd18efd3efd70fec8ad09611a20cdbf99440b2c1d40085c29be036f891d65358` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/_static/fonts/glyphicons-halflings-regular.woff` | non-text / `font/woff` | 23320 | `0777` | `fc969dc1c6ff531abcf368089dcbaf5775133b0626ff56b52301a059fc0f9e1e` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/html/_static/file.png` | non-text / `image/png` | 286 | `0777` | `5c4bc9a16aebf38c4b950f59b8e501ca36495328cb9eb622218bce9064a35e3e` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/html/_static/fonts/glyphicons-halflings-regular.eot` | non-text / `application/vnd.ms-fontobject` | 20335 | `0777` | `f495f34e4f177cf0115af995bbbfeb3fcabc88502876e76fc51a4ab439bc8431` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/html/_static/fonts/glyphicons-halflings-regular.ttf` | non-text / `font/sfnt` | 41280 | `0777` | `bd18efd3efd70fec8ad09611a20cdbf99440b2c1d40085c29be036f891d65358` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/html/_static/fonts/glyphicons-halflings-regular.woff` | non-text / `font/woff` | 23320 | `0777` | `fc969dc1c6ff531abcf368089dcbaf5775133b0626ff56b52301a059fc0f9e1e` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/html/_static/minus.png` | non-text / `image/png` | 90 | `0777` | `47e7fc50db3699f1ca41ce9a2ffa202c00c5d1d5180c55f62ba859b1bd6cc008` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/html/_static/plus.png` | non-text / `image/png` | 90 | `0777` | `54115199b96a130cba02147c47c0deb43dcc9b9f08b5162bba8642b34980ac63` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/fmt/doc/html/objects.inv` | non-text / `application/octet-stream` | 5698 | `0777` | `b98559d4091d0fcf43b463b90f6d85a4d5dbfd5c5cf60653ff642d52b567cb48` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/libwebsockets/plugins/deaddrop/assets/BungeeTint-Regular.ttf` | non-text / `font/sfnt` | 217004 | `0664` | `87331f23bbadd408bed375551c5e7e3306faf5d0cd659ceb25d8efc1d3a6b795` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/libwebsockets/plugins/deaddrop/assets/deaddrop-title.png` | non-text / `image/png` | 14529 | `0664` | `e61aa51fbfa70678755d54fd3c77e826336741d11894fb2627865627b46eed16` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/libwebsockets/plugins/lwsws-logo.png` | non-text / `image/png` | 5172 | `0664` | `c5e7e38abdf78485cd83616ae513dcadcabd34529d7e446efaca8d93025a500e` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `src/thirdparty/utfcpp/tests/test_data/utf8_invalid.txt` | non-text / `application/octet-stream` | 20010 | `0777` | `8801a72c12264db5dce15b0120c082176b745be08681b60017ea7bb9c09fc316` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/data/compress/zipbomb.zbin` | non-text / `application/zlib` | 1960 | `0664` | `bf0e15e2687f9366f9b12504c54890081c0d432863a5847cf9946cd56a69c590` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/single/tests/compiler/fail/bad_utf8_arrayblock.c` | non-text / `application/octet-stream` | 22869 | `0777` | `67aac70528b003b2b469a6d10a4231b802888850b38ef7c6b5fdd7f73a5a55bf` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/single/tests/compiler/fail/bad_utf8_string.c` | non-text / `application/octet-stream` | 22867 | `0777` | `d282eedd298e2d59a9a1897762c1f41d65b3bf399a6b5b78b06b8363b76d8b81` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/ASC12` | non-text / `application/octet-stream` | 3072 | `0777` | `7c7d5b477a6dc93c5a1e46865452887b3e608b02c3eec749495c927df91ba52f` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/ASC14` | non-text / `application/octet-stream` | 3584 | `0777` | `315ac15dd3c7429d8a0d3a6039df4e5e4b51eaaa22d3e413c2ab601cd92f8fc3` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/ASC16` | non-text / `application/octet-stream` | 4096 | `0777` | `a8bad6fd78475a6bc2a05438c19207a9bb8c0f4f4099f60384e158cdc3eba580` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK12` | non-text / `application/octet-stream` | 196272 | `0777` | `d83b7f1603e87dd68bebc3a6a159c57909779684188cba790146f59424c28a02` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK14` | non-text / `application/octet-stream` | 228984 | `0777` | `aa393fb0402103e332b5a208b0207e310fdd29a90b99ec96c243d8ef22715848` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK16C` | non-text / `application/octet-stream` | 261696 | `0777` | `05eb2a664fad93cc31f3900a9c9c67048038f52eb8e8b57cf9cca2e15d30ec59` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK16F` | non-text / `application/octet-stream` | 261696 | `0777` | `5e3ead1b59be70cf3b800db21b8cd9f425c82f85918e74f743f6b2460edb54e0` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK16H` | non-text / `application/octet-stream` | 261696 | `0777` | `7dfb04d43fcdf48309a9fc61086dc59395d6a7aabdd74a0bfe36249c6dca309d` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK16K` | non-text / `application/octet-stream` | 261696 | `0777` | `956f4eff7978ccbe949a496831795e734db8ee9f5b42ab8dfeaa6c03efcac190` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK16L` | non-text / `application/octet-stream` | 261696 | `0777` | `956f4eff7978ccbe949a496831795e734db8ee9f5b42ab8dfeaa6c03efcac190` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK16S` | non-text / `application/octet-stream` | 261696 | `0777` | `9989af5f8ab5426278b20ded8deb4d33dbfce0a3d5c3af16d46616db1aa2cf97` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK16V` | non-text / `application/octet-stream` | 261696 | `0777` | `0cd8bf4fad112ebbb0c5c961c28c2b25e388020a37d0e9f3afc2406f07788254` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK16X` | non-text / `application/octet-stream` | 261696 | `0777` | `89c96c69413c51b729f196ec320fa4b9b3290cfe2446024917dc3de650bda8f5` | 非文本：仅元数据/身份核验，未逆向或执行 |
| `testsuite/std/fonts/HZK16Y` | non-text / `application/octet-stream` | 261696 | `0777` | `4ac09fde37ce68df1f17f3bdee31749793c0a0de4f295a856c9474c29d7e6166` | 非文本：仅元数据/身份核验，未逆向或执行 |

## 附录 B. Fork 相对基线的完整文件差异

范围为 `277d0b1cbc4350844d9aff07a604dfa519650d9e` → `a4475b1afe998bb03a37bdb0596b64ab01042ed3`，使用 Git 默认 rename 识别，共 2,710 个记录。`A/M/D/R` 是 Git 文件状态，不是问题状态；`-` 表示 Git 不能给出文本增删行数。另述无 rename 的模块统计不能与本表混作同一口径。

| 状态 | 当前/删除路径 | rename 原路径 | 新增行 | 删除行 |
| --- | --- | --- | ---: | ---: |
| A | `.dockerignore` | — | 3 | 0 |
| M | `.editorconfig` | — | 3 | 0 |
| M | `.gitattributes` | — | 7 | 0 |
| M | `.github/workflows/ci.yml` | — | 344 | 25 |
| M | `.github/workflows/codeql-analysis.yml` | — | 8 | 7 |
| M | `.github/workflows/coverity-scan.yml` | — | 28 | 20 |
| D | `.github/workflows/docker-publish.yml` | — | 0 | 54 |
| A | `.github/workflows/docker-smoke.yml` | — | 43 | 0 |
| M | `.github/workflows/gh-pages.yml` | — | 8 | 11 |
| D | `.github/workflows/release.yml` | — | 0 | 373 |
| M | `.gitignore` | — | 11 | 0 |
| A | `.node-version` | — | 1 | 0 |
| A | `AGENTS.md` | — | 215 | 0 |
| A | `CHANGELOG.md` | — | 52 | 0 |
| D | `CLAUDE.md` | — | 0 | 392 |
| A | `CMakePresets.json` | — | 106 | 0 |
| A | `CODE_OF_CONDUCT.md` | — | 74 | 0 |
| A | `CONTRIBUTING.md` | — | 54 | 0 |
| M | `Dockerfile` | — | 18 | 6 |
| A | `LICENSE` | — | 14 | 0 |
| A | `NOTICE` | — | 36 | 0 |
| D | `QWEN.md` | — | 0 | 206 |
| M | `README.md` | — | 258 | 149 |
| M | `README_CN.md` | — | 248 | 142 |
| A | `SECURITY.md` | — | 66 | 0 |
| D | `TODO.md` | — | 0 | 55 |
| M | `cmake/FindPCRE.cmake` | — | 90 | 36 |
| M | `compat/parser_error.c` | — | 47 | 49 |
| M | `compat/simuls/apply.c` | — | 4 | 4 |
| M | `compat/simuls/break_string.c` | — | 21 | 16 |
| M | `compat/simuls/dump_socket_status.c` | — | 6 | 8 |
| M | `compat/simuls/parse_command.c` | — | 499 | 523 |
| M | `compat/simuls/process_string.c` | — | 28 | 28 |
| M | `compat/simuls/tail.c` | — | 34 | 30 |
| M | `docs/.vitepress/config.mts` | — | 5 | 5 |
| M | `docs/.vitepress/sidebar.ts` | — | 3 | 1 |
| D | `docs/CLAUDE.md` | — | 0 | 538 |
| M | `docs/apply/index.md` | — | 1 | 0 |
| M | `docs/apply/master/index.md` | — | 1 | 0 |
| A | `docs/apply/master/prepare_shutdown.md` | — | 32 | 0 |
| A | `docs/archive/multicore/README.md` | — | 13 | 0 |
| A | `docs/archive/multicore/multicore-actor-vm-plan-2026-06.md` | — | 439 | 0 |
| A | `docs/archive/multicore/multicore-refactor-execution-plan-2026-06.md` | — | 672 | 0 |
| A | `docs/archive/multicore/multicore-runtime-v1-2026-06.md` | — | 234 | 0 |
| A | `docs/backlog-full-plan-2026-09.md` | — | 323 | 0 |
| A | `docs/backlog-plan-2026-09.md` | — | 113 | 0 |
| M | `docs/build.md` | — | 59 | 10 |
| M | `docs/cli/lpcc.md` | — | 86 | 3 |
| A | `docs/coding-standard.md` | — | 319 | 0 |
| A | `docs/concepts/general/async.md` | — | 83 | 0 |
| M | `docs/concepts/general/index.md` | — | 1 | 0 |
| M | `docs/driver/config.md` | — | 5 | 0 |
| A | `docs/e4-read-source-line-port-eval-2026-08.md` | — | 78 | 0 |
| M | `docs/efun/arrays/member_array.md` | — | 10 | 1 |
| M | `docs/efun/calls/call_out_walltime.md` | — | 2 | 2 |
| A | `docs/efun/calls/call_out_walltime_background.md` | — | 27 | 0 |
| A | `docs/efun/calls/call_out_walltime_gateway.md` | — | 27 | 0 |
| M | `docs/efun/calls/index.md` | — | 2 | 0 |
| M | `docs/efun/calls/remove_call_out.md` | — | 2 | 2 |
| M | `docs/efun/contrib/file_length.md` | — | 2 | 2 |
| M | `docs/efun/ed/ed_cmd.md` | — | 4 | 0 |
| A | `docs/efun/external/external_close.md` | — | 23 | 0 |
| A | `docs/efun/external/external_close_stdin.md` | — | 25 | 0 |
| A | `docs/efun/external/external_create.md` | — | 44 | 0 |
| A | `docs/efun/external/external_exit_code.md` | — | 24 | 0 |
| A | `docs/efun/external/external_kill.md` | — | 43 | 0 |
| A | `docs/efun/external/external_run.md` | — | 54 | 0 |
| M | `docs/efun/external/external_start.md` | — | 46 | 5 |
| A | `docs/efun/external/external_stderr.md` | — | 21 | 0 |
| A | `docs/efun/external/external_stdout.md` | — | 25 | 0 |
| A | `docs/efun/external/external_write.md` | — | 41 | 0 |
| D | `docs/efun/external/index.md` | — | 0 | 5 |
| M | `docs/efun/filesystem/index.md` | — | 2 | 0 |
| M | `docs/efun/filesystem/read_bytes.md` | — | 7 | 4 |
| M | `docs/efun/filesystem/read_file.md` | — | 4 | 3 |
| A | `docs/efun/filesystem/read_json.md` | — | 30 | 0 |
| M | `docs/efun/filesystem/write_bytes.md` | — | 2 | 1 |
| M | `docs/efun/filesystem/write_file.md` | — | 8 | 0 |
| A | `docs/efun/filesystem/write_json.md` | — | 29 | 0 |
| M | `docs/efun/general/compress_file.md` | — | 20 | 6 |
| M | `docs/efun/general/index.md` | — | 2 | 0 |
| A | `docs/efun/general/json_encode_frozen.md` | — | 31 | 0 |
| A | `docs/efun/general/json_encode_frozen_or_zero.md` | — | 30 | 0 |
| M | `docs/efun/general/uncompress_file.md` | — | 24 | 6 |
| M | `docs/efun/index.md` | — | 7 | 0 |
| M | `docs/efun/interactive/ed.md` | — | 5 | 0 |
| M | `docs/efun/interactive/set_encoding.md` | — | 26 | 6 |
| M | `docs/efun/mudlib/domain_stats.md` | — | 8 | 0 |
| A | `docs/efun/objects/recompile_object.md` | — | 70 | 0 |
| M | `docs/efun/objects/reload_object.md` | — | 4 | 0 |
| M | `docs/efun/objects/restore_object.md` | — | 5 | 0 |
| M | `docs/efun/objects/save_object.md` | — | 10 | 1 |
| M | `docs/efun/pcre/index.md` | — | 6 | 0 |
| M | `docs/efun/pcre/pcre_version.md` | — | 4 | 0 |
| A | `docs/efun/promises/promise_reject.md` | — | 55 | 0 |
| M | `docs/efun/sockets/socket_create.md` | — | 2 | 2 |
| M | `docs/efun/sockets/socket_get_option.md` | — | 23 | 6 |
| M | `docs/efun/sockets/socket_set_option.md` | — | 3 | 2 |
| M | `docs/efun/strings/ltrim.md` | — | 4 | 1 |
| M | `docs/efun/strings/rtrim.md` | — | 4 | 1 |
| M | `docs/efun/strings/trim.md` | — | 7 | 2 |
| M | `docs/efun/system/find_call_out.md` | — | 4 | 2 |
| A | `docs/efun/system/include_list.md` | — | 25 | 0 |
| A | `docs/efun/system/request_clean_up.md` | — | 27 | 0 |
| A | `docs/evidence/asan-async-lsan.txt` | — | 429 | 0 |
| A | `docs/evidence/asan-ftest-a1-a2-2026-08-19.txt` | — | 52417 | 0 |
| A | `docs/evidence/asan-ftest-final.txt` | — | 51488 | 0 |
| A | `docs/evidence/asan-ftest-leaks-full.txt` | — | 51775 | 0 |
| A | `docs/evidence/asan-osenv-lsan.txt` | — | 208 | 0 |
| A | `docs/evidence/asan-ownercontract-lsan.txt` | — | 208 | 0 |
| A | `docs/evidence/asan-recompile-final.txt` | — | 208 | 0 |
| A | `docs/evidence/asan-sockettls-lsan.txt` | — | 5258 | 0 |
| A | `docs/evidence/e2-afl-compile-fuzzer_stats.txt` | — | 43 | 0 |
| R100 | `docs/evidence/e2-afl-compile.stderr` | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/mount-origin/error.css` | 0 | 0 |
| A | `docs/evidence/e2-afl-compile.stdout` | — | 69 | 0 |
| A | `docs/evidence/e2-afl-restore-fuzzer_stats.txt` | — | 43 | 0 |
| R100 | `docs/evidence/e2-afl-restore.stderr` | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/libs/placeholder` | 0 | 0 |
| A | `docs/evidence/e2-afl-restore.stdout` | — | 74 | 0 |
| A | `docs/evidence/e2-fuzz-compile-invalid.stderr` | — | 4 | 0 |
| A | `docs/evidence/e2-fuzz-compile-invalid.stdout` | — | 28 | 0 |
| A | `docs/evidence/e2-fuzz-compile-missing.stderr` | — | 1 | 0 |
| A | `docs/evidence/e2-fuzz-compile-missing.stdout` | — | 19 | 0 |
| A | `docs/evidence/e2-fuzz-compile-mixed.stderr` | — | 2 | 0 |
| A | `docs/evidence/e2-fuzz-compile-mixed.stdout` | — | 25 | 0 |
| A | `docs/evidence/e2-fuzz-compile-valid.stderr` | — | 1 | 0 |
| A | `docs/evidence/e2-fuzz-compile-valid.stdout` | — | 19 | 0 |
| A | `docs/evidence/e2-fuzz-restore-invalid.stderr` | — | 1 | 0 |
| A | `docs/evidence/e2-fuzz-restore-invalid.stdout` | — | 19 | 0 |
| A | `docs/evidence/e2-fuzz-restore-missing.stderr` | — | 1 | 0 |
| A | `docs/evidence/e2-fuzz-restore-missing.stdout` | — | 19 | 0 |
| A | `docs/evidence/e2-fuzz-restore-mixed.stderr` | — | 1 | 0 |
| A | `docs/evidence/e2-fuzz-restore-mixed.stdout` | — | 19 | 0 |
| A | `docs/evidence/e2-fuzz-restore-nested.stderr` | — | 1 | 0 |
| A | `docs/evidence/e2-fuzz-restore-nested.stdout` | — | 19 | 0 |
| A | `docs/evidence/e2-fuzz-restore-nested_mapping_error.stderr` | — | 1 | 0 |
| A | `docs/evidence/e2-fuzz-restore-nested_mapping_error.stdout` | — | 19 | 0 |
| A | `docs/evidence/e2-fuzz-restore-valid.stderr` | — | 1 | 0 |
| A | `docs/evidence/e2-fuzz-restore-valid.stdout` | — | 19 | 0 |
| A | `docs/evidence/e2-fuzzing.md` | — | 40 | 0 |
| A | `docs/evidence/e3-v2-phase1-simul-efun-reload.md` | — | 46 | 0 |
| A | `docs/evidence/ftest-final-2026-08-19.txt` | — | 52860 | 0 |
| A | `docs/evidence/l2-ftest-buildsync.txt` | — | 51481 | 0 |
| A | `docs/evidence/l2-lpc-tests-buildsync.txt` | — | 3435 | 0 |
| A | `docs/evidence/l2-lpc-tests-final.txt` | — | 3435 | 0 |
| A | `docs/evidence/l2-recompile-check.txt` | — | 209 | 0 |
| A | `docs/evidence/l2-tools-bench-build.txt` | — | 245 | 0 |
| A | `docs/evidence/l5-ctest.txt` | — | 853 | 0 |
| A | `docs/evidence/l5-lpcc-smoke.txt` | — | 60 | 0 |
| A | `docs/evidence/l5-ofile-tests.txt` | — | 145 | 0 |
| A | `docs/evidence/l6-tsan-driver-owner.txt` | — | 208 | 0 |
| A | `docs/evidence/l6-tsan-driver-recompile.txt` | — | 208 | 0 |
| A | `docs/evidence/l6-tsan-full.txt` | — | 3435 | 0 |
| A | `docs/evidence/l6-tsan-owner-first.txt` | — | 512 | 0 |
| A | `docs/evidence/l6-tsan-summary.md` | — | 38 | 0 |
| A | `docs/evidence/l7-bench-lpc_vm_bench.txt` | — | 5 | 0 |
| A | `docs/evidence/l7-bench-object_store_bench.txt` | — | 5 | 0 |
| A | `docs/evidence/l7-bench-owner_runtime_bench.txt` | — | 5 | 0 |
| A | `docs/evidence/l7-ftest-regression.txt` | — | 51482 | 0 |
| A | `docs/evidence/l7-master-reload-rounds.md` | — | 49 | 0 |
| A | `docs/evidence/l7-stress-asan.txt` | — | 209 | 0 |
| A | `docs/evidence/lpc-tests-asan-final-2026-08-19.txt` | — | 3598 | 0 |
| A | `docs/evidence/lpc-tests-final-2026-08-19.txt` | — | 3471 | 0 |
| A | `docs/evidence/lpc-tests-tsan-final-2026-08-19.txt` | — | 3471 | 0 |
| A | `docs/evidence/manifest.schema.json` | — | 143 | 0 |
| A | `docs/evidence/s16-ws-wss-smoke.md` | — | 104 | 0 |
| A | `docs/evidence/t3-1-scratch-arena.md` | — | 66 | 0 |
| A | `docs/evidence/t3-2-structured-diagnostics.md` | — | 69 | 0 |
| A | `docs/evidence/t3-3-diagnostic-rendering.md` | — | 68 | 0 |
| A | `docs/evidence/t3-4-lpcshell.md` | — | 61 | 0 |
| A | `docs/evidence/ubsan-enumerate.txt` | — | 3435 | 0 |
| A | `docs/evidence/ubsan-fnonull-probe.txt` | — | 25 | 0 |
| A | `docs/evidence/ubsan-o0-probe.txt` | — | 25 | 0 |
| A | `docs/evidence/ubsan-tlsmodel-exp.txt` | — | 26 | 0 |
| A | `docs/external-required-readiness-2026-09.md` | — | 108 | 0 |
| A | `docs/fluffos-xk-overview.md` | — | 181 | 0 |
| M | `docs/index.md` | — | 89 | 50 |
| A | `docs/lpc-modern-runtime.md` | — | 241 | 0 |
| M | `docs/lpc/constructs/inherit.md` | — | 6 | 0 |
| M | `docs/lpc/index.md` | — | 2 | 0 |
| A | `docs/lpc/source-files.md` | — | 41 | 0 |
| A | `docs/lpc/types/promise.md` | — | 85 | 0 |
| A | `docs/lpcshell-prerequisite-plan-2026-08.md` | — | 95 | 0 |
| A | `docs/multicore-production-gate.md` | — | 131 | 0 |
| A | `docs/multicore-runtime-v2.md` | — | 46 | 0 |
| A | `docs/multicore-runtime-v4.md` | — | 323 | 0 |
| A | `docs/multicore-runtime.md` | — | 29 | 0 |
| A | `docs/optimization-master-plan-2026-08-18.md` | — | 467 | 0 |
| A | `docs/owner-multicore-api.md` | — | 304 | 0 |
| M | `docs/package-lock.json` | — | 2051 | 15654 |
| M | `docs/package.json` | — | 13 | 15 |
| A | `docs/project-knowledge-map.md` | — | 504 | 0 |
| A | `docs/project-modernization-plan-2026-10.md` | — | 1943 | 0 |
| A | `docs/project-scope.md` | — | 90 | 0 |
| A | `docs/recompile-followup-plan-2026-08.md` | — | 298 | 0 |
| A | `docs/recompile-object-special-design-2026-08.md` | — | 1001 | 0 |
| A | `docs/recompile-object-v2-design-2026-08.md` | — | 171 | 0 |
| A | `docs/reports/capacity-evidence-2026-08-11/lpc_vm_bench_capacity.json` | — | 115 | 0 |
| A | `docs/reports/capacity-evidence-2026-08-11/lpc_vm_bench_raw.json` | — | 87 | 0 |
| A | `docs/reports/capacity-evidence-2026-08-11/object_store_bench_capacity.json` | — | 115 | 0 |
| A | `docs/reports/capacity-evidence-2026-08-11/object_store_bench_raw.json` | — | 87 | 0 |
| A | `docs/reports/capacity-evidence-2026-08-11/owner_runtime_bench_capacity.json` | — | 264 | 0 |
| A | `docs/reports/capacity-evidence-2026-08-11/owner_runtime_bench_raw.json` | — | 236 | 0 |
| A | `docs/reports/owner-scheduler-capacity-2026-08-09.md` | — | 72 | 0 |
| A | `docs/runbooks/gateway-security.md` | — | 89 | 0 |
| A | `docs/runbooks/rollback.md` | — | 41 | 0 |
| A | `docs/tsan-ftest-report-2026-08-19.md` | — | 41 | 0 |
| A | `docs/upstream-absorption-plan-2026-10.md` | — | 1084 | 0 |
| A | `docs/upstream-absorption-plan-d07e7641-735bd31f.md` | — | 901 | 0 |
| A | `docs/upstream-sync-evidence-2026-08.md` | — | 480 | 0 |
| A | `docs/upstream-sync-optimization-plan-2026-08.md` | — | 902 | 0 |
| A | `docs/vendor-libwebsockets-manifest-2026-08.md` | — | 39 | 0 |
| M | `docs/zh-CN/apply/index.md` | — | 1 | 0 |
| M | `docs/zh-CN/apply/master/index.md` | — | 1 | 0 |
| A | `docs/zh-CN/apply/master/prepare_shutdown.md` | — | 29 | 0 |
| M | `docs/zh-CN/build/build.md` | — | 7 | 5 |
| M | `docs/zh-CN/efun/ltrim.md` | — | 3 | 1 |
| M | `docs/zh-CN/efun/pcre_version.md` | — | 2 | 0 |
| M | `docs/zh-CN/efun/rtrim.md` | — | 3 | 1 |
| M | `docs/zh-CN/efun/set_encoding.md` | — | 30 | 6 |
| M | `docs/zh-CN/efun/socket_create.md` | — | 2 | 0 |
| M | `docs/zh-CN/efun/trim.md` | — | 4 | 1 |
| M | `docs/zh-CN/efun/write_file.md` | — | 5 | 0 |
| A | `etc/config.test` | — | 391 | 0 |
| M | `src/.clang-format` | — | 19 | 1 |
| M | `src/.clang-tidy` | — | 36 | 16 |
| M | `src/CMakeLists.txt` | — | 223 | 33 |
| M | `src/Config.example` | — | 3 | 0 |
| M | `src/backend.cc` | — | 588 | 82 |
| M | `src/backend.h` | — | 94 | 10 |
| M | `src/base/internal/EGCIterator.h` | — | 124 | 20 |
| M | `src/base/internal/debugmalloc.cc` | — | 36 | 30 |
| M | `src/base/internal/debugmalloc.h` | — | 23 | 7 |
| M | `src/base/internal/external_port.h` | — | 14 | 1 |
| M | `src/base/internal/file.cc` | — | 3 | 3 |
| M | `src/base/internal/file.h` | — | 1 | 1 |
| D | `src/base/internal/hash.cc` | — | 0 | 12 |
| D | `src/base/internal/hash.h` | — | 0 | 9 |
| M | `src/base/internal/log.cc` | — | 13 | 14 |
| M | `src/base/internal/log.h` | — | 6 | 6 |
| M | `src/base/internal/lru_cache.h` | — | 15 | 7 |
| M | `src/base/internal/md.cc` | — | 70 | 33 |
| M | `src/base/internal/md.h` | — | 66 | 10 |
| M | `src/base/internal/outbuf.cc` | — | 6 | 6 |
| M | `src/base/internal/outbuf.h` | — | 7 | 7 |
| M | `src/base/internal/port.cc` | — | 120 | 6 |
| M | `src/base/internal/port.h` | — | 5 | 3 |
| M | `src/base/internal/rc.cc` | — | 70 | 11 |
| M | `src/base/internal/rc.h` | — | 5 | 5 |
| M | `src/base/internal/rusage.cc` | — | 1 | 1 |
| M | `src/base/internal/rusage.h` | — | 1 | 1 |
| M | `src/base/internal/stats.cc` | — | 3 | 3 |
| M | `src/base/internal/stats.h` | — | 9 | 6 |
| M | `src/base/internal/stralloc.cc` | — | 118 | 76 |
| M | `src/base/internal/stralloc.h` | — | 112 | 48 |
| M | `src/base/internal/strput.cc` | — | 2 | 2 |
| M | `src/base/internal/strput.h` | — | 2 | 2 |
| M | `src/base/internal/strutils.cc` | — | 281 | 70 |
| M | `src/base/internal/strutils.h` | — | 90 | 51 |
| M | `src/base/internal/tracing.cc` | — | 152 | 76 |
| M | `src/base/internal/tracing.h` | — | 10 | 37 |
| A | `src/base/internal/vm_thread_local.h` | — | 14 | 0 |
| M | `src/base/package_api.h` | — | 1 | 1 |
| M | `src/base/std.h` | — | 57 | 58 |
| M | `src/comm.cc` | — | 1115 | 304 |
| M | `src/comm.h` | — | 34 | 20 |
| M | `src/compiler/internal/LexStream.h` | — | 21 | 7 |
| A | `src/compiler/internal/compile_arena.cc` | — | 362 | 0 |
| A | `src/compiler/internal/compile_arena.h` | — | 191 | 0 |
| M | `src/compiler/internal/compiler.cc` | — | 691 | 364 |
| M | `src/compiler/internal/compiler.h` | — | 117 | 81 |
| A | `src/compiler/internal/diagnostic.cc` | — | 121 | 0 |
| A | `src/compiler/internal/diagnostic.h` | — | 129 | 0 |
| A | `src/compiler/internal/diagnostic_render.cc` | — | 167 | 0 |
| A | `src/compiler/internal/diagnostic_render.h` | — | 56 | 0 |
| M | `src/compiler/internal/disassembler.cc` | — | 137 | 117 |
| M | `src/compiler/internal/generate.cc` | — | 126 | 42 |
| M | `src/compiler/internal/generate.h` | — | 7 | 7 |
| M | `src/compiler/internal/grammar.autogen.cc` | — | 1721 | 1501 |
| M | `src/compiler/internal/grammar.autogen.h` | — | 21 | 18 |
| M | `src/compiler/internal/grammar.y` | — | 215 | 70 |
| D | `src/compiler/internal/grammar_rules.cc` | — | 0 | 316 |
| M | `src/compiler/internal/grammar_rules.h` | — | 2 | 2 |
| A | `src/compiler/internal/grammar_rules_classes.cc` | — | 70 | 0 |
| A | `src/compiler/internal/grammar_rules_functions.cc` | — | 225 | 0 |
| A | `src/compiler/internal/grammar_rules_program.cc` | — | 98 | 0 |
| M | `src/compiler/internal/icode.cc` | — | 128 | 50 |
| M | `src/compiler/internal/icode.h` | — | 5 | 5 |
| M | `src/compiler/internal/keyword.h` | — | 13 | 11 |
| M | `src/compiler/internal/lex.cc` | — | 1086 | 620 |
| M | `src/compiler/internal/lex.h` | — | 69 | 28 |
| A | `src/compiler/internal/lpc_modern_profile.cc` | — | 197 | 0 |
| A | `src/compiler/internal/lpc_modern_profile.h` | — | 54 | 0 |
| A | `src/compiler/internal/lpc_source_encoding.cc` | — | 111 | 0 |
| A | `src/compiler/internal/lpc_source_encoding.h` | — | 20 | 0 |
| M | `src/compiler/internal/scratchpad.cc` | — | 45 | 365 |
| M | `src/compiler/internal/scratchpad.h` | — | 11 | 42 |
| M | `src/compiler/internal/trees.cc` | — | 80 | 36 |
| M | `src/compiler/internal/trees.h` | — | 25 | 25 |
| M | `src/include/opcodes_extra.h` | — | 1 | 20 |
| M | `src/include/pcre_flags.h` | — | 8 | 7 |
| M | `src/include/runtime_config.h` | — | 16 | 0 |
| M | `src/interactive.h` | — | 27 | 14 |
| M | `src/local_options.README` | — | 6 | 0 |
| M | `src/main.cc` | — | 4 | 2 |
| A | `src/main_fuzz_compile.cc` | — | 220 | 0 |
| A | `src/main_fuzz_restore.cc` | — | 159 | 0 |
| M | `src/main_generate_keywords.cc` | — | 5 | 6 |
| M | `src/main_json2o.cc` | — | 4 | 4 |
| M | `src/main_lpcc.cc` | — | 164 | 8 |
| A | `src/main_lpcshell.cc` | — | 387 | 0 |
| M | `src/main_o2json.cc` | — | 9 | 11 |
| M | `src/main_symbol.cc` | — | 3 | 1 |
| M | `src/mainlib.cc` | — | 66 | 24 |
| M | `src/net/msp.cc` | — | 5 | 4 |
| M | `src/net/msp.h` | — | 3 | 3 |
| M | `src/net/sys_telnet.h` | — | 13 | 13 |
| M | `src/net/telnet.cc` | — | 131 | 61 |
| M | `src/net/telnet.h` | — | 18 | 17 |
| M | `src/net/tls.cc` | — | 74 | 17 |
| M | `src/net/tls.h` | — | 13 | 0 |
| M | `src/net/websocket.cc` | — | 361 | 23 |
| M | `src/net/websocket.h` | — | 48 | 1 |
| M | `src/net/ws_ascii.cc` | — | 68 | 119 |
| M | `src/net/ws_ascii.h` | — | 4 | 9 |
| M | `src/net/ws_telnet.cc` | — | 40 | 99 |
| M | `src/net/ws_telnet.h` | — | 5 | 10 |
| M | `src/ofile.cc` | — | 57 | 45 |
| M | `src/ofile.h` | — | 3 | 3 |
| M | `src/packages/async/async.cc` | — | 715 | 131 |
| M | `src/packages/async/async.spec` | — | 7 | 0 |
| M | `src/packages/compress/compress.cc` | — | 227 | 122 |
| M | `src/packages/contrib/CMakeLists.txt` | — | 1 | 0 |
| M | `src/packages/contrib/contrib.cc` | — | 398 | 224 |
| M | `src/packages/contrib/contrib.spec` | — | 9 | 0 |
| A | `src/packages/contrib/cycles.cc` | — | 377 | 0 |
| M | `src/packages/core/add_action.cc` | — | 90 | 61 |
| M | `src/packages/core/add_action.h` | — | 10 | 10 |
| M | `src/packages/core/call_out.cc` | — | 396 | 95 |
| M | `src/packages/core/call_out.h` | — | 26 | 13 |
| M | `src/packages/core/core.spec` | — | 93 | 1 |
| M | `src/packages/core/crc32.cc` | — | 3 | 3 |
| M | `src/packages/core/crc32.h` | — | 1 | 1 |
| M | `src/packages/core/custom_crypt.cc` | — | 25 | 25 |
| M | `src/packages/core/custom_crypt.h` | — | 1 | 1 |
| M | `src/packages/core/debug.cc` | — | 3 | 3 |
| M | `src/packages/core/dns.cc` | — | 228 | 43 |
| M | `src/packages/core/dns.h` | — | 7 | 7 |
| M | `src/packages/core/dumpstat.cc` | — | 12 | 12 |
| M | `src/packages/core/dumpstat.h` | — | 2 | 2 |
| M | `src/packages/core/ed.cc` | — | 260 | 162 |
| M | `src/packages/core/ed.h` | — | 22 | 22 |
| M | `src/packages/core/efuns_main.cc` | — | 700 | 221 |
| M | `src/packages/core/encoding.cc` | — | 23 | 22 |
| M | `src/packages/core/file.cc` | — | 367 | 218 |
| M | `src/packages/core/file.h` | — | 16 | 12 |
| M | `src/packages/core/heartbeat.cc` | — | 142 | 65 |
| M | `src/packages/core/heartbeat.h` | — | 7 | 5 |
| M | `src/packages/core/interactive.cc` | — | 8 | 1 |
| A | `src/packages/core/json.cc` | — | 322 | 0 |
| A | `src/packages/core/json.h` | — | 11 | 0 |
| M | `src/packages/core/mssp.cc` | — | 17 | 17 |
| M | `src/packages/core/outbuf.cc` | — | 1 | 1 |
| M | `src/packages/core/outbuf.h` | — | 1 | 1 |
| A | `src/packages/core/promises.cc` | — | 228 | 0 |
| M | `src/packages/core/rc.cc` | — | 2 | 2 |
| M | `src/packages/core/reclaim.cc` | — | 25 | 21 |
| M | `src/packages/core/regexp.cc` | — | 168 | 167 |
| M | `src/packages/core/regexp.h` | — | 10 | 10 |
| M | `src/packages/core/replace_program.cc` | — | 73 | 43 |
| M | `src/packages/core/replace_program.h` | — | 5 | 5 |
| M | `src/packages/core/save.cc` | — | 4 | 4 |
| M | `src/packages/core/sprintf.cc` | — | 137 | 76 |
| M | `src/packages/core/sprintf.h` | — | 2 | 2 |
| M | `src/packages/core/string.cc` | — | 6 | 6 |
| M | `src/packages/core/sys.cc` | — | 44 | 14 |
| M | `src/packages/core/telnet_ext.cc` | — | 17 | 16 |
| M | `src/packages/core/time.cc` | — | 33 | 14 |
| M | `src/packages/core/trace.cc` | — | 6 | 10 |
| A | `src/packages/core/vm_owner.cc` | — | 806 | 0 |
| A | `src/packages/core/vm_worker.cc` | — | 749 | 0 |
| M | `src/packages/crypto/crypto.cc` | — | 35 | 19 |
| M | `src/packages/db/db.cc` | — | 210 | 123 |
| M | `src/packages/db/db.h` | — | 24 | 19 |
| M | `src/packages/develop/checkmemory.cc` | — | 163 | 122 |
| M | `src/packages/develop/develop.cc` | — | 47 | 18 |
| M | `src/packages/develop/develop.spec` | — | 0 | 2 |
| M | `src/packages/develop/disassembler.cc` | — | 5 | 5 |
| A | `src/packages/dwlib/CMakeLists.txt` | — | 5 | 0 |
| M | `src/packages/dwlib/dwlib.cc` | — | 81 | 54 |
| M | `src/packages/external/CMakeLists.txt` | — | 5 | 0 |
| M | `src/packages/external/external.cc` | — | 1339 | 132 |
| A | `src/packages/external/external.h` | — | 27 | 0 |
| M | `src/packages/external/external.spec` | — | 23 | 1 |
| M | `src/packages/external/socketpair.cc` | — | 14 | 7 |
| A | `src/packages/gateway/CMakeLists.txt` | — | 6 | 0 |
| A | `src/packages/gateway/gateway.cc` | — | 3453 | 0 |
| A | `src/packages/gateway/gateway.h` | — | 663 | 0 |
| A | `src/packages/gateway/gateway.spec` | — | 41 | 0 |
| A | `src/packages/gateway/gateway_session.cc` | — | 7116 | 0 |
| M | `src/packages/math/math.cc` | — | 82 | 38 |
| M | `src/packages/matrix/matrix.cc` | — | 82 | 166 |
| M | `src/packages/mudlib_stats/mudlib_stats.cc` | — | 256 | 150 |
| M | `src/packages/mudlib_stats/mudlib_stats.h` | — | 19 | 19 |
| M | `src/packages/ops/ops.cc` | — | 157 | 53 |
| M | `src/packages/ops/ops.spec` | — | 7 | 0 |
| M | `src/packages/ops/parse.cc` | — | 96 | 91 |
| M | `src/packages/ops/parse.h` | — | 4 | 4 |
| M | `src/packages/parser/parser.cc` | — | 378 | 263 |
| M | `src/packages/parser/parser.h` | — | 28 | 28 |
| M | `src/packages/pcre/CMakeLists.txt` | — | 5 | 4 |
| M | `src/packages/pcre/pcre.cc` | — | 817 | 424 |
| M | `src/packages/pcre/pcre.h` | — | 36 | 33 |
| M | `src/packages/sha1/sha1.cc` | — | 7 | 7 |
| M | `src/packages/sockets/socket_efuns.cc` | — | 618 | 179 |
| M | `src/packages/sockets/socket_efuns.h` | — | 38 | 29 |
| A | `src/packages/sockets/socket_text.h` | — | 54 | 0 |
| M | `src/packages/sockets/sockets.cc` | — | 43 | 49 |
| M | `src/packages/trim/trim.cc` | — | 14 | 8 |
| M | `src/packages/uids/uids.cc` | — | 16 | 14 |
| M | `src/packages/uids/uids.h` | — | 6 | 6 |
| M | `src/portbind.cc` | — | 6 | 7 |
| M | `src/symbol.cc` | — | 30 | 13 |
| M | `src/symbol.h` | — | 2 | 2 |
| M | `src/tests/CMakeLists.txt` | — | 125 | 2 |
| A | `src/tests/bench_compile.cc` | — | 394 | 0 |
| A | `src/tests/bench_diagnostic_render.cc` | — | 83 | 0 |
| A | `src/tests/bench_explode.cc` | — | 109 | 0 |
| A | `src/tests/bench_scratchpad.cc` | — | 125 | 0 |
| A | `src/tests/compound_free_probe.cc` | — | 403 | 0 |
| A | `src/tests/fuzz/corpus/compile/invalid_sequence` | — | 5 | 0 |
| A | `src/tests/fuzz/corpus/compile/mixed_sequence` | — | 7 | 0 |
| A | `src/tests/fuzz/corpus/compile/valid_sequence` | — | 5 | 0 |
| A | `src/tests/fuzz/corpus/gateway/coalesced_valid` | — | - | - |
| R100 | `src/tests/fuzz/corpus/gateway/empty` | `testsuite/single/tests/efuns/has_errors.c` | 0 | 0 |
| A | `src/tests/fuzz/corpus/gateway/invalid_json` | — | - | - |
| A | `src/tests/fuzz/corpus/gateway/oversized_header` | — | - | - |
| A | `src/tests/fuzz/corpus/gateway/truncated_frame` | — | - | - |
| A | `src/tests/fuzz/corpus/gateway/valid_data_frame` | — | - | - |
| A | `src/tests/fuzz/corpus/gateway/valid_fragmented_a` | — | - | - |
| A | `src/tests/fuzz/corpus/gateway/valid_fragmented_b` | — | 1 | 0 |
| A | `src/tests/fuzz/corpus/restore/invalid_sequence` | — | 5 | 0 |
| A | `src/tests/fuzz/corpus/restore/mixed_sequence` | — | 5 | 0 |
| A | `src/tests/fuzz/corpus/restore/nested_mapping_error` | — | 4 | 0 |
| A | `src/tests/fuzz/corpus/restore/valid_sequence` | — | 5 | 0 |
| A | `src/tests/gateway_full_wire_bench.cc` | — | 811 | 0 |
| A | `src/tests/gateway_fuzz.cc` | — | 433 | 0 |
| A | `src/tests/gateway_fuzz_smoke_main.cc` | — | 22 | 0 |
| A | `src/tests/lpc_vm_bench.cc` | — | 548 | 0 |
| A | `src/tests/object_store_bench.cc` | — | 579 | 0 |
| A | `src/tests/owner_runtime_bench.cc` | — | 1097 | 0 |
| A | `src/tests/test_compile_arena.cc` | — | 290 | 0 |
| A | `src/tests/test_egc.cc` | — | 109 | 0 |
| A | `src/tests/test_file_io.cc` | — | 1518 | 0 |
| M | `src/tests/test_lpc.cc` | — | 982 | 111 |
| A | `src/tests/test_lpc_compiler.cc` | — | 2319 | 0 |
| A | `src/tests/test_lpc_gateway.cc` | — | 6531 | 0 |
| A | `src/tests/test_lpc_owner.cc` | — | 17891 | 0 |
| A | `src/tests/test_lpc_support.h` | — | 453 | 0 |
| A | `src/tests/test_mudlib.h` | — | 96 | 0 |
| A | `src/tests/test_object_store_summary.cc` | — | 364 | 0 |
| M | `src/tests/test_ofile.cc` | — | 15 | 3 |
| A | `src/tests/test_socket_text.cc` | — | 45 | 0 |
| A | `src/tests/trace_io_probe.cc` | — | 105 | 0 |
| A | `src/tests/ws_smoke.py` | — | 342 | 0 |
| M | `src/thirdparty/backward-cpp/backward.hpp` | — | 27 | 11 |
| M | `src/thirdparty/crypt/CMakeLists.txt` | — | 23 | 1 |
| M | `src/thirdparty/crypt/crypt.h` | — | 4 | 6 |
| M | `src/thirdparty/crypt/crypt_r.c` | — | 4 | 0 |
| M | `src/thirdparty/filesystem/CMakeLists.txt` | — | 1 | 1 |
| M | `src/thirdparty/libevent/CMakeLists.txt` | — | 12 | 0 |
| M | `src/thirdparty/libevent/bufferevent_mbedtls.c` | — | 1 | 2 |
| M | `src/thirdparty/libevent/bufferevent_openssl.c` | — | 1 | 1 |
| M | `src/thirdparty/libevent/bufferevent_ssl.c` | — | 6 | 5 |
| M | `src/thirdparty/libevent/event.c` | — | 7 | 1 |
| M | `src/thirdparty/libevent/http.c` | — | 9 | 2 |
| M | `src/thirdparty/libevent/sha1.c` | — | 8 | 1 |
| M | `src/thirdparty/libevent/ssl-compat.h` | — | 1 | 1 |
| A | `src/thirdparty/libwebsockets/.github/codeql.yml` | — | 35 | 0 |
| A | `src/thirdparty/libwebsockets/.github/workflows/codeql.yml` | — | 43 | 0 |
| M | `src/thirdparty/libwebsockets/.gitignore` | — | 2 | 1 |
| M | `src/thirdparty/libwebsockets/.sai.json` | — | 145 | 193 |
| M | `src/thirdparty/libwebsockets/CMakeLists-implied-options.txt` | — | 37 | 7 |
| M | `src/thirdparty/libwebsockets/CMakeLists.txt` | — | 282 | 85 |
| M | `src/thirdparty/libwebsockets/LICENSE` | — | 131 | 4 |
| M | `src/thirdparty/libwebsockets/README.md` | — | 135 | 24 |
| M | `src/thirdparty/libwebsockets/READMEs/README.build-windows.md` | — | 34 | 68 |
| M | `src/thirdparty/libwebsockets/READMEs/README.build.md` | — | 80 | 23 |
| A | `src/thirdparty/libwebsockets/READMEs/README.cbor-cose.md` | — | 272 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.cbor-lecp.md` | — | 344 | 0 |
| M | `src/thirdparty/libwebsockets/READMEs/README.coding.md` | — | 42 | 53 |
| M | `src/thirdparty/libwebsockets/READMEs/README.ctest.md` | — | 9 | 15 |
| A | `src/thirdparty/libwebsockets/READMEs/README.display-list.md` | — | 148 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.event-loops-intro.md` | — | 285 | 0 |
| M | `src/thirdparty/libwebsockets/READMEs/README.fault-injection.md` | — | 30 | 6 |
| A | `src/thirdparty/libwebsockets/READMEs/README.html-parser.md` | — | 304 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.http-cache.md` | — | 40 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.jit-trust.md` | — | 446 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.jpeg-decoder.md` | — | 77 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.json-lejp.md` | — | 136 | 0 |
| M | `src/thirdparty/libwebsockets/READMEs/README.logging.md` | — | 188 | 5 |
| A | `src/thirdparty/libwebsockets/READMEs/README.lws_backtrace.md` | — | 181 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.lws_cache.md` | — | 160 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.lws_fixed3232.md` | — | 87 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.lws_map.md` | — | 117 | 0 |
| M | `src/thirdparty/libwebsockets/READMEs/README.lws_metrics.md` | — | 2 | 2 |
| A | `src/thirdparty/libwebsockets/READMEs/README.lws_ota.md` | — | 302 | 0 |
| D | `src/thirdparty/libwebsockets/READMEs/README.lws_sequencer.md` | — | 0 | 149 |
| M | `src/thirdparty/libwebsockets/READMEs/README.lws_sul.md` | — | 18 | 7 |
| M | `src/thirdparty/libwebsockets/READMEs/README.lws_system.md` | — | 24 | 0 |
| M | `src/thirdparty/libwebsockets/READMEs/README.lwsws.md` | — | 25 | 27 |
| A | `src/thirdparty/libwebsockets/READMEs/README.png-decoder.md` | — | 56 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.secure-streams.md` | — | 26 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.stdin.md` | — | 86 | 0 |
| A | `src/thirdparty/libwebsockets/READMEs/README.tcp_fastopen.md` | — | 32 | 0 |
| M | `src/thirdparty/libwebsockets/READMEs/README.tls-sessions.md` | — | 4 | 2 |
| D | `src/thirdparty/libwebsockets/READMEs/README.vulnerability-reporting.md` | — | 0 | 12 |
| M | `src/thirdparty/libwebsockets/READMEs/release-checklist` | — | 0 | 5 |
| A | `src/thirdparty/libwebsockets/SECURITY.md` | — | 6 | 0 |
| M | `src/thirdparty/libwebsockets/changelog` | — | 98 | 0 |
| M | `src/thirdparty/libwebsockets/cmake/FindOpenSSLbins.cmake` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/cmake/LwsCheckRequirements.cmake` | — | 16 | 6 |
| M | `src/thirdparty/libwebsockets/cmake/libwebsockets-config.cmake.in` | — | 18 | 3 |
| M | `src/thirdparty/libwebsockets/cmake/lws_config.h.in` | — | 45 | 2 |
| M | `src/thirdparty/libwebsockets/cmake/lws_config_private.h.in` | — | 4 | 3 |
| A | `src/thirdparty/libwebsockets/cmake/pico_sdk_import.cmake` | — | 64 | 0 |
| D | `src/thirdparty/libwebsockets/contrib/Android.mk` | — | 0 | 29 |
| D | `src/thirdparty/libwebsockets/contrib/abi/README.md` | — | 0 | 58 |
| D | `src/thirdparty/libwebsockets/contrib/abi/libwebsockets.json` | — | 0 | 257 |
| D | `src/thirdparty/libwebsockets/contrib/abi/lws-abi-update.sh` | — | 0 | 17 |
| D | `src/thirdparty/libwebsockets/contrib/cross-aarch64-android.cmake` | — | 0 | 58 |
| D | `src/thirdparty/libwebsockets/contrib/cross-aarch64.cmake` | — | 0 | 48 |
| D | `src/thirdparty/libwebsockets/contrib/cross-arm-android-gnueabi.cmake` | — | 0 | 46 |
| D | `src/thirdparty/libwebsockets/contrib/cross-arm-linux-gnueabihf.cmake` | — | 0 | 44 |
| D | `src/thirdparty/libwebsockets/contrib/cross-atmel.cmake` | — | 0 | 119 |
| D | `src/thirdparty/libwebsockets/contrib/cross-esp32.cmake` | — | 0 | 68 |
| D | `src/thirdparty/libwebsockets/contrib/cross-linkit.cmake` | — | 0 | 84 |
| D | `src/thirdparty/libwebsockets/contrib/cross-ming.cmake` | — | 0 | 47 |
| D | `src/thirdparty/libwebsockets/contrib/cross-openwrt-makefile` | — | 0 | 97 |
| D | `src/thirdparty/libwebsockets/contrib/cross-w32.cmake` | — | 0 | 49 |
| D | `src/thirdparty/libwebsockets/contrib/cross-w64.cmake` | — | 0 | 50 |
| D | `src/thirdparty/libwebsockets/contrib/iOS.cmake` | — | 0 | 229 |
| D | `src/thirdparty/libwebsockets/doc-assets/abstract-overview.svg` | — | 0 | 348 |
| D | `src/thirdparty/libwebsockets/doc-assets/accept-flow-1.svg` | — | 0 | 46 |
| D | `src/thirdparty/libwebsockets/doc-assets/accept-flow-2.svg` | — | 0 | 71 |
| D | `src/thirdparty/libwebsockets/doc-assets/accept-flow-3.svg` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/doc-assets/fault-injection.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/http-proxy-overview.svg` | — | 0 | 84 |
| D | `src/thirdparty/libwebsockets/doc-assets/lifecycle-context.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/lifecycle-server-wsi.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/lifecycle-wsi.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-crypto-overview.svg` | — | 0 | 43 |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-detailed-latency-example.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-fts.svg` | — | 0 | 63 |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-overview.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-relpol-1.svg` | — | 0 | 30 |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-relpol-2.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-relpol-3.svg` | — | 0 | 64 |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-relpol-4.svg` | — | 0 | 84 |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-relpol-5.svg` | — | 0 | 156 |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-smp-example.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/lws-smp-ov.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/lws_dll.svg` | — | 0 | 133 |
| D | `src/thirdparty/libwebsockets/doc-assets/lws_metrics-decimation.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/lws_metrics-policy.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/lws_sequencer.svg` | — | 0 | 955 |
| D | `src/thirdparty/libwebsockets/doc-assets/lws_struct-overview.svg` | — | 0 | 62 |
| D | `src/thirdparty/libwebsockets/doc-assets/lwsac.svg` | — | 0 | 131 |
| D | `src/thirdparty/libwebsockets/doc-assets/lwsgt-overview.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/smd-message.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/smd-proxy.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/smd-single-process.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/ss-explain.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/ss-operation-modes.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/ss-state-flow-server.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/ss-state-flow.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/doc-assets/threadpool-states.svg` | — | 0 | 153 |
| D | `src/thirdparty/libwebsockets/doc-assets/threadpool.svg` | — | 0 | 2 |
| D | `src/thirdparty/libwebsockets/doc-assets/wss2.png` | — | - | - |
| M | `src/thirdparty/libwebsockets/include/libwebsockets.h` | — | 180 | 19 |
| D | `src/thirdparty/libwebsockets/include/libwebsockets/abstract/abstract.h` | — | 0 | 138 |
| D | `src/thirdparty/libwebsockets/include/libwebsockets/abstract/protocols.h` | — | 0 | 88 |
| D | `src/thirdparty/libwebsockets/include/libwebsockets/abstract/protocols/smtp.h` | — | 0 | 115 |
| D | `src/thirdparty/libwebsockets/include/libwebsockets/abstract/transports.h` | — | 0 | 65 |
| D | `src/thirdparty/libwebsockets/include/libwebsockets/abstract/transports/unit-test.h` | — | 0 | 81 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-adopt.h` | — | 15 | 7 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-async-dns.h` | — | 46 | 2 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-backtrace.h` | — | 280 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-bb-spi.h` | — | 2 | 0 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-cache-ttl.h` | — | 348 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-callbacks.h` | — | 12 | 11 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-cgi.h` | — | 26 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-client.h` | — | 112 | 8 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-conmon.h` | — | 49 | 3 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-context-vhost.h` | — | 211 | 44 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-cose.h` | — | 511 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-display.h` | — | 80 | 14 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-dll2.h` | — | 24 | 1 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-dlo.h` | — | 524 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-dsh.h` | — | 22 | 1 |
| R056 | `src/thirdparty/libwebsockets/include/libwebsockets/lws-esp32-spi.h` | `src/thirdparty/libwebsockets/lib/drivers/devices/display/ssd1306.h` | 20 | 34 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-eventlib-exports.h` | — | 70 | 3 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-fault-injection.h` | — | 22 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-freertos.h` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-genaes.h` | — | 2 | 2 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-gencrypto.h` | — | 9 | 2 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-genec.h` | — | 2 | 2 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-genhash.h` | — | 15 | 3 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-genrsa.h` | — | 3 | 2 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-html.h` | — | 717 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-http.h` | — | 23 | 4 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-ili9341-spi.h` | — | 6 | 6 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-jose.h` | — | 5 | 2 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-jpeg.h` | — | 104 | 0 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-jrpc.h` | — | 229 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-jwk.h` | — | 5 | 3 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-jws.h` | — | 8 | 6 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-lecp.h` | — | 539 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-lejp.h` | — | 49 | 18 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-logs.h` | — | 522 | 27 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-lwsac.h` | — | 21 | 0 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-map.h` | — | 188 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-misc.h` | — | 501 | 8 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-mqtt.h` | — | 47 | 1 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-network-helper.h` | — | 21 | 6 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-ota.h` | — | 122 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-protocols-plugins.h` | — | 2 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams-client.h` | — | 163 | 29 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams-policy.h` | — | 24 | 34 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams-serialization.h` | — | 599 | 0 |
| R056 | `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams-transport-proxy.h` | `src/thirdparty/libwebsockets/lib/abstract/private-lib-abstract.h` | 21 | 29 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-secure-streams.h` | — | 110 | 196 |
| D | `src/thirdparty/libwebsockets/include/libwebsockets/lws-sequencer.h` | — | 0 | 243 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-service.h` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-sha1-base64.h` | — | 4 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-spa.h` | — | 2 | 1 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-spd1656-spi.h` | — | 55 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-spi.h` | — | 21 | 1 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-ssd1306-i2c.h` | — | 7 | 7 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-ssd1675b-spi.h` | — | 55 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-struct.h` | — | 316 | 47 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-system.h` | — | 82 | 1 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-timeout-timer.h` | — | 9 | 58 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-tokenize.h` | — | 31 | 5 |
| R050 | `src/thirdparty/libwebsockets/include/libwebsockets/lws-uc8176-spi.h` | `src/thirdparty/libwebsockets/include/libwebsockets/abstract/transports/raw-skt.h` | 33 | 7 |
| A | `src/thirdparty/libwebsockets/include/libwebsockets/lws-upng.h` | — | 160 | 0 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-vfs.h` | — | 14 | 3 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-write.h` | — | 4 | 21 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-ws-ext.h` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-ws-state.h` | — | 18 | 2 |
| M | `src/thirdparty/libwebsockets/include/libwebsockets/lws-x509.h` | — | 18 | 9 |
| A | `src/thirdparty/libwebsockets/lgtm.yml` | — | 3 | 0 |
| M | `src/thirdparty/libwebsockets/lib/CMakeLists.txt` | — | 125 | 31 |
| D | `src/thirdparty/libwebsockets/lib/abstract/README.md` | — | 0 | 170 |
| D | `src/thirdparty/libwebsockets/lib/abstract/abstract.c` | — | 0 | 355 |
| D | `src/thirdparty/libwebsockets/lib/abstract/protocols/smtp/private-lib-abstract-protocols-smtp.h` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/lib/abstract/protocols/smtp/smtp-sequencer.c` | — | 0 | 323 |
| D | `src/thirdparty/libwebsockets/lib/abstract/protocols/smtp/smtp.c` | — | 0 | 382 |
| D | `src/thirdparty/libwebsockets/lib/abstract/test-sequencer.c` | — | 0 | 274 |
| D | `src/thirdparty/libwebsockets/lib/abstract/transports/raw-skt.c` | — | 0 | 408 |
| D | `src/thirdparty/libwebsockets/lib/abstract/transports/unit-test.c` | — | 0 | 540 |
| M | `src/thirdparty/libwebsockets/lib/core-net/CMakeLists.txt` | — | 3 | 3 |
| M | `src/thirdparty/libwebsockets/lib/core-net/adopt.c` | — | 84 | 45 |
| M | `src/thirdparty/libwebsockets/lib/core-net/client/client.c` | — | 12 | 11 |
| M | `src/thirdparty/libwebsockets/lib/core-net/client/conmon.c` | — | 6 | 3 |
| M | `src/thirdparty/libwebsockets/lib/core-net/client/connect.c` | — | 126 | 85 |
| M | `src/thirdparty/libwebsockets/lib/core-net/client/connect2.c` | — | 66 | 67 |
| M | `src/thirdparty/libwebsockets/lib/core-net/client/connect3.c` | — | 209 | 67 |
| M | `src/thirdparty/libwebsockets/lib/core-net/client/connect4.c` | — | 41 | 29 |
| M | `src/thirdparty/libwebsockets/lib/core-net/client/sort-dns.c` | — | 14 | 22 |
| M | `src/thirdparty/libwebsockets/lib/core-net/close.c` | — | 244 | 61 |
| M | `src/thirdparty/libwebsockets/lib/core-net/dummy-callback.c` | — | 114 | 84 |
| M | `src/thirdparty/libwebsockets/lib/core-net/lws-dsh.c` | — | 291 | 175 |
| M | `src/thirdparty/libwebsockets/lib/core-net/network.c` | — | 247 | 117 |
| M | `src/thirdparty/libwebsockets/lib/core-net/output.c` | — | 18 | 37 |
| M | `src/thirdparty/libwebsockets/lib/core-net/pollfd.c` | — | 58 | 51 |
| M | `src/thirdparty/libwebsockets/lib/core-net/private-lib-core-net.h` | — | 174 | 62 |
| M | `src/thirdparty/libwebsockets/lib/core-net/route.c` | — | 45 | 32 |
| D | `src/thirdparty/libwebsockets/lib/core-net/sequencer.c` | — | 0 | 335 |
| M | `src/thirdparty/libwebsockets/lib/core-net/service.c` | — | 89 | 51 |
| M | `src/thirdparty/libwebsockets/lib/core-net/socks5-client.c` | — | 37 | 36 |
| M | `src/thirdparty/libwebsockets/lib/core-net/sorted-usec-list.c` | — | 23 | 4 |
| M | `src/thirdparty/libwebsockets/lib/core-net/state.c` | — | 8 | 7 |
| A | `src/thirdparty/libwebsockets/lib/core-net/transport-mux-client.c` | — | 337 | 0 |
| A | `src/thirdparty/libwebsockets/lib/core-net/transport-mux-common.c` | — | 813 | 0 |
| A | `src/thirdparty/libwebsockets/lib/core-net/transport-mux-proxy.c` | — | 404 | 0 |
| M | `src/thirdparty/libwebsockets/lib/core-net/vhost.c` | — | 344 | 206 |
| A | `src/thirdparty/libwebsockets/lib/core-net/wol.c` | — | 99 | 0 |
| M | `src/thirdparty/libwebsockets/lib/core-net/wsi-timeout.c` | — | 34 | 126 |
| M | `src/thirdparty/libwebsockets/lib/core-net/wsi.c` | — | 211 | 85 |
| M | `src/thirdparty/libwebsockets/lib/core/CMakeLists.txt` | — | 30 | 8 |
| M | `src/thirdparty/libwebsockets/lib/core/alloc.c` | — | 54 | 1 |
| M | `src/thirdparty/libwebsockets/lib/core/buflist.c` | — | 180 | 1 |
| M | `src/thirdparty/libwebsockets/lib/core/context.c` | — | 451 | 160 |
| M | `src/thirdparty/libwebsockets/lib/core/libwebsockets.c` | — | 738 | 81 |
| M | `src/thirdparty/libwebsockets/lib/core/logs.c` | — | 275 | 66 |
| M | `src/thirdparty/libwebsockets/lib/core/lws_dll2.c` | — | 36 | 0 |
| A | `src/thirdparty/libwebsockets/lib/core/lws_map.c` | — | 269 | 0 |
| M | `src/thirdparty/libwebsockets/lib/core/private-lib-core.h` | — | 342 | 43 |
| M | `src/thirdparty/libwebsockets/lib/core/vfs.c` | — | 25 | 4 |
| R072 | `src/thirdparty/libwebsockets/lib/cose/CMakeLists.txt` | `src/thirdparty/libwebsockets/lib/abstract/CMakeLists.txt` | 7 | 25 |
| A | `src/thirdparty/libwebsockets/lib/cose/cose_key.c` | — | 1188 | 0 |
| A | `src/thirdparty/libwebsockets/lib/cose/cose_sign.c` | — | 543 | 0 |
| A | `src/thirdparty/libwebsockets/lib/cose/cose_sign_alg.c` | — | 271 | 0 |
| A | `src/thirdparty/libwebsockets/lib/cose/cose_validate.c` | — | 1051 | 0 |
| A | `src/thirdparty/libwebsockets/lib/cose/cose_validate_alg.c` | — | 274 | 0 |
| A | `src/thirdparty/libwebsockets/lib/cose/private-lib-cose.h` | — | 148 | 0 |
| M | `src/thirdparty/libwebsockets/lib/drivers/CMakeLists.txt` | — | 9 | 6 |
| M | `src/thirdparty/libwebsockets/lib/drivers/button/lws-button.c` | — | 2 | 2 |
| D | `src/thirdparty/libwebsockets/lib/drivers/devices/display/ili9341.h` | — | 0 | 95 |
| M | `src/thirdparty/libwebsockets/lib/drivers/display/ili9341-spi.c` | — | 272 | 76 |
| M | `src/thirdparty/libwebsockets/lib/drivers/display/lws-display.c` | — | 320 | 9 |
| A | `src/thirdparty/libwebsockets/lib/drivers/display/spd1656-spi.c` | — | 463 | 0 |
| M | `src/thirdparty/libwebsockets/lib/drivers/display/ssd1306-i2c.c` | — | 173 | 37 |
| A | `src/thirdparty/libwebsockets/lib/drivers/display/ssd1675b-spi.c` | — | 495 | 0 |
| A | `src/thirdparty/libwebsockets/lib/drivers/display/uc8176-spi.c` | — | 1017 | 0 |
| M | `src/thirdparty/libwebsockets/lib/drivers/led/led-gpio.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/drivers/netdev/wifi.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/drivers/spi/bitbang/lws-bb-spi.c` | — | 3 | 0 |
| M | `src/thirdparty/libwebsockets/lib/drivers/spi/lws-spi.c` | — | 68 | 1 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/CMakeLists.txt` | — | 54 | 16 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/glib/glib.c` | — | 29 | 27 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/libev/libev.c` | — | 64 | 43 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/libevent/libevent.c` | — | 48 | 58 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/libuv/CMakeLists.txt` | — | 1 | 0 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/libuv/libuv.c` | — | 122 | 76 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/libuv/private-lib-event-libs-libuv.h` | — | 16 | 11 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/poll/poll.c` | — | 23 | 22 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/private-lib-event-libs.h` | — | 0 | 41 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/sdevent/sdevent.c` | — | 31 | 17 |
| M | `src/thirdparty/libwebsockets/lib/event-libs/uloop/uloop.c` | — | 35 | 28 |
| M | `src/thirdparty/libwebsockets/lib/jose/CMakeLists.txt` | — | 2 | 1 |
| M | `src/thirdparty/libwebsockets/lib/jose/jwe/enc/aescbc.c` | — | 6 | 6 |
| M | `src/thirdparty/libwebsockets/lib/jose/jwe/enc/aesgcm.c` | — | 4 | 4 |
| M | `src/thirdparty/libwebsockets/lib/jose/jwe/jwe-rsa-aescbc.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/jose/jwe/jwe-rsa-aesgcm.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/jose/jwe/jwe.c` | — | 8 | 5 |
| A | `src/thirdparty/libwebsockets/lib/jose/jwk/jose_key.c` | — | 649 | 0 |
| M | `src/thirdparty/libwebsockets/lib/jose/jwk/jwk.c` | — | 14 | 637 |
| M | `src/thirdparty/libwebsockets/lib/jose/jws/jose.c` | — | 12 | 6 |
| M | `src/thirdparty/libwebsockets/lib/jose/jws/jws.c` | — | 33 | 24 |
| M | `src/thirdparty/libwebsockets/lib/jose/private-lib-jose.h` | — | 22 | 4 |
| M | `src/thirdparty/libwebsockets/lib/misc/CMakeLists.txt` | — | 91 | 4 |
| A | `src/thirdparty/libwebsockets/lib/misc/backtrace.c` | — | 392 | 0 |
| M | `src/thirdparty/libwebsockets/lib/misc/base64-decode.c` | — | 54 | 14 |
| A | `src/thirdparty/libwebsockets/lib/misc/cache-ttl/file.c` | — | 1005 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/cache-ttl/heap.c` | — | 608 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/cache-ttl/lws-cache-ttl.c` | — | 300 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/cache-ttl/private-lib-misc-cache-ttl.h` | — | 104 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/css-lextable-strings.txt` | — | 120 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/css-lextable.h` | — | 1337 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/css-prop-constants.txt` | — | 125 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/css-propconst-lextable.h` | — | 1110 | 0 |
| M | `src/thirdparty/libwebsockets/lib/misc/dir.c` | — | 155 | 41 |
| A | `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-font-mcufont.c` | — | 528 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-jpeg.c` | — | 195 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-lhp.c` | — | 982 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-png.c` | — | 187 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-rect.c` | — | 247 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-ss.c` | — | 352 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/dlo/dlo-text.c` | — | 413 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/dlo/dlo.c` | — | 900 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/dlo/private-lib-drivers-display-dlo.h` | — | 62 | 0 |
| M | `src/thirdparty/libwebsockets/lib/misc/fsmount.c` | — | 4 | 4 |
| M | `src/thirdparty/libwebsockets/lib/misc/fts/trie-fd.c` | — | 0 | 1 |
| M | `src/thirdparty/libwebsockets/lib/misc/getifaddrs.c` | — | 4 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/ieeehalfprecision.c` | — | 228 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/jpeg.c` | — | 2756 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/jrpc/jrpc.c` | — | 386 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/jrpc/private-lib-misc-jrpc.h` | — | 88 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/lecp.c` | — | 1709 | 0 |
| M | `src/thirdparty/libwebsockets/lib/misc/lejp.c` | — | 153 | 24 |
| A | `src/thirdparty/libwebsockets/lib/misc/lhp-ss.c` | — | 241 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/lhp.c` | — | 2156 | 0 |
| M | `src/thirdparty/libwebsockets/lib/misc/lws-struct-lejp.c` | — | 43 | 41 |
| M | `src/thirdparty/libwebsockets/lib/misc/lws-struct-sqlite.c` | — | 239 | 24 |
| M | `src/thirdparty/libwebsockets/lib/misc/lwsac/lwsac.c` | — | 39 | 3 |
| A | `src/thirdparty/libwebsockets/lib/misc/minilex.c` | — | 288 | 0 |
| M | `src/thirdparty/libwebsockets/lib/misc/romfs.c` | — | 6 | 2 |
| M | `src/thirdparty/libwebsockets/lib/misc/threadpool/threadpool.c` | — | 33 | 14 |
| A | `src/thirdparty/libwebsockets/lib/misc/upng-gzip.c` | — | 1038 | 0 |
| A | `src/thirdparty/libwebsockets/lib/misc/upng.c` | — | 726 | 0 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/CMakeLists.txt` | — | 7 | 1 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/drivers/netdev/wifi-esp32.c` | — | 5 | 3 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/drivers/pwm-esp32.c` | — | 14 | 1 |
| A | `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/drivers/spi-esp32.c` | — | 297 | 0 |
| A | `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/esp32-lws_ota.c` | — | 201 | 0 |
| D | `src/thirdparty/libwebsockets/lib/plat/freertos/esp32/esp_attr.h` | — | 0 | 58 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-file.c` | — | 2 | 1 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-init.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-pipe.c` | — | 20 | 5 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-resolv.c` | — | 14 | 6 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-service.c` | — | 14 | 13 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/freertos-sockets.c` | — | 22 | 8 |
| M | `src/thirdparty/libwebsockets/lib/plat/freertos/private-lib-plat-freertos.h` | — | 16 | 6 |
| M | `src/thirdparty/libwebsockets/lib/plat/optee/network.c` | — | 6 | 0 |
| M | `src/thirdparty/libwebsockets/lib/plat/optee/private-lib-plat-optee.h` | — | 2 | 1 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/CMakeLists.txt` | — | 6 | 0 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/android/android-resolv.c` | — | 25 | 20 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/private-lib-plat-unix.h` | — | 16 | 1 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-caps.c` | — | 20 | 20 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-fds.c` | — | 4 | 4 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-file.c` | — | 3 | 2 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-init.c` | — | 74 | 15 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-pipe.c` | — | 17 | 3 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-plugins.c` | — | 11 | 8 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-resolv.c` | — | 10 | 9 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-service.c` | — | 50 | 5 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-sockets.c` | — | 85 | 18 |
| M | `src/thirdparty/libwebsockets/lib/plat/unix/unix-spawn.c` | — | 381 | 60 |
| A | `src/thirdparty/libwebsockets/lib/plat/unix/unix-systemd.c` | — | 85 | 0 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/CMakeLists.txt` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/private-lib-plat-windows.h` | — | 15 | 0 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/windows-file.c` | — | 34 | 13 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/windows-init.c` | — | 44 | 1 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/windows-pipe.c` | — | 13 | 0 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/windows-plugins.c` | — | 4 | 2 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/windows-resolv.c` | — | 15 | 4 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/windows-service.c` | — | 13 | 6 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/windows-sockets.c` | — | 92 | 23 |
| M | `src/thirdparty/libwebsockets/lib/plat/windows/windows-spawn.c` | — | 106 | 29 |
| M | `src/thirdparty/libwebsockets/lib/roles/CMakeLists.txt` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/cgi/cgi-server.c` | — | 266 | 200 |
| M | `src/thirdparty/libwebsockets/lib/roles/cgi/ops-cgi.c` | — | 10 | 13 |
| M | `src/thirdparty/libwebsockets/lib/roles/cgi/private-lib-roles-cgi.h` | — | 2 | 0 |
| M | `src/thirdparty/libwebsockets/lib/roles/dbus/CMakeLists.txt` | — | 2 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/dbus/dbus.c` | — | 16 | 12 |
| M | `src/thirdparty/libwebsockets/lib/roles/h1/ops-h1.c` | — | 27 | 17 |
| M | `src/thirdparty/libwebsockets/lib/roles/h1/private-lib-roles-h1.h` | — | 2 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/h2/hpack.c` | — | 38 | 8 |
| M | `src/thirdparty/libwebsockets/lib/roles/h2/http2.c` | — | 285 | 47 |
| M | `src/thirdparty/libwebsockets/lib/roles/h2/ops-h2.c` | — | 147 | 15 |
| M | `src/thirdparty/libwebsockets/lib/roles/h2/private-lib-roles-h2.h` | — | 6 | 0 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/CMakeLists.txt` | — | 6 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/client/client-http.c` | — | 918 | 221 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/compression/brotli/brotli.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/compression/stream.c` | — | 1 | 1 |
| A | `src/thirdparty/libwebsockets/lib/roles/http/cookie.c` | — | 744 | 0 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/date.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/header.c` | — | 29 | 8 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/minilex.c` | — | 5 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/parsers.c` | — | 93 | 74 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/private-lib-roles-http.h` | — | 6 | 0 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/server/access-log.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/server/fops-zip.c` | — | 4 | 2 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/server/lejp-conf.c` | — | 158 | 8 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/server/lws-spa.c` | — | 23 | 7 |
| M | `src/thirdparty/libwebsockets/lib/roles/http/server/server.c` | — | 511 | 237 |
| M | `src/thirdparty/libwebsockets/lib/roles/listen/ops-listen.c` | — | 3 | 2 |
| M | `src/thirdparty/libwebsockets/lib/roles/mqtt/client/client-mqtt-handshake.c` | — | 32 | 3 |
| M | `src/thirdparty/libwebsockets/lib/roles/mqtt/client/client-mqtt.c` | — | 42 | 8 |
| M | `src/thirdparty/libwebsockets/lib/roles/mqtt/mqtt.c` | — | 386 | 128 |
| M | `src/thirdparty/libwebsockets/lib/roles/mqtt/ops-mqtt.c` | — | 39 | 6 |
| M | `src/thirdparty/libwebsockets/lib/roles/mqtt/primitives.c` | — | 1 | 2 |
| M | `src/thirdparty/libwebsockets/lib/roles/mqtt/private-lib-roles-mqtt.h` | — | 32 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/netlink/ops-netlink.c` | — | 184 | 103 |
| M | `src/thirdparty/libwebsockets/lib/roles/pipe/ops-pipe.c` | — | 9 | 2 |
| M | `src/thirdparty/libwebsockets/lib/roles/private-lib-roles.h` | — | 18 | 17 |
| M | `src/thirdparty/libwebsockets/lib/roles/raw-file/ops-raw-file.c` | — | 4 | 4 |
| M | `src/thirdparty/libwebsockets/lib/roles/raw-proxy/ops-raw-proxy.c` | — | 2 | 2 |
| M | `src/thirdparty/libwebsockets/lib/roles/raw-skt/ops-raw-skt.c` | — | 76 | 18 |
| M | `src/thirdparty/libwebsockets/lib/roles/ws/client-parser-ws.c` | — | 207 | 61 |
| M | `src/thirdparty/libwebsockets/lib/roles/ws/client-ws.c` | — | 106 | 119 |
| M | `src/thirdparty/libwebsockets/lib/roles/ws/ext/extension-permessage-deflate.c` | — | 52 | 46 |
| M | `src/thirdparty/libwebsockets/lib/roles/ws/ext/extension.c` | — | 10 | 11 |
| M | `src/thirdparty/libwebsockets/lib/roles/ws/ops-ws.c` | — | 189 | 34 |
| M | `src/thirdparty/libwebsockets/lib/roles/ws/private-lib-roles-ws.h` | — | 12 | 1 |
| M | `src/thirdparty/libwebsockets/lib/roles/ws/server-ws.c` | — | 34 | 21 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/CMakeLists.txt` | — | 11 | 8 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/README.md` | — | 20 | 1 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/policy-common.c` | — | 104 | 12 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/policy-json.c` | — | 118 | 37 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/private-lib-secure-streams.h` | — | 195 | 36 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-h1.c` | — | 224 | 50 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-h2.c` | — | 8 | 4 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-mqtt.c` | — | 750 | 152 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-raw.c` | — | 12 | 2 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/protocols/ss-ws.c` | — | 49 | 10 |
| D | `src/thirdparty/libwebsockets/lib/secure-streams/secure-streams-client.c` | — | 0 | 993 |
| D | `src/thirdparty/libwebsockets/lib/secure-streams/secure-streams-process.c` | — | 0 | 801 |
| D | `src/thirdparty/libwebsockets/lib/secure-streams/secure-streams-serialize.c` | — | 0 | 1510 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/secure-streams.c` | — | 678 | 119 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/CMakeLists.txt` | — | 27 | 0 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/README.md` | — | 89 | 0 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/sspc-deserialize.c` | — | 1025 | 0 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/sspc-transport-wsi.c` | — | 288 | 0 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/sspc-transport.c` | — | 515 | 0 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/client/sspc.c` | — | 851 | 0 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/proxy/proxy-deserialize.c` | — | 858 | 0 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/proxy/proxy-transport-wsi.c` | — | 292 | 0 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/proxy/proxy-transport.c` | — | 421 | 0 |
| A | `src/thirdparty/libwebsockets/lib/secure-streams/serialized/proxy/proxy.c` | — | 492 | 0 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/system/auth-api.amazon.com/auth.c` | — | 2 | 3 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/system/auth-sigv4/sign.c` | — | 8 | 3 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/system/captive-portal-detect/captive-portal-detect.c` | — | 4 | 5 |
| M | `src/thirdparty/libwebsockets/lib/secure-streams/system/fetch-policy/fetch-policy.c` | — | 3 | 3 |
| M | `src/thirdparty/libwebsockets/lib/system/CMakeLists.txt` | — | 15 | 3 |
| M | `src/thirdparty/libwebsockets/lib/system/async-dns/async-dns-parse.c` | — | 72 | 32 |
| M | `src/thirdparty/libwebsockets/lib/system/async-dns/async-dns.c` | — | 467 | 148 |
| M | `src/thirdparty/libwebsockets/lib/system/async-dns/private-lib-async-dns.h` | — | 29 | 8 |
| M | `src/thirdparty/libwebsockets/lib/system/dhcpclient/dhcpc4.c` | — | 5 | 2 |
| M | `src/thirdparty/libwebsockets/lib/system/fault-injection/fault-injection.c` | — | 82 | 10 |
| M | `src/thirdparty/libwebsockets/lib/system/metrics/metrics.c` | — | 10 | 7 |
| M | `src/thirdparty/libwebsockets/lib/system/ntpclient/ntpclient.c` | — | 1 | 1 |
| A | `src/thirdparty/libwebsockets/lib/system/ota/ota.c` | — | 735 | 0 |
| M | `src/thirdparty/libwebsockets/lib/system/smd/private-lib-system-smd.h` | — | 2 | 2 |
| M | `src/thirdparty/libwebsockets/lib/system/smd/smd.c` | — | 95 | 46 |
| A | `src/thirdparty/libwebsockets/lib/system/stdin.c` | — | 175 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/CMakeLists.txt` | — | 108 | 46 |
| M | `src/thirdparty/libwebsockets/lib/tls/lws-gencrypto-common.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/tls/lws-genec-common.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/CMakeLists.txt` | — | 5 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/lws-genaes.c` | — | 27 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/lws-genec.c` | — | 42 | 29 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/lws-genhash.c` | — | 16 | 1 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/lws-genrsa.c` | — | 95 | 30 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-client.c` | — | 118 | 40 |
| A | `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-extensions.c` | — | 514 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-server.c` | — | 15 | 7 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-session.c` | — | 60 | 8 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-ssl.c` | — | 4 | 3 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-tls.c` | — | 4 | 3 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/mbedtls-x509.c` | — | 155 | 61 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/private-lib-tls-mbedtls.h` | — | 24 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_code.h` | — | 1 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_methods.h` | — | 4 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_types.h` | — | 16 | 2 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/internal/ssl_x509.h` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/openssl/ssl.h` | — | 41 | 3 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/include/platform/ssl_pm.h` | — | 2 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_cert.c` | — | 2 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_lib.c` | — | 93 | 4 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_methods.c` | — | 4 | 1 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_pkey.c` | — | 3 | 1 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_stack.c` | — | 2 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/library/ssl_x509.c` | — | 7 | 5 |
| M | `src/thirdparty/libwebsockets/lib/tls/mbedtls/wrapper/platform/ssl_pm.c` | — | 173 | 46 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/lws-genaes.c` | — | 2 | 1 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/lws-genec.c` | — | 25 | 17 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/lws-genhash.c` | — | 20 | 3 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/lws-genrsa.c` | — | 11 | 9 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-client.c` | — | 130 | 101 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-server.c` | — | 72 | 95 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-session.c` | — | 11 | 6 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-ssl.c` | — | 31 | 6 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-tls.c` | — | 33 | 61 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/openssl-x509.c` | — | 197 | 14 |
| M | `src/thirdparty/libwebsockets/lib/tls/openssl/private-lib-tls-openssl.h` | — | 37 | 0 |
| A | `src/thirdparty/libwebsockets/lib/tls/private-jit-trust.h` | — | 149 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/private-lib-tls.h` | — | 10 | 6 |
| M | `src/thirdparty/libwebsockets/lib/tls/private-network.h` | — | 5 | 1 |
| M | `src/thirdparty/libwebsockets/lib/tls/tls-client.c` | — | 28 | 86 |
| A | `src/thirdparty/libwebsockets/lib/tls/tls-jit-trust.c` | — | 714 | 0 |
| M | `src/thirdparty/libwebsockets/lib/tls/tls-network.c` | — | 39 | 18 |
| M | `src/thirdparty/libwebsockets/lib/tls/tls-server.c` | — | 6 | 6 |
| M | `src/thirdparty/libwebsockets/lib/tls/tls-sessions.c` | — | 2 | 1 |
| M | `src/thirdparty/libwebsockets/lib/tls/tls.c` | — | 172 | 33 |
| M | `src/thirdparty/libwebsockets/libwebsockets.dox` | — | 2327 | 125 |
| M | `src/thirdparty/libwebsockets/lwsws/CMakeLists.txt` | — | 0 | 1 |
| M | `src/thirdparty/libwebsockets/lwsws/main.c` | — | 1 | 0 |
| D | `src/thirdparty/libwebsockets/minimal-examples/CMakeLists.txt` | — | 0 | 50 |
| D | `src/thirdparty/libwebsockets/minimal-examples/README.md` | — | 0 | 89 |
| D | `src/thirdparty/libwebsockets/minimal-examples/abstract/protocols/smtp-client/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/abstract/protocols/smtp-client/README.md` | — | 0 | 29 |
| D | `src/thirdparty/libwebsockets/minimal-examples/abstract/protocols/smtp-client/main.c` | — | 0 | 126 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/README.md` | — | 0 | 12 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-async-dns/CMakeLists.txt` | — | 0 | 30 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-async-dns/main.c` | — | 0 | 372 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-dhcpc/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-dhcpc/README.md` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-dhcpc/main.c` | — | 0 | 99 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-fts/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-fts/README.md` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-fts/canned-1.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-fts/canned-2.txt` | — | 0 | 42 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-fts/les-mis-utf8.txt` | — | 0 | 14399 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-fts/main.c` | — | 0 | 230 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-fts/the-picture-of-dorian-gray.txt` | — | 0 | 8904 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-gencrypto/CMakeLists.txt` | — | 0 | 28 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-gencrypto/README.md` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-gencrypto/lws-genaes.c` | — | 0 | 825 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-gencrypto/lws-genec.c` | — | 0 | 132 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-gencrypto/main.c` | — | 0 | 50 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-jose/CMakeLists.txt` | — | 0 | 28 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-jose/README.md` | — | 0 | 22 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-jose/jwe.c` | — | 0 | 2297 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-jose/jwk.c` | — | 0 | 350 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-jose/jws.c` | — | 0 | 962 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-jose/main.c` | — | 0 | 56 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lejp/CMakeLists.txt` | — | 0 | 22 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lejp/main.c` | — | 0 | 219 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_dsh/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_dsh/README.md` | — | 0 | 22 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_dsh/main.c` | — | 0 | 453 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_sequencer/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_sequencer/libwebsockets.org.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_sequencer/main.c` | — | 0 | 399 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_smd/CMakeLists.txt` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_smd/main.c` | — | 0 | 312 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_struct-json/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_struct-json/README.md` | — | 0 | 108 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_struct-json/main.c` | — | 0 | 801 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_struct-json/test2.c` | — | 0 | 236 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_struct_sqlite/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_struct_sqlite/README.md` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_struct_sqlite/main.c` | — | 0 | 204 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_tokenize/CMakeLists.txt` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_tokenize/README.md` | — | 0 | 37 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lws_tokenize/main.c` | — | 0 | 739 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lwsac/CMakeLists.txt` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lwsac/README.md` | — | 0 | 22 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-lwsac/main.c` | — | 0 | 83 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-secure-streams/CMakeLists.txt` | — | 0 | 31 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-secure-streams/README.md` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-secure-streams/main.c` | — | 0 | 384 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-smtp_client/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-smtp_client/README.md` | — | 0 | 41 |
| D | `src/thirdparty/libwebsockets/minimal-examples/api-tests/api-test-smtp_client/main.c` | — | 0 | 261 |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/README.md` | — | 0 | 3 |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/minimal-ws-proxy/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/minimal-ws-proxy/README.md` | — | 0 | 38 |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/minimal-ws-proxy/minimal-ws-proxy.c` | — | 0 | 99 |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/minimal-ws-proxy/mount-origin/example.js` | — | 0 | 67 |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/minimal-ws-proxy/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/minimal-ws-proxy/mount-origin/index.html` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/minimal-ws-proxy/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/minimal-ws-proxy/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/client-server/minimal-ws-proxy/protocol_lws_minimal.c` | — | 0 | 240 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/README.md` | — | 0 | 7 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jwe/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jwe/README.md` | — | 0 | 70 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jwe/key-rsa-4096.private` | — | 0 | 1 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jwe/key-rsa-4096.pub` | — | 0 | 1 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jwe/main.c` | — | 0 | 288 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jwk/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jwk/README.md` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jwk/main.c` | — | 0 | 200 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jws/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jws/README.md` | — | 0 | 60 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-jws/main.c` | — | 0 | 215 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-x509/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-x509/README.md` | — | 0 | 59 |
| D | `src/thirdparty/libwebsockets/minimal-examples/crypto/minimal-crypto-x509/main.c` | — | 0 | 204 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-client/README.md` | — | 0 | 4 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-client/minimal-dbus-client/CMakeLists.txt` | — | 0 | 36 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-client/minimal-dbus-client/README.md` | — | 0 | 49 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-client/minimal-dbus-client/minimal-dbus-client.c` | — | 0 | 281 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-client/minimal-dbus-ws-proxy-testclient/CMakeLists.txt` | — | 0 | 33 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-client/minimal-dbus-ws-proxy-testclient/README.md` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-client/minimal-dbus-ws-proxy-testclient/minimal-dbus-ws-proxy-testclient.c` | — | 0 | 436 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-server/README.md` | — | 0 | 4 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-server/minimal-dbus-server/CMakeLists.txt` | — | 0 | 36 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-server/minimal-dbus-server/README.md` | — | 0 | 96 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-server/minimal-dbus-server/main.c` | — | 0 | 535 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-server/minimal-dbus-ws-proxy/CMakeLists.txt` | — | 0 | 38 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-server/minimal-dbus-ws-proxy/README.md` | — | 0 | 115 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-server/minimal-dbus-ws-proxy/main.c` | — | 0 | 101 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-server/minimal-dbus-ws-proxy/org.libwebsockets.wsclientproxy.conf` | — | 0 | 14 |
| D | `src/thirdparty/libwebsockets/minimal-examples/dbus-server/minimal-dbus-ws-proxy/protocol_lws_minimal_dbus_ws_proxy.c` | — | 0 | 795 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/CMakeLists.txt` | — | 0 | 36 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/lws-button.c` | — | 0 | 497 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/CMakeLists.txt` | — | 0 | 6 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/bb-i2c.c` | — | 0 | 107 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/bb-i2c.h` | — | 0 | 51 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/component.mk` | — | 0 | 5 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/devices.c` | — | 0 | 187 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/gpio-esp32.c` | — | 0 | 36 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/gpio-esp32.h` | — | 0 | 13 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/gpio.h` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/i2c.c` | — | 0 | 32 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/i2c.h` | — | 0 | 35 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/policy.h` | — | 0 | 134 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/main/ssd1306.h` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/partitions.csv` | — | 0 | 5 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/private-lib-plat-freertos.h` | — | 0 | 131 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/sdkconfig` | — | 0 | 1301 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-c3dev/sdkconfig.h` | — | 0 | 426 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/CMakeLists.txt` | — | 0 | 35 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/banded-img.h` | — | 0 | 64 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/main/CMakeLists.txt` | — | 0 | 6 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/main/devices.c` | — | 0 | 207 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/main/policy.h` | — | 0 | 90 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/partitions.csv` | — | 0 | 5 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/scan/output.bmp` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/scan/pic.h` | — | 0 | 64 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/scan/pic.h.1` | — | 0 | 64 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/scan/scan` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/scan/scan.c` | — | 0 | 70 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/scan/scan.sh` | — | 0 | 8 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/sdkconfig` | — | 0 | 1152 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-heltec-wb32/sdkconfig.h` | — | 0 | 426 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/CMakeLists.txt` | — | 0 | 33 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/main/CMakeLists.txt` | — | 0 | 7 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/main/cat-565.h` | — | 0 | 19200 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/main/devices.c` | — | 0 | 272 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/main/policy.h` | — | 0 | 87 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/main/static-policy.h` | — | 0 | 228 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/partitions.csv` | — | 0 | 5 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/rgb565/rgb565.c` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/sdkconfig` | — | 0 | 1151 |
| D | `src/thirdparty/libwebsockets/minimal-examples/embedded/esp32/esp-wrover-kit/sdkconfig.h` | — | 0 | 426 |
| D | `src/thirdparty/libwebsockets/minimal-examples/gtk/minimal-gtk/CMakeLists.txt` | — | 0 | 47 |
| D | `src/thirdparty/libwebsockets/minimal-examples/gtk/minimal-gtk/README.md` | — | 0 | 32 |
| D | `src/thirdparty/libwebsockets/minimal-examples/gtk/minimal-gtk/main.c` | — | 0 | 210 |
| D | `src/thirdparty/libwebsockets/minimal-examples/gtk/minimal-gtk/warmcat.com.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/README.md` | — | 0 | 8 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-attach/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-attach/README.md` | — | 0 | 35 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-attach/minimal-http-client-attach.c` | — | 0 | 277 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-captive-portal/CMakeLists.txt` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-captive-portal/README.md` | — | 0 | 45 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-captive-portal/minimal-http-client-captive-portal.c` | — | 0 | 321 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-certinfo/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-certinfo/README.md` | — | 0 | 77 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-certinfo/minimal-http-client-certinfo.c` | — | 0 | 231 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-certinfo/warmcat.com.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-custom-headers/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-custom-headers/README.md` | — | 0 | 45 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-custom-headers/minimal-http-client-custom-headers.c` | — | 0 | 228 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-custom-headers/warmcat.com.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-h2-rxflow/CMakeLists.txt` | — | 0 | 35 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-h2-rxflow/README.md` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-h2-rxflow/minimal-http-client.c` | — | 0 | 303 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-h2-rxflow/warmcat.com.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-hugeurl/CMakeLists.txt` | — | 0 | 49 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-hugeurl/README.md` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-hugeurl/minimal-http-client-hugeurl.c` | — | 0 | 222 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-hugeurl/warmcat.com.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-multi/CMakeLists.txt` | — | 0 | 202 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-multi/README.md` | — | 0 | 30 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-multi/minimal-http-client-multi.c` | — | 0 | 702 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-multi/warmcat.com.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-post/CMakeLists.txt` | — | 0 | 109 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-post/README.md` | — | 0 | 74 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-post/libwebsockets.org.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client-post/minimal-http-client-post.c` | — | 0 | 277 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client/CMakeLists.txt` | — | 0 | 156 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client/README.md` | — | 0 | 76 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client/minimal-http-client.c` | — | 0 | 413 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-client/minimal-http-client/warmcat.com.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/README.md` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/README.md` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/ba-passwords` | — | 0 | 1 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/minimal-http-server-basicauth.c` | — | 0 | 113 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/mount-origin/index.html` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/mount-secret-origin/index.html` | — | 0 | 11 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-basicauth/mount-secret-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-cgi/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-cgi/README.md` | — | 0 | 28 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-cgi/minimal-http-server.c` | — | 0 | 105 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-cgi/my-cgi-script.sh` | — | 0 | 36 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/README.md` | — | 0 | 20 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/minimal-http-server-custom-headers.c` | — | 0 | 223 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/mount-origin/index.html` | — | 0 | 20 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-custom-headers/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/CMakeLists.txt` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/README.md` | — | 0 | 49 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/ba-passwords` | — | 0 | 2 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/minimal-http-server-deaddrop.c` | — | 0 | 173 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/mount-origin/deaddrop.css` | — | 0 | 70 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/mount-origin/deaddrop.js` | — | 0 | 296 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/mount-origin/drop.svg` | — | 0 | 102 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/mount-origin/index.html` | — | 0 | 35 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-deaddrop/uploads/user1/placeholder.txt` | — | 0 | 1 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/README.md` | — | 0 | 20 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/minimal-http-server-dynamic.c` | — | 0 | 343 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/mount-origin/index.html` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-dynamic/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/README.md` | — | 0 | 30 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/minimal-http-server-eventlib-demos.c` | — | 0 | 208 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/candide-uncompressed.zip` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/candide.zip` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/http2.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/leaf.jpg` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/lws-common.js` | — | 0 | 125 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/test.css` | — | 0 | 190 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/test.html` | — | 0 | 261 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/test.js` | — | 0 | 543 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-demos/mount-origin/wss-over-h2.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/CMakeLists.txt` | — | 0 | 185 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/README.md` | — | 0 | 59 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/glib.c` | — | 0 | 96 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/libev.c` | — | 0 | 76 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/libevent.c` | — | 0 | 84 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/libsdevent.c` | — | 0 | 86 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/libuv.c` | — | 0 | 91 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/minimal-http-server-eventlib-foreign.c` | — | 0 | 341 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/mount-origin/index.html` | — | 0 | 16 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/private.h` | — | 0 | 15 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-foreign/uloop.c` | — | 0 | 59 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/README.md` | — | 0 | 33 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/minimal-http-server-eventlib-smp.c` | — | 0 | 171 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/mount-origin/index.html` | — | 0 | 15 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib-smp/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/README.md` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/minimal-http-server-eventlib.c` | — | 0 | 124 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/mount-origin/index.html` | — | 0 | 15 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-eventlib/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-get/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-get/README.md` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-get/minimal-http-server-form-get.c` | — | 0 | 147 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-get/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-get/mount-origin/after-form1.html` | — | 0 | 12 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-get/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-get/mount-origin/index.html` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-get/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-get/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-file/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-file/README.md` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-file/minimal-http-server-form-post-file.c` | — | 0 | 262 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-file/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-file/mount-origin/after-form1.html` | — | 0 | 14 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-file/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-file/mount-origin/index.html` | — | 0 | 29 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-file/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-file/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-lwsac/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-lwsac/README.md` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-lwsac/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-lwsac/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post-lwsac/minimal-http-server-form-post.c` | — | 0 | 219 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/README.md` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/minimal-http-server-form-post.c` | — | 0 | 223 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/mount-origin/after-form1.html` | — | 0 | 12 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/mount-origin/index.html` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-form-post/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/CMakeLists.txt` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/README.md` | — | 0 | 18 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/lws-fts.index` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/minimal-http-server.c` | — | 0 | 126 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/mount-origin/dorian-gray-wikipedia.jpg` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/mount-origin/index.html` | — | 0 | 30 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/mount-origin/lws-fts.css` | — | 0 | 154 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/mount-origin/lws-fts.js` | — | 0 | 220 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-fulltext-search/the-picture-of-dorian-gray.txt` | — | 0 | 8904 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-h2-long-poll/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-h2-long-poll/README.md` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-h2-long-poll/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-h2-long-poll/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-h2-long-poll/minimal-http-server.c` | — | 0 | 158 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-mimetypes/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-mimetypes/README.md` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-mimetypes/minimal-http-server-mimetypes.c` | — | 0 | 94 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-mimetypes/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-mimetypes/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-mimetypes/mount-origin/index.html` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-mimetypes/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-mimetypes/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-mimetypes/mount-origin/test.tar.bz2` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/README.md` | — | 0 | 48 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/minimal-http-server.c` | — | 0 | 187 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost1/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost1/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost1/index.html` | — | 0 | 18 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost1/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost1/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost2/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost2/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost2/index.html` | — | 0 | 17 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost2/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost2/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost3/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost3/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost3/index.html` | — | 0 | 17 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost3/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-multivhost/mount-origin-localhost3/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-proxy/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-proxy/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-proxy/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-proxy/minimal-http-server-proxy.c` | — | 0 | 84 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-proxy/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-proxy/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-proxy/mount-origin/http2.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-proxy/mount-origin/index.html` | — | 0 | 17 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-smp/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-smp/README.md` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-smp/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-smp/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-smp/minimal-http-server-smp.c` | — | 0 | 140 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-smp/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-smp/mount-origin/index.html` | — | 0 | 12 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-smp/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-smp/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse-ring/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse-ring/README.md` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse-ring/minimal-http-server-sse-ring.c` | — | 0 | 403 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse-ring/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse-ring/mount-origin/example.js` | — | 0 | 38 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse-ring/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse-ring/mount-origin/index.html` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse-ring/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse-ring/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/README.md` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/minimal-http-server-sse.c` | — | 0 | 233 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/mount-origin/example.js` | — | 0 | 38 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/mount-origin/index.html` | — | 0 | 20 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-sse/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/README.md` | — | 0 | 64 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/minimal-http-server-tls-80.c` | — | 0 | 136 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/mount-origin/404.html` | — | 0 | 13 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/mount-origin/example.js` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/mount-origin/http2.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/mount-origin/index.html` | — | 0 | 18 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-80/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/README.md` | — | 0 | 60 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/minimal-http-server-tls-mem.c` | — | 0 | 465 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/mount-origin/404.html` | — | 0 | 11 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/mount-origin/example.js` | — | 0 | 22 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/mount-origin/http2.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/mount-origin/index.html` | — | 0 | 17 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls-mem/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/README.md` | — | 0 | 45 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/minimal-http-server-tls.c` | — | 0 | 149 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/mount-origin/404.html` | — | 0 | 11 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/mount-origin/example.js` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/mount-origin/http2.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/mount-origin/index.html` | — | 0 | 17 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server-tls/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server/README.md` | — | 0 | 18 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server/minimal-http-server.c` | — | 0 | 90 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server/mount-origin/index.html` | — | 0 | 15 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/http-server/minimal-http-server/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/README.md` | — | 0 | 4 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client-multi/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client-multi/README.md` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client-multi/minimal-mqtt-client-multi.c` | — | 0 | 443 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client-multi/selftest.sh.broken-on-travis` | — | 0 | 32 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client-multi/warmcat.com.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client-multi/wget-log` | — | 0 | 11 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client/README.md` | — | 0 | 51 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client/minimal-mqtt-client.c` | — | 0 | 349 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client/mosq-ca.crt` | — | 0 | 22 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client/mosq-server.crt` | — | 0 | 20 |
| D | `src/thirdparty/libwebsockets/minimal-examples/mqtt-client/minimal-mqtt-client/mosq-server.key` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/README.md` | — | 0 | 11 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-adopt-tcp/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-adopt-tcp/README.md` | — | 0 | 57 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-adopt-tcp/minimal-raw-adopt-tcp.c` | — | 0 | 190 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-adopt-udp/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-adopt-udp/README.md` | — | 0 | 49 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-adopt-udp/minimal-raw-adopt-udp.c` | — | 0 | 198 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-audio/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-audio/README.md` | — | 0 | 37 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-audio/audio.c` | — | 0 | 211 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/README.md` | — | 0 | 41 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/minimal-raw-fallback-http-server.c` | — | 0 | 149 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/mount-origin/index.html` | — | 0 | 15 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-fallback-http-server/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-file/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-file/README.md` | — | 0 | 48 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-file/minimal-raw-file.c` | — | 0 | 160 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-netcat/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-netcat/README.md` | — | 0 | 38 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-netcat/minimal-raw-netcat.c` | — | 0 | 257 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/CMakeLists.txt` | — | 0 | 31 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/README.md` | — | 0 | 49 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/minimal-raw-proxy-fallback.c` | — | 0 | 136 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/mount-origin/404.html` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/mount-origin/index.html` | — | 0 | 15 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy-fallback/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy/CMakeLists.txt` | — | 0 | 31 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy/README.md` | — | 0 | 41 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-proxy/minimal-raw-proxy.c` | — | 0 | 91 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-serial/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-serial/README.md` | — | 0 | 46 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-serial/minimal-raw-file.c` | — | 0 | 231 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-vhost/CMakeLists.txt` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-vhost/README.md` | — | 0 | 42 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-vhost/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-vhost/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/raw/minimal-raw-vhost/minimal-raw-vhost.c` | — | 0 | 160 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/README.md` | — | 0 | 14 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-alexa/CMakeLists.txt` | — | 0 | 43 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-alexa/README.md` | — | 0 | 77 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-alexa/alexa.c` | — | 0 | 675 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-alexa/alexa_linux.ppn` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-alexa/audio.c` | — | 0 | 469 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-alexa/main.c` | — | 0 | 421 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-alexa/porcupine_params.pv` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-alexa/private.h` | — | 0 | 8 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-avs/CMakeLists.txt` | — | 0 | 41 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-avs/avs.c` | — | 0 | 432 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-avs/main-client.c` | — | 0 | 123 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-avs/main.c` | — | 0 | 366 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-avs/year.wav` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-blob/CMakeLists.txt` | — | 0 | 134 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-blob/README.md` | — | 0 | 67 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-blob/minimal-secure-streams.c` | — | 0 | 632 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-client-tx/CMakeLists.txt` | — | 0 | 76 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-client-tx/README.md` | — | 0 | 40 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-client-tx/minimal-secure-streams-client-tx.c` | — | 0 | 207 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-cpp/CMakeLists.txt` | — | 0 | 50 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-cpp/main.cxx` | — | 0 | 107 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-hugeurl/CMakeLists.txt` | — | 0 | 133 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-hugeurl/README.md` | — | 0 | 72 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-hugeurl/minimal-secure-streams.c` | — | 0 | 445 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-metadata/CMakeLists.txt` | — | 0 | 41 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-metadata/README.md` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-metadata/minimal-secure-streams.c` | — | 0 | 332 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-metrics-proxy/metrics-proxy-policy.json` | — | 0 | 59 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-perf/CMakeLists.txt` | — | 0 | 133 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-perf/README.md` | — | 0 | 104 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-perf/minimal-secure-streams.c` | — | 0 | 545 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-policy2c/CMakeLists.txt` | — | 0 | 28 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-policy2c/README.md` | — | 0 | 49 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-policy2c/minimal-secure-streams.c` | — | 0 | 671 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-post/CMakeLists.txt` | — | 0 | 49 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-post/README.md` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-post/minimal-secure-streams-post.c` | — | 0 | 564 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-proxy/CMakeLists.txt` | — | 0 | 28 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-proxy/README.md` | — | 0 | 33 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-proxy/main.c` | — | 0 | 309 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-seq/CMakeLists.txt` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-seq/README.md` | — | 0 | 65 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-seq/minimal-secure-streams.c` | — | 0 | 399 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-server-raw/CMakeLists.txt` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-server-raw/README.md` | — | 0 | 54 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-server-raw/main.c` | — | 0 | 104 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-server-raw/ss-server.c` | — | 0 | 111 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-server/CMakeLists.txt` | — | 0 | 28 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-server/README.md` | — | 0 | 72 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-server/main.c` | — | 0 | 292 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-server/ss-client.c` | — | 0 | 86 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-server/ss-server.c` | — | 0 | 237 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-sigv4/CMakeLists.txt` | — | 0 | 104 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-sigv4/README.md` | — | 0 | 46 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-sigv4/ss-s3-main.c` | — | 0 | 245 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-sigv4/ss-s3-put.h` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-sigv4/ss-s3-ss.c` | — | 0 | 218 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-smd/CMakeLists.txt` | — | 0 | 145 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-smd/README.md` | — | 0 | 132 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-smd/minimal-secure-streams-smd.c` | — | 0 | 364 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-smd/multi.c` | — | 0 | 419 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-staticpolicy/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-staticpolicy/README.md` | — | 0 | 61 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-staticpolicy/minimal-secure-streams.c` | — | 0 | 287 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-staticpolicy/static-policy.h` | — | 0 | 1513 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-staticpolicy/static-policy.json` | — | 0 | 218 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-testsfail/CMakeLists.txt` | — | 0 | 104 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-testsfail/README.md` | — | 0 | 17 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams-testsfail/minimal-secure-streams-testsfail.c` | — | 0 | 864 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams/CMakeLists.txt` | — | 0 | 133 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams/README.md` | — | 0 | 67 |
| D | `src/thirdparty/libwebsockets/minimal-examples/secure-streams/minimal-secure-streams/minimal-secure-streams.c` | — | 0 | 562 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/README.md` | — | 0 | 9 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-binance/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-binance/README.md` | — | 0 | 67 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-binance/main.c` | — | 0 | 382 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-echo/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-echo/README.md` | — | 0 | 37 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-echo/minimal-ws-client-echo.c` | — | 0 | 172 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-echo/protocol_lws_minimal_client_echo.c` | — | 0 | 283 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-ping/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-ping/README.md` | — | 0 | 117 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-ping/libwebsockets.org.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-ping/minimal-ws-client-ping.c` | — | 0 | 163 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-pmd-bulk/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-pmd-bulk/README.md` | — | 0 | 164 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-pmd-bulk/minimal-ws-client-pmd-bulk.c` | — | 0 | 132 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-pmd-bulk/protocol_lws_minimal_pmd_bulk.c` | — | 0 | 282 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-rx/CMakeLists.txt` | — | 0 | 32 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-rx/README.md` | — | 0 | 39 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-rx/libwebsockets.org.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-rx/minimal-ws-client.c` | — | 0 | 151 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-spam-tx-rx/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-spam-tx-rx/libwebsockets.org.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-spam-tx-rx/minimal-ws-client.c` | — | 0 | 221 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-spam/CMakeLists.txt` | — | 0 | 86 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-spam/README.md` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-spam/libwebsockets.org.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-spam/minimal-ws-client-spam.c` | — | 0 | 286 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-tx/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-tx/README.md` | — | 0 | 33 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client-tx/minimal-ws-client.c` | — | 0 | 346 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client/README.md` | — | 0 | 58 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client/libwebsockets.org.cer` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-client/minimal-ws-client/minimal-ws-client.c` | — | 0 | 216 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/README.md` | — | 0 | 13 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-broker/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-broker/README.md` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-broker/minimal-ws-broker.c` | — | 0 | 97 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-broker/mount-origin/example.js` | — | 0 | 80 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-broker/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-broker/mount-origin/index.html` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-broker/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-broker/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-broker/protocol_lws_minimal.c` | — | 0 | 217 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/README.md` | — | 0 | 54 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/minimal-ws-raw-proxy.c` | — | 0 | 459 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/mount-origin/example.js` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/mount-origin/index.html` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-raw-proxy/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-echo/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-echo/README.md` | — | 0 | 30 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-echo/minimal-ws-server-echo.c` | — | 0 | 118 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-echo/protocol_lws_minimal_server_echo.c` | — | 0 | 232 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-bulk/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-bulk/README.md` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-bulk/minimal-ws-server-pmd-bulk.c` | — | 0 | 143 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-bulk/mount-origin/example.js` | — | 0 | 62 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-bulk/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-bulk/mount-origin/index.html` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-bulk/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-bulk/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-bulk/protocol_lws_minimal_pmd_bulk.c` | — | 0 | 223 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-corner/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-corner/README.md` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-corner/minimal-ws-server-pmd-corner.c` | — | 0 | 108 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-corner/mount-origin/example.js` | — | 0 | 85 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-corner/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-corner/mount-origin/index.html` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-corner/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-corner/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd-corner/protocol_lws_minimal.c` | — | 0 | 271 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd/CMakeLists.txt` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd/README.md` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd/minimal-ws-server-pmd.c` | — | 0 | 108 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd/mount-origin/example.js` | — | 0 | 68 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd/mount-origin/index.html` | — | 0 | 21 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-pmd/protocol_lws_minimal.c` | — | 0 | 160 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-ring/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-ring/README.md` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-ring/minimal-ws-server-ring.c` | — | 0 | 97 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-ring/mount-origin/example.js` | — | 0 | 65 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-ring/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-ring/mount-origin/index.html` | — | 0 | 20 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-ring/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-ring/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-ring/protocol_lws_minimal.c` | — | 0 | 281 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threadpool/CMakeLists.txt` | — | 0 | 27 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threadpool/README.md` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threadpool/minimal-ws-server-threadpool.c` | — | 0 | 137 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threadpool/mount-origin/example.js` | — | 0 | 76 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threadpool/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threadpool/mount-origin/index.html` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threadpool/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threadpool/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threadpool/protocol_lws_minimal_threadpool.c` | — | 0 | 324 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-foreign-libuv-smp/CMakeLists.txt` | — | 0 | 44 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-foreign-libuv-smp/README.md` | — | 0 | 39 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-foreign-libuv-smp/minimal-ws-server.c` | — | 0 | 204 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-foreign-libuv-smp/mount-origin/example.js` | — | 0 | 68 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-foreign-libuv-smp/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-foreign-libuv-smp/mount-origin/index.html` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-foreign-libuv-smp/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-foreign-libuv-smp/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-foreign-libuv-smp/protocol_lws_minimal.c` | — | 0 | 321 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-smp/CMakeLists.txt` | — | 0 | 26 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-smp/README.md` | — | 0 | 39 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-smp/minimal-ws-server.c` | — | 0 | 154 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-smp/mount-origin/example.js` | — | 0 | 68 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-smp/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-smp/mount-origin/index.html` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-smp/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-smp/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads-smp/protocol_lws_minimal.c` | — | 0 | 302 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads/CMakeLists.txt` | — | 0 | 29 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads/README.md` | — | 0 | 25 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads/minimal-ws-server.c` | — | 0 | 135 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads/mount-origin/example.js` | — | 0 | 68 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads/mount-origin/index.html` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-threads/protocol_lws_minimal.c` | — | 0 | 297 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/README.md` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/minimal-ws-server.c` | — | 0 | 144 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/mount-origin/example.js` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/mount-origin/index.html` | — | 0 | 16 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server-timer/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/CMakeLists.txt` | — | 0 | 24 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/README.md` | — | 0 | 30 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/localhost-100y.cert` | — | 0 | 34 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/localhost-100y.key` | — | 0 | 52 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/minimal-ws-server.c` | — | 0 | 118 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/mount-origin/example.js` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/mount-origin/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/mount-origin/index.html` | — | 0 | 19 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/mount-origin/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/mount-origin/strict-csp.svg` | — | 0 | 53 |
| D | `src/thirdparty/libwebsockets/minimal-examples/ws-server/minimal-ws-server/protocol_lws_minimal.c` | — | 0 | 154 |
| D | `src/thirdparty/libwebsockets/plugin-standalone/CMakeLists.txt` | — | 0 | 75 |
| D | `src/thirdparty/libwebsockets/plugin-standalone/protocol_example_standalone.c` | — | 0 | 148 |
| M | `src/thirdparty/libwebsockets/plugins/CMakeLists.txt` | — | 8 | 0 |
| M | `src/thirdparty/libwebsockets/plugins/acme-client/protocol_lws_acme_client.c` | — | 132 | 152 |
| A | `src/thirdparty/libwebsockets/plugins/deaddrop/assets/BungeeTint-Regular.ttf` | — | - | - |
| A | `src/thirdparty/libwebsockets/plugins/deaddrop/assets/deaddrop-title.png` | — | - | - |
| M | `src/thirdparty/libwebsockets/plugins/deaddrop/assets/deaddrop.css` | — | 86 | 11 |
| M | `src/thirdparty/libwebsockets/plugins/deaddrop/assets/deaddrop.js` | — | 232 | 83 |
| M | `src/thirdparty/libwebsockets/plugins/deaddrop/assets/index.html` | — | 48 | 24 |
| M | `src/thirdparty/libwebsockets/plugins/deaddrop/protocol_lws_deaddrop.c` | — | 625 | 372 |
| M | `src/thirdparty/libwebsockets/plugins/protocol_client_loopback_test.c` | — | 1 | 0 |
| M | `src/thirdparty/libwebsockets/plugins/protocol_fulltext_demo.c` | — | 2 | 2 |
| M | `src/thirdparty/libwebsockets/plugins/protocol_lws_openmetrics_export.c` | — | 13 | 14 |
| M | `src/thirdparty/libwebsockets/plugins/protocol_lws_raw_test.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/plugins/protocol_lws_status.c` | — | 2 | 2 |
| M | `src/thirdparty/libwebsockets/plugins/protocol_post_demo.c` | — | 11 | 3 |
| A | `src/thirdparty/libwebsockets/plugins/protocol_urlarg.c` | — | 148 | 0 |
| M | `src/thirdparty/libwebsockets/plugins/raw-proxy/protocol_lws_raw_proxy.c` | — | 2 | 2 |
| M | `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/chacha.c` | — | 2 | 2 |
| M | `src/thirdparty/libwebsockets/plugins/ssh-base/crypto/sc25519.c` | — | 2 | 2 |
| M | `src/thirdparty/libwebsockets/plugins/ssh-base/include/lws-ssh.h` | — | 9 | 0 |
| M | `src/thirdparty/libwebsockets/plugins/ssh-base/sshd.c` | — | 62 | 18 |
| M | `src/thirdparty/libwebsockets/plugins/ssh-base/telnet.c` | — | 5 | 2 |
| A | `src/thirdparty/libwebsockets/scripts/ahrefs-topsites.sh` | — | 6 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/attack.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/autobahn-test-client.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/autobahn-test-server.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/build-gcov.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/client-ca/create-ca.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/client-ca/create-client-cert.sh` | — | 2 | 1 |
| M | `src/thirdparty/libwebsockets/scripts/client-ca/create-server-cert.sh` | — | 1 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/ctest-background-kill.sh` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/scripts/ctest-background.sh` | — | 23 | 2 |
| M | `src/thirdparty/libwebsockets/scripts/gcov.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/h2load-smp.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/h2load.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/h2spec.sh` | — | 0 | 0 |
| A | `src/thirdparty/libwebsockets/scripts/mozilla-trust-gen.sh` | — | 194 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/test-dbus-proxy.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/travis_control.sh` | — | 0 | 0 |
| M | `src/thirdparty/libwebsockets/scripts/travis_install.sh` | — | 0 | 0 |
| D | `src/thirdparty/libwebsockets/test-apps/1.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/2.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/3.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/4.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/5.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/6.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/7.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/8.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/CMakeLists.txt` | — | 0 | 259 |
| D | `src/thirdparty/libwebsockets/test-apps/android/README` | — | 0 | 51 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/app.iml` | — | 0 | 115 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/build.gradle` | — | 0 | 42 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/AndroidManifest.xml` | — | 0 | 29 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/java/org/libwebsockets/client/LwsService.java` | — | 0 | 132 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/java/org/libwebsockets/client/MainActivity.java` | — | 0 | 246 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/java/org/libwebsockets/client/ThreadService.java` | — | 0 | 284 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/jni/Android.mk` | — | 0 | 41 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/jni/Application.mk` | — | 0 | 49 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/jni/LwsService.cpp` | — | 0 | 307 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/jni/NativeLibs.mk` | — | 0 | 1324 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/drawable/warmcat.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/layout/activity_main.xml` | — | 0 | 81 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/mipmap-hdpi/ic_launcher.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/mipmap-mdpi/ic_launcher.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/mipmap-xhdpi/ic_launcher.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/mipmap-xxhdpi/ic_launcher.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/values/colors.xml` | — | 0 | 6 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/values/dimens.xml` | — | 0 | 5 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/values/strings.xml` | — | 0 | 5 |
| D | `src/thirdparty/libwebsockets/test-apps/android/app/src/main/res/values/styles.xml` | — | 0 | 11 |
| D | `src/thirdparty/libwebsockets/test-apps/android/build.gradle` | — | 0 | 23 |
| D | `src/thirdparty/libwebsockets/test-apps/android/gradle.properties` | — | 0 | 20 |
| D | `src/thirdparty/libwebsockets/test-apps/android/settings.gradle` | — | 0 | 1 |
| D | `src/thirdparty/libwebsockets/test-apps/candide-uncompressed.zip` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/candide.zip` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/favicon.ico` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/http2.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/leaf.jpg` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/libwebsockets-test-server.service` | — | 0 | 12 |
| D | `src/thirdparty/libwebsockets/test-apps/libwebsockets.org-logo.png` | — | - | - |
| D | `src/thirdparty/libwebsockets/test-apps/libwebsockets.org-logo.svg` | — | 0 | 66 |
| D | `src/thirdparty/libwebsockets/test-apps/lws-cgi-test.sh` | — | 0 | 36 |
| D | `src/thirdparty/libwebsockets/test-apps/lws-common.js` | — | 0 | 125 |
| D | `src/thirdparty/libwebsockets/test-apps/lws-ssh-test-keys` | — | 0 | 51 |
| D | `src/thirdparty/libwebsockets/test-apps/lws-ssh-test-keys.pub` | — | 0 | 1 |
| D | `src/thirdparty/libwebsockets/test-apps/private/index.html` | — | 0 | 1 |
| D | `src/thirdparty/libwebsockets/test-apps/test-client.c` | — | 0 | 893 |
| D | `src/thirdparty/libwebsockets/test-apps/test-lejp.c` | — | 0 | 121 |
| D | `src/thirdparty/libwebsockets/test-apps/test-server.c` | — | 0 | 751 |
| D | `src/thirdparty/libwebsockets/test-apps/test-sshd.c` | — | 0 | 788 |
| D | `src/thirdparty/libwebsockets/test-apps/test.css` | — | 0 | 190 |
| D | `src/thirdparty/libwebsockets/test-apps/test.html` | — | 0 | 258 |
| D | `src/thirdparty/libwebsockets/test-apps/test.js` | — | 0 | 543 |
| D | `src/thirdparty/libwebsockets/test-apps/wss-over-h2.png` | — | - | - |
| M | `src/thirdparty/libwebsockets/win32port/zlib/deflate.c` | — | 1 | 1 |
| M | `src/thirdparty/libwebsockets/win32port/zlib/inflate.c` | — | 3 | 2 |
| M | `src/thirdparty/libwebsockets/win32port/zlib/inftrees.c` | — | 8 | 10 |
| M | `src/thirdparty/libwebsockets/win32port/zlib/zlib.h` | — | 2 | 0 |
| M | `src/tools/CMakeLists.txt` | — | 9 | 3 |
| M | `src/tools/build_applies.cc` | — | 9 | 9 |
| M | `src/tools/build_packages_genfiles.sh` | — | 59 | 2 |
| A | `src/tools/make_func.autogen.cc` | — | 2007 | 0 |
| M | `src/tools/make_func.y` | — | 84 | 62 |
| M | `src/tools/make_options_defs.cc` | — | 6 | 3 |
| M | `src/tools/preprocessor.hpp` | — | 244 | 166 |
| M | `src/user.cc` | — | 11 | 8 |
| M | `src/user.h` | — | 4 | 4 |
| A | `src/vm/context.h` | — | 222 | 0 |
| A | `src/vm/frozen_value.h` | — | 32 | 0 |
| M | `src/vm/internal/applies` | — | 5 | 0 |
| M | `src/vm/internal/apply.cc` | — | 224 | 65 |
| M | `src/vm/internal/apply.h` | — | 16 | 5 |
| M | `src/vm/internal/base/apply_cache.cc` | — | 202 | 4 |
| M | `src/vm/internal/base/apply_cache.h` | — | 10 | 1 |
| M | `src/vm/internal/base/array.cc` | — | 269 | 167 |
| M | `src/vm/internal/base/array.h` | — | 44 | 43 |
| M | `src/vm/internal/base/buffer.cc` | — | 23 | 16 |
| M | `src/vm/internal/base/buffer.h` | — | 5 | 5 |
| M | `src/vm/internal/base/class.cc` | — | 23 | 13 |
| M | `src/vm/internal/base/class.h` | — | 8 | 6 |
| M | `src/vm/internal/base/debug.cc` | — | 15 | 13 |
| M | `src/vm/internal/base/debug.h` | — | 1 | 1 |
| M | `src/vm/internal/base/function.cc` | — | 182 | 56 |
| M | `src/vm/internal/base/function.h` | — | 51 | 14 |
| M | `src/vm/internal/base/interpret.cc` | — | 1456 | 664 |
| M | `src/vm/internal/base/interpret.h` | — | 138 | 88 |
| M | `src/vm/internal/base/machine.h` | — | 30 | 27 |
| M | `src/vm/internal/base/mapping.cc` | — | 213 | 129 |
| M | `src/vm/internal/base/mapping.h` | — | 49 | 44 |
| M | `src/vm/internal/base/object.cc` | — | 560 | 298 |
| M | `src/vm/internal/base/object.h` | — | 110 | 50 |
| M | `src/vm/internal/base/old_qsort_inc.h` | — | 10 | 10 |
| M | `src/vm/internal/base/program.cc` | — | 218 | 10 |
| M | `src/vm/internal/base/program.h` | — | 66 | 33 |
| A | `src/vm/internal/base/promise.cc` | — | 2145 | 0 |
| A | `src/vm/internal/base/promise.h` | — | 255 | 0 |
| A | `src/vm/internal/base/scoped_current_object_as_master.h` | — | 57 | 0 |
| M | `src/vm/internal/base/svalue.cc` | — | 149 | 25 |
| M | `src/vm/internal/base/svalue.h` | — | 185 | 77 |
| A | `src/vm/internal/context.cc` | — | 374 | 0 |
| M | `src/vm/internal/eval_limit.cc` | — | 1 | 1 |
| M | `src/vm/internal/eval_limit.h` | — | 4 | 1 |
| A | `src/vm/internal/frozen_value.cc` | — | 323 | 0 |
| A | `src/vm/internal/layout_digest.h` | — | 53 | 0 |
| A | `src/vm/internal/lpc_vm_profile.cc` | — | 239 | 0 |
| A | `src/vm/internal/lpc_vm_profile.h` | — | 73 | 0 |
| M | `src/vm/internal/master.cc` | — | 16 | 14 |
| M | `src/vm/internal/master.h` | — | 6 | 6 |
| A | `src/vm/internal/object_store.cc` | — | 2723 | 0 |
| M | `src/vm/internal/otable.cc` | — | 23 | 16 |
| M | `src/vm/internal/otable.h` | — | 1 | 1 |
| M | `src/vm/internal/otable_test.cc` | — | 1 | 1 |
| A | `src/vm/internal/owner.cc` | — | 6800 | 0 |
| A | `src/vm/internal/owner_executor.cc` | — | 33 | 0 |
| A | `src/vm/internal/owner_executor.h` | — | 28 | 0 |
| A | `src/vm/internal/owner_future_store.cc` | — | 728 | 0 |
| A | `src/vm/internal/owner_future_store.h` | — | 214 | 0 |
| A | `src/vm/internal/owner_runtime_coordinator.cc` | — | 207 | 0 |
| A | `src/vm/internal/owner_runtime_coordinator.h` | — | 129 | 0 |
| A | `src/vm/internal/owner_runtime_metrics.cc` | — | 10 | 0 |
| A | `src/vm/internal/owner_runtime_metrics.h` | — | 132 | 0 |
| A | `src/vm/internal/owner_scheduler_state.cc` | — | 444 | 0 |
| A | `src/vm/internal/owner_scheduler_state.h` | — | 193 | 0 |
| A | `src/vm/internal/owner_service_registry.cc` | — | 153 | 0 |
| A | `src/vm/internal/owner_service_registry.h` | — | 36 | 0 |
| A | `src/vm/internal/owner_task_manifest.cc` | — | 263 | 0 |
| A | `src/vm/internal/owner_task_manifest.h` | — | 84 | 0 |
| A | `src/vm/internal/owner_trace_store.cc` | — | 198 | 0 |
| A | `src/vm/internal/owner_trace_store.h` | — | 179 | 0 |
| M | `src/vm/internal/posix_timers.cc` | — | 103 | 51 |
| A | `src/vm/internal/recompile.cc` | — | 913 | 0 |
| A | `src/vm/internal/recompile.h` | — | 306 | 0 |
| A | `src/vm/internal/recompile_layout.cc` | — | 434 | 0 |
| A | `src/vm/internal/recompile_layout.h` | — | 127 | 0 |
| M | `src/vm/internal/simul_efun.cc` | — | 395 | 66 |
| M | `src/vm/internal/simul_efun.h` | — | 89 | 4 |
| M | `src/vm/internal/simulate.cc` | — | 683 | 211 |
| M | `src/vm/internal/simulate.h` | — | 52 | 36 |
| A | `src/vm/internal/source_spelling.cc` | — | 62 | 0 |
| A | `src/vm/internal/source_spelling.h` | — | 60 | 0 |
| M | `src/vm/internal/trace.cc` | — | 112 | 47 |
| M | `src/vm/internal/trace.h` | — | 2 | 2 |
| M | `src/vm/internal/vm.cc` | — | 84 | 18 |
| A | `src/vm/internal/worker.cc` | — | 1100 | 0 |
| A | `src/vm/object_handle.h` | — | 191 | 0 |
| A | `src/vm/owner.h` | — | 252 | 0 |
| M | `src/vm/vm.h` | — | 10 | 1 |
| A | `src/vm/worker.h` | — | 173 | 0 |
| A | `testsuite/adm/daemons/gateway_d.c` | — | 22 | 0 |
| A | `testsuite/clone/a2_void_dots.lpc` | — | 4 | 0 |
| A | `testsuite/clone/async_info_names.lpc` | — | 14 | 0 |
| A | `testsuite/clone/async_inherit_base.lpc` | — | 5 | 0 |
| A | `testsuite/clone/async_inherit_park.lpc` | — | 8 | 0 |
| A | `testsuite/clone/async_inherit_park_child.lpc` | — | 10 | 0 |
| A | `testsuite/clone/bad_macro_params.lpc` | — | 2 | 0 |
| A | `testsuite/clone/clean_up_deadline.lpc` | — | 12 | 0 |
| A | `testsuite/clone/completion_duplicate.lpc` | — | 8 | 0 |
| A | `testsuite/clone/completion_late_failure.lpc` | — | 13 | 0 |
| A | `testsuite/clone/completion_never.lpc` | — | 5 | 0 |
| A | `testsuite/clone/completion_unknown.lpc` | — | 4 | 0 |
| A | `testsuite/clone/compress_file_io.lpc` | — | 7 | 0 |
| A | `testsuite/clone/defarg_child.c` | — | 6 | 0 |
| A | `testsuite/clone/defarg_fp.c` | — | 5 | 0 |
| A | `testsuite/clone/defarg_parent.c` | — | 2 | 0 |
| A | `testsuite/clone/dual_fail_only_lpc.lpc` | — | 5 | 0 |
| A | `testsuite/clone/dual_inherit_c_spelling.lpc` | — | 6 | 0 |
| A | `testsuite/clone/dual_only_c.c` | — | 3 | 0 |
| A | `testsuite/clone/dual_only_lpc.lpc` | — | 3 | 0 |
| A | `testsuite/clone/dual_pick_c.c` | — | 3 | 0 |
| A | `testsuite/clone/dual_pick_c.lpc` | — | 3 | 0 |
| A | `testsuite/clone/dual_pick_default.c` | — | 3 | 0 |
| A | `testsuite/clone/dual_pick_default.lpc` | — | 3 | 0 |
| A | `testsuite/clone/dual_pick_lpc.c` | — | 3 | 0 |
| A | `testsuite/clone/dual_pick_lpc.lpc` | — | 3 | 0 |
| A | `testsuite/clone/dual_retry_child.c` | — | 6 | 0 |
| A | `testsuite/clone/dual_retry_child.lpc` | — | 3 | 0 |
| A | `testsuite/clone/dual_retry_parent.lpc` | — | 3 | 0 |
| A | `testsuite/clone/dwlib_long_name.lpc` | — | 14 | 0 |
| A | `testsuite/clone/file_io_editor.lpc` | — | 12 | 0 |
| A | `testsuite/clone/file_io_state.lpc` | — | 45 | 0 |
| A | `testsuite/clone/gateway_exec_user.c` | — | 35 | 0 |
| A | `testsuite/clone/gateway_login_example.c` | — | 404 | 0 |
| A | `testsuite/clone/gateway_login_exec_example.c` | — | 14 | 0 |
| A | `testsuite/clone/gateway_net_dead_destruct_user.c` | — | 24 | 0 |
| A | `testsuite/clone/include_list_a.h` | — | 3 | 0 |
| A | `testsuite/clone/include_list_b.h` | — | 2 | 0 |
| A | `testsuite/clone/include_list_nest.h` | — | 2 | 0 |
| A | `testsuite/clone/include_list_skipped.h` | — | 3 | 0 |
| A | `testsuite/clone/include_list_src.lpc` | — | 10 | 0 |
| M | `testsuite/clone/login.c` | — | 14 | 16 |
| M | `testsuite/clone/memory_holder.c` | — | 31 | 52 |
| A | `testsuite/clone/move_object_item.lpc` | — | 3 | 0 |
| A | `testsuite/clone/move_object_reset_dest.lpc` | — | 9 | 0 |
| A | `testsuite/clone/priv_alias_grandleaf.lpc` | — | 1 | 0 |
| A | `testsuite/clone/priv_alias_leaf.lpc` | — | 1 | 0 |
| A | `testsuite/clone/priv_alias_leaf_own.lpc` | — | 3 | 0 |
| A | `testsuite/clone/priv_proto_deep.lpc` | — | 3 | 0 |
| A | `testsuite/clone/priv_proto_priv.lpc` | — | 3 | 0 |
| A | `testsuite/clone/priv_proto_pub.lpc` | — | 3 | 0 |
| A | `testsuite/clone/priv_same_ab.lpc` | — | 2 | 0 |
| A | `testsuite/clone/priv_same_ba.lpc` | — | 2 | 0 |
| A | `testsuite/clone/priv_same_child.lpc` | — | 4 | 0 |
| A | `testsuite/clone/reclaim_fp_helper.lpc` | — | 4 | 0 |
| A | `testsuite/clone/recompile_blueprint.c` | — | 34 | 0 |
| A | `testsuite/clone/recompile_lifecycle_probe.c` | — | 53 | 0 |
| A | `testsuite/clone/recompile_simul_compile_probe.c` | — | 4 | 0 |
| A | `testsuite/clone/recompile_unrelated_probe.c` | — | 6 | 0 |
| A | `testsuite/clone/reload_counter.lpc` | — | 6 | 0 |
| A | `testsuite/clone/reload_self_destruct.lpc` | — | 13 | 0 |
| M | `testsuite/clone/replaceable_1.c` | — | 2 | 3 |
| M | `testsuite/clone/replaceable_2.c` | — | 2 | 3 |
| M | `testsuite/clone/user.c` | — | 102 | 132 |
| A | `testsuite/clone/websocket_session_user.lpc` | — | 109 | 0 |
| M | `testsuite/command/callouts.c` | — | 8 | 10 |
| M | `testsuite/command/codefor.c` | — | 17 | 19 |
| M | `testsuite/command/crasher.c` | — | 399 | 399 |
| M | `testsuite/command/dest.c` | — | 24 | 26 |
| M | `testsuite/command/ed.c` | — | 5 | 7 |
| M | `testsuite/command/eval.c` | — | 9 | 11 |
| M | `testsuite/command/external_curl.c` | — | 4 | 5 |
| M | `testsuite/command/gmcp.c` | — | 4 | 5 |
| M | `testsuite/command/more.c` | — | 61 | 93 |
| M | `testsuite/command/msdp.c` | — | 2 | 3 |
| M | `testsuite/command/mxp.c` | — | 2 | 3 |
| M | `testsuite/command/quit.c` | — | 6 | 8 |
| M | `testsuite/command/refs.c` | — | 2 | 2 |
| M | `testsuite/command/rm.c` | — | 4 | 6 |
| M | `testsuite/command/say.c` | — | 4 | 5 |
| M | `testsuite/command/shutdown.c` | — | 3 | 5 |
| M | `testsuite/command/speed.c` | — | 165 | 169 |
| M | `testsuite/command/stress_memory.c` | — | 2 | 2 |
| M | `testsuite/command/stress_string.c` | — | 39 | 34 |
| M | `testsuite/command/test_input_to.c` | — | 9 | 9 |
| M | `testsuite/command/test_tls.c` | — | 2 | 4 |
| M | `testsuite/command/tests.c` | — | 368 | 64 |
| M | `testsuite/command/update.c` | — | 13 | 15 |
| M | `testsuite/command/who.c` | — | 13 | 15 |
| A | `testsuite/data/compress/zipbomb.zbin` | — | - | - |
| A | `testsuite/data/gateway_net_dead_destruct_user.o` | — | 4 | 0 |
| A | `testsuite/etc/config.recompile` | — | 395 | 0 |
| M | `testsuite/etc/config.test` | — | 14 | 5 |
| M | `testsuite/etc/efun_define.c` | — | 1 | 1 |
| A | `testsuite/etc/test-scopes.tsv` | — | 82 | 0 |
| M | `testsuite/etc/vimrc` | — | 1 | 0 |
| A | `testsuite/format.sh` | — | 101 | 0 |
| M | `testsuite/include/tests.h` | — | 1 | 1 |
| M | `testsuite/inherit/base.c` | — | 16 | 24 |
| M | `testsuite/inherit/clean_up.c` | — | 3 | 6 |
| M | `testsuite/inherit/master/valid.c` | — | 104 | 52 |
| M | `testsuite/inherit/nomask.c` | — | 1 | 1 |
| A | `testsuite/inherit/priv_alias_base.lpc` | — | 3 | 0 |
| A | `testsuite/inherit/priv_alias_mid.lpc` | — | 3 | 0 |
| A | `testsuite/inherit/priv_proto_body.lpc` | — | 3 | 0 |
| A | `testsuite/inherit/priv_proto_mid.lpc` | — | 1 | 0 |
| A | `testsuite/inherit/priv_same_a.lpc` | — | 5 | 0 |
| A | `testsuite/inherit/priv_same_b.lpc` | — | 5 | 0 |
| M | `testsuite/inherit/tests.c` | — | 4 | 8 |
| A | `testsuite/single/async_phase2_base.lpc` | — | 4 | 0 |
| A | `testsuite/single/async_phase2_probe.lpc` | — | 31 | 0 |
| M | `testsuite/single/inh.c` | — | 1 | 2 |
| M | `testsuite/single/master.c` | — | 188 | 69 |
| M | `testsuite/single/on_destruct_bad.c` | — | 7 | 9 |
| M | `testsuite/single/on_destruct_good.c` | — | 19 | 7 |
| A | `testsuite/single/owner_singleton.c` | — | 1 | 0 |
| A | `testsuite/single/promise_destruct_probe.lpc` | — | 14 | 0 |
| A | `testsuite/single/recompile_probe.c` | — | 22 | 0 |
| M | `testsuite/single/simul_efun.c` | — | 225 | 151 |
| M | `testsuite/single/tests/applies/on_destruct_test.c` | — | 17 | 17 |
| M | `testsuite/single/tests/compiler/0default_args.c` | — | 3 | 3 |
| M | `testsuite/single/tests/compiler/0default_args_1.c` | — | 3 | 3 |
| M | `testsuite/single/tests/compiler/64bit.c` | — | 27 | 27 |
| A | `testsuite/single/tests/compiler/a2_grammar.lpc` | — | 39 | 0 |
| A | `testsuite/single/tests/compiler/async_phase2_smoke.lpc` | — | 38 | 0 |
| M | `testsuite/single/tests/compiler/at_block.c` | — | 5 | 5 |
| A | `testsuite/single/tests/compiler/bad_macro_params.lpc` | — | 7 | 0 |
| M | `testsuite/single/tests/compiler/class.c` | — | 30 | 31 |
| M | `testsuite/single/tests/compiler/conditional.c` | — | 19 | 19 |
| M | `testsuite/single/tests/compiler/constant_expr.c` | — | 29 | 29 |
| A | `testsuite/single/tests/compiler/deep_definition_chain.lpc` | — | 32 | 0 |
| A | `testsuite/single/tests/compiler/deep_expr_recursion.lpc` | — | 28 | 0 |
| A | `testsuite/single/tests/compiler/deep_nesting_caps.c` | — | 35 | 0 |
| A | `testsuite/single/tests/compiler/deep_statement_chain.lpc` | — | 29 | 0 |
| A | `testsuite/single/tests/compiler/default_args_error_unwind.c` | — | 69 | 0 |
| A | `testsuite/single/tests/compiler/default_args_inherit.c` | — | 30 | 0 |
| M | `testsuite/single/tests/compiler/fail/arr_index.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_$var.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_break.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_continue.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_for_init.c` | — | 1 | 2 |
| M | `testsuite/single/tests/compiler/fail/bad_foreach.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_local_init.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_local_init2.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_param.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_param2.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_return.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_return2.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/bad_string_index.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/class_redef.c` | — | 2 | 2 |
| M | `testsuite/single/tests/compiler/fail/div_zero.c` | — | 2 | 2 |
| M | `testsuite/single/tests/compiler/fail/div_zero3.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/global_modifier.c` | — | 3 | 9 |
| A | `testsuite/single/tests/compiler/fail/inherit_exact_ext.lpc` | — | 3 | 0 |
| M | `testsuite/single/tests/compiler/fail/local_redef.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/local_redef2.c` | — | 3 | 3 |
| M | `testsuite/single/tests/compiler/fail/lvar_func.c` | — | 2 | 2 |
| M | `testsuite/single/tests/compiler/fail/lvar_func2.c` | — | 2 | 2 |
| A | `testsuite/single/tests/compiler/fail/missing_package_efun.c` | — | 5 | 0 |
| M | `testsuite/single/tests/compiler/fail/mixed_case.c` | — | 2 | 2 |
| M | `testsuite/single/tests/compiler/fail/mod_zero.c` | — | 2 | 2 |
| M | `testsuite/single/tests/compiler/fail/no_member.c` | — | 3 | 3 |
| M | `testsuite/single/tests/compiler/fail/no_ret_type.c` | — | 1 | 2 |
| M | `testsuite/single/tests/compiler/fail/no_ret_type.crlf.c` | — | 1 | 2 |
| M | `testsuite/single/tests/compiler/fail/no_such_class.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/redef_function.c` | — | 2 | 4 |
| M | `testsuite/single/tests/compiler/fail/static_local.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/string_case.c` | — | 2 | 2 |
| M | `testsuite/single/tests/compiler/fail/two_default.c` | — | 2 | 2 |
| M | `testsuite/single/tests/compiler/fail/var_to_nonvar.c` | — | 2 | 3 |
| M | `testsuite/single/tests/compiler/fail/void_arg.c` | — | 1 | 2 |
| M | `testsuite/single/tests/compiler/fail/void_for_var.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/void_local.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/fail/void_member.c` | — | 1 | 1 |
| A | `testsuite/single/tests/compiler/foreach_unwind.lpc` | — | 43 | 0 |
| M | `testsuite/single/tests/compiler/function.c` | — | 31 | 32 |
| M | `testsuite/single/tests/compiler/function_callother.c` | — | 1 | 1 |
| M | `testsuite/single/tests/compiler/function_inherit.c` | — | 37 | 37 |
| M | `testsuite/single/tests/compiler/function_inherit_2.c` | — | 0 | 1 |
| M | `testsuite/single/tests/compiler/get_include_path.c` | — | 9 | 9 |
| A | `testsuite/single/tests/compiler/huge_file_line_table.lpc` | — | 40 | 0 |
| A | `testsuite/single/tests/compiler/include_merge_limits.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/compiler/include_path_limits.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/compiler/include_paths.lpc` | — | 132 | 0 |
| M | `testsuite/single/tests/compiler/literal.c` | — | 28 | 28 |
| A | `testsuite/single/tests/compiler/long_local_name.lpc` | — | 33 | 0 |
| M | `testsuite/single/tests/compiler/mixed_declarations.c` | — | 157 | 157 |
| M | `testsuite/single/tests/compiler/optimize.c` | — | 17 | 17 |
| A | `testsuite/single/tests/compiler/positive_compilation.lpc` | — | 11 | 0 |
| A | `testsuite/single/tests/compiler/preprocessor.lpc` | — | 126 | 0 |
| A | `testsuite/single/tests/compiler/private_inherit_alias_chain.lpc` | — | 15 | 0 |
| A | `testsuite/single/tests/compiler/private_inherit_same_name.lpc` | — | 25 | 0 |
| A | `testsuite/single/tests/compiler/private_prototype_descendant.lpc` | — | 10 | 0 |
| M | `testsuite/single/tests/compiler/succeed.c` | — | 6 | 8 |
| M | `testsuite/single/tests/compiler/switch.c` | — | 487 | 487 |
| M | `testsuite/single/tests/compiler/update_simulefun.c` | — | 1 | 3 |
| M | `testsuite/single/tests/compiler/utf8.c` | — | 117 | 108 |
| A | `testsuite/single/tests/compiler/valid_override_file.lpc` | — | 68 | 0 |
| A | `testsuite/single/tests/compiler/warn_%s.lpc` | — | 8 | 0 |
| A | `testsuite/single/tests/compiler/warn_%s_a.lpc` | — | 4 | 0 |
| A | `testsuite/single/tests/compiler/warn_b.lpc` | — | 4 | 0 |
| M | `testsuite/single/tests/crasher/1.c` | — | 1 | 1 |
| M | `testsuite/single/tests/crasher/1063-aux.c` | — | 1 | 2 |
| M | `testsuite/single/tests/crasher/11.c` | — | 3 | 3 |
| M | `testsuite/single/tests/crasher/12.c` | — | 4 | 4 |
| M | `testsuite/single/tests/crasher/14.c` | — | 2 | 2 |
| M | `testsuite/single/tests/crasher/15.c` | — | 3 | 3 |
| M | `testsuite/single/tests/crasher/16b.c` | — | 1 | 2 |
| M | `testsuite/single/tests/crasher/17.c` | — | 2 | 2 |
| M | `testsuite/single/tests/crasher/17b.c` | — | 2 | 2 |
| M | `testsuite/single/tests/crasher/283.c` | — | 2 | 2 |
| M | `testsuite/single/tests/crasher/4.c` | — | 4 | 4 |
| M | `testsuite/single/tests/crasher/5.c` | — | 3 | 3 |
| M | `testsuite/single/tests/crasher/7.c` | — | 3 | 4 |
| M | `testsuite/single/tests/crasher/750.c` | — | 8 | 10 |
| M | `testsuite/single/tests/crasher/773.c` | — | 1 | 1 |
| M | `testsuite/single/tests/crasher/864.c` | — | 8 | 10 |
| M | `testsuite/single/tests/crasher/871.c` | — | 2 | 2 |
| M | `testsuite/single/tests/crasher/9.c` | — | 4 | 4 |
| M | `testsuite/single/tests/crasher/callout0.c` | — | 10 | 5 |
| M | `testsuite/single/tests/crasher/catch.c` | — | 307 | 307 |
| A | `testsuite/single/tests/crasher/living_parse_nonobject.lpc` | — | 15 | 0 |
| A | `testsuite/single/tests/crasher/parser_handler_destruct.lpc` | — | 39 | 0 |
| A | `testsuite/single/tests/crasher/reclaim_funptr_owner.lpc` | — | 23 | 0 |
| A | `testsuite/single/tests/crasher/replaceable_empty.lpc` | — | 14 | 0 |
| A | `testsuite/single/tests/crasher/rp_base.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/crasher/socket_long_host.lpc` | — | 21 | 0 |
| A | `testsuite/single/tests/crasher/terminal_colour_error_replace.lpc` | — | 17 | 0 |
| A | `testsuite/single/tests/efuns/a2_runtime.lpc` | — | 50 | 0 |
| M | `testsuite/single/tests/efuns/abs.c` | — | 9 | 9 |
| A | `testsuite/single/tests/efuns/acatch_defer_catch_value.lpc` | — | 87 | 0 |
| A | `testsuite/single/tests/efuns/acos.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/add_action.c` | — | 23 | 14 |
| D | `testsuite/single/tests/efuns/add_light.c` | — | 0 | 3 |
| M | `testsuite/single/tests/efuns/all_inventory.c` | — | 16 | 19 |
| M | `testsuite/single/tests/efuns/allocate.c` | — | 6 | 6 |
| A | `testsuite/single/tests/efuns/allocate.lpc` | — | 18 | 0 |
| M | `testsuite/single/tests/efuns/allocate_buffer.c` | — | 3 | 4 |
| M | `testsuite/single/tests/efuns/allocate_mapping.c` | — | 8 | 6 |
| A | `testsuite/single/tests/efuns/allocate_mapping_callback_throw.c` | — | 34 | 0 |
| A | `testsuite/single/tests/efuns/angle.lpc` | — | 15 | 0 |
| A | `testsuite/single/tests/efuns/asin.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/assemble_class.lpc` | — | 7 | 0 |
| M | `testsuite/single/tests/efuns/async.c` | — | 65 | 51 |
| A | `testsuite/single/tests/efuns/async_await.lpc` | — | 42 | 0 |
| A | `testsuite/single/tests/efuns/async_db_exec_bound_args.lpc` | — | 44 | 0 |
| A | `testsuite/single/tests/efuns/async_foreach.lpc` | — | 19 | 0 |
| A | `testsuite/single/tests/efuns/async_info_names.lpc` | — | 37 | 0 |
| A | `testsuite/single/tests/efuns/async_info_scheduler.lpc` | — | 37 | 0 |
| A | `testsuite/single/tests/efuns/async_inherit.lpc` | — | 48 | 0 |
| A | `testsuite/single/tests/efuns/async_inherit_park.lpc` | — | 34 | 0 |
| A | `testsuite/single/tests/efuns/async_nested_callbacks.lpc` | — | 121 | 0 |
| A | `testsuite/single/tests/efuns/async_promise.c` | — | 51 | 0 |
| A | `testsuite/single/tests/efuns/async_yield.lpc` | — | 37 | 0 |
| A | `testsuite/single/tests/efuns/async_yield_shutdown.lpc` | — | 19 | 0 |
| A | `testsuite/single/tests/efuns/atan.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/b3_bp.c` | — | 4 | 0 |
| M | `testsuite/single/tests/efuns/badshad.c` | — | 5 | 6 |
| M | `testsuite/single/tests/efuns/badshad2.c` | — | 5 | 6 |
| A | `testsuite/single/tests/efuns/base_name.lpc` | — | 5 | 0 |
| M | `testsuite/single/tests/efuns/bind.c` | — | 7 | 7 |
| A | `testsuite/single/tests/efuns/bind_destruct_owner.c` | — | 31 | 0 |
| A | `testsuite/single/tests/efuns/break_cycles.c` | — | 144 | 0 |
| A | `testsuite/single/tests/efuns/buffer_transcode.lpc` | — | 7 | 0 |
| A | `testsuite/single/tests/efuns/bufferp.lpc` | — | 5 | 0 |
| M | `testsuite/single/tests/efuns/cache_stats.c` | — | 1 | 1 |
| M | `testsuite/single/tests/efuns/call_other.c` | — | 15 | 15 |
| M | `testsuite/single/tests/efuns/call_out.c` | — | 26 | 11 |
| A | `testsuite/single/tests/efuns/call_out_walltime.lpc` | — | 11 | 0 |
| M | `testsuite/single/tests/efuns/call_stack.c` | — | 20 | 22 |
| A | `testsuite/single/tests/efuns/call_stack.lpc` | — | 37 | 0 |
| A | `testsuite/single/tests/efuns/call_stack_driver_frame.lpc` | — | 35 | 0 |
| A | `testsuite/single/tests/efuns/callout_memory_refs.lpc` | — | 40 | 0 |
| M | `testsuite/single/tests/efuns/capitalize.c` | — | 22 | 23 |
| A | `testsuite/single/tests/efuns/ceil.lpc` | — | 5 | 0 |
| M | `testsuite/single/tests/efuns/children.c` | — | 9 | 11 |
| M | `testsuite/single/tests/efuns/classes.c` | — | 6 | 3 |
| A | `testsuite/single/tests/efuns/classp.lpc` | — | 9 | 0 |
| M | `testsuite/single/tests/efuns/clear_bit.c` | — | 12 | 13 |
| A | `testsuite/single/tests/efuns/clear_debug_level.lpc` | — | 9 | 0 |
| M | `testsuite/single/tests/efuns/clone_object.c` | — | 8 | 9 |
| M | `testsuite/single/tests/efuns/clonep.c` | — | 5 | 5 |
| M | `testsuite/single/tests/efuns/command.c` | — | 22 | 22 |
| A | `testsuite/single/tests/efuns/compress.lpc` | — | 10 | 0 |
| A | `testsuite/single/tests/efuns/compress_file.c` | — | 41 | 0 |
| A | `testsuite/single/tests/efuns/compress_file_integrity.lpc` | — | 66 | 0 |
| A | `testsuite/single/tests/efuns/compress_file_long_path.lpc` | — | 14 | 0 |
| A | `testsuite/single/tests/efuns/compressedp.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/copy.lpc` | — | 11 | 0 |
| A | `testsuite/single/tests/efuns/cos.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/cp.c` | — | 5 | 6 |
| M | `testsuite/single/tests/efuns/crc32.c` | — | 2 | 3 |
| M | `testsuite/single/tests/efuns/crypt.c` | — | 21 | 10 |
| M | `testsuite/single/tests/efuns/ctime.c` | — | 6 | 6 |
| M | `testsuite/single/tests/efuns/db.c` | — | 85 | 65 |
| A | `testsuite/single/tests/efuns/debug_info.lpc` | — | 13 | 0 |
| A | `testsuite/single/tests/efuns/debug_levels.lpc` | — | 3 | 0 |
| A | `testsuite/single/tests/efuns/debug_message.lpc` | — | 3 | 0 |
| A | `testsuite/single/tests/efuns/debugmalloc.lpc` | — | 19 | 0 |
| M | `testsuite/single/tests/efuns/deep_inherit_list.c` | — | 6 | 6 |
| M | `testsuite/single/tests/efuns/deep_inventory.c` | — | 13 | 13 |
| M | `testsuite/single/tests/efuns/defer.c` | — | 2 | 2 |
| M | `testsuite/single/tests/efuns/destruct.c` | — | 4 | 4 |
| M | `testsuite/single/tests/efuns/disable_commands.c` | — | 10 | 10 |
| M | `testsuite/single/tests/efuns/disable_commands2.c` | — | 37 | 38 |
| A | `testsuite/single/tests/efuns/distance.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/dotprod.lpc` | — | 21 | 0 |
| A | `testsuite/single/tests/efuns/dual_extension.lpc` | — | 56 | 0 |
| A | `testsuite/single/tests/efuns/dump_file_descriptors.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/dump_jemalloc.lpc` | — | 3 | 0 |
| M | `testsuite/single/tests/efuns/dump_prog.c` | — | 6 | 1 |
| A | `testsuite/single/tests/efuns/dump_stralloc.lpc` | — | 26 | 0 |
| M | `testsuite/single/tests/efuns/dumpallobj.c` | — | 17 | 2 |
| A | `testsuite/single/tests/efuns/dwlib_replace_objects.lpc` | — | 20 | 0 |
| M | `testsuite/single/tests/efuns/ed.c` | — | 9 | 9 |
| A | `testsuite/single/tests/efuns/element_of.lpc` | — | 9 | 0 |
| M | `testsuite/single/tests/efuns/enable_wizard.c` | — | 12 | 12 |
| M | `testsuite/single/tests/efuns/encoding.c` | — | 2 | 2 |
| M | `testsuite/single/tests/efuns/environment.c` | — | 8 | 10 |
| M | `testsuite/single/tests/efuns/error.c` | — | 2 | 2 |
| A | `testsuite/single/tests/efuns/error_handler_mapping_size.lpc` | — | 9 | 0 |
| A | `testsuite/single/tests/efuns/evaluate.lpc` | — | 8 | 0 |
| A | `testsuite/single/tests/efuns/event.lpc` | — | 15 | 0 |
| M | `testsuite/single/tests/efuns/exec.c` | — | 7 | 8 |
| A | `testsuite/single/tests/efuns/exp.lpc` | — | 5 | 0 |
| M | `testsuite/single/tests/efuns/explode.c` | — | 81 | 7 |
| A | `testsuite/single/tests/efuns/external_promise.c` | — | 48 | 0 |
| A | `testsuite/single/tests/efuns/external_start.lpc` | — | 11 | 0 |
| A | `testsuite/single/tests/efuns/fetch_class_member.lpc` | — | 8 | 0 |
| A | `testsuite/single/tests/efuns/fetch_variable.lpc` | — | 8 | 0 |
| A | `testsuite/single/tests/efuns/file_io_failure.lpc` | — | 21 | 0 |
| A | `testsuite/single/tests/efuns/file_length.lpc` | — | 8 | 0 |
| M | `testsuite/single/tests/efuns/file_name.c` | — | 1 | 2 |
| M | `testsuite/single/tests/efuns/file_size.c` | — | 10 | 10 |
| M | `testsuite/single/tests/efuns/filter.c` | — | 41 | 41 |
| M | `testsuite/single/tests/efuns/find_call_out.c` | — | 5 | 5 |
| A | `testsuite/single/tests/efuns/find_cycles.c` | — | 73 | 0 |
| M | `testsuite/single/tests/efuns/find_living.c` | — | 12 | 12 |
| M | `testsuite/single/tests/efuns/find_object.c` | — | 12 | 12 |
| M | `testsuite/single/tests/efuns/find_player.c` | — | 16 | 16 |
| A | `testsuite/single/tests/efuns/first_inventory.lpc` | — | 17 | 0 |
| A | `testsuite/single/tests/efuns/floatp.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/floor.lpc` | — | 5 | 0 |
| M | `testsuite/single/tests/efuns/function_exists.c` | — | 14 | 11 |
| A | `testsuite/single/tests/efuns/function_owner.lpc` | — | 3 | 0 |
| M | `testsuite/single/tests/efuns/function_profile.c` | — | 6 | 7 |
| M | `testsuite/single/tests/efuns/functionp.c` | — | 3 | 3 |
| M | `testsuite/single/tests/efuns/functions.c` | — | 4 | 5 |
| D | `testsuite/single/tests/efuns/generate_source.c` | — | 0 | 3 |
| D | `testsuite/single/tests/efuns/get_char.c` | — | 0 | 3 |
| M | `testsuite/single/tests/efuns/get_config.c` | — | 8 | 3 |
| A | `testsuite/single/tests/efuns/get_os_env.lpc` | — | 24 | 0 |
| A | `testsuite/single/tests/efuns/geteuid.lpc` | — | 7 | 0 |
| A | `testsuite/single/tests/efuns/getuid.lpc` | — | 7 | 0 |
| M | `testsuite/single/tests/efuns/goodshad.c` | — | 5 | 5 |
| A | `testsuite/single/tests/efuns/has_cycle.c` | — | 105 | 0 |
| M | `testsuite/single/tests/efuns/hash.c` | — | 48 | 33 |
| M | `testsuite/single/tests/efuns/implode.c` | — | 10 | 12 |
| M | `testsuite/single/tests/efuns/in_edit.c` | — | 4 | 4 |
| A | `testsuite/single/tests/efuns/include_list.lpc` | — | 28 | 0 |
| M | `testsuite/single/tests/efuns/inherit_list.c` | — | 5 | 5 |
| M | `testsuite/single/tests/efuns/inherits.c` | — | 10 | 10 |
| M | `testsuite/single/tests/efuns/input_to.c` | — | 4 | 4 |
| M | `testsuite/single/tests/efuns/interactive.c` | — | 4 | 5 |
| M | `testsuite/single/tests/efuns/intp.c` | — | 7 | 7 |
| A | `testsuite/single/tests/efuns/is_daylight_savings_time.lpc` | — | 4 | 0 |
| A | `testsuite/single/tests/efuns/json_encode_frozen.c` | — | 52 | 0 |
| M | `testsuite/single/tests/efuns/keys.c` | — | 7 | 7 |
| M | `testsuite/single/tests/efuns/light.c` | — | 2 | 2 |
| A | `testsuite/single/tests/efuns/link.lpc` | — | 13 | 0 |
| M | `testsuite/single/tests/efuns/living.c` | — | 3 | 3 |
| M | `testsuite/single/tests/efuns/livings.c` | — | 3 | 3 |
| A | `testsuite/single/tests/efuns/load_object_long_path.lpc` | — | 24 | 0 |
| A | `testsuite/single/tests/efuns/log.lpc` | — | 7 | 0 |
| A | `testsuite/single/tests/efuns/log10.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/log2.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/lower_case.c` | — | 10 | 10 |
| A | `testsuite/single/tests/efuns/ltrim.lpc` | — | 10 | 0 |
| M | `testsuite/single/tests/efuns/malloc_status.c` | — | 1 | 1 |
| M | `testsuite/single/tests/efuns/map.c` | — | 36 | 36 |
| M | `testsuite/single/tests/efuns/map_delete.c` | — | 8 | 8 |
| M | `testsuite/single/tests/efuns/mapp.c` | — | 3 | 3 |
| M | `testsuite/single/tests/efuns/master.c` | — | 2 | 2 |
| M | `testsuite/single/tests/efuns/match_path.c` | — | 8 | 8 |
| A | `testsuite/single/tests/efuns/math_vector_results.lpc` | — | 28 | 0 |
| M | `testsuite/single/tests/efuns/matrix.c` | — | 19 | 1 |
| A | `testsuite/single/tests/efuns/matrix_type_check.lpc` | — | 44 | 0 |
| A | `testsuite/single/tests/efuns/max.lpc` | — | 7 | 0 |
| M | `testsuite/single/tests/efuns/member_array.c` | — | 20 | 6 |
| A | `testsuite/single/tests/efuns/memory_info.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/memory_summary.lpc` | — | 5 | 0 |
| D | `testsuite/single/tests/efuns/message.c` | — | 0 | 4 |
| A | `testsuite/single/tests/efuns/min.lpc` | — | 8 | 0 |
| M | `testsuite/single/tests/efuns/mkdir.c` | — | 9 | 1 |
| A | `testsuite/single/tests/efuns/modernization_compat.lpc` | — | 57 | 0 |
| M | `testsuite/single/tests/efuns/move_object.c` | — | 7 | 7 |
| M | `testsuite/single/tests/efuns/mud_status.c` | — | 6 | 6 |
| A | `testsuite/single/tests/efuns/nested_array_free_recursion.lpc` | — | 34 | 0 |
| A | `testsuite/single/tests/efuns/network_stats.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/new.lpc` | — | 10 | 0 |
| A | `testsuite/single/tests/efuns/next_bit.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/next_inventory.lpc` | — | 16 | 0 |
| A | `testsuite/single/tests/efuns/norm.lpc` | — | 5 | 0 |
| D | `testsuite/single/tests/efuns/notify_fail.c` | — | 0 | 6 |
| M | `testsuite/single/tests/efuns/nullp.c` | — | 3 | 3 |
| A | `testsuite/single/tests/efuns/num_classes.lpc` | — | 9 | 0 |
| M | `testsuite/single/tests/efuns/objectp.c` | — | 2 | 3 |
| M | `testsuite/single/tests/efuns/origin.c` | — | 13 | 9 |
| A | `testsuite/single/tests/efuns/os_env.c` | — | 24 | 0 |
| A | `testsuite/single/tests/efuns/owner_executor_contract.c` | — | 1751 | 0 |
| A | `testsuite/single/tests/efuns/owner_payload.c` | — | 218 | 0 |
| A | `testsuite/single/tests/efuns/package_socket_external.lpc` | — | 91 | 0 |
| M | `testsuite/single/tests/efuns/package_trim.c` | — | 15 | 0 |
| A | `testsuite/single/tests/efuns/parse_utf8.lpc` | — | 46 | 0 |
| M | `testsuite/single/tests/efuns/parser.c` | — | 10 | 3 |
| A | `testsuite/single/tests/efuns/parser_debug_output.c` | — | 34 | 0 |
| A | `testsuite/single/tests/efuns/parser_owner_probe.c` | — | 24 | 0 |
| A | `testsuite/single/tests/efuns/parser_reentry.c` | — | 75 | 0 |
| M | `testsuite/single/tests/efuns/pcre.c` | — | 187 | 103 |
| A | `testsuite/single/tests/efuns/pcre_cache.lpc` | — | 7 | 0 |
| A | `testsuite/single/tests/efuns/pcre_error_paths.lpc` | — | 23 | 0 |
| A | `testsuite/single/tests/efuns/pcre_replace.lpc` | — | 24 | 0 |
| A | `testsuite/single/tests/efuns/pcre_replace_callback_zero.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/pcre_replace_type_checks.lpc` | — | 25 | 0 |
| A | `testsuite/single/tests/efuns/pcre_version.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/perf_counter_ns.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/pluralize.c` | — | 24 | 0 |
| M | `testsuite/single/tests/efuns/pointerp.c` | — | 2 | 2 |
| A | `testsuite/single/tests/efuns/pow.lpc` | — | 7 | 0 |
| M | `testsuite/single/tests/efuns/present.c` | — | 2 | 2 |
| A | `testsuite/single/tests/efuns/present_destruct.c` | — | 53 | 0 |
| M | `testsuite/single/tests/efuns/previous_object.c` | — | 5 | 5 |
| D | `testsuite/single/tests/efuns/printf.c` | — | 0 | 4 |
| A | `testsuite/single/tests/efuns/program_info.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/promise_body_owned.lpc` | — | 75 | 0 |
| A | `testsuite/single/tests/efuns/promise_contracts.lpc` | — | 83 | 0 |
| A | `testsuite/single/tests/efuns/promise_memory_refs.lpc` | — | 22 | 0 |
| A | `testsuite/single/tests/efuns/promise_phase1.lpc` | — | 60 | 0 |
| A | `testsuite/single/tests/efuns/promise_reject_default.lpc` | — | 58 | 0 |
| A | `testsuite/single/tests/efuns/query_charmode.lpc` | — | 3 | 0 |
| M | `testsuite/single/tests/efuns/query_ed_mode.c` | — | 10 | 10 |
| A | `testsuite/single/tests/efuns/query_host_name.lpc` | — | 4 | 0 |
| M | `testsuite/single/tests/efuns/query_idle.c` | — | 2 | 2 |
| M | `testsuite/single/tests/efuns/query_ip_name.c` | — | 3 | 3 |
| M | `testsuite/single/tests/efuns/query_ip_number.c` | — | 3 | 3 |
| A | `testsuite/single/tests/efuns/query_ip_port.lpc` | — | 3 | 0 |
| M | `testsuite/single/tests/efuns/query_load_average.c` | — | 1 | 2 |
| A | `testsuite/single/tests/efuns/query_notify_destruct.lpc` | — | 4 | 0 |
| A | `testsuite/single/tests/efuns/query_notify_fail.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/query_num.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/query_privs.c` | — | 1 | 1 |
| A | `testsuite/single/tests/efuns/query_replaced_program.lpc` | — | 9 | 0 |
| M | `testsuite/single/tests/efuns/query_snoop.c` | — | 1 | 1 |
| M | `testsuite/single/tests/efuns/query_snooping.c` | — | 1 | 1 |
| M | `testsuite/single/tests/efuns/query_verb.c` | — | 7 | 7 |
| M | `testsuite/single/tests/efuns/random.c` | — | 4 | 4 |
| M | `testsuite/single/tests/efuns/read_buffer.c` | — | 25 | 27 |
| M | `testsuite/single/tests/efuns/read_bytes.c` | — | 15 | 15 |
| M | `testsuite/single/tests/efuns/read_file.c` | — | 26 | 26 |
| A | `testsuite/single/tests/efuns/real_time.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/receive.c` | — | 2 | 2 |
| A | `testsuite/single/tests/efuns/reclaim_objects.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/recompile_object.c` | — | 170 | 0 |
| A | `testsuite/single/tests/efuns/recompile_simul_one.c` | — | 16 | 0 |
| A | `testsuite/single/tests/efuns/recompile_special_targets.c` | — | 85 | 0 |
| A | `testsuite/single/tests/efuns/recompile_stress.c` | — | 107 | 0 |
| A | `testsuite/single/tests/efuns/refs.lpc` | — | 5 | 0 |
| M | `testsuite/single/tests/efuns/reg_assoc.c` | — | 17 | 17 |
| M | `testsuite/single/tests/efuns/regexp.c` | — | 27 | 27 |
| A | `testsuite/single/tests/efuns/reload_object_self_destruct.lpc` | — | 22 | 0 |
| M | `testsuite/single/tests/efuns/remove_action.c` | — | 23 | 8 |
| M | `testsuite/single/tests/efuns/remove_call_out.c` | — | 18 | 15 |
| M | `testsuite/single/tests/efuns/rename.c` | — | 16 | 16 |
| A | `testsuite/single/tests/efuns/repeat_string.lpc` | — | 12 | 0 |
| A | `testsuite/single/tests/efuns/replace_program.lpc` | — | 19 | 0 |
| M | `testsuite/single/tests/efuns/replace_string.c` | — | 79 | 69 |
| M | `testsuite/single/tests/efuns/replaceable.c` | — | 10 | 11 |
| A | `testsuite/single/tests/efuns/request_clean_up.lpc` | — | 10 | 0 |
| A | `testsuite/single/tests/efuns/resolve.lpc` | — | 31 | 0 |
| A | `testsuite/single/tests/efuns/restore_from_string.lpc` | — | 20 | 0 |
| M | `testsuite/single/tests/efuns/restore_object.c` | — | 38 | 32 |
| M | `testsuite/single/tests/efuns/restore_variable.c` | — | 31 | 31 |
| A | `testsuite/single/tests/efuns/restore_variable.lpc` | — | 84 | 0 |
| A | `testsuite/single/tests/efuns/restore_variable_class.lpc` | — | 22 | 0 |
| A | `testsuite/single/tests/efuns/restore_variable_parser_desync.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/restore_variable_unterminated_escape.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/rm.c` | — | 4 | 4 |
| M | `testsuite/single/tests/efuns/rmdir.c` | — | 4 | 4 |
| A | `testsuite/single/tests/efuns/round.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/rp_a.c` | — | 4 | 0 |
| A | `testsuite/single/tests/efuns/rp_leaf.c` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/rp_mid.c` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/rtrim.lpc` | — | 10 | 0 |
| A | `testsuite/single/tests/efuns/save_file_integrity.lpc` | — | 16 | 0 |
| M | `testsuite/single/tests/efuns/save_object.c` | — | 35 | 7 |
| A | `testsuite/single/tests/efuns/save_object_source.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/save_variable.c` | — | 30 | 31 |
| M | `testsuite/single/tests/efuns/say.c` | — | 17 | 17 |
| M | `testsuite/single/tests/efuns/secure_random.c` | — | 4 | 4 |
| M | `testsuite/single/tests/efuns/send_nullbyte.c` | — | 5 | 5 |
| M | `testsuite/single/tests/efuns/set_bit.c` | — | 10 | 15 |
| A | `testsuite/single/tests/efuns/set_bit_negative.lpc` | — | 24 | 0 |
| A | `testsuite/single/tests/efuns/set_clean_up.c` | — | 33 | 0 |
| A | `testsuite/single/tests/efuns/set_config.lpc` | — | 4 | 0 |
| M | `testsuite/single/tests/efuns/set_debug_level.c` | — | 11 | 2 |
| A | `testsuite/single/tests/efuns/set_eval_limit.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/set_heart_beat.c` | — | 64 | 36 |
| M | `testsuite/single/tests/efuns/set_hide.c` | — | 14 | 2 |
| M | `testsuite/single/tests/efuns/set_light.c` | — | 25 | 25 |
| M | `testsuite/single/tests/efuns/set_living_name.c` | — | 15 | 15 |
| A | `testsuite/single/tests/efuns/set_notify_destruct.lpc` | — | 11 | 0 |
| M | `testsuite/single/tests/efuns/set_privs.c` | — | 4 | 4 |
| M | `testsuite/single/tests/efuns/sha1.c` | — | 69 | 72 |
| M | `testsuite/single/tests/efuns/shadow.c` | — | 25 | 23 |
| A | `testsuite/single/tests/efuns/shallow_inherit_list.lpc` | — | 5 | 0 |
| M | `testsuite/single/tests/efuns/shutdown.c` | — | 5 | 6 |
| A | `testsuite/single/tests/efuns/sin.lpc` | — | 7 | 0 |
| M | `testsuite/single/tests/efuns/sizeof.c` | — | 7 | 7 |
| M | `testsuite/single/tests/efuns/snoop.c` | — | 2 | 1 |
| A | `testsuite/single/tests/efuns/socket_accept.lpc` | — | 13 | 0 |
| A | `testsuite/single/tests/efuns/socket_acquire.lpc` | — | 8 | 0 |
| A | `testsuite/single/tests/efuns/socket_address.lpc` | — | 14 | 0 |
| A | `testsuite/single/tests/efuns/socket_binary_fragments.c` | — | 104 | 0 |
| A | `testsuite/single/tests/efuns/socket_bind.lpc` | — | 14 | 0 |
| A | `testsuite/single/tests/efuns/socket_close.lpc` | — | 13 | 0 |
| A | `testsuite/single/tests/efuns/socket_connect.lpc` | — | 9 | 0 |
| A | `testsuite/single/tests/efuns/socket_create.lpc` | — | 14 | 0 |
| A | `testsuite/single/tests/efuns/socket_error.lpc` | — | 8 | 0 |
| A | `testsuite/single/tests/efuns/socket_get_option.lpc` | — | 102 | 0 |
| A | `testsuite/single/tests/efuns/socket_listen.lpc` | — | 18 | 0 |
| A | `testsuite/single/tests/efuns/socket_release.lpc` | — | 21 | 0 |
| A | `testsuite/single/tests/efuns/socket_status.lpc` | — | 13 | 0 |
| A | `testsuite/single/tests/efuns/socket_text_close_reuse.c` | — | 132 | 0 |
| A | `testsuite/single/tests/efuns/socket_text_eof.c` | — | 98 | 0 |
| A | `testsuite/single/tests/efuns/socket_text_fragments.c` | — | 169 | 0 |
| A | `testsuite/single/tests/efuns/socket_text_invalid.c` | — | 98 | 0 |
| M | `testsuite/single/tests/efuns/socket_tls_server.c` | — | 8 | 2 |
| A | `testsuite/single/tests/efuns/socket_tls_text_fragments.c` | — | 181 | 0 |
| A | `testsuite/single/tests/efuns/socket_tls_verify_requires_nonempty_sni.lpc` | — | 42 | 0 |
| A | `testsuite/single/tests/efuns/socket_write.lpc` | — | 12 | 0 |
| M | `testsuite/single/tests/efuns/sockets.c` | — | 21 | 22 |
| M | `testsuite/single/tests/efuns/sort_array.c` | — | 1 | 1 |
| M | `testsuite/single/tests/efuns/sprintf.c` | — | 226 | 227 |
| A | `testsuite/single/tests/efuns/sprintf.lpc` | — | 348 | 0 |
| A | `testsuite/single/tests/efuns/sprintf_column_object.lpc` | — | 26 | 0 |
| A | `testsuite/single/tests/efuns/sqrt.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/sscanf.c` | — | 21 | 21 |
| A | `testsuite/single/tests/efuns/stat.lpc` | — | 48 | 0 |
| A | `testsuite/single/tests/efuns/store_class_member.lpc` | — | 10 | 0 |
| M | `testsuite/single/tests/efuns/store_variable.c` | — | 3 | 3 |
| M | `testsuite/single/tests/efuns/strftime.c` | — | 1 | 1 |
| A | `testsuite/single/tests/efuns/string_add_object.c` | — | 60 | 0 |
| A | `testsuite/single/tests/efuns/string_ascii_cache.lpc` | — | 191 | 0 |
| A | `testsuite/single/tests/efuns/string_decode.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/string_difference.lpc` | — | 7 | 0 |
| A | `testsuite/single/tests/efuns/string_encode.lpc` | — | 9 | 0 |
| A | `testsuite/single/tests/efuns/stringp.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/strsrch.c` | — | 11 | 0 |
| A | `testsuite/single/tests/efuns/strsrch_reverse_egc.lpc` | — | 23 | 0 |
| M | `testsuite/single/tests/efuns/strwidth.c` | — | 4 | 1 |
| M | `testsuite/single/tests/efuns/sys_network_ports.c` | — | 20 | 25 |
| M | `testsuite/single/tests/efuns/sys_reload_tls.c` | — | 13 | 2 |
| A | `testsuite/single/tests/efuns/sys_reload_tls.lpc` | — | 47 | 0 |
| M | `testsuite/single/tests/efuns/talker.c` | — | 8 | 10 |
| A | `testsuite/single/tests/efuns/tan.lpc` | — | 5 | 0 |
| D | `testsuite/single/tests/efuns/telnet_ga.c` | — | 0 | 3 |
| D | `testsuite/single/tests/efuns/telnet_msp_oob.c` | — | 0 | 7 |
| D | `testsuite/single/tests/efuns/telnet_nop.c` | — | 0 | 3 |
| A | `testsuite/single/tests/efuns/terminal_colour.lpc` | — | 34 | 0 |
| A | `testsuite/single/tests/efuns/test_bit.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/efuns/this_object.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/this_player.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/throw.lpc` | — | 6 | 0 |
| M | `testsuite/single/tests/efuns/time.c` | — | 1 | 1 |
| M | `testsuite/single/tests/efuns/to_float.c` | — | 11 | 11 |
| M | `testsuite/single/tests/efuns/to_int.c` | — | 11 | 11 |
| M | `testsuite/single/tests/efuns/trace.c` | — | 1 | 1 |
| A | `testsuite/single/tests/efuns/trace_io.lpc` | — | 13 | 0 |
| A | `testsuite/single/tests/efuns/trace_overflow_recovery.lpc` | — | 57 | 0 |
| M | `testsuite/single/tests/efuns/transcode.c` | — | 8 | 2 |
| A | `testsuite/single/tests/efuns/trim.lpc` | — | 15 | 0 |
| A | `testsuite/single/tests/efuns/typeof.lpc` | — | 12 | 0 |
| A | `testsuite/single/tests/efuns/uncompress.lpc` | — | 7 | 0 |
| A | `testsuite/single/tests/efuns/uncompress_file.lpc` | — | 12 | 0 |
| A | `testsuite/single/tests/efuns/uncompress_file_corrupt.lpc` | — | 28 | 0 |
| A | `testsuite/single/tests/efuns/uncompress_zipbomb.lpc` | — | 36 | 0 |
| A | `testsuite/single/tests/efuns/undefinedp.lpc` | — | 9 | 0 |
| A | `testsuite/single/tests/efuns/unhandled_rejection_locus.lpc` | — | 104 | 0 |
| M | `testsuite/single/tests/efuns/unique_mapping.c` | — | 28 | 28 |
| M | `testsuite/single/tests/efuns/unloaded.c` | — | 1 | 2 |
| A | `testsuite/single/tests/efuns/upper_case.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/uptime.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/userp.lpc` | — | 4 | 0 |
| M | `testsuite/single/tests/efuns/users.c` | — | 3 | 3 |
| M | `testsuite/single/tests/efuns/values.c` | — | 7 | 7 |
| M | `testsuite/single/tests/efuns/virtual.c` | — | 14 | 16 |
| A | `testsuite/single/tests/efuns/virtualp.lpc` | — | 5 | 0 |
| A | `testsuite/single/tests/efuns/vowel.lpc` | — | 8 | 0 |
| A | `testsuite/single/tests/efuns/wizardp.lpc` | — | 3 | 0 |
| A | `testsuite/single/tests/efuns/worker_payload.c` | — | 54 | 0 |
| A | `testsuite/single/tests/efuns/write_buffer.lpc` | — | 9 | 0 |
| A | `testsuite/single/tests/efuns/write_buffer_bounds.lpc` | — | 22 | 0 |
| A | `testsuite/single/tests/efuns/write_bytes.lpc` | — | 7 | 0 |
| A | `testsuite/single/tests/efuns/write_file.lpc` | — | 9 | 0 |
| A | `testsuite/single/tests/efuns/zonetime.lpc` | — | 6 | 0 |
| A | `testsuite/single/tests/operators/assign_specialized.lpc` | — | 97 | 0 |
| A | `testsuite/single/tests/operators/buffer_range_assign.lpc` | — | 45 | 0 |
| M | `testsuite/single/tests/operators/class.c` | — | 29 | 30 |
| A | `testsuite/single/tests/operators/compound_assign_float.lpc` | — | 115 | 0 |
| A | `testsuite/single/tests/operators/compound_assign_index_rhs.lpc` | — | 54 | 0 |
| A | `testsuite/single/tests/operators/compound_assign_undefined.lpc` | — | 82 | 0 |
| M | `testsuite/single/tests/operators/foreach.c` | — | 11 | 11 |
| A | `testsuite/single/tests/operators/foreach_large.lpc` | — | 55 | 0 |
| A | `testsuite/single/tests/operators/foreach_ref_mapping.c` | — | 36 | 0 |
| A | `testsuite/single/tests/operators/index_lvalue_alias.lpc` | — | 173 | 0 |
| M | `testsuite/single/tests/operators/mapping.c` | — | 3 | 3 |
| M | `testsuite/single/tests/operators/range.c` | — | 11 | 11 |
| A | `testsuite/single/tests/operators/shift_mod64.lpc` | — | 45 | 0 |
| M | `testsuite/single/tests/operators/string_index.c` | — | 33 | 33 |
| M | `testsuite/single/tests/operators/switch.c` | — | 14 | 14 |
| M | `testsuite/single/tests/std/base64.c` | — | 11 | 9 |
| M | `testsuite/single/tests/std/bitmap_font.c` | — | 1 | 1 |
| M | `testsuite/single/tests/std/break_string.c` | — | 15 | 15 |
| M | `testsuite/single/tests/std/database.c` | — | 43 | 37 |
| M | `testsuite/single/tests/std/element_of_weighted.c` | — | 14 | 15 |
| M | `testsuite/single/tests/std/highest.c` | — | 5 | 6 |
| M | `testsuite/single/tests/std/json.c` | — | 3 | 3 |
| M | `testsuite/single/tests/std/lowest.c` | — | 5 | 6 |
| M | `testsuite/single/tests/std/number_string.c` | — | 19 | 20 |
| M | `testsuite/single/tests/std/percent.c` | — | 5 | 6 |
| M | `testsuite/single/tests/std/present_clone.c` | — | 31 | 32 |
| M | `testsuite/single/tests/std/range.c` | — | 7 | 8 |
| M | `testsuite/single/tests/std/reduce.c` | — | 47 | 41 |
| M | `testsuite/single/tests/std/sum.c` | — | 7 | 8 |
| M | `testsuite/single/void.c` | — | 295 | 2 |
| M | `testsuite/std/all_environment.c` | — | 7 | 8 |
| M | `testsuite/std/base64.c` | — | 92 | 98 |
| M | `testsuite/std/bitmap_font.c` | — | 65 | 67 |
| M | `testsuite/std/break_string.c` | — | 65 | 73 |
| M | `testsuite/std/database.c` | — | 405 | 510 |
| M | `testsuite/std/element_of_weighted.c` | — | 23 | 26 |
| M | `testsuite/std/highest.c` | — | 13 | 16 |
| M | `testsuite/std/http.c` | — | 9 | 7 |
| M | `testsuite/std/json.c` | — | 537 | 526 |
| M | `testsuite/std/lowest.c` | — | 14 | 17 |
| M | `testsuite/std/number_string.c` | — | 26 | 35 |
| M | `testsuite/std/percent.c` | — | 4 | 4 |
| M | `testsuite/std/present_clone.c` | — | 39 | 47 |
| M | `testsuite/std/range.c` | — | 22 | 29 |
| M | `testsuite/std/reduce.c` | — | 17 | 25 |
| M | `testsuite/std/sum.c` | — | 18 | 22 |
| M | `testsuite/std/telnet.c` | — | 113 | 132 |
| A | `third_party/cyclonedx.schema.json` | — | 1697 | 0 |
| A | `third_party/manifest.yaml` | — | 188 | 0 |
| A | `third_party/sbom.json` | — | 555 | 0 |
| A | `tools/build/regenerate_grammar.py` | — | 79 | 0 |
| A | `tools/build/test_generated_dependencies.py` | — | 124 | 0 |
| A | `tools/build/test_regenerate_grammar.py` | — | 68 | 0 |
| A | `tools/docs/check-actions-pins.py` | — | 242 | 0 |
| A | `tools/docs/check-docs.py` | — | 233 | 0 |
| A | `tools/docs/check-evidence.py` | — | 350 | 0 |
| A | `tools/docs/check-workflows.py` | — | 510 | 0 |
| A | `tools/docs/derive-cleanup.py` | — | 78 | 0 |
| A | `tools/docs/test-evidence-gate.py` | — | 236 | 0 |
| A | `tools/loadtest/xkx_gateway_loadtest.py` | — | 813 | 0 |
| A | `tools/lpc-modern-runtime-stress.sh` | — | 97 | 0 |
| A | `tools/lpc-syntax/.gitignore` | — | 1 | 0 |
| A | `tools/lpc-syntax/README.md` | — | 165 | 0 |
| A | `tools/lpc-syntax/bin/format-corpus.mjs` | — | 231 | 0 |
| A | `tools/lpc-syntax/docusaurus-plugin.cjs` | — | 41 | 0 |
| A | `tools/lpc-syntax/format.mjs` | — | 1800 | 0 |
| A | `tools/lpc-syntax/generate_ebnf.py` | — | 533 | 0 |
| A | `tools/lpc-syntax/grammar.ebnf` | — | 424 | 0 |
| A | `tools/lpc-syntax/grammar_lexical.ebnf.in` | — | 101 | 0 |
| A | `tools/lpc-syntax/highlight.mjs` | — | 60 | 0 |
| A | `tools/lpc-syntax/lint.mjs` | — | 125 | 0 |
| A | `tools/lpc-syntax/lpc-grammar.json` | — | 2351 | 0 |
| A | `tools/lpc-syntax/lpc.tmLanguage.json` | — | 185 | 0 |
| A | `tools/lpc-syntax/package.json` | — | 21 | 0 |
| A | `tools/lpc-syntax/prism-lpc.cjs` | — | 124 | 0 |
| A | `tools/lpc-syntax/test-format-corpus.mjs` | — | 82 | 0 |
| A | `tools/lpc-syntax/test.mjs` | — | 1679 | 0 |
| A | `tools/lpc-syntax/tokenizer.mjs` | — | 401 | 0 |
| A | `tools/owner-runtime-v4-stress.sh` | — | 73 | 0 |
| A | `tools/owner-scheduler-capacity.sh` | — | 143 | 0 |
| A | `tools/perf/README.md` | — | 187 | 0 |
| A | `tools/perf/compile_report.py` | — | 115 | 0 |
| A | `tools/perf/corpus/s_arrays_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_arrays_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_arrays_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_arrays_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_arrays_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_arrays_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_arrays_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_arrays_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_arrays_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_arrays_09.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_classes_09.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_functions_09.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_loops_09.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_macros_09.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mappings_09.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_mixed_09.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_objects_09.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_strings_09.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_00.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_01.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_02.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_03.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_04.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_05.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_06.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_07.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_08.c` | — | 100 | 0 |
| A | `tools/perf/corpus/s_switch_09.c` | — | 100 | 0 |
| A | `tools/perf/gen_compile_corpus.py` | — | 249 | 0 |
| A | `tools/perf/measure_explode.py` | — | 73 | 0 |
| A | `tools/perf/measure_object_store.py` | — | 152 | 0 |
| A | `tools/perf/measurement.py` | — | 59 | 0 |
| A | `tools/perf/official_compare/analyze_official_vs_xk.py` | — | 103 | 0 |
| A | `tools/perf/official_compare/portable_fluffos_loadtest.py` | — | 290 | 0 |
| A | `tools/perf/official_compare/portable_microbench.py` | — | 355 | 0 |
| A | `tools/perf/official_compare/portable_mudlib/clone/login.c` | — | 17 | 0 |
| A | `tools/perf/official_compare/portable_mudlib/clone/user.c` | — | 44 | 0 |
| A | `tools/perf/official_compare/portable_mudlib/etc/config.template` | — | 39 | 0 |
| A | `tools/perf/official_compare/portable_mudlib/etc/motd` | — | 1 | 0 |
| A | `tools/perf/official_compare/portable_mudlib/single/master.c` | — | 87 | 0 |
| A | `tools/perf/official_compare/portable_mudlib/single/simul_efun.c` | — | 1 | 0 |
| A | `tools/perf/official_compare/portable_mudlib/std/bench_clone.c` | — | 11 | 0 |
| A | `tools/perf/official_compare/portable_mudlib/std/bench_target.c` | — | 115 | 0 |
| A | `tools/perf/official_compare/run_official_vs_xk.py` | — | 554 | 0 |
| A | `tools/perf/official_compare/run_perf_truth_gate.py` | — | 487 | 0 |
| A | `tools/perf/results/after/compile2-1.json` | — | 22 | 0 |
| A | `tools/perf/results/after/compile2-2.json` | — | 22 | 0 |
| A | `tools/perf/results/after/compile2-3.json` | — | 22 | 0 |
| A | `tools/perf/results/after/compile2-4.json` | — | 22 | 0 |
| A | `tools/perf/results/after/compile2-5.json` | — | 22 | 0 |
| A | `tools/perf/results/before/compile2-1.json` | — | 12 | 0 |
| A | `tools/perf/results/before/compile2-2.json` | — | 12 | 0 |
| A | `tools/perf/results/before/compile2-3.json` | — | 12 | 0 |
| A | `tools/perf/results/before/compile2-4.json` | — | 12 | 0 |
| A | `tools/perf/results/before/compile2-5.json` | — | 12 | 0 |
| A | `tools/perf/test_compile_report.py` | — | 261 | 0 |
| A | `tools/perf/test_object_store_bench.py` | — | 92 | 0 |
| A | `tools/perf/tests/test_official_compare_contract.py` | — | 369 | 0 |
| A | `tools/sbom-generate.py` | — | 220 | 0 |
| A | `tools/style/check-format.py` | — | 639 | 0 |
| A | `tools/style/test-check-format.py` | — | 374 | 0 |
| A | `tools/testsuite/README.md` | — | 183 | 0 |
| A | `tools/testsuite/portable-timeout.py` | — | 97 | 0 |
| A | `tools/testsuite/run-isolated.sh` | — | 363 | 0 |
| A | `tools/testsuite/run-targeted.py` | — | 1040 | 0 |
| A | `tools/testsuite/test-compound-free.py` | — | 83 | 0 |
| A | `tools/testsuite/test-global-include.py` | — | 130 | 0 |
| A | `tools/testsuite/test-lpcc-io.py` | — | 135 | 0 |
| A | `tools/testsuite/test-port-isolation.sh` | — | 118 | 0 |
| A | `tools/testsuite/test-run-isolated-portability.py` | — | 153 | 0 |
| A | `tools/testsuite/test-run-targeted.py` | — | 585 | 0 |
| A | `tools/testsuite/test-trace-io.py` | — | 133 | 0 |
| A | `tools/testsuite/test-websocket.py` | — | 638 | 0 |
| A | `tools/wsl-cmake-build.sh` | — | 16 | 0 |

本表文本增删合计：+583586 / -187308；文件状态：A=869，M=955，D=879，R=7。本表不把机械格式化、生成物与真实行为改动等量解释。

## 附录 C. Fork 独有提交全集

`277d0b1cbc4350844d9aff07a604dfa519650d9e..a4475b1afe998bb03a37bdb0596b64ab01042ed3`，共 696 个提交，按 Git topo-order 列出。标题只是索引，不作为功能/安全已验证的证据；标题中的 URL/邮箱若出现则脱敏，提交身份不改。

| Commit | 提交日期 | Git 标题 |
| --- | --- | --- |
| `a4475b1afe998bb03a37bdb0596b64ab01042ed3` | 2026-10-12 | `Finalize local modernization audit` |
| `5f5b5ad86e7bb3730cb5d57f62a66bf162ae4615` | 2026-10-11 | `refactor compiler grammar rule implementations` |
| `69eb481402b59e27fac7e71c794734e5c718da49` | 2026-10-11 | `fix compiler benchmark relative output paths` |
| `bf9a0236a3519c3d47b1fe1f02e4d00f88ce5414` | 2026-10-11 | `maint: refresh Alpine build and vendor records` |
| `31141ff7eefc0cce51097515cab7977564668800` | 2026-10-11 | `style: complete repository format gates` |
| `b52a461141616f1c4d9c90c081f091e401301cce` | 2026-10-11 | `fix: make libevent SSL fd callbacks type-safe` |
| `8960c6acc4856022d7b8f3e8b0b0ab8c740221b7` | 2026-10-11 | `fix: limit UBSan null suppression to GCC` |
| `1e7988d7e9df757c4624c8fd5846ec1f7d779835` | 2026-10-11 | `Fix UTF-8 socket text fragmentation` |
| `8be287ecae10007a2a5a77f9c5ab2c5cdb9af704` | 2026-10-11 | `refactor owner task identity construction` |
| `956b04abafa47e5dc37d6be7f8dabf15542f0000` | 2026-10-11 | `refactor owner status serialization` |
| `20acfcc6402a65aa4c91b5f72f02d1c1eef5d371` | 2026-10-11 | `optimize object store path index lifecycle` |
| `9ac588ca4c9149c542596aa80f8936bbb735a8d4` | 2026-10-11 | `fix gateway session creation cleanup` |
| `4610b70f2d306142bc8aa1bb1768acf658c7a526` | 2026-10-11 | `document remaining U11 environment boundaries` |
| `b93cea1ac67caa27096a72f8ce391495a7a735c8` | 2026-10-11 | `fix debugmalloc chain traversal` |
| `d70e18bdc42a460a21f9b9db630d28e28d2a806a` | 2026-10-11 | `fix: resolve qualified lpc inherited calls` |
| `da691cf3d5c240b14c643b85f97031ffc065ee53` | 2026-10-11 | `test: cover inherited async resume` |
| `9d3fa50895286a1a888fa82fdd00fca4b25250de` | 2026-10-11 | `test: cover async await contracts` |
| `52316a07b60f60602aac0a41ff816969fcc08778` | 2026-10-11 | `test: verify inherited async info names` |
| `ee1bb080bb8ab0802f92dd7d53f09a53962c94bb` | 2026-10-11 | `test: cover async yield shutdown cleanup` |
| `64cb52f6c44d5ff39d78df21a297a99073be6083` | 2026-10-11 | `test: cover async yield turn boundary` |
| `1a725824c391c926a8b1fa3c319fe20416f8d934` | 2026-10-11 | `test: cover dump jemalloc diagnostics` |
| `14b2937819d52bee79da36ffa576b41505e55ce7` | 2026-10-11 | `fix: guard trace rendering for driver frames` |
| `7bac87e390505c129d0dde8c46d76e7b6105a9e7` | 2026-10-11 | `docs: classify covered async efuns` |
| `25fa335932cdfe6f409de2b7e820f674b72a5714` | 2026-10-11 | `test: cover resolve callback lifecycle` |
| `c86caca3d322891e50a63656c429f12784618305` | 2026-10-11 | `test: cover dump_stralloc output` |
| `eff1e8335a3b24a7b89db0ea6d89dd643cea4c8f` | 2026-10-11 | `test: cover debug_info contract` |
| `03f056f0f2b29837494a3aeaeb67779cb43365c5` | 2026-10-11 | `test: lock down deprecated descriptor dump contract` |
| `41600636151c8cf3467d0e317106a89ea3904f09` | 2026-10-11 | `docs: record disabled DWLIB coverage boundary` |
| `07bb5bd06b27585e1a6dace3282867d797f0f4bb` | 2026-10-10 | `test: classify upstream efun gaps and cover buffer transcoding` |
| `31e3e9788f2c7dcd0cc0e76dcea824774f41309b` | 2026-10-10 | `test: cover PCRE and socket failure contracts` |
| `ebd4b7bc86ace1e6e87553f9788a87708ac1cfe8` | 2026-10-10 | `test: cover U11 promise and call stack contracts` |
| `324e963be2d5a0a31fc109ddc72f2975926ec97d` | 2026-10-10 | `test: cover runtime notification boundaries` |
| `0e1850f670c22bb8e8340815c21363b496ed8da0` | 2026-10-10 | `test: cover socket efun boundaries` |
| `3cf9455a794e9d5ee252f4b569013beb2aa87603` | 2026-10-10 | `test: cover restore parser failures` |
| `38e7a1df96dc19f719ea4532a6f4d5b182fe0262` | 2026-10-10 | `test: cover runtime and notification efuns` |
| `5e44059a7b40fc3c9a46f354763512886edd8aec` | 2026-10-10 | `test: cover identity and inheritance efuns` |
| `c508792dbe108855ce51bf348823f48ab6e90035` | 2026-10-10 | `test: cover utility and inventory efuns` |
| `fe2b9fdc73f6df2e403f0644352dd564e9e4ca17` | 2026-10-10 | `fix: preserve class allocation accounting` |
| `e63afc26acdda3bf56f8c812d4c695f8c39c9059` | 2026-10-10 | `test: cover basic efun boundaries` |
| `2f9980ebfab288ffffd6086a243c88ac2246044f` | 2026-10-10 | `test: cover compression package boundaries` |
| `1d1eaf53b74b92db62859f117ed2248873928972` | 2026-10-10 | `test: add upstream vector and set coverage` |
| `416ffb507bf9e56bc9ec631b68bfe4fc3306b732` | 2026-10-10 | `fix: release link authorization path arguments` |
| `48a973a156fd6fa7b190453fb9c80963c8d9c2a5` | 2026-10-10 | `test: cover upstream string efuns` |
| `0f14dac0d4ee64675ddab36adb5d35e47ffebc15` | 2026-10-10 | `test: add upstream type and string efun coverage` |
| `1ea036e7731e6a401350867c9c381f20b2b20dc3` | 2026-10-10 | `test: adapt upstream math efun coverage` |
| `7c7faf48dfbc8baf9e82d940c611a346c7760cd3` | 2026-10-10 | `test: close U11 coverage gaps` |
| `e38d98688d70196cfa2f1b2c70d8075d79048bd4` | 2026-10-10 | `refactor: complete U10 tooling and LPC test split` |
| `ffded9ff0fd7f2a6b34510714f74d0305047e548` | 2026-10-09 | `Align contributor guidance with project standards` |
| `5d8bd8293017bd547d69526d678ac9d3a57d4897` | 2026-10-09 | `Harden WebSocket lifetimes and deduplicate paths` |
| `0bc3088d6f99ca2b6363fe959c98124eae938ebf` | 2026-10-08 | `fix(net): clean pre-logon users by connection identity` |
| `a7b4d0b698dda12eebd15fd5663aa03b50bfab7f` | 2026-10-08 | `perf(vm): index object-store bridge summaries within the read lock` |
| `2d3f38597c2d769291add2acca281c17b951fb97` | 2026-10-08 | `fix(vm): scope compound free worklists through late destruction` |
| `cab8fc8c7f0e54c6da92bfeace8197b5b64add12` | 2026-10-07 | `perf(strings): reuse Unicode classification for explode subranges` |
| `22f45747fde590bd7aefacf043fd954d4505f962` | 2026-10-07 | `fix(build): track generation inputs and verify grammar fallbacks` |
| `acf0980d80ea7a18f6e0a4f371fab0fe4bac7b2b` | 2026-10-07 | `fix: preserve empty global include configuration` |
| `73b50b412b88e3a2457e32d5140d7464425b094b` | 2026-10-07 | `fix: preserve external package internals and count promise references once` |
| `11e12695068a20697f409db39e8d8f7e3f238676` | 2026-10-07 | `fix: bound include paths and preserve permission spelling` |
| `32083e392926b7fc04ac3f273ad1afd95ccf6bf6` | 2026-10-07 | `fix: account owner and active callout references in memory checks` |
| `700f24e3a4d5702ce945e1bc83231ffd92cdd7a1` | 2026-10-07 | `fix: report trace dump results on the main thread` |
| `12f1ceefa6fac267e09d5c187e688a9c9950ede4` | 2026-10-07 | `fix(sockets): preserve getter arguments and returned values` |
| `50abb999294e5f86e94a1819ec546b93fbba265b` | 2026-10-07 | `fix(io): report tool and peripheral resource failures accurately` |
| `96252a1639db57f3bdbbf80babe4bea50e9be472` | 2026-10-07 | `fix(io): preserve save data and reject read and close failures` |
| `6d791864a53b874b5aad606a7dab6ae4e81a3dde` | 2026-10-07 | `fix(compress): preserve source on failure and reject file aliases` |
| `1e7206a15e25b16e438835c74ae9889f7e0dc7eb` | 2026-10-07 | `fix(core): reject incomplete file and editor writes` |
| `47ec0a9f3b9bf0206c343bc381a9a0ce352dd86e` | 2026-10-07 | `fix(perf): preserve compile samples and reject invalid evidence` |
| `fa5e321f924d94e1a20793d9e8bf31a9974d5dd5` | 2026-10-06 | `Establish trustworthy test completion and modernization baselines` |
| `5c71593af1c45365e39e243136555b5d23dc5480` | 2026-10-06 | `Absorb source spelling and runtime fixes` |
| `f5b536a09c60922e9d29e4b7d4e0d535c26d625e` | 2026-10-05 | `Record successful remote CI matrix` |
| `a50e7d5266dc16e9b17ef15946203925cad8ee83` | 2026-10-04 | `Finalize upstream absorption validation records` |
| `b16a95240d46895bbb2105b9a2c6d05c3447eade` | 2026-10-04 | `Keep Windows owner coordinator alive through teardown` |
| `8f452ec99c2a7ef01e744657dc89c02a4f006ab0` | 2026-10-04 | `Trace Windows static teardown source` |
| `adf41dea3b6a9f290a84c0f0d721d92dfe360849` | 2026-10-04 | `Keep Windows worker runtime alive through process teardown` |
| `e20f7d9f6ae86f368d61c72131e0d77ff8a1a778` | 2026-10-04 | `Trace Windows static thread teardown` |
| `4d93db10ca9367279c291974ee9c30f76b948bee` | 2026-10-04 | `Diagnose Windows owner thread termination` |
| `abad071808b93ff0a0f239839d9dd544cdd44171` | 2026-10-04 | `Fix Windows backward teardown lifetime` |
| `a0d7d1a6ab210b4b2e09255684cc7dc4e7efe2e0` | 2026-10-04 | `Contain owner executor cleanup exceptions` |
| `20fccf11465dfafde1e0f0d4ef4b64a8048cd0b8` | 2026-10-04 | `Stabilize worker shutdown on Windows` |
| `3250b923e452f04a9112c719f18c24354b9804c1` | 2026-10-04 | `Fix Windows TLS headers and external baseline docs` |
| `625dda76b851792bb1312124e3323705e27adeb5` | 2026-10-04 | `Fix PCRE2 portability and cleanup edge cases` |
| `126eca85dff9eb7a8a51fb9aa57a7315a2f3317e` | 2026-10-04 | `Complete upstream absorption and PCRE2 migration` |
| `fe163d67f40cef6d445f076e4a2d565799cd4627` | 2026-09-14 | `Test gateway ingress sequence recovery` |
| `28e5fefcb8464969ee39480d52c55512bf83dc76` | 2026-09-14 | `Fix cross-platform CLI and long-path tests` |
| `6748f47f8d64e2859dda79a015b7b765d22bef2e` | 2026-09-14 | `Complete fuzz gates and CI fixes` |
| `c53236332d919ed7d2e8c385793bda467424dacb` | 2026-09-14 | `fix: treat compiler diagnostics as data` |
| `ad04f6243a7b0d5c61b2d4c74eb6f22b291e94dd` | 2026-09-14 | `docs: close the T3/E4 and batch status records` |
| `a22321e5f262c74e46eaa0153610026c8588c58a` | 2026-09-14 | `P8: external-required readiness record, lpcshell in the install paths` |
| `5308bd5702420c7ab13f6354344936105051d5bc` | 2026-09-13 | `L7 extension: master-target reload rounds` |
| `2be6c29097ebe9b6b528dd5949e55a441ed02a2b` | 2026-09-13 | `T3.4: interactive compile-and-diagnose shell (lpcshell)` |
| `6edf8e92fa72bc5b9aaf5fb2f0630fa6f28dd131` | 2026-09-13 | `T3.3: render structured compile diagnostics (snippet + macro chain)` |
| `ff3b1d0acd1cacbb66da750cb762b86b077b1722` | 2026-09-13 | `T3.2: record structured compile diagnostics next to the rendered text` |
| `9f3cbfc7c38aef275f2a2157483338610179e793` | 2026-09-13 | `T3.1: make the compile arena instanced and add session bindings` |
| `102ab329244c58f8d980c3f4d0765e3b5aa30853` | 2026-09-13 | `P7-3: keep owner-status mappings alive across failing test assertions` |
| `1733a162f089db9be486673a064b1ace36940cb1` | 2026-09-13 | `P7-2: wake the event loop with a self-pipe instead of off-thread libevent` |
| `24bed50693575a5b2c714e4f75d94aab5956d57e` | 2026-09-13 | `P7-1: make varargs consumption explicit and checked` |
| `0a97384c50fc70d3d8111aab81f195a0080f4564` | 2026-09-13 | `P5: document status closure (E3 v2 / L7 / C-S2 cross-refs / lpcshell baseline)` |
| `d7a0caeda559408bf118a94c65976a6f119601e4` | 2026-09-13 | `P4: conditional acceptance closure (S16/E1/T1/T2/T4) with live and C++ evidence` |
| `e90833249793e9713c7a0de371384c52d9533a74` | 2026-09-13 | `P3: parser/db/websocket/contrib/compiler functional fixes and open items` |
| `962e4acf80e293358967ff0fe9eb2f283e962835` | 2026-09-13 | `fix: bound decompression, deep-free recursion, codegen recursion and trace output (P2 batch)` |
| `7448dee535d769ee2884aad9684338cad6817637` | 2026-09-13 | `fix: close UB and truncation paths in bit, shift, vector and foreach ops (P1 batch)` |
| `76ae52641d178bcc65040506c8c8981596062cc2` | 2026-09-13 | `fix: close seven remotely/LPC-reachable memory-corruption paths (P0 batch 1)` |
| `4e3552b6b32f1658f4526c1dd6c9858738dc3683` | 2026-09-13 | `docs: full backlog disposition plan covering every archived item` |
| `1b163459702dbfe49f1a57c94ddf9e4aec6587ca` | 2026-09-13 | `gateway: accept the first ingress frame after a process restart` |
| `0ec3b7bf879149c914d422d3851410139fa89718` | 2026-09-13 | `fix(sockets): zero the skipped slot of a T_ARRAY binary send` |
| `01dab47659bc9ac4ef008857153d22a09a1fc5fb` | 2026-09-13 | `docs: record the four-item backlog disposition` |
| `ba8afa2da7c906252a3b6e828242c1e15f6fbbc9` | 2026-09-13 | `fix: release socket resources at shutdown and clear compound-assign subtypes` |
| `75580b7a57c3e6155234f0cb535b4e9c5be2aa06` | 2026-09-13 | `docs: record the batch and how to verify a build from source` |
| `5869b794740302081f35e1d3e23caaa52b4b836a` | 2026-09-13 | `compiler: store effective var type and make the include teardown unconditional` |
| `09b3f209cc848d8f0712448da907916684a956a4` | 2026-09-13 | `fix(promise): keep adoption pay-outs working under the settle guard` |
| `1d8f3184181acf998fc3fb9611a8745739eb6005` | 2026-09-13 | `vm: specialize plain-slot assigns (#1358)` |
| `b3e93914c86475e991d7840fa77e9962fc837428` | 2026-09-13 | `vm: harden promise settle/park guards and expose backlog counter` |
| `77c14b00064ad2926e1627aa60199712eea5d31b` | 2026-09-13 | `fix(sprintf): render promise values in %O output` |
| `85ad7f48906104e987e8dff8fdf513424fe9388a` | 2026-09-13 | `Refocus project docs for independent self-use fork` |
| `8799e2bc53867f4f7de313af7a5a3cd8331c8466` | 2026-09-13 | `docs: record object-store validation` |
| `012ae728d53e1703e93f4c8201f30ee0cde24e66` | 2026-09-13 | `vm: prefer owner-local live object lookup` |
| `722d88127cf13c216cf9a7650ae7ca528676a72b` | 2026-09-13 | `feat: add owner-bound external process promises` |
| `6f45e2626ffa6506899d6812246439bc211aea62` | 2026-09-13 | `feat: add Promise forms for async I/O` |
| `46568b7cc82ed32c9a92fa5b309db13084047594` | 2026-09-13 | `feat: add async await VM execution` |
| `84c338f9559af9c092b851c45364e8dc993af89e` | 2026-09-13 | `vm: add native Promise substrate` |
| `8fa24188233c93fd9663653f2c1d0be542e81a1a` | 2026-09-13 | `vm: make indexed lvalues stack-owned` |
| `d79684ecdba51c7dc4a0a76445737cad2fe182c9` | 2026-09-13 | `docs: close implementation release plan audit` |
| `f051d230a10be9b5d6e9f0f40cc69b6801f4d4c3` | 2026-09-12 | `test: restore the selected simul efun index` |
| `6abab98f0738c07e7b0a5922e82ef91030537e15` | 2026-09-12 | `docs: bind execution plan to current evidence` |
| `bbee8c84dbff954acca65ec1105517e3eacc5c42` | 2026-09-12 | `test: keep Windows CPU timing probes measurable` |
| `f1020aeddde255110679a0d737e451ca910c0188` | 2026-09-12 | `fix: harden gametick and recompile portability` |
| `5f32df4df3839e22503f633f37b6effd47310f70` | 2026-09-12 | `fix: generate logical operator opcodes` |
| `77fc050c54fa1255d40cddae37f2f425138b74b4` | 2026-09-12 | `fix: keep benchmark helpers portable` |
| `60a7626a71469f85688a562d9e4f8f23b012d1cb` | 2026-09-12 | `fix: keep benchmark helpers portable` |
| `82506217c93011b3f5036a2d881cbaf80713f5b1` | 2026-09-12 | `docs: document TSan generator workaround` |
| `b36bae5cea4f2fa65d19b3dacab425f44baea45b` | 2026-09-12 | `docs: refresh remaining-work execution baseline` |
| `c6aba24514267e3b8f5b4ca01db2ecf4847878e7` | 2026-09-12 | `docs: define remaining work execution plan` |
| `bbfb644571667cd1a396a78159c6a5f31fb01bc0` | 2026-09-12 | `docs: bind local audit evidence to implementation commit` |
| `e7044bf35085a5d182cb4d2b55056f76f01eca81` | 2026-09-12 | `audit: harden VM compiler and release gates` |
| `d07e7641fb8895a29e68de9af1a08677b75dd77b` | 2026-09-04 | `docs(agents): add project architecture and workflow guide` |
| `d6396e7da1431e14dd26cb8be19688f683d94cbf` | 2026-09-04 | `chore: remove unused Comet workflow` |
| `f583e03e6c015ddbbb1f1435a33e0350f3bac6d0` | 2026-08-24 | `docs(agents): de-hardcode env numbers, link CMakePresets, add cold-start architecture section` |
| `c86c1312d6e764c8dc75cee385577f0312460702` | 2026-08-19 | `chore: gitignore tool state (.comet/, .pi/, CLAUDE.md) -- not tracked, not pushed` |
| `da346ff9bf0f8bb88e7f4caee74c924ee5c9809d` | 2026-08-19 | `docs(simul_efun): #1247 fix 4th false sorted-contract comment (activate step 3) + test wording` |
| `c69495010dcd76ca483d560385b4662763bfba8f` | 2026-08-19 | `docs(simul_efun): #1247 correct false sorted-contract comments (pointer order, not lexicographic)` |
| `376e97c69d7a77d2268983a46f5108fb9243435f` | 2026-08-19 | `test(config): #1247 track etc/config.test (test-suite runtime config, was untracked)` |
| `9fdeed151c461253592b19878d47d58df665471b` | 2026-08-19 | `docs(evidence): #1247 TSan 444/444 PASSED (setarch -R ASLR workaround) + plan §5 sanitizer gates unified` |
| `dcfd3ea118fea3c1f11ad2d258f5b34af7df3905` | 2026-08-19 | `docs(evidence): #1247 line-number precision (simul_efun.cc:57/59 pointer compare)` |
| `11076eaf8966311c2bd79be4bca8b88ba6f73b7d` | 2026-08-19 | `docs(evidence): #1247 ASan full lpc_tests 444/444 PASSED (SimulEfunReload pointer-order assert fix) + artifact` |
| `6316f5931705f952151766a586378bd5947fe488` | 2026-08-19 | `fix(test): #1247 SimulEfunReloadAddDropReadd fragile position-vs-index assert` |
| `7bbffc398a011041e69caffff9be4197bf9fd41c` | 2026-08-19 | `docs(evidence): #1247 lpc_tests 444/444 run artifact + plan gate counts unified (424 baseline -&gt; 444 current)` |
| `5ac149ba31802d85995247a228cb9cbdb4a5fcba` | 2026-08-19 | `docs(evidence): #1247 wording precision (defprog-&gt;filename first parent, CONTRIB-3/ADDACTION-1 location lines)` |
| `9713de7cedb87d35885449c1bb4a766b3b483ed9` | 2026-08-19 | `docs: #1247 regression-test correction narrative + TSan deferred notes in plan gates and §5` |
| `ca7d04ba1ccdfdde2104c125ba58ba4e9a833892` | 2026-08-19 | `docs(evidence): #1247 full -ftest final run artifact (0 Check Failed, Checks succeeded, 52860 lines)` |
| `64f1f396381123d38ff0a5b63af4539ca1df7a76` | 2026-08-19 | `test(regression): #1247 remove superseded warn_%.lpc (replaced by warn_%s.lpc + warn_%s_a.lpc fixture)` |
| `551a9518f26b6787c270c4246fb96cc9f51ad8c1` | 2026-08-19 | `fix(test): #1247 correct 3 regression tests to actually hit their fix paths` |
| `13aa9f81ac914c098e7b6238c8ff96d4ea058e4f` | 2026-08-19 | `docs(evidence): #1247 TSan gate blocked by WSL2 OOM (build-tsan killed, dmesg 5 OOM); UBSan runs with ASan build` |
| `d41a8acc8f53e082a97fff69c2fff924e4eda0e6` | 2026-08-19 | `test(regression): #1247 evidence-matrix 新增测试 rows landed (12/12)` |
| `7bb432cf43bf34ef478992184c5c6b73dc83b09d` | 2026-08-19 | `fix(test): #1247 replace_program test alloc-dealloc-mismatch (new vs FREE)` |
| `16b681f8ade3fad3ee23b72dc530ba3db5ceecd1` | 2026-08-19 | `docs(evidence): #1247 post-C ASan re-verification record` |
| `d0f0efdb56ec311b213d31a3385a618ddda5e28a` | 2026-08-19 | `docs: WSL2 memory discipline (LTO off for dev builds, ASan -j2) + plan :402 error() semantics` |
| `e242748b2986bf2a0d797e3b7a13ebf3d289a934` | 2026-08-19 | `docs(agents): WSL2 memory discipline for builds (6GB total, LTO link is the hog)` |
| `6ec1c1773ccc16eeb313c5821d3818fd91dfc78d` | 2026-08-19 | `docs(evidence): #1247 TELNET-1/3 test column landed (C++ layer)` |
| `bbd28346bcbeb42252a6a31270a6236f0639eeeb` | 2026-08-19 | `test(telnet): #1247 K4/K5 regression tests (TELNET-1/3) + AGENTS.md quantitative verification` |
| `bafd83b58256d0b4be7405e6d7a04ccbd36d93d9` | 2026-08-19 | `docs(evidence): #1247 C-S2 numbers aligned with committed raw JSON` |
| `8d32e17d66cbb60d85d61a33897d0c98b0cc80bf` | 2026-08-19 | `test(bench): #1247 C-S2 raw A/B JSON (10 runs, before/after)` |
| `7e84983b122f6858e753b119dc91807b1ec40482` | 2026-08-19 | `docs(plan): #1247 C-S2 gate revisions + raw A/B data + cleanup` |
| `5a0765eaba6922f86287e32c2b3c9b360fedcda3` | 2026-08-19 | `fix(compiler): #1247 C-S1 error() exception semantics + C-S2 A/B gates` |
| `5d0e1530f02df778d6fbae7dca338c5d2fcaa780` | 2026-08-19 | `feat(bench): #1247 C-S2 bench_compile + fixed LPC compile corpus` |
| `1ec243e5f6b754e4995604e1c3ad28681d3eef80` | 2026-08-19 | `feat(compiler): #1247 C-S1 scratchpad -&gt; compile_arena migration` |
| `45a2c4f693c2d45e77f45412f0c947d70997363d` | 2026-08-19 | `feat(recompile): #1247 B-S3 per-kind lifecycle policy + migration transaction` |
| `2fb626048ad343e8bd5276a386d1749cb12e46c9` | 2026-08-19 | `feat(recompile): #1247 B-S2 pure recompile_layout module + full descriptor` |
| `7eeaae3c2c08c5f5994d8655e5431e042409ae70` | 2026-08-19 | `test: #1247 B-S1 replace_program block-move regression + layout assert` |
| `3916813bd4e09543780efdd4fdd130a5a585f800` | 2026-08-19 | `feat(vars): #1247 B-S1 ObjectVariableBlock decoupled variable storage` |
| `f624ff7ded4e2f957e9254d88ae370afa22f23ef` | 2026-08-19 | `docs(evidence): TSan full-ftest report for #1247 A phase` |
| `a3c015c0a46b1aa7c569241ee599bf192caa2c97` | 2026-08-19 | `docs(evidence): A-S2 relabel to factual 核对说明, dedup tail note` |
| `e279b947185818a09e2dfd06bafa0a8bd1ca6905` | 2026-08-19 | `docs(evidence): align rule-3 test column with repo reality` |
| `91a5e446ff93d3bf00cf5d7e5f8a1c1f95c298a6` | 2026-08-19 | `test: #1247 A.3 rule-3 runtime regression tests (a2_runtime.lpc)` |
| `12dec936e06cc4c1d5f21952502d768b4e22bf16` | 2026-08-19 | `docs(evidence): A-S2 SHA backfill + ASan/UBSan re-run after A-S1/A-S2` |
| `682f7703b0bcceae4d0d4eb889c18a9561ccf9a6` | 2026-08-19 | `fix: #1247 A-S2 grammar_rules_exprs/types ports (INT_MIN const-fold, void... guard)` |
| `94f5176f5aafcd5c9471510b7936d2188de5992f` | 2026-08-19 | `refactor: #1247 converge source-spelling resolution into vm/internal/source_spelling` |
| `d8ef20b0aa529fb4823c752d9f289a65510de3d6` | 2026-08-19 | `fix(security): #1247 batch9 .lpc loader, PARSE-1/CALLOUT-1 crash fixes, OBJ-2 nest semantics` |
| `81f7e203c4ca1609a1188fc30b99e20b8e534d3e` | 2026-08-19 | `style(matrix): normalize #1247 MATRIX-1 comment indent (2-space)` |
| `f466a3c4f8104d5e61fea616382dc6821d8019d5` | 2026-08-18 | `test(regression): #1247 19 upstream test paths (batch8)` |
| `028955dde22125da496ff8c811c837bd3e058f50` | 2026-08-18 | `fix(security): #1247 A-S1 batch7 ASYNC-1..9, MUDLIB-5..8 equivalent` |
| `b121f6e0dd2f253768fdcca51ca3399319b771bf` | 2026-08-18 | `fix(security): #1247 A-S1 batch6 CONTRIB-1..4, DB-1/2, ARRAY-1/2, SPRINTF-1..7` |
| `538e2531fac4f106d82cb4f4a23b0753053e66ef` | 2026-08-18 | `fix(security): #1247 A-S1 batch5 PARSER-1..9 deferred verb nodes (UAF)` |
| `db3a653c38fece48433626ee50d2546a71333aa4` | 2026-08-18 | `fix(security): #1247 COMM-1 remove dead detach-path check; ledger + plan sync` |
| `7a005dce891c6897aae556f64ce667b55d50d40f` | 2026-08-18 | `fix(security): #1247 A-S1 batch4 matrix indent + COMM-1 bad_init_call teardown` |
| `4056b8a276620807d87fd140cd8070091ec5305e` | 2026-08-18 | `fix(security): #1247 A-S1 batch4 RECLAIM-1, CHECKMEM-1, DWLIB-1, EXTERNAL-1, MATRIX-1, APPLY-1/2, MAPPING-1, TRACE-1, SOCK-2/4` |
| `916da12936befe3c66d444cd6ed86b8d2c535be7` | 2026-08-18 | `docs(evidence): #1247 A-S0 matrix + #1247-equivalent anchors on 10 covered points` |
| `c065ca7426f4f75c232ee7c11f38aee2373e7f32` | 2026-08-18 | `fix(security): #1247 A-S1 batch3 PORT-1/2, STRALLOC-1, STRUTILS-1, COMPILER-1, DISASM-1, COMPRESS-1, ADDACTION-1, CALLOUT-2, EFUNS-1` |
| `9565ccb47ce0981ea1e86c2001cd5f1b65b64219` | 2026-08-18 | `fix(security): #1247 A-S1 batch2 BUFFER-1, OBJ-2/3/4, TIME-1, ED-1..4, FILE-1..3` |
| `0d3f8ed8f83a2918dffe3754c1aed069a6b7697e` | 2026-08-18 | `fix(security): #1247 A-S1 batch1 TELNET-1/3, TREES-1, OPS-1/2, INTERP-1/2/3` |
| `2785c8a6165eab7d32842306468cd0b48eab17aa` | 2026-08-18 | `refactor(recompile): 删 kTimeout case 死赋值（分支自组 claims 消息，reason 未用）` |
| `ef2f20ac3721b938943c45ad1987729ea83cd59f` | 2026-08-18 | `fix(recompile): quiesce 收口 — 移除 main_draining 前置检查、failure 枚举分派、死计数器清理` |
| `f2b3e6f69d767d28e482c8bc9cace597a1845ee6` | 2026-08-18 | `E3 v2 Phase 2: 事务三段提交（create 段 + 回滚）+ P1/P2 收口` |
| `af24bf61decb5b29fc8c795b5348dcca43393d71` | 2026-08-18 | `E3 v2 Phase 1: transaction-based simul_efun reload (master/simul_efun, no create)` |
| `40a0420f408f185ce3185f44f8b5f034627cab38` | 2026-08-17 | `docs: 收尾打磨（§6 清单去重 + L2 状态补 hash + 行号 1258 精度）` |
| `1ff878bcb03a23dd0f9e6099af26a13eee31a6e7` | 2026-08-17 | `docs(plan): v0.8 全阶段闭环（L1-L10 完成）+ P6-P8 状态标记` |
| `5e1f8750b1f999eea848f8bbf323ca0de90332c1` | 2026-08-17 | `docs(plan): L10 T3 lpcshell 前置基建立项` |
| `be0ae8b0b7e0a6667f55360d6ff158882f5eafad` | 2026-08-17 | `docs(eval): L9 E4 read_source_line 移植评估` |
| `76783215dbe19688ec53584b900dd6b8ba7bec5f` | 2026-08-17 | `docs(design): L8 E3 v2 设计（master/simul_efun 热重载 + __INIT/create + 回滚）` |
| `7bb356459fdf4d5fe0cd28fabba7f2e6473bf82a` | 2026-08-17 | `test(recompile): L7 热重载压测契约 + bench 基线 + ASan 零报错` |
| `af507fbca8bce06c63054fce879ec311f05ec662` | 2026-08-17 | `docs(evidence): L6 TSan 验证完成（全量零警告）+ 分类表` |
| `3dafdbf0375d2f1efea63e19bfff2a244d4de116` | 2026-08-17 | `docs(evidence): L5 lpcc 冒烟补跑（mudlib 内路径，exit 0）` |
| `b7141ebcf8364f126ddb873457993fda6de88742` | 2026-08-17 | `refactor(tests): RAII helper 统一 current_object 临时赋值惯用法（L2）` |
| `edf5b8bfaed354b4487eb9b3338d439881a22b61` | 2026-08-17 | `refactor(compiler): lex.cc 四处局部 extern 替换为正规头文件（L4）` |
| `4830d2ba31854c06f64b9bf43ec84f29baab5e69` | 2026-08-17 | `fix(build): UBSan null-check TLS 误报全局关闭（L1 定案）+ 证据文档 L3 改写` |
| `0ca1f840e703264f7feee2a55b41dd253d257c6c` | 2026-08-17 | `refactor(recompile): 事务模块抽取 + 资源自持 + 锁纪律收口（架构审阅）` |
| `424049dcac1ea94085cfc4e4778cad62da169cb5` | 2026-08-16 | `fix(build): 限制容量脚本并行编译数` |
| `b223927fb936cf8a0c88162f46c1f1370f139e47` | 2026-08-16 | `docs: 提交 recompile 专项设计 v0.2-v0.4 审计版本` |
| `4aef7ad55052b8b938fc604ffc261852299940fc` | 2026-08-16 | `fix(recompile): 执行中守卫补顶层帧检查（架构审阅）` |
| `6099a8177885dc06486d241270eec87d3bcce83e` | 2026-08-16 | `test(recompile): P6 回调保留与重复重载 + efun 文档` |
| `4d15f15c8afaf79e1617af8995a90336950a5adb` | 2026-08-16 | `feat(recompile): efun + config + master 授权（E3 P5, v0.4 §5）` |
| `e7612199b40356ebfd0a307a00b1c071e45363bc` | 2026-08-16 | `feat(recompile): staging compile + layout + snapshot + no-fail commit（E3 P3/P4, v0.4 §8/§9）` |
| `353ff2042d2495c024d810bf1df20c0bdec0bd36` | 2026-08-16 | `feat(funptr): program generation + creation-time program（E3 P2, v0.4 §10）` |
| `225fc567802856fb1dc4232ce0b3d3dc67be38b8` | 2026-08-16 | `feat(owner): recompile quiescence gate（E3 P1, v0.4 §7）` |
| `104bde1319dca05d4f98fc9ab476884f5d338da0` | 2026-08-16 | `docs(evidence): R 系列纠正记录与基线转绿（A5）` |
| `67e0268044db06eb3c07a1f5706cfb283d91dc72` | 2026-08-16 | `fix(regression): 定位并修复当前基线三项红（R5）` |
| `7f0fdea57f0637ac0cb57ae3102bb7b4038dc5b1` | 2026-08-16 | `test(core): cover set_clean_up lifecycle（R4b, A7）` |
| `24cec31fe661d577161014c7d8d7ace28c4d74bf` | 2026-08-16 | `fix(lpcc): validate audit arguments（R4a, A4）` |
| `39289f4ba07dc80adbbb9b47a72a203e8de3c48b` | 2026-08-16 | `fix(contrib): serialize process environment access（R3, A3）` |
| `1437c4cf2438cbd5e62c0f5688874f7f3fd27410` | 2026-08-16 | `fix(fuzz): harnesses fail closed and self-validating（R2, A2）` |
| `a7344288c42dc7abc7f5eb808e1f6a75bb806d91` | 2026-08-16 | `fix(lws): adopt accepted sockets on default vhost（R1, A1/A6）` |
| `ef4759b71cc0c1eea7fca71de27ca79620ef7622` | 2026-08-16 | `docs: recompile_object 热重载专项设计方案 v0.1（供审计）` |
| `eba5b62e4276fd66adefd885e0807743aa201786` | 2026-08-16 | `docs: 遗留项执行记录——S16/E1/E2/T1/T2/T4 完成，E3/T3 授权待专项设计` |
| `fff68619f25cb720a11727b02dce6a84fad15b43` | 2026-08-16 | `feat(lpcc): --batch 批量编译（T4, 上游 3ec802f6）` |
| `61ab872eaaa6ec6742dae97b399022782962d2eb` | 2026-08-16 | `feat(contrib): get_os_env/set_os_env 配置 allow-list（T1, 上游 88b21f97 #1238）` |
| `1231615204ab2d452c986fe0fe55e5e7e69279c4` | 2026-08-16 | `feat(fuzz): AFL++ compile/restore harness（E2, 上游 b1fb96f3 #1317）` |
| `a3865995f177a18960bf053ac00915ab87a5c6ed` | 2026-08-16 | `feat(contrib): 循环引用回收 efun（E1, 上游 PR #1276）` |
| `0322733b854a09821689428d45ce714f1dee7ffc` | 2026-08-16 | `deps(lws): libwebsockets 4.2.1 → 4.5.8（S16, 上游 8fe05a5d #1260）` |
| `38b4adb38d5a905254d3279345d6e2b65104bd60` | 2026-08-16 | `fix(object): dealloc_object 去掉恒真数组判断（S8 修正残留）` |
| `2657d589ee72b815cdf03a83b44b08ad6cd90daa` | 2026-08-16 | `docs: M6 收尾——方案状态表与执行证据汇总` |
| `8f759db11e5844294726e792236a06516b323c08` | 2026-08-16 | `perf(strings): 纯 ASCII sizeof/索引 O(1)（P2, 上游 1e4d4145 #1344）` |
| `bec0f5a5293354ea58b5bf296ccd8b852d87f362` | 2026-08-16 | `perf(vm): 热路径去 per-svalue 堆分配（P1, 上游 3e341817 #1342）` |
| `dd6643045e0c32f3222f90507f119c1dc13a9c16` | 2026-08-16 | `fix(audit): S14/S15/S17 三批小修复` |
| `8b878d68c7c34728419bebe9ebbac26474a8be80` | 2026-08-16 | `fix(restore): AFL++ 发现的内存安全修复（S13, 上游 b1fb96f3 #1317）` |
| `f4d220f575d5feb1007ee9f0d41b1a26fd302dc2` | 2026-08-16 | `fix(ops): f_lsh/f_rsh/f_xor 清除残留 T_UNDEFINED subtype（S12, 上游 dca0eae0 #1302）` |
| `93ed8e8b5f934e9caaccd09708e9ebaad380f64c` | 2026-08-16 | `fix(audit): 5 latent gaps 本地适用 3 项（S10, 上游 8b0aee8a）` |
| `44ac54641ab6556261e38fb30fcccf2d1565aa4c` | 2026-08-16 | `fix(debugmalloc): md 链 XOR-mangle + dealloc_object 变量释放（S8, 上游 b0d3d297 #1330）` |
| `8d8dc4821499787ccecaecb5b55fb038cecafd78` | 2026-08-15 | `fix(refcount): object refcount over-decrement 系列（S7, 上游 948b49ed #1327）` |
| `67ab54cded929877a5046c8ea6a85dae5eb5bea1` | 2026-08-15 | `fix(lexer): #if 求值器 C 栈递归上限（S6, 上游 4d5345f5）` |
| `c272cb178062271282c3527a0eb28c38f1883005` | 2026-08-15 | `chore(lens): 移除 pi-lens 项目缓存与会话数据（扩展已卸载）` |
| `6d061f61454b7d123d7496fb0deed72b5c561488` | 2026-08-15 | `fix(stability): 2026-07-20 审计 10 bug 中本地适用的 6 项（S5, 上游 d9171788）` |
| `7b8934edca0c1cf29816171755c84aa2d8ea3b54` | 2026-08-15 | `fix(apply): apply_low 默认参数填充栈损坏与异常展开泄漏（S4, 上游 2d317e45）` |
| `00f610d6bdd6bf4faeebbead18846e6bed959c74` | 2026-08-15 | `fix(default-args): 默认参数跨 4 条调用路径填充 + helper 名冲突（S3, 上游 aec12ca7 #1298）` |
| `cab4c0fa1ad23131266af5fa57511008df221044` | 2026-08-15 | `fix(interp): string/object 拼接 UAF 与 F_ADD_EQ 栈不平衡（S2, 上游 ec9b6a4a）` |
| `e6451f2aab650d85841d8244129a94422013e75e` | 2026-08-15 | `fix(present): object_present() 空指针崩溃（S1, 上游 98f09f3d）` |
| `c63c0629db8aa5310b3f303ea4b2ae002f4c5e27` | 2026-08-15 | `docs(evidence): G0 可复现基线通过——upstream 配置、候选水合与证据文档` |
| `c7d9a3f323d4e95cf5a3575f76c5b5e91c54ce67` | 2026-08-15 | `docs: 提交上游同步优化方案 v2.4 与 pi-lens 会话记录` |
| `80b0f32902c83c48c4961b519b6d126a8368a107` | 2026-08-15 | `chore: 项目级禁用 LPC LSP（.c 为真 C 代码，避免 pi-lens 误分析）` |
| `2699aa63e86b358811c026419d1f35e48aef7a0e` | 2026-08-11 | `Merge pull request #36 from FengYunCalm/feat/exec-audit-plan-2026-08-09` |
| `0414074eb6dace121fe00874bb651b24d32cad5c` | 2026-08-11 | `docs: close PR 36 final merge audit` |
| `f45778ff7f2894fbdd0291d1b62d7a9b73a943cb` | 2026-08-11 | `fix(ci): download bound capacity raw evidence` |
| `e588262ba71e4922f24d07852299f5a225cfd9e6` | 2026-08-11 | `fix: support POSIX timer thread ids on musl` |
| `8dcd2e0db945eef2b3b83b0df0c503f739367953` | 2026-08-11 | `docs: record PR 36 concurrency audit closure` |
| `2efe06a1a3513c8d26aabf18f94ae1c0d9a84fcf` | 2026-08-11 | `docs: record PR 36 concurrency audit closure` |
| `3fa46cf46106e83aeb30fb4a117496823c7df6a3` | 2026-08-11 | `fix: close PR 36 concurrency and runner gaps` |
| `87732d7c31cfa814f021140f72488f41fbc01ec7` | 2026-08-11 | `docs: add final PR 36 merge audit` |
| `04088aa90fd6c12dfb597b9dccbce7585a87a93f` | 2026-08-11 | `fix: close PR 36 runtime and release audit gaps` |
| `4903e7ae3c92cccb9c7de227f9e6fb3057a58d76` | 2026-08-11 | `fix(evidence): retain raw benchmark files next to envelopes (R2-F07)` |
| `5ae332e5885272ffcf9cbac6b933433722dd4218` | 2026-08-11 | `chore: ignore local harness state and isolated-runner artifacts` |
| `92868e5f14d6ab180b2eee11f7a691c9c89fdb96` | 2026-08-11 | `fix(evidence): directory scan skips envelope-bound raw files (R2-F07)` |
| `77b0535e78077de68cec4e5adaaddb022644f5df` | 2026-08-11 | `docs: reconcile T01-T18 status with current-HEAD evidence, link second-round audit (R2-F13)` |
| `8049d73144cb728d502cb2d0350c71b57dc9b855` | 2026-08-11 | `docs(evidence): commit current-HEAD capacity envelopes with raw digest binding (R2-F07/F13)` |
| `093884d38af8a85dfe837d9988a2b0649bd35733` | 2026-08-11 | `fix(testsuite): driver port-0 support, isolated runner consumes it, concurrency stress (R2-F11)` |
| `dd6c917b2df912adbfefa34c50cea085b70935c4` | 2026-08-11 | `fix(gateway): event-accurate output continuation metrics (R2-F10)` |
| `b2948a7b685bc7c8b5ea18e0d7b213f2e14a2a86` | 2026-08-11 | `fix(release): gate-first DAG, single-build digest promotion, least privilege, dry-run (R2-F08)` |
| `3ac781f36c3c147d7f9bf504ea6e04340c1d6c04` | 2026-08-11 | `fix(sbom): structured github-actions entries serialize deterministically (R2-F09)` |
| `f130bb00756156a61a31b5f264e98a878233865b` | 2026-08-11 | `fix(supply-chain): commit-SHA action pins, digest-pinned images, verified downloads (R2-F09)` |
| `db4e46fcd6218f984dd585edc708a5103bd9be57` | 2026-08-11 | `fix(evidence): envelope-only gate, derived cleanup, strict time/run-id/digest checks (R2-F07)` |
| `f84d5fbcdb0004740e53a43d48cfaaa3bdab8cd0` | 2026-08-11 | `fix(core): sys_reload_tls main-thread + master-authorization contract (R2-F12)` |
| `e092029d072bbd622735e416a1b67e9069f4daa2` | 2026-08-11 | `fix(object): enforce main-thread object-target admission, split RefGuard API (R2-F06)` |
| `aae123f44726dbfbc52e060d026b33477390d1ea` | 2026-08-11 | `fix(owner): lifecycle-slot reservation, queryable failed tombstones, full payload metering (R2-F05)` |
| `852112650123bf685964f4cbad2775052a07056a` | 2026-08-10 | `fix(docker): lowercase-normalized image refs, metadata tags, real driver path smoke (R2-F03)` |
| `d7fa70f26b4d4e02f71cb5b76f1ed64d1a4ef413` | 2026-08-10 | `test(fuzz): fixture-per-scenario lifecycle, fd re-resolve, hard smoke assertions (R2-F04)` |
| `a1a82d7fd56fc191fa87fbf0d23e499efa0cf77a` | 2026-08-10 | `build(cmake): split fuzz targets, never build no-main gateway_fuzz by default (R2-F02)` |
| `f346a512b9ff8758a3dc15be52461ad507af01b1` | 2026-08-10 | `fix(ci): restore workflow parsing, unique check names, actionlint gate (R2-F01)` |
| `1b664e090c95d402b636908528719f298c1de096` | 2026-08-10 | `build(sbom): deterministic timestamp from manifest last_reviewed` |
| `71ab40f0d22c03eeabf0abbb79beebf60c9b641d` | 2026-08-10 | `docs(release): record gtest repeat limitation and regenerate SBOM` |
| `72bfc82652409f6b5092a3e6bf0715bc74d58e07` | 2026-08-10 | `docs: reconcile T01-T18 status table with current-HEAD evidence (F09)` |
| `5e7a766b84d27677ffa089f74a8372ea62bf46fe` | 2026-08-10 | `fix(release,evidence,supply-chain): gate-first release, non-empty evidence, CycloneDX SBOM (F06/F07/F13)` |
| `a7713cf51710c2f106faa6ba5c05534687de6a24` | 2026-08-10 | `fix(test,gateway,ci): real libFuzzer target, continuation accounting, sanitizer scoping, port TOCTOU mitigation (F08/F10/F11/F12)` |
| `a7233fff53ae27d5ec7ecc6147e093a13ca62ca7` | 2026-08-10 | `fix(object): close handle acquire TOCTOU and worker refcount contract (F05)` |
| `baffcaad71da6e619024000a9320e4654103b09f` | 2026-08-10 | `fix(owner): transactional future submission, hard caps, exception terminalization (F02/F03/F04)` |
| `5feb67ac4dbb083cbf4ee9c21485103a62d858c9` | 2026-08-10 | `fix(ci): restore trusted CI baseline (F01)` |
| `bc92d6a427ef5892b2b5cc031769a0f69157f066` | 2026-08-10 | `docs: record execution status of T01-T18 on the plan document` |
| `be59cfd1dfe7af92762d557cf8b5c70d71c9e4b8` | 2026-08-10 | `ci(docs): doc/evidence lint job for all changes incl. docs-only (T17)` |
| `522f4ed2863644ae8d3dabf02025b207fb3c84fe` | 2026-08-10 | `docs(runbooks): release/rollback/gateway-security manuals and capacity prep (T16/T18)` |
| `0d5b213ab5fbf8e39043c3cc5290b5c1e3dd84cb` | 2026-08-10 | `test(fuzz): gateway frame parser fuzz target and session-id boundary table (T15)` |
| `097ce6988af39b051121e3354528adab5fd23c23` | 2026-08-10 | `test(owner): scheduler capacity matrix with evidence-wrapped reports (T14)` |
| `7256cdcdf735469cb22486c9772beb520812e165` | 2026-08-10 | `feat(gateway): add master output scan accounting before any index refactor (T13)` |
| `78ffebd35f9cda550145d4519e95f22013ff57ee` | 2026-08-10 | `fix(future): terminal TTL, hard cap, capacity rejection and observability (T12)` |
| `c8740ab63bd009b07249ffc683fc19e18df8c1a6` | 2026-08-10 | `fix(object): add acquire/RAII reference guard for cross-lock handle use (T11)` |
| `6044641ac4d7ce3fb813422c5a435c6974577540` | 2026-08-10 | `fix(owner): executor exception boundary, RAII claim release, fault-injection tests (T10)` |
| `e42147b639930ab688d134d33237efe8cb469b42` | 2026-08-10 | `security: report channel, SLA, key-fixture packaging guard (T09)` |
| `68242673eef1178fb0eead428861226253570424` | 2026-08-10 | `build(supply-chain): dependency manifest, locked jemalloc checksum, SBOM (T08)` |
| `3db4f67c92d71381710f6bfee1cfed81cf707a6a` | 2026-08-10 | `build(release): add named CMake presets for reproducible builds (T07)` |
| `f6db23d6997c86233012f1694870d0fb948b4fdb` | 2026-08-10 | `build(cmake): stop polluting source tree and fix glob/include hygiene (T06)` |
| `63f68fb61d34086aaf7338a70671f620f1e6e212` | 2026-08-10 | `build(ci): layered ASan/UBSan/TSan sanitizer matrix (T05)` |
| `34781bebba57802e7e871286592f9da605579467` | 2026-08-09 | `test(testsuite): isolated LPC runner with free ports and loopback bind (T04)` |
| `34e4601fcb1a4e689406a0e68b40c5e10eae4948` | 2026-08-09 | `docs(evidence): register evidence manifest, gate checker and fix doc drift (T03)` |
| `a3df7f2b3478fa13084436f31921dca792ad79e6` | 2026-08-09 | `ci(release): reorder release DAG to artifact-first, tag-last (T02)` |
| `8e82d4cf8ee79e36af1825e72ff08279f89c6863` | 2026-08-09 | `fix(core): harden sys_reload_tls port index bounds (T01)` |
| `997931cb722ddcc3acba6b5f5e21aee2afd996dd` | 2026-08-09 | `docs: add codebase audit and execution plan 2026-08-09 (T01-T18)` |
| `24bd5f4f5126963966d1d91787f327f4cc84a5e7` | 2026-08-09 | `parser: bound debug output formatting` |
| `8ae524458170b3ee35365042613f0812ca860d1d` | 2026-08-09 | `contrib: guard pluralize prefix length` |
| `509bb6010b60c7d315393e261a0b0b6b9c01221b` | 2026-08-09 | `compress: use dynamic path storage` |
| `9bbf4ca61c32561d900434a4cb3124f194814228` | 2026-08-09 | `vm: build call_other diagnostics dynamically` |
| `05eb98fa14f68eff8d94db97fe5946fe1ab50302` | 2026-08-09 | `gateway: preserve 64-bit pressure counts` |
| `77b3c9604ded2e9a1e5b6bbe546a6e3426e0e10b` | 2026-08-09 | `owner: preserve 64-bit future counts` |
| `053c485e564f13478510cc9977a4dbbe21c2714f` | 2026-08-09 | `gateway: preserve main queue count width` |
| `6e99c902cb94503cf835fde3a5d93d2b0856a1d7` | 2026-08-09 | `owner: preserve 64-bit scheduler counts` |
| `75a2e9b0e1672fc4156c2b4916c99d89f3fe68fe` | 2026-08-09 | `object-store: preserve 64-bit global diagnostics` |
| `68e6bc038747f117f0310bd48ef10e33e96ff355` | 2026-08-09 | `object-store: preserve 64-bit shard diagnostics` |
| `bbf5a045b5b7a4461ee4d709007206fb242aa39f` | 2026-08-09 | `gateway: preserve 64-bit session diagnostics` |
| `91466ac059928e2021c4a83e29aa75a836bcd4db` | 2026-08-09 | `gateway: preserve 64-bit session counters` |
| `aa223bdc73118a2cb706597836d3b2633a93c5e6` | 2026-08-09 | `worker: preserve 64-bit status counters` |
| `401ef4381c7607fea804634b2dc5faf31bd89c50` | 2026-08-09 | `gateway: preserve 64-bit runtime counters` |
| `34f48bdc2eb4ae45eef06ec71b80b796ad1a0c1b` | 2026-08-09 | `owner: recover main drain after callback error` |
| `d0cad5d59d6579a0ef8ec9f4851681a68cb808a5` | 2026-08-09 | `owner: append detached main traces after unlock` |
| `d3dd6f36ee8713eb9e41d69fa4451cd93d8e9012` | 2026-08-09 | `owner: record queued trace after admission` |
| `1beca539d7c6285a07e2f53c717e0dc952152a61` | 2026-08-09 | `owner: process queued cancellation after unlock` |
| `e599910edab7f5cf625c3c599a43cedbd3b48b0b` | 2026-08-09 | `owner: process drained tasks after unlock` |
| `7bb9d4dacdb125413a0fdeadda7609aa8f803c1e` | 2026-08-09 | `owner: keep terminal completion outside coordinator` |
| `e0035c2f1611f3322bf428e09e39673050faeb9b` | 2026-08-09 | `owner: complete compute-result backpressure after unlock` |
| `70377996dfcaa62c5b9a0fe82ee9d53058dfe5fd` | 2026-08-09 | `owner: complete rejected submissions after unlock` |
| `c66ba566c920b9db96c0f4fb126e1a0bcb83cf23` | 2026-08-09 | `owner: shorten message submission lock scope` |
| `9de1467d1c798de139356f91a09beb031dcafd17` | 2026-08-09 | `owner: linearize trace append metadata` |
| `6c0fae231783660f8c50c6de61c8091aa47439d9` | 2026-08-09 | `owner: preserve diagnostic counter width` |
| `5cbb0f3a4fa24f9fc66c5d5c2c4bf312f89580ed` | 2026-08-09 | `owner: build mailbox status after runtime unlock` |
| `a822ef93de811f62b2f289b3deafac618156d071` | 2026-08-09 | `owner: detach scheduled tasks before completion work` |
| `3221856360cc4de5d4e8ace9f91ad603597344c5` | 2026-08-08 | `owner: guard public scheduler depth reads` |
| `d3bbc7c679932d1615a57d2939c56bdaa58ae147` | 2026-08-08 | `owner: guard scheduler debug reference traversal` |
| `4c933f53340b50c8ad7b40b55eb22538383f5df6` | 2026-08-08 | `owner: build status mappings after runtime unlock` |
| `561ba3eb537f20139078e94d4048c62a51803362` | 2026-08-08 | `owner: preserve status counter width` |
| `e6a75e5851dc8122bd1666e96f49ae8025f44eea` | 2026-08-08 | `owner: reject non-terminal future completion states` |
| `216ca81d405df91b9fb5fb4edc2ea95553a0e61e` | 2026-08-08 | `owner: resolve future targets after store unlock` |
| `332cdfa6f5cc7ac036133b8af09b0eb1d15575c1` | 2026-08-08 | `owner: preserve runtime metric width` |
| `a0f665c3b33f682731bffaafbd0227d69177d432` | 2026-08-08 | `gateway: expose room projection cpu averages` |
| `c377fcdbda16cfa317394ceeac081f176842cab3` | 2026-08-08 | `gateway: track generic future watch polling` |
| `a4e119023804d07891b0fc7aeb200e35e43cbd7b` | 2026-08-08 | `perf: track owner future pending count` |
| `eb66bdb4cde234794d950d44b9f7c0190edc3610` | 2026-08-08 | `fix: share gateway future watch capacity` |
| `c92cd56f794233c511deb0baa8277ba09a37fd30` | 2026-08-08 | `fix: sync generated compiler grammar` |
| `4e19353b55c47fa2ce3fdd342fd31a035339ebd2` | 2026-08-08 | `test: cover parser rule reentry guard` |
| `dbd455087382016c43ea6f4ccef95e9842faa572` | 2026-08-08 | `fix: stabilize parser state handling` |
| `0b4352753a07e807fb8babeafb1c8a5380ce3fed` | 2026-08-08 | `fix: bound preprocessor directive diagnostics` |
| `40d142a101a9b796ba121c78b8f90a409b547051` | 2026-08-08 | `fix: preserve 64-bit LPC runtime values` |
| `7f2c9ac3eb8494267edceab876a2676dceb78b0c` | 2026-08-08 | `fix: harden mudlib stats persistence` |
| `839782d3ae105152778f9ad472fb78db3d60872c` | 2026-08-08 | `fix: harden PCRE capture and matching boundaries` |
| `ad606b36309d033c221521b4f318ad1dfec62405` | 2026-08-08 | `fix: harden database connection parameter handling` |
| `fcfd45f3141cc0554d0eaf0f258083c7baf00ace` | 2026-08-08 | `fix: make program disassembly output dynamically sized` |
| `b5bf1e0d62b866e1b28dea42dc066d69bd1d3ab1` | 2026-08-08 | `fix: make runtime checks portable on Windows` |
| `756ef5a9260847c2be9c7da2f6005090bf1a87e1` | 2026-08-08 | `fix: bound compiler and DNS debug formatting` |
| `7ae43c6f623e24382d6aa6b4b21e499b7a325fba` | 2026-08-08 | `harden compiler generator and array bounds` |
| `c6e2ae4c4db62d3d8171b128d20f506c69081199` | 2026-08-08 | `fix: bound file move path construction` |
| `8dc10dc9bc864c8949f3f4cd92676d169850f4cc` | 2026-08-08 | `fix: harden network address formatting and lookup` |
| `a2e3218aa1c7c6804ef21d38c2587eb45d93d307` | 2026-08-08 | `fix: preserve exact save header capacity` |
| `850250af31714e5562bf6fa4787c3d0452529573` | 2026-08-08 | `fix: bound save object serialization buffers` |
| `acc2e0259062a3ad5520c4c7ee6519aa87648229` | 2026-08-08 | `fix: make driver shutdown controlled and bounded` |
| `7e52781d2430c37ea68798c1673f5441efa6344d` | 2026-08-07 | `fix(gateway): correct EFUN stack contracts` |
| `2a515e016ed4e831bb6fa06f3973c99ffa2b46c2` | 2026-08-07 | `fix: preserve gateway destroy efun stack contract` |
| `95845c304ca0fc14a24aac2ff89716d40b2845f1` | 2026-08-07 | `fix: use portable timeval microsecond type` |
| `bbe227ea05581c320fd8974286dc19c047280de9` | 2026-08-07 | `fix: preserve 64-bit file sizes` |
| `ad78a4513b44aee3142f7e8f45cc0e889ded03e5` | 2026-08-07 | `fix: harden Windows external process startup` |
| `edd09b4c95560d6465bdce9ff264eca583d5d2d8` | 2026-08-07 | `fix: preserve 64-bit write byte offsets` |
| `e4dbbdf4277e5ca977d10534947d5bab7d39f426` | 2026-08-07 | `fix: close network descriptors on exec` |
| `accaf4281dc22faec32e0331852dc34480ffd8ac` | 2026-08-07 | `fix: isolate external process startup` |
| `31d91eb7610cf539ac33ef1dc0d9d1435be5b4ae` | 2026-08-07 | `fix: preserve 64-bit read byte offsets` |
| `b95c5e75d1216756453ba18378ecdef09d633814` | 2026-08-07 | `fix: reject out-of-range json integers` |
| `b024616bf2329a70fc86bcb19ee3082f03bf48a9` | 2026-08-07 | `fix: validate mud port frame lengths` |
| `0e03eb6cf5210b19ac2802f2aa05891e56d92c6c` | 2026-08-07 | `fix: reject unauthenticated external gateway bind` |
| `ce1687c5444263bd41952edd0a5f47e8eddf5d9d` | 2026-08-07 | `fix: bound gateway aggregate output fifo` |
| `b236d3500f0e23a1c645a2c111cb8e426e56dfff` | 2026-08-07 | `fix: validate gateway session identifiers` |
| `2d64f4650c901d2784f1e28668197f292ff07603` | 2026-08-07 | `fix: bind gateway ingress stream ownership` |
| `5f696e61740e2d38f249e59998770e448ce02853` | 2026-08-07 | `fix: bound gateway slow master output` |
| `d669d3637c273231bd361f70ec4dd012b0eb7bbe` | 2026-08-07 | `fix: bound gateway inbound frame buffering` |
| `c9dce9842989cba87a99df512bdb1bd6d76acd0d` | 2026-08-07 | `fix: bound gateway inbound json conversion` |
| `114c2999dfda74393e9fa7b5cac598a67e081482` | 2026-08-07 | `fix: enforce gateway sys session ownership` |
| `0d082b2ebcaf1ed27163270bfd8cbb49cb7dea89` | 2026-08-07 | `tests: harden owner contract diagnostics` |
| `186e3e50bdd1255eaf7de25e1a9e5ef9a675ace4` | 2026-08-06 | `merge: retain multicore archive snapshot history` |
| `e45ba12b28da2d7d98740ecad13b021fe46e172a` | 2026-07-24 | `chore: snapshot multicore working state before recovery` |
| `9a024264debb25b281bf96dc9ffa1453c1e67a8e` | 2026-08-05 | `docs: update PostCSS security override` |
| `1aec8c82969c33328671467036c6fb699d064ac9` | 2026-08-04 | `gateway: preserve authoritative room batch totals` |
| `5f77f5c0950834a13af1d3d74567fb0571cc2aed` | 2026-08-03 | `gateway: compact room message wave transactions` |
| `b48b943b2d7b7bfdcb98ae733723e50dc6d5d547` | 2026-08-02 | `gateway: trust locally encoded room wire` |
| `0d236657c3546f3720701d505177dac8285c8911` | 2026-08-02 | `auth: verify legacy DES hashes safely` |
| `071f4d593f8167e094e7cc65f3a2216500310640` | 2026-08-02 | `timing: align performance wall with thread cpu` |
| `e1dddf9d39ed554d35b4b4b95bc3ce49eb84675c` | 2026-08-02 | `gateway: expose aggregate receive dispatch time` |
| `6eea1b4d3309aaf38504f2b66f1194fd9a8e6c4f` | 2026-08-01 | `gateway: yield after each admitted gameplay frame` |
| `bb353aa04aec285705f2d94caeb8f1c931d69c15` | 2026-08-01 | `scheduler: bound interactive runtime drains` |
| `4098492dc50e23fbefad273f1e04b7649f129850` | 2026-07-31 | `gateway: close pressured room output drain` |
| `ccd277caafb778fc43811ad913d356ef5d55d85f` | 2026-07-31 | `gateway: acknowledge replayable southbound ingress` |
| `46a65c27e9ced262c4622d472277add08773e466` | 2026-07-29 | `owner: add allocation-light commit observation` |
| `03bcbe1b75566963bcfd348a62fd1d7911d66c86` | 2026-07-28 | `gateway: defer status snapshots past read dispatch` |
| `70803314f05236adf82e28472df705d15e6072b1` | 2026-07-28 | `gateway: complete frozen room output pipeline` |
| `b8f63832c21e583767593b96e44fc53cd7b1ab7d` | 2026-07-26 | `gateway: fill room recipient slices natively` |
| `2cf188c9c545d0d6ed8286a067ca31bc29b562ff` | 2026-07-26 | `gateway: compact room wave sequence handoff` |
| `5cf31af726984c933346739891ec85c9377cb9b7` | 2026-07-26 | `gateway: reserve room message waves and reuse validated templates` |
| `021afff8759a8f4edd4bc18a8693c4c01e9f59c5` | 2026-07-26 | `gateway: queue shared room message waves in native fifo slots` |
| `16477a2454a2d096ba40f49d2d31dbd0f3bae9e4` | 2026-07-26 | `gateway: reuse validated room message templates` |
| `83145222ead1ef6f777d867c3000ba9dce8c6c5c` | 2026-07-26 | `tests: add representative LPC workload benchmark` |
| `3a4458c0cdbf613891a123c7002e1d2955dbe82c` | 2026-07-25 | `gateway: fill reserved room message batches natively` |
| `72a9de3450759b19deb378a36ff02bac367ba989` | 2026-07-25 | `gateway: bound synchronized read admission` |
| `9760473cc45eb5e503a0f3fd5a96238931056176` | 2026-07-25 | `gateway: reserve room fanout FIFO slots in native batches` |
| `ccb3a7017bfc376de7fa256f72c0e89b1de4e038` | 2026-07-25 | `gateway: bound dispatch and fill reserved chat batches natively` |
| `7e511907f6d9a3e5913ca086cf9ab0ea8827d22b` | 2026-07-25 | `owner: harden main-queue completion and shutdown lifecycle` |
| `2ed9191b037075394770870462127ec73bf3af10` | 2026-07-25 | `runtime: add prioritized walltime scheduling without hot-path atomics` |
| `2fd4031b3996de6f0f98b66f11cfc7a289c00860` | 2026-07-16 | `perf: allow native frozen JSON at JSON depth` |
| `f797740b892c50757248be8d57174c59d22f8ffe` | 2026-07-15 | `perf: avoid frozen JSON fallback exceptions` |
| `df70c1be233eb812da0c2542b82952c0e5daa830` | 2026-07-15 | `perf: encode frozen owner JSON natively` |
| `fca6c1c8e31e070a3024cad22369e7781b03303e` | 2026-07-14 | `gateway: preserve sessions across master reconnects` |
| `6f0bfdb27890bc47115fbc8fe16bac1a236c0ad5` | 2026-07-14 | `bench: measure owner async thread cpu domains` |
| `b0e1be4dafbcf30dafb7ef9d8f44864511f23f6e` | 2026-07-12 | `gateway: publish owner encoded output natively` |
| `acda4837008bf1f6e35a944d3888383b864e1848` | 2026-07-12 | `bench: measure session future publish cost` |
| `24081ff0abc662269a56bfcca9f7578e54d49a55` | 2026-07-12 | `bench: measure session owner submit upfront cost` |
| `7b9a7599cdc7e1b7414eaa2085f4caeee256a04f` | 2026-07-12 | `bench: measure owner submit watch orchestration` |
| `71009acae4efd37307e9b6969c58e8bf1bf9228e` | 2026-07-12 | `runtime: index owner futures by task` |
| `8f847614bfb8dfa82f750f2b4cbba1256c13f682` | 2026-07-12 | `bench: measure owner future lookup scaling` |
| `4696ba4493c4ab198859087da6f0c81983034799` | 2026-07-12 | `bench: measure frozen payload traversal cost` |
| `81ce6c4b2a4075ea845756ad413e783002519c28` | 2026-07-12 | `runtime: make LPC profiling explicitly opt-in` |
| `c129f645740d3bcaf0c12afbbb7a1ca80d67fcf9` | 2026-07-11 | `gateway: add generic owner future completion watch` |
| `60ceb8a55adebd0365ffc75e272b21506dc76b47` | 2026-07-11 | `engine: complete owner payload delivery pipeline` |
| `42afcad1eb0f7e91580c0891f3f7bae9d7aac295` | 2026-07-10 | `engine: route gateway work through owner queues` |
| `c654ef2395a3bfe95d2d3f2f0c8b49dd70957b42` | 2026-07-08 | `gateway: defer receive drains on threaded owner queues` |
| `761e304829e169bab34ff0f51401d9410ee64727` | 2026-07-05 | `runtime:` |
| `e94fea0b750ff8f004e353b63a1423ebd10d774e` | 2026-07-05 | `perf: add owner runtime thread matrix bench` |
| `2859ea8d5d749c52cfa9dc198100dfe86321c2cf` | 2026-07-05 | `gateway: harden session unbind cleanup` |
| `d51ae0bc12b1e2ce63b86c7d6e671a211ce1db42` | 2026-07-05 | `perf: compile out lpc vm profile probes in legacy builds` |
| `b5308f28efadda2a41f38d411fb4a002e6913657` | 2026-07-04 | `tools: record perf compare source metadata` |
| `bf6d6e4ac003e1138cf59d9c6ec1a21721cd9582` | 2026-07-04 | `perf: harden portable comparison and common vm path` |
| `4e0b03e1ceb056a8bf9285f87e93915ea92d2a13` | 2026-07-04 | `perf: make official comparison gate fair to legacy mode` |
| `41079561ae59354d865c16158fc69eb09411b87c` | 2026-07-04 | `perf: add performance truth gate` |
| `4be34224b7ed5f20226cc9981ec689c4c7e58a96` | 2026-07-04 | `perf: add lifecycle probes and owner access fast path` |
| `734d712a7c6ba6efd68e516010be38d8f75c25e1` | 2026-07-04 | `perf: add official FluffOS comparison harness` |
| `e507b1bff2d9d63443f208cfd69ea7cf18413bdb` | 2026-07-03 | `runtime: reduce gateway owner adapter tail latency` |
| `59410d44d6e3df1c235e1f40544ee786b8f631b2` | 2026-07-03 | `runtime: stabilize gateway owner stress paths` |
| `f493e25506c96d8aaba0a22cca04b65a39244f2d` | 2026-06-30 | `tests: make owner payload future waits yield` |
| `23f967ca6dd233904f6c74ef4b0705094c738f6c` | 2026-06-30 | `docs: refresh bilingual runtime overview` |
| `5a00f2363183fbda3b2dbddeadce2da5bf0aca12` | 2026-06-30 | `lpc:` |
| `13efdeebf3cb8590cf0107f98a7b708fcf2cd17c` | 2026-06-29 | `Merge pull request #34 from FengYunCalm/codex/fix-docs-dependabot-security` |
| `4b030d45ddb97a041d42eac7044aa5261b49bcf1` | 2026-06-29 | `docs: fix dependabot security updates` |
| `212e7685a6b7e7a5fdb1f3e3e2b846f252eb4ff6` | 2026-06-29 | `ci: fix github actions failures (#32)` |
| `85f3c6cdeef3adfaa0e799f2dac5e192f25616b0` | 2026-06-29 | `Merge pull request #31 from FengYunCalm/codex/codeql-action-v4` |
| `f02ba3cb642ae5f6e047a401dc7172075729888b` | 2026-06-29 | `ci: update CodeQL action versions` |
| `3efaedd7e14559b0e4b8165a5f146ddfbeb243e3` | 2026-06-29 | `Merge pull request #30 from FengYunCalm/codex/security-codeql-final-toctou` |
| `06ce567caab486414093d80da4c966b0bbb697c3` | 2026-06-29 | `security: avoid unsafe move restore overwrite` |
| `177f3eea7558f069af3c0ae35932c27db1407237` | 2026-06-29 | `Merge pull request #29 from FengYunCalm/codex/security-codeql-followup` |
| `cf0078aea964e9eaec15378f3ed5c662debe3b61` | 2026-06-28 | `security: clear remaining CodeQL findings` |
| `d1725da8b89660b3d6aed8f0232c2cdd32d8c54f` | 2026-06-28 | `Merge pull request #28 from FengYunCalm/codex/codeql-workflow-dispatch` |
| `98049e71c84d6a497da2609821c72378cce6f2d8` | 2026-06-28 | `ci: allow manual CodeQL scans` |
| `febf7205977121007aed6b9c9cb98e1cf6d12ef5` | 2026-06-28 | `Merge pull request #27 from FengYunCalm/codex/security-codeql-cleanup` |
| `cb53925ace1517fff3d1c9e44c6f0eb67ae836c1` | 2026-06-28 | `security: harden vendored crypto and libevent` |
| `c02ceb237fc51a14c5991a627aa6a9411b0bdca6` | 2026-06-28 | `security: remove filesystem TOCTOU checks` |
| `525fd405077251770b6b1a0906823bc7c6ca4ae1` | 2026-06-28 | `security: fix first-party CodeQL findings` |
| `e788abdcfbfd1fed126ec03a745a675aa12a1912` | 2026-06-28 | `Merge pull request #26 from FengYunCalm/codex/lpc-modern-runtime` |
| `57f9133a8238dc6246e2f2a6c58d7151468caf92` | 2026-06-28 | `docs: document lpc modern runtime contracts` |
| `0f5051ba9583a03f0d918aef3d1cf1d6d83839e5` | 2026-06-28 | `ci: gate lpc modern runtime baseline` |
| `0bb0be408a4f5b4dd86b0b1e2ba56b3e8f98c831` | 2026-06-28 | `tools: add vm object runtime bench and stress profiles` |
| `10b0eda1f4940919811661a5879b0a73c288c585` | 2026-06-28 | `runtime: unify callback payload strict diagnostics` |
| `8dd155bd1f41addab7be0d4f8f96f712198d470a` | 2026-06-28 | `gateway: enforce session fifo and io adapter boundary` |
| `af5a0addeb1fda47fe68651aff383a9fad2c2009` | 2026-06-28 | `runtime: harden scheduler fairness and tuning config` |
| `95077980e93da7bdc198fa2b3a47006c61481806` | 2026-06-28 | `runtime: add owner service shard registry and tick groups` |
| `db45c49b83c1874a0d4eb6e68ea50073597fa012` | 2026-06-28 | `object: add snapshot persistence pipeline` |
| `85d97aa655a3dfecb05299302500b7bf7821a73c` | 2026-06-28 | `object: introduce object handle capability contract` |
| `7cfe5e656b2047dd003884a8f541711feb3e455c` | 2026-06-28 | `vm: optimize apply dispatch cache` |
| `de127d999929f37fb41f7b6c95121b6408ce0aac` | 2026-06-28 | `vm: add lpc profiling and dispatch cache probes` |
| `293f121d5dc8b50e7c558c41d265825ca9ce18d9` | 2026-06-28 | `compiler: implement lpcc owner audit json output` |
| `db307fb5e657fa8a6eb50ded16d335488932cda0` | 2026-06-28 | `lpc: add owner safe modern runtime APIs` |
| `151f700a0dfa5f20a088f1fa2450811e883c8663` | 2026-06-28 | `develop: keep debugmalloc efuns exported as stubs` |
| `fa6ba42fd562e6e7152cb01931a01bb36917700e` | 2026-06-28 | `lpc: add modern profile pragmas and audit scaffolding` |
| `78999a729a12ccef1d64343b9153f15ce58c6bb0` | 2026-06-28 | `develop: keep debugmalloc efuns linkable without debugmalloc` |
| `96987d9e335f3549815498cab06cd556ace16370` | 2026-06-28 | `Merge pull request #25 from FengYunCalm/codex/engine-runtime-v4-hardening` |
| `403064a101f74fd635ed5544d97f13b3b8886f6e` | 2026-06-28 | `runtime: document and gate runtime v4 baseline` |
| `fd3d201fb349dfa6aaef871f110a3f0f378985a3` | 2026-06-28 | `runtime: unify owner callback diagnostics` |
| `3bde2671eddc6f9bab027ee2d837f6bcb37c78b2` | 2026-06-28 | `runtime: add owner scheduler backpressure controls` |
| `892f9c1ed9d12a5c4f2f1d8599d22141b97e1d07` | 2026-06-28 | `runtime: reduce object store global lock pressure` |
| `ee5fe87ba821b4e94ea732c2ff11fcf807d7b644` | 2026-06-28 | `runtime: add owner runtime benchmark reports` |
| `23da5a1b0ea4e08c15aa4d4bcdbebf240d699c2a` | 2026-06-28 | `runtime: harden owner runtime module boundaries` |
| `c314e5cb863edd3c60def4b4d13c40bb24709cf7` | 2026-06-28 | `Merge pull request #24 from FengYunCalm/codex/multicore-runtime-v3-architecture` |
| `cca8e785d9c59f9dcdcfbce3b3a3d74c72b465c1` | 2026-06-28 | `multicore: update runtime v3 contracts` |
| `b5e701872071b5586389e93172c9c222d51b1cee` | 2026-06-28 | `multicore: add owner runtime performance probes` |
| `1182f0effd8518d828fe1dfd155d9e8315f5dcbd` | 2026-06-28 | `multicore: make main fallback explicit only` |
| `fe8a3f8da6614b113512262ede0308e1e2009451` | 2026-06-28 | `multicore: unify owner callback admission` |
| `cc43d77218519094abb4911db46a0bf8de0cd891` | 2026-06-28 | `multicore: isolate object lifecycle writes by owner shard` |
| `fa1d0474243540f1a07403a35bf18111e3c4d81c` | 2026-06-27 | `multicore: shard object store owner fast paths` |
| `bd223ecee6bca06921416f22d77ad09c591ae5e9` | 2026-06-27 | `multicore: split owner runtime metrics` |
| `751ebd3f3401d1097561f69b0236018a91e6fbb0` | 2026-06-27 | `multicore: extract owner scheduler state` |
| `28de4e042133d0fc222ce7ebc6c465bd831dc16e` | 2026-06-27 | `multicore: extract owner future store` |
| `1217a29be12e20ab50e1ee9be36f423540ee5582` | 2026-06-27 | `multicore: extract owner trace store` |
| `404514849a944d959e528c63976bcef0064d2228` | 2026-06-27 | `multicore: extract owner task manifest module` |
| `ab706df977c87643ef2ef2575a084bd06affdfc5` | 2026-06-27 | `docs: promote multicore engine baseline` |
| `f0079d568abd207a09167577d213fc2d6a2ad0a7` | 2026-06-27 | `docs: refresh multicore production documentation` |
| `de550ad90afe74f0a2752804760b790d4e4e9170` | 2026-06-27 | `docs: record multicore production baseline` |
| `4e4811c13eb448a6f1e4ac3d1fcc030e8c22d57d` | 2026-06-27 | `Merge pull request #23 from FengYunCalm/codex/multicore-production-perfect-engine` |
| `4b747073a90372d9fa1abe77ebad728f3f845863` | 2026-06-26 | `docs: align multicore runtime owner task allowlist` |
| `e30e7a8fac439dbf216f1a1fd9937c81741fa816` | 2026-06-26 | `multicore: align owner executor tests with production task routing` |
| `8f9a03f00c0d94b5371a4bd84577e55d195b573f` | 2026-06-26 | `docs: refresh production perfect engine contract` |
| `27a37ab8a5802340f8239d05c570d9d08d76237a` | 2026-06-26 | `multicore: expose production perfect runtime contract` |
| `85b0393c6fa1c448d756d79c1ebab2d9432409be` | 2026-06-26 | `multicore: make owner messages executor safe` |
| `6ba65912967e5e4353cc7ca17692ae28eff22d8a` | 2026-06-26 | `Merge pull request #22 from FengYunCalm/codex/multicore-v2-engine` |
| `8ddf507535b834293c1af11653ad4661053f1edd` | 2026-06-26 | `multicore: document runtime v2 contract` |
| `9c1c9bbca347897d45b3a2600437d35b0eb872b2` | 2026-06-26 | `multicore: trace owner executor callback manifests` |
| `452d3eed48f3628fc9f27fffb83d35021253f612` | 2026-06-26 | `multicore: add owner runtime v2 manifest contract` |
| `05a79c395145ca2d4d7dc340c325033229303d66` | 2026-06-26 | `build: eliminate residual warning risks` |
| `c961f70a7eb7c9ad09894ad1706ea1910c693de8` | 2026-06-26 | `Merge pull request #21 from FengYunCalm/codex/multicore-refactor-execution` |
| `b244b3506ccfe2ae6c615f3b63a9957e8c49f5b2` | 2026-06-26 | `multicore: finalize production gate contract status` |
| `e7b5743dd7b5b38e00e308b37fe923b8984e8796` | 2026-06-25 | `multicore: complete socket release production gate` |
| `83f998289dde58d6207a7d278589410458db53ff` | 2026-06-25 | `multicore: record mudlib audit completion` |
| `936c7dcdcf853dd4b2339506f4be6ba675715a93` | 2026-06-25 | `multicore: accept 30m production pressure evidence` |
| `4db95d5b05c4f3382335e6ace123304415d6b515` | 2026-06-25 | `multicore: improve gateway loadtest diagnostics` |
| `303ebaa957754753b3e259f329aa329d0d8456ca` | 2026-06-25 | `multicore: isolate sprintf runtime state` |
| `899d6ba453b915ea41015331401e700f6e6df64a` | 2026-06-25 | `multicore: report gateway latency by command` |
| `31edf9b19af70b15664e5ee53e4f74b1419fa224` | 2026-06-25 | `multicore: refine gateway loadtest production metrics` |
| `d20f5844a4b0b75e5e72e28d86a277f3b2fd0fe6` | 2026-06-25 | `multicore: route gateway output through main queue` |
| `c48b84ff205828b31139c7ed7bf4fab7ed9c284f` | 2026-06-25 | `multicore: isolate owner thread runtime state` |
| `6db6a3854dea4452131952e90d63d659ba8381b7` | 2026-06-24 | `multicore: add production gate evidence contract` |
| `00a2ce83ec7a3f8f25dde320ef7c8d53e1d1cc9b` | 2026-06-24 | `multicore: add gateway production loadtest entry` |
| `3d3c919f5004042f2a9da75d26fb7ebda9f72267` | 2026-06-24 | `multicore: document mudlib production gate blockers` |
| `d58ca3e49d802f4f5f1e08804a73d26487abf072` | 2026-06-24 | `multicore: execute gateway commands on owner executor` |
| `aa4f08f3193c9e6db7ea390b616a7c367d9ab1e7` | 2026-06-24 | `multicore: dispatch socket callbacks through owner executor` |
| `f3c012e50c0c9612c761961a55e6fcb44f2466ff` | 2026-06-24 | `multicore: dispatch async dns callbacks through owner executor` |
| `c1400f8a644ad3460027ebfbc7062290dd4b21fb` | 2026-06-24 | `multicore: run callout on owner executor` |
| `cbfc1cc448682a529f198448f7d8322f9effde5d` | 2026-06-24 | `multicore: run heartbeat on owner executor` |
| `94025c93bb10f23f928abb01bc63bad560a385ec` | 2026-06-24 | `multicore: add owner executor callback task boundary` |
| `16a2c52b9af9ac09bf7035653b8d455148b99c1a` | 2026-06-24 | `multicore: complete owner local lifecycle retirement` |
| `1c35bb048ab63396169adc1ce6a45c985fb81363` | 2026-06-23 | `multicore: activate guarded gateway command executor` |
| `d55976882acbef903d8765c65bd73de8c08015f6` | 2026-06-23 | `multicore: satisfy interactive mode command frame gate` |
| `7b9cd4e292931bedb31e13541712a1f7a23683de` | 2026-06-23 | `multicore: satisfy command parser frame gate` |
| `b94570023854413033cd7f81c96037d55348093e` | 2026-06-23 | `multicore: satisfy input callback command frame gate` |
| `c7f0a12c6478f7e1d2e206c3826a58b59932330a` | 2026-06-23 | `multicore: contract owner local object lifecycle` |
| `0f1df34e4dfcbbe3569bb447c0dd2078e70d136a` | 2026-06-23 | `multicore: extract owner executor compilation unit` |
| `00117d6e5f29194026a89a8d9d996c545f1f723f` | 2026-06-23 | `multicore: add owner executor runtime facade` |
| `f01d8b34b87d21176250049bfb8c5b844c3181e8` | 2026-06-23 | `multicore: contract owner executor dependencies` |
| `ea46419c238c04ff6be8a39aad21f0e65a2000be` | 2026-06-23 | `multicore: extract owner executor module` |
| `8b0018d74b71d70137360e7052723aca5e9ca08c` | 2026-06-23 | `multicore: contract owner executor boundary` |
| `d5d7f0b73ee5d9ae670320d6680f648ac0461413` | 2026-06-23 | `multicore: contract domain task frozen payloads` |
| `9d5c10b151711f13e39075193bd42efbb47688d3` | 2026-06-23 | `multicore: cover std helper owner boundaries` |
| `7329a85e60445ee1f5dc6ac1832de8c28ef4a741` | 2026-06-23 | `multicore: frame gateway prompt apply` |
| `fe77ea32a5299234aa71dd2d470be93efa9b1356` | 2026-06-23 | `multicore: frame gateway input callback apply` |
| `5344a74149bbd88d592464f089066980fc565bf6` | 2026-06-23 | `multicore: frame gateway process input apply` |
| `bd532fdb5b684b764a79521eb0ff9703382adacd` | 2026-06-23 | `multicore: frame gateway localecho restore` |
| `ab850a76070de083c4d68e5db8cdde27966b840c` | 2026-06-23 | `multicore: frame gateway ed commands` |
| `128141262c6c01d35185777709036e9d9c5f0c5e` | 2026-06-23 | `multicore: frame gateway mxp tag filter` |
| `3572367fa096a4e2065f0c5ef8dc1adfbfde5dd9` | 2026-06-23 | `multicore: frame gateway input mode deltas` |
| `00c7745f9cb624d2158612e1e5d925102df736c0` | 2026-06-23 | `multicore: frame gateway parser context` |
| `2c2f3a0206906bf5be4182eab36ad004bba5a22d` | 2026-06-23 | `multicore: detach gateway input callback frames` |
| `dd5ccd149e230733cdfef34d863655e3a9946037` | 2026-06-23 | `multicore: queue gateway terminal mode deltas` |
| `5406a565965ae3dd65db8cd564d047ca192b74c8` | 2026-06-23 | `multicore: queue gateway localecho restore side effect` |
| `0229bad4af1800ca2985a6526297f2c979580b33` | 2026-06-23 | `multicore: route gateway command replies through main queue` |
| `2873576d80d513d3ade785590e1f5c9c1762517d` | 2026-06-22 | `multicore: document gateway activation blocker contract` |
| `e805aaa4153c921cce4880a318e3e5b871d52e63` | 2026-06-22 | `multicore: expose gateway activation blockers` |
| `1daa8fed983b18a3a2ecbc83f8a65b5e78b279de` | 2026-06-22 | `multicore: document gateway observability readiness` |
| `1e631fb1de142167e73cd87084442714e36917e6` | 2026-06-22 | `multicore: report gateway side effect observability` |
| `051826736b52cf760fce4c55f569cac1aad26044` | 2026-06-22 | `multicore: document gateway prompt io coverage` |
| `884a42f1665e85e4a8126744549bbea2cf2846e3` | 2026-06-22 | `multicore: trace gateway prompt io readiness` |
| `61d788e2b89c058fc5d19dce83b25d2388c51c0e` | 2026-06-22 | `multicore: document gateway mode flags coverage` |
| `53e0f71a5738a45f68b649e2af4e5747706ce234` | 2026-06-22 | `multicore: trace gateway interactive mode flags` |
| `26e298802d122940bc7eafc13ac97b23ce1b2949` | 2026-06-22 | `multicore: document gateway parser payload coverage` |
| `aab37ece324fc943c5e3255957299fe4a1efa1de` | 2026-06-22 | `multicore: trace gateway parser side effect state` |
| `d95656607156070d532224534b0ea60766b3c1cb` | 2026-06-22 | `multicore: document gateway input callback coverage` |
| `7967c2414474ede31efe3b125487bbd7a43ffc03` | 2026-06-22 | `multicore: cover gateway get_char callback snapshots` |
| `74601ab530defac2718dd0a8bfd2737ab4bbe98a` | 2026-06-22 | `multicore: cover active gateway input callback snapshots` |
| `5b1f08d8699adc415693901d0e70d54c08fa6c43` | 2026-06-22 | `multicore: trace gateway command input callback state` |
| `543f67e4cdf6552b424e64cdfd25d3f8ffa0cf4c` | 2026-06-22 | `multicore: describe gateway command side effect boundaries` |
| `175d0853d94df4ce7d0bff4b7fc71927a7aca456` | 2026-06-22 | `multicore: expose gateway command side effect gates` |
| `c8cb218545deae256c5d0b8845c68e490f5e2174` | 2026-06-22 | `multicore: close gateway command executor activation explicitly` |
| `9d3d0a84d4cb25fd0b55024fb2823275416f9c7e` | 2026-06-22 | `multicore: implement explicit ordinary lpc dispatch` |
| `53f021e19484a7ec4a338673e1e535ba72343db0` | 2026-06-22 | `multicore: expose ordinary lpc activation policy` |
| `7fc41219fcb80be8f3b9d4017e48e970dcc0497c` | 2026-06-22 | `multicore: mark object store owner-local readiness` |
| `2a0b732047afd24b6ad17dd52a77ec21127044f5` | 2026-06-22 | `multicore: mark object ref boundary readiness` |
| `12989cf0b018ff7e1ce0c117d66d04ad55d9d5a5` | 2026-06-22 | `multicore: mark apply return owner-local readiness` |
| `a9497dd1dc84d2350afd5a3e3ecd670566fdbe1a` | 2026-06-22 | `multicore: mark value stack owner-local readiness` |
| `4a86d643826c67e33f8e125349c0a073550c2ef4` | 2026-06-22 | `multicore: mark control stack owner-local readiness` |
| `ebeb722019b6cc0f4c1dc99ae42cdec5bfff6ba0` | 2026-06-22 | `multicore: advance owner executor readiness contracts` |
| `6e6f60e9d3668f2ea17c2ddca4dbeffa00f6c694` | 2026-06-17 | `Harden cross-owner LPC boundaries` |
| `b573055ca2ff0b75c0a65797b0f6a9567e57deea` | 2026-06-17 | `Harden owner snapshot API and documentation` |
| `2e5e510d774ca1e48daec2bf77f74b6c29772de4` | 2026-06-16 | `Add Owner/Actor Shard VM documentation and update README` |
| `c4718b3c6ce95195c2bf5fbed510c56714625317` | 2026-06-16 | `Fix owner_safe_query crash by removing C++ exception handling` |
| `18cfb2dcf643e6284e29d5d9ba2a5b38175170b8` | 2026-06-16 | `Add owner_safe_query() for cross-owner read-only method calls` |
| `7ca12d2293f2fe4f234d84caeb8ad6cfd0232756` | 2026-06-16 | `Add object snapshot API for safe cross-owner read-only access` |
| `918fb1ff8aa56300fd6b88886bc2c02341c2f19f` | 2026-06-16 | `Keep legacy helpers on default owner` |
| `97eccc49ef0fc3bbdca527881e9c52c0c9b9d260` | 2026-06-16 | `Keep loaded singletons on legacy owner` |
| `36cbbe60afaf61c5433a855c724cb2e686cbe919` | 2026-06-16 | `Constrain owner enforcement for legacy scope` |
| `bccf6b6bd8b77931ac4c25ae00ad1dc62931eef2` | 2026-06-16 | `Pass frozen payloads through owner async calls` |
| `79db504bd7616e12a0acb7a7df2a61aa83ea9690` | 2026-06-16 | `Execute owner async calls on drain` |
| `77de770f904fe4150bb42e608277faca19560342` | 2026-06-16 | `Use effective owner for access enforcement` |
| `16fd8df3e78c460835a7dde142a98f7a690e884c` | 2026-06-16 | `Update multicore validation status` |
| `4ecc2f0a03d13cdd529601b4d2a27016224675c5` | 2026-06-16 | `Guard detached VMContext setters` |
| `5e021011bf435262337f8e77dee67ade22e80fa2` | 2026-06-16 | `Route worker results through owner futures` |
| `6ddc329f8000f56a56277ed438402a10dfef913b` | 2026-06-16 | `Queue ED callbacks by owner` |
| `9bff4d6daaeeee362555e4843fdf770ec5438012` | 2026-06-16 | `Expose frozen owner future results` |
| `be140309e342aa1c121069a2e4db6d52d8b63db1` | 2026-06-16 | `Fail stale owner object messages` |
| `1d7bc757c21d0c880418e8cb1c4cf40b21516a05` | 2026-06-16 | `Track main owner queue claims` |
| `299a87ed262721105c5ea33fefca7852c5388567` | 2026-06-16 | `Document owner runtime migration status` |
| `4ab5f0b2829a207791f2675efa0ca439008374bc` | 2026-06-16 | `Reset VMContext thread execution state` |
| `04671e7b44f1d5de7255337495a15fe994bac3df` | 2026-06-16 | `Track owner executor claim lifecycle` |
| `c0b33a3b3613d93872f71d663e3784eaf6f64227` | 2026-06-16 | `Expose runnable owner shard status` |
| `3bd3736e04518bddc632ad3d0bd8ff1081678725` | 2026-06-16 | `Return target handles for owner async calls` |
| `4c005ab414a34692ed13603cb99a5c225400a701` | 2026-06-16 | `Validate full owner payload mappings` |
| `da5484db5b98c59e5b39a5be863c2d4def24ae31` | 2026-06-16 | `Trace stale owner tasks with original owner` |
| `d045164dfea4efe5985100b8443367bfc280d69e` | 2026-06-16 | `Allocate owner trace sequences atomically` |
| `da7433645dfbab2564c3fc8290714a2c7d725d97` | 2026-06-16 | `Sync owner message trace states` |
| `1b1f20628e92a7277c6f9bb47a05f32dfc275079` | 2026-06-16 | `Cover owner payload mapping traversal` |
| `085add9f72b61d4dfd05e0231e6b748f1621dbd8` | 2026-06-16 | `Fix owner payload mapping traversal` |
| `95bf76d90613c25f43c7351d57c3fd239bb0704d` | 2026-06-16 | `Cover enforced cross-owner object writes` |
| `f1c698ef8a41c4712c1c681b4aae4c163608b5a3` | 2026-06-16 | `Cover enforced cross-owner call_other` |
| `8231f8f9336ec6a4c5103323e8959f6926af544b` | 2026-06-16 | `Avoid scheduling active owners twice` |
| `253010e989d713d938c61c04b5f38ea3e511e5bb` | 2026-06-16 | `Fail owner futures on mailbox purge` |
| `ef83435e10755a72b496f79ae404fa26f021a097` | 2026-06-16 | `Fix pending owner message accounting` |
| `fcd23e455676f01a8fe4eb3de548c4bb6e353488` | 2026-06-16 | `Track pending owner messages in shards` |
| `c9eca6d40732dec0271861a6718f30af5b5bffde` | 2026-06-16 | `Cover multicore enforcement modes` |
| `6ca393f7ba4438b053c8c2c81c9408a3e892aaf3` | 2026-06-16 | `Track owner shard active queues` |
| `7ecc3a5f77bf498cf6bda8e7043873d2b8cb577d` | 2026-06-16 | `Keep destructed objects out of owner shard activity` |
| `cf8a0b301306abaf517d7d1db0d0b3dee88a3e8e` | 2026-06-16 | `Update owner shard state on object destruction` |
| `4126c26ff2ce0e74479292bbf69c76041b9302f1` | 2026-06-16 | `Merge pull request #20 from FengYunCalm/work/owner-object-lifecycle-state` |
| `d644d6f0388ca57e84763f7410b0ca529a19d669` | 2026-06-16 | `Sync object lifecycle guards through VM context` |
| `c49d88a63d73773ad345fed97f8ca9e4f925492b` | 2026-06-16 | `Merge pull request #19 from FengYunCalm/work/owner-error-depth-state` |
| `744dc80745e743c7eb9326bb778f02f00536e785` | 2026-06-16 | `Sync error handler depths through VM context` |
| `d535d13b308874206b61cfa21770b1b80f192963` | 2026-06-16 | `Merge pull request #18 from FengYunCalm/work/owner-stack-temp-state` |
| `2ead85049d62f33c45c12923e1763685ab998982` | 2026-06-16 | `Sync stack temporary depth through VM context` |
| `b72b988197f81fbedc313f7625cfbe208b739f28` | 2026-06-16 | `Merge pull request #17 from FengYunCalm/work/owner-error-flags-state` |
| `18f1c51971b754a5d7f8198b04c72e2c1d3adc27` | 2026-06-16 | `Sync error flags through VM context` |
| `055b32893c02b87885d8f8664352bba55717d776` | 2026-06-16 | `Merge pull request #16 from FengYunCalm/work/owner-inherit-offset-state` |
| `24e0af2e45046edb6b5857b49d987f2e16ff972e` | 2026-06-16 | `Sync inherit offsets through VM context` |
| `ac33faa13f7dd1910dcf75497c44027f6fad76a6` | 2026-06-15 | `Merge pull request #15 from FengYunCalm/work/owner-call-origin-state` |
| `f1d705b9d766fd3015beceb4e4b88ffe9a56e03b` | 2026-06-15 | `Sync call origin through VM context` |
| `e25c5d515d78c05a2d72a66a314ec0325e9ec04b` | 2026-06-15 | `Merge pull request #14 from FengYunCalm/work/owner-error-context-state` |
| `098b0fc29480248ab366cc9e60f6be1517f22f99` | 2026-06-15 | `Sync error context through VM context` |
| `aa7021af469f0a772210ec6ef58f4a0c0fcb87e6` | 2026-06-15 | `Merge pull request #13 from FengYunCalm/work/owner-current-object-state` |
| `a8f1ce7e3223798d7a8bbbd2e29db4ecfaae45f2` | 2026-06-15 | `Route execution frame state through VM context` |
| `32e5ed81a6e7368def72904241cdf632543c8b83` | 2026-06-15 | `Merge pull request #12 from FengYunCalm/work/owner-command-giver-state` |
| `1802650dde839cf6f1f197023951cbc9d9ec8189` | 2026-06-15 | `Sync command giver through VM context` |
| `9489c1c375c15bab6118e103e6bf149049f4f33f` | 2026-06-15 | `Merge pull request #11 from FengYunCalm/work/owner-local-player-state` |
| `2da8ca4c0398d46e05f59e9b0e0ba5ab283a5662` | 2026-06-15 | `Scope current interactive in VM context` |
| `4a378d5ebff721514ecae07b1c141be36a2294ef` | 2026-06-15 | `Merge pull request #10 from FengYunCalm/work/owner-queued-network-callbacks` |
| `d09ed3f6a429cc463071680090cd043259aaae37` | 2026-06-15 | `Queue network callbacks by owner` |
| `159d6c2c4857d3b9a109770f08d736ed2c111ef7` | 2026-06-15 | `Merge pull request #9 from FengYunCalm/work/owner-queued-async-callbacks` |
| `dc5a7806ec7777ad1bbaadf678454df1a7a4974d` | 2026-06-15 | `Queue async completions by owner` |
| `a4bc0a4ad8aad9fa917e6f66200493280e61e5e2` | 2026-06-15 | `Merge pull request #8 from FengYunCalm/work/owner-main-queue-dispatch` |
| `ef1c9970da64f7217489c4d4e81ac26315071e29` | 2026-06-15 | `Route main callbacks through owner queue` |
| `8f5638f527bf599030fbc0bb7d45b6446ebaf47d` | 2026-06-15 | `Merge pull request #7 from FengYunCalm/work/multicore-owner-runtime` |
| `e2f5473a58a1c1bbbc144c1e9d94de22cb1195b2` | 2026-06-15 | `Expand multicore owner runtime validation` |
| `668f95526d1e9450cea97e8805223535b131c777` | 2026-06-14 | `docs: polish public repository README` |
| `f6218fef971a3e64e612c4995e5cc302291ec73c` | 2026-06-14 | `docs: update repository clone URL` |
| `86661687c56a0f14be49ce79ac1d89a904182e9c` | 2026-06-14 | `Merge pull request #6 from FengYunCalm/work/local-master-progress-pr` |
| `143c83440f3d4b9b96830bf100f67ca1270de640` | 2026-06-14 | `Fix local master merge include cleanup` |
| `3e8afb30ef91cf34f633b6315111c48e0352c119` | 2026-06-14 | `Merge remote-tracking branch 'origin/master' into work/local-master-progress-pr` |
| `f1624c5d4fef689aa9546b476bdef48bf271ea0d` | 2026-06-14 | `Merge pull request #5 from FengYunCalm/work/full-game-owner-runtime` |
| `2a145355202f2b113d1481f3a498ca2624173f5f` | 2026-06-14 | `Register owner LPC domain tasks` |
| `0756fd35004d75d5a5b372eaba5ecaa3ce6ddf78` | 2026-06-13 | `Merge pull request #4 from FengYunCalm/work/full-owner-thread-runtime` |
| `0d7c6fdf67002afe12282fc9e3fc73163e8e4b5d` | 2026-06-13 | `Restrict owner LPC task allowlist` |
| `5af26878bda2ab78d2a7eae7713dba0243127ed3` | 2026-06-13 | `Add registered owner LPC tasks` |
| `b02092d9fb61fad9aaab8b9cbbba65eee1f8cb82` | 2026-06-13 | `Merge pull request #3 from FengYunCalm/work/final-owner-thread-lpc` |
| `64ae0ec6955049dbf5d3fcfb8183f8be6f53ff32` | 2026-06-13 | `Add restricted owner thread LPC canary` |
| `991dcbfd6dcf72a0daf5a5aa3090265affba21e6` | 2026-06-13 | `Merge pull request #2 from FengYunCalm/work/multicore-runtime-v2` |
| `37ccd48240988562c6d3ca40ec1b2efebc6d700c` | 2026-06-13 | `fix: harden worker hash and owner probes` |
| `ba21a555ecdf32f4bd4340c9efe22854c79971ec` | 2026-06-13 | `fix: bind combat worker hash to snapshot` |
| `45a355ced8bae9945cf2a6ff07056cc1cc6f187f` | 2026-06-12 | `feat: extend worker runtime for multicore tasks` |
| `88b7cb4c5369d0a8f657e0488f0be590d334862a` | 2026-06-12 | `Merge pull request #1 from FengYunCalm/work/vmcontext-foundation` |
| `2b332ed8079f6959418c4324aaaa41f2debba8fb` | 2026-06-12 | `fix: size opcode instruction table` |
| `8b61061e77a4073153d976d2389de9bf49788897` | 2026-06-11 | `feat: submit snapshot worker tasks asynchronously` |
| `5b19f8c1de471c0987fa20f1acef5cd3ff88f019` | 2026-06-11 | `fix: guard compiler parse node cursor` |
| `e1388f0b5241f815ba5e2458dde2041bc30e23e7` | 2026-06-11 | `feat: run controlled owner LPC probe off main` |
| `5031539eb8f8dbfdf486cb10b419df6c38a5d303` | 2026-06-10 | `feat: trace owner message commits` |
| `e45a30d816b3148538f5d952032c0e9d7da59225` | 2026-06-10 | `feat: guard owner thread experiment tasks` |
| `1e6151bf4b4a53bde12d066790e8b5c5df7d5009` | 2026-06-10 | `feat: trace owner object boundaries` |
| `4da6865a1c2abb194f6c91b5ba58a6f5bd4a18ce` | 2026-06-10 | `feat: trace all_inventory owner access` |
| `af69f7cfc3b8c9d9b2d9e401fdaa1dae6d8403ee` | 2026-06-10 | `feat: trace environment owner access` |
| `fd811ae4defbad1ed96ee3b03baa9de6d6c8ca9d` | 2026-06-10 | `feat: bind owner scope in VMContext` |
| `a6412cbd3974530f7eb7b655d71e6391c56da4cb` | 2026-06-10 | `feat: guard VMContext object store ownership` |
| `cd3d49a561766e1e2b853706cec8210209b9d4a5` | 2026-06-10 | `feat: bind VMContext per thread` |
| `dca4c448826003582781d48a26f0f58099caaccb` | 2026-06-10 | `feat: add owner thread experiment` |
| `f2ed2b7226075c4b82015d9848e525cff4aa85e5` | 2026-06-10 | `feat: add VM execution scope` |
| `a63086bdce362febc6e20517a20b1cb067d4ba9a` | 2026-06-10 | `feat: add owner access trace` |
| `8f8de084a4fb667ba85c506b9e1131212e28274b` | 2026-06-10 | `feat: trace owner timers` |
| `4aa978ca41dc015c5f89d6a005a00572fc470ad8` | 2026-06-10 | `feat: add owner task trace` |
| `516d0657f3de74f3100f577e6491843742b52484` | 2026-06-09 | `feat: add owner mailbox purge` |
| `aa826d838a85a074aa13e6d448744127ac3bd1f7` | 2026-06-09 | `feat: add owner epoch guards` |
| `ba16ab535d0fddd17a7731bdc0d3c78998e22154` | 2026-06-09 | `feat: add owner guard efun` |
| `1d153b2ca8169488f6feb278dc9a614b68d1c3e9` | 2026-06-09 | `feat: add owner scheduler dispatch` |
| `96fe1bd2193b04fa86b80ad7599f2106c5144cb4` | 2026-06-09 | `feat: add owner mailbox runtime` |
| `a9e22f014bdf699d403c1529a1564909ea937714` | 2026-06-09 | `feat: add VM owner metadata` |
| `6fe9861e13a81908b7faad3c65f52711c6d420c8` | 2026-06-09 | `feat: add actor score worker task` |
| `3f2d62e3ef5cba9a8b312f6df41f9e0e5391f077` | 2026-06-09 | `feat: add keyed snapshot worker task` |
| `93aa5cf313df501ef14f163d53ca928a44fb4f28` | 2026-06-09 | `feat: add owner-key VM worker scheduling` |
| `fd33c6e8b5481c1e28e93a51f2fc7ce05251200f` | 2026-06-09 | `feat: add async VM worker tasks` |
| `1739b6446d8dd40e005829fc7f493e695cd3a105` | 2026-06-09 | `feat: add VM worker runtime` |
| `bff55f70348f19a9283f3358984fe0ff766e46bf` | 2026-06-14 | `Integrate local master VM worker progress` |
| `8df5c233356d11148b2a1fecec69f4850b0e6572` | 2026-05-08 | `feat: add gateway session integration` |
| `9cc92a9e79e24506dbf77d109cc09704ed41930a` | 2026-04-21 | `fix: regenerate parser artifacts` |
| `dabc82ea6d096710a072408aea2a3e21be47d02a` | 2026-04-21 | `文档: 补齐双语 README 与发布规范` |
| `ce09fd4555d90fdeb5acb677bd061bcb8425028d` | 2026-04-04 | `docs: remove build-guide onboarding hint` |
| `f56fa04f7d54b5f48fc76d7109f3a12ea4522d0f` | 2026-04-04 | `docs: remove build-guide onboarding hint` |
| `0dece062d03adc3eb4bfbfbf95c4403d637b7cbf` | 2026-04-04 | `docs: simplify README to source-clone onboarding` |
| `b3ed86135c51777db27d48f6caa5ec70c11b8341` | 2026-04-04 | `docs: simplify README to source-clone onboarding` |
| `0314227152a8cf5b1c030c42c4be337e1e4a1c42` | 2026-04-03 | `docs: add README language switch` |
| `88cad07ceb9cb78d70f1d1b2d9bd3543ae930248` | 2026-03-27 | `开源: 完善开源规范与仓库说明` |
| `b1ef68beff2d34f1c49ef7a2549497fd72c4cbe7` | 2026-03-27 | `Merge branch 'xkx-warning-cleanup'` |
| `9b221ca76641c998c2e9ff3d58cb1a8ead1dee17` | 2026-03-27 | `开源: 补齐许可声明并稳定 Windows 安装` |
| `fc4ef65417ac5594f7c2e6a8a34d818940d224d1` | 2026-03-27 | `优化: 清理编译告警与构建配置` |
| `73d1b7bcc86602c011a7b8e8c7cad05eef66903a` | 2026-03-27 | `Add read_json/write_json efuns` |

## 附录 D. 本地快照中上游独有提交全集

`a4475b1afe998bb03a37bdb0596b64ab01042ed3..79548f86f66af19f61e03a0cca1aadc95f1e8973`，共 366 个提交。这里的“独有”只描述 DAG 包含关系，不等于补丁未吸收，也不等于安全修复。已确认残余补丁见正文；其余需逐 patch 核对。此表不是远端实时最新清单。

| Commit | 提交日期 | Git 标题 |
| --- | --- | --- |
| `79548f86f66af19f61e03a0cca1aadc95f1e8973` | 2026-10-07 | `master: probe author_file() with the master's object name (#1435)` |
| `328aaf10635ef8ce636b2b6cebd549bb165e462d` | 2026-10-05 | `mudlib_stats: no warning for a stat file that does not exist yet (#1436)` |
| `5e138c3c4088a71c8d0ff6f9e2159efc16ffd332` | 2026-10-05 | `fix: Coverity CID 1687676 in tracing.cc (#1433)` |
| `56964d9d96faaea641d6e2d4b944be88d786fa7a` | 2026-10-05 | `compiler: warn when a statically-float value is truncated into an int (#1413) (#1426)` |
| `4dd319f079ca2ca2285c140de49ec76f72c19f04` | 2026-10-05 | `tracing: keep debug_message() on the main thread; drop the rc and tracing unit tests (#1432)` |
| `1b921bb3f0ccc01f1c87d6e316c313716bf4e2d1` | 2026-10-05 | `ws: refuse a connection whose X-Real-IP header does not parse (#1431)` |
| `91de2eaf7d605b785eee37d9c866b522dbf07091` | 2026-10-04 | `fix: Coverity CID 1687673, 1687672, 1687671, 1687670, 1687669 (#1430)` |
| `01026e1fc7e9375d3c451c9d48855820712a2af1` | 2026-10-04 | `build(deps): bump the npm_and_yarn group across 1 directory with 6 updates (#1429)` |
| `2c93fd8657a1586402634b8f52a31a21a33e0999` | 2026-10-04 | `Fix #1407 (build without sockets) and #1408 (no global include file) (#1425)` |
| `b2ba3f8f0031b2718448acfdbaccfe550e38931c` | 2026-10-03 | `Consolidate 4 Devin PRs: error propagation, matrix/websocket dedup, security audit, unit test coverage (#1333)` |
| `d48a03d877e21f0f5b878f9a1f5a0c2ca6490271` | 2026-10-03 | `test: end-to-end checks for UTF-8 split reads (#1422) and nested async callbacks (#1421) (#1424)` |
| `a4fc80c5a2fb30a8e21bf3836986514da1287e96` | 2026-10-03 | `Fix deadlock in async code re: rapid mysql queries (#1421)` |
| `e92b5cf2389015a75db471e08be7c78c6e117b12` | 2026-10-03 | `compiler: normalize master-supplied include directories (#1389)` |
| `f3b8030e67c5cd9ff40826649158633bddc58e25` | 2026-10-03 | `Fix UTF8 reads in TCP text mode (#1422)` |
| `de11656d00516ecfb8a19afb0c249f9b7de8eafa` | 2026-10-03 | `explode: avoid rescanning Unicode on every token. (#1423)` |
| `3f457bb115d4fddb8c845c7229394261df3256d5` | 2026-10-03 | `compiler: fix two #1400 regressions in private inherited slots (#1420)` |
| `795eb371f1af522280738f454b8fc81afe231f1c` | 2026-09-29 | `vm: stable identity for named-function references, surviving recompile_object() (#1399)` |
| `a73b03efa262a9349912c6e65143f4ef164d9353` | 2026-09-29 | `chore: ignore AI tooling data and remove obsolete QWEN guide (#1419)` |
| `7c9f17d9d60271de739e33905e5fbf58bafe0a13` | 2026-09-29 | `ci(windows): drop the temporary exit-code diagnostics left in by #1412 (#1417)` |
| `5270e7d617705c46e35edb0d0609d11d88a7c602` | 2026-09-29 | `ci: install PCRE2 dev packages instead of PCRE1` |
| `37e7d8974c437c6bbecded7aae1b811f349e3d23` | 2026-09-29 | `build(deps): bump the npm_and_yarn group across 1 directory with 2 updates` |
| `03354159148c5723cf7e370b54b17fe1752721a5` | 2026-09-27 | `fix: Coverity CID 1687067, 1687066, 1687065, 1687064 in pcre.cc` |
| `5894e7e1a0b68786015a99d084ffdcc64def0bb6` | 2026-09-27 | `fix(windows): stop every binary aborting at exit on mingw-w64 r420+ (#1412)` |
| `7c65e1864cd9bc8297a834f6e2664973aa0260b1` | 2026-09-26 | `build(deps): bump the npm_and_yarn group across 1 directory with 2 updates (#1411)` |
| `932e93094ba5e5ebf0f1e3c441a9c1d341497cc3` | 2026-09-26 | `compiler: coerce int op= through mapping/array index (#1384) (#1392)` |
| `71061e6d7883a7015b45d6321ba5ecf538bb2096` | 2026-09-26 | `build(deps): bump the npm_and_yarn group across 1 directory with 4 updates (#1398)` |
| `65f6a8b8b61e770c834760d48c4f8ee04d7a190b` | 2026-09-26 | `fix: formatLPC's token-order gate false-positives on CRLF directives (#1410)` |
| `d140ec28c65072d9e5e95f172b8e4ee836591d77` | 2026-09-26 | `feat: upgrade PACKAGE_PCRE to PCRE2 (#1404)` |
| `2c27287500daf48a87a1f89d69a97d8b9603028d` | 2026-09-16 | `fix: keep #include and inherit in source order (#1405)` |
| `968205b37c4555e074c6ef06b912beb9faeddd02` | 2026-09-16 | `compiler: keep inherited private functions on their own slots (#1400) (#1403)` |
| `3b505bef19eb9d9f5c844a1c2d0d8016acb9c701` | 2026-09-16 | `fix: trim charset matches Unicode characters, not bytes (#1401) (#1402)` |
| `f1c242b1db54383d701838ee8aef512a0d923e81` | 2026-09-14 | `docs: LLM onboarding, ecosystem map, and site refresh` |
| `8ad78675c2f282b534b446e7c4f10cb86735fff3` | 2026-09-14 | `build(deps): bump nanoid (#1386)` |
| `6e56d4e9bf37d80aafa1b96163553241e67a0818` | 2026-09-13 | `fix: sys_reload_tls() reloads websocket TLS certs (#1395)` |
| `d41f000f0246606a3a781a69d80b47978e9e29a2` | 2026-09-11 | `fix: Coverity CID 1686645, 1686624, 1686623, 1686622 (#1394)` |
| `63e8a0cb358c6084919e725c660f2e40d27d22e7` | 2026-09-11 | `compiler: persist DECL_NOSAVE when a global shadows inherit (#1381) (#1393)` |
| `070724d11affed72fc96ac1cc4058f833b6cd262` | 2026-09-11 | `compiler: keep total_lines across compile_file() (#1385) (#1391)` |
| `735bd31fcd07aa3ec55cd6b0dca2f664544682eb` | 2026-09-11 | `docs: document promise_reject default reason and indirect self-resolve (#1390)` |
| `7c808c8b9587cb7686998d4e3073d56c73aa18c6` | 2026-09-09 | `Promise combinators, promise_cancel(), and await inside foreach (#1319 phase 1.5) (#1353)` |
| `b8dd58662500c60e9ed208bae980dd6f079239d3` | 2026-09-09 | `efun: external handle API, promise start, and cancel-kill (#1377)` |
| `7bcd22ebe8a496c3a3fe2a90296f1ee22a5f3f69` | 2026-09-09 | `vm: stack-resident index lvalues and specialized assigns (#1358) (#1379)` |
| `b1745c824a0e0e4cd869c5927d26f7c2362bcc00` | 2026-09-09 | `docs: add an LPC shadowing page (#1380)` |
| `ac9f61912c787ad204dab672ac042b9f78a58d2b` | 2026-09-07 | `preprocessor: scan '#' lines like C (splice first) (#1383)` |
| `faccd243bf2debacd65aa6875681a4679c8bdf02` | 2026-09-03 | `efun: add include_list() for compiled #include files (#1356) (#1375)` |
| `a781b9180d159c428fba3a63303a3c911964a238` | 2026-09-03 | `compiler: store the file_info line table as int words (#1374)` |
| `11f23e2051c39c85c955df21fe5c393bd9aceed2` | 2026-09-03 | `test: cover every arithmetic op= on call_other/evaluate (#1365) (#1373)` |
| `9bce345a0c1d6b288cc22d9d3043ef651d1658fa` | 2026-09-03 | `compiler: keep declared int on += from call_other/evaluate (#1372)` |
| `24211e799d8b2a462ac5702fe4be71db9980882b` | 2026-09-03 | `explode: stop rescanning ASCII on every token (#1366) (#1371)` |
| `1da7a0b677b9846e5c5e75bb15c29abe0a37bdbd` | 2026-09-02 | `build(deps): bump browserslist (#1370)` |
| `c80ce56f244e3dd6322188cd9a83dfa926bbbf69` | 2026-09-02 | `build(deps): bump fast-uri (#1369)` |
| `e0d6cca23925a718e74b40d56359ea9cf4e4fe1e` | 2026-09-02 | `compiler: name the package when an efun was not compiled in (#1368)` |
| `134bebd76f971344358d86ada7b5cb42edb916d4` | 2026-09-02 | `vm: put the foreach temporaries count back when an error unwinds (#1367)` |
| `f3ab999d134edaa8187feff3e8080160e5356199` | 2026-09-02 | `preprocessor: fix an apostrophe eating the file, and the reason it could (#1363)` |
| `9c673da730ce01b7b3211c9463e0314ffbd7b8fd` | 2026-09-01 | `replace_program(): skip pending-swap bookkeeping when the target is a true no-op (#1345)` |
| `7af5c3fffe2505c7cb764951bed863b16eee471b` | 2026-08-31 | `tracing: stop a trace that fills its buffer from wedging tracing for good (#1361)` |
| `e0ce7d2604447c9f96d1c8d9a2800ce8258383ec` | 2026-08-30 | `testsuite: make promise_drain_yield check its own premise (#1360)` |
| `de945701234d348e3dd3e7aee59bf9e06e58539b` | 2026-08-28 | `Fix two Coverity findings in the async/await code (#1354)` |
| `a540f77d03ceca03d87f7cfb611e5d51cbbdf637` | 2026-08-28 | `Fix a segfault on the first error reported from a 65536-line file (#1355)` |
| `858d5da9d068e69eb4fa287730937e6b0564690b` | 2026-08-27 | `Native async/await, promise&lt;T&gt;, and promise-form efuns (#1319 phase 1) (#1347)` |
| `1e74a7585c6a3703a545c1655c61071efc7f7e9c` | 2026-08-20 | `Fix the intermittent DB testsuite failure, and a Coverity copy-instead-of-move (#1351)` |
| `dd2a3a14a4d1890db02a7ec56aa103e3b1d14b95` | 2026-08-20 | `ci: supersede a branch's older runs instead of racing them` |
| `ed328800a9afc779fdcfc949eb0919cf0969fb55` | 2026-08-20 | ``compiler: drop `maximum local variables`, and fix three over-reaches`` |
| `17d9d4f1aca1a77d7716f9139d6ad5ef38279ac8` | 2026-08-20 | `docs: stop referring to a construct this branch does not have` |
| `52de0005302d84a9c5e85b9a3cadfe25cc611d84` | 2026-08-20 | `ci: check pull requests whatever branch they target` |
| `4853debc724b8faad8cba2f17c5d25b9ea772966` | 2026-08-20 | `testsuite: format tree_block_scope.lpc` |
| `1a0b118df9e215a555293adc2ff241e42a1ca213` | 2026-08-20 | `compiler: finish three fixes in this series that had an unfixed sibling` |
| `9e11248f5177e62f987bd8314144cb83b626da70` | 2026-08-20 | `compiler: reject a local declaration inside a default-argument expression` |
| `3d24d7aefc5595570f072f9929d3f62f4e1d53fb` | 2026-08-20 | ``compiler: make `maximum local variables` work, and cap it at what bytecode can encode`` |
| `331b456eaaea6813008a7140e8f5118d8d7406d2` | 2026-08-20 | `compiler: bound diagnostic output (34KB of source no longer yields 35MB)` |
| `bf8861c9c1e3cc56ae8e2cb1c32b52843dfc5d5e` | 2026-08-20 | `compiler: fix three crash/UB defects found by fuzzing the front end` |
| `153ebe098d1c9f0e0776bc241fabf5f3a62ec897` | 2026-08-20 | `compiler: separate name scoping from slot numbering, and fix two crashes around it` |
| `58bcc963adf95806a7c4cc323b9ce69d19a2848a` | 2026-08-20 | `compiler: end the local scope of a block used in expression position` |
| `97ef8d3dde50bc4e89a1f839c6c1f530eed6781d` | 2026-08-20 | `compiler: stop block expressions leaking their inner declaration's type, and give __INIT its locals` |
| `990f7b1255f01264d4f3d077d35462966a3fb46f` | 2026-08-20 | `vm: fix three inverted local-index guards, and add the missing fourth` |
| `6cf257cedbb38e4b122ad79df08742c6629860aa` | 2026-08-12 | `build(deps): bump the npm_and_yarn group across 1 directory with 2 updates (#1346)` |
| `1099b48236b0464204ef3c76b42734a94ba5776b` | 2026-08-12 | `Speed up diagnostic rendering, and let the caller own the compile arena (#1343)` |
| `1e4d4145fd6ec377f510c3c829a0901d869fc6c3` | 2026-08-12 | `Make sizeof()/indexing O(1) for pure-ASCII strings (#1344)` |
| `3e3418179b78b9b36e69e90d0fccb36a206bc30b` | 2026-08-09 | `Remove per-svalue heap allocation from the VM's hot paths (#1342)` |
| `2024182d6894710dcce0b537b92d02e1f69bf8fd` | 2026-08-04 | `build(deps): bump the npm_and_yarn group across 1 directory with 3 updates (#1340)` |
| `16c57df887592f830c6abf96a502d8fc2eea7f80` | 2026-08-02 | `websocket: drain output before driver-initiated close (supersedes #1338) (#1339)` |
| `19ffcc7aea39934802e1a64d19d4d913ca538a03` | 2026-07-29 | `wasm/index.html: settle NOWRAP_COLS at 200` |
| `241e45708f5ce6cfb0d0f5301d22c4ea5e504a47` | 2026-07-29 | `wasm/index.html: make NOWRAP_COLS a real no-wrap ceiling (2000, not 160)` |
| `bc30e2f08bbc3b79835b074cbce7e30bc46d2c6f` | 2026-07-29 | `wasm/index.html: bump NOWRAP_COLS 80 -&gt; 160 for wide ASCII banners` |
| `6899f66b9e5f20c96f02d925a28f3bd228ba5464` | 2026-07-29 | `Replace mobile command-input textarea with contenteditable div` |
| `74ca5d21df76e3e54ede44961f90b4dbc8b1f929` | 2026-07-29 | `Fix mobile input bar rendering ~3 lines tall with the prompt/text misaligned` |
| `4b623a700bdfb5595dbacc2080b323cfdd6a652d` | 2026-07-29 | `feat(wasm-www): replace the native command input with a custom widget + history` |
| `eaa1a6ee13e10e5772513c328cfcaf780abe7a11` | 2026-07-29 | `fix(wasm-www): mask password prompts with CSS, never input.type=password` |
| `5cc89de8acd2de84115e25868537e4fbcd1a89cc` | 2026-07-29 | `feat(wasm-www): mobile keyboard fixes + a soft-wrap toggle for the terminal` |
| `e059c891cbd234383a1ae17f860f9108f3651159` | 2026-07-28 | `docs: document the real rules for throw(), add a structured catch() example (#1335)` |
| `88224998226d06b6ec225a75a68901c9f80be29f` | 2026-07-27 | `compiler: coerce return &lt;expr&gt;; to the function's declared return type (#1334)` |
| `5f7fba054f7ebbcc58fd7acd9b471d83f177f441` | 2026-07-27 | `docs: document the third argument (flag) of function_exists() (#1326)` |
| `7bbedece7c00ac37d9fed907dbe403ea65ff5188` | 2026-07-27 | `docs: document that shuffle() reorders its argument array in place (#1332)` |
| `b0d3d2977b680a6ebcb5edaedd95441d57a03d34` | 2026-07-27 | `tests: stress remove_interactive() net_dead teardown shapes (#1327) (#1330)` |
| `032c00b7984a51004e3406b6be3e26186d959b08` | 2026-07-27 | `testsuite: format two fixtures that landed unformatted in b1fb96f` |
| `948b49ed7f0ed28579b2fa09e32368812f3dad5e` | 2026-07-27 | `Fix object-refcount over-decrement bugs behind #1327's corruption crashes` |
| `4d4ad6d9ce31c2d1e0e2fb05ceb788fff34b8ed8` | 2026-07-27 | `docs: document the third argument (scroll_lines) of ed_start() (#1328)` |
| `6436410c8329db2ae58ee1f4707362e701281c02` | 2026-07-26 | `build(deps): bump the npm_and_yarn group across 1 directory with 4 updates (#1321)` |
| `719f3e357bee27c5e501ac745e553a310a740293` | 2026-07-26 | `build(deps): bump svgo (#1315)` |
| `b1fb96f38121c8ccad590e1d8539a6ab50ba11c2` | 2026-07-24 | `Add AFL++ fuzzing harnesses; fix 5 pre-existing memory-safety/DoS bugs found by them, plus a Coverity-flagged ownership-transfer hardening (#1317)` |
| `5db89572a25537bb809fdb14ce00a1375265aebf` | 2026-07-24 | `Feat/wasm mobile ux (#1318)` |
| `691a74bd859f5419ffa11bde12075f6a734e0bd5` | 2026-07-24 | `feat(wasm-www): multi-connection Game tabs + a driver Logs tab in the web terminal` |
| `0096a6e70c98268fe5d6872599010633c2f90b14` | 2026-07-23 | `feat(wasm): enable the pcre package on the WebAssembly target` |
| `3bfb7483fe9cb8c3597e5f895587ea7b8a6ca725` | 2026-07-23 | `fix(lpc-syntax): refuse files with broken quoting instead of shredding them` |
| `d1a491c1d3b727bd032f99f38d3178ceb1343fce` | 2026-07-23 | `fix(lpc-syntax): tokenizer mis-lexes unescaped quote-char literal '''` |
| `007bb8637098c3efb97c989468627299358432da` | 2026-07-23 | `feat: synthetic resolve() on WASM instead of raising an LPC error` |
| `e33bb5da6ebd4257881792c9c12a6cfaa4c2ddb5` | 2026-07-23 | `fix: query_ip_number() returned uninitialized garbage under WASM` |
| `1654bf629756940aff4eaeedea8421eb35f945a3` | 2026-07-23 | ``fix(lpc-syntax): tokenizer mis-lexes bare `(::name(` parent-call guard`` |
| `3ec802f6a777f494b6eae40d735a1733521c8afd` | 2026-07-23 | `Fix null backbone_domain crash; add lpcc --batch` |
| `f3e5bfa79968610eda1fa9ba8c2a248f76fe9350` | 2026-07-22 | `compiler: eliminate C-stack recursion from macro expansion and lexer; raise nesting cap to 65535` |
| `25c87bbc294ff5acab9ac800cb9c3763e057916f` | 2026-07-21 | `Lower MAX_EXPANSION_NESTING 64 -&gt; 32 for ASan stack margin` |
| `4d5345f575c361b2391051a1652474f52234ae44` | 2026-07-21 | `fix: 3 more sibling bugs from self-review (preprocessor recursion x2, db.cc lock symmetry)` |
| `2d317e45317f189fa0cc70f40ec6a64e74986345` | 2026-07-21 | `fix: apply.cc's inline default-args fill had the same push_svalue() stack-corruption bug` |
| `a55e1572f2da841632531396c2a8777a4c9fb145` | 2026-07-21 | `docs+tests: replace standalone audit MD with regression tests and AGENTS.md/README lessons` |
| `d917178831fe5c1aa396b26dbebc9b178cc49aa1` | 2026-07-21 | `fix: resolve all 10 confirmed driver-stability bugs from the 2026-07-20 audit` |
| `39e24504d0238c1553f17eddb9d1f3df04d10059` | 2026-07-21 | `audit: self-review consolidation -- fix formatting, a broken repro, and a factual error` |
| `20ec1ee8674a0eeed8630a60d04cd1dfaa1bfbdb` | 2026-07-21 | `audit: 8th confirmed crash (restore_object uninitialized-size abort) -- audit complete` |
| `866aab8a2ff5797267b77ae82fe50af70dfdf536` | 2026-07-21 | `audit: document mudlib_error_handler() ref-count leak; refloop/hot-reload clean` |
| `0196e71d682ee5aeea340c7a299bb4440dd55e9a` | 2026-07-21 | `audit: document socket_acquire() O_EFUN_SOCKET gap (static finding)` |
| `435a40939df9835b1b41cb418c435d394f24b2de` | 2026-07-21 | `audit: checkpoint in-progress scratch reproducers (save/restore, refloop/hotreload, sockets/parser, math/matrix)` |
| `7d6348809f4d9fef98420ad6c71ea55e131f9186` | 2026-07-21 | `audit: document 2 more confirmed bugs (default-args stack corruption, macro nesting overflow)` |
| `b97b9e28082593a10c61c1f59ff082a330d2a352` | 2026-07-21 | `audit: document 5 confirmed crashing bugs (compiler front-end + FFI)` |
| `8b0aee8aa26c26f433532274308a3b905a1eac76` | 2026-07-21 | `Harden five latent memory-safety / correctness gaps found in a driver audit` |
| `bf73c66ec8757ec4f534d4551b3de503a2dfbf92` | 2026-07-21 | `fix: uninitialized float variables must stay undefined until first assignment (#1305)` |
| `d0549220d8981feb68e4251d6084f42761650f69` | 2026-07-20 | `Zero-init declared float variables as T_REAL; pin int op= float type contract (#1303)` |
| `dca0eae0a5c836f8f6fdecfe5104f4583366af60` | 2026-07-20 | `Fix stale T_UNDEFINED subtype in f_xor/f_lsh/f_rsh (#1302)` |
| `1797eaafcd50cf44471b2fcb64fe03180742e620` | 2026-07-20 | `websocket: sync vendored ws-over-h2 mux servicing with upstream 774c64a (#1301)` |
| `aec12ca79cb111e97ba2e77314661746cd298f17` | 2026-07-20 | `Default arguments: fix helper-name collisions and fill on all direct-call paths (#1298)` |
| `ec9b6a4abc6011331129871ba7a0d425e3e6b2c9` | 2026-07-20 | `fix: stack corruption and use-after-free in string/object concatenation` |
| `98f09f3d837e1d606f265113a4fb11777eab10c6` | 2026-07-20 | `fix: null-pointer crash in present() when id() destructs the searcher` |
| `0da9f26b338d42b4f60002f98d5832a7da6207e9` | 2026-07-20 | `websocket: fix pmd-over-h2 short return and ext tx-drain wedge (Firefox disconnects) (#1299)` |
| `0f91897cd17b9d79af426fdebac466ab0a4822a8` | 2026-07-19 | `fix: address Coverity Scan defects in disassembler and lpcc (#1294)` |
| `06d23cfbcc32f54dff0acad2ddf6bdfbfb1d7ef3` | 2026-07-18 | `compiler: fix line-attribution bug for lineless return statements (#1293)` |
| `c054bc020c0f7e2430799fc83d539a14ba3d38c7` | 2026-07-18 | `lpcc: emit --ast output for objects already loaded during boot (#1290)` |
| `46fdc2b523cd701c3a8f2ea4713b3ae81d2afe7b` | 2026-07-18 | `websocket: fix ws-over-h2 (RFC 8441) instead of disabling h2 (#1289)` |
| `b67f59eebe65e3c7f1262380d21c00abf85a19ef` | 2026-07-18 | `Fix object-destruct UAF and compiler_tests string-table corruption (#1288)` |
| `99c0c1778e27ef33184150579b39980429e547bc` | 2026-07-18 | `compiler: strip the actual source extension in the_file_name() (#1287)` |
| `389d9f769c1a4fbebf5066a9e6176333831aea66` | 2026-07-17 | `lpcc: JSON-native stage outputs, wasm lpcc, disassembler fixes (#1283)` |
| `e2e21f4e501577b3c09681d53e7baddc15271a74` | 2026-07-17 | `lpc-syntax: move the VS Code extension out to fluffos/fluffos-vscode (#1282)` |
| `ca0119d39af3a1410c443f44c0d110d39b055670` | 2026-07-17 | `docs: LPC style guide + formatter reference; for (;;) spacing fix (#1281)` |
| `62aeb7bd9689c72f72fadfa340f7f713627fe267` | 2026-07-17 | `make_options_defs: terminate "Generate Success!" with a newline (#1280)` |
| `c6a146cfca207ac887a35a62fc88a329e9c2072f` | 2026-07-17 | `lpcshell: predefine __LPCSHELL__ so a mudlib can tell it apart from a driver` |
| `17280e6a5cc8de7d428bb17a625e5c2a600d9701` | 2026-07-17 | `lpcshell: report runtime errors instead of a bogus syntax error` |
| `1cabb728e1a455fa6be1603c07c06775821a00f8` | 2026-07-17 | `docs: document dual (expr)/{ block } syntax for catch and time_expression (#1278)` |
| `5568b6f5e5063bdf82532df69b7a316ebac62079` | 2026-07-17 | `Add missing trailing newline to trace_start() error messages (#1277)` |
| `fb2202546cca686b0ce9df7e6ba861accb8c27be` | 2026-07-17 | `lpc-syntax: source-following LPC formatter with printWidth wrapping and full testsuite reformat (#1270)` |
| `86d13cdb830582247fcba434586c3f33cf0f341b` | 2026-07-16 | `reference loops: docs, runtime cycle efuns, orphan collector, copy() unwind fix (#1276)` |
| `b040aae3c97b83fa376399457eb623f478f8bf97` | 2026-07-16 | `core: don't fire lazy resets from the clean_up sweep (#1274)` |
| `4a9b2ed2d04d923e82ec675195143733b15a7166` | 2026-07-15 | `docs: rewrite FFI reference around buffer features, add worked libc examples (#1275)` |
| `0098c3ec5026232b38043fe399295ecf2ecc1b66` | 2026-07-15 | `trace: stop recursive dump_trace() re-entry from exploding error output (#1273)` |
| `1426d890bdfd48e9046d82ddc65014a15cf48a4b` | 2026-07-14 | `fix: stop stat()'s flag argument from being clobbered by valid_read (#1272)` |
| `8a0bbc11bf2266e39c9a5840fcca66756d0dfc29` | 2026-07-14 | `compiler: bound codegen recursion by expression depth only, not object size` |
| `a89bb37ebc42fe595f19b0993ed951d3c0c28dae` | 2026-07-14 | `Revert "fix: cap optimize()/i_generate_node() recursion depth"` |
| `5c5cdac623e1efad2a1124eb0006af2db1c310d2` | 2026-07-14 | `fix: guard pcre_replace_* regression tests with __PACKAGE_PCRE__` |
| `1d147bcca427b189b7a5c9e4e1a3a469447f85f8` | 2026-07-14 | `fix: clear the shared compile log before checking it in inherit_pct_filename_warn.lpc` |
| `a3a009d9d85d7b0dc71286fa27c92a9028917893` | 2026-07-14 | `fix: avoid a shared function name in inherit_pct_filename_warn.lpc` |
| `251dfad9a230aedf4c1bad12ac001b81d6bab4e2` | 2026-07-14 | `fix: guard dwlib_replace_objects.lpc test with __PACKAGE_DWLIB__` |
| `d23d24172ff5f14571ec621e69c3818c053c8b90` | 2026-07-14 | `change: mask shift counts mod 64 instead of erroring` |
| `fd7ae2704df6932c7deb9966b9211e31dbf532a3` | 2026-07-14 | `docs: capture round-2 review discoveries in AGENTS.md/README` |
| `3085be0089de8e44fa87496cd2f4b8eeed7b0599` | 2026-07-14 | `fix: MySQL_connect() leaks internal buffers on failed connect` |
| `7a471202077c0fc05a122390faf0a38969e5a243` | 2026-07-14 | `fix: ops.cc compound-assign efuns don't clear the lvalue's subtype` |
| `cdb1e9e92373897867e885cf01c30838f8700c83` | 2026-07-14 | `fix: f_ffi_struct_layout() leaks offs array on unknown type code` |
| `04fd64b466626c0200af6949bd51fed6d8f8efa9` | 2026-07-14 | `fix: socket_write() leaks uninitialized heap memory for T_ARRAY sends` |
| `9260e704e516755bc5b5d2cf6a991462df1eb6ae` | 2026-07-14 | `fix: mudlib_stats.cc save_stat_list() unbounded sprintf` |
| `06ff4ed073b00f3cdd1faabec17e75848a680168` | 2026-07-14 | `fix: math.cc sentinel-value collision and printf format/arg mismatch` |
| `02a1424a92bcfa7797be675d974aa21f5864159a` | 2026-07-14 | `fix: parser.cc unbounded recursion, error-path leak, and backtracking DoS` |
| `cce39ab88f0fb4c2a1c0f391c2a9bd9d30b91302` | 2026-07-14 | `fix: jsbridge.cc mark_callback_refs() corrupts non-shared strings` |
| `0172da2a484ba3a30c410f82bf32b584db8bab14` | 2026-07-14 | `fix: move_object() missing destruct recheck after lazy try_reset()` |
| `88e1fe510b54667361300b3e60b99edcf051b0c7` | 2026-07-14 | `fix: terminal_colour() stack exhaustion from per-segment alloca()` |
| `8b7488d6680eb7cb7de15695875610bdacd23f44` | 2026-07-14 | `fix: compress.cc stack overflow and unbounded decompression growth` |
| `fa54f063af70b301f13044f1d4fae57fe83cb05a` | 2026-07-14 | `fix: guard &lt;&lt;/&gt;&gt; shift operators against out-of-range shift counts` |
| `04820a5c28fd7baa631fbd9972e586cbd0dce277` | 2026-07-14 | `fix: bound PORT_TYPE_ASCII reads by remaining room in ip-&gt;text, not buf` |
| `cc6b97cbec76144fee65e6d173ee4871b9094144` | 2026-07-14 | `fix: compute pcre_match()'s is_string from the actual subject slot` |
| `0115186f9defadb00ade08ac6ecc5730a32bff90` | 2026-07-14 | `fix: bound the C-stack cost of freeing deeply nested arrays/mappings/classes` |
| `117cbc18753e534c9c0b060ba158b3b6cf8d66a0` | 2026-07-14 | `fix: cap optimize()/i_generate_node() recursion depth` |
| `c4dbb17c9678458bf4df8377e1d32e86059828cb` | 2026-07-14 | `fix: reject string callbacks and heap-copy bound args in async efuns` |
| `beb68b1142d3aeb5f0d3e160cc823e6dfa618b95` | 2026-07-14 | `fix: string-char lvalue reentrancy and missing/broken inc-dec write-back` |
| `a82bdd302085fa9139f19c173da5dec26aa9b884` | 2026-07-14 | `fix: handle self-destruct during reload_object(), guard free_object() null` |
| `2bdfd9383880f2161ccaa12be50af4d396b59224` | 2026-07-14 | `fix: validate matrix transform array elements are T_REAL` |
| `1994acca1964fb4c6d8db9cf89503bb19f36f8f2` | 2026-07-14 | `fix: validate pcre_replace() argument/element types, fix leaks on error paths` |
| `6e359e590d16313798b0076cd7a3bbef391e2671` | 2026-07-14 | `fix: pass return-type-mismatch warning as a yywarn() argument, not format` |
| `86592aa76bee80eb440208d812b129ee25da3b22` | 2026-07-14 | `fix: guard #if expression evaluator against INT_MIN / -1 SIGFPE` |
| `28b6e2920faddfc1fdb3f7d2f44839686373b460` | 2026-07-14 | `fix: check set_bit()'s bit index for sign before 32-bit truncation` |
| `ce84c0b7b6f6e552d3dd251c007f84d13710e596` | 2026-07-14 | `fix: bound replace_objects() buffer copies against master-apply strings` |
| `d13d03a01eb4c07dfcd464a1dc751f8fc85e000f` | 2026-07-14 | `fix: widen load_object() real_name/obname buffers to prevent stack overflow` |
| `3bdbe9676ed10628304ae3a7bd43e4f8f64b0920` | 2026-07-14 | `www: real ws backpressure fix, teardown leak fix, src/www docs, websocket smoke test in CI` |
| `f45461cb97b8b7883e1a2e7527c460a20e8ecca9` | 2026-07-14 | `web terminals on xterm.js; TUI library fixes; emsdk pin; lws output-wedge fix` |
| `cc2c2c2b56741eba437ebe98c633f6742c9b4d56` | 2026-07-13 | `docs: capture third-party vendoring and autogen-pin lessons in AGENTS.md / README (#1264)` |
| `897821731f85cae4bfb5a15dfa17235a2c07b19a` | 2026-07-13 | `build: stop autogen churn -- pin generator versions, gate copy-back on input hash (#1263)` |
| `5ac4e8c0e62b88e780bd053fbb6084546442edbb` | 2026-07-13 | `thirdparty: fix libwebsockets configure probes under clang 18+` |
| `c9820c6b088f2ea6a43dfd409e777912213637f6` | 2026-07-13 | `thirdparty: fix backward-cpp v1.6 on musl and Windows` |
| `c74ccc8523f3f04d6247d561da6643852e891539` | 2026-07-13 | `wasm: add crash/error debug modal to the default page (#1261)` |
| `1575e60234cf5581ef4d3189492a5cd3754b9468` | 2026-07-13 | `thirdparty: prune widecharwidth test scaffolding` |
| `f35d4a71f3f1a9ceab59a13af36bbc7df1552a8d` | 2026-07-13 | `thirdparty: prune unused libevent test/sample trees` |
| `2d303af0b073f6eb2bebbb5d28cf2478c1f56452` | 2026-07-13 | `thirdparty: prune unused libtelnet doc/test/util trees` |
| `8fe05a5d40b8c454ece7a6dd8852aafa33c5273f` | 2026-07-13 | `thirdparty: update libwebsockets to 4.5.8` |
| `9cc0555ff391d39ad7010e519c6a69afaf632533` | 2026-07-13 | `thirdparty: update utfcpp to 4.1.1` |
| `3e2eb0c888bd8d810dcb07766fa68191ffe6c916` | 2026-07-13 | `thirdparty: update fmt to 12.2.0` |
| `b3ddc42bd59ab17d426958060a9d2de78e2d2cb2` | 2026-07-13 | `thirdparty: update json to 3.12.0` |
| `94f293413de027d6efbd9e85b185bc286d549c92` | 2026-07-13 | `thirdparty: update filesystem to v1.5.14` |
| `680fa6039624e815c15e29eeea7d1d5bd2256d01` | 2026-07-13 | `thirdparty: update backward-cpp to v1.6` |
| `ae936d21ab89e8f2cfd9da2e90e0639a383d30b8` | 2026-07-13 | `thirdparty: update scope_guard to 0.9.4` |
| `82b38f30c50723d64f51c95e3dbb48d358ad4aec` | 2026-07-13 | `thirdparty: update argparse to v3.2` |
| `6b6f1699525c8c6b3b7c8d50c02003d85f33f217` | 2026-07-12 | `lpc-syntax: wire formatter into vscode extension, fix tokenizer/formatter bugs (#1259)` |
| `d6a2eed00880bd0718876a2cb8fde85884bba035` | 2026-07-12 | `Fix three Coverity defects: lexer buffer overrun, dangling filename pointer, infinite loop` |
| `e2a0ddf0d84aae3c84247a2c05ea67c04d3f5b39` | 2026-07-12 | `ci: share platform builds between ci and release via composite actions (#1257)` |
| `ca02555de8dd01e82581dfa52a9d3163f7aa6dba` | 2026-07-12 | `object: bound restore size pre-pass on true recursion depth, not the sizes[] index (#1256)` |
| `8d93bfa81e0f55bda4c277cca1ff4d7c3aeed284` | 2026-07-12 | `ci: release from a release/vX trigger branch (#1255)` |
| `4558ecedbbe47f96d83540f4047d508c6ac76193` | 2026-07-12 | `ci: cut a release from a pushed v* tag too (#1254)` |
| `a259dd6689b2caab75c4802209e038a74d227b60` | 2026-07-12 | `ci: monthly auto-release when master has changes since the last tag (#1253)` |
| `c5f3c3bdc7372c2bd27ba29cf225dedd2dfb2a3f` | 2026-07-12 | `docs: document call_out handle validity and mid-compile valid_read behavior (#1252)` |
| `bc20bfe3ae842b1acd4ec79cf0e8fa1d884c3080` | 2026-07-12 | `json: buffer-based single-pass parser/encoder and comprehensive tests (#1163)` |
| `814c4fa5b9b0c182f0ee5b59171d12ff42d017d8` | 2026-07-12 | `vm: string foreach/ref fixes, buffers as byte arrays (foreach, strict bytes, to_buffer), thorough ref tests; #1196 docs follow-up (#1250)` |
| `1ad5953549413f695d16779276abd53716d414be` | 2026-07-12 | `docs: validate "See Also" references, drop dead ones, document valid_ffi (#1251)` |
| `1ba6584da56af661f7f96cfa40f16b0553baffdc` | 2026-07-11 | `feat: allow & as syntactic sugar for ref (#1196)` |
| `6e6d1c772b6e11003f5cb45d92593e7f4a2fab89` | 2026-07-11 | `docs: flesh out 31 TBW efun reference pages (#1248)` |
| `021ba349fc0a52636e28d26542ffb4c120d3a34a` | 2026-07-11 | `docs: capture memory-safety audit lessons in AGENTS.md / README (#1249)` |
| `670a73093d433be195d4fb8f8771731b67c1ae39` | 2026-07-11 | `audit round 4: memory-safety fixes across uncovered subsystems` |
| `4bbd61912233675886baf61f86da0c8c8ef30cf3` | 2026-07-11 | `async: fix flaky memory-checker ref race and two more bugs` |
| `f304fae8c68a6099fe682cebb3f3e72caaeb010b` | 2026-07-11 | `packages/vm/net: bounds, overflow and lifetime hardening` |
| `e6d3578dc7b7b2796b04e0a1704503271484bb68` | 2026-07-11 | `vm/compiler: don't pass source-derived text as a printf format` |
| `6b50c016a193a86f1b6f01b21327e404e29d03e6` | 2026-07-11 | `vm/compiler: guard INT_MIN / -1 and INT_MIN % -1 (SIGFPE / UB)` |
| `07d0782e150fbbcd6d98e3d5920e122569139a1d` | 2026-07-11 | `external: tear down efun socket when posix_spawn() fails` |
| `d4f735d946fe4570f3560d48c340e39491c9051c` | 2026-07-11 | `comm: fix VM-stack corruption from input_to() to a '#' apply` |
| `ac1981f67ead41595423422033c4a7e3c87ba82c` | 2026-07-11 | `object: harden restore against deeply-nested and error-path corruption` |
| `6393ddad4bad69078325e7f2e82beb30e6a357e8` | 2026-07-11 | `Address Codacy static-analysis flags on the new code` |
| `3ecc4c56ade3f38bdcec347894d1b0afcd6180be` | 2026-07-11 | `pcre: fix heap overflow in pcre_replace() with nested capture groups` |
| `ab101a74ded74ff60b702886f14555d67bb236b2` | 2026-07-11 | `sprintf: fix column/table UAF and pad leak` |
| `6117620e13dfa2f6a00250d4ed538964406e9e0a` | 2026-07-11 | `compress: free output_data on the uncompress() error path` |
| `9211908f7cc8708388fc0f30fbb9dbb710fd116a` | 2026-07-11 | `sockets: bound host copy in lpcaddr_to_sockaddr` |
| `93439e09d624ed12ac9fbd59deaebed9006c7aa8` | 2026-07-11 | `dwlib: bound replace_dollars() by the replacement length, not the pattern` |
| `fca233040f08f1ead263c21053d5e675eabc0345` | 2026-07-11 | `contrib: fix replaceable() null-deref and terminal_colour() leak` |
| `dcd4e8839b14e821e7e22dfe877cca4553d4e6e3` | 2026-07-11 | `parser: skip non-objects in living_parse() (parse_command %l)` |
| `9b2cc9a0d87fbfecb4ccdad9c4e7534e5eee76b2` | 2026-07-11 | `disassembler: use sizeof(LPC_INT) for the switch-table minval width` |
| `d2f907a3cdd375a185492de2ba17cc81dcb1aa24` | 2026-07-11 | `lexer: avoid infinite loop on a malformed #define parameter list` |
| `efb01a45b976809affd1422ec63cb01764039d7c` | 2026-07-11 | `stralloc: cap hash-table power-of-two rounding to avoid overflow hang` |
| `9478640c96a76bc4d36322dc276001bc8fff5865` | 2026-07-11 | `port: guard random_number/secure_random_number against n &lt;= 0` |
| `cb17196d67cc0c1f87cf5f15f40ce47ce0c90f67` | 2026-07-11 | `vm: fix out-of-bounds read and tainted format in call-other type check` |
| `02b8bebed04931441d9442a6419b7bcb697398ed` | 2026-07-11 | `vm: fix signed-overflow OOB write in write_buffer()` |
| `23c48f9bb068ccbea4c12083ec512d19502f8cb0` | 2026-07-11 | `ed: bound four static-buffer writes in the line editor` |
| `9c1e559325ccddbc758ae7285e06130480ba8c47` | 2026-07-11 | `reclaim: stop double-decrementing func_ref for FP_LOCAL funptrs` |
| `8c11403f63574bbe9f85432f0d7175a75bc7de3b` | 2026-07-11 | `sockets: set close-on-exec on the accepted fd, not the listener index` |
| `0f85cfcd119bcb69596c105b0cdb81f3189e862f` | 2026-07-11 | `contrib: fix query_replaced_program() reading a bogus object slot` |
| `5150ba51ce098d5be12ec582708a8bc598382077` | 2026-07-11 | `call_out: guard null funptr owner in reclaim_call_outs` |
| `fb733e2a97b639feddad356067e02d0ba0b5df76` | 2026-07-11 | `telnet: fix ZMP argument array off-by-one overflow` |
| `ac44db3e7bc9bca816f758d797ca714ccd5f6106` | 2026-07-11 | `strutils: fix infinite loop in reverse EGC search with no aligned match` |
| `df0da4b968c2924d006ad8e5c3a4bf2a44b30209` | 2026-07-11 | `sprintf: bound the integer/float print so a huge precision can't overrun` |
| `a14eff35b08555db283bef668287e6dd80b5abbf` | 2026-07-11 | `vm: don't leak the result array when allocate()'s initializer errors` |
| `3b6a9d7a4358ae29197babe9ced2f229eb4a67e6` | 2026-07-11 | `files: clamp read_file line-search so the terminator stays in bounds` |
| `48191c23e85f906b53013cfc41d69fe3065c1a1a` | 2026-07-11 | `parser: don't use verb nodes/results freed by a mid-parse destruct` |
| `c675ee1fc0601910326ed3b6192af0485c801b07` | 2026-07-11 | `vm: close the source fd when recompile_object's compile aborts` |
| `61757296b6e46100e48b5c4b927fa3e298a82d38` | 2026-07-11 | `vm: fix buffer range assignment copying from the buffer_t header` |
| `89060c5a6b3fe9f3a9493fbc0e0fbace669af37c` | 2026-07-11 | `docs: fully-expandable generated sidebar + Chinese docs via Docusaurus i18n (#1246)` |
| `5f7be1008f28ac400e7fa436b6e353656bb7db58` | 2026-07-11 | `Char-mode input: deliver real keystrokes (BS/DEL, whole UTF-8, raw ESC); fix NAWS lost at logon (#1245)` |
| `b0f61c331350e321ac4c1f53c5ff16e654419b0d` | 2026-07-11 | `Preserve this_player() in resolve() and async_* callbacks (#1104)` |
| `9d4a926a758cf03b3b6657a7573fea1adbb3a2c8` | 2026-07-11 | `Fix value-stack underflow when a safe_apply callee throws (#1014)` |
| `a136a45cd5c578394e066bf8c331662fcddc9e36` | 2026-07-11 | `Add set_clean_up() efun: schedule an object's next clean_up query (#918)` |
| `a5a17420509e62881cea48098fd84133b9cccf5c` | 2026-07-11 | `Pin grapheme-cluster string semantics around CRLF (#1071)` |
| `3dcd99955c837665b907de8b7400e27a7b3f6ff7` | 2026-07-11 | `Diagnose 'class Foo { ... } var;' with a targeted error (#788)` |
| `310a53e6cd04dedf0f528bbd4e51560dfc76d989` | 2026-07-11 | `Keep float variables float across compound assignment (#993)` |
| `6bb08851ffa332fcc47d79907e66d1517e23abee` | 2026-07-11 | `call_stack(4): drop the dead 'true \|\|' condition (#547)` |
| `6a25f5fe9b87ca0c307b31cbc7f45089d2e5abcf` | 2026-07-11 | `wasm: shrink the driver to ~0.8MB over the wire (33.5MB -&gt; 3.5MB raw) (#1243)` |
| `d38dc2c8338e1cabb49acd9a8291771e6f6161e3` | 2026-07-11 | `lexer: strip comments from directive payloads before parsing (#1240) (#1241)` |
| `82d82f3beb7a838e89c4c3492940a948adddaac0` | 2026-07-11 | `Add recompile_object() efun: in-place program update, state preserved (#1237)` |
| `c364856f0d1372eabf4288dc8306960bf59b6b90` | 2026-07-11 | `lexer: block comments on directive lines may span physical lines (#1236) (#1239)` |
| `e5c7bbf96a648d71d8f8cad96164d1f72a35e2a3` | 2026-07-11 | `sprintf: keep indents and the wrap grid intact in column mode` |
| `00fdf64eceac3c5518c0f53c92beb3aaf2092691` | 2026-07-11 | `parser: keep UTF-8 words during tokenization` |
| `88b21f970baeada70c30afbd0eabcd75d01ea50f` | 2026-07-11 | `contrib: get_os_env()/set_os_env() with config allow-lists` |
| `8a367cff4d80e451828726d30528fa4de33e9ddc` | 2026-07-11 | `vm: don't leak unique_mapping state when a size limit unwinds it` |
| `23fae1a35caa0eebf38ef4be0cefb0044c563e75` | 2026-07-11 | `vm: count loads before the valid_read apply in load_object` |
| `e6f0c377f85007680b837a4187995fd010647894` | 2026-07-10 | `wasm: run the full driver in the browser (Emscripten port) (#1231)` |
| `6f33a65c138abda59690a20250f485af03b028fe` | 2026-07-10 | `build: add pkg-config and libffi to dependency lists (#1235)` |
| `55956a24b7f2f4b58fc19588b92718da9dce92ad` | 2026-07-10 | `Add inherit_program / include_file master applies; auto hot-reload demo (#1230)` |
| `d4791ce9884012e8774c81c8534db637b9e48498` | 2026-07-10 | `testsuite: re-drop generated artifacts resurrected by the rebase` |
| `b08c29129879c58f5e7a57381fcf33947228c521` | 2026-07-10 | `compiler: instrs[] overflow past 512 opcodes corrupted globals; dwlib scratch ref` |
| `c46b8f8b533f7725c9e009d5658a51ae37763d45` | 2026-07-10 | `compiler: drop the stashed source fd when a nested compile is rejected` |
| `eb024fa96df24390b1dd6c70b9424b7427ddb5cf` | 2026-07-10 | `compiler: correct the excluded-entry comments per review` |
| `88fe45dd4d25f2e1fecaa9ec6def5dd06a4bfb16` | 2026-07-10 | `review fixes: test hygiene, lpcshell exit scope, dwlib CI coverage, doc nits` |
| `e72683ab62ca4834753ae6b4232b6b1f76823c61` | 2026-07-10 | `compiler: don't leak per-compile function bookkeeping for pure prototypes` |
| `ae1baf9b2488aa9dea880105d0f89f9cea47b8c0` | 2026-07-10 | `docs: register request_clean_up in the efun indexes` |
| `ea57b78923d863e07cb1883434e85c525dd95570` | 2026-07-10 | `dwlib: make the package buildable again (opt-in)` |
| `851628331532dc158d816781ed07b38f29adfab4` | 2026-07-10 | `compiler: name the function in argument-mismatch warnings` |
| `8e301dad9913adb4a9406655fbb3954fe6e72381` | 2026-07-10 | `base: replace whashstr with std::hash (issue #1177)` |
| `168ba9f3b36cd418243c323dba2d62e39c644ee1` | 2026-07-10 | `lpcshell: non-interactive and script-file modes` |
| `75091675af150e3177c280dc8323e4a51df27763` | 2026-07-10 | `core: member_array() flag 4 runs a function argument as a predicate` |
| `64eabf810a01721f3bb1926fe568b4b468082489` | 2026-07-10 | `core: new efun request_clean_up() to resume clean_up queries` |
| `6b5e875b54a72123bd956f949368e9cd3b456df6` | 2026-07-10 | `vm: add 'display preload progress' runtime option` |
| `e7d99f1091a7d2711987e457f48483008403c947` | 2026-07-10 | `strutils: emoji skin-tone modifiers add no width in u8_width()` |
| `8d017be6f6793d770703cf7b82c3368493071672` | 2026-07-10 | `contrib: pluralize() -ff and -is fixes` |
| `0358c3541195e4a61a4347f4ed8cbc1a78d90f9a` | 2026-07-10 | `db: fix SQLite3_fetch row targeting and MySQL date/time fetches` |
| `5ed1bb574d1b4f2a45d30e26afdcf0b1c6ff7427` | 2026-07-10 | `contrib: terminal_colour wraps by display columns, not bytes` |
| `d64e37519c14e75fe66cbcda3f02d29e2cbb63b3` | 2026-07-10 | `compiler: descend into inherits when a ::-lookup hits a prototype entry` |
| `733f9b69897bf7a627350a7db0b0be8902e93899` | 2026-07-10 | `core: remove_action() accepts the function pointer form of an action` |
| `bc25bfc8a86583addfb3f37db9f9c584ef75b9f2` | 2026-07-10 | `testsuite: stop tracking generated test artifacts` |
| `a77885d87c8a0529a7a9062781ce9a3f6b4d0720` | 2026-07-10 | `comm: contain prompt-path errors; guard null error context` |
| `8c621ed15ef64f58ed81898a4dd792dc4a1f82b6` | 2026-07-10 | `vm: repair instead of crash on value-stack underflow in restore_context` |
| `214b6bf986acdee4850df54332f38c407a9bd646` | 2026-07-10 | `vm: reversible explode keeps fields when the string is all delimiters` |
| `60793bf5cd449d075b4a4adc9465f9693a4e8d7e` | 2026-07-10 | `vm: don't leak the source fd when a compile aborts via error()` |
| `5b74e1918727ba2614bb911a715aaa1a425d1a3b` | 2026-07-10 | `vm: don't truncate foreach iteration count to 16 bits` |
| `9927e93bf363c5a7ad1167133dbfc3fd0f14f793` | 2026-07-10 | `db: fix double sqlite3_finalize after a failed statement execution` |
| `56f09112d1a75b4e72db81d874cb52cb57f553be` | 2026-07-10 | `pcre: don't loop forever on zero-width matches in pcre_match_all` |
| `887f9ebbd1d768a689781a999c602894e41a8fdd` | 2026-07-10 | `docs: document UTF-8 native strings in the LPC language reference (#1226)` |
| `c914f03d669e201be79c8e451979e3656b2ba704` | 2026-07-09 | `Add local search and documentation guide for docs site (#1221)` |
| `319b5ef300906dd4a99bc1713c38ce77be57f0bb` | 2026-07-09 | `Don't reject pending call_out handles below the allocation counter (#1220)` |
| `f3f767d216022c38a7f12bcb02b0c714c94a2c5d` | 2026-07-09 | `websocket: keep protocol units whole across ws messages, refuse MCCP on ws (#1219)` |
| `5d1cefce25f2ec372f792280486a0d4314618a79` | 2026-07-09 | `Fix new Coverity Scan defects (run 1663736-1663755) (#1218)` |
| `b1b81aacb5db6b2fd25ccea4353c0b0b115443af` | 2026-07-09 | `Route websocket telnet-subprotocol output through libtelnet, fixing MCCP (#1217)` |
| `88ce5569d16903f4ba1b036cf8bc7a6c8f9308da` | 2026-07-09 | `Suppress trigraph warnings in CMake configuration (#1214)` |
| `af928e9686495ab7316bb4a91cb001b9fe8e16bc` | 2026-07-09 | `fix 4-byte data loss on wss:// output bursts larger than 2048 bytes (#1216)` |
| `3bd12c657ecbd03d9e5663de8795dff24564f537` | 2026-07-09 | `fix config md` |
| `bad749be954879324e0f03b96eda6114054b1df0` | 2026-07-09 | `clang-format: pin PointerAlignment so the reformat is reproducible` |
| `271bde89a1c0404f36136f2eb039226cfdda9490` | 2026-07-09 | `clang-format: reformat the source tree with clang-format 23` |
| `816a05cc149917c23c8dbb3777c957d68593cc5c` | 2026-07-09 | `docs/zh-CN: mirror the efun doc pass into the Chinese translation` |
| `c9b5f2f0ebeb541b950a0efed388911637577593` | 2026-07-09 | `docs/efun: document FFI + dwlib efuns, fix return-type errors, drop stale docs` |
| `fd705f0634d0855afc7be6b68cb1e81b86d2e0cd` | 2026-07-09 | `docs: reconcile FFI plan + preprocessor/diagnostics docs with implementation` |
| `a439c89022207f39a9d30b75988337b56a47d860` | 2026-07-09 | `std/decimal: exact fixed-point decimal arithmetic as an LPC library` |
| `d63da8e405f6293ef17369e842e4f8370aa3ee00` | 2026-07-09 | `async: account the in-flight request in async_mark_request (Windows Debug leak)` |
| `182d09d2919ebb6eeb26628b0c3167f9de890e0e` | 2026-07-09 | `testsuite: guard link.lpc with efun_defined (link is not an efun on Windows)` |
| `31b34ab6fc10b54dcfa24d7a08b2f7da6d5e59db` | 2026-07-09 | `testsuite: make socket_connect / link portable (macOS/Windows)` |
| `c0b35359e1d1436998a3bc53eab674584b8c6aef` | 2026-07-09 | `Fix async_db_exec crash: process_efun_callback must precede valid_database` |
| `d9e9fbf8da5e2ac0b0088b9a8346b3c9f9110494` | 2026-07-09 | `CI/Docker: install libffi for package_ffi on every platform` |
| `9be761aa005dca64e2742fd473e8b2124171a5e9` | 2026-07-09 | `package_ffi: foreign function interface for LPC (libffi), with callbacks` |
| `ef4d6744b09f6b2424b7fbb030ee5829a0e3cbd5` | 2026-07-09 | `docs/ffi-plan: buffers throughout — no implicit string&lt;-&gt;char* marshalling` |
| `b9e67f1d496810a3963f78e1b897c77fea514761` | 2026-07-09 | `docs: add package_ffi design plan (libffi-based LPC FFI + tools/ffi)` |
| `ebc86aab021fa25b4dec59dad0535975d8a3baf9` | 2026-07-09 | `Every efun has a test: ~130 new files close the whole spec inventory` |
| `55e47edc070c9539ac959d8fbc4feaefb9c62ba2` | 2026-07-09 | `Float exponent notation: 2.5e2, 1e3, 2E-5 are float literals now` |
| `62e29ab5f23ea3acfa9f99e5904b502787d44acb` | 2026-07-09 | `Syntax coverage matrix; fix name::fn() super calls into .lpc parents` |
| `12d99317d8662203da3b2104b9bd7b4bb79491b4` | 2026-07-09 | `Testsuite coverage round 2: ctest test named "testsuite"; save_object overflow fixed` |
| `f5fa913fd3e4a9d944380ac98e5f4137f78e6ee5` | 2026-07-09 | `LPC testsuite: gtest-style harness, ctest registration, high-value pins` |
| `a0f6c74561fb6e5307eb029e46e9f43c1b102a9a` | 2026-07-09 | `Grammar contract moves to tools/lpc-syntax; add VS Code extension` |
| `34a9a8dd77b8d21a5511a212779b3f9538dd4799` | 2026-07-09 | `lexer.l keeps only generated-TU-private primitives; prune lexer_utils dead code` |
| `7fe7c5c9a990a55e3e38e956754d8205b0b32533` | 2026-07-09 | `docs/lpc: source-file resolution, diagnostics, full preprocessor reference` |
| `5e04adbb1db173029bb00e0b51819043cfe9f692` | 2026-07-09 | `Tokenize build dir as $BUILD_ROOT$ in checked-in scanner/parser` |
| `592dfc06885410dde0fccc43da0adcf0f1b08284` | 2026-07-09 | `Prefer .lpc source extension; rename testsuite to .lpc` |
| `1cf1dc7b47e27b38ec493b80233b221c872cf2f3` | 2026-07-09 | `Preprocessor state folds into CompileState; per-compile session gone` |
| `cc647d3ca8e37623d2360c6942f51ec517fe81f8` | 2026-07-09 | `Diagnostics: full clang shape -- gutters and located expansion notes with definition snippets` |
| `a78f25eeaa9006c7703c1832f4be2dac12fd5ae6` | 2026-07-09 | `Grammar as a machine contract: 3-layer EBNF + lpc-grammar.json + JS tokenizer/highlighter/formatter` |
| `1672e9a13864e515d3f16797e8db8142b3022c85` | 2026-07-09 | `Hardening: lexer.l rename, LexTokenStream removed, five real bugs fixed, coverage along the failure axes` |
| `460d2e38002f9fcfb488facc29e0c581a53f58da` | 2026-07-09 | `Staged compiler outputs: lpcc -E / --tokens / --ast / -O0` |
| `c48ce9ef0fdc3f9baf4df387b083b4be8a68f81f` | 2026-07-09 | `One compiler state object: CompileState g_compile` |
| `b550ccf04fb1b072c9aab2cd627fadd0d929f003` | 2026-07-09 | `Zero-copy scanning: yy_scan_buffer everywhere; flex allocations on the arena` |
| `9fba6cd38561066f3d559297dbc2fab535987eda` | 2026-07-09 | `Arena engineering: deque-retained chunks, DMALLOC accounting, stats, multi-round whole-compile benchmark` |
| `738c00d368c471f0dca97e3af241f770c0a91fc3` | 2026-07-09 | `Replace the LexStream byte-stream hierarchy with flex-native scan buffers` |
| `13b0fcf15ce8f4ea06c5261087e087f64325d00c` | 2026-07-09 | `Scratchpad: C++ arena allocator, typed value stack, malloc-free compiles` |
| `c5ddaa4bc7a1e70c5b01bdf6c3db22158f25a446` | 2026-07-09 | `CI green on every platform; security hardening; macro redefinition is legal` |
| `48fe0e9b5a88f30d75ed44918eb1e36a183eab17` | 2026-07-09 | `Grammar-driven lexical simplification; minimal token inventory` |
| `1a7a76de704bd743e1ec1e73517ea26362b74f76` | 2026-07-09 | `Flex-native input endgame + structured clang-style diagnostics` |
| `9ac79102fe2e263bef571359bf875d259b061eea` | 2026-07-09 | `Reentrant push-driven front-end: one scan, LexTokenStream, lpcshell` |
| `f79c309b33458be8065f234cf4a9a651ff5dfbd6` | 2026-07-09 | `Language: template literals, mapping dot access + optional chaining, %g` |
| `bf21cb1ef4753fb28cc975faf544fd63988b002c` | 2026-07-09 | `Flex lexer migration: hand-written scanner replaced by a generated DFA` |
| `51888b074058ba3cb00b9653e1e01086743262e1` | 2026-07-09 | `Grammar modernization: top-down restructure, named references, split rule files` |
| `9e5a1f16dd303ae446944ad5cc12c594003bb3ba` | 2026-07-02 | `docs: modernize concepts/general and fix correctness (#1211)` |
| `0a928bfe9d62522030345274e7c699b2d4b60238` | 2026-07-02 | `docs: gitignore Docusaurus build artifacts (#1212)` |
| `2063e954367b44d8ce70ce9512b5020634bf9348` | 2026-06-24 | `Fix Docusaurus sidebar, broken links, and gh-pages CI (#1209)` |
| `2f261c5e3f1338882ed093cda51d3798cb59c5b6` | 2026-06-23 | `Add LPC/Driver sidebar subsections and update docs agent guides` |
| `1ee613bd158419d9ea72070db276081e8b052612` | 2026-06-23 | `Migrate docs from VitePress to Docusaurus 3.10.1` |
| `d922ff5dfe82a8606acf8832ed2ec9d4c246c855` | 2026-06-23 | `Upgrade docs to VitePress 1.6.4 and remove dead Docusaurus deps` |
| `74df97071848274b2b9880931400ade145fcc318` | 2026-06-23 | `Fix first-class functions to restore compile-time error detection (#1194)` |
| `053711e685a7bb5c5d862af152fa2abd80ac679c` | 2026-06-23 | `Fix max_eval_cost() return type in docs: void → int (#1193)` |
| `7fb561ecda9e47cd29b025177339d8209409d744` | 2026-06-23 | `Fix unguarded dump_trace() causing abort in Debug builds (#1192)` |
| `332e4b03070385d2d92691dfcd7f5f05405181ee` | 2026-06-23 | `correcting pcre_replace_callback documentation (#1201)` |
| `c90b7d0879010be09abff7a0e67fdcfe035c8b13` | 2026-06-23 | `document sort_array extra args forwarded to comparator (#1202)` |
| `3ba76af21d582102f4cb92401334a80fff859e55` | 2026-06-23 | `Fixed typo in code example (#1205)` |
| `b3427755c66cc99627c314dda012dc2e72b3aa1d` | 2026-06-21 | ``Fix crash and leak when taking a `ref` into a mapping element (#1207)`` |
| `d1951b8671040216d333c4039bdb43fd1a1c7398` | 2026-06-21 | `Add Windows dev environment, agent guide, and doc overhaul (#1208)` |
| `e1a50d264287b4b0b100f6969adbfeff9f6f74c4` | 2026-06-02 | `Generate driver config docs and a starter config from rc.cc tables (#1206)` |
| `ee8137603946d12248e9ed429f09eb2388e007e3` | 2026-01-27 | `gdb can be invoked in one line (#1182)` |

## 附录 E. 模块差异完整分组（no-renames）

按路径前两段归组，根目录文件单列。数据来自同一 merge-base → HEAD；不用增删规模冒充语义变更量。

| 模块/路径 | 变更路径 | 新增行 | 删除行 | 非文本 diff |
| --- | ---: | ---: | ---: | ---: |
| `.dockerignore` | 1 | 3 | 0 | 0 |
| `.editorconfig` | 1 | 3 | 0 | 0 |
| `.gitattributes` | 1 | 7 | 0 | 0 |
| `.github/workflows` | 7 | 431 | 490 | 0 |
| `.gitignore` | 1 | 11 | 0 | 0 |
| `.node-version` | 1 | 1 | 0 | 0 |
| `AGENTS.md` | 1 | 215 | 0 | 0 |
| `CHANGELOG.md` | 1 | 52 | 0 | 0 |
| `CLAUDE.md` | 1 | 0 | 392 | 0 |
| `CMakePresets.json` | 1 | 106 | 0 | 0 |
| `CODE_OF_CONDUCT.md` | 1 | 74 | 0 | 0 |
| `CONTRIBUTING.md` | 1 | 54 | 0 | 0 |
| `Dockerfile` | 1 | 18 | 6 | 0 |
| `LICENSE` | 1 | 14 | 0 | 0 |
| `NOTICE` | 1 | 36 | 0 | 0 |
| `QWEN.md` | 1 | 0 | 206 | 0 |
| `README.md` | 1 | 258 | 149 | 0 |
| `README_CN.md` | 1 | 248 | 142 | 0 |
| `SECURITY.md` | 1 | 66 | 0 | 0 |
| `TODO.md` | 1 | 0 | 55 | 0 |
| `cmake/FindPCRE.cmake` | 1 | 90 | 36 | 0 |
| `compat/parser_error.c` | 1 | 47 | 49 | 0 |
| `compat/simuls` | 6 | 592 | 609 | 0 |
| `docs/.vitepress` | 2 | 8 | 6 | 0 |
| `docs/CLAUDE.md` | 1 | 0 | 538 | 0 |
| `docs/apply` | 3 | 34 | 0 | 0 |
| `docs/archive` | 4 | 1358 | 0 | 0 |
| `docs/backlog-full-plan-2026-09.md` | 1 | 323 | 0 | 0 |
| `docs/backlog-plan-2026-09.md` | 1 | 113 | 0 | 0 |
| `docs/build.md` | 1 | 59 | 10 | 0 |
| `docs/cli` | 1 | 86 | 3 | 0 |
| `docs/coding-standard.md` | 1 | 319 | 0 | 0 |
| `docs/concepts` | 2 | 84 | 0 | 0 |
| `docs/driver` | 1 | 5 | 0 | 0 |
| `docs/e4-read-source-line-port-eval-2026-08.md` | 1 | 78 | 0 | 0 |
| `docs/efun` | 51 | 910 | 60 | 0 |
| `docs/evidence` | 69 | 345966 | 0 | 0 |
| `docs/external-required-readiness-2026-09.md` | 1 | 108 | 0 | 0 |
| `docs/fluffos-xk-overview.md` | 1 | 181 | 0 | 0 |
| `docs/index.md` | 1 | 89 | 50 | 0 |
| `docs/lpc` | 4 | 134 | 0 | 0 |
| `docs/lpc-modern-runtime.md` | 1 | 241 | 0 | 0 |
| `docs/lpcshell-prerequisite-plan-2026-08.md` | 1 | 95 | 0 | 0 |
| `docs/multicore-production-gate.md` | 1 | 131 | 0 | 0 |
| `docs/multicore-runtime-v2.md` | 1 | 46 | 0 | 0 |
| `docs/multicore-runtime-v4.md` | 1 | 323 | 0 | 0 |
| `docs/multicore-runtime.md` | 1 | 29 | 0 | 0 |
| `docs/optimization-master-plan-2026-08-18.md` | 1 | 467 | 0 | 0 |
| `docs/owner-multicore-api.md` | 1 | 304 | 0 | 0 |
| `docs/package-lock.json` | 1 | 2051 | 15654 | 0 |
| `docs/package.json` | 1 | 13 | 15 | 0 |
| `docs/project-knowledge-map.md` | 1 | 504 | 0 | 0 |
| `docs/project-modernization-plan-2026-10.md` | 1 | 1943 | 0 | 0 |
| `docs/project-scope.md` | 1 | 90 | 0 | 0 |
| `docs/recompile-followup-plan-2026-08.md` | 1 | 298 | 0 | 0 |
| `docs/recompile-object-special-design-2026-08.md` | 1 | 1001 | 0 | 0 |
| `docs/recompile-object-v2-design-2026-08.md` | 1 | 171 | 0 | 0 |
| `docs/reports` | 7 | 976 | 0 | 0 |
| `docs/runbooks` | 2 | 130 | 0 | 0 |
| `docs/tsan-ftest-report-2026-08-19.md` | 1 | 41 | 0 | 0 |
| `docs/upstream-absorption-plan-2026-10.md` | 1 | 1084 | 0 | 0 |
| `docs/upstream-absorption-plan-d07e7641-735bd31f.md` | 1 | 901 | 0 | 0 |
| `docs/upstream-sync-evidence-2026-08.md` | 1 | 480 | 0 | 0 |
| `docs/upstream-sync-optimization-plan-2026-08.md` | 1 | 902 | 0 | 0 |
| `docs/vendor-libwebsockets-manifest-2026-08.md` | 1 | 39 | 0 | 0 |
| `docs/zh-CN` | 11 | 87 | 14 | 0 |
| `etc/config.test` | 1 | 391 | 0 | 0 |
| `src/.clang-format` | 1 | 19 | 1 | 0 |
| `src/.clang-tidy` | 1 | 36 | 16 | 0 |
| `src/CMakeLists.txt` | 1 | 223 | 33 | 0 |
| `src/Config.example` | 1 | 3 | 0 | 0 |
| `src/backend.cc` | 1 | 588 | 82 | 0 |
| `src/backend.h` | 1 | 94 | 10 | 0 |
| `src/base` | 34 | 1437 | 623 | 0 |
| `src/comm.cc` | 1 | 1115 | 304 | 0 |
| `src/comm.h` | 1 | 34 | 20 | 0 |
| `src/compiler` | 33 | 6321 | 3707 | 0 |
| `src/include` | 3 | 25 | 27 | 0 |
| `src/interactive.h` | 1 | 27 | 14 | 0 |
| `src/local_options.README` | 1 | 6 | 0 | 0 |
| `src/main.cc` | 1 | 4 | 2 | 0 |
| `src/main_fuzz_compile.cc` | 1 | 220 | 0 | 0 |
| `src/main_fuzz_restore.cc` | 1 | 159 | 0 | 0 |
| `src/main_generate_keywords.cc` | 1 | 5 | 6 | 0 |
| `src/main_json2o.cc` | 1 | 4 | 4 | 0 |
| `src/main_lpcc.cc` | 1 | 164 | 8 | 0 |
| `src/main_lpcshell.cc` | 1 | 387 | 0 | 0 |
| `src/main_o2json.cc` | 1 | 9 | 11 | 0 |
| `src/main_symbol.cc` | 1 | 3 | 1 | 0 |
| `src/mainlib.cc` | 1 | 66 | 24 | 0 |
| `src/net` | 13 | 783 | 376 | 0 |
| `src/ofile.cc` | 1 | 57 | 45 | 0 |
| `src/ofile.h` | 1 | 3 | 3 | 0 |
| `src/packages` | 92 | 22863 | 3958 | 0 |
| `src/portbind.cc` | 1 | 6 | 7 | 0 |
| `src/symbol.cc` | 1 | 30 | 13 | 0 |
| `src/symbol.h` | 1 | 2 | 2 | 0 |
| `src/tests` | 41 | 35826 | 116 | 6 |
| `src/thirdparty` | 1329 | 68250 | 150084 | 98 |
| `src/tools` | 7 | 2418 | 245 | 0 |
| `src/user.cc` | 1 | 11 | 8 | 0 |
| `src/user.h` | 1 | 4 | 4 | 0 |
| `src/vm` | 81 | 25789 | 2340 | 0 |
| `testsuite/adm` | 1 | 22 | 0 | 0 |
| `testsuite/clone` | 64 | 1134 | 206 | 0 |
| `testsuite/command` | 24 | 1165 | 919 | 0 |
| `testsuite/data` | 2 | 4 | 0 | 1 |
| `testsuite/etc` | 5 | 493 | 6 | 0 |
| `testsuite/format.sh` | 1 | 101 | 0 | 0 |
| `testsuite/include` | 1 | 1 | 1 | 0 |
| `testsuite/inherit` | 11 | 148 | 91 | 0 |
| `testsuite/single` | 527 | 14703 | 3977 | 0 |
| `testsuite/std` | 17 | 1469 | 1642 | 0 |
| `third_party/cyclonedx.schema.json` | 1 | 1697 | 0 | 0 |
| `third_party/manifest.yaml` | 1 | 188 | 0 | 0 |
| `third_party/sbom.json` | 1 | 555 | 0 | 0 |
| `tools/build` | 3 | 271 | 0 | 0 |
| `tools/docs` | 6 | 1649 | 0 | 0 |
| `tools/loadtest` | 1 | 813 | 0 | 0 |
| `tools/lpc-modern-runtime-stress.sh` | 1 | 97 | 0 | 0 |
| `tools/lpc-syntax` | 17 | 8324 | 0 | 0 |
| `tools/owner-runtime-v4-stress.sh` | 1 | 73 | 0 | 0 |
| `tools/owner-scheduler-capacity.sh` | 1 | 143 | 0 | 0 |
| `tools/perf` | 132 | 13831 | 0 | 0 |
| `tools/sbom-generate.py` | 1 | 220 | 0 | 0 |
| `tools/style` | 2 | 1013 | 0 | 0 |
| `tools/testsuite` | 12 | 3658 | 0 | 0 |
| `tools/wsl-cmake-build.sh` | 1 | 16 | 0 | 0 |
| **合计** | **2717** | **583698** | **187420** | **105** |

## 附录 F. 物理目录元数据完整分组

于 2026-10-12T06:25:04+00:00 再次只读遍历，不跟随符号链接；排除本轮新建的两份报告，以保持起点分母。共 1,720,673 个物理文件/链接，遍历错误 0，与期初计数相同。目录另计，不并入文件分母。

Git 跟踪列属于审计源码集合。其余包含本地构建、忽略依赖/缓存、运行和工具状态及 Git 内部文件，只统计元数据，未读取全部内容、执行二进制或验证旧证据仍有效。非忽略未跟踪业务文件在审计起点为 0。

| 根路径 | 普通文件 | 符号链接 | 其他文件 | 目录 | Git 跟踪 | 未跟踪/内部元数据部分 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `.coveralls.yml` | 1 | 0 | 0 | 0 | 1 | 0 |
| `.dockerignore` | 1 | 0 | 0 | 0 | 1 | 0 |
| `.editorconfig` | 1 | 0 | 0 | 0 | 1 | 0 |
| `.git` | 1734 | 0 | 0 | 283 | 0 | 1734 |
| `.gitattributes` | 1 | 0 | 0 | 0 | 1 | 0 |
| `.github` | 5 | 0 | 0 | 2 | 5 | 0 |
| `.gitignore` | 1 | 0 | 0 | 0 | 1 | 0 |
| `.node-version` | 1 | 0 | 0 | 0 | 1 | 0 |
| `.pi` | 1 | 0 | 0 | 1 | 0 | 1 |
| `AGENTS.md` | 1 | 0 | 0 | 0 | 1 | 0 |
| `CHANGELOG.md` | 1 | 0 | 0 | 0 | 1 | 0 |
| `CMakeLists.txt` | 1 | 0 | 0 | 0 | 1 | 0 |
| `CMakePresets.json` | 1 | 0 | 0 | 0 | 1 | 0 |
| `CODE_OF_CONDUCT.md` | 1 | 0 | 0 | 0 | 1 | 0 |
| `CONTRIBUTING.md` | 1 | 0 | 0 | 0 | 1 | 0 |
| `ChangeLog` | 1 | 0 | 0 | 0 | 1 | 0 |
| `Copyright` | 1 | 0 | 0 | 0 | 1 | 0 |
| `Credits` | 1 | 0 | 0 | 0 | 1 | 0 |
| `Dockerfile` | 1 | 0 | 0 | 0 | 1 | 0 |
| `LICENSE` | 1 | 0 | 0 | 0 | 1 | 0 |
| `NOTICE` | 1 | 0 | 0 | 0 | 1 | 0 |
| `README.md` | 1 | 0 | 0 | 0 | 1 | 0 |
| `README_CN.md` | 1 | 0 | 0 | 0 | 1 | 0 |
| `SECURITY.md` | 1 | 0 | 0 | 0 | 1 | 0 |
| `Testing` | 2 | 0 | 0 | 2 | 0 | 2 |
| `build-asan` | 2038 | 0 | 0 | 285 | 0 | 2038 |
| `build-dev-debug` | 2091 | 0 | 0 | 293 | 0 | 2091 |
| `build-modernization-evidence` | 1694476 | 16 | 0 | 103419 | 0 | 1694492 |
| `build-portable-release` | 2037 | 0 | 0 | 298 | 0 | 2037 |
| `build-tsan` | 2018 | 0 | 0 | 285 | 0 | 2018 |
| `build-u06c-clang-off` | 655 | 0 | 0 | 251 | 0 | 655 |
| `build-u06c-clang-on` | 655 | 0 | 0 | 251 | 0 | 655 |
| `build-u06c-gcc-off` | 655 | 0 | 0 | 251 | 0 | 655 |
| `build-u06c-gcc-on` | 655 | 0 | 0 | 251 | 0 | 655 |
| `build-ubsan` | 2001 | 0 | 0 | 285 | 0 | 2001 |
| `cmake` | 7 | 0 | 0 | 1 | 7 | 0 |
| `compat` | 8 | 0 | 0 | 2 | 8 | 0 |
| `docs` | 8857 | 8 | 0 | 618 | 1153 | 7712 |
| `etc` | 1 | 0 | 0 | 1 | 1 | 0 |
| `fix_permission.sh` | 1 | 0 | 0 | 0 | 1 | 0 |
| `qodana.yaml` | 1 | 0 | 0 | 0 | 1 | 0 |
| `src` | 1769 | 0 | 0 | 261 | 1768 | 1 |
| `testsuite` | 763 | 0 | 0 | 32 | 736 | 27 |
| `third_party` | 3 | 0 | 0 | 1 | 3 | 0 |
| `tools` | 195 | 0 | 0 | 26 | 178 | 17 |
| **合计** | **1720649** | **24** | **0** | **107099** | **3882** | **1716791** |
