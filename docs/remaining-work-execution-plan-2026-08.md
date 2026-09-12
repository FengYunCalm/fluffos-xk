# FluffOS_XK 剩余工作方案（最终审计收口）

> **当前状态：`blocked`。本文件不再承载可直接执行的待办。**

## 1. 已收口的运行时工作

源码验证锚点为 `f051d230a10be9b5d6e9f0f40cc69b6801f4d4c3`。本批次已完成并推送：

- `e7044bf3`：recompile/layout 跨翻译单元 ABI 误读修复；
- `f1020aed`：gametick 溢出和 recompile migration 前置条件收口；
- `bbee8c84`：Windows Release CPU timing probe workload 修正；
- `f051d230`：simul_efun rollback 测试使用实际 `survivor_slot`。

与最后一个源码提交直接相关的 Debug、ASan、UBSan simul_efun 定向集分别为 `3/3`、`3/3`、`3/3`。GitHub CI `34702766236` 的 15 个矩阵 job 和 Evidence Gate 全部成功；Docker `34702766223`、CodeQL `34702766232` 成功。3 份 capacity envelope 绑定同一 SHA，并通过 `check-evidence.py --mode gate`。

## 2. 封存的阻塞事实

| 范围 | 状态 | 当前事实 |
|---|---|---|
| Promise / `T_PROMISE` / async substrate | `deferred` | 缺少本地 value、EGC、park/resume、owner、cancel、shutdown、recompile 和错误传播合同。 |
| stack-lvalue ABI | `deferred` | 缺少稳定栈槽、nested reference、unwind、owner 隔离和 legacy opcode 合同。 |
| external-handle / canonical object-store | `deferred` | 现有 bridge 与 fast path 不能证明完整单写迁移、stale 拒绝、回滚和清理。 |
| Libevent、日志、gateway、future retention 等风险 | `unverified` | 本批次没有为各风险生成新的 fail-first、故障注入和压力证据。 |
| upstream 缺失对象 | `external-required` | partial clone 无法 hydrate 的对象不作为本地实现依据。 |
| branch protection / required checks | `external-required` | live 查询显示 `main` 未启用 branch protection。 |
| 签名、provenance、attestation、registry | `external-required` | 没有真实签名服务、来源证明和受保护 registry mutation 证据。 |
| 生产容量 | `external-required` | 没有真实 300-player、长时稳定性和受保护生产环境证据。 |

这些状态是审计结论，不是本批次遗留的开放清单。

## 3. 最终判定

当前不能标记 `release-ready`。本地回归与 CI 只能证明已交付源码在现有门禁下通过，不能替代 deferred 架构、live 保护环境、签名/provenance 或生产容量证据。

本方案已经收口：不保留 TODO、逐阶段恢复命令、未绑定的“下一步”或隐含的自动执行授权。新的架构或外部门禁如获授权，需另行建立独立方案。
