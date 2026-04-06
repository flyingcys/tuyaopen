# apps/xiaozhi v1.0.0 文档

本文档对应 `apps/xiaozhi` 当前仓库内可见版本 `1.0.0`。  
目录覆盖设备注册、OTA/激活、WebSocket、MQTT+UDP、MCP、Linux 运行、CLI 命令、完整流程和排障说明。

## 阅读顺序

1. [概览](./01-overview.md)
2. [设备注册与激活](./02-device-registration-and-activation.md)
3. [WebSocket 通道](./03-websocket.md)
4. [MQTT + UDP 通道](./04-mqtt-udp.md)
5. [MCP 协议与工具调用](./05-mcp.md)
6. [Linux 构建、运行与验证](./06-linux-run-and-verify.md)
7. [CLI 命令参考](./07-cli-reference.md)
8. [常见问题与排障](./08-troubleshooting.md)
9. [完整流程总览](./09-full-process.md)

## 版本范围

- 应用版本：`1.0.0`
- 主要代码路径：`apps/xiaozhi/src/`
- 协议参考来源：
  - `xiaozhi-esp32/docs/websocket.md`
  - `xiaozhi-esp32/docs/mqtt-udp.md`
  - `xiaozhi-esp32/docs/mcp-protocol.md`
  - `xiaozhi-esp32/docs/mcp-usage.md`

## 说明

- 本目录以 `apps/xiaozhi` 当前实现为准。
- 对协议语义的解释参考 `xiaozhi-esp32` 文档与源码行为。
- 若后续出现 `v1.1.0`、`v1.2.0`，请按同样结构新增版本目录。
