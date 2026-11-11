# FluffOS_XK 整理执行方案（2026-11）

> 状态：**待批准**。批准后按 U0 → U9 顺序连续执行，不中途停下等确认。
> 问题依据：[refactor-findings-2026-11.md](refactor-findings-2026-11.md)。
> 本文每个单元都给出改动清单、精确步骤、验证命令、完成判据与回滚方式，可交给不了解
> 上下文的执行者按序完成。

## 0 决策摘要

### 0.1 目标

| 维度 | 目标 | 度量 |
| --- | --- | --- || 源码量 | 净减少 | 手写 C/C++ 从 173,008 行降到 ≤168,000 行（核算见下） |
| 观测面 | 契约化 | owner 可观测面 1,058 键 → **51**；gateway 状态面 365 → 0；合计删 ≈1,372 |
| 重复实现 | 消除 | 116 块随 U5b 删除而自然归零；其余 36/18/13/9 块降到 ≤2 |
| 状态机 | 显式 | 7 个隐式状态机改为 enum class + 单一转场函数（SM-01/02/05/06/07/09/11） |
| 文档 | 单一来源 | docs 根目录 18 份历史计划/报告全部归档（保留 1 份生效中的重编译设计） |
| 门禁 | 防复发 | 新增 3 条会失败的检查（status key、重复块、测试文件体量） |

行数下降的构成（避免写一个拍脑袋的降幅）：

| 来源 | 估算行数 |
| --- | --- |
| U5b 删 owner 观测键（≈1,150 键 × 约 2 行：键写入 + 对应计算） | ≈2,800 |
| U5b 删 handle 诊断字段及其赋值 | ≈100 |
| U3/U3b 删假审计 + 两个零消费者 efun | ≈310 |
| U7 合并重复（只计被删掉的那一份；array 三份去二、interpret 两对去二 …） | ≈2,500 |
| U6 状态机（净增，约） | +150 |
| 合计净减 | **≈5,560 → 约 167,400** |

### 0.2 不做（硬边界）

1. 不改任何 LPC efun 的名称、参数、返回类型、flags 数值、存档格式。
   **唯一例外**：U3b 明确列出的两个零消费者 bench efun（`vm_worker_bench`、
   `vm_worker_actor_bench`），它们没有任何 LPC 调用者。
2. 不放宽 owner 串行、epoch/destruct/stale 检查、main-required 边界。
3. 不重构 `src/thirdparty`。
4. 不手改生成物；`grammar.autogen.*`、`grammar.ebnf`、`lpc-grammar.json`、
   `lpc.tmLanguage.json` 只能由 `generate_ebnf` 目标或 Bison 重新生成。
5. 不触碰机器上正在运行的 driver，不覆盖其构建目录，不重启服务。
6. 不做上游 flex 迁移、Docusaurus 迁移、wasm、依赖升级。
7. 不做 `-j$(nproc)` 或无界并行构建。
8. 不为了"减少行数"合并语义不同的代码；说不出语义差异的不合并。

### 0.3 单元与提交

| 单元 | 内容 | 提交粒度 |
| --- | --- | --- |
| U0 | 基线冻结与验证能力 | 1（仅记录，不改源码） |
| U1 | 仓库卫生与安全 | 2（安全凭据 1 + 卫生 1） |
| U2 | 文档单一来源 | 2（归档 1 + 重写 1） |
| U3 | 伪能力与零消费者 efun 删除 | 2（假审计 1 + bench efun 1） |
| U4 | 上游 P0 修复吸收 | 每提交 1，共约 12 |
| U5 | 观测层契约冻结与收口 | 4（冻结文档 1 + 删键 2 + 网关状态拆分 1）；U5d 下游验收无提交 |
| U6 | 状态机显式化 | 每个状态机 1，共 7 |
| U7 | 重复实现合并 | 每项 1，共 9（U7-2 已取消） |
| U8 | 测试结构 | 3 |
| U9 | 长期门禁 | 3 |

## U0 基线冻结与验证能力

**目的**：在改动任何源码前，得到一组可复现的"改动前"数字与一条可靠的定向验证路径。
没有这一步，后面所有"通过"都无法证明。

### 步骤

1. 资源检查（必须全部满足才继续）：

```bash
df -h / ; free -h
```

判据：`/` 可用 ≥ 30 GiB；`MemAvailable` ≥ 2 GiB。低于 2 GiB 时先结束无关进程，
不得直接开始编译（本项目 WSL2 约束）。

2. 冻结身份：

```bash
GIT_OPTIONAL_LOCKS=0 git -c core.fsmonitor=false status --short --branch
GIT_OPTIONAL_LOCKS=0 git rev-parse HEAD
git rev-parse upstream/master
git merge-base HEAD upstream/master
cmake --version | head -1
c++ --version | head -1
```

把输出写入 `audit-evidence/refactor-2026-11/U0-baseline.txt`（该目录已被
`.git/info/exclude` 排除，属于本地证据，不入库）。

3. 建立干净构建（**不复用**现有 `build-*` 目录，避免污染其他任务）：

```bash
cmake --preset dev-debug -B build-refactor-2026-11
cmake --build build-refactor-2026-11 --target driver lpcc lpcshell lpc_tests ofile_tests --parallel 4
```

内存低于 3 GiB 时把 `--parallel 4` 改为 `--parallel 2`。

4. 记录改动前基线（每条命令的通过/失败数写入同一证据文件）：

```bash
ctest --test-dir build-refactor-2026-11 --output-on-failure 2>&1 | tail -30
cd testsuite && ../build-refactor-2026-11/bin/driver etc/config.test -ftest 2>&1 | tail -30
```

5. 记录机械基线（后续每个单元都要重跑对比，口径与 U5a/U9a 一致）：

```bash
# 手写 C/C++ 行数
find src -type f \( -name '*.cc' -o -name '*.h' -o -name '*.c' \) -not -path 'src/thirdparty/*' \
  -print0 | xargs -0 wc -l | tail -1
# status key 数（-A2 容许多行调用的键落在下一行）
grep -rhn 'add_mapping_[a-z]*(' -A2 src --include=*.cc \
  | grep -o '"[a-z][a-z0-9_]*"' | tr -d '"' | sort -u | wc -l
# ctest 用例数与 LPC 用例数
ctest --test-dir build-refactor-2026-11 -N | grep -c 'Test  #'
find testsuite -name '*.c' -o -name '*.lpc' | wc -l
# 生成的 EBNF 与源码一致
cmake --build build-refactor-2026-11 --target generate_ebnf
git -c core.fsmonitor=false diff --stat -- tools/lpc-syntax/grammar.ebnf tools/lpc-syntax/lpc-grammar.json tools/lpc-syntax/lpc.tmLanguage.json
```

**完成判据**：步骤 1 判据满足；步骤 3 全部目标生成成功；步骤 4 的结果被记录
（无论通过或失败——若基线本身有失败，先记录失败清单，后续单元不得把它算作回归，
也不得顺手修复范围外失败）；步骤 5 的 diff 为空。

**回滚**：删除 `build-refactor-2026-11/`。

## U1 仓库卫生与安全

### U1a 凭据与死配置（独立提交）

| 文件 | 动作 | 依据 |
| --- | --- | --- |
| `.coveralls.yml` | 删除 | 含明文 `repo_token`；服务已不使用；唯一消费者是历史审计文档 |
| `qodana.yaml` | 删除 | 无任何 workflow 引用 |
| `fix_permission.sh` | 删除 | 无消费者；`chmod` 语义已由 `.gitattributes`/CI 覆盖 |
| `src/outdated_define.txt` | 删除 | 无构建/工具消费者；内容与 `runtime_config.h` 迁移后重复 |
| `.ignore` | 删除 | 仅重复 `/audit-evidence/`，该规则并入 `.gitignore` 后无意义 |
| `docs/add_missing_efuns.py`、`docs/fix_md_header.py`、`docs/fix_seealso.py` | 删除 | 一次性文档迁移脚本，无任何消费者（仅历史审计文档提到过） |

保留：`docs/gen_index.py` + `docs/update_index.sh`（真实成对的索引生成器，
`update_index.sh` 调用 `gen_index.py`）。

新增/修改：

1. `.gitignore` 追加（从 `.git/info/exclude` 迁移，使所有 clone 一致）：

```gitignore
/audit-evidence/
/evidence/
/testsuite/log/
/testsuite/*.sqlite
/testsuite/*.o
/testsuite/.targeted-*
/.codex/
/.claude/
/.codex-worktrees/
```

2. 从索引移除只存在于本地的证据文件，并移除编译产物：

```bash
git rm --cached -r audit-evidence
git rm testsuite/test.o testsuite/data/gateway_net_dead_destruct_user.o
```

3. 清理 `.git/info/exclude` 中已迁入 `.gitignore` 的条目（保留 `/.codex*`、
   `/.claude/`、`/tools/loadtest/__pycache__/`、`.gitnexus/` 等纯本地条目）。

4. 同步 `tools/docs/check-docs.py` 的 `KNOWN_PREFIXES`：删掉 `qodana.yaml`、
   `fix_permission.sh` 两项（否则前缀白名单携带已删除文件）。其"历史文档非本地引用"
   允许列表在 U2 归档后需要重跑一次，删掉指向已移动路径的条目。

**安全动作（不在代码仓库内，需单独执行）**：`repo_token` 已进入 git 历史。
若该 token 在任何仍可访问的服务上有效，必须在该服务侧吊销并重建；仓库侧删除文件
不能撤销泄露。执行者在报告里必须写明"已吊销/不存在"或"该服务已下线"。

**验证**：

```bash
python3 tools/docs/check-docs.py
python3 tools/docs/check-workflows.py --self-test
git -c core.fsmonitor=false status --short
```

判据：`check-docs.py` 通过（删除的文件没有被文档链接引用；若被引用，同一提交内
更新引用）；`status` 只显示本单元的预期改动。

### U1b 门禁配置对齐

1. `src/.clang-tidy` 顶部注释改为事实描述：当前**无** CI 门禁，规则用于本地评审；
   由 U9c 真正接入后删除该声明。
2. 复核 `src/.clang-format` 与 `docs/coding-standard.md` §3.3 的 YAML 值逐项一致
   （`IndentWidth`、`ColumnLimit`、`SortIncludes`、`QualifierAlignment`、
   `AllowShortIfStatementsOnASingleLine` 等）。不一致时以规范为准修改配置文件。

**验证**：

```bash
python3 tools/style/check-format.py --all-native
```

判据：通过（只读检查，不写源码）。若因环境缺少 clang-format 18.1.8 失败，记录为
环境缺口，不得改用任意 PATH 版本。

## U2 文档单一来源

**目的**：让任何新会话只读两份文档就能知道"现在是什么状态、下一步做什么"。

### U2a 归档历史计划

1. 新建 `docs/archive/plans/`，用 `git mv` 移动（**内容不改**，保留冻结历史）：

```bash
mkdir -p docs/archive/plans
# 仓库根的两份历史审计产物
git mv AUDIT_FINDINGS.md   docs/archive/plans/AUDIT_FINDINGS-2026-10.md
git mv REMEDIATION_PLAN.md docs/archive/plans/REMEDIATION_PLAN-2026-10.md
# docs 根目录的历史计划/报告
git mv docs/backlog-plan-2026-09.md               docs/archive/plans/
git mv docs/backlog-full-plan-2026-09.md          docs/archive/plans/
git mv docs/branch-evidence-archive-20261019.md   docs/archive/plans/
git mv docs/e4-read-source-line-port-eval-2026-08.md docs/archive/plans/
git mv docs/external-required-readiness-2026-09.md   docs/archive/plans/
git mv docs/lpcshell-prerequisite-plan-2026-08.md    docs/archive/plans/
git mv docs/optimization-master-plan-2026-08-18.md   docs/archive/plans/
git mv docs/project-modernization-plan-2026-10.md    docs/archive/plans/
git mv docs/recompile-followup-plan-2026-08.md       docs/archive/plans/
git mv docs/recompile-object-special-design-2026-08.md docs/archive/plans/
git mv docs/tsan-ftest-report-2026-08-19.md          docs/archive/plans/
git mv docs/upstream-absorption-plan-2026-10.md      docs/archive/plans/
git mv docs/upstream-absorption-plan-d07e7641-735bd31f.md docs/archive/plans/
git mv docs/upstream-sync-evidence-2026-08.md        docs/archive/plans/
git mv docs/upstream-sync-optimization-plan-2026-08.md docs/archive/plans/
git mv docs/vendor-libwebsockets-manifest-2026-08.md docs/archive/plans/
git mv docs/build_v2017.md                         docs/archive/plans/
git mv docs/project-knowledge-map.md               docs/archive/plans/project-knowledge-map-2026-08.md
```

**不移**：`docs/bug.md`（用户排查页，sidebar 引用）、`docs/fluffos-xk-overview.md`
（index 与 sidebar 引用）、`docs/multicore-runtime*.md`、`docs/multicore-production-gate.md`、
`docs/owner-multicore-api.md`、`docs/lpc-modern-runtime.md`、
`docs/recompile-object-v2-design-2026-08.md`（这 7 份是**仍在生效的合同/设计**，
不是历史计划）。

2. 归档前必须提取仍未完成的条目（见 U2b 步骤 1），否则归档等于丢弃待办。

3. 建立归档索引 `docs/archive/plans/README.md`：一句话说明"这些文档是历史记录，
   当前状态见 `docs/roadmap.md`；不得据其判断当前代码行为"。

4. 修正引用：

```bash
grep -rn "project-modernization-plan-2026-10\|upstream-absorption-plan\|AUDIT_FINDINGS\|REMEDIATION_PLAN\|project-knowledge-map\|build_v2017\|vendor-libwebsockets-manifest\|external-required-readiness" \
  --include='*.md' --include='*.yml' --include='*.py' --include='*.ts' --include='*.mts' . | grep -v '^./docs/archive/'
```

逐条改为新路径。已知必须改的：

- `docs/.vitepress/sidebar.ts:65` 的 `Build (v2017)` 条目（归档后删除该行）。
- `docs/coding-standard.md` 中指向旧主计划的链接（第 5 行与 §2 多处），改为指向
  `docs/roadmap.md`。
- `docs/archive/plans/upstream-absorption-plan-2026-10.md` 与 `REMEDIATION_PLAN-2026-10.md`
  内部互相引用，需改为同目录目标。
- `docs/evidence/*.md` 已移入 `docs/archive/evidence/`，原引用需同步。
- `AGENTS.md`、`README*.md` 若引用上述文档，同一提交内更新。

### U2b 建立唯一当前状态文档

新建 `docs/roadmap.md`，只含四部分，总长 ≤200 行：

1. **当前状态**：一张表（构建、测试、sanitizer、上游位置、已知缺口），每项标注
   `verified`（有本次可复现命令）/ `unverified` / `external-required`。
2. **未完成待办**：从归档文档中提取的、确实未完成的条目，每条给 ID、证据位置、
   依赖、验收方式。提取规则：只保留满足"有明确代码位置 + 有可执行验收"的条目；
   其余（措辞模糊、已被后续提交覆盖、无验收）直接丢弃，并在提交信息里说明丢弃数量。
   已确认的种子集合（不用重新读 700 KB 就能开工，但必须逐条回原文档核实后再写入）：
   - `upstream-absorption-plan-2026-10.md` 自述未完成：I15/P1 集成、D2、H（websocket
     TLS 热重载）、P2/P3。
   - 本方案的 U4 待判定项（9 个上游提交）与需设计项（typed efun、hooks、fused store）。
   - findings §5.5 Worker 总线缩减（独立课题）、§5.6 已在本方案 U3b 关闭。
   - U10 推迟的四个状态机（SM-03/SM-04/SM-08 + 中价值组）。
3. **如何验证**：本项目的最小验证命令集（从 `AGENTS.md` §5 复制，不新写规则）。
4. **文档索引**：指向 `coding-standard.md`、`architecture.md`、
   `owner-status-contract.md`、本方案的归档位置。

### U2c 重写架构文档

新建 `docs/architecture.md`（目标 ≤300 行），替代被归档的 `project-knowledge-map.md`：

- 模块表（路径 → 职责 → 入口函数），数据来自本仓 `src/` 实际结构。
- 三条主调用链（启动、LPC 编译、owner 任务）用 10 行以内的伪代码表达。
- 6 条不可破坏合同（从 findings §2.4 复制）。
- 指向 `owner-multicore-api.md`、`multicore-runtime-v4.md` 等既有权威文档。

**禁止**：不复制 `project-knowledge-map.md` 的枚举式段落；任何"某字段为 1 表示…"
的清单只能存在于 `docs/owner-status-contract.md`（U5 产出）。

**验证**：

```bash
python3 tools/docs/check-docs.py
```

判据：链接有效、commit SHA 引用有效、docs 根目录不再有任何历史计划/报告文档（保留的
`recompile-object-v2-design-2026-08.md` 属合同类，不计）。

## U3 伪能力删除（`lpcc --owner-audit`）

**删除目标**（findings §5.3）：`lpc_modern_profile.h` / `lpc_modern_profile.cc` 整体删除，
内容按归属重新分配，不保留重复实现。

| 原内容 | 处置 | 新归属 |
| --- | --- | --- |
| `LpcOwnerAuditRule/Finding/Report`、`lpc_owner_audit_rules`、`lpc_owner_audit_source`、`kLpcOwnerAuditSchemaV1` | 删除 | - |
| `lpc_modern_pragma_name`、`lpc_modern_pragma_name_for_flag`、`LPC_MODERN_PRAGMA_*` | 删除 | 编译期 pragma 名称/位的唯一来源是 `lex.h` 的 `PRAGMA_MODERN_LPC`/`PRAGMA_STRICT_OWNER` 与 `lex.cc:1715-1716` 的名称表（本项已有生产消费者，测试专用副本无存在理由）|
| `lpc_source_encoding_from_source`、`extract_source_encoding_pragma`、`strip_line_comment`、`trim_copy` | 移动到 | `src/compiler/internal/lpc_source_encoding.cc`（匿名 namespace）；必须修改 `lpc_source_encoding.cc:89` 的调用点 |
| `kLpcSourceEncodingSchemaV1`、`kLpcInternalStringEncoding` | 移动到 | `src/compiler/internal/lpc_source_encoding.h`（`lpc_source_encoding.cc:35,91`、`main_lpcc.cc:47` 改 include）|
| `kLpcModernProfileSchemaV1`、`kLpcModernProfileModeOptIn`、`kLpcStrictOwnerPolicyV1` | 移动到 | `src/vm/internal/owner.cc` 文件内匿名 namespace 常量（各自仅此一处使用）|
| `--owner-audit --format=json` 分支（`main_lpcc.cc:85-88`） | 删除 | - |
| `owner.cc:822-826`、`1954-1958` 的 10 个 `lpcc_owner_audit_*` key | 删除 | - |
| `test_lpc_owner.cc` 中 `lpc_owner_audit_*`、`lpc_modern_pragma_name*`、`lpcc_owner_audit_*` 断言（`589-621`、`11293-11295`、`11804-11805`） | 删除 | - |
| `test_lpc_support.h:63` 对 `lpc_modern_profile.h` 的 include | 改为 include `lpc_source_encoding.h` | - |

**保留**：`src/compiler/internal/lpc_source_encoding.{h,cc}`（真实的 ICU 编码探测），
`lex.h` 的两个 pragma 位，`lex.cc:1715-1716` 的名称表，`lex.cc:1736`
的 `source_encoding` 指令解析（唯一权威）。

**依赖方向修正**：改动前是 `lpc_source_encoding.cc → lpc_modern_profile.cc`
（编码层依赖演示层）；改动后只剩 `lpc_source_encoding.*` 自身，不再有反向依赖。

**验证**：

```bash
cmake --build build-refactor-2026-11 --target lpcc driver lpcshell lpc_tests --parallel 4
build-refactor-2026-11/src/tests/lpc_tests --gtest_filter='*SourceEncoding*'
build-refactor-2026-11/bin/lpcc --help
build-refactor-2026-11/bin/lpcc --owner-audit --format=json /tmp/a /tmp/b ; echo "exit=$?"
grep -rn "lpc_modern_profile" src tools testsuite
```

判据：编译通过（尤其是 `lpc_source_encoding.cc` 与 `main_lpcc.cc`）；
`*SourceEncoding*` 用例仍有命中且通过（**这是必须保留的真覆盖**，见下）；
`lpcc --owner-audit` 返回用法错误 `exit=1` 而不是 JSON 报告；`grep` 零命中。

**必须保留的测试**：`test_lpc_owner.cc:605-640` 那组同时包含审计断言和
`#pragma source_encoding("GBK")` 真编译断言。**只删审计部分**（`lpc_owner_audit_*`、
`LpcOwnerAuditReport`、`report.source_encoding` 断言），保留真实编译的 GBK 用例并
把它重命名到 `SourceEncoding*` 前缀，否则 U3 会顺手删掉一个真实能力测试。

**回滚**：`git revert` 该提交。

### U3b 删除零消费者的 bench efun

**依据**（findings §5.6）：`vm_worker_bench` 与 `vm_worker_actor_bench` 在 LPC 侧零调用
（本仓 testsuite、下游 `main`/`rewrite` 均无引用），唯一的"消费者"是
`test_lpc_owner.cc:4746-4749` 对 `.spec` 宽度的自证式断言。

动作：

1. 删 `src/packages/core/core.spec:404`（`vm_worker_bench`）与 `:406`
   （`vm_worker_actor_bench`）。
2. 删 `src/packages/core/vm_worker.cc:619-672` 与 `:674-…` 两个 `f_vm_worker_*` 实现块。
3. 删 `test_lpc_owner.cc:4746-4749` 对应的 spec 宽度断言。
4. **保留** C++ API `vm_worker_benchmark()` / `vm_worker_actor_benchmark()`
   （`src/tests/owner_runtime_bench.cc`、`bench_*` 目标在用），也保留
   `vm_worker_task` 里的 bench 任务类型调度（不在本项范围）。

验证：

```bash
cmake --build build-refactor-2026-11 --target driver lpc_tests owner_runtime_bench --parallel 4
ctest --test-dir build-refactor-2026-11 --output-on-failure
cd testsuite && ../build-refactor-2026-11/bin/driver etc/config.test -ftest
grep -rn "vm_worker_bench\b\|vm_worker_actor_bench\b" src testsuite
```

判据：全量测试通过；`grep` 仅剩 C++ 符号（如 `vm_worker_benchmark`）不再出现 LPC efun 名。

## U4 上游 P0 修复吸收

**规则**：每个上游提交一个本地提交；吸收前必须 `git show <sha>` 读完整 diff；
若本地同段代码已被改写，先判断等价性，**不得整段覆盖本地实现**。每个提交都要有
能失败的回归测试或说明为何无法构造（例如纯 Coverity 静态修复）。

### U4 清单（按风险与价值排序）

| 序 | 上游 | 内容 | 本地落点 | 验证 |
| --- | --- | --- | --- | --- |
| 1 | `7a66f8bf` | LPC 删除用户时的 UAF + Coverity CID 1688265-1688271 | `src/comm.cc` `process_user_command_text` | 新增 C++ 用例：命令执行中 destruct 用户后仍返回；ASan 构建跑该用例 |
| 2 | `a4fc80c5` | async 快速 mysql 查询死锁 | `src/packages/async/async.cc`、`src/packages/db/` | 需要 DB，标 `unverified` 直到本地 SQLite 路径覆盖；至少补嵌套 async 回归 |
| 3 | `923cdbc0` | `compress_file`/`uncompress_file` 短文件名越界 | `src/packages/compress/compress.cc` | 已有 `testsuite/single/tests/efuns/compress_file_long_path.lpc`，补短名用例 |
| 4 | `ddabcc89` | 64 位 LPC int 收窄 + `terminal_colour()` | `src/packages/contrib/contrib.cc` 等 | 新增 LPC 用例：超大 int 参与比较/换算；终端宽度用例 |
| 5 | `ffbd54a6` `c51f482b` `a2902952` | `catch(expr)` 不裁剪可能出错的运算；catch 逗号/块/ref/`$()` | `src/compiler/internal/`（`icode.cc`/`grammar.y`/`grammar_rules_*`） | LPC 用例：catch 内 error 后状态一致；改动 `grammar.y` 后必须 `generate_ebnf` 并提交生成物 |
| 6 | `5e138c3c` `91de2eaf` | Coverity（tracing 等） | 逐 CID 定位 | 编译 + 定向用例；无行为变化的写明 |
| 7 | `56964d9d` | 静态 float 截断进 int 时告警 | `src/compiler/internal/` | LPC 编译用例：断言诊断出现/不出现 |
| 8 | `328aaf10` | `mudlib_stats` 文件不存在时不报警 | `src/packages/mudlib_stats/` | 首次启动 smoke |
| 9 | `2ed199b1` `cadb2e8c` | 测试检查 mkdir/unlink/rmdir 返回值；Windows `_mkdir` | `src/tests/` | 编译 + 该用例 |
| 10 | `553232a2` | `request_term_type()` 完整 TTYPE SEND | `src/net/telnet.cc:761` | 需要 telnet 对端；标 `external-required`，只做代码对齐 + 单测覆盖字节序列 |
| 11 | `2c93fd86` | `-DPACKAGE_SOCKETS=OFF` 与无 global include 文件的构建 | `src/CMakeLists.txt`、`src/packages/sockets/` | 本地额外跑一次 `cmake -DPACKAGE_SOCKETS=OFF` 配置 + 构建 driver |
| 12 | `79548f86` `970d9f09` `01397dff` `2a7e23e3` `4dd319f0` `e92b5cf2` `d48a03d8` `de11656d` `3f457bb1` | 待判定项 | 见 findings §9 | 先 `git show` 判定，等价则记录"已吸收"并跳过；不等价才改 |

**禁止**：吸收 `0b09e62f`（typed efun）、`3b4bc500`（hooks）、`a26a1e06`（fused store）、
`b2ba3f8f`（四合一）到本单元。它们改变架构，必须另立方案。

**验证**（每个提交）：

```bash
cmake --build build-refactor-2026-11 --target driver lpcc lpc_tests --parallel 4
ctest --test-dir build-refactor-2026-11 --output-on-failure
cd testsuite && ../build-refactor-2026-11/bin/driver etc/config.test -ftest
```

**完成判据**：每个提交都有"上游 sha → 本地文件/函数 → 验证方式"的对应记录，
写入 `docs/refactor-plan-2026-11.md` 的执行记录表（本文末尾）。

## U5 观测层契约冻结与收口

**目的**：把 ≥1,353 个 status key 降到 51 个（下游实测值），并把"迁移脚手架"从对外
契约里彻底删除。这是本次整理最大的单笔减重。

### U5a 冻结 51 个键（先写文档，不改代码）

**已完成的前置调查**（findings §5.4）：下游 `xiakexing` `main`（`f6acdd30e`）实际读
51 个键；`rewrite`（`661fdfc60`）不读任何 owner 状态；`vm_object_store_status()`
在两个分支都零命中。因此不再需要外部确认环节。

0. **先录删键前的下游基线**（U5d 的对照物，必须在任何删除前完成）：

```bash
# 在 xiakexing 仓库，用当前（未修改）驱动构建启动测试实例
# 管理员执行 ownercheck full，完整输出存到：
#   <fluffos-xk>/audit-evidence/refactor-2026-11/ownercheck-before-keys.txt
```

把"通过/失败的 subcheck 集合"抄到 `docs/owner-status-contract.md` 的"下游基线"
一节。这一步只记录，不改任何代码。

1. 生成现状清单（本地证据，不入库）：

```bash
mkdir -p audit-evidence/refactor-2026-11
grep -rhn 'add_mapping_[a-z]*(' -A2 src --include=*.cc \
  | grep -o '"[a-z][a-z0-9_]*"' | tr -d '"' | sort -u \
  > audit-evidence/refactor-2026-11/status-keys.txt
wc -l audit-evidence/refactor-2026-11/status-keys.txt
```

（单行统计为 1,198；`-A2` 会把多行调用的键一并取出，结果在 1,300–1,400 之间，
少量非 key 字面量由人工剔除。）

2. 新建 `docs/owner-status-contract.md`，表格登记 51 个保留键（缺失则 U9a 门禁失败）：
   键 / 含义 / 类型 / 来源函数 / 下游读法。数据直接取自 findings §5.4 的四个分组表。

3. 同一文档加"2026-11 已移除"一节，列出删除的键名（供下游排查）。

**判据**：`docs/owner-status-contract.md` 的 51 行与 findings §5.4 逐字一致；
不包含任何 `bridge`/`retirement`/`_consistent` 类键。

### U5b 删除除 51 个冻结键之外的全部 owner 观测键

**已尝试一次，停在上测试阶段**（2026-11）。生产侧删除已验证可行：在 `object_store.cc`
去掉 `vm_object_shard_contract_mapping`（35 键）、`add_owner_local_lifecycle_contract`
（11 键）、`status_record_mapping`/`execution_shard_mapping` 的嵌套重复、以及 status 里的
bridge/retirement/readiness 键后，`driver` 与 `lpc_tests` 仍能构建，失败只剩 3 个用例。

**真正的阻塞点是测试侧结构，不是生产代码**：测试里的断言助手（如
`expect_owner_local_store_complete_contract` 的 19 行断言）在**同一个文件的多个测试里逐字重复**，
内容完全相同。这带两个后果：

1. 无法定点修改：两个副本字节相同，任何“只改这一处不影响另一处”的编辑都无从下手。
2. 改一处就漏一处：如果手工只改一个副本，另一个副本继续断言已删除的键。

因此 U5b 的正确顺序是：**先把测试里的重复断言助手抽到 `test_lpc_support.h` 并只保留一份**
（U7 同类工作），再删生产键，最后删那份唯一助手里已失效的断言。反过来做会被重复副本卡住。

**已加载的失败信息**（供下次直接验证）：去键后失败的是
`TestVmObjectStoreRecordsOwnerMigrationTrace`、
`TestVmObjectStoreReportsPointerBridgeSkippedWhenRecordBridgeRetired`、
`TestVmObjectStoreShardRemovesDestructedObject`；`TestVmObjectStoreTracksPendingCallouts` /
`TracksPendingOwnerMessages` / `VmOwnerHeartbeatTraceRecordsScheduledEvent` 只需把断言
从嵌套 `status_record`/`execution_shard` 改为平铺计数即可通过（这两个改动已在尝试中验证过）。

以下为原计划内容，顺序保持不变：

1. 先删零下游消费者的整个对象存储观测面：`src/vm/internal/object_store.cc`
   （388 处 key）。保留 `vm_object_store_status()` / `vm_object_store_owner_status()`
   这两个 efun **名称与返回类型**（LPC 契约不变），只保留计数类子集（对象数、
   tombstone 数、注册/析构计数），删除全部 `bridge`/`retirement`/`_consistent`/
   `_source` 字段与 `vm_object_shard_contract_mapping` 的 35 键合同表。
2. 再删 `src/vm/internal/owner.cc`（1,552 处）中不属于 51 键的部分；
   保留 51 键所在的 7 个 mapping 的其余计算逻辑不动。
3. 接着 `src/packages/gateway/gateway.cc`（306）、`gateway_session.cc`（202）、
   `src/packages/core/vm_owner.cc`（84）。
4. 同步删除只用于断言被删键的测试（`src/tests/test_lpc_owner.cc`、
   `test_lpc_gateway.cc`、`testsuite/single/tests/efuns/owner_*.c`）。
   **删除测试时必须在提交信息里列出被删断言的键名**，避免"删测试让检查通过"。
5. 同步删除 `VMObjectHandleResolveResult` 的 28 个诊断字段（findings §5.2），
   保留 `object`、`status`。
6. 同步精简 `docs/owner-multicore-api.md` 中枚举迁移状态的段落。

**明确不动**（findings §5.5）：

- `src/packages/core/vm_worker.cc`（≈108 键）。它不是迁移脚手架，而是 Worker 总线
  的功能合同，下游 `worker_bus_d.c` / `async_compute_d.c` 在读。删它需单独课题。
- `src/vm/internal/trace.cc`（27 处）与 `promise.cc`（17 处）：属于真实的运行时
  观测（下游读 `events` 结构），不属迁移脚手架。

**不删**：51 个冻结键、`vm_object_handle` efun（`rewrite` 在用）、
`vm_owner_guard*` 返回值、`vm_owner_task_trace` 的 `events` 结构。

**验证**：

```bash
cmake --build build-refactor-2026-11 --target driver lpcc lpc_tests --parallel 4
ctest --test-dir build-refactor-2026-11 --output-on-failure
cd testsuite && ../build-refactor-2026-11/bin/driver etc/config.test -ftest
python3 tools/docs/check-status-contract.py --check   # U9a 完成后才有
```

判据：测试全绿；`docs/owner-status-contract.md` 登记的 51 个键仍能从对应 efun 取出；
`check-status-contract.py --check` 无输出。

**可删量核算**（实测，脚本 `tools/docs/status-contract.py --dump`）：owner 面
`owner.cc` 846 / `object_store.cc` 192 / `vm_owner.cc` 48，去重 **1,058**，保留 51 →
**删 1,007**；gateway 面 `gateway.cc` 303 / `gateway_session.cc` 62 → **删 365**。
合计 **≈1,372**。不动：`vm_worker.cc` 58、`trace.cc` 7、`promise.cc` 14。

### U5c 把网关状态装配移出主逻辑文件

> 实施记录（2026-11，`243cb784`）：按“可读性”而非“文件位置”落地。装配体直接引用 8 个文件内
> 全局量、私有类型 `GatewayIngressSequenceState` 和 `kGatewayDeferred*` 常量，搬进新文件就得
> 为缩减文件长度而公开这些内部量；实际做法是在 `gateway.cc` 内拆成 6 个命名 builder，
> 主入口回到 14 行。输出键与宏使用次数逐项比对不变。

**不拆 `GatewayRuntimeCounters` 结构体**（200 个字段的引用遍布 gateway.cc；改布局是纯机械大 diff，
不提高可读性）。改做两件事：

1. 把 `gateway.cc` 的 306 行 mapping 装配与 `gateway_session.cc` 的 202 行一起移到新文件
   `src/packages/gateway/gateway_status.cc`（声明放 `gateway.h`），让主逻辑文件回归
   “接收/调度/转发”本身。纯代码移动，字段名与输出不变。
2. 删除只被 `*_for_test()` 读取、且没有任何门禁或 benchmark 使用的计数器。
   判定方法：对每个计数器 grep 非测试消费者；无消费者且不在 `tools/*.sh`、
   `tools/perf/` 中出现的，删除。保留 `owner-scheduler-capacity.sh` 与
   `tools/perf/compile_report.py` 读取的那些。

**验证**：

```bash
cmake --build build-refactor-2026-11 --target driver lpc_tests --parallel 4
ctest --test-dir build-refactor-2026-11 -R 'Gateway' --output-on-failure
tools/owner-scheduler-capacity.sh --build-dir build-refactor-2026-11 \
  --report-dir build-refactor-2026-11/reports/capacity
```

判据：`gateway_status.cc` 只含装配代码，不新增业务分支；容量报告仍能完整产出
（证明保留的计数器仍被真实消费者读取）。

### U5d 下游验收（实施后必做）

用新构建的 driver 在下游 `xiakexing` 的测试实例上跑一次全量 owner 检查：

1. 切换到 `xiakexing` `main`（`f6acdd30e` 或其后代），保持工作区干净。
2. 用本仓库构建的 `driver` 启动测试实例，以管理员身份执行 `ownercheck full`。
3. 期望：输出中不再出现 `FAIL`；`[ownercheck] PASS`、`[ownercheck mailbox] PASS`、
   `[ownercheck scheduler] PASS`、`[ownercheck thread] PASS` 均出现。
4. 把完整输出存到 `audit-evidence/refactor-2026-11/ownercheck-after-keys.txt`。

**已知风险**：memory 记录下游 `room` 域在未修改的 `main` 上会因下游合同缺口失败
（属下游缺陷，不是驱动回归）。因此判定口径是"**与删键前的同一 checkout 结果一致**"：
先在删键前跑一次存为 baseline，删键后再跑一次，只允许出现与本次改动无关的相同失败。

**判据**：两次输出除时间/计数外无差异；新增失败即为回归，必须停并回滚。

## U6 状态机显式化

每个状态机一个提交。统一目标形态：

```cpp
enum class XxxState { kA, kB, kC };
// 唯一转场入口；非法转场返回 false 或 error()，不静默降级
bool xxx_transition(Xxx* x, XxxState next);
// 不变量：任何时刻 x->state 只能由 xxx_transition 修改
```

要求：`state` 字段改为私有/受控访问；所有 `strcmp`/位测试改为 `switch`；
原语义逐条对照并在提交信息列出。**不得改变任何可观察行为**（trace 文本可保留
旧字符串，通过一个 `xxx_state_name()` 映射）。

### 顺序与要点

| 序 | 状态机 | 落点 | 关键约束 |
| --- | --- | --- | --- |
| 1 | SM-06 owner future | `owner_future_store.h/cc` | 删除 `OwnerFutureRecord::state` 字符串，改用 `OwnerFutureState`；`cancelled`/`timed_out` 折叠为终态枚举值 `kCancelled`/`kTimedOut`；`quota_rejected` 保留为完成结果字段。trace/status 输出用 `owner_future_state_name()` 保持原字符串 |
| 2 | SM-05 owner 任务 | `owner.cc`（`admission_state` 等 30 处） | 新增 `enum class OwnerTaskAdmission`、`OwnerTaskTerminal`；`normalize_task_text` 只用于外部传入的 `const char*`（LPC 调用者可传任意字符串），内部路径一律用枚举 |
| 3 | SM-07 重编译四层 | `recompile.h`、`owner_runtime_coordinator.h`、`simul_efun.h` | 不合并四层（它们属于不同所有者），改为在 `recompile.h` 增加一个只读查询 `recompile_phase_of(object_t*)`，让外部不再自行拼状态 |
| 4 | SM-11 协程 | `base/promise.h` | `queued`/`abandoned`/`registered` → `enum class CoroutineState{kParked,kQueued,kAbandoned}` + `registered` 保留（正交事实）。必须保留 `queued ⇒ 不得 free` 的既有约束 |
| 5 | SM-01 用户连接 | `interactive.h`、`comm.cc` | **只动关闭状态，不动位域**。`CLOSING`/`NET_DEAD`/`cleanup_pending` 合并为 `enum class CloseState { kOpen, kClosing, kDead }` 一个字段，并提供 `bool comm_can_write(const interactive_t*)` 作为唯一可写判定；输入模式位与协商位保持原位（它们本质是位集，改成枚举只会把 `|=` 变成数百行 switch）。`telnet_receive_depth`/`output_send_depth` 保持独立（重入计数，不是状态） |
| 6 | SM-09 socket | `socket_efuns.h` | `state` 与 `flags` 并存是协议需要（多标记叠加），不大改；只把互相矛盾的组合加断言并在 `check_valid_socket` 中拒绝 |
| 7 | SM-02 端口表 | `base/internal/external_port.h`、`base/internal/rc.cc` | **不改成 vector**：`external_port[i]` 索引遍布 net 代码，换容器是无效改动。只做两件：(a) 给 `port_def_t` 加 `bool configured`，替掉 `fd == -1 && kind == UNDEFINED` 双条件推断；(b) `port number` 与 `external_port_1` 冲突从"警告后忽略"改为启动失败（错误文本保留）|

### 验证（每个状态机）

```bash
cmake --build build-refactor-2026-11 --target driver lpc_tests --parallel 4
ctest --test-dir build-refactor-2026-11 --output-on-failure -R 'Owner|Future|Gateway|Socket|Context'
cd testsuite && ../build-refactor-2026-11/bin/driver etc/config.test -ftest
```

判据：行为不变（trace/status 字符串经映射后与改动前逐字一致，用改动前后的
`vm_owner_runtime_status()` JSON 对比证明）；sanitizer 不在本单元要求，U6 结束后
统一跑一次 ASan 定向。

**回滚**：每个状态机独立提交，可单独 revert。

## U7 重复实现合并

每项一个提交；每项先写一个"改动前后行为等价"的证明（现有测试 + 新增针对性用例）。

| 序 | 位置 | 动作 | 证明 |
| --- | --- | --- | --- |
| 1 | `vm/internal/base/interpret.cc:1121` `copy_lvalue_range` 与 `:1340` `assign_lvalue_range` | 合并为 `apply_range_lvalue(lval, from, RangeAssignSource policy)`；`enum class RangeAssignSource { kKeep, kAdopt }`。**两处语义差异必须都保留**：(a) `kAdopt` 且数组独占（`fv->ref == 1`）时转移元素、`free_empty_array`；(b) `kAdopt` 结束时释放 `from`（string 用 `free_string_svalue`、buffer 用 `free_buffer`）。两个旧函数保留为一行包装（调用点不动） | 现有 lvalue/range 测试 + 新增"窃取数组元素后源值引用计数"用例（断言 `ref` 与元素身份不变）|
| 2 | `vm/internal/base/interpret.cc:1952` `setup_new_frame` 与 `:2003` `setup_inherited_frame` | 抽出继承链解析：`resolve_inherited_function(int* findex)`（处理 `FUNC_ALIAS`、二分查 inherit、累加 `function_index_offset`/`variable_index_offset`、切 `current_prog`），两个调用点保留自己的差异（`setup_new_frame` 先 `vm_set_inherit_offsets_fast(0,0)`，`setup_inherited_frame` 不重置）| 继承调用用例（含别名、多层继承、`name::fn()`）|
| 3 | `vm/internal/base/array.cc`：`alist_sort`（`:1491`）、`intersect_array`（`:1665`）、`union_array`（`:1825`） | 抽出 `fill_sort_table(src, dst, alloc_tag, free_label)`（归一化：destructed object → `const0u`、非 shared string → intern）。**alloc_tag/free_label 逐字保留原字符串**（`"alist_sort: sv_tab"` / `"intersect_array: sv2_tab"` / `"union_array: sv2_tab"`），否则 DEBUGMALLOC 统计与日志会变。`alist_sort` 的原地分支（`free_string_svalue` 路径）另抽 `normalize_in_place`，不得与临时表版本合并 | 现有 array 测试 + 新增"含 destructed object / 非 shared string / 混合类型"的 union/intersect/sort 用例 |
| 4 | `vm/internal/base/object.cc:345/369`（`case ']'` 与 `case '/','}'`） | 两分支合并为一个条件（仅 `is_mapping` 真值相反） | `restore_object` 用例（含嵌套 mapping/array） |
| 5 | `vm/internal/base/mapping.cc`：`add_to_mapping`（`:857`）与 `unique_add_to_mapping`（`:937`） | 抽出唯一的合并循环，用 `enum class MergePolicy { kOverwrite, kKeepFirst }`；`MAX_MAPPING_SIZE` 计数与 stats 更新只能有一份 | mapping 合并/唯一合并用例 + 超大小 mapping 的 error 路径用例 |
| 6 | `packages/sockets/socket_efuns.cc`：`handle_tls_handshake`（`:112`）与 `handle_tls_server_handshake`（`:139`） | 合并为 `advance_tls_handshake(int fd, bool server_side)`，内部选 `SSL_accept`/`SSL_connect` 与错误文本；状态转换、`S_BLOCKED`、`EV_WRITE` 重驱、失败关闭只留一份 | socket TLS 用例 + `src/tests/ws_smoke.py`；需真实 TLS 对端的部分标 `external-required` |
| 7 | `packages/compress/compress.cc`：`f_compress_file`（`:180`）与 `f_uncompress_file`（`:238`） | 抽出共享的"参数校验 + realpath + 输出路径策略"函数，两个 efun 只保留各自的压缩/解压调用 | 压缩往返用例（短名/长名/不存在/无权限/目标已存在） |
| 8 | `net/ws_ascii.cc:31` `ws_ascii_callback` 与 `net/ws_telnet.cc:37` `ws_telnet_callback` | 抽出共用的回调骨架（`SESSION_CREATED`/`ESTABLISHED`/`CLOSED`/`FREE` + `per_vhost_data`），两个协议只保留 `RECEIVE` 差异与 send 实现；用回调函数指针或 `WebSocketProtocol` 小结构传递差异，不引入继承 | websocket 用例 + `src/tests/ws_smoke.py` |
| 9 | `main_fuzz_compile.cc`/`main_fuzz_restore.cc` | 共用 driver 初始化 | 两个 fuzz 目标各跑 100 次 |
| 10 | `async.cc`/`dns.cc`/`heartbeat.cc`/`call_out.cc`/`socket_efuns.cc` | owner executor 提交样板合并为一个 helper（输入：target、type、key、callback、policy） | `ctest -R 'Owner|Async|Callout|Heartbeat'` |

**已取消**：原 U7-2（`object_store.cc` 的 mapping 双写）。它已被 U5b 删除那 35 键合同表
与迁移字段后自然消失；若 U5b 完成后仍有 ≥20 块的重复，再按 U5b 的结果重新评估，
**不为已经不存在的重复写代码**。

**不做**：`compiler.cc` ↔ `program.cc` 的 10 块重复在判定前不动（findings §4.2 标
"待判定"）；判定标准是两处写入的 program 字段是否同一职责，给出结论再决定。

**验证**：每项完成后重跑重复扫描，记录该项目标的重复块数下降；全 U7 结束时：

```bash
cmake --build build-refactor-2026-11 --target driver lpcc lpc_tests --parallel 4
ctest --test-dir build-refactor-2026-11 --output-on-failure
cd testsuite && ../build-refactor-2026-11/bin/driver etc/config.test -ftest
```

## U8 测试结构

> 状态（2026-11）：**未开始**。已完成前置测量：`test_lpc_owner.cc` 里 8 行的
> `mapping_number`/`mapping_string` 局部 lambda 对逐字重复 77 次（≈616 行），
> 这是拆分前应先回收的重复，也是拆分时每个新文件都要带的样板。
> 阻塞点：77 份文本完全相同，逐条 `edit` 无法定位唯一匹配；这一步应落成一个
> 可审阅的批量替换 diff（同一对 helper 提到 `test_lpc_support.h`，再删局部定义），
> 而不是手工改 77 处。

| 序 | 动作 | 判据 |
| --- | --- | --- |
| 1 | 把 `src/tests/test_lpc_owner.cc`（17,976 行）按现有 fixture 边界拆成 `test_owner_task.cc`、`test_owner_future.cc`、`test_owner_object_store.cc`、`test_owner_recompile.cc`、`test_owner_gateway_contract.cc`（名称按实际内容定），共用 `test_lpc_support.h` 中的 fixture；**只移动代码，不改断言** | 拆分前后 `lpc_tests --gtest_list_tests` 的用例名集合完全相同；`ctest` 结果相同 |
| 2 | 三个 bench（`lpc_vm_bench.cc`/`object_store_bench.cc`/`owner_runtime_bench.cc`）共用装配与报告模块，保留各自 workload | 三个 binary 的 JSON 输出字段不变；CI 中 `build/reports/*_bench_smoke.json` 结构不变 |
| 3 | `test_lpc_gateway.cc`（7,210 行）同样按主题拆分；与 owner 测试共用的 mapping 取值 helper 收敛到 `test_lpc_support.h` | 同上 |

**禁止**：拆分时修改任何断言、测试名或 fixture 初始化顺序。测试名是 CI filter
的输入（`-R 'Owner|Future|Gateway|Context|Object|Socket'`），改名会静默减少覆盖。

## U9 长期门禁

### U9a status key 契约检查

新增 **一个**脚本 `tools/docs/status-contract.py`（≤100 行，只读），两个模式：

```bash
python3 tools/docs/status-contract.py --dump   # 打印当前代码里的字面键（升序）
python3 tools/docs/status-contract.py --check  # 与合同文档比对，不一致则 exit 1
```

实现要点（不要写两套提取逻辑）：

1. 用**容许多行**的正则一次性取出键：
   `add_mapping_[a-z]+\(\s*[A-Za-z_][A-Za-z0-9_]*\s*,\s*"([a-z][a-z0-9_]*)"`，
   跨行用 `[\s\S]` 或先把注释剥离再把换行归一为空格。
2. 合同文档 `docs/owner-status-contract.md` 用固定表格格式（第一列反引号键名），
   脚本只认第一列。
3. 双向比对：代码有文档无 → 失败；文档有代码无 → 失败。
4. **已知盲点写进文档**：变量键（`add_mapping_*(m, key, …)`）无法静态校验，当前 10 处
   调用点（`net` 端口统计、`owner.cc` 的 compute result field、`mapping.cc` 的通用
   helper、flag 名表）在合同文档里列为"豁免站点"并附文件:行。

接入 CI `style-and-lpc` job。验证：故意新增一个未登记字面键→失败；删除→通过；
把一行调用拆成三行→仍被识别（验证多行处理）。

### U9b 重复实现基线棘轮

新增 `tools/style/check-duplication.py`（**≤60 行**，只读），思路保持最小：

1. 归一化（去注释/字符串/字面量/空白）后，对每个文件算"重复 20 行窗口数"
   （窗口哈希出现次数 > 1 的个数）。
2. 基线文件 `tools/style/duplication-baseline.txt` 每行 `路径 计数`。
3. **只比单文件计数**，总量不卡（否则一次合理拆分会被判为变差）。
4. 任一文件计数 > 基线 → 失败并打印新增窗口的首行位置。

排除：`src/thirdparty/`、`*.autogen.*`、`src/tests/fuzz/corpus/`。
验证：基线=当前值→通过；在任一文件里复制 25 行→失败；拆分文件→需人工更新基线
（脚本只报告，不自动改基线）。

### U9c clang-tidy 门禁

在 CI 增加 diff-scoped 检查：

1. 只对 PR 改动的 `.cc/.h` 运行固定版本 clang-tidy（18.1.8，与 clang-format 同版本
   策略），使用配置生成的 `compile_commands.json`。
2. 首轮只启用已校准的检查集：`readability-identifier-naming`、
   `readability-braces-around-statements`、`modernize-use-nullptr`、
   `bugprone-macro-parentheses`、`bugprone-sizeof-expression`、
   `performance-for-range-copy`（与 `src/.clang-tidy` 注释一致）。
3. 报告模式先跑一次，把命中数写入基线；随后改为"不得新增"。
4. 接入后删除 U1b 中"当前无门禁"的注释。

### U9d 测试文件体量上限

不用新脚本：直接在 `tools/docs/check-docs.py` 加一条 15 行的循环断言：

- `src/tests/*.cc` 单文件 ≤ **3,500** 行（U8 拆成约 6 个文件后最大应在 3,000 左右）；
- `testsuite/**/*.lpc` 单文件 ≤ **1,000** 行。

阈值取整到有真实余量的值；如果 U8 拆分后某个文件超限，先调整拆分边界而不是抬高阈值。

### U9e 文档结构检查

扩展 `tools/docs/check-docs.py`：

1. `docs/` 根目录只允许白名单文件（`index.md`、`architecture.md`、`roadmap.md`、
   `coding-standard.md`、`project-scope.md`、`build.md`、合同类文档）；
   新增计划类文档必须落在 `docs/archive/plans/` 或具备 `> 状态：` 头。
2. 计划/证据类文档若 90 天未被修改且状态为 `DRAFT`，列入待归档清单（只报告，
   不自动移动）。

## U10 本轮不做（需单独方案）

以下状态机确实存在，但改动会触及 VM 所有权语义或整个 package 结构，本轮的验证预算不足以
证明行为不变。**不列入本次执行范围**，只记录结论与前置条件：

| ID | 状态机 | 为何推迟 | 开始前必须有的东西 |
| --- | --- | --- | --- |
| SM-03 | 对象生命周期（`O_*` 位 + `destructed_objects` + `ObjectRecord.destructed` + shard tombstone） | 三处描述同一事实，合并要改 object store 与 destruct 路径的写入顺序 | 专用设计文档 + ASan/UBSan soak + 下游 mudlib smoke |
| SM-04 | funptr 生命周期（`lifecycle_state` + 三条链表 + 事务 pin） | 与重编译事务、弱注册表、出生 journal 强耦合 | 重编译事务的失败注入用例集（当前仅有正常路径） |
| SM-08 | parser 全局状态（14 个文件静态量） | 需要把整个 parser 改成会话对象，属包级重构 | 嵌套 parse 的失败用例（先证明现在真的会坏） |
| SM-10 至 SM-19 | Promise/协程已部分覆盖（SM-11）；其余（网关会话、编译器预处理、终端模式、后端调度、外部进程、worker 任务、配置、编译期选项）为中价值 | 单独收益低于本轮其他单元 | 按本方案同一形态逐个推进 |

## 附 A 删除总账

| 层级 | 对象 | 规模 | 风险 | 依据 |
| --- | --- | --- | --- | --- |
| L1 | `.coveralls.yml`、`qodana.yaml`、`fix_permission.sh`、`src/outdated_define.txt`、`.ignore` | 5 文件 | 无 | 无消费者（findings §6.3） |
| L1 | `testsuite/test.o`、`testsuite/data/gateway_net_dead_destruct_user.o` | 2 文件 | 无 | 编译产物 |
| L1 | `audit-evidence/**` 取消跟踪 | 14 文件 | 无 | 目录已本地排除，索引内外不一致 |
| L2 | `lpc_owner_audit_*` + `lpcc --owner-audit` + 相关测试 + 10 个 readiness key | ≈250 行 + 用例 | 低 | 伪能力（findings §5.3） |
| L2 | `vm_worker_bench` / `vm_worker_actor_bench` 两个 LPC efun（保留 C++ 基准 API） | 2 个 efun + 4 行断言 | 无 | 零消费者（findings §5.6） |
| L3 | 迁移脚手架/自证标志类 status key 及其测试断言（owner 面 1,007 + gateway 面 365） | ≈1,372 key | 低（下游只读 51 个，已实测） | findings §5.4 |
| L4 | 重复实现（9 项；原 U7-2 已取消） | ≈3,500 行 | 中（需逐项等价证明） | findings §4 |
| L5 | 20 份历史文档归档（仓库根 2 + docs 根 18），含 `project-knowledge-map.md` 重写为 architecture | ≈800 KB | 低（只移动，内容不改） | findings §6 |
| L5 | `docs/evidence/*.txt/.stderr/.stdout` 中可由 harness 重跑的 fuzz 输出 | 约 40 文件 | 低 | 可复现，非唯一证据 |
| L5 | `docs/evidence/*.md` 设计记录 | 约 15 文件 | 低 | 移入 `docs/archive/evidence/` |

**不删**（明确保留）：

- 所有 LPC efun、apply、parser 契约。
- `vm_owner_runtime_status()` 的计数类键与 U5a 冻结的 51 个键。
- `tools/perf/official_compare/` 的对照 harness 与 corpus。
- `docs/evidence/manifest.schema.json`（CI 依赖）。
- 任何被 CI 引用（`.github/workflows/*.yml`、`tools/*.sh`）的文件。

## 附 B 门禁转化表

| 发现 | 转化为 | 类型 |
| --- | --- | --- |
| ≥1,353 个 status key 无预算 | U9a `check-status-contract.py` | 新增会失败的检查 |
| 文件内重复 116/36/18/13/9 块 | U9b `check-duplication.py` + 基线棘轮 | 新增会失败的检查 |
| `.clang-tidy` 无门禁 | U9c diff-scoped tidy job | 新增会失败的检查 |
| `test_lpc_owner.cc` 17,976 行 | U8 拆分 + U9d 体量上限 | 结构 + 检查 |
| 9 份并行计划文档 | U2 归档 + U9e 文档结构检查 | 删除 + 检查 |
| 伪审计能力 | U3 删除 | 删除实现 |
| `interpret.cc` 两份 lvalue 赋值 | U7-1 合并 | 合并 |
| owner future 双状态表示 | U6-1 枚举化 | 合并 |
| pragma 名称两处定义 | U3 删除测试专用副本 | 删除实现 |
| `.o` 入索引 | U1 删除 + `.gitignore` | 删除 |

## 附 C 验证矩阵与资源预算

| 阶段 | 必跑 | 可选 | 预算 |
| --- | --- | --- | --- |
| 每个代码单元 | 定向构建 + `ctest -R <相关>` + LPC `-ftest:路径` | — | ≤15 min |
| U5 / U6 结束 | 全量 `ctest` + 全量 `-ftest` | ASan 定向 | ≤45 min |
| U7 / U8 结束 | 全量 `ctest` + 全量 `-ftest` + ASan 定向 | UBSan 定向 | ≤60 min |
| 全部结束 | `dev-debug` 全量 + `asan` 全量 + LPC 全量 + `check-format` + `check-docs` + `generate_ebnf` 一致性 | `portable-release` 构建 | ≤120 min |

资源规则（来自 `AGENTS.md` §4.2）：

1. 每次构建前 `df -h /` 与 `free -h`。
2. 普通构建 `--parallel 4`；`MemAvailable` < 3 GiB 用 `--parallel 2`；ASan 固定 2。
3. 同一时刻只运行一个构建目录；构建期间不并行跑 driver 或全量 `-ftest`。
4. 构建失败先查 OOM（`free -h`、`dmesg | grep -i oom`），排除后再看编译错误。

## 附 D 批次与提交计划

| 批次 | 单元 | 预计提交数 | 可独立发布 |
| --- | --- | --- | --- |
| B1 | U0 + U1 | 3 | 是 |
| B2 | U2 | 2 | 是 |
| B3 | U3 | 2 | 是 |
| B4 | U4 | 12 | 是（可拆成 4a/4b） |
| B5 | U5 | 4（含 U5d 下游验收） | 是（删键完即可发布） |
| B6 | U6 | 7 | 是 |
| B7 | U7 | 9 | 是 |
| B8 | U8 | 3 | 是 |
| B9 | U9 | 3 | 是 |

提交信息格式（沿用仓库现状）：`<area>: <what changed>`，正文写"删除/合并了什么、
证据在哪、验证命令与结果"。每个提交只含一个逻辑改动，不混格式化与改名。

## 附 E 未验证与外部依赖

| 项 | 状态 | 说明 |
| --- | --- | --- |
| 51 个保留键的下游使用 | **已实测** | xiakexing `main` `f6acdd30e` 静态核实；`rewrite` `661fdfc60` 不消费（findings §5.4） |
| 删键后下游 `ownercheck full` | **实施后必验** | U5d：删键前后各跑一次，只允许出现与改动无关的相同失败 |
| TTYPE SEND（U4-10）真实对端行为 | **external-required** | 需要 telnet 客户端；本地只能验证字节序列 |
| mysql async 死锁（U4-2） | **unverified** | 需要 MySQL；SQLite 路径不等价 |
| Windows/macOS 行为 | **unverified** | 本轮无该平台；相关改动保持平台中立，由 CI 兜底 |
| 性能不回归 | **unverified** | 只做结构整理，不设性能门槛；若 U6/U7 触及热路径（`interpret.cc`、`object_store.cc`），用 `tools/perf/compile_report.py` 与 `owner_runtime_bench` 做前后对比，不做绝对指标承诺 |
| ASan/UBSan/TSan | **未运行** | U0 之后按验证矩阵执行；本机内存 5.8 GiB 总量/1.5 GiB 可用，sanitizer 构建前必须释放内存 |

## 附 F 最终只读复核清单（交付前自查）

1. `git status` 干净；无构建产物、无日志、无 `/tmp` 路径引用进入仓库。
2. 每个单元的完成判据都有对应命令输出（写入证据目录），不是"看起来对"。
3. 所有"已吸收"的上游条目都有本地文件/函数证据；不能只写"已核对"。
4. 被删的测试都在提交信息里列出，且删除理由是"验证的能力已删除"，不是"测试失败"。
5. `docs/owner-status-contract.md` 与代码中的 key 集合完全一致（U9a 通过）。
6. 重复块基线已随删除下调；不是把基线调高换通过。
7. `generate_ebnf` 后 `git diff` 为空（生成物与源头一致）。
8. `check-docs.py` 通过；归档文档不存在于 docs 根目录。
9. 没有为了通过检查而关闭 FORTIFY、断言、sanitizer 或 owner 检查。
10. 最终报告给出：净行数变化、key 数量变化、重复块变化、各阶段测试通过数、
    未完成项与外部依赖。
11. 下游 `ownercheck full` 的删键前后输出已对比，无新增失败（U5d）。
12. Worker 总线未被改动：`git diff --stat -- src/packages/core/vm_worker.cc` 只包含
    U3b 删的两个 efun，不包含批量删键。
13. 51 个冻结键逐个能在新构建上取到（用一个一次性 `ownercheck full` 输出或
    逐 efun 调用证明），不是只看测试绿。

## 附 G 执行记录（执行时填写）

| 单元 | 提交 | 内容 | 验证结果 | 备注 |
| --- | --- | --- | --- | --- |
| U0 | — | 基线冻结（598/598 CTest，433 文件 -ftest） | 通过 | 证据 `audit-evidence/refactor-2026-11/` |
| U1 | `103dbf62` `852ea7f6` | 仓库卫生 + 凭据 + clang-tidy 状态说明 | check-docs OK | |
| U2 | `8a5437d8` | 20 份历史计划归档；新增 roadmap/architecture；冻结历史检查策略 | check-docs OK (1149) | |
| U5a | `4cb89b28` | 51 键 freeze + `tools/docs/status-contract.py` | `--dump` 1,058 键；`--check` 预期为红直到 U5b | |
| U3 | `524f0027` | 删假审计（含编码层归还）+ 2 个零消费者 bench efun | ctest 597/597；-ftest 433 文件退出码 0 | 两个子单元合并为一次提交（同一测试文件） |
| U4 | `3e01bff9` | 64 位索引/区间截断、TTYPE、输入移除守卫 | ctest 597/597；-ftest 435 文件退出码 0 | 12 项中已吸收 4、已存在 3、部分 2、需设计/拆分 3 |
| U5b-a | `deb7e6bd` | 删除 object-store 迁移脚手架（1058→1008 键） | ctest 597/597；-ftest 434 文件退出码 0 | object_store.cc -258 行；嵌套三份重复副本合为一份 |
| U5b-b | `f7fc15f8` | 删除生产门禁清单与代码自述键（1008→926） | ctest 596/596；-ftest 433 文件退出码 0 | 删 50 键 + `audit_owner_evidence.lpc` 及其 scope 行 |
| U7-a | `5372a0dd` | mapping.cc 两份合并循环、object.cc 两份括号匹配 | ctest 596/596；-ftest 433 文件退出码 0 | 顺带修复覆盖版本漏恢复 `unfilled` |
| U7-b | `288bd809` | deep_inventory 与 deep_inventory_array 共尾 | ctest 596/596；-ftest 433 | 净减 43 行 |
| U7-c | `a90b1fb4` `aee32345` | 区间赋值两份实现合并 + 所有权契约测试 | ctest 596/596；-ftest 434 文件退出码 0 | 净减 108 行；新增 18 断言 |
| U9-a | `4b274d1a` | 状态键契约接入 CI（只在 documented 键消失时报错） | `status-contract --check` 退出码 0 | 之前两边都报错，等于门禁不可用 |
| R-11 | `b3ab58ae` `4ee32b1d` `4cf22dc6` `e202a3d5` | efun 参数 64 位截断收口 | ctest 596/596；-ftest 435 | 12 个 efun + 4 个测试文件 |
| U5c-a | `6f3232bb` `7a1e8d2f` | 网关状态函数：后端块拆出 + 151 个计数器统一 | ctest 596/596；-ftest 435 | 987 → 551 行；gateway.cc −375 行 |
| U5c | — | 网关状态装配移出主逻辑文件 | — | 见 roadmap R-14 |
| U6 | — | 7 个状态机显式化 | — | 未开始 |
| U7-b | — | 剩余重复项（`deep_inventory` 尾巴、interpret.cc 区间赋值等） | — | 见 roadmap R-15 |
| U8 | — | 测试结构拆分 | — | 未开始 |
| U9 | — | 长期门禁（契约/重复/体量） | — | U9a 脚本已就位，未接 CI |
