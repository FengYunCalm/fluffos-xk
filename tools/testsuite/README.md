# 定向测试与完成合同

`run-targeted.py` 是带证据记录的定向入口。它不构建目标，不安装依赖，也不删除成功或失败的证据。
构建与测试不得在同一构建目录并行执行。代码规范见 [coding-standard.md](../../docs/coding-standard.md)。

## 运行条件

- 监督器需要 Linux `/proc`、`waitid(WNOWAIT)`、child subreaper，以及 Python 的
  `os.pidfd_open` / `signal.pidfd_send_signal`。缺少能力时拒绝运行，不降级为仅终止进程组。
- 以独立、单线程、没有既有子进程的 Python 进程运行。监督期间收养并回收本次任务的孤儿进程，
  包括切换 session 的后代；结束后恢复原 subreaper 设置。不支持嵌入共享的多线程进程。
- Python 版本号不保证系统调用接口存在。先选择提供所需接口的解释器；下面的 `python3` 指该解释器。
- C++ 验证使用对应构建缓存指向的源码副本。源码副本必须仍存在；不把当前工作区冒充旧二进制的源码。

```bash
python3 -B -c 'import os, signal; assert hasattr(os, "pidfd_open") and hasattr(signal, "pidfd_send_signal")'
python3 -B tools/testsuite/test-run-targeted.py
python3 -B tools/testsuite/test-run-targeted.py --live-driver build-dev-debug/bin/driver
python3 -B tools/testsuite/run-targeted.py --binary build-dev-debug/src/tests/lpc_tests \
  --gtest-filter='DriverTest.TestName*' --timeout 180
python3 -B tools/testsuite/run-targeted.py --driver build-dev-debug/bin/driver \
  --case /single/tests/efuns/package_trim --timeout 180
python3 -B tools/testsuite/run-targeted.py --driver build-dev-debug/bin/driver \
  --all-lpc --timeout 1800
```

`--case` 只接收一个入口；多个入口须分别调用。重复传入 `--case` 只保留最后一个值。
`--config config.recompile` 选择 `testsuite/etc/config.recompile`，不能传 `etc/` 前缀。
`--evidence-dir` 必须指向尚不存在的目录；未指定时创建私有临时目录。
`--tool EXECUTABLE -- ARG...` 只证明该工具的退出与诊断合同，不证明其内部断言或性能结论。
CTest 模式只记录调度合同；需要精确断言覆盖时使用 `--binary`。

## 文件 I/O 故障注入

Linux 的 `file_io_tests` 在测试可执行文件链接时包装 stdio/zlib 与 POSIX 启动调用。
文件故障只作用于夹具路径；进程故障只在专用场景激活，并在子进程启动前返回错误。
生产 driver 不含注入开关。测试还用私有符号链接访问 `/dev/full`，不填满磁盘。
夹具要求隔离 mudlib，CTest 通过本 runner 执行，不直接把每个 GTest 注册到源码目录。
配置需要 Python 3.9 或更新版本；若默认解释器缺少上述 pidfd 能力，用
`-DPython3_EXECUTABLE=/absolute/path/to/python3` 显式选择具备能力的解释器。

## lpcc I/O 验证

```bash
python3 -B tools/testsuite/test-lpcc-io.py --lpcc build-dev-debug/bin/lpcc
```

该工具复用上述监督器与输入身份记录，在隔离 mudlib 中验证三种输出模式、批处理 stdin、
正常 EOF、读取错误和 `/dev/full` 写入失败。重定向由独立启动器设置，随后直接 exec lpcc；
不改变被测程序，也不启动真实游戏服务。启动器失败用独立退出码区分，原始输出保存在日志。
`test-summary.json` 核对发现数、执行数和零跳过；每个场景保留实际进程记录。
满足 Linux 测试配置时，CTest 的 `lpcc_io_tests` 调用同一入口。

## trace 线程与写出验证

```bash
python3 -B tools/testsuite/test-trace-io.py \
  --probe build-dev-debug/src/tests/trace_io_probe --driver build-dev-debug/bin/driver
```

Linux 的 `trace_io_probe` 只在测试链接中检查日志调用线程并注入后台分配异常，生产 driver
没有注入开关。10 个场景覆盖正常写出、stop、并发启停、打开/写入/JSON/分配失败、
未收集事件、退出前主事件循环报告，以及真实 LPC trace 文件解析。
后台写出任务返回固定大小的错误信息；主线程在既有 wakeup/drain 或最终析构 join 后报告。
测试同时检查正常进程退出、原始日志、JSON 和零跳过，不把“进程未崩溃”单独算作通过。
WSL2 的 TSan 测试加 `--disable-aslr`；构建仍需整体使用 `setarch x86_64 -R` 包装。

## 复合值释放与退出验证

```bash
python3 -B tools/testsuite/test-compound-free.py \
  --binary build-dev-debug/src/tests/compound_free_probe \
  --evidence-dir build-dev-debug/compound-free-evidence
```

Linux 的 `compound_free_probe` 覆盖混合值、共享引用、16/17 项队列边界、宽图、深链、
静态/线程局部晚析构、显式退出、异常后复位，以及四个线程重叠排空独立队列，共 11 个场景。
进程必须正常退出并输出唯一完成标记；sanitizer、超时、遗漏执行或跳过均不算通过。
链接包装只统计测试中的 C++ 分配和回收，生产 driver 不含观察器。
冷队列场景只预热 debug 分配器的独立元数据，不提前使用复合值队列。

本脚本的 `--case` 可重复选择专项场景；这与 `run-targeted.py` 的单个 LPC `--case` 不同。
WSL2 上给本脚本加 `--disable-aslr`；运行 `run-targeted.py` 时则把整个 Python 命令放在
`setarch x86_64 -R` 后，不能给后者传入 `--disable-aslr`。
每次使用新的证据目录。真实 driver 仍须独立运行 `owner_payload`、`owner_executor_contract`
和 `nested_array_free_recursion`，不能用 probe 替代退出链和深层 LPC 图验证。

`compound_free_probe bench-narrow` / `bench-wide` 输出释放耗时、冷启动分配及保留存储。
每轮测量 2000 次释放，构造不计入耗时；用于 Release 配对测量，不是自动性能通过门禁。

## WebSocket 与连接生命周期

```bash
python3 -B tools/testsuite/test-websocket.py \
  --driver build-dev-debug/bin/driver \
  --evidence-dir build-dev-debug/websocket-evidence
```

脚本在私有 sandbox 启动本次 driver；只连接 `127.0.0.1` 的动态端口，不接触已有实例。
父进程复用单线程监督器；日志读取线程仅在受监督的工作进程中运行。TLS 客户端验证仓库的
`localhost` 测试证书，不关闭证书校验。`--case` 可重复选择帮助中列出的用例。

14 个用例覆盖 WS/WSS ascii、WS/WSS telnet、普通 telnet/TLS，以及可信/不可信代理头。
检查 Unicode 输出、消息分片、burst、exec、`net_dead` 内 destruct、关闭后用户表回收、
WS 关闭前排空和 MCCP 边界；普通 telnet 保持原有压缩能力。WS-telnet 保留原始换行，
不同于普通 telnet 的 CRLF；WS 关闭期按完整字节流核对，不要求每个二进制帧单独构成 UTF-8。
普通连接只验证小尾包断开，不把 WS 的延迟排空合同扩展到普通 `remove_interactive()`。

每个用例必须按预定检查集合完成，driver 正常退出且无 sanitizer 诊断。
`test-summary.json` 核对发现数、执行数、零跳过；原始进程证据使用上述 runner 格式。
每个 peer 的收发证据在 sandbox 的 `log/wire-*.bin`，上限 8 MiB；记录依次为方向字节
（0 发送、1 接收）、网络字节序的 32 位长度、原始字节。TLS 记录的是解密后的应用字节。

原生反例位于 `src/tests/test_lpc.cc`：`TestWebsocketPrelogonTeardownKeepsSessionIdentity`
核对未绑定/另一绑定及重复清理；`TestTransportPrelogonCleanupReleasesDescriptors`
在私有 libevent base 排空 finalizer 后核对 fd 和事件；
`TestMudPortPrelogonInvalidLengthCleansUser` 覆盖两条非法长度入口。
这些确定性反例不能由可能已完成登录的真实快速关闭试验替代。

## LPC 用例与异步完成

`testsuite/command/tests.c` 按稳定路径顺序执行。原始日志中的开始记录保留执行顺序，可用同一源码重放。
`A>`、`B>`、`C>` 是开始记录；`D>` 是完成记录；`T>` 包含能力和完成屏障；`X>` 记录预先排除的项。
普通 `C>` 用例必须执行实际断言，不能用空 `do_tests()` 或补一个恒真断言获得通过。

异步用例在 `do_tests()` 返回前调用 `/command/tests->begin_async()`，保存返回的 token。
在最后一个回调完成断言后，将 token 传给 `complete_async()`。每个 token 只能完成一次。
未知或重复 token、晚到断言失败、未完成 token、无完成记录都使运行失败。
用例不得用固定延时或直接 `shutdown(0)` 替代屏障；退出由调度器统一决定。

`testsuite/etc/test-scopes.tsv` 是入口分类的唯一数据来源，driver 与 Python 验收器分别读取并核对：

| role | 含义 |
|---|---|
| `runtime` | 普通用例；`requires` 中的能力必须全部满足，否则在执行前记为不可用 |
| `fixture` | 其他用例使用的辅助对象，不独立计为通过 |
| `coverage_gap` | 已确认缺少有效验证，不计为通过；必须保留缺口与理由 |

未登记的普通入口仍须满足断言及完成合同。不能根据运行失败、skip 或零断言结果自动追加排除项。
测试能力来自实际 driver 的编译条件和运行上下文，不根据 Python 侧猜测包配置。
正向编译样例由 `compiler/positive_compilation.lpc` 显式加载并验证，不冒充运行时值断言。

## Global include 配置

```bash
/usr/bin/python3 -B tools/testsuite/test-global-include.py \
  --driver build-organize-debug/bin/driver --evidence-dir /tmp/global-include-evidence
```

九个用例覆盖缺省、空白、引号、尖括号、未加引号、缺失文件和未闭合定界符。
每个用例在私有 sandbox 启动真实 driver，加载最小 master/simul 和编译探针；
同时检查退出状态、结果标记、空配置返回值或具体 include 错误，不以启动成功代替语义验证。

## 可选包与 Promise 引用检查

`/single/tests/efuns/package_socket_external` 用于四种 sockets/external 开关组合。
每个组合使用显式开关和独立 Debug/Release 构建目录；入口验证缺包 efun 的编译诊断，
以及 external 启用时经典回调和 Promise 两种形式的输出与完成。关闭 sockets 不应关闭
external 所需的内部实现，也不应重新暴露公共 socket efun。

`/single/tests/efuns/promise_memory_refs` 验证共享 Promise、待执行反应和执行中回调的引用。
启用 DEBUGMALLOC_EXTENSIONS 时调用真实 `check_memory`，不使用对象名豁免。
`external_promise` 是辅助对象，不能直接作为定向 `do_tests` 入口。

## 证据与失败

- 原始 stdout/stderr 字节保存在每进程日志中。解码文本仅供判定；默认每进程上限为 64 MiB，
  超限即失败。运行、终止宽限和管道/子进程收尾分别有界。
- GTest 的必需发现集合、实际开始与完成集合必须非空且一致。少跑、重复、部分 skip、
  计数矛盾及 sanitizer 诊断都不能通过。`DISABLED` 单独记录，不补入通过数。
- `summary.json` 记录状态、计数、能力和执行前排除理由。失败摘要保留错误与已有进程记录，
  不伪造尚未取得的通过数。`records.json` 保留命令、实际退出码、监督器结果、超时、时长与日志路径。
- `inputs-<sha256>.json` 记录源码（包括相关未跟踪文件）、构建缓存、生成输入、编译/链接参数、
  Python、ELF 动态链接闭包及实际隔离配置的身份。执行前后身份变化使本次证据失效。
  动态库内容哈希标识实际文件版本；不宣称已证明所有后期 `dlopen` 的加载身份。
- 子进程环境在启动边界按白名单过滤。保留必要的路径、locale/TZ、sanitizer 和本次隔离目录；
  不透传凭据、GTest 分片/重复或未知外部配置。不要把秘密放进命令参数或测试输出。

## 隔离边界

文件副本和进程监督不是 OS 安全沙箱。它们不阻止任意出站连接，也不阻止原生代码或外部程序
访问副本之外的路径。只能运行已审阅的本轮夹具、loopback 服务和受控子进程。
未知上游/下游输入或可执行脚本需要另行确认副作用与隔离能力；不得连接真实游戏服务。
