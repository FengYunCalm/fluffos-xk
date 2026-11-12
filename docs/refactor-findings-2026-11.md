# FluffOS_XK 全库整理：问题清单（2026-11）

> 状态：调查结论。本文只陈述证据，不含实施方案；实施见
> [refactor-plan-2026-11.md](refactor-plan-2026-11.md)。
> 本轮未修改任何源码、未运行构建、未运行测试。

## 0 摘要

| 结论 | 数字 | 证据位置 |
| --- | --- | --- |
| 手写 C/C++ 规模 | 173,008 行 / 311 文件（不含 `src/thirdparty` 400,180 行 / 772 文件） | §2.1 |
| 对上游时间差 | merge-base `277d0b1c`（2026-01-18）；落后 `upstream/master` 390 提交，领先 796 提交 | §9 |
| 私有 LPC 契约规模 | owner 可观测面 3 文件共 **1,058** 个字面键，下游只读 **51** 个；gateway 状态面另 365 个（零消费） | §5 |
| 观测实现占比 | `owner.cc` 1,552 次 mapping 写入 = 22.8% 文件行；`object_store.cc` 14.2% | §5 |
| 计数器字段 | ≈390 个 `std::atomic<` 声明（gateway 227、vm 59、base 23、测试 76）；`gateway.h` 单结构体 200 个 | §5 |
| 实测重复 | 1,832 个重复归一化窗口；`object_store.cc` 文件内重复 ≈2,320 行 | §4 |
| 测试规模 | C++ 37,925 行（三个文件占 67%）；LPC 用例 693 个文件 | §7 |
| 文档规模 | 999 篇 tracked Markdown / 50,303 行 / ≈2.2 MB；docs 根目录混放 19 份计划/报告/设计文档（18 份属历史） | §6 |
| 核心问题 | 不是缺功能，而是**观测层与计划文档按线性速度增长，而没有一个删除动作** | §10 |

一句话结论：这个仓库的复杂度增长几乎全部来自"证明自己能跑"（readiness 标志、计数器、
迁移证据、实施记录），而不是来自 LPC 运行时能力。要让仓库变健康，删的对象是这些，
不是功能。

## 1 方法与边界

### 1.1 基线

| 项 | 值 |
| --- | --- |
| 分支 | `main`，与 `origin/main` 同 SHA |
| HEAD | `1f3569f6 docs: use repository-safe downstream revision reference` |
| 工作区 | `git status --short --branch` 无改动、无暂存、无未跟踪（`/audit-evidence/`、`/evidence/` 由 `.git/info/exclude` 本地排除） |
| 上游引用 | `git fetch upstream --prune` 后 `upstream/master` = `38a718f4` |
| merge-base | `277d0b1c`（2026-01-18） |
| 下游仓库 | `/home/mechrevo/projects/xiakexing`：`main` = `f6acdd30e`（工作区干净），`rewrite` = `661fdfc60` |

### 1.2 已执行的检查

- 全库目录/文件/行数清点（`git ls-files`、按目录聚合）。
- 归一化重复块扫描：去注释、字符串归一、数字归一、去空行后取 14 行窗口（跨文件）与
  20 行窗口（文件内），统计完全相同的窗口。
- 关键头文件与实现全读：`vm/context.h`、`vm/owner.h`、`vm/worker.h`、
  `vm/object_handle.h`、`vm/internal/owner_runtime_coordinator.h`、
  `vm/internal/owner_future_store.h`、`vm/internal/simul_efun.h`、`vm/internal/recompile.h`、
  `vm/internal/base/function.h`、`vm/internal/base/promise.h`、`vm/internal/base/object.cc`、
  `vm/internal/base/interpret.cc`（局部）、`vm/internal/object_store.cc`（局部）、
  `interactive.h`、`comm.h`、`comm.cc`（局部）、`backend.h`、`user.h`、
  `base/internal/external_port.h`、`base/internal/rc.h`、`base/internal/rc.cc`、
  `compiler/internal/lpc_modern_profile.{h,cc}`、`packages/core/call_out.h`、
  `packages/sockets/socket_efuns.h`、`packages/sockets/socket_text.h`、
  `packages/gateway/gateway.h`、`packages/parser/parser.cc`（头部）、`user.h`、`net/websocket.h`。
- 门禁现状：`.github/workflows/*.yml` 全读；`src/.clang-format`、`src/.clang-tidy`、
  `.editorconfig`、`tools/style/check-format.py` 存在性核对。
- 上游 40 个非合并提交逐条列出，并按本地源码抽查是否已吸收。
- 下游仓库 `xiakexing` 双分支核对：`main` 的 `ownercheck.c`（2,634 行）、
  `owner_runtime_d.c`、`multicore_config_d.c`、`worker_driver_adapter.c` 全读或按其读取点
  取证；`rewrite` 全分支扫描驱动 efun 使用面。结论见 §5.4。

### 1.3 未覆盖（不得当作已验证）

- **未运行任何构建、CTest、LPC testsuite、sanitizer、benchmark**。本轮没有编译产物；
  所有行为判断来自源码阅读，属于"已读取"或"静态推断"，不是运行证据。
- 未逐行读完 `gateway_session.cc`（7,113 行）、`owner.cc`（6,808 行）、
  `test_lpc_owner.cc`（17,976 行）全文；这些文件只做了结构与热点定位。
- 调查阶段未验证下游 mudlib 的**运行**行为；实施阶段后来走了真实登录链路并执行私有副本的
  `ownercheck full`。保留原始数据副本通过、重建数据副本失败，结果随数据状态变化，不能证明删键后的
  驱动回归；下游对实际删除 12 个键的静态引用数仍为 0。
- 未核对 `src/thirdparty` 内部质量；统一第三方原树，不做整理。
- 未审查 `rewrite` 分支的其余部分（仅核实它不消费 owner 观测面）；它的架构和规范
  不属于本仓库范围。

## 2 事实地图（供后续会话快速装载）

### 2.1 规模

| 范围 | 文件 | 行数 |
| --- | --- | --- |
| `src/`（手写，不含 thirdparty） | 311 | 173,008 |
| `src/thirdparty/`（vendored） | 772 | 400,180 |
| `src/tests/` | 25 | 37,925 |
| `docs/`（tracked `.md`） | 999 | 50,303 |
| `testsuite/`（tracked） | 759 | — |
| 全仓 tracked 文件 | 3,926 | — |

### 2.2 模块分布

| 模块 | 文件 | 行数 | 职责 |
| --- | --- | --- | --- |
| `src/vm/` | 82 | 40,568 | 解释器、对象/程序、apply、owner、worker、object store、recompile |
| `src/packages/` | 86 | 55,928 | efun 与运行时能力（core 19,947、gateway 11,270、parser 4,077、ops 3,124、sockets 3,082、contrib 3,713） |
| `src/compiler/` | 32 | 21,891 | 词法、语法、icode、codegen、诊断、arena |
| `src/base/` | 36 | 5,381 | 配置、日志、文件、内存、统计、平台 |
| `src/net/` | 13 | 2,357 | telnet、TLS、WebSocket、协议适配 |
| `src/tools/` | 3 | 2,174 | `make_func`、`build_applies`、`make_options_defs`（构建期生成器） |

### 2.3 调用链（实际入口）

```text
src/main.cc                     极薄进程入口
  -> src/mainlib.cc             CLI 解析、信号、配置、master/simul_efun 装载、生命周期
     -> src/backend.cc          事件循环（gametick、tick/walltime 事件、owner main drain）
        -> src/comm.cc          交互连接、输入、命令分发
        -> src/net/             telnet / TLS / WebSocket 传输
        -> src/packages/...     heartbeat、call_out、async、socket、gateway 回调
     -> src/vm/internal/        interpret、apply、object、program、simul_efun、trace
```

LPC 编译链：`compile_file()` → `LexStream`/`lex.cc` → `grammar.y`（Bison，缺失时用
`grammar.autogen.cc`）→ `trees.cc` → `icode.cc` → `generate.cc` → `program_t`。

### 2.4 不可破坏的合同（整理时必须保持）

1. `owner` 内 LPC 串行；普通 legacy LPC 默认不进后台执行。
2. 跨 owner 只传 ObjectHandle / frozen payload / snapshot / message / future / commit。
3. main thread 保留 IO adapter、cleanup adapter 与 documented main-required surface。
4. owner epoch、destruct、stale task 检查不因整理而放宽。
5. 已有 LPC API 名称、参数、返回类型、flags 数值、存档格式不变。
6. 生成物从源头重新生成，不手改 `grammar.autogen.*`、`grammar.ebnf`、
   `lpc-grammar.json`、`lpc.tmLanguage.json`。

## 3 隐式状态机清单

判据：一个生命周期被拆成多个互相独立的字段，转场散落在多个函数里，且没有任何一处
能完整说出"现在处于哪个合法状态"。下列每项都给出状态集合、存放位置、转场点与后果。

### 3.1 高价值（先改）

| ID | 状态机 | 存放位置 | 实际状态集合 | 后果 |
| --- | --- | --- | --- | --- |
| SM-01 | 用户连接生命周期 | `interactive.h:76` `unsigned int iflags` + `int connection_type` + `bool cleanup_pending` + `unsigned int telnet_receive_depth` + `std::size_t output_send_depth` | 至少 4 个正交维度共 22 个位：输入模式（`SINGLE_CHAR`/`WAS_SINGLE_CHAR`/`NOECHO`/`NOESC`）、协商（`USING_TELNET/LINEMODE/MXP/ZMP/GMCP/COMPRESS/MSP/MSDP`）、关闭（`CLOSING`/`NET_DEAD`/`cleanup_pending`）、缓冲（`CMD_IN_BUF`）、网关（`GATEWAY_SESSION`）、能力缓存（`HAS_PROCESS_INPUT`/`HAS_WRITE_PROMPT`）、握手（`HANDSHAKE_COMPLETE`） | `WAS_SINGLE_CHAR` 是转瞬状态却与持久协商位同处一字；关闭由 3 个字段共同表达，`telnet_receive_depth` 承担"回调中不能释放"的重入语义，读代码无法回答"这个连接还能不能写" |
| SM-02 | 外部端口表 | `base/internal/external_port.h:73` `port_def_t external_port[5]` + `rc.cc` 中 `port_start` 推断 + `PORT_TYPE_UNDEFINED` | `UNSET → (port number 隐式创建 telnet) → external_port_N 覆盖/追加 → TLS/WS 附加配置` | 5 个槽位的合法性只由 `fd == -1` / `kind == UNDEFINED` 推断；`port number` 与 `external_port_1` 冲突走"警告并忽略"，无法从类型上禁止 |
| SM-03 | 对象生命周期 | `vm/internal/base/object.h`（20 个 `O_*` 位）+ 全局 `destructed_objects` 链 + `vm_owner_epoch` + `simulate.cc` `ObjectLifecycleStage` | `allocated → registered → (owner 迁移)* → pending_destruct → destructed → freed`，另有 `restricted_destruct_object`、`load_object_depth` | "已析构但对象内存仍在"与"已注册但不在 object store"由不同字段表达；`object_store.cc` 用 `ObjectRecord.destructed` + 对象 `O_DESTRUCTED` + shard tombstone 三处描述同一事实 |
| SM-04 | funptr 生命周期 | `vm/internal/base/function.h:55` `lifecycle_state {Live, StagedBorn, Invalid}` + `registry_linked` + `birth_context` + `transaction_pin_context` | `newborn(staged) → live → invalid`，并叠加"在 owner registry 中"、"被事务 pin"、"在出生 journal 中"三个正交标记 | 三个链表指针 + 一个枚举 + 一个 bool 共同表达一个状态；`dealloc_funp` 必须同时判断，漏一处即 UAF |
| SM-05 | Owner 任务生命周期 | `vm/internal/owner.cc`（`admission_state`/`future_state`/`trace.state` 全为 `const char*`，比较点 `3791`/`3799`） | 字符串字面量：`accepted`、`accepted_dispatch_rejected`、`rejected`、`rejected_backpressure`、`rejected_callback_not_allowlisted`、`dropped_callback_invalid_target`、`completed`、`failed`、`cancelled`、`timed out` 等 30+ 调用点 | 状态用 `strcmp` 比较（`owner.cc:3791,3799`）；拼错一个字符串不会编译失败，只会让 trace 静默漏计 |
| SM-06 | Owner future 生命周期 | `vm/internal/owner_future_store.h:63` `enum class OwnerFutureState{kUnknown,kPending,kCompleted,kFailed}` **与** `OwnerFutureRecord::state`（`std::string`）**与** `cancelled`/`timed_out`/`terminal_cleanup_required`/`quota_rejected` | `pending → (completed|failed|cancelled|timed_out) → taken/reaped` | **同一状态存在两份表示**（枚举 + 字符串）；两份可能不一致，且 `cancelled`/`timed_out` 与 `state` 重复表达终态 |
| SM-07 | 重编译事务 | `owner_runtime_coordinator.h:18` `OwnerRecompileState{kOpen,kClosing,kFrozen}` + `recompile.h:220` `RecompileExecutionPhase` + `recompile.h:105` `RecompileLifecycle{kIdle,kPrepared,kSwapped,kCreatePhase,kFinalized}` + `simul_efun.h:77` `State{Prepared,Activated,Finalized}` | 四层状态机串行推进，每层有自己的终态判定 | 四层状态由四个类型各自维护，没有一处能回答"这个对象现在是不是正被重编译"；`OwnerRuntimeCoordinator` 的 `claim_begin_locked`/`claim_end` 锁合同不对称，需靠注释约束 |
| SM-08 | 解析器全局状态 | `packages/parser/parser.cc:52-67`：`master_state`、`pi`、`hash_table`、`special_table`、`verbs`、`num_objects`、`my_objects`、`my_string`、`num_literals`、`literals`、`words`、`num_words`、`parse_vn`、`parse_verb_entry` 共 14 个文件静态可变量 | `MS_HAS_USERS|MS_HAS_LITERALS|MS_HAS_SPECIALS` + 一次 parse 的全部中间量 | 没有 parse 会话对象，也就无法嵌套/重入；任何经 LPC 回调再进入 parse 的路径都会覆盖外层中间量 |
| SM-09 | Socket 生命周期 | `packages/sockets/socket_efuns.h:36` `enum socket_state` + `:56` `state` 字段 + `unsigned short flags`（12 位）+ `release_ob`/`release_ob_epoch` + `ssl` + `SocketTextAccumulator` | `CLOSED/FLUSHING/UNBOUND/BOUND/LISTEN/HANDSHAKE/DATA_XFER` 与 `S_RELEASE/S_BLOCKED/S_LINKDEAD/S_TLS_IS_SERVER/...` 组合 | 状态枚举与位标记可表达互相矛盾组合（如 `STATE_DATA_XFER|S_RELEASE|S_LINKDEAD`）；`release_ob` 与 `owner_ob` 的双主设计没有不变量检查 |

### 3.2 中价值（结构清晰化）

| ID | 状态机 | 存放位置 | 问题 |
| --- | --- | --- | --- |
| SM-10 | Promise | `base/promise.h:54` `uint8_t state` + `handled` + `resolving` + `body_owned` | 四个独立字段表达"已决/已采纳/只读观察/body 专用"；注释写的规则比代码结构更强，改一处要读全段注释 |
| SM-11 | 协程 | `base/promise.h:214` `queued` + `abandoned` + `registered` | 3 个 bool 推出 4 个状态（`parked`/`queued`/`abandoned`/`live`），代码靠 `implies queued` 注释维持不变量 |
| SM-12 | Gateway 会话与入站序列 | `packages/gateway/gateway.h:342` `GatewayMessageEventReservationOrigin`、`GATEWAY_READ_PAUSE_*`、`gateway_session_id`、`gateway_master_fd`、rebind 计数 | 会话阶段（`attached → detached → rebind → expired`）没有枚举；由 `gateway_session_id` 是否为空 + 定时器 + 计数器共同表达 |
| SM-13 | 编译器预处理 | `compiler/internal/lex.h:20` `PRAGMA_*` 9 位 + `EXPECT_*` 7 位 + `compiler.h` 上下文位 11 个（`LOOP_CONTEXT` … `NO_SUSPEND_CONTEXT`）+ 2 个 case-label 位 + `lex.cc:3766` `directive_scan_state_t`/`directive_char_state_t` + include 深度 | 三套位控制同一编译期状态，无单一所有者（不存在上游那种 `CompileState`）；`ASYNC_CATCH_CONTEXT` 已存在，但"普通 catch 体内"无独立位（对应上游 #1443 的缺口） |
| SM-14 | 用户终端模式 | `comm.cc:342` `UserTerminalModeDelta{LineMode,CharMode}` + `SINGLE_CHAR`/`WAS_SINGLE_CHAR` | 一个"模式切换 + 待提交差异"的 delta 队列与 iflags 双写 |
| SM-15 | 后端调度 | `backend.h:25` `BackendEventPriority` + `TickEvent::valid_` + `*_budget_yields` 计数器 | 事件是否有资格执行由 `valid_` 原子 + 队列 tick + 预算让出三者决定，没有状态枚举 |
| SM-16 | 外部进程 | `packages/external/external.cc:50` `HandleState{Created,Running,Done}` | 枚举方向正确，但 `Done` 合并了"正常退出/失败/被杀"，调用方还要看 exit code |
| SM-17 | Worker 任务 | `vm/worker.h:8` `VMWorkerTaskState{kUnknown,kPending,kSucceeded,kFailed}` | 与 owner future 的枚举（SM-06）是同一语义的第二份定义 |
| SM-18 | 配置 | `include/runtime_config.h`（`__RC_*` 编号）+ `base/internal/rc.cc` 默认值表 + `CONFIG_INT/CONFIG_STR` 宏 | 每个设置项在 3 处必须一致（编号、默认值、文档）；`__RC_STR_1__`、`__RC_INT_18/22/23` 是已废弃占位编号，仍然占位 |
| SM-19 | 编译期选项 | `src/local_options`、`src/base/internal/options_internal.h`、`src/outdated_define.txt` | 与运行时配置并行的第二套配置机制；`outdated_define.txt` 无任何消费者（§4.3） |

### 3.3 已经是正确形态（保留并作为模板）

| 模式 | 位置 | 说明 |
| --- | --- | --- |
| 显式三阶段派发表 | `vm/internal/simul_efun.h:77` | `State{Prepared,Activated,Finalized}` + 四个显式转移函数 + 头文件写明"activate 只许一次" |
| 显式互斥状态 + 单一锁 | `vm/internal/owner_runtime_coordinator.h:18` | `OPEN→CLOSING→FROZEN→OPEN`，明确"不引入第二把锁" |
| 生命周期枚举 + RAII 引用 | `vm/object_handle.h` `VMObjectRefGuard` | 所有权通过类型表达，释放路径唯一 |

整理时的目标形态就是这个表：**状态用 enum class 表达，转场只允许经过一个函数，
不变量写在类型旁边而不是长注释里。**

## 4 重复实现

### 4.1 实测方法

去注释、字符串归一、数字归一、空白归一后，取固定行窗口统计完全相同内容；
跨文件 14 行窗口、文件内 20 行窗口。生成物之间的重复不计入缺陷。

### 4.2 跨文件热点

| 重复块 | 位置 | 判定 |
| --- | --- | --- |
| 565 | `compiler/internal/grammar.autogen.cc` ↔ `src/tools/make_func.autogen.cc` | **非缺陷**：两份都是同一 Bison 语法的生成产物，属于构建设计 |
| 90 / 88 / 77 | `tests/lpc_vm_bench.cc` ↔ `object_store_bench.cc` ↔ `owner_runtime_bench.cc` | **缺陷**：三个 benchmark 各自复制同一套环境装配、计时、报告逻辑 |
| 12 | `tests/test_lpc_gateway.cc` ↔ `test_lpc_owner.cc` | 缺陷：同一 LPC fixture/mapping 取值 helper 写了两遍 |
| 10 | `net/ws_ascii.cc` ↔ `net/ws_telnet.cc` | 缺陷：`ws_ascii_callback`（`:31`）与 `ws_telnet_callback`（`:37`）的 `SESSION_CREATED`/`ESTABLISHED`/`RECEIVE`/`CLOSED`/`FREE` 分支与 `per_vhost_data` 定义各自一份（会话建立/关闭已抽到 `WebSocketSession`，但回调骨架没吸干净） |
| 10 | `compiler/internal/compiler.cc` ↔ `vm/internal/base/program.cc` | 待判定：疑似 program 元数据写入重复 |
| 5 | `main_fuzz_compile.cc` ↔ `main_fuzz_restore.cc` | 缺陷：两个 fuzz 入口复制同一套 driver 初始化 |
| 1–2 × 5 | `async.cc` / `dns.cc` / `heartbeat.cc` / `call_out.cc` / `socket_efuns.cc` | 缺陷：owner executor 任务提交样板重复 5 份（每份都带 policy/route/reason 字段） |

### 4.3 文件内热点

| 重复块 | 估算行数 | 位置 | 判定 |
| --- | --- | --- | --- |
| 116 | ≈2,320 | `vm/internal/object_store.cc`（`vm_object_shard_contract_mapping` 起于 `:1353` 与 owner status 展平版起于 `:1596`） | 缺陷：同一组 20+ owner-local 契约键**装配两次**（嵌套子 mapping + 展平版），两份会各自漂移。注意 lookup 路径本身已统一到 `finalize_owner_local_lookup_locked`，不是重复点 |
| 36 | ≈720 | `vm/internal/base/array.cc`：`alist_sort`（`:1491`）、`intersect_array`（`:1665`）、`union_array`（`:1825`） | 缺陷：同一段"把 array 复制/共享到临时表并堆排序"的循环写了三份，含 destructed object 与 shared string 处理。任何一处漏处理（如新增 T_PROMISE）只改一份就是真 bug |
| 18 | ≈360 | `packages/sockets/socket_efuns.cc`：`handle_tls_handshake`（`:112`，`SSL_connect`）与 `handle_tls_server_handshake`（`:139`，`SSL_accept`） | 缺陷：TLS 握手状态推进（`STATE_HANDSHAKE→STATE_DATA_XFER`、`S_BLOCKED`、`EV_WRITE` 重驱、失败关闭）写了两份，仅 SSL 调用与错误文本不同 |
| 14 | ≈280 | `vm/internal/owner.cc` | 缺陷：trace/mapping 装配复制 |
| 13 | ≈260 | `vm/internal/base/object.cc`（`345`/`369`） | 缺陷：`case ']'` 与 `case '/','}'` 除 `is_mapping` 外完全相同 |
| 9 | ≈180 | `vm/internal/base/mapping.cc`：`add_to_mapping`（`:857`）与 `unique_add_to_mapping`（`:937`） | 缺陷：两份逐行相同的合并循环，含 `MAX_MAPPING_SIZE` 计数/统计更新（错误处理稍差即造成计数漂移） |
| 9 | ≈180 | `vm/internal/base/interpret.cc`：`copy_lvalue_range`（`:1121`）与 `assign_lvalue_range`（`:1340`） | **高风险**：两份逐段相同，差异有两处且都未写进名字——(a) 消耗型版本在数组独占（`fv->ref == 1`）时**转移**元素而不是复制，并 `free_empty_array`；(b) 结束时额外释放 `from`（string 用 `free_string_svalue`、buffer 用 `free_buffer`）。新增值类型时必须同时改两份 |
| 9 | ≈180 | `vm/internal/base/interpret.cc`：`setup_new_frame`（`:1952`）与 `setup_inherited_frame`（`:2003`） | 缺陷：继承链上找真实函数的别名/二分/偏移累积循环写了两份。这是帧解码热路径，两份漂移会直接表现为继承调用走错函数 |
| 4 | ≈80 | `packages/compress/compress.cc`：`f_compress_file`（`:180`）与 `f_uncompress_file`（`:238`） | 缺陷：参数校验 + realpath + 目录/权限策略整段复制，属于路径安全敏感区 |
| 194 / 136 | ≈3,880 / 2,720 | `tests/test_lpc_owner.cc` / `test_lpc_gateway.cc` | 缺陷：测试夹具与断言样板重复 |

### 4.4 语义重复（不同文件，同一职责）

| 职责 | 实现 A | 实现 B | 说明 |
| --- | --- | --- | --- |
| `#pragma modern_lpc`/`strict_owner` 名称与位 | `compiler/internal/lex.cc:1715`（映射到 `lex.h` `PRAGMA_MODERN_LPC=128`） | `compiler/internal/lpc_modern_profile.cc:29`（映射到 `LPC_MODERN_PRAGMA_*`） | 两套名称表 + 两套位定义；B 只有测试消费者 |
| `#pragma source_encoding` 解析 | `lex.cc:1736` | `lpc_modern_profile.cc:80` | 同一指令两套解析实现 |
| future 终态 | `OwnerFutureState` 枚举 | `OwnerFutureRecord::state` 字符串 + `cancelled`/`timed_out` | 见 SM-06 |
| worker 任务态 | `VMWorkerTaskState` | `OwnerFutureState` | 同一语义的两个枚举 |
| HTML/文本转义 | `net/ws_ascii.cc` | `net/ws_telnet.cc` | 见 §4.2 |
| benchmark 装配 | 3 个 bench 文件 | — | 见 §4.2 |

## 5 观测层过载（最大的单一问题）

### 5.1 事实

| 指标 | 值 |
| --- | --- |
| 全仓 `add_mapping_*` 字面键（含 efun 结果 mapping） | 1,504 |
| owner 可观测面（`owner.cc` 846 / `object_store.cc` 192 / `vm_owner.cc` 48）去重 | **1,058** |
| gateway 状态面（`gateway.cc` 303 / `gateway_session.cc` 62） | 365 |
| 下游 mudlib（xiakexing main）实际读取 | **51**（全部包含在 1,058 内，零缺失） |
| 键值被写成硬编码常量 `1` 的写入点（6 个主文件） | ≈245 |
| 以 `_ready` 结尾（全仓） | 183 |
| 以 `_policy` 结尾 | 43 |
| 以 `_blocker` 结尾 | 18 |
| `owner.cc` 中 mapping 写入调用 | 1,552（占 6,808 行的 22.8%） |
| `object_store.cc` | 388（14.2%） |
| `gateway.cc` / `gateway_session.cc` | 306 / 202 |
| `std::atomic<` 声明总数 | ≈390（gateway 227 / vm 59 / base 23 / 测试 76） |
| `gateway.h` 单结构体 `GatewayRuntimeCounters` 字段 | 200 |
| 本轮待删（1,058 − 51 + 365） | **≈1,372** |

`vm_object_shard` 合同 mapping 一个函数就写 35 个键
（`object_store.cc` `vm_object_shard_contract_mapping`），其中
`owner_local_store_ready`、`owner_local_store_complete`、
`global_live_object_bridge_ready`、`global_record_bridge_ready`、
`global_index_bridge` 等描述的是"迁移是否完成"，不是运行时事实。

### 5.2 两个具体后果

1. **接口被实现细节污染。** `VMObjectHandleResolveResult`（`vm/object_handle.h:53`）
   有 13 个解析状态 + 30 个字段，其中 28 个在回答"这条诊断结论是从哪个内部索引
   查到的"（`global_record_id_scan_bridge_used`、`owner_local_cross_shard_record_source`、
   `diagnosed_via_owner_local_path_index` …）。调用者不能只问"这个 handle 还能用吗"，
   必须穿过滤镜才能拿到 `status`。
2. **能力被冒充为完成。** 6 个主文件里有 **245 处**键的值是硬编码常量 `1`（`owner.cc` 165、
   `gateway_session.cc` 37、`object_store.cc` 23、`vm_owner.cc` 14），例如
   `gateway_encoding_boundary_ready`、`session_fifo_contract_ready`、
   `process_input_apply_frame_executor_ready`、`input_callback_state_snapshot_ready`。
   只要代码里写了 `add_mapping_pair(map, "x_ready", 1)` 就"ready"，没有对应测试或行为
   绑定。这是自证式标志，不是证据。（注：`executor_safe`、`main_required` 这类键来自
   任务描述表而不是常量，属于真实声明，不在本条批评范围。）

### 5.3 伪能力：`lpcc --owner-audit`

`compiler/internal/lpc_modern_profile.cc` 的 `lpc_owner_audit_source()` 逐行做子串
匹配：命中 `call_other(` / `move_object(` / `destruct(` 就报
`cross_owner_mutable_write`（severity `error`）。

- 它不理解作用域、owner、是否 same-owner、是否有 message 替代路径，因此对**任何**
  `destruct()` 调用都报 error，包括合法路径。
- 唯一的消费者是 `main_lpcc.cc:85` 的 `--owner-audit --format=json`。
- 该文件同时被**活的编码层依赖**：`lpc_source_encoding.cc:89` 调用
  `lpc_modern_profile.cc:130` 的 `lpc_source_encoding_from_source()`，
  `lpc_modern_profile.h` 还存放 `kLpcSourceEncodingSchemaV1`、
  `kLpcInternalStringEncoding` 两个被编译器和 `main_lpcc.cc` 使用的常量。
  也就是说，编码层反向依赖了一个审计演示文件。
- 唯一的测试（`tests/test_lpc_owner.cc:589-…`）验证的是"规则表有 4 条"和"含
  `call_other(` 的文本能产出发现"，即验证误报器能误报。
- 它的 ready 状态被写进 `vm_owner_runtime_status()`，对外表现为该能力可用。

结论：这是一个把"grep 结果"包装成"静态审计"的层。它不提高安全性，只提高信任风险；
同时把编码实现拖进了一个语义上不相关、且有第二名"所有者"的模块。

### 5.4 消费方实测（2026-11-11，对 xiakexing 两个分支）

被审仓库：`/home/mechrevo/projects/xiakexing`，工作区在 `main`（干净）。

| 分支 | 提交 | 是否读驱动 owner 状态 | 证据 |
| --- | --- | --- | --- |
| `main`（现生产 mudlib） | `f6acdd30e` | 是，读 51 个键 | `server/cmds/admin/ownercheck.c`、`server/daemons/services/system/multicore/{owner_runtime_d.c,multicore_config_d.c}`、`worker_driver_adapter.c` |
| `rewrite`（重写分支） | `661fdfc60` | **否** | 全分支 533 个文件，`vm_owner_*_status`/`vm_object_store_status` 零命中；只用了 8 个驱动 efun：`vm_object_handle`、`gateway_{disconnected,session_send,grace_snapshot,grace_expired,receive,logon}` |

`main` 实际读取的 51 个键（按来源 mapping 分组）：

| 来源 | 个数 | 键 |
| --- | --- | --- |
| `vm_owner_thread_status()` | 19 | `enabled`、`thread_count`、`max_owner_threads`、`thread_dispatched`、`main_dispatched`、`thread_message_dispatched`、`thread_context_bound`、`thread_object_store_isolated`、`thread_owner_bound`、`thread_owner_cleared`、`thread_lpc_rejected`、`thread_owner_state_guarded`、`executor_safe_task_dispatched`、`executor_runnable_task_dispatched`、`thread_lpc_probe_executed`、`thread_lpc_probe_guarded`、`thread_lpc_probe_failed`、`thread_lpc_canary_succeeded`、`thread_lpc_task_succeeded` |
| `vm_owner_runtime_status()` | 13 | `lpc_task_allowlist`、`registered_owner_task_domains_ready`、`registered_owner_task_domain_count`、`gateway_command_execute_ready`、`target_owner_message_executor_ready`、`service_shard_executor_ready`、`ordinary_lpc_default_closed`、`normal_path_main_fallback_count`、`owner_quiesce_attempts`、`owner_quiesce_success`、`owner_quiesce_timeouts`、`owner_quiesce_admission_rejected`、`owner_epoch` |
| mailbox / drain / purge / schedule | 7 | `owner_queue_depth`、`queue_depth`、`drained`、`purged`、`remaining`、`dispatched`、`tasks` |
| 任务/事件记录 | 5 | `task_id`、`owner_id`、`task_type`、`task_key`、`state` |
| `vm_owner_status()` | 6 | `object`、`owner_id`、`default_owner_id`、`total_checks`、`mismatch_checks`、`has_explicit_owner` |
| guard / trace | 2 | `success`、`events` |

（分组合计 52，其中 `owner_id` 在两组重复，去重后 **51**。）

两个直接后果：

1. **`vm_object_store_status()` 与 `vm_object_store_owner_status()` 在 `main` 中零命中。**
   `object_store.cc` 的 388 处 key（含整套 bridge/retirement/consistency 字段）**没有
   任何下游消费者**，只被驱动自己的 C++ 测试断言。这部分可以整体删除，风险只在
   本地测试。
2. **`rewrite` 不读任何 owner 状态。** 它是下一代 mudlib（533 文件，`.lpc` 扩展名，
   模块化目录），Owner 模型改成了应用层领域所有权，不再依赖驱动的运行时观测面。
   即使不删，这些键在未来也没有第二个使用者。

因此结论从"必须先获得下游确认"改为：**冻结这 51 个 owner 键，其余可删**。
删除后的动态验收已在私有副本执行，但结果受下游数据状态影响，未形成可归因的前后门禁；
因此本结论只保留 12 个实际删除键的零引用证据，不把 `ownercheck full` 结果写成驱动回归通过。

### 5.5 第二套契约：Worker 总线不属本次范围

同一个下游还读第二套映射：`worker_bus_d.c`（331 行）与 `async_compute_d.c`（526 行）
通过 `vm_worker_task/submit/poll/status` 使用 Worker 总线，读取下列键：

`task`、`task_id`、`type`、`status`、`success`、`error`、`options`、`snapshot`、
`snapshot_hash`、`input_hash`、`result_spec`、`results`、`damage`、`owner_key`、
`accepted`、`timeout_ms`、`ttl_ms`。

`vm_worker.cc` 共发出 ≈108 个字面键（含 bench/actor_score/combat_damage 全套结果结构），
与 owner 观测面**不重叠**。

结论：**本轮不碰 `vm_worker.cc`**。它不是迁移脚手架，而是有真实消费者的功能合同；
要缩减它是另一个独立课题（需逐键确认下游读法）。因此可删量按实测重新核算：
**owner 面 1,058 − 51 = 1,007，gateway 面 365，合计 ≈1,372 个键**（trace.cc 7、
promise.cc 14、`vm_worker.cc` 58 不动）。

### 5.6 零消费者的 LPC efun

`vm_worker_bench` 与 `vm_worker_actor_bench` 登记在 `core.spec`，但：

- LPC 侧零调用：`testsuite/` 无引用，下游 `main`/`rewrite` 也无引用。
- 唯一"消费者"是 `test_lpc_owner.cc:4749` 对 `.spec` 宽度的断言
  （`void f_vm_worker_bench()` 必须存在）。也就是**它存在，只因为测试断言它存在**。
- 真正的基准调用走 C++ API（`vm_worker_benchmark()` / `vm_worker_actor_benchmark()`），
  不需要经过 LPC。

同类待审（已有下游消费者，不删）：`vm_worker_task/submit/submit_batch/poll/poll_batch/status`。

对驱动契约的三个额外含义：

1. `vm_object_handle()` efun **只有 `rewrite` 在用**（`server/runtime/objects/registry.lpc`
   2 处），`main` 不用；因此它是未来契约，必须保留。
2. `rewrite` 只依赖 8 个驱动 efun（上述 1 个 handle + 7 个 `gateway_*`），说明驱动的
   owner 观测面在下一代 mudlib 里已经退场；删键不会阻塞重写。
3. `rewrite` 用 `contracts/*.yaml|json` + `module.yaml` + `acceptance/` 把合同做成机器
   可验证资产，这正是本仓库 U9a（status key 契约检查）要对齐的形制。

## 6 文档与流程腐化

### 6.1 计划与报告文档堆在 docs 根目录

`docs/` 根目录下曾有 **19 份**计划/报告/设计文档（不计本次新建的两份），其中 18 份
是历史计划或一次性证据，1 份（`recompile-object-v2-design-2026-08.md`）是仍在生效的
设计权威。U2a 已把下表 18 份整体移入 `docs/archive/plans/`（路径已按归档后位置书写）。
下表列出其中 9 份有代表性的文档：

| 文档 | 体量 | 自述状态 | 基线 | 问题 |
| --- | --- | --- | --- | --- |
| `docs/archive/plans/project-modernization-plan-2026-10.md` | 308 KB | "已进入实施阶段" + 实施记录 | `fe163d67` | 计划与实施日志混写，正文与 §11 记录重复 |
| `docs/archive/plans/upstream-absorption-plan-2026-10.md` | 217 KB | "已进入实施阶段" | 上游 `795eb371` | 目标已过期 40 提交；同一表格既当设计又当记录 |
| `docs/archive/plans/optimization-master-plan-2026-08-18.md` | 35 KB | `DRAFT v0.3`，要求 A-S0 证据闭合后实施 | `2785c8a6` | 依赖 `/tmp/pr1247.diff`（临时路径，违反项目自己的持久化规则） |
| `docs/archive/plans/upstream-sync-optimization-plan-2026-08.md` | 52 KB | `DRAFT v2.4 - 仅供审计` | `80b0f329` | 永远不会执行的草案，仍在 docs 根目录 |
| `docs/archive/plans/upstream-sync-evidence-2026-08.md` | 54 KB | "随方案执行更新" | `80b0f329` | 证据与方案互相引用，两处都要读 |
| `docs/archive/plans/backlog-plan-2026-09.md` | 6 KB | "已执行" | — | 已完成记录占据 docs 根目录 |
| `docs/archive/plans/backlog-full-plan-2026-09.md` | 21 KB | "只含方案；执行按批次另行授权" | — | 33 项 NEEDS-PORT，无后续状态 |
| `docs/archive/plans/AUDIT_FINDINGS-2026-10.md`（原仓库根） | 11,547 行 | "69 项均未实施修复" | `55c7360b` | 与 `REMEDIATION_PLAN-2026-10.md` 成对，唯一引用者是主计划 |
| `docs/archive/plans/REMEDIATION_PLAN-2026-10.md`（原仓库根） | 4,182 行 | "已获准连续实施；external-required" | 同上 | 同上 |

另外 9 份同类文档同样曾占据根目录：`branch-evidence-archive-20261019.md`（本身就是
一份归档）、`external-required-readiness-2026-09.md`（时效快照）、
`e4-read-source-line-port-eval-2026-08.md`、`lpcshell-prerequisite-plan-2026-08.md`、
`recompile-followup-plan-2026-08.md`、`recompile-object-special-design-2026-08.md`、
`tsan-ftest-report-2026-08-19.md`、`upstream-absorption-plan-d07e7641-735bd31f.md`、
`vendor-libwebsockets-manifest-2026-08.md`，加上上游遗留的 `build_v2017.md`。
已有 `docs/archive/`（74 份）存在，说明归档机制本来就有，新文档只是没有继续走进去。

七个不同基线、九份计划文档、三种 status 语义（已执行 / 已进入实施 / DRAFT）。任何新会话
都必须先读 700 KB 才能判断"现在做到哪一步"——这正是开发者体验与智能体体验的核心
成本。

### 6.2 叙事型知识文档

`docs/archive/plans/project-knowledge-map-2026-08.md`（46 KB）包含若干段 1,500–3,000 字的单段落文本，逐条
枚举 migration bridge 的字段名（`owner_local_global_record_scan_bridge_used/found/
source/skipped/reason` …）。信息密度低、无法检索、随代码漂移。它应当被替换为
"状态 + 契约 + 指向代码的表"，而不是重述实现。

### 6.3 仓库卫生

| 项 | 证据 | 影响 |
| --- | --- | --- |
| `.coveralls.yml` 含明文 `repo_token` | 文件内容 | **已泄露的凭据**，且服务（Travis）早已不用 |
| `.git/info/exclude` 承担了本该进 `.gitignore` 的规则 | `/audit-evidence/`、`/evidence/`、`/testsuite/log/`、`/testsuite/*.sqlite` | 本地排除对其他 clone 不可见；同一目录在别人机器上是未跟踪噪声 |
| `audit-evidence/` 397 个目录在本地、其中 14 个文件被 tracked | `git ls-files audit-evidence` = 14 | 状态不一致：目录整体被本地排除，却有文件在索引里 |
| 编译产物入索引 | `testsuite/test.o`、`testsuite/data/gateway_net_dead_destruct_user.o` | 仓库携带二进制 |
| 无消费者的配置/脚本 | `qodana.yaml`（无 workflow）、`fix_permission.sh`、`src/outdated_define.txt`、`.ignore` | 死配置制造"我们有这条门禁"的错觉 |
| 15 个 `build-*` 目录 | 工作区 | 已在 `.gitignore` 内，属于本地状态，不是缺陷 |

## 7 测试与门禁

### 7.1 现状（正面）

- CI 覆盖 12 个构建矩阵（Ubuntu gcc/clang × Debug/RelWithDebInfo、ASan、UBSan、TSan、
  macOS ×2、Windows ×2），另有 workflow lint、style/LPC corpus、docs、evidence gate、fuzz。
- `tools/style/check-format.py --all-native` 已作为只读门禁接入，clang-format 版本固定
  18.1.8 并校验版本。
- LPC 格式检查走 `testsuite/format.sh`（`--check` 模式），有明确排除表。
- evidence gate 校验报告 schema + `EXPECTED_TESTED_SHA`，不接受空报告。

### 7.2 缺口

| 缺口 | 证据 | 后果 |
| --- | --- | --- |
| `.clang-tidy` 无任何门禁 | `grep clang-tidy .github/workflows tools/` 零命中 | 规范 §10 声称静态分析在校准中，实际从未运行；配置会随版本漂移 |
| 无"重复实现"基线 | 无任何检测工具 | 新增复制不会失败；§4 的 1,832 个窗口无法防回归 |
| 无"新增 status key"预算 | 无检查 | ≥1,353 个 key 可继续增长 |
| C++ 测试体量失衡 | `test_lpc_owner.cc` 17,976 行 / 894 KB | 单文件无法在一次上下文内读完，局部最优会继续加长 |
| 三个 bench 各自复制装配 | §4.2 | 三个数据源可能用不同条件测同一件事 |
| `testsuite/` 携带 `.o` 与 `.sqlite` | §6.3 | 测试结果可能被旧产物污染 |

## 8 安全项

| ID | 问题 | 证据 | 严重性 |
| --- | --- | --- | --- |
| SEC-01 | `.coveralls.yml` 提交了 `repo_token` 明文 | 文件内容 | 高（历史已泄露，需吊销；文件应删） |
| SEC-02 | 伪审计能力对外宣告 ready | §5.3 | 中（信任风险：使用者会以为跨 owner 访问已被检查） |
| SEC-03 | `src/base/internal/options_internal.h` 与 `local_options` 描述的编译期开关无单一所有者 | §3.2 SM-19 | 低（配置漂移风险） |

## 9 上游处置台账（`795eb371..38a718f4`，40 个非合并提交）

判定口径：

- **已吸收**：本地源码中存在等价实现/修复（给出抽查位置）。
- **可直接吸收**：我们的同一段代码未改，修复面小，风险低。
- **需设计**：与本地架构并行（typed efun / hooks / fused store），要单独方案。
- **不适用**：文档、依赖升级、被本地架构取代。

| 提交 | 内容 | 判定 | 实际处置（2026-11） |
| --- | --- | --- | --- |
| `38a718f4` | 修 typed-efun 扫描的 Coverity 缺陷 | 需设计 | 依赖 #1454 引入的 `EfunArgs`，本地无该层 |
| `1db93cd0` `773c91c9` `3da29757` `0f103316` `59d8d2a8` `01026e1f` | docs 依赖升级（npm/yarn 组） | 不适用 | 本地 docs 用 VitePress，依赖树不同 |
| `553232a2` | `request_term_type()` 发送完整 TTYPE SEND | **已吸收** | `net/telnet.cc` 改用 `telnet_ttype_send()`，不再发未终结的 3 字节子协商 |
| `2c93fd86` | 修 `-DPACKAGE_SOCKETS=OFF` 与无 global include 文件的构建 | **可直接吸收** | 本地 CI 矩阵未验证关闭 sockets 的构建；属构建健壮性 |
| `0b09e62f` | 由 spec 生成 typed efun 绑定（`EfunArgs`/`Value`/`CallResult`，隐藏 `st_num_arg`/`sp`） | 需设计 | 本地 `efuns_main.cc` 仍有 60+ 处直接读 `st_num_arg`；属于架构迁移，不是补丁 |
| `923cdbc0` | `compress_file`/`uncompress_file` 短文件名越界读 | **已吸收** | 本地 `compress.cc` 已用 `input_path.size() >= kGzExtensionLength` 先判长度（`f_compress_file` 与 `f_uncompress_file` 两处都有）；本轮核对确认，未改动 |
| `7a66f8bf` | Coverity CID 1688265-1688271 + LPC 删除用户时的 UAF | **已部分吸收** | 延迟释放机制本地已有（`cleanup_pending` + `telnet_receive_depth` + `output_send_depth`，等价上游 InteractiveScope）；本轮补上缺失的两处“会话已丢就不再派发”检查（`process_user_command_text` 的 mxp 后、`print_prompt` 的 write_prompt 后），并为它们加了注释。hooks 相关项不适用（本地无 hooks） |
| `3b4bc500` | hooks：function/efun/net/around 连接点 | 需设计 | 全新扩展面；与 owner admission 的交互必须先设计 |
| `a26a1e06` | 复合赋值/`++`/索引/成员目标的 fused store | 需设计 | 本地 `icode.h` 无 fused 概念；会与本地 managed lvalue/async 实现冲突 |
| `ddabcc89` | 不再在校验前把 64 位 LPC int 收窄；修 `terminal_colour()` 崩溃 | **已部分吸收** | VM 核心部分（索引/lvalue/区间/slice_array/字符串迭代器）已改完并附回归测试；efun 参数批次（allocate/test_bit/member_array/replace_string/read_file 等）与 `terminal_colour` 未做，见 roadmap R-11 |
| `cadb2e8c` `2ed199b1` | Windows 测试用 `_mkdir`、检查 mkdir/unlink/rmdir 返回值 | **可直接吸收** | 测试健壮性，风险为零 |
| `970d9f09` | `__FILE__`/`__DIR__` 在 `#include` 内指回编译对象 | 待判定 | 本地 `lex.cc:3431` `add_define("__FILE__", -1, dir)` 语义需与上游对比 |
| `2a7e23e3` | `async_db_exec` 从析构里释放 callback 引用 | 待判定 | 本地 `async.cc` 的 promise 形态已改写，需按语义核对是否仍存在同一引用泄漏 |
| `01397dff` | `async_db_exec` 省略 callback 时返回 promise | 待判定 | 本地存在 `async_db_exec_promise` 错误文本（`async.cc:925`），疑似已有 |
| `a2902952` `c51f482b` `ffbd54a6` | 编译器：`catch(expr)` 不再裁剪可能出错的运算；catch 逗号/块/ref/`$()` | **可直接吸收** | 本地 `icode.cc:799-809` 只有 `F_CATCH`/`F_END_ACATCH` 基本生成；无"catch 体内"上下文位 |
| `774d1783` | 配置整行读取；超长设置报错而不是丢弃剩余 | **已吸收** | `base/internal/rc.cc:221` 起的新解析器已按整行读并 `over-long` 报错 |
| `df21db8e` `9d24db92` | 文档修正（diagnostics 链接、`telnet_suboption` 载荷语义） | 可直接吸收 | 纯文档 |
| `79548f86` | master `author_file()` 用 master 对象名探测 | 待判定 | 本地 `master.cc:123,157` 用 `add_slash(CONFIG_STR(__MASTER_FILE__))` |
| `328aaf10` | `mudlib_stats`：统计文件尚不存在时不报警 | **已吸收** | 本地 `mudlib_stats.cc` 已有 `errno != ENOENT` 判断 |
| `5e138c3c` `91de2eaf` | Coverity CID 修（tracing、多文件） | **可直接吸收** | 需按 CID 逐条核对本地是否同构 |
| `56964d9d` | 静态 float 值被截断进 int 时告警 | **需设计** | 这是新增的用户可见诊断（会改变 mudlib 编译输出），不属 bug 修复；本地无此诊断，需单独批准后做（roadmap R-12） |
| `4dd319f0` | `debug_message()` 保持主线程；删除 rc/tracing 单测 | 待判定 | 本地已有独立 trace dump worker 设计（`base/internal/tracing.cc`），语义不同，不能直搬 |
| `1b921bb3` | WebSocket 拒绝 `X-Real-IP` 解析失败的连接 | **已吸收** | 本地 `net/websocket.cc:160` 已有 trusted-proxy 校验与拒绝 |
| `b2ba3f8f` | 四个 PR 合并（错误传播、matrix/websocket 去重、安全审计、单测覆盖） | 需拆分 | 主计划 §5.2 已要求拆子项，不能整包吸收 |
| `d48a03d8` | UTF-8 分片读 / 嵌套 async 回调端到端测试 | 待判定 | 本地已有 `SocketTextAccumulator` 与 async 测试，需按用例对齐 |
| `a4fc80c5` | 修 async 快速 mysql 查询死锁 | **优先吸收** | 并发缺陷，本地 async 已改写但 mysql 路径需核对 |
| `e92b5cf2` | 规范化 master 提供的 include 目录 | 待判定 | 本地 `include` 处理有授权语义（memory: 2440），不能简单规范化 |
| `f3b8030e` | TCP 文本模式 UTF-8 读修复 | **已吸收** | 本地 `SocketTextAccumulator` + `telnet_input_trans` 分离已覆盖同类问题 |
| `de11656d` | `explode` 避免逐 token 重扫 Unicode | 待判定 | 归档的 modernization 计划称其 U07 单元已处理，需按实现核对 |
| `3f457bb1` | 修 private inherited slot 两个回归 | 待判定 | 本地已吸收 `968205b3`（B 单元），该后续修复是否在内需核对 |

统计（共 40 个提交）：已吸收 3、可直接吸收 15、优先吸收 2、待判定 9、需设计/拆分 5、
不适用 6（npm 依赖升级）。

**限制**：本表判定基于本地源码抽查，未逐 hunk 对比上游 diff；"可直接吸收"不等于
"已确认本地同构"，实施时必须先 `git show <sha>` 对比再改。

## 10 结论

1. **能力不缺。** owner 多核执行、Promise/async、重编译事务、编码边界都已成型，
   并有真实门禁。问题不在功能面。
2. **复杂度增长只有一个方向：证明自己。** 全仓 1,504 个 `add_mapping_*` 字面键
   （其中 owner 可观测面 1,058、gateway 状态面 365，下游只读 51）、≈245 处写入恒为
   常量 1、≈390 个原子计数器、19 份并行计划文档、397 个证据目录，全部是"向自己证明"
   的产物。它们不提高运行时正确性，但持续消耗每个后续会话的上下文。
3. **契约边界已实测清楚。** 下游 `main` 读两套面：owner 状态 51 个键、Worker 总线
   ≈58 个键；`rewrite` 一个也不读；`vm_object_store_status()` 在两个分支都是零消费者。
   因此本轮可删 **≈1,372 个键**（owner 面除 51 外全删 + gateway 状态面全删；
   Worker 总线与 trace/promise 不动）。
4. **因此"删"的对象是明确的**：迁移脚手架（retirement/readiness/bridge 键）、
   伪能力（`--owner-audit`、零消费者的 bench efun）、无消费者配置、并行计划文档、
   重复实现、仓库噪声。功能与既有 LPC 契约不动。
5. **验收方式清楚。** 删完后由下游在测试实例建一次 `ownercheck full`，用真实 mudlib
   证明 51 个键仍可用；这是运行验证，不是设计前置条件。
