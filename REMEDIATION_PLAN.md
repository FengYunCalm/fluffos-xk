# FluffOS_XK 审计修复方案

> 对应 [AUDIT_FINDINGS.md](AUDIT_FINDINGS.md) 的 AUD-001–AUD-024。
> **本文件是待批准的实施方案，不是实施记录。** 本轮只获准调查和写两份报告，没有执行以下代码修复、构建、测试、安装或回退。
> 现行唯一实施主计划仍是 [docs/project-modernization-plan-2026-10.md](docs/project-modernization-plan-2026-10.md)。批准实施本方案后，把问题 ID、依赖和证据挂靠到该计划的对应单元；本文件不擅自取代主计划或开启另一条开发路线。

## 1. 目标、范围与影响

### 1.1 验收对象

源码调查基线为 `a4475b1afe998bb03a37bdb0596b64ab01042ed3`，分支 `main`。上游本地快照 `79548f86f66af19f61e03a0cca1aadc95f1e8973`，merge-base `277d0b1cbc4350844d9aff07a604dfa519650d9e`。最初工作区、暂存区及非忽略未跟踪集合均为空。后续若出现他人改动，必须重新划定本单元文件范围，不覆盖、清理或夹带提交。

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

### 1.4 实施验证前置与当前证据边界

| 阻塞/前置 | 处理方式 | 影响范围 |
| --- | --- | --- |
| AUD-007 的冻结窗口需要实施并验证 | 按 AUD-007 复用已定位的 vm_owner_recompile_quiesce_begin/end。先补 fail-first，再在实际回收入口接入窗口；冻结/拒绝/异常恢复测试通过后推进遍历算法。不能以 main-thread-only 推断隔离充分，也不能在冻结失败后继续写图 | 007、018；设计入口已明确，动态验证尚未执行 |
| macOS/Windows 可运行环境 | 必须用真实原生平台验证 006/008/020；交叉编译不能冒充平台通过 | 对应单元未齐证据时保持 blocked/unverified |
| SQLite/libpq/MySQL 开发依赖与 Linux 测试包装 | 用现有包配置显式启用；缺开发库时不能把 DB 用例计为跳过通过。不连接用户数据库；真实 PostgreSQL/MySQL 集成另需私有服务授权 | 004、014、015 |
| 最新上游公告/在线 GitHub 配额 | 本次只依赖已有本地对象。在线资料恢复后核对新的安全修复；没有证据不宣称最新安全清单完整 | 新发现如改变本方案范围需重新审定 |
| 本次全仓深度覆盖未全部闭合 | 执行者不能把修完 24 项称为全仓无遗漏安全审计完成；补充深审与动态平台证据要单列 | 最终总结的结论强度 |

## 2. 工作顺序与里程碑

### 2.1 依赖顺序

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
| 阶段一：确认与止血 | 公共准入、001/009/010；005 的敏感日志修复可独立完成；为 007 建立 owner 隔离证据 | ASCII 真实 LPC 关闭回调与全部分隔边界回归；没有 UAF/错误游标；日志不含合成隐藏输入。未证实的 007 保持阻塞 |
| 阶段二：资源与正确性 | 按依赖推进 002–008、011–022（已完成的 005 不重复）；015 可先于 P1 的 004，因它是资源状态前置 | 各 ID 的新测试/真实装配/平台要求逐条满足；任何缺证项仍为 pending/blocked，不补记通过 |
| 阶段三：收口与维护 | 023/024、归属文档/主计划、整合的定向回归、必要的性能对照、最终差异与提交审查 | 24 个 ID 均有独立状态与证据；没有未修复批准项被悄悄遗漏；不越界发布或宣称全仓漏洞清零 |

工作量粗估合计 **224–416 工时**，是代码/夹具/定向验收的规划区间，不是当前耗时或交付承诺；不含等待平台、审批、外部服务、全量补充审计。可以由不同责任角色并行只读调查，但同一工作区不要并行写同一文件或混跑构建。

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
for d in "$BD" "$BA" "$BU" "$BT"; do test ! -e "$d" && test ! -L "$d"; done
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
| AUD-004 | P1 | AUD-015 | 并发/运行时维护者 | 16–32 小时 | DbIoTest.AuditConnectionGeneration* |
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
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-002 · 交互输出队列没有字节预算和背压

- **修复目标：**在实际入队点统一预留并归还字节预算，超限关闭具体慢会话并计数，不阻塞主事件循环或静默截断。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/net/ws_ascii.cc:148–155`、`src/net/ws_telnet.cc:123–130`、`src/net/telnet.cc:97–105`。
- **作用范围：**`src/net/ws_ascii.cc`、`src/net/ws_telnet.cc`、`src/net/telnet.cc`、`src/comm.cc`、`src/net/websocket.cc`、`src/net/websocket.h`、`src/comm.h`、`src/base/internal/rc.cc`、`src/base/internal/rc.h`、`src/include/runtime_config.h`、`src/tests/test_lpc_gateway.cc`、`src/Config.example`。测试归属 `src/tests/test_lpc_gateway.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-001 已满足；网络/C++ 运行时维护者；24–40 小时。
- **必须击中的根因：**evbuffer_add/bufferevent_write 的实际入队点未检查单会话或全局累计字节；读端限额、单次 write 长度和 libwebsockets 帧校验不能限制待发送队列。

#### 操作

1. 新增拟议配置 maximum pending output bytes=1048576、maximum total pending output bytes=67108864，单位为实际排队字节；配置缺省和超限关闭策略随本方案一起审批，不保留 unlimited 作为默认。
2. 把预算检查放到 ws_ascii_send、ws_telnet_send 和 bufferevent 输出的实际追加处。复用公共 WebSocket session/helper；不同传输只适配长度/完成通知，不复制计数规则。
3. 检查 len > limit - pending，避免加法溢出；成功入队后记账，失败、部分发送和清理时准确归还。保持已有 lws_write TLS-expanded-return 处理：按请求 payload 长度 drain。
4. 增加 main-thread 状态计数与原因，日志只记录长度/状态，不记录输出正文；不能让新增日志泄漏 NOECHO 输入。
5. native 夹具调用真实 LPC 输出，按单会话、全局、写错误和断线矩阵验证。仅在私有端口进行慢读，不碰现有 driver。

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

- **验收：**队列峰值不越过已批准预算，关闭/错误后计数归零；正常字节流、TLS/WebSocket 协议和回调顺序不变；对照吞吐回退超过 5% 时停止而非放宽门槛。
- **风险与缓解：**新增资源限制会改变超额输出行为；默认值是方案建议而非现有合同或容量测量。编码、压缩、分片、关闭都必须归还正确字节数。
- **文档同步：**`docs/driver/config.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-003 · 异步文件/数据库执行没有工作线程和排队内存上界

- **修复目标：**复用现有请求队列、唤醒和 owner completion，改为有界 worker 数、请求数及字节预算；getdir 在分配处限制目录项。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/packages/async/async.cc:284–299`、`src/packages/async/async.cc:449–474`。
- **作用范围：**`src/packages/async/async.cc`、`src/packages/async/async.h`、`src/base/internal/rc.cc`、`src/include/runtime_config.h`、`src/tests/test_lpc.cc`、`testsuite/etc/test-scopes.tsv`、`src/Config.example`、`src/vm/internal/simulate.cc`、`src/tests/test_lpc_owner.cc`。测试归属 `src/tests/test_lpc.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
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
F='DriverTest.TestAuditAsyncThreadBudget*:DriverTest.TestAsyncNestedCallbacksDrainAllRequests:DriverTest.TestAsyncThrowingCallbackStillDrainsNestedRequest'
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
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-004 · 异步数据库请求只保存可复用整数连接号

- **修复目标：**内部 generation+连接发布状态；在准入时锁内取代号，执行时锁内验证同一连接；两个异步 API 共用这条路径。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/packages/async/async.cc:402–418`、`src/packages/async/async.cc:554–564`、`src/packages/db/db.cc:639–662`。
- **作用范围：**`src/packages/async/async.cc`、`src/packages/db/db.cc`、`src/packages/db/db.h`、`src/tests/CMakeLists.txt`、`testsuite/single/tests/efuns/async.c`。测试归属 `src/tests/test_db_io.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
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
- **新增 native filter：**`DbIoTest.AuditConnectionGeneration*`，目标 `db_io_tests`。

```bash
N=AUD-004
F='DbIoTest.AuditConnectionGeneration*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/db_io_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_db_identity`。测试源是 `testsuite/single/tests/efuns/audit_db_identity.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-004
L=audit_db_identity
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**暂停/复用反例稳定失败在旧源码、修复后旧任务被拒绝；新库没有旧任务的副作用；错误和回调次数一致，TSan 无该路径数据竞争。
- **风险与缓解：**不能改变公开整数 handle 语义；不能在执行期间把裸 db_t* 保存在可重分配槽数组之外；失败连接发布必须与 AUD-015 配合。
- **文档同步：**`docs/efun/async/async_db_exec.md`、`docs/efun/db/db_close.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

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
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-006 · 非 Linux 构建没有执行求值时间限制

- **修复目标：**保留 Linux 计时器；非 Linux 使用线程私有单调 deadline 和 VM 安全点检查，使 set_eval/get_eval 语义一致。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P1 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/vm/internal/eval_limit.cc:9–29`、`src/vm/internal/base/interpret.cc:2478–2487`。
- **作用范围：**`src/vm/internal/eval_limit.cc`、`src/vm/internal/base/interpret.cc`、`src/vm/internal/eval_limit.h`、`src/vm/internal/posix_timers.cc`、`src/vm/internal/base/promise.cc`、`.github/workflows/ci.yml`、`src/tests/lpc_vm_bench.cc`。测试归属 `src/tests/test_lpc.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
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
if (++poll_count == 64) {
  poll_count = 0;
  if (std::chrono::steady_clock::now() >= deadline) {
    outoftime = 1;
  }
}
#endif
```

#### 定向验证

- **场景：**真实 macOS 和 Windows 各运行有截止时间的正常计算、超过预算的有限计算、嵌套 catch、set_eval_limit 和不同 owner 互不污染测试。
- **profile/真实边界：**Linux Debug/UBSan 保持回归；macOS+Windows 原生行为；VM benchmark 对照。
- **新增 native filter：**`DriverTest.TestAuditEvalBudget*`，目标 `lpc_tests`。

```bash
N=AUD-006
F='DriverTest.TestAuditEvalBudget*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **新增 LPC 对象：**`/single/tests/efuns/audit_eval_budget`。测试源是 `testsuite/single/tests/efuns/audit_eval_budget.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-006
L=audit_eval_budget
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**两种非 Linux 平台均真实中断超额计算且之后能继续执行；owner 间预算不串扰；Linux 原语义保持；正常 VM benchmark 回退不超过 5%。
- **风险与缓解：**wall time 与线程 CPU 时间不同；不能把跨 await 的等待算成未定义的新预算；必须沿用现有挂起/恢复的 set_eval 调用语义并记录平台差异。 此项保证纯 LPC 运算在安全点响应预算，不承诺可中断任意阻塞 C/外部库调用。
- **文档同步：**`docs/efun/system/set_eval_limit.md`、`docs/efun/system/eval_cost.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

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

- **新增 LPC 对象：**`/single/tests/efuns/audit_reclaim_graph`。测试源是 `testsuite/single/tests/efuns/audit_reclaim_graph.lpc`；纳入本单元文件清单并注册 scope。

```bash
N=AUD-007
L=audit_reclaim_graph
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --driver "$BD/bin/driver" --case "/single/tests/efuns/$L" \
  --timeout 180 --evidence-dir "$E/$N-lpc-debug"
```

#### 完成、风险和回退

- **验收：**工作量随唯一容器及引用槽近线性增长；共享/环/深图无 C 栈溢出；每条 destruct 引用正确释放，mapping 结构完整，后续回收仍可执行。
- **风险与缓解：**已有 owner-wide quiescence 入口已核对，可复用而不重做调度器；仍须通过真实 claim/拒绝/恢复测试。冻结失败不得访问全局对象图，end 必须使用匹配 epoch。mapping key 与 funptr program 引用沿用现有语义，worklist 保活先于重新开放 owner 结束。
- **文档同步：**`docs/efun/system/reclaim_objects.md`、`docs/owner-multicore-api.md`、`docs/multicore-runtime-v4.md`。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

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
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

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
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

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
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-011 · Telnet 按网络分块净化文本，破坏跨块编码序列

- **修复目标：**在具体 interactive_t 会话保存流式解码状态，只缓存不完整后缀；关闭/编码切换时按既有替换合同收口。
- **来源处理策略：**按 §7 选择性回移 5f7be1008f28 的相关 hunk，保留 fork 适配并记录未取部分。
- **优先级/来源：**P2 / fork 未同步上游修复。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/net/telnet.cc:66–93`。
- **作用范围：**`src/net/telnet.cc`、`src/comm.h`、`src/comm.cc`、`src/base/internal/strutils.cc`、`src/packages/sockets/socket_efuns.cc`、`src/tests/test_lpc_gateway.cc`。测试归属 `src/tests/test_lpc_gateway.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
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
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-012 · get_char 破坏控制字符，并按字节切断 UTF-8 字符

- **修复目标：**字符模式完整交付合法 UTF-8 序列和控制字节；不完整片段等待，行模式编辑/净化不改。
- **来源处理策略：**按 §7 选择性回移 5f7be1008f28 的相关 hunk，保留 fork 适配并记录未取部分。
- **优先级/来源：**P2 / fork 未同步上游修复。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/comm.cc:1453–1475`、`src/comm.cc:1504–1530`。
- **作用范围：**`src/comm.cc`、`src/tests/test_lpc_gateway.cc`、`docs/efun/interactive/get_char.md`、`docs/archive/Bugs`。测试归属 `src/tests/test_lpc_gateway.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**AUD-011 已满足；网络/C++ 运行时维护者；4–8 小时。
- **必须击中的根因：**SINGLE_CHAR 的提取器仍采用单字节/特殊清零规则；ESC 的行模式净化条件没有排除字符模式。

#### 操作

1. 读取 5f7be100 的限定 hunk。删除 first_cmd_in_buf 在 SINGLE_CHAR 下将 BS/DEL 置零的分支；保留 on_user_input 里已经正确的 SINGLE_CHAR 分支。
2. 将单字符提取改成按完整有效 UTF-8 序列交付，临时缓冲最多 4 字节加终止符；cmd_in_buf 与 first_cmd_in_buf 共用长度判定。不完整前缀等待，畸形字节走已明确的替换/进展合同，不能永久停顿。
3. ESC 的 no-ANSI/strip-before-process-input 替换只用于非 SINGLE_CHAR；保留 on_user_input 已正确的 BS/DEL 行模式编辑分支和 IAC 解析。
4. 逐项核对 Telnet、WebSocket Telnet 和 gateway snapshot 的调用路径，测试各配置组合。更新实际 get_char 归属文档；不修改 docs/archive/Bugs。

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
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

### AUD-013 · 超长配置行使后续配置静默停止解析

- **修复目标：**保留有界行缓冲，显式区分 EOF、超长行和 I/O 失败；任何读取失败均停止配置初始化，不再悄悄忽略余下安全指令，也不打印整行配置值。
- **来源处理策略：**在当前 fork 修残余问题，保留已有 owner/session/现代 LPC 行为；没有经核验的小补丁时不整包覆盖。
- **优先级/来源：**P2 / 上游继承问题。
- **输入证据：**[审计发现](AUDIT_FINDINGS.md) 同 ID；`src/base/internal/rc.cc:32–33`、`src/base/internal/rc.cc:229–254`。
- **作用范围：**`src/base/internal/rc.cc`、`src/tests/test_lpc.cc`、`etc/config.test`、`docs/driver/config.md`。测试归属 `src/tests/test_lpc.cc`；只改本问题实际需要的部分，证据/调用者不自动成为修改目标。
- **前置/责任/工时：**构建/依赖维护者；4–8 小时。
- **必须击中的根因：**把 getline 的缓冲耗尽失败与正常 EOF 当成同一终止条件。

#### 操作

1. 保留 K_MAX_CONFIG_LINE_LENGTH=120 的有界读取；显式检查 getline 的 fail/bad/eof 状态。超长行在解析任何后续配置前报错退出，不用读取整条任意长行后才检查长度。
2. 正确区分空 EOF、无末尾换行的合法行、恰好 119 字节的合法行和 120 字节以上的失败行；保留原注释剥除/trim/空行处理。删除会打印整行值的 truncated warning。配置启动失败就停止，不引入不必要的运行时配置事务重构。
3. 核对外部端口、路径与 TLS 配置的 scanf 宽度，保持经过长度检查后才能进入解析的单一入口。

#### 关键修改示例

```text
while (std::getline(buffer, line)) {
  ++line_number;
  if (line.size() >= K_MAX_CONFIG_LINE_LENGTH) {
    reject_overlong_config_line(line_number);
  }
  config_lines.push_back(line);
}
if (!buffer.eof()) {
  reject_config_read_error();
}
```

#### 定向验证

- **场景：**119/120/121 字节边界、有/无末尾换行、超长注释及后续合法配置；验证报错行号和不会悄悄继续启动。
- **profile/真实边界：**新进程配置解析测试，Debug、ASan；不把失败配置喂给现有 driver。
- **新增 native filter：**`DriverTest.TestAuditConfigLine*`，目标 `lpc_tests`。

```bash
N=AUD-013
F='DriverTest.TestAuditConfigLine*'
B="$BD"
HOME="$E/home" "$PY" -B tools/testsuite/run-targeted.py \
  --binary "$B/src/tests/lpc_tests" --gtest-filter "$F" \
  --timeout 180 --evidence-dir "$E/$N-native-debug"
```

- **独立 LPC case：**不适用或由 native 夹具运行真实 LPC 回调覆盖。不能增加没有断言的假用例。

#### 完成、风险和回退

- **验收：**超限或读取失败明确非零退出；合法边界值全部解析；后续配置既不丢失也不在失败后被误认为生效。
- **风险与缓解：**不能只换成无界 std::getline：后续 sscanf 的目标仍有固定数组。保留原可接受长度，不扩大未审计输入范围。
- **文档同步：**`docs/driver/config.md`；只更新受影响的合同/流程，不复制历史测试数字。
- **回滚：**执行 §4.2 R1，完整回退本单元及其必要生成/配置/测试内容；依赖本 ID 的单元先回退。回退后恢复 pending/withdrawn 状态，不保留 fixed 声明。
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

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
- **当前状态：**未实施、未运行上述验证。只有本卡和公共前置全部满足才标完成。

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
- 编译器两卡同时运行 `tools/build/regenerate_grammar.py --write/--check`（GNU Bison 3.8.2）和 CMake `generate_ebnf`。检查三个 Bison fallback 与 EBNF/JSON/TextMate 合同输出，不手工维护副本。上述模式会创建临时文件，均只属于获批实施阶段。
- 修改 CMake/CI 时运行 `tools/docs/check-workflows.py --self-test` 与可用的固定版 actionlint；缺工具记 blocked，不以源码看似合法冒充检查通过。不要为本报告运行全库构建或网站部署。

### 6.3 单元与集成顺序

1. 每卡先运行自己的最窄 fail-first 与修后同一命令。观察故障本身、selected/executed/assertion/async 完成、非零退出和 sanitizer 报告。
2. 内存卡运行 ASan/UBSan；并发卡运行 TSan。TSan 的 configure、构建枚举和测试均按 §3 包装，不能只给最终测试加 setarch。
3. 中央 VM/编译器/owner 更改的定向测试全部通过后，才扩到相关 CTest 和 LPC 全量集合。全量仍使用现有隔离入口及当前 scope/异步完成协议；本轮报告没有执行这些检查。
4. 不把 fixture/helper/能力不支持项算成通过；预先按 testsuite 现有分类记录。DB 可选后端缺库则标阻塞，不能悄悄换成没有该后端的二进制继续宣称覆盖。
5. 旧协议 fuzz targets不能替代 ASCII/Telnet 新路径。001/009/010、011/012 必须增加实际生产同源的分段/合包属性用例；DAG/Promise 用唯一节点/边数和生命周期检查，不只用时间是否较短。

### 6.4 性能证据

只有实际触及热点的卡（尤其 003、006、007/018）需要对应对照。按现行主计划的冻结源码/原始证据方法，让 baseline 和 candidate 共享其他全部已完成修复、同一工具链/flags/硬件/输入。不能把 3882 文件快照或 mock 数字写成真实硬件性能。

- VM 热点可使用实际目标 `lpc_vm_bench`：先在对应配置构建该目标，再从 runner 的 `--tool "$B/src/tests/lpc_vm_bench" -- --json "$E/<本轮唯一输出>.json"` 入口运行；它接受 `--json`，不要捏造 `--iterations` 参数。分别跑两版并保留原始 JSON。
- 至少交替 7 轮并记录负载/异常值。拟批准的告警线为相同 workload 中位耗时 +5% 或 p95 +10%；这些是本方案建议阈值，不是假称项目已有结论。超过阈值先调查输入/热身/系统负载与真实算法，不能去掉安全检查。
- GC 等缺陷应同时用工作量计数证明线性复杂度；输出/请求预算验证高水位不超配置。时间数字不能替代这些硬约束。不可用旧基线缺其他安全修复的“更快版本”比较。

## 7. 上游同步清单与执行顺序

只读输入是本地 `upstream/master@79548f86f66a`。本轮 HTTP 403/DNS 策略和搜索限制使在线最新公告未完整核验；以下不是“上游今天全部安全补丁”列表。

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

纯内部半开边界修复 AUD-019 不需改公开语义文档。全局 skills/rules 现有原则已覆盖本次方法，本次不擅自扩大权限或新增全局规则。

## 9. 最终只读复核记录与验收边界

- 已对照原任务 109 个子项、24 个问题 ID、fork/merge-base/工作区证据、主要调用者与实际 CLI/目标复核。生成前会把所有 SHA 与 git rev-parse/cat-file 核对，失配立即拒绝写入。源码与证据引用锚定 `a4475b1afe99`，不是泛指上游。
- 两份文档 ID 必须各出现一次详细标题，集合为 AUD-001…AUD-024；字段、依赖图、源文件行范围与原始摘录须经落盘后的机械核对。核对失败只修报告，不能顺便修运行代码。
- 方案保留 SQL 临时结果所有权、半开索引返回的 pair 合同、ASCII LF/CRLF 指针差异和 CHAR_MODE/UTF-8 输入政策；上游路径已核对。全文读取不代表逐行深审。
- 配置读取方案保留有界缓冲，不因修静默截断而新增任意长行分配；Windows 安装统一现有 CMake TARGETS 规则，不添加路径回退掩盖错误。
- **设计复核与实施门禁：**003 的 drain/stop 次序及 007 的 owner 冻结入口已按当前代码核对；实施后仍须通过真实 callback/claim 测试，失败按原子单元停止，不能换弱化方案。
- **当前实施状态：24 项均未实施。**构建/原生/LPC/Sanitizer/fuzz/性能运行均未执行；无源码修复、无现有 driver 重启或终止、无部署/发布。批准后的执行者按 §2–§8 连续推进，局部阻塞不授权绕过安全约束。
