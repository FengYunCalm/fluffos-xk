# Project Scope / 项目范围

## Independent fork / 独立 fork

FluffOS_XK is an independent fork maintained primarily for team and personal
self-use. It is not intended to imitate the official FluffOS public release
process.

FluffOS_XK 是独立维护的 fork，主要供团队和个人自用。本项目不以模仿官方
FluffOS 的正规公开发布流程为目标。

The default deliverable is a tested source tree and rebuildable binaries for
our own downstream mudlibs. Game content, accounts, deployment configuration,
secrets, and operations policy stay in the downstream game repository.

默认交付物是经过测试的源码树，以及供自有下游 mudlib 重建的二进制。游戏内容、
账号、部署配置、密钥和运维策略仍保留在下游游戏仓库。

## Out of scope by default / 默认不在范围内

The project does not maintain or require:

- public GitHub Release tags or release announcements;
- registry image promotion or public package distribution;
- release signing, provenance, or attestation workflows;
- branch-protection and official required-check governance;
- an official cross-platform release matrix or external production-capacity claim.

以下内容默认不维护，也不作为任务完成条件：

- GitHub 公开 Release tag 或发布公告；
- registry 镜像推广或公开包分发；
- release 签名、provenance 或 attestation 流程；
- 分支保护和官方 required-check 治理；
- 官方跨平台发布矩阵或外部生产容量声明。

Removing these release surfaces does not remove local engineering quality
requirements. A change is ready for self-use only after the relevant build,
regression tests, LPC tests, sanitizer checks, security checks, and downstream
smoke checks pass.

移除这些发布表面不等于降低本地工程质量要求。变更只有在相关构建、回归测试、
LPC 测试、sanitizer、安全检查和下游 smoke 检查通过后，才适合自用。

## Local acceptance / 自用验收

Use a pinned commit from this repository, build the required targets with the
appropriate CMake preset, run the affected tests, and validate the real
下游 mudlib before replacing its driver binaries. Runtime changes must retain
same-owner serialization, owner admission, stale-task rejection, safe cleanup,
and the documented main-thread compatibility boundary.

从本仓库固定一个 commit，使用适用的 CMake preset 构建目标，运行受影响测试，并在
替换 driver 二进制前验证真实下游 mudlib。运行时变更必须保持 same-owner 串行、
owner admission、stale task 拒绝、安全清理以及文档规定的 main-thread 兼容边界。

The Docker workflow is a local build-and-run smoke check only. It never
publishes an image. CI, SBOM, dependency pin checks, CodeQL, sanitizer builds,
and runtime contract tests remain useful self-use safeguards.

Docker workflow 只负责本地构建和运行 smoke，不发布镜像。CI、SBOM、依赖 pin 检查、
CodeQL、sanitizer 构建和运行时合同测试仍是有价值的自用安全保障。
