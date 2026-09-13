# 全量留档处置方案（2026-09-13）/ Full Backlog Disposition Plan

> 本方案覆盖留档的**全部**内容：上游候选池、E/T/S/L 系列遗留、参考分支价值点、
> 已知限制、external-required 项。基于三路并行深度调研（pr-1276 逐项审计 25 条、
> pr-1237 逐项审计 26 条、E/T/L 系列文档-代码交叉核对）生成。
> **本文档只含方案；执行按批次另行授权。**

## 0. 调研统计总览

| 判定 | 数量 | 说明 |
| --- | --- | --- |
| NEEDS-PORT（真缺陷/真缺失） | **33** | 见 P0-P3 批次 |
| 已完成但未收口 | **9** | E3 v2、L7、C-S2、E1、S16 升级本体等——**文档滞后于代码** |
| COVERED（本地已有等价保护） | 12 | 无需动作，记录判定依据 |
| N/A | 8 | 本地无此功能/文件 |
| 大项序列 | T3 四阶段（含 E4） | 唯一的 L 级别工作 |

**关键发现**：文档严重滞后于代码。`docs/upstream-sync-evidence-2026-08.md:212-216`
仍列 E3 v2/T3/E4 为 deferred，但 E3 v2（含 master/simul_efun 重载、三段 commit、
变量迁移事务）与 B-S1/B-S2/B-S3 **已全部实施并验证**（`recompile.h:55-176`、
`efuns_main.cc:3441-3536`、`docs/evidence/e3-v2-phase1-simul-efun-reload.md`、
gtest 425/425 + ftest 444/444）。S16 的 libwebsockets **本地已是 4.5.8**
（thirdparty CMakeLists PATCH_NUMBER=8）。L7 压测本体已完成
（`recompile_stress.c` 1000 次重载 + 三 bench 基线 + evidence 留痕）。

---

## 执行状态（2026-09 收口）

| 批次 | 状态 | 证据 |
| --- | --- | --- |
| P0 | 已完成 | 各单元的 C++/LPC 回归测试与提交记录 |
| P1 | 已完成 | ops/math/foreach/bit 回归 + 全量 ftest |
| P2 | 已完成 | compress/嵌套释放/深度限制/load_object 回归 + ASan |
| P3 | 已完成 | parser/lex/member_array/remove_action/ws 等回归 + live ws smoke |
| P4 | 已完成 | S16 live ws/wss smoke（docs/evidence/s16-ws-wss-smoke.md）、T1/T2/T4/E1/E3-v2 合同测试 |
| P4-7 (E2 fuzz) | blocked-env | 需要 afl-clang-fast，本机不存在；corpus 就绪 |
| P5 | 已完成 | E3 v2/L7/C-S2 状态改写 + lpcshell 前置修订 |
| P6 | 已完成 | T3.1-T3.4（docs/evidence/t3-*.md）+ L7 master 轮次（docs/evidence/l7-master-reload-rounds.md） |
| P7 | 已完成 | num_varargs 契约、libevent 跨线程 self-pipe、LSan 归零（P7-1/2/3 提交） |
| P8 | external-required（记录已产出） | docs/external-required-readiness-2026-09.md |

## 1. P0 批次：远程/普通 LPC 可触发的内存损坏（7 项，约 1.5 天）

### P0-1 PORT_TYPE_ASCII 缓冲溢出【最高危·远程可触发】
- **缺陷**：`comm.cc:1064` 在栈上分配 1MB `buf[MAX_TEXT]`；ASCII 分支
  `:1066/:1111` 以 `text_space = sizeof(buf)` 为上限，`:1197-1198`
  `memcpy(ip->text + ip->text_end, buf, num_bytes)` **不检查 `ip->text` 剩余
  空间**——`text_end > 0` 时溢出 `interactive_t` 相邻字段。对照 TELNET 分支
  `:1076` 的正确写法（`sizeof(ip->text) - text_end`）。
- **修复**：ASCII 分支改为与 TELNET 相同的剩余空间计算；1MB 栈缓冲改为堆
  分配（`interactive_t` 生命周期内持有或按连接分配）。
- **测试**：向 ASCII 端口分多次发送合计超过缓冲的数据，断言无溢出且消息完
  整（可用 TSan/ASan 构建验证）。
- **验收**：ASan 全量 0 新报告；telnet 路径回归不受影响。

### P0-2 async efun callback 损坏 + 悬垂 args
- **缺陷**：`interpret.h:63-71` 的 callback union 中 `f.fp` 与 `f.str` 共用；
  `process_efun_callback`（`interpret.cc:575-608`）接受 T_STRING callback 并把
  `args` 指向 VM 栈；`async.cc:854/890/929/961` 无条件 `cb->f.fp->hdr.ref++`
  ——string callback 时**改写共享字符串内存**；bound args 在 pop 栈后悬垂，
  异步回调时读已弹出的栈（`interpret.cc:613-615`）。
- **修复**（对齐上游 pr-1276 "reject string callbacks and heap-copy bound args
  in async efuns"）：async 四个 efun 入口（`f_async_read` 等）校验 callback
  必须为 T_FUNCTION，否则 `error()`；`process_efun_callback` 对 bound args
  做堆拷贝（`allocate_array` + 逐元素 `assign_svalue_no_free`），回调完成后释放。
- **测试**：`async_read("/f", "name_of_function")` → 稳定报错；带默认参数绑定
  的回调 → 异步触发时值正确。
- **注意**：改动 `process_efun_callback` 影响所有使用它的包（call_out 等），
  需全量回归。

### P0-3 reload_object 自毁 + free_object null 解引用
- **缺陷**：`object.cc:2067-2071` 的 null 检查后 `:2071` 仍无条件解引用；
  reload 流程 `call_create`（`object.cc:2287`）中 LPC `destruct(this_object())`
  会把栈槽清零（`interpret.cc:5647-5660`），`efuns_main.cc:3374`
  `free_object(&(sp--)->u.ob)` 对 null 解引用——崩溃链完整。
- **修复**：`free_object` 的 null 守卫提前到函数入口；`reload_object` 在
  `call_create` 后重查 `obj->flags & O_DESTRUCTED`；`f_reload_object` 相应防御。
- **测试**：create() 中自毁的对象执行 reload_object() → 稳定报错不崩溃。

### P0-4 pcre_match is_string 错位 + pcre_replace 零校验
- **缺陷**：`pcre.cc:185` 在弹出可选 flag 参数**之前**计算
  `is_string = ((sp-1)->type == T_STRING)`——4 参数调用时 `(sp-1)` 是 flag
  （T_NUMBER）→ 误走 array 分支把 pattern 的 `char*` 当 `array_t*` 用（`:216`）
  类型混淆；`f_pcre_replace` 的 pattern/subject/replacements 三个主参数零运行
  时校验（`:333-335`；泄漏已由 `:329` DEFER 修复）。
- **修复**：`is_string` 改为按弹完可选参数后的实际 subject 槽位判定；入口
  增加 T_STRING/T_ARRAY 类型校验。
- **测试**：4 参数 `pcre_match`、非 string subject、replacements 含非 string
  元素。

### P0-5 move_object 在 lazy try_reset 后使用已销毁对象
- **缺陷**：`simulate.cc:1902-1904` 的 `try_reset(dest)` 运行 LPC reset（可
  destruct 任意对象），随后 `:1906-1943` 直接写 `dest->contains`、跑
  `setup_new_commands`，无 `O_DESTRUCTED` 重查——幽灵对象与链表损坏。
- **修复**：`try_reset` 后对 `dest` 与 `item` 重查 `O_DESTRUCTED`，已销毁则
  提前返回；`f_move_object` 入口补 `o2` 检查。
- **测试**：对象的 reset() 中 destruct 目标后 move_object。

### P0-6 dwlib replace_objects 栈溢出
- **缺陷**：`dwlib.cc:739-751` `char buf[2000]` + `strcpy(obname)` +
  `strcat(master apply 返回串)` 完全无界。
- **修复**：`snprintf` 或 `std::string` 重写拼接。注意 dwlib 为 opt-in 包，
  验证需 `__PACKAGE_DWLIB__` 构建。
- **测试**：master `object_name()` 返回超长串时 replace_objects。

### P0-7 matrix transform 元素类型
- **缺陷**：`matrix.cc:52-54` 的 #1247 MATRIX-1 仅校验 `size >= 16`；
  `:77-78` 等 7 处直接写 `matrix->item[i].u.real` 不查元素类型——元素为
  T_STRING/T_MAPPING 时 double 位覆盖 union 指针，释放时按坏指针 free。
- **修复**：7 处 size 校验旁增加逐元素 `type == T_REAL` 检查。
- **测试**：含 string 元素的矩阵调用各 transform。

---

## 2. P1 批次：语义绕过与 UB（4 项，约 0.5 天）

### P1-1 set_bit 符号检查绕过
`efuns_main.cc:2410-2417`：上界检查只挡正大数，`:2414` 截断 int 后 `:2415`
才查 `< 0`——传 `-4294967291` 截断后为 5 绕过。修法：读取 `u.number` 后
以 64 位值先查 `< 0`；**同步排查 clear_bit/check_bit** 同型问题。

### P1-2 shift 运算越界（4 处）
`ops.cc:394`（f_lsh）、`:408`（f_lsh_eq）、`:934`（f_rsh）、`:948`（f_rsh_eq）
对负数或 ≥64 的计数是 UB。上游最终语义是 **mod 64 掩码**（"mask shift counts
mod 64 instead of erroring"）——注意这是**行为变更**（原先负数左移是 UB），
采用上游语义并带测试；`<<=`/`>>=`（f_lsh_eq/f_rsh_eq）同批。本批与已完成的
subtype 修复（7a471202）同文件，合并提交。

### P1-3 math.cc sentinel 与 printf
`math.cc:264/288/310` `error("…%d…", (total + INT_MAX))` 把 LPC_FLOAT 传给
`%d`（UB）；sentinel 方案（`:198/:216/:227/:239/:303` 的 `-INT_MAX` 系列浮点
相等比较）与上游"改独立返回标志"不一致；`norm()`（`:179`）仍用
`sp->u.arr->size`，与 `:212` vector_op 的 #1247 MATH-1 修复不同步。三项一起
改：printf 格式修正、sentinel 改为独立输出参数、norm 统一用参数 `a`。

### P1-4 foreach 迭代计数 16 位截断
`svalue.h:51` `unsigned short subtype`；`interpret.cc:3258`（数组
`sp->subtype = arr->size`）、`:3312/:3351`（mapping/next 递减）——元素
>65535 回绕。**修法**：计数迁出 subtype（参照字符串 foreach 已用 32 位
`codepoint index` 的模式，`interpret.cc:3344`），或对 size > 0xFFFF 的容器
在 F_FOREACH 入口报错。与 P3 的同项合并执行。

---

## 3. P2 批次：DoS 加固与健壮性（6 项，约 1 天）

### P2-1 compress 解压无界增长
`compress.cc:265-280` do-while 无输出上限（zip bomb 无限 realloc）。加累计
输出上限（对照 `__MAX_BUFFER_SIZE__`）。错误路径泄漏 #1247 已修，无需重复。

### P2-2 深嵌套容器释放的 C 栈成本
`svalue.cc:124-180 → array.cc:168-183 → mapping.cc:126-156` 纯递归释放无上
限；`reclaim.cc:16-32` 的 MAX_RECURSION=25 只覆盖 GC 扫描。修法：对容器深
度超阈值（如 500）改显式栈迭代释放（保持 T_ERROR_HANDLER 等类型语义），或
复用延迟释放队列。上游同型修复为 "bound the C-stack cost of freeing deeply
nested arrays/mappings/classes"。

### P2-3 dump_trace 重入门闩
`trace.cc:53-174` 无重入保护：`:111/:147/:164` 的 `svalue_to_string` 触发嵌
套 error → 递归 dump_trace。加 `static bool in_dump_trace` 门闩（重入直接返
回）。

### P2-4 stat flag 先存后用
`efuns_main.cc:2697` 先 `check_valid_path`（LPC apply 可写栈），`:2733` 才读
flag——已是垃圾。入口先把 flag 拷入局部。

### P2-5 optimize()/i_generate_node() 递归深度
`generate.cc:57-238`、`icode.cc:361-428` 递归无深度计数（本地 `kMaxIfExprDepth`
只覆盖预处理）。采用上游最终方案：**按表达式深度限制**（不是对象尺寸）。

### P2-6 load_object 缓冲容量不一致
`simulate.cc:651` 四个 400 字节栈缓冲 vs `object.h:23 MAX_OBJECT_NAME_SIZE
2048`，`:771` 已用 2048——统一提升，消除合法长路径被错误拒绝。

---

## 4. P3 批次：功能移植（pr-1237，16 项，约 2-3 天）

| # | 项 | 缺陷/缺失位置 | 要点 |
| --- | --- | --- | --- |
| P3-1 | parser UTF-8 分词 | `parser.cc:127-128`（`uisalnum` C locale）、`:3251-3298` 逐字节截断可切多字节 | 复用 `strutils.cc:546 u8_egc_split`（ICU 簇迭代） |
| P3-2 | unique_mapping unwind 泄漏 | `mapping.cc:685-689`（FIXME 自认 garbage） | 翻译循环 try/catch，异常路径释放 result 的 key 与临时 map |
| P3-3 | load_object 计数顺序 | `simulate.cc:653` valid_read 先于 `:657` 计数 | `+1`/`-1` 对移到 check_valid_path 之前 |
| P3-4 | 参数不匹配警告带函数名 | `compiler.cc:1180/1183` 两处 yywarn 无名 | 参照 `:1728-1731` 先例插入 name |
| P3-5 | ::-lookup 原型下钻 | `compiler.cc:1003-1006` 命中原型即 `return 0`，跳过 `:1023-1035` 继承搜索 | 原型条目 continue 本层继续下钻 |
| P3-6 | whashstr → std::hash | `hash.cc:5` + 使用点 `stralloc.cc:74`、`lex.cc:4540`、`add_action.cc:80`、`preprocessor.hpp:23`（独立副本需同步） | 确认 `& (size-1)` 桶语义不变 |
| P3-7 | member_array flag 4 | `efuns_main.cc:1076-1081` 有 flag 槽位但只消费 `&1/&2` | 增补 `&4` 函数谓词分支 |
| P3-8 | request_clean_up() efun | 全仓无此符号 | 新增 efun：提前触发目标对象 clean_up 调度（与 O_WILL_CLEAN_UP/deadline 机制衔接） |
| P3-9 | remove_action 函数指针形态 | `add_action.cc:594-620` 只按名字匹配 | 扩展 `string\|function`，V_FUNCTION 按 funp 相等匹配 |
| P3-10 | db：MySQL date/time + double finalize | `db.cc:750-797` switch 缺 DATE/TIME/DATETIME/TIMESTAMP；`:973-993` finalize 后未置 `results=0` | switch 增补按字符串返回；close 后置空 |
| P3-11 | websocket：拒绝 ws 上 MCCP + wss 输出 rest 计算 | `telnet.cc:536-539` 对 ws 会话同样协商 COMPRESS2；`ws_ascii.cc:144-145` `rest = new_numbytes - numbytes` 为**负值**（子代理发现的相邻 bug，上游修 4 字节丢失） | ws 会话跳过 COMPRESS2 协商并回 WONT/DONT；rest 改为 `numbytes - new_numbytes` 并做 wss 输出突发回归 |
| P3-12 | restore_context 值栈下溢 | `interpret.cc:5713` `pop_n_elems(sp - econ->save_sp)` 无下溢钳制（DEBUG 崩溃/release UB） | 对 `sp < econ->save_sp` 钳制并上报（对齐 `promise.cc:789-804` 的既有纪律） |
| P3-13 | strutils emoji 零宽 | `strutils.cc:492-544` 无 FE0F/FE0E、1F3FB-1F3FF 分支 | 修饰符区间计 0 宽，或改走 `u8_egc_split` 按簇计宽 |
| P3-14 | pluralize -ff + terminal_colour 显示列 | `contrib.cc:1409-1419`（-ff 得到 bluves）、`:924-949` 逐字节回绕 | -ff 例外；回绕改用 `u8_width` 显示宽度 |
| P3-15 | display preload progress 选项 | `vm.cc:60/66` 无条件 debug_message，rc 无选项 | 新增 rc 项包住输出 |
| P3-16 | stat 测试基建补齐（可选） | 上游 `inherit_pct_filename_warn.lpc` 等本地无 | 低价值，跟随 P2-5 一起补 |

---

## 5. P4 批次：conditional 验收收口（6 项，约 0.5-1 天）

| 项 | conditional 验收内容 | 现状 | 剩余 |
| --- | --- | --- | --- |
| E1 cycles | copy 相邻回归 + sanitizer | cycles.cc 迭代 DFS + 3 个 LPC 测试在位；sanitizer 全量 444/444 留痕在库 | 仅差把 conditional 升 accepted 的证据记录（S） |
| T1 os_env | owner-worker 拒绝的 C++ 证据 | `contrib.cc:3126-3132` 有 main-thread 检查；LPC 测试自认"by code review" | C++ 合同测试：owner worker 调 `get/set_os_env` 稳定拒绝（S） |
| T2 set_clean_up | deadline-sweep 集成冒烟 | `backend.cc:614-622` sweep 在位；LPC 测试仅参数形状 | deadline 到期→clean_up 被调→one-shot 回退的定向验证（S） |
| T4 lpcc | CLI 表驱动矩阵 | `main_lpcc.cc:85` argc 精确校验在位 | argc 0-6 表驱动 + 未知 flag + batch 退出码合同（S） |
| S16 lws smoke | live ws/wss smoke + ASan | **本地已 4.5.8** + default-vhost 修复（`websocket.cc:126-136`）+ vendor manifest；testsuite 无 ws 测试 | 真实 ws/wss 握手收发断开 + ASan 零报错 + evidence 留痕（S） |
| E3 v2 dropped-name | 删除函数名的运行期报错路径 | `simul_efun.cc:375` 报错在位，零测试覆盖 | 合同测试：删除函数名后旧调用点稳定报错（S） |

**E2 fuzz**（第 7 项）：bounded AFL smoke 需 `afl-clang-fast` 环境；corpus
目录（`src/tests/fuzz/corpus/`）不存在需建最小集。无环境时按纪律标
BLOCKED-env，不得以自校准替代。**估计 S-M（环境受限）**。

---

## 6. P5 批次：已完成项的文档收口（约 0.5 天）

1. **E3 v2 状态改写**：`upstream-sync-evidence:212-216` 与
   `recompile-followup-plan` 状态表改写为"v2 已实施"（引用
   `docs/evidence/e3-v2-phase1-simul-efun-reload.md` 与 B-S3 四件套测试）。
2. **L7 状态改写**：本体已完成（recompile_stress + 三 bench + evidence 留痕）。
3. **C-S2 交叉引用对齐**：evidence `:389` 与 optimization-master-plan C.3 互引。
4. **lpcshell-prerequisite-plan §1.1 修订**：compile_arena（C-S1 产物）已可
   复用为 ScratchArena 基座，评估从"不能复用"修订为"消费侧扩展"。

---

## 7. P6 批次：大项序列（T3 四阶段 + L7 扩展，约 5-10 天）

**T3 lpcshell（前置：先完成 P5-4 修订）**：

| 阶段 | 内容 | 门禁 | 估计 |
| --- | --- | --- | --- |
| T3.1 | ScratchArena：基于现有 `compile_arena` 扩展会话级持有 + `Binding` 嵌套释放 + 跨周期存活统计 | gtest 单测/嵌套/生命周期 + ASan | M |
| T3.2 | 结构化诊断：`Diagnostic` 对象模型（message/snippet/ranges/fixits/expansions）+ yyerror/yywarn 接入 + 格式兼容开关 | 全量回归无回归 + 字段 golden 对比 | M-L |
| T3.3 | 诊断渲染栈（=E4）：`read_source_line`（memchr 优化版直接带入）+ emit_snippet + 宏展开级联 | golden 20 组 + 编译耗时基线 | M |
| T3.4 | lpcshell REPL 独立二进制：非交互/脚本模式、多行续入、错误恢复、`#` 调试命令 | REPL 交互合同 + ASan/TSan | M-L |

**L7 扩展**（S）：压测契约加 master 为目标的轮次（配合 E3 v2）。

**明确不移植**：#1237 的 shadow/virtual 生存性语义——本地 v1/v2 均判定拒绝
（special-design §11），本地实现了上游没有的 owner quiescence + 变量迁移事务，
整体移植反而降级。

---

## 8. P7 批次：已知限制处置（约 1-2 天）

### P7-1 num_varargs 显式化
现状：thread-local（`interpret.cc:147`）由 codegen 写入、恰好 6 个 opcode
消费（`:3254/:3270/:3376/:3411/:4154/:4251`）。**方案**（三层，递进）：
1. 提取 `consume_num_varargs()` helper，6 个消费点统一调用；
2. 定义 `constexpr` 消费 opcode 清单 + `static_assert`/Debug 巡检
   （drain 后 num_varargs != 0 即 fatal）；
3. 显式协议（node/操作数携带）**不做**——大改高风险，helper + 清单已把
   "新增 opcode 漏复位"的静态风险消除大半。

### P7-2 libevent 竞态（self-pipe 方案）
现状：`async.cc:258` 工作线程调 `add_walltime_event` → `event_new` 与主线程
事件循环竞争。**方案**：标准 self-pipe——backend 初始化时注册一个读端 pipe
的 level event；工作线程完成后 `write(pipe, 1)`；主线程 event 触发 →
`check_reqs()`。把"创建 libevent event"完全收回主线程。改动面：
`async.cc` 工作线程侧 2 处调用点 + `backend.cc` 新增 pipe event。风险中，
需 ws/async 全量回归。

### P7-3 ASan 存量泄漏（8760B/19）
simul_efun 模块（`simul_efuns_prepare simul_efun.cc:276/277`、
`get_simul_efuns :190`）+ 测试侧 SaveSimulTable（`test_lpc.cc:24955`）。
方案：shutdown 路径补 simul_efun 表释放；测试侧 fixture 改 RAII。验收：
LSan 全量归零（与 P4 的 S16 smoke 一起做）。

---

## 9. P8 批次：external-required（环境依赖，方案仅供执行时使用）

1. **Docker daemon**：`docker-smoke.yml` 已就绪，缺可运行 Docker 的执行环境
   ——用户侧启用 Docker Desktop/WSL2 集成后手动触发一次。
2. **生产容量压测**：原 `capacity-300-player-pair` runbook 已删（git 历史可
   恢复），需目标环境（端口/证书/客户端机器人）；建议先做 30 人冒烟再上 300。
3. **跨平台矩阵**：CI 增加 `macos-latest`、`windows-latest`、
   `ubuntu-24.04-clang-system` 三个 job（当前矩阵 GCC/Clang Debug/RelWithDebInfo
   已绿）；Windows 需先解决 `bash cp -f` 安装步骤的 CI 适配。

---

## 10. 执行顺序与批次依赖

```text
P0（7 项内存损坏）──────────────┐
P1（4 项 UB，P1-4 并入 P3-16）──┤
P2（6 项 DoS）──────────────────┼─→ 每批独立提交 + 全量门禁
P3（16 项功能移植）─────────────┘
P4（6 项验收收口）─→ P5（文档收口，依赖 P4 结论）
P7-3 ASan 存量 ────→ 与 P4 S16 smoke 同场执行
P6（T3 四阶段）────→ 依赖 P5-4 修订；T3.3 依赖 T3.1/T3.2
P7-1/P7-2 独立
P8 按环境可用性随时插入
```

总工作量估计：**约 12-18 人日**（P6 占 5-10 天；P0-P5/P7 合计 7-8 天）。
建议每批一个 PR/commit 系列，批间跑全量门禁。

## 11. 统一门禁

每个批次合并前必须全绿：
1. Debug 构建 + 完整 LPC 套件（0 失败）；
2. `lpc_tests` 全量（当前基线 457/457）；
3. ASan 全量套件（0 报告；P7-3 完成后 LSan 也要求 0）；
4. 触及并发路径的批次加 TSan 定向 + 全量；
5. 新修复必须带 LPC 或 gtest 回归测试（命名与放置风格照 `#1247` 先例）；
6. 证据落盘 `docs/evidence/`，文档状态同步改写（杜绝"代码先行文档滞后"）。

## 12. 风险与回滚

- **P0-2** 触及 `process_efun_callback` 全局路径——独立提交、可整体 revert。
- **P0-1/P3-11** 触及网络路径——ASan+TSan 双跑，wss 突发回归必做。
- **P1-2 mod 64** 是行为变更（原 UB），CHANGELOG 明示。
- **P2-2** 释放路径改造保持 T_ERROR_HANDLER/T_LVALUE 语义，ASan 双跑。
- **P6** 每阶段独立授权与原子提交（照 T3 计划的门禁纪律）。
- 全部批次遵守既有回滚纪律：单批单 revert，`git bisect` 友好。
