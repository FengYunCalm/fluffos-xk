# FluffOS_XK 项目规则

## 1. 文档定位与项目边界

本文件是代理进入仓库后的操作规则和架构导航，不替代完整设计文档、API 参考或发布门禁。

- 稳定的项目事实写在这里；详细语义以 `README.md` 和 `docs/` 下的归属文档为准。
- 构建选项以 `CMakePresets.json` 和 `src/CMakeLists.txt` 为准。
- 测试入口以 `src/tests/CMakeLists.txt`、各测试目标和 `testsuite/` 的实际实现为准。
- 不把当前分支、提交号、临时构建目录、机器负载或一次性 benchmark 数值写成永久规则。
- 只修改与任务相关的文件；不覆盖工作区已有改动，不提交构建产物、日志、账号、密钥或下游 mudlib 数据。
- 提交、推送、部署和其他外部副作用必须有明确授权。

FluffOS_XK 是面向现代 LPC/MUD 项目的 FluffOS 引擎维护分支。仓库提供可重建的 `driver`、`lpcc` 及相关工具；游戏 mudlib、世界数据、账号、部署配置和运维策略应保留在下游游戏仓库。

项目保留经典 FluffOS/LPC 运行模型，同时提供受控的 owner/service 多核执行、现代 LPC 合同、源码与会话编码边界、gateway/session 支持以及 VM/运行时诊断能力。普通 legacy LPC 不会因为启用多核能力而自动变成可任意后台并行执行的代码。

## 2. 仓库目录与模块分布

| 路径 | 作用 | 常见入口 |
| --- | --- | --- |
| `CMakeLists.txt` | 项目级 CMake 配置、版本信息、GTest 探测，并进入 `src/` | `cmake -S . ...` |
| `CMakePresets.json` | 本地开发、发布和 sanitizer 预设 | `cmake --preset <name>` |
| `cmake/` | CMake 查找模块和构建辅助函数 | `Find*.cmake`、`helper.cmake` |
| `src/` | driver、编译器、VM、网络、功能包、工具和 C++ 测试 | `src/CMakeLists.txt` |
| `src/base/` | 配置、日志、文件、内存、统计和平台基础设施 | `src/base/internal/` |
| `src/compiler/` | LPC 词法、语法、编译、icode、program 生成和反汇编 | `src/compiler/internal/compiler.cc` |
| `src/vm/` | LPC VM、对象/程序、apply、解释器、owner、worker 和对象存储 | `src/vm/internal/` |
| `src/packages/` | 按功能拆分的 efun 和运行时 package | `src/packages/core/`、`src/packages/<name>/` |
| `src/net/` | telnet、TLS、WebSocket 和协议适配 | `src/net/` |
| `src/tools/` | efun/options 生成、构建期辅助和预处理工具 | `src/tools/CMakeLists.txt` |
| `src/tests/` | GoogleTest、driver 回归测试、benchmark 和 fuzz 目标 | `src/tests/test_lpc.cc` |
| `testsuite/` | 使用实际 driver 执行的 LPC 层测试 mudlib | `testsuite/etc/config.test` |
| `docs/` | VitePress 文档站、API 参考、架构说明和验收记录 | `docs/index.md` |
| `compat/` | 兼容层和平台适配辅助代码 | `compat/` |
| `.github/workflows/` | CI、发布、静态分析和安全扫描 | `.github/workflows/` |
| `src/thirdparty/` | 随源码构建的第三方库 | 不做无关重构 |

## 3. 核心架构与代码入口

### 3.1 构建关系

```text
CMakeLists.txt
  -> src/CMakeLists.txt
     -> 生成 headers / grammar
     -> libdriver + packages + bundled libraries
     -> driver / lpcc / symbol / o2json / json2o / tests / benchmarks
```

根 `CMakeLists.txt` 只负责项目级配置、Git 版本推导、GTest 探测和 `add_subdirectory(src)`。大多数编译选项、第三方库、生成文件、`libdriver`、可执行文件和测试注册都在 `src/CMakeLists.txt`。

### 3.2 driver 启动与运行时

```text
src/main.cc
  -> src/mainlib.cc::driver_main()
     -> 参数解析、信号与配置初始化、master/simul_efun 加载
     -> backend 事件循环
        -> comm 输入与命令
        -> net 协议和连接
        -> heartbeat / callout / async / socket / gateway 回调
     -> VM、对象系统、apply 和 package efun
```

- `src/main.cc` 是很薄的进程入口；CLI 解析和启动生命周期在 `src/mainlib.cc`。
- `src/backend.cc` 负责事件循环及时间驱动入口。
- `src/comm.cc` 处理传统交互连接、输入和命令分发。
- `src/net/` 处理 telnet、TLS、WebSocket 等协议层。
- `src/vm/internal/` 处理解释器、对象/程序生命周期、apply、master、simul_efun、trace、对象存储、owner 和 worker。
- `src/packages/core/` 及其他 package 提供 efun 和具体运行时功能；package 的注册和链接由 CMake 统一接入 `libdriver`。

### 3.3 LPC 编译链路

```text
LPC source
  -> LexStream / lex
  -> grammar.y 或构建树中的 Bison 输出
  -> parse tree / trees / icode
  -> generate
  -> program
```

编译器主要位于 `src/compiler/internal/`：`lex.*`、`grammar.y`、`compiler.*`、`trees.*`、`icode.*`、`generate.*` 和 `disassembler.*`。`compile_file()` 是编译入口。

有 Bison 时，grammar 输出写入构建树；没有可用 Bison 时使用仓库中的 `grammar.autogen.cc` 和 `grammar.autogen.h`。修改语法时必须同时检查源 grammar、生成规则和 fallback 生成文件；构建不应把生成文件随意写回源码树。

### 3.4 VM、对象和 owner/service 边界

VM 代码分为公共头文件、`src/vm/internal/` 的运行时模块和 `src/vm/internal/base/` 的基础值/对象实现。修改对象、程序、解释器、apply 或全局状态时，先阅读调用者及对应测试，不要只在单个函数内修补共享状态问题。

owner/service 多核路径必须遵守以下边界：

- 同一 owner 内保持 LPC 串行语义。
- 普通 legacy LPC 的后台执行默认关闭；只有明确开放的路径才能进入 owner executor。
- 跨 owner 数据使用 same-owner 访问、snapshot、frozen payload、ObjectHandle、owner message/future、commit proposal 或 service shard 合同。
- 可变 array/mapping/object 引用不能绕过 owner 边界直接在线程间共享。
- main thread 保留 IO adapter、cleanup adapter、明确兼容 fallback 和 documented main-required surface；新增 fallback 必须有明确原因、计数或合同测试。
- owner、epoch、destruct 和 stale task 检查属于对象生命周期安全边界，不能为省事绕过。

详细 owner API、运行时合同和生产门禁见：

- `docs/owner-multicore-api.md`
- `docs/multicore-runtime-v4.md`
- `docs/multicore-production-gate.md`
- `src/vm/owner.h`
- `src/vm/worker.h`

## 4. 构建规则

### 4.1 构建预设

`CMakePresets.json` 是预设名称、选项和输出目录的唯一来源。使用预设时，CMake 至少满足该文件声明的版本要求；直接使用根 `CMakeLists.txt` 时，最低项目版本以根文件为准。

| 预设 | 输出目录 | 用途 | 关键约束 |
| --- | --- | --- | --- |
| `dev-debug` | `build-dev-debug/` | 日常开发和调试 | Debug、`MARCH_NATIVE=ON`、LTO 关闭 |
| `portable-release` | `build-portable-release/` | 可移植发布构建 | Release、`MARCH_NATIVE=OFF`、LTO 开启 |
| `asan` | `build-asan/` | AddressSanitizer | Debug、LTO 关闭、固定低并行 |
| `ubsan` | `build-ubsan/` | UndefinedBehaviorSanitizer | Debug、LTO 关闭 |
| `tsan` | `build-tsan/` | ThreadSanitizer | Debug、LTO 关闭 |

推荐命令：

```bash
# 日常开发
cmake --preset dev-debug
cmake --build --preset dev-debug --target driver lpcc lpc_tests --parallel 4

# C++ 测试
ctest --test-dir build-dev-debug --output-on-failure

# LPC 测试：必须从 testsuite/ 运行
cd testsuite
../build-dev-debug/bin/driver etc/config.test -ftest
```

Sanitizer 使用对应预设；ASan 编译固定 `--parallel 2`。已存在的 `build-sync/`、`build-prod/` 等目录属于本地构建目录，不是 `CMakePresets.json` 中的预设名；不要把不同构建目录的缓存和产物混用。

### 4.2 WSL2 磁盘与内存

- 构建前执行 `df -h /` 和 `free -h`。
- 不使用 `-j$(nproc)` 或其他无限制并行构建；普通构建固定 `-j4`（内存充足且确认安全时才提高到 `-j8`），ASan 固定 `-j2`。
- 同一时刻只运行一个构建目录；构建期间不要并行运行 driver、全量 `-ftest` 或 benchmark。
- 可用内存低于 2 GiB 时，先停止无关进程并清理确认可重建的旧构建输出，再开始构建。
- `ENABLE_LTO` 在非 Debug 构建默认可能开启；开发和 sanitizer 构建保持 LTO 关闭，发布构建是否开启以预设为准。
- 构建失败或无输出时，先检查 `free -h` 和 `dmesg | grep -i oom`，排除 OOM 后再分析编译错误。

## 5. 测试与验证规则

验证手段必须匹配改动对象，不默认执行与任务无关的全量门禁。

| 改动类型 | 最小验证 |
| --- | --- |
| Markdown、注释、AGENTS 或配置说明 | `git diff --check`、路径/命令/链接一致性检查 |
| C++ 行为或接口 | 受影响目标的定向构建、定向 GoogleTest/CTest |
| LPC 行为 | 从 `testsuite/` 运行对应 `-ftest:路径` |
| 内存或并发行为 | 相关 ASan/UBSan/TSan 预设和定向测试 |
| 生成器、grammar 或生成文件 | 运行生成步骤，检查源文件与生成结果一致 |
| 发布或架构级变更 | 在定向验证通过后扩大到对应全量门禁 |

报告验证时给出实际数字：通过/失败数量、失败测试、构建目标、耗时或 diff 行数。没有运行的检查不能写成已通过。

### 5.1 C++ 测试

`src/tests/CMakeLists.txt` 注册 `lpc_tests` 和 `ofile_tests`，并定义 benchmark、gateway/fuzz 等辅助目标。GTest 未找到时测试目标可能不会注册；先确认 CMake 配置输出，再决定是补依赖还是报告阻塞。

定向示例：

```bash
cmake --build --preset dev-debug --target lpc_tests --parallel 4
build-dev-debug/src/tests/lpc_tests --gtest_filter='DriverTest.TestName*'
```

### 5.2 LPC testsuite

`testsuite/etc/config.test` 中的路径相对 mudlib 根，driver 的当前工作目录决定配置和测试树的解析结果。完整测试必须从 `testsuite/` 目录运行：

```bash
cd testsuite
../build-dev-debug/bin/driver etc/config.test -ftest
```

`-f*` 是 mudlib 功能入口，不是 driver 内部测试开关：`src/mainlib.cc` 的 `case 'f'` 将参数转发给 master 的 `flag()`；当前 `-ftest` 实现位于 `testsuite/single/master.c`，再进入 `/command/tests`。不要在 `src/` 中寻找 `-ftest` 的测试实现，也不要从仓库根目录运行完整 `-ftest`。

## 6. 修改工作流

1. 修改前检查 `git status --short --branch`、相关 diff 和适用的 `AGENTS.md`；发现并行改动时不得覆盖。
2. 先读归属文档、目标模块和调用者，再选择最小修改范围；共享函数、对象生命周期、线程状态和公共接口必须检查全部调用路径。
3. 生成文件先改源头和生成规则，再重新生成；不要把构建树产物复制回源码树作为临时修复。
4. 行为改动同时补充正常、边界、失败和异常路径的测试；文档、配置、接口或用户可见行为改动同步更新归属文档。
5. 按改动类型执行第 5 节的定向验证；验证失败时保留失败事实，不绕过门禁或降低阈值。
6. 提交前检查 `git diff --check`、完整 diff、暂存区文件列表和是否混入构建产物。

## 7. 快速定位索引

| 问题 | 首先阅读 |
| --- | --- |
| CLI、启动、配置加载 | `src/mainlib.cc`、`src/base/internal/rc.cc` |
| 事件循环、命令和网络 | `src/backend.cc`、`src/comm.cc`、`src/net/` |
| LPC 编译、语法或生成 | `src/compiler/internal/compiler.cc`、`lex.cc`、`grammar.y`、`generate.cc` |
| VM 解释器和对象生命周期 | `src/vm/internal/base/interpret.cc`、`object.cc`、`program.cc`、`src/vm/internal/simulate.cc` |
| apply、master、simul_efun、efun | `src/vm/internal/apply.cc`、`src/vm/internal/master.cc`、`src/vm/internal/simul_efun.cc`、`src/packages/core/efuns_main.cc` |
| owner、worker、ObjectHandle | `src/vm/internal/owner.cc`、`owner_executor.cc`、`worker.cc`、`object_store.cc`、`src/packages/core/vm_owner.cc`、`vm_worker.cc` |
| CMake 选项和目标 | `CMakePresets.json`、`src/CMakeLists.txt`、相关子目录 `CMakeLists.txt` |
| C++ 回归测试 | `src/tests/test_lpc.cc`、`src/tests/test_ofile.cc` |
| LPC 回归测试 | `testsuite/single/master.c`、`testsuite/command/tests.c`、`testsuite/single/tests/` |
| 用户可见 API 或语义 | `README.md`、`docs/`、对应 `docs/efun/` 或 `docs/apply/` |

保持本文件描述稳定事实和可重复操作；发现目录、构建预设、测试入口或安全边界变化时，先更新归属文档，再同步修订这里的摘要。
