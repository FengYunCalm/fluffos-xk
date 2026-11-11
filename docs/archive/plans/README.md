# 归档：历史计划与实施记录

> 本目录是**冻结历史**，只用于追溯。当前状态与待办见 [路线图](../../roadmap.md)。
> 不得据本目录的任何文档判断当前代码行为：它们描述的是各自基线时刻的状态。
>
> 归档策略：`tools/docs/check-docs.py` 对 `docs/archive/` 只校验 Markdown 链接可达，
> 不再校验内联路径与 commit SHA（历史文档会引用已删除文件与上游提交）。

## 计划与报告（2026-11 归档）

| 文档 | 原位置 | 内容 |
| --- | --- | --- |
| [project-modernization-plan-2026-10.md](project-modernization-plan-2026-10.md) | `docs/` | 全面整理主计划 + 实施记录（308 KB，基线 `fe163d67`） |
| [upstream-absorption-plan-2026-10.md](upstream-absorption-plan-2026-10.md) | `docs/` | 上游 38 提交吸收账本（基线 `795eb371`） |
| [upstream-absorption-plan-d07e7641-735bd31f.md](upstream-absorption-plan-d07e7641-735bd31f.md) | `docs/` | 更早的上游吸收方案 |
| [upstream-sync-optimization-plan-2026-08.md](upstream-sync-optimization-plan-2026-08.md) | `docs/` | 上游同步草案 v2.4（基线 `80b0f329`） |
| [upstream-sync-evidence-2026-08.md](upstream-sync-evidence-2026-08.md) | `docs/` | 同名方案的实施证据 |
| [optimization-master-plan-2026-08-18.md](optimization-master-plan-2026-08-18.md) | `docs/` | 深度优化草案 v0.3（基线 `2785c8a6`） |
| [backlog-plan-2026-09.md](backlog-plan-2026-09.md) | `docs/` | 四项遗留事项（已执行） |
| [backlog-full-plan-2026-09.md](backlog-full-plan-2026-09.md) | `docs/` | 全量留档处置方案（33 项 NEEDS-PORT） |
| [AUDIT_FINDINGS-2026-10.md](AUDIT_FINDINGS-2026-10.md) | 仓库根 | 69 项审计发现（基线 `55c7360b`） |
| [REMEDIATION_PLAN-2026-10.md](REMEDIATION_PLAN-2026-10.md) | 仓库根 | 对应的修复方案 |
| [lpcshell-prerequisite-plan-2026-08.md](lpcshell-prerequisite-plan-2026-08.md) | `docs/` | lpcshell 前置方案 |
| [recompile-followup-plan-2026-08.md](recompile-followup-plan-2026-08.md) | `docs/` | 重编译后续项 |
| [recompile-object-special-design-2026-08.md](recompile-object-special-design-2026-08.md) | `docs/` | 重编译 v1 特殊设计（被 v2 取代） |
| [branch-evidence-archive-20261019.md](branch-evidence-archive-20261019.md) | `docs/` | 分支证据归档 |
| [external-required-readiness-2026-09.md](external-required-readiness-2026-09.md) | `docs/` | 外部依赖就绪快照 |
| [e4-read-source-line-port-eval-2026-08.md](e4-read-source-line-port-eval-2026-08.md) | `docs/` | E4 行号/端口评估 |
| [tsan-ftest-report-2026-08-19.md](tsan-ftest-report-2026-08-19.md) | `docs/` | TSan/LPC 报告 |
| [vendor-libwebsockets-manifest-2026-08.md](vendor-libwebsockets-manifest-2026-08.md) | `docs/` | 第三方清单记录（权威清单是 `third_party/manifest.yaml`） |
| [build_v2017.md](build_v2017.md) | `docs/` | 上游 v2017 构建说明 |
| [project-knowledge-map-2026-08.md](project-knowledge-map-2026-08.md) | `docs/` | 旧知识地图（已被 [architecture.md](../../architecture.md) 取代） |

## 设计证据（2026-11 归档）

`../evidence/` 下为阶段性设计/验证记录（scratch arena、结构化诊断、lpcshell 等）。
原始命令行日志已删除（可由 harness 重跑），保留的是带结论的说明文档。
