# 架构地图

> 本文替代旧 `project-knowledge-map.md` 的叙述式说明：只写"东西在哪、谁拥有什么、
> 什么不能改"，不复制实现细节。状态与待办见 [roadmap.md](roadmap.md)。

## 1 定位

FluffOS_XK 是独立维护的 FluffOS 引擎 fork：保留经典 LPC driver/VM 模型，增加受控的
owner/service 多核执行、Promise/async、重编译事务、源码与会话编码边界。游戏 mudlib、
世界数据、账号与运维策略留在下游仓库。

## 2 模块地图

| 路径 | 行数 | 职责 | 主要入口 |
| --- | --- | --- | --- |
| `src/main.cc` | 薄 | 进程入口 | `main()` |
| `src/mainlib.cc` | — | CLI、配置、master/simul_efun 装载、生命周期 | `driver_main()` |
| `src/backend.cc` | — | 事件循环、gametick、tick/walltime 事件、owner main drain | `init_backend()`、`backend()` |
| `src/comm.cc` | 2,729 | 交互连接、输入、命令分发、输出预算 | `new_user()`、`on_user_input()`、`process_user_command()` |
| `src/net/` | 2,357 | telnet、TLS、WebSocket、协议适配 | `net/telnet.cc`、`net/websocket.cc`、`net/ws_*.cc` |
| `src/vm/` | 40,568 | 对象/程序、解释器、apply、owner、worker、object store、recompile | `vm/internal/base/interpret.cc`、`vm/internal/owner.cc` |
| `src/compiler/` | 21,891 | 词法、语法、icode、codegen、诊断、编译 arena | `compile_file()` |
| `src/packages/` | 55,928 | efun 与运行时能力（core 19,947 / gateway 11,270 / parser 4,077 / sockets 3,082 …） | 各包 `.spec` + `*_main.cc` |
| `src/base/` | 5,381 | 配置、日志、文件、内存、统计、平台 | `base/internal/rc.cc` 等 |
| `src/tools/` | 2,174 | 构建期生成器 | `make_func`、`build_applies`、`make_options_defs` |
| `src/tests/` | 37,925 | C++ 回归、benchmark、fuzz | `test_lpc.cc`、`test_lpc_owner.cc` |
| `testsuite/` | — | LPC 层回归 mudlib | `etc/config.test` + `-ftest` |

## 3 三条主调用链

### 3.1 启动与主循环

```text
main()
 -> driver_main()            解析参数、读配置、初始化日志/内存/网络、装载 master 与 simul_efun
    -> init_backend()        建 event_base、self-pipe、tick 队列
    -> backend()             事件循环
       -> comm/net           连接与输入
       -> gametick            heartbeat、call_out、对象清扫
       -> owner main drain    主线程执行被要求留在主线程的 owner 任务
```

### 3.2 LPC 编译

```text
compile_file()
 -> LexStream / lex.cc       预处理器 + 词法（含 #pragma、#include、编码边界）
 -> grammar.y (Bison)        语法；无 Bison 时用 grammar.autogen.cc，两者必须同源
 -> trees.cc                 语法树
 -> icode.cc                 中间码
 -> generate.cc              program_t（含变量表、函数表、继承表）
```

改语法必须同时处理 `grammar.y`、生成的 `grammar.autogen.*`，并用 `generate_ebnf`
重建 `grammar.ebnf` / `lpc-grammar.json` / `lpc.tmLanguage.json`；不得手改生成物。

### 3.3 owner 任务

```text
调用方（comm / call_out / heartbeat / async / gateway / socket）
 -> vm_owner_enqueue_task*()          owner 邮箱（同 owner FIFO）
    或 vm_owner_enqueue_executor_task()  owner executor（受控并行）
    或 vm_owner_enqueue_main_task()      主线程队列（IO/清理/兼容回退）
 -> owner worker 执行（绑定 VMOwnerScope，检查 owner id + epoch）
 -> 结果经 owner future / message / commit 回到目标 owner
```

`vm_context()` 持有执行态、错误态、栈镜像与 owner 态；非主线程同步 object store 会被
拒绝并计数（`vm_context_object_store_sync_rejections()`）。

## 4 谁拥有什么（单一所有者表）

| 事实 | 唯一所有者 | 其他人只能 |
| --- | --- | --- |
| 对象身份、生命周期、owner id/epoch | `object_t` + `vm/internal/object_store.cc` | 通过 `ObjectHandle`、snapshot、owner message 访问 |
| 对象目录与路径索引 | owner shard（`VMObjectShard`） | 只读查询 |
| owner 任务队列与调度 | `vm/internal/owner.cc` | 入队，不直接执行 |
| future 终态与 payload | `owner_future_store.h` | poll/take，不写 |
| 重编译事务 | `vm/internal/recompile.cc`（`RecompileExecutionContext`） | 通过 efun 发起 |
| simul_efun 派发表 | `vm/internal/simul_efun.cc` | 只读派发 |
| 配置项 | `src/include/runtime_config.h` + `base/internal/rc.cc` | `get_config()`/`set_config()` |
| LPC 可观测状态键 | `docs/owner-status-contract.md`（冻结清单） | 读取已登记键 |
| 代码规范 | `docs/coding-standard.md` | — |

新增"第二份状态"之前先回答：这份事实能不能从已有所有者派生？能派生就不要存。

## 5 不可破坏的合同

1. **owner 内串行**：同一 owner 的 LPC 不并行；普通 legacy LPC 默认不进后台执行。
2. **跨 owner 只传合同值**：ObjectHandle / frozen payload / snapshot / message / future /
   commit proposal；不共享可变 array/mapping/object 引用。
3. **主线程边界**：IO adapter、cleanup adapter、documented main-required 表面必须留在
   主线程；新增回退要有原因、计数与合同测试。
4. **生命周期安全**：owner epoch、destruct、stale task 检查是安全边界，不得为简化绕过。
5. **LPC 公共契约**：efun 名称/参数/返回类型/flags 数值/存档格式不变；默认参数与
   回调顺序不变。
6. **生成物单向**：`grammar.autogen.*`、`grammar.ebnf`、`lpc-grammar.json`、
   `lpc.tmLanguage.json` 只能由源头重新生成。

## 6 读代码的推荐顺序

1. `docs/roadmap.md`（现在做到哪）
2. 本文件（东西在哪）
3. `src/vm/context.h`、`src/vm/owner.h`、`src/vm/object_handle.h`
4. `src/vm/internal/owner.cc`、`src/vm/internal/object_store.cc`
5. `src/backend.cc`、`src/comm.cc`
6. `src/compiler/internal/compiler.cc` + `lex.cc` + `grammar.y`

## 7 权威文档

| 主题 | 文档 |
| --- | --- |
| owner API | [owner-multicore-api.md](owner-multicore-api.md) |
| 运行时硬化基线 | [multicore-runtime-v4.md](multicore-runtime-v4.md) |
| 运行时验收 | [multicore-production-gate.md](multicore-production-gate.md) |
| 重编译事务 | [recompile-object-v2-design-2026-08.md](recompile-object-v2-design-2026-08.md) |
| 代码规范 | [coding-standard.md](coding-standard.md) |
| 状态键契约 | [owner-status-contract.md](owner-status-contract.md) |
