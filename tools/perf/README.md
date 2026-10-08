# 性能基准与证据校验

`src/tests/bench_compile.cc` 用真实 `compile_file()` 编译固定语料。它不测量 loader、
inherit/retry 或 mudlib 启动的端到端耗时。语料生成器为 `gen_compile_corpus.py`；
仓库内的 `corpus/` 包含固定种子的 100 个文件。生成器会写文件，不属于只读检查。

## 运行

先按[主方案 U00/U17](../../docs/project-modernization-plan-2026-10.md)构建独立的 Release
目标 `bench_compile`。基线与候选使用相同工具链、依赖、优化选项和本工具版本；
不把旧测量代码与新测量代码的差异算作运行时优化收益。

以下路径是示例；`--evidence-dir` 必须尚不存在，JSON 输出放在本轮私有目录中。
监督条件、环境和退出合同见 [testsuite 工具说明](../testsuite/README.md)。

```bash
/usr/bin/python3 -B tools/testsuite/run-targeted.py \
  --tool build-organize-release/src/tests/bench_compile \
  --evidence-dir /tmp/compile-run-001 --timeout 120 -- \
  --corpus /absolute/private/corpus --rounds 20 --json /absolute/private/report.json
/usr/bin/python3 -B tools/perf/compile_report.py /absolute/private/report.json
```

`--rounds` 接受大于零的十进制整数；不再将非法或非正数静默转换为一次运行。
`--corpus` 相对调用目录解析。历史行为保留：相对 `--json` 路径在测试 mudlib 目录解析，
因此建议始终传绝对路径。输出不能与语料文件同路径、硬链接或符号链接到同一文件。

成功返回 0；预热或测量中的编译失败返回 1；参数、输入身份、JSON 打开/写入/关闭或
stdout 写入失败返回 2。失败编译仍记录各次结果，但不能签收为成功性能证据。
失败输出可能留下不完整文件，不能仅凭 JSON 文件存在判断成功。

## schema 2

旧 JSON 字段和已有 stdout 行保持原义。新增字段不要求旧消费者升级；签收新性能证据时
必须使用 schema 2 校验器。旧证据不自动补字段，也不改写已保存的原始数据。

| 字段 | 含义 |
|---|---|
| `corpus_files` | 固定顺序的路径、字节数和 SHA-256；运行前后内容必须一致 |
| `warmup_success` / `warmup_failures` | 每个文件的预热结果及失败数 |
| `round_samples` | 按发生顺序保存每轮总耗时及每个文件的耗时、成功状态；文件索引对应 `corpus_files` |
| `successful_compiles` / `failures` | 测量阶段成功数、失败数，不含预热 |
| `lifecycle` | 同一完整语料的首尾轮次窗口比较；窗口各为 `max(1, rounds / 10)` 向下取整；单轮为 `null` |
| `operation_secs` | 全部单文件编译操作的样本数、median、p95、p99 |
| `round_secs` | **旧字段**：排序后的轮次耗时，不是原始时序 |
| `per_file_secs_last_round` | **旧字段**：最后一轮按文件顺序排列的耗时 |
| `degradation` | **旧字段**：最后一轮不同文件的首尾 10% 耗时比，仅描述输入分布，禁止作为生命周期退化门禁 |

分位数从排序副本取索引 `min(n - 1, floor(n * p))`，median 的 `p` 为 0.5。
stdout 的 `round_secs` 分位数仍描述轮次；`operation_secs` 描述单次编译操作。
样本数量必须随数字报告。少量样本的 p95/p99 只是样本统计，不证明尾延迟稳定性；
7 组 A/B 用于评估运行间噪声，也不能当成 7 个操作样本来证明尾延迟。

`arena` 只记录 compile arena 的 chunk malloc、保留块/字节、周期字节和重置数，
不记录进程总分配次数。`peak_rss_kb` 是进程高水位；不可用时为 -1。
RSS 包含 VM 初始化与记录存储，不等于编译器独占内存或 arena 保留字节。

校验器拒绝缺字段、非有限数、缺失或失败样本、错误计数和不能从原始样本重算的摘要。
它不独立证明文件哈希对应真实输入，也不认证二进制：必须结合 runner 的运行身份、
成功退出和私有语料核验。前后哈希不是 OS 沙箱，测试期间不得有其他写入者修改语料。

## 回归测试

```bash
/usr/bin/python3 -B tools/perf/test_compile_report.py
/usr/bin/python3 -B tools/perf/test_compile_report.py \
  --benchmark build-organize-release/src/tests/bench_compile \
  --evidence-root /tmp/compile-selftest-001
```

第一条只验证统计合同。第二条同时运行真实工具：异构语料、时序与摘要、JSON 转义、
失败编译后的恢复、非法轮数、输出打开/写入失败、stdout 失败及三类文件别名。
`--evidence-root` 必须尚不存在，测试保留原始日志和输入，不自动清理。
`/dev/full` 测试需要相应设备；跳过项不能作为已覆盖的失败路径。

## explode 分类扫描与耗时

`src/tests/bench_explode.cc` 调用真实 `explode_string()`，不复制其实现。分别使用 ASCII
`x` 和 Unicode `中`，以 `|` 连接 1k、10k、50k 个 token。每次调用都核对数组长度和
每个 token 的字节。只在私有进程内调整数组上限，不改变 driver 配置合同。

`measure_explode.py` 复用定向 runner 的隔离输入、进程回收和身份记录。基线与候选须从
各自源码树调用同版本脚本，使用相同工具链、依赖和构建选项。证据目录必须尚不存在；
正式证据使用持久目录，不将 `/tmp` 作为唯一副本。

```bash
cmake --build build-explode-release --target bench_explode --parallel 4
/usr/bin/python3 -B tools/perf/measure_explode.py \
  --binary build-explode-release/src/tests/bench_explode \
  --evidence-dir build-explode-evidence/time-001
```

Release 目录须事先配置。每个进程预热一次，随后记录五次顺序调用的耗时；计时包含
结果分配，不包含结果核验和释放。`report.json` 保留六组原始样本、输入字节数及核验结果。
少量样本不证明游戏端到端性能或尾延迟；前后交错运行多组，单列 ASCII 与 Unicode 的变化。

分类工作量使用独立 GCC 构建，不给生产代码增加计数器。配置时补充本机依赖路径：

```bash
mkdir -p build-explode-profile-tmp
export TMPDIR="$PWD/build-explode-profile-tmp"
cmake -S . -B build-explode-coverage \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_LTO=OFF -DMARCH_NATIVE=OFF \
  '-DCMAKE_CXX_FLAGS=--coverage -fprofile-dir=%q{TMPDIR}/gcov' \
  '-DCMAKE_EXE_LINKER_FLAGS=--coverage -Wl,--undefined=__gcov_reset'
cmake --build build-explode-coverage --target bench_explode --parallel 4
/usr/bin/python3 -B tools/perf/measure_explode.py \
  --binary build-explode-coverage/src/tests/bench_explode \
  --coverage-build build-explode-coverage \
  --evidence-dir build-explode-evidence/work-001
```

运行时的 `TMPDIR` 由 runner 指向每例私有目录；不放宽环境白名单。GCC 在该目录写
profile，脚本保留原始 `.gcda`、对应 `.gcno` 和 gcov JSON。链接参数强制引入静态
libgcov 中的重置函数，否则弱引用不能保证函数被链接。该方式已用 GCC/gcov 13.3 验证。

计数模式在预热后重置覆盖计数，仅测一次；汇总各编译单元中 `all_ascii()` 的逐字节
循环计数，写入 `scanned_bytes`。缺失计数、进程失败或结果不符均拒绝签收；计数模式的
耗时不用于 Release 性能比较。此方法依赖当前分类循环的源码和 GCC profile 格式；
修改扫描实现或工具链时须重新核验计数含义。

## object store 扫描量与锁时间

`object_store_bench` 的默认 32 对象模式和 `object_store_bench_v1` 字段保留。
新增规模模式只在 Linux 上使用链接包装，观察实际读锁和普通 C++ `operator new`；
生产 driver 不链接这些观察器。`measurement.py` 供两个运行时测量脚本共享监督器和
GCC 行计数读取，不复制运行时算法。

```bash
cmake --build build-organize-release --target object_store_bench --parallel 4
/usr/bin/python3 -B tools/perf/test_object_store_bench.py \
  --binary build-organize-release/src/tests/object_store_bench \
  --evidence-dir build-object-store-evidence/contracts
/usr/bin/python3 -B tools/perf/measure_object_store.py \
  --binary build-organize-release/src/tests/object_store_bench \
  --objects 1000 --operation status --rounds 3 \
  --evidence-dir build-object-store-evidence/time-001
```

`--objects` 可选 1000、10000、50000；`--operation` 可选 `current`、`missing`、`stale`、
`path`、`status`。省略选择器时遍历全部组合；不传选择器可能运行数小时，不能把整个批次
放进短时外层超时后再从头重跑。每个 probe 的监督上限由 `--timeout` 指定，默认 900 秒。
先估计最大规模的准备与清理时间，再固定预算；监督上限不改变 driver 的评估限额。

每例初始化真实 VM，创建指定数量的对象，再销毁其中 10% 并保留 tombstone。
实际 record 数还包括 VM 启动对象，必须读取输出，不能把请求数量当成全表大小。
夹具在私有进程中启用 audit 模式、提高数组上限至所需数量，并为每次独立创建/销毁
重置原评估预算。准备、报告和清理不计入操作耗时。有效/陈旧 handle 与 path 重复使用
同一个探针对象，不代表游戏混合负载。

Release 先预热一次，再记录顺序样本。`elapsed_ns` 包含完整 API 调用；
`read_lock_ns` 只计获取读锁后到解锁前的持有时间，不包含等待获取锁的时间。
每次调用必须完整释放唯一读锁。`cpp_allocations` 不是 malloc/VM 分配总量，
不能据此宣称进程总分配减少。GNU `/usr/bin/time` 的独立 JSON 记录整个进程的峰值
RSS（KiB）和墙钟时间，包含准备、报告及退出；RSS 不能当成临时索引独占内存。

工作量使用独立 GCC 构建，配置方式同上面的 explode 覆盖构建，但链接选项须同时包含
`-Wl,--undefined=__gcov_reset,--undefined=__gcov_dump`。给脚本传入
`--coverage-build <目录>` 后，脚本强制只执行一次冷调用，在调用前重置计数、调用后立即
落盘；后续状态核验和销毁不混入扫描计数。输出分别记录按 ID 的全表比较、最终全表核验
和临时索引插入，不能把三者混成一个数字。

性能签收前固定工具链、输入、CPU affinity、地址随机化条件和预热规则，先做 A/A
噪声校准，再冻结阈值。执行至少 7 组交错 A/B。尾延迟每类操作至少需要 10000 个样本；
少量慢诊断样本仅报告已观测分位，不能宣称稳定 p95/p99。原始报告、进程记录和二进制
哈希是验收依据；外层任务中断时只恢复已完整验证的记录，不能接纳半写报告。
