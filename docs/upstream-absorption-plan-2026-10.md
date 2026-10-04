# FluffOS_XK 大改优化与上游吸收方案（2026-10）

状态：已进入实施阶段。P0、I01–I09、I13、F、G、D1 的 Linux/WSL2 本地原子单元已按正文记录完成；I15/P1 集成、D2、H、P2/P3 及 Windows/macOS/Alpine/Docker/远端 CI 仍按各节保留 `unverified` 或 `external-required`。

本文同时承担实施记录和剩余执行合同；动态证据只以实际命令、二进制、日志和环境为准，不把未执行的平台或发布流程冒充通过。下游 mudlib、生产实例、发布和部署仍是独立边界。

## 1. 基线、证据与结论

### 1.1 固定基线

- 本地：`fe163d67f40cef6d445f076e4a2d565799cd4627`，`main`。
- 上游：`795eb371f1af522280738f454b8fc81afe231f1c`。
- `v2026.1001.0` 的标签对象为 `9c33f463727d714b70e74650813c5fac83c35c10`，剥离标签后等于上述上游提交；前一轮审计通过 `git ls-remote` 核实；本轮沿用固定目标，不刷新上游引用。以后 master 前进不自动改变本文目标。
- 本地 `upstream/master` 仍为 `f1c242b1`，不用于代替固定目标。
- 开始审计时已有两处外部改动：`src/vm/internal/base/object.cc`、`testsuite/single/tests/efuns/save_object.c`，合计 23 行增加；本文不取得这两处改动的所有权。
- 本方案原本即为未跟踪文件。普通 `git diff` 看不到其内容，必须单独检查，不能把空 diff 当作方案通过审计。

证据分级：

1. **本地源码确认**：当前文件确实具有/缺少指定分支或合同，不等于运行验证通过。
2. **上游补丁确认**：固定提交的代码、测试或提交说明；上游复现不是本机复现。
3. **待实施复现**：需要编译、运行或目标平台才能证明的行为，保持 `unverified`。
4. **设计未闭合**：不允许让执行者自行猜测实现；先完成对应设计出口条件。

历史方案只能提供线索。例如 [旧吸收方案](upstream-absorption-plan-d07e7641-735bd31f.md) 中“本地不存在 Promise”已不适用于当前 `src/vm/internal/base/promise.cc`。不修改历史记录来伪造当前证据。

### 1.2 审计发现及处置

| 问题 | 证据与影响 | 本版处置 |
|---|---|---|
| trim 根因、范围错误 | `strutils.h` 的三个函数都按字节集合裁剪；package 只是包装。不是 `trim` 与两端函数的 Unicode 空白判定不一致 | A 改共享实现，保留默认六个 ASCII 空白，修复显式 charset 的码点语义 |
| 漏掉 foreach unwind 修复 | `134bebd7` 涉及普通 `catch`；本地 `do_catch`、控制帧及 error context 没有保存临时栈计数 | 增加 F，不能以 Promise 架构不同为由排除 |
| 错称 index op= 已覆盖 | 本地 `grammar.y` 的 `unknown_dynamic_rhs` 只识别 call_other/evaluate，不识别 mapping/array index | 增加 G，含 grammar fallback 生成链 |
| 错称 websocket TLS 已覆盖 | `sys.cc::f_sys_reload_tls()` 明确拒绝 websocket | H 已补 staged SSL_CTX、旧握手/SNI 与失败原子设计；实现/平台仍未验证 |
| 混淆 formatter 与编译器 | `2c272875` 只修改 `tools/lpc-syntax` 与其文档 | 排除理由改为本地无对应 formatter，而非编译器 source-order 合同 |
| PCRE 替换合同混淆 | 现有数组形式替换首次匹配的捕获组，元素是字面文本；模板 `$1` 是上游新增 string 形式 | D 分为引擎迁移与 API 扩展，不把 `$1` 解释强加给数组形式 |
| PCRE 依赖迁移不全 | 漏掉 `Dockerfile`、真实 `docs/build.md`、中文构建说明及旧 CMake cache | D 列出真实路径和缓存处理，覆盖 Alpine 静态链接 |
| PCRE 缓存和回调描述不实 | `pcre_cache()` 仅查询，无清空接口；callback 同步执行；匹配参数不都属于编译缓存 key | 删除虚构合同，补共享 code 的重入存活性验证 |
| 函数身份不足 | 上游按定义文件、原始槽位、同源函数出现序号迁移；同一 owner 可重复继承同名函数 | E 不采用 owner+名字作为完整身份 |
| E 的事务设计不能直接执行 | reserve 不保证 unordered_map 插入无分配；prepare 快照不覆盖初始化中新生/释放的 funp；rollback 会恢复 generation | §8 已补状态机、出生 journal、引用账本和 owner 边界；I15 先约束事务中不可回滚的生命周期变更 |
| 错误验证入口 | 本地无 `trim`/`pcre_match`/`pcre_config` 同名测试；`lpcc --version`、`symbol --help` 返回用法错误 1 | 改为实际对象、精确返回码及非空测试门禁 |
| “只读审计”会写入 | cmake、CTest、LPC 会生成文件/启动进程；`ctest -N` 的 discovery 也可能执行程序 | 将静态复核与动态验证彻底分开 |

### 1.3 推荐次序与架构边界

本轮不是重新发明 driver：保留 LPC VM、对象存储、owner executor、编译 arena、主线程 IO adapter 与现有重编译事务。先补安全与验证基础，再增加能力，最后根据数据优化热点。

依赖顺序以 §12 为准：P0 测试可信度 → P1 生命周期/事务/网络基础 → P2 A/B/F/G/D1 → P3 E/D2/H → P4 资源与性能验收。Windows C 可在平台可用时独立验证。依赖图不是并行构建许可；普通构建 4 并行、ASan 2，并且一次只运行一个构建目录。

架构约束：

- 同一 owner 仍串行；普通 legacy LPC 不获得新的后台执行许可。跨 owner 不共享普通 funptr、array/mapping、SSL 对象或 PCRE match_data。
- live object 查询继续使用 owner-sharded object store。新函数索引附着对象，不另建全局对象目录，不缓存失效 ObjectHandle。
- 重编译复用 OwnerRuntimeCoordinator 的 OPEN/CLOSING/FROZEN 状态机；不增加第二把 admission 锁，不把任意 LPC/IO 纳入可回滚事务。
- TLS/连接/日志调度保留 main-required；PCRE callback 保持同步；Promise 继续经过 main/owner admission 恢复。
- 一类状态只有一个权威来源：函数逻辑身份、TLS policy、PCRE options、测试入口不得分别复制多套配置。新增 helper 是为已有重复风险收口，不是兼容旧新两套运行时。
- [项目范围](project-scope.md)、[owner API](owner-multicore-api.md)、[运行时合同](multicore-runtime-v4.md)、[重编译设计](recompile-object-v2-design-2026-08.md)继续约束实施；过时的文字由实现单元同步修订，不能反过来忽略当前代码。
- 没有真实预算/性能证据的容量和提速结论列为待测，不以“大改”为理由引入 lock-free、换分配器、全局重写或公开发布工程。

## 2. 38 个上游提交的处置账本

区间为 `v2026.0901.0..795eb371f`，共 38 个提交。表中“历史记录”不表示本次重新运行过其回归；未给出当前证明的项目不标为“已覆盖”。

| 上游提交 | 当前处置及证据入口 |
|---|---|
| `9c673da7` | 历史吸收项：`src/packages/core/replace_program.cc`；保留本地 no-op 合同，不重复整块移植 |
| `f3ab999d` | 历史吸收项：`src/compiler/internal/lex.cc`；不重做预处理器 |
| `134bebd7` | **F：补入候选**；普通 catch 的临时栈计数恢复与 Promise 无关 |
| `e0d6cca2` | 历史吸收项：disabled-package efun 诊断；按已有生成链保留 |
| `c80ce56f`, `1da7a0b6`, `8ad78675` | npm 依赖更新，非本次运行时吸收范围；不据此宣称依赖无漏洞 |
| `24211e79` | 历史吸收项：ASCII/EGC explode 路径；不与 trim 重写混合 |
| `9bce345a`, `11f23e20` | 本地 `grammar.y::unknown_dynamic_rhs` 已有动态调用分支；相关历史测试不证明 index RHS 已覆盖 |
| `a781b918` | 历史吸收项：file-info 表；不改变本地 program ABI |
| `faccd243`, `ac9f6191` | 历史吸收项：include_list、splice-first；沿用本地编译 arena/记录生命周期 |
| `b1745c82` | 本地不存在同名 shadowing 页面，**不是已覆盖文档**；B 仅更新现有 inherit 文档 |
| `7bcd22eb` | 不直接移植 stack-lvalue ABI；当前本地已有 managed lvalue/async，不能再用“不存在 Promise”作为理由 |
| `b8dd5866`, `7c808c8b`, `735bd31f` | 不整包移植 external/Promise；已有本地 handle/cancel/owner 实现。未做逐分支等价证明，不标全覆盖 |
| `070724d1` | 历史 total_lines 项；不纳入新增编码范围 |
| `63e8a0cb` | 源码确认 `define_new_variable()` 持久化 `VAR_TEMP(n)->type`，已有 NOSAVE 修正；不触碰外部 save_object 改动 |
| `d41f000f` | 拆分：本地 `open_spawn_pipe/open_stdin_pipe` 已检查 CLOEXEC/nonblocking 失败；Promise 与 icode 的其余 hunk 留待针对性核验，不以整条提交已覆盖结案 |
| `6e56d4e9` | **H：websocket TLS 热更新未实现**；本地仍明确拒绝 |
| `f1c242b1` | 上游网站/LLM 文档组织，不替换本地 VitePress 体系 |
| `3b505bef` | A：三个 trim 函数的 Unicode charset |
| `968205b3` | B：保留 private inherited slot |
| `2c272875` | formatter 的 include/inherit 保序；不属于本地 compiler 修复 |
| `d140ec28` | D1/D2：引擎与新增 API 分开吸收 |
| `65f6a8b8`, `71061e6d`, `7c65e186`, `37e7d897` | formatter/npm 更新，不纳入本次代码范围 |
| `932e9309` | **G：index RHS 的复合赋值类型修复尚缺** |
| `5894e7e1` | C：Windows backward-cpp 退出生命周期 |
| `03354159` | D 的错误清理/类型检查参考；不得覆盖本地已有 PCRE 回归 |
| `5270e7d6` | D1 的构建依赖切换 |
| `7c9f17d9`, `a73b03ef` | 上游临时 CI 诊断、QWEN 清理；本地无对应吸收收益 |
| `795eb371` | E：有价值，但需本地事务、owner、重复继承适配设计 |

上游原文可用固定链接核对：[trim](https://github.com/fluffos/fluffos/commit/3b505bef1)、[private slot](https://github.com/fluffos/fluffos/commit/968205b37)、[PCRE2](https://github.com/fluffos/fluffos/commit/d140ec28c)、[named funp](https://github.com/fluffos/fluffos/commit/795eb371f)。离线 Git 对象是本次主要证据，网页不可访问不授权绕过网络安全策略。

## 3. 实施前边界与验证规程

### 3.1 真正只读的进入检查

从仓库根目录运行；仅检查，不保存新文件、不安装依赖、不刷新 Git 引用：

```bash
export GIT_OPTIONAL_LOCKS=0 GIT_NO_LAZY_FETCH=1
git -c core.fsmonitor=false status --short --branch
git -c core.fsmonitor=false rev-parse HEAD
git -c core.fsmonitor=false show --no-ext-diff --no-textconv --stat --oneline 795eb371f1af522280738f454b8fc81afe231f1c
git -c core.fsmonitor=false diff --no-ext-diff --no-textconv -- src/vm/internal/base/object.cc testsuite/single/tests/efuns/save_object.c
git -c core.fsmonitor=false diff --no-ext-diff --no-textconv --check
# no-index 在文件有差异但无空白错误时也返回 1，必须同时检查诊断。
python3 - <<'PY'
import subprocess
r = subprocess.run(['git', '-c', 'core.fsmonitor=false', 'diff', '--no-ext-diff',
                    '--no-textconv', '--no-index', '--check', '/dev/null',
                    'docs/upstream-absorption-plan-2026-10.md'],
                   capture_output=True, text=True)
if r.returncode not in (0, 1) or r.stdout or r.stderr:
    raise SystemExit(r.stdout + r.stderr or 'git diff check failed')
PY
df -h /
free -h
```

- Git 对象缺失时停在该证据项；`fetch` 会写对象库/引用，不是只读。经授权取得对象后重新核对固定 SHA。
- 上游 master 变化不自动升级目标；任何扩大目标区间都要更新本账本。
- 记录所有现存改动的内容/哈希，不仅是文件名。发现新的并行改动先确认归属；同一文件不得猜测哪些行可以回退。
- 在 E 确需修改 `object.cc` 时，先解决该文件既有改动的所有权，不允许“只按路径暂存”。本方案不授予处理那两处外部改动的权限。
- PCRE2 依赖用 `pkg-config --modversion libpcre2-8` 检查；没有 pkg-config 信息不等于库不存在，最终以 D1 的 CMake 编译/链接探针为准，不能用 `|| true` 把缺依赖当成功。
- 可用内存小于 2 GiB 不构建；不自行停止未知进程或清理旧构建。安装包、删除缓存、拉取依赖另属有写入行为。

### 3.2 动态验证有副作用，不属于审计命令

行为修复先增加最小回归并在本次源码基线构建上看到预期失败，再改实现并复测。编译失败、测试路径不存在、package 未启用、0 个用例或 timeout 都不是有效的 fail-first。D2 等新增 API 先用可在旧版本正常编译的注册/可用性断言证明缺失，再接功能用例；不能用“未知 efun 导致夹具编译失败”替代反例。测试基础设施、纯生成或测量单元分别用发现集合差异、源/产物一致性或基线数据验证，不强造运行时缺陷。

基本构建步骤（实施阶段，从根目录；用到哪个预设才配置哪个）：

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug --target driver --parallel 4
# 仅该单元需要 C++ 用例时追加 lpc_tests 目标。
# 仅内存相关单元追加；不要直接 build 未配置的 asan 目录。
cmake --preset asan
cmake --build --preset asan --target driver --parallel 2
```

构建目录已有时先核对 generator、编译器、sanitizer、LTO、package、Bison 和 PCRE 缓存。不删除或重配置归属不明的目录；冲突时使用新目录且后续命令保持同一目录。不要默认编译 lpcc、全部目标或运行全仓 CTest。

定向 C++ 测试先用 `lpc_tests --gtest_list_tests --gtest_filter=...` 确认非空，再执行同一过滤器。列举也会执行二进制，不归入只读审计。`DriverTest.TestLPC_FunctionInherit`、`DriverTest.TestRecompileMigrationInitFailureRollsBack`、`DriverTest.TestSimulEfunReloadCreateFailureRollback` 是现有真实名称。

### 3.3 LPC 定向运行与隔离

`testsuite/etc/config.test` 默认全地址固定端口，测试会写日志、生成/删除夹具并关停**本次测试进程**。不直接用于运行实例。`tools/testsuite/run-isolated.sh` 只支持整套/ports-only；它并未复制 mudlib，不能传入虚构的 `--test` 参数，也不能拿它宣称共享测试文件完全隔离。

以下为后续实施用的**有限定向运行模板**（Linux/WSL2；会创建并保留临时目录、启动新 driver）。本次只做语法与入口静态核对，未运行模板，不能把它作为已验证的测试工具。每次从仓库根启动，`CASE` 使用不带扩展名的真实 LPC 对象；更新夹具后重建沙箱，不复用旧拷贝。

```bash
CASE=/single/tests/efuns/package_trim DRIVER=build-dev-debug/bin/driver python3 - <<'PY'
import os, pathlib, re, shutil, stat, subprocess, tempfile
root = pathlib.Path.cwd()
case = os.environ['CASE']
assert re.fullmatch(r'/single/tests/[A-Za-z0-9_/]+', case), case
assert any((root / 'testsuite' / (case.lstrip('/') + ext)).is_file()
           for ext in ('.c', '.lpc')), 'missing test object'
driver = (root / os.environ['DRIVER']).resolve(strict=True)
work = pathlib.Path(tempfile.mkdtemp(prefix='fluffos-absorb-'))
mud = work / 'testsuite'
print('evidence directory:', work, flush=True)
skip = shutil.ignore_patterns('.run-isolated*', 'log', 'trace*.json')
def regular_tree(path):
    mode = path.lstat().st_mode
    assert stat.S_ISDIR(mode), ('not a real directory', path)
    names = os.listdir(path)
    ignored = skip(str(path), names)
    for name in names:
        if name in ignored:
            continue
        entry = path / name
        mode = entry.lstat().st_mode
        if stat.S_ISDIR(mode):
            regular_tree(entry)
        else:
            assert stat.S_ISREG(mode), ('symlink or special file', entry)
# 源目录必须静止；不读取符号链接指向的下游数据或特殊设备。
regular_tree(root / 'testsuite')
regular_tree(root / 'src/www')
shutil.copytree(root / 'testsuite', mud, ignore=skip)
(mud / 'log').mkdir(exist_ok=True)
# config.test 的静态资源路径相对 testsuite；复制而不链接回工作区。
shutil.copytree(root / 'src/www', work / 'src/www', ignore=skip)
cfg = (mud / 'etc/config.test').read_text()
for pattern, replacement in (
    (r'^mud ip : .*$', 'mud ip : 127.0.0.1'),
    (r'^port number : .*$', 'port number : 0'),
    (r'^external_port_2: websocket .*$', 'external_port_2: websocket 0'),
    (r'^external_port_3: websocket .*$', 'external_port_3: websocket 0'),
    (r'^external_port_4: telnet .*$', 'external_port_4: telnet 0'),
):
    cfg, count = re.subn(pattern, replacement, cfg, flags=re.M)
    assert count == 1, pattern
(mud / 'etc/config.absorb').write_text(cfg)
log = work / 'driver.log'
try:
    with log.open('wb') as out:
        result = subprocess.run([str(driver), 'etc/config.absorb', '-ftest:' + case],
                                cwd=mud, stdout=out, stderr=subprocess.STDOUT,
                                timeout=180, check=False)
except subprocess.TimeoutExpired:
    raise SystemExit('TIMEOUT; evidence retained at ' + str(work))
output = log.read_text(errors='replace')
assert result.returncode == 0, (result.returncode, log)
assert 'C> ' + case in output and 'Checks succeeded.' in output, log
assert 'test skipped' not in output.lower(), log
assert not re.search(r'AddressSanitizer|UndefinedBehaviorSanitizer|ThreadSanitizer|runtime error:', output), log
print('PASS:', case, '; inspect full log:', log)
PY
```

这个模板不宣称能隔离任意外部进程、任意整套测试或恶意并发替换源文件；只用于下表中的非网络候选，源目录有并行修改时停止复制。D1/D2 的测试还必须在用例内正向断言 PCRE efun/版本可用，不能把未启用 package 的空函数当通过。当前 C++ 测试的 `TESTSUITE_DIR` 固定到原 testsuite，切换 cwd 无效；P0 先实现隔离根选择和无初始化的测试枚举，之后统一使用 §11.5 的定向 runner，不继续依赖这份 LPC 模板运行 C++ 测试。Windows 使用同一配置/夹具原则，但超时和退出码由 CI 的本机 runner 记录。

| 单元 | LPC 对象（未存在者必须由该单元先新增） | 补充验证 |
|---|---|---|
| A | `/single/tests/efuns/package_trim`（已有） | C++ UTF-8/非法序列边界；ASan |
| B | `/single/tests/compiler/private_inherit_same_name`（新增） | `DriverTest.TestLPC_FunctionInherit`；ASan |
| D1 | `/single/tests/efuns/pcre`、`pcre_replace`、`pcre_replace_type_checks`（均已有） | 新增的边界断言并入这些测试；ASan/UBSan |
| D2 | `/single/tests/efuns/pcre2`、`pcre_error_paths`（新增） | D1 全部回归必须保留 |
| F | `/single/tests/compiler/foreach_unwind`（新增） | 新增 `DriverTest.ForeachTemporariesRestoredOnUnwind` 白盒断言 |
| G | `/single/tests/operators/compound_assign_index_rhs`（新增） | normal/Bison-fallback 两条生成路径；UBSan |
| E | `/single/tests/compiler/named_function_identity`（新增）、`/single/tests/efuns/recompile_object`（已有） | §8 的新增事务白盒测试和定向 owner/TSan |

同名单元格中的缩写对象均位于首个对象的目录。不新建与已有 `.c` 同名的 `.lpc` 来遮蔽旧断言；`testsuite/command/tests.c` 的目录遍历会跳过有 `.lpc` 双生文件的 `.c`。

## 4. A：Unicode charset 裁剪

依据上游 `3b505bef`，但不改变默认字符集。当前缺陷是显式 `charset` 被 `find_first_not_of/find_last_not_of` 当字节集合，例如 U+3000 与《共享 UTF-8 前缀，裁剪可留下残缺字符。

修改范围：

- `src/base/internal/strutils.h`、`src/base/internal/strutils.cc`：三个函数统一按 Unicode scalar value 匹配 charset。
- `src/packages/trim/trim.cc`：仅在需要调整调用时修改；保留省略/空 charset 使用 `\t\n\v\f\r ` 的既有语义。
- `src/tests/test_lpc.cc`、`testsuite/single/tests/efuns/package_trim.c`。
- `docs/efun/strings/trim.md`、`ltrim.md`、`rtrim.md` 及 `docs/zh-CN/efun/` 的三个对应页面。

执行步骤：

1. 新增 `trim("　《正文》　", "　") == "《正文》"`；两端函数分别保留另一端，`trim("《正文》", "　")` 不变。覆盖 ASCII、中间内容、空串、全 trimset、四字节字符。
2. 默认 `trim("　x　")` 必须不删 U+3000；显式空 charset 在 package 层仍采用旧默认。直接 C++ helper 的空 charset 则是不删除，两层区别必须测试。
3. 复用 ICU 的 `U8_NEXT/U8_PREV`，一次解析 charset，按码点计算字节区间。源字符串遇非法序列不继续跨过该边界；非法 charset 字节不借用其他字符的合法字节。记录并测试该边界，不隐式扩大 Unicode 空白集合。
4. 返回值仍为 `std::string` 值，不重新引入对临时对象的引用。检查 `rc.cc`、`lex.cc`、`diagnostic_render.cc`、`external.cc`、`main_generate_keywords.cc` 的直接调用；ASCII 默认行为不变。
5. 按 §3 定向验证，不因修改共享头文件额外启动全仓构建。不得把上游 unordered_set 实现当作必须引入第三套逻辑的理由。

## 5. B：private inherited slot

当前 `compiler.cc::handle_functions()` 对 `new_index != final_index` 一律写 `FUNC_ALIAS | final_index`。上游修复的关键不仅是“不写 alias”，而是 hidden 分支**写回 `cur_def->flags`**，保留各自继承槽；空置该槽不是修复。

修改：`src/compiler/internal/compiler.cc`；新增上游同名夹具 `testsuite/inherit/priv_same_a.lpc`、`priv_same_b.lpc`，`testsuite/clone/priv_same_ab.lpc`、`priv_same_ba.lpc`、`priv_same_child.lpc`，测试 `testsuite/single/tests/compiler/private_inherit_same_name.lpc`；在现有 `docs/lpc/constructs/inherit.md` 说明 private 同名不互相 override。

执行：

1. 先移植上游 fixture 的断言并复现 A/B 与 B/A 两种继承顺序的内部调用串线。
2. 仅改 slot 生成分支：`DECL_HIDDEN` 保留 flags，其他分支仍生成 alias。不扩大成所有 private/visible 情形，不修改 grammar。
3. 测试 parent 私有内部调用、child 公开同名函数、公开 override、反向继承和同一祖先重复继承；B 是 E 的前置，不混合两个修复。
4. §3 的 LPC、现有继承 C++ 用例及 ASan 通过才完成。

## 6. C：Windows 退出生命周期

上游 `5894e7e1` 描述了 MinGW-w64 r420+ 下函数内静态 mutex/condition_variable 先于 namespace-scope `SignalHandling` 销毁的问题。本地 `src/thirdparty/backward-cpp/backward.hpp` 仍有对应静态对象；这构成适用性证据，**本次未在 Windows 重现**。

修改仅限该头文件的 Windows 分支及 `.github/workflows/ci.yml` 的现有 Windows 矩阵；不改 `main.cc`，不整包升级 vendor。

1. 先用 Windows Debug、RelWithDebInfo 重复运行退出探针，保留原始退出码、编译器/winpthreads 版本和 stderr。
2. 主要修复是同步原语的存活期覆盖 reporter 与析构过程。上游进程生命周期存储可作为最小方案；不以单纯 catch 异常代替存活期修复。
3. 若采用上游 join/detach 兜底，必须审查 reporter 是否已收到正常退出通知、线程成员是否仍 joinable。detach 也可能失败；禁止声称“catch-all 保证永不 abort”，更不能以改返回码掩盖失败。保留正常 crash handler 行为，Linux 分支不变。
4. 验证至少三种真实路径：`driver --version` 预期 0；`lpcc` 无参数、`symbol` 无参数预期 1（非 3，不能统一要求 0）；`lpc_tests --gtest_list_tests` 预期 0。再在 §3 临时 mudlib 用 `lpcc etc/config.absorb /single/tests/efuns/package_trim` 做成功编译退出探针（预期 0），并运行一项 driver 定向测试。
5. 每种配置重复运行并保留全部退出码；有 hang/abort 就未完成。没有 Windows 环境时为 `external-required`，不能以 Linux 通过抵扣。

## 7. D：PCRE2 迁移与 API 扩展

### 7.1 D1：先只换引擎，保留现有 LPC 合同

参考 `d140ec28`、`03354159`、`5270e7d6`，不整文件覆盖本地 PCRE。

**确定的合同：**

- `pcre.spec` 的现有 efun 名称、参数顺序和数值 flags 不变。D1 不增加 mapping options/string replacement，不默认开启 UCP/JIT。
- 当前默认只有 `PCRE_UTF8`，D1 使用 `PCRE2_UTF`；显式 `(*UCP)` 等 pattern 能力仍由 PCRE2 解释。上游默认 UCP 会扩大 `\w/\d` 等验证器的接受集合，不能悄悄带入。
- `pcre_replace(..., string *replacements, ...)` 仍只处理**首次匹配的捕获组**，数组元素是字面值；`$1` 不展开。嵌套组只由外层区间拥有，未参与组、组数错误沿用本地断言。
- callback 同步执行，保留 capture 和索引、trailing bound args、非字符串返回值的原输入回退。不得新增 Promise 等待或异步存储 VM 栈指针。
- `pcre_cache()` 仍为无参查询 mapping，不新增清空操作。引擎名称/大小数值会变，不把它们作为跨引擎稳定字节数。
- PCRE1/2 pattern 语法及错误文本不可能保证全部相同；执行前收集本仓库 PCRE 用例，记录确定的差异并更新归属文档，不能用“接口没变”宣称完全兼容。

**文件清单与顺序：**

1. `cmake/FindPCRE.cmake`、`src/packages/pcre/CMakeLists.txt`：沿用单一 `find_package(PCRE)` 包入口，但只查 PCRE2 ≥10.42 的 `pcre2.h`/`pcre2-8`，定义 `PCRE2_CODE_UNIT_WIDTH=8`、静态时 `PCRE2_STATIC`。D1/D2 共用这一最低版本，不迁移两次依赖合同。这里的 PCRE 是功能包名，不是 PCRE1 备用实现；不制造第二套变量别名。编译链接探针核对使用的符号，目标平台的初始化/测试核对 Unicode 支持；交叉编译时不能冒充已运行目标库配置检查。
2. 旧 `PCRE_LIBRARY/PCRE_INCLUDE_DIR` cache 可能继续命中 PCRE1。实施者拥有的构建目录采用新目录或显式清除这些 cache 项后重新配置；检查实际链接命令/动态依赖。找库模块增加能识别错误库的编译链接探针，不能只检查文件存在。
3. `src/packages/pcre/pcre.h`、`pcre.cc`：改 code/match_data/context/offset 类型及成对释放。使用 `PCRE2_SIZE` 到 LPC/容器长度的显式范围检查；`PCRE2_UNSET` 先判定再转换；限制结果为 driver 最大字符串/数组长度。
4. `src/include/pcre_flags.h` 更新注释/后端映射，不更改六个旧 bit。D1 不需要复制一个测试头文件；新增常量在 D2 再处理。
5. `.github/workflows/ci.yml`（包括矩阵外安装步骤）、`codeql-analysis.yml`、`coverity-scan.yml`：Linux `libpcre2-dev`，macOS `pcre2`，MSYS2 `mingw-w64-x86_64-pcre2`。
6. `Dockerfile` 的 Alpine 构建切换 PCRE2 开发包，并核验该固定 Alpine 版本提供的 `libpcre2-8.a` 和静态依赖。不能只改 apt/brew 而留下不能构建的静态镜像；本任务不运行镜像构建/发布。
7. 更新 `docs/build.md`、`docs/zh-CN/build/build.md`、`docs/efun/pcre/` 及 `docs/zh-CN/efun/` 对应现有页面。README 只在真实内容失效时修改，不创建不存在的 `docs/build.mdx`。

**资源/错误/重入：**

- 编译缓存 key 为 pattern 与全部**编译语义**（D1 为 compile flags；D2 仅增加本文开放的 UCP，不引入 extra/newline/BSR/JIT 配置）。offset、match/depth/heap limits、执行 flags 是每次调用的 match context，不靠膨胀 compiled-code 缓存来隔离。
- 每次匹配独立 match_data/context，不能共享可变 ovector。不同 limit/offset 调用交错后不得相互污染。
- code 的缓存借用必须覆盖整个执行。外层 callback 可重入 PCRE 并触发 cache 淘汰；须先复制出仍要使用的 metadata/captures 或持有 code 的有效所有权，不能释放后继续访问 name table/code。
- 沿用本地 DEFER/VM error-context 约定，覆盖编译失败、类型错误、分配失败、callback 抛错/自毁、converter 缓冲释放。`mark_pcre_cache()` 只是 debugmalloc 标记，不证明库内部内存无泄漏。
- NOMATCH 正常返回；非法 UTF、offset 非字符边界、limit 超限等负码必须与正常无匹配区分。禁止暴露 `NO_UTF_CHECK` 让 LPC 绕过 UTF 验证。
- 保留库提供的有限 match/depth/heap 默认限制，记录实际版本和默认值；它们不是严格 wall-clock 或整个数组批次的费用上界，不能宣称天然解决全部 ReDoS。
- `pcre_match_all` 必测空串、尾部零宽、空/非空分支交替、UTF-8 推进；不能简单 `offset++` 或忽略返回码 0 的 ovector 容量问题。

**D1 验收：**

运行 §3 的三个现有 PCRE 对象，在其中增加字面 `$1`、非法 UTF、错误/未知类型、交错调用和重入回归。Debug、ASan、UBSan 定向通过；PCRE package 关闭时用 package-aware 构建验证，不把未加 guard 的全套测试失败误记为迁移回归。旧库 CMake cache 的负面配置测试必须清晰拒绝错误链接。

Linux 动态、Alpine 静态、macOS、Windows 按实际环境分别记录；没有目标环境是外部待验，不为凑结果拉起部署。系统 PCRE2 未经 sanitizer 插桩时，ASan 不能证明其库内部全部路径；泄漏检测另记结果，不能使用 preset 的 `detect_leaks=0` 声称 LSan 通过。

### 7.2 D2：确定的 API 与选项合同

D1 完成后单独交付。最低 PCRE2 10.42；finder 同时检查实际使用的 config、pattern_info、pattern_convert、substitute 符号。D2 不把所有 PCRE2 开关转成 LPC 公共 API，不启用 JIT，不暴露 no_utf_check 或原始后端 option bits。需要 JIT 时另凭 P4 数据立项；JIT 会改变 depth/heap limit 的适用性，不能伪装成无语义成本的加速。

参数槽固定如下（未列出的既有返回类型、数值 mode 含义不变）：

| efun | 新增合同 | 兼容边界 |
|---|---|---|
| pcre_match(string, pattern, options) | 第三参数接受 int 或 mapping | 现有四参数纯 int 形式维持第三参数覆盖第四参数的既有行为并记录；新的 mapping 形式不允许第四参数，避免两套 options 优先级 |
| pcre_match(string *, pattern, mode, options) | 第四参数接受 int 或 mapping | 第三参数仍是数组筛选 mode；不允许用 mapping 猜测模式 |
| pcre_match_all(subject, pattern, options) | 第三参数 int/mapping | 返回形状不变 |
| pcre_extract(subject, pattern, include_names, options) | 第四参数 int/mapping | 第三参数仍是 include_names，不能偷换为 options |
| pcre_assoc(subject, patterns, tokens, default, options) | 第五参数 int/mapping | default 可为任意既有值，不能把其 mapping 当 options |
| pcre_replace(subject, pattern, replacement, options) | 第四参数 int/mapping；replacement 为旧 string * 或新 string | 数组形式保留首次匹配捕获组的字面替换；string 形式才按下表解释模板 |
| pcre_replace_callback | 保留现有参数形式和末尾数值 flags | **不新增 mapping options**；尾部 mapping 始终是业务 bound arg。避免新增同义 efun 或抢占业务参数 |
| pcre_config() | 无参，返回下述固定 mapping | 不读取/修改进程环境，不提供 setter |
| pcre_info(pattern, options=0) | 返回编译元数据 mapping | 只接受编译选项；执行和替换选项报错 |
| pcre_convert(pattern, options) | 返回转换后的 PCRE2 pattern | options 必填，使用独立的转换选项表，不接受普通匹配选项 |

普通 mapping 的唯一 key 集合：

| key | 类型、默认与合法值 | 适用范围/后端归属 |
|---|---|---|
| flags | int，默认 0，仅六个现有 LPC bit 的并集 | 编译/执行 bit 分流到现有转换函数；mapping 内未知 bit 拒绝，不重解释旧 int 调用 |
| ucp | int 0/1，默认 0 | 所有接受 mapping 的匹配与 info；compile key；UTF 始终开启 |
| offset | int，默认 0，0..subject 字节长度且在 UTF-8 边界 | 单 subject match、match_all、extract、replace；array match/assoc/info 拒绝 |
| match_limit | 正 int，不超过 PCRE2 配置默认上限 | 每次调用的 match context；省略用库默认；禁止 0/负数绕过 |
| depth_limit | 正 int，不超过库默认上限 | 同上 |
| heap_limit | 正 int，单位 KiB，不超过库默认上限 | 同上；不是输出大小限制 |
| global | int 0/1，默认 0 | 仅 string replacement；1 替换全部非重叠匹配 |
| literal | int 0/1，默认 0 | 仅 string replacement；1 时 replacement 全部按字面解释 |
| unset_empty | int 0/1，默认 0 | 仅 string replacement；1 把已声明但未参与的组替换为空 |

无“最后写入者胜出”的隐式合并：mapping 的 flags 若设置了与 ucp 等独立选项冲突的内容，明确拒绝；当前六个 bit 不含 UCP，不能虚构冲突。错误应包含 efun/key 与原因，不回显完整输入。所有整数在转换为 PCRE2_SIZE/uint32_t 前检查范围。对不适用 efun 的 key 一律报错，而非忽略。

string template 的合同：默认仅替换第一处；只开放 $$、$数字、${数字}、$name、${name}，数字按最长连续十进制串解析且允许组 0，name 限 ASCII 字母/下划线开头、后接字母/数字/下划线。普通非 dollar 文本按原样保留；孤立 $、空/未闭合括号及本表以外的扩展引用报错。实际展开交给 PCRE2，不打开 SUBSTITUTE_EXTENDED。引用不存在的组始终报错；未参与组默认报错、unset_empty=1 时为空；literal=1 时两者不适用，若同时指定 unset_empty 则拒绝以暴露无效配置。无匹配返回原串；非法模板即使无匹配也必须报错，先验证引用/语法再执行。不以手写正则替换循环复制 PCRE2 引擎：模板验证只解析本地承诺的 dollar 语法，实际匹配和替换使用 pcre2_substitute；输出扩容前受 driver 字符串上限约束。零宽 global 使用 PCRE2 的标准推进，不按单字节前进。

固定结果表：

- pcre_config：version(string)、unicode(int)、unicode_version(string)、jit_available(int)、jit_enabled(固定 0)、match_limit(int)、depth_limit(int)、heap_limit(int)。库报不支持的必须项使依赖探针失败，不随平台缺 key。
- pcre_info：capture_count(int)、backref_max(int)、min_length(int)、compiled_size(int)、names(mapping，名称→组号数组，允许重复名)。compiled_size 只作当前构建诊断，不能跨版本当性能标准；元数据复制完再释放 code 引用。
- pcre_convert options：syntax 必填，限 glob/posix_basic/posix_extended；glob_separator 仅允许单字符正斜杠、反斜杠或点，默认正斜杠，只用于 glob；glob_escape 仅允许单个 ASCII 标点（码点 33–47、58–64、91–96、123–126），默认反斜杠，只用于 glob，且与 separator 不同。不提供短别名、不接受 NUL、多字节、字母/数字 escape。两个默认值在每个 convert context 中显式设置，不能继承 Windows 不同的库默认值；setter 返回失败必须报错。PCRE2 convert buffer 使用 pcre2_converted_pattern_free 释放，返回长度/UTF 和 driver 上限均检查。

### 7.3 D2 实施结构、错误与验收

1. 在现有 pcre.cc/h 内定义 ParsedPcreOptions 和一张适用性表；所有入口先完成槽位、类型、长度和 key 校验，再取缓存和分配结果。D1 的六 bit 转换函数复用，不增加第二个数字解释器。
2. 缓存只拥有不可变 compiled code；每次执行借用计数引用，每次独立 match_data/match_context。cache 淘汰只释放 cache 自己的引用。D2 编译 key=pattern 字节串+compile flags/UCP；不把执行 limit、offset、global、replacement 混进 key。
3. 匹配数组/assoc 共用一次编译，但每个 subject 单独复位 match state；整个批次另受现有 eval/结果容量限制。库 match_limit 不是批次 CPU 上界，P4 需测最坏耗时。
4. 以受管 svalue 容器收集结果，失败和 C++ 异常都释放已持有字符串；禁止 std::vector<svalue_t> 裸值析构被误当作释放 LPC 引用。见 I14。错误转换沿用 VM error-context，不把任意负码变 NOMATCH。
5. 修改 pcre.spec、pcre.cc/h、finder、现有 flags 说明及三个新 efun 的中英文文档。由 CMake 生成 efun 表；不引入测试手写 flags 副本；若需要测试头由现有头派生并比较。
6. 新增 testsuite/single/tests/efuns/pcre2.lpc 与 pcre_error_paths.lpc（拟新增）：逐个 key 的正常/0/负数/溢出/错误类型/未知 key/不适用入口；string/array match 的槽位反例；末尾业务 mapping 的 callback；数组 $1 字面保留；模板组、literal、unset、零宽、offset边界；交错 limit/UCP 不串状态；cache 淘汰与递归 callback。
7. Debug/ASan/UBSan、依赖探针与目标平台链接分别验收；库不插桩的范围仍注明。模板/convert 输出上限和低 limit 错误必须先能失败再修复，不靠放宽输入校验通过。

选项约束对照 [PCRE2 10.42 separator 文档](https://man.adelielinux.org/html/packages/system/pmmx/pcre2-10.42-r0/man-3-pcre2_set_glob_separator.html)及 [escape 文档](https://www.pcre.org/current/doc/html/pcre2_set_glob_escape.html)；本地合同有意更窄，不能扩大为“任意 ASCII”。实施探针同时验证最低版本的这些实际 API。

D2 设计已定案，状态为 design-reviewed/unverified。它增加 API，不承诺与上游所有选项兼容；如要扩大 key 集合，须先更新此表及反例，不能实施时顺手添加。

## 8. E：命名函数身份与热重编译事务

### 8.1 目标与明确不做的事

无 bound args 的重复 local/efun/simul 引用获得稳定 identity，便于回调注销；local 引用在无歧义的重编译后执行新代码。保留 object identity、owner、变量迁移、simul 累积索引、默认参数与 async 的现有模型。匿名函数不变成热迁移代码；不引入全局无锁函数表，不改普通 LPC 引用计数为原子计数。

必须先修 I01–I05、I11–I12、I15 和 B。当前 function.h 声称 FP_FUNCTIONAL 会比较代次，实际 function.cc 分支没有比较；不能把“保留现有检查”当实现步骤。rollback 恢复代次后又成功提交，会复用数值；单靠 generation 不能识别失败事务中的逃逸指针。

### 8.2 身份合同（同时也是测试预期）

| 对象 | canonical identity | 重编译/删除/bind 后行为 |
|---|---|---|
| 无 bound args 的 FP_LOCAL | 拥有对象 + local kind + 声明身份 + 原始绑定来源 | 唯一对应则原 funptr 地址不变、调用新代码；缺失/歧义永久 stale |
| FP_EFUN 无 bound args | 拥有对象 + efun kind + opcode | 不受该 owner 普通重编译影响；不同 owner 不相等 |
| FP_SIMUL 无 bound args | 拥有对象 + simul kind + 累积 sindex | 成功发布过的索引不压缩、不复用；inactive 时按现有 undefined 错误。本次事务临时新增索引随 rollback 撤回，须同时执行 I15 的编译隔离与出生指针失效，不能只依赖累计索引口号 |
| 带 bound args 的 local | 每次独立 funptr，不比较参数深相等 | 与无绑定 local 用同一解析/journal；不新增 lazy refresh 旁路 |
| FP_FUNCTIONAL | 每次独立，持有所属代码 | 原 owner 成功重编译后 stale；失败回滚前已有的仍可调用；失败期间新建者永久 invalid |
| bind 到相同 owner | 保持现有 no-op，返回原指针 | 不重新 canonicalize |
| bind 到新 owner | 按现有 valid_bind 授权创建独立副本，不 intern | local 继续禁止重绑定；副本继承原 invalid 状态，不允许 bind 洗掉 stale；重绑 functional 记录新 owner 的代次 |

声明身份不是裸函数名。构建每个 program 的不可变目录，记录：定义文件的规范 LPC 路径、函数名、kind、从 top program 到声明程序的继承链；链的每步用“程序路径+同路径直接继承出现序号”。构造时保留 alias 解析前 ref_index 对应的来源，再保存当前 resolved index；alias 只用于执行，不抹掉来源。

同一个 resolved slot 若来自不同绑定来源，不合并 canonical identity。这是有意比上游保守的本地合同，避免未来 alias 分裂后随机择一；反之同一来源重复获取仍相等。重复祖先链的形状/同源计数改变、同路径多个候选无法唯一对应时 stale，不按遍历顺序猜。普通无关继承插入不改变其他路径的身份。

函数在一次**成功提交**中缺失或变歧义后，该旧指针永久 stale；日后同名重加产生新 identity，不能复活注销过的句柄。临时 staged 缺失但事务失败，旧状态恢复。不能通过调用 apply(name)绕过 private/hidden/alias 与继承偏移。

### 8.3 状态与所有权设计

新增对象附属 FunptrRegistry（拟命名，放在 function.h/cc，object_t 只持指针）：

- 一张只包含 canonical named funptr 的弱索引，以及全部该对象拥有的 funptr 的 intrusive 弱链。后者覆盖 bound、functional 与 bind 副本，服务事务和析构；不是第二张强引用对象表。
- funptr 增加明确初始化的 intrusive links、registry 标志、生命周期状态 Live/StagedBorn/Invalid、逻辑声明标识与可选 prepared resolution。所有构造点和 f_bind 都经统一初始化 helper；严禁沿用 *new_fp=*old_fp 复制链表链接/注册状态。bind 在覆盖新 owner/gen 前先计算来源的有效性（Invalid、destruct、适用 kind 的旧代次不等、失效 resolution），将其失效结果继承到副本；不能只复制显式 Invalid 位而洗掉尚未物化为该位的代次失效。
- registry 随对象而非 owner-id 分片迁移，因此 owner 改变不搬表、不留 TLS 旧表。执行和修改仍须处在对象合法 owner admission 或 main 的受控清理/FROZEN 路径；跨 owner 只传 ObjectHandle/frozen payload，不传 funptr。
- 表不增加 funptr/owner 引用；普通 funptr 仍持 owner +1、bound args +1、相应 code 的 func_ref +1。dealloc_funp 必须先 unlink，再释放 args/program/owner，避免 owner 最后一个引用释放后访问 registry。
- prepare 对已有 funptr 每个 +1 事务 pin；引用在逻辑 finish/rollback 后统一释放。初始化释放最后一个外部引用不会让 journal 悬挂。新生 funptr 注册即由事务 +1 pin，不在提交点新分配节点。
- local 的旧/new code 都以现有 func_ref 规则记账，不能把 owner->prog 当作旧指针的释放目标。functional 的定义 program 与 owner top program 是两个概念；保留原定义 program pin。I04 对 coroutine 的 owner top program 另持引用，防止释放地址复用。
- debugmalloc/reclaim/cycle breaker 不把弱链当强根；被回收的 funptr 走同一 unlink。锁顺序不新增：先取得既有 admission，再访问对象私有表；不得持 OwnerRuntimeCoordinator mutex 执行 LPC、释放任意复合值或取得新表锁。

一旦实测发现某条现有 funptr free 路径绕过 owner admission，先修其 cleanup adapter（I12），不通过全局 mutex 或 atomic refcount 掩盖整个对象图的线程错误。

### 8.4 事务状态表与 no-fail 边界

复用 RecompilePrepared 的三段流程，以及 I15 的 scoped RecompileExecutionContext（拟名，挂在现有 VMContext）。该 context 同时持目标集合、staged program 集合和出生 journal；不另造一套 TLS 活动事务标志。registry 的 canonical key 是逻辑身份，提交时不需要哈希 rekey；索引桶、目录和 journal 节点在 fallible 段建好。unordered_map::reserve 不是节点预分配，不作为无分配证明。

| 阶段 | 可以做 | 禁止做/失败处置 |
|---|---|---|
| prepare（FROZEN） | 建 staged 目录；遍历目标 registry；pin 旧指针；解析每个身份；分配 resolution/journal 和新 code pin；保存 old/new cleanup flags | 失败只释放未发布状态；不得改 live funptr key、generation、目标变量 |
| commit_swap | 已持有入口 context；发布新 program/变量/dispatch 与 prepared resolution 指针，推进到 Swapped 阶段 | 仅字段写/引用转移；不得插 map、调 LPC、分配、释放可能再入的复合值 |
| run_create | 沿现有 kind 的 init/migrate/create 顺序执行；local 通过 staged resolution 调用；新建 funptr 完成全部 fallible 初始化后登记出生 journal | 不永久改旧指针身份；不得把 old functional 用于新变量布局；任意异常进入 rollback |
| commit_finish 逻辑段 | 用预备字段更新存活 local 的 index/program/gen；将缺失/歧义标 Invalid 并从 canonical 表无分配移除；将新生 StagedBorn 改 Live | 不寻找名字、不再解析 alias、不新增节点；不复活已 Invalid 指针 |
| rollback 逻辑段 | 恢复旧 program/变量/dispatch/flags；丢弃 staged resolution；旧指针回到原状态；所有事务新生指针标 Invalid 并 unintern | 不清除永久 Invalid，不把新生指针嫁接为旧 canonical，不恢复已 destruct 对象 |
| cleanup | 在既有 main cleanup 路径释放 loser pins/journal/变量块；最后再结束 admission freeze | 逻辑状态已稳定后才释放复杂资源；清理不得调用任意 LPC；若释放路径会分配，不能混在上述 no-fail 逻辑段里 |

入口 context 从授权 hook 前的 Entered 阶段开始；事务体显式区分 Prepared/Swapped/Finalized/RolledBack，防止重复 rollback/free。run_create_guarded 覆盖 const char* 与其他 C++ 异常，先恢复 VM context、rollback，再按 VM 错误合同上报；不是吞异常继续成功。析构只作当前状态应做的清理，不隐式再执行一次已完成事务。

初始化重入规则：

1. 旧 canonical 引用被再次获取时，仍返回已 pin 的原对象，调用读取 staged resolution；若 staged 中该名字缺失则报错，不借旧代码执行新布局。
2. run_create 复用 VMContext 中 RecompileExecutionContext 的出生链，沿同步嵌套 LPC 调用生效。真正新生或 bind 副本通过构造函数登记到该事务 intrusive 出生链，包括在非目标对象方法中创建、或 bind 到非目标 owner 的指针；只挂目标对象链会遗漏这两种逃逸。该合同有意保守：事务期间新建的这些指针失败后一律 Invalid，外部对象其他业务状态不回滚；获取非目标对象已有 canonical 指针不算新生。完成构造与登记前不发布到 VM 栈。表插入/内存失败仍在 run_create 的可失败段，走整个事务 rollback。
3. 临时创建后立即释放、写入外部对象、藏在嵌套 array/mapping/callback、递归获取和 new bind 都被链记录。不能只扫描重编译目标的变量块寻找逃逸引用。
4. 使用当前 quiesce epoch 作为日志诊断身份；正确性由永久 Invalid 状态与 journal 所有权保证，不依赖 epoch/generation 将来“不太可能复用”。既有 uint64 计数到上限必须在 prepare 拒绝，不允许 wrap。
5. 失败后的旧指针可用、新生指针永久无效；随后成功或再次失败均不改变此结论。已 destruct 目标仅保留资源回收所需 pin，不能重新放回 live object store。

引用账本（每个 local，其他 kind 按有无 code pin缩减）：

| 动作 | funptr | owner | code |
|---|---|---|---|
| 普通构造 | 外部 +1 | +1 | live code func_ref +1 |
| prepare pin | +1 | 无新增 | staged code 的待提交 func_ref +1；旧 pin保留 |
| 成功 | 释放事务 +1 | 不变 | staged pin 转为指针的 live pin；旧 pin在 cleanup -1 |
| 失败 | 释放事务 +1 | 不变 | staged pin -1，旧 pin保持 |
| 新生失败后仍被外部持有 | 保留外部 ref，状态 Invalid | 保持 +1 到最终析构 | 不再依赖 staged 可调用索引；其调试/所有权 pin按统一析构释放 |
| 最终释放 | 先 unintern/unlink，再到 0 | -1 | 实际拥有的 code pin -1，禁止按当前 owner program 推断 |

### 8.5 文件、实施子单元与验收

范围：function.h/cc、program.h/cc 与编译结果发布路径、object.h/object.cc 的附属 registry 生命周期、recompile.h/cc、efuns_main.cc 的 recompile/bind 入口、simulate.cc 的对象失效路径、实际 reclaim/cycle-breaker 调用点。首先处理 object.cc 外部改动归属，不覆盖或擅自纳入提交。

- E1：先添加不可变声明目录和解析器，以重复继承/private/alias 测试证明身份解析，不接热迁移。正常编译和 staged 编译共用一个目录构造点，在 program 向对象/其他执行者发布前构建完成，失败释放未发布 program；审查任何绕过该点的现有加载路径。不在 worker 第一次取函数时懒建共享目录。目录发布后只读，随 program 释放；内部指针不写入持久化代码格式。
- E2：基于 E1 的身份目录，复用 P1/I05 已建的统一构造/clone/unlink、全 funptr 弱登记和出生 journal，再增加 canonical named 索引；先只做重复获取 identity，热重载仍 stale。不引入 raw-index 临时 key 再迁移第二遍。新增 local/efun/simul/bound/bind/回收后地址复用用例；debugmalloc 与引用账本通过。
- E3：接入 staged resolution、出生 journal 与状态机；不另做 bound lazy refresh。对所有目标 kind 测成功、LPC error、C++ exception、prepare/create 分配失败和自毁。
- E4：开启成功提交后的 local 迁移，更新函数/recompile 归属文档；functional 保持失效合同。验证 callback 注销的用户收益而非只测 ==。

拟新增 LPC：testsuite/single/tests/compiler/named_function_identity.lpc、testsuite/clone/named_fn_base.lpc、named_fn_child.lpc；沿用 recompile_object.c。src/tests/test_lpc.cc 新增明确前缀 TestNamedFunptr，并至少含 IdentityKinds、RepeatedInheritance、AliasSplit、StagedBirthRollbackRetry、BoundBirthRollback、BindAfterSuccessfulRecompile、BirthInNonTargetOwner、InitDropsLastReference、DeleteReaddInvalid、RefBalance、OwnerCleanup。

上述均为拟新增测试名，不声称已存在。每个过滤器须证明实际用例非零。白盒允许设定代次/注入分配失败，但必须同时有经真实 LPC 编译、真实事务入口的集成用例；普通精确布局、可迁移布局、master、simul 四种均覆盖。Debug/ASan/UBSan 与触达 registry/cleanup 的 TSan 单独记录。默认参数、async、private 访问检查不得绕过。设计状态为 design-reviewed/unverified，不以文档替代 fail-first。

## 9. 补入的 F/G 与独立候选 H

### F：foreach 异常恢复临时栈计数

上游 `134bebd7` 的普通循环错误会跳过 `F_EXIT_FOREACH`，遗留非零计数，使 `break_point()` 后续不再检查栈。本地该计数同时存在于 DEBUG TLS 镜像和 `VMContext.execution`；必须用现有 setter 同步，而不是复制上游全局赋值。

修改候选：`src/vm/internal/base/interpret.h`、`interpret.cc`、`src/vm/internal/context.cc`（仅确需时），`src/tests/test_lpc.cc` 和新增 `testsuite/single/tests/compiler/foreach_unwind.lpc`。

1. 在 DEBUG 白盒测试记录循环前计数，触发 LPC foreach 内 error、catch 后比对两份计数均恢复；再执行正常 LPC，证明检查未被永久关闭。
2. 覆盖直接 catch、函数返回/pop_control_stack、safe_apply/restore_context、嵌套循环与正常退出。沿本地 `VMContext` 保存/恢复路径统一恢复到进入边界的值，不统一归零，避免破坏外层合法 temporary。
3. 确认每个退出路径只恢复一次，Release 不引用 DEBUG-only 字段；async/owner 上下文保存额外跑对应定向用例。
4. 若当前 driver 已通过新增白盒反例，则先定位实际恢复路径并修正适用性，不为匹配上游而强改。

### G：mapping/array index RHS 的 op= 类型

上游 `932e9309` 的本地落点是 `src/compiler/internal/grammar.y`，不是上游已拆出的 `grammar_rules_exprs.cc`。

1. 新增 `testsuite/single/tests/operators/compound_assign_index_rhs.lpc`：`int i` 对持有 1.5 的 mapping、mixed array、反向索引分别执行 `+=`、`-=`、`*=`、`/=`，断言类型仍为 int；例如初值 10 的 `+=` 应为 11，初值 7 的 `/=` 应为 7（先按目标类型转换 RHS）。
2. 扩展 `unknown_dynamic_rhs` 到本地 `NODE_BINARY_OP` 的 `F_INDEX/F_RINDEX`，保持仅四个算术复合赋值受影响。不要复制本地不存在的上游 map-member opcode。
3. 反例：mixed 左值仍可提升 float，已有 call_other/evaluate 行为不退化，普通 mixed 变量和字符索引的错误不因宽泛 `to_int()` 被吞掉；零除错误、求值次数与副作用顺序不变。
4. 同步 `src/compiler/internal/grammar.autogen.cc`、`grammar.autogen.h`。当前 fallback 生成器为 Bison 3.8.2；按仓库生成规则从源 grammar 重新生成，不手改生成 hunk，不把构建树输出直接复制回源码树。
5. 正常有 Bison 和 `CMAKE_DISABLE_FIND_PACKAGE_BISON=TRUE` 的独立 fallback 构建目录各运行该定向测试；两个目录串行。若缺相符生成器，G 为生成链阻塞，不提交不一致源/产物。更新真实失效的 LPC 复合赋值语义说明。

生成及 fallback 验证命令（只在 G 实施授权后，从根目录运行；先确认 Bison 3.8.2 和该新构建目录无归属冲突）：

```bash
bison -Wall --warnings=none \
  --defines=src/compiler/internal/grammar.autogen.h \
  --output=src/compiler/internal/grammar.autogen.cc \
  src/compiler/internal/grammar.y
cmake -S . -B build-absorb-fallback -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_LTO=OFF -DMARCH_NATIVE=OFF -DCMAKE_DISABLE_FIND_PACKAGE_BISON=TRUE
cmake --build build-absorb-fallback --target driver --parallel 4
```

使用 §11.5 已验证的 runner，`--case` 设为 `/single/tests/operators/compound_assign_index_rhs`，`--driver` 先后设为正常构建与 `build-absorb-fallback/bin/driver`。检查 fallback 配置确实选择源码生成文件；若生成器带来无关整文件漂移，先查 flags/版本/路径，不手动抹掉差异。此处不生成 `.output`/report 到源码目录。

### H：websocket TLS 证书热更新的失败原子设计

**方案选择：保留同一个 listener/context/vhost，准备新的 SSL_CTX，再一次性发布。**不重建 vhost、不切断连接、不用监听端口重启模拟热更新。只支持项目实际 bundled OpenSSL backend；其他 backend 清晰报不支持，不静默退回非原子路径。

当前证据不支持直接调用 lws_tls_cert_updated：bundled lib/tls/tls-network.c 忽略 lws_tls_server_certs_load 返回值并总返回 0；openssl-server.c 的 loader 直接向 live vhost->tls.ssl_ctx 装证书和 key，不能保证中途失败仍是旧配对。仅在外围先调用 tls_server_init 校验也不充分，两次读文件有 TOCTOU，且 lws 的 ALPN/SNI/options 与普通 TLS 不同。

还有第二处约束：lws_ssl_server_name_cb 以“当前 vhost ssl_ctx 指针等于 SSL_get_SSL_CTX”寻找原 vhost。热切换后，尚在握手的旧 SSL 会找不到，Debug 可触发 assert。因此 SSL_CTX swap 必须连同 SNI 来源身份修复，不是交换一个指针即可结案。

#### H1 合同与数据流

- 保留 sys_reload_tls 的线程→master授权→1-based索引→TLS类型校验顺序；未授权者不能读取路径/打开文件。原有 TLS 端口合同不变，websocket 仅在已配置 TLS 时开放。
- 每次仅更新指定端口；共享同证书路径的其他端口不会隐式一起变。成功后新建 SSL 使用新证书；在发布前已创建 SSL 的连接（包括未完成握手），在本项目同一目标 vhost 上继续使用其旧 generation。已有 WS session 不重连、不重置压缩/协议状态。
- 读取证书链/key 到一次调用私有的受限内存快照，PEM 解析并校验配对、算法可用性、叶证书/已提供中间链的一致性与有效期；根证书不要求随链发送，不把服务端加载校验误写成必须受系统公共 CA 信任，测试自签证书仍可用。拒绝不可用证书。不提供新路径参数、不泄露 key/PEM/log原文。读入上限用明确常量并有越界测试；拟定每文件 1 MiB，属于新管理接口输入限制，文档说明，不用于声称网络容量。
- 不允许以“校验过同一路径”为第二次打开的担保。准备 helper 从同一快照生成 ctx；PEM chain 逐项解析，不能把 PEM 字节误传给 lib 的 ASN.1 memory 参数。释放私钥临时缓冲时使用已有 OpenSSL 清零释放机制。
- 发布前完整构建 TLS options、cipher、CA、ALPN、SNI、session-id context 与 callback；只有准备成功才交换 live ctx 和对应证书状态元数据。失败释放 staged，旧状态不变；发布点无 IO、无分配、无 LPC。
- SSL_CTX 的 listener 引用与每个 SSL 自身引用分开：释放旧 listener ref 不销毁活跃旧 SSL 的 ctx；退出由既有 lws lifecycle 清理，不复制 context、不手改活跃 wsi->ssl。新 ctx 不复制旧 session cache/ticket keys，旧 generation 的恢复凭据不得用于新连接绕过新证书；已有连接保持不变，新 generation 自身仍可正常会话恢复。

#### H2 bundled 库的最小适配边界

新增内部 staged-server-context API（拟名 prepare/commit/discard，不是公共 efun）：提取现有 openssl-server.c 的 policy/证书构造为单一 helper，入参含目标真实 vhost、policy 和已解析 snapshot，出参独立 staged bundle。初次启动和 reload 共用 helper，不能维护两套 TLS 配置。

保持原 lws_tls_cert_updated 对其他调用者不改公共语义，本项目 reload 不调用它。新 helper 的成功/失败明确返回，所有 openssl 设置返回值检查；现有会忽略 cipher/CA 失败的分支不能复制到准备路径。仅提交 bundle 中明确列出的 ctx、skipped_certs/证书状态字段，不整体 memcpy vhost。

SNI 修正：SSL/ctx 持有可追溯的原 vhost 身份（独立 ex_data 索引，不能覆盖 lib 已存 context 的索引）；该 vhost 与活跃 SSL 同寿命。回调不再靠 active ctx 的指针相等反查。对本项目每端口的 default vhost，同 vhost 的旧握手保持原 ctx；真正选择其他 SNI vhost 时才按既有路由切换其 ctx。默认 vhost 不得误选内部 system vhost。

修改范围：src/packages/core/sys.cc、src/net/tls.h/cc、websocket.h/cc、src/base/internal/external_port.h（只存必要 policy/状态）、bundled libwebsockets 的 lib/tls/openssl/openssl-server.c、lib/tls/private-network.h 以及最小声明/导出归属头。先列出 helper 所需每个字段，再实现；不整包升级库，不照抄私有 struct 到 C++。

如果当前链接方式需要公共可见导出，则在 bundled include/libwebsockets 对应 TLS 头加项目内部扩展符号并做编译链接探针；不为系统外部 libwebsockets 提供行为不同的 fallback。本项目构建是否实际命中 bundled 必须由链接命令证明。

#### H3 实施与验收

1. 先完成 I06–I09；在 src/tests 新增仅 loopback 的 TLS reload 集成用例/辅助脚本，接现有 ws_smoke.py 而不声称其已支持全部测试。证书用临时测试 CA/临时文件生成，不改仓库固定 fixture，不使用生产 key。
2. 先加入 fail-first：现有 websocket 端口明确拒绝，坏 key 不应改变证书，新连接应换指纹；先保留当前失败事实。
3. 实现 staged helper 并测试 prepare/discard，无 listener 副作用；再修改初始 TLS policy 路径，最后接 sys_reload_tls，不能先开放 efun 再补原子性。
4. 测既有 ascii/telnet WS 会话连续收发；证书 A→B→C；A 上未完成握手跨越发布；SNI/无 SNI；旧票据在新 generation 不恢复、同代恢复成功；坏 key/坏 chain/过期/超限/读文件失败；发布前分配失败；失败后新连接仍见旧指纹；高频 reload 后 ctx/SSL/evbuffer 计数回基线。
5. 授权失败、缺 hook、owner worker 调用、负索引/超大索引/明文端口均拒绝且没有文件读取；多个端口同路径仅目标变化。ASan 下做连接中断/退出，Windows/macOS/Linux 分别记录可用结果。
6. 更新现有 docs/efun/system/sys_reload_tls.md 与 docs/runbooks/gateway-security.md 的不支持项/原子合同。TLS协议最低版本调整归 I08，不借 reload 顺手放宽。

设计状态为 design-reviewed/unverified。没有真实 loopback TLS 握手、旧半握手和失败后可用性证据时，不能称热更新完成；不触碰在线实例。

## 10. 交付、停止条件与最终只读复核

每个单元的记录追加在本文，不改写历史 August 证据。至少包括：实际文件清单/源码 SHA、依赖版本、完整验证命令、非空用例数/通过失败数、退出码、耗时、原始日志路径、sanitizer 是否启用泄漏检查、外部未验平台。

| 单元 | 当前状态 | 进入代码/完成的条件 |
|---|---|---|
| A/B | `unverified` | 授权实施后 fail-first、定向回归与 ASan |
| F/G | `unverified` | 白盒/类型反例先失败；G 生成链一致 |
| C | `external-required` | Windows 两配置真实退出路径验证 |
| D1 | `complete`（Linux/WSL2；外部平台 `external-required`） | 依赖探针、旧合同回归、内存与本地链接验证已完成；外部平台和 Docker 需对应环境 |
| D2 | `design-reviewed/unverified` | §7 接口/错误/内存合同固定；D1 通过后实施 |
| E1–E4 | `design-reviewed/unverified` | §8 设计固定；I 前置与 B 通过，解决 object.cc 外部改动归属 |
| H1–H3 | `design-reviewed/unverified` | §9 原子/TLS身份设计固定；I06–I09 通过，有独立 loopback 验证环境 |
| I01–I15 | `source-reviewed/unverified` | §11 逐项 fail-first；未复现的风险不得报成已发生事故 |
| P4 测量候选 | `measurement-required` | 先取得 workload/预算/热点证据，不预设容量或提速结论 |

停止受影响单元的条件：未得到预期 fail-first、断言/sanitizer 失败、未知生成差异、零用例、外部修改碰撞、资源不足或真实权限阻塞。保留失败证据，先只读定位根因；不能改变断言、忽略返回码、跳过检查或引入备用旧引擎使门禁表面通过。独立单元在授权范围内可以继续。

回退只撤销本任务所属且已核验的 hunk；禁止全仓 reset/clean 或按整个混改文件恢复。部署、重启、tag、release、push、触发远端 CI 不包含在源码实施许可中；本次工作区已有外部改动，提交/推送前必须暂停并确认归属。

**最终复核只读：**

1. 对照本次任务开始时的内容哈希和实际 diff，确认只修改批准文件；两处既有外部改动逐字保持。
2. 重读本文各单元、所有记录与失败日志；检查新文件、命令工作目录、package guard、测试过滤器、公共文档及生成链一致性。
3. `git diff --check`；本文仍未跟踪时另用 §3.1 的 no-index check。源码/构建查找只通过读取文件或 Git 查看，不运行 cmake/CTest/driver。
4. 核验真实动态证据来自哪些源码/二进制与环境；证据不足就保留未验证状态，而不是在“只读审计”里偷偷启动全量验证。
5. 以“某单元本地验收通过/外部待验/设计待定”报告，不使用笼统 `release-ready`。方案审计完成、功能实现完成、平台验证完成是三个不同结论。

本版已扩充专项设计和本地跨模块审计，尚未运行任何候选修复的构建或测试；代码实施与外部验证尚未发生。

## 11. 本地跨模块审计与前置修复 I

### 11.1 证据账本

以下均定位到当前真实文件；搜索索引/历史摘要不作为确认依据。状态只表示静态控制流或结构证据，不表示已经复现崩溃、泄漏或攻击。本轮未编译、未跑 sanitizer、未扫描线上环境。优先级表示进入大改的次序，不是 CVSS 分数。

| 编号/优先级 | 当前证据（文件与符号） | 影响与证据边界 | 归属 |
|---|---|---|---|
| I01 高 | src/vm/internal/base/function.cc::call_function_pointer 仅 FP_LOCAL 检查 owner_gen；FP_FUNCTIONAL 直接 setup_new_frame。function.h 的注释声称两者均检查 | 旧匿名代码可能访问新变量布局；缺检查静态确认，运行后果待复现 | P1；E 前置 |
| I02 高 | recompile.cc::run_create_guarded 只 catch const char*；commit_swap 已发布新 program/变量。Prepared 析构是资源释放而不是事务恢复 | 其他 C++ 异常不能依赖该 catch 回滚；不能声称 noexcept 注释已保证异常安全 | P1；E/H 公用错误纪律 |
| I03 中 | recompile.cc::snapshot_recompile_targets 用旧 ob 调 function_exists(clean_up)得 precomputed_flags；commit_swap 与 rollback 都用此同一值 | 新增/移除 clean_up 后 O_WILL_CLEAN_UP 与新程序不一致；旧/new 派生状态未分开 | P1；E 前置 |
| I04 高 | promise.h::lpc_coroutine_t.prog_generation 为 uint32，而 object_t/funptr 用 uint64；object_prog 明示不持引用 | 代次达到 2^32 后正常 park/resume 也会不等；owner program 地址复用是另一个条件风险，不等同于已复现 UAF | P1；E 前置 |
| I05 高 | recompile.cc 在 swap ++generation、rollback 恢复 old_generation；funptr 没有失败事务出生状态；local 调用只比较代次数值 | 失败期间逃逸指针在随后成功时可能再次通过相同代次，旧索引被用于不同程序；必须测 rollback→retry，不只测失败瞬间 | P1→E3 |
| I06 高 | websocket.cc::close_user_websocket 先置 pss->user=null；ws_ascii.cc/ws_telnet.cc 的 CLOSED 在 user 为空时提前返回，跳过 evbuffer_free | 主动关闭路径不能释放 session buffer；具体残留量待连接测试 | P1；H 前置 |
| I07 高 | comm.cc 及 ws_ascii.cc/ws_telnet.cc 都用 event_base_once 携带裸 interactive_t*，remove_interactive 最终 FREE(ip)，没有可取消的 logon event 句柄 | 关闭先于回调时存在悬挂引用风险；事件排序与 ASan 复现待测 | P1；H 前置 |
| I08 高 | socket_efuns.cc::socket_connect 设置 SSL_VERIFY_PEER/SNI，但未绑定目标 DNS/IP 身份；SSL_new/SSL_set_fd/SNI 等返回值未完整检查。tls.cc 排除 TLS1.1 但没有统一 minimum version | 验链不等于验目标；TLS1.0 是否实际可连取决于库/系统策略，不写成所有环境均允许 | P1；H 前置 |
| I09 高 | ws_ascii.cc/ws_telnet.cc 在 ESTABLISHED 直接用 X-Real-IP 覆盖 getpeername，无 trusted-proxy 判定；坏值 return false 等于 callback 成功码 0 | 直连客户端可影响记录的地址；坏头还会留下未初始化 session 后续路径，是否触发需测试 | P1；H 前置 |
| I10 高 | src/tests/CMakeLists.txt 把 lpc_tests、ofile_tests、compile_arena_tests 一起传给单 target 的 gtest_discover_tests；两个 driver 测试宏指向源码 testsuite | 不能据 CTest 成功证明三个目标均被发现；测试会写源码 mudlib，非只读检查 | P0 |
| I11 中 | owner_runtime_coordinator.cc 的 active_owner_claims 等 getter 无锁；efuns_main.cc 的 quiesce 失败日志在 begin 返回后读取 active_owner_claims，worker 仍可 claim_end 写入 | 该 unlocked reader 与受锁 writer 不构成同步；不是把所有统计都泛称有竞态 | P1；E 前置 |
| I12 高 | socket_efuns.cc::cleanup_socket_queued_callback 在 main_required 且非 main 时入队；入队返回 0 却在当前线程 free_funp/free_object/free_array | 拒绝/停机分支绕过自身声明的清理线程边界；实际触发条件和同类 adapter 需故障注入 | P1；E 前置 |
| I13 高 | frozen_value.cc 的 safe helper 有 depth=8，vm_copy_frozen_svalue/array/mapping 自身递归不带深度；socket callback 直接调用 copy；局部 array/map 只处理 false，不持异常清理 guard | 直接入口不具备安全边界；循环、过深和中途分配异常会绕过前置包装保障。是否由外部协议直接到达另行证明 | P1/P4 |
| I14 中 | pcre.cc::pcre_match_all 用 vector<vector<svalue_t>> 持有复制的 LPC 引用，交给 f_pcre_match_all 组装；C++ vector 析构不 free_svalue | 采集/组装中途抛错不能靠 vector 自然析构回收 LPC 值；需要注入分配失败核验具体分支 | D1 前置并入同一单元 |
| I15 高 | recompile.cc::run_create 在 create 已执行后才检查 O_DESTRUCTED；__INIT 后还可继续 copy_matches。simulate.cc::destruct_object 在事务专用拦截缺席时执行 socket/外部进程清理、abandon_coroutines、object store 标记等副作用 | 检测到销毁不等于能回滚销毁；仅恢复 program/变量不能恢复这些生命周期状态。静态缺口确认，具体自毁/交叉销毁路径待真实测试 | P1 首要事务前置；E |

### 11.2 VM/事务：I01–I05 与 I15 的直接实施步骤

**先实施 I15 的生命周期屏障：**

- 现有 efuns_main.cc::g_recompile_transaction_active 在 master 授权 hook 前拒绝嵌套，不能把这个边界推迟到 prepare。主线程检查后、任何授权 hook 前安装 scoped RecompileExecutionContext，以非空 context 接替现有布尔 guard，不并存第二个活动状态；此时为 Entered，目标集合可为空，失败退出按作用域撤销。prepare 填好目标/staged program 集合，swap 前启用生命周期屏障；Swapped/run_create 才登记出生指针。逻辑 finish/rollback 后停止出生登记，但嵌套 guard 保留到本次清理和退出结束。I05/E 复用同一 context，不增加第二把 owner admission 锁。
- 对事务目标的 destruct/reload/replace_program、owner 转移及改变对象身份/继承绑定的路径，先逐一核对现有 guard；缺少约束的在实际变更入口拒绝，且必须在 on_destruct、关闭 socket、取消协程、改 object store 或排入延迟 replace 队列**之前**拒绝。不只在 efun 包装层检查，也不把 prepare 的一次检查当成 run_create 的保障。事务内部受控 swap 不调用用户可绕过的豁免开关。
- 为保持准备时确定的对象族边界，拒绝从事务目标 blueprint 新建 clone，或将依赖本次 staged program 的新对象发布到目标集之外；在 clone/program 挂接与 live store 发布点检查，分配但未发布的候选正常回收。非 simul 事务不禁止加载完全无关的对象；若现有路径已拒绝则复用它，不新增重复 guard。
- simul 另有不经 funptr 的逃逸：simul_efuns_rollback 恢复旧 num_simul_efun，而新编译代码的 F_SIMUL_EFUN 直接保存索引，不会进入出生链。因此 SimulEfun 事务激活临时 dispatch 后，直到 finish/rollback 稳定前，禁止发起新的 LPC 编译（包括看似无关对象的懒加载）；在 compile_file 改动编译全局状态前拒绝。事务自己的 staged 编译在此前完成；调用/克隆不依赖 staged program 的既有已编译对象仍按其他边界允许。既有长期 sindex 保持稳定，不新增另一套全局索引 tombstone 表。
- run_create 在每次 __INIT/create 返回后、任何变量迁移访问前仍做存活/身份一致性防御检查。防御检查触发属于单元失败，不能把 O_DESTRUCTED 清掉或把对象重新塞回 live store 伪装回滚。正常公共入口应由前置屏障阻止这种状态。
- 范围：recompile.h/cc、VMContext、efuns_main.cc、compiler.cc::compile_file、simulate.cc 及已核实的 replace/owner-transfer/program 发布直接调用者；不修改下游 mudlib。这是有意收紧初始化合同：重编译中的目标不能被销毁、换身份或扩展为未纳入事务的对象族，需更新现有 recompile 文档。
- 在普通可迁移布局的 __INIT、master/simul 的 create 中，分别测自毁、销毁另一目标、调用外部对象反向销毁目标、延迟 replace、owner 转移、clone/新继承对象发布。另测授权 hook 内嵌套 recompile、simul create 懒加载包含新 simul 调用的程序后抛错/重试；禁止编译的阶段不得残留已发布程序，成功/回滚后正常编译恢复。验证非 simul 无关对象的允许行为。单次被拒绝的生命周期操作不得更改 handle/epoch、owner/store、socket/Promise；整个失败事务的 program/变量槽位/引用恢复按下述浅回滚边界验收。随后正常重编译成功，不能只用 C++ 清除 flags 模拟恢复。

**再实施其余 VM 修复：**

1. 先补真实 LPC 反例：带对象变量的匿名函数在成功重编译后调用；新增/删除 clean_up；初始化把 local/functional/bound 指针存到非目标对象后抛错，随后成功重编译再调用。沿用 testsuite/single/tests/efuns/recompile_object.c，辅助程序放 testsuite/clone/，不改外部 save_object.c。匿名闭包在改前没有失效检查这一点必须由反例确认。
2. I01 在 FP_FUNCTIONAL 进入 setup_new_frame 前校验 owner destruct、owner_gen 和事务 Invalid 状态；所有 funptr kind 的 owner_gen 由统一构造 helper 初始化。bind 到不同 owner 记录新的 owner_gen，但保留 invalid，不能把失效指针重新包装成可用指针。更新 function.h 失效注释及归属 API 文档。
3. I02 为事务建立显式状态机；fallible prepare 也用所有权 guard 包住已 add_ref 但尚未 push_back 的 target，以及 obj_vars_init 成功但 migrations.push_back 失败时的变量块。PreparedVariableMigration 的 unique_ptr 本身不等于释放 obj_var_block 的 payload；采用单一析构/转移责任，避免补 guard 后又与原手动清理双重释放。run_create 捕获所有异常后先恢复 VM context，再走同一 rollback，最后按已有错误策略上报/重抛。不得把 std::bad_alloc 变成功或为了构造错误文本再次分配。测试在 swap 后注入非 const char* 异常，并核对 program/变量/dispatch/flags/generation/admission 与所有引用数。
4. I03 将 old_derived_flags 与 new_derived_flags 分开：旧值直接快照，new 值在 prepare 从 staged program 的既有 apply 查找表查询获得；复用/提取现有纯 program 查询，不依赖 P3/E1 才新增的声明身份目录，不临时把 live ob->prog 换成 staged 来调用 function_exists。沿用已有 apply/函数可见性规则，成功只发布 new，rollback 只恢复 old。普通对象、clone family、master、simul 都要验证；destruct 对象不得被回滚复活。
5. I04 将 coroutine 代次统一为 uint64；park 时另 reference_prog(owner top program)，所有完成/取消/destruct/错误路径成对释放。定义 program pin 与 owner top program pin 独立记账，即使两者地址相同也有两次取得/释放；不可用定义程序代替 owner 程序作 guard。C++ 白盒把代次设到 2^32 附近，再走真实 await/resume；继承 async、recompile、replace_program、取消分别验证，不能循环十亿次伪造压力验收。
6. I05 的永久 Invalid 与出生 journal 按 §8 实现；在 E 开启迁移前仍保持成功重编译后旧 local/functional stale 的现有合同。P1 只先完成全 funptr 弱登记、事务 pin 和失败出生失效，不开启 canonical identity。目标外的引用不能只靠扫描变量块找；随后 E1–E4 复用这套登记，禁止再做第二套 journal。
7. 针对 I01–I05 在 src/tests/test_lpc.cc 增加 TestRecompileSafety 前缀的 FailureException、CleanupFlags、EscapedBirthRetry、CoroutineGeneration64、ProgramPinBalance 等拟新增用例；Debug/ASan/UBSan 与 owner 涉及部分的 TSan。分配失败覆盖 prepare/add-ref、run_create、输出/错误转换；无分配段用测试 allocator 断言，而不是仅审查 reserve 调用。
8. 回滚保证仅覆盖 program/dispatch/generation/派生 flags、迁移变量块与其槽位引用、指针事务状态及本事务持有的资源；不承诺任意 LPC 状态快照。copy_matches 使用引用复制，Master/SimulEfun 在 __INIT 前迁入旧值，因此共享 array/mapping 的原地修改可能保留，即使它从目标自身变量可达；create 的外部 IO、非目标对象修改和 reset 调度也不自动撤销。不为修事务指针引入深拷贝或全 VM 写日志。增加“槽位重新赋值回滚、共享容器原地修改不回滚”的成对用例，明确写入现有 recompile 文档与迁移说明。所有清理在状态稳定后执行，不能在协调器 mutex 内释放任意 LPC 对象图。

I15 的拟新增白盒前缀为 TestRecompileLifecycleBoundary；每种真实 LPC 入口至少一项集成反例。其余专项的“自毁/rollback”用例必须按此拒绝合同验收，而不是接受已销毁目标被复活。

### 11.3 连接/安全：I06–I09 的直接实施步骤

- I06：CLOSED 无论 user 是否存在都释放自己的 buffer，取出并清空句柄后再做可能重入的 remove_interactive；关闭/WRITEABLE/RECEIVE 对空 buffer、空 user、重复关闭均一致。不能为修 leak 提前释放仍会用于 flush 的 buffer。将 ascii/telnet 的共同 session teardown 收口为一个 helper，不重写两个协议。验证主动/对端关闭、握手失败、写入失败、半关闭、close→writable→closed 与重复关闭；计数回到基线，ASan/显式泄漏检查通过。
- I07：在现有 interactive 生命周期内增加可取消的 logon event 句柄，不另造全局 session 表。三个入口改用同一个 main-thread schedule helper：创建 event 后把句柄交给 interactive；触发时先 detach/free event 再 logon；remove_interactive 在 FREE(ip)前取消并清空。失败时关闭本次新连接并清理已分配状态，不留下裸回调。测试真实 loopback 快连快断，并用定向事件排序测试确保 close 先于 timer；原生 telnet/ascii 与 WS 两协议均覆盖。仅白盒强行调用已 free 指针不算真实可达性证明。
- I08：TLS client 开启既有 VERIFY_PEER 时，同时校验目标身份：有 SNI hostname 用 SSL_set1_host，无 SNI 则校验本次 connect 的数字 IP SAN；名字是验证目标，不靠反向 DNS 推断。SSL_new/set_fd/set_host/SNI/default CA 等失败必须 unwind SSL/CTX/事件并清晰返回失败，不置 DATA_XFER。保留既有显式关闭验签选项的合同，但不能因此跳过 SSL 资源错误检查。证书 hostname 不匹配、IP SAN 不匹配、未知 CA、过期及有效证书各测；仅握手成功不等于验证通过。
- I08 的 protocol policy：普通 TLS client/server 与 bundled WS 统一 minimum TLS1.2，允许 TLS1.3；不再用零散 NO_TLSv1_x bit 模拟最小版本，不新增降低到 TLS1.0 的 fallback。这是有意的安全/兼容变化，旧客户端拒绝需列在迁移说明；用能提供旧协议的受控测试 peer 证明拒绝，若测试 OpenSSL 根本不支持旧协议则不能把其本地失败算 driver 验收。policy 在 H 的新旧 context 共用一份。
- I09：新增启动配置 websocket trusted proxy cidrs（拟配置名），默认空。值为逗号分隔的数字 CIDR（IPv4/IPv6），配置时校验并编译，非法配置启动失败；不做 DNS、不把所有 loopback/内网默认当可信。只用真实 peer 地址判定可信，IPv4-mapped IPv6 统一规范化。可信 peer 才可应用唯一、完整的 X-Real-IP 数字地址；可信来源的非法/重复/过长头拒绝连接，非可信来源一律忽略该头并保留真实 peer。保留 peer/effective 两个事实，后续审计能追溯，不能覆盖唯一的原始地址。
- I09 范围：rc.cc/配置声明、port_def_t 的必要不可变配置、ws_ascii.cc/ws_telnet.cc 的共享取址 helper、现有配置说明与 gateway-security.md。测试 IPv4/IPv6/映射地址、可信/不可信代理、有/无/坏头及握手拒绝后释放。新默认会改变依赖任意来源伪装地址的部署，实施前需填写实际代理 CIDR；本文不写真实网络信息，也不直接修改下游配置。

I06/I07 的共同根是 session 资源与异步事件所有权，不应各加一个局部 if 完事。I08/I09 的安全限制只落在真实建连/取址路径，不能只在测试 wrapper 或预检查中生效。

### 11.4 owner/冻结值：I11–I13 的直接实施步骤

1. I11 以现有 coordinator mutex 保护一个只含数值的只读 snapshot。已有持锁路径使用明确的 locked helper，外部读者先取一次 snapshot 再格式化日志；避免 getter 自己加锁导致原先持锁调用死锁。检查每个 getter 直接调用者及 timeout 日志，不把全部 counters 改 atomic 而留下组合状态不一致。白盒并发 claim/timeout/snapshot，加真实 owner 压力的 TSan；WSL2 的 setarch 运行约束照既有规则记录。
2. I12 给 main-required cleanup 独立于业务 admission 的可靠交付路径，复用现有 main cleanup adapter。SocketQueuedCallback 在 capture 的可失败阶段预留 cleanup record；提交清理不得再分配。队列关闭顺序：拒绝新业务→停止/回收 worker→main 排空已承诺 cleanup→销毁对象/上下文。入队失败绝不在 worker 就地 free 普通 LPC 引用，也不故意泄漏来掩盖竞态。必要的 shutdown 尾部链由现有 coordinator 持有，不在每个 package 新建私有兜底队列。
3. 审计同一 cleanup API 的所有调用者，至少覆盖 socket、async、Promise/external 回调的取消/入队拒绝；每次将任务所有权明确标为 caller/queue/main-consumed，正好转移一次。故障注入 callback 清理入队拒绝、owner 迁移/失效、destruct、thread_stopping，验证执行线程、pending 数、free 次数及 shutdown 无遗留。不能声称现有其他 package 都有相同 bug；只有复核命中的路径才修改。
4. I13 将深度校验合并到实际 copy 的单一递归/工作栈实现；入口创建 budget/context，子调用只传递 context，不重新从 0 开始。保留默认最大深度 8，以及明确使用自定义 max_depth 的现有调用者；目录查明全部调用点后逐项指定参数，不把合法更深的合同悄悄收紧。每个待构造 array/mapping/临时 key 都有异常释放 guard，失败不发布半成品 dest。
5. 不增加可变对象跨 owner 共享：string key 与 payload 类型规则不变，object/function/class 仍拒绝，socket 自有 buffer 深复制规则仍由该适配层承担。循环与 8/9 层、重复 DAG、空值、非法 key、非 frozen 值、copy 中途分配异常全部测试；直接调用 vm_copy_frozen_svalue 也必须受约束，不能只测 vm_clone_frozen_value。
6. 节点/字节/批次预算在 P4 按真实调用者分布确定，不假称 depth=8 已限定总内存；也不能用无穷大预算保护一个公开 raw copy 入口。P1 完成深度/异常闭环，P4 才以明确的统一 budget 结构扩展节点/字节参数与超限错误。

### 11.5 测试与 PCRE 所有权：I10/I14

I10 分三步，不假装本轮已修改 CMake 或已有下述 runner：

1. 每个现有 target 分别 gtest_discover_tests，使用 TEST_PREFIX 隔离同名 case；本地链接 GTest::Main，并非自定义 driver main。发现阶段检查 --gtest_list_tests 不进入 fixture 的 driver 初始化，也没有静态初始化写入；列举仍属于执行二进制，不放进只读审计。GTest 缺失应明确 blocked，不拿空 CTest 作为成功。构建 lpc_tests/ofile_tests/compile_arena_tests 后各列非空 case，再证明 CTest 的三组集合与各二进制列举一致。
2. 增加测试专用 FLUFFOS_TEST_MUDLIB 输入，在拟新增 src/tests/test_mudlib.h 收口解析，由 lpc_tests/ofile_tests 的 fixture 初始化复用；只影响测试程序，缺省保持既有 TESTSUITE_DIR，设置后必须是存在的绝对目录、含 `.fluffos-test-sandbox` 文件且内容严格为 `version=1\n`，canonical path 不等于源码 testsuite；非法值拒绝而不退回源码。fuzz/bench 中实际写 mudlib 的入口复用同一 helper；无写入的 arena 测试不强加 driver 依赖。runner 同时设置 cwd 与该变量，不能误以为只改 CWD 就覆盖编译进宏的路径。
3. 拟新增 tools/testsuite/run-targeted.py，作为唯一新定向 runner。复用/抽出既有脚本中适用的配置处理，不改旧 run-isolated.sh 参数合同。CLI 四种互斥模式：--binary 配 --gtest-filter；--ctest-dir 配 --ctest-regex；--driver 配 --case（对象路径）或 --all-lpc；--tool 指定本方案内的 bench/fuzz 可执行文件，-- 后参数按 argv 原样传递、绝不经 shell 执行。工具模式不假造 GTest 参数或用例数，按 P4/fuzz 自己的结果合同验收。公共参数 --timeout 为正秒数、默认 180；全量运行需显式提供合适预算。每个实际测试进程新建唯一临时根，复制受控的 testsuite/src/www、写沙箱标记、只改副本配置；统一 loopback/测试端口、日志与环境，不接收现有运行 mudlib 作为写入根。C++ 使用其实际读取的 etc/config.test，不能仅新增 config.absorb 就宣称 C++ 配置隔离。符号链接/特殊文件、源目录并行改变按 §3 拒绝，保留证据目录，不自动删除。
   - GTest 模式先在同一二进制枚举并断言过滤器非空，再运行；枚举不初始化 driver。CTest 模式从已配置目录用 --show-only=json-v1 和 -R 取得精确非空集合，逐项用转义且锚定的 -R、--parallel 1 运行，每项另建沙箱；不让多个 CTest 子进程共写一个临时 mudlib，也不依赖新版本 CMake 的 TEST_LAUNCHER 特性。外层选择不得隐式扩大到全仓测试。
   - 每次记录源码/二进制身份、命令、退出码、通过/失败/跳过数量、超时和原始日志。测试模式中 0 用例、全跳过、缺 package 均失败；所有模式的 sanitizer 诊断、结果合同与退出码矛盾均失败，工具模式不能仅以退出 0 宣称 benchmark 有收益。LPC 校验沿 §3 的真实 runner 输出。超时必须终止并回收本次进程树，不能只杀父进程遗留外部子进程；实现 Windows/Linux 对应的进程组/Job 归属机制，不按模糊进程名杀进程。
   - runner 自测覆盖非法 CLI/过滤器、零用例、非零退出、skip、timeout 及带子进程的超时回收、两次运行目录不同、源码不被写入；随后三个真实 GTest 目标和一个真实 LPC 对象通过。先验证 helper/runner，再将后续单元迁到此入口。复制、进程启动和日志生成属于写操作；清理只处理已核验属于本次的临时目录。

I14 并入 D1，不在 PCRE1 写一套随后丢弃的引擎：用受管 svalue 集合或现有 array 容器接管匹配结果；每个元素在 push/扩容/结果数组分配失败时恰好释放一次，移动转交后原槽清零。区分 compiled code、match_data、LPC capture、转换 buffer 四种所有权。覆盖第一个/中间/最后一次结果分配失败、空匹配、非法 UTF 后的已收集值及 callback 抛错；ASan、debugmalloc 引用数和真正启用的泄漏检查分别记证据，不把 vector 析构当 proof。

### 11.6 覆盖边界与尚不能定性的候选

本轮覆盖：编译器语法/生成链/诊断边界、VM funptr/Promise/重编译、owner admission/cleanup/frozen copy、网络会话/TLS/地址信任、PCRE/Unicode、CMake/tests/CI依赖入口。**不是整仓每一行无缺陷认证**；第三方库全量漏洞、每个可选 package、真实下游 mudlib 和容量没有被跑过。

以下进入 P4 的“先测再决定”，不混入已确认 bug 数：

- WS evbuffer_add 未在协议写入口体现总队列预算；需要沿 comm/gateway/socket 的真实背压链核对每连接、owner、全局和未握手资源上限。禁止未测就给出容量值，或直接丢数据/无限增长当兼容。
- 单次 PCRE match_limit、eval cost 与整个批次 wall time 不等价；frozen DAG 的复制扩张也不由深度独自限定。二者都要节点/字节/批次证据，不靠微基准平均数说明最坏情况安全。
- 重编译仍扫描 obj_list；这是 FROZEN 下的对象族枚举，不因存在 legacy list 就改成另一张常驻索引。先测总对象数/同族 clone 数与 freeze 时长，再决定是否用现有 owner-sharded store 增加 family 查询；不得恢复全局 ObjectTable 为权威。
- 编译 arena、pending-free worklist、destruct drain、callout/heartbeat、诊断渲染与 owner 调度只收集真实热点；没有 profile 证据不换分配器、不合并内存池、不为预想的核数重写调度器。
- 历史 backlog/可选 note 不自动等于当前缺陷。动态诊断字符串继续遵守“格式串固定、动态文本作数据”的既有规则；历史修复若已存在，不重复移植。未复现/已不适用的候选在账本写理由，不保留伪待办。

## 12. 大改执行路线、验收和回退

### 12.1 阶段依赖与原子交付

下面是后续实施计划，不是本轮执行记录。实施授权后按表连续推进安全独立单元；遇到真实阻塞只停止受影响链。任何代码更改前仍须完成该单元现状/调用者/最窄 fail-first 复核，批准后不得实施时擅自改变公共合同。

| 阶段 | 交付单元与先后顺序 | 阶段出口 |
|---|---|---|
| P0 验证可信度 | 固定并保留 dirty 基线→I10 注册/helper/runner→依赖与 baseline 记录 | 三组测试非空且可隔离执行；记录现有失败。外部改动未明确归属不阻塞无交集的本地单元，但阻塞对混改文件的修改及提交 |
| P1 基础安全 | I11/I12→I15 生命周期屏障→I01/I02/I03/I04→I05出生 journal；I06/I07→I08/I09；I13深度/异常 | 事务失败恢复、引用/线程边界、连接释放和安全反例通过；不开放新 E/D2/H API |
| P2 吸收已有修复 | A、B、F、G、D1（含 I14）逐个原子交付；Windows C 独立 | 现有用户合同保持；PCRE1 退出链接；grammar 源/产物一致；平台缺席明确待验 |
| P3 功能升级 | P1+ B 后 E1→E2→E3→E4；D1 后 D2；I06–I09 后 H1→H2→H3 | 每个专项的失败/回滚/跨模块集成通过，再更新对应归属文档 |
| P4 资源/性能/集成 | §11.6 先测；只实现证据支持的预算/热点优化；最后一次受影响集成矩阵 | 没有正确性换性能；默认/边界/失败/恢复路径均有当前证据；未测平台单列 |

I15、I05 和 E 共用一个执行 context、目标边界和出生 journal，不重复开发；D1 和 I14 共用结果所有权改造；H 和 I08 共用 TLS policy；I06/I07 共用 session 生命周期。每次只认一个权威实现，避免“大改”变成叠加多套旁路。

每个原子单元固定顺序：读归属文档/所有直接调用者→按 §3.2 建立反例或相匹配的基线证据→最小实现→必要生成→定向测试→相关 sanitizer/故障注入→只读 diff/证据复核。需要提交时只含本单元授权文件，是否 commit/push 另按当前授权处理；默认不触发远端 CI、不部署、不重启游戏。P0 不得将用户的两处外部改动偷偷复制成“本次修复”。

### 12.2 性能与资源的可执行测量合同

1. 选择同一机器、相同编译器/依赖/优化选项的 base/candidate；记录源码 SHA+未提交补丁摘要、二进制 hash、CPU/内存、频率策略、owner 模式、测试数据、所有开关。现有性能数字不能跨版本套用。
2. 只构建当前被测目标：owner_runtime_bench、lpc_vm_bench、object_store_bench、bench_compile、bench_scratchpad、bench_diagnostic_render 中实际相关的目标。首轮先读取各目标真实 CLI，记录 --help/无参数的实际退出合同，不臆造参数。禁止一边构建一边跑 driver/bench。
3. 正确性先过，性能使用非 sanitizer 优化构建；至少 1 次预热和 5 次独立采样，交替 base/candidate，保留原始输出，给 median 与范围。若工作负载确实产生足够延迟样本，再报告 p95/p99；5 个总耗时不能冒充 p99。
4. 最小工作负载：无/多 owner、小/大对象族热重编译、继承/匿名/命名调用、短/大/退化正则、浅宽/深 frozen payload、慢读/断线/碎片 WS、TLS 全握手/恢复握手/reload。输入规模和预算必须写明，达到内存保护阈值立即停止该样本而非压垮宿主机。
5. 资源观测同时记录 RSS 峰值、活动/待清理对象和任务数、program/funptr 引用、session/SSL/CTX/evbuffer 数、队列长度与拒绝原因、freeze/主线程停顿。RSS 不降不自动等于泄漏；对象计数不回归也不能被“分配器缓存”一句话掩盖。
6. 预算方案先填 workload 实测峰值/最坏增幅与可接受主线程停顿，再统一落到已有配置/入口；未填完不能实施拍脑袋阈值。新预算的超限动作必须明确为拒绝/延迟/关闭中的一种，并有可观察错误与恢复测试；不能静默截断 LPC 值或丢可靠消息。
7. 不预先承诺提速百分比。只有超出采样噪声且不损害慢路径/资源上界的变化才保留；无收益的复杂优化撤回本单元，保留正确性修复。错误率、超时和丢消息必须同时为合同允许范围，而不是只比较吞吐。

### 12.3 合并后的最小集成矩阵

- 每个单元仍只跑最窄定向验证；P1/P2/P3 相应阶段完成后，才串行扩大到真实 driver 的相关整组 LPC 与三个 C++ 目标。大改全部完成后在隔离 testsuite 运行一次完整 -ftest，不能把它放入每次只读复核或每次小改。
- F/E/I01–I05/I15 必测默认参数、private/inherit、async/await、旧变量布局、simul reload、master、生命周期越界拒绝及错误恢复；G 同时检查 Bison/fallback。D1/D2 测 package 开/关、动态/静态链接和有限内存失败。
- I06–I09/H 使用临时 loopback 端口与测试 CA，覆盖明文/TLS 原生端口和 WS ascii/telnet；没有生产负载/真实平台就不写生产容量结论。
- ASan/UBSan 针对实际触达内存路径；TSan 针对 admission、cleanup、registry；libFuzzer 只在相关 gateway/输入边界确被修改时运行其已有目标。系统库未插桩、LSan 关闭、Windows/Alpine/macOS 缺席均是证据范围限制，不用 fake passed 补齐矩阵。
- 现有 master::flag 与 tests.c runner 的退出/汇总检查沿 §3；零用例、全跳过、不支持的 package、仅编译而未执行必须区分。任何测试文件自身编译失败都不是“预期行为失败”的 fail-first。

### 12.4 最终交付口径

代码阶段的交付是：本文件各单元有对应实施记录，归属 API/运行合同更新，定向/集成验证可复现，未验平台和测量候选可见。审计中未证实的风险可以经反例证明不适用后关闭，不能悄悄删除；批准的修复不能用重命名、文档补丁或部署完成来替代。

方案阶段的交付是：专项设计、跨模块问题、依赖、文件范围、测试/异常路径与外部副作用边界已写清；**不代表源码已经优化，也不代表整仓审计证明无漏洞**。准备大改不要求一次不可回退的巨型提交，按上述原子单元执行才能定位回归。

### 12.5 最终审计与实施准入

本次最终静态审计补齐：授权 hook 前的嵌套事务边界、simul 临时索引经新 bytecode 逃逸的隔离、浅回滚与共享容器边界、迁移块分配失败所有权、声明目录的正常发布路径、D1/D2 依赖一致性，以及原先只提到名字却未定义的定向 runner。原有 38 项上游账本与 I01–I15 保留；这些收口不冒充新增运行时验证。

**结论：方案可以进入分阶段实施。**这表示取得实施授权后可从 P0 的无冲突单元开始，不表示越过 E 的混改文件归属、D1 的依赖、C/H 的目标平台或 P4 的测量门禁。遇到这些条件只停止相应单元，不要求在开始 P0 前先完成整个项目的动态验收；也不能因准入结论就标记尚未实施的单元完成。源码实施不自动授权提交、推送、远端 CI、部署或重启。

## 13. 实施记录（2026-10-02 起）

### P0 / I10：测试注册、隔离 helper 与定向 runner

- **状态**：`complete`（P0 入口基线和 I10 本地交付已通过；后续行为单元仍按依赖顺序推进）。
- **实际改动**：
  - `src/tests/CMakeLists.txt` 将 `lpc_tests`、`ofile_tests`、`compile_arena_tests` 分别注册 `gtest_discover_tests`，使用独立 `TEST_PREFIX`，不再把三个二进制错误地作为一个 target 参数传入。
  - 新增 `src/tests/test_mudlib.h`；`lpc_tests`/`ofile_tests` 及实际会初始化 driver 的 bench/fuzz 入口统一读取 `FLUFFOS_TEST_MUDLIB`。未设置时保留编译时 `TESTSUITE_DIR` 默认；显式值必须为绝对、存在、非源码 testsuite、含精确 `.fluffos-test-sandbox` 标记的目录，非法值拒绝且不回退。
  - 新增 `tools/testsuite/run-targeted.py` 与 `test-run-targeted.py`。runner 的 GTest、CTest、driver、tool 四种模式互斥；先做非空发现，所有实际进程使用独立 testsuite/src/www 副本、独立配置/loopback 端口/日志和进程组超时回收；记录命令、源码 HEAD、二进制 SHA-256、退出码、耗时、通过/失败/跳过、超时及原始日志。tool 模式只报告执行，不声称性能收益。
- **需求/根因**：原 CMake 调用只把 `lpc_tests` 当 discovery target，导致此前 CTest 仅发现 472 个 `lpc_tests` 用例，`ofile_tests` 的 2 个和 `compile_arena_tests` 的 8 个没有进入集合；C++ fixture 的 `TESTSUITE_DIR` 又被编译期常量锁定，切换 cwd 不能隔离其写入 mudlib。
- **基线反例与证据**：修改前 `build-dev-debug` 的 `ctest --test-dir build-dev-debug -N` 记录为 472 个测试，单独 `--gtest_list_tests` 记录为 `lpc_tests=472`、`ofile_tests=2`、`compile_arena_tests=8`；日志见 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/baseline/ctest-before.txt` 和 `gtest-before.txt`。这一区别是测试设施基线，不冒充运行时行为反例。
- **验证**：
  - `python3 tools/testsuite/test-run-targeted.py`：4/4 通过，覆盖非法 CLI、零用例、非零退出、全跳过、超时和独立证据目录/源文件不变；日志为测试进程输出。
  - `cmake -S . -B build-absorb-p0 -DCMAKE_BUILD_TYPE=Debug -DENABLE_LTO=OFF -DMARCH_NATIVE=OFF -DBUILD_TESTING=ON`；`cmake --build build-absorb-p0 --target lpc_tests ofile_tests compile_arena_tests --parallel 4`；`cmake --build build-absorb-p0 --target driver --parallel 4`，全部退出 0。一次早期 helper 编译错误（错误使用不存在的 `fs::is_absolute`）已按日志修正，修正后重建通过；原始日志见 `p0-build.log`/`p0-build-retry.log`/`p0-driver-build.log`。
  - `ctest --test-dir build-absorb-p0 -N` 与 `--show-only=json-v1`：482 个，分组为 `lpc_tests=472`、`ofile_tests=2`、`compile_arena_tests=8`；目标集合非空。`run-targeted.py` 实际运行 `lpc_tests` 1/1、`ofile_tests` 2/2、`compile_arena_tests` 8/8，均通过；CTest 模式另逐项运行 compile-arena 8/8，通过。证据目录分别为 `p0-run-lpc`、`p0-run-ofile`、`p0-run-arena`、`p0-run-ctest-arena`。
  - `run-targeted.py --driver build-absorb-p0/bin/driver --case /single/tests/efuns/package_trim --timeout 180`：1/1 LPC 对象通过，driver SHA-256 为 `58fca5160924f7d4f58d5e728a2eacb2e13f74ad50ab3fa734f8e0acfe921954`；证据目录为 `p0-run-driver-trim-2`。
  - 设置不存在的 `FLUFFOS_TEST_MUDLIB` 运行真实 `DriverTest` 返回非零，并明确报告 override 目录错误；未回退到源码 testsuite，日志为 `p0-invalid-mudlib.log`。
- **限制/剩余依赖**：本单元尚未运行 sanitizer；完整三目标 CTest、后续新用例和最终隔离 `-ftest` 仍由相应阶段门禁覆盖。P1 可统一使用此 runner；既有 `object.cc`/`save_object.c` 外部改动未触碰。

### P1 / I11：owner runtime 组合 snapshot

- **状态**：`complete`（源码、直接调用者、正常定向验证和 TSan 定向验证通过）。
- **实际改动**：`src/vm/internal/owner_runtime_coordinator.{h,cc}` 用同一 coordinator mutex 提供 `OwnerRuntimeCounterSnapshot`/`snapshot()`/`snapshot_locked()`，移除无锁数值 getter；持锁状态机只调用 `snapshot_locked()`，外部超时日志先获取一次 `snapshot()`。`owner.cc` 的 runtime status 在既有锁作用域内复制一次组合计数，避免多个 getter 跨越 claim/release。
- **需求/根因**：I11 的反例是 worker `claim_end()` 修改普通 `uint64_t` 时，`f_recompile_object()` 在 quiesce 返回后读取无锁 `active_owner_claims()`；此前不是组合值一致性问题，而是明确的数据竞争。单一 snapshot 保留普通整数和状态机锁语义，避免 getter 自锁导致已有持锁路径死锁。
- **反例与验证**：新增 `TestRecompileQuiesceSnapshotSerializesClaimAndTimeout`，在 coordinator 锁保护下注入 active claim，真实 `vm_owner_recompile_quiesce_begin(1ms)` 返回 timeout，再 `claim_end()` 并验证 active=0、attempt/timeout 计数递增；`TestRecompileQuiesceCountsSuccess` 改为验证同一 snapshot。源码检索确认生产代码不再直接调用旧 getter。
- **验证命令与结果**：`cmake --build build-absorb-p0 --target lpc_tests --parallel 4` 退出 0；正常 `lpc_tests --gtest_filter=<I11/I12 10 cases>` 为 10/10 passed；`setarch x86_64 -R cmake --build build-absorb-i11-tsan --target lpc_tests --parallel 2` 退出 0；同一 10 cases 的 TSan 运行为 10/10 passed、无 TSan 报告。源码 HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；dirty patch SHA-256=`235580b024470ebb85d5e8a303d4f80f1b07330ad5d1310933fc5761433a7f6c`；normal/TSan binary SHA-256 分别为 `27a5fece92fb3924bcfea28ad4fa68864017dfe640fac37f233f6541fc8d3b6c` / `eb2b72fac40dd021bec39950646335f6a96c89ce6d1bd919d494e28da5f6b548`。原始日志：`/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i11-i12/{normal-build.log,normal-targeted.log,tsan-build.log,tsan-targeted.log,identity-after.log}`。
- **限制/剩余依赖**：未经 `setarch x86_64 -R` 的 WSL2 TSan 进程稳定以 exit 66 报 `unexpected memory mapping`，原始证据为 `tsan-wsl-fail.log`；这是既有环境限制，不记为代码通过。I11 所属 P1 仍依赖后续 I15 及其余 P1 集成矩阵。

### P1 / I12：main-required callback cleanup 可靠交付

- **状态**：`complete`（命中 caller 已迁移至预备节点；正常/TSan 定向验证通过）。
- **实际改动**：`VMOwnerCallbackCleanupRecord` 在 `src/vm/owner.h` 中承载预先准备的固定函数指针/context；`owner.cc` 以无分配 intrusive atomic stack 接收入队，主线程 drain 时 detach/reverse/执行，并把 prepared backlog 纳入 status/drain 结果。socket capture 在可能分配 args 前准备 cleanup record；async（含 Promise）、DNS、socket 和 callout 的取消/过期/执行后清理全部转为该路径。callout record 在生命周期结束时恰好释放一次；callback 可以删除所属 record，drain 不再访问其内容。
- **需求/根因**：I12 的反例是 `cleanup_socket_queued_callback()` 在 main-required 且非 main 时，旧 API 入队返回 0 就当前线程 `free_funp/free_object/free_array`；这会在 worker 触碰普通 LPC 引用。新提交不依赖 owner admission、锁竞争或分配，入队失败不再有 worker 就地释放旁路；所有命中的 package callback 通过 main cleanup adapter 消费。异步 Promise/external 相关调用者完成了同一 ownership 审计，未把未命中的 package 声称为同类缺陷。
- **反例与验证**：新增 `TestVmOwnerPreparedCallbackCleanupSurvivesCallbackDestruction`，预备节点经真实 main drain 执行 callback，callback 删除 record 后测试仍完成，覆盖 drain 的 lifetime boundary；既有 callout/DNS/async/socket dispatch 与 stale-owner tests 保持通过。普通构建定向集合为 10/10，TSan 定向集合为 10/10；包括 prepared cleanup、callout、DNS、async、socket、stale drop、executor boundary。
- **验证命令与结果**：与 I11 共用上述 normal/TSan build 和 raw logs；`normal-targeted.log`、`tsan-targeted.log` 各记录 `10 tests ... [ PASSED ] 10 tests`。首次不带 `setarch` 的 TSan `--gtest_list_tests` 以 exit 66 失败，原始日志 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i11-i12/tsan-wsl-fail.log`；按项目 WSL2 约束改用 `setarch x86_64 -R` 后构建/运行通过。代码边界未修改用户已有 `src/vm/internal/base/object.cc` 或 `testsuite/single/tests/efuns/save_object.c`。
- **限制/剩余依赖**：没有伪造 allocation-failure 或 Windows/macOS 外部平台证据；prepared callback 只覆盖本次审计命中的 async/DNS/socket/callout 所有权路径。I15 及后续 P1/P2 集成仍未完成，本记录不把整仓或最终 shutdown 矩阵标为通过。

### P1 / I15：第一次实现失败的只读根因与执行修复计划

- **状态**：`blocked/root-cause-found`。本节是在连续本地修复尝试后暂停代码修改期间追加；在本节的两轮只读审计完成前，不得继续改动 I15 源码或测试。
- **失败反例**：在当前源码 HEAD `fe163d67f40cef6d445f076e4a2d565799cd4627`、ASan driver SHA-256 `07be8a5e7f4286cd37ad15f0253fe1f0f36e20d68a22bde5118b814b55527800` 下，从隔离 sandbox 使用 `etc/config.recompile` 执行最小 LPC 对象 `recompile_trace_min`：先调用 `recompile_object(master())` 成功，再在同一个 LPC 调用中 `catch(error("trace probe after master reload"))`。进程退出非零并触发 `AddressSanitizer: heap-use-after-free`，读取点为 `src/vm/internal/trace.cc:32::get_trace_details`，释放点为 `src/vm/internal/recompile.cc:722::release_pin_and_snapshot_refs` 经 `commit_finish()`；原始报告为 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i15-recompile-gate-asan/sandbox-001/trace-min.stderr`。同一报告的释放栈保留 `safe_apply_master_ob()` 到 `driver_main()`，证明测试命令仍在外层 `master::flag()` apply 内运行；不是 simul 编译屏障或后续测试造成的偶发崩溃。
- **最小化确认**：隔离版本只保留“master 成功重编译→立即产生 LPC error/trace”，不包含 simul、生命周期 probe、nested probe 或多次重编译，仍在 `trace.cc:32` 复现。因此根因已经从“simul 事务/rollback”缩小为 master swap 时的活动 master frame。
- **根因**：`src/vm/internal/recompile.cc::snapshot_recompile_targets()` 以 `RecompileExecutionContext::allows_authorization_frame()` 为条件，跳过了目标 program 的全部 `current_prog`/control-stack 检查；`efuns_main.cc` 在授权前记录的 `caller_program != old_prog` 只能说明调用者不是 master，不能识别“只应短暂存在的授权 hook frame”，更不能排除外层 `master::flag()` frame。实际 `safe_apply()` 在返回前已经由 `apply_low_impl()` 弹出授权 hook；快照阶段仍在栈上的是真实外层 `/single/master` frame。随后 `commit_finish()` 释放旧 master program，而 `get_svalue_trace()` 仍通过 `p[1].prog` 读取该 frame，形成确定性 UAF。`program_t::ref` 不能替代活动解释器 frame pin；按合同应在 swap 前拒绝活动目标 frame，而不是延期释放或给所有 master frame 加兼容豁免。
- **合同依据**：`docs/recompile-object-v2-design-2026-08.md` §1.3/§2.1 明确 master 只能由第三方在没有 master 活动 frame 的窗口重载，self/active-frame reload 必须得到稳定的 `recompile_object target program is executing`；`docs/evidence/l7-master-reload-rounds.md` 也明确 `ftest` 经 master apply 时只能验证该拒绝，driver-side 才验证无 master frame 的成功管线。当前新增的 `recompile_object.c` master 成功调用违反了这个测试入口边界，不是应由执行 guard 放宽的合法例外。

#### 直接可执行的修复步骤（根因修复，不引入旁路）

1. **收紧唯一执行 guard**：在 `src/vm/internal/recompile.h` 删除 `RecompileExecutionContext::begin_authorization()`、`allows_authorization_frame()` 及其两个 authorization 指针字段；在 `src/vm/internal/recompile.cc` 删除对应实现。`snapshot_recompile_targets()` 无条件执行现有 `current_prog == old_prog` 检查和全部 `control_stack` frame 检查，保持统一错误前缀 `recompile_object target program is executing`。不新增 program 引用计数、延迟 free、第二个 guard 或 master 特判；不改变目标族遍历、owner quiescence、simul 编译屏障和事务 context。
2. **移除错误豁免调用点**：在 `src/packages/core/efuns_main.cc` 删除 `caller_program` 局部变量、授权前 `begin_authorization()` 调用及“跳过预期授权 frame”的注释；授权 hook 仍在 Entered context 内 fail-closed 执行，hook 返回后才进入 quiesce/prepare。删除 `efuns_main.cc`、`recompile.cc` 和已命中的其他 I15 文件中本轮临时 `std::fprintf("I15 ...")` 调试输出；稳定错误必须走既有 LPC error 合同，不能把地址/phase 写入用户日志。
3. **修正测试入口而非放宽合同**：`testsuite/single/tests/efuns/recompile_object.c` 不再在 `master::flag()` 驱动的 `-ftest` 调用中断言 master 成功或运行 master create rollback；该入口保留/增加 master active-frame 的拒绝断言并在 catch 后执行一次 trace/后续 LPC 语句，作为本 UAF 回归。simul 成功和普通 blueprint/nested authorization 覆盖继续保留，但不能把外层 master frame 当成目标 authorization frame。
4. **保留合法成功覆盖**：继续使用 `src/tests/test_lpc.cc::TestMasterReloadSuccess` 与 `TestMasterReloadStressRounds` 验证无活动 master frame 时的 swap、apply cache、授权 apply 和重复发布；如需覆盖 public `f_recompile_object()` 而不是只调用白盒 transaction，给现有 `testsuite/clone/recompile_lifecycle_probe.c` 增加一个最小 `run_master_recompile()`/错误返回 helper，由 C++ `safe_apply()` 从 driver 主调用进入（不经过 `master::flag()`），临时打开 `CONFIG_INT(__RECOMPILE_OBJECT_ENABLED__)` 并用 RAII 恢复；成功后清理/固定 master program 引用，不能 raw restore 后留下指向已释放 program 的 `master_applies`。生命周期 mode 1–7 的 create 拒绝回归同样从该无外层 master frame 的入口驱动；ftest 只测 active-frame rejection。
5. **不吸收临时诊断文件**：`recompile_trace_min.c`、`recompile_min.c` 及其 stdout/stderr 只留在本次授权 evidence sandbox，不能复制到仓库；ASan 原始报告保留，不删除或覆盖。用户已有 `src/vm/internal/base/object.cc` 和 `testsuite/single/tests/efuns/save_object.c` 仍逐字保留。

#### 修复后最窄验证与阶段出口

- 先只构建受影响目标：`cmake --build build-asan --target driver --parallel 2`；若修改 `src/tests/test_lpc.cc`，另以同一 ASan 目录构建 `lpc_tests`，一次只运行一个构建目录。先跑 fail-first 对应的 `/single/tests/efuns/recompile_special_targets`（`--config recompile`）和 `/single/tests/efuns/recompile_object`，再跑新增/既有 `TestMasterReloadSuccess`、`TestMasterReloadStressRounds`、`TestRecompileLifecycleBoundary` 定向 GTest；每个 runner 记录非空 case 数、pass/fail/skip、退出码、耗时、源码 HEAD、binary SHA-256 和原始日志。
- ASan 必须证明两条相反路径：① `ftest` 内 master reload 在 commit 前返回 `target program is executing`，随后 trace/后续 LPC 正常，零 UAF；② 无外层 master frame 的合法 master success 和模拟 create failure/rollback 仍通过。不能以“把 master reload 一律禁用”或只删掉 trace 调用代替根因修复。之后再跑普通 Debug 定向验证；本阶段不声称 LSan、Windows/macOS 或完整 `-ftest` 已通过。
- 只读复核命令包括 `git diff --check`、受影响符号全局引用检查、临时 `I15` 输出为零、两处外部改动逐字 diff；任何失败立即保留新证据并重新回到只读根因调查，不降低断言、不忽略退出码、不继续试错。
- **阶段出口**：执行 guard 对所有实际活动 target frame 生效；active master ftest rejection 与合法第三方 success 均有独立证据；ASan 运行无 `trace.cc:32` UAF；I15 才能从 `blocked/root-cause-found` 改为 `unverified`，不得直接标 `complete`。I15 的其余生命周期、simul 编译、异常回滚和最终集成门禁仍按 §11.2/§12 执行。

#### 第一次只读审计（控制流/合同）

- `safe_apply()` 的真实实现（`src/vm/internal/apply.cc`）在 `apply_low_impl()` 返回后才返回调用者；授权 LPC frame 已弹出。故意跳过全部目标 frame 没有“保护授权 frame”的必要性。
- ASan 释放栈的外层 `safe_apply_master_ob()` 是 driver 启动 `master::flag()`，不是当前 `f_recompile_object()` 的授权调用；它与 `trace-min` 的最小重现输入完全对应。删除豁免后，该 frame 会在 `snapshot_recompile_targets()` 被拒绝，旧 program 不会进入 `commit_swap()`/`commit_finish()`。
- 无外层 target frame 时，`current_prog` 和 `control_stack` 中均不等于 old master program；因此统一 guard 不会阻断第三方 master success，也不影响现有 white-box success/stress。嵌套 transaction 仍由 Entered context 在授权 hook 前拒绝，职责没有重叠。

#### 第二次只读审计（所有权/测试与副作用）

- 不采用“给活动 frame 增加 ref”或“延迟释放旧 program”：这会改变现有 VM frame 合同并掩盖非法 swap；稳定拒绝是设计与旧 v1 行为要求。修复只删除一个错误豁免，影响面小且可回滚。
- `recompile_object.c` 的 master 成功/rollback 代码必须移出 `master::flag()`，否则修复后预期会稳定失败；保留该入口的 rejection+后续 trace 才能直接覆盖本次 UAF。合法 success 必须由 C++ 主线程发起的非 master apply 入口证明，不能把白盒 commit-only 测试冒充 public efun 验证。
- 计划未授权 commit/push、远端 CI、部署、重启、下游 mudlib 或删除证据；只创建本任务 evidence sandbox。验证顺序禁止在 driver 运行时并行构建。两处预-existing 外部改动不在任何修复 hunk 内。
- **审计结论**：上述计划同时满足根因、公共合同、活动 frame 生命周期、测试入口和副作用边界；没有发现需要新增权限、兼容别名、第二事务状态或不可逆操作的隐藏依赖。执行前仍须以当前真实 diff 再确认 debug 输出和新增测试 hunk 的归属；执行后按本节命令回到动态证据，不能用本只读审计代替测试。

### P1 / I15：simul 累积表 orphan permanent identifier 的只读根因与执行计划

- **状态**：`blocked/root-cause-found`。上一项执行 guard 修复后，I15 的新定向反例已保留；本节为该失败的只读根因调查和执行前计划。在本节计划的只读审计完成前，不继续修改 I15 源码或测试。
- **失败反例与证据**：当前源码 HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`、driver SHA-256=`cb067a71825faa47342bf3d4a8459634a2d1f2c1a4213b97507a7ba7b31c24b6`，隔离 `etc/config.test`（实际启用 recompile object）运行 `/single/tests/efuns/recompile_special_targets`，业务断言已输出 `8 special reload checks ok`，但退出码为 255。两次 simul reload 后，debugmalloc 在 `check_memory()` 报两条 `WARNING: Found orphan permanent identifier: find_or_add_perm_ident:2 0211` 和 `*LEAK`；一次 reload 的最小副本同样报一条。原始记录、命令、sandbox 和日志分别在 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i15-fix-asan-special/records.json`、`001-driver-lpc.log`、`simul-one.log`。
- **根因**：`testsuite/single/simul_efun.c::defarg_simul(int a: (: 77 :))` 触发 `src/compiler/internal/grammar_rules.cc` 的进程级 `default_arg_seq`，每次编译产生不同的 `#__<seq>_defarg_simul_a` helper。`simul_efuns_prepare()` 按 I15 合同保留旧 `simul_names`/dispatch index，并把失配的旧 helper 标成 `IHE_ORPHAN`；这是为已编译 `F_SIMUL_EFUN` 调用保留槽位，不是可释放的临时节点。`src/packages/develop/checkmemory.cc` 的 identifier mark 只标记 `IHE_SIMUL|IHE_EFUN`，不标记 `IHE_ORPHAN`；`mark_simuls()` 目前只标记 name string。于是仍被 live cumulative `simul_names` 引用的旧 helper ident 被误判为泄漏。一次/两次 reload 的一条/两条告警与该模型一一对应；普通 simul dispatch 断言已通过，故根因不是事务提交、回滚或函数槽失效。
- **反例边界**：不能全局把所有 `IHE_ORPHAN` 标记为存活，否则失败事务中预插入但未发布的 newborn identifier 会逃过泄漏检查；也不能删除旧槽或重用 dispatch index，否则旧 bytecode 的硬编码 sindex 会失效。只应标记 live `simul_names` 中实际保留的永久 identifier。

#### 直接可执行的根因修复步骤

1. 在 `src/compiler/internal/lex.h`/`lex.cc` 增加只读、非分配的 permanent-identifier 查找 helper：按现有 `IdentHash()` 遍历 `ident_hash_head` 链，命中字符串后返回 `ident_hash_elem_t*`，不得调用 `find_or_add_perm_ident()`，不得改变 hash cursor、sem_value、token 或列表。
2. 在 `src/vm/internal/simul_efun.cc::mark_simuls()` 对每个 live `simul_names[i].name` 调用该 helper，并对命中的 identifier block 执行 `EXTRA_REF`；保留现有 name string `EXTRA_REF`。缺失 entry 只跳过并让一致性/泄漏检查暴露问题，不在 mark 阶段分配或创建。
3. 不改 `simul_efuns_prepare/activate/rollback/finish` 的 cumulative-table 语义，不清理 `IHE_ORPHAN`，不改 default-argument helper 命名，不增加第二张全局目录或兼容别名。该修复只补 debugmalloc 的真实根引用，失败事务未进入 live `simul_names` 的 identifier 仍不可达并继续被报告。
4. 增加最窄回归证据：现有 `recompile_special_targets`（两次 simul reload + default-argument simul）必须在 `config.recompile` 下返回 0、打印完整成功 marker 且无 orphan/`*LEAK`；`recompile_simul_one` 验证一次 reload；普通 `recompile_object` 再验证没有误伤其他生命周期路径。若需要 C++ 覆盖，仅添加针对非分配 mark helper 的最小断言，不复制第二套 simul 表逻辑。

#### 执行前只读审计结论与验证顺序

- 已核对 `simul_efun.cc` 的 live table 保留/`IHE_ORPHAN` 转换、`lex.cc` 的 permanent hash 链和 `checkmemory.cc` 的 mark 条件，三处控制流闭合；`defarg_simul` 是实际触发每次新 helper 的源头，非猜测。
- 先修改上述三处归属源码，再只构建受影响 `driver`（ASan 先 `--parallel 2`；不得与 driver 运行并行）；依次用新隔离 sandbox 运行 `recompile_simul_one`、`recompile_special_targets`、`recompile_object`，记录退出码、marker、leak 行、binary/source identity 和原始日志。再用普通 Debug driver 重复 special/object 定向集；失败即保留证据并回到只读根因，不降低 debugmalloc 检查。
- 只读收尾核对 `git diff --check`、helper 的全部引用、`IHE_ORPHAN`/`mark_simuls` 路径、两处外部改动逐字保持；不执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。
- **审计结论**：修复范围是一个现有内存标记边界，保持 owner/transaction/dispatch 合同和失败 newborn 检测；计划无新增权限、公共运行时语义或不可逆副作用。执行前置条件已满足，执行后 I15 仍只能按全部 P1 门禁标记 `unverified` 或 `complete`，不能因该单项通过提前宣称阶段完成。

### I15 follow-up attempt 1：保留失败证据并修正标记 API 边界

- **状态**：`blocked/root-cause-found`；本次尝试不计为通过。已按计划只构建 `build-asan/bin/driver`，但第一次实现把 `EXTRA_REF(BLOCK(ihe))` 用到了 `ident_hash_elem_t*`。`BLOCK()` 只适用于 `stralloc` 的 `block_t` 字符串 payload，不适用于 debugmalloc 的 `md_node_t` payload；`ihe` 不是 counted string。定向 special case 在业务断言 `8 special reload checks ok` 后触发 driver SIGSEGV，说明是标记代码的指针域错误，而非把该崩溃误报为原始 leak 修复。
- **反例与证据**：ASan driver SHA-256=`cb067a71825faa47342bf3d4a8459634a2d1f2c1a4213b97507a7ba7b31c24b6`；命令记录在 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i15-fix-asan-special-after-mark/records.json`，原始输出为同目录 `001-driver-lpc.log`，末尾为 `Segmentation fault (Address not mapped...)`，未运行后续 object case。
- **只读修正结论**：`src/base/internal/md.h` 已有适用于任意 debugmalloc payload 的 `DO_MARK(ptr, kind)`/`PTR_TO_NODET` 合同；`checkmemory.cc` 对 identifier 正是 `DO_MARK(hptr, TAG_PERM_IDENT)`。下一次最小修改只把 `mark_simuls()` 中 identifier 分支改为 `DO_MARK(ihe, TAG_PERM_IDENT)`，保留 `lookup_perm_ident()` 的非分配查找，不再使用 `EXTRA_REF/BLOCK`，不改其他 transaction/table 代码。修正后先重复 special case；仍失败则保留新日志并停止，不叠加试错。

### I15 follow-up attempt 2：成功标记但重复 root diagnostic，未计通过

- **状态**：`blocked/root-cause-found`；`i15-fix-asan-special-after-mark2` 的业务断言和退出码已通过，但不能作为通过证据，因为原始日志包含两条 `Expected node of type 0211: got ... 1211`。`DO_MARK` 不是幂等宏：它要求 node tag 尚未带 `TAG_MARKED`；`mark_simuls()` 先标记 active identifier，随后 `checkmemory.cc` 的常规 `IHE_SIMUL|IHE_EFUN` 扫描再次调用 `DO_MARK`，因此产生了可观察的错误诊断。日志 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i15-fix-asan-special-after-mark2/001-driver-lpc.log` 已保留。
- **只读修正计划**：`mark_simuls()` 只对命中且 token 含 `IHE_ORPHAN`、同时不含 `IHE_SIMUL|IHE_EFUN` 的 permanent identifier 调用一次 `DO_MARK`；active/efun identifier 留给现有 checkmemory 扫描，避免重复标记；name string 的原有标记不变。该条件仍精确覆盖 cumulative live table 保留的 inactive helper，且不会全局标记 orphan 或修改 sem/token。先用同一 ASan driver 重跑 special case，并要求日志同时满足 exit 0、`Checks succeeded.`、无 `LEAK`/`orphan`/`Expected node`/sanitizer，再继续其他对象。

### I15 follow-up attempt 3：rollback provisional simul identifier/name 未释放

- **状态**：`blocked/root-cause-found`。`i15-fix-asan-special-after-mark3` 的 special case 已满足退出 0、marker 和无 leak；随后按计划运行普通 `recompile_object` 时仍失败，故不能把当前 I15 标为通过。业务路径先输出 `I15 simul error=... mode=1`，随后 debugmalloc 报 `Bad ref count for shared string "#__3_defarg_simul_a", is 1 - should=0`（唯一引用来自 `src/vm/internal/simul_efun.cc:281::ref_string`），并报一个 orphan permanent identifier；原始日志为 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i15-fix-asan-object-after-mark3/001-driver-lpc.log`。
- **根因**：`simul_efuns_prepare()` 对每个 fresh dispatch name 增加了一次 `ref_string()`，并预插入一个 permanent identifier，以保证 no-fail activate；`simul_efuns_finish()` 的成功路径把这些引用转移给 live cumulative table，但 `simul_efuns_rollback()`/Prepared `simul_efuns_discard()` 只释放 shadow arrays，没有释放 fresh name 引用或移除仅由本次 prepare 创建的 hash node。失败 create 后该 name 不在 live `simul_names`，其 string ref 和 identifier 都不可达；若只 `free_string`，identifier 的 `name` 会悬空。当前 `simul_efun.cc` 注释把 token-0 node 称为 driver-lifetime harmless，但实际 debugmalloc/identifier mark 已证明这是可观察泄漏且 name ownership 不闭合。
- **反例边界**：不能在 rollback 清理所有 `p->idents`（其中包含旧 live names），不能按内容删除已有 efun/orphan identifier，也不能把 failed fresh dispatch slot 放回 live table；只回收本次 prepare 实际新建的 identifier 和对应 name ref。成功 finish 不得释放这些引用，失败 transaction 外逃的 funptr invalid/journal 合同不改变。

#### 直接可执行的 provisional ownership 修复

1. 在 `simul_efun_prepared_t` 增加本次 prepare 的 provisional-entry journal（name 指针 + 仅当 hash lookup 之前不存在时新建的 `ident_hash_elem_t*`），容量不超过 `num_new`；journal 数组由 `TAG_SIMULS` 分配，在 prepare 记录每个 fresh name 的 `ref_string`，并只记录实际新分配的 ident。existing identifier（同内容但不同编译字符串、efun 或旧 orphan）只释放本次新增的 name ref，不删除 node。
2. 在 `lex.h/lex.cc` 增加按精确 pointer 的 `remove_perm_ident()`，仅允许 `sem_value==0` 且无 `IHE_RESWORD/IHE_EFUN/IHE_SIMUL` 的 provisional node；在 permanent hash ring 中解除链接、同步 head/tail/current cursor，最后 `FREE` node。该 helper 不分配，不改变其他 identifier；调用前后均保持 name 字符串仍有效。
3. 在 `simul_efuns_discard()` 的 Prepared 分支和 `simul_efuns_rollback()` 的 Activated 分支，先移除 journal 中的 newly-created identifiers，再对 journal names 执行匹配次数的 `free_string`，最后释放 journal/dispatch shadow arrays。`simul_efuns_finish()` 和 Activated defensive discard 只释放 journal 容器，不释放已发布名称/identifier。保持 activate 的 no-allocation swap 段不变。
4. 用现有 `recompile_object` 的 simul create failure/rollback 证明 `#__<seq>_defarg_simul_a` 的 ref 回到 0、没有 orphan/`*LEAK`，并重新跑 special 两次 reload 证明成功路径名称仍被 live table 保留；再跑普通 blueprint/lifecycle case。若 hash-ring removal 在动态 compiler entry 存在时命中未预期分支，先保存证据并停止，不用“强制清空”绕过。

- **执行前复核**：`find_or_add_perm_ident()` 的引用模型、`simul_efuns_prepare` 的 line 281 ref、Prepared/Activated/Finalized 三态和 `free_unused_identifiers()` 的 head/tail reset 已读取；journal 只管理本事务新建资源，不改变失败 funptr 的永久 invalid 规则。修改前必须再检查当前 dirty diff 不含用户 `object.cc`/`save_object.c` hunk；构建与 driver 运行严格串行。

### I15 follow-up attempt 4：C++ 回归仍断言已撤回的 provisional identifier

- **状态**：`blocked/root-cause-found`；driver 的三个 LPC 定向用例已通过，但 ASan 白盒集合不能通过，故不把 I15 标为完成。`run-targeted.py --binary build-asan/src/tests/lpc_tests --gtest-filter='DriverTest.TestSimulEfunReloadCreateFailureRollback:DriverTest.TestMasterReloadSuccess:DriverTest.TestMasterReloadPublicEntryOutsideMasterApply:DriverTest.TestMasterReloadStressRounds:DriverTest.TestRecompileLifecycleBoundary'` 为 4/5，通过的四个 master/lifecycle 用例无 sanitizer；失败项在 `src/tests/test_lpc.cc:26563` 断言 `ihe->token & IHE_ORPHAN`。原始日志为 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i15-fix-asan-gtests/002-gtest-run.log`，同时记录了该失败路径因断言提前退出而留下的 `SaveSimulTable` 测试快照泄漏，不能把它误判为运行时 provisional ownership 再次泄漏。
- **根因**：follow-up attempt 3 的已执行设计明确在 rollback 中移除本次 prepare 新建的 permanent identifier 并释放对应 name ref；旧 C++ 测试仍用 `find_or_add_perm_ident(kBad)` 重新创建该名字，再要求新建的 token-0 节点具有旧的 `IHE_ORPHAN` 状态。这不是 driver 的随机失败，而是测试合同与“临时 dispatch slot/identifier 随 rollback 撤回”的当前计划不一致；`TestSimulEfunReloadCreateFailureRollback` 的 26527 段仍可验证 activated 阶段 `IHE_SIMUL`，失败后应查询而不是重新插入。
- **直接可执行修复**：只改该测试的失败后断言：用新增的非分配 `lookup_perm_ident(kBad)`，要求返回 `nullptr`，并要求 `FindDispatchIndex(kBad) < 0`，把注释从“inert residue/dropped-name semantics”改为“failed fresh name and temporary dispatch slot are withdrawn”。不改变 dropped-name 回归（它仍保留 live cumulative slot 和 `IHE_ORPHAN`），不改 `simul_efuns_*` ownership 代码。修正后在同一 ASan binary 上先重跑该单项，再重跑前述 5 项集合；若单项仍出现 LeakSanitizer/identifier leak，暂停并重新审计 hash-ring removal，不叠加修复。
- **证据/限制**：ASan `lpc_tests` 构建退出 0；普通/ASan LPC `recompile_simul_one`、`recompile_special_targets`、`recompile_object` 均各 1/1 通过，`Checks succeeded.` 且无 `LEAK`、orphan、`Bad ref`、sanitizer signature。ASan GTest 集合的失败项为白盒旧断言，不能用 4/5 作为通过；LSan 只有在修正白盒测试后才可重新判定。此次没有 commit/push、远端 CI、部署、重启或下游 mudlib 操作。

### I15 follow-up attempt 5：provisional rollback 合同与白盒断言已同步

- **状态**：`unverified`（本次 follow-up 的根因与回归已闭合；I15 全部生命周期/异常/平台矩阵尚未完成，不能标 `complete`）。按 attempt 4 的最小计划，`src/tests/test_lpc.cc` 失败后改用非分配 `lookup_perm_ident(kBad)`，确认返回 `nullptr`，并确认 `FindDispatchIndex(kBad) < 0`；没有重新插入已撤回的 identifier。新增 `testsuite/single/tests/efuns/recompile_simul_one.c` 作为一次 simul reload 的最小 LPC 回归对象。
- **验证**：`cmake --build build-asan --target lpc_tests --parallel 2` 和 `cmake --build build-dev-debug --target lpc_tests --parallel 4` 均退出 0。ASan runner 对 `DriverTest.TestSimulEfunReloadCreateFailureRollback` 为 1/1（实际执行、无 LSan/ASan/UBSan 诊断），证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i15-fix-asan-gtest-simul-rollback/`；ASan 集合 `DriverTest.TestSimulEfunReloadCreateFailureRollback:DriverTest.TestMasterReloadSuccess:DriverTest.TestMasterReloadPublicEntryOutsideMasterApply:DriverTest.TestMasterReloadStressRounds:DriverTest.TestRecompileLifecycleBoundary` 为 5/5，原始日志 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i15-fix-asan-gtests-after-test-contract/002-gtest-run.log`，无 sanitizer signature；同一过滤器普通 Debug 为 5/5，日志 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i15-fix-debug-gtests-after-test-contract/002-gtest-run.log`。此前同一二进制的 ASan LPC 三对象和 Debug 三对象各 1/1 证据仍保留在 `i15-fix-asan-object-after-provisional/`、`i15-fix-asan-special-after-provisional/`、`i15-fix-asan-simul-one-after-provisional/`、`i15-fix-debug-object/`、`i15-fix-debug-special/`、`i15-fix-debug-simul-one/`；driver SHA-256=`70b75494a553c198e83adfd6eee6dbca64c80523dc6cb517730faa126237116a`，ASan `lpc_tests` SHA-256=`ee355919dac92a8707db82febe88842c3386bb727fd5a16e8f40ec593d3b972c`，Debug `lpc_tests` SHA-256=`f93175ff803bd7ce4a7661799fd9c2d45d15e975f9edfb4f3cb12c8de98c52e5`，源码 HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`。
- **结果与限制**：失败 create 后 fresh identifier 和临时 dispatch index 均撤回；已发布的 cumulative dropped-name slot 仍由独立回归保持。当前证据覆盖 Linux/WSL2 Debug+ASan 的定向路径，不覆盖 LSan 以外的独立 leak harness、UBSan/TSan、Windows/macOS、完整 `-ftest` 或 I15 剩余生命周期矩阵；I01–I05 等后续依赖继续阻塞于 I15 阶段出口。未执行 commit/push、远端 CI、部署、重启或下游 mudlib操作。

### I15 follow-up attempt 6：启用 recompile 配置的当前二进制复核（2026-10-02）

- **实际变更/根因**：无新的运行时代码变更；复核 attempt 5 的白盒断言同步和 provisional rollback 实现。此前误选 `/single/tests/efuns/simul_efun` 被 runner 正确拒绝，因为该路径是 `/single/simul_efun.c` sefun 实现而不是可独立 `-ftest` case；该失败证据保留于 `/tmp/fluffos-xk-i15-simul-debug/`，不作为源码失败或通过计数。
- **验证**：当前源码 HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`。`cmake --build --preset dev-debug --target driver --parallel 4` 成功；Debug driver SHA-256=`f6be1b844a64bf4ef077db8ee9567432af20f6e4a638f068457c5b717b538482`，运行 `run-targeted.py --driver build-dev-debug/bin/driver --case /single/tests/efuns/recompile_object --config config.recompile` 为 selected/passed 1、failed 0、skipped 0，`Checks succeeded.`，日志 `/tmp/fluffos-xk-i15-recompile-enabled-debug/001-driver-lpc.log` 明确包含 `I15 simul error="*recompile_object: create() failed, transaction rolled back`。ASan driver SHA-256=`70b75494a553c198e83adfd6eee6dbca64c80523dc6cb517730faa126237116a`，同一 enabled case 为 1/1，证据 `/tmp/fluffos-xk-i15-recompile-enabled-asan/`，无 sanitizer signature；Debug `recompile_special_targets` 为 1/1，证据 `/tmp/fluffos-xk-i15-special-debug/`。
- **状态/限制**：`unverified`。本次补足的是 `config.recompile` 下 enabled create-failure rollback、simul probe、200 次重复 recompile 和 Debug special 路径；仍不把 I15 全部生命周期/异常矩阵、P1 阶段、Windows/macOS、UBSan/TSan 或 full LPC matrix 宣称完成。未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。

### I15 follow-up attempt 7：补齐真实 master create 生命周期入口矩阵（2026-10-02）

- **实际变更**：扩展 `src/tests/test_lpc.cc::TestMasterReloadPublicEntryOutsideMasterApply`，通过真实 LPC `master.c::create()` 从已经加载的 probe 触发 mode 1–7：目标自毁、目标自 reload、`replace_program`、owner 转移、`move_object`、从事务目标新建对象以及由外部 probe 反向销毁目标；每项都要求 public `recompile_object()` 失败、目标未标记 destructed、旧 program 仍在。新增 mode 8 和 `testsuite/clone/recompile_unrelated_probe.c`，证明 Master 事务允许加载无关 LPC 对象，compile barrier 仅属于 active SimulEfun dispatch。测试增加 master program RAII restore，避免成功复核污染后续测试。
- **根因/合同**：原已有 probe 只覆盖 mode 7，无法证明其余真实生命周期 mutation entry 在 `on_destruct`、重绑定、移动、replace queue 或 clone/program publish 前被同一个 transaction context 拦截，也无法证明非-simul 的无关编译没有被过度收紧。本次没有放宽 guard 或新增运行时路径，只扩大真实入口覆盖。
- **反例边界**：mode 1–7 任何一次若返回成功、改变 `O_DESTRUCTED`、替换 `master_ob->prog` 或绕过目标身份检查，GTest 立即失败；mode 8 必须成功，防止把 `blocks_new_compile()` 错误应用到所有 transaction kind。误选不存在的 `/single/tests/efuns/simul_efun` 仍不计入测试，正确 simul create failure 由 `recompile_object` enabled case 覆盖。
- **验证**：
  1. `cmake --build --preset asan --target lpc_tests --parallel 2` 成功。
  2. `run-targeted.py --binary build-asan/src/tests/lpc_tests --gtest-filter='DriverTest.TestMasterReloadPublicEntryOutsideMasterApply'`：discovery 1，运行 1，PASSED 1，FAILED 0，SKIPPED 0，证据 `/tmp/fluffos-xk-i15-gtest-master-matrix-asan/`，binary SHA-256=`4cac804445a5096390e662cd36f15faa58c709607b58a7937abafb6b8dc24a8b`。
  3. 同一 ASan binary 的 I15 集合（`TestSimulEfunReloadCreateFailureRollback`、`TestMasterReloadSuccess`、`TestMasterReloadPublicEntryOutsideMasterApply`、`TestMasterReloadStressRounds`、`TestRecompileLifecycleBoundary`）：discovery/运行 5，PASSED 5，FAILED 0，SKIPPED 0，证据 `/tmp/fluffos-xk-i15-gtest-master-matrix-asan-full/`；日志无 `AddressSanitizer`、`LeakSanitizer`、`runtime error`、`SUMMARY:` 或 `WARNING:`。
  4. `cmake --build --preset dev-debug --target lpc_tests --parallel 4` 成功；同一 I15 集合 discovery/运行 5，PASSED 5，FAILED 0，SKIPPED 0，证据 `/tmp/fluffos-xk-i15-gtest-master-matrix-debug-full/`，binary SHA-256=`411357f38c8c695c7282b78e72cf0981afb00bf0c8d94b31fc2d723867d6e8f9`。
  5. 所有 runner 记录 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。
- **状态/限制**：`unverified`。真实 master lifecycle mode 1–8 与白盒/ASan/Debug 矩阵通过；I15 其余异常注入、完整生命周期集成矩阵、UBSan/TSan、外部平台和 full LPC matrix 仍未闭合，不能推进 I01–I05 或标记 P1 complete。

### I15 follow-up attempt 8：UBSan/TSan 定向矩阵（2026-10-02）

- **实际变更/根因**：无新的运行时代码变更；按 attempt 7 的阶段出口补充未覆盖的 sanitizer 证据。第一次未使用 WSL2 所需的 `setarch x86_64 -R` 构建 TSan，在生成 `efuns.autogen.cc` 时以 `ThreadSanitizer: unexpected memory mapping`、exit 66 失败；原始构建输出已保留，随后按项目约束重跑。
- **验证**：`cmake --build --preset ubsan --target driver lpc_tests --parallel 2` 成功；UBSan I15 GTest 5/5、enabled `recompile_object` 1/1、enabled `recompile_special_targets` 1/1 通过，证据分别为 `/tmp/fluffos-xk-i15-gtest-ubsan/`、`/tmp/fluffos-xk-i15-recompile-enabled-ubsan/`、`/tmp/fluffos-xk-i15-special-ubsan/`，无 UBSan 诊断。`setarch x86_64 -R cmake --build --preset tsan --target driver lpc_tests --parallel 2` 成功；同一 TSan 三组分别为 GTest 5/5、enabled `recompile_object` 1/1、enabled `recompile_special_targets` 1/1，证据 `/tmp/fluffos-xk-i15-gtest-tsan/`、`/tmp/fluffos-xk-i15-recompile-enabled-tsan/`、`/tmp/fluffos-xk-i15-special-tsan/`，无 TSan 报告。
- **身份/限制**：source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；UBSan driver SHA-256=`d55e27541ae0d0e62e16820651f1ebc7cc59353a7b9bbc43ac81f5edb7d78452`，TSan driver SHA-256=`7b768e8725fb9a3a0b836d09c86cadd37825b12e593f9a4d31c8bd7d51d19800`。TSan 首次未加 `setarch` 的失败不计为源码失败；Linux/WSL2 sanitizer 矩阵现已覆盖，Windows/macOS/Alpine 与完整 `-ftest` 仍未验证，I15 保持 `unverified`。

### I15 follow-up attempt 9：补齐 simul create 生命周期入口矩阵（2026-10-02）

- **实际变更**：扩展 `testsuite/single/simul_efun.c` 的事务探针：mode 2–8 分别覆盖 simul 目标自毁、嵌套 `recompile_object`、`replace_program`、owner 转移、`move_object`、从 simul 目标新建对象、以及外部 probe 反向销毁目标；每次由 `/single/tests/efuns/recompile_object` 设置模式后走真实 public `recompile_object()`。mode 1 的无关 LPC 编译反例保持不变。第一次 fail-first 编译暴露 mode 5 使用整数 owner 参数，而 `vm_set_owner_id` 的实际 LPC 合同要求 owner 路径字符串；原始诊断保留在 `/tmp/fluffos-xk-i15-simul-lifecycle-asan/001-driver-lpc.log`，随后仅按现有 `testsuite/single/master.c` 合同改为 `"owner/recompile/probe"`，未放宽任何 guard。
- **根因/合同**：此前真实 LPC 只覆盖 simul create 内的 lazy unrelated compile，未覆盖其余生命周期 mutation entry；本次复用同一 `RecompileExecutionContext` 的 create 阶段约束，未增加第二事务状态或改变普通 blueprint 的 v1 exact-layout 语义。mode 2–8 若任一操作成功、目标消失或目标无法继续响应，测试即失败；mode 1/8 继续区分 simul compile barrier 与非-simul unrelated compile 合同。
- **验证**：
  1. 修复后的 ASan `recompile_object`：selected/passed 1，failed/skipped 0，`Checks succeeded.`，无 sanitizer/LEAK/orphan 诊断；证据 `/tmp/fluffos-xk-i15-simul-lifecycle-asan2/`，driver SHA-256=`70b75494a553c198e83adfd6eee6dbca64c80523dc6cb517730faa126237116a`。
  2. 普通 Debug `recompile_object`：1/1，证据 `/tmp/fluffos-xk-i15-simul-lifecycle-debug/`，driver SHA-256=`f6be1b844a64bf4ef077db8ee9567432af20f6e4a638f068457c5b717b538482`。
  3. UBSan `recompile_object`：1/1，证据 `/tmp/fluffos-xk-i15-simul-lifecycle-ubsan/`，driver SHA-256=`d55e27541ae0d0e62e16820651f1ebc7cc59353a7b9bbc43ac81f5edb7d78452`；TSan（`setarch x86_64 -R`）同案 1/1，证据 `/tmp/fluffos-xk-i15-simul-lifecycle-tsan/`，driver SHA-256=`7b768e8725fb9a3a0b836d09c86cadd37825b12e593f9a4d31c8bd7d51d19800`。
  4. 当前源码下 `recompile_special_targets`：ASan、UBSan、TSan 各 1/1，证据分别为 `/tmp/fluffos-xk-i15-special-asan-final/`、`/tmp/fluffos-xk-i15-special-ubsan-final/`、`/tmp/fluffos-xk-i15-special-tsan-final/`；`recompile_simul_one` ASan 1/1，证据 `/tmp/fluffos-xk-i15-simul-one-asan-final/`。
  5. 当前 ASan GTest 的 `DriverTest.TestRecompile*` discovery/运行 7，PASSED 7，FAILED/SKIPPED 0；master+simul lifecycle filter discovery/运行 7，PASSED 7，FAILED/SKIPPED 0；证据 `/tmp/fluffos-xk-i15-gtest-asan-final/`、`/tmp/fluffos-xk-i15-gtest-asan-lifecycle/`，binary SHA-256=`4cac804405a5096390e662cd36f15faa58c709607b58a7937abafb6b8dc24a8b`。所有成功 runner 记录 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`。
- **状态/限制**：真实 master/simul create lifecycle、ordinary `__INIT` 白盒 rollback、异常回滚和 Linux/WSL2 Debug/ASan/UBSan/TSan 定向证据已覆盖；I15 仍 `unverified`，因为完整 P1 LPC matrix、更多 failure-injection/长期重复泄漏检查及 Windows/macOS/Alpine 外部环境尚未闭合。未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。

### I15 follow-up attempt 10：补上普通 destruct 入口并修正 runner 断言合同（2026-10-03）

- **实际变更/根因**：`src/packages/core/efuns_main.cc::f_destruct` 在 `destruct_object()` 前接入同一 `vm_recompile_reject_lifecycle()`；此前 simul create 的 mode 2（目标自毁）绕过了已覆盖的 `simulate.cc` 路径，不能接受“事后发现 O_DESTRUCTED”作为回滚。与此同时，`tools/testsuite/run-targeted.py` 的 LPC 合同新增 `Check failed`/`Checks failed` 拒绝；旧 runner 只看 `Checks succeeded.`，会把断言失败与成功汇总并存的输出误报为通过。self-test `tools/testsuite/test-run-targeted.py` 新增并验证该失败标记路径，未修改 testsuite 语义。
- **反例/证据**：真实 `recompile_object` 的 functional funptr 反例在 `/tmp/fluffos-xk-i01-functional-baseline2/001-driver-lpc.log` 报出 `recompile_object.c:96, Check failed: expected stale functional pointer error`；这证明旧 runner 的“成功 marker”不是充分验收条件。mode 2–8 的生命周期合同由当前 `/single/tests/efuns/recompile_object` 重新执行，不接受只检查返回码的假阳性。
- **验证**：Debug、ASan、UBSan、TSan（`setarch x86_64 -R`）的 enabled `recompile_object` 各 selected/passed `1/1`、failed/skipped `0`，最终证据分别为 `/tmp/fluffos-xk-i01-bind-stale-fixed/`、`/tmp/fluffos-xk-i01-bind-stale-asan/`、`/tmp/fluffos-xk-i01-bind-stale-ubsan/`、`/tmp/fluffos-xk-i01-bind-stale-tsan/`；TSan driver SHA-256=`dbe3b76b59b66b17d7f4465b0b91face88c5db11ea7520a9efacd3da23788c40`。runner 自测 5/5，均无 sanitizer/LEAK/orphan 诊断。
- **状态/限制**：普通入口屏障和结果合同已修复；I15 仍 `unverified`，完整 P1 LPC matrix、额外 failure-injection/长期泄漏样本和 Windows/macOS/Alpine 外部环境仍未闭合。未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。

### I01：统一 funptr 代次初始化、functional stale 检查与 bind 失效继承（2026-10-03）

- **实际变更**：`src/vm/internal/base/function.{h,cc}` 新增统一 `initialize_funp_header()`，覆盖 EFUN、LOCAL、SIMUL、FUNCTIONAL 的 `owner_gen`/初始状态；`call_function_pointer()` 在 fake frame/setup 前统一检查 `invalid` 与 local/functional owner generation。`src/packages/core/efuns_main.cc::f_bind` 改为只复制 union payload，不复制 header 状态；按现有参数栈引用转移合同调用 helper，并继承 stale/invalid，不能通过换 owner 洗掉失效。`testsuite/clone/recompile_blueprint.c` 与 `testsuite/single/tests/efuns/recompile_object.c` 增加真实 functional closure、成功重编译后的 stale、跨 bind stale 和 fresh-generation 反例。
- **根因/反例**：旧 `call_function_pointer()` 只检查 `FP_LOCAL`，`FP_FUNCTIONAL` 直接进入执行；新增反例在 `/tmp/fluffos-xk-i01-functional-baseline2/001-driver-lpc.log` 稳定失败于 `recompile_object.c:96`。第一次把 f_bind 改用统一 helper 时错误重复取得已由对象参数栈持有的 owner 引用，`/tmp/fluffos-xk-i01-bind/001-driver-lpc.log` 报 `Bad ref count for object command/tests, is 2 - should be 1`；根因确认后以 `retain_owner_ref=false` 保留原有“移除参数槽即转移引用”语义。
- **验证**：`cmake --build build-dev-debug --target driver lpc_tests --parallel 4`、`build-asan --target driver --parallel 2`、`build-ubsan --target driver --parallel 2`、`setarch x86_64 -R cmake --build build-tsan --target driver --parallel 2` 均成功；Debug `DriverTest.TestRecompile*` discovery/运行 `7/7`、failed/skipped `0`，证据 `/tmp/fluffos-xk-i01-gtest-debug/`，binary SHA-256=`2f8f7a814eb6c3ca9f831f806227cd9c0d0dedac124d924f6d635dfb542f9390`。最终 enabled functional/stale-bind LPC 在 Debug/ASan/UBSan/TSan 分别 `1/1` 通过，证据 `/tmp/fluffos-xk-i01-bind-stale-fixed/`、`/tmp/fluffos-xk-i01-bind-stale-asan/`、`/tmp/fluffos-xk-i01-bind-stale-ubsan/`、`/tmp/fluffos-xk-i01-bind-stale-tsan/`；`bind`、`bind_destruct_owner`、`break_cycles` Debug 各 `1/1` 通过（`/tmp/fluffos-xk-i01-bind-fixed/`、`/tmp/fluffos-xk-i01-bind-destruct-fixed/`、`/tmp/fluffos-xk-i01-break-cycles-fixed/`）。所有 runner 记录 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`，`git diff --check` 通过。
- **状态/限制**：I01 本地原子单元 `complete`；I05 的事务出生 journal/永久失效全链、E1–E4 canonical identity 尚未实施，不能把本单元的 `invalid` 字段当作完整 I05 交付。未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。

### I02：异常路径 VM context 恢复、迁移块单一所有权与事务回滚（2026-10-03）

- **实际变更**：`src/vm/internal/recompile.cc` 为 `PreparedVariableMigration` 增加唯一 noexcept 析构责任；`obj_vars_init()` 成功但 migration vector 插入失败时，临时 `unique_ptr` 仍负责 payload，commit swap 后只转移到对象/rollback 重新取回，`migrations.clear()` 不再与析构重复释放。`run_create_guarded()` 保存并恢复 `VMExecutionState`，对所有 C++/LPC/未知异常统一按“restore_context → pop_context → VM execution restore → rollback”顺序处理，仍由现有包入口返回稳定 LPC 错误，不把 `std::bad_alloc` 转成成功。内部 `RecompilePrepared` 增加只供白盒测试设置的函数指针故障注入点，生产入口不设置。`src/tests/test_lpc.cc` 增加 swap 后抛出 `std::runtime_error` 的回滚测试，并在异常钩子故意清空 current object/program 后验证执行寄存器恢复；同时核对 old/new program ref、变量槽位/layout、flags、generation 和 migration 状态。
- **需求/根因**：I02 的反例是 `run_create_guarded()` 只捕获 `const char*`，C++ 异常可在新 program/变量状态已发布后绕过 rollback；原有 `PreparedVariableMigration` 的 `unique_ptr` 不会自动释放 `ObjectVariableBlock` 内部 `svalue_t` payload，新增手工清理又会与后续路径重叠。首次构建还直接以 `undefined reference to PreparedVariableMigration::~PreparedVariableMigration()` 证明了仅声明析构而未定义的中间状态；补上单一析构后链接通过。
- **反例/验证**：`TestRecompileCreateNonLpcExceptionRollsBack` 的注入异常清空 VM execution registers，定向运行证明异常不逃逸、目标仍指向 old program、变量仍为原值、代次/flags/refcount/migration 均恢复；既有 `TestRecompileMigration*` 三项覆盖 add/remove、reorder 和 `__INIT` 失败后的 payload rollback。`git diff --check` 通过；本单元相关 dirty patch SHA-256=`bf3f73191225b1ca7e74841b598a3ddd73f40a0004701c8c0a34abc717f1f33c`，source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`。
- **验证命令与结果**：`cmake --build build-dev-debug --target driver --parallel 4`、`cmake --build build-dev-debug --target lpc_tests --parallel 4` 成功；Debug 新测试 `1/1`（binary `68001a2b2fdafc67bd428baf881800741695d5a61c3d487c27f3824463650c3f`，证据 `/tmp/fluffos-xk-i02-debug-failure3/`），`TestRecompileMigration*` `3/3`（binary 同上，证据 `/tmp/fluffos-xk-i02-debug-gtests/`），enabled `recompile_object` `1/1`（driver `eb2b3a5faad1e99713088386e0cd468c4d9e8ab6944b778886ca08b1096f5287`，证据 `/tmp/fluffos-xk-i02-debug-recompile-object/`），runner self-test `5/5`。ASan 构建 `driver+lpc_tests` 成功；I02 GTest 集合 `5/5`（lpc binary `ca3e08359cd147e9ceb85ee642f64a39dab01636c115f42be88cbb7f0532672f`，证据 `/tmp/fluffos-xk-i02-asan-gtests/`），enabled LPC `1/1`（driver `37bf180b53891914167394bf3ecf7f1f6a01ae8cd373f23a0877b867e4fad673`，证据 `/tmp/fluffos-xk-i02-asan-lpc/`），无 ASan/LSan 诊断。UBSan I02 GTest `5/5`（binary `2a91fb17934b559f5051ba01e7ae895e5a58f4a72b6133635797dbbf68edae71`）和 LPC `1/1`（driver `91029c4f9c5d889522c21904db518293f5d2fe3923f2e405be90db63bf6e2809`）通过，证据 `/tmp/fluffos-xk-i02-ubsan-gtests/`、`/tmp/fluffos-xk-i02-ubsan-lpc/`，无 UBSan 诊断。按 WSL2 约束使用 `setarch x86_64 -R` 构建/运行 TSan；GTest `5/5`（binary `86325670ad5b9e43eca2519f182a549f8516c3abcb1d650b614edf10f176bf2f`）和 LPC `1/1`（driver `c481f74219e8a9f4d440ef97ecffc75078f1d1739457b84712ffca422b1740fc`）通过，证据 `/tmp/fluffos-xk-i02-tsan-gtests/`、`/tmp/fluffos-xk-i02-tsan-lpc/`，无 TSan 报告。
- **状态/限制**：I02 本地原子单元 `complete`；本次故障注入覆盖 swap 后非 `const char*` 异常和已有 `__INIT` 失败路径，未伪造 allocator 全局故障或声称独立 LSan/Windows/macOS/Alpine/full LPC matrix 已通过。I03/I04/I05 及 P1 其余单元仍未实施。未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。

### I03：分离 old/new 派生 flags 并按 staged program 查询（2026-10-03）

- **实际变更**：`RecompileTarget` 拆分为 `old_derived_flags` 与 `new_derived_flags`。snapshot 阶段只直接保存对象当前的 `O_WILL_CLEAN_UP` 位，保留 clean_up 一次性 sweep 已清除后的真实旧状态；新位在目标快照阶段从已经准备好的 staged program apply table 查询，不能读取仍指向旧 program 的 `object_t::prog`。`src/vm/internal/base/interpret.{h,cc}` 提取无对象依赖的 `function_exists_in_program()`，现有 `function_exists()` 复用同一纯 program 查询，避免 recompile 自己复制 apply lookup 规则。commit 只发布 new 位，rollback 只恢复 old 位；其他 object flags 不变。
- **需求/根因**：旧代码只有一个 `precomputed_flags`，在 old program 上查询后同时用于 swap 和 rollback。反例是 old program 没有 `clean_up()`、staged program 新增 `clean_up()`：commit 后位仍为 0；反向反例是 old program 的 clean_up 位已被一次性 sweep 清除但方法仍存在，rollback 会错误地重新置位。先加失败测试时 `/tmp/fluffos-xk-i03-fail-first/002-gtest-run.log` 稳定报 `Expected: (bp->flags & 0x100u) != (0u), actual: 0 vs 0`，确认测试确实区分旧实现。
- **验证**：普通 Debug `TestRecompileDerivedFlags*` discovery/运行 `2/2`、failed/skipped `0`，证据 `/tmp/fluffos-xk-i03-debug2/`，lpc_tests SHA-256=`15bd214924e9f2fe36d6002fc75f68e50251f760de1cb9a6b62a92752791493d`；同一 Debug 下 `TestRecompile*`、`TestMasterReload*`、`TestSimulEfunReload*` 共 `17/17`，证据 `/tmp/fluffos-xk-i03-debug-gtests/`，driver LPC `recompile_object` `1/1`，证据 `/tmp/fluffos-xk-i03-debug-lpc/`，driver SHA-256=`5fa5c3d7fdb5821732bd8198eed17e7190f95c774579cec27aeeab6ea1515919`。ASan 共 `17/17` + LPC `1/1`，证据 `/tmp/fluffos-xk-i03-asan-gtests/`、`/tmp/fluffos-xk-i03-asan-lpc/`，lpc_tests SHA-256=`62dfba48478ddceb6db9929e4850b80026bdd6efc34a7c3e43fdce0ac74bc945`，driver SHA-256=`4afedc79baf5643ae91c6512b6b602f718135b8faa261aeedf83ef934b6280ff`，无 ASan/LSan 诊断。UBSan 共 `17/17` + LPC `1/1`，证据 `/tmp/fluffos-xk-i03-ubsan-gtests/`、`/tmp/fluffos-xk-i03-ubsan-lpc/`，lpc_tests SHA-256=`5581dd0720c80bba972d99298cf8d5334bad539e2e8b7feee36b36768b1963a6`，driver SHA-256=`e7ac793865376fb6e3f8b8cec79364b7255fbdba6b95fa5beef6d06ce55ed2ef`，无 UBSan 诊断。按 WSL2 约束使用 `setarch x86_64 -R` 的 TSan 共 `17/17` + LPC `1/1`，证据 `/tmp/fluffos-xk-i03-tsan-gtests/`、`/tmp/fluffos-xk-i03-tsan-lpc/`，lpc_tests SHA-256=`4cb9b25cb902bf3035c6c4c22f07ee636637263039e4549be308d162684ba429`，driver SHA-256=`ad5c45ea9d6d2194090f0250bd8ec30d036a9d0a83a9be5c08f513d3eec3fa64`，无 TSan 报告。所有 runner 记录 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；`git diff --check` 通过；本单元相关 dirty patch SHA-256=`2f8dde440513da49a5db2ec2b829ff380875e9f94b8d4cdd950021ec65d98734`。
- **状态/限制**：I03 本地原子单元 `complete`，普通 blueprint family、master 与 simul_efun family 定向路径均通过；完整 P1 LPC matrix、独立长期泄漏样本及 Windows/macOS/Alpine 外部环境仍未闭合。未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。

### I04：coroutine 代次统一为 uint64 并独立持有 owner-top program pin（2026-10-03）

- **实际变更**：`src/vm/internal/base/promise.h` 将 `lpc_coroutine_t::prog_generation` 统一为 `uint64_t`，并明确 `object_prog` 是独立的 owner-top program 引用；`coroutine_await_pending()` 在 park 时对定义 program 和 `current_object->prog` 分别取得引用，即使两者地址相同也记两次；`free_coroutine()` 在完成、拒绝、destruct/abandon、replace 和错误清理路径分别释放两个 pin。resume guard 继续比较 owner 对象的 program 与 `object_prog`，不再错误地用继承函数的定义 program 判断 owner 是否被替换。未改变 owner-serial、普通 legacy LPC 后台执行权限或异步公共 API。
- **需求/根因/反例**：计划 I04 的两个独立风险同时存在：旧 `prog_generation` 为 `uint32_t`，达到 `2^32` 后会截断对象的 `uint64_t` 代次；旧 coroutine 只 pin 定义 program，继承 async 的定义 program 与 owner top program 可不同，且 owner top program 被 replace/detach 后仅靠地址比较可能访问失效地址。`TestRecompileSafetyCoroutineGeneration64AndProgramPin` 将对象代次置于 `UINT32_MAX+17`，真实 park/resume 要求正常完成，并在 park/完成前后要求 owner program ref 分别为基线 `+2`/恢复基线；`TestRecompileSafetyCoroutineRejectsRecompile` 和 `TestRecompileSafetyCoroutineRejectsReplaceProgram` 分别通过真实 transaction/replace queue 验证 stale frame 不恢复执行；既有继承 async、rejection 和 destruct abandonment 测试覆盖定义 program 不等于 owner program、错误/取消和对象销毁路径。静态 fail-first 对照为旧字段的 32 位截断及旧单 pin 只能产生 `+1`，不接受仅用长循环模拟代次。
- **验证**：Debug `cmake --build build-dev-debug --target lpc_tests --parallel 4` 成功；定向过滤器 `DriverTest.TestRecompileSafetyCoroutineGeneration64AndProgramPin:DriverTest.TestRecompileSafetyCoroutineRejectsRecompile:DriverTest.TestRecompileSafetyCoroutineRejectsReplaceProgram:DriverTest.TestAsyncAwaitResumesAfterYieldAndCatchesRejection:DriverTest.TestAsyncAwaitDestructRejectsSuspendedFrame:DriverTest.TestAsyncPromiseFormsResolveAndRejectThroughOwnerAdmission` discovery/运行 `6/6`，PASSED `6`、FAILED/SKIPPED `0`，日志 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i04-debug-gtests/002-gtest-run-six.log`，lpc_tests SHA-256=`44dfe529de42de98c08a190961a69683b9e241d3d7a18d02bb7b6060eb89e2f4`。ASan `cmake --build build-asan --target lpc_tests --parallel 2` 和同一过滤器 `6/6` 通过，证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i04-asan-gtests/`，lpc_tests SHA-256=`aa49d45a2e61c6ad898ca15792ebe378a6f3d1e67f9bcdf9f3f2fd1bac554074`，无 ASan/LSan 诊断。UBSan `cmake --build build-ubsan --target lpc_tests --parallel 4` 和同一过滤器 `6/6` 通过，证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i04-ubsan-gtests/`，lpc_tests SHA-256=`d712e97fdecf0eeda3eb9dbb3c6fea37694d0578cb82516533c856f0ea394c7c`，无 UBSan 诊断。按 WSL2 约束以 `setarch x86_64 -R` 运行 TSan `lpc_tests`，同一过滤器 `6/6` 通过，证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i04-tsan-gtests/`，lpc_tests SHA-256=`9be14c1ca1ce3680a202c8ae107eb35559304cb92676bbfed7918f0662c10f3a`，无 TSan 报告。source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；`git diff --check` 通过；当前相关文件 SHA-256 为 `promise.h=3d662026f82d799cd326bc6f3d3d76794c242a3585458b6736bcef3ede49472d`、`promise.cc=7d514ca8de57167fbb275d452d26255179339aae1e1771f569e969a2634eaf61`、`test_lpc.cc=8ecde6a2e735c2750b3643a8d391c498d266185fe2ef089fdcefd69782603f5e`；未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。
- **状态/限制**：I04 本地原子单元 `complete`；Debug/ASan/UBSan/TSan 仅覆盖 Linux/WSL2 定向 GTest，继承、真实 recompile、replace、destruct abandonment、成功完成和错误拒绝均有反例；完整 P1 LPC matrix、独立长期泄漏样本、Windows/macOS/Alpine 外部环境和 I05 永久 Invalid/出生 journal 仍未闭合。

### I05：永久 Invalid、全 funptr 弱登记、事务 pin 与共享出生 journal（2026-10-03）

- **实际变更**：`src/vm/internal/base/function.{h,cc}` 为所有 funptr 统一初始化 `owner_gen`、生命周期状态和登记链；新增 owner 弱登记、unlink、owner detach、对象清理和 birth/transaction pin 链。`make_efun_funp`、`make_lfun_funp`、`make_simul_funp`、`make_functional_funp` 与 `f_bind` 共用同一构造/登记入口；`call_function_pointer()` 在 fake frame/setup 前拒绝永久 `Invalid` 和 local/functional 代次失配，bind 到新 owner 仍继承失效状态。`src/vm/internal/recompile.{h,cc}` 让 I15 的单一 `RecompileExecutionContext` 在 `Swapped` 阶段登记出生指针，在准备完成后 pin 所有目标 funptr；提交将 `StagedBorn` 提升为 `Live`，回滚永久改为 `Invalid`，两种结果都释放统一 pin。`RecompilePrepared` 与 context 增加双向 detach，避免白盒 RAII 作用域相反时留下悬空 context。`object.{h,cc}`、`reclaim.cc` 在对象/回收路径先解除 owner 登记；没有开启 E 的 canonical identity 或成功迁移。
- **需求/根因/反例**：只恢复 `prog_generation` 不能识别失败事务中逃逸的指针；失败后再成功提交可能复用相同代次，使旧索引在新 program 上错误通过。初次 LPC 反例在 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-baseline2/001-driver-lpc.log` 暴露了测试探针未真正产生 escaped pointer（`Expected 1, Actual 0`），随后补齐 master/simul 两个真实 create 入口的 mode 9/10。ASan fail-first 又发现 `RecompilePrepared` 非拥有的 context 指针在白盒局部 context 先析构时触发 `stack-use-after-scope`；提取证据保留于 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-asan-fail-first/001-asan-gtest-run-extracted.log`，修复为双向 detach 后重跑通过。回滚 probe 覆盖目标外 owner、失败后重试仍 Invalid；提交 probe 覆盖 newborn 从 `StagedBorn` 到 `Live`。
- **验证**：source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`，`git diff --check` 通过。本单元相关 dirty patch（当前 I01–I05 累积文件集合）SHA-256=`0ea117d4718c8655dfbee8c9392d970f0123c42038ffab494793cccd31588df1`。
  1. Debug `cmake --build --preset dev-debug --target driver lpc_tests --parallel 4` 成功；`run-targeted.py --binary build-dev-debug/src/tests/lpc_tests --gtest-filter='DriverTest.TestMasterReloadPublicEntryOutsideMasterApply:DriverTest.TestRecompile*'` discovery/运行 `14/14`、PASSED `14`、FAILED/SKIPPED `0`，证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-debug-gtests-final2/`，lpc_tests SHA-256=`97996471608663230f1e3b4a29e9f993574eb6991d9a5d48d3ac97c397a36c28`；enabled `recompile_object` `1/1`，证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-debug-lpc-final2/`，driver SHA-256=`b9e806dd00c027123facd0d03ba42f6ab5b6603c491e3956c07796a29bab9402`。
  2. ASan `lpc_tests` 与 driver 均构建成功；GTest `14/14`，证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-asan-gtests-final/`，lpc_tests SHA-256=`d7a813f06c4df380f7f96036c85510d587c25a48569dad8ebf6bec26d899459d`；enabled LPC `1/1`，证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-asan-lpc-final2/`，driver SHA-256=`65565c955bd6cdf5b3988a669b150cf7a8cb57c668fd5a4d6644b451ca98ae8c`，无 ASan/LSan 诊断。
  3. UBSan GTest `14/14`、enabled LPC `1/1`，证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-ubsan-gtests-final/`、`/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-ubsan-lpc-final2/`；lpc_tests SHA-256=`ce54fa4615b783759ef224d4053c8ecae9af005fbac0aac7411628baafcba74f`，driver SHA-256=`70d5c8edb22e3a8c321cea49cca165686d748cc272e32b4635633704b63ba8aa`，无 UBSan 诊断。
  4. 按 WSL2 约束使用 `setarch x86_64 -R` 构建/运行 TSan；GTest `14/14`、enabled LPC `1/1`，证据 `/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-tsan-gtests-final/`、`/tmp/fluffos-xk-upstream-absorption-2026-10-01a0fbb2/i05-tsan-lpc-final2/`；lpc_tests SHA-256=`0d55ddd42c7255a9c139ffd2d3d065b78648b6a0faf2c8e61839112b3a32afcf`，driver SHA-256=`a3aec036614a2cc7f08e4f97c6f25b972637aa58379803f171645c0bf2214f2b`，无 TSan 报告。
  5. 真实 LPC `recompile_object` 同时覆盖 simul rollback、master rollback/retry、target 外 owner、successful newborn commit；所有最终 runner 记录 `selected/passed=1/1`、`failed/skipped=0`、`Checks succeeded.`。保留外部 `src/vm/internal/base/object.cc` 改动和下游 `testsuite/single/tests/efuns/save_object.c` 未修改；未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。
- **状态/限制**：I05 本地原子单元 `complete`；I01–I05 仍只保持旧 funptr stale 合同，E1–E4 canonical identity/成功迁移尚未开启。完整 P1 LPC matrix、独立长期泄漏样本、Windows/macOS/Alpine 和最终 full `-ftest` 仍未验证，不能将本记录扩展为 P1 或 E 完成。

### I06：WebSocket session teardown 统一释放与重入安全（2026-10-03）

- **实际变更**：`src/net/websocket.{h,cc}` 新增唯一的 `websocket_session_teardown()` helper，统一接管 CLOSED、握手失败和 writable/receive 失败路径的 `user` 句柄摘除、`interactive_t::lws` 断开、`remove_interactive()` 重入和 `evbuffer` 释放；调用顺序保留 flush 所需的 buffer，关闭路径不再对已经 CLOSED 的 socket 重新排 writable。`src/net/ws_ascii.cc` 与 `src/net/ws_telnet.cc` 均只调用这一 helper，重复关闭、空 user、空 buffer 都安全。`src/packages/gateway/gateway_session.cc` 的临时 interactive 清理也取消可能存在的 logon event。未改变 owner 语义、TLS 公共配置或下游 mudlib。
- **需求/根因/反例**：原 ascii/telnet 各自处理 CLOSED，存在 user 已被清空但 buffer 未统一释放、或重入 `remove_interactive()` 后重复释放的分叉风险；关闭回调还可能在已关闭连接上重新安排 writable。实现初版的 live 反例发现 `event_add(event, nullptr)` 不能替代明确的零时延参数，WS 连接握手成功后没有收到 application frame；修为 `{0,0}` 后才进入真实 logon/teardown 路径。原有 `ws_smoke.py telnet-mccp` 单次 `recv()` 还会把分片的初始协商误报为失败，测试工具改为在 8 秒窗口内持续收取协商数据，避免把客户端竞态当成 driver 回归。
- **验证**：source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；`cmake --build build-absorb-p0 --target driver lpc_tests --parallel 4`、`build-asan --parallel 2`、`build-ubsan --parallel 4`、`build-tsan --parallel 2` 均成功，原始构建日志 `/tmp/fluffos-xk-i06-i07-20261003/builds/`。Debug/ASan/UBSan/TSan 的 `DriverTest.TestUserLogonSchedulingUsesCancellableEvent` 均 `1/1` PASSED、FAILED/SKIPPED `0`；TSan 按 WSL2 约束使用 `setarch x86_64 -R`，日志 `/tmp/fluffos-xk-i06-i07-20261003/gtest-{debug,asan,ubsan,tsan}-i06-i07.log`。真实 loopback ASan driver 的 I06/I07 矩阵包括 ws/wss ascii、ws/wss telnet、ws/wss MCCP refusal、plain telnet MCCP、WS 6000 字节与 5000 次 flush burst，全部 `rc=0`；另执行原生 telnet/TLS 快连快断 `200` 次和 WebSocket handshake-abort `100` 次，全部 `rc=0`，证据 `/tmp/fluffos-xk-i06-i07-20261003/live-final/`。显式 LSan 运行同一 `11` 项矩阵加 native close，`all_rc=0`、driver `rc=0`、无 ASan/LSan/UBSan 诊断，证据 `/tmp/fluffos-xk-i06-i07-20261003/lsan-full/`。相关二进制 SHA-256：Debug driver=`c9c916fc5863a25051bd0603421fdfd1ccc896f1749e2b9f60ff61e5feb41af`、lpc_tests=`21a51bb4007420ad120222db748733ee0582d7363f864170d53b7adffe3ea88c`；ASan driver=`b485bad670ae1ca98df68b633a7dcbfff3cacee3a1d1fa591e3db4ca7afee96b`、lpc_tests=`8e808475e4caa0fc7101a190ef971ab90464a8eddb7ee4568f2d9c02e7c97ef1`；UBSan driver=`87905b7d856a86b5ce6e2276f41f85072643d89a463c00b141dd5458cf2cd373`、lpc_tests=`b34b920a829e511be8f3cca1dcc56d3b2a66093bc49b14280bcf2182c6a32b4b`；TSan driver=`0ffaedb1110aeac95039daae7b0fa586599528805d34a60e3de643176d24da30`、lpc_tests=`ae9ec2589ad06317eccbc08403f86bee09d5f457cab5c473059706b864bff283`。`git diff --check` 通过；`src/vm/internal/base/object.cc` SHA-256=`2c5a35e58076962a41241471be9630e9365b9f83af4f3945229ec7583393be7e`、`testsuite/single/tests/efuns/save_object.c` SHA-256=`b59f6eab79e6544a747a7b34075027f3a9fa776e17b21dc004ca1f08d27bc36e`，本单元未覆盖或改写两处外部改动。未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。
- **状态/限制**：I06 本地原子单元 `complete`；I08/I09 TLS 身份、最低版本和 trusted proxy 尚未实施，Windows/macOS/Alpine、生产负载与最终 full `-ftest` 仍未验证。

### I07：interactive logon event 可取消且由 session 生命周期拥有（2026-10-03）

- **实际变更**：`src/interactive.h` 增加 `ev_logon` 句柄；`src/comm.{h,cc}` 增加唯一 main-thread `schedule_user_logon()`/`cancel_user_logon()` helper。三个建连入口统一存储 event 句柄；callback 先把句柄从 interactive detach、free，再检查 `CLOSING` 后调用 `on_user_logon()`；`remove_interactive()` 在 `FREE(ip)` 前取消事件。event 使用显式 `{0,0}` timeout，失败立即释放 event 并关闭新连接；gateway 临时 session 同样走取消路径。没有新增全局 session 表或普通 legacy LPC 后台执行权限。
- **需求/根因/反例**：旧实现用无句柄的 `event_base_once()` 捕获裸 `interactive_t*`，close/free 与 timer 的顺序没有共同所有权，快连快断可能留下悬空回调。反例覆盖 callback 已排队后 close、WS handshake abort、TLS/plain 原生连接立即断开；初版实现的空 timeout 反例还证明测试必须区分“event 已创建”与“logon 真正执行”。
- **验证**：同 I06 的四种构建均成功；`DriverTest.TestUserLogonSchedulingUsesCancellableEvent` Debug/ASan/UBSan/TSan `1/1`，日志 `/tmp/fluffos-xk-i06-i07-20261003/gtest-{debug,asan,ubsan,tsan}-i06-i07.log`。ASan + 显式 LSan loopback 矩阵覆盖 plain telnet/TLS 快连快断、WS ascii/telnet 主动关闭、handshake abort、MCCP 协商、write burst 和重复关闭；`live-final` 与 `lsan-full` 全部退出 `0`，无 sanitizer 诊断。`src/comm.cc` SHA-256=`5246779d275ac2122a776330c6d3d5d2e4e1af6c36f6c2501e438e1f25d3718c`、`src/interactive.h` SHA-256=`16ee3f3f84af19f9994eb9652234347ca860d32659e4ee9fef7d71baeedeca5b`、`src/tests/ws_smoke.py` SHA-256=`9c6295ec7a61bb252a90f7ee8abc0657fd0ad4c9383c7e5b3fedbf627b4db319`；`git diff --check` 通过。未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。
- **状态/限制**：I07 本地原子单元 `complete`；I08/I09 与 H 的 TLS policy/trusted proxy 仍未实施，外部平台和完整 P1/P2/P3 集成矩阵仍未闭合。

### I08：TLS client 身份验证、最低协议版本与失败 unwind（2026-10-03）

- **实际变更**：`src/net/tls.{h,cc}` 抽出唯一的 `kTlsMinimumProtocolVersion=TLS1_2_VERSION` 和 `tls_set_minimum_protocol_version()`；普通 TLS client/server context 均在加载证书或开始使用前调用 `SSL_CTX_set_min_proto_version()`，失败即释放 context。client 的 `SSL_CTX_set_default_verify_paths()` 失败不再只告警后继续。新增 `tls_configure_client_identity()`：有 SNI 时同时设置 SNI 和 `SSL_set1_host()` 的 DNS 身份；无 SNI 且启用 `SO_TLS_VERIFY_PEER` 时从本次 numeric peer `sockaddr` 设置 IP SAN 期望值；显式关闭验签仍保留原有 `SSL_VERIFY_NONE` 合同，但不跳过 `SSL_new`、`SSL_set_fd`、SNI/identity 和 event 注册的返回值检查。`src/packages/sockets/socket_efuns.cc` 在任一 TLS 初始化、身份配置或 event 注册失败时统一走 `socket_close(...SC_FORCE|SC_FINAL_CLOSE)`，释放 SSL/CTX、event、fd 和 option 引用，避免半初始化 socket 留在表中。`src/thirdparty/libwebsockets/include/libwebsockets/lws-context-vhost.h` 增加最小 TLS 版本字段；`openssl-server.c` 在实际 SSL_CTX 创建点应用字段并对不支持的后端失败；`src/net/websocket.cc` 将同一个 TLS1.2 policy 传给 bundled OpenSSL WebSocket vhost，没有再添加散落的 `SSL_OP_NO_TLSv1_1` 模拟位。`src/tests/test_lpc.cc` 增加 client identity 返回矩阵和 client/server context 最低版本断言。
- **根因/反例**：旧 socket client 只调用 `SSL_new`、`SSL_set_fd`、`SSL_set_tlsext_host_name` 而忽略返回值，启用 `SO_TLS_VERIFY_PEER` 也没有把目标主机名或 numeric IP 绑定到证书验证参数；`tls.cc` 的 `SSL_OP_NO_TLSv1_1` 不能表达统一的最低版本策略，且默认 CA 路径失败仍会继续。fail-first 快照 `/tmp/fluffos-xk-i08-fail-first-20261003/fail-first.txt` 保留了旧调用链和 source HEAD。实现后的反例覆盖：空 SNI、无 peer 的验签身份配置返回失败；`openssl s_client -tls1_1` 对 legacy TLS 4003 与 WebSocket TLS 4002 均返回 `no protocols available`；`-tls1_2` 两端均完成握手并明确报告 `Protocol version: TLSv1.2`。一次完整 LSan 运行首次因旧的本任务 Debug driver 未退出导致 bind error，证据保留在 `/tmp/fluffos-xk-i08-live-asan-lsan-20261003/`；随后杀掉本任务 PID 后以干净端口重跑，不能把前一次混用二进制的结果计入通过。
- **验证**：
  1. `cmake --build build-dev-debug --target driver lpc_tests --parallel 4`、`cmake --build build-asan --target driver lpc_tests --parallel 2`、`cmake --build build-ubsan --target driver lpc_tests --parallel 4` 成功；首次 TSan 构建在 GTest discovery 阶段因 WSL2 未使用 `setarch x86_64 -R` 以 exit 66 失败，原始日志 `/tmp/fluffos-xk-i08-build-tsan-20261003.log` 保留；随后 `setarch x86_64 -R cmake --build build-tsan --target lpc_tests --parallel 2` 成功，重试日志 `/tmp/fluffos-xk-i08-build-tsan-20261003-retry.log`。所有失败记录均不是源码通过证据。
  2. `DriverTest.TestTlsClientIdentityAndMinimumProtocol:DriverTest.TestSysReloadTlsPermissionAndThreadMatrix` 在 Debug/ASan/UBSan/TSan（TSan 使用 `setarch x86_64 -R`）均 discovery/运行 `2/2`、PASSED `2`、FAILED/SKIPPED `0`，日志 `/tmp/fluffos-xk-i08-test-{debug,asan,ubsan,tsan}-20261003.log`，无 sanitizer signature；build 日志分别为 `/tmp/fluffos-xk-i08-build-debug-20261003.log`、`/tmp/fluffos-xk-i08-build-asan-20261003.log`、`/tmp/fluffos-xk-i08-build-ubsan-20261003.log` 和 TSan retry 日志。
  3. Debug loopback 的 ws/wss ascii、telnet、两种 MCCP refusal、plain telnet MCCP、6000 字节 burst、5000 次 flush burst 全部成功；WSS 的 burst/flush 在修改后的 TLS1.2 context 下分别报告 `y_count=6000/6000`、`160000/160000`，证据 `/tmp/fluffos-xk-i08-live-debug-20261003/`、`/tmp/fluffos-xk-i08-live-debug-wss-burst-20261003/`、`/tmp/fluffos-xk-i08-live-debug-ws-burst-flush-20261003/`、`/tmp/fluffos-xk-i08-live-debug-wss-burst-flush-20261003/`。协议探针证据 `/tmp/fluffos-xk-i08-protocol-20261003/`：legacy 与 WebSocket TLS1.1 均被拒绝，TLS1.2 均完成握手。
  4. 干净 ASan+LSan driver 的同一 `11` 项 WebSocket/plain-MCCP/burst 矩阵加 native telnet close 全部 `rc=0`，driver `rc=0`，日志无 `AddressSanitizer`、`LeakSanitizer`、`runtime error` 或 `SUMMARY:`，证据 `/tmp/fluffos-xk-i08-live-asan-lsan-clean-20261003/`。所有成功验证 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；相关当前 binary SHA-256：Debug driver=`ec071aa28b9447f4b5a5e960e4a0910710fa8ccf50f2483461938edc4bb1d444`、Debug lpc_tests=`ef6bc356ac8a447942ffb3c16063c9136a4d5e5bb8e13fa8eb4cb3bbb40f60f3`；ASan driver=`5e2d10e21fa4046ecb2244c3deb9da9777d0bbb46fd66c0ed665a91426744ef6`、ASan lpc_tests=`27b5c54bf4e8fbd5d032831f3660499e83f6ff3219a4eeb47c60143998a63870`；UBSan driver=`c27549b96c6990c96e512ea89e159ba6498bbb96f81927c44d3114ed5def36b5`、UBSan lpc_tests=`7130663b167201e72b50e2db641f6599520b8ebad0d8874b3c03036968e1fec8`；TSan driver=`10a90b2a1f711ab4435a10d837e2d537149e86060ca858c5babd5541a62c4e7e`、TSan lpc_tests=`26872252975ccabf032817a3c89d695f7842156b26bae0fc5977c8f2381839b2`；`git diff --check` 通过。未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。
- **状态/限制**：I08 Linux/WSL2 本地原子单元 `complete`；TLS1.2 policy 已覆盖普通 TLS server/client 和 bundled OpenSSL WebSocket vhost，client SNI/DNS、numeric IP SAN 配置、失败 unwind、低版本拒绝和 LSan loopback 均有定向证据。当前没有真实 Windows/macOS/Alpine 环境，未伪造外部平台通过；I09 trusted proxy、H staged SSL_CTX 热更新及完整 P1/P2/P3 集成矩阵仍未闭合。

### I09：WebSocket trusted proxy CIDR、真实 peer 判定与头解析拒绝（2026-10-03）

- **实际变更**：`port_def_t` 增加每个 WebSocket listener 的预编译 trusted-proxy CIDR；`websocket_parse_trusted_proxy_cidrs()` 在启动配置阶段只接受数字 IPv4/IPv6 CIDR，规范化网络位并把 IPv4-mapped IPv6 转为 IPv4，非法、空项、越界前缀和非数字地址使启动失败。新增唯一的 `websocket_get_client_address()`：先读取真实 TCP peer，只有真实 peer 命中本端口的 CIDR 才读取 `X-Real-IP`；非可信 peer 忽略该头，可信 peer 只接受单一、完整、数字地址，重复头（libwebsockets 合并为逗号）、非法值、空值和超长值拒绝。ascii/telnet 两个 WebSocket 建连入口共用该 helper，不再各自解析可伪造的头。补充 `src/Config.example`、gateway 安全手册和 parser 回归测试。
- **根因/合同**：旧实现无 trusted-proxy 边界，任何 WebSocket peer 都可以用 `X-Real-IP` 改写会话来源，并在 ascii/telnet 两个 callback 中重复使用 `evutil_getaddrinfo()`；非数字值的行为、重复头和失败释放没有统一合同。本次约束落在 `new_user()` 之前的真实 WebSocket 建连入口，不增加全局目录、DNS 解析、兼容别名或普通 legacy LPC 权限。默认配置为空，因此既有部署不信任任何 `X-Real-IP`；可信代理的缺失头保持真实 peer。
- **反例/验证**：fail-first 与当前实现均保留完整 live 原始响应；未配置 trusted CIDR 时，loopback peer 携带 `X-Real-IP: not-an-ip` 仍成功收到应用帧；配置 `127.0.0.1/32` 后，合法 `198.51.100.7` 收到应用帧，而非法值和重复 `X-Real-IP` 均只收到升级响应后关闭、没有应用帧。非法配置 `not-a-cidr` 在 listener 创建前以 exit `255` 拒绝，并输出稳定诊断。parser 回归同时覆盖 IPv4、IPv6、IPv4-mapped IPv6、网络位归一化、越界前缀、非数字地址和尾逗号。
- **验证**：
  1. `cmake --build build-dev-debug --target driver lpc_tests --parallel 4` 成功；`build-dev-debug/src/tests/lpc_tests --gtest_filter='DriverTest.TestWebsocketTrustedProxyCidrParser'` discovery/运行 `1/1`、PASSED `1`、FAILED/SKIPPED `0`，日志 `/tmp/fluffos-xk-i09-gtest-parser-20261003.log`。
  2. `cmake --build build-asan --target lpc_tests driver --parallel 2` 成功；同一 parser GTest 在 `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1` 下 `1/1` 通过，无 ASan/LSan 诊断，日志 `/tmp/fluffos-xk-i09-gtest-parser-asan-20261003.log`。
  3. 真实 Debug loopback driver 使用隔离 mudlib 副本和端口 4011/4012：未信任 malformed header `app_frame=True`；可信配置下 valid header `app_frame=True`、malformed header `app_frame=False`、duplicate header `app_frame=False`，全部客户端检查通过；证据 `/tmp/fluffos-xk-i09-live-20261003/{untrusted,trusted}.log` 及命令输出。
  4. 真实 ASan+LSan loopback driver 使用端口 4014：valid header `app_frame=True`、malformed header `app_frame=False`，driver exit `0`，无 ASan/LSan/UBSan 诊断；证据 `/tmp/fluffos-xk-i09-live-20261003/asan-trusted.log`。
  5. source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；Debug driver SHA-256=`ef40abd512869874a916a936c765f35da613603ccf6ee2f9ec66fffc7df6c169`、lpc_tests=`afb68d86f557f74c5f1cc25814f7d3facd0dcd0d1136d565682a220082b73618`；ASan driver=`a81605c6fe38d10bb60ef07c800e6a749ef22234ab0d0382c33b21b8cc480d20`、lpc_tests=`0e710e6c4235a2de4e917cd1cbcbe8766f43979d745b9e2bfb69cb12808701eb`。`git diff --check` 通过；未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。
- **状态/限制**：I09 Linux/WSL2 本地原子单元 `complete`；真实 header 信任/拒绝、启动失败、Debug/ASan+LSan parser 与 live 路径已验证。没有真实 Windows/macOS/Alpine 环境，未伪造外部平台通过；I06–I09 的完整 P1 集成矩阵、H staged SSL_CTX 热更新和后续 P2/P3 单元仍未闭合。

### I13：frozen value 深度、循环与异常安全复制边界（2026-10-03）

- **状态**：`complete`（Linux/WSL2 本地原子单元；P1 总集成、P4 统一预算和外部平台仍未闭合）。
- **实际改动**：`src/vm/internal/frozen_value.cc` 将 `vm_copy_frozen_svalue()` 的公开 raw 入口改为创建单一 `FrozenCopyState`，复制递归始终携带默认最大深度 `8`、array/mapping 活动路径集合和异常边界；过深、空 array/mapping、循环、非法 mapping key 和 object/function/buffer/class 均在目标发布前返回失败。重复 DAG 在离开当前活动路径后仍可复制。每个新 array、mapping 和临时 mapping key 分别由释放 guard 管理；dest 只在完整构造后发布；入口捕获 C++ 分配异常并返回 `false`。`vm_clone_frozen_value()` 直接复用这一受约束复制路径，不再先走一套会与实际复制分叉的递归。原有 `vm_frozen_value_safe_with_max_depth()` 保留调用者的自定义深度合同，并增加同一循环/空值边界。socket callback 的 buffer 深复制分支未改变，其他 frozen raw caller 继续经统一入口。
- **需求/根因**：旧 `vm_copy_frozen_svalue()`/array/mapping 递归没有深度 context；socket callback 直接调用 raw copy，因此只在前置 safe helper 中验证无法保护所有入口。原实现没有活动路径检测，且构造中途异常不由 RAII 统一接管。修复不增加跨 owner 可变引用共享，不把自定义 JSON 深度收紧为 8，也不增加第二套预算或全局目录。
- **反例与验证覆盖**：新增 `DriverTest.TestFrozenValueCopyEnforcesDepthCyclesAndFailureCleanup`，直接调用 raw copy 覆盖 8 层成功、9 层失败、自定义 `max_depth`、自引用循环、重复 DAG、空 string、空 array、空 mapping、非法 key、array 中途遇到 buffer 失败和 mapping 中途遇到 buffer 失败；失败时 dest 保持空值。现有 mapping/frozen owner tests 继续覆盖正常 string key/value。`TestVmOwnerSocketCallbacksDispatchThroughOwnerExecutor` 与 `TestVmOwnerSocketCallbackExecutorDropsStaleOwnerEpoch` 证明实际 socket callback caller 的深复制、owner admission 和 stale drop 路径未回退。
- **验证命令与结果**：
  1. `cmake --build build-dev-debug --target lpc_tests --parallel 4`、`cmake --build build-dev-debug --target driver owner_runtime_bench --parallel 4`、`cmake --build build-asan --target lpc_tests --parallel 2`、`cmake --build build-ubsan --target lpc_tests --parallel 4` 成功；按 WSL2 约束 `setarch x86_64 -R cmake --build build-tsan --target lpc_tests --parallel 2` 成功。
  2. `run-targeted.py --binary build-dev-debug/src/tests/lpc_tests --gtest-filter='DriverTest.*Frozen*'`：discovery/运行 `9/9`，PASSED `9`、FAILED/SKIPPED `0`，证据 `/tmp/fluffos-xk-i13-debug-frozen/`；socket caller filter `2/2`，证据 `/tmp/fluffos-xk-i13-debug-socket/`。ASan 同两组分别 `9/9`、`2/2`，证据 `/tmp/fluffos-xk-i13-asan-frozen/`、`/tmp/fluffos-xk-i13-asan-socket/`，无 ASan/LSan 诊断；UBSan frozen filter `9/9`，证据 `/tmp/fluffos-xk-i13-ubsan-frozen/`，无 UBSan 诊断。
  3. `setarch x86_64 -R python3 tools/testsuite/run-targeted.py --binary build-tsan/src/tests/lpc_tests --gtest-filter='DriverTest.TestFrozenValueCopyEnforcesDepthCyclesAndFailureCleanup'`：discovery/运行 `1/1`，PASSED `1`、FAILED/SKIPPED `0`，证据 `/tmp/fluffos-xk-i13-tsan-frozen/`，无 TSan 报告。TSan 使用 `setarch` 是 WSL2 必要条件。
  4. 当前 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；Debug/ASan/UBSan/TSan `lpc_tests` SHA-256 分别为 `04b011be7055e5a965f2b2ecc2d75518d105eb43553b680ff35b2d8b751b8c7b`、`50c411a03ad2ec392920bd3cc21d6fe90524bb5545dad67523492824959193cb`、`b71cd40c9b6db00898079e53fb0d80619331296e274d7b327fc0ac84438790c3`、`0e6b7f6d7fac641a75d94ebf3cb9f30b9ec4adbe996e81d39b18716a0816dc3b`；Debug driver=`516252b12497eeed2230867be1e229c7a329abe3cdd004a8fbdfd08555cc3155`，owner_runtime_bench=`b33f4405596987671517b6cd0f1126d763bf7bc3863077eb32ccfbc46efa9214`。相关文件 SHA-256：`frozen_value.cc=77e08591bae7b9290701c4ef8000ab899da137bff3bfa2e9d57fb4e4acd0127c`、`test_lpc.cc=c4ff4bca38c1df078522be1ac5732a069ebee6bfa17e0662707fad84ac1009a3`；本单元两文件 dirty diff SHA-256=`dde571183ff596b050e450f87e4e107670d9c5dd86b46a34887439c33e5872fe`。`git diff --check` 通过。
- **范围限制**：仓库没有可用的全局 allocator-failure 注入接口；中途 child rejection 已实际验证父 array/map guard，C++ 异常路径由 catch/RAII 覆盖，未把它冒充为全局分配器故障注入。没有真实 Windows/macOS/Alpine 环境，未伪造外部平台或 full LPC matrix 通过；P1 其他集成、P4 真实调用分布测量和后续 P2/P3 单元仍未闭合。`src/vm/internal/base/object.cc` SHA-256=`2c5a35e58076962a41241471be9630e9365b9f83af4f3945229ec7583393be7e`、`testsuite/single/tests/efuns/save_object.c` SHA-256=`b59f6eab79e6544a747a7b34075027f3a9fa776e17b21dc004ca1f08d27bc36e`，本单元未修改两处既有外部改动。

### A：Unicode charset 裁剪（2026-10-03）

- **状态**：`complete`（Linux/WSL2 本地原子单元；P2 阶段集成和外部平台仍未闭合）。
- **实际改动**：`src/base/internal/strutils.{h,cc}` 将 `ltrim`、`rtrim`、`trim` 从字节集合 `find_*_not_of` 改为共享的 ICU `U8_NEXT/U8_PREV` Unicode scalar value 遍历；charset 只解析一次，按码点建立集合，源字符串遇非法 UTF-8 时在该边界停止，不跨过非法字节。保留默认六个 ASCII 空白字符；直接 C++ helper 的显式空 charset 不删除，`src/packages/trim/trim.cc` 的空 charset 仍先转换为历史默认集合。移除会导致实现分叉的 `std::string&&` trim overload，所有现有调用者使用 const-reference 返回值。更新英文和中文 trim/ltrim/rtrim 文档、`package_trim.c`，并新增 C++ UTF-8/非法序列边界测试。
- **需求/根因/反例**：旧实现把 charset 的 UTF-8 原始字节作为字符集合；U+3000（`E3 80 80`）与 U+300A（`E3 80 8A`）共享前缀，裁剪《会留下残缺字节。失败证据 `/tmp/fluffos-xk-a-trim-debug-lpc/001-driver-lpc.log` 初次使用未重建的旧 Debug driver，明确报 `package_trim.c:38` 的残缺字符；重建 driver 后 `/tmp/fluffos-xk-a-trim-debug-lpc-retry/001-driver-lpc.log` 暴露新增测试把全四字节字符 trim 的期望误写为单字符，修正为全空后通过。两次原始日志均保留，不计为源码通过或隐藏。
- **反例与验证覆盖**：C++ `StrUtilsTest.TrimCharsetMatchesUtf8Scalars` 覆盖 U+3000/U+300A 共享前缀、三函数方向、显式空 charset、默认 ASCII charset、全四字节字符、非法源字符串和非法 charset；LPC `package_trim` 覆盖 ASCII、中间字符、空串、全 trimset、Unicode charset、默认空参数和四字节字符。`rc.cc`、`lex.cc`、`diagnostic_render.cc`、`external.cc`、`main_generate_keywords.cc` 的现有直接调用随共享 header 一并编译，未保留第二套裁剪逻辑。
- **验证命令与结果**：
  1. `cmake --build build-dev-debug --target lpc_tests --parallel 4`、`cmake --build build-dev-debug --target driver --parallel 4` 成功；Debug C++ runner `StrUtilsTest.TrimCharsetMatchesUtf8Scalars` discovery/运行 `1/1`、PASSED `1`、FAILED/SKIPPED `0`，证据 `/tmp/fluffos-xk-a-trim-debug-cpp/`；Debug LPC runner `/single/tests/efuns/package_trim` `selected/passed=1/1`、failed/skipped `0`，证据 `/tmp/fluffos-xk-a-trim-debug-lpc-final/`。
  2. `cmake --build build-asan --target driver lpc_tests --parallel 2` 成功；ASan C++ 与 LPC 分别 `1/1`，证据 `/tmp/fluffos-xk-a-trim-asan-cpp/`、`/tmp/fluffos-xk-a-trim-asan-lpc/`，`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1` 下无 ASan/LSan 诊断。
  3. `cmake --build build-ubsan --target driver lpc_tests --parallel 2` 成功；UBSan C++ 与 LPC 分别 `1/1`，证据 `/tmp/fluffos-xk-a-trim-ubsan-cpp/`、`/tmp/fluffos-xk-a-trim-ubsan-lpc/`，无 UBSan 诊断。
  4. 按 WSL2 约束 `setarch x86_64 -R cmake --build build-tsan --target driver lpc_tests --parallel 2` 成功；TSan C++ 与 LPC 分别 `1/1`，证据 `/tmp/fluffos-xk-a-trim-tsan-cpp/`、`/tmp/fluffos-xk-a-trim-tsan-lpc/`，无 TSan 报告。
  5. 所有成功 runner 的 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；Debug/ASan/UBSan/TSan driver SHA-256 分别为 `67d8245a89bd7e2e5098e5f8340ae4ddd0afef6ed109ecb6476a54898d13571d`、`6ad5da74fbe85d1409357f83e577700f78e8b252ba8fe3b5039abf0d3bc3b7eb`、`e18359e5a365521db50fb8fcbc6a8be19bbfa82054cac92e71706a0102bfc0d5`、`0119d637bbf8882e16f134ba4fd39a873ed09e1a9abc4a80bcb2f77f0c8bc985`；对应 `lpc_tests` 为 `dc6df3a39f5e34c6ba39e8bf3646ae09a1f02522f4d95f6ad37b64cbec2ab294`、`d9c89693e58ddf9ff67c2813a0c47c05508ec7decda8d6470613447c1cf2e66b`、`e62487335b0dfae26b9ea4f0bb784d6e163f5f64a201a88808379b76be60aafe`、`81aa7b552a66b1cae49f019e6ccf8c26bd1cfd333c9a5b705007e341cd4b2cfb`。文件 SHA-256：`strutils.h=9bbfb2eedb23d87861ebada0f84a3d62b744289b82d79008515183a7a219a67d`、`strutils.cc=d0b07909f9986bc09c82ec53759df1d0c793a53136fc92ab44690ac3e688bdf4`、`package_trim.c=5122b6d8e224a6e637b72565bd17af94f5f7846229c866d9dc6257d0e163468d`、`test_lpc.cc=fb452c7572aed1982c1cfec7d60834a04239fee4f05f72107965506f9436ed57`。`git diff --check` 通过；保留既有 `object.cc`/`save_object.c` 外部改动，未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。

### B：private inherited slot（2026-10-03）

- **状态**：`complete`（Linux/WSL2 本地原子单元；E 依赖已满足，P2/P3 集成和外部平台仍未闭合）。
- **实际改动**：`src/compiler/internal/compiler.cc::handle_functions()` 只在 `DECL_HIDDEN` 的继承槽写回 `cur_def->flags`，保留该 private definition 的独立 slot；visible/public overload 继续写 `FUNC_ALIAS | final_index`，未改 grammar 或其他函数分类。新增 `/inherit/priv_same_{a,b}.lpc`、`/clone/priv_same_{ab,ba,child}.lpc` 和 `/single/tests/compiler/private_inherit_same_name.lpc`，覆盖两种继承顺序、父对象内部调用、父内 function pointer、child public same-name 定义；更新 `docs/lpc/constructs/inherit.md` 说明合同。
- **需求/根因/反例**：旧逻辑对所有不同 `new_index` 写 alias；第二个同名 private inherit 会把第一个 inherit 的 `F_CALL_FUNCTION_BY_ADDRESS` 槽指向后者。隔离旧逻辑构建 `build-b-private-baseline` 的 runner 在 `/tmp/fluffos-xk-b-private-baseline/001-driver-lpc.log` 复现 `private_inherit_same_name.lpc:8`，期望 `1`、实际 `2`；恢复新逻辑后同一测试通过。该 baseline build 仅保留在本任务自有构建目录，未覆盖既有构建目录。
- **验证命令与结果**：
  1. 当前实现 Debug LPC `/single/tests/compiler/private_inherit_same_name` discovery/运行 `1/1`、PASSED `1`、FAILED/SKIPPED `0`，证据 `/tmp/fluffos-xk-b-private-debug-failfirst/`；现有 `DriverTest.TestLPC_FunctionInherit` Debug C++ `1/1` 通过，证据 `/tmp/fluffos-xk-b-private-debug-cpp/`。
  2. ASan 新 LPC 与现有继承 C++ 均 `1/1` 通过，证据 `/tmp/fluffos-xk-b-private-asan/`、`/tmp/fluffos-xk-b-private-asan-cpp/`，`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1` 下无 ASan/LSan 诊断。
  3. UBSan 新 LPC 与现有继承 C++ 均 `1/1` 通过，证据 `/tmp/fluffos-xk-b-private-ubsan/`、`/tmp/fluffos-xk-b-private-ubsan-cpp/`，无 UBSan 诊断。
  4. 按 WSL2 约束使用 `setarch x86_64 -R`，TSan 新 LPC 与现有继承 C++ 均 `1/1` 通过，证据 `/tmp/fluffos-xk-b-private-tsan/`、`/tmp/fluffos-xk-b-private-tsan-cpp/`，无 TSan 报告。
  5. 成功 runner 的 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；当前 Debug/ASan/UBSan/TSan driver SHA-256 分别为 `647d0737bcf0eaee085340696f88365ff7649f0a8846af99b1e6f85e1e4ca2f2`、`c93543091bf04b6fb1a3821458f63a5dc2b2867ff0f03168cf199c33179dfa53`、`0254b2b54d67ec7b8b7a793d538c4407139e99627e084beff43e92760dda2d41`、`2d42cd7e602d1d6fd474df85daaf80910e278a119085246bc0664fadb80c6397`；对应 `lpc_tests` 为 `7cc179c308a84145d7da04a298536c6b243e8bae2e54f29fe7468ec076ad9a7b`、`ad78e158e95e724bf051e37439477cbd34a05ff28159a381a579e132ab7a74b6`、`70d76a711001b28930b77b9ab6de2d229c585763b610a23569fdcf50e33517e9`、`92fdc81ca53820042b30b9768d9b59871f1dffd7b6039c876f6a02b21049ce0d`；旧逻辑 baseline driver=`3ea12f860a13ff9df55c131bad5676cd17c04d5e657fa205605d04bb7bb1a58a`。新增 fixture SHA-256：`priv_same_a.lpc=1ba8f3057d218cbe095f4af40963975ff221752166ac386544a4025a615abd87`、`priv_same_b.lpc=e77a39871a065a538de48c5b29f9311500badc062517c45450269c0633a70f83`、`priv_same_ab.lpc=1e46241e5481ce91c22fde4eb5c8f3dfc82766f9a81a14cc9c2fcd71906f7389`、`priv_same_ba.lpc=56830bc3d3c86fea3e139485bb3428c2cd31d246c9df0fb5d22fd8b78a5c749f`、`priv_same_child.lpc=6ddf21a8b80a2001d68b0a10f6ffa6edb1c98c9b5a12be2fdb0d9b339672e112`、`private_inherit_same_name.lpc=d8e692e43ebd113b274968fd12d0e7af90c61fd475dec1875ba8ae64d73727e4`。`git diff --check` 通过；未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。
- **范围限制**：没有真实 Windows/macOS/Alpine 环境，未伪造外部平台通过；public/visible alias 保持原分支并由现有继承 C++ fixture 编译验证，完整 P2/P3 集成矩阵和 E 后续单元仍未闭合。

### I15 follow-up attempt 11：Release 编译暴露 debugmalloc 字段越界引用

- **状态**：`blocked/root-cause-found`；这是 F 的 Release 验证前置阻塞，不把 Debug/ASan/UBSan/TSan 通过误报为 Release 完成。
- **失败证据**：新建的 `build-absorb-f-release` 在配置完成后按 `cmake --build build-absorb-f-release --target driver lpc_tests --parallel 4` 编译失败；`src/vm/internal/simul_efun.cc:319` 访问 `PTR_TO_NODET(ident)->tag`，但 Release 未定义 `DEBUGMALLOC_EXTENSIONS`，因此 `md_node_t` 没有 `tag` 字段。原始日志保留为 `/tmp/fluffos-xk-i15-release-build-fail-20261003.log`，SHA-256=`98113d4ecfa872cb7b87c341cf71eaecfb099bd9b68ad701003d5987b40346e`。
- **根因**：`simul_efuns_prepare()` 已用非分配 `lookup_perm_ident()` 判断 permanent identifier 是否在 prepare 前存在；不存在时紧随其后的 `find_or_add_perm_ident()` 必须返回本次新建的节点。`md_node_t::tag` 只在 `DEBUGMALLOC_EXTENSIONS` 下存在，不能作为 provisional journal 的语义判定。该访问位于 I15 provisional rollback 代码，不是 F 的 foreach 恢复代码；F 的 Debug/各 sanitizer 证据仍保留，Release gate 必须先解除这个独立编译错误。
- **直接可执行修复计划**：只把 `src/vm/internal/simul_efun.cc` 的 `entry.ident` 条件改为 `!had_perm_ident ? ident : nullptr`；不访问 allocator debug 字段，不改变 `lookup_perm_ident()`、hash ownership、rollback/remove 顺序或 live table。先用只读审计确认 `entry.ident` 只由 `release_provisional_entries()` 消费，再重建 Release `driver lpc_tests`，运行 Release 的 F case；若再次失败，保留原始日志并暂停，不叠加修复。
- **执行前审计结论**：`entry.ident` 的唯一消费者是 provisional rollback/discard 的 `remove_perm_ident()`；`had_perm_ident` 已在同一次 `find_or_add_perm_ident()` 前读取，足以区分本次新建节点和已有 orphan/efun 节点。删除 `tag` 访问不会放宽生产事务边界，也不涉及用户既有 `object.cc`/`save_object.c` 改动。

### I15 follow-up attempt 11 结果：Release 编译阻塞已解除

- **实际变更**：按上一节的最小计划，将 `src/vm/internal/simul_efun.cc` 的 provisional journal 条件改为 `entry.ident = !had_perm_ident ? ident : nullptr`；没有访问 `DEBUGMALLOC_EXTENSIONS` 专属字段，没有改变 identifier hash ownership 或 rollback 顺序。
- **验证**：`cmake --build build-absorb-f-release --target driver lpc_tests --parallel 4` 重试成功，日志 `/tmp/fluffos-xk-f-release-build-retry.log`；Release LPC `/single/tests/compiler/foreach_unwind` 为 `1/1`，证据 `/tmp/fluffos-xk-f-release-lpc/`，driver SHA-256=`0be15f18daf3f9421f459204f8ca601fbb414295019e1fa2d5f69c4db07e7940`。Release C++ 白盒过滤器因测试受 `#ifdef DEBUG` 保护而 discovery 为 0；runner 原始记录 `/tmp/fluffos-xk-f-release-cpp/` 保留，不把 Release 白盒零用例冒充通过或源码失败。I15 整体仍按既有记录保持 `unverified`，本记录只关闭 Release 编译前置阻塞。

### F：foreach 异常恢复临时栈计数（2026-10-03）

- **状态**：`complete`（Linux/WSL2 本地原子单元；Debug/Release/ASan/UBSan/TSan 定向验证通过，P2 集成和外部平台仍未闭合）。
- **实际改动**：`src/vm/internal/base/interpret.h` 为 DEBUG 控制帧保存进入时的 `stack_in_use_as_temporary`；`push_control_stack()` 在真实 VMContext 中记录该边界。`unwind_to_acatch_marker()` 在 `pop_control_stack()` 后恢复保存值，`save_context()`/`restore_context()` 将 `error_context_t` 的临时栈深度纳入 safe_apply/异常恢复；全部恢复使用现有 `vm_context_set_stack_temporary_depth()`，不复制 TLS 全局写入。正常 `F_EXIT_FOREACH`、函数返回、直接 catch、嵌套循环和 safe_apply 错误路径由同一白盒回归覆盖。
- **需求/根因/反例**：foreach 的临时值在异常跳过 `F_EXIT_FOREACH` 时遗留计数，旧实现的最小对照 GTest 使 `caught_in_foreach` 后 DEBUG TLS/VMContext 计数从 `0` 变为 `1`，嵌套路径变为 `3`，后续正常/return 路径仍保持 `3/4`。旧逻辑对照证据为 `/tmp/fluffos-xk-f-baseline-cpp/002-gtest-run.log`，`1` 个测试运行、`0` 通过、`1` 失败；没有用单纯 LPC 返回码替代白盒计数断言。恢复目标是进入边界值，不是无条件归零，因此保留外层合法 temporary。
- **验证命令与结果**：
  1. Debug `cmake --build build-absorb-f-debug --target driver lpc_tests --parallel 4` 成功；LPC `/single/tests/compiler/foreach_unwind` discovery/运行 `1/1`，C++ `DriverTest.ForeachTemporariesRestoredOnUnwind` discovery/运行 `1/1`，均 `PASSED`、failed/skipped `0`。证据分别为 `/tmp/fluffos-xk-f-debug-lpc-final/`、`/tmp/fluffos-xk-f-debug-cpp-final/`；binary SHA-256 为 driver=`5f42146b8c31ddfa2e691c303e8909d003a0d973f7bc14508370c55d5f8826e0`、lpc_tests=`952e933ef03ab957d3f8e207d6a9b03b8999717ed8d88f14a948782e0118dc61`。
  2. ASan（`detect_leaks=1:halt_on_error=1:abort_on_error=1`）LPC/C++ 各 `1/1`，证据 `/tmp/fluffos-xk-f-asan-lpc-final/`、`/tmp/fluffos-xk-f-asan-cpp-final/`，无 ASan/LSan 诊断；driver=`41eb6d9a47643ef2946d5df630d662a55fce18f0ec8459b93c1834887b690d3f`、lpc_tests=`ffb8be3ef8e9ef142928a068f608c577173bd496c74b300c877ed3b98e736e8c`。
  3. UBSan（`halt_on_error=1:print_stacktrace=1`）LPC/C++ 各 `1/1`，证据 `/tmp/fluffos-xk-f-ubsan-lpc-final/`、`/tmp/fluffos-xk-f-ubsan-cpp-final/`，无 UBSan 诊断；driver=`872ce31f1abe30bdd8f48ac21f4dde7f0d8ab71b00b45951a6eb580f282ae5bc`、lpc_tests=`3019f157a962bb88f319156d4ce4b28a1f9ec6b03a02d5bd19f5973e8d8dc20c`。
  4. 按 WSL2 约束使用 `setarch x86_64 -R` 构建/运行 TSan；LPC/C++ 各 `1/1`，证据 `/tmp/fluffos-xk-f-tsan-lpc-final/`、`/tmp/fluffos-xk-f-tsan-cpp-final/`，无 TSan 报告；driver=`97ffce07a3a04546469768d876f5a2b5d98d756edf1e63305abeb6775a3bf1d5`、lpc_tests=`16412df9976ca77a7713237bd8a2ba37d70f9fb4faedb3300b1b557f9b601c97`。
  5. Release `cmake --build build-absorb-f-release --target driver lpc_tests --parallel 4` 成功，driver/lpc_tests 分别为 `0be15f18daf3f9421f459204f8ca601fbb414295019e1fa2d5f69c4db07e7940`/`88d14f07cbb020023f9cd42eebbc70eafb2299894f22834346ed026ee8873e93`；Release LPC `1/1`，证据 `/tmp/fluffos-xk-f-release-lpc/`。Release 白盒测试有意不注册，因测试主体位于 `#ifdef DEBUG`；零 discovery 记录已保留在 `/tmp/fluffos-xk-f-release-cpp/`。
  6. 所有成功 runner 的 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；`git diff --check` 通过。相关文件 SHA-256：`interpret.h=f4f8b1b6937edde67ddd9ce480b511be3baba474a85625f889ce4147f2f4229a`、`interpret.cc=8eaa365a0fc06e7d30b788ed4c7a1a085fc6c3674ddcf89475afc3d31a9f3a0c`、`promise.h=3d662026f82d799cd326bc6f3d3d76794c242a3585458b6736bcef3ede49472d`、`promise.cc=c778c014a1e5362da73880ab1ffa614a94f75f4c1bc9db18d3c5ecf2adea7639`、`simul_efun.cc=2d291c951319789214da2ae5d223a1f90f3301de2d0e7d2af3c990ea1a62cfa2`、`test_lpc.cc=c7b4603bf5ea4a0ce97b7c952db56acc0530e1aa7659eeecc69a48de4b9a2bab`、`foreach_unwind.lpc=804960ed393a605b0fa7422d56dd9e7180e82b678e574515f46a9fd9e01f6f06`。
- **限制/剩余依赖**：没有真实 Windows/macOS/Alpine 环境，未伪造外部平台；未运行完整 `-ftest` 或 P2/P3 集成矩阵。Release 只验证编译和 LPC 路径，DEBUG-only 白盒断言只在 Debug/ASan/UBSan/TSan 运行。

### G fail-first 与 fallback 构建阻塞（2026-10-03）

- **状态**：`blocked/root-cause-found`；正常 Bison 路径已完成 fail-first 和修复后的定向通过，fallback gate 尚未宣称通过。
- **fail-first 证据**：新增 fixture `testsuite/single/tests/operators/compound_assign_index_rhs.lpc` 后，未修改 grammar 的 Debug driver 运行 `/single/tests/operators/compound_assign_index_rhs` 失败；第 16 行 mapping index RHS 的 `intp(i)` 期望 `1`、实际 `0`，日志 `/tmp/fluffos-xk-g-fail-first/001-driver-lpc.log`，SHA-256=`f2d9fc3958056142a63721bec5c815172cfb6baaffbd35af721354826e96db42`。这证明测试触达真实 LPC 编译/复合赋值入口，不是空过滤器。
- **fallback 失败证据**：按 G 规定的 `cmake -S . -B build-absorb-g-fallback -DCMAKE_BUILD_TYPE=Debug -DENABLE_LTO=OFF -DMARCH_NATIVE=OFF -DCMAKE_DISABLE_FIND_PACKAGE_BISON=TRUE` 配置失败，日志 `/tmp/fluffos-xk-g-fallback-configure.log`，SHA-256=`77869a9d5834a79925a02d229121d8766099c7e228b18a0b581d7220c68f42ca`。`src/CMakeLists.txt` 的 grammar 分支允许 Bison 不存在，但 `src/tools/CMakeLists.txt:2` 无条件执行 `find_package(BISON REQUIRED)`，而 `make_func.cc` 只在构建树中由 `make_func.y` 生成；全局禁用 Bison 会在 grammar fallback 分支前于 tools 阶段终止配置。
- **根因审计**：`make_func` 是 efun 生成器的唯一工具目标，`src/CMakeLists.txt` 仅通过该目标生成 `efuns.autogen.{cc,h}`；源树没有 `make_func` 的 fallback 生成产物。补充一个由仓库内 `make_func.y` 生成的 source-tree fallback，并让 tools CMake 在 Bison 可用时继续使用 `BISON_TARGET`、不可用时使用该产物，是解除指定 fallback gate 的最小 build-config/生成物修复；不改变 grammar 语义、efun 表或运行时 API。
- **直接可执行修复计划**：用本机已确认的 Bison 3.8.2 从 `src/tools/make_func.y` 生成 `src/tools/make_func.autogen.cc`；把 `src/tools/CMakeLists.txt` 的 Bison 查找改为非 REQUIRED，Bison 存在时保持现有 `BISON_TARGET(MakeFunc ...)`，否则把 `make_func.autogen.cc` 作为 `make_func` 源。先在只读审计确认该 target 没有其他调用者和 fallback 产物不进入源码运行时，再重新配置/编译 fallback 目录；若生成器或构建再次失败，保留日志并暂停 G，不叠加试错修改。
- **执行前审计结论**：`make_func.y` 的唯一入口是 `make_func <packages.fullspec>`，生成文件只用于构建期 `make_func`；条件化 Bison 不会让普通 Bison 构建改走 fallback，也不会改变 `grammar.autogen.*` 的正常生成分支。新增 fallback 只补齐计划已经要求的 `CMAKE_DISABLE_FIND_PACKAGE_BISON=TRUE` 验收环境。

### G 结果：index RHS 复合赋值类型与 fallback 生成链闭合（2026-10-03）

- **状态**：`complete`（Linux/WSL2 本地原子单元；P2/P3 集成、完整 `-ftest` 和外部平台仍未闭合）。
- **实际改动**：`src/compiler/internal/grammar.y` 将 `unknown_dynamic_rhs` 拆为 dynamic-call 与 `NODE_BINARY_OP` 的 `F_INDEX/F_RINDEX` 两类，仅四个算术 `op=` 在目标左值为 `TYPE_REAL` 且 RHS 为 `TYPE_ANY/TYPE_UNKNOWN` 时走已有 `promote_to_float()`；普通 mixed 左值、字符索引和错误路径不走宽泛转换。按 Bison 3.8.2 规定命令重新生成 `src/compiler/internal/grammar.autogen.cc`；`grammar.autogen.h` 无 token/type 变化，重新生成结果与现有文件一致。
- **新增覆盖**：`testsuite/single/tests/operators/compound_assign_index_rhs.lpc` 覆盖 mapping forward index、mixed array forward/reverse index 的 `+=`、`-=`、`*=`、`/=`，dynamic call/evaluate 反例、mixed 左值提升、字符索引类型错误和除零错误。旧逻辑 fail-first 为 `1` 个失败（mapping index 第 16 行期望 `intp(i)=1`、实际 `0`），修复后不再依赖空过滤器。
- **fallback 生成链修复**：因原 `src/tools/CMakeLists.txt` 的 `find_package(BISON REQUIRED)` 使计划指定的无 Bison 配置无法到达 grammar fallback，新增由 `src/tools/make_func.y` 用 Bison 3.8.2 直接生成的 `src/tools/make_func.autogen.cc`；tools CMake 在 Bison 可用时保留 `BISON_TARGET`，不可用时使用该 source-tree 生成物。普通路径仍明确显示 Bison 3.8.2 并生成构建树 `make_func.cc`；fallback 配置明确显示 `make_func` 和 grammar 都使用源码生成物。
- **验证命令与结果**：
  1. 正常 Bison Debug `cmake --build build-absorb-f-debug --target driver --parallel 4` 成功；新测试 `/single/tests/operators/compound_assign_index_rhs` `1/1` 通过，driver SHA-256=`d474b52fa8b48009b8108a6feaff184474e2e4849529ca2ebcdd32ec28bcc1e6`，证据 `/tmp/fluffos-xk-g-debug-final/`。既有 `/single/tests/operators/compound_assign_float` `1/1` 通过，证据 `/tmp/fluffos-xk-g-debug-existing-final/`。
  2. `CMAKE_DISABLE_FIND_PACKAGE_BISON=TRUE` fallback 配置在修复后成功，`build-absorb-g-fallback` 的 `driver` 构建成功；新测试 `1/1` 通过，driver SHA-256=`0f0e8eba2971884e752af01974de9ff5934a5de9f7b35e822a7b2fd202a3dd4e`，证据 `/tmp/fluffos-xk-g-fallback-lpc-final/`。fallback 的源码依赖由 `build-absorb-g-fallback` 的 `DependInfo/build.make` 只读核对为 `src/compiler/internal/grammar.autogen.cc` 与 `src/tools/make_func.autogen.cc`。
  3. ASan（`detect_leaks=1:halt_on_error=1:abort_on_error=1`）新测试 `1/1` 通过，无 ASan/LSan 诊断，driver SHA-256=`f6a4f608e8d6ac7b49df6088c216217adf7c83df504a7f0cf519f6d63ee90d76`，证据 `/tmp/fluffos-xk-g-asan-lpc/`。
  4. UBSan（`halt_on_error=1:print_stacktrace=1`）新测试 `1/1` 通过，无 UBSan 诊断，driver SHA-256=`5c119ec7e47f0840369a24e79d85de554697df1efcce537fc21ccb4c32ad4072`，证据 `/tmp/fluffos-xk-g-ubsan-lpc/`。
  5. 按 WSL2 约束使用 `setarch x86_64 -R` 构建/运行 TSan；新测试 `1/1` 通过，无 TSan 诊断，driver SHA-256=`390f9ef38ba4f1cba8d0bbcea180b15361058e0012802da8dc0ea20a05b157dc`，证据 `/tmp/fluffos-xk-g-tsan-lpc/`。
  6. 所有成功 runner 的 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`；`git diff --check` 通过。相关文件 SHA-256：`grammar.y=d9557ad7eedb1cbf2807c35ae30188ed8fc8d23067e6b795a6a95a31a3123567`、`grammar.autogen.cc=957a2728daa94f35c83d30d816eeb331f9e6edd954da2525d3c2675eb567982d`、`grammar.autogen.h=839cbff5c25a979472841875dfad961b46bb8f1ad3551c9e11fdba6bcf1bf0ef`、`src/tools/CMakeLists.txt=68de24424c72c674ee4595fadb9d7fc5cbf5b82e88a8d1e069c791850757dadf`、`make_func.autogen.cc=4fd04aa8fc448c60d7d1fd27b7f5826a02a63d2e83366cfab6a077e9740e04cd`、fixture=`18238437a3af8ee1f812d1943839fb3ccf13889180585f4117f104f04615f123`。
- **限制/剩余依赖**：未运行完整 LPC `-ftest`、fuzz 或 P2/P3 集成矩阵；没有真实 Windows/macOS/Alpine 环境，未伪造外部平台通过；未执行 commit/push、远端 CI、部署、重启或下游 mudlib 操作。

### D1：PCRE2 迁移、缓存租约与 legacy efun 合同（2026-10-03）

- **状态**：`complete`（Linux/WSL2 本地原子单元已闭合；Windows/macOS/Alpine 外部构建、Docker daemon、libFuzzer、远端 CI 属于 external-required；P2/P3 集成不属于 D1）。
- **实际改动**：`cmake/FindPCRE.cmake` 只接受 PCRE2 10.42+、`pcre2.h` 和 8-bit `pcre2-8`，在配置时执行真实 API 编译/链接探针，并清除旧 PCRE1 缓存变量；`src/packages/pcre` 全部改用 PCRE2 8-bit API，保留 legacy efun 名称、参数顺序、数值 flags、数组返回和 callback 合同，固定 UTF-8 校验且不启用 UCP/JIT。缓存键现在同时包含共享 pattern 和 compile flags；每个运行实例独立持有 ovector/match state，缓存项用 borrower lease 管理，重入期间被驱逐的项进入 rooted detached 链，最后一个 borrower 释放后才销毁。编译错误、参数类型、异常 match code、UTF-8 非法字节和超长 replacement 均在真实使用点拒绝。CI、Alpine static Dockerfile、英文/中文构建文档和 pcre API 文档同步切换到 PCRE2；Docker static 依赖显式包含 `pcre2-static`。
- **需求/根因/反例**：旧 package 直接依赖 PCRE1 `pcre.h`/`libpcre`，不能满足 PCRE2 版本、UTF-8、缓存键和独立 match state 合同；新增 LPC 反例覆盖 invalid pattern/subject、错误参数类型、3/4 参数 flag 形态、UTF-8 非法字节、数组/命名 capture、未参与 capture、zero-length/global match、replacement 长度/嵌套 capture 和 callback 重入。callback 进一步编译 `520` 个唯一 pattern，强制 outer callback 仍持有 pattern 时触发缓存驱逐，验证 detached lease 路径未释放正在使用的 PCRE2 code。旧库拒绝探针使用只含 `pcre.h`/`libpcre.so` 的隔离 root，CMake 以 `Could NOT find PCRE (missing: PCRE_LIBRARY PCRE_INCLUDE_DIR PCRE2_VERSION_OK PCRE2_API_PROBE)` 退出 `1`，没有回退链接 PCRE1。
- **验证命令与结果**：自建的隔离 PCRE2 10.42 头文件和 static `libpcre2-8.a` 通过 `FindPCRE` API probe；Debug/ASan/UBSan 的 `driver` 分别构建成功，binary SHA-256 为 `c01b88211d29ddb9e14b7facb29768198d04f9dfb71750e2515c844b2f446c41`、`2f29fdaa7322b7c56920a2faa37d9aafb2853533e1fdcf2cc995c24aaae6dfee`、`53b236c8cfe220bd38c31a040b8296739b3a1c9e000be60869ca3293d79256d4`；`/single/tests/efuns/pcre`、`pcre_replace`、`pcre_replace_type_checks` 在三种构建各 `1/1`，合计 `9/9`，failed/skipped `0`，证据目录为 `/tmp/fluffos-xk-d1-final-{pcre,pcre_replace,pcre_replace_type_checks}-{debug,asan,ubsan}/`，ASan/LSan 和 UBSan 无诊断。`PACKAGE_PCRE=OFF` 的隔离 Debug configure/driver build 成功；无 PCRE2 的普通 `PACKAGE_PCRE=ON` configure 和旧 PCRE1 cache 隔离 probe 均明确失败；`git diff --check` 通过。当前 source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`。
- **补充闭合验证（source HEAD=`fe163d67f40cef6d445f076e4a2d565799cd4627`）**：
  1. Debug/ASan/UBSan `driver --all-lpc --config config.test` 均 `292/292`，runner 记录 `failed=0 skipped=0`，证据分别为 `/tmp/fluffos-xk-d1-final/lpc-debug/`、`lpc-asan/`、`lpc-ubsan/`；当前 driver SHA-256 分别为 `fb373dc7462bc859bd46f72f6995c450e7985e219979b46659c1ded63fb76fd2`、`3e0ebb25177205701e9e3c6e2616b4d9d454dc3fef39da22f8fe5eb896ca14e5`、`482813204a5a5ef420ba9be3a6cfc1f775ad51747b5cbace6cf2fa346e39599c`。修复 Release-only cleanup 缺陷后，Debug/ASan/UBSan/Release 的最终重建 LPC 也均为 `292/292`，证据为 `/tmp/fluffos-xk-d1-final/lpc-{debug,asan,ubsan}-final/` 和 `/tmp/fluffos-xk-d1-final/release/lpc/`；最终 driver SHA-256 为 `e78cd73b0db394e7cc48bebf029edc9faec29616c658217ee519862b4b070147`、`5288602be8b1263ce7f88f8a1ad4a42f5a305ff92fb127e09d1e3b7308c4247d`、`42fc16ebcca517175faef2d068f0365a48300797af3dd24bed42e54a8dde15c1`、`05e32c788ab66a89e3a6afa3415b3a552baea8a862bf57b629f14f35b4565200`。
  2. Debug/ASan/UBSan 的 `lpc_tests` 均 `486 passed + 1 skipped`（487 tests）；Debug/ASan/UBSan CTest 在补建 `ofile_tests`、`compile_arena_tests` 后均 `497/497` 通过，唯一跳过为未构建 lpcshell 交互合同。日志为 `/tmp/fluffos-xk-d1-final/gtest-{debug,asan,ubsan}.log`、`ctest-{asan,ubsan}.log`；Debug CTest 终端记录同样为 `497/497`；Release 最终 CTest 为 `496/496` 通过，唯一跳过仍为 lpcshell 交互合同。日志为 `/tmp/fluffos-xk-d1-final/ctest-{debug,asan,ubsan}-final.log` 和 `/tmp/fluffos-xk-d1-final/release/ctest.log`。
  3. `PACKAGE_PCRE=OFF` Debug 构建、CTest `497/497` 和非 PCRE `/single/tests/efuns/package_trim` `1/1` 通过；完整 LPC 运行按合同在 `/single/tests/efuns/pcre` 处明确拒绝未启用的 PCRE efun，未误记为通过。普通缺 PCRE2 配置和注入 PCRE1 cache 的两个隔离配置均以 `1` 退出，并报告 `PCRE2_VERSION_OK PCRE2_API_PROBE` 缺失。
  4. Debug/ASan/UBSan `gateway_fuzz_smoke` 各运行 256 个输入，均报告 `frames_received=1024`、`length_rejected=256`、`json_rejected=512`、`masters_left=0`、`pending_left=0`；本机无可用 Clang/libFuzzer，未把失效的旧 PCRE1 `build-fuzz` 目录冒充当前 fuzz 证据。
  5. 当前 Debug driver 实际 live 验收通过：`ws-ascii`、`ws-telnet`、`ws-mccp-telnet`、`telnet-mccp`、`ws-burst`、`ws-burst-flush`，以及 `wss-ascii`、`wss-telnet`、`wss-mccp-telnet`、`wss-burst`、`wss-burst-flush`；WSS burst 分别收到期望的 `6000` 和 `160000` 个 `y`，MCCP websocket 拒绝且 plain telnet 接受。证据为 `/tmp/fluffos-xk-d1-final/{ws-live,wss-live}/`。
  6. `.github/workflows/*.yml` 已由 PyYAML 解析通过；Docker CLI 存在但 daemon 不可连接（`/var/run/docker.sock` 不存在），因此未执行 Alpine/Docker image build。Windows/macOS/Alpine 主机构建、真实 libFuzzer、远端 CI 仍待对应环境。
  7. 修正 `tools/testsuite/test-port-isolation.sh` 只比较本次新增的 sandbox、fallback lockdir 和同一 driver 可执行文件进程，避免既有目录或其他 checkout 的 driver 造成误报；`--quick`（`1+4`）和完整压力矩阵（`5+20`）均通过，日志为 `/tmp/portiso-serial-*.log`、`/tmp/portiso-conc-*.log`。工作区原有两个 `.run-isolated-*` 目录和 `/home/mechrevo/projects/xiakexing/driver/bin/driver` 均未删除或干扰结果。
  8. Release CTest 首次暴露 `TestSimulEfunReloadCreateFailureRollback`：`release_provisional_entries()` 把有副作用的 `remove_perm_ident()` 放在 `assert()` 参数内，`NDEBUG` 下整个清理调用被删除，导致 fresh identifier 残留；已将调用移出 `assert`，Debug/Release 定向测试均通过，随后四配置全量 LPC 和 Release CTest 均通过。
- **范围限制**：发布、部署、重启或下游 mudlib 操作仍不属于 D1；P2/P3 集成矩阵和其他平台不属于本地 D1 闭合证据。
