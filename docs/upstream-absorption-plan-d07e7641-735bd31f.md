# FluffOS_XK 上游吸收实施方案

## 1. 范围与基线

本方案审计并规划 upstream `fluffos/fluffos` 从 `6cf257ce` 到 `735bd31f` 的 37 个提交，针对本地基线 `d07e7641` 制定可回滚、可验证的吸收步骤。

固定事实：

- 本地基线：`d07e7641`
- upstream 目标：`735bd31f`
- 本地与 upstream 的 merge base：`277d0b1c`
- `735bd31f` 的父提交：`7c808c8b`
- 当前实现保留 owner/multicore、`VMContext`、`ObjectHandle`、future、snapshot、recompile transaction、compile arena、编码边界和 legacy LPC 语义。
- 本地没有原生 `T_PROMISE`、Promise VM、parked async frame 或 `acatch`。
- 本方案中的适用 hunk 已按本地架构实施；`faccd243` 的 `include_list()` 适配、测试和文档已完成并纳入本地复验。Promise/`T_PROMISE`、stack-lvalue ABI 和 external-handle 相关提交仍明确延期。

### 吸收原则

1. 不合并 upstream 分支，不进行批量 `cherry-pick`。
2. 以行为和 hunk 为单位分析，不以 commit ancestry 推断依赖。
3. upstream 生成文件只作为参考；本地必须修改源文件和生成规则，再重新生成。
4. compiler、lexer、file table、Promise、lvalue、EGC、诊断、配置、program layout 和 backend event 变更均视为语义或 ABI 变更。
5. 同一 owner 内保持 LPC 串行语义；跨 owner 不共享可变 array/mapping/object 引用。
6. 每个可独立回滚的候选使用单独原子提交。
7. 每个候选必须有 fail-first 回归测试、定向构建、匹配风险的 Sanitizer 验证和明确回滚条件。
8. 不修改历史 August 证据；当前基线和当前结果单独记录。

## 2. 37 个提交的最终分类

| 提交 | 分类 | 实施结论 |
|---|---|---|
| `990f7b12` | 适配吸收 | 修正 `PUSH_LOCAL`、`F_TRANSFER_LOCAL`、`F_LOCAL`、`F_VOID_ASSIGN_LOCAL` 的 local-index 检查。 |
| `97ef8d3d` | 语义移植 | 适配 block expression 的类型恢复和 `__INIT` locals 计算。 |
| `58bcc963` | 语义移植 | 适配表达式位置 block 的局部名称清理。 |
| `153ebe09` | 必须吸收 | 分离名称作用域生命周期与 slot high-water，修复局部变量相关崩溃。 |
| `bf8861c9` | 拆分吸收 | scope/slot、空 class 初始化、动态诊断格式分别处理，不整体复制。 |
| `331b456e` | 本地适配 | 将诊断输出和 warning 数量限制适配到本地 `yyerror()`、`yywarn()`、`prepare_logs()`。 |
| `3d24d7ae` | 不单独吸收 | 采用最终 `ed328800` 的常量方案，避免把原本失效的配置项变成有效限制。 |
| `9e11248f` | 语义移植 | 增加默认参数表达式中的局部声明检查。 |
| `1a0b118d` | 拆分吸收 | 吸收 `__TREE__` scope/type 修复；诊断窗口修复另行适配。 |
| `4853debc` | 随附处理 | 仅格式化测试，不单独创建提交。 |
| `52de0005` | 独立配置 | 可单独吸收 CI branch-filter 修改。 |
| `17d9d4f1` | 不吸收 | 本地不存在 `acatch`，没有对应运行时内容。 |
| `ed328800` | 拆分吸收 | 吸收最大 locals 和 `__TREE__` 的最终修复；backend hunk 需按本地实现审计。 |
| `dd2a3a14` | 独立配置 | 可单独吸收 CI 旧任务取消逻辑。 |
| `1e74a758` | 测试适配 | 采用 DB handle 隔离；Coverity `std::move` hunk 不适用于本地诊断实现。 |
| `858d5da9` | 暂缓设计 | 依赖本地不存在的 Promise/async VM，禁止直接移植。 |
| `a540f77d` | 分阶段吸收 | 先吸收 16-bit file-info 分块和 decoder bound。 |
| `de945701` | 暂缓设计 | 等 Promise/cycle walker 存在后适配。 |
| `e0ce7d26` | 暂缓设计 | 仅是 Promise drain 测试修订，等 Promise 实现后适配。 |
| `7af5c3ff` | 适配吸收 | 修复 trace buffer 溢出后无法恢复的问题。 |
| `9c673da7` | 高风险适配 | 重新实现本地 `replace_program()` no-op 判断，不能复制上游大段实现。 |
| `f3ab999d` | 语义移植 | 只移植 apostrophe/comment 处理语义，不复制 upstream lexer 生成链。 |
| `134bebd7` | 暂缓设计 | 依赖 Promise coroutine，必须使用本地 `VMContext` temporary state。 |
| `e0d6cca2` | 本地适配 | 生成 disabled-package efun 补集表并改进 undefined-function 诊断。 |
| `c80ce56f` | 独立依赖 | `docs/package-lock.json` 的 `fast-uri` 更新，和 runtime 分开。 |
| `1da7a0b6` | 独立依赖 | `docs/package-lock.json` 的 `browserslist` 更新，和 runtime 分开。 |
| `24211e79` | 适配吸收 | 吸收 EGC ASCII 优化、CRLF 排除、负长度保护和 cursor 语义。 |
| `9bce345a` | 语义移植 | 只为静态未知的 `call_other()`/`evaluate()` 结果保留 typed arithmetic assignment 类型。 |
| `11f23e20` | 测试适配 | 在本地语法和测试框架中覆盖全部 arithmetic `op=`。 |
| `a781b918` | ABI 适配 | 必须同步所有 file-info 生产者、decoder 和消费者。 |
| `faccd243` | 已适配并验证 | 以 `program_t` 内联持久 storage 记录实际打开的 include，完成 `include_list()`、生成链、文档和 LPC 回归。 |
| `ac9f6191` | 语义移植 | 与 `f3ab999d` 合并为本地 splice-first 预处理器方案。 |
| `b1745c82` | 独立文档 | 确认本地 shadowing 语义后，改写为 VitePress 本地页面。 |
| `7bcd22eb` | 暂缓设计 | 与本地全局/thread-local lvalue scratch 和 ABI 冲突，禁止直接移植。 |
| `b8dd5866` | 暂缓设计 | 依赖 Promise，并需要独立 external handle 生命周期设计。 |
| `7c808c8b` | 暂缓设计 | 等 Promise 基础设施完成后实现 combinator、cancel 和 await-foreach。 |
| `735bd31f` | 暂缓文档 | Promise 实现不存在时不添加文档；保留为未来行为验收项。 |

## 3. 依赖关系

```text
A-S0 基线、证据和 benchmark
  |
  +--> A-S1 低风险硬化
  |      +--> 990f7b12 local-index guards
  |      +--> 24211e79 EGC
  |      +--> bf8861c9/331b456e diagnostics
  |      +--> 7af5c3ff tracing
  |      +--> local backend event lifecycle audit
  |
  +--> A-S2 compiler scope/slot/type
         +--> 97ef8d3d
         +--> 58bcc963
         +--> 153ebe09
         +--> bf8861c9 scope/empty-class hunks
         +--> 9e11248f
         +--> 1a0b118d
         +--> ed328800 max-local final semantics
         +--> 9bce345a -> 11f23e20

A-S2
  |
  +--> B-S1 file-info
  |      +--> a540f77d chunking and bounds
  |      +--> a781b918 integer-word ABI
  |
  +--> B-S2 include storage / include_list
  |
  +--> B-S3 preprocessor
         +--> f3ab999d lexical contract
         +--> ac9f6191 splice-first scanning

B-S1/B-S2
  |
  +--> C-S1 package diagnostics, DB, CI, docs/dependencies
  |
  +--> C-S2 replace_program, lvalue and Promise tracks
         +--> 858d5da9
                +--> de945701
                +--> 7c808c8b
                       +--> 134bebd7
                       +--> b8dd5866
                              +--> 735bd31f
```

上述是实施依赖，不是 upstream ancestry 依赖。特别是：

- `a540f77d` 必须先于 `a781b918`，但 `a781b918` 不能只修改 `program_t`。
- `97ef8d3d`、`58bcc963`、`153ebe09`、`9e11248f`、`1a0b118d` 属于同一个 locals 不变量系列，但应拆成可验证的原子提交。
- `f3ab999d` 与 `ac9f6191` 有语义重叠，应统一测试语料，不能分别引入两套 lexer 逻辑。
- `858d5da9` 之后的 Promise 提交不能在本地 Promise substrate 不存在时预先移植。

## 4. A-S0：冻结基线和建立证据

### 4.1 Git 与对象证据

```bash
git status --short --branch
git diff --check
GIT_NO_LAZY_FETCH=1 git show --stat 735bd31f
GIT_NO_LAZY_FETCH=1 git diff 6cf257ce..735bd31f --check
```

部分 clone 若缺对象，只能对缺失对象执行定向 hydration，随后重复 `GIT_NO_LAZY_FETCH=1` 检查。不能用未读取的 patch 推断依赖。

同时记录：

- `main`、`origin/main` 和 upstream ref
- merge base 和目标 parent
- `.git/config` 中的 HTTPS URL
- 全局 URL rewrite 导致的实际 SSH transport
- 工作区是否存在并行改动

不修改 `/home/mechrevo/.gitconfig`；transport 问题只作为审计事实记录。

### 4.2 构建基线

构建前检查磁盘和内存：

```bash
df -h /
free -h
```

日常基线：

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug \
  --target driver lpcc lpc_tests --parallel 4
```

只运行与本阶段有关的 C++/LPC 测试；完整 CTest 和完整 LPC testsuite 留到最终门禁。

### 4.3 Benchmark 基线

使用现有 harness，不重写 workload：

```text
tools/perf/official_compare/run_official_vs_xk.py
tools/perf/official_compare/portable_fluffos_loadtest.py
```

固定以下场景、配置、重复次数和机器信息：

- `mixed`
- `readonly`
- `clone`
- `dispatch`
- 已知 ASCII 的 `explode()` 负载
- CPU time、RSS、median、p95

Benchmark manifest 必须记录 source commit、build preset、runtime config、workload 参数、样本数量和异常样本。没有 unchanged-harness baseline 时，不得宣称性能改进。

## 5. A-S1：低风险硬化

### 5.1 Local-index guards：`990f7b12`

修改：

```text
src/vm/internal/base/interpret.cc
```

统一验证 local index 位于当前 frame 的有效范围内。测试非法 bytecode 或 interpreter boundary，确认得到 LPC invalid-program 错误而不是越界读写。

该提交不处理默认参数 local-slot 错误；默认参数问题属于 A-S2。

门禁：

- Debug/Release 定向构建
- invalid-program 回归测试
- ASan/UBSan
- owner build 中不改变 owner affinity

### 5.2 EGC：`24211e79`

修改范围：

```text
src/base/internal/EGCIterator.h
src/base/internal/strutils.cc
src/base/internal/strutils.h
src/vm/internal/base/array.cc
src/tests/test_strutils.cc
src/tests/test_lpc.cc
testsuite/single/tests/efuns/explode.lpc
testsuite/single/tests/efuns/strsrch.lpc
```

实施要求：

1. `all_ascii()` 对 `slen < 0` 不使用 ASCII 快速路径。
2. ASCII 快速路径排除 CR，保持 CRLF 的 UAX GB3 语义。
3. `reset()` 后正确失效 ICU readiness、cursor 和 count 状态。
4. 已知 ASCII 子范围不得重复扫描。
5. `count()` 仍把 ASCII cursor 放在结束位置。
6. 保持 off-end `previous()` 的既有兼容行为。
7. 单独检查 `len() == -1` 调用路径，不误改 NUL-terminated API。
8. 保持本地 thread-local `BreakIteratorPool`。
9. 暂不顺便修复错误 include guard。

测试：

- ASCII、UTF-8 混合串
- CR、LF、CRLF
- 纯 ASCII 子范围和重置后的子范围
- `slen == -1` 及其他负值
- `count()`、`next()`、`previous()` 的 cursor
- 字符串、buffer、range lvalue
- 8000/50000 token 的 `explode()`
- ASan/UBSan
- 与 A-S0 相同 harness 的性能对比

### 5.3 诊断硬化

拆成两个提交。

#### 动态格式字符串

扫描并修改：

```text
src/compiler/internal/compiler.cc
src/compiler/internal/grammar.y
src/compiler/internal/grammar_rules.cc
src/compiler/internal/trees.cc
```

所有动态缓冲区必须使用 literal format，例如：

```cpp
yyerror("%s", buf);
yywarn("%s", buf);
```

不能改变静态格式调用的参数语义。

#### 输出限制

本地没有 upstream `render_diagnostic`，只适配：

```text
src/compiler/internal/compiler.cc
src/compiler/internal/lex.cc
src/compiler/internal/lex.h
```

保留本地 120 字符 source context 契约，另外验证：

- warning 数量有界
- 1025-byte diagnostic buffer 不溢出
- 左裁剪时 `...` 不被当作 source column
- 长宏展开不会导致异常内存增长
- 主错误消息不会因窗口化丢失

测试放入现有 `src/tests/test_lpc.cc` 和相应 LPC compiler tests。

### 5.4 Tracing：`7af5c3ff`

修改：

```text
src/base/internal/tracing.h
src/base/internal/tracing.cc
src/packages/core/trace.cc
testsuite/single/tests/efuns/trace_overflow_recovery.lpc
```

要求：

- `Tracer::start()` 无条件 collect pending buffer。
- `f_trace_start()`、`f_trace_end()` 无条件调用 collect。
- walltime trace callback 无条件 collect。
- 空 filename 仍由 collect 自己短路。
- 另用独立提交修正 `MAX_EVENTS` 边界，禁止 buffer 超容量。

测试：

- 普通 start/end
- buffer 达到 `MAX_EVENTS`
- overflow 后再次 start
- overflow 后显式 end
- walltime 自动结束
- 旧 trace 不能被写入新 trace 文件
- trace 文件必须生成
- TSan 检查与 async/owner 交互

### 5.5 Backend event lifecycle

该项不是直接复制 `ed328800`，因为本地没有 upstream-only 的 `upstream/src/backend_libevent.cc`。

已确认的本地行为：

- `add_walltime_event()` 已对 event 创建和 `event_add()` 失败进行 fatal。
- `clear_tick_events()` 会交换并清理队列，销毁 walltime event，并重置 owner-main drain 状态。
- gateway 使用 1ms continuation、bounded drain 和 `BackendEventPriority::kGateway`。

仍需处理以下返回值被忽略的入口：

```text
src/comm.cc
src/net/ws_ascii.cc
src/net/ws_telnet.cc
```

`event_base_once()` 失败不能留下已创建但永远不 logon 的 `interactive_t`、未关闭 socket 或未释放连接状态。实施前先建立 failure injection seam，再决定采用安全清理还是进程 fatal。

验证：

- event schedule failure
- socket/user 清理
- callback 前后 `clear_tick_events()`
- owner-main drain flag 重置
- gateway continuation 不饿死普通 tick
- Debug owner build、TSan

## 6. A-S2：编译器作用域、slot、类型和 assignment

### 6.1 Locals 不变量

本地实现位置：

```text
src/compiler/internal/compiler.cc
src/compiler/internal/compiler.h
src/compiler/internal/grammar.y
src/compiler/internal/grammar_rules.cc
src/compiler/internal/generate.cc
```

固定以下不变量：

```text
current_number_of_locals = 当前可见名称数量
max_num_locals          = 当前编译 frame 的 slot high-water
locals_ptr              = 当前名称作用域起点
type_of_locals_ptr      = 当前类型 slot 起点
```

必须完成：

1. block 结束时释放名称，但不降低 slot high-water。
2. `clean_up_locals()` 只清理当前 active window。
3. `pop_n_locals()` 防止负值导致越界。
4. `reallocate_locals()` 同时扩容 `type_of_locals` 和 `locals`。
5. `__INIT` 使用最终 slot high-water。
6. `catch`、`time_expression` 和 DEBUG `__TREE__` 都恢复外层 type/context。
7. 不加入本地不存在的 `acatch`。

### 6.2 默认参数和 `__TREE__`

在本地 `function_context_t` 增加 `entry_num_locals` 等价字段。在解析默认参数前保存 slot high-water，默认参数表达式中新建 local 必须报错。

DEBUG `__TREE__ {}` 必须：

- 释放 block local names
- 恢复外层 type
- 不覆盖 loop/switch context
- 不错误禁止 `break` 和 `continue`

适配测试：

```text
testsuite/single/tests/compiler/block_expr_decl_type.lpc
testsuite/single/tests/compiler/block_expr_scope.lpc
testsuite/single/tests/compiler/nested_lambda_locals.lpc
testsuite/single/tests/compiler/scope_error_recovery.lpc
testsuite/single/tests/compiler/empty_class_init.lpc
testsuite/single/tests/compiler/default_arg_decl.lpc
testsuite/single/tests/compiler/tree_block_scope.lpc
```

测试 `for`、`foreach`、`switch`、嵌套 lambda、编译错误恢复、重复 local name 和类型泄漏。

### 6.3 最大 local 变量数

不采用 `3d24d7ae` 的中间方案。当前 `CFG_INT(__MAX_LOCAL_VARIABLES__)` 存在双重 offset，原配置实际上没有正确读入 `config_int[]`。

采用 `ed328800` 的最终方向：

- `kMaxLocalVariables = 255`
- local index 与 bytecode 单字节编码一致
- `__MAX_LOCAL_VARIABLES__` 改为保留的 `__RC_INT_9__`
- 删除 `rc.cc`、`Config.example`、配置文档和测试配置中的有效设置
- 保留配置 slot 9 的数字位置，不重排 runtime config ABI
- 旧配置行继续无效，不使原本的 `64` 限制突然生效

验收：

- 200 locals 可以正确运行。
- 300 locals 必须编译失败。
- 不允许生成无法编码或运行时静默截断的 slot。
- `compile_file()`、lambda、`__INIT` 和 undefined-name path 使用同一常量。
- 不再有 `CFG_INT(__MAX_LOCAL_VARIABLES__)` 形式的 active reader。
- Debug、Release、ASan。

### 6.4 Typed compound assignment

本地规则仍在 `grammar.y`/`grammar_rules.cc`，不能复制 upstream `grammar_rules_exprs.cc`。

只处理：

- 静态未知的 `call_other()`/`evaluate()` 结果
- `this_object()->` 调用
- function pointer call
- typed `int`/`float` array element
- `+=`、`-=`、`*=`、`/=`

不处理：

- 所有 mixed 变量的强制包装
- `%=` 和 bitwise assignment
- 无条件转换所有 `TYPE_ANY`

扩展：

```text
testsuite/single/tests/operators/compound_assign_float.lpc
```

覆盖两个方向、四个 arithmetic operator、`this_object()->`、`call_other()`、`evaluate()`、function pointer 和 typed array element，并确认 mixed 变量仍保留运行时类型行为。

## 7. B-S1：file-info 行表和 ABI

### 7.1 第一阶段：`a540f77d`

修改：

```text
src/compiler/internal/compiler.cc
src/vm/internal/base/interpret.cc
src/vm/internal/base/interpret.h
```

保持旧 16-bit table 格式，但：

- 单个 count 最大为 `65535`
- 零行文件仍保留一个 entry
- decoder 增加明确 end bound
- 所有 runtime line lookup 使用同一 end bound

测试：

```text
testsuite/single/tests/compiler/huge_file_line_table.lpc
```

覆盖 65535、65536 及更大文件、include 边界、第一条错误、错误文件名和错误行号。该阶段暂不解决 file id 超过 16-bit 的问题。

### 7.2 第二阶段：`a781b918`

同步修改：

```text
src/compiler/internal/compiler.cc
src/compiler/internal/disassembler.cc
src/vm/internal/base/program.h
src/vm/internal/base/interpret.cc
src/vm/internal/base/interpret.h
```

本地 `src/main_lpcc.cc` 没有 line-info consumer，不复制 upstream 对应 hunk。

格式固定为：

```text
file_info[0] = 总分配字节数
file_info[1] = 以 lpc_file_info_t 为单位的 line-info offset
之后为 (count, file_id) int-word pairs
```

必须同步检查：

- `program_t::file_info`
- `save_file_info()`
- `epilog()` 的 allocation/copy
- `prepare_cases()`
- `translate_absolute_line()`
- `find_line()`
- `dump_line_numbers()`
- 所有 reinterpret cast 和 decoder signature

验收：

- `lpc_file_info_t` 大小有 static assertion。
- file-info reader 不再使用 `unsigned short*`。
- header offset、allocation size 和 line-info end 一致。
- 65536 行、大 file id、switch case、compile error、runtime traceback 和 disassembler 全部正确。
- ASan、UBSan、Release。

## 8. B-S2：`include_list()`

### 8.1 本地前置设计

当前 `A_INCLUDES=16` 超出本地 `NUMPAREAS=12`，因此不能直接复制 upstream `include_list()`。本地实现已完成：编译器把实际打开的 include 名称复制到 `program_t` 的同一 allocation，程序释放沿用现有 program 生命周期，efun 返回规范化且去重的 LPC array。

已落地的设计点：

1. 编译阶段记录实际打开的 include 文件，包含嵌套 include，保持 first-seen order。
2. 排除主源文件；条件禁用分支不会产生记录。
3. 在 `epilog()` 中把 include block 计入 program allocation 并复制到持久内存。
4. `program_t` 增加 include data pointer 和 size；旧 program 释放时随 allocation 一并释放。
5. 在 core spec 和 efun registration 中加入 `include_list()`。
6. 返回 LPC array，处理重复项、空/损坏 packed data 和单一 leading slash。
7. LPC 回归覆盖直接/嵌套 include、重复项、未打开 header、主源文件排除和默认对象参数；现有 object/recompile 生命周期测试覆盖持久 program 的释放路径。

文件：

```text
src/compiler/internal/compiler.cc
src/compiler/internal/compiler.h
src/vm/internal/base/program.h
src/vm/internal/base/array.cc
src/vm/internal/base/array.h
src/packages/core/core.spec
src/packages/core/efuns_main.cc
```

### 8.2 测试

```text
testsuite/clone/include_list_a.h
testsuite/clone/include_list_b.h
testsuite/clone/include_list_nest.h
testsuite/clone/include_list_skipped.h
testsuite/clone/include_list_src.lpc
testsuite/single/tests/efuns/include_list.lpc
```

覆盖直接 include、嵌套 include、重复 include、未打开 header、主源文件排除和默认对象参数；packed storage 与已有 program 生命周期一起经过 Debug、Release、ASan、UBSan、TSan 验证。

## 9. B-S3：预处理器语义

### 9.1 本地实现范围

本地只修改：

```text
src/compiler/internal/lex.cc
src/compiler/internal/lex.h
testsuite/single/tests/compiler/preprocessor.lpc
```

将 `f3ab999d` 与 `ac9f6191` 的行为统一到本地 lexer：

- apostrophe/字符字面量不能吞掉整个文件。
- 字符串、字符、backtick 内的 comment marker 保留为数据。
- directive 采用 splice-first：先处理反斜杠换行，再识别 comment 和 directive 阶段。
- header、keyword、payload 之间允许注释和空白。
- raw newline 终止 directive。
- `extract_args()` 正确处理 quote、character literal、backtick、括号、escape 和 continuation。

不复制：

```text
lexer.autogen.cc
lexer.l
lexer_scan.h
CapturedTextSrc/scan_one_unit 模板结构
```

除非本地未来明确引入相同生成链。

### 9.2 验证

测试 comment 位于 directive 各阶段、CRLF/LF、splice、EOF continuation、字符串化、宏参数、嵌套括号、字符串中的 `/*`/`//`、错误位置和宏展开。使用 compiler fuzz 以及 ASan/UBSan。

## 10. C-S1：独立运行时、生成器、测试和文档

### 10.1 Package generator

先单独修复：

```text
src/tools/build_packages_genfiles.sh
```

清理命令必须引用 `$PACKAGES_SPEC_FULL`，不能删除字面量 `PACKAGES_SPEC_FULL`。

随后适配 `e0d6cca2`：

- 生成 build-tree 中的 `packages_missing_efuns.autogen.h`。
- 只列出 disabled package 的 efun。
- 排除 enabled package 已声明的同名 efun。
- 生成 sentinel。
- 在 CMake 中声明生成文件依赖。
- 在本地 undefined-function action 中附加 package 名称。
- 动态消息继续使用 literal format。

测试 package 全开、关闭 DB/crypto 等 package、同名 efun、普通拼写错误和 clean rebuild。

### 10.2 DB testsuite

适配本地已有：

```text
testsuite/single/tests/efuns/db.c
```

不得假设 DB handle 为 `1`。每个测试独立建立、使用和关闭连接，避免 process-global handle 泄漏到后续测试。

### 10.3 CI、docs 依赖和 shadowing 文档

以下提交单独处理，不能与 runtime 混合：

```text
52de0005
dd2a3a14
c80ce56f
1da7a0b6
b1745c82
```

要求：

- CI YAML 修改后只验证 workflow 语义。
- `docs/package-lock.json` 更新后运行 docs install/build。
- `b1745c82` 改写为本地 VitePress 路径和 sidebar。
- 不导入本地不存在的 `acatch` 说明。

## 11. C-S2：高风险独立设计

### 11.1 `replace_program()` no-op

先增加 fail-first 测试：

- 相同 target program 不产生 pending swap。
- 不同 variable layout 仍执行迁移。
- 不同 function table 不误判 no-op。
- private/nomask/inherit/diamond 场景正确。
- last-call-wins 保持不变。
- staged recompile transaction 不被清除。
- `apply_lookup_table` 和 `apply_cache_items` 计数保持一致。

本地实现必须使用：

```text
src/packages/core/replace_program.cc
src/vm/internal/recompile.cc
src/vm/internal/base/program.cc
```

判断 no-op 时比较 program/inherit 等价性、变量布局、function lookup table、访问 flags、pending swap 和 staged recompile 状态。不能用 upstream 365 行实现替换本地迁移逻辑。

### 11.2 stack-resident lvalue

`7bcd22eb` 与本地以下状态冲突：

```text
global_lvalue_byte
global_lvalue_codepoint_sv
global_lvalue_range_sv
```

实现前必须确定：

- stack lvalue ownership
- nested/chained lvalue 生命周期
- string/buffer/range lvalue
- `free_indexed_lvalue()` 错误路径
- error unwind
- owner thread 隔离
- `svalue_t` ABI
- opcode 编码
- specialized assignment 对 legacy bytecode 的影响

测试至少覆盖：

```text
testsuite/single/tests/operators/assign_specialized.lpc
testsuite/single/tests/operators/index_lvalue_alias.lpc
testsuite/single/tests/operators/ref.lpc
```

另加嵌套 range、alias、catch/error、destruct 和 owner worker 场景。设计完成前禁止直接吸收。

### 11.3 Promise、async/await 和 external handle

#### Promise 基础

`858d5da9` 的 223 文件大改不能直接复制。需要先设计本地：

- Promise `svalue_t` 类型和引用计数
- mark/sweep/EGC
- Promise state machine
- reaction/continuation queue
- microtask drain budget
- owner affinity
- `VMContext` park/resume
- error context 与 stack restore
- shutdown/reclaim/destruct
- recompile 与 parked frame
- cycle detection

#### 实施顺序

```text
858d5da9  Promise/async 基础
    ↓
de945701  cycle walker 与相关修复
    ↓
7c808c8b  combinator/cancel/await foreach
    ↓
134bebd7  foreach temporary unwind
    ↓
b8dd5866  external handle + promise start
    ↓
735bd31f  promise_reject 文档和测试
```

`134bebd7` 必须保存和恢复本地 `VMExecutionState::stack_in_use_as_temporary`，不能复制上游假设全局 VM 状态的 unwind 逻辑。

#### External handle

保留旧 callback API：

```text
external_start(index, args, callback...)
```

未来新增：

```text
external_create(index, args) -> handle
external_start(handle) -> promise
```

必须定义：

- invalid index 的 `error()` 行为
- stdout/stderr 和 socket EOF
- child reaping
- nonzero exit 的 fulfill 语义
- abort/cancel 的 rejection reason
- owner destruction
- handle close
- repeated start
- child/socket 泄漏
- callback API 与 promise API 的兼容关系

`735bd31f` 只有在 Promise 实现完成后才加入：

- `PROMISE_REASON_NO_REASON` 默认 reason
- `*promise rejected` 文本
- direct self-rejection 错误
- indirect self-resolution rejection
- `promise_reject_default.lpc`

## 12. 统一验证和提交门禁

### 12.1 每个原子候选

提交前执行：

```bash
git diff --name-only
git diff --check
git status --short
```

只允许本候选文件，禁止加入构建产物、日志、账号、密钥、下游 mudlib 数据和并行改动。

### 12.2 构建

普通构建：

```bash
cmake --build --preset dev-debug \
  --target driver lpcc lpc_tests --parallel 4
```

ASan：

```bash
cmake --preset asan
cmake --build --preset asan \
  --target driver lpcc lpc_tests --parallel 2
```

高风险和最终阶段增加：

- UBSan
- TSan
- owner/multicore build
- `portable-release`
- 定向 CTest
- 最终完整 CTest
- 最终从 `testsuite/` 运行完整 LPC testsuite

LPC 测试必须从 `testsuite/` 目录运行：

```bash
cd testsuite
../build-dev-debug/bin/driver etc/config.test -ftest:<target>
```

### 12.3 Stop gate

出现以下任一情况，停止当前候选，不降低阈值：

- fail-first 测试未由失败变为通过。
- 新增 ASan/UBSan/TSan 错误。
- owner affinity 或 `VMContext` 生命周期被破坏。
- compiler slot、file-info 或 program layout 不一致。
- 旧 LPC 行为发生未解释变化。
- benchmark 出现未解释回退。
- 无法证明对象、event、Promise 或 child process 的失败路径安全。

### 12.4 回滚

每个候选单独提交，例如：

```text
port upstream 990f7b12 local-index guards
port upstream 24211e79 EGC fast paths
port upstream 7af5c3ff tracing recovery
port upstream a540f77d chunked file-info
adapt upstream a781b918 integer file-info ABI
```

回滚只使用本任务提交的 `git revert`，不使用 `reset --hard`，不清理其他改动。

## 13. 最终执行顺序

```text
A-S0  冻结基线、生成 manifest、采集 benchmark baseline
  ↓
A-S1  local-index guards
      EGC
      诊断硬化
      tracing recovery
      backend event lifecycle
  ↓
A-S2  compiler scope/slot/type
      default-argument checks
      __TREE__
      maximum local variables
      arithmetic compound assignment
  ↓
B-S1  a540 file-info chunking
      a781 integer-word file-info ABI
  ↓
B-S2  include storage 和 include_list()
  ↓
B-S3  f3 + ac9 预处理器语义
  ↓
C-S1  package diagnostics
      DB isolation
      CI、docs dependency、shadowing 文档
  ↓
C-S2  replace_program no-op
      lvalue ABI
      Promise/async/await
      external handle API
```

优先级最高且可以在当前本地架构中开始设计的候选为：

```text
990f7b12
24211e79
7af5c3ff
a540f77d
9bce345a + 11f23e20
e0d6cca2
1e74a758
```

`858d5da9`、`7bcd22eb`、`b8dd5866` 和 `7c808c8b` 不属于普通 upstream sync，必须等本地 Promise、lvalue 和 owner 生命周期设计完成后另行实施。
