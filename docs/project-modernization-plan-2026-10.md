# FluffOS_XK 全面整理与优化执行方案

日期：2026-10-06。状态：**用户已批准按本方案和代码规范连续实施；当前从 U00 开始，未完成项不得视为通过**。

调查与最终审计阶段仅修改文档。用户随后批准严格按本方案连续开发；源码、测试、构建和隔离验证按各单元范围执行。独立的[代码规范](coding-standard.md)继续适用。§1.2 的排除项、单列策略审批、工作区归属和外部操作授权不因开发启动而取消；不部署、重启运行实例或操作下游业务数据。

## 1. 结论、目标与边界

推荐路线：**保留当前 fork，补齐可证实的正确性缺口，固定兼容性基线，再按模块吸收上游和简化实现。不要用上游整树覆盖本地，也不要先全仓格式化。**

问题不是“大部分代码旧，所以全部换新”。本地同时存在：已经吸收的修复；上游没有的 owner/service、gateway、编码及事务能力；确实未跟进的编译器结构和修复；反复扩展后留下的重复状态、长函数和验证债务。必须分别处理。

整理完成必须同时满足：

1. 原有 mudlib 不改源码、不迁移存档、不改配置，就能继续执行原来合法且受支持的行为。
2. 现有 LPC efun/apply、配置、CLI、网络、存档及对外状态结构保持兼容。不能只证明函数名相同。
3. 已确认的正确性缺口有 fail-first 和修复后证据；不能用删测试、放宽检查或隐藏错误结案。
4. 简化减少真实的重复实现和维护成本，不削弱 owner、生命周期、权限、编码、输入容量和失败清理约束。
5. 性能优化有相同机器、配置、输入和构建条件下的前后数据。没有数据就不承诺提速。
6. 未验证平台、未取得的下游样本和未完成专项明确列出，不以单个单元通过宣称全项目闭环。

### 1.1 与既有方案的关系

[旧上游吸收方案](upstream-absorption-plan-2026-10.md)保存历史设计和实施证据；[重编译设计](recompile-object-v2-design-2026-08.md)、[owner API](owner-multicore-api.md)、[运行时合同](multicore-runtime-v4.md)、[编码合同](lpc-modern-runtime.md)仍是各模块语义依据。

[代码规范](coding-standard.md)是统一写法和排版的唯一归属文档；本文 U10 负责其配置、工具、推广和验收，不另开第二份执行计划。

本文是已获准的新一轮整理主计划，统一安排执行顺序；旧方案只提供对应单元的详细设计和证据，不再同时驱动另一套待办。发现旧文档与源码不一致时，以实际源码和可复现行为建立新基线，不照抄旧状态。

### 1.2 不纳入默认实施的事项

- 不撤销 fork，不 merge 上游，不迁移下游 mudlib、账号、世界数据或部署配置。
- 不删除现有公共 API、旧配置键、状态 mapping key、协议模式或仍有消费者的兼容接口。
- 不改变多核默认策略；不让普通 legacy LPC 自动进入后台并行执行。
- 不更换架构为 actor-only、lock-free、GC、JIT，不无依据更换分配器。
- 不默认引入 FFI、WASM、新语法、PCRE 新 API、TLS 热更新或新版警告策略。
- 不把公开 Release、签名、registry、provenance 等作为团队自用整理的先决条件，见[项目范围](project-scope.md)。

“不能破坏接口”与“收紧此前允许的安全策略”可能冲突。此类变更必须单列影响面和兼容样本，不能借安全修复之名悄悄改变权限语义；也不能为兼容而放行已明确禁止的行为。冲突未解决时，暂停相应变更，继续其他独立单元。

## 2. 调查基线与证据范围

### 2.1 固定身份

| 项目 | 本次实际值 |
|---|---|
| 本地分支 | main；首轮调查开始和首轮落盘前工作区均无已有修改；规范补充阶段保留首轮新增的本计划 |
| 本地 HEAD | 5c71593af1c45365e39e243136555b5d23dc5480 |
| 本地与上游历史共同祖先 | 277d0b1cbc4350844d9aff07a604dfa519650d9e |
| 本地 upstream/master 跟踪引用 | f1c242b1db54383d701838ee8aef512a0d923e81；不是当前公网最新值 |
| 旧吸收基线 / v2026.1001.0 | 795eb371f1af522280738f454b8fc81afe231f1c |
| 本次公网上游固定快照 | 328aaf10635ef8ce636b2b6cebd549bb165e462d |
| HEAD 与旧吸收基线的历史差异 | 本地侧 630 个、上游侧 350 个提交；不是“630 个未吸收功能”或“350 个缺陷” |
| 旧吸收基线之后 | 本次逐条核对 15 个上游提交，见 §5 |

本轮没有 git fetch、更新跟踪引用或签出上游。公网快照在内存中读取并与 git archive HEAD 比较；没有在工作区解压上游。所读快照压缩包 SHA-256 为 edd16c5a61635399ec168443e4786898adafff3e33daa1fc098fca41dc674bc8。后续上游再有提交，不自动改变本文基线。

固定来源：

- [上游快照](https://github.com/fluffos/fluffos/tree/328aaf10635ef8ce636b2b6cebd549bb165e462d)
- [本次新增 15 个提交的补丁](https://github.com/fluffos/fluffos/compare/795eb371f1af522280738f454b8fc81afe231f1c...328aaf10635ef8ce636b2b6cebd549bb165e462d.patch)
- 上游较早编译器改造通过本地已有 Git 对象读取；不依赖提交标题推断已吸收。

### 2.2 全树对照结果

按归档中的普通文件、精确路径和字节内容比较，不含目录、符号链接及权限位：

| 状态 | 文件数 | 解释 |
|---|---:|---|
| 字节相同 | 1178 | 无需为“跟新”改写 |
| 同路径内容不同 | 904 | 必须区分本地能力、上游改进和生成差异 |
| 仅本地存在 | 1585 | 包括 fork 能力、文档、测试；不是废弃文件清单 |
| 仅上游存在 | 1584 | 包括重命名、测试、工具和新功能；不是全部应移植 |

去掉 src/thirdparty、src/tests、src/www、src/wasm 后，src 下共有 **325 个非相同路径项**。附录 A 对每项列出两侧存在性、行数和归属执行单元，避免漏掉本地独有代码或把上游拆分误判为缺失功能。

上游 testsuite 的 .c/.lpc 文件先匹配精确路径，再匹配另一扩展名：77 个字节相同，379 个内容不同，532 个没有本地同名对象。这里包括 clone/inherit/master 等夹具，**不是 532 个缺失测试用例**。不能把全部 .c → .lpc 重命名当成新增覆盖。

### 2.3 调查深度与明确限制

本轮完成全树文件级一一对应、重点模块调用路径审阅、旧吸收账本与当前源码核对，以及新增 15 个提交的逐条处置。深读对象包括 compiler/VM/owner/object store、Promise/async/external、网络/TLS、文件/压缩、测试入口、构建和第三方清单。

没有逐行人工审阅全部第三方代码、全部 350 个历史提交的所有 hunk 或每个 mudlib 样本。附录的文件归属不是“此文件已证明无缺陷”。第三方、其余功能包和新增测试的余项由 U10/U11/U15 按明确出口完成；不能在实施时直接把这些行标成已吸收。

方案编制阶段没有构建、运行 driver、GoogleTest、sanitizer、fuzz、benchmark 或下游游戏。因此调查部分的“确认”指静态源码事实；实施后的运行证据和未验证边界统一记录在 §11。

## 3. 架构认识与不可破坏的合同

### 3.1 实际调用链

- 启动：src/main.cc → src/mainlib.cc → rc/master/simul 初始化 → src/backend.cc 事件循环。
- 输入：src/comm.cc、src/net/* 和 gateway adapter → VM/apply；网络和 cleanup 仍有 main-required 边界。
- 编译：LexStream → lex.cc → grammar.y/grammar_rules.cc → trees/icode/generate → program。当前不是上游的 Flex + push parser。
- 执行：VMContext 与 owner admission 保证同 owner 串行；service/worker 不能直接共享普通 LPC 可变引用。
- 对象：owner-sharded store 是 live lookup 主体；ObjectTable 和 object_records 仍承担兼容/诊断索引，不能仅凭名字包含 bridge 就删除。
- Promise：settle、microtask、async park/resume、destruct/recompile 清理都受 main/owner 与 frame-owned storage 约束。
- 重编译：复用 OwnerRuntimeCoordinator 和现有 prepare/activate/rollback，不另造事务或全局目录。

### 3.2 U00 必须冻结的兼容矩阵

| 面向 mudlib 的表面 | 需要冻结的内容 | 验证入口 |
|---|---|---|
| efun | 名称、参数数量/顺序/类型、默认参数、varargs、返回类型和值、失败形式、flags 数值、package 开关 | src/packages/*/*.spec、生成元数据、testsuite/单元反例 |
| master/simul/apply | hook 名称、调用顺序、参数、授权前后路径拼写、返回约定、reload 累积槽位 | docs/apply、master.cc、simul_efun.cc、真实 master fixture |
| LPC 语言 | 求值顺序、默认参数、可见性、继承/alias、算术转换、catch/foreach、宏/include、错误位置 | compiler tests + 原有 mudlib 编译/运行 |
| 源文件/对象身份 | 无扩展名优先 .lpc 再 .c；显式扩展名精确请求；identity 无扩展名；clone/inherit/recompile/replace 一致 | [源文件合同](lpc/source-files.md)、dual_extension、save/restore |
| 持久化 | save/restore 格式、NOSAVE、名字规范化、旧存档读取、失败保留原文件 | ofile_tests、save_object、restore_object、隔离存档样本 |
| 异步 | callback 参数/顺序/时机、command_giver、引用所有权、Promise 状态、取消/超时 | async/external/Promise LPC + C++ |
| IO/协议 | STREAM/BINARY、UTF-8/GBK 边界、telnet/MCCP、WS/WSS、TLS 验证和关闭时序 | loopback 真实握手与分片测试 |
| 多核/状态 | owner/epoch/generation、ObjectHandle、状态 mapping 字段、队列顺序/上限/计数 | owner/object store/gateway 合同测试 |
| 配置/CLI/构建 | 键名、默认值、范围、包组合、退出码、旧别名和支持平台 | rc/CLI 测试、CMake 配置矩阵 |
| 诊断/工具 | mudlib 可见错误、compile_error 参数、既有诊断样式与结构化记录、工具输出协议 | compiler_diag、lpcshell、脚本消费者 |

兼容清单必须区分“文档承诺且受支持的行为”“本次已证实的实现缺陷”“待裁决策略”。错误时报告成功、损坏有效 UTF-8 或读错栈槽不能冻结为必须保留的行为；修复后的失败形式仍从既有 API 合同选择并登记差异，不能借此任意换成新异常/返回体系。

本地已有 PCRE2、managed lvalue、源码扩展名适配、main-thread event parking、private slot 修复等，不因上游 diff 很大而重复实现。公共 C++ 接口的实际嵌入消费者也纳入 U00；内部 struct 不默认承诺永久二进制 ABI，但不能漏掉真实使用方。

## 4. 已确认事实与待验证风险

优先级：P0 为可能造成数据损坏、内存/线程错误的优先项；P1 为功能正确性和兼容性；P2 为结构、性能或维护。优先级不是已经发生事故的证明。

| ID | 级别 | 当前源码事实与位置 | 影响/证据边界 | 单元 |
|---|---|---|---|---|
| F01 | P0 | compress.cc 的文件压缩/解压循环忽略部分 read/write/close 结果，末尾仍 unlink 源文件并返回 1；gzread 的负值没有在写出前完整分流 | 有错误报告/数据保全缺口；需损坏输入和故障注入复现，不能在真实数据上验证 | U01 |
| F02 | P0 | core/file.cc、ed.cc、base/object.cc 等仍有写入/flush/rename 错误传播缺口 | 上游 b2ba3f8f 有对应修复；逐 hunk 核对，不能套用其源文件处理或覆盖本地 source-spelling | U01 |
| F03 | P1 | sockets.spec 声明 getter 两参数；sockets.cc::f_socket_get_option 读取 sp-2/sp-1，push 返回值后再 pop_2_elems | 参数和返回栈位置不符合两参数合同，需前后栈哨兵/不同 fd、option 的回归 | U02a |
| F04 | P1 | socket text-mode 每次读取单独 sanitize，未保存跨 read 的 UTF-8 尾部 | 合法多字节字符可能在 TCP/TLS 分片处受损；EOF 残片策略是独立兼容决定 | U02b |
| F05 | P0 | tracing.cc 的 dump thread 调 debug_message；log.cc 的日志状态不是为任意后台线程同步的 | 线程边界缺口；不能仅在 logger 外加一把锁，更不能因 dump 失败再从 worker 调 logger | U03 |
| F06 | P1 | async.cc 已在回调前释放 finished 队列锁并保留 active_callbacks | 上游新 async deadlock 修复的核心结构已存在；不重复移植，只补嵌套回调+check_memory 反例 | U04 |
| F07 | P1 | lex.cc include 路径组装仍可能保留多重分隔符，固定缓冲区拼接也需核对长度上限；上游相应逻辑位于 lexer_utils.cc | 不能修改 valid_read 接收的原始拼写来“修复”include_list；长路径边界必须测试 | U05 |
| F08 | P1 | rc.cc 对空 global include 仍走补引号分支；PACKAGE_SOCKETS 条件与 external/socket_err 依赖耦合 | 缺省 include 与关闭 sockets 的构建/启动是待复现风险，不先宣称编译已失败 | U06 |
| F09 | P2 | EGCIterator 的子区间复用只覆盖已知 ASCII，非 ASCII 后缀会重复分类 | Unicode explode 存在重复扫描结构；是否成为真实热点需要计数及基准 | U07 |
| F10 | P2 | object_store.cc::owner_local_bridge_summary_locked 遍历每个 shard record，又调用线性 global ID scan | 汇总最坏含二次工作量；普通有效 handle 先走快速解析并提前返回，不能误报所有 lookup 都 O(N²) | U08 |
| F11 | P2 | ws_ascii/ws_telnet 重复代理地址、建连/关闭处理；matrix 重复参数读取/变换计算 | 可借上游 helper 消除重复，但必须保留本地 output queue、TLS payload drain 和编码路径 | U09 |
| F12 | P2 | test_lpc.cc 28415 行；gateway_session.cc 7476 行；owner.cc 6551 行；lex.cc 5141 行 | 说明职责集中，不证明慢或错误；按职责和测试依赖拆分，不按行数配额拆分 | U10/U12/U13 |
| F13 | P2 | 本地手写 lexer/pull parser；上游已有 Flex、CompileState、拆分规则和 staged output | 存在实质现代化差距，也有本地语义和 arena 差异；不能整包 cherry-pick | U13/U14 |
| F14 | P1 | src/CMakeLists.txt 对 ENABLE_UBSAN 无条件附加 -fno-sanitize=null；注释解释的是特定 GCC/TLS 问题 | 当前条件比解释范围宽，Clang 也受影响；必须用工具链复现实验证明最小排除范围 | U06c |
| F15 | P2 | third_party/manifest.yaml 写 lws 4.2.1；vendor 专项文档称基于 4.5.8 系列，且留下待补精确提交项 | 清单与归属证据矛盾；先核真实树/版本/哈希，不直接升级或宣称有 CVE | U15 |
| F16 | P2 | src/.clang-format 的说明与实际值不符，且本地尚未固定指针对齐；clang-tidy 允许任意函数大小写；CONTRIBUTING 要求随文件风格 | 不仅是排版差异，还缺统一规范、版本和执行入口；U10 按独立代码规范对齐，不运行全 cwd 的旧格式脚本 | U10 |
| F17 | P1 | run-targeted.py::run_binary 只拒绝全跳过，未核发现/执行集合相等；run_driver 在全量无 C> 时补 passed=1；run_ctest 先判 Not Run 再判 timeout/非零退出 | 当前 runner 不满足本文非空、无跳过的验收合同；不能直接拿其 exit=0 签收单元 | U00a |
| F18 | P1 | testsuite/command/tests.c::execute 在调用 do_tests 前就输出 C>，定向执行返回后调用 shutdown 的 do_the_nasty_deed；后者立即 shutdown(0)；socket_tls_server.c 则由 call_out 才启动实际测试 | C> 是开始标志，不是完成标志；异步测试可能还未执行即请求退出，须真实 driver 验证完成屏障 | U00a/U02/U04 |
| F19 | P1 | bench_compile.cc 的 head/tail10 比较是排序后不同文件；round_secs 在输出前排序；JSON 打开/写出失败不影响最终退出码 | 不同输入难度不能证明生命周期退化；时序和证据写出可能失真，不据此报告优化效果 | U00c/U13 |
| F20 | P1 | run-targeted.py::run_process 超时后第二次 communicate 无 timeout；terminate_process 只等父进程；record_process 在运行后才算 binary hash；command_environment 继承全部环境 | 父进程退出、后代仍持管道时存在无界等待路径；事后哈希与 HEAD 不足以证明实际执行身份；复制目录不等于 OS 沙箱 | U00a/U00b |

**不认定为冗余的检查：** owner/epoch/destruct、TLS hostname/信任链、最大字符串/数组/解压/编译深度、跨线程访问、动态 efun 类型、回滚清理、shutdown 排空、sandbox marker。需要删除一条检查时，必须证明所有生产调用路径在同一不变量下已经保证它，且检查之间没有 LPC callback、解锁、await 或对象状态变化。

### 4.1 其余模块的必查轴（不冒充已发现缺陷）

以下属于 U11/U15 的明确任务范围。每组都要记录“调用者/不变量/失败路径/现有测试/上游剩余差异/处置”；没有差异可以保留现状，不能为凑整理数量改代码。

| 模块 | 必查内容 | 门禁与归属 |
|---|---|---|
| backend/comm、heartbeat/callout | 队列重入、预算、取消、对象销毁、shutdown；命令和事件顺序 | legacy 输入/定时事件与 owner 分派比较；U11/U12 |
| apply/interpret/function/program | 栈平衡、引用转移、异常展开、默认参数、继承、funptr 身份和 stale program | 实际 LPC + C++ 白盒，含 Release；U11/U13 |
| array/mapping/class/buffer/svalue/stralloc | 深层释放、别名、容量/溢出、字符串 EGC 缓存、debugmalloc 与实际所有权 | 深度/边界/失败输入，ASan/UBSan；U11 |
| load/destruct/replace/recompile | blueprint/clone、活跃 frame、create 错误、rollback、事务中新生引用 | 复用既有 I15/E 设计；不重新允许活动 master 被重编译；U11 |
| Promise/async/external/DNS | frame-owned 参数、取消、超时、回调清理、子进程身份、输出排空、owner admission | 无后台 LPC ref 变更；U04/U11 |
| parser/parse_command | 回调删除规则或销毁对象后的遍历安全、中文高位字节、重入和状态恢复 | 保留延迟释放边界，已有 parser 回归；U11 |
| PCRE/regexp | legacy flags、缓存键、独立 match state、callback、限额和异常释放 | 不通过放宽 UTF 校验或启用 JIT 来“优化”；U11/U15 |
| DB | 结果/连接生命周期、异步执行、错误传播、包组合和线程使用 | 真实隔离数据库才证明 DB 行为；无环境明确未验；U04/U11 |
| crypto/sha1/uids/dwlib | 既有 hash/crypt 兼容、权限 hook、默认包可用性 | 不因算法旧就删 efun 或改旧口令格式；新增安全替代单独设计；U11/U15 |
| JSON、sprintf/sscanf、contrib/trim/math | 格式/类型转换、Unicode、浮点边界、结果大小、正则与格式字符串输入 | 原合法格式/返回/错误合同保持；U11 |
| gateway wire/session/owner service | 缓冲上限、结构验证、消息顺序、重试/背压、失效 handle、权限意图 | 不删除消息边界校验；U12 |
| TLS/WS/socket/transport | 证书与 SNI、部分写、分片/关闭、事件/ctx 生命周期、fd 重用、代理身份 | U02/U03/U09；真实 loopback 握手 |
| tools/lpcc/lpcshell/转换工具 | 退出码、stdout/stderr、编译诊断、源编码、数据格式、缺包行为 | 工具生成物不回写源码；U11/U13/U16 |
| CMake/CI/vendor | 可选包、静态/动态链接、生成器缺席、平台退出、编译选项和许可证 | U06/U15；不默认触发外部 workflow |
| 文档、样例、测试数据、开发脚本 | 可执行路径、失效说明、危险默认端口、测试污染、依赖版本清单 | U10/U11/U16；文档检查不能被误用为运行测试授权 |
| 时钟/随机/平台边界 | deadline 的时钟来源、时间跳变/溢出、随机种子可重放、signed char/整数窄化、locale/TZ、大小写与长路径 | 不假定跨平台等价；U06/U11/U15，实际不支持的平台不虚报 |
| 压力与关闭 | 慢消费者、长期背压、重连/取消、fd 继承、信号中断、shutdown 中仍有工作 | 固定容量合同、EINTR/EAGAIN 分流与清理责任；U01/U03/U04/U12/U17 |
| 路径与文件身份 | 同文件/硬链接别名、符号链接、临时文件冲突、stat→open 的时间差、失败后权限与文件保全 | 隔离夹具验证，不能自动引入全局禁 symlink 新策略；U01/U05/U11 |
| 验证基础设施 | 用例发现与完成、异步屏障、随机顺序重放、二进制/输入身份、证据写出和子进程清理 | 基础设施本身要有反例和真实进程验证；U00a–c |

## 5. 上游逐项处置

### 5.1 795eb371 之后的 15 个提交

SHA 是固定补丁中的真实提交前缀。A=已有本地等价核心，B=需移植/适配，C=功能或策略改变须兼容决策，D=不直接纳入；A 不等于本次已经运行验证。

| 提交 | 主题 | 处置与本地落点 |
|---|---|---|
| 3f457bb1 | private inherited alias/prototype 两个回归 | A：HEAD 5c71593a 已含 compiler 与夹具；U00/U11 复核覆盖，不再次叠加 |
| de11656d | Unicode explode 子区间不再反复扫描 | B：U07，EGCIterator；保留本地错误与 ICU 生命周期 |
| f3b8030e | TCP text UTF-8 跨读取拼接 | B+C：U02b 移植 carry 思路；不自动采用 EOF 丢弃残片策略 |
| e92b5cf2 | include_list 路径标准化 | B：U05，从上游 lexer_utils 适配到本地 lex；valid_read 拼写不变 |
| a4fc80c5 | async callback 锁顺序/active 请求标记 | A：本地已解锁后回调并持有 active_callbacks；U04 补回归，不整包替换 |
| d48a03d8 | UTF-8 和 nested async 端到端测试 | B：U02/U04 吸收测试场景；用既有隔离 runner 和 Python loopback，不新增默认 Node 测试依赖 |
| b2ba3f8f | I/O 错误、WS/matrix 去重、安全检查、测试 | B+C：必须按 §5.2 拆分，不能一次 cherry-pick |
| 2c93fd86 | 无 sockets 构建、无 global include | B：U06，保留 external 独立使用场景；关包不是关闭回归的理由 |
| 01026e1f | docs npm 更新 | D：U15 按本地 VitePress lockfile 独立审查，不覆盖上游 lockfile |
| 91de2eaf | WS/测试 Coverity 修正 | B：WS 内容随 U09；测试内容与后续删除一起看，不复制随后废弃的测试 |
| 1b921bb3 | 拒绝无法解析的 X-Real-IP | C：U09 先固定现有代理配置/信任边界；未经批准不改变代理接受策略 |
| 4dd319f0 | trace 日志回主线程，删除 rc/tracing 单测 | B+D：U03 吸收线程修复；测试按本地 driver 隔离 fixture 保留/重写，不直接删覆盖 |
| 56964d9d | 静态 float→int 截断警告 | C：不默认启用，避免旧 mudlib 的 warning/error 策略变化；U14 只登记 |
| 5e138c3c | trace 写出错误处理补充 | B：与 U03 合并审阅最终版本，不能停留在 b2ba3f8f 的中间实现 |
| 328aaf10 | stats 文件不存在不报警 | B：U01d 区分首次运行 ENOENT 和真实读取失败，不屏蔽全部错误 |

### 5.2 b2ba3f8f 必须拆成的子项

| hunk 类别 | 动作 | 兼容约束 |
|---|---|---|
| write_file/write_bytes、compress/uncompress、ed、save/restore、file_length | U01a–c 逐项移植错误传播和资源关闭 | 保持正常返回/append/覆盖语义；失败时不误删源文件；不假称整个写操作原子化 |
| external spawn、symbol、lpcc、mudlib_stats | U01d 逐项核现有等价逻辑后适配 | 保留外部进程 handle/cancel/owner 合同与 CLI 退出码 |
| socket_get_option | U02a 修复栈槽位与结果交付 | 公开签名和类型不变；invalid fd、closed fd、unknown option 各有反例 |
| socket owner 与 TLS 配置校验 | 与 getter 分离审查 | 已有 TLS fail-closed 不能回退；新增 owner 限制须先确认 master 授权合同，不能偷改合法跨对象管理行为 |
| tracing | U03 取后续修正后的最终意图 | worker 只写出/返回结果，主线程日志；不能让持锁 join 与 worker 回调互相等待 |
| ws_common / X-Real-IP | U09a 只提取等价公共逻辑；U09b 再审策略 | 生命周期和信任策略分开提交；不新增长期重复实现 |
| matrix | U09c 提取实际共用的类型/长度读取及计算 | 维度、顺序、返回类型、错误位置/类型不变；不顺手改变浮点求值次序 |
| base file path/rename、EXDEV | U01c/U11 的故障分支验证 | 不用真实挂载点或真实业务文件注入跨设备错误；不抄不适合本平台的链接器技巧 |
| LRU/base/file 测试、rc/tracing fixture | U11 按实际发现集合接入 | 最终上游删除 rc/tracing 单测说明中间 fixture 有问题；不能把“测试来自上游”当有效证据 |
| AGENTS、format、根 CMake、文档 | 逐项评估，默认不替换本地工程规则 | 本地 scope/隔离/生成链/并发限制优先 |

### 5.3 旧基线之前的有价值差异

旧方案 §2 有 v2026.0901.0..795eb371 的 38 提交账本；其中 F/G/D1、Windows 生命周期、private slot 等已有后续实施记录。新执行者必须从 HEAD 重新核对，不能根据旧表的“尚缺”重做。

| 系列/入口 | 当前判断 | 本轮动作 |
|---|---|---|
| 51888b07、bf21cb1e、9ac79102、1a7a76de | 语法规则拆分、Flex、push frontend 的实质变化尚未整体吸收 | U13 先做等价拆分与状态整理；U14 再做可终止的迁移可行性单元 |
| 738c00d3、c48ce9ef、1cf1dc7b、b550ccf0 | 去掉 LexStream 包装、统一 CompileState、预处理/扫描器状态和零拷贝 | 借鉴减少状态副本的思路；不得把 scanner reentrant 误解成整个编译器可并行 |
| 13b0fcf1、9fba6cd3 | arena、typed value stack、编译测量 | 本地已有独立 ScratchArena/诊断 arena；U13 先核生命期，不重复建立第三套 arena |
| 460d2e38、a78f25ee、a0f6c745 | staged compiler、语言机器合同与工具 | U14/U16 评估内部测试和文档复用；新增 CLI/工具不是保持兼容所必需 |
| f79c309b、55e47edc、9be761aa | 新语法/字面量/FFI | 有功能价值，但会改变语言或安全表面；登记为本轮默认不实施 |
| 24211e79、3b505bef、968205b3、932e9309、134bebd7 | 字符串、private slot、算术、foreach 修复 | 核当前实现和测试，不因历史不同 SHA 就重复移植；Unicode 新尾项由 U07 补齐 |
| d140ec28、03354159、5270e7d6 | PCRE2 后端和清理 | 本地 D1 已完成；旧 numeric flags 保留。D2 新 API 不自动开放 |
| 795eb371 | named-function 稳定身份 | 依赖本地 recompile/owner/重复继承；按旧方案 E 的身份/journal 设计单独评估，不覆盖 function registry |
| 6e56d4e9 | WSS 证书热更新 | 当前仍是独立高风险功能；复用旧 H 设计，需单独批准和 live-WSS 证据 |
| Promise cancel / 外部进程系列 | 本地与上游状态、body-owned、handle 和取消语义存在差异 | 保留本地合同，逐 hunk 检查清理/错误修复；不移植新状态模型 |

所有历史提交仍有价值的剩余 hunk 由 U11 的机械清单归档到对应单元。处理状态只能是“等价且有证据 / 已适配且有证据 / 不适用并有理由 / 被兼容决策阻塞”；“提交很旧”“diff 太大”“上游已测试”都不是结案理由。

### 5.4 上游代码规范专项补充

已深读固定上游的 .clang-format、.clang-tidy、.editorconfig、LPC style-guide/formatter、CI、format-corpus 入口及 package/VM 栈、头文件约束；对照 ws_common、matrix、tracing、grammar_rules_exprs、test_strutils 的实际写法，并与本地 package_api、efun dispatcher、owner coordinator 和编辑器配置核对。

主要决定及来源链接保存在[代码规范 §2](coding-standard.md#upstream-basis)，不在本计划复制完整规则。重要结论：

- 原生代码采用统一的固定排版、snake_case 函数、PascalCase 类型、kPascalCase C++ 常量；保留公开/ABI/VM 旧名称，不建立别名层。
- LPC 有独立语法和排版规则，不能把 .c LPC 当 C++。从上游适配 formatter 必须验证本地 grammar、stringize、源码编码和位置语义。
- 工具检查排版、人工审查生命周期/抽象，实际 driver 验证行为；任一层都不能替代另一层。
- 上游仍有规则与实现矛盾，不能把“上游这样写”当作豁免；本地不再允许新代码自由跟随旧文件的任意风格。

本轮仅新增规范、更新本计划。本地未发现 clang-format/clang-tidy；没有安装或运行 formatter，未修改配置、源代码或 CI。配置解析、工具自测和自动门禁的真实通过证据由 U10 实施取得。

## 6. 执行规则与顺序

### 6.1 授权后的一次性进入检查

以下只读检查不配置 CMake、创建 sandbox、安装依赖或运行 driver。先检查仓库、info 和全局 attributes 是否启用外部 clean/smudge/diff 过滤器；有外部过滤器时先核其副作用，未知则改用原始 Git blob 与文件字节比对，不把 git diff 当作天然无执行路径：

~~~bash
export GIT_OPTIONAL_LOCKS=0 GIT_NO_LAZY_FETCH=1
export PYTHONDONTWRITEBYTECODE=1
git -c core.fsmonitor=false status --short --branch
git -c core.fsmonitor=false rev-parse HEAD
git -c core.fsmonitor=false diff --no-ext-diff --no-textconv --check
git -c core.fsmonitor=false diff --no-ext-diff --no-textconv
git -c core.fsmonitor=false diff --cached --no-ext-diff --no-textconv --check
git -c core.fsmonitor=false diff --cached --no-ext-diff --no-textconv
git -c core.fsmonitor=false ls-files --others --exclude-standard -z
~~~

HEAD 或相关文件变化时，先刷新受影响事实和 hunk，不机械套本文行号。不清理、stash、reset、暂存或覆盖其他人的修改。首轮落盘新增本文；规范补充只更新本文并新增 docs/coding-standard.md。执行者不能把这些文档当成未归属脏改动自动删除。

### 6.2 单元规则

1. 读本单元归属文档、实现、全部直接调用者和现有测试；新增/修改代码遵守[代码规范](coding-standard.md)。
2. 对已确认缺陷写最小反例，旧实现必须在目标断言失败。若缺陷本身是编译失败、死锁或无界等待，允许分别以匹配的编译诊断或有界 watchdog/线程证据作为预期失败；必须证明已经到达目标路径。缺工具、0 测试、skip、无归因的 timeout 不是反例。
3. 实施本单元允许的最小修改。生成文件先改源头，再按项目固定版本生成。
4. 在新源码构建上跑原反例、正常/边界/错误/恢复路径和所需 sanitizer。
5. 核完整 diff、接口快照、文档和原始证据。通过后才进入依赖单元。
6. 单元失败保留日志；追根因，不连续堆补丁。影响范围变化时先回到只读复核。

纯机械拆分没有行为 fail-first；以拆分前非空发现集合、拆分后同集合/断言数、符号及生成结果一致作为证据。性能单元以基线数据代替伪造失败。已有等价修复（如 U04）的新增回归允许旧实现通过，不能故意改坏代码制造红灯；规范、文档及调查单元使用相应配置/一致性证据。

每项证据记录：HEAD、tracked dirty diff 与相关 untracked 文件清单/哈希、源文件模式及字节、相关配置/二进制/动态依赖哈希、工具链版本、精确命令、退出码、发现/必需/开始/完成/通过/失败/跳过数、时长及日志路径。HEAD 或 git diff 单独都不覆盖新测试文件；执行前后身份必须一致。详见 U00b，不得把旧二进制或变化中的输入跑绿作为当前源码证据。

### 6.3 依赖图与交付块

~~~text
U00 可信基线：U00a 工具自测 → U00b 初次构建 → U00a 真实完成屏障复核 → U00b 基线验收
  ├─ U01/U02/U03/U04/U05/U06：正确性与兼容修复，彼此按共享路径约束
  ├─ U10a–c：规范/配置/checker 小范围试点，不阻塞独立 P0/P1 修复
  ├─ U11a：上游剩余差异与测试分类（U11 的步骤 1–3）
  └─ U15a：依赖/平台事实与安全分类（U15 的步骤 1–2）
U00c 测量校准 → U07/U08 与后续性能证据
U11a → U11b 测试适配（步骤 4–6） → 对应模块语义回归
U10a + U11a → U10d LPC formatter；不等待 U14
U00 + 对应回归 → U09 等价去重、U10e 测试拆分
U03/U04/U08 + U10e → U12 owner/gateway 整理
U05 + U10e + U11b 编译器覆盖 → U13 编译器等价整理
U13/U11b → U14 评估；不替换也是有效且需记录的决策
对应模块完成语义整理 → U10g 该模块排版（不先格式化即将搬动的大文件）
U10c/U10d → U10f；U15 实际依赖变更回到受影响模块重验
U16 随各单元更新文档；U10/U11/U15 的收口和全部获准单元 → U17
~~~

U11a/U11b、U15a 只是原单元的阶段名称，不另建主计划。缺格式工具只能阻塞相应风格单元，不能挡住已有可靠反例的独立数据安全修复。U16 不提前冻结未来 U14 的结论；U10g 在 U12/U13 移动之后处理对应文件，避免重复格式化。

独立调查可并行；同一时刻只构建一个目录，构建不与 driver/benchmark 并行。源码实现按原子子项交付，不能将 U01–U17 混成一次大提交。提交和外部操作遵守当次授权及工作区归属；本次编制方案不执行这些操作。

## 7. 逐单元实施卡

下列 U00–U17 的执行状态以 §11 的实际记录为准，未记录完成的项仍待执行。标为“新增”的文件/用例是设计目标，不能在创建及验证前声称其命令已通过。路径用相对仓库根表示；符号优先于会漂移的行号。

### U00：可信验证基础设施与兼容基线

前置：批准实施；工作区归属清楚。范围限现有 runner、自测、testsuite 调度、测试/基准目标、接口清单；不先修改生产语义。U00a 的纯工具自测先执行，真实 driver 部分在 U00b 初次构建后完成；基础设施未通过前，不签收后续单元。

#### U00a：修复验收中的假通过和无界等待

修改 tools/testsuite/{run-targeted.py,test-run-targeted.py}、testsuite/{command/tests.c,single/master.c,single/tests/efuns/shutdown.c} 及需登记完成的异步夹具；保持现有命令入口，复用而非另造 runner/master。

1. GTest 按同一 filter/环境核发现的必需用例、实际 RUN 和完成集合；禁止少跑、重复、部分跳过或计数矛盾仍通过。DISABLED/包缺席/平台不适用在选择前分类并记录理由，不从执行失败或 skip 倒推为不适用。
2. CTest 先判 timeout/非零退出，再核真实 Not Run/Skipped。每个用例的结果须可追溯；外层 CTest pass 不等于内部断言覆盖。C++ 精确验收优先 --binary；无法证明内部执行时只记录调度结果。
3. LPC 移除无记录时补 passed=1 的路径。区分 A> 预期编译失败、B> crasher 和 C> 普通用例；原 C> 保留为开始标志，新增明确完成记录。核预期身份集合与实际完成集合；空 do_tests、包关闭分支不算功能覆盖，不能仅数日志字符串。
4. 在 tests.c 建立测试内部的 pending/完成登记和失败汇总。异步用例在 do_tests 返回前登记，在最终 callback/Promise 完成及断言后结算；每次登记带用例身份且只能结算一次；未知或重复完成视为 fixture 失败，不能只看一个总计数。退出只能发生在全部必需用例完成、pending=0 且无失败之后。更新 shutdown 夹具，不以 15 秒定时器或立即 shutdown(0) 代替屏障；晚到错误必须使本次失败。未登记的异步场景不能计为已覆盖。
5. watchdog 保留并设置总期限。进程退出、管道排空、后代清理分别有期限；第二次 communicate 不得无限等待。原始 stdout/stderr 字节保存到本轮日志，解码副本用于判定；不能用 errors=replace 后的文本冒充原始字节。父进程提前退出、后代持 stdout/忽略终止、输出量过大和用户中断均需保留失败证据并结束本任务持有的进程树；不杀不相关 PID。不具备进程树约束的平台明确阻塞该测试，不能把进程组当作所有逃逸进程的隔离保证。
6. 自测反例包括：2 个发现只运行 1 个、1 pass+1 skip、伪成功标志、零用例、晚到断言失败、异步永不完成、父进程先退/后代持管道、记录写入失败。默认每进程日志上限 64 MiB；触顶保留已获原始字节并标资源失败，不截断后判通过。基线合法日志超预算时，在重新执行前调整并登记预算。
7. mock 仅证明解析器/监督器；必须再用真实 driver 验证同步、async 文件和 socket/TLS 各一条“完成才退出”的正例及故意不完成的反例。全量测试记录实际执行顺序；先核现有 random 排序方式，提供测试专用可重放的顺序/种子，不改生产随机 API。

#### U00b：冻结源码、构建、输入和兼容表面

1. 将 §3.2 所有公开表面登记到兼容清单，从 .spec、生成元数据及真实调用者核对。同时记录未知/不适用项、原因及负责人；不把未知误写成“保持不变”。
2. 冻结实施起点的完整源码身份及 mudlib/语料：tracked 内容、相关 untracked、模式/链接/编码属性、配置和工具版本均记录。baseline 构建目录不是源码快照；须有获准的独立源码副本或可重建的精确源码身份，禁止在源码变化后重建目录而仍称 baseline。
3. 在本轮拥有的 build-organize-baseline-debug 构建 driver/lpcc/lpc_tests/ofile_tests/compile_arena_tests；候选只用 build-organize-debug 等另一组目录。性能另建相同优化配置的 baseline/candidate Release 目录，不拿 Debug 二进制充当基准。记录包开关、实际库路径及版本。CMake 的 GTest discovery 会执行二进制，属于构建副作用；WSL2 的 TSan 包装必须覆盖该阶段。
4. 源码、生成物、CMakeCache、编译/链接选项、二进制和动态库身份在构建/运行前后核验；禁止与同目录构建并行运行。二进制先算哈希，再运行并复核；运行后单算哈希不证明实际加载身份。对子进程显式传递已登记的必要环境，凭据类和未知外部配置不透传；测试需要的 locale/TZ、sanitizer、工具路径等保留为输入，GTest 分片/重复/过滤环境不得暗改执行集合。这不改变生产 get_os_env 合同。证据记录器只记录必要键及工具身份，不转储含秘密的全部环境。
5. 增加 testsuite/single/tests/efuns/modernization_compat.lpc（新文件），覆盖返回/异常、flags、source-spelling、save/restore 和基本 callback；复杂场景保留独立 fixture。原基线语料与新增缺陷反例分开：旧实现预期失败的测试不能混入“原行为基线必须全绿”。
6. 运行已有 runner 自测和一次基线 C++/LPC 集合。先完成 U00a 的真实 driver 复核；每个已有失败独立归因。只阻塞依赖其正确性的路径；没有证据证明独立时不继续，不能拿历史失败豁免最终验收。
7. 下游样本由用户提供获准副本或合成脱敏样本，不自动抓账号/密钥。覆盖 master/simul、继承/clone、登录/命令、定时事件、持久化、网络/DB/外部进程和热更新；明确哪些真实能力缺席。

历史行为基线与原子单元基线分开：每个修复还要与紧邻的已通过状态对照，避免把前一单元收益算给后一单元。验证基础设施修复可以供 baseline/candidate 共用，但必须相同版本且不偷偷携带候选生产修复；无法回编相同 harness 时不能声称严格 A/B。

#### U00c：先校准测量，再签收优化

范围 src/tests/bench_compile.cc、实际消费其输出的 tools/perf 脚本及相应测试；仅在性能单元前要求通过，不阻塞 U01–U06 的独立修复。

1. 生命周期比较改为同一文件/固定同构工作量跨轮次对照，保留文件身份、原始时序、各轮样本和成功/失败数。排序只能作用于统计副本。不同文件的 head/tail10 只能描述输入分布，不再作为退化门禁。
2. 用现有 JSON 库或已验证的序列化能力正确转义路径；输出打不开、写出/close 失败、缺字段、非有限数和样本不全均不得签收性能证据。先查 stdout/JSON 的所有消费者；旧字段不能未经兼容结论删除或重新定义。
3. 分别标明 arena chunk malloc、总分配、retained bytes 和进程 RSS；没有采集总分配就不声称总分配下降。当前 bench_compile 以 compile_file 为主，不证明 loader/inherit/retry 的端到端收益，后者用真实 driver workload 补证。
4. 以不同难度输入、恒定跨轮输入、故意失败编译和不可写输出做反例；工具自测后再跑真实编译基准。7 组 A/B 用于运行间噪声；p95/p99 必须来自明确数量的操作样本，不能用 7 个轮次包装可靠尾延迟。

U00 出口：必需测试发现及完成非空且一致；原基线、反例和候选身份不混淆；证据与退出清理可信。基线建立可记录已定位的旧缺陷，但 runner/完成屏障本身必须通过；旧缺陷影响的路径仍标 blocked，不能因已记录就算运行通过。下游缺席标 external-required，不阻断独立内部单元，也不能据此结案“所有原 mudlib 完全兼容”。

### U01：文件与资源失败路径（四个原子子项）

范围：src/packages/core/file.cc、ed.cc、compress/compress.cc、contrib/contrib.cc、mudlib_stats/mudlib_stats.cc、external/external.cc、src/vm/internal/base/object.cc、src/symbol.cc、对应文档和测试。禁止同时重写 source-spelling 或存档格式。

- **U01a write/ed：** 核 fwrite/gzwrite/ferror/fclose/gzclose 的成功判据，短写、延迟 flush 失败均不得报告成功；ed 失败不得清除“已修改”状态或丢弃 buffer。维持原返回码/错误入口，不统一成新异常体系。
- **U01b compress：** gzread < 0 与 EOF 分支分开；检查每次写入和最终关闭。输出确认成功之前不删源文件。成功后是否删除源文件沿用原合同，删除本身失败也要按现有错误合同报告。补同文件/硬链接/符号链接别名测试，确认目标写入不会先破坏源文件；不能用有 TOCTOU 间隙的路径比较宣称已经解决文件身份问题。对既存目标不承诺无损覆盖；若目标打开后已被截断，应明确失败语义，不随手 unlink 可能属于用户的旧目标。
- **U01c save/restore/rename/file_length：** 沿用既有 tmp→rename 流程，失败先释放句柄再抛错；读取循环区分 EOF 与 ferror。正常覆盖、权限 hook、路径拼写不变。额外目录 fsync、权限保留或完全原子替换是新语义，未经单列设计不附加。
- **U01d peripheral：** 核 spawn 返回的真实错误码和 socketpair 失败；本地已处理的 hunk 不重做。stats 文件不存在不警告，但其他读取/写入错误必须可见；symbol/lpcc 不改变已有退出码合同。

反例：扩充现有 compress_file.c；新增 uncompress_file_corrupt.lpc、file_io_failure.lpc；native 故障注入覆盖短写、close 失败、rename 失败、ferror、输出打不开和清理再入。新增名称若与 U11 适配测试重合，使用同一个文件，不能复制两套。

故障注入要在读取、写入、flush/close、rename、删除各失败点核所有权和主错误保留；重试仅用于合同允许的 EINTR/EAGAIN，不能盲目再次关闭可能已被复用的 fd。注入只作用于测试编译目标或夹具私有依赖，默认不改变生产逻辑；禁止填满真实磁盘、修改真实权限树或删除业务文件来复现。实现注入前需核现有测试能力；不得新增能被生产配置开启的“强制成功/失败”后门。

验收：旧实现命中对应反例；修复后正常往返/覆盖与失败保源同时通过；ASan/UBSan 无诊断；资源数和存档格式不回归。只对获准的隔离测试文件执行删除/覆盖。

### U02：socket 参数、所有权审查与 UTF-8 分片

前置：U00。范围：src/packages/sockets/{sockets.cc,socket_efuns.cc,socket_efuns.h}、直接 callback 路径和网络测试。

**U02a getter：** 从 sp-1 读取 fd、sp 读取 option；在消耗参数前取得需要持有的返回值，最终用项目现有 replace/pop/push 模式留下且只留下一个结果。验证字符串引用不悬空。不要只改两个偏移而遗漏返回值被 pop 的问题。

用例（新增 socket_get_option.lpc + native 栈哨兵）：不同 fd/option、int/string/未设置值、非法/已关闭 fd、未知 option、调用前后的其他 LPC 局部值及多次调用。setter/getter 类型和当前 master valid_socket 行为不变。

**U02b text 分片：** 为每个 socket 保存至多 3 字节不完整 UTF-8 尾部；仅 STREAM/STREAM_TLS 文本路径使用；连接初始化、clear、关闭、失败和 fd 重用时清零。BINARY 模式按原字节交付，不 sanitize。复用已有 u8_incomplete_tail，不复制 UTF-8 解码器。

测试必须用真实 loopback TCP 和 TLS，覆盖 2/3/4 字节字符各位置的读取切分、连续 carry、ASCII 混合、非法序列、关闭/重用和 callback 销毁 socket。peer 的分次 write 或 sleep 不保证接收端分次 read；须用测试专用观测/受控握手证明实际读取边界，未观测到的分片不算覆盖。共享 helper 单测不能代替端到端读路径；完成以 U00a 的异步屏障为准。

**EOF 残片决策出口：** 上游选择丢弃未完成序列，本地旧代码会产生替代字符；回调次数/时机也可能被改变。先测并登记两侧观测。默认不静默采用丢弃：若当前合同未规定，单列策略让用户批准。carry、EOF、读错误、本地 close 和回调重入是同一状态机，不能先合入 carry、把 EOF 留空；未决时阻塞整个 U02b，U02a 与其他独立单元继续。

**权限子项：** owner 校验与 getter 修复分开。枚举 valid_socket/master 授权、socket_release/acquire 和跨对象管理消费者。保持已授权合法场景；任何新增拒绝必须有单独兼容结论。TLS 已有非空 SNI/hostname/verify 约束绝不能回退为上游较弱实现。

验收：U02a 栈深度/结果精确；U02b 完整合法文本无替代符损坏、binary 字节一致、TLS 验证保持；ASan/UBSan，触达共享状态再加定向 TSan。

### U03：trace writer 与日志线程边界

范围：src/base/internal/{tracing.cc,tracing.h,log.cc,log.h} 及 main/backend 的实际收尾调用者。

1. 建立 driver fixture 下的 trace start/stop/flush/退出反例，覆盖写出成功、目标不可写、JSON/分配异常和未收集事件。
2. 以 4dd319f0 + 5e138c3c 最终逻辑为参照，让 dump worker 只拥有已移出的事件与输出参数，并返回有界结果；debug_message 和日志句柄操作只在 main thread 进行。
3. 复用既有 main-thread wakeup/drain 或现有主线程收集点；不得新建第二套日志线程池。清楚定义 shutdown 时最后一个结果由谁处理。
4. 审阅持锁 flush、Tracer::stop、析构 join 的调用图；不能让主线程持锁 join，而 worker 为返回结果等待同锁或主线程。
5. 保留 trace 文件格式、时间/事件语义和启停 API。只改失败报告位置与所有权；不得吞掉输出失败。

验收：TSan 下 flush/stop/退出无数据竞争/死锁；真实 driver trace 可解析；失败能由主线程报告。上游删除独立 tracing 单测不是本地删除覆盖的理由。若原 fixture 无正确 VM 初始化，修 fixture。

### U04：async 已覆盖修复的反例补齐

范围：src/packages/async/async.cc、testsuite/single/tests/efuns/async.c 与新 async_nested_callbacks.lpc。

1. 确认 check_reqs 的 callback 外无 finished 锁，active_callbacks 的生命周期覆盖清理和 debug mark；所有异常路径也必须移除 active 记录。
2. 补 callback 内再次提交 async、callback 内 check_memory、callback 抛错/销毁/重编译拒绝、shutdown 排空的测试。
3. 无需 DB 的 async 文件工作覆盖主要锁关系；MySQL 特有路径只在获准的隔离测试库验证，不访问真实游戏 DB。
4. 旧实现若全部通过，不为“吸收提交”修改生产代码。若测试揭示差异，保留证据并限定到具体清理/锁路径。

验收：嵌套回调有确定完成标志和有界等待，active/queued/引用计数回到基线；TSan + Debug memory check。DB 缺环境时只将对应子项标外部待验。

### U05：include 路径、长度和权限拼写

范围：src/compiler/internal/lex.cc::init_include_path/inc_open、include_list 记录、master fixture。保留现有 LexStream/arena。

1. 扩充 include_list.lpc 和 get_include_path 场景：绝对/相对路径、有/无尾斜线、:DEFAULT:、拒绝权限、长目录/文件名、嵌套 include。
2. 先记录 valid_read 接收的路径字符串及调用次数，固定其现有合法行为。
3. 在成功打开并记录 current_file/include 表的边界规范化展示路径；不要提前剥掉 master 提供的斜线或改写授权输入。
4. 将已证明可能越界的固定缓冲区拼接改为本模块现有有界字符串方式；拒绝超过现有容量合同的路径，不扩大最大对象/源文件长度。
5. 多余分隔符的拼接优化若改变授权 hook 输入，独立评估而不是与展示规范化混做。

验收：include_list 返回的路径可按原权限 stat，原权限拒绝不绕过，长路径不会越界；Debug/ASan/UBSan + 编译失败后再编译成功。只更改 lex 不要求无关 grammar 重生成。

### U06：配置、可选包和 sanitizer 有效性

- **U06a**：在新构建目录覆盖 PACKAGE_SOCKETS=OFF 且 PACKAGE_EXTERNAL=ON/OFF。若 external 只需要 socket 错误常量，将头文件依赖放到真正消费者，不能让 package_api 全局重新暴露全部 sockets 实现；所有 socket 清理/mark 调用按真实包边界处理。保留缺包 efun 诊断。
- **U06b**：global include 缺省/空/带引号/尖括号各有实际启动+编译测试；空值保持“无 include”，不要合成为空文件名。错误值仍明确报错。
- **U06c**：GCC 现用版本与 Clang 分别在有/无 null 检查下运行最小 TLS/VM 初始化反例。若只有特定 GCC 组合重现，才缩小 -fno-sanitize=null 的条件；Clang 验证成功后恢复其检查。若新证据表明不是误报，先修真实根因，不重新扩大抑制。

- **U06d 生成链：** 核 src/tools/CMakeLists.txt 的 make_func.y→make_func.autogen.cc，以及主 grammar.y→grammar.autogen.cc/.h 两条 Bison/fallback 链；不是只查主 grammar。当前两组 fallback 标记 Bison 3.8.2。固定实际生成版本/参数/相对路径，在两个私有树重复生成并比较；配置无 Bison 的独立目录（CMAKE_DISABLE_FIND_PACKAGE_BISON=TRUE）验证真实 fallback 构建和同一语义用例。还要核 .spec/applies/options 的依赖，修改源输入后增量构建必须重生成；构建树产物不得手抄回源码作为修复。

范围：src/CMakeLists.txt、src/tools/CMakeLists.txt、src/base/package_api.h、rc.cc、external CMake、具体受条件编译影响的 object/simulate/checkmemory 路径。新建独立构建目录，不清现有缓存，不因包关闭跳过相应配置验收。生成文件更新使用经核验的显式生成步骤；入口缺失时先补唯一入口与自测，不手改生成正文。

验收：四种 sockets/external 组合的目标构建和缺包诊断；空 include 的启动/编译；Debug 与 Release 都通过；sanitizer 实际参数与报告覆盖相符。记录缺席平台，不把配置成功当作运行成功。

### U07：Unicode explode 重复扫描

范围：src/base/internal/EGCIterator.h、explode 调用者及字符串测试。

1. 借 de11656d 对已知有效子区间保留原 ASCII/ICU 路径；非 ASCII 后缀变成 ASCII 仍可保守用 ICU。
2. 每次新子区间重置/初始化对应 ICU 文本状态，不能复用旧 BreakIterator 的 offset。
3. 保留指针范围、负长度和跨 buffer 的边界；不要直接做无关对象指针相减。
4. 增加 Unicode、组合字符、emoji、CRLF、空字段、多字节分隔符及跨 buffer reset 测试。

验收：结果逐字节相同，ICU 持有资源正确；ASCII 和 Unicode 分别测 1k/10k/50k token，记录分类扫描工作量和时间。只有重复扫描计数下降且结果一致，才报告该优化完成；不能套用上游毫秒数。

### U08：object store 汇总路径降复杂度

范围：object_store.cc 的 summary/lookup/resolve/status 及其直接调用者；不删除 ObjectTable 或公开 bridge 状态字段。

1. 先分别测有效 handle 快路径、缺失/陈旧 handle、按 path 查找和状态汇总。记录 object/shard/destructed 数量、全表 scan 次数、锁持有时间。
2. 构造 1k/10k/50k 对象和相同比例 tombstone；增加 epoch 错、对象迁移、缺少局部记录、索引不一致的白盒反例。
3. 第一子项只消除 summary 内“每条局部记录再扫全局”的重复遍历。可在同一读锁范围构建一次只读 ID→record 临时索引并查询（实施前核实际锁是否允许分配，以及所有 record 指针在此范围内的生命期）；不新增长期权威目录，不缓存悬空 record 指针。重复 ID 等异常状态必须与原遍历诊断一致，不能由 emplace 覆盖后掩盖。
4. 比较 summary 所有字段和诊断结果，而非仅“found”。分配失败不能留下半发布状态；若临时索引成本无收益或改变失败合同，保留测量结论，改为另行审阅无新增分配的双向核对算法，不叠加长期缓存。
5. 第二子项只有在测量确认后才将诊断汇总从不需要它的内部路径分离；不得让 stale/owner 检查变成“仅调试检查”，不得把 dirty flag 或版本缓存当成已证明的一致性。

验收：结果/公开字段一致，所有生命周期反例通过，TSan 无新增竞争；有 N 扩大时工作量由二次向线性变化的计数和 Release 基准。有效 handle 快路径不能因本次优化反而增加扫描/分配。

### U09：有证据的 WS/matrix 去重

**U09a WS 等价提取：** 新增 src/net/ws_common.{h,cc} 或在现有最合适的内部模块承接共同逻辑，二选一后只保留一份实现；CMake 注册随同修改。先提取纯参数/地址处理，再处理共同关闭步骤，不一次搬完整 callback。

必须保留：WS ascii/telnet 差异；telnet option table 和 WS 禁 MCCP；LWS_PRE/队列所有权；TLS 的 lws_write 返回值可能大于 payload，按请求 payload 长度出队；driver 主动关闭要等尾部消息 flush；session detach/destruct 回调重入。

**U09b 代理策略：** 无 X-Real-IP、合法 v4/v6、无效值、直连伪造 header、现有受信代理分别测试。未明确受信代理合同前，只修解析器资源/长度问题，不自动采用新增拒绝策略。

**U09c matrix：** 将重复的数组形状和数值读取移到一个内部 helper，共同变换只提取已完全相同的算式；保留错误参数号、矩阵维度、结果顺序和原浮点计算顺序。NaN/Inf、int/float 混合及坏类型均比较前后行为。

验收：真实 loopback ws/wss + ascii/telnet + close/burst/MCCP 组合；类型反例和矩阵数值回归；ASan/UBSan。借上游 helper，不借其更弱或不同的生命周期策略。

<a id="u10-style"></a>

### U10：统一规范、格式检查与测试结构

**唯一规范：[代码规范](coding-standard.md)。** 本节只定义推广顺序、文件改动和验收，不再维护另一份排版/命名规则。U10a–c 在 U00 基线后尽早与独立正确性单元推进，不作为全部 P0/P1 的前置条件；U10d–g 随相关模块逐步推进，不能把规则建设拖到整个重构结束。

#### U10a：连接规范入口，消除相互冲突的指导

前置：U00；只改归属文档，不改运行时。

1. AGENTS.md 的修改流程增加 coding-standard.md 链接；CONTRIBUTING.md 的 Code style 引用规范，不再把“随现有文件”作为最终规则。
2. 同步修正 CONTRIBUTING 中已过时的 fork 范围和构建说明，改为引用项目范围、CMakePresets 和隔离测试入口；移除先删除通用 build 再 install 的推荐流程。只改说明，不执行旧命令。
3. testsuite/etc/vimrc 的格式化说明须区分 C/C++ 和 LPC；不得让 .c LPC 自动使用 clang-format，也不强制用户安装编辑器插件或改本机配置。

出口：每个常用入口都能找到同一规范；AGENTS/贡献说明不复制规则表，不再互相冲突。

#### U10b：对齐机器配置和工具版本

允许修改：src/.clang-format、src/.clang-tidy、.editorconfig、必要的 .gitattributes/测试夹具属性；不格式化业务源码。

1. 按代码规范 §3.3 更新 src/.clang-format，修正错误的 LLVM/4 注释。在配置注释中记录唯一机器版本声明 Formatter: clang-format 18.1.8；后续 checker 从此读取，不另建版本常量副本。
2. 从可信 LLVM 发布源取得该版本并核实际版本、来源和哈希。未安装时明确阻塞，不回退 PATH 版本。先用 stdin 和假定源文件名检查配置解析、Type*/Type&、case、lambda、模板、构造初始化、空函数和多行调用；保存实际输出作为 checker 自测样例。
3. .editorconfig 对 Python 显式设 4 空格；保留 Makefile/Windows/CRLF/LF 例外。枚举 GBK、非法 UTF-8、无 EOF 换行和预处理敏感夹具，按字节语义添加精确属性/排除，不能将整个 testsuite 设为不检查。在私有 fixture 验证 working tree→index→重新签出后的字节仍一致；只看当前工作区哈希不能证明 Git 换行转换安全。不得直接对用户索引执行 renormalize。
4. clang-tidy 以实际 C++ compile_commands 为输入；U00 若未导出，仅在本轮构建目录启用 CMAKE_EXPORT_COMPILE_COMMANDS=ON，不顺手改变其他构建。校准 snake_case 函数、class 成员后缀和 kPascalCase 常量/枚举；原公开/第三方/生成符号保留。C11 不硬套 C++ 的命名规则。工具必须使用 18.1.8，并先确认该版本支持所选 naming options。核 GCC/Clang/MSVC compile_commands 的实际参数兼容；不能删除影响宏、include 或 ABI 的参数来让 clang-tidy 启动。缺可用编译数据库只阻塞该项，不把静态分析当已通过。
5. 初始检查集显式列出并逐项验证：readability-identifier-naming、readability-braces-around-statements、modernize-use-nullptr、modernize-use-override、bugprone-macro-parentheses、bugprone-sizeof-expression、performance-for-range-copy。其他现有 bugprone/misc/performance 等检查先在批准的试点目标报告并分类；只能在已核误报、替代覆盖和完整差异后调整，不能通过换一个小列表静默删除有效安全检查。
6. 不执行 clang-tidy --fix，不直接把全部检查提升为全库 -Werror。既有告警记录为基线，新告警逐项处理；不能仅因诊断落在改动行外就隐藏新增问题。

出口：版本与配置可复现；正例通过，故意错误的样例能命中；排除夹具字节哈希不变；新规范与机器配置没有冲突。此阶段不宣称全库已符合规范。

#### U10c：最小只读检查入口与原生代码试点

新增 tools/style/check-format.py 和 tools/style/test-check-format.py，使用现有 Python 3 标准库，不新增常驻服务或格式化依赖框架。实际排版仍由 clang-format 完成，不能重写一个 C++ formatter。

检查器接口固定如下；这些路径/命令在实现前不存在：

- --paths PATH...：检查明确文件；可检查本轮新增未跟踪文件，拒绝不存在、越界、符号链接和语言不明的输入。
- --base COMMIT：检查本地完整 commit 与当前工作区的 tracked 差异；包括 staged/unstaged，不自动 fetch。新增未跟踪文件由 --paths 补充，不能声称 --base 已覆盖它们。
- --all-native：检查全部纳入范围的手写 C/C++，只用于模块整理后的收口或明确授权的全量检查。
- 三种范围互斥；不提供 --write/--fix，不安装工具，不提交，不输出临时源码到工作区，不修改索引。

实现与反例：

1. 从仓库位置确定配置和路径，不依赖 cwd。Git 路径使用 -z 和 argv 传参，覆盖空格、非 ASCII、换行及以连字符开头的路径，不用 shell 拼接。
2. 对新增文件全文件检查；对 --base 的既有文件计算新增/修改行段，用 clang-format --output-replacements-xml 与 --lines 取得格式差异。显式指定仓库唯一配置，不依赖最近目录自动发现 .clang-format。Git 取原始 blob 与工作区字节，不执行外部 diff/textconv/clean/smudge 过滤器或懒获取。删除行需映射到当前相邻语法上下文，不能因没有新增行而漏检。
3. formatter 可能扩展到相邻语法范围；所有 replacement 的真实 byte span 都必须展示，不能静默丢弃越出 diff 行段的结果，也不能自动写这些位置。扩展到无关/并行改动时改为更小的人工编辑后重查。
4. src/thirdparty、生成物、构建产物、原始证据、字节夹具、LPC/语言不明文件在唯一的范围分类处排除；输出每类数量和理由。显式 --paths 输入不可检查时返回错误；--base/--all-native 确实无适用项时输出 not-applicable，而不是 passed: 0。
5. 检查器从 src/.clang-format 读取版本声明并核实际二进制；缺工具/错版本、配置无效、XML 不完整、解码失败、格式差异均非零退出。stdout 给结果与诊断，不在 check 模式写报告或 cache。读取前后校验文件身份/哈希；并行变更时报告失效，不能使用旧差异签收新内容。
6. 自测覆盖这些失败分支、排除范围、rename/delete、stage/unstage/new-file、空集合及幂等；在私有临时 fixture 验证 check 前后文件字节/mtime、Git 索引和配置不变。自测会创建 fixture，不能称为纯只读阶段。
7. 试点先选 source_spelling.cc/.h、一个有真实测试的纯 utility 和一个小型 efun；新命名规则只约束新私有符号，不改现有公开名字。只做明确获准文件/函数的排版；命名、花括号补全、include 顺序或表达式变化分别审阅和测试。

src/format_source.sh 已有入口不能继续作为推荐批处理命令。本轮检查未找到仓内调用者，但不据此断言没有外部用户：新文档只推荐只读 checker；删除/改写旧入口须先完成消费者和权限评估，不新增转发 shim，也不让 CI 执行它。

出口：checker 的完整自测通过，试点实际格式检查通过；故意未格式化内容必须失败；现有定向行为测试通过；未选择文件及排除项不变。

#### U10d：LPC 规范与受控 formatter 适配

前置：U00、U10a；U11 提供本地语法/测试分类。无需等待或先实施 U14 的 Flex 替换。

1. 从固定上游移植 tools/lpc-syntax 中 grammar contract、tokenizer、formatter、format-corpus 及所需测试的最小依赖闭包；保留来源/许可证。按本地 grammar/预处理实际语义生成/适配，不直接使用上游 grammar JSON 冒充本地语言。
2. 工具只需要 Node。上游最低 18 是语法兼容下限，不是推荐继续使用该老版本；在实施时选择仍受维护、能运行本地依赖闭包的单一精确版本，核来源并固定。这个依赖仅用于开发检查，不加入 driver 运行依赖，不安装 Docusaurus/编辑器扩展或新的 package manager。
3. 新增 testsuite/format.sh 作为明确 LPC 范围入口，默认 --check；写入必须显式 --write 并有文件列表。写入前全部文件先解析/验证，拒绝链接、越界、未知编码和并行变化；单文件写入不得留下截断内容。文件集不承诺跨文件原子性，失败须列出已改与未改文件，不自动恢复覆盖并行编辑。不要照搬上游“无参数就全库写入”的默认行为。格式化引擎和范围/排除清单各只有一个来源。
4. 完整保留 token 顺序、literal/directive 内容、幂等和拒绝残缺输入检查；为本地 owner/encoding/async/default argument/.c+.lpc 等差异补反例。支持不了的合法语法标未覆盖，不能跳过后称全库通过。
5. 单独验证 stringize：同文件、跨 include、条件分支、宏续行和宏重定义；再验证 __LINE__、错误位置、heredoc、GBK、非法 UTF-8、CRLF、无末尾换行。无法获得确定上下文时拒绝自动改写敏感文件，而不是假设空白无语义。
6. 上游 formatter 保留单行布局/无括号语句；本地新作者规则更严格。语法感知检查只报告缺花括号/非空单行体，不自动改 AST。针对故意语法/格式 fixture 设置精确例外，不能用宽泛正则批量修复。
7. 在隔离语料副本先做格式化前后比较，用当前真实 driver 运行全部受影响测试；不能以 JS tokenizer 的两次同结果证明兼容。未实现受支持的源码形式前，不把整个 testsuite 自动格式化。

出口：工具正/反例及真实 driver 验证通过；格式、词法和本地语义一致，敏感夹具不变。若无法满足，明确阻塞自动格式化子项；人工遵循规范不等于已完成自动门禁。

#### U10e：测试和职责结构整理

1. 在已有定向覆盖下拆 test_lpc.cc：共用 fixture/helpers 放测试私有头与实现，按 owner、gateway、recompile、compiler/语言、efun 移动。保持 TEST_F 的 suite/name，不改变断言。
2. 优先仍注册到同一 lpc_tests 目标；需要新目标时先核发现、初始化和 sandbox 差异，不复制另一份 init_main。
3. 清理失效状态注释和代码复述；保留 owner、异常、arena、ABI 的关键理由。结构移动、排版、行为修复各自可独立审阅。

出口：拆分前后非空发现集合相等；相关组全部通过；公共 fixture/规则无重复；不按任意行数配额制造文件。

#### U10f：接入 CI 与编辑器说明

1. .github/workflows/ci.yml 添加只读 native style 检查和已通过 U10d 的 LPC 检查；先跑 checker 自测，再跑实际范围。用 PR base 的明确本地 SHA，浅克隆缺对象时由有授权的 checkout 配置准备；checker 不自行联网。
2. CI 和本地使用相同配置/工具版本。普通文档-only 变化可输出不适用；规范、格式配置、checker 或 workflow 变化必须触发自测与固定试点集合，U10g 收口后触发完整原生范围检查。push 事件没有 PR base，须明确 before/head；首次 push、全零 SHA 或历史不可用时改为明确全量范围或失败，不能选空范围通过。有代码而工具缺席必须失败。不得给格式任务授予写仓库权限或自动修复后 push；验证 PR 内容只用无 secrets/写权限的受限 job，不用高权限触发器执行提交者代码。
3. clang-tidy 从已经配置的受影响构建读取 compilation database；不让“lint”顺手全仓构建。排版任务本身不需要 driver 编译。
4. 在 CONTRIBUTING 给出手动安装/编辑器配置说明，明确 LPC 与 C/C++ 的语言识别及敏感文件禁用 format-on-save。只修改仓库说明，不修改用户编辑器配置。Python/Shell/CMake/JS 等辅助语言按规范 §9 逐批检查，用已验证的现有检查与定向自测；无自动工具时记录人工审阅范围，不能把 native/LPC 两项通过说成所有语言均有自动门禁。

出口：本地复现 CI 命令；故意错误样例红、修正后绿；无源文件写入；流水线 yaml/配置检查通过。未获准实际触发远端时记录该边界。

#### U10g：按模块统一存量并最终收口

1. 在对应正确性单元通过后，依次整理 base/utils → 小型 packages → net → compiler → VM/owner/gateway → tests/tools。每批列出精确文件、语言、排除项和基线哈希，不一次全仓格式化；compiler/owner/gateway 应先完成 U12/U13 对应结构移动，再格式化该模块。
2. 每批先只生成到 stdout 的格式差异并审阅；经该批授权再编辑。大型尚在修复/并行修改的文件等待所属单元，不靠删除、覆盖或长期 ignore 绕过。
3. 纯排版不改变 token/预处理/可见字符串/名称；git diff -w 只作辅助，不能证明宏、__LINE__、LPC stringize 或诊断位置无变化。按实际风险补预处理/运行验证；发现语义差异即停止该批。
4. 名称/可见性/RAII/去重等非排版问题返回对应实施卡，不能混进格式提交。私有重命名必须检查全部消费者；公开旧名直接保留，不另造兼容别名。
5. 最终 --all-native 和已实现的 LPC 全量检查通过；辅助语言人工/工具覆盖分别列明。剩余项必须是有原因的生成物/字节夹具/兼容符号等明确例外，而非普通旧代码的永久豁免。

U10 总出口：规范、配置、编辑器说明和 CI 一致；新增代码不再自由选风格；存量按批准范围完成统一；行为/生成结果/敏感字节不回归。仅有 Markdown 或只有一个格式脚本，不算 U10 实施完成。

### U11：上游余项和测试资产收口

这是补齐广度的可执行调查/适配单元，不是“其余以后再说”。

1. 以附录 A 为生产文件索引；对 904 个同路径差异、两侧独有文件分别标记生成/重命名/本地能力/上游修复/新功能/无关工具。补普通文件之外的 mode、symlink、子模块和大小写/路径冲突检查；不因 §2.2 未统计这些项目而漏验。按路径、旧/新符号和 hunk 身份记录归属，路径相同或改名检测不能替代语义核对。
2. 对 350 个历史提交先按 git log --name-status 建文件候选，再读取实际 patch/hunk；name-status 本身不是 hunk 清单。用当前文件对比和已完成单元证据去重。不得以 patch-id 不同直接认定未吸收，也不得以相同主题认定覆盖。
3. 对 testsuite 的 532 个未匹配源对象区分测试入口与依赖夹具，formatter 相关测试归 U10d；排除 FFI/新语法等本轮不开放功能；379 个差异对象逐组核新增断言。
4. 按 compiler、strings/arrays/mappings、files/save、socket/TLS、async/Promise/external、math/matrix/PCRE、parser、rc/CLI 分批适配。每批先记录非零发现集合，再真实运行；不原样复制上游 master/config。
5. 审核上游新增 native 测试对 init_main、master、cwd、线程和全局状态的假设。复用 test_mudlib.h 和 run-targeted.py；不重新引入“枚举测试就启动 driver”。
6. 对无改进可吸收的条目记理由和对应当前源码。若发现新缺陷，新增有边界的原子修复卡；涉及公共合同变化则停在设计出口，不能自行扩大本轮授权。

出口：本单元清单每一项有明确处置与证据，不允许 unknown 或笼统“跟随上游”。清单先写入本文的实施记录/附录，确实过大且用户同意后才拆数据文件，避免生成第二份主计划。

### U12：owner/gateway 职责与重复状态

前置：U00、U03/U04、U08 相关正确性验证、U10e fixture 拆分；范围 owner.cc、owner_scheduler、owner_runtime_coordinator、gateway_session/gateway 及直接消费者。

1. 为现有状态画出“写入点→读取点→生命周期”表：admission、mailbox、future、reservation、session attach/detach、retry wave、cleanup。复用已有 coordinator/scheduler，不创建新的全局 manager。
2. owner.cc 先分离状态展示/trace 序列化，再分离任务构造/完成清理；中央 enqueue_owner_task_locked 的 quiescence、backpressure、manifest、owner 检查仍是唯一准入点。
3. gateway_session.cc 先提取纯 wire 编解码/投影，再提取 reservation/retry 的内部状态机；不改事件顺序、序列号、deadline、队列上限、计数和 main-required 策略。
4. 找到多个 bool 实际表达同一排他状态时，仅在全部写入/消费者已枚举后改为内部 enum；原公开 mapping 字段在边界由该状态推导，不能删除或改名。
5. 只有确定无消费者的私有 wrapper 才删除；owner safety checks、不同锁域的重复验证和回调后的再验证保留。模板化只在两处以上真实同型逻辑成立时采用。

验收：既有 status key/value 类型和语义、消息先后、背压/超时/重编译拒绝、destruct/drop 清理全部一致；ASan/TSan，owner runtime 与 gateway 实际端到端测量。结构整理单元不夹带调参、增加并行度或缓存。

### U13：编译器等价现代化（先做，不要求换前端）

前置：U00、U05、U10e/U11 编译器覆盖；统一格式按 U10a–c 执行。范围 compiler/internal、src/tools/CMakeLists、编译相关调用者与文档。

1. 参照上游 grammar_rules 拆分职责，把本地相同语义函数移入 expr/stmt/function 等私有实现文件；先不改 grammar production、token 值、AST 或 bytecode。
2. 从实际全局状态入手登记 reset/save/restore/错误退出点；将同一编译会话的瞬态状态集中到现有最合适的状态容器。不能再加 CompileState 与一套旧 globals 双写映射长期并存。
3. 保留 compile_file 的不可重入约束、inherit abort/retry、diagnostic 生命周期、source-spelling 和 recompile 隔离。将状态放进 struct 不代表允许多线程编译。
4. 核 ScratchArena、compile arena、诊断 arena 各自消费者；仅合并拥有相同生命周期的存储。已发布 program、macro/predefine、跨调用诊断不得引用提前 reset 的 arena。
5. 使用 U00c 校准后的 bench_compile 和真实 driver 相同语料，分别测实际采集的分配指标、峰值 RSS、编译吞吐、失败后恢复。只有有收益时再移植具体 arena/零拷贝技巧；compile_arena_tests 纳入 arena 变更验证。

验收：正常和失败诊断、宏/include/CRLF/编码、继承/private/默认参数、async/managed lvalue、编译错误后继续、master/simul/recompile 隔离全部通过。涉及 grammar/.spec/生成依赖时按 U06d 覆盖主 grammar、make_func、applies/options 和无 Bison fallback；不得手修生成文件。

### U14：Flex/push 前端和新增上游能力的决策出口

这是**可执行的评估单元，不是已经批准的整套替换方案**。上游前端差异很大，当前证据不足以安全承诺“一次换掉还完全兼容”。

1. 用固定上游快照核对 lexer/grammar/CompileState/arena 的完整依赖闭包，列出本地语言/编码/diagnostic/source-spelling/recompile 差异；以 U11 已归类的语法和失败用例为输入。
2. 在获准的独立实验构建中建立前后语义对照，不把两个前端并入正常 driver 或新增兼容选择开关。基线 driver、实验 driver 分别编译和运行同一输入。
3. 对 token、AST、bytecode 的结构差异作解释，最终以合法程序语义、权限 hook、诊断及失败后恢复为准。不能因内部 token 不同就误判不兼容，也不能只比 parse 成功率。
4. 检查源码 slurp 的内存峰值、超大 include/macro、所有扫描缓冲/arena 错误清理和 fallback 生成；保持本地资源上限，不能借上游更大限额放宽。
5. 输出三选一的明确结论：保留当前前端并完成 U13；只吸收局部内部机制；提交依赖完整、逐函数/文件映射的前端替换补充设计。

选择“替换”时必须补齐实际差异和回归证据后再批准实施；不得把本文当作允许直接移除 LexStream/lex.cc 的授权。新语言、FFI、警告、PCRE D2、WSS 热更新都各自有独立合同，不捆绑进入“前端升级”。

出口：评估结论、收益/风险和唯一选择明确；若替换未获批，主整理可以按“已整理但未替换前端”交付，不能宣称已吸收全部现代上游能力。

### U15：依赖、平台与安全维护

范围：third_party/manifest.yaml、sbom、vendor 专项说明、CMake/Docker/workflows、相关工具和本地文档栈。

1. 对每个 vendor 核源码版本、来源 commit、许可证、实际本地补丁和内容哈希。先解决 lws 清单矛盾，再判断是否升级；不能通过单改版本字符串制造“已更新”。
2. 对上游 vendor 差异按安全修复/平台修复/功能/构建变化分类，读取对应发行说明/官方公告。没有版本和受影响条件证据，不报告具体 CVE 已命中或已修复。
3. 单个依赖单独升级；driver/library API、Windows、静态/动态链接、package OFF、TLS 验证、编码等受影响项定向验证。保留实际 fork 补丁，不整树覆盖。
4. 审查 Alpine 3.18 的维护状态和当前可重建性；若需要升级固定镜像，先测试 PCRE2/ICU/OpenSSL/静态包组合，不改 registry，不部署。镜像构建需要当次授权和可用环境。
5. docs 使用本地 VitePress 依赖和 lockfile，只按本地需要更新；不因为上游 npm commit 新就强行同步。
6. 收窄实际可修的编译警告/sanitizer 抑制；不能删除检查换绿。Linux 本地、WSL2、Windows/macOS/Alpine 的证据分别记。固定实际 compiler/libc/ICU/OpenSSL/zlib 等版本，核静态/动态加载来源；许可证、子模块身份和构建生成器同列，不只看 manifest 的版本字符串。依赖升级后必须重建候选并重跑受影响单元，不能引用升级前证据。

出口：源码/清单/SBOM/补丁来源相符；许可证保留；未拥有平台写 external-required。哈希不一致先调查，不自动“更新期望值”。公开发布工程不加入本单元。

### U16：文档与规则单一来源

1. 行为/API 说明归 docs/efun、docs/apply、docs/lpc、lpc-modern-runtime、owner/runtime/gateway/recompile 实际归属页；方案只记录决策和证据链接。
2. 将旧计划中已经失效的“尚未实现/已完成”与当前源码对齐；历史失败原始证据不删，不把后续通过追写成从未失败。
3. AGENTS 只同步稳定目录/入口/边界，并引用 coding-standard.md；CONTRIBUTING 和编辑器说明由 U10a/U10f 对齐，不能复制整份规范或实施方案。
4. 只对确实无消费者、无历史价值且已批准删除的重复材料执行删除；文档整理授权不等于任意删除证据和旧接口。

验收：实际实现与文档一致；链接/路径/命令可解析；一个合同只有一个权威定义；没有互相矛盾的两个主计划或完成状态。

### U17：最终集成与原 mudlib 验收

前置：本轮批准的单元均已通过，安全/兼容决策均已落实或明确排除。

1. 对照 §3.2 的冻结清单比较所有公开表面；不存在未说明的签名、flags、defaults、错误类型、mapping key、保存格式、回调顺序或网络策略变化。
2. 用当前源码分别构建 Debug、Release/portable 和受影响 sanitizer 目标；不能只用 Debug 证明没有 NDEBUG 副作用。
3. 在 U00a 验证过的完成合同下串行运行隔离全量 LPC 和相关 C++ 集成一次；核实际身份集合与通过/失败/跳过、退出码、异步排空和资源清理。未启用 package 的空函数不算覆盖；发生顺序相关失败用已记录顺序重放，不能靠再随机跑一次绿替代修复。
4. 对获准的代表性 mudlib 副本跑原编译、启动、登录/命令、定时事件、save/restore、socket/DB/外部进程和热更新脚本。baseline 与候选二进制用独立数据副本；脚本不得连接线上服务。
5. 按 §8 完成性能/内存前后对照和 soak；实际 workload 不足时标边界，不推算生产容量。
6. 检查 diff、生成一致性、文档、证据和批准范围；不残留调试绕行、永久双实现、失效状态或本轮可执行却未完成的清单项。

最终状态只能为：complete（本轮范围及平台全部有证据）、complete-local/external-required（内部完成但下游/外部平台未验）、blocked（仍有必需功能、兼容、工具或证据失败）。排除某策略/U14 替换必须在实施前明确，不得为结案临时删范围。声明只能覆盖实际 mudlib 样本、配置和平台，不能以有限回归证明任意 mudlib 均兼容。不能用“方案已写完”替代工程完成。

## 8. 验证命令与测量合同

### 8.1 命令性质

本节构建/运行属于实施阶段，会创建构建输出、日志、隔离副本或测试数据；**不是只读审计命令**。只读阶段不运行 --help/--list 来试探未知二进制的启动副作用。现有 runner 在执行时会创建 sandbox/records。

创建新目录前核对磁盘/内存；普通构建 4 并行，ASan 2；空闲内存低于 2 GiB 时先停止，不擅自清理其他人的输出。配置可能运行探针/访问依赖，缺依赖先记录，不自动安装或触发远端流水线。

以下是分阶段示例，**不是从上到下一次执行的脚本**。先完成 U00a 工具自测与隔离约束；每卡只构建其受影响目标。命令在仓库根执行，新目录必须为本轮持有，已存在则先核归属/缓存，不默认覆盖。

~~~bash
df -h /
free -h
# U00b：生产源码尚未改动时构建；同时冻结源码、语料与工具链身份。
cmake --preset dev-debug -B build-organize-baseline-debug
cmake --build build-organize-baseline-debug --target driver lpcc lpc_tests ofile_tests compile_arena_tests --parallel 4

# 候选：例如只验证文件/socket LPC 修复，先只构建 driver。
cmake --preset dev-debug -B build-organize-debug
cmake --build build-organize-debug --target driver --parallel 4
python3 tools/testsuite/run-targeted.py \
  --driver build-organize-debug/bin/driver \
  --case /single/tests/efuns/compress_file --timeout 180

# 新 getter 用例创建并接入完成协议后才运行。
python3 tools/testsuite/run-targeted.py \
  --driver build-organize-debug/bin/driver \
  --case /single/tests/efuns/socket_get_option --timeout 180

# C++ 卡才构建该目标；Modernization* 是拟新增前缀，需登记真实名称。
cmake --build build-organize-debug --target lpc_tests --parallel 4
python3 tools/testsuite/run-targeted.py \
  --binary build-organize-debug/src/tests/lpc_tests \
  --gtest-filter='DriverTest.Modernization*' --timeout 180
~~~

Sanitizer 按卡选择，下面以 native 目标示例；需要 LPC/网络验证时还必须构建对应 driver 并实际运行，不能只构建不测试：

~~~bash
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
  cmake --preset asan -B build-organize-asan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
  cmake --build build-organize-asan --target lpc_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
  python3 tools/testsuite/run-targeted.py \
  --binary build-organize-asan/src/tests/lpc_tests \
  --gtest-filter='DriverTest.Modernization*' --timeout 180

UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  cmake --preset ubsan -B build-organize-ubsan
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  cmake --build build-organize-ubsan --target lpc_tests --parallel 4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  python3 tools/testsuite/run-targeted.py \
  --binary build-organize-ubsan/src/tests/lpc_tests \
  --gtest-filter='DriverTest.Modernization*' --timeout 180

# 下列 setarch 包装针对本项目 WSL2/x86_64 约束，覆盖构建后的 GTest 枚举。
TSAN_OPTIONS=halt_on_error=1:exitcode=66 \
  setarch x86_64 -R cmake --preset tsan -B build-organize-tsan
TSAN_OPTIONS=halt_on_error=1:exitcode=66 \
  setarch x86_64 -R cmake --build build-organize-tsan --target lpc_tests --parallel 4
TSAN_OPTIONS=halt_on_error=1:exitcode=66 \
  setarch x86_64 -R python3 tools/testsuite/run-targeted.py \
  --binary build-organize-tsan/src/tests/lpc_tests \
  --gtest-filter='DriverTest.Modernization*' --timeout 180
~~~

U17 或 U00 的阶段集成才运行全量 LPC；须先证明异步完成与必需用例集合正确：

~~~bash
python3 tools/testsuite/run-targeted.py \
  --driver build-organize-debug/bin/driver --all-lpc --timeout 1800
~~~

ASan 测试显式设置 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1，并检查子进程继承的真实选项；TSan/UBSan 使用失败即非零退出的配置。现有 asan test preset 的 detect_leaks=0 不能作为内存清理通过的证据。若启用 LSan 暴露既有泄漏，保留并归因，不关闭检查后把单元标为完整通过。网络/进程资源还要分别检查 fd、线程、event、引用和 pending 队列，不能只靠 LSan。

上述普通例子不替代各卡的定向过滤器。新增测试实施时登记精确完整名；runner discovery 为 0 必须失败。recompile 场景使用 --config config.recompile；不能把路径写成 etc/config.recompile 传给该参数。命令参数来自现有 run-targeted.py，不新造 runner 别名；命令存在不等于当前结果合同可靠，必须先通过 U00a。

网络使用 src/tests/ws_smoke.py 已有场景，先在获准的新 sandbox 启动只监听 loopback 的 driver，核脚本 host/port 配置再执行；脚本不是 ctest 目标。固定端口被占用时不能杀未知进程或改测线上服务。测试证书仅在隔离目录生成，不使用真实私钥，不关闭 hostname/CA 校验来凑成功。

**文件副本不是安全沙箱。** 现有 runner 只复制 testsuite/www、设置 mud ip/端口及 FLUFFOS_TEST_MUDLIB，仍继承环境，不能拦截所有出站连接、绝对路径写入或 external 子进程。任何新上游/下游样本、配置、CMake 探针和脚本先核实际网络/文件/进程副作用。仅使用本轮文件、loopback 服务和受控子进程；不确定的样本只能在获准的 OS 隔离环境运行，否则阻塞相应项。不能以改绑 loopback 证明出站隔离，也不能把 marker 当作可信来源认证。

本轮启动的 driver/server/child 无论成功、失败、超时或中断都须收尾并核实际退出；保留失败日志，不自动删整个 evidence/sandbox。资源回收与删除证据是两种权限，不能混为一谈。

fuzz 必须区分真实目标与 smoke：

| 入口 | 已有构建条件/合同 | 验收边界 |
|---|---|---|
| fuzz_compile / fuzz_restore | BUILD_FUZZERS=ON；入口为 config + input_file；可用 AFL++ 插桩构建 | 普通编译后逐个语料回放只是 smoke，不是覆盖引导 fuzz |
| gateway_fuzz_smoke | src/tests 默认目标 | 固定确定性场景，只证明 smoke 合同 |
| gateway_fuzz | Clang + GATEWAY_FUZZ_LIBFUZZER=ON | 真实 libFuzzer；核目标解析代码而非只有 main/harness 获得覆盖与 sanitizer 插桩 |
| 其他 encoding/压缩边界 | 先查实际目标及可达消费路径 | 没有目标时新增受控 harness 或明确未覆盖，不能填一个猜测的 target |

语料数量/实际执行计数必须非空；固定输入大小、时间/内存预算，保留失败输入。会写语料或 crash 的 AFL++/libFuzzer 使用本轮可写副本与 artifact/output 目录，**不得将仓库语料作为可变输出目录**。不把 fork-server 与 owner/异步多线程初始化混用而不评估；发现初始化后线程与 fork 不兼容时先采用独立进程回放，不声称已经完成覆盖引导 fuzz。不对生产驱动发送攻击载荷。

### 8.2 性能测量

| 维度 | 输入与观测 | 判定原则 |
|---|---|---|
| 编译器 | 同一 tools/perf/corpus + 获准的真实 mudlib；成功/失败/宏密集/include 密集 | 吞吐、每轮延迟、分配数、峰值 RSS、arena retained；单看 parse 微基准不算整体收益 |
| 字符串 | ASCII/中文/emoji/CRLF；固定 token 数与字节数 | 输出一致，分类扫描次数减少，大小增长趋势合理 |
| object store | 有效/失效 handle、path、summary；1k/10k/50k 对象与 tombstone | scan 计数、锁时间、median/p95/p99、RSS；不把慢诊断路径冒充正常快路径 |
| owner/gateway | 单 owner、多 owner、广播、背压、detach/retry | 实际完成率、队列延迟、公平性、主线程占用、内存；保留相同消息顺序和语义 |
| 网络/async | 分片、burst、close、nested callback、shutdown | 完整交付率、event loop 尾延迟；本轮 fd/线程/引用回到稳定基线，退出后无残留子进程 |

baseline/candidate 必须同编译器、依赖、CPU affinity、优化/LTO、配置、包开关和输入；使用同一版本校准后的测量工具，但不共享可写数据目录。记录 CPU/频率策略、宿主负载、locale/TZ/随机顺序。若单元本身升级依赖/工具链，该差异是唯一声明的实验变量，其余条件固定；U17 多项集成比较只能报告整体效果，不能归功于其中一个改动。Release 性能不能与 Debug/sanitizer 横比；固定预热后至少 7 组交错 A/B，输出带时序和输入身份的原始样本与离散度。尾延迟另用至少 10000 个同类操作样本计算 p95/p99；不足时只报告样本量和已观测分位，不作稳定尾延迟声明。先测机器噪声，再固定回归阈值；不能看见结果后放宽阈值。任何可重复的语义回归一票否决；稳定性能回归必须解释并经批准，不能用别处更快抵消。

不预写“提升 30%”或容量承诺。性能预算/目标只有从实际 workload 和资源约束得到后才进入实现；单纯结构整理以无可测回归和维护成本下降为验收。维护收益用原重复算法/状态来源/依赖关系与修改后的具体对应证明，不以文件变短或新增 class 数量评分。

U17 soak 先固定负载及预算，baseline/candidate 各预热 5 分钟、稳定运行至少 30 分钟，包含至少 3 轮连接重建、对象加载/销毁及相关取消/背压，再排空。逐分钟采集完成数、队列、fd、线程、对象/引用、retained bytes 和 RSS。短样本不能宣称长期稳定；allocator/arena 合法保留不能误判为泄漏，持续增长也不能以“缓存”解释后略过。机器资源不足或负载不代表真实场景时，报告未验边界，不删减后仍写通过。

### 8.3 构建/用例覆盖账本

批准的每个单元先写“配置×目标×精确用例×预期合同”行，再执行；不能只引用上面的占位过滤器。缺目标、包关闭、跳过和外部缺席分别记录，实际运行记录回填附录 B。

- U00：driver/lpcc、lpc_tests、ofile_tests、compile_arena_tests 的非空集合；普通基线与缺陷反例分列。
- U06a：sockets/external 的 ON/ON、OFF/ON、ON/OFF、OFF/OFF；每组独立目录、显式 PACKAGE_SOCKETS/PACKAGE_EXTERNAL 值，Debug/Release 分别构建和启动；同一名称不能复用旧缓存冒充另一组合。
- U06d/U13：Bison 3.8.2 的显式再生成与 CMAKE_DISABLE_FIND_PACKAGE_BISON=TRUE 的真实 fallback；make_func、grammar、.spec/applies/options 均有依赖与结果检查。
- U01/U02/U07/U09：实际 LPC/网络/数值反例与 ASan/UBSan；内存验证包含正常退出，不以强制杀进程绕过清理。
- U03/U04/U08/U12：真实相关 workload 与定向 TSan；构建后 discovery、执行和收尾都在同一 sanitizer 条件下。
- U15/U17：准确列出验证过的工具链、链接方式、平台和包组合；portable-release 使用现有 preset 的优化/STATIC 值，不能与别的配置交叉充当性能基线。

## 9. 兼容冲突、失败和回退

| 风险 | 预防与停止条件 | 回退边界 |
|---|---|---|
| 大面积风格 diff 掩盖语义 | 先反例，格式与行为分开；禁止全仓 -i | 只撤本单元源码，不覆盖并行修改 |
| 上游覆盖本地能力 | hunk 级适配，source-spelling/owner/Promise/TLS 合同作为门禁 | 不 merge，不 reset，不把上游整目录当恢复源 |
| 失败清理仍破坏数据 | 私有测试文件 + 故障注入；源文件保全断言 | 生产数据零操作；不能声称代码回退可恢复已丢数据 |
| 状态集中造成悬空/双释放 | 所有权表、异常/重入/destruct 测试 | 回到上一个通过的原子单元；不叠另一套缓存规避 |
| 新解析器接受/拒绝旧语法不同 | U14 语义对照与下游真实编译 | 实验不并入默认；无“双前端永久兼容层” |
| 安全策略改变合法用户行为 | 单列 EOF/owner/X-Real-IP/新警告策略 | 未批准保持现有合法合同，不擅改 defaults |
| 测试声称绿但未执行 | U00a 的发现/完成集合、异步屏障、skip 分类、正确 cwd 与执行前后身份 | 基础设施问题先修；开始日志、包装器 exit=0 或定时退出不算完成 |
| 工具链 suppress 掩盖真实问题 | GCC/Clang 最小复现、最窄条件 | 不扩大 sanitizer 禁用范围 |
| 真实平台/下游缺席 | 明确 external-required | 不以 mock/历史日志替代当前环境证据 |
| 基线或依赖在验证后变化 | 当前源码/新增文件/生成物/二进制/依赖身份与证据绑定 | 失效的受影响行重跑，不重用先前绿色记录 |
| 副本测试越界或遗留进程 | 按实际权限核出站/绝对路径/子进程；有界清理 | 不操作真实数据，不杀未知进程，不自动删除失败证据 |

批准实施后可连续完成所有独立安全单元；只在真实授权/不可逆副作用/合同冲突/硬阻塞处暂停相应路径，不因为普通技术选择反复询问。回退动作仍须尊重工作区归属。未提交改动只反向修改本任务且当前仍匹配的片段；已提交单元用新的反向提交撤销，经当次版本控制授权执行。已有依赖单元时先核能否独立撤回，否则阻塞依赖链，不回退到“看起来能编译”就继续。测试输入、证据和业务数据不随源码回退删除。本文不提供清工作区、强推或删除业务数据的命令。

## 10. 方案最终只读复核与交付核验

首轮落盘前已作两轮主代理只读复核。本次最终审计重新读取完整方案与规范，并追踪 runner、testsuite 调度、benchmark、CMake/生成链和相关调用者。新增结论来自当前源码，不是实际运行复现；未启动构建或测试，也没有把主代理复核包装成独立模型审计。

复核后保留的关键修正：

- 新上游基线固定到 328aaf10，而非过时的 upstream/master；新增提交逐条取最终补丁。
- socket getter 同时修参数偏移和结果栈；不能只改一个症状。
- async 核心修复已经存在，不以新提交标题推动重复改造。
- object store 的二次遍历限定在 summary/相关慢路径，承认有效 handle 的提前返回。
- include_list 展示规范化与 valid_read 路径拼写分离。
- EOF carry、socket owner、X-Real-IP、新警告不默认为无语义成本修复。
- IO 修复不擅自增加任意删除、覆盖或“完全原子性”承诺。
- 最新 tracing 修复与中间提交后的修正一起处理，不复制上游已删除的不适当 fixture。
- 所有构建/测试/fuzz/网络命令都明确为写入阶段；本轮不运行。
- 第三方、安全公告和下游兼容不能从版本数字、旧证据或模拟结果推定。
- 规范补充将上游事实与本地规则分开；U10a–g 覆盖规范入口、固定工具、配置、只读检查、LPC 语义验证、CI 和存量统一。
- 新工具/配置明确为待实施；未安装工具不能被写成格式检查已通过。

### 10.1 最终审计修正清单

| 审计项 | 原方案漏洞或遗漏 | 已纳入的解决与验收 |
|---|---|---|
| R01 验收可信度 | runner 接受部分跳过/少跑，LPC 无记录补通过 | U00a：负面自测、集合等式、真实执行结果 |
| R02 异步完成 | C> 被当完成；单例 do_tests 返回即请求退出 | U00a/U02/U04：pending 屏障、晚到错误、真实 driver 反例 |
| R03 有界退出 | 父进程结束不等于后代/管道结束，二次等待无期限 | U00a/§8.1：独立期限、进程树清理、输出预算、失败证据 |
| R04 证据身份 | dirty diff 漏 untracked；事后 binary hash；构建目录不冻结源码 | U00b：前后哈希、完整源码/语料与实际动态库身份 |
| R05 测量失真 | 不同文件被当生命周期比较，JSON 写失败仍成功 | U00c/§8.2：同工作量跨轮次、原始时序、写出错误、指标范围 |
| R06 依赖与失败合同 | 风格工具阻塞正确性；死锁/构建缺陷被禁止用其真实症状作反例 | §6：硬依赖与并行路径拆开，按缺陷类型定义有效失败 |
| R07 状态机半交付 | carry 先实施但 EOF/关闭策略未决 | U02b：完整状态机同一出口；不阻塞 U02a |
| R08 网络验证 | 分次 write 被当作分次 read 的证明；副本被误当安全沙箱 | U02/§8.1：观测真实读边界、副作用核查与受控环境 |
| R09 生成与 TSan | 只查主 grammar，漏 make_func/增量依赖；setarch 漏构建枚举 | U06d/§8.1/§8.3：双生成链、fallback、目标覆盖和完整包装 |
| R10 规范门禁 | 配置变更可被当 doc-only；push 无 PR base；Git 过滤器/自动配置选择 | U10b/c/f：固定来源、前后身份、事件范围、负面测试、无高权限执行 |
| R11 文件安全 | 只看返回码，未覆盖别名破坏源文件、主错误丢失与重复 close | U01：私有别名夹具、失败点矩阵、错误/资源所有权 |
| R12 fuzz 证据 | BUILD_FUZZERS 被当所有 fuzz 目标；运行库/语料存在被当覆盖 | §8.1：真实入口/插桩区分，私有可写语料，smoke 与 fuzz 分列 |
| R13 广度与长期行为 | mode/link/平台边界、顺序重放、压力/排空和 soak 不够明确 | §4.1/U00/U11/U15/§8：明确输入、预算、指标及外部缺席 |
| R14 收尾与回退 | 证据在依赖更新后失效；只说“撤回本单元”未核依赖 | U15/U17/§9：重验受影响行、反向修改与依赖链停止条件 |

本次只修订原方案，代码规范和 325 项差异索引保持原样。文档验收检查引用/路径、命令与真实入口、修改范围、空白和 U00–U17 的出口一致性；未跟踪文档单独用 no-index 空白检查，不能以空的 git diff --check 当作已检查。对 /dev/null 的 git diff --no-index --check，退出码 1 且无诊断表示存在内容差异，不是空白错误；要求退出码为 0/1 且 stdout/stderr 无诊断，其他结果失败，不能统一忽略非零退出。文档通过不代表工程已实施，也不代表所有运行时问题已复现或排除。

## 11. 实施记录

### U00a/U00b：内部验收基础设施与 Debug 基线完成

- 用户已批准按本方案和代码规范连续实施。实施起点为 5c71593af1c45365e39e243136555b5d23dc5480；工作区仅有本会话编制的两份文档，未发现其他源码或暂存改动。
- 原始证据根：/tmp/fluffos-xk-modernization-5c71593a-d2ts34gg。initial-source.json 固定 3667 个 tracked 文件及两份 untracked 文档的身份；baseline-source 是本轮独立、无硬链接的源码副本。
- 初次 Git checkout 与工作区有 2549 项完整权限位差异及 6 项字节差异。仅在本轮私有副本按原始内容/模式校准，3667 项全部重新匹配；原工作区未改动。详见 baseline-reconciliation.json。不能用 Git checkout 相同提交代替原始字节与模式一致。
- 兼容清单为证据根内的 compatibility-surfaces.json：513 条启用的 efun 声明与生成元数据名称集合一致；冻结 11 个生成文件及 §3.2 的 10 类表面，另列公共 C++ 消费者。每类均记录调用者、验证边界、后续单元和责任方；不把源码冻结当成全部语义已证明。
- U00a 已修复部分跳过、少跑、重复完成、零断言与伪成功的验收路径。测试调度统一登记异步完成，按稳定路径顺序执行。运行器记录原始日志、源码/构建/动态链接文件/隔离配置身份，并在启动边界过滤环境；进程树退出与清理有界。使用合同见 tools/testsuite/README.md。
- 工具反例自测：24/24 通过（u00-close-selftests.log）。真实 driver 完成屏障：8/8 符合预期，其中同步/async/Promise/TLS 正例 4 项，晚到失败/永不完成/重复 token/未知 token 反例 4 项（u00-final-live）。
- 基线 Debug 构建包含 driver、lpcc、lpcshell、lpc_tests、ofile_tests、compile_arena_tests。C++ 基线 497/497 通过：487 + 2 + 8，0 跳过（u00-final-lpc_tests、u00-final-ofile_tests、u00-final-compile_arena_tests）。补齐生成 fullspec 身份后，arena 8/8 再验证通过（u00-close-arena）；生产源码未改变。
- LPC 基线：337 个必需入口的开始与完成集合一致，337 通过、0 失败、0 执行中跳过（u00-close-lpc）。执行前分类的 46 项为 17 个辅助对象、17 个覆盖缺口、12 个当前能力不可用项；缺口与理由保留在 testsuite/etc/test-scopes.tsv 和摘要，不计入通过。17 个覆盖缺口交 U11；包/交互能力交 U06/U17。
- lpcc 正常工具运行通过；缺失输入按原合同返回 1，并保留失败记录（u00-final-lpcc、u00-final-lpcc-missing）。新增 modernization_compat.lpc 覆盖返回/异常、flags、源码拼写、存档往返和 callback；它不替代完整下游验收。
- 已定位基线输入问题：C++ 源码检查需要 ../src，runner 现复制被测二进制构建缓存对应的源码；不再仅复制 src/www。解释器使用具备 pidfd 接口的 /usr/bin/python3，不因默认解释器缺接口而降级清理合同。
- 外部待验：没有获准的真实下游 mudlib、存档、数据库、交互客户端或外部 C++ 嵌入清单。责任方为用户/下游维护者；后续交 U17，不能声称“全部原 mudlib 完全兼容”。Linux 之外的平台及其他包组合未在本基线证明。
- 机器上的既有 driver 进程不属于本轮新启动任务，不终止、不重启、不覆盖其构建目录。

### 11.2 U00c：测量与写出合同

- 本单元从 fa5e321f 开始，证据根为 /tmp/fluffos-xk-modernization-fa5e321f-u00c-44Rhc4。只修改编译基准、校验工具、测试与归属文档，不修改编译器或运行时行为。
- 消费者核对覆盖 tools/perf 与 docs：未发现自动解析旧编译报告的现役脚本；旧 JSON 字段及 stdout 行仍保留原义。新签收入口为 tools/perf/compile_report.py，字段和限制见 tools/perf/README.md。历史 C-S2 的 degradation 不能再作为生命周期结论，原始数字不改写。
- 新增 schema 2：固定语料路径/字节/SHA-256、预热结果、原始逐轮逐文件时序、成功/失败数和操作级分位数。同一完整语料跨轮次比较；单轮不生成生命周期比值。未采集总分配次数，不宣称总分配下降。
- 修复 JSON 转义与打开/写入/关闭错误、stdout 错误、非法轮数静默钳制及输出别名破坏语料。运行前后核对语料身份；同路径、硬链接、符号链接输出均拒绝。复制目录仍不是 OS 沙箱。
- 反例：fail-first.log 因自测使用错误 runner 参数而无效，不作为缺陷证据。纠正调用后，fail-first-native.log 记录旧工具的 8 个真实反例全部失败（5 个断言失败、3 个缺字段错误）；9 个统计合同测试通过。原始日志与被破坏的私有别名夹具保留。
- 候选 Debug 初验 17/17；最终 Release 测试 19/19、0 跳过，包含 9 个统计合同测试及 10 个真实工具场景（final-native.log，9.727 秒）。验证实际进程退出码、未超时、监督器无额外失败及运行前后身份一致，不把 runner 启动失败当反例通过。
- 单独的 Release 真实语料校准：100 个文件、20 轮、2000 次测量编译，预热和测量均成功；报告通过重算校验（release-corpus、release-report.json）。这是测量工具验收，不是引擎提速证据；本单元不报告 A/B 收益或稳定尾延迟结论。
- 构建目标仅 bench_compile；候选目录为 build-organize-debug / build-organize-release，LTO 关闭，MARCH_NATIVE 开启。首次 configure 因系统缺 PCRE2 开发头而失败，随后显式复用 U00b 已冻结的 PCRE2 头和库路径，不关闭 package、不降低探针要求。后续性能 A/B 必须使用相同测量代码与优化配置。
- U00c 内部出口完成。U01–U17 仍按依赖推进；真实下游兼容和平台缺席状态不变，不据此声称整体整理已完成。

### 11.3 U01a：写入与 ed 失败路径

- 证据根：/tmp/fluffos-xk-modernization-47ec0a9f-u01a-2zu1_9hk。write_file 现在核完整写入、流错误和最终关闭；空字符串、flags 0–3、打开失败异常保持原合同。ed 的行写入、换行、流错误和关闭失败返回既有 EDERR；活动会话的 w/x 保留修改标志和缓冲区，不发送写入成功回调。目标可能已部分改变，不新增原子替换或强制退出后的缓冲区持久化。
- 反例：fail-first 中 5 个 write_file 故障场景失败、2 个原合同场景通过；fail-first-file 中 5 个 ed 场景全部命中状态丢失。早期缺 zlib 头、未生成新构建目标、ed 夹具缺 VM 错误上下文及受限改名失败均保留日志，不计为生产缺陷证据。夹具改为从已授权的私有文件启动编辑，不关闭权限检查。
- 新增 Linux file_io_tests：链接包装仅在测试可执行文件生效，核短写、流错误、关闭、引用数和 fd 数；真实 /dev/full 覆盖普通/gzip 写入及 ed 的 w/x。临时链接仅位于夹具私有目录，不填满磁盘。CTest 经既有 runner 隔离执行；Python3_EXECUTABLE 选择具备 pidfd 能力的解释器。
- 最终 Debug/ASan/UBSan 各 17/17、0 跳过；CTest 包装入口 1/1，通过记录指向 /tmp/fluffos-targeted-n8noh8qz。LPC file_io_failure 在三种构建各 1/1，覆盖正常追加/覆盖、压缩往返、空串和打开失败。ASan 沿用预设的 detect_leaks=0，未宣称做过 LSan 全堆泄漏验收；fd、ed buffer 与 callback 引用数另有断言。
- 仅构建 file_io_tests 与 driver。主仓库保持 OLD_ED 原配置；私有副本的新式 ed + gateway 组合构建失败，gateway_session.cc 直接读取只在 OLD_ED 下存在的 ed_buffer。另建无 gateway 组合仍链接失败：comm.cc 的 gateway probe/计数引用没有随包边界关闭。两处文件与本单元基线完全一致，是既有条件编译缺口；交 U06/U12 处理，不修改到私有树制造通过，不把关闭 package 当成原组合通过。
- 默认配置的正确性修复已验证；新式 ed 的完整配置验收仍阻塞。本条不代表 U01 全部完成，U01b–d 尚未实施。

### 11.4 U01b：压缩/解压失败保源与文件别名

- 证据根：/tmp/fluffos-xk-modernization-1e7206a1-u01b-gy_ta2cm。旧实现的 22 项反例中，20 项失败，2 项打开失败合同已正确；覆盖忽略读写/关闭/删源错误、三类文件别名、坏 CRC 和截断 gzip。负读取长度产生的超大 fwrite 请求由测试包装器拦截并记为失败，未让反例真正越界读取。
- 先打开目标但不截断，用已打开的句柄核对身份后才截断普通文件。POSIX 比较设备号/inode；Windows 普通文件使用卷号和文件索引，不使用不可靠的 CRT inode。保留不同文件的符号链接跟随、覆盖和非 gzip 透明读取，不改变两个 efun 的参数、缺省后缀及权限回调拼写。
- 输入/输出由独立 RAII guard 持有；成功路径显式核对读写、流错误及两次关闭，全部成功才删源；删源失败返回 0。失败不随手删除目标，不承诺既存目标无损或并发改名下的原子事务。
- Debug、ASan、UBSan 各 49/49（含 U01a 的 17 项），0 跳过；覆盖新增 fd 接管、元数据、截断失败与真实 /dev/full 写满设备。旧 compress_file 和新增 compress_file_integrity 在三种构建各 2/2；覆盖空文件、4096 字节边界、跨三块二进制、已有目标覆盖、透明读取和坏 gzip 保源。ASan 沿用 detect_leaks=0，未声称 LSan 全堆检查通过。
- LPC 夹具的首次失败来自 12289 字节写入超过 config.test 的 10000 字节单次传输上限；另一次来自 buffer 字节不支持 ^=。只把夹具调整为 8193 字节（三块）及普通赋值，没有更改配置或运算符。早期重复 --case 只选择最后一项，最终每项独立运行，未把未运行项计为通过。日志保留。
- 新增 compress 测试能力分类，缺包时显式记录不可用，不把无断言当作通过。Windows 分支尚无目标环境验证，留在 U15/U17 平台验收；新式 ed 的既有条件编译阻塞仍未解决。U01c–d 继续执行，U01 整体尚未完成。

### 11.5 U01c：存档、读取和 rename 失败路径

- 证据根：/tmp/fluffos-xk-modernization-6d791864-u01c-xpvgq9wc。按路径先后保留 `fail-first-*` 分组与日志；不是把不同修改阶段冒充同一基线。真实 LPC 反例确认 12000 字节变量的压缩存档原先报成功，但恢复为 0；gzprintf 的内部格式化上限和 0 错误返回被忽略是根因。
- 存档改为按原格式写出字段并检查字节数；保留原正数成功值、flags、权限回调、路径拼写及 tmp→rename。RAII 管理流和序列化缓冲区；头写入或序列化异常先关闭流再清理临时文件，不吞原异常。恢复必须读到 EOF，并在应用变量前检查关闭错误；延迟读取错误、截断 gzip 不再被接受。没有增加整体对象回滚或路径别名/并发替换/断电原子性保证。
- 同类检查覆盖 read_file/read_bytes/write_bytes：关闭或流错误不再报成功，read/write_bytes 的 fstat 失败不再中止进程。file_length 读错不再无限循环，保持只计换行符的合同；缓冲区改为局部变量，并消除扫描中的数组起点前指针算术。
- rename 的 EXDEV staging/复制失败恢复/删暂存源失败保留数据已由现有实现覆盖，只补故障注入验证，没有重写。注入只在私有文件上模拟 EXDEV；未使用真实跨设备挂载。补齐复制/恢复诊断的行尾：一次原生 78 项全过但 runner 拒绝的证据仍保留，原因是无换行日志吞并了 GTest 完成行，没有放松 runner。
- 最终 Debug、ASan、UBSan 各 80/80，0 跳过（含 U01a/b 的 49 项）；Debug 另有 7/7 存档/64 位偏移 C++ 回归。save_object、restore_object、read_file、read_bytes、rename、save_file_integrity 在三种构建各 6/6。真实大记录覆盖四种存档 flags；错误测试核对旧快照、变量状态、句柄关闭和临时文件清理。ASan 保持 detect_leaks=0，未宣称 LSan 全堆验证。
- Windows 目标环境、新式 ed 包组合与真实下游验证仍在原 U06/U15/U17 门禁中。本单元完成默认 Linux 配置；U01d 尚未完成。

## 附录 A：生产源码差异逐文件索引

这是本次固定快照的精确路径对照，不是删除清单。状态 M=两侧存在但内容不同，L=仅本地，U=仅上游；行数按换行字节计，不作为质量评分。相同文件不重复列出。权限位、符号链接、vendor/tests/docs/tools 的余项分别交 U11/U15/U16；本表只覆盖 §2.2 定义的 325 项。

归属表示审阅/实施入口，不能把整行对应文件直接替换：

- U13/U14：编译器/生成链；只有经批准的语义等价部分进入运行时。
- U08/U12：保留本地 owner/store/gateway，内部简化，不从上游补一个同名实现。
- U01–U09：按具体修复卡处理，非该修复的 hunk 继续由 U11 分类。
- U10/U11：基础、测试支持及其他功能包逐项核对；“其他”不是自动忽略。
- U15：构建/平台/依赖；内部公共头变更还必须检查直接消费者。

| 路径 | 状态 | 本地行数 | 上游行数 | 归属 |
|---|---|---:|---:|---|
| src/.clang-format | M | 9 | 14 | U10/U11 |
| src/CMakeLists.txt | M | 712 | 932 | U06/U15 |
| src/Config.example | M | 345 | 308 | U10/U11 |
| src/backend.cc | M | 945 | 356 | U10/U11 |
| src/backend.h | M | 130 | 86 | U10/U11 |
| src/backend_libevent.cc | U | 0 | 208 | U10/U11 |
| src/base/internal/EGCIterator.h | M | 226 | 239 | U07 |
| src/base/internal/crash_handler.cc | U | 0 | 42 | U10/U11 |
| src/base/internal/crash_handler.h | U | 0 | 22 | U10/U11 |
| src/base/internal/debugmalloc.cc | M | 107 | 101 | U10/U11 |
| src/base/internal/debugmalloc.h | M | 108 | 117 | U10/U11 |
| src/base/internal/external_port.h | M | 75 | 65 | U10/U11 |
| src/base/internal/file.cc | M | 60 | 81 | U10/U11 |
| src/base/internal/file.h | M | 6 | 6 | U10/U11 |
| src/base/internal/log.cc | M | 116 | 115 | U03 |
| src/base/internal/log.h | M | 96 | 98 | U03 |
| src/base/internal/lru_cache.h | M | 105 | 105 | U10/U11 |
| src/base/internal/md.cc | M | 227 | 189 | U10/U11 |
| src/base/internal/md.h | M | 132 | 130 | U10/U11 |
| src/base/internal/outbuf.cc | M | 88 | 88 | U10/U11 |
| src/base/internal/outbuf.h | M | 16 | 16 | U10/U11 |
| src/base/internal/port.cc | M | 204 | 100 | U10/U11 |
| src/base/internal/port.h | M | 35 | 33 | U10/U11 |
| src/base/internal/rc.cc | M | 471 | 656 | U06 |
| src/base/internal/rc.h | M | 30 | 33 | U06 |
| src/base/internal/rusage.cc | M | 111 | 111 | U10/U11 |
| src/base/internal/rusage.h | M | 53 | 53 | U10/U11 |
| src/base/internal/scratchpad.cc | U | 0 | 342 | U10/U11 |
| src/base/internal/scratchpad.h | U | 0 | 181 | U10/U11 |
| src/base/internal/stats.cc | M | 49 | 49 | U10/U11 |
| src/base/internal/stats.h | M | 60 | 57 | U10/U11 |
| src/base/internal/stralloc.cc | M | 518 | 508 | U10/U11 |
| src/base/internal/stralloc.h | M | 249 | 212 | U10/U11 |
| src/base/internal/strput.cc | M | 21 | 21 | U10/U11 |
| src/base/internal/strput.h | M | 23 | 23 | U10/U11 |
| src/base/internal/strutils.cc | M | 701 | 764 | U07 |
| src/base/internal/strutils.h | M | 235 | 252 | U07 |
| src/base/internal/tracing.cc | M | 258 | 299 | U03 |
| src/base/internal/tracing.h | M | 190 | 195 | U03 |
| src/base/internal/vm_thread_local.h | L | 14 | 0 | U10/U11 |
| src/base/package_api.h | M | 57 | 57 | U10/U11 |
| src/base/std.h | M | 183 | 183 | U10/U11 |
| src/comm.cc | M | 2388 | 1200 | U10/U11 |
| src/comm.h | M | 90 | 78 | U10/U11 |
| src/compiler/internal/LexStream.h | L | 57 | 0 | U13/U14 |
| src/compiler/internal/README.md | U | 0 | 156 | U13/U14 |
| src/compiler/internal/compile_arena.cc | L | 320 | 0 | U13/U14 |
| src/compiler/internal/compile_arena.h | L | 189 | 0 | U13/U14 |
| src/compiler/internal/compiler.cc | M | 3106 | 4077 | U13/U14 |
| src/compiler/internal/compiler.h | M | 316 | 706 | U13/U14 |
| src/compiler/internal/compiler_utils.cc | U | 0 | 260 | U13/U14 |
| src/compiler/internal/compiler_utils.h | U | 0 | 7 | U13/U14 |
| src/compiler/internal/diagnostic.cc | L | 112 | 0 | U13/U14 |
| src/compiler/internal/diagnostic.h | L | 129 | 0 | U13/U14 |
| src/compiler/internal/diagnostic_render.cc | L | 169 | 0 | U13/U14 |
| src/compiler/internal/diagnostic_render.h | L | 56 | 0 | U13/U14 |
| src/compiler/internal/disassembler.cc | M | 786 | 1099 | U13/U14 |
| src/compiler/internal/disassembler.h | M | 7 | 22 | U13/U14 |
| src/compiler/internal/generate.cc | M | 877 | 1028 | U13/U14 |
| src/compiler/internal/generate.h | M | 24 | 27 | U13/U14 |
| src/compiler/internal/grammar.autogen.cc | M | 6883 | 4348 | U13/U15（生成物） |
| src/compiler/internal/grammar.autogen.h | M | 159 | 197 | U13/U15（生成物） |
| src/compiler/internal/grammar.y | M | 3780 | 1194 | U13/U14 |
| src/compiler/internal/grammar_rules.cc | M | 365 | 521 | U13/U14 |
| src/compiler/internal/grammar_rules.h | M | 14 | 338 | U13/U14 |
| src/compiler/internal/grammar_rules_decls.cc | U | 0 | 326 | U13/U14 |
| src/compiler/internal/grammar_rules_exprs.cc | U | 0 | 2285 | U13/U14 |
| src/compiler/internal/grammar_rules_loops.cc | U | 0 | 266 | U13/U14 |
| src/compiler/internal/grammar_rules_switch.cc | U | 0 | 222 | U13/U14 |
| src/compiler/internal/grammar_rules_types.cc | U | 0 | 225 | U13/U14 |
| src/compiler/internal/icode.cc | M | 1359 | 1385 | U13/U14 |
| src/compiler/internal/icode.h | M | 42 | 42 | U13/U14 |
| src/compiler/internal/keyword.h | M | 31 | 32 | U13/U14 |
| src/compiler/internal/lex.cc | L | 5141 | 0 | U13/U14 |
| src/compiler/internal/lex.h | L | 184 | 0 | U13/U14 |
| src/compiler/internal/lexer.autogen.cc | U | 0 | 7543 | U13/U15（生成物） |
| src/compiler/internal/lexer.h | U | 0 | 418 | U13/U14 |
| src/compiler/internal/lexer.l | U | 0 | 1246 | U13/U14 |
| src/compiler/internal/lexer_rules.cc | U | 0 | 403 | U13/U14 |
| src/compiler/internal/lexer_rules.h | U | 0 | 156 | U13/U14 |
| src/compiler/internal/lexer_rules_pp.cc | U | 0 | 1345 | U13/U14 |
| src/compiler/internal/lexer_rules_pp.h | U | 0 | 144 | U13/U14 |
| src/compiler/internal/lexer_scan.h | U | 0 | 317 | U13/U14 |
| src/compiler/internal/lexer_utils.cc | U | 0 | 2427 | U13/U14 |
| src/compiler/internal/lexer_utils.h | U | 0 | 33 | U13/U14 |
| src/compiler/internal/lpc_modern_profile.cc | L | 197 | 0 | U13/U14 |
| src/compiler/internal/lpc_modern_profile.h | L | 54 | 0 | U13/U14 |
| src/compiler/internal/lpc_source_encoding.cc | L | 108 | 0 | U13/U14 |
| src/compiler/internal/lpc_source_encoding.h | L | 20 | 0 | U13/U14 |
| src/compiler/internal/scratchpad.cc | L | 60 | 0 | U13/U14 |
| src/compiler/internal/scratchpad.h | L | 22 | 0 | U13/U14 |
| src/compiler/internal/stage_output.cc | U | 0 | 266 | U13/U14 |
| src/compiler/internal/stage_output.h | U | 0 | 29 | U13/U14 |
| src/compiler/internal/trees.cc | M | 526 | 498 | U13/U14 |
| src/compiler/internal/trees.h | M | 199 | 208 | U13/U14 |
| src/include/ffi.h | U | 0 | 40 | U10/U11 |
| src/include/opcodes_extra.h | L | 3 | 0 | U10/U11 |
| src/include/pcre_flags.h | M | 18 | 36 | U10/U11 |
| src/include/promise.h | U | 0 | 68 | U10/U11 |
| src/include/runtime_config.h | M | 130 | 124 | U06/U15 |
| src/include/type.h | M | 25 | 26 | U10/U11 |
| src/interactive.h | M | 100 | 118 | U10/U11 |
| src/local_options.README | M | 378 | 372 | U10/U11 |
| src/main.cc | M | 4 | 4 | U10/U11 |
| src/main_fuzz_compile.cc | M | 224 | 131 | U10/U11 |
| src/main_fuzz_restore.cc | M | 161 | 127 | U10/U11 |
| src/main_generate_keywords.cc | M | 57 | 56 | U10/U11 |
| src/main_json2o.cc | M | 70 | 70 | U10/U11 |
| src/main_lpcc.cc | M | 220 | 284 | U13/U14 |
| src/main_lpcshell.cc | M | 388 | 571 | U13/U14 |
| src/main_o2json.cc | M | 77 | 75 | U10/U11 |
| src/main_symbol.cc | M | 51 | 49 | U10/U11 |
| src/mainlib.cc | M | 479 | 433 | U10/U11 |
| src/net/msp.cc | M | 34 | 34 | U09/U11 |
| src/net/msp.h | M | 6 | 6 | U09/U11 |
| src/net/net_compat.h | U | 0 | 38 | U09/U11 |
| src/net/sys_telnet.h | M | 342 | 342 | U09/U11 |
| src/net/telnet.cc | M | 735 | 752 | U09/U11 |
| src/net/telnet.h | M | 39 | 38 | U09/U11 |
| src/net/tls.cc | M | 155 | 101 | U09/U11 |
| src/net/tls.h | M | 23 | 24 | U09/U11 |
| src/net/transport.h | U | 0 | 50 | U09/U11 |
| src/net/transport_libevent.cc | U | 0 | 705 | U09/U11 |
| src/net/transport_native.h | U | 0 | 20 | U09/U11 |
| src/net/websocket.cc | M | 454 | 261 | U09/U11 |
| src/net/websocket.h | M | 35 | 23 | U09/U11 |
| src/net/ws_ascii.cc | M | 208 | 85 | U09/U11 |
| src/net/ws_ascii.h | M | 22 | 13 | U09/U11 |
| src/net/ws_common.cc | U | 0 | 220 | U09/U11 |
| src/net/ws_common.h | U | 0 | 64 | U09/U11 |
| src/net/ws_telnet.cc | M | 172 | 134 | U09/U11 |
| src/net/ws_telnet.h | M | 22 | 13 | U09/U11 |
| src/ofile.cc | M | 341 | 342 | U10/U11 |
| src/ofile.h | M | 21 | 21 | U10/U11 |
| src/packages/async/async.cc | M | 1108 | 858 | U04 |
| src/packages/async/async.spec | M | 13 | 11 | U04 |
| src/packages/compress/compress.cc | M | 318 | 398 | U01 |
| src/packages/contrib/contrib.cc | M | 3242 | 3274 | U10/U11 |
| src/packages/contrib/contrib.spec | M | 68 | 69 | U10/U11 |
| src/packages/contrib/cycles.cc | M | 377 | 595 | U10/U11 |
| src/packages/core/CMakeLists.txt | M | 7 | 15 | U06/U15 |
| src/packages/core/add_action.cc | M | 845 | 859 | U10/U11 |
| src/packages/core/add_action.h | M | 43 | 43 | U10/U11 |
| src/packages/core/call_out.cc | M | 865 | 826 | U10/U11 |
| src/packages/core/call_out.h | M | 58 | 59 | U10/U11 |
| src/packages/core/core.spec | M | 462 | 413 | U10/U11 |
| src/packages/core/crc32.cc | M | 46 | 46 | U10/U11 |
| src/packages/core/crc32.h | M | 11 | 11 | U10/U11 |
| src/packages/core/crctab.h | M | 86 | 88 | U10/U11 |
| src/packages/core/custom_crypt.cc | M | 436 | 436 | U10/U11 |
| src/packages/core/custom_crypt.h | M | 31 | 31 | U10/U11 |
| src/packages/core/debug.cc | M | 27 | 27 | U10/U11 |
| src/packages/core/dns.cc | M | 424 | 116 | U10/U11 |
| src/packages/core/dns.h | M | 13 | 22 | U10/U11 |
| src/packages/core/dns_libevent.cc | U | 0 | 252 | U10/U11 |
| src/packages/core/dns_stub.cc | U | 0 | 145 | U10/U11 |
| src/packages/core/dumpstat.cc | M | 173 | 184 | U10/U11 |
| src/packages/core/dumpstat.h | M | 10 | 10 | U10/U11 |
| src/packages/core/ed.cc | M | 3606 | 3546 | U01/U11 |
| src/packages/core/ed.h | M | 160 | 160 | U01/U11 |
| src/packages/core/efuns_main.cc | M | 3910 | 3637 | U01/U11 |
| src/packages/core/encoding.cc | M | 204 | 204 | U10/U11 |
| src/packages/core/file.cc | M | 1167 | 1112 | U01/U11 |
| src/packages/core/file.h | M | 29 | 25 | U01/U11 |
| src/packages/core/heartbeat.cc | M | 340 | 278 | U10/U11 |
| src/packages/core/heartbeat.h | M | 29 | 27 | U10/U11 |
| src/packages/core/interactive.cc | M | 51 | 44 | U10/U11 |
| src/packages/core/json.cc | L | 324 | 0 | U10/U11 |
| src/packages/core/json.h | L | 11 | 0 | U10/U11 |
| src/packages/core/mssp.cc | M | 91 | 91 | U10/U11 |
| src/packages/core/outbuf.cc | M | 17 | 17 | U10/U11 |
| src/packages/core/outbuf.h | M | 6 | 6 | U10/U11 |
| src/packages/core/promises.cc | M | 229 | 257 | U10/U11 |
| src/packages/core/rc.cc | M | 56 | 56 | U06 |
| src/packages/core/reclaim.cc | M | 131 | 216 | U10/U11 |
| src/packages/core/regexp.cc | M | 1642 | 1643 | U10/U11 |
| src/packages/core/regexp.h | M | 37 | 37 | U10/U11 |
| src/packages/core/replace_program.cc | M | 238 | 601 | U10/U11 |
| src/packages/core/replace_program.h | M | 16 | 17 | U10/U11 |
| src/packages/core/save.cc | M | 100 | 100 | U10/U11 |
| src/packages/core/sprintf.cc | M | 1412 | 1431 | U10/U11 |
| src/packages/core/sprintf.h | M | 7 | 7 | U10/U11 |
| src/packages/core/string.cc | M | 67 | 67 | U10/U11 |
| src/packages/core/sys.cc | M | 106 | 99 | U10/U11 |
| src/packages/core/telnet_ext.cc | M | 260 | 261 | U10/U11 |
| src/packages/core/time.cc | M | 181 | 173 | U10/U11 |
| src/packages/core/trace.cc | M | 40 | 56 | U10/U11 |
| src/packages/core/vm_owner.cc | L | 793 | 0 | U10/U11 |
| src/packages/core/vm_worker.cc | L | 743 | 0 | U10/U11 |
| src/packages/crypto/crypto.cc | M | 218 | 202 | U10/U11 |
| src/packages/db/db.cc | M | 1259 | 1275 | U10/U11 |
| src/packages/db/db.h | M | 79 | 74 | U10/U11 |
| src/packages/develop/checkmemory.cc | M | 1096 | 1509 | U10/U11 |
| src/packages/develop/develop.cc | M | 312 | 300 | U10/U11 |
| src/packages/develop/develop.spec | M | 25 | 30 | U10/U11 |
| src/packages/develop/disassembler.cc | M | 70 | 70 | U10/U11 |
| src/packages/dwlib/dwlib.cc | M | 876 | 883 | U10/U11 |
| src/packages/external/CMakeLists.txt | M | 7 | 16 | U01/U04/U06 |
| src/packages/external/external.cc | M | 1545 | 1412 | U01/U04/U06 |
| src/packages/external/external.spec | M | 23 | 23 | U01/U04/U06 |
| src/packages/ffi/CMakeLists.txt | U | 0 | 18 | U06/U15 |
| src/packages/ffi/ffi.cc | U | 0 | 932 | U10/U11 |
| src/packages/ffi/ffi.h | U | 0 | 26 | U10/U11 |
| src/packages/ffi/ffi.spec | U | 0 | 44 | U10/U11 |
| src/packages/gateway/CMakeLists.txt | L | 6 | 0 | U12 |
| src/packages/gateway/gateway.cc | L | 3556 | 0 | U12 |
| src/packages/gateway/gateway.h | L | 684 | 0 | U12 |
| src/packages/gateway/gateway.spec | L | 41 | 0 | U12 |
| src/packages/gateway/gateway_session.cc | L | 7476 | 0 | U12 |
| src/packages/jsbridge/CMakeLists.txt | U | 0 | 8 | U06/U15 |
| src/packages/jsbridge/jsbridge.cc | U | 0 | 497 | U10/U11 |
| src/packages/jsbridge/jsbridge.h | U | 0 | 18 | U10/U11 |
| src/packages/jsbridge/jsbridge.spec | U | 0 | 7 | U10/U11 |
| src/packages/math/math.cc | M | 360 | 359 | U10/U11 |
| src/packages/matrix/matrix.cc | M | 648 | 501 | U09 |
| src/packages/mudlib_stats/mudlib_stats.cc | M | 663 | 614 | U01 |
| src/packages/mudlib_stats/mudlib_stats.h | M | 57 | 57 | U01 |
| src/packages/ops/ops.cc | M | 1385 | 1360 | U10/U11 |
| src/packages/ops/ops.spec | M | 82 | 106 | U10/U11 |
| src/packages/ops/parse.cc | M | 1604 | 1603 | U10/U11 |
| src/packages/ops/parse.h | M | 9 | 9 | U10/U11 |
| src/packages/parser/parser.cc | M | 3825 | 3845 | U10/U11 |
| src/packages/parser/parser.h | M | 231 | 231 | U10/U11 |
| src/packages/pcre/CMakeLists.txt | M | 13 | 13 | U06/U15 |
| src/packages/pcre/pcre.cc | M | 1520 | 1976 | U10/U11 |
| src/packages/pcre/pcre.h | M | 57 | 72 | U10/U11 |
| src/packages/pcre/pcre.spec | M | 9 | 12 | U10/U11 |
| src/packages/sha1/sha1.cc | M | 116 | 116 | U10/U11 |
| src/packages/sockets/socket_efuns.cc | M | 2403 | 2154 | U02/U06 |
| src/packages/sockets/socket_efuns.h | M | 131 | 132 | U02/U06 |
| src/packages/sockets/sockets.cc | M | 394 | 395 | U02/U06 |
| src/packages/trim/trim.cc | M | 87 | 87 | U10/U11 |
| src/packages/uids/uids.cc | M | 164 | 164 | U10/U11 |
| src/packages/uids/uids.h | M | 26 | 26 | U10/U11 |
| src/portbind.cc | M | 121 | 120 | U10/U11 |
| src/symbol.cc | M | 52 | 58 | U10/U11 |
| src/symbol.h | M | 20 | 20 | U10/U11 |
| src/tools/CMakeLists.txt | M | 14 | 8 | U06/U15 |
| src/tools/build_applies.cc | M | 107 | 149 | U06/U15 |
| src/tools/build_packages_genfiles.sh | M | 88 | 106 | U06/U15 |
| src/tools/make_func.autogen.cc | L | 2007 | 0 | U13/U15（生成物） |
| src/tools/make_func.y | M | 621 | 608 | U06/U15 |
| src/tools/make_options_defs.cc | M | 57 | 57 | U06/U15 |
| src/tools/preprocessor.hpp | M | 1266 | 1252 | U06/U15 |
| src/user.cc | M | 50 | 50 | U10/U11 |
| src/user.h | M | 25 | 25 | U10/U11 |
| src/vm/context.h | L | 223 | 0 | U11/U12 |
| src/vm/frozen_value.h | L | 31 | 0 | U11/U12 |
| src/vm/internal/applies | M | 84 | 83 | U11/U12 |
| src/vm/internal/apply.cc | M | 541 | 434 | U11/U12 |
| src/vm/internal/apply.h | M | 66 | 55 | U11/U12 |
| src/vm/internal/base/apply_cache.cc | M | 270 | 74 | U11/U12 |
| src/vm/internal/base/apply_cache.h | M | 17 | 8 | U11/U12 |
| src/vm/internal/base/array.cc | M | 2166 | 2167 | U11/U12 |
| src/vm/internal/base/array.h | M | 74 | 74 | U11/U12 |
| src/vm/internal/base/buffer.cc | M | 107 | 104 | U11/U12 |
| src/vm/internal/base/buffer.h | M | 30 | 30 | U11/U12 |
| src/vm/internal/base/class.cc | M | 91 | 95 | U11/U12 |
| src/vm/internal/base/class.h | M | 24 | 24 | U11/U12 |
| src/vm/internal/base/debug.cc | M | 123 | 124 | U11/U12 |
| src/vm/internal/base/function.cc | M | 478 | 715 | U11/U12 |
| src/vm/internal/base/function.h | M | 103 | 92 | U11/U12 |
| src/vm/internal/base/interpret.cc | M | 5783 | 5852 | U11/U12 |
| src/vm/internal/base/interpret.h | M | 277 | 302 | U11/U12 |
| src/vm/internal/base/machine.h | M | 84 | 83 | U11/U12 |
| src/vm/internal/base/mapping.cc | M | 1351 | 1351 | U11/U12 |
| src/vm/internal/base/mapping.h | M | 120 | 116 | U11/U12 |
| src/vm/internal/base/object.cc | M | 2386 | 2352 | U11/U12 |
| src/vm/internal/base/object.h | M | 235 | 192 | U11/U12 |
| src/vm/internal/base/old_qsort_inc.h | M | 56 | 56 | U11/U12 |
| src/vm/internal/base/program.cc | M | 319 | 122 | U11/U12 |
| src/vm/internal/base/program.h | M | 302 | 287 | U11/U12 |
| src/vm/internal/base/promise.cc | M | 2185 | 2623 | U11/U12 |
| src/vm/internal/base/promise.h | M | 256 | 344 | U11/U12 |
| src/vm/internal/base/scoped_current_object_as_master.h | L | 55 | 0 | U11/U12 |
| src/vm/internal/base/svalue.cc | M | 341 | 350 | U11/U12 |
| src/vm/internal/base/svalue.h | M | 286 | 331 | U11/U12 |
| src/vm/internal/context.cc | L | 346 | 0 | U11/U12 |
| src/vm/internal/eval_limit.cc | M | 30 | 30 | U11/U12 |
| src/vm/internal/eval_limit.h | M | 27 | 24 | U11/U12 |
| src/vm/internal/frozen_value.cc | L | 313 | 0 | U11/U12 |
| src/vm/internal/layout_digest.h | L | 53 | 0 | U11/U12 |
| src/vm/internal/lpc_vm_profile.cc | L | 233 | 0 | U11/U12 |
| src/vm/internal/lpc_vm_profile.h | L | 70 | 0 | U11/U12 |
| src/vm/internal/master.cc | M | 198 | 231 | U11/U12 |
| src/vm/internal/master.h | M | 18 | 34 | U11/U12 |
| src/vm/internal/object_store.cc | L | 2487 | 0 | U08/U12 |
| src/vm/internal/otable.cc | M | 123 | 129 | U11/U12 |
| src/vm/internal/otable_test.cc | M | 44 | 44 | U11/U12 |
| src/vm/internal/owner.cc | L | 6551 | 0 | U11/U12 |
| src/vm/internal/owner_executor.cc | L | 31 | 0 | U11/U12 |
| src/vm/internal/owner_executor.h | L | 28 | 0 | U11/U12 |
| src/vm/internal/owner_future_store.cc | L | 747 | 0 | U11/U12 |
| src/vm/internal/owner_future_store.h | L | 209 | 0 | U11/U12 |
| src/vm/internal/owner_runtime_coordinator.cc | L | 172 | 0 | U11/U12 |
| src/vm/internal/owner_runtime_coordinator.h | L | 130 | 0 | U11/U12 |
| src/vm/internal/owner_runtime_metrics.cc | L | 10 | 0 | U11/U12 |
| src/vm/internal/owner_runtime_metrics.h | L | 132 | 0 | U11/U12 |
| src/vm/internal/owner_scheduler_state.cc | L | 429 | 0 | U11/U12 |
| src/vm/internal/owner_scheduler_state.h | L | 188 | 0 | U11/U12 |
| src/vm/internal/owner_service_registry.cc | L | 152 | 0 | U11/U12 |
| src/vm/internal/owner_service_registry.h | L | 36 | 0 | U11/U12 |
| src/vm/internal/owner_task_manifest.cc | L | 234 | 0 | U11/U12 |
| src/vm/internal/owner_task_manifest.h | L | 81 | 0 | U11/U12 |
| src/vm/internal/owner_trace_store.cc | L | 191 | 0 | U11/U12 |
| src/vm/internal/owner_trace_store.h | L | 178 | 0 | U11/U12 |
| src/vm/internal/posix_timers.cc | M | 134 | 89 | U11/U12 |
| src/vm/internal/recompile.cc | L | 902 | 0 | U11/U12 |
| src/vm/internal/recompile.h | L | 300 | 0 | U11/U12 |
| src/vm/internal/recompile_layout.cc | L | 422 | 0 | U11/U12 |
| src/vm/internal/recompile_layout.h | L | 124 | 0 | U11/U12 |
| src/vm/internal/simul_efun.cc | M | 536 | 226 | U11/U12 |
| src/vm/internal/simul_efun.h | M | 105 | 21 | U11/U12 |
| src/vm/internal/simulate.cc | M | 2550 | 2669 | U11/U12 |
| src/vm/internal/simulate.h | M | 84 | 78 | U11/U12 |
| src/vm/internal/source_spelling.cc | L | 63 | 0 | U11/U12 |
| src/vm/internal/source_spelling.h | L | 61 | 0 | U11/U12 |
| src/vm/internal/trace.cc | M | 341 | 358 | U11/U12 |
| src/vm/internal/trace.h | M | 8 | 8 | U11/U12 |
| src/vm/internal/vm.cc | M | 216 | 167 | U11/U12 |
| src/vm/internal/worker.cc | L | 1069 | 0 | U11/U12 |
| src/vm/object_handle.h | L | 187 | 0 | U08/U12 |
| src/vm/owner.h | L | 246 | 0 | U11/U12 |
| src/vm/vm.h | M | 58 | 49 | U11/U12 |
| src/vm/worker.h | L | 183 | 0 | U11/U12 |

## 附录 B：执行者完成记录模板

每个原子子项从开始就记录状态，失败/阻塞也保留证据；只有实际通过才能填完成，不预填成功：

| 单元 | 源码/构建/依赖身份 | 实际修改 | fail-first 或基线 | 发现/必需/完成/失败/跳过 | sanitizer/性能 | 兼容差异 | 未验边界 | 状态 |
|---|---|---|---|---|---|---|---|---|
| 待执行 | — | — | — | — | — | — | — | 未开始 |

批准时先明确本轮选择的单元与 U14/策略变更范围；一旦确定，执行者按依赖连续完成，不在中途自行丢弃单元或把未完成事项改成“不适用”。
