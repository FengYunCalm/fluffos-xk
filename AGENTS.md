# FluffOS_XK 项目规则

## 构建与磁盘（WSL2）

- 本仓库在 WSL2 下开发；**WSL2 的 vhdx 磁盘在大量写入时占用飙升**（曾因全并行构建导致宿主机磁盘占满、WSL 被迫重启）。
- 构建时**不要用 `-j$(nproc)` 全并行**，固定限制并行度（普通构建 `-j4` 或 `-j8`，ASan 见下节）；同一时刻只跑一个构建目录。
- 构建前查磁盘余量（`df -h /`）；磁盘紧张时优先删除可重建的旧 `build-*` 目录（均可由 CMake 重建，仓库不依赖其内容）。

## 构建与内存（WSL2）

- WSL2 可用内存随宿主机配置浮动（本机长期紧张：pi/Java 等常驻进程占用可观），**构建前必须 `free -h` 实测**；可用 < 2Gi 时先清理（删旧 build-* 目录见磁盘节；另停无关进程）再构建；构建期间不并行跑 driver/-ftest/bench 等内存大户。
- **LTO 是内存爆掉的根因**：`ENABLE_LTO` 默认 ON（src/CMakeLists.txt 顶部 option），非 Debug 构建启用 `CMAKE_INTERPROCEDURAL_OPTIMIZATION` 后由 CMake 生成 `-flto=auto`，LTO1 并行数 = nproc，链接期全程序 IR 同时驻留（数 GB 峰值）。
- **dev 构建一律关 LTO**：优先 `cmake --preset=dev-debug`（CMakePresets.json 已内置 `ENABLE_LTO=OFF`），或手写 `cmake -S . -B build-xxx -DENABLE_LTO=OFF`（已配置的目录用同一命令重配）；LTO 只留给 prod/CI 构建（`--preset=portable-release`）。
- **ASan 构建固定 `-j2`**：大 TU（如 `src/compiler/internal/grammar.autogen.cc`）单文件编译即吃 GB 级内存，-j4 在内存紧张时仍会爆；普通构建 -j4。
- 构建失败/无输出时先查 `free -h` 与 `dmesg | grep -i oom`，确认不是内存打爆再排查编译错误。

## 验证粒度

- 验证手段与对象匹配：行为改动用针对性测试，文案/文档/注释改动用一致性检查（grep/读文件）；**默认不全量**——轻量改动不跑全量门禁（lpc_tests 全量、-ftest 全量、ASan 只留给实质代码改动），只有实质代码改动才跑全量构建 + 对应测试。
- 验证必须定量、定向：只跑与改动相关的测试（`--gtest_filter` 定向、`-ftest:路径` 定向、单目标构建），用数字证据（通过/失败计数、耗时、diff 行数）报告；全量门禁只在用户明确要求或阶段验收需要时跑。
- 全量 -ftest 必须从 `testsuite/` 目录跑（`cd testsuite && ../build-sync/bin/driver etc/config.test -ftest`）；从仓库根跑会因 mudlib 目录错误空转，`grep -c "Check Failed"` 得 0 不代表通过。

## 架构速览（冷启动）

- `src/main.cc` 只是 5 行 shim；CLI 解析与启动流程全在 `src/mainlib.cc` 的 `driver_main()`——手写 argv 循环、无 getopt，且有多处独立扫描（--version 提前退出、--tracing 预扫、-d 预扫、尾部 switch），新增 flag 需同步多处。
- **`-f*` 参数是 mudlib 功能而非 driver 功能**：`mainlib.cc` 的 `case 'f'` 把 `-f<arg>`（如 "test"）原样转发 `master::flag()`（APPLY_FLAG）；`-ftest` 的实现在 `testsuite/single/master.c` 的 `flag()` → `/command/tests`（`-ftest:路径` 的定向参数即此 arg），"Check Failed" 输出在 `testsuite/single/simul_efun.c`。找 ftest 实现去 mudlib，不要在 src/ 里 grep。
- 构建目录约定：`build-sync`（日常同步）、`build-asan`/`build-tsan`（sanitizer）、`build-prod`（发布）；预设见 `CMakePresets.json`（dev-debug / portable-release / asan / ubsan / tsan）。
- `testsuite/etc/config.test` 的 `mudlib directory` 是相对路径 `./`，driver 的 cwd 决定 mudlib 根；`src/CMakeLists.txt` 顶部集中了所有编译选项 option。

<comet-ambient-resume>
<!-- Managed by Comet. Edits inside this block may be replaced by comet init/update. -->
<!-- Contract: comet.resume_probe.v2 -->

## Comet Ambient Resume

在这个仓库中，开始处理需要改动或调查的任务前，如果可能存在活跃 Comet workflow，把当前用户请求传入只读探针：`comet resume-probe . --stdin --json`。

- 如果用户通过宿主明确调用任意 Comet Skill（例如 `@comet`、`/comet`、`@comet-native` 或 `/comet-hotfix`），显式调用优先于本恢复协议；不要运行 resume probe，直接进入被调用的 Skill。
- 只信任返回的 `workflow`、`skill` 和 `entrySource`；它们只由项目配置或无配置兼容回退决定。不得扫描或切换另一套 workflow。
- 如果 probe 返回 `auto_resume`，简短说明选中的 active change，并进入 `nextCommand` 指向的永久入口。不要把状态命令当作恢复入口直接推进。
- 如果 probe 返回 `ask_user`，只问一个简短问题并等待用户回复。
- 如果当前请求未明确调用 Comet Skill，且 probe 返回 `out_of_scope` 或 `none`，不要进入 Comet workflow。
- 如果配置或状态无效且没有 `nextCommand`，停止并报告原因；不要猜测另一个 workflow。
- 不能只因为存在 active change 就把无关任务挂到该 change。Native 的未提交改动由 Native 入口检查，不由探针自动归因。
</comet-ambient-resume>
