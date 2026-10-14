# FluffOS_XK 审计修复方案

> 对应 [AUDIT_FINDINGS.md](AUDIT_FINDINGS.md) 的 AUD-001–AUD-069。
> **本方案已获准连续实施；执行状态以[主计划审计账本](docs/project-modernization-plan-2026-10.md#audit-remediation-ledger)为准。** 各卡保留修复步骤、依赖与验收合同，不把历史审计结果当作当前修复证据。
> 现行唯一实施主计划仍是 [docs/project-modernization-plan-2026-10.md](docs/project-modernization-plan-2026-10.md)。问题 ID、依赖和证据挂靠到该计划的对应单元；本文件不取代主计划，也不授权安装、部署、发布或回退。

## 1. 目标、范围与影响

### 1.1 验收对象

最初源码调查基线为 `a4475b1afe998bb03a37bdb0596b64ab01042ed3`；续审基线为 `55c7360bdbd81ef717290c8cd3b233da42d67724`，两者产品源码相同，仅新增两份报告，分支 `main`。上游本地快照 `79548f86f66af19f61e03a0cca1aadc95f1e8973`，merge-base `277d0b1cbc4350844d9aff07a604dfa519650d9e`。最初工作区、暂存区及非忽略未跟踪集合均为空。后续若出现他人改动，必须重新划定本单元文件范围，不覆盖、清理或夹带提交。

每个 ID 必须拥有：能击中目标根因的 fail-first 或同等静态边界证明、最小修复、正常/边界/失败回归、必要的 Sanitizer/真实装配验证、归属文档更新和可追溯提交。**不能用扫描零命中、旧日志或源码 ready 常量代替这些证据。**

本方案只修复已列问题及其必要回归。以下均不在实施范围：下游 mudlib/账号/世界数据；现有 driver 进程；覆盖既有构建目录；发布/部署/registry/tag/merge/force-push；无关重构；把全部上游 366 个非祖先提交批量合入；为了测试通过屏蔽断言、权限或 owner 检查。

### 1.2 必须保留的合同

1. 已能运行的 mudlib 和现有 LPC API 名称/返回类型保持可用。`get_char` 的整字符/控制键交付是纠正已存在的错误行为，不新增兼容别名。ASCII 行分隔语义不扩展成另一种协议。
2. owner 内串行、main IO/cleanup adapter、epoch/destruct、frozen payload、Promise 拥有引用、重编译事务边界不放宽。普通 legacy LPC 不因此获得任意后台执行权限。
3. 数据库句柄仍为现有整数；内部增加代际身份，不把旧请求自动转发给重用同一数字的新连接。
4. TLS 身份验证、输入权限、可中断预算、日志脱敏、编译诊断格式安全继续有效。
5. 文法改源头并重生成；不手改 grammar.autogen 或生成的 EBNF/JSON/TextMate 文件。
6. 规范以 [docs/coding-standard.md](docs/coding-standard.md) 为准；不因为旧文件风格不同而扩大无关重排/改名。

### 1.3 明确的兼容性/资源变更

- AUD-002/003 增加真正生效的输出/异步预算。越界必须走可识别的拒绝/关闭路径，不能部分丢数据后假报成功。无效的非正预算拒绝配置，不能把 0 偷换成无限；全局输出预算不得小于单会话预算。**拟议默认值**：每会话待发送 1 MiB、全局待发送 64 MiB；异步 worker 4、未终结请求 1024、未归还的请求独占字节 32 MiB。额度覆盖排队、执行中与等待交付，最终 cleanup 才归还。字节按请求独占的 payload/缓冲计费，LPC 回调共享对象用请求数量限额约束保留引用；不把这些上限冒称全 VM RSS 或数据库结果集的硬上限。拟新增配置键为 `maximum pending output bytes`、`maximum total pending output bytes`、`maximum async workers`、`maximum async requests`、`maximum async pending bytes`；按现有运行时配置机制接入，不另造配置系统。这些新预算只允许启动时配置；运行时 set_config 必须拒绝修改，不能在持有额度时悄悄调低上限。它们不是当前配置，也不是实测容量。批准实施须包含这些默认和可配置语义；若真实兼容场景不满足，修订本单元方案，而不是默默换值或改为无限。
- AUD-006 的非 Linux 预算会终止过去无法被预算停止的纯 LPC 忙循环；不承诺中断任意阻塞 C/外部库调用。
- AUD-007 在全局回收期间使用已有 owner 冻结窗口，窗口内按现有合同拒绝新 owner admission。拟议等待上限 250 ms 不是整个回收耗时上限；拿不到窗口时，手动调用明确失败，自动回收延期，不修改对象图。兼容性、排队/拒绝和 worker 暂停时间必须实测，不预填性能通过。
- AUD-013 对非法过长配置行明确失败，替代“静默忽略后续配置”。合法行原义不变。
- AUD-022 不再把历史自用快照冒称当前 ready；保留既有 key/值类型，缺少当前证据时为 unverified。不新增自动发布/容量平台。
- AUD-039 在 create 分配前增加既有 external 权限检查，会收紧原先允许的非特权分配政策；必须明确批准，不能称为无行为变化。AUD-040 保留 boolean 返回值，超限整次返回 0，不能截断后返回 1。
- AUD-041 为 Debug 专用测试声明真实能力依赖；优化配置的排除项单列，不补成通过。其余运行测试的零断言拒绝合同保持。

### 1.4 实施验证前置与当前证据边界

| 阻塞/前置 | 处理方式 | 影响范围 |
| --- | --- | --- |
| AUD-007 的冻结窗口需要实施并验证 | 按 AUD-007 复用已定位的 vm_owner_recompile_quiesce_begin/end。先补 fail-first，再在实际回收入口接入窗口；冻结/拒绝/异常恢复测试通过后推进遍历算法。不能以 main-thread-only 推断隔离充分，也不能在冻结失败后继续写图 | 007、018；设计入口已明确，动态验证尚未执行 |
| macOS/Windows 可运行环境 | 必须用真实原生平台验证 006/008/020；交叉编译不能冒充平台通过 | 对应单元未齐证据时保持 blocked/unverified |
| SQLite/libpq/MySQL 开发依赖与 Linux 测试包装 | 用现有包配置显式启用；缺开发库时不能把 DB 用例计为跳过通过。不连接用户数据库；真实 PostgreSQL/MySQL 集成另需私有服务授权 | 004、014、015 |
| 最新上游与安全适用性 | 已取得 6 个在线新增提交和公开查询缓存；仍须核对真实组件/patch 与告警适用性，不能宣称安全清单完整 | 新发现改变实施范围时先更新方案 |
| 本次全仓深度覆盖未全部闭合 | 不能把修完 69 项称为全仓无遗漏审计完成；逐路径/逐 hunk、来源和真实平台证据另见 §10 | 最终总结的结论强度 |

## 2. 工作顺序与里程碑

### 2.1 依赖顺序

先修验收可信度 AUD-037/038/041/042/044；它们不能通过放宽门禁过关。随后优先处理 AUD-001、AUD-029、AUD-032、AUD-062、AUD-066 及相邻安全单元。原 001–024 的依赖如下；新增 025–069 的依赖以各卡为准，没有依赖的单元按 P0→P1→P2→P3 串行进入构建。

```text
准入/基线 → (001 + 009 + 010) → 002、011 → 012
015 → 004 → 003
006 → 008
007 → 018
005、013、014、016、017、019、020、021、022、023、024 独立推进
```

001/009/010 共用 ASCII 行解析与会话生命周期，作为一个原子交付单元；不能只修 CR 前读就宣称 UAF 已消失。015/004/003 依次固定资源状态、请求身份、生产预算。不能为并行推进跳过这些依赖。同一时刻只构建一个目录；不同平台的独立验证可在各自机器执行。

### 2.2 三阶段

| 阶段 | ID/内容 | 退出条件 |
| --- | --- | --- |
| 阶段一：可信验证与止血 | 公共准入、037/038/041/042/044、全部 P0；001/009/010 同一原子单元；005 可独立修，007 先建 owner 隔离证据 | 原验收缺口有负向自测；P0 真实路径回归；隐藏输入不入日志；缺 owner 证据时 007 仍阻塞 |
| 阶段二：资源与正确性 | 按各卡依赖推进全部 P1/P2；015 先于 004、004 先于 003；同文件不并行写，单机不并行构建 | 每个 ID 满足新测试、实际装配、必要 sanitizer/平台要求；缺证/失败项不补记通过 |
| 阶段三：收口与维护 | 023/024、所有归属文档/主计划、整合回归、必要性能对照、差异与提交审查 | 69 个 ID 均有独立状态与证据；批准项不得悄悄遗漏，不越界发布或宣称全仓漏洞清零 |

工作量逐卡粗估合计 **494–956 工时**，是代码/夹具/定向验收的规划区间，不是当前耗时或交付承诺；不含等待平台、审批、外部服务、全量补充审计。可以由不同责任角色并行只读调查，但同一工作区不要并行写同一文件或混跑构建。

## 3. 公共准入与验证配方

### 3.1 真正只读的前置核对

从仓库根目录运行。下列命令只检查，不建目录、不 configure、不创建测试产物：

```bash
export GIT_OPTIONAL_LOCKS=0 GIT_NO_LAZY_FETCH=1 GIT_TERMINAL_PROMPT=0
git status --short --branch
git diff --no-ext-diff --no-textconv --stat
git diff --cached --no-ext-diff --no-textconv --stat
git rev-parse HEAD
git branch --show-current
git rev-parse refs/remotes/origin/main
git diff --check
df -h /
free -h
/usr/bin/python3 -B -c 'import os; assert hasattr(os, "pidfd_open")'
cmake --version
bison --version
```

先核对 CMakePresets.json 的最低 CMake 要求；版本不足则解决工具前置，不擅自降级 preset。逐项读本 ID 的现有位置、直接调用者、测试入口及权威文档。审计之后的改动可能移动行号，以符号/实际差异重新定位，不能机械套用旧行号。保留已有和并行改动；完整暂存区若混有归属不明内容，暂停相关版本控制操作，不暂停安全的独立读取。

**不要把以下操作称为只读：**

| 操作 | 实际副作用 | 允许时点 |
| --- | --- | --- |
| CMake configure/build、CTest/GTest 列表探测 | 写 cache、生成源、二进制，部分构建会执行 GTest 枚举 | 实施批准后，私有新构建目录 |
| Bison/EBNF 生成与 formatter --write | 改源文件/生成物 | 批准后，纳入该 ID 文件清单 |
| runner、fuzz、benchmark、部分 --check/dry-run 脚本 | 创建临时树、日志、证据或 Python/tool 缓存；可能创建进程、socket、测试文件 | 批准后，隔离目录/私有 loopback |
| Windows install | 复制产物并写 install manifest | 仅批准后的新私有 prefix，不替换现用二进制 |
| SBOM/evidence 生成 | 写 JSON/Markdown/摘要 | 批准后，不覆盖冻结历史材料 |
| Git stage/commit/push/revert | 改索引/历史/远端，push 可能触发 CI | 按项目授权；推送前核对完整待推送集合和工作流，不含部署/发布授权 |

### 3.2 批准后建立隔离工作目录

仅在本轮实施获准后执行。工作目录取当前仓库，不硬编码另一台机器路径。新目录拒绝复用，以免覆盖证据：

```bash
set -euo pipefail
umask 077
ROOT="$(git rev-parse --show-toplevel)"
PY=/usr/bin/python3
RUN_ID="$(git rev-parse --short=8 HEAD)-$(date -u +%Y%m%dT%H%M%SZ)"
E="$ROOT/build-modernization-evidence/audit-$RUN_ID"
test ! -L "$ROOT/build-modernization-evidence"
if test ! -d "$ROOT/build-modernization-evidence"; then mkdir "$ROOT/build-modernization-evidence"; fi
mkdir "$E"
mkdir "$E/tmp" "$E/home"
BD="$ROOT/build-audit-debug-$RUN_ID"
BA="$ROOT/build-audit-asan-$RUN_ID"
BU="$ROOT/build-audit-ubsan-$RUN_ID"
BT="$ROOT/build-audit-tsan-$RUN_ID"
BR="$ROOT/build-audit-gcc-relwithdebinfo-$RUN_ID"
for d in "$BD" "$BA" "$BU" "$BT" "$BR"; do test ! -e "$d" && test ! -L "$d"; done
export TMPDIR="$E/tmp" PYTHONDONTWRITEBYTECODE=1
DB_FLAGS=(-DPACKAGE_DB=ON -DPACKAGE_ASYNC=ON
  -DPACKAGE_DB_SQLITE=1 -DPACKAGE_DB_DEFAULT_DB=1
  -DPACKAGE_DB_MYSQL= -DPACKAGE_DB_POSTGRESQL=)
```

`build-modernization-evidence/` 是既有持久证据父目录；若它缺失，先创建该父目录，不在 /tmp 只保留唯一证据。主计划 §8 的源码归属/冻结规则仍适用。不能复制一个旧二进制到新位置后给它配候选源码充当同源基线。

内存低于 2 GiB 或磁盘不足时延后本任务，不擅自终止既有进程或清理旧目录。所有构建固定 **--parallel 1**；本机已经有高并行 sanitizer/CMake 不可靠的事实。构建失败/无输出先检查内存/OOM，再调查错误，不反复改代码试运气。

### 3.3 构建所需的最窄 profile

每次只配置/构建本单元需要的一项；下面是配方，不是要求立即同时建立四个树。显式的 SQLite 配置用于 DB 回归，避免把可选功能未启用算成通过。

```bash
# Debug
cmake --preset dev-debug -B "$BD" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${DB_FLAGS[@]}"
cmake --build "$BD" --target driver lpc_tests --parallel 1

# ASan；采用现有 preset 的 sanitizer 组合，不临时关闭检测
cmake --preset asan -B "$BA" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${DB_FLAGS[@]}"
cmake --build "$BA" --target driver lpc_tests --parallel 1

# UBSan；若要验证 null 类错误，确认实际 compiler flags 没有禁用相应检查
cmake --preset ubsan -B "$BU" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${DB_FLAGS[@]}"
cmake --build "$BU" --target driver lpc_tests --parallel 1

# WSL2 上 TSan：configure/build 内的 GTest 枚举也必须关闭 ASLR
setarch x86_64 -R cmake --preset tsan -B "$BT" "${DB_FLAGS[@]}"
setarch x86_64 -R cmake --build "$BT" --target driver lpc_tests --parallel 1
```

007/018/019 等边界单元必要时在 UBSan profile 指定可用的 Clang 编译器，并复查 compile_commands；不得一面使用项目对 GCC 禁用的检查，一面声称该性质已被验证。DB 的 `db_io_tests` 是 004/015 拟新增的独立目标；实现注册后只额外构建它。libpq/MySQL 包装测试另在新 DB profile 显式打开实际后端，不能直接访问用户服务。

### 3.4 统一运行接口

执行者先按各 ID 创建下文的测试名称。这些 **TestAudit*/DbIoTest 名称是拟新增的测试，不是已存在或已通过的测试**。注册后必须在 runner 的发现数中出现；空 filter、跳过、零断言或异步回调未完成均不算通过。

```bash
# 变量 B 是选定的已构建目录；F 来自各问题的精确 filter
# N 是该 ID（例如 AUD-001），每次使用全新证据目录
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"

# DB 独立目标使用同一个 runner，只更换 binary 和明确的 filter
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/db_io_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-db-debug"

# L 是下文的 LPC 对象名；路径不带 .lpc 后缀
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$B/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"

# ASan/UBSan 分别选择 BA/BU，使用不同证据目录；保留启用的检查
# TSan 同时包装 runner，子进程继承禁用 ASLR 的状态
HOME="$E/home" setarch x86_64 -R "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$BT/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-tsan"
```

运行前核对 B 的 CMakeCache 中 CMAKE_HOME_DIRECTORY 是对应候选源码；profile 的 compiler/options/二进制摘要和源输入必须匹配。runner 会复制完整测试与源码依赖，保留完成屏障、身份和覆盖合同。不要从仓库根直接运行完整 `driver etc/config.test -ftest`；直接 driver 入口必须在 testsuite cwd，但本方案优先用隔离 runner。

网络 native 夹具应执行真正的 LPC process_input/get_char/logon 逻辑。不能用 C++ 空回调替代生命周期验证；也不能为没有交互环境的 LPC 测试写“直接 return”再计为通过。涉及 libevent free 的资源断言先推进夹具自己的 event_base 完成 finalizer，不要求 bufferevent_free 返回即关闭 fd。

### 3.5 fail-first、fuzz、生成与风格

1. 先加能击中根因的回归或受控探针，保留修复前失败记录。对结构体内前读等 sanitizer 未必捕获的性质，用生产同源的内部边界函数加独立内存夹具和静态证明，不造假失败。
2. 再作最小修复，在同样输入上通过；补正常、精确边界、失败、取消/异常。信号/悬挂实验只针对 runner 自己的进程，超时必须由现有 subreaper+pidfd 监督收敛。
3. 新 LPC 文件放 `testsuite/single/tests/efuns/audit_*.lpc`，按现有格式注册 `testsuite/etc/test-scopes.tsv`；辅助对象单独分类，不计作有断言测试。故意编译失败的 LPC 由正向测试加载并断言诊断，不作为应成功的裸用例。
4. 修改 grammar.y 后，使用 **Bison 3.8.2** 执行 `tools/build/regenerate-grammar.sh`，再执行 `cmake --build "$BD" --target generate_ebnf --parallel 1`。核对 grammar.autogen.cc/.h、grammar.ebnf、lpc-grammar.json、lpc.tmLanguage.json 的实际差异。脚本和 target 都会写文件，只在批准后运行。
5. 网络 001/009/010/011/012 对每个短样本遍历全部分片位置，并增加固定种子的有界随机序列；包括空输入、边界 UTF-8、控制键、关闭回调和连续重连。编译器 016/017 枚举保护上下文/循环/嵌套函数组合。属性测试必须执行真实生产路径，不只验证模型。
6. 仅在动到 gateway framing/shared transport 时运行既有 `gateway_fuzz_smoke` 或 Clang 的 `gateway_fuzz`。后者必须显式启用 GATEWAY_FUZZ_LIBFUZZER；设定总时间、单输入超时和 RSS 上限。默认构建不能误链接无 main 的 libFuzzer target。fuzz 语料/崩溃保留在 E，不散落根目录。
7. C/C++ 使用固定版本的现有 format/tidy 工具，只格式化本单元的显式文件。LPC 使用 `testsuite/format.sh --write <本单元明确文件列表>`，原始字节和故意 lexer 错误 fixture 不格式化。最后 `git diff --check`。不得为无关遗留告警批量改名/重排。

### 3.6 性能与真实环境证据

复杂度改进必须证明真实访问次数/峰值队列与唯一对象、边数或预算一致。不能以一次 wall clock 的漂亮数字代替根因修复。对输出、异步、求值、回收及表达式保守执行的热点，使用固定 workload、同机器/配置、交替基线与候选采样。基线必须包含除当前被测变化外的所有已合入修复；源码快照和二进制都保留，按主计划 §8 记录归属。

现有 `lpc_vm_bench` 的已核验 CLI 只接受 `--json path`/`--help`，不要臆造 iterations 等参数。需要该热点对照时，在同配置的两份优化构建中只编译该目标并用隔离 runner 执行：

```bash
cmake --build "$B" --target lpc_vm_bench --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --tool "$B/src/tests/lpc_vm_bench" --timeout 600 \
  --evidence-dir "$E/$N-vm-bench-candidate-01" \
  -- --json "$E/$N-vm-bench-candidate-01.json"
```

至少采集 7 轮交替样本，保留原始指标及离散程度。**拟议性能门槛**为稳定 workload 的中位吞吐下降不超过 5%、p95 延时增加不超过 10%；这是需随实施批准的验收值，不是既有实测事实。若波动无法支持这个判断，结果记 inconclusive；若超限，保留安全修复并停止该单元的完成判定，调查根因，不能删除必要检查换取数字。协议慢读、平台计时、DB 竞争各用对应真实入口，不用 VM benchmark 替代。

### 3.7 新增任务卡的优化 profile 与重复验证

AUD-041/044 使用与 Debug 分离的 GCC 优化目录 BR；§3.2 已声明并检查该目录不存在。配置前仍须检查磁盘与可用内存。该命令保留 preset 的优化/加固策略，不关闭 FORTIFY：

```bash
cmake --preset portable-release -B "$BR" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_EXE_LINKER_FLAGS=-flto=1 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${DB_FLAGS[@]}"
# 41 只构建 driver；44 只构建 file_io_tests；分次执行，不启动第二个构建。
```

1. 依赖必须满足实际 CMake 探测。缺 PCRE/ICU/DB 等开发依赖时保持 blocked；本次依赖取证在发现报告 §10 的 E/deps，不把缺库包装成跳过通过，不私自修改系统依赖或旧构建缓存。
2. AUD-044 核对实际 compile_commands 与符号：优化产品调用确实为 __fprintf_chk，测试 wrapper 同样注册。Debug 与优化均验证两项写失败；不得通过降低加固级别消除失败。
3. AUD-025–069 的新 native filter 必须先真实注册。选择 BA/BU/BT 重跑时同时替换构建路径与证据目录后缀；TSan 包装 configure、构建枚举和 runner。不要只换目录名或复用 Debug 二进制。
4. 内存/数值卡的 UBSan profile 必须覆盖所需 null、alignment、float-cast-overflow 等检查；按卡需要选可用 Clang 并核对实际 flags，不能把未启用的检测计为通过。
5. AUD-041 的优化单例是预期拒绝场景：只有明确缺少 debug 能力的诊断才满足负向断言。其他失败、零断言成功或空 filter 都失败。Debug 必须实际运行断言；全量的配置不支持项预先排除并单列，不计通过。

## 4. 失败处置、回滚与提交

### 4.1 失败处置

- 预期的 fail-first 失败保留原日志；它不算一次“修复失败”。单次修复后失败先追踪根因，不靠屏蔽/重试掩盖。连续修复仍未解决时停止该原子单元，转只读根因调查和书面补救计划，经复核批准后再改。
- 任一断言、sanitizer、fuzz、owner affinity、异步完成、输入身份或证据清理错误，不能标完成。缺平台、库、许可或真实环境则明确 blocked/external-required。
- 排查资源不足时不得杀现有 driver、重启服务或删除既有构建/数据。成功/失败证据均保留在持久 E；无需也不得制造假的历史结果。

### 4.2 R1：每个问题采用的回滚规则

警告：回滚会改变工作区，可能影响依赖该修复的后续单元。先只读核对完整提交、依赖和工作区，再取得针对本次回退的授权。

仅逆向本单元的完整原子提交，不使用 reset/clean/强推，不恢复整目录覆盖他人改动。已经提交时，以新 revert commit 回退，先回退依赖单元再回退被依赖单元；001/009/010 必须整体回退。生成物、测试和配置/文档随对应提交一起处理，不能只回退 C++。尚未提交的修改只按已核验 diff 精确逆向，本单元以外内容不动。发生冲突先停，不自动解决归属不明内容。

回退后重跑该单元的必要正常/失败验证并记录“原问题重新出现或修复撤回”；不得回滚代码后仍保留 fixed 状态。安全修复回退可能重新暴露漏洞，运行实例处置另需授权，不能把回滚自动变成部署。

### 4.3 提交/推送与交付

每个独立交付单元完成且验证通过后精确暂存、原子提交；001/009/010 为同一交付单元。核对完整暂存区，不只看本次文件清单。推送前核对远端、分支、全部待推送提交和 CI 触发条件；若会触发部署/发布，需要对应授权。常规源码提交推送按项目常驻授权执行，不擅自新建远端、合并分支或改写历史。

提交信息包含问题 ID、根因和验证摘要；完整日志留 E，不把构建产物、账号、私有连接配置或会话 ID 加入仓库。最终逐 ID 列固定/阻塞/未执行，不以一次总测试通过替代逐项证据。

## 5. 逐问题实施卡

每张卡继承 §3/§4 的准入、profile、完成屏障和回滚要求。下列代码只展示关键修改方向，不是完整可直接套用的补丁；新增 helper 名称是方案内拟新增符号，须按步骤完成定义、生产接入和测试，不能凭示例跳过所有权/异常处理。

> 重跑约定：卡片中的 `N` 是首轮标签。同一命令的 fail-first、修复后和再次重跑须用不同标签，例如 `AUD-003-before`、`AUD-003-after-1`。runner 拒绝已存在的 evidence-dir；不得删除、覆盖或改写旧证据来重跑。


| ID | 优先级 | 依赖 | 责任角色 | 预计工时 | 验证入口 |
| --- | --- | --- | --- | --- | --- |
| AUD-001 | P0 | 公共准入 | 网络/C++ 运行时维护者 | 8–16 小时 | DriverTest.TestAuditAsciiLifetime* |
| AUD-002 | P1 | AUD-001 | 网络/C++ 运行时维护者 | 24–40 小时 | DriverTest.TestAuditOutputBudget* |
| AUD-003 | P1 | AUD-004 | 并发/运行时维护者 | 24–40 小时 | DriverTest.TestAuditAsyncBudget* |
| AUD-004 | P1 | AUD-015 | 并发/运行时维护者 | 16–32 小时 | DbIoTest.AuditConnection* |
| AUD-005 | P1 | 公共准入 | 网络/C++ 运行时维护者 | 4–8 小时 | DriverTest.TestAuditNoechoLog* |
| AUD-006 | P1 | 公共准入 | 构建/依赖维护者 | 24–40 小时 | DriverTest.TestAuditEvalBudget* |
| AUD-007 | P1 | 公共准入 | 并发/运行时维护者 | 16–32 小时 | DriverTest.TestAuditReclaimGraph* |
| AUD-008 | P1 | AUD-006 | 并发/运行时维护者 | 16–32 小时 | DriverTest.TestAuditSignalDispatch* |
| AUD-009 | P2 | AUD-001 | 网络/C++ 运行时维护者 | 2–4 小时 | DriverTest.TestAuditAsciiEmptyLine* |
| AUD-010 | P2 | AUD-001、AUD-009 | 网络/C++ 运行时维护者 | 4–8 小时 | DriverTest.TestAuditAsciiCrLf* |
| AUD-011 | P2 | AUD-001 | 网络/C++ 运行时维护者 | 16–24 小时 | DriverTest.TestAuditTelnetStreamText* |
| AUD-012 | P2 | AUD-011 | 网络/C++ 运行时维护者 | 4–8 小时 | DriverTest.TestAuditCharMode* |
| AUD-013 | P2 | 公共准入 | 构建/依赖维护者 | 4–8 小时 | DriverTest.TestAuditConfigLine* |
| AUD-014 | P2 | 公共准入 | 构建/依赖维护者 | 4–8 小时 | tools/build/test-db-config.py |
| AUD-015 | P2 | 公共准入 | 并发/运行时维护者 | 8–16 小时 | DbIoTest.AuditBackendCleanup* |
| AUD-016 | P2 | 公共准入 | 编译器/VM 维护者 | 8–16 小时 | DriverTest.TestAuditAwaitContext* |
| AUD-017 | P2 | 公共准入 | 编译器/VM 维护者 | 8–16 小时 | DriverTest.TestAuditDiscardedExpression* |
| AUD-018 | P2 | AUD-007 | 核心维护者/文档责任人 | 8–16 小时 | DriverTest.TestAuditPromiseReclaim* |
| AUD-019 | P2 | 公共准入 | 编译器/VM 维护者 | 2–4 小时 | DriverTest.TestAuditFunctionIndex* |
| AUD-020 | P2 | 公共准入 | 构建/依赖维护者 | 4–8 小时 | Windows 原生 build+私有 prefix install；产物一致性检查 |
| AUD-021 | P2 | 公共准入 | 构建/依赖维护者 | 8–16 小时 | tools/test-sbom.py |
| AUD-022 | P2 | 公共准入 | 核心维护者/文档责任人 | 8–16 小时 | DriverTest.TestAuditOwnerEvidenceState* |
| AUD-023 | P3 | 公共准入 | 核心维护者/文档责任人 | 2–4 小时 | git diff --check、文档/CLI/排除清单一致性；不要求无关构建 |
| AUD-024 | P3 | 公共准入 | 构建文档维护者 | 2–4 小时 | 文档静态核对 |

### AUD-001 · ASCII 输入回调销毁会话后继续访问 interactive_t

- **修复目标：**把既有接收期 guard 覆盖到所有直接同步回调的接收路径；回调后在持有 guard 时检查会话存活，失效立即返回。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P0 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/comm.cc:1332–1343`、`src/comm.cc:2043–2046`、`src/comm.cc:2079–2091`。
- **作用范围：**`src/comm.cc`、`src/comm.h`、`src/net/telnet.cc`、`src/tests/test_lpc_gateway.cc`、`testsuite/clone`。测试归属 `src/tests/test_lpc_gateway.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**网络/C++ 运行时维护者；8–16 小时。
- **必须击中的根因：**ASCII 分行循环跨 LPC 回调保留裸 ip；现有延迟清理仅由 TelnetReceiveGuard 的深度计数保护。owner scope 只绑定执行上下文，不延长会话生命。

#### 操作

1. 复用现有 TelnetReceiveGuard/finish_deferred_user_cleanup，不新增第二套会话引用计数。把 guard 定义移动到 get_user_data 可用的位置，并让 ASCII 接收整个调用栈持有它。
2. 回调前固定下一段游标；owner_bound_safe_apply 返回后，在 guard 仍持有分配时检查 ip->ob 和 CLOSING。只有存活时才再次处理 ip；异常返回也必须释放 guard。
3. 同时执行 AUD-009、AUD-010 的边界修正；同一分行循环作为一个原子提交，三项各保留独立测试和验收。
4. 新增实际 LPC sink 夹具，放在 clone 辅助目录，不能把它单独计成已通过的 do_tests 入口。保留 Telnet、binary、MUD 和 pre-login 现有清理测试。

#### 关键修改示例

```text
// 此处已由接收期 guard 保持 ip 的分配存活。
owner_bound_safe_apply(APPLY_PROCESS_INPUT, ip->ob, 1, ORIGIN_DRIVER, "interactive");
if (!ip->ob || (ip->iflags & CLOSING)) {
  return;
}
```

#### 定向验证

- **场景：**私有 bufferevent 夹具接入实际 LPC process_input：分别关闭当前连接、destruct 自身、关闭后建立另一会话；推进夹具 event_base 后检查旧会话只清理一次。
- **profile/真实边界：**Debug、ASan；接收路径随机分片的有界性质测试。
- **新增 native filter：**`DriverTest.TestAuditAsciiLifetime*`，目标 `lpc_tests`。

```bash
N=AUD-001
F='DriverTest.TestAuditAsciiLifetime*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

#### 完成、风险和回退

- **验收：**三类销毁/替换场景无 UAF、无重复释放、无遗留事件/fd；普通多行输入仍按原顺序回调。
- **风险与缓解：**不能用已释放 ip 做 IP_VALID 检查；不能误关替代会话；bufferevent 的最终关闭是延迟发生的。
- **文档同步：**`docs/lpc-modern-runtime.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已验证，纳入 U02 原子提交；Debug/ASan 定向与完整 `lpc_tests` 证据见主计划 U02。

### AUD-002 · 交互输出队列没有字节预算和背压

- **修复目标：**在实际入队点统一预留并归还字节预算，超限关闭具体慢会话并计数，不阻塞主事件循环或静默截断。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/net/ws_ascii.cc:148–155`、`src/net/ws_telnet.cc:123–130`、`src/net/telnet.cc:97–105`。
- **作用范围：**`src/net/ws_ascii.cc`、`src/net/ws_telnet.cc`、`src/net/telnet.cc`、`src/comm.cc`、`src/net/websocket.cc`、`src/net/websocket.h`、`src/comm.h`、`src/base/internal/rc.cc`、`src/base/internal/rc.h`、`src/include/runtime_config.h`、`src/tests/test_lpc_gateway.cc`、`src/Config.example`。测试归属 `src/tests/test_lpc_gateway.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-001 已满足；网络/C++ 运行时维护者；24–40 小时。
- **必须击中的根因：**evbuffer_add/bufferevent_write 的实际入队点未检查单会话或全局累计字节；读端限额、单次 write 长度和 libwebsockets 帧校验不能限制待发送队列。

#### 操作

1. 已加入配置 `maximum pending output bytes=1048576` 和 `maximum total pending output bytes=67108864`，单位是实际排队字节；两项必须为正数，且全局上限不得小于单连接上限，不保留 unlimited 默认。
2. 已把统一预算检查放到 `comm_queue_output()`，覆盖 bufferevent、WS ASCII 和 WS Telnet 的实际追加路径；各传输只负责提供目标 evbuffer 和完成通知，不复制计数规则。
3. 已使用 `incoming <= limit - pending` 形式避免加法溢出；成功入队后记账，失败、发送后的 drain、连接清理均归还。保留 `lws_write` TLS-expanded-return 合同，按请求 payload 长度 drain。
4. 已增加 main-thread 计数和原因日志，日志只记录长度/状态，不记录输出正文；预算拒绝会关闭具体慢会话，不阻塞事件循环或静默截断。
5. native 夹具通过真实 LPC `write()` 触发输出，覆盖单连接边界、全局累计、拒绝关闭、队列 drain 和生命周期清理；验证仅使用私有测试对象/缓冲区，不接触既有 driver。

#### 关键修改示例

```text
if (pending > limit || len > limit - pending) {
  // 请求关闭当前慢会话；不得截断后继续报告成功。
  reject_output_for_current_session();
  return;
}
```

#### 定向验证

- **场景：**在私有 loopback/受控传输夹具暂停读端；检查精确边界、单块超限、累计超限、恢复读取、关闭和 TLS 返回值大于 payload 的场景。
- **profile/真实边界：**Debug、ASan、实际私有 loopback 慢读；吞吐对照。
- **新增 native filter：**`DriverTest.TestAuditOutputBudget*`，目标 `lpc_tests`。

```bash
N=AUD-002
F='DriverTest.TestAuditOutputBudget*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

#### 完成、风险和回退

- **验收：**队列峰值不越过配置预算，拒绝后具体会话关闭且全局计数最终归零；正常字节流、WebSocket drain 方向和回调顺序不变。当前 Debug/ASan 的 `lpc_tests` 均为 524/524，`DriverTest.TestAuditOutputBudget*` 均为 5/5，`DriverTest.TestAuditAsciiLifetime*` 均为 4/4；AUD-002 的新增回归为 5/5，既有 ASCII 生命周期回归为 8/8。受控 AF_UNIX loopback 已覆盖暂停读端、恢复读取和队列 drain；独立吞吐对照基准尚未执行，不能宣称性能门槛已验证。
- **风险与缓解：**新增资源限制会改变超额输出行为；默认值是方案建议而非现有合同或容量测量。编码、压缩、分片、关闭都必须归还正确字节数。
- **文档同步：**`docs/driver/config.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**源码、配置、文档、直接回归和受监督 runner 已实施；Debug/ASan 完整日志为 `build-modernization-evidence/audit-u02-rerun-20261014T000000Z/debug-full-after-loopback-fix.log` 和 `asan-full-after-loopback-fix.log`，均为 524/524；此前 loopback 测试的失败日志保留在同一证据目录，修复后定向日志为 `debug-output-budget-after-callback-fix.log` 和 `asan-output-budget-after-callback-fix.log`。使用 `/usr/bin/python3` 的受监督 runner 在 Debug/ASan 各选中 5 项并通过，证据目录分别为 `build-modernization-evidence/audit-u02-rerun-20261014T000000Z/AUD-002-native-debug-systempy/` 和 `AUD-002-native-asan-systempy/`；同一私有构建中的直接 GTest 结果已持久化。配置零值拒绝的原始证据仍保留在 `build-modernization-evidence/audit-u02-debug-current/config-rejection-after-1/`；此前默认 Python 因 pidfd 能力不足拒绝的记录不作为当前通过依据。受控 AF_UNIX loopback 慢读/恢复读取已验证；独立吞吐对照基准尚未执行，固定 clang-format 18.1.8 不可用，不宣称性能或格式门禁完成。

### AUD-003 · 异步文件/数据库执行没有工作线程和排队内存上界

- **修复目标：**复用现有请求队列、唤醒和 owner completion，改为有界 worker 数、请求数及字节预算；getdir 在分配处限制目录项。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/packages/async/async.cc:284–299`、`src/packages/async/async.cc:449–474`。
- **作用范围：**`src/packages/async/async.cc`、`src/packages/async/async.h`、`src/base/internal/rc.cc`、`src/include/runtime_config.h`、`src/tests/test_lpc.cc`、`src/tests/test_lpc_support.h`、`src/tests/test_lpc_owner.cc`、`testsuite/single/tests/efuns/audit_async_budget.lpc`、`testsuite/etc/test-scopes.tsv`、`src/Config.example`、`src/vm/internal/simulate.cc`。文档同步为 `docs/driver/config.md`、`docs/efun/async/async_read.md`、`docs/efun/async/async_getdir.md`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-004 已满足；并发/运行时维护者；24–40 小时。
- **必须击中的根因：**缺少覆盖 queued、running、finished-undelivered 的统一准入预算；MAX_ARRAY_SIZE 只限制交付结果，没有约束生产时的分配。

#### 操作

1. 沿用总原则冻结的 maximum async workers、maximum async requests、maximum async pending bytes 及默认值。额度覆盖排队、执行中、主线程/owner executor 等待交付，只有最终 cleanup 归还。新预算只允许启动配置。
2. 在本 async 模块内维护固定上限的 joinable worker 集合、请求队列和 condition_variable。锁内按存活 worker 数而不是 reqs.empty() 判断创建；空闲 worker 等待队列，异常与线程创建失败按既有请求错误合同清理。不得另外造通用调度框架，不再 detach。
3. 保留 complete_all_asyncio() 的可重复 drain 和 callback 递归提交语义，不把它改为永久关闭入口。每轮先 check_reqs()/推进 main delivery；再在各自锁内取队列、current_works、finished 和 live_requests/新 in-flight 计数快照。计入已投递到 owner executor、尚未 cleanup 的请求。释放锁后再 yield/等待，不能持 reqs_lock 等待。
4. 新增仅用于最终退出的 stop_async_workers()，声明在 async.h。在 shutdownMudOS() 中保留 prepare_shutdown -> 首次 complete_all_asyncio 的次序；在 vm_worker_stop()/vm_owner_thread_stop() 已停止其他生产者后、external/promise/db/端口/VM 清理之前调用它。该函数先再 drain 一次，允许现有 callback 链完成，再在 reqs_lock 内关闭准入、通知空闲 worker，锁外 join 全部 worker。结束断言队列、in-flight 和 worker 都归零。
5. 本次不声称能强制取消任意阻塞 native IO，也不承诺有界的整个 shutdown 时长；既有 complete_all_asyncio 本就等待 IO。不得用 detach/强杀线程/提前清理 DB 与 Promise 换取退出。真实 DB 超时属于对应部署/连接合同；受控关闭测试中的 IO 必须由夹具明确完成。
6. getdirthread 在每次 resize 前检查 MAX_ARRAY_SIZE 与请求独占字节预算。达到原 MAX_ARRAY_SIZE 时按既有合同截断并保留 readdir 次序；新增字节预算不足则按请求错误合同交付和清理，不能把不完整目录假报为成功。不能先缓存所有项再截断。
7. 补正常/拒绝/worker 异常/线程创建失败/取消/owner 析构及两轮嵌套回调测试。增加 owner-executor callback 已入队但未 cleanup 场景，防止 drain 提前返回。最终关闭测试在独立受监督子进程验证，不把共享测试进程的 pool 永久关闭。保留已有 TestAsyncNestedCallbacksDrainAllRequests 和 TestAsyncThrowingCallbackStillDrainsNestedRequest。

#### 关键修改示例

```text
// 必须在 reqs_lock 内同时计入 queued/running/finished。
if (live_request_count >= request_limit ||
    request_bytes > byte_limit - reserved_bytes) {
  return reject_async_request(data);
}
```

#### 定向验证

- **场景：**用可控工作屏障提交超过上限的正常短任务，核对线程数、拒绝合同、回调唯一性、取消/关闭归还；大目录夹具必须受控并位于隔离根。
- **profile/真实边界：**Debug、ASan、TSan；实际异步 LPC 完成屏障。
- **新增 native filter：**`DriverTest.TestAuditAsyncThreadBudget*`，目标 `lpc_tests`；同时保留下面两个既有 drain 回归。

```bash
N=AUD-003
F='DriverTest.TestAuditAsyncThreadBudget*:DriverTest.TestAsyncNestedCallbacksDrainAllRequests:DriverTest.TestAsyncThrowingCallbackStillDrainsNestedRequest:DriverTest.TestControlledShutdownPreparesMudlibBeforeRuntimeTeardown'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_async_budget`。测试源是 `testsuite/single/tests/efuns/audit_async_budget.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-003
L=audit_async_budget
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**活动线程、请求和字节峰值不越过预算；每个被接纳请求只交付/清理一次；退出后队列、worker、owner 引用均回到基线。
- **风险与缓解：**不得另建与 owner 调度并行的通用框架；拒绝路径不能遗失 Promise/回调和 live_requests 引用标记。 complete_all_asyncio 仍是 drain，不是 stop；最终 pool stop 放在其他生产者停止之后、共享资源清理之前。不得承诺未实现的 native IO 强制取消。
- **文档同步：**`docs/driver/config.md`、`docs/efun/async/async_read.md`、`docs/efun/async/async_getdir.md`。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已实施，核心受监督定向验证闭合：Debug、ASan、TSan 的 native 过滤器各 `4/4`（`audit-u03-debug-native-r10/`、`audit-u03-asan-native-r10/`、`audit-u03-tsan-native-r10/`），Debug `lpc_tests` 全量 `525/525`（`audit-u03-gtest-debug-full-r10/`）；新增 async LPC 在 Debug、ASan、TSan 各 `1/1`（`audit-u03-lpc-debug-r10/`、`audit-u03-lpc-asan-r10/`、`audit-u03-lpc-tsan-r10/`），既有 `/single/tests/efuns/async` getdir/read/write 回归也在三配置各 `1/1`（`audit-u03-lpc-async-debug-r1/`、`audit-u03-lpc-async-asan-r1/`、`audit-u03-lpc-async-tsan-r1/`），均无跳过、失败或 sanitizer 诊断。初次 ASan 运行因夹具第二批次未清零计数而保留 `3/4` 失败证据（`build-modernization-evidence/audit-u03-asan-native-r3/`），修正夹具后复验通过。TSan 构建/运行使用 `setarch x86_64 -R`；LPC 监督 runner 使用 `/usr/bin/python3.12`，系统默认 Python 3.14 缺少 pidfd API，runner 按 fail-closed 拒绝降级。`tools/style/check-format.py` 因本机缺少要求的 clang-format 18.1.8 未能运行，故格式门禁仍未验证。未将真实数据库、强制线程创建耗尽、任意阻塞 native IO 取消或长时生产压力写成已验证；这些边界仍按本卡第 5 条保留。

### AUD-004 · 异步数据库请求只保存可复用整数连接号

- **修复目标：**内部 generation+连接发布状态；在准入时锁内取代号，执行时锁内验证同一连接；两个异步 API 共用这条路径。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/packages/async/async.cc:402–418`、`src/packages/async/async.cc:554–564`、`src/packages/db/db.cc:639–662`。
- **作用范围：**`src/packages/async/async.cc`、`src/packages/db/db.cc`、`src/packages/db/db.h`、`src/tests/CMakeLists.txt`、`src/tests/test_db_io.cc`、`testsuite/single/tests/efuns/audit_db_identity.lpc`、`testsuite/etc/test-scopes.tsv`。只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-015 已满足；并发/运行时维护者；16–32 小时。
- **必须击中的根因：**Request 只有 handle，执行时再次查槽位；锁保证单次访问互斥，但不能证明两次查找是同一代连接。

#### 操作

1. db_t 增加内部 uint64_t generation 和 connecting/ready 状态；每次槽位复用递增代号，溢出拒绝复用，不能回到旧代号。
2. create/connect 在局部零初始化连接状态中完成，锁内发布 ready；失败时先保留错误信息再清理，不发布半初始化 c。find_db_conn 的执行用途只返回 ready 连接。
3. 给 Request 增加 generation；f_async_db_exec 与 f_async_db_exec_promise 通过同一个锁内 snapshot helper 捕获 handle+generation。
4. dbexecthread 在持有 db_mutex 后验证代号，再调用 cleanup/execute；不匹配按既有无效连接错误交付一次，绝不把旧任务转到新连接。
5. 在 Linux 新建 db_io_tests，复用现有 ELF --wrap 做法，包装 _Z13db_lock_mutexv；仅暂停非主线程且尚未进入真实锁的调用。不能在已经持有 db_mutex 后暂停并等待主线程 db_close。
6. 原生用例执行实际 LPC/SQLite 语句；覆盖 callback/Promise、失败重连、槽位增长、关闭时正在执行及关闭后尚未执行。

#### 关键修改示例

```text
db_t* db = find_db_conn(req->handle);
if (!db || db->generation != req->generation) {
  finish_invalid_database_request(req);
  return nullptr;
}
```

#### 定向验证

- **场景：**ELF test-only wrapper 在异步线程调用 db_lock_mutex 前暂停，主线程关闭并重建同号 SQLite 连接，再释放 worker；旧请求不得触达新库。
- **profile/真实边界：**新 db_io_tests 目标；Debug、ASan、TSan；SQLite 隔离集成。
- **新增 native filter：**`DbIoTest.AuditConnection*`，目标 `db_io_tests`；包含代际失配、失败重连和槽位增长。

```bash
N=AUD-004
F='DbIoTest.AuditConnection*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/db_io_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC fixture：**`/single/tests/efuns/audit_db_identity`。测试源是 `testsuite/single/tests/efuns/audit_db_identity.lpc`，按 `fixture` 注册；它由 `db_io_tests` 通过真实 driver/SQLite 路径加载，不作为独立 `do_tests` 用例或独立 LPC runner 通过项。

#### 完成、风险和回退

- **验收：**暂停/复用反例稳定失败在旧源码、修复后旧任务被拒绝；新库没有旧任务的副作用；错误和回调次数一致，TSan 无该路径数据竞争。
- **风险与缓解：**不能改变公开整数 handle 语义；不能在执行期间把裸 db_t* 保存在可重分配槽数组之外；失败连接发布必须与 AUD-015 配合。
- **文档同步：**`docs/efun/async/async_db_exec.md`、`docs/efun/db/db_close.md`、`docs/efun/db/db_connect.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已实施。旧源码 fail-first 的 callback/Promise 两项均失败；修复后 Debug、ASan 受监督 runner 各发现并通过 4/4，TSan 私有构建直接运行通过 4/4，覆盖代际拒绝、失败连接不发布和槽位增长。证据位于 `build-modernization-evidence/audit-u04-failfirst-20261014T010000Z/`、`audit-u04-debug/`、`audit-u04-asan/`、`audit-u04-tsan/`、`audit-u04-native-debug-systempy-20261014T022300Z/` 和 `audit-u04-native-asan-systempy-20261014T022300Z/`。TSan 运行使用私有构建的 `setarch -R` 规避本机 PIE shadow mapping 限制；固定 clang-format 18.1.8 不可用，格式门禁仍未通过。

### AUD-005 · connections 调试日志记录 NOECHO 输入正文

- **修复目标：**在模式位清理之前决定脱敏；隐藏输入只输出固定标记或长度，保留非隐藏调试能力。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/comm.cc:1573–1581`。
- **作用范围：**`src/comm.cc`、`src/base/internal/log.cc`、`src/tests/test_lpc_gateway.cc`、`src/comm.h`。测试归属 `src/tests/test_lpc_gateway.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**网络/C++ 运行时维护者；4–8 小时。
- **必须击中的根因：**debug 输出位于 NOECHO 检查和模式清理之前，直接格式化 user_command。snoop 的 NOECHO 保护不覆盖日志。

#### 操作

1. 在 get_user_command 的唯一正文日志点根据当前 NOECHO 决定固定脱敏文本。检查 consume_user_command_snapshot 和直接 process_user_command 路径有无等价正文日志。
2. 使用内存日志 sink 或隔离输出捕获做负断言；保留实际 callback 得到原始输入的断言，不能把业务输入替换成脱敏文本。
3. 覆盖 NOECHO 退出时的 localecho restore、callback destruct 和普通输入，确认交互模式时序未改。

#### 关键修改示例

```text
debug(connections, "get_user_command: user_command = (%s)\n",
      (ip->iflags & NOECHO) ? "<redacted>" : user_command);
```

#### 定向验证

- **场景：**用非真实凭据的固定测试标记分别走普通输入、NOECHO、字符模式与 gateway snapshot；捕获日志，隐藏标记不得出现。
- **profile/真实边界：**Debug 原生+真实 LPC 回调；日志负断言；ASan 接收回归。
- **新增 native filter：**`DriverTest.TestAuditNoechoLog*`，目标 `lpc_tests`。

```bash
N=AUD-005
F='DriverTest.TestAuditNoechoLog*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

#### 完成、风险和回退

- **验收：**所有测试标记在隐藏模式日志中出现次数为 0；LPC callback 收到原输入；普通调试和 echo 恢复不变。
- **风险与缓解：**不能用关闭整个 connections 日志掩盖问题；不能把真实密码写入回归日志；模式位清理后再检查会漏保护。
- **文档同步：**`docs/efun/interactive/input_to.md`、`docs/efun/interactive/get_char.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已实施并已提交于 `e8ceb350`。旧源码 fail-first 在 `build-modernization-evidence/audit-3d7a0127452c-noecho-fail-first-3/` 复现 NOECHO 正文泄漏；修复后 Debug 受监督 runner 的 `DriverTest.TestAuditNoechoLog*` 为 `selected=2, passed=2, skipped=0, failed=0`，证据在 `audit-3d7a0127452c-noecho-debug-pass-2/`；同一过滤器在隔离 ASan 构建中为 `2/2`，证据在 `audit-3d7a0127452c-noecho-asan-run-2/`。既有 gateway input_to、get_char snapshot 和 callback self-destruct 兼容回归为 `3/3`，证据在 `audit-3d7a0127452c-noecho-compat-debug/`。验证覆盖普通输入、NOECHO input_to、NOECHO get_char、原始 callback 参数和固定 `<redacted>` 日志标记；`consume_user_command_snapshot` 无同类正文 debug 点。未运行真实外部 TCP/TLS/WebSocket 客户端，固定 clang-format 18.1.8 仍不可用；这些边界不标为已验证。

### AUD-006 · 非 Linux 构建没有执行求值时间限制

- **修复目标：**保留 Linux 计时器；非 Linux 使用线程私有单调 deadline 和 VM 安全点检查，使 set_eval/get_eval 语义一致。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/vm/internal/eval_limit.cc:9–29`、`src/vm/internal/base/interpret.cc:2478–2487`。
- **作用范围：**`src/vm/internal/eval_limit.cc`、`src/vm/internal/base/interpret.cc`、`src/vm/internal/eval_limit.h`、`src/vm/internal/base/promise.cc`、`.github/workflows/ci.yml`、`src/tests/test_lpc.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**构建/依赖维护者；24–40 小时。
- **必须击中的根因：**计时实现只编译于 __linux__，其他平台只有警告而无等价保护。

#### 操作

1. 在 eval_limit 模块封装非 Linux 的 thread_local deadline/剩余预算；set_eval 初始化、get_eval 返回剩余值，0 和最大值的语义按现有 efun 合同处理。
2. eval_instruction 的安全点按固定小批次（拟议每 64 条指令）检查非 Linux deadline，过期走现有 outoftime/error 路径；Linux 路径不改变。
3. 为 suspend/resume、异常恢复、nested catch 和 worker VMContext 切换核对预算重设点，不能复用其他线程的 deadline。
4. macOS/Windows CI 增加实际定向运行，而不是仅编译。重用 lpc_vm_bench 测量正常路径；新默认采样间隔若不能满足延迟和吞吐标准，需修订方案而非降低验收。

#### 关键修改示例

```text
#ifndef __linux__
if (++poll_count >= 64) {
  poll_count = 0;
  if (steady_clock::now() >= deadline) {
    outoftime = 1;
  }
}
#endif
```

#### 定向验证

- **场景：**Linux 运行有截止时间的有限计算；非 Linux 分支在隔离的非 Linux 编译/运行探针中验证 deadline、过期和线程私有状态；macOS 与 Windows 的原生回归由 CI 实际执行。
- **profile/真实边界：**Linux Debug、ASan、UBSan 定向回归；非 Linux 使用 `-U__linux__` 的同一源码探针；未把 Linux 模拟结果写成 macOS/Windows 实机证据。
- **新增 native filter：**`DriverTest.TestAuditEvalBudget*`，目标 `lpc_tests`。

```bash
N=AUD-006
F='DriverTest.TestAuditEvalBudget*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **平台 CI：**`.github/workflows/ci.yml` 在 macOS/Windows 的 Debug 与 RelWithDebInfo 矩阵中运行同一过滤器；该路径必须保留真实失败，不得只编译通过。

#### 完成、风险和回退

- **验收：**两种非 Linux 平台的 CI 均真实中断超额计算且之后能继续执行；owner 间预算不串扰；Linux 原语义保持；正常 VM benchmark 回退不超过 5%。本地 `-U__linux__` 探针只证明非 Linux 实现可编译并具备独立 deadline 语义，不替代两种真实平台证据。
- **风险与缓解：**wall time 与线程 CPU 时间不同；不能把跨 await 的等待算成未定义的新预算；必须沿用现有挂起/恢复的 set_eval 调用语义并记录平台差异。 此项保证纯 LPC 运算在安全点响应预算，不承诺可中断任意阻塞 C/外部库调用。
- **文档同步：**`docs/efun/system/set_eval_limit.md`、`docs/efun/system/eval_cost.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已实施并完成本地可执行范围。`TestAuditEvalBudgetDeadline`（含不同线程独立预算检查）在隔离 Debug、ASan、UBSan 构建的受监督 runner 中各通过 1/1；最终证据位于 `build-modernization-evidence/audit-aud006-eval-budget-01/native-debug-final2/`、`native-asan-final2/` 和 `native-ubsan-final4/`。同一 `eval_limit.cc` 以 `-U__linux__` 编译并由独立 C++ 探针验证 deadline 到期和重置语义，最终日志在 `build-modernization-evidence/audit-aud006-eval-budget-01/nonlinux-probe-final/probe-run.log`。完整 `lpc_tests` 在 Debug、ASan、UBSan 中各执行并通过 528/528，证据位于 `full-debug-final6/`、`full-asan-final6/` 和 `native-ubsan-final3/`。Linux Debug 基线与当前源码各交替运行 7 轮 `lpc_vm_bench`，无指标中位回退超过拟议 5% 门槛，比较结果在 `benchmark-comparison.json`。macOS/Windows 原生 CI 尚未运行，属于 `external-required`，不能以本地源码探针替代。

### AUD-007 · reclaim_objects 对共享或循环复合值重复遍历

- **修复目标：**在一轮回收中采用显式工作表和 visited 集合；区分容器只扫描一次与每个引用槽必须处理。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/packages/core/reclaim.cc:24–50`、`src/packages/core/reclaim.cc:113–135`。
- **作用范围：**`src/packages/core/reclaim.cc`、`src/packages/core/call_out.cc`、`src/backend.cc`、`src/vm/internal/base/svalue.cc`、`src/tests/test_lpc.cc`、`src/vm/internal/owner_runtime_coordinator.h`、`src/vm/internal/owner_runtime_coordinator.cc`、`src/tests/test_lpc_compiler.cc`。测试归属 `src/tests/test_lpc.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**并发/运行时维护者；16–32 小时。
- **必须击中的根因：**递归访问没有按复合分配身份去重；后台 tick 的时间片只在整次 callback 返回后检查，不能打断一次 reclaim。

#### 操作

1. 在 reclaim_objects(bool) 实际入口同时保护手动和自动路径：复用 src/vm/internal/owner_runtime_coordinator.h/.cc 的 vm_owner_recompile_quiesce_begin/end，不新增或重命名一套全局 barrier。begin/end 内部取非递归 runtime mutex，调用者不得预先持此锁。已核对 active_owner_claims 及 CLOSING/FROZEN 准入；recompile_epoch 只保护窗口身份，不是 object/program epoch。
2. 拟定冻结等待上限为 250 ms（方案值，不是实测保证）。冻结成功后才 reclaim_call_outs、遍历对象或修改 svalue。失败时不做任何图修改；手动调用报告既有风格的明确错误，自动调用保留下一轮调度并记录延期原因/现有 quiesce 计数。嵌套、线程停止、非主线程、超时分开处理，绝不把失败当成功清理。
3. 在 reclaim.cc 定义小型不可复制 GcQuiesceScope，仅用成功返回的 epoch 在作用域结束调用 end。它先于 worklist/visited 容器构造，使待扫引用和容器都释放后才重新开放 owner。异常出口同样结束窗口；不得在持窗口时运行普通 LPC callback。更新 coordinator 的用途说明及 owner 文档，保留现有 API 名称和状态字段。
4. 将递归展开改为显式 worklist；每轮 visited 按容器/闭包/Promise 的实际地址及类型去重，不用纯深度阈值掩盖总量。所有变量根共用同一集合。只入队有明确保活责任的槽/容器，不能保存可能被回收的裸 svalue_t*。引用数必须在完整 pass/异常出口恢复。
5. 对 mapping 的 destructed object key 沿用 gc_mapping 的摘链、count/unfilled/全局节点计数逻辑；不能把 key 直接改成 0。活节点值与复合 key 才入队，并保证节点不会在其槽仍待扫时被删。保留 funptr_detach_owner 对 creation-time program 的单次引用释放。
6. 补共享 DAG、多个根同指容器、互相循环、深链、mapping key、空容器和重复 pass。用唯一节点/边访问计数而非仅秒数验收线性工作量；容器/对象引用数和清理计数必须正确。
7. 补真实 owner claim 的冻结成功、超时、嵌套拒绝、非主线程/线程停止拒绝和自动延期测试。冻结期间新 admission 应被拒绝；任何失败都不改图、不调用 end。只有本次进入 CLOSING 后的超时才恢复本次窗口为 OPEN；嵌套拒绝必须保留外层 FROZEN 及其 epoch，不能释放外层窗口。成功持有人释放后才恢复调度。同时跑既有 DriverTest.TestRecompileQuiesce*，确保原重编译合同不变。

#### 关键修改示例

```text
// 去重的是容器内容，不是指向对象的每一条引用。
if (visited.insert(container_identity).second) {
  enqueue_container_slots(container);
}
```

#### 定向验证

- **场景：**少量节点构成共享 DAG/环，在测试计数器中验证每个容器内容只扫描一次；同一已 destruct 对象的每条引用仍分别归零并减计数。
- **profile/真实边界：**Debug、ASan、UBSan；TSan owner 隔离前置检查。
- **新增 native filter：**`DriverTest.TestAuditReclaimSharedGraph*`，目标 `lpc_tests`；同时回归既有重编译冻结窗口。

```bash
N=AUD-007
F='DriverTest.TestAuditReclaimSharedGraph*:DriverTest.TestRecompileQuiesce*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC helper：**`testsuite/single/tests/reclaim_deep.c`。它由 `lpc_tests` 的 reclaim 过滤器加载，覆盖深链和 Promise-owned graph；不作为独立 LPC runner 用例，不注册为独立 scope 条目。

#### 完成、风险和回退

- **验收：**工作量随唯一容器及引用槽近线性增长；共享/环/深图无 C 栈溢出；每条 destruct 引用正确释放，mapping 结构完整，后续回收仍可执行。
- **风险与缓解：**已有 owner-wide quiescence 入口已核对，可复用而不重做调度器；仍须通过真实 claim/拒绝/恢复测试。冻结失败不得访问全局对象图，end 必须使用匹配 epoch。mapping key 与 funptr program 引用沿用现有语义，worklist 保活先于重新开放 owner 结束。
- **文档同步：**`docs/efun/system/reclaim_objects.md`、`docs/owner-multicore-api.md`、`docs/multicore-runtime-v4.md`。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**PASS，已纳入提交 `7173f1b6`。Promise settlement、reaction、链式 Promise、协程状态和 Promise root 已接入迭代 worklist，并在 owner-runtime 静止门闩内执行。当前源码的 Debug、ASan、UBSan 与 LPC 全量证据归档于 `audit-evidence/aud007-018-current-02/`：Debug/ASan/UBSan 的可用 C++ 全集均为 529/529（仅排除未构建的 `TestLpcshellInteractiveContract`），Promise/reclaim 定向过滤器为 2/2，LPC 全量为 496/496；lpcc I/O 为 12/12。`TestLpcshellInteractiveContract` 的未执行状态和外部平台限制仍单独保留，不计为通过。

### AUD-008 · 操作信号处理器直接执行 VM、日志和线程停止

- **修复目标：**handler 仅设置 lock-free pending 标志并使用经过审核的异步安全唤醒；VM/诊断/join 在正常执行安全点完成。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/mainlib.cc:141–193`、`src/vm/internal/owner.cc:6287–6319`。
- **作用范围：**`src/mainlib.cc`、`src/vm/internal/owner.cc`、`src/backend.cc`、`src/backend.h`、`src/vm/internal/base/interpret.cc`、`src/vm/internal/eval_limit.cc`、`src/tests/CMakeLists.txt`。测试归属 `src/tests/test_lpc.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-006 已满足；并发/运行时维护者；16–32 小时。
- **必须击中的根因：**把正常线程的诊断/VM 调用/关闭工作放在异步中断上下文。fork 加入 owner 线程后扩大了继承设计的死锁风险。

#### 操作

1. 为操作信号定义进程级 lock-free atomic pending bitset，并 static_assert always_lock_free。handler 不分配、不格式化、不调用 LPC、不 join。
2. 若需要即时唤醒，新增只保存/恢复 errno 并向既有非阻塞 self-pipe write 的专用薄层；初始化/关闭期间屏蔽相应信号，避免 fd 复用。
3. 在 eval_instruction 安全点与 backend 调度处消费标志，使长时间 LPC 也能响应。保留信号原有诊断/退出意图，但实际 stop 和 VM 调用只在正常主线程执行。
4. 保留 SIGTERM/SIGHUP 已有路径；单独核对 posix_timers 的线程定向信号，不把求值超时信号改成主线程全局信号。
5. 新增自有 probe 与父进程监督；覆盖持 owner mutex、持日志锁、活动 worker、空闲事件循环和重复信号。

#### 关键修改示例

```text
void sigint_handler(int) {
  pending_signals.fetch_or(kPendingInterrupt, std::memory_order_relaxed);
  signal_safe_backend_wakeup();
}
```

#### 定向验证

- **场景：**仅对自建短生命周期 probe 在已知持锁屏障期间发送操作信号；验证 handler 返回、主线程安全点处理和限时退出，不能向现有 driver 发信号。
- **profile/真实边界：**私有 signal probe、Debug/ASan/TSan；Linux+macOS。
- **新增 native filter：**`DriverTest.TestAuditSignalDispatch*`，目标 `lpc_tests`。

```bash
N=AUD-008
F='DriverTest.TestAuditSignalDispatch*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_signal_progress`。测试源是 `testsuite/single/tests/efuns/audit_signal_progress.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-008
L=audit_signal_progress
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**handler 的调用链仅含已证明异步安全操作；私有 probe 限时完成、不自 join、不重入 mutex；诊断和清理在主线程执行一次。
- **风险与缓解：**只把处理推到 backend 不足以打断正在计算的 LPC；不能破坏 Linux 定向求值计时器，也不能从 signal handler 调用会加锁的通用调度器。
- **文档同步：**`docs/multicore-runtime-v4.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已实施并完成 Linux 本地可执行范围验证；macOS 原生验证仍为 `external-required`。操作信号 handler 只对进程级 lock-free pending mask 执行 `fetch_or`，并以 `static_assert(std::atomic<unsigned int>::is_always_lock_free)` 固定约束；SIGINT/SIGUSR1/SIGUSR2 的 VM、日志、回溯和退出动作统一在主线程安全点消费。backend tick 和主线程解释器每 64 条指令检查 pending mask，SIGTERM/SIGHUP 仍沿用原有受控关闭路径。旧源码 fail-first 记录在 `audit-evidence/aud008-current-01/fail-first-source.log`。Debug、ASan、UBSan、TSan 的 `DriverTest.TestSigtermUsesControlledShutdownWithSuccessExitCode`、`DriverTest.TestAuditSignalDispatch*`、`DriverTest.TestControlledShutdownPreparesMudlibBeforeRuntimeTeardown` 均为 3/3；TSan 首次未使用 `setarch -R` 的映射失败保留，按项目合同重试成功。隔离 driver 实际发送 SIGUSR2 后继续运行，再发送 SIGTERM 并以 0 退出；日志和状态在 `audit-evidence/aud008-current-01/runtime-signal-final2-status.log`。当前证据未包含 macOS 实机或外部客户端，不能把这些边界标为已验证；隔离 LPC signal-progress fixture 为 1/1，状态和 runner 证据在 `audit-evidence/aud008-current-01/lpc-signal-progress-status-03.log` 与 `lpc-signal-progress-03/`；完整证据目录为 `audit-evidence/aud008-current-01/`。

### AUD-009 · ASCII 空行在缓冲区起点前读一个字节

- **修复目标：**保持原始 LF 指针不变；仅在原始 LF 大于行首时检查前一个 CR。
- **来源处理策略：**按 §7 选择性回移 b2ba3f8f0031 的相关 hunk，保留 fork 适配并记录未取部分。
- **优先级/来源：**P2 / fork 未同步上游修复。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/comm.cc:1315–1327`。
- **作用范围：**`src/comm.cc`、`src/tests/test_lpc_gateway.cc`、`src/comm.h`。测试归属 `src/tests/test_lpc_gateway.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-001 已满足；网络/C++ 运行时维护者；2–4 小时。
- **必须击中的根因：**未先证明 nl > p 就读取分隔符前一字节。

#### 操作

1. 只读 b2ba3f8f 在上游 src/net/transport_libevent.cc 的 ASCII 前读 hunk，并映射到本地 src/comm.cc。保留已吸收的其他内存/权限修复，不整包 cherry-pick。
2. 把 ASCII 行边界计算收敛为内部小函数，输入当前有效起点、长度和 LF 偏移，输出 line_length 与 consumed_length；先证明 LF 偏移大于 0 才读取前一字节。生产 ASCII 分支和 C++ 测试使用同一个函数。
3. 测试独立短数组的首字节 LF、空行、CRLF、下一行空行以及全部拆包位置。ASan 可能看不到 interactive_t 内部成员的前读，因此同时验证行边界函数在独立 redzone 数组上不越界；不得破坏 interactive_t 邻接字段制造假失败。

#### 关键修改示例

```text
char* const lf = nl;
char* line_end = lf;
if (line_end > p && line_end[-1] == '\r') {
  --line_end;
}
*line_end = '\0';
```

#### 定向验证

- **场景：**私有 ASCII 接收夹具覆盖起始 LF、空行、连续 LF、CRLF、分片 CR/LF；检查字节访问边界和 callback 次数。
- **profile/真实边界：**Debug、ASan；实际 LPC process_input 夹具。
- **新增 native filter：**`DriverTest.TestAuditAsciiEmptyLine*`，目标 `lpc_tests`。

```bash
N=AUD-009
F='DriverTest.TestAuditAsciiEmptyLine*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

#### 完成、风险和回退

- **验收：**不存在 p 之前的读；所有空行按原合同处理；ASan 及 callback 序列断言通过。
- **风险与缓解：**必须与 AUD-010 的游标修正协调，不能加一个判断却继续复用被减一的 nl。 ASan 对结构体成员内部前读可能不报警，不能只依赖 sanitizer 退出码。
- **文档同步：**`docs/project-modernization-plan-2026-10.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已验证，纳入 U02 原子提交；Debug/ASan 定向与完整 `lpc_tests` 证据见主计划 U02。

### AUD-010 · ASCII CRLF 修剪后下一行游标退回分隔符

- **修复目标：**下一行始终使用原始 LF+1；正文修剪使用独立 line_end。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P2 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/comm.cc:1318–1343`。
- **作用范围：**`src/comm.cc`、`src/tests/test_lpc_gateway.cc`。测试归属 `src/tests/test_lpc_gateway.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-001、AUD-009 已满足；网络/C++ 运行时维护者；4–8 小时。
- **必须击中的根因：**同一个可变 nl 同时表示原始分隔符位置和修剪后的正文末尾。

#### 操作

1. 在 AUD-009 的 lf/line_end 分离后，令 next=lf+1；text_start 与 p 均由 next 更新，不再使用变动后的 nl。
2. 回调后检查 guard.connection_alive()，再判断 buffer 是否已消费完；失效时不碰游标。
3. 表驱动遍历每个拆包边界，直接断言每条 LPC 输入的正文和顺序，不只比较最终计数。

#### 关键修改示例

```text
char* const next = lf + 1;
ip->text_start = static_cast<int>(next - ip->text);
// 在回调后的存活检查通过之后：
p = next;
```

#### 定向验证

- **场景：**比较同一正常多行命令在所有拆包方式下的回调序列；混合 LF/CRLF、空正文、末尾不完整行均需覆盖。
- **profile/真实边界：**Debug、ASan；全拆包位置性质测试。
- **新增 native filter：**`DriverTest.TestAuditAsciiCrLf*`，目标 `lpc_tests`。

```bash
N=AUD-010
F='DriverTest.TestAuditAsciiCrLf*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

#### 完成、风险和回退

- **验收：**一次/多次分片得到完全相同的正文序列；最后一条完整行无需额外字节触发；无空命令回归。
- **风险与缓解：**改变正文结束位置时要保留残片搬移逻辑，不能把未完整行当成完整命令。
- **文档同步：**`docs/project-modernization-plan-2026-10.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已验证，纳入 U02 原子提交；Debug/ASan 定向与完整 `lpc_tests` 证据见主计划 U02。

### AUD-011 · Telnet 按网络分块净化文本，破坏跨块编码序列

- **修复目标：**在具体 interactive_t 会话保存流式解码状态，只缓存不完整后缀；关闭/编码切换时按既有替换合同收口。
- **来源处理策略：**按 §7 选择性回移 5f7be1008f28 的相关 hunk，保留 fork 适配并记录未取部分。
- **优先级/来源：**P2 / fork 未同步上游修复。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/net/telnet.cc:66–93`。
- **作用范围：**`src/net/telnet.cc`、`src/interactive.h`、`src/comm.cc`、`src/packages/core/encoding.cc`、`src/tests/test_lpc_gateway.cc`、`testsuite/single/tests/efuns/audit_telnet_text.lpc`、`testsuite/etc/test-scopes.tsv` 及 `docs/lpc-modern-runtime.md`、`docs/efun/interactive/set_encoding.md`。调用者和既有 encoding/UTF-8 工具只在需要时检查；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-001 已满足；网络/C++ 运行时维护者；16–24 小时。
- **必须击中的根因：**on_telnet_data 对每个独立块直接转换/净化，没有保留会话级未完成后缀；临时转换缓冲所有权也需在失败分支保全。

#### 操作

1. 明确缓存归属为具体 interactive_t，不挂在可重绑的 object 上；关闭和 session 替换清空的只能是旧会话。
2. 对原生 UTF-8 输入复用现有 encoding 工具中的有效前缀/不完整后缀判定；非法完整序列立即按既有规则替换，只等待确实不完整的尾部。
3. 对 ICU 转码使用有状态流式转换接口，不以会改变 converter 状态的两次预检/实际转换实现流式输入；转换输出进入同一个 UTF-8 后缀处理器。
4. 临时转换缓冲改为明确作用域所有权，错误路径不能覆盖唯一指针后失去释放；设置/清除编码时做 flush/reset。
5. 测试实际 TELNET_EV_DATA → on_user_input 路径；binary/MUD 帧不经过文本净化。

#### 关键修改示例

```text
// 概念骨架：状态属于当前会话；只保留不完整后缀。
auto decoded = decode_session_stream(ip, input, /*flush=*/false);
on_user_input(ip, decoded.data(), decoded.size());
```

#### 定向验证

- **场景：**同一有效文本按每一个字节边界拆分、逐字节到达、非法中间字节、EOF 残片及关闭回调场景；非 UTF-8 session encoding 另测。
- **profile/真实边界：**Debug、ASan；真实 Telnet 分片输入；UTF-8/已支持非 UTF-8 编码。
- **新增 native filter：**`DriverTest.TestAuditTelnetStreamText*`，目标 `lpc_tests`。

```bash
N=AUD-011
F='DriverTest.TestAuditTelnetStreamText*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_telnet_text`。测试源是 `testsuite/single/tests/efuns/audit_telnet_text.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-011
L=audit_telnet_text
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**所有分片方式的有效文本结果相同；非法序列与 EOF 替换次数确定；会话替换不串缓存；ASan 无错误缓冲泄漏。
- **风险与缓解：**不能照搬 socket efun 的整个缓存，因为交互入口还有 ICU 转码、Telnet 控制序列和会话替换；EOF 回调可能关闭会话。
- **文档同步：**`docs/lpc-modern-runtime.md`、`docs/efun/interactive/set_encoding.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已实施并完成本地可执行范围验证。Fail-first 的基线分片测试确认跨块 UTF-8/非法输入失败；修复后 Debug 与 ASan 受监督 native runner 各 `3/3`（发现数=执行数=通过数，无跳过），Debug LPC case `audit_telnet_text` 为 `1/1`。持久证据见 `audit-evidence/aud011-current-01/`，其中保留基线失败、一次 runner 参数错误和后续成功记录。实际外部 Telnet 客户端、EOF 网络关闭时的真实平台组合及非 Linux 运行证据未执行，不能以本地夹具替代；该边界不阻止 Linux 本单元提交。

### AUD-012 · get_char 破坏控制字符，并按字节切断 UTF-8 字符

- **修复目标：**字符模式完整交付合法 UTF-8 序列和控制字节；不完整片段等待，行模式编辑/净化不改。
- **来源处理策略：**按 §7 选择性回移 5f7be1008f28 的相关 hunk，保留 fork 适配并记录未取部分。
- **优先级/来源：**P2 / fork 未同步上游修复。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/comm.cc:1453–1475`、`src/comm.cc:1504–1530`。
- **作用范围：**`src/comm.cc`、`src/comm.h`、`src/packages/gateway/gateway_session.cc`、`src/tests/test_lpc_gateway.cc`、`docs/efun/interactive/get_char.md`、`docs/archive/Bugs`。测试归属 `src/tests/test_lpc_gateway.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-011 已满足；网络/C++ 运行时维护者；4–8 小时。
- **必须击中的根因：**SINGLE_CHAR 的提取器仍采用单字节/特殊清零规则；ESC 的行模式净化条件没有排除字符模式。

#### 操作

1. 读取 5f7be100 的限定 hunk。删除 first_cmd_in_buf 在 SINGLE_CHAR 下将 BS/DEL 置零的分支；保留 on_user_input 里已经正确的 SINGLE_CHAR 分支。
2. 将单字符提取改成按完整有效 UTF-8 序列交付，临时缓冲最多 4 字节加终止符；cmd_in_buf 与 first_cmd_in_buf 共用长度判定。不完整前缀等待，畸形字节走已明确的替换/进展合同，不能永久停顿。
3. ESC 的 no-ANSI/strip-before-process-input 替换只用于非 SINGLE_CHAR；保留 on_user_input 已正确的 BS/DEL 行模式编辑分支和 IAC 解析。
4. 逐项核对 Telnet、WebSocket Telnet 和 gateway snapshot 的调用路径，测试各配置组合。让 gateway snapshot 与原生提取共用 UTF-8 长度判定，避免快照消费再次把多字节字符截断或把 BS/DEL 变成空字符串。更新实际 get_char 归属文档；不修改 docs/archive/Bugs。

#### 关键修改示例

```text
const int len = single_char_bytes(ip);
if (len == 0) {
  ip->iflags &= ~CMD_IN_BUF;
  return nullptr;
}
memcpy(tmp, p, len);
tmp[len] = '\0';
```

#### 定向验证

- **场景：**原生接收夹具逐项检查 ESC、BS、DEL、普通字符、UTF-8 字符和跨分片序列；行模式必须维持现有编辑语义。
- **profile/真实边界：**Debug、ASan；实际 get_char LPC callback。
- **新增 native filter：**`DriverTest.TestAuditCharMode*`，目标 `lpc_tests`。

```bash
N=AUD-012
F='DriverTest.TestAuditCharMode*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

#### 完成、风险和回退

- **验收：**BS/DEL/ESC 按合同交付；每个完整合法 UTF-8 字符只触发一次 callback，分片不产生坏字符串或停顿；行模式不变。
- **风险与缓解：**不能直接套用上游整段旧 comm.cc，fork 有额外 owner/gateway 输入路径；字符数和字节数语义须沿用既有 API。
- **文档同步：**`docs/efun/interactive/get_char.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已实施并完成本地可执行范围验证。字符模式原生 gateway 夹具覆盖 BS、DEL、ESC、ASCII、完整 3/4 字节 UTF-8、跨分片 UTF-8、非法标量范围、畸形前导字节和 gateway snapshot 消费；Debug 与 ASan 字符模式定向 native runner 各 `1/1`，组合字符/行模式受监督 native runner 各 `12/12`（发现数=执行数=通过数，无跳过），Debug 与 ASan 组合回归各 `12/12`。持久证据见 `audit-evidence/aud012-current-01/`。实际外部 Telnet 客户端、非 Linux 运行证据未执行，不能以本地夹具替代。

### AUD-013 · 超长配置行使后续配置静默停止解析

- **修复目标：**保留有界行缓冲，显式区分 EOF、超长行和 I/O 失败；任何读取失败均停止配置初始化，不再悄悄忽略余下安全指令，也不打印整行配置值。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P2 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/base/internal/rc.cc:32–33`、`src/base/internal/rc.cc:229–254`。
- **作用范围：**`src/base/internal/rc.cc`、`src/tests/test_lpc.cc`、`docs/driver/config.md`。测试归属 `src/tests/test_lpc.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**构建/依赖维护者；4–8 小时。
- **必须击中的根因：**把 getline 的缓冲耗尽失败与正常 EOF 当成同一终止条件。

#### 操作

1. 保留 `K_MAX_CONFIG_LINE_LENGTH=120` 的有界逐字符读取；显式区分正常 EOF、`fail` 和 `bad`。超长设置在解析任何后续配置前报错退出，不读取或打印整条任意长行。
2. 正确区分空 EOF、无末尾换行的合法行、恰好 119 字节的合法行和 120 字节以上的失败行；保留原注释剥除、trim、空行处理及内部空格语义。删除会打印整行值的 truncated warning。配置启动失败就停止，不引入运行时配置事务重构。
3. 核对外部端口、路径与 TLS 配置的 `scanf` 宽度，保持经过长度检查后才能进入解析的单一入口。

#### 关键修改合同

```text
逐字符读取一个物理行：
  - 注释尾部直接丢弃；设置内容最多保留 119 字节；
  - 120 字节以上的设置立即按物理行号拒绝；
  - 无字符 EOF 结束读取；带内容 EOF 仍处理最后一行；
  - stream bad/fail（非 EOF）立即拒绝配置。
```

实现使用有界逐字符读取，而不是无界 `std::getline`；因此不会把固定 `sscanf` 目标数组的长度检查推迟到任意长输入已经读入之后。

#### 定向验证

- **场景：**119/120/121 字节边界、有/无末尾换行、超长注释及后续合法配置；验证报错行号和不会悄悄继续启动。
- **profile/真实边界：**新进程配置解析测试，Debug、ASan；不操作正在运行的 driver。失败配置只喂给隔离的测试子进程和私有构建二进制。
- **新增 native filter：**`DriverTest.TestAuditConfigLine*`，目标 `lpc_tests`。

```bash
F='DriverTest.TestAuditConfigLine*'
python3 -B tools/testsuite/run-targeted.py \
  --binary "$E/native-debug-pcre-off/lpc_tests-after-reader-fix" \
  --gtest-filter "$F" --evidence-root "$E/native-debug-pcre-off/runner-after-reader-fix"
ASAN_OPTIONS=detect_leaks=0 python3 -B tools/testsuite/run-targeted.py \
  --binary "$E/native-asan-pcre-off/lpc_tests" \
  --gtest-filter "$F" --evidence-root "$E/native-asan-pcre-off/runner-detect-leaks-off"
```

- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

#### 完成、风险和回退

- **验收：**超限或读取失败明确非零退出；合法边界值全部解析；后续配置既不丢失也不在失败后被误认为生效。
- **风险与缓解：**不能只换成无界 std::getline：后续 sscanf 的目标仍有固定数组。保留原可接受长度，不扩大未审计输入范围。
- **文档同步：**`docs/driver/config.md`；已记录 119 字节合法上限、超长设置拒绝、长注释忽略和无换行 EOF 语义。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**已实施并完成本地可执行范围验证。`rc.cc` 现在用有界逐字符读取，区分正常 EOF、带内容 EOF、超长设置和读取失败；超长设置报物理行号并且不打印配置值。Debug 受监督 runner `TestAuditConfigLine*` 为发现 2、执行 2、通过 2、失败 0；ASan 使用项目预设的 `ASAN_OPTIONS=detect_leaks=0` 同样为 `2/2`。ASan 首次未关闭默认 LeakSanitizer 时记录了配置初始化子进程的分配泄漏，原始失败证据保留于 `audit-evidence/aud013-current-01/native-asan-pcre-off/runner/`，不能把该次写成通过。隔离 Debug driver 对 120 字节设置行返回 255，并报告 `line 397` 与 `exceeds 119 characters`；完整持久证据见 `audit-evidence/aud013-current-01/`。未取得非 Linux 原生运行证据；本单元不宣称跨平台动态通过。

### AUD-014 · 数据库编号比较漏写变量展开，错误也未终止 CMake

- **修复目标：**只收集启用后端，严格解析整数并比较完整去重长度，默认编号不合法直接 FATAL_ERROR；Windows 参数统一。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P2 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/packages/db/CMakeLists.txt:6–19`、`src/packages/db/CMakeLists.txt:36–43`。
- **作用范围：**`src/packages/db/CMakeLists.txt`、`src/CMakeLists.txt`、`.github/workflows/ci.yml`、`src/packages/db/db.cc`。测试归属 `tools/build/test-db-config.py`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**构建/依赖维护者；4–8 小时。
- **必须击中的根因：**冲突检查把 ${PACKAGE_DB_*} 与字面量 "{PACKAGE_DB_*}" 比较；错误分支使用不存在的 message(FATAL ...) 模式，而不是 FATAL_ERROR。

#### 操作

1. 对已启用 PACKAGE_DB_* 变量验证十进制整数和有效取值，不把空字符串/关闭选项当成编号。
2. 复制编号列表，list(REMOVE_DUPLICATES)，比较长度；默认后端不在列表时 message(FATAL_ERROR ...)。
3. 清理 Windows matrix 中 SQLite=1 与公共命令 SQLite=2 的重复覆盖，只保留一个值；同时设置一致的 PACKAGE_DB_DEFAULT_DB，禁用 MySQL/Postgres 时不能仍默认它们。
4. 新增 test-db-config.py，每个用例使用独立测试输出目录并显式断言 configure 返回值/诊断；它是会落盘的测试，不宣称只读。

#### 关键修改示例

```text
set(unique_ids ${enabled_ids})
list(REMOVE_DUPLICATES unique_ids)
list(LENGTH enabled_ids original_count)
list(LENGTH unique_ids unique_count)
if(NOT original_count EQUAL unique_count)
  message(FATAL_ERROR "Database backend IDs must be unique")
endif()
```

#### 定向验证

- **场景：**CMake 独立配置矩阵：有效编号、非相邻重复、无效默认、空/非数字、关闭全部后端和仅 SQLite；断言失败发生于 configure。
- **profile/真实边界：**Python 驱动的 CMake configure 矩阵；有效配置再构建 lpc_tests/DB 定向 LPC。
- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

新增 `tools/build/test-db-config.py` 的 CLI 固定为 `--source <仓库> --work-dir <全新目录>`，拒绝复用目录。对启用的后端验证展开后的正整数及 C++ int 表示范围；已有 CMake false 值（含 0/空）仍表示禁用，不能误改禁用语义。矩阵覆盖 SQLite-only、三后端互异且默认有效、每对后端相同、默认未启用、负/非数字及超范围编号。区分校验诊断和 FindPackage 失败，不把缺依赖当作编号校验通过。有效配置必须另完成编译；缺库则保持阻塞。

```bash
"$PY" -B tools/build/test-db-config.py \
  --source "$ROOT" --work-dir "$E/AUD-014-config-matrix"
```

#### 完成、风险和回退

- **验收：**所有非法矩阵在配置阶段非零失败；合法既有编号/默认行为不变；Windows 参数不再重复覆盖。
- **风险与缓解：**既有有效编号保持原义；不能擅自重排已发布后端数字。配置错误不能降级为换一个后端继续构建。
- **文档同步：**`docs/driver/config.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-015 · 数据库结果与失败连接的资源清理不完整

- **修复目标：**清理所有 PGresult；所有后端失败连接在错误已复制后变成可重复关闭的空状态，槽位重用前零初始化。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P2 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/packages/db/db.cc:1198–1214`、`src/packages/db/db.cc:1227–1238`、`src/packages/db/db.cc:842–852`、`src/packages/db/db.cc:370–379`。
- **作用范围：**`src/packages/db/db.cc`、`src/packages/db/db.h`、`src/tests/CMakeLists.txt`、`src/tests/test_db_io.cc`。测试归属 `src/tests/test_db_io.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**并发/运行时维护者；8–16 小时。
- **必须击中的根因：**驱动槽位释放与后端资源释放没有统一失败/关闭后置条件；部分连接失败分支只释放外层结构，结果所有权未逐项收口。

#### 操作

1. Postgres_cleanup 在置空前 PQclear；Postgres_execute 的 SELECT 和 PGRES_COMMAND_OK 返回合同保持不变，结果持续拥有到下一次 cleanup/close。当前 PostgreSQL backend 没有独立 commit/rollback callback，本项不新增它们。
2. 为 MySQL_connect 的 mysql_real_connect 失败分支补 mysql_close(tmp) 后再释放外层结构；检查 mysql_init 失败，不对空指针调用后续 API。
3. SQLite_connect 失败关闭后立即置空；各 backend close/cleanup 对部分初始化状态幂等。Postgres_close 只关闭仍拥有的 conn/res。
4. f_db_connect 失败先复制错误，再完成 backend cleanup/close，最后释放槽位；新代槽位 c 零初始化，不依赖上一连接的残留字节。
5. 用计数 wrapper 覆盖查询成功/失败、事务成功/失败、连接各阶段失败、重复 cleanup、close 后槽位复用。不能以禁用 PostgreSQL 作为通过。

#### 关键修改示例

```text
static void Postgres_cleanup(dbconn_t* c) {
  if (c->postgres.res) {
    PQclear(c->postgres.res);
    c->postgres.res = nullptr;
  }
}
```

#### 定向验证

- **场景：**隔离 db_io_tests 包装 libpq/MySQL C API，核对每个结果/连接恰好释放一次；SQLite 失败路径加入防双关；真实外部 DB 仅作为额外授权集成。
- **profile/真实边界：**新 db_io_tests；libpq/MySQL 测试包装；Debug+ASan/LSan；SQLite 集成。
- **新增 native filter：**`DbIoTest.AuditBackendCleanup*`，目标 `db_io_tests`。

```bash
N=AUD-015
F='DbIoTest.AuditBackendCleanup*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/db_io_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_db_cleanup`。测试源是 `testsuite/single/tests/efuns/audit_db_cleanup.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-015
L=audit_db_cleanup
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**每个成功创建的 PGresult/连接恰好释放一次；失败连接无泄漏/双关，诊断在释放后仍有效；ASan/LSan 和 SQLite 回归通过。
- **风险与缓解：**不能简单在所有失败路径再 close 一遍：SQLite 已关闭却未置空，MySQL 有手动分配结构；必须先使后端 close 幂等且失败后指针状态清楚。
- **文档同步：**`docs/efun/db/db_close.md`、`docs/efun/db/db_exec.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-016 · 嵌套循环覆盖 catch/time_expression 的禁止挂起上下文

- **修复目标：**禁止挂起状态独立于 break/continue/switch 流控状态，进入/退出受保护语法时可靠保存恢复；继续保留运行时硬守卫。
- **来源处理策略：**保留 fork 的对外能力，修当前实现根因，不借机删除合同或覆盖已有修复。
- **优先级/来源：**P2 / fork 引入问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/compiler/internal/grammar.y:899–904`、`src/compiler/internal/grammar.y:2143–2151`、`src/vm/internal/base/promise.cc:1628–1644`。
- **作用范围：**`src/compiler/internal/grammar.y`、`src/vm/internal/base/promise.cc`、`src/compiler/internal/grammar.autogen.cc`、`src/compiler/internal/grammar.autogen.h`、`src/compiler/internal/grammar_rules.h`、`src/tests/test_lpc_compiler.cc`、`testsuite/single/tests/compiler/fail`。测试归属 `src/tests/test_lpc_compiler.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**编译器/VM 维护者；8–16 小时。
- **必须击中的根因：**将禁止挂起条件存放在可被普通流控覆盖的 SPECIAL_CONTEXT 位；foreach 等直接重置 context。

#### 操作

1. 新增明确的 no_suspend 深度/状态，或使用不会被循环 context 赋值覆盖的独立位；逐一审查所有 context= 赋值，不只改 foreach。
2. catch/time_expression 进入时保存并设置禁止挂起，正常退出与语法错误恢复都恢复；await 同时核验 async 函数和当前函数上下文。 acatch 本身允许挂起，但内层 acatch 不能解除外层同步 catch 的禁止状态；为该组合补反例。
3. 保持运行时 protected control-stack guard。新增合法与非法矩阵，错误必须发生在编译阶段且诊断稳定。
4. 执行 tools/build/regenerate_grammar.py --write，再运行 generate_ebnf；重新检查所有生成结果，只提交源头推导出的产物。

#### 关键修改示例

```text
if (no_suspend_depth != 0) {
  yyerror("await cannot suspend in this protected context");
} else {
  emit_existing_await_node();
}
```

#### 定向验证

- **场景：**编译失败矩阵覆盖 protected-context × for/foreach/while/do/switch × await；合法 async 循环和匿名/嵌套函数边界另测。
- **profile/真实边界：**Debug、ASan/UBSan；Bison fallback 和 EBNF/语法合同重生成。
- **新增 native filter：**`DriverTest.TestAuditAwaitContext*`，目标 `lpc_tests`。

```bash
N=AUD-016
F='DriverTest.TestAuditAwaitContext*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_await_context`。测试源是 `testsuite/single/tests/efuns/audit_await_context.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-016
L=audit_await_context
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**受保护结构内所有嵌套 await 均编译拒绝；合法异步循环可挂起/恢复；连续编译失败不污染下一文件状态；生成物一致。
- **风险与缓解：**错误恢复路径必须恢复上下文；不能为让编译通过而删 coroutine_await_pending 的 protected-stack 检查。
- **文档同步：**`docs/lpc-modern-runtime.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-017 · catch 表达式被丢弃值优化器删除潜在异常操作

- **修复目标：**结果丢弃时保留可能抛错/调用/分配的运算；只删除已证明无异常副作用的节点，或直接生成完整求值再 POP 的 IR。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P2 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/compiler/internal/grammar.y:3053–3057`、`src/compiler/internal/trees.cc:407–433`。
- **作用范围：**`src/compiler/internal/grammar.y`、`src/compiler/internal/trees.cc`、`src/compiler/internal/icode.cc`、`src/compiler/internal/generate.cc`、`src/tests/test_lpc_compiler.cc`。测试归属 `src/tests/test_lpc_compiler.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**编译器/VM 维护者；8–16 小时。
- **必须击中的根因：**把“结果不用”误当作“运算没有可观察行为”；抛错也是副作用。

#### 操作

1. 先补回归并反汇编确认具体运算是否被删除；用运行时传入的操作数避免测试只覆盖折叠器。
2. 在 insert_pop_value 给可删除节点建立保守、明确的纯值白名单；算术/索引/类型转换只有证明不会抛错才删。不能只靠函数名或没有写变量判断纯度。
3. 对无法证明纯的表达式保留完整求值并生成 F_POP_VALUE；检查 icode/generate 对 POP 的既有栈效果，避免栈积累。
4. 同时覆盖普通丢弃值语句和 catch 两种语法；若改 grammar，同 AUD-016 重生成，否则不制造无关生成物差异。

#### 关键修改示例

```text
// 对可能抛错的表达式保留节点，使用既有 POP 生成方式。
CREATE_UNARY_OP(new_node, F_POP_VALUE, 0, expression);
```

#### 定向验证

- **场景：**用非常量操作数构造已知会抛错的算术、索引、转换，比较 catch(expr)、catch({expr;}) 与保存结果后执行的行为；先排除常量折叠影响。
- **profile/真实边界：**Debug、ASan/UBSan；LPC 可观察异常比较；VM benchmark。
- **新增 native filter：**`DriverTest.TestAuditDiscardedExpression*`，目标 `lpc_tests`。

```bash
N=AUD-017
F='DriverTest.TestAuditDiscardedExpression*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_catch_effects`。测试源是 `testsuite/single/tests/efuns/audit_catch_effects.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-017
L=audit_catch_effects
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**所有形式的可观察异常一致；真实纯值仍可消除；栈深度和普通执行结果不变；性能回退有真实对照证据。
- **风险与缓解：**不要把所有优化全部关闭；也不能仅改 catch(expr) 而让块语句的相同优化继续吞异常。
- **文档同步：**`docs/efun/calls/catch.md`、`docs/lpc-modern-runtime.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-018 · Promise 持有的已销毁对象引用不参与 reclaim_objects

- **修复目标：**在 AUD-007 的统一复合遍历中增加 Promise 内容，按 mark_promise 的实际拥有字段逐项覆盖，并保持 owner 准入。
- **来源处理策略：**保留 fork 的对外能力，修当前实现根因，不借机删除合同或覆盖已有修复。
- **优先级/来源：**P2 / fork 引入问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/packages/core/reclaim.cc:24–75`。
- **作用范围：**`src/packages/core/reclaim.cc`、`src/vm/internal/base/promise.h`、`src/vm/internal/base/promise.cc`、`src/vm/internal/base/svalue.cc`、`src/tests/test_lpc.cc`。测试归属 `src/tests/test_lpc.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-007 已满足；核心维护者/文档责任人；8–16 小时。
- **必须击中的根因：**fork 引入 T_PROMISE 后，引用标记已经扩展，但实际回收遍历没有同等扩展；mark_promise 与 reclaim 不可相互替代。

#### 操作

1. 逐字段对照 mark_promise 与 free_promise：结算 value、reaction 回调/绑定参数、后继 Promise 等，建立所有权清单，借用字段不能额外释放。
2. 将 Promise 分配身份纳入 visited；通过显式工作表遍历，不递归展开共享后继。
3. 仅清理 destruct 对象引用，不触发 reaction、取消或重新结算；沿用 owner/main-thread 安全边界。
4. 测试释放外层 Promise 前后的对象引用差额，避免只断言访问 destruct 对象返回 0 而漏掉真实保留。

#### 关键修改示例

```text
case T_PROMISE:
  // 使用与 mark_promise 相同的拥有字段清单；进入统一 visited/worklist。
  enqueue_promise_owned_slots(v->u.promise);
  break;
```

#### 定向验证

- **场景：**Promise resolved/rejected/pending、共享链/环、reaction 绑定参数和 next Promise 各自持有 destruct 对象；检查引用计数与对象最终释放而不改 Promise 结算状态。
- **profile/真实边界：**Debug、ASan/LSan、UBSan；真实异步完成屏障。
- **新增 native filter：**`DriverTest.TestAuditPromiseReclaim*`，目标 `lpc_tests`。

```bash
N=AUD-018
F='DriverTest.TestAuditPromiseReclaim*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_promise_reclaim`。测试源是 `testsuite/single/tests/efuns/audit_promise_reclaim.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-018
L=audit_promise_reclaim
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**每条持有的 destruct 引用及时清理；共享链不重复扫描/释放；Promise 状态、回调顺序和普通活对象引用不变。
- **风险与缓解：**不能在回收中执行 reaction 或改变 settled 状态；后继/共享链只扫描一次，但每条被清理引用都必须减计数。
- **文档同步：**`docs/lpc-modern-runtime.md`、`docs/efun/system/reclaim_objects.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**PASS。Promise result、pending reaction、后继 Promise、handler funptr、command giver、coroutine frame/defer 值以及外部 Promise roots 均纳入统一迭代回收图，并由 owner-runtime 静止门闩保护。当前源码的 Debug、ASan、UBSan 和 LPC 全量证据归档于 `audit-evidence/aud007-018-current-01/`；Promise 状态不因回收而重新结算。

### AUD-019 · 函数表查询允许等于函数总数的越界索引

- **修复目标：**将上界改为半开区间，保持无效索引返回 {nullptr, 0} 的 pair 合同；检查 alias/inherited 跳转后同样有有效边界。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P2 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/vm/internal/base/interpret.cc:1831–1845`、`src/compiler/internal/compiler.cc:2660–2667`。
- **作用范围：**`src/vm/internal/base/interpret.cc`、`src/compiler/internal/compiler.cc`、`src/vm/internal/base/program.h`、`src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc`。测试归属 `src/tests/test_lpc.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**编译器/VM 维护者；2–4 小时。
- **必须击中的根因：**把元素数量当成合法最大下标。函数标志数组按函数总数分配，没有证明额外哨兵。

#### 操作

1. 补最小 program 表边界回归，明确 count 是总数而不是最后索引。
2. 修正首个比较，检查所有 get_function_at_index 直接调用者是否依赖错误的 count 哨兵；保持既有 {nullptr, 0} 返回值，不把 pair 返回类型写成裸指针。
3. 覆盖继承和 alias 的合法索引；如果发现第二级别索引来自不可信值，再在同一边界函数校验，不增加无依据的全局兼容层。

#### 关键修改示例

```text
if (findex < 0 || findex >= prog->last_inherited + prog->num_functions_defined) {
  return std::make_pair(nullptr, 0);
}
```

#### 定向验证

- **场景：**手工构建最小合法 program 夹具，测试 -1、0、最后一个、count、count+1；另外覆盖 inherited/alias 的有效路径。
- **profile/真实边界：**Debug、ASan/UBSan；合法继承/别名 LPC 回归。
- **新增 native filter：**`DriverTest.TestAuditFunctionIndex*`，目标 `lpc_tests`。

```bash
N=AUD-019
F='DriverTest.TestAuditFunctionIndex*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_function_lookup`。测试源是 `testsuite/single/tests/efuns/audit_function_lookup.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-019
L=audit_function_lookup
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**所有越界值不读取数组并返回 {nullptr, 0}；边界内含 inherited/alias 查询保持一致；ASan/UBSan 通过。
- **风险与缓解：**不能以更大的额外分配掩盖错误；不在没有攻击入口证据时扩展为任意字节码 RCE 声明。
- **文档同步：**已检查：这是内部半开边界/错误处理修复，现有文档未因此失效；不为它重复添加规范或 README。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-020 · Windows 安装复制旧路径且没有传播复制失败

- **修复目标：**让 CMake install(TARGETS) 管理可执行目标，依赖复制使用目标生成表达式；不拼接旧 src 路径。
- **来源处理策略：**保留 fork 的对外能力，修当前实现根因，不借机删除合同或覆盖已有修复。
- **优先级/来源：**P2 / fork 引入问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/CMakeLists.txt:694–698`、`src/CMakeLists.txt:728–739`。
- **作用范围：**`src/CMakeLists.txt`、`.github/workflows/ci.yml`、`cmake/helper.cmake`。测试归属 `null`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**构建/依赖维护者；4–8 小时。
- **必须击中的根因：**输出目录修改只覆盖 driver/lpcc/lpcshell，Windows 的 Bash cp 特例未同步路径，也没有 RESULT_VARIABLE/失败传播。

#### 操作

1. 移除 Windows+Bash 的硬编码 cp 特例，统一使用当前 else 分支已有的 install(TARGETS driver lpcc lpcshell symbol o2json json2o RUNTIME DESTINATION bin)。不引入新的 DLL 打包框架。
2. 保留其他仍必要的安装内容，不改变发布/部署路径或覆盖用户安装；CMake 根据目标路径和配置选择实际产物。
3. Windows CI 在私有前缀执行安装，检查六种工具存在；重点比对 driver/lpcc/lpcshell 的目标 hash。不能只相信 cmake --install 退出码。

#### 关键修改示例

```text
install(TARGETS driver lpcc lpcshell symbol o2json json2o
        RUNTIME DESTINATION bin)
```

#### 定向验证

- **场景：**Windows 原生单配置和多配置各执行隔离前缀安装，核对目标 exe 的存在、SHA-256 和 build 中目标文件一致，不运行/覆盖已安装实例。
- **profile/真实边界：**Windows 原生 build+私有 prefix install；产物一致性检查。
- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

在真实 Windows 的现有 MSYS2/Ninja 构建环境执行，使用新目录和 prefix。Ninja 是单配置，前三个程序当前在 bin，后三个程序当前在 src；不要假设任意 --version 参数都存在。下列安装验证不声称产物已成为可分发安装包。

```bash
BW="$ROOT/build-audit-windows-$RUN_ID"
P="$E/AUD-020-install"
test ! -e "$BW"; test ! -e "$P"
cmake -S "$ROOT" -B "$BW" -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=OFF
cmake --build "$BW" --target driver lpcc lpcshell symbol o2json json2o --parallel 1
cmake --install "$BW" --prefix "$P"
for t in driver lpcc lpcshell; do
  test -s "$P/bin/$t.exe"
  cmp "$BW/bin/$t.exe" "$P/bin/$t.exe"
done
for t in symbol o2json json2o; do
  test -s "$P/bin/$t.exe"
  cmp "$BW/src/$t.exe" "$P/bin/$t.exe"
done
(cd "$E" && "$P/bin/driver.exe" -V)
```

#### 完成、风险和回退

- **验收：**已支持 Windows 生成器的隔离安装成功，driver/lpcc/lpcshell hash 与实际 build 目标一致，其他工具未丢失；Linux 规则不回退。
- **风险与缓解：**不把 MinGW 的 DLL 依赖解析强行套给 MSVC；只有已支持的生成器才能声明通过。
- **文档同步：**`docs/build.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-021 · 供应链清单与实际 vendored 源码身份不一致

- **修复目标：**以 manifest 为身份单一来源，记录可核对的 commit/版本/补丁；按 source URL 构建 purl，增加不先覆写产物的 check。
- **来源处理策略：**保留 fork 的对外能力，修当前实现根因，不借机删除合同或覆盖已有修复。
- **优先级/来源：**P2 / fork 引入问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`third_party/manifest.yaml:68–76`、`src/thirdparty/libwebsockets/CMakeLists.txt:500–507`、`tools/sbom-generate.py:30–43`。
- **作用范围：**`third_party/manifest.yaml`、`tools/sbom-generate.py`、`tools/test-sbom.py`、`docs/project-modernization-plan-2026-10.md`。测试归属 `tools/test-sbom.py`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**构建/依赖维护者；8–16 小时。
- **必须击中的根因：**自报版本/patch 元数据未与实际 source revision 绑定，purl 用展示 name 拼装；生成后 validate 不能证明提交中的原 SBOM 未漂移。

#### 操作

1. 核对每个 vendored 组件实际版本、源码 commit、许可证和相对该来源的补丁；libwebsockets 先修正已确认版本，精确来源无法恢复则显式标记 unverified。
2. manifest 增加 GitHub owner/repo 或使用已记录 source URL 的规范解析；purl 采用 owner/repo，不从人类展示名猜 namespace。
3. 保留 sbom-generate 生成操作的明确写入语义；新增 --check，只在内存生成期望值并比较已提交 sbom.json，不先覆盖后验证。CI 改用 --check。
4. 用已确认元数据重生成 sbom.json 和相关哈希；为错误版本、owner 缺失、patch 漂移、已提交 SBOM 过期与重复运行幂等补测试。
5. 联网核对安全公告是外部前置；不能因本次访问失败写成“没有漏洞”。不做依赖升级或 release。

#### 关键修改示例

```text
# 输出 purl 的逻辑示例：owner/repo 必须来自已验证来源。
purl = f"pkg:github/{owner}/{repo}@{version}"
```

#### 定向验证

- **场景：**离线比较 manifest、实际源码版本、已知 source revision、补丁差异与生成 SBOM；检查各组件 purl owner/repo；修改输入后的只读 check 必须失败。
- **profile/真实边界：**Python 离线单元测试、目录摘要/版本检查、SBOM 只读漂移检查。
- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

新增离线 `tools/test-sbom.py`，用 unittest 入口，不访问在线 advisory 或 registry。生成器新增的 `--check` 只比较并以非零退出报告漂移，不覆盖目标；未知来源/补丁不得自动填成 none。使用 E 内 fixture，不能篡改真实第三方目录制造测试。

```bash
TMPDIR="$E/tmp" "$PY" -B tools/test-sbom.py
"$PY" -B tools/sbom-generate.py --check
```

#### 完成、风险和回退

- **验收：**manifest/source/SBOM 三方身份可追溯；修改任一输入会使 --check 非零且不改文件；生成两次结果稳定；无法确认项明示。
- **风险与缓解：**此项不是授权升级全部依赖、发布 SBOM 或上传服务；无法证明精确 vendored 基线时写不确定，不编造 commit。
- **文档同步：**`docs/vendor-libwebsockets-manifest-2026-08.md`、`docs/project-modernization-plan-2026-10.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-022 · owner 状态把历史验收写死为当前 ready

- **修复目标：**立即停止无条件 current-ready 声明；历史说明与当前可证明状态分离，只有绑定当前构建的真实验证结果才能置 ready。
- **来源处理策略：**保留 fork 的对外能力，修当前实现根因，不借机删除合同或覆盖已有修复。
- **优先级/来源：**P2 / fork 引入问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/vm/internal/owner.cc:1602–1631`、`docs/multicore-production-gate.md:6–15`、`docs/multicore-production-gate.md:124–127`。
- **作用范围：**`src/vm/internal/owner.cc`、`docs/multicore-production-gate.md`、`tools/docs/check-evidence.py`、`docs/evidence/manifest.schema.json`、`src/tests/test_lpc_owner.cc`。测试归属 `src/tests/test_lpc_owner.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**核心维护者/文档责任人；8–16 小时。
- **必须击中的根因：**状态字段脱离当前 commit、构建配置及原始证据有效性；权威合同要求失效时回到 incomplete/blocked，但代码没有该判断。

#### 操作

1. 保持公开字段名和类型；当没有当前绑定证据时 ready=0、状态=unverified/blocked，并提供明确原因。现存字符串只能标为 historical，不能继续作为 current digest。
2. 复用 tools/docs/check-evidence.py 的 current/source_sha/tested_sha/raw digest 语义，不另建 release gate；先补缺失/过期证据的负例。
3. 本原子单元默认交付 fail-closed 状态，不新建未授权线上压力、真实容量测试或伪造“当前通过”记录。将恢复 ready=1 的前置写入权威文档：真实运行、绑定本二进制、验证通过。
4. 如果实现自动装载验证结果，需要另行批准明确的输入格式/可信边界；本方案不隐式授权新增运行时配置或外部证据服务。

#### 关键修改示例

```text
// 没有绑定当前二进制的有效证据时，不报告 current-ready。
add_mapping_pair(map, "production_gate_ready", 0);
add_mapping_pair(map, "production_gate_status", "unverified");
```

#### 定向验证

- **场景：**无证据、历史证据、commit 不符、原始日志摘要损坏和当前有效 envelope；验证 API 状态不能仅凭常量报告 ready。
- **profile/真实边界：**Debug 原生/LPC；既有 evidence validator/gate 的失败矩阵。
- **新增 native filter：**`DriverTest.TestAuditOwnerEvidenceState*`，目标 `lpc_tests`。

```bash
N=AUD-022
F='DriverTest.TestAuditOwnerEvidenceState*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_owner_evidence`。测试源是 `testsuite/single/tests/efuns/audit_owner_evidence.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-022
L=audit_owner_evidence
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**缺证据/旧证据时所有 current-ready 相关声明一致为未验证；历史数据可辨认；既有可见键/类型保持；无新的绕过开关。
- **风险与缓解：**不能为让 ready 回到 1 伪造日志或加入可手工绕过的配置开关；不引入本项目不需要的正式发布认证体系。
- **文档同步：**`docs/multicore-production-gate.md`、`docs/owner-multicore-api.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-023 · 权威排版说明仍描述已被 CI 扩大的 pilot 范围

- **修复目标：**仅更新实际归属文档的范围和命令，去除失效过渡语句；不再次全仓重排代码。
- **来源处理策略：**保留 fork 的对外能力，修当前实现根因，不借机删除合同或覆盖已有修复。
- **优先级/来源：**P3 / fork 引入问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`docs/coding-standard.md:29–29`、`.github/workflows/ci.yml:87–104`。
- **作用范围：**`docs/coding-standard.md`。测试归属 `null`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**核心维护者/文档责任人；2–4 小时。
- **必须击中的根因：**格式门禁扩大后，权威说明和过渡状态未同步。

#### 操作

1. 以 tools/style/check-format.py 和 CI 实际参数为事实来源，更新 coding-standard 的原生源码门禁范围与固定 clang-format 版本。
2. 复核 U10 状态及仍写 pilot/过渡门禁的实际归属引用，只修改已失效叙述，不重写冻结历史报告。
3. 核对命令存在、链接可解析、LPC 原始字节及 EOF fixture 排除项未变；未安装固定工具时如实记未运行，不临时用另一版本替代。

#### 关键修改示例

```text
-当前必过格式范围仅为 pilot 文件。
+原生源码格式门禁使用 `tools/style/check-format.py --all-native`；
+具体发现与排除规则以该工具为准，生成文件和第三方文件不手工重排。
```

#### 定向验证

- **场景：**逐字对照 check-format.py 的 --all-native 发现/排除规则、CI 调用与文档，确认 generated/thirdparty/raw-byte fixture 的边界一致。
- **profile/真实边界：**git diff --check、文档/CLI/排除清单一致性；不要求无关构建。
- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

逐项核对 `.github/workflows/ci.yml` 的 --all-native、tools/style/check-format.py 的 own/native 排除规则，以及规范中试点/全量表述；不执行全库格式写入。运行 `git diff --check -- docs/coding-standard.md`，并人工核对文档路径、CLI 和生成/第三方排除范围。

#### 完成、风险和回退

- **验收：**规范与 CI 的范围/版本/命令一致；没有源码或生成物重排；链接和 diff 检查通过。
- **风险与缓解：**不能把排版检查扩大为未授权全仓命名重构；只改文档，不动第三方/生成文件。
- **文档同步：**`docs/coding-standard.md`、`docs/project-modernization-plan-2026-10.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-024 · 构建手册混淆数据库编号，并给出失配的测试/并行命令

- **目标/优先级/来源：**把 SQLite 参数解释改为唯一后端编号；给出 SQLite-only 与多后端的完整有效编号/默认值示例；修正测试 cwd，并把该页不限并行示例按项目规则改为明确上限。 P3；上游继承问题。
- **范围/前置：**只编辑 `docs/build.md`；读取 `docs/efun/db/db_connect.md`、`src/CMakeLists.txt`、`src/packages/db/CMakeLists.txt`、`AGENTS.md`。无其他 ID 实现前置。
- **根因与来源处理：**上游遗留构建说明没有按真实 CMake 参数语义、当前目录和 fork 的资源约束同步校准。 保留正确参数名和 fork 行为，仅校准上游遗留文档；不升级数据库或回移无关代码。

#### 操作

1. 说明 PACKAGE_DB_SQLITE=X 中 X 是 LPC 可见的数据库后端编号，不是 SQLite API 版本；启用后端编号必须不同，DEFAULT_DB 必须选已启用编号。
2. SQLite-only 示例显式使用 MYSQL=""、SQLITE=1、DEFAULT_DB=1；多后端示例可使用 MYSQL=1、SQLITE=2、DEFAULT_DB=1。不能把 2 解释为更新版本。
3. Running Tests 同一命令块从 build 返回 ../testsuite，再使用 ../build/bin/driver；或明确拆成各自从仓库根开始的两段。不能依赖隐含 cwd 重置。
4. 检索该页所有 nproc/无限并行示例，改为项目普通/ASan 明确上限并引用 AGENTS §4.2 的资源检查；本机执行仍遵守更保守的 --parallel 1。
5. 只对文档做 diff/链接/命令路径检查，不为验证说明去启动现有 driver 或覆盖现有构建。

#### 关键修改示例

```text
PACKAGE_DB_SQLITE=<id> sets the database backend ID exposed to LPC.
It does not select a SQLite API version. Enabled backend IDs must be unique.
PACKAGE_DB_DEFAULT_DB must name one enabled backend.
```

#### 验证、验收与回退

- **验证：**静态对照 src/CMakeLists.txt、src/packages/db/CMakeLists.txt、docs/efun/db/db_connect.md 和 AGENTS §4.2；按命令序列推导 cwd；不实际 configure、安装或启动 driver。

```bash
git diff --check -- docs/build.md
```

- **C++/LPC/Sanitizer：**纯文档改动不适用；编号实现的动态回归属于 AUD-014，不在此假造通过记录。
- **验收：**页面不再出现 SQLite API version 1/2 的误说；示例编号/default 自洽，测试 cwd 可逐步解析，不建议无限并行；参数名和真实构建合同一致。
- **风险/缓解：**只改文档，不升级 SQLite、不更改编号语义、不把构建示例变成实际部署；保留用户可搜索的参数名。
- **回滚：**按 §4.2 R1 逆向本 ID 文档提交，不回退其他源码或构建状态。
- **工时/角色：**2–4 小时；构建文档维护者。
- **归属文档：**`docs/build.md`。
- **当前状态：**未实施。
<a id="aud-025"></a>
### AUD-025 · 冷启动 ScratchArenaBinding 留下空 current_

- **优先级 / 责任视角：**P1；C++ / 内存与编译器。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**冷 binding 记录空 chunk。首次分配建立 base_，回退仍把 current_ 置空，且漏减 base_ 使用量。下一次 alloc 因 base_ 非空而解引用空 current_。 修复后空起始 mark → base_->used 清零、统计回退、current_=base_。
- **定位 / 改动文件：**src/compiler/internal/compile_arena.cc:174–208、220–240、285–324；`src/compiler/internal/compile_arena.cc`、`src/tests/test_compile_arena.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**合法内部调用顺序导致崩溃及 live/cycle 计量失真；未证实普通 LPC 直接控制该顺序。
- **预计工时：**4–8 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**arena 回退或溢出拒绝可能破坏嵌套 scope、代际和统计；保留 LIFO 合同，并验证拒绝后用量与原有绑定仍有效。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“current_ = chunk; // chunk 为空而 base_ 已存在”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 空起始 mark 回退时归还新 base_ 的 used，并把 current_ 恢复到可复用 base_；保留 retained/live 链。
3. 保持 generation 和严格 LIFO 拒绝逻辑；bytes_released、cycle_bytes、bytes_live 同步回退。
4. 补冷/嵌套 binding、oversize、异常退出和下一次 begin/reset。
5. 退出冷 binding 后计量归零且可再次分配；内层回退不破坏外层数据；次序/代际错误仍拒绝。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `src/compiler/internal/compile_arena.h`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：current_ = chunk; // chunk 为空而 base_ 已存在。
- 后：空起始 mark → base_->used 清零、统计回退、current_=base_。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if (chunk) {
  released += chunk->used > used ? chunk->used - used : 0;
  chunk->used = used;
} else if (base_) {
  released += base_->used;
  base_->used = 0;
}
current_ = chunk ? chunk : base_;
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-025
B="$BD"
cmake --build "$B" --target compile_arena_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/compile_arena_tests" --gtest-filter 'CompileArenaTest.AuditColdBinding*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“退出冷 binding 后计量归零且可再次分配；内层回退不破坏外层数据；次序/代际错误仍拒绝。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_arena_cold_binding.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-026"></a>
### AUD-026 · ScratchArena 尺寸运算可回绕

- **优先级 / 责任视角：**P2；C++ / 内存与编译器。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**align_up、header+size、used+need、alloc_string(len+1) 都先作未检查的 size_t 加法，极端参数可变成小分配或重叠区间。 修复后先检查加法；if(need>capacity-used)。
- **定位 / 改动文件：**src/compiler/internal/compile_arena.cc:32–45、174–216；`src/compiler/internal/compile_arena.cc`、`src/tests/test_compile_arena.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**内部分配合同缺口，不能据此宣称远程可利用。
- **预计工时：**4–8 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**arena 回退或溢出拒绝可能破坏嵌套 scope、代际和统计；保留 LIFO 合同，并验证拒绝后用量与原有绑定仍有效。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“need=align_up(size); if(used+need>capacity)”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 分配/游标变化前检查 len+1、对齐加数、header+size；非法尺寸抛 std::bad_alloc，不返回可用指针。
3. 用 need > size-used 判断剩余空间；保留零尺寸返回空的合同。
4. 只验证极值被拒绝和状态不变，不实际申请巨量内存；成功分配继续验证无重叠。
5. 0、对齐和块边界正常；各溢出邻界拒绝，计量不变；后续小分配可用。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `src/compiler/internal/compile_arena.h`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：need=align_up(size); if(used+need>capacity)。
- 后：先检查加法；if(need>capacity-used)。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
constexpr bool size_add_fits(size_t left, size_t right) noexcept {
  return right <= std::numeric_limits<size_t>::max() - left;
}
// 对齐加法、Chunk 头部加法、字符串终止字节都须在实际运算前使用同一检查。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-026
B="$BD"
cmake --build "$B" --target compile_arena_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/compile_arena_tests" --gtest-filter 'CompileArenaTest.AuditAllocationOverflow*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“0、对齐和块边界正常；各溢出邻界拒绝，计量不变；后续小分配可用。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_arena_size_bounds.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-027"></a>
### AUD-027 · 编译永久改变进程 locale

- **优先级 / 责任视角：**P1；LPC 编译器 / C++。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**setlocale(LC_ALL,"C") 返回修改后的值而不是原值，DEFER 恢复的仍是 C；切换本身也影响其他线程。 修复后只绑定本线程的 C locale，退出恢复原线程状态。
- **定位 / 改动文件：**src/compiler/internal/compiler.cc:2215–2220；`src/compiler/internal/compiler.cc`、`src/tests/test_lpc_compiler.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**后续数值转换、格式和并发线程的 locale 行为改变。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“保存 setlocale(...,"C") 的返回值”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 移除进程全局 LC_ALL 切换，改为编译器内部线程局部 C-locale 作用域。
3. POSIX 用 newlocale/uselocale；Windows 用 CRT 线程局部模式；恢复先前线程 locale/模式，失败报告。
4. 覆盖成功、错误和嵌套退出；并行观察线程不得看到 locale 改变。
5. 非 C locale 下 LPC 小数语义稳定，前后 locale 相同，观察线程不受影响；缺平台记 external-required。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `src/compiler/internal/compiler.cc`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：保存 setlocale(...,"C") 的返回值。
- 后：只绑定本线程的 C locale，退出恢复原线程状态。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
// POSIX 路径的关键动作；compiler_locale 由本次作用域拥有。
locale_t previous = uselocale(compiler_locale);
// RAII 析构先恢复 previous，再 freelocale(compiler_locale)。
// Windows 用已验证的线程 CRT locale 作用域，不退回进程全局切换。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-027
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditCompilerLocale*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“非 C locale 下 LPC 小数语义稳定，前后 locale 相同，观察线程不受影响；缺平台记 external-required。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_compiler_locale.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-028"></a>
### AUD-028 · 内建预定义字符串构造无容量边界

- **优先级 / 责任视角：**P2；LPC 编译器 / C++。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**加引号值由 strcpy/strcat 拼入 MAXLINE，版本先 sprintf 到 1024 字节数组；这些内建构建元数据构造路径实际可达。 修复后动态加引号构造；版本直接传入。
- **定位 / 改动文件：**src/compiler/internal/lex.cc:3219–3225、3263–3276；`src/compiler/internal/lex.cc`、`src/compiler/internal/lex.h`、`src/tests/test_lpc_compiler.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**元数据长度合同缺失；未证明玩家输入可达。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“固定数组 strcat；版本先 sprintf 到 1024 字节数组”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 版本和加引号值改为单一动态字符串构造，保持内容不变，直接把 PROJECT_VERSION 传入加引号构造。
3. 只改这条实际使用的构造路径；lpc_predefs 无生产写入者，不恢复 -D，不把旧列表的 sscanf 顺手纳入本单元。
4. 让测试调用同一内部生产构造函数，验证短值、长值及邻界；不新增 LPC 接口。
5. 短值逐字保持；长构建元数据安全构造或按既有宏长度上界明确拒绝；版本不经过固定 sprintf 缓冲。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `src/compiler/internal/lex.h`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：固定数组 strcat；版本先 sprintf 到 1024 字节数组。
- 后：动态加引号构造；版本直接传入。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
std::string quoted = "\"" + std::string(val) + "\"";
add_predefine(def, -1, quoted.c_str());
// 版本调用直接使用 PROJECT_VERSION，不再先 sprintf 到 version[1024]。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-028
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditPredefineBounds*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“短值逐字保持；长构建元数据安全构造或按既有宏长度上界明确拒绝；版本不经过固定 sprintf 缓冲。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_predefine_bounds.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-029"></a>
### AUD-029 · 宏实参采集先越界再报错

- **优先级 / 责任视角：**P0；LPC 编译器 / C++。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**extract_args 的空首参数读 out-1；逗号分支 argv[++argc] 可先写 argv[NARGS]，下一轮才到数量上限检查。调用者传栈数组。 修复后先检查参数起点与下一槽，再读写。
- **定位 / 改动文件：**src/compiler/internal/lex.cc:4307–4406、4410–4413；`src/compiler/internal/lex.cc`、`src/tests/test_lpc_compiler.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**编译期边界前读和栈越界写，后续诊断不能撤销写入。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“uisspace(*(out-1)); argv[++argc]=out;”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 收尾先确认 out>argv[argc]；空实参直接写终止符，不读前字节。
3. 递增 argc 和写下一槽前检查槽边界，保留宏形参数量错误语义。
4. 覆盖空首/中/末参数、嵌套括号/引号、数量边界及错误后再次编译。
5. 合法输入不回归，超量先拒绝；ASan/UBSan 无越界，下一次正常编译成功。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `src/compiler/internal/lex.cc`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：uisspace(*(out-1)); argv[++argc]=out;。
- 后：先检查参数起点与下一槽，再读写。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if (out > argv[argc] && uisspace(*(out - 1))) {
  *(out - 1) = 0;
} else {
  *out++ = 0;
}
// 放在 parcnt < 0 的正常返回之后、argv[++argc] 之前：
if (argc + 1 >= NARGS) {
  lexerror("Maximum macro argument count exceeded");
  return -1;
}
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-029
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditMacroArguments*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“合法输入不回归，超量先拒绝；ASan/UBSan 无越界，下一次正常编译成功。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_macro_arguments.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-030"></a>
### AUD-030 · 字符候选绕过预处理指令长度上限

- **优先级 / 责任视角：**P1；LPC 编译器 / C++。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**append 限制 DEFMAX，十六/八进制候选却先无限追加 char_candidate。续行可在回放到受限输出前占用任意候选内存。 修复后候选与输出共享长度预算，超限不再增长。
- **定位 / 改动文件：**src/compiler/internal/lex.cc:3783–3893、3987–4028；`src/compiler/internal/lex.cc`、`src/tests/test_lpc_compiler.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**编译期内存和 CPU 放大；受限输出数组不等于受限临时存储。
- **预计工时：**8–16 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“char_candidate.push_back 不受 append 上限保护”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 每次候选追加前按已输出+待决候选总量检查 DEFMAX，保持单一追加规则。
3. 超限锁存原超长诊断并终止该编译，不截断后当合法源；保留正确行号与下次编译状态。
4. 覆盖十六/八进制、缺结束引号、LF/CRLF 续行和字符/注释歧义。
5. 候选峰值受限；超长有确定诊断；边界内源码、位置映射及下一次编译一致。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `src/compiler/internal/lex.cc`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：char_candidate.push_back 不受 append 上限保护。
- 后：候选与输出共享长度预算，超限不再增长。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
bool append_pending(std::string& pending, char byte, size_t remaining) {
  if (pending.size() >= remaining) return false;
  pending.push_back(byte);
  return true;
}
// 等价检查落在既有捕获器中；remaining 来自主输出预算，false 必须终止该指令。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-030
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditDirectiveCandidateBudget*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“候选峰值受限；超长有确定诊断；边界内源码、位置映射及下一次编译一致。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_directive_candidate_budget.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-031"></a>
### AUD-031 · case 常量移位漏掉低六位合同

- **优先级 / 责任视角：**P2；LPC 编译器 / C++。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**constant 左右移直接使用 $3，未按运行时合同取低六位；负数或不小于 64 的计数触发 C++ UB/构建差异。 修复后LPC 64 位移位语义；count &= 63。
- **定位 / 改动文件：**src/compiler/internal/grammar.y:1235–1242；`src/compiler/internal/grammar.y`、`src/tests/test_lpc_compiler.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**编译结果偏离 LPC 模 64 语义。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“$$=$1<<$3; $$=$1>>$3;”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 复用安全 LPC 移位实现，计数取低六位；左移用无符号位运算，不拒绝合同允许的计数。
3. 同步 Bison fallback 和 generate_ebnf 输出，不手改生成物。
4. 比对运行时、普通折叠、case 常量三条路径。
5. -1/0/63/64/65、极值计数和符号边界结果一致，UBSan 无 shift 报告。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `src/compiler/internal/grammar.y`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：$$=$1<<$3; $$=$1>>$3;。
- 后：LPC 64 位移位语义；count &= 63。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
/* grammar.y 的 number 移位动作；右移仍保持 LPC 的算术右移语义。 */
$$ = static_cast<LPC_INT>(static_cast<uint64_t>($1) << ($3 & 63));
/* 对应右移分支： */
$$ = $1 >> ($3 & 63);
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-031
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditCaseShift*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“-1/0/63/64/65、极值计数和符号边界结果一致，UBSan 无 shift 报告。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_case_shift.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-032"></a>
### AUD-032 · replace_string 跳表复制漏记目标长度

- **优先级 / 责任视角：**P0；efun / VM / 生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**增长分支的 skip_table 复制推进 src/dst2，却不增加 dlen，也不检查目标剩余容量。后续检查使用偏小 dlen。 修复后先检查 remaining，再复制并更新唯一长度。
- **定位 / 改动文件：**src/packages/core/efuns_main.cc:2261–2318；`src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**可写过按最大字符串长度分配的缓冲。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“skip 只推进 dst2，后续用旧 dlen”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. skip、替换、单字节及尾段共用实际已写长度，所有路径复制前检查剩余容量。
3. 保持超限失败返回、弹栈和释放合同，不以写后检查代替。
4. 覆盖 first/last 范围、连续跳过、缩短/增长及容量邻界。
5. 参考结果逐字节一致，越界在复制前拒绝；ASan/清理检查全部路径。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/strings/replace_string.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：skip 只推进 dst2，后续用旧 dlen。
- 后：先检查 remaining，再复制并更新唯一长度。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if (skip >= max_string_length - dlen) {
  pop_n_elems(st_num_arg);
  push_svalue(&const0u);
  FREE_MSTR(dst1);
  return;
}
// 随后保留既有 skip 字节复制，并同步已写计数：
dlen += skip;
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-032
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditReplaceStringCapacity*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“参考结果逐字节一致，越界在复制前拒绝；ASan/清理检查全部路径。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_replace_string_capacity.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-033"></a>
### AUD-033 · TLS 重试把负返回值当发送偏移

- **优先级 / 责任视角：**P1；网络 / TLS / 生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**SSL_write 的 WANT 等负返回被判可继续，off 却未归零，写入 w_off 与 len-off。 修复后retry → consumed=0，仅正消费量推进。
- **定位 / 改动文件：**src/packages/sockets/socket_efuns.cc:1145–1226；`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**后续可从缓冲起点之前读取，队列和网络统计错误；未证实代码执行。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“重试保留 off<0，随后 w_off=off”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 可重试错误归一为消费 0 字节；只有正返回值推进偏移和统计。
3. 按 WANT_READ/WANT_WRITE 等待正确事件，遵守同一数据重试要求。
4. 注入 WANT、EINTR、部分成功、致命错误并检查生产队列。
5. 0<=w_off<=总长度、剩余长度正确；无前缀泄漏/重发/漏发，统计不负增。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/sockets/socket_write.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：重试保留 off<0，随后 w_off=off。
- 后：retry → consumed=0，仅正消费量推进。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
case SSL_ERROR_WANT_READ:
case SSL_ERROR_WANT_WRITE:
  // 本次没有写出应用字节；沿用既有重试事件选择。
  off = 0;
  break;
// 其他负返回仍走既有失败分支，不归一化为成功。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-033
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditTlsWriteRetry*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“0<=w_off<=总长度、剩余长度正确；无前缀泄漏/重发/漏发，统计不负增。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_tls_write_retry.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-034"></a>
### AUD-034 · sscanf 的 %s%x 搜索越过终止符

- **优先级 / 责任视角：**P1；efun / VM / 生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**尾部单个 0 无 x 前缀时仍 tmp+=2，循环随后读取终止符之后的位置。 修复后先判终止，再有界推进。
- **定位 / 改动文件：**src/vm/internal/base/interpret.cc:5400–5414；`src/vm/internal/base/interpret.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**越界读取和不可信匹配结果。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“非前缀尾部 0 仍 tmp+=2 后解引用”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 后继字符存在才检查前缀；无前缀按仍在字符串内的步幅推进。
3. 前缀完整且第三字节存在才检查该字节。
4. 保留匹配计数、未匹配参数和其他转换语义。
5. 空输入、尾部 0、完整/不完整前缀、合法十六进制和多转换；ASan 无越界。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/strings/sscanf.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：非前缀尾部 0 仍 tmp+=2 后解引用。
- 后：先判终止，再有界推进。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
// 只在未识别到合法 0x/0X 前缀的分支推进一个字节：
++tmp;
// 不使用 tmp += 2；下一轮在读取 *tmp 时仍位于原字符串或终止字节。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-034
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditSscanfHexTail*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“空输入、尾部 0、完整/不完整前缀、合法十六进制和多转换；ASan 无越界。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_sscanf_hex_tail.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-035"></a>
### AUD-035 · TLS 延迟刷新前已释放并清空 SSL

- **优先级 / 责任视角：**P1；网络 / TLS / 生命周期。
- **依赖：**AUD-060 先完成对应公共入口或测试合同。
- **目标：**socket_close 在判断 S_BLOCKED 延迟关闭前 SSL_free 并置空；刷新仍按 TLS 模式调用 SSL_write。 修复后先选 flush/final，TLS 只在 final 释放。
- **定位 / 改动文件：**src/packages/sockets/socket_efuns.cc:2031–2074；`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**向 SSL_write 传入空 SSL，可能崩溃；不是 UAF，也不是 comm.cc interactive 清理。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“SSL_free → ssl=null → STATE_FLUSHING”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 先决定 flush/final；普通关闭需刷新时保留 SSL/CTX 到最终关闭。
3. 最终路径一次释放 TLS/事件/队列，回调只通知一次并保留身份重查。
4. 与 AUD-060 共核完整关闭状态机。
5. 普通待发按合同完成后恰好释放一次；强制/致命关闭不续写、不传空 SSL。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/sockets/socket_close.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：SSL_free → ssl=null → STATE_FLUSHING。
- 后：先选 flush/final，TLS 只在 final 释放。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
// 此块必须位于“仍需 STATE_FLUSHING，返回等待续写”的分支之后。
if (lpc_socks[fd].ssl) {
  SSL_free(lpc_socks[fd].ssl);
  lpc_socks[fd].ssl = nullptr;
}
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-035
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditTlsFlushClose*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“普通待发按合同完成后恰好释放一次；强制/致命关闭不续写、不传空 SSL。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_tls_flush_close.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-036"></a>
### AUD-036 · TLS accept 早失败遗漏事件资源

- **优先级 / 责任视角：**P1；网络 / TLS / 生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**事件和 ev_data 已分配，SSL_CTX 缺失/SSL_new 失败却只 close fd 再 clear_socket，资源指针被清掉。 修复后完整撤销资源，然后清槽。
- **定位 / 改动文件：**src/packages/sockets/socket_efuns.cc:748–783；`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**事件/上下文反复泄漏及旧事件关联；fd 已关闭，不能报 fd 泄漏。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“close(accept_fd); clear_socket(i,1);”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 早失败汇入完整新槽撤销，释放事件/上下文再清槽，fd 只关闭一次。
3. 不交付半初始化槽，不绕过身份/回调约束。
4. 注入两处失败，推进夹具 event_base 后检查资源。
5. 重复失败后事件/上下文/fd 回基线；槽复用不接受旧回调；正常 accept 不回归。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/sockets/socket_accept.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：close(accept_fd); clear_socket(i,1);。
- 后：完整撤销资源，然后清槽。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
using EventPtr = std::unique_ptr<event, decltype(&event_free)>;
EventPtr read_event(nullptr, event_free);
EventPtr write_event(nullptr, event_free);
// 新 event 先交给这两个局部 owner；完整成功后才 release 到 socket 槽位。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-036
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditTlsAcceptCleanup*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“重复失败后事件/上下文/fd 回基线；槽复用不接受旧回调；正常 accept 不回归。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_tls_accept_cleanup.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-037"></a>
### AUD-037 · runner 拒绝合法百分号文件名

- **优先级 / 责任视角：**P2；测试工具 / CI / 证据。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**validate_case 字符白名单排除 %，拒绝已有 warn_%s 和 warn_%s_a，driver 尚未启动。 修复后组件结构 + 文件归属 + argv 原样传递。
- **定位 / 改动文件：**tools/testsuite/run-targeted.py:746–759；`tools/testsuite/run-targeted.py`、`tools/testsuite/test-run-targeted.py`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**用例不能通过定向入口执行，不是编译失败。
- **预计工时：**4–8 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**不能通过放宽路径边界、降低证据身份或静默跳过解决门禁失败；负向自测必须继续拒绝路径逃逸、零断言、字节被替换和异步未完成。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“/[A-Za-z0-9_.-]+ 白名单”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 按组件和根归属校验；拒绝 NUL/控制字符、反斜杠、空组件与 ./..，允许语料合法字符。
3. 确认实际 .c/.lpc 文件位于隔离 mudlib，继续 argv 传参、不经 shell。
4. 补现有 %/Unicode 文件名与穿越/符号链接越根负例，再重跑两项。
5. 两项真实执行且身份一致；越根启动前拒绝；少跑/异步门禁不放宽。 正常、精确边界、失败与异常/清理必须分别有断言。同时保留 runner 的 selected/executed、断言与异步完成门禁。
6. 同步 `tools/testsuite/README.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：/[A-Za-z0-9_.-]+ 白名单。
- 后：组件结构 + 文件归属 + argv 原样传递。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```diff
- [A-Za-z0-9_.-]
+ [A-Za-z0-9_.%-]
# 仅示意既有文件名字符类增加 %；前缀、分量、root containment 检查不变。
```

**定向命令与通过标准**

保留现有用例身份；需要新增自测时在实际 runner/测试入口注册。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-037
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --tool "$PY" --timeout 180 --evidence-dir "$E/$N-runner-selftest" \
  -- -B tools/testsuite/test-run-targeted.py
cmake --build "$BD" --target driver --parallel 1
for CASE in 'warn_%s' 'warn_%s_a'; do
  HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
    --driver "$BD/bin/driver" --case "/single/tests/compiler/$CASE" \
    --timeout 180 --evidence-dir "$E/$N-$CASE"
done
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“两项真实执行且身份一致；越根启动前拒绝；少跑/异步门禁不放宽。”。 本卡使用上列既有测试身份和 runner/CI 自测，不新增无关 LPC runtime 文件。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；本卡已实施：`validate_case()` 允许合法 `%` 组件，保留组件/隔离根校验和 argv 原样传递；runner 自测及 `warn_%s`、`warn_%s_a` 两个真实入口均通过。证据见 `build-modernization-evidence/audit-f3317359-20261013T001301Z/` 下的 `AUD-037-after-selftest-v2.log`、`AUD-037-after-warn_%s/` 和 `AUD-037-after-warn_%s_a/`；修前拒绝记录保留在对应 `AUD-037-before-*` 目录。该卡状态为 `fixed/linux-debug`，尚未代表其余平台或 69 项整体完成。

<a id="aud-038"></a>
### AUD-038 · SQLite 异步回归未接完成屏障

- **优先级 / 责任视角：**P2；LPC / 测试合同。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**两项虽为 runtime，却未登记异步 token。do_tests 返回即完成；全量报 assertion after case completion，单例先结束后约 183 秒超时。 修复后runtime + begin_async()/complete_async(token) 围住真实回调。
- **定位 / 改动文件：**testsuite/single/tests/efuns/async_db_exec_bound_args.lpc:17–44；testsuite/single/tests/efuns/db.c:93–108；testsuite/etc/test-scopes.tsv:10、36；`testsuite/single/tests/efuns/async_db_exec_bound_args.lpc`、`testsuite/single/tests/efuns/db.c`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**回归覆盖和完成时序失真；单例超时后端根因未另证，不能猜成 DB 锁。
- **预计工时：**8–16 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**分类和完成协议会改变统计；能力必须来自被测 driver，排除项单列且不计通过，真实异步回调完成前不能归还 token。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“do_tests 返回时没有登记未完成 token”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 保持现有 runtime 角色及已正确声明的 async/db/sqlite 能力依赖，不改分类/门禁。
3. 在 do_tests 返回前调用 /command/tests->begin_async() 并保存 token；最后一个回调完成断言及 db_close 后调用 complete_async(token)。
4. 用真实 DB 回调推进，替代固定延时；保留 bound args 与拒绝 string callback 的既有回归。
5. 完成标记晚于最终回调，两项不超时；禁止回调/故意失败必须失败；仍挂起则另追 shutdown。 正常、精确边界、失败与异常/清理必须分别有断言。同时保留 runner 的 selected/executed、断言与异步完成门禁。
6. 同步 `tools/testsuite/README.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：do_tests 返回时没有登记未完成 token。
- 后：runtime + begin_async()/complete_async(token) 围住真实回调。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
private nosave int completion_token;
// do_tests 在注册真实异步工作前：
completion_token = "/command/tests"->begin_async();
// 只在最后一个真实回调完成断言及关闭数据库之后：
"/command/tests"->complete_async(completion_token);
```

**定向命令与通过标准**

保留现有用例身份；需要新增自测时在实际 runner/测试入口注册。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-038
cmake --build "$BD" --target driver --parallel 1
for L in async_db_exec_bound_args db; do
  HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
    --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
    --timeout 180 --evidence-dir "$E/$N-$L-debug"
done
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“完成标记晚于最终回调，两项不超时；禁止回调/故意失败必须失败；仍挂起则另追 shutdown。”。 本卡使用上列既有测试身份和 runner/CI 自测，不新增无关 LPC runtime 文件。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；本卡已实施：`async_db_exec_bound_args` 和 `db` 在真实异步工作注册前登记 token，并仅在最终回调断言与 `db_close()` 后完成 token；移除了原固定轮询完成假设。使用显式 SQLite 的新 Debug 构建 `build-audit-debug-57bf8a02-20261013T001301Z`，两项均为 `selected=1,passed=1,failed=0,skipped=0`，无超时或诊断。证据见 `build-modernization-evidence/audit-f3317359-20261013T001301Z/AUD-038-after-built-v2-async_db_exec_bound_args/` 和 `AUD-038-after-built-v2-db/`；修前失败记录保留在 `AUD-038-before-*`，未启用 SQLite 的错误构建结果另行保留，不计通过。该卡状态为 `fixed/debug-sqlite`，尚未代表其余平台或 69 项整体完成。

<a id="aud-039"></a>
### AUD-039 · 无执行权限对象可占满外部句柄池

- **优先级 / 责任视角：**P1；外部进程 / 权限 / 资源。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**external_create 分配全局持久配额，权限仅在 external_run 检查；被拒绝执行的对象仍可长期占满槽。 修复后权限通过后才能分配全局槽位；失败无资源占用。
- **定位 / 改动文件：**src/packages/external/external.cc:152–166、1340–1357、1363–1388；`src/packages/external/external.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**其他对象无法创建；不等于绕过权限启动进程。当前源码明确把 allocation 视为非特权；增加 create 鉴权是待批准的权限语义调整，不是声称现在可绕过 run 启动进程。
- **预计工时：**8–16 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**create 鉴权将收紧源码明确允许的非特权分配政策，须随方案批准；复用已有权限操作并验证回调重入/销毁与 arity 恢复，不改变 run 时检查。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“alloc_handle_id 先于权限检查”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 复用 external_run/start 的 check_valid_socket("external", -1, current_object, "N/A", -1)，在 external_create 占用槽位以及建立参数 vector 前检查；不新增 efun、clone 入口或 master 操作名。
3. 先保存 st_num_arg，权限回调返回后恢复；拒绝时抛既有风格的工厂调用错误，不预留槽。回调可销毁调用对象，授权后必须重新验证 owner 的存活与 epoch，不能沿用回调前的裸状态。
4. 保留 kMaxHandles 上界与 run 时再次鉴权。测试拒绝对象重复申请、允许对象达到上限、释放后再申请，以及权限回调抛错/重入/销毁；此项改变原先明确允许的非特权分配政策，须随方案批准。
5. 拒绝对象重复 create 不影响授权对象；回调重入/销毁安全；创建后撤权仍不能 run。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/external/external_create.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：alloc_handle_id 先于权限检查。
- 后：权限通过后才能分配全局槽位；失败无资源占用。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
int const num_arg = st_num_arg;
int const allowed = check_valid_socket("external", -1, current_object, "N/A", -1);
st_num_arg = num_arg;
if (!allowed) {
  error("external_create: permission denied.\n");
}
// 回调后先验证 owner 存活与 epoch，再调用 alloc_handle_id()。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-039
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditExternalCreateAdmission*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“拒绝对象重复 create 不影响授权对象；回调重入/销毁安全；创建后撤权仍不能 run。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_external_create_admission.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-040"></a>
### AUD-040 · external_write 截断 stdin 却返回成功

- **优先级 / 责任视角：**P2；外部进程 / 权限 / 资源。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**queue_stdin 复用 append_capped 静默丢超容量字节，随后仍返回 1。 修复后先确认整次可接收，再追加；否则 0。
- **定位 / 改动文件：**src/packages/external/external.cc:124–135、679–687、1524–1533；`src/packages/external/external.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**子进程收到残缺输入，LPC 却得到成功。
- **预计工时：**8–16 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**保留 boolean 返回值；超限整次拒绝且队列不变，不改成字节数、不改变 stdout/stderr 截断策略。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“append_capped(...); return 1;”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 以当前 h->in_buf.size() 和 __MAX_STRING_LENGTH__ 计算剩余容量；flush_stdin 已通过 erase 移除已消费前缀，不另设冗余游标。
3. 在追加/flush 改变状态前判断整次输入能否容纳；超限直接返回 0 且队列不变，能容纳才完整追加。保留 boolean 合同 1=本次全部接收、0=未接收，不改成字节数，也不改变 stdout/stderr 的截断策略。
4. 保留 Created/Running、EOF/close、空输入行为；用有界管道模拟部分写与 EAGAIN，验证满额拒绝、随后重试、逐字节完整和清理。
5. 超限返回 0 且逻辑队列不变；成功字节完全一致，无丢弃/重复。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/external/external_write.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：append_capped(...); return 1;。
- 后：先确认整次可接收，再追加；否则 0。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
bool pending_bytes_fit(size_t pending, size_t incoming, size_t limit) {
  return pending <= limit && incoming <= limit - pending;
}
// 在 append/write 改变任何状态前检查；失败不得留下部分追加。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-040
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditExternalStdinCapacity*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“超限返回 0 且逻辑队列不变；成功字节完全一致，无丢弃/重复。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_external_stdin_capacity.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-041"></a>
### AUD-041 · 非 Debug 零断言用例未预分类

- **优先级 / 责任视角：**P2；LPC / 测试合同。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**constant_expr 的断言全受 __DEBUG__ 保护，优化配置没有对应的预执行能力排除，严格门禁因此拒绝零断言。 修复后明确 debug 能力依赖，不伪记通过。
- **定位 / 改动文件：**testsuite/single/tests/compiler/constant_expr.c:1–39；testsuite/etc/test-scopes.tsv；.github/workflows/ci.yml；`testsuite/etc/test-scopes.tsv`、`testsuite/command/tests.c`、`tools/testsuite/run-targeted.py`、`tools/testsuite/test-run-targeted.py`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**CI 分类缺口，不是缺语言模块；不能改门禁伪通过。
- **预计工时：**4–8 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**分类和完成协议会改变统计；能力必须来自被测 driver，排除项单列且不计通过，真实异步回调完成前不能归还 token。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“条件体 + 默认 runtime”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 既有能力清单增加准确 debug 条件，为该测试声明依赖，不加假断言。
3. 非 Debug 预列配置不支持并保留原因；Debug 必须真实执行。
4. 同步能力生产者、分类校验、自测和 discovered/executed 对账。
5. Debug 有实际断言；优化配置预排除；其他运行用例故意零断言仍失败。 正常、精确边界、失败与异常/清理必须分别有断言。同时保留 runner 的 selected/executed、断言与异步完成门禁。
6. 同步 `tools/testsuite/README.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：条件体 + 默认 runtime。
- 后：明确 debug 能力依赖，不伪记通过。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```text
/single/tests/compiler/constant_expr	runtime	debug	Requires compiler debug introspection.
# debug 从该 driver 的 __DEBUG__ 派生，并加入 runner 的已知能力集合。
```

**定向命令与通过标准**

保留现有用例身份；需要新增自测时在实际 runner/测试入口注册。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-041
# BD 与 BR 按 §3 配置；构建与运行逐次执行。
cmake --build "$BD" --target driver --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case /single/tests/compiler/constant_expr \
  --timeout 180 --evidence-dir "$E/$N-debug"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --tool "$PY" --timeout 180 --evidence-dir "$E/$N-scope-selftest" \
  -- -B tools/testsuite/test-run-targeted.py
cmake --build "$BR" --target driver --parallel 1
# 以下单独运行：预期非零且诊断须明确为缺少 debug 能力。
# 由新增自测断言该精确拒绝原因；任意其他失败都不能算通过。
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BR/bin/driver" --case /single/tests/compiler/constant_expr \
  --timeout 180 --evidence-dir "$E/$N-optimized-rejected"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“Debug 有实际断言；优化配置预排除；其他运行用例故意零断言仍失败。”。 本卡使用上列既有测试身份和 runner/CI 自测，不新增无关 LPC runtime 文件。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；本卡已实施：testsuite 宣布并输出 `debug` 能力，`constant_expr` 在 `test-scopes.tsv` 声明 `runtime/debug`；Debug 新构建真实执行为 `selected=1,passed=1,failed=0,skipped=0`，RelWithDebInfo 运行在启动测试后明确输出 `unavailable / Requires compiler debug introspection.` 并以非零失败，不把零断言记为通过。runner scope 自测同时覆盖 debug=1、debug=0、缺能力和零断言拒绝。证据见 `build-modernization-evidence/audit-f3317359-20261013T001301Z/AUD-041-after-debug/`、`AUD-041-optimized-rejected/`、`AUD-041-after-scope-selftest.log`；构建日志为 `AUD-041-configure-relwithdebinfo.log` 与 `AUD-041-build-driver-relwithdebinfo.log`。该卡状态为 `fixed/debug-classified`，尚未代表其余平台或 69 项整体完成。

<a id="aud-042"></a>
### AUD-042 · CI 定向失败详细日志未持久归档

- **优先级 / 责任视角：**P2；测试工具 / CI / 证据。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**嵌套 runner 只打印 /tmp 证据路径，CI 未归档该目录完整失败日志；file_io 的具体断言无法从现有 CI 恢复。 修复后持久证据根 + always 上传 + 保留退出码。
- **定位 / 改动文件：**.github/workflows/ci.yml；tools/testsuite/run-targeted.py:1000–1039；src/tests/CMakeLists.txt；`.github/workflows/ci.yml`、`src/tests/CMakeLists.txt`、`tools/testsuite/run-targeted.py`、`tools/testsuite/test-run-targeted.py`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**根因核验被迫另行同型复跑；日志缺失不代表产品有错。
- **预计工时：**8–16 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**不能通过放宽路径边界、降低证据身份或静默跳过解决门禁失败；负向自测必须继续拒绝路径逃逸、零断言、字节被替换和异步未完成。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“只剩临时路径，原日志未上传”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 每次 runner/嵌套 CTest 显式使用工作区下唯一 evidence-root；runner 在父目录下创建不可覆盖的唯一子目录。
3. always 上传受控 summary、有界完整日志和身份元数据；不上传凭据、环境或任意 /tmp。
4. 保留失败码，用受控失败确认 artifact 可定位具体断言。
5. 已知失败可从 artifact 恢复断言与身份，job 仍失败；成功/中断可追踪且敏感文件不归档。 正常、精确边界、失败与异常/清理必须分别有断言。同时保留 runner 的 selected/executed、断言与异步完成门禁。
6. 同步 `tools/testsuite/README.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：只剩临时路径，原日志未上传。
- 后：持久证据根 + always 上传 + 保留退出码。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```yaml
# 证据上传步骤沿用仓库已固定的 action SHA。
if: always()
# path 指向 runner 显式配置的本任务证据根，不匹配整个 /tmp。
```

**定向命令与通过标准**

保留现有用例身份；需要新增自测时在实际 runner/测试入口注册。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-042
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --tool "$PY" --timeout 180 --evidence-dir "$E/$N-runner-selftest" \
  -- -B tools/testsuite/test-run-targeted.py
# CTest 的 file_io_tests 使用 --evidence-root build/test-evidence/file_io；
# 随后对 CI 文件执行 tools/docs/check-workflows.py --self-test 与固定版 actionlint。
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“已知失败可从 artifact 恢复断言与身份，job 仍失败；成功/中断可追踪且敏感文件不归档。”。 本卡使用上列既有测试身份和 runner/CI 自测，不新增无关 LPC runtime 文件。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；本卡已实施：`run-targeted.py` 增加互斥的 `--evidence-root`，在持久父目录下以进程号和时间戳创建唯一子目录，失败 summary、日志、records 和输入身份均留在工作区；`src/tests/CMakeLists.txt` 的 `file_io_tests` 使用该入口；CI 在 Ubuntu 单元测试后以 `always()` 上传 `build/test-evidence`，不上传任意 `/tmp`。runner 自测通过，Debug `file_io_tests` 真实 runner 结果为 `selected=96,passed=96,failed=0,skipped=0`，CTest 精确选择 `1/1 Passed`；同一次 CTest 可见两个唯一证据子目录，未覆盖旧证据。CI 工作流自测通过；本机没有 `actionlint`，未声称该项通过。证据见 `build-modernization-evidence/audit-f3317359-20261013T001301Z/AUD-042-runner-selftest.log`、`AUD-042-configure-dev-debug.log`、`AUD-042-build-file-io-tests.log`、`AUD-042-file-io-run.log`、`AUD-042-ctest-file-io.log`、`AUD-042-workflow-selftest.log`、`AUD-042-actionlint.log`，持久 runner 证据在 `build-dev-debug/test-evidence/file_io/`。原 CI 临时日志不可恢复；此前 GCC RelWithDebInfo 同型复跑的 2 项失败仍按 AUD-044 测试注入缺口归类。该卡状态为 `fixed/persistent-evidence`，尚未代表 CI 远端重新运行或 69 项整体完成。

<a id="aud-043"></a>
### AUD-043 · 空输入尾裁剪形成起点之前的指针

- **优先级 / 责任视角：**P2；LPC 编译器 / C++。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**handle_pragma 的 strlen-1 与 user_parser 初始/末轮指针可越过起点。后者 p>=buff 会阻止解引用，不能报成已证实前读。 修复后length>0 才访问 start[length-1]。
- **定位 / 改动文件：**src/compiler/internal/lex.cc:handle_pragma；src/packages/core/add_action.cc:325–333；`src/compiler/internal/lex.cc`、`src/packages/core/add_action.cc`、`src/tests/test_lpc_compiler.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**C++ 指针形成/比较超界 UB；未证明当前构建会崩溃。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。

**实施步骤**

1. 已冻结本卡源码/编译配置与输入身份，核对两个调用者并保留“start + strlen(start) - 1”风险证据；未把未注册、空 filter 或 skip 当作复现。
2. 两处改用非负长度索引，仅 `length>0` 才访问 `length-1`。
3. 保留 pragma 诊断和空命令返回 0，不扩大空白定义。
4. 原生用例覆盖空 pragma；LPC 入口覆盖空串、全 ASCII 空格、普通命令和命令 action，且使用既有 command-giver 恢复合同。
5. 原生 targeted runner 选中并通过 1/1，LPC targeted runner 选中并通过 1/1；正常结果/诊断不变，未把 sanitizer 不可观察性伪造为通过。内存路径仍按 §4 在后续 sanitizer 波次复核。
6. 已同步 `docs/efun/interactive/command.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：start + strlen(start) - 1。
- 后：length>0 才访问 start[length-1]。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
// handle_pragma：保留原 iswspace 谓词，只修正长度边界。
size_t length = strlen(str);
while (length > 0 && iswspace(str[length - 1])) {
  str[--length] = '\0';
}

// user_parser：复用原 length；仍只删除 ASCII 空格，不改成所有空白。
length = strlen(buff);
while (length > 0 && buff[length - 1] == ' ') {
  buff[--length] = '\0';
}
if (length == 0) {
  return 0;
}
// 保留后续 p = strchr(buff, ' ')；删去旧的 p - buff + 1 赋值。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-043
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestCompilerEmptyPragmaDoesNotReadBeforeBuffer' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“正常结果/诊断不变；边界夹具确认无非法指针；不伪造 sanitizer 可观察性。”。本次 `AUD-043-native-debug` 的原生 runner 为 selected=1/passed=1，`AUD-043-lpc-debug-v2` 的 LPC runner 为 selected=1/passed=1；LPC 入口为 `testsuite/single/tests/efuns/audit_empty_tail_trim.lpc`，已登记为 runtime。涉及语言行为的新增入口使用 §3.4 隔离执行，不运行未注册对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：两个生产路径、原生回归、LPC 回归、归属文档和 targeted runner 证据均已取得；本卡已实施，待精确提交。

<a id="aud-044"></a>
### AUD-044 · FORTIFY 绕过 fprintf 故障注入

- **优先级 / 责任视角：**P2；文件 IO / 错误传播。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**GCC 优化/FORTIFY 把两处 fprintf 降成 __fprintf_chk，测试仅 wrap fprintf，故障钩子未执行。修复后同一注入状态覆盖 fprintf 与 __fprintf_chk。
- **定位 / 改动文件：**src/tests/test_file_io.cc:308、602–642；src/symbol.cc；src/packages/mudlib_stats/mudlib_stats.cc:532；src/tests/CMakeLists.txt；`src/tests/test_file_io.cc`、`src/tests/CMakeLists.txt`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**两个写失败回归失效并导致 CI 失败；不是已证实的产品输出丢失。
- **预计工时：**4–8 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**不要伪造成功或丢掉首个系统错误；只操作夹具目录，保留权限检查及既有成功/失败返回值，覆盖 EOF、EIO、消失路径和跨设备失败。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“仅 --wrap=fprintf / __wrap_fprintf”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 保留 FORTIFY，在同一故障状态机接入 `__fprintf_chk` 包装；通过 `__vfprintf_chk` 正常转发并保留 `flag`/varargs。
3. Linux 测试目标启用 `--wrap=__fprintf_chk`；包装函数仅在 glibc 下编译，非 glibc 平台不引入该符号依赖。
4. 继续要求 hits>=1，验证正常写和两个 printf 变体的故障上报。
5. 同型优化配置 96/96 且钩子命中；Debug 不回归，正常 fortified 输出不被屏蔽。 正常、精确边界、失败与异常/清理必须分别有断言。同时保留 runner 的 selected/executed、断言与异步完成门禁。
6. 已同步 `tools/testsuite/README.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：仅 --wrap=fprintf / __wrap_fprintf。
- 后：同一注入状态覆盖 fprintf 与 __fprintf_chk，正常路径经 __vfprintf_chk 保留 FORTIFY flag。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
extern "C" int __wrap___fprintf_chk(FILE*, int, const char*, ...);
// Linux/glibc 的 file_io_tests 链接选项增加：-Wl,--wrap=__fprintf_chk
// 正常转发使用 __vfprintf_chk，保留原 flag；故障路径复用现有注入逻辑。
```

**定向命令与通过标准**

保留现有用例身份；需要新增自测时在实际 runner/测试入口注册。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-044
for PROFILE in debug optimized; do
  if test "$PROFILE" = debug; then B="$BD"; else B="$BR"; fi
  cmake --build "$B" --target file_io_tests --parallel 1
  HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
    --binary "$B/src/tests/file_io_tests" \
    --gtest-filter 'FileIoTest.SymbolWriteFailureIsReported:FileIoTest.StatWriteFailureIsReported' \
    --timeout 180 --evidence-dir "$E/$N-$PROFILE-faults"
done
# 两种构建都必须实际命中故障注入钩子，使用各自的原始日志。
```

通过标准是 GCC 优化与 Debug 均为 2/2、故障钩子命中、正确错误文本且无静默写成功；不能把 94/96 记为通过。本次先保留 `audit-deep-55c7360b-20261012T072825Z/gtest-audit44-gcc-relwithdebinfo-file_io_tests/` 的修前 94/96 失败证据，再以新目录 `build-audit-gcc-relwithdebinfo-55c7360b-audit44-r3` 完成优化 CTest 96/96，并以 `build-dev-debug` 完成 Debug CTest 96/96；两次均包含目标写失败用例，优化二进制含 `__wrap___fprintf_chk`。本卡使用上列既有测试身份和 runner/CI 自测，不新增无关 LPC runtime 文件。

**失败与回滚**：遵守 R1。保留原始失败（包括先前 r2 优化构建因默认 Python 缺少 pidfd 能力而拒绝，以及 r3 首次修复前 94/96 失败）、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：源码、CMake 链接包装、归属文档和 Debug/优化 96/96 证据均已取得；本卡已实施，待精确提交。

<a id="aud-045"></a>
### AUD-045 · member_array 下标窄化及逆向失败哨兵错误

- **优先级 / 责任视角：**P2；efun / VM / 生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**起点先从 LPC_INT 窄化为 int 再校验；逆向搜索把未找到的 -1 再映射成 size。 修复后先验证宽类型起点；仅命中时映射。
- **定位 / 改动文件：**src/packages/core/efuns_main.cc:1092–1108、1243–1253；`src/packages/core/efuns_main.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**返回错误位置，甚至把未找到报告成非负下标。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“i=(int)start；i=-1 后仍 size-i-1”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 在 LPC_INT 中按容器规则验证起点，再作有证明的窄化；保留字符串越界报错与数组未找到语义。
3. 只有命中时才进行反向下标映射，-1 始终保留。
4. 覆盖普通、前缀及谓词组合和极值起点。
5. 正反向未找到均 -1；实际命中下标正确；大起点不回绕成有效下标；负起点仍拒绝。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/arrays/member_array.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：i=(int)start；i=-1 后仍 size-i-1。
- 后：先验证宽类型起点；仅命中时映射。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if ((flags & 4) && i >= 0) {
  i = v->size - i - 1;
}
// requested_start 先以 LPC_INT 验证容器范围，再转换为内部索引。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-045
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditMemberArrayIndices*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“正反向未找到均 -1；实际命中下标正确；大起点不回绕成有效下标；负起点仍拒绝。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_member_array_indices.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-046"></a>
### AUD-046 · 原生骰子与编辑距离循环不响应预算

- **优先级 / 责任视角：**P1；efun / VM / 生命周期。
- **依赖：**AUD-006 先完成对应公共入口或测试合同。
- **目标：**roll_MdN 的 O(rolls) 与 string_difference 的 O(mn) 循环不轮询 outoftime，解释器只在回到 opcode 分派后处理超时。 修复后每批共享超时检查 + 异常安全工作表。
- **定位 / 改动文件：**src/packages/contrib/contrib.cc:2835–2853、2874–2910；src/vm/internal/base/interpret.cc:2478–2484；`src/packages/contrib/contrib.cc`、`src/vm/internal/base/interpret.cc`、`src/vm/internal/base/interpret.h`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**事件循环被长时间占用；不把 WSL 结果推广为各平台计时器验收。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“原生循环无预算轮询”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 把解释器现有超时处理提为同一内部检查函数，保留 max_eval_error/catch 不能吞掉超时的合同。
3. 两循环以固定小批量轮询，预处理/初始化长循环也检查；不修改结果算法。
4. 编辑距离工作表改为异常安全所有权，使预算 error 释放临时内存；接入 AUD-006 的平台计时合同。
5. 正常结果一致；注入耗尽可预测中止且释放内存；真实隔离 driver 的有界输入验证响应时间，无负载外推。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/contrib/roll_MdN.md`、`docs/efun/contrib/string_difference.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：原生循环无预算轮询。
- 后：每批共享超时检查 + 异常安全工作表。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if (outoftime) {
  max_eval_error = 1;
  error("Too long evaluation. Execution aborted.\n");
}
// 这是共享 poll 的核心；保留解释器现有计时重设/诊断语义，再在两个循环分批调用。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-046
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditNativeLoopBudget*' \
  --timeout 15 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“正常结果一致；注入耗尽可预测中止且释放内存；真实隔离 driver 的有界输入验证响应时间，无负载外推。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_native_loop_budget.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-047"></a>
### AUD-047 · get_dir 两遍扫描遇提前 EOF 解引用空目录项

- **优先级 / 责任视角：**P1；文件 IO / 错误传播。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**第一遍计数后 rewinddir；第二遍只判断 i<count，不判断 readdir 是否为空。目录变化或读取错误可使其提前结束。 修复后先验 de/errno，按实际 i 完成结果。
- **定位 / 改动文件：**src/packages/core/file.cc:311–328、345–384；`src/packages/core/file.cc`、`src/tests/test_file_io.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**空指针解引用；第一遍也未区分 EOF 与错误。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**不要伪造成功或丢掉首个系统错误；只操作夹具目录，保留权限检查及既有成功/失败返回值，覆盖 EOF、EIO、消失路径和跨设备失败。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“for(...; i<count; de=readdir(...)) 直接用 de”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 每次 readdir 前清 errno，返回空后先区分正常 EOF 与错误，任何 d_name 访问前检查目录项。
3. 正常提前 EOF 按实际填充 i 收缩数组并仅排序有效项；错误释放数组/关闭目录并返回既有失败值。
4. 保留最大数组限制、通配符与排序；不用第一遍计数证明第二遍一定有项目。
5. 确定性注入提前 EOF/错误及匹配变化；无空槽/空解引用/泄漏，正常排序与限制不变。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/filesystem/get_dir.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：for(...; i<count; de=readdir(...)) 直接用 de。
- 后：先验 de/errno，按实际 i 完成结果。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```diff
- for (i = 0, de = readdir(dirp); i < count; de = readdir(dirp)) {
+ for (i = 0, de = readdir(dirp); i < count && de; de = readdir(dirp)) {
# 此结构修改仍须配套逐次 errno 清零、区分 EOF/EIO、按实际 i 缩短数组。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-047
B="$BD"
cmake --build "$B" --target file_io_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/file_io_tests" --gtest-filter 'FileIoTest.AuditGetDirEnumeration*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“确定性注入提前 EOF/错误及匹配变化；无空槽/空解引用/泄漏，正常排序与限制不变。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_get_dir_enumeration.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-048"></a>
### AUD-048 · get_dir 忽略元数据读取失败

- **优先级 / 责任视角：**P2；文件 IO / 错误传播。
- **依赖：**AUD-047 先完成对应公共入口或测试合同。
- **目标：**逐项 large_file_stat 返回值被忽略，失败后 encode_stat 使用之前目录/文件的 st；过长路径还伪造全零元数据。 修复后检查失败 → 清理并返回失败。
- **定位 / 改动文件：**src/packages/core/file.cc:354–375；`src/packages/core/file.cc`、`src/tests/test_file_io.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**输出看似有效但属于其他对象或虚构的大小/时间。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**不要伪造成功或丢掉首个系统错误；只操作夹具目录，保留权限检查及既有成功/失败返回值，覆盖 EOF、EIO、消失路径和跨设备失败。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“large_file_stat(...); // assume it works”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. metadata 模式检查 stat 成功，不复用旧 st；路径不容纳也明确失败。
3. 失败释放中间结果、关闭目录，按 get_dir 已有失败约定返回 0，不造元数据哨兵。
4. 保持成功路径的 file_size/时间定义，文档说明元数据失败不返回部分伪结果。
5. 第二项 stat 失败不会得到第一项元数据；不泄漏、不返回伪零；正常/目录/长路径边界覆盖。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/filesystem/get_dir.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：large_file_stat(...); // assume it works。
- 后：检查失败 → 清理并返回失败。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if (stat(path, &st) != 0) {
  // 释放 DIR、字符串和已构造数组，沿 get_dir 既有失败通路返回。
  return nullptr;
}
// path/st 对应既有局部变量；清理不可省略，也不能返回伪造的 size/mtime。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-048
B="$BD"
cmake --build "$B" --target file_io_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/file_io_tests" --gtest-filter 'FileIoTest.AuditGetDirMetadata*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“第二项 stat 失败不会得到第一项元数据；不泄漏、不返回伪零；正常/目录/长路径边界覆盖。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_get_dir_metadata.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-049"></a>
### AUD-049 · link 的 EXDEV 符号链接目标与返回值错误

- **优先级 / 责任视角：**P2；文件 IO / 错误传播。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**回退 symlink(from,to) 直接使用相对当前目录的 from，实际会相对目标目录解释；symlink 失败仍落到 return 0。 修复后目标目录相对路径；检查 symlink 结果。
- **定位 / 改动文件：**src/packages/core/file.cc:895–905、938–946；`src/packages/core/file.cc`、`src/tests/test_file_io.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**返回成功但链接损坏，或根本没有创建。不是复制回退。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**不要伪造成功或丢掉首个系统错误；只操作夹具目录，保留权限检查及既有成功/失败返回值，覆盖 EOF、EIO、消失路径和跨设备失败。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“symlink(from,to)；失败仍 return 0”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 仅 EXDEV 分支按目标父目录计算词法相对源目标，不改变权限回调原始输入和次数，不 canonicalize 跟随源链接。
3. symlink 失败走既有错误/失败合同，不能落到成功返回。
4. 保留正常硬链接与目标存在处理；使用私有夹具注入 EXDEV。
5. F_LINK 的 symlink 失败分支返回 1，不能落到函数末尾的成功值 0；F_RENAME 分支不改。
6. 回退目标实际可读且与源一致；失败不成功；权限拒绝无副作用，既有目标不被覆盖。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
7. 同步 `docs/efun/filesystem/link.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：symlink(from,to)；失败仍 return 0。
- 后：目标目录相对路径；检查 symlink 结果。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
// relative_target 从已验证的 from 计算，基准为已验证的 to 的父目录。
if (symlink(relative_target.c_str(), to) != 0) {
  return 1;
}
return 0;
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-049
B="$BD"
cmake --build "$B" --target file_io_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/file_io_tests" --gtest-filter 'FileIoTest.AuditCrossDeviceLink*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“回退目标实际可读且与源一致；失败不成功；权限拒绝无副作用，既有目标不被覆盖。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_cross_device_link.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-050"></a>
### AUD-050 · sprintf 数组 %O 转换只作用于首元素

- **优先级 / 责任视角：**P2；efun / VM / 生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**首元素把 finfo 从 INFO_T_LPC 改成 INFO_T_STRING，后续迭代不恢复原转换；后面的数值可报字符串类型错，字符串失去 %O 表示。 修复后每元素从原类型开始，明确转换临时值所有权。
- **定位 / 改动文件：**src/packages/core/sprintf.cc:1075–1103、数组迭代尾部；`src/packages/core/sprintf.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**合法格式输出错误或抛错。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“首元素永久改写 finfo 类型”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 保留原转换类型，每个元素重新执行 %O 转换，其他格式标志不丢。
3. 每轮沿用已有 clean 释放，下一轮恢复原转换类型；保持列/表其他 flags 和缓冲寿命，不引入新的临时池。
4. 比对逐元素 sprintf("%O") 拼接，并验证宽度、精度、列/表及错误后的状态。
5. 空/单/多元素、混合类型和嵌套值结果正确；ASan/引用检查无泄漏，后续调用不受影响。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/strings/sprintf.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：首元素永久改写 finfo 类型。
- 后：每元素从原类型开始，明确转换临时值所有权。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
const int conversion_type = finfo & INFO_T;
// 每次进入数组元素转换时恢复类型位：
finfo = (finfo & ~INFO_T) | conversion_type;
// 保留每轮末尾现有 clean 释放；列/表及其他标志位不重置。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-050
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditSprintfArrayObject*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“空/单/多元素、混合类型和嵌套值结果正确；ASan/引用检查无泄漏，后续调用不受影响。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_sprintf_array_object.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-051"></a>
### AUD-051 · parse_command 默认介词表可为空

- **优先级 / 责任视角：**P1；LPC parser / 缓存与生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**未传有效显式表时使用可空 gPrepos_list，随即访问 parr->size。 修复后先处理空表为未匹配，再遍历。
- **定位 / 改动文件：**src/packages/ops/parse.cc:1154–1167；`src/packages/ops/parse.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**合法的缺省/未配置场景可崩溃。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“parr=gPrepos_list; for(...parr->size)”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 介词表为空时按未匹配返回，设置 fail 和返回槽；不制造新默认词表。
3. 空数组、含非字符串项和显式数组保持现有选择及交换规则。
4. 验证 master 缺 apply/错误类型和后续可用调用。
5. 缺省缺表返回未匹配不崩溃；有效单词/多词表正常。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/parsing/parse_command.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：parr=gPrepos_list; for(...parr->size)。
- 后：先处理空表为未匹配，再遍历。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if (parr == nullptr) {
  *fail = 1;
  return nullptr;
}
// 放在读取 parr->size 或 item 之前；不临时制造默认介词。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-051
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditParsePrepositionTable*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“缺省缺表返回未匹配不崩溃；有效单词/多词表正常。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_parse_preposition_table.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-052"></a>
### AUD-052 · living_parse 压缩候选却复用原下标缓存

- **优先级 / 责任视角：**P2；LPC parser / 缓存与生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**live 按 tix 压缩排列，gId_list/gPluid_list/gAdjid_list 仍按原 obarr 下标缓存；item_parse 用压缩下标查这些缓存。 修复后live[obix]=obarr[obix]，保持唯一索引域。
- **定位 / 改动文件：**src/packages/ops/parse.cc:1050–1083；load_lpc_info/match_object；`src/packages/ops/parse.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**错误匹配、漏匹配或把另一个对象的 ID 用到 living 对象。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“live[tix++]=obarr[obix]，缓存却按 obix”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. living 临时数组用 allocate_array(obarr->size) 初始化为零，保留原数组长度。
3. 仅把 living 对象复制到原 obix 槽位，不再压紧到 tix，也不缩短数组；复用 item_parse 跳过非对象的既有分支。
4. gId_list/gPluid_list/gAdjid_list 仍以原 obix 查找；对象引用计数按既有 assign/free 合同配对，不引入第二套缓存。
5. 冷/热缓存一致，混合数组不串对象；destruct 和非对象仍安全。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/parsing/parse_command.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：live[tix++]=obarr[obix]，缓存却按 obix。
- 后：live[obix]=obarr[obix]，保持唯一索引域。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
array_t* live = allocate_array(obarr->size);
// 保留原 living 判定，仅复制到原槽位，不写 live->item[tix++]：
assign_svalue_no_free(&live->item[obix], &obarr->item[obix]);
// 非 living 槽保持 0；长度保持 obarr->size，临时数组按既有路径释放。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-052
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditLivingParseCacheIndices*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“冷/热缓存一致，混合数组不串对象；destruct 和非对象仍安全。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_living_parse_cache_indices.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-053"></a>
### AUD-053 · 多词 ID 游标在调用者处多推进一词

- **优先级 / 责任视角：**P2；LPC parser / 缓存与生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**find_string 单词命中留下当前词，多词命中却留下词后位置；single_parse/item_parse 都再加一。 修复后保持 return fpos；成功多词匹配后 --*cix_in。
- **定位 / 改动文件：**src/packages/ops/parse.cc:1119–1131、1294–1347；`src/packages/ops/parse.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**跳过后一词，导致错误匹配/失败。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“多词返回词后；调用者又 +1”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. find_string 多词匹配成功后仅把输出参数 *cix_in 退回最后消费词；返回值 fpos 仍是匹配起点。
3. split 为空或零词时走不匹配分支；调用者继续按既有约定递增词游标。
4. 核对 item_parse、adjectiv_parse、prepos_parse 对起点返回值与输出词游标的不同用法。
5. 只消费实际匹配词数；多词后下一词保留，单词行为不变。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/parsing/parse_command.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：多词返回词后；调用者又 +1。
- 后：保持 return fpos；成功多词匹配后 --*cix_in。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if ((*cix_in - fpos) == split->size) {
  --*cix_in;
  free_array(split);
  return fpos;
}
// 此前须排除零词 split；返回起点的合同不变。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-053
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditMultiwordParseCursor*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“只消费实际匹配词数；多词后下一词保留，单词行为不变。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_multiword_parse_cursor.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-054"></a>
### AUD-054 · living_parse 回退成功仍保留失败标志

- **优先级 / 责任视角：**P2；LPC parser / 缓存与生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**item_parse 失败置 fail=1；find_player/find_living 回退找到对象后返回值和游标成功，但未清 fail。 修复后对象/引用/游标准备完成后同步提交 fail=0。
- **定位 / 改动文件：**src/packages/ops/parse.cc:1075–1105；`src/packages/ops/parse.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**调用者丢弃正确回退结果。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“回退 return 成功对象，但 fail 仍为 1”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 仅在回退实际找到并持有对象后设置 fail=0，再提交返回值及游标。
3. 保留两种查找顺序和对象引用，不在未找到时清失败。
4. 覆盖候选为空/非空、成功/失败以及相邻格式项。
5. 回退成功能继续匹配，失败仍失败；引用和游标无泄漏/多推进。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/parsing/parse_command.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：回退 return 成功对象，但 fail 仍为 1。
- 后：对象/引用/游标准备完成后同步提交 fail=0。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
// find_living/find_player 的成功 fallback，在返回已构造结果前：
*fail = 0;
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-054
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditLivingFallbackStatus*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“回退成功能继续匹配，失败仍失败；引用和游标无泄漏/多推进。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_living_fallback_status.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-055"></a>
### AUD-055 · SQLite 执行失败没有更新错误文本

- **优先级 / 责任视角：**P2；数据库 / 错误合同。
- **依赖：**AUD-038 先完成对应公共入口或测试合同。
- **目标：**prepare 失败会写 errormsg，sqlite3_get_table 执行失败却只清结果并返回 0，错误缓存为空或仍是上次错误。 修复后捕获本次错误 → 统一清理 → 返回失败。
- **定位 / 改动文件：**src/packages/db/db.cc:1028–1053；`src/packages/db/db.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**调用方收到错误/陈旧诊断，妨碍处理和定位。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**错误文本与句柄资源必须对应同一次操作；保留现有错误类型，不能把其他连接的诊断或旧 statement 当作当前结果。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“get_table 失败只 finalize/return”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 执行失败时在清理前复制 sqlite3_errmsg 到同一有界错误缓存并终止。
3. 清理 results 与状态一次，成功/下一次失败不能复用无关旧错误。
4. 不改变同步/异步结果协议或重复执行策略。
5. 实际约束失败有相应非空诊断，不带上次错误；句柄随后可正常执行，资源回基线。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/db/db_exec.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：get_table 失败只 finalize/return。
- 后：捕获本次错误 → 统一清理 → 返回失败。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
snprintf(c->SQLite3.errormsg, sizeof(c->SQLite3.errormsg), "%s",
         sqlite3_errmsg(c->SQLite3.handle));
// 在 sqlite3_finalize 前复制；随后释放结果并沿既有错误返回通路退出。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-055
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditSqliteExecuteError*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“实际约束失败有相应非空诊断，不带上次错误；句柄随后可正常执行，资源回基线。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_sqlite_execute_error.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-056"></a>
### AUD-056 · query_multiple_short 缺回退对象时漏掉结果缓冲

- **优先级 / 责任视角：**P2；dwlib / 值语义。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**res 已分配；no_dollars 下既无 command_giver 又无 /global/player 的分支 pop 参数并 push_undefined，却未交出或释放 res。 修复后res 始终有所有者，只有入 VM 时转移。
- **定位 / 改动文件：**src/packages/dwlib/dwlib.cc:184–229；`src/packages/dwlib/dwlib.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**重复调用泄漏原生字符串；不推断其他回调路径已经安全。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**格式化及替换不能更改调用者共享值或泄漏部分结果；保留输出语义，异常时清理，测试重复调用与别名输入。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“缺回退分支丢失 res”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 让 res 在创建后由同一 RAII/作用域守卫持有，只有 push_malloced_string 真正移交时释放守卫。
3. 缺回退对象与异常路径都释放；保留返回 undefined 和转换回调顺序。
4. 覆盖有/无 command_giver、回退存在/缺失及回调错误。
5. 缺回退重复调用引用/分配回基线；成功输出不变，无双重释放。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/contrib/query_multiple_short.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：缺回退分支丢失 res。
- 后：res 始终有所有者，只有入 VM 时转移。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
// 仅在 res 尚未交给 VM 栈、且 multiple_short 不存在的返回分支：
free_array(res);
// 其余已转交所有权的路径不重复释放。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-056
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditMultipleShortOwnership*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“缺回退重复调用引用/分配回基线；成功输出不变，无双重释放。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_multiple_short_ownership.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-057"></a>
### AUD-057 · parse_refresh 清标志导致旧缓存不释放

- **优先级 / 责任视角：**P2；LPC parser / 缓存与生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**refresh 清 PI_SETUP、设 PI_REFRESH；remove_ids 却仅在 PI_SETUP 时释放。再次 refresh 还会抹掉 PI_REFRESH，旧数组随后被覆盖。 修复后缓存所有权覆盖 SETUP/REFRESH，待刷新状态不丢。
- **定位 / 改动文件：**src/packages/parser/parser.cc:563–617、918–942；`src/packages/parser/parser.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**ID/复数/形容词数组和字符串引用泄漏。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“清 SETUP 后，释放函数又要求 SETUP”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. remove_ids 按 PI_SETUP|PI_REFRESH 识别仍拥有的缓存，释放后归零；不读从未初始化的字段。
3. 重复 refresh 保留 PI_REFRESH，只有实际重建/清理完成才结束待刷新状态。
4. 维持正在解析时延迟释放的约束，核对 remove_parser_info 和 interrogate_object。
5. 单次/多次刷新、错误及销毁后引用回基线；活动解析不悬垂，新 ID 下一次生效。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/parsing/parse_refresh.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：清 SETUP 后，释放函数又要求 SETUP。
- 后：缓存所有权覆盖 SETUP/REFRESH，待刷新状态不丢。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
// remove_ids 的拥有判定：
if (pinfo->flags & (PI_SETUP | PI_REFRESH)) {
  // 保留原有 ids/plurals/adjectives 释放与字段清空。
}
// 重复 refresh 必须保留 PI_REFRESH，直到缓存真正被释放。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-057
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditParseRefreshOwnership*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“单次/多次刷新、错误及销毁后引用回基线；活动解析不悬垂，新 ID 下一次生效。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_parse_refresh_ownership.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-058"></a>
### AUD-058 · roll_MdN 把 64 位参数窄化成 int

- **优先级 / 责任视角：**P2；efun / VM / 生命周期。
- **依赖：**AUD-046 先完成对应公共入口或测试合同。
- **目标：**rolls/sides/bonus 使用 int，LPC_INT 实参在验证和运算前已截断；roll 本身已是 LPC_INT，不能误报累加器是 int。 修复后全程 LPC_INT + 同一整数合同和预算。
- **定位 / 改动文件：**src/packages/contrib/contrib.cc:2835–2853；`src/packages/contrib/contrib.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**合法 64 位值变成负数/其他数，结果偏离合同。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“int rolls,sides,bonus = LPC_INT 实参”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 三种参数保持 LPC_INT，边界/正数判断在原宽度进行，检查 random_number 的参数宽度。
3. 累加沿用项目 LPC 64 位整数溢出合同，避免引入 C++ UB。
4. 与 AUD-046 共用有界循环，不能把窄化修复变成不受限长循环。
5. 小值不回归；大 bonus、受控 RNG 的大 sides 保持值；极大 rolls 被预算正常终止。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/contrib/roll_MdN.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：int rolls,sides,bonus = LPC_INT 实参。
- 后：全程 LPC_INT + 同一整数合同和预算。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
LPC_INT rolls;
LPC_INT sides;
LPC_INT bonus;
LPC_INT result;
// 参数检查在 LPC_INT 域内完成；累加使用项目既有 64 位溢出合同。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-058
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditRollMdnWidth*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“小值不回归；大 bonus、受控 RNG 的大 sides 保持值；极大 rolls 被预算正常终止。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_roll_mdn_width.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-059"></a>
### AUD-059 · socket_status 混用状态枚举和 flags 位图

- **优先级 / 责任视角：**P2；网络 / TLS / 生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**用 flags & STATE_FLUSHING 判断刷新状态，实际应读取 state；无关位值可错误隐藏/暴露 owner 槽。 修复后state == STATE_FLUSHING。
- **定位 / 改动文件：**src/packages/sockets/socket_efuns.cc:2394–2402；`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**状态 API 返回错误 owner；不据此推断权限提升。
- **预计工时：**2–4 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“flags & STATE_FLUSHING”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 改为 state != STATE_FLUSHING；保留 owner 存活检查和引用计数。
3. 用枚举状态和独立 flags 组合验证，不靠当前数字巧合。
4. 不改变返回数组形状和其他字段。
5. 刷新时 owner 槽为 0，合法活动 socket 为相应对象，独立 flags 不改变结果。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/sockets/socket_status.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：flags & STATE_FLUSHING。
- 后：state == STATE_FLUSHING。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if (lpc_socks[fd].state == STATE_FLUSHING) {
  // 保留原有 FLUSHING 输出内容。
}
// 不再用 flags & STATE_FLUSHING。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-059
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditSocketStatusState*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“刷新时 owner 槽为 0，合法活动 socket 为相应对象，独立 flags 不改变结果。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_socket_status_state.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-060"></a>
### AUD-060 · SC_FINAL_CLOSE 仍可进入延迟刷新

- **优先级 / 责任视角：**P1；网络 / TLS / 生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**FINAL 只跳过已有 FLUSHING 的前置拒绝；后续 S_BLOCKED 分支不检查 FINAL，强制清理可再次返回 FLUSHING。 修复后FINAL → 唯一最终清理；非 FINAL 才可延迟。
- **定位 / 改动文件：**src/packages/sockets/socket_efuns.cc:2038–2074、2449–2467；`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**描述符、事件、队列可能未最终回收，TLS 生命周期与 AUD-035 交叉。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**清理顺序改变可能产生重复释放或 stale callback；保留 TLS 校验与 session/owner 归属，推进本夹具 event_base 后核对 fd、事件和 SSL 资源。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“FINAL 之后仍按 S_BLOCKED 进入 FLUSHING”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. FINAL 必须绕过排空，回收全部资源；普通关闭才可进入 FLUSHING。
3. 核对所有 FINAL 调用者、回调重入与槽 generation；不能只在调用者清 S_BLOCKED。
4. 与 AUD-035 合并验证正常排空和强制丢弃两种不同合同。
5. FINAL 返回后推进夹具事件即可资源归零，不续写/重复回调；普通关闭仍按合同排空。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/sockets/socket_close.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：FINAL 之后仍按 S_BLOCKED 进入 FLUSHING。
- 后：FINAL → 唯一最终清理；非 FINAL 才可延迟。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
// 普通重复关闭保持幂等；SC_FINAL_CLOSE 必须越过 FLUSHING 早退。
if (lpc_socks[fd].state == STATE_FLUSHING && !(flags & SC_FINAL_CLOSE)) {
  return EESUCCESS;
}
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-060
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditSocketFinalClose*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“FINAL 返回后推进夹具事件即可资源归零，不续写/重复回调；普通关闭仍按合同排空。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_socket_final_close.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-061"></a>
### AUD-061 · parser 递归预算跨请求累计

- **优先级 / 责任视角：**P1；LPC parser / 缓存与生命周期。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**parse_recurse_steps 是函数 static，初始化一次且无重置。正常请求累计到 1000000 后，后续解析立即返回。 修复后request-local budget，递归共享，本次结束即结束。
- **定位 / 改动文件：**src/packages/parser/parser.cc:3171–3192、3300–3361；`src/packages/parser/parser.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**正常解析逐渐失效，预算不是本次请求的资源边界。
- **预计工时：**8–16 小时。
- **来源处理策略：**保留 fork 的有效功能，在当前实现修根因；不整体回退 owner、异步或测试门禁。
- **风险与缓解：**缓存/索引修复可能改变匹配优先级或引用生命周期；保留合法输入匹配语义，失败分支恢复状态，测试共享引用、刷新及析构。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“static int parse_recurse_steps=0”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 计数改为最外层本次解析入口的局部值，传到 parse_recurse 及所有递归调用。
3. 同一请求所有规则/起点共享原阈值；不要每层/每规则重置；超限沿正常清理链退出。
4. 验证正常重复请求、单次耗尽、错误后新请求及所有解析入口。
5. 总历史工作量超过阈值不影响下一请求；单请求仍受原阈值限制，临时词/引用清理正确。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/parsing/parse_sentence.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：static int parse_recurse_steps=0。
- 后：request-local budget，递归共享，本次结束即结束。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
int parse_recurse_steps = 0;
// 这行位于最外层解析请求入口，不是 parse_recurse 的每层调用。
// 所有递归分支及规则尝试按引用共享该计数；下一请求重新从 0 开始。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-061
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditParserRequestBudget*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“总历史工作量超过阈值不影响下一请求；单请求仍受原阈值限制，临时词/引用清理正确。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_parser_request_budget.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-062"></a>
### AUD-062 · PCRE2 合法前瞻捕获使替换长度下溢

- **优先级 / 责任视角：**P0；正则 / 内存与字符串。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**捕获只验证在 subject 内；最终尾段却用 group 0 的 match_end 减去捕获的 last_end。前瞻捕获可超过整体匹配终点，差值下溢后进入 memcpy。 修复后tail=subject_length-validated_cursor。
- **定位 / 改动文件：**src/packages/pcre/pcre.cc:1235–1340；调用者 f_pcre_replace/f_pcre_replace_callback；`src/packages/pcre/pcre.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**巨大复制长度和内存破坏；未运行可执行攻击样本。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**前瞻/后顾与零宽捕获本身是合法语法；不能一概拒绝以掩盖越界。按真实 subject 坐标重建输出，保留既有外层捕获优先的重叠处理规则。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“tail=match_end-last_capture_end”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 用 subject 范围和唯一源游标拼装非重叠捕获替换：游标从 0 开始、最终尾段为 subject_length-cursor。
3. 保留外层捕获优先的重叠规则；支持合法前瞻/后瞻，不通过拒绝合法模式绕过根因。
4. 复核总输出长度与实际复制一致；两个 replace 入口共享同一实现。
5. 前瞻/后瞻、零宽、嵌套/可选捕获、空替换及上限，输出正确且 ASan 无越界。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/pcre/pcre_replace.md`、`docs/efun/pcre/pcre_replace_callback.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：tail=match_end-last_capture_end。
- 后：tail=subject_length-validated_cursor。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
size_t ret_pos = 0;
size_t subject_pos = 0;
for (auto const& segment : segments) {
  size_t prefix = static_cast<size_t>(segment.start) - subject_pos;
  memcpy(ret + ret_pos, run->subject + subject_pos, prefix);
  ret_pos += prefix;
  memcpy(ret + ret_pos, segment.replacement, segment.replacement_length);
  ret_pos += segment.replacement_length;
  subject_pos = static_cast<size_t>(segment.end);
}
size_t tail = static_cast<size_t>(run->s_length) - subject_pos;
memcpy(ret + ret_pos, run->subject + subject_pos, tail);
ret_pos += tail;
ret[ret_pos] = '\0';
// 替换原 group-0 prefix/tail/suffix 三段拼接，不与旧拼接同时保留。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-062
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditPcreCaptureReplacement*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“前瞻/后瞻、零宽、嵌套/可选捕获、空替换及上限，输出正确且 ASan 无越界。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_pcre_capture_replacement.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-063"></a>
### AUD-063 · pcre_assoc 空匹配推进越尾且余量失真

- **优先级 / 责任视角：**P1；正则 / 内存与字符串。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**空匹配直接 tmp++，未验证终点且未同步 used；下一轮可解引用尾后，并向 PCRE2 声明过长剩余 subject。也可能落到 UTF-8 续字节。 修复后offset 与 length 同源；终点不推进，UTF 按完整字符。
- **定位 / 改动文件：**src/packages/pcre/pcre.cc:1015–1069；`src/packages/pcre/pcre.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**越界读、错误匹配/UTF 错误；不是已具边界辅助函数的 match_all 路径。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**零宽匹配推进不能越过终点或拆开 UTF-8 码点；同步实际源游标与剩余字节数，保留既有 token/default 结果，不误改已单独处理空匹配的 match_all 路径。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“tmp++，used 不变，下一轮 while(*tmp)”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 用唯一字节 offset 推导剩余长度，每次执行匹配使用真实 suffix 长度。
3. 空匹配在终点停止，否则复用 UTF/非 UTF 模式相应的有界推进，保持字节拼接不遗漏。
4. 保留模式优先级、tokens/default、嵌套返回数组和正常 suffix 锚点语义。
5. 空串/空模式、开头/中间/末尾空匹配、UTF-8 和非 UTF 模式均有界；拼接还原输入，无重复丢字。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/pcre/pcre_assoc.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：tmp++，used 不变，下一轮 while(*tmp)。
- 后：offset 与 length 同源；终点不推进，UTF 按完整字符。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
if (rmp->begin == tmp) {
  if (used == totalsize) break;
  used = pcre_advance_after_empty_match(str->u.string, totalsize, used);
  tmp = str->u.string + used;
}
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-063
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditPcreAssocEmptyMatch*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“空串/空模式、开头/中间/末尾空匹配、UTF-8 和非 UTF 模式均有界；拼接还原输入，无重复丢字。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_pcre_assoc_empty_match.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-064"></a>
### AUD-064 · 运行时浮点转整数未检查可表示范围

- **优先级 / 责任视角：**P2；VM / 编译器数值语义。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**f__to_int 直接把 LPC_FLOAT 赋给 LPC_INT；非有限或超出整数可表示范围时是 C++ UB。 sprintf 的浮点 %d 分支先 floor 后直接赋整数，同样未校验。 修复后finite + 半开区间检查 → cast。
- **定位 / 改动文件：**src/packages/core/efuns_main.cc:3132–3140；src/packages/core/sprintf.cc:1357–1366；`src/packages/core/efuns_main.cc`、`src/vm/internal/base/svalue.h`、`src/tests/test_lpc.cc`、`src/packages/core/sprintf.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**结果依编译器/平台变化；未证明内存破坏。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**越界浮点转换的失败语义必须与 VM/编译期一致；保留合法端点、截断方向和已有整数行为，不靠关闭 sanitizer 规避 UB。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“LPC_INT temp = floating_value”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 建立编译器与运行时共用的内部检查转换：有限且处于 [-2^63,2^63) 才截向零转换。
3. 上界不能用四舍五入后的 double(INT64_MAX) 作包含边界；失败走 LPC 数值范围错误。
4. to_int 保持截向零；sprintf %d 保持先 floor 再转换。仅这些 float 路径复用检查，string/buffer 合同不改。
5. 零/负小数、上下界邻点、NaN/无穷都确定；正常值与旧输出一致，越界在 cast 前拒绝。 同时覆盖 sprintf %d 的既有 floor 语义和异常边界。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/floats/to_int.md`、`docs/efun/strings/sprintf.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：LPC_INT temp = floating_value。
- 后：finite + 半开区间检查 → cast。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
inline bool lpc_float_to_int(LPC_FLOAT value, LPC_INT& result) {
  if (!std::isfinite(value) || value < -0x1p63 || value >= 0x1p63) {
    return false;
  }
  result = static_cast<LPC_INT>(value);
  return true;
}
// to_int 直接传 value；sprintf 的既有向下取整分支传 floor(value)。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-064
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditFloatToIntRuntime*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“零/负小数、上下界邻点、NaN/无穷都确定；正常值与旧输出一致，越界在 cast 前拒绝。 同时覆盖 sprintf %d 的既有 floor 语义和异常边界。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_float_to_int_runtime.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-065"></a>
### AUD-065 · 常量浮点转整数绕过运行时范围合同

- **优先级 / 责任视角：**P2；VM / 编译器数值语义。
- **依赖：**AUD-064 先完成对应公共入口或测试合同。
- **目标：**promote_to_int 在 NODE_REAL 上直接窄化并先改节点类型，越界常量在编译器内发生同类 C++ UB。 修复后共用范围转换成功后提交节点。
- **定位 / 改动文件：**src/compiler/internal/compiler.cc:1825–1832、1912、2090；`src/compiler/internal/compiler.cc`、`src/tests/test_lpc_compiler.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**同一表达式常量和运行时结果/诊断不一致。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**越界浮点转换的失败语义必须与 VM/编译期一致；保留合法端点、截断方向和已有整数行为，不靠关闭 sanitizer 规避 UB。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“先设 NODE_NUMBER，再直接窄化”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 复用 AUD-064 的同一转换规则；确认可表示后才改节点 kind/type/value。
3. 失败生成编译期范围诊断，不发布损坏 program，也不静默推迟规避验证。
4. 核对赋值、显式转换及所有 promote_to_int 调用路径。
5. 常量/运行时边界语义一致，失败后下一次编译正常；含 float-cast-overflow 的专项插桩验证。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/floats/to_int.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：先设 NODE_NUMBER，再直接窄化。
- 后：共用范围转换成功后提交节点。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
LPC_INT converted;
if (!lpc_float_to_int(node->v.real, converted)) {
  yyerror("Floating constant is outside the LPC integer range");
  return node;
}
node->kind = NODE_NUMBER;
node->v.number = converted;
// type 同步到既有整数节点合同；失败不改写原 real 节点。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-065
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditFloatToIntConstant*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“常量/运行时边界语义一致，失败后下一次编译正常；含 float-cast-overflow 的专项插桩验证。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_float_to_int_constant.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-066"></a>
### AUD-066 · replace_dollars 消费游标不一致导致负复制长度

- **优先级 / 责任视角：**P0；dwlib / 值语义。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**匹配后 currentold 前进但扫描 i 仍逐字递增，可重新匹配已消费范围得到负 len；尾检查还有跨分配指针相减。零长度产出还被误判未发生替换。 修复后同一 offset 模型 + checked length + 明确匹配状态。
- **定位 / 改动文件：**src/packages/dwlib/dwlib.cc:819–882；`src/packages/dwlib/dwlib.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**负 len 转无符号大长度进入 strncpy；空替换结果错误。现有 replacement-length 修复未覆盖这些路径。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**格式化及替换不能更改调用者共享值或泄漏部分结果；保留输出语义，异常时清理，测试重复调用与别名输入。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“currentold 前进而 i 不跳过；跨分配指针算尾长”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 用源 offset、扫描 offset、输出长度统一所有计算；消费非空模式后跳到其末尾，禁止再扫已消费范围。
3. 空模式保证扫描至少推进一个字节但保留未消费源字符；用独立 matched 标志区分零产出与未匹配。
4. 复制前检查剩余源/目标容量，尾段只在同一字符串内计算；保持规则顺序和返回类型。
5. 重叠/内嵌标记、空 key/空替换、多规则和边界；对照结果正确，ASan 无负长度/越界。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/contrib/replace_dollars.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：currentold 前进而 i 不跳过；跨分配指针算尾长。
- 后：同一 offset 模型 + checked length + 明确匹配状态。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
size_t scan = 0;
size_t consumed = 0;
size_t written = 0;
bool matched = false;
// 每次命中后 consumed = scan + key_length；全部拷贝以长度做 checked arithmetic。
// 空 key 时 scan 仍须至少前进一字节；是否命中不能用 written != 0 判断。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-066
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditReplaceDollarsCursors*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“重叠/内嵌标记、空 key/空替换、多规则和边界；对照结果正确，ASan 无负长度/越界。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_replace_dollars_cursors.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-067"></a>
### AUD-067 · include 宏展开直接递归且无预算

- **优先级 / 责任视角：**P1；LPC 编译器 / C++。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**handle_include 在文件包含深度检查之前对宏值递归调用自身，未走普通宏展开预算；循环宏或长链可耗尽原生栈。 修复后受现有上限约束的迭代展开，再走文件包含。
- **定位 / 改动文件：**src/compiler/internal/lex.cc:1181–1199；`src/compiler/internal/lex.cc`、`src/tests/test_lpc_compiler.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**编译期栈耗尽/不可控展开；不提供可执行危险源。
- **预计工时：**8–16 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**词法/编译期修复可能改变合法源码、诊断及恢复状态；保持源码拼写与错误合同，新增有效/无效输入矩阵，涉及 grammar 时按 §3 重生成。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“return handle_include(def->exps)”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 改为迭代解析宏链，复用现有宏展开上限，不另设随意的小兼容阈值。
3. 每一步计预算，循环/超限输出编译诊断；正常定界符后才进入原文件包含路径。
4. 保留符号记录、源码行号和已验证的权限回调原始拼写。
5. 普通/间接链正确，循环/超限有界拒绝；之后正常编译成功，无原生栈增长。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `src/compiler/internal/lex.cc`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：return handle_include(def->exps)。
- 后：受现有上限约束的迭代展开，再走文件包含。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
for (int expansion = 0; expansion < EXPANDMAX; ++expansion) {
  // 在当前拼写上做一次宏查找；得到带分隔符的路径才进入既有打开流程。
  // 未定义或形成循环时编译失败，不再递归调用 handle_include。
}
// 预算耗尽必须报告编译错误；此循环替换递归，不与递归并存。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-067
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditIncludeMacroBudget*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“普通/间接链正确，循环/超限有界拒绝；之后正常编译成功，无原生栈增长。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_include_macro_budget.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-068"></a>
### AUD-068 · uncompress 注册 string 输入却无实现

- **优先级 / 责任视角：**P2；压缩 / 类型与资源。
- **依赖：**无新增前置依赖；仍遵守 §2 的验收可信度优先与单构建约束。
- **目标：**签名为 buffer uncompress(string|buffer)，运行时却只接受 T_BUFFER，所有 string 在 inflate 前直接返回 undefined。 修复后按类型获取 bytes/length，共享解压实现。
- **定位 / 改动文件：**src/packages/compress/compress.spec:10；src/packages/compress/compress.cc:338–352；`src/packages/compress/compress.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**编译可通过而已声明重载始终不工作。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**保持 string/buffer 的精确字节长度和输入存活期；保留非法流、空输入与输出上限合同，不删注册重载或取消解压上限。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“非 T_BUFFER 直接 undefined”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. string/buffer 都先取得借用字节区间与精确长度，共用现有 inflate 和输出上限。
3. 字符串用 SVALUE_STRLEN，不用 strlen；输入保持存活直到解压结束，返回仍是 buffer。
4. 保留非法压缩数据的 undefined 与输出超限错误，完善现有归属文档，不删注册重载。
5. 两种输入同字节得到同 buffer；嵌入 NUL、截断、空数据及解压上限，清理正确。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/general/uncompress.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：非 T_BUFFER 直接 undefined。
- 后：按类型获取 bytes/length，共享解压实现。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
const unsigned char* input;
size_t input_size;
if (sp->type == T_BUFFER) {
  input = sp->u.buf->item;
  input_size = sp->u.buf->size;
} else {
  input = reinterpret_cast<const unsigned char*>(sp->u.string);
  input_size = SVALUE_STRLEN(sp);
}
// 校验到 zlib 长度类型后接入同一 inflate 通路；输入 svalue 在消费完成前保活。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-068
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditUncompressStringInput*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“两种输入同字节得到同 buffer；嵌入 NUL、截断、空数据及解压上限，清理正确。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_uncompress_string_input.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

<a id="aud-069"></a>
### AUD-069 · sprintf 数组数值转换原地修改输入元素

- **优先级 / 责任视角：**P2；efun / VM / 生命周期。
- **依赖：**AUD-064 先完成对应公共入口或测试合同。
- **目标：**INFO_ARRAY 的 carg 指向调用方数组元素；%f/%d 的数字类型适配直接改 carg->type/u，从而修改共享输入数组。 修复后局部数字副本适配，输入只读。
- **定位 / 改动文件：**src/packages/core/sprintf.cc:1075–1081、1357–1369、1404–1405；`src/packages/core/sprintf.cc`、`src/tests/test_lpc.cc`。先读直接调用者；新增测试必须执行生产入口，不用复制算法替代。
- **影响：**本应格式化的调用改变后续业务可见的数组类型和值；未把此现象扩张成已证跨 owner 写入。
- **预计工时：**4–8 小时。
- **来源处理策略：**引入来源仍不确定；先核对本地 blame/log 与可用上游 hunk。没有足够证据时保留“不确定”，不凭提交标题归因；有适配且已核验的上游修复时优先按 §7 移植。
- **风险与缓解：**保留 efun 返回类型、合法输入与 owner/引用合同；不要把越界修复变成输入丢弃或隐式改值，用别名输入、精确边界和异常清理回归约束。

**实施步骤**

1. 冻结本卡源码/编译配置与输入身份。核对上述位置和当前调用者，先复现“carg 指向数组元素，适配时直接改 type/u”；记录预期失败，不把新测试未注册、空 filter 或 skip 当作复现。
2. 数值适配在当前迭代的局部 svalue副本上完成，格式化只读取原元素，禁止改写 carg 指向的输入。
3. 沿用 AUD-064 的范围检查及原 floor/浮点规则；局部数字不增加引用所有权或堆分配。
4. 验证别名数组、多元素、失败中途和后续调用；与 AUD-050 分别保留回归。
5. %@f/%@d 输出不变，输入及其别名的 tag/value 前后相同；中途范围/类型错误也不产生部分修改。 正常、精确边界、失败与异常/清理必须分别有断言。内存路径跑 ASan/UBSan；涉及共享线程或回调准入时再跑 TSan。
6. 同步 `docs/efun/strings/sprintf.md`；核对链接、示例、输入/返回值与实际语义。按 §4 精确提交本卡，不修改或启动现有 driver。

**修改前后示例**

- 前：carg 指向数组元素，适配时直接改 type/u。
- 后：局部数字副本适配，输入只读。
- 以下是拟议实现片段，不是已经存在或已验证的补丁。新增私有 helper 要在本卡列出的生产路径接入；不得只在测试包装层生效。

```cpp
svalue_t numeric_value;  // 放在本轮元素处理的作用域，不能只活在 if 内。
if ((finfo & INFO_T) == INFO_T_FLOAT && carg->type == T_NUMBER) {
  numeric_value = *carg;
  numeric_value.type = T_REAL;
  numeric_value.u.real = carg->u.number;
  carg = &numeric_value;
}
// real -> integer 对称处理并调用 AUD-064 的检查；不写原数组槽位。
```

**定向命令与通过标准**

下列 filter 是拟新增名称；先在本卡测试文件实现并确认 runner 发现数非零。 同一命令先记录修前失败，再用新证据目录记录修后结果。变量来自 §3；重复执行必须更换 E 或末级目录，不覆盖旧证据。

```bash
N=AUD-069
B="$BD"
cmake --build "$B" --target lpc_tests --parallel 1
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter 'DriverTest.TestAuditSprintfArrayInputImmutability*' \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

通过标准是本卡全部选中用例执行、断言/异步屏障完成、预期拒绝不留资源，且“%@f/%@d 输出不变，输入及其别名的 tag/value 前后相同；中途范围/类型错误也不产生部分修改。”。 涉及语言行为时新增或修正 `testsuite/single/tests/efuns/audit_sprintf_array_input_immutability.lpc` 及 scope 分类；用 §3.4 的隔离 LPC 入口运行，不运行未注册的对象来制造零测试通过。

**失败与回滚**：遵守 R1。保留原始失败、源码/二进制摘要、命令、返回码、发现/执行数量、超时和清理记录；失败或缺平台时保持 blocked，不能降低门禁。仅逆向本卡完整原子提交；已有依赖时先退依赖，运行实例/数据的回滚不在本方案授权中。

**完成条件**：以上检查、归属文档和精确提交均有证据；目前本卡未实施。

## 6. 批量回归、静态分析与门禁

### 6.1 合并边界

- 001/009/010 共享一个 ASCII 生命期/边界单元；004、015、003 仍按依赖分开，不能为“DB 一次整理”混入无关 SQL 行为。007/018 共享已验证的遍历器，但分别验证复杂度和 Promise 拥有字段。
- 同类 efun 校验修在生成/真实调用约束处；不能给每个入口套一个绕过实际 worker/执行点的包装。不得统一吞错、关闭 sanitizer 或删除 owner 检查。
- 格式化只用于本单元实际修改的 native 文件；有意非法 LPC/raw-byte 夹具沿用现有排除规则。第三方、生成物、历史文档不批量改风格。

### 6.2 格式与静态检查

下例的数组必须替换为该实施卡实际修改的精确文件。示例中的两条 native 路径适用于 ASCII 单元，不是其他单元的默认写范围。

```bash
NATIVE_PATHS=(src/comm.cc src/tests/test_lpc_gateway.cc)
export CLANG_FORMAT=clang-format-18
"$CLANG_FORMAT" --version  # 必须是规范规定的 18.1.8
"$CLANG_FORMAT" -i -- "${NATIVE_PATHS[@]}"
"$PY" -B tools/style/check-format.py --paths "${NATIVE_PATHS[@]}"
git diff --check -- "${OWNED_PATHS[@]}"

# 有 clang-tidy 时，对相关编译单元使用对应 build 的编译数据库。
# 它不代替 ASan/UBSan/TSan，也不允许为消警报扩大禁用项。
clang-tidy -p "$B" "${NATIVE_PATHS[0]}"
```

- 新 LPC 用例使用 `bash testsuite/format.sh --write <明确文件列表>`，再运行同范围只读检查。非法语法/原始字节测试先核对排除合同；不得让 formatter 修掉被测输入。
- 实际改变 grammar/相关生成源的编译器卡运行 `tools/build/regenerate_grammar.py --write/--check`（GNU Bison 3.8.2）和 CMake `generate_ebnf`；纯实现修复不无故重生成。检查三个 Bison fallback 与 EBNF/JSON/TextMate 合同输出，不手工维护副本。上述模式会创建临时文件，均只属于获批实施阶段。
- 修改 CMake/CI 时运行 `tools/docs/check-workflows.py --self-test` 与可用的固定版 actionlint；缺工具记 blocked，不以源码看似合法冒充检查通过。不要为本报告运行全库构建或网站部署。

### 6.3 单元与集成顺序

1. 每卡先运行自己的最窄 fail-first 与修后同一命令。观察故障本身、selected/executed/assertion/async 完成、非零退出和 sanitizer 报告。
2. 内存卡运行 ASan/UBSan；并发卡运行 TSan。TSan 的 configure、构建枚举和测试均按 §3 包装，不能只给最终测试加 setarch。
3. 中央 VM/编译器/owner 修后的定向测试全部通过后，才扩到相关 CTest 和 LPC 全量集合。保留隔离、scope 与异步完成协议；续审已有修前动态失败记录（发现报告 §10），不是修后通过证据。
4. 不把 fixture/helper/能力不支持项算成通过；预先按 testsuite 现有分类记录。DB 可选后端缺库则标阻塞，不能悄悄换成没有该后端的二进制继续宣称覆盖。
5. 旧协议 fuzz targets不能替代 ASCII/Telnet 新路径。001/009/010、011/012 必须增加实际生产同源的分段/合包属性用例；DAG/Promise 用唯一节点/边数和生命周期检查，不只用时间是否较短。

### 6.4 性能证据

只有实际触及热点的卡（尤其 003、006、007/018）需要对应对照。按现行主计划的冻结源码/原始证据方法，让 baseline 和 candidate 共享其他全部已完成修复、同一工具链/flags/硬件/输入。不能把 3882 文件快照或 mock 数字写成真实硬件性能。

- VM 热点可使用实际目标 `lpc_vm_bench`：先在对应配置构建该目标，再从 runner 的 `--tool "$B/src/tests/lpc_vm_bench" -- --json "$E/<本轮唯一输出>.json"` 入口运行；它接受 `--json`，不要捏造 `--iterations` 参数。分别跑两版并保留原始 JSON。
- 至少交替 7 轮并记录负载/异常值。拟批准的告警线为相同 workload 中位耗时 +5% 或 p95 +10%；这些是本方案建议阈值，不是假称项目已有结论。超过阈值先调查输入/热身/系统负载与真实算法，不能去掉安全检查。
- GC 等缺陷应同时用工作量计数证明线性复杂度；输出/请求预算验证高水位不超配置。时间数字不能替代这些硬约束。不可用旧基线缺其他安全修复的“更快版本”比较。

## 7. 上游同步清单与执行顺序

本地只读基准是 `upstream/master@79548f86f66a`；续审另取得在线 6 个新增提交缓存，终点为 a2902952d74a。以下区分本地已核验残余与在线候选，不是“上游今天全部安全补丁”列表。

| 顺序 | 上游对象 | 本地对应 | 采取动作 | 不取的部分 |
| --- | --- | --- | --- | --- |
| 1 | `b2ba3f8f0031` | AUD-009，`src/comm.cc` | 读取上游 `src/net/transport_libevent.cc` 的前读条件；与 001/010 的本地算法一起移植 | 同提交的 allocator/simul/runtime 大量改动不能整包混入 |
| 2 | `5f7be1008f28` | AUD-011/012，`src/net/telnet.cc`、`src/comm.cc` | 回移完整序列后转换、控制键与字符模式处理的语义，保留当前编码/session 状态 | NAWS、协商等其他 hunk 单独记录“已等价/不适用/待审”，不得默认为已修好 |
| 参考 | `858d5da9` | AUD-018 | 仅核对上游 GC/VM-state 设计，使用本 fork 的 Promise/owner 字段做安全遍历 | 它不是本 fork Promise 漏扫的可直接应用补丁，不伪装安全 cherry-pick |

获批后的只读取证命令（没有 fetch/merge 副作用）：

```bash
git show --format= b2ba3f8f0031 -- src/net/transport_libevent.cc
git show --format= 5f7be1008f28 -- src/net/telnet.cc src/comm.cc
git show --stat --oneline 858d5da9
```

1. 先在上述具体上游路径核验 hunk，再映射到本地真实调用链。记录选取片段、未取原因、来源 SHA 和本地等价测试。
2. 本批 **可安全整包 cherry-pick 的提交为 0**；两个候选均夹带其他改动，直接 merge 还会引入 366 个未逐项语义核验的上游提交。不得为满足命令形式执行危险的整包同步。
3. 用精确编辑移植确认的 hunk，再执行本地测试/生成步骤。提交说明保留 `Upstream-reference: <确切 SHA>`，不要假称 git cherry-pick -x 已发生。
4. 若以后找到单一、已完整审查且与本 fork ABI/owner 兼容的修复提交，才可在该单元授权内选择带来源记录的 cherry-pick；完整 merge、冲突重写、大范围 ABI/包调整另行评估。当前没有批准这类操作。

续审另取得本地快照后的 6 个在线提交，完整 SHA、标题和缓存入口见发现报告 §10.3。优先级与动作如下；这些对象不保证已在本地 Git 对象库，不能直接把 git show/cherry-pick 当作可执行的已确认步骤。

| 顺序 | 在线候选 | 关联单元 / 必须复核 |
| --- | --- | --- |
| 3 | 774d1783c4193ff620a8ef2692e9ef360b33a20c | AUD-013；整行读取、超长设置拒绝、IO 错误及原配置语义 |
| 4 | ffbd54a67d4f64f97656a2776f77da2a57d9d80d、c51f482b55ffdd6c2a6b260c07decdb186c70b26 | AUD-017；错误可观察性，逗号/块/引用/$() 的完整组合 |
| 5 | a2902952d74a53b5b031aa2d98633bec5fba6e55 | AUD-016；catch 上下文位与本 fork async/await、循环边界 |
| 6 | 9d24db92b8e693fcd40b5e57eaaa07864ee404f5、df21db8e8c6ccd196fdbb045b760d29d9afdbe44 | 两项文档候选；仅本地仍有对应失配时同步，不当作安全补丁 |

先逐 hunk 证明仍有真实残余且适配本 fork，再精确移植、补回归并记录 Upstream-reference。若确认单个提交无无关变更、接口/owner 相容，才在该实施单元授权内考虑 cherry-pick；**当前预先批准的整包 cherry-pick/merge 数仍为 0**。不能仅凭在线标题取代 013/016/017 的本地测试与生成步骤。

## 8. 文档同步与归属

本次只交付两份根目录报告，不直接修下面的文件。实施时按真实行为变化更新权威来源，不双写 README/JSDoc，不改冻结历史。

| 权威文件/范围 | 对应内容 | 更新条件 |
| --- | --- | --- |
| `docs/driver/config.md`、`src/Config.example` | 输出/async 预算、超限策略、配置读取失败 | AUD-002/003/013；明确单位、默认值、非法值、权限、统计与兼容影响 |
| `docs/efun/db/`、`docs/build.md` | DB 代际/错误、编号与 backend 构建配置 | AUD-004/014/015/024；不把编号当 API 版本 |
| `docs/build.md` | Windows 安装、cwd、有限并行、生成流程 | AUD-020/024；遵守当前 build 与项目资源规则 |
| Telnet/get_char/encoding 对应文档 | 输入完整性、控制键、字符模式 | AUD-011/012；已接受的字符序列与原先过滤策略边界都要说明 |
| eval_limit/await/catch/time_expression 归属文档 | 预算与可观察异常/不可挂起约束 | AUD-006/016/017；不改变 acatch 语义 |
| reclaim/Promise/owner 合同 | 根集合、遍历生命周期与准入 | AUD-007/018；屏障未证实时不能写成完成 |
| `docs/multicore-production-gate.md`、`docs/multicore-runtime-v4.md` | 真实状态 vs 外部证据、字段类型与 unknown | AUD-022；不改历史成绩或伪造新 manifest |
| `docs/project-modernization-plan-2026-10.md`、`third_party/manifest.yaml` | 声明版本、实际身份、来源/PURL、patch 状态 | AUD-021；SBOM按脚本生成，不手改生成物 |
| `docs/coding-standard.md` | 已启用的全 native 范围与保留排除项 | AUD-023；历史 U10 完成事实不重写 |
| `docs/project-modernization-plan-2026-10.md` | 获批新工作挂靠、ID/依赖/真实验收证据 | 开始实施前按用户批准范围接入唯一主计划，不并行造第二套 TODO |
| `AGENTS.md` | 稳定入口/资源或 owner 约束摘要 | 只有事实改变才同步；不复制完整发现清单 |
| `CHANGELOG.md` | 用户可见修复、错误/超限/跨平台行为 | 各修复真实完成后追加，不预填“已修复” |
| `CONTRIBUTING.md` | 新增测试入口对贡献流程的影响 | 现有流程足够则不改；不为了清单凑修改 |
| fork 差异/历史 absorption 文档 | 来源引用与新证据 | 新决策进现行主计划；冻结旧计划、Bugs 和历史证据不回写 |

新增 AUD-025–069 的具体权威文件列在各卡；query_multiple_short 与 replace_dollars 当前没有专页，实施时沿同包 query_num 的 contrib 归属新增两页，不假称现有页面。其他已列文档路径须与实际文件核对。

纯内部半开边界修复 AUD-019 不需改公开语义文档。全局 skills/rules 现有原则已覆盖本次方法，本次不擅自扩大权限或新增全局规则。

## 9. 最终只读复核记录与验收边界

- 对照原任务 A–P 的 109 个子项、69 个问题 ID、fork/merge-base、调用者及实际 CLI/目标核对。最初源码为 a4475b1afe99，续审为 55c7360b；两者产品源码相同，后续报告提交不冒充新的产品基线。
- 两份文档分别有 AUD-001…AUD-069 的唯一详细节。交付前已机械核对 ID、统计、链接与新增源码引文：55 段引文一致、88 段 shell 通过 bash -n、73 个本地链接有效，git diff --check 通过。依赖和改动范围另按各卡核对；未来源码变化后必须复核行号。命令语法通过不代表实施命令或拟议补丁已执行。发现报告错误只修报告，不顺手改运行代码。
- 保留 SQL 临时结果所有权、半开索引 pair、ASCII 分隔/游标、CHAR_MODE/UTF-8、owner/epoch 和 Promise 合同。新增卡另保留 external_write 的 boolean 语义、合法 PCRE 前瞻/零宽捕获、字符串字节长度与输入值不变性。
- 配置读取保持有界，Windows 安装使用现有 CMake TARGETS，FORTIFY/完成屏障/零断言门禁不能关闭。AUD-039 是明确的权限政策收紧，须随实施方案一并批准。
- AUD-003 drain/stop、007 owner 冻结、038 异步完成及 041 能力预分类均须由真实路径验证；拿到入口或写出代码片段不算已通过。
- **69 项修复均未实施。** 续审已做隔离构建/测试，既有失败和边界见发现报告 §10；没有操作现有 driver、部署、发布或回滚。批准后按依赖连续实施，缺证/失败项保留 blocked，不补记完成。

## 10. 原审计目标的剩余闭合工作

这些是明确未完成的证据工作，不是已经完成的修复；不由两份报告落盘自动关闭。

1. **源码深度矩阵。** 以发现报告的 109 个检查点与分片文件列表为索引，逐个记录 VM opcode/编译器产生点/efun 的输入类型、栈效应、引用/owner、异常/析构/挂起路径和对应测试。每个未覆盖分支明确列出，不用“全文已读”替代语义结论。只读检查可以继续；新增测试实现仍须在批准范围内。
2. **差异与来源。** 按附录 B 的路径逐 hunk 核对安全模型、内存、事件循环与热重载影响；按 34 项不确定来源逐条检查 blame/log/上游对照，记录证明不了的部分。不得只按提交标题或缺 SHA 归因。在线 6 个候选先读缓存 patch 或只读 HTTP 资料，不在只读阶段 fetch/merge。
3. **当前失败的验证。** AUD-037/038/041/042/044 获批修复后，先跑本卡负向/正常测试，再重跑原失败命令；完整 LPC 与 CTest 都须有真实完成/覆盖记录。SQLite 单例若仍超时，停止该卡完成判定并追踪退出期原因，不延长超时或假结算。
4. **原生平台与运行负载。** 为原生 macOS/Windows 与 WASM runtime 分别提供真实工具链、隔离输出和可追溯产物；006/008/020 按卡验证计时/信号/安装。持续慢读/TLS、外部数据库与容量只在获准的独立环境进行，不能接管现有 driver 或借用用户数据库。
5. **安全适用性。** 以真实组件 revision/patch 核对公告与 3 条 CodeQL open 告警，分成确认问题、明确不适用和待补证；没有证据不声称误报、已修复或零漏洞。若新增问题改变批准的实施范围，先更新问题表和方案再实施。
6. **最终对账。** 每项给出 passed/failed/blocked/unverified 与证据路径；未完成清单和原因必须随交付保留。修复完成、审计覆盖和真实平台验收是三种不同结论，不相互代替。
