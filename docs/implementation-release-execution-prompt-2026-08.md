# FluffOS_XK 实施与发布执行 Prompt（已退休）

本文件是历史执行 Prompt，不再作为当前 agent 的启动指令。当前执行方案已经完成最终审计收口，见 [`docs/implementation-release-execution-plan-2026-08.md`](implementation-release-execution-plan-2026-08.md)。

不得依据本文件自动启动新的源码修改、upstream 吸收、全量测试、发布、部署、registry mutation 或真实签名流程。本批次的运行时源码验证锚点为 `f051d230a10be9b5d6e9f0f40cc69b6801f4d4c3`，结论为 `blocked`；Promise、stack-lvalue、external-handle/object-store 完整迁移、签名/provenance 和生产容量均已明确记录为 deferred 或 external-required。

如需开始新的架构或发布工作，必须先建立新的范围和授权，并重新执行只读基线审计；不得把本历史 Prompt 当作持续执行授权或当前待办清单。
