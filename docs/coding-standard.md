# FluffOS_XK 代码规范

本文是本仓库手写代码的统一规范。新增代码和修改后的代码按本文编写；未触及的历史代码按主计划分批整理，不以“原文件就是这样写的”为新差异辩护。

**规范已定义不等于门禁已部署。** 配置对齐、工具版本固定、自动检查、存量整理及验证统一安排在[全面整理方案 U10](project-modernization-plan-2026-10.md#u10-style)。本次制定规范不授权改源码、批量格式化、安装工具或触发外部流水线。

## 1. 范围、优先级与单一来源

1. 适用于本仓库维护的 C++17/C11、LPC 测试与示例、Python、Shell、CMake、JS/TS 和技术文档。不改变下游 mudlib 的编码风格或要求下游重命名。
2. 接口兼容、正确性、权限、所有权、线程边界和数据完整性优先于排版。不得为符合风格改变公开名称、签名、flags 数值、存档、回调顺序或错误合同。
3. 第三方原树、自动生成物、冻结的原始证据和字节敏感夹具不按普通源文件格式化。源文件扩展名不足以确定语言：LPC 的 .c/.h 不能交给 C/C++ formatter。
4. 本文解释人工规则及明确的格式选择；src/.clang-format、src/.clang-tidy、.editorconfig 和 .gitattributes 承载对应机器配置。实施时必须使配置与本文一致；不维护第二份风格指南或个人覆盖配置。
5. AGENTS.md 和 CONTRIBUTING.md 只保留规范入口及适用条件，不复制规则表。构建、授权、测试隔离和平台资源限制仍以项目规则为准。
6. 本文中的“必须/禁止”是验收要求；“优先”允许有具体语义或性能依据的例外。例外写在最接近原因的位置，说明对象、原因和验证方式；不允许无范围的文件级忽略。

<a id="upstream-basis"></a>

## 2. 上游依据与本地取舍

研究基线固定为上游 328aaf10635ef8ce636b2b6cebd549bb165e462d。本节区分上游已经定义的规则、实际写法和本地选择，不把 Google 的全部 C++ 规则隐式引入本项目。

| 依据 | 已核对的上游事实 | 本地决定 |
|---|---|---|
| [C++ 格式配置][up-format] | Google base、2 空格、100 列、不排序 include；关闭指针对齐推断，固定 type* name | 接受；补齐控制流和引用等明确配置，固定工具版本 |
| [EditorConfig][up-editor] | UTF-8、LF、空格、末尾换行；Windows 脚本和 CRLF 夹具有例外 | 接受文本基线；Python 明确 4 空格，原始编码/换行夹具另行保护 |
| [clang-tidy][up-tidy] | 函数命名允许 aNy_CasE；禁用强制尾置返回、C 数组替换、部分可读性建议 | 不照搬宽松命名或全部通配检查；按 §4 固定新名称，逐项校准检查 |
| [LPC 风格指南][up-lpc-style] | 2 空格、100 列、K&R、snake_case、LPC 数组标记与 C++ 指针排版不同 | 接受语言差异；不沿用“周围代码优先”或类名二选一 |
| [LPC formatter][up-lpc-format] | 保序、字面量保护、幂等；保留人工换行；未知/残缺输入拒绝写入 | 接受保护机制；不得把 tokenizer 自洽等同于 driver 语义等价 |
| [CI][up-ci] | 运行 LPC syntax 测试和 testsuite/format.sh --check | 借鉴自动检查；本地工具尚未接入，不能声称已有同等门禁 |
| [AGENTS 的 package/栈/内存/头文件说明][up-agents] | 强调统一 package 入口、.spec 类型分发、栈平衡、引用标记和异常清理 | 适配到本地真实调用路径，不照搬上游对象、arena 和并发实现 |
| [ws_common][up-ws]、[matrix][up-matrix] | 提取实际重复逻辑，保留协议差异；数组元素检查与顶层类型检查分开 | 作为职责拆分方法，不作为整文件替换模板 |
| [tracing][up-trace]、[字符串测试][up-tests] | 有 RAII、匿名 namespace、snake_case 方法和描述性测试名；同时仍有单行无花括号、混合 const 写法 | 吸收明确所有权和测试意图；历史排版不构成新代码的例外 |

上游也存在需要拒绝继承的问题：

- .clang-format 的说明仍写 LLVM/4，而实际配置是 Google/2。
- clang-tidy 的全大写常量/任意函数名与部分现代代码写法不一致。
- LPC 指南允许省略花括号、类名两种命名，并优先模仿现有文件；本地对新代码固定一种规则。
- LPC 指南的“格式化始终安全”不能作为保证。上游自己的 formatter/AGENTS 也记录过 token 检查通过而真实 driver 行为改变的情况。
- 上游 AGENTS 汇集大量事故细节，不复制成另一份本地长规则；只提取适用于当前架构的约束。

## 3. 文件与 C/C++ 排版

### 3.1 文本和文件名

- 普通文本使用 UTF-8、LF、末尾一个换行；删除行尾空白，缩进不用 Tab。
- Makefile 的 recipe 使用 Tab。现有 Windows 脚本的 CRLF 按 .editorconfig 保留。
- CRLF/LF、GBK/非法 UTF-8、无末尾换行等语义夹具以原字节为准；.gitattributes 的文本转换也必须核对。编辑器不得自动“修复”这些输入。
- 新 C++ 文件使用 lower_snake_case.cc/.h；新 C 文件使用 .c/.h；新 LPC 文件使用 lower_snake_case.lpc。既有 .c LPC、公开 include 路径和历史文件不为风格改名。
- 新文件不使用 final、new、v2、optimized 等阶段性后缀代替职责名；确有版本合同的名称不受此条影响。

### 3.2 C/C++ 的确定布局

| 项目 | 规则 |
|---|---|
| 基础风格 | Google 排版，不等于采纳 Google 的全部语言禁令 |
| 缩进 | 2 空格；namespace 内不额外缩进 |
| 行宽 | 100 列目标；不可拆字面量、URL、表格数据和语义夹具允许超过，不截断内容 |
| 花括号 | K&R/Attach；if/else/for/while/do 的非空语句体必须有花括号 |
| 短函数 | 只有真正空函数/空 lambda 可写成 {}；非空函数体分行 |
| case/default | 相对 switch 缩进一级，语句再缩进一级 |
| 访问标签 | public:/protected:/private: 比成员少缩进一个空格 |
| 指针/引用 | Type* ptr、Type& ref、const Type* ptr、const Type& ref；不按文件推断 |
| const | 新代码使用 const Type；指针自身只读写 Type* const ptr。formatter 不自动移动 qualifier |
| 声明 | 一行一个有意义的声明；尤其禁止 Type* a, b 这类视觉误导 |
| 空格 | if (cond)、call(arg)、a + b；逗号后空格，括号内无填充 |
| 空行 | 按逻辑块分组；通常一个空行，不用连续空行或分隔线制造层级 |
| include | 保留依赖顺序，禁止 formatter 排序或跨组移动；分组见 §5 |
| 尾注 | 代码后至少两个空格再写 //；不手工对齐大量变量、赋值号和参数列 |

多行参数、初始化列表和链式表达式交给固定版本 formatter 排版，不逐文件发明另一种悬挂缩进。显式命名返回类型放在函数名前；只有模板/泛型返回类型确实依赖参数、或语法要求时才使用尾置返回。

以下是新代码排版示意，不是新增运行时 API：

~~~cpp
constexpr size_t kBatchLimit = 64;

enum class FlushState { kPending, kDone };

class BufferWindow {
 public:
  explicit BufferWindow(size_t limit) : limit_(limit) {}

  size_t limit() const {
    return limit_;
  }

 private:
  size_t limit_;
};
~~~

### 3.3 机器配置的目标

U10 对齐 src/.clang-format 时保留多文档 YAML 结构，并核对以下值。其他未覆盖选项由固定版本的 Google base 决定，不由个人编辑器覆盖。

~~~yaml
BasedOnStyle: Google
IndentWidth: 2
ColumnLimit: 100
UseTab: Never
SortIncludes: Never
IncludeBlocks: Preserve
DerivePointerAlignment: false
PointerAlignment: Left
ReferenceAlignment: Pointer
QualifierAlignment: Leave
BreakStringLiterals: false
ReflowComments: false
BreakBeforeBraces: Attach
NamespaceIndentation: None
AccessModifierOffset: -1
IndentCaseLabels: true
SpaceBeforeParens: ControlStatements
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
AllowShortFunctionsOnASingleLine: Empty
AllowShortLambdasOnASingleLine: Empty
~~~

这是一组配置约束，不是允许格式器自动插入/删除花括号或改写表达式。新增代码的花括号由编写者保证，检查器报告缺口；不能用自动 AST 改写混入纯排版提交。

C++ 排版工具目标版本为 **LLVM clang-format 18.1.8**（[该版本选项文档](https://releases.llvm.org/18.1.8/tools/clang/docs/ClangFormatStyleOptions.html)）。这是本地选择，不是上游已经固定的版本。本次环境未发现 clang-format/clang-tidy，尚未验证该配置和二进制；U10b 必须从可信来源取得固定版本、核配置解析和样例输出后才启用。不能回退到 PATH 上的任意版本。升级 formatter 必须单独审阅输出差异。

## 4. 命名、类型和表达式

### 4.1 新代码命名表

| 对象 | 规则 | 示例 |
|---|---|---|
| C++ 类型、class、struct、类型别名、enum class | PascalCase | OwnerSnapshot、FlushState |
| C++/C 函数、方法、namespace、局部/普通全局变量、参数 | lower_snake_case | flush_pending、owner_id |
| C++ class 的数据成员 | lower_snake_case_ | pending_count_ |
| 纯数据 struct 的公开字段 | lower_snake_case | owner_epoch |
| C++ 具名常量与 enum class 枚举值 | kPascalCase | kBatchLimit、FlushState::kDone |
| C 常量/枚举、宏、构建 feature 宏 | UPPER_SNAKE_CASE | MAX_ITEMS、PACKAGE_SOCKETS |
| C++ 模板类型参数 | PascalCase | Value、Allocator |
| GoogleTest suite/case | PascalCase、说明行为与条件 | SocketOptionTest.RejectsClosedDescriptor |
| LPC 函数、变量、内部 class | lower_snake_case | do_tests、pending_tasks、class request_state |
| LPC 宏与符号常量 | UPPER_SNAKE_CASE | MAX_RETRIES |

这些规则不重命名既有 API、C/第三方 ABI、VM 的 object_t/svalue_t、.spec 的 f_xxx 约定、TEST_F suite/name 或可被下游引用的符号。命名统一不创建旧名→新名的别名层；现有公开拼写直接保留。

- 布尔量使用含义明确的 is_/has_/can_/needs_ 或状态词，例如 empty、closed；不用同义否定组合。
- 长度、容量、索引、时间标明单位或含义，例如 byte_count、timeout_ms、owner_epoch，不用 val1、data2、tmp_ok。
- 不添加匈牙利前缀、m_ 成员前缀或类型后缀来重复类型信息。局部循环 i/j、坐标 x/y、协议 fd 等范围清楚的惯用名可以保留。
- U10 校准 clang-tidy 的命名选项；当前 aNy_CasE、常量全大写和 StaticConstantPrefix 的旧值不是新规范。

### 4.2 类型与表达式

1. 能表达身份、生命周期或所有权的类型优于裸整数/多个布尔量；但不得为美化接口更换公开 flags、handle 或序列化类型。
2. std::size_t/size_t 用于容器长度，固定宽度整数用于明确位宽的 wire/持久化字段，LPC_INT/LPC_FLOAT 用于对应 VM 数值。不用 int 接收潜在窄化的容器大小而不检查。
3. 值在最窄作用域声明并初始化；新增普通对象默认成员初始化，不能使用未初始化值作为隐式状态。
4. auto 用在类型由右侧清楚给出、迭代器、lambda 或冗长模板返回值；不能遮蔽 signedness、所有权、单位和窄化。保留 auto*、auto&、const auto& 的指针/引用意图。
5. 用 nullptr，不用 0/NULL 表示新 C++ 空指针。C 代码保留 C11 写法。
6. 新 C++ 使用 static_cast 等明确转换。reinterpret_cast 只用于实际 ABI/字节布局边界，并说明布局、对齐和生命周期依据；不能用 cast 绕过类型约束。
7. std::move 只用于真实所有权转移；不对 const 对象、普通标量或可直接返回的具名局部值机械添加 move。
8. 优先范围 for；需要索引、跨容器关联、受控迭代器失效或反向遍历时使用明确索引/迭代器。
9. 条件复杂时提取有意义的局部变量或小函数；不嵌套三元表达式，不用逗号表达式压缩状态变化。

## 5. 文件组织、依赖与抽象

### 5.1 头文件和实现文件

- 新 C++ 头文件使用 #pragma once；新 C/LPC 头文件使用唯一、无保留标识符冲突的 include guard。既有 guard 不为统一形式改写。
- 头文件只公开消费者需要的声明，直接包含声明实际需要的依赖；可前置声明时不用沉重实现头。模板的必要定义除外。
- .cc 私有 helper 放匿名 namespace；不要把私有 helper 放进全局公共头，也不要在头文件使用全局 using namespace。
- 不机械给所有函数 inline、noexcept、constexpr 或所有类 final。尤其不得给可能经过 LPC error() 的路径添加 noexcept。

首个 include 按实际职责确定：

| 实现类型 | 首个依赖 |
|---|---|
| efun/package VM 实现 | base/package_api.h；不再重复包含 base/std.h |
| 依赖 driver 平台/VM 编译环境的普通实现 | base/std.h，然后本模块接口 |
| base 内部、独立纯 C++ utility、独立生成工具 | 自己的接口或真正必要的平台前置头，不为形式引入整个 VM |
| 测试 | 按现有 fixture 的初始化/宏依赖确定；不能把上游“所有 .cc 第一行 std.h”机械套给测试 |

其余 include 按本模块接口、标准库、第三方库、项目依赖分组；有平台/生成头顺序要求时保留并说明。已有 include 顺序的调整不是纯空白改动，必须独立编译验证。package_api.h 是现有统一入口，不是向其继续堆入任意依赖的理由；只有实际缺失的明确接口才直接补充所属头文件。

### 5.2 职责和去重

1. 一个函数完成一个可命名的职责；按输入解析、权限/值域检查、状态转换、输出构造拆分，不按任意行数切块。
2. 优先早返回，减少成功路径嵌套；早返回前的资源、VM 栈和状态必须已处于可清理状态。
3. 至少存在两个真实同型消费者时再提取共用 helper；helper 必须减少重复的不变量，而不是只缩短几行。
4. 不为一个调用者创建 manager、factory、strategy、事件总线或通用模板层。不提前为未提出的后端设计兼容路径。
5. 一个状态有一个权威持有者。输出 mapping/日志从状态派生，不维护仅为显示而存在的第二份可变状态。
6. 纯结构整理不调整队列预算、重试次数、并发度、权限策略或缓存策略；这些是独立行为变更。

## 6. 检查、错误处理和所有权

### 6.1 何时检查，何时不要重复检查

| 场景 | 规则 |
|---|---|
| LPC .spec 已覆盖的顶层参数类型 | 先核所有真实入口的 VM 分发检查，再决定是否移除重复判断；不能只看 .spec 就删 |
| mixed、未由元数据覆盖的 varargs、直接 native 入口 | 在真正消费值的位置保留必要检查 |
| array/mapping/class 内部元素、尺寸、值域 | 顶层类型检查不覆盖这些内容，必须按算法合同检查 |
| 网络、文件、配置、下游返回值、跨 owner 消息 | 验证输入结构、范围、身份和权限；失败明确传播 |
| callback/await/解锁/重入之后 | 原对象、epoch、迭代器、栈槽和类型可能已变化，必须重验相关不变量 |
| 仅内部可证明的不变量 | 可以 assert；需要保证的 Release 行为不能只靠 assert |

禁止把未知状态静默变成 0/空数组/成功，禁止 catch (...) {}，禁止靠重试掩盖生命周期错误。只有 API 明确定义的 best-effort 行为才允许忽略失败，并在所属边界说明。

### 6.2 错误与资源

1. 保留所属模块的错误合同。LPC error()、系统 errno、返回码和内部状态不能为了统一外观被替换成另一种公开行为。
2. 本地 error_handler/throw_error 使用 C++ throw；普通自动对象能参与异常展开。原始指针和仅在正常返回执行的手写 cleanup 不会因此自动安全。
3. 优先复用现有 RAII 容器/guard；原始 FILE、fd、libevent/ICU 资源需要正确 deleter。VM 引用不能套默认 delete 或任意共享所有权包装。
4. 一个资源只有一个实际所有者。交给 VM 栈、容器或异步任务时，说明接管发生在调用前还是成功返回后，避免 guard 与 VM 同时释放或过早 release 后泄漏。
5. 清理析构不抛错、不调用任意 LPC、不等待持有同锁的 worker。进程 fatal/abort 和非本项目 C 库的非局部跳转不在普通 RAII 成功保证内。
6. 短写、flush、close、rename、EOF 与读取错误分别判断；析构关闭资源不能代替 API 必须报告的最终写入结果。
7. 动态文本作为格式化参数，不能作为 printf 风格格式串；格式占位符与 LPC_INT、size_t、浮点等实参类型匹配。

### 6.3 VM 栈与异步

- N 个参数的首参在 sp - (N - 1)，末参在 sp；返回栈变化按 .spec 的返回类型计算，不能先 push 结果再误 pop 掉结果。
- 先执行可能 error() 的调用，取得稳定结果后再初始化/推入栈槽。不能让 STACK_INC 后留下未初始化的 svalue。
- 使用项目 assign/free/push/put 引用 API；弄清 copy、retain、move 和 borrowed。对象回调可能把栈上的 object 改成 0，不能继续按旧标签释放。
- 异步任务持有 frame-owned 数据；不捕获随当前 frame 消失的 sp 指针、临时 string_view、局部引用或无所有权裸指针。
- 新增 VM 图外引用必须接入对应 debug mark/cleanup；Debug 引用检查和 sanitizer 各有检测边界，不能互相替代。

## 7. 并发与性能写法

详细合同仍见[owner API](owner-multicore-api.md)和[运行时说明](multicore-runtime-v4.md)。这里只规定代码应如何表达约束：

- 在声明处说明函数是 main-thread-only、same-owner-only 还是可在线程间调用。跨线程传递 ObjectHandle、snapshot/frozen payload 或既有 message/future，不传普通 LPC 可变引用。
- _locked 后缀表示调用者已持有声明指定的锁；函数内部不能再锁同一个非递归 mutex。后缀不是可替代锁合同的证明。
- 锁由具名 guard 持有，作用域尽量窄。不得把 LPC callback、未知阻塞 IO、join 或跨 owner 等待移入锁内来简化代码。
- 原子 memory_order 必须对应已说明的发布/读取关系；不凭“更快”从默认序降为 relaxed。
- 对 hot path 的扫描、分配、序列化和锁有明确预算；先测量，再引入缓存/索引。新增缓存必须说明失效、owner/epoch/destruct 和内存上限。
- 不以大量内联、全局缓存、复制状态或隐式 fallback 换取微基准好看。错误路径和常规路径分开测量。
- 注释可以说明复杂度和关键边界，不把一次测试的吞吐数字写成永久性能事实。

## 8. LPC 测试与示例

### 8.1 固定排版

LPC 同样采用 2 空格、100 列、K&R 和每个非空控制流体有花括号。非空函数体分行；空 create 等可写 {}。普通逻辑块和函数之间用一个空行，不保留装饰性空白。

| 语法 | 统一写法 |
|---|---|
| 函数与控制流 | query_name()、if (ready)、foreach (name in names) |
| 调用式关键词 | catch(expr)、new("/obj")；块形式按当前语言语法书写 |
| 数组声明 | string *names；这里 * 是数组标记，不按 C++ 的 Type* 改写 |
| 数组 / mapping | ({ 1, 2 })、([ "name": value ])；空值 ({})、([]) |
| functional | (: callback :)、(: $1 + $2 :) |
| cast / 下标 | (string)value、names[0]、text[1..<2] |
| 成员、限定调用 | ob->query_name()、efun::write() |
| mapping 与默认参数冒号 | "key": value、string name: (: "default" :) |
| 指令与顺序 | #define/#include 从第 0 列开始；#include/inherit 按源码顺序，绝不排序 |

语法测试可以故意违反风格、类型或语法。必须能定位测试目的，且检查器明确分类；不能把整个 testsuite 变成风格豁免区。

### 8.2 写法和测试组织

1. 新内部辅助函数/状态默认 private；master/simul/apply、do_tests、被继承或跨对象调用的合同保持需要的可见性。不将已有函数改 private/nomask 当作风格整理。
2. 名字、值、类型和预期结果在断言中清楚表达。正例、边界、错误、异常后恢复分别覆盖；失败夹具必须证明失败原因，不能仅断言“编译失败”。
3. 使用现有测试头、fixture 和隔离 runner，不再实现一套 master 或输出统计。clone/inherit 夹具放在相应目录，避免被当成测试入口执行。
4. 测试不依赖文件运行顺序；异常路径也恢复 master hook、全局状态和持有资源。计时用有界完成条件，不用 sleep 后碰运气。
5. 不把格式调整与 .c→.lpc 改名、pragma、声明顺序、private/nomask、错误文本或返回值调整混做。
6. LPC formatter 必须理解本地语法后才能使用。现有 C/C++ 编辑器插件不是 LPC formatter；工具未验证前用本文人工排版，不假称自动检查通过。

### 8.3 LPC 格式化的安全要求

- 只处理明确识别的有效源码；保留字面量、heredoc、注释内容、指令/继承顺序和换行合同。
- stringize 宏使用参数原始拼写；保护可能在 include 或条件分支中定义的宏。不能因为当前文件没有 #define 就假定没有外部 stringize 语义。
- __LINE__、编译诊断位置、源编码和有意构造的空白可能依赖原始排版。保护明确敏感片段；无法证明安全的输入拒绝自动改写。
- 输入/输出 token 检查、字面量字节检查和幂等检查是必要检查，不是语义证明。修改 formatter 后，必须用实际本地 driver 运行格式化后的目标语料。
- 新风格检查只报告缺花括号、非空单行函数等问题；不让 formatter 自动改变语法。上游保留人工布局的行为与本地更严格作者规则须分开处理。

## 9. 辅助语言和文案

| 内容 | 固定选择 |
|---|---|
| Python | 4 空格、snake_case、100 列目标；现有已核验工具优先，不为风格新增运行依赖 |
| Shell | 2 空格；引用路径；保留 shebang 的实际 shell；管道和失败处理按真实语义编写 |
| CMake | 2 空格、lowercase 命令、路径带引号；不在排版时重排条件、探针或 target 依赖 |
| JS/TS | 2 空格、分号、单引号优先、camelCase/PascalCase；保留框架所需结构，不引入新 bundler |
| JSON/YAML | 2 空格；不重排有序列表、依赖锁或配置含义；生成 lockfile 用所属工具 |
| Markdown/注释 | 用准确术语说明约束；API、命令和源文引用不为中文表述改名 |

新内部 C/C++ 注释默认使用简明英文，与现有源码主体一致；中文术语或中文边界测试可以保留中文。规范/方案默认中文。说明“为什么必须这样做”，不逐行翻译实现。

长算法理由放归属文档，代码保留不变量与链接；许可证、作者归属和必要安全理由不能作为噪声删除。TODO 必须点明未完成行为及可定位的后续事项；不保留“以后优化”“maybe fix”一类没有出口的注释。

Shell 的 set -e/-u/pipefail 不是可以无测试批量加入的装饰。Python import/执行、工具 --help、检查器 --check 也不天然只读；按主计划核实际副作用。

## 10. 评审与工具责任

| 层次 | 工具负责 | 人工评审负责 |
|---|---|---|
| 文本/排版 | 编码与换行分类、固定 formatter 输出、行尾空白、目标范围 | 字节敏感夹具是否误入、include/宏/位置语义是否受影响 |
| 命名与普通写法 | 经校准的命名/braces 检查；报告旧符号例外 | 新名称是否准确、可见性和接口是否保持 |
| 静态分析 | 使用实际 compile_commands 和固定检查集，无 --fix 自动改写 | 告警根因、抑制理由、所有生产调用者 |
| VM/并发/生命周期 | 回归测试、sanitizer 和实际 driver 证据 | 引用转移、栈平衡、owner/锁/异常边界 |
| 性能与架构 | 前后相同条件的数据 | 是否需要抽象/缓存、复杂度与维护成本 |

提交评审前按改动范围回答：

1. 新代码是否按同一套格式和命名编写，所有例外能否解释？
2. 是否引入第二个状态来源、无消费者抽象、重复检查或无依据 fallback？
3. 是否保持参数/返回、类型、权限、栈、引用、异常和异步生命期？
4. 格式检查是否真的执行了正确语言、正确工具版本和非空目标集合？
5. 是否保留原行为，并有匹配风险的验证，而不是仅“format/lint 通过”？

工具只报告问题，默认不写源码、不自动安装、修复、提交或推送。全库统一通过模块级整理完成，不靠一次无差别格式化，也不靠永久忽略存量来宣称已经统一。具体推广步骤和完成记录只写在主计划 U10。

[up-format]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/src/.clang-format
[up-editor]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/.editorconfig
[up-tidy]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/src/.clang-tidy
[up-lpc-style]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/docs/lpc/style-guide.md
[up-lpc-format]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/docs/lpc/formatter.md
[up-ci]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/.github/workflows/ci.yml
[up-agents]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/AGENTS.md
[up-ws]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/src/net/ws_common.cc
[up-matrix]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/src/packages/matrix/matrix.cc
[up-trace]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/src/base/internal/tracing.cc
[up-tests]: https://github.com/fluffos/fluffos/blob/328aaf10635ef8ce636b2b6cebd549bb165e462d/src/tests/test_strutils.cc
