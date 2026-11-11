# 路线图与当前状态

> 本文是仓库**唯一**的"现在是什么状态、下一步做什么"来源。
> 历史计划与实施记录在 [`archive/plans/`](archive/plans/README.md)，只作历史证据，
> 不得据其判断当前代码行为。
> 维护规则：条目只有三种状态——`verified`（本节有可复现命令且已执行）、
> `unverified`（存在但未在本机验证）、`external-required`（需要本仓之外的平台或环境）。
> 任何"完成"声明必须同时给出命令与结果，否则只能是 `unverified`。

## 1 当前状态

基线：`main` 分支，本轮整理从 `1f3569f6` 开始，方案见
[2026-11 整理方案](refactor-plan-2026-11.md)，问题依据见
[2026-11 问题清单](refactor-findings-2026-11.md)。

| 项 | 状态 | 证据 / 说明 |
| --- | --- | --- |
| Debug 构建（driver/lpcc/lpcshell/lpc_tests/ofile_tests 等） | verified | `cmake --preset dev-debug -B build-refactor-2026-11` + `cmake --build ... --parallel 2` |
| CTest | verified | 598/598 通过（60s），基线记录见 `audit-evidence/refactor-2026-11/U0-ctest.log` |
| LPC testsuite（`-ftest`） | verified | 退出码 0，"Checks succeeded"，433 个测试文件 |
| 格式门禁（clang-format 18.1.8） | external-required | 本机未安装该版本；`tools/style/check-format.py` 明确报错退出，不用其他版本替代 |
| clang-tidy 门禁 | unverified | `src/.clang-tidy` 尚未接入 CI（本轮 U9c 处理） |
| ASan / UBSan | unverified | 本轮整理未运行；改动涉及 VM 热路径时按方案 U6/U7 验证矩阵执行 |
| TSan | external-required | 历史记录为本机工具链 `unexpected memory mapping` |
| Windows / macOS 构建 | external-required | 由 CI 矩阵覆盖，本机无平台 |
| 真实下游 mudlib（xiakexing） | unverified（每次发版前必验） | 本轮 U5d 用 `ownercheck full` 对比删键前后 |
| 上游位置 | `upstream/master` = `38a718f4`；merge-base `277d0b1c`（2026-01-18） | 落后 390 / 领先 796 |

## 2 待办

### 2.1 本轮整理未完成单元

以[方案](refactor-plan-2026-11.md)的执行记录表为准；未打勾的单元即未完成。
不要在本文件复制单元细节，避免第二份待办清单。

### 2.2 本轮不做、需要单独方案

| ID | 事项 | 为什么推迟 | 开始前需要 |
| --- | --- | --- | --- |
| R-01 | 对象生命周期状态收口（`O_*` 位 + `destructed_objects` + `ObjectRecord.destructed` + shard tombstone） | 三处描述同一事实，合并要改 object store 与 destruct 的写入顺序 | 设计文档 + sanitizer soak + 下游 smoke |
| R-02 | funptr 生命周期状态收口（`lifecycle_state` + 三条链表 + 事务 pin） | 与重编译事务、弱注册表、出生 journal 强耦合 | 事务失败注入用例集 |
| R-03 | parser 全局状态改为会话对象（14 个文件静态量） | 包级重构；先要有嵌套 parse 的失败复现 | 失败用例 + 上游/下游兼容评估 |
| R-04 | Worker 总线观测面缩减（`vm_worker.cc` ≈108 键） | 有真实下游消费者（`worker_bus_d`、`async_compute_d`），需逐键确认 | 下游使用清单 + 兼容策略 |
| R-05 | 上游 typed efun 绑定（`EfunArgs`/`Value`/`CallResult`，隐藏 `st_num_arg`/`sp`） | 架构迁移，与本地 owner/promise 路径交叉 | 迁移设计 + 全 efun 回归 |
| R-06 | 上游 hooks（function/efun/net/around 连接点） | 新扩展面，需与 owner admission 一起设计 | 设计文档 |
| R-07 | 上游 fused store（复合赋值/`++`/索引目标的融合写） | 与本地 managed lvalue/async 实现冲突 | 等价性分析 + 性能对照 |
| R-08 | WebSocket TLS 热重载 | 当前明确拒绝，属安全敏感变更 | 设计文档 + 真机握手验证 |
| R-09 | PCRE2 新 API（`pcre_config`/`pcre_info`/`pcre_convert` 等） | 本地 `.spec` 未公开这些 efun，不能把不存在接口伪装成覆盖 | API 决策 + 回归 |
| R-10 | 编译器前端把折叠后的 64 位常量写进 int32 操作数 | 复现：`int big = 1 << 32;`（正确）vs `a[0 .. 1 << 32]`（边界被截断成 1）。VM 侧已修，前端常量发射路径未查 | 复现用例 + 常量发射路径定位（`icode`/`generate`）；上游 #1441 也未覆盖此路径 |
| R-11 | 上游 #1441 的 efun 参数批次与 `terminal_colour()` 崩溃修复 | **已完成**：allocate/allocate_buffer/test_bit/next_bit/member_array/replace_string/read_file/read_buffer/write_buffer/strsrch/fetch_class_member/store_class_member 均已改（`b3ab58ae` `4ee32b1d` `4cf22dc6` `e202a3d5`）；`terminal_colour` 崩溃本身未复现，未改 | 继续扫同类：任何 `int x = ...u.number` 都是可疑点 |
| R-12 | 新增“浮点截断为 int”编译警告（上游 #1426） | 这是新增的用户可见诊断，会改 mudlib 编译输出，不属 bug 修复 | 先确认下游是否接受新警告，再实施 |
| R-13 | owner 状态面收口（`vm/internal/owner.cc` 875 个键 → 契约内的实算字段） | 已完成生产门禁清单 / 代码自述 / 重构计划三类（50 键，含删除 `audit_owner_evidence.lpc`）；剩余 875 键中大半是 readiness gate 表与真实计数器 | 按 mapping 构建函数逐组判定；gate 表保留（它们是真实准入条件） |
| R-14 | gateway 状态面收口（`gateway_status_internal()`） | **进行中**：987 → 421 行。已拆出后端切片块（`6f3232bb`）、151 个计数器与 36 处延迟采样改单令牌宏（`a4bcf55c` `e6ac5c7f`）、房间输出投影块（`5adba526`） | 剩余 421 行里主要是按子系统交错的真实计数与阈值；继续拆需先重新勘测连续段 |
| R-15 | `deep_inventory` 与 `deep_inventory_array` 共享 55 行计数→分配→收集→收缩尾巴，只差计数循环 | **已完成**（`288bd809`）：抽出 `deep_inventory_build()`，两个入口只剩各自的计数循环 | — |
| R-16 | `copy_lvalue_range` / `assign_lvalue_range` 两份区间赋值（152 + 129 行） | **已完成**（`a90b1fb4` 合并 + `aee32345` 补所有权契约测试） | — |
| R-17 | 剩余重复项 | U7 已完成的 4 项之外，需重新做结构扫描才知道还有什么；8 行以上的重复块在 `array.cc` 仍有 ~112 处命中（含有意保留的对称分支） | 按块分组后逐项判定，不批量删除 |
| R-18 | SM-06 owner future 三表示 | **已完成**（`caecf828`）：只存枚举，字符串仅在线输出与 API 边界出现 | — |
| R-19 | SM-05 owner 任务准入/失败状态仍是字符串 + `strcmp` 与子串匹配 | 30+ 调用点，且 `owner_task_failure_code()` 靠 `find("rejected")` 做分类；转枚举需要先把分类规则改成显式表 | 先列全部取值与分类映射，再换类型；不要用字符串前缀判断冒充分类 |
| R-20 | 已完成的两组重复合并 | `socket_efuns.cc` TLS 握手（`7398e170`）、`compress.cc` 两个 efun（`96ad487c`） | — |
| R-21 | 剩余重复 | `interpret.cc` 帧解码：**已完成**（`9f88ce89`）；三个 benchmark 的环境装配：**已完成**（`82f89903`，含三份已漂移的写报告逻辑） | — |
| R-25 | 重构后的 sanitizer 复核 | **未执行**：本系列改过引用计数与生命周期密集代码（区间赋值、deep_inventory 共尾、`&`/`|` 临时表、帧解析），但它们只用 Debug 构建 + testsuite 验证过 | 下次在内存充足时跑 `cmake --preset asan` + 定向测试；本次可用内存仅 2 GiB，按项目规则未启动 ASan 构建 |
| R-26 | 被丢弃任务的 `failure_code` 为空 | **已定位未改**：`drop_owner_executor_callback()`（`src/vm/internal/owner.cc:3737`）只写 trace 的 state，不动 `task.admission_state`，所以任务级状态里 `failure_code` / `drop_reason` 仍报“无失败”，实际任务已被丢弃 | 修要么把 drop 状态写回 task（需与 trace 同一锁，否则与主线程读状态竞态），要么让任务级状态改读最后一条 trace；两者都要先确定下游是否读这两个键 |
| R-22 | `array.cc` 三份排序临时表 | **已完成**（`7e4add49`）：`&`/`|` 改调 `alist_sort()`，两份内联循环删除 | — |
| R-23 | `intersect_array`/`union_array` 零 LPC 覆盖 | **已补**（同一提交）：27 断言，含多重集语义与 ref>1 不被原改 | `&` 取较小重数、`|` 取较大重数且保留左操作数顺序——旧代码如此，现在有测试锁住 |

### 2.3 上游待吸收

见[方案](refactor-plan-2026-11.md) U4 清单；其中 9 个提交仍需 `git show` 判定本地是否等价。

## 3 如何验证

最小验证集（按改动类型选择，细则见 `AGENTS.md` §5）：

```bash
# 构建（同一时刻只用一个构建目录；可用内存 < 3 GiB 时用 --parallel 2）
cmake --preset dev-debug -B build-refactor-2026-11
cmake --build build-refactor-2026-11 --target driver lpcc lpc_tests --parallel 2

# C++ 回归
ctest --test-dir build-refactor-2026-11 --output-on-failure

# LPC 回归（必须从 testsuite/ 运行）
cd testsuite && ../build-refactor-2026-11/bin/driver etc/config.test -ftest

# 文档与门禁
python3 tools/docs/check-docs.py
python3 tools/style/check-format.py --all-native   # 需要 clang-format 18.1.8
```

## 4 文档索引

| 文档 | 用途 |
| --- | --- |
| [architecture.md](architecture.md) | 模块地图、主调用链、不可破坏合同 |
| [coding-standard.md](coding-standard.md) | 代码规范（唯一风格来源） |
| [owner-status-contract.md](owner-status-contract.md) | LPC 可观测状态键的冻结契约 |
| [project-scope.md](project-scope.md) | 项目范围与默认交付 |
| [owner-multicore-api.md](owner-multicore-api.md) | owner 多核 API 合同 |
| [multicore-runtime-v4.md](multicore-runtime-v4.md) | 运行时硬化基线 |
| [multicore-production-gate.md](multicore-production-gate.md) | 运行时验收口径 |
| [recompile-object-v2-design-2026-08.md](recompile-object-v2-design-2026-08.md) | 重编译事务设计 |
| [archive/plans/](archive/plans/README.md) | 历史计划与实施记录（只读） |
