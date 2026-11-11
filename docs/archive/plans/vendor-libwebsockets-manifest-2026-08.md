# libwebsockets Vendor Patch Manifest（2026-10）

> 生成日期：2026-10-11（R2 复核，延续 A6 审计项）
> 本地基准：`src/thirdparty/libwebsockets`（当前 HEAD 树）
> 上游对照：`1797eaaf`（同步 ws-over-h2 mux servicing 的上游提交）
> 内容差异：2 个本地 TLS 适配；另有 28 个纯模式差异。4.5.8 的升级基线为 `8fe05a5d`，其后的 ws-over-h2 修复已包含在本地 vendor 树中。

## 1. 来源说明

本地 vendor 树与本仓库保存的上游同步提交 `1797eaaf` 逐文件对照。
`8fe05a5d` 的 4.5.8 升级以及其后的 `5ac4e8c0`、`46fdc2b5`、`0da9f26b`、
`1797eaaf` 上游修复均已进入当前树；不能把这些上游内容误记为本地补丁。
模式差异来自本地 checkout 的执行位，不改变文件内容。

## 2. 内容差异（2 个本地适配）

| 文件 | 归属 | 内容 |
|---|---|---|
| `include/libwebsockets/lws-context-vhost.h` | 本地 TLS 合同 | 增加 `ssl_min_proto_version` 字段，供驱动传递最低 TLS 版本。 |
| `lib/tls/openssl/openssl-server.c` | 本地 TLS 合同 | 在 OpenSSL 后端应用 `ssl_min_proto_version`，不支持时明确失败。 |

这两个字段由驱动侧 TLS 配置直接使用；它们不是 libwebsockets 4.5.8 的上游变更，升级 vendor 时必须重新核对。

## 3. 模式差异（28 个，仅文件位变化，内容与 1797eaaf 一致）

`cmake/`（2 个）、`lib/jose`/`lib/tls/mbedtls`（4 个）、`scripts/`（18 个）与
`win32port/zlib/`（4 个）为执行位/权限位差异，内容为空；本地保留 checkout 默认位。

## 4. 保留理由与跟踪

- **保留**：上游 4.5.8 及其后的 ws-over-h2 修复，优于只保留 `8fe05a5d` 的快照；
  构建与本驱动 WS 层已验证兼容（Gateway 传输/代理/回调 19 个 case 全通过）。
- **跟踪**：后续 lws vendor 升级以 `1797eaaf` 对照；再升级时逐文件比较内容、模式
  和本地 TLS 字段，并更新本表来源提交。
- 本地驱动侧适配（非 vendor 上游内容）包括：`src/CMakeLists.txt` 的
  `LWS_SUPPRESS_DEPRECATED_API_WARNINGS ON`、`LWS_WITH_LEJP ON`；
  `src/net/websocket.cc` 的 `lws_http_mount` 初始化（`cache_no` 字段）与
  default-vhost adoption；以及上表列出的最低 TLS 版本字段和 OpenSSL 应用逻辑。
  这些改动不应在 vendor 更新时被覆盖。
