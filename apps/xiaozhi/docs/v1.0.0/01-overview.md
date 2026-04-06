# v1.0.0 概览

## 1. 应用定位

`apps/xiaozhi` 是 TuyaOpen 版本的小智应用实现，协议行为对齐 `xiaozhi-esp32`，支持两类通信方式：

- `WebSocket`：控制消息和音频可共用同一条 WebSocket 连接。
- `MQTT + UDP`：MQTT 负责控制消息，UDP 负责实时音频数据。

## 2. 当前能力

`v1.0.0` 已覆盖以下模块：

- 设备身份初始化：`device_id`、`client_id`
- OTA 配置拉取
- 激活码展示与挑战应答激活
- WebSocket 握手与消息收发
- MQTT 建链、订阅、控制消息交换
- UDP 音频通道建立与 AES-CTR 加密传输
- `listen`、`abort`、`mcp`、`goodbye`、`tts`、`stt`、`llm` 等消息处理
- Linux 目标的音频采集、播放和手工验证

## 3. 代码目录

核心代码主要位于：

- `apps/xiaozhi/src/xiaozhi_app.c`
- `apps/xiaozhi/src/xiaozhi_ota.c`
- `apps/xiaozhi/src/xiaozhi_ws.c`
- `apps/xiaozhi/src/xiaozhi_mqtt_udp.c`
- `apps/xiaozhi/src/xiaozhi_protocol.c`
- `apps/xiaozhi/src/xiaozhi_mcp.c`
- `apps/xiaozhi/src/cli_cmd.c`

## 4. 关键概念

### 4.1 设备身份

- `device_id`：设备物理身份，通常来源于 MAC 或硬件标识。
- `client_id`：软件实例身份，通常为本地生成并持久化的 UUID。
- `serial_number`：可选，用于激活 v2 语义。
- `activation_secret`：可选，用于基于 challenge 生成 HMAC。

### 4.2 OTA 引导

应用启动后可先访问 OTA 配置服务，服务端可下发：

- `mqtt` 配置
- `websocket` 配置
- 激活码或激活挑战
- 固件升级地址和版本信息

### 4.3 会话与状态

小智的语音会话围绕 `session_id` 展开，典型状态流转是：

`空闲 -> 连接 -> 收到 server hello -> 开始 listen -> 上传音频 -> 接收 TTS/STT/MCP -> 结束会话 -> 回到空闲`

## 5. 协议版本要点

- WebSocket `hello.version` 在 CLI 中可配置，默认值为 `1`。
- MQTT+UDP `hello.version` 在当前实现中固定使用 `3`。
- OTA check payload 顶层 `version` 为 `2`。

## 6. 平台说明

### 6.1 Linux

- 支持原生构建和运行。
- 当前 Linux 语音链路重点支持 WebSocket。
- 音频设备依赖 ALSA `default`。

### 6.2 T5AI / ESP 类平台

- 支持通过 TuyaOpen 构建配置编译。
- OTA 逻辑中补齐了 ESP 侧运行时字段和激活相关信息。

## 7. 建议阅读路径

- 想看注册和初始化：先读 [设备注册与激活](./02-device-registration-and-activation.md)
- 想看 WebSocket：读 [WebSocket 通道](./03-websocket.md)
- 想看 MQTT 音频链路：读 [MQTT + UDP 通道](./04-mqtt-udp.md)
- 想看整体时序：读 [完整流程总览](./09-full-process.md)
