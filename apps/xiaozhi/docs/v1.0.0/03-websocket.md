# v1.0.0 WebSocket 通道

## 1. 适用范围

代码位置：`apps/xiaozhi/src/xiaozhi_ws.c`、`apps/xiaozhi/src/xiaozhi_ws_handshake.c`、`apps/xiaozhi/src/xiaozhi_protocol.c`

WebSocket 通道用于：

- 建立控制连接
- 交换文本 JSON 消息
- 传输二进制音频帧

## 2. 配置项

可通过 CLI 写入：

```bash
xz_ws url wss://your.server/ws
xz_ws token your_token
xz_ws version 1
xz_proto websocket
```

查看当前配置：

```bash
xz_ws show
```

关键字段：

- `url`
- `token`
- `version`

## 3. 握手流程

### 3.1 URL 解析

支持：

- `ws://`
- `wss://`

默认端口：

- `ws://` 使用 `80`
- `wss://` 使用 `443`

### 3.2 握手请求

当前实现已对齐上游以下行为：

- 生成随机 `Sec-WebSocket-Key`
- 发送 `Host: host:port`
- 发送 `Upgrade: websocket`
- 校验 `Sec-WebSocket-Accept`

同时还会附带业务头：

- `Authorization`
- `Protocol-Version`
- `Device-Id`
- `Client-Id`

## 4. 建链后的 hello 交换

### 4.1 设备端发送 hello

WebSocket 建立后，设备会发送：

```json
{
  "type": "hello",
  "version": 1,
  "transport": "websocket",
  "features": {
    "mcp": true
  },
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  }
}
```

若平台启用了 AEC，还可能带上：

```json
"aec": true
```

### 4.2 服务端返回 hello

设备要求服务端返回：

- `type = hello`
- `transport = websocket`

典型返回：

```json
{
  "type": "hello",
  "transport": "websocket",
  "session_id": "xxx",
  "audio_params": {
    "format": "opus",
    "sample_rate": 24000,
    "channels": 1,
    "frame_duration": 60
  }
}
```

收到后会记录：

- `session_id`
- 服务器侧 `sample_rate`
- 服务器侧 `frame_duration`

## 5. 设备到服务端的消息

### 5.1 listen

用于开始、停止或上报唤醒检测：

```json
{
  "session_id": "xxx",
  "type": "listen",
  "state": "start",
  "mode": "manual"
}
```

支持的 `state`：

- `start`
- `stop`
- `detect`

支持的 `mode`：

- `auto`
- `manual`
- `realtime`

### 5.2 abort

用于中断当前会话或打断播报：

```json
{
  "session_id": "xxx",
  "type": "abort",
  "reason": "wake_word_detected"
}
```

### 5.3 mcp

用于传输 MCP JSON-RPC：

```json
{
  "session_id": "xxx",
  "type": "mcp",
  "payload": {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/list",
    "params": {}
  }
}
```

## 6. 服务端到设备的消息

当前处理的核心消息包括：

- `hello`
- `stt`
- `tts`
- `llm`
- `mcp`
- `system`
- `alert`
- `custom`

其中：

- `stt`：识别结果文本
- `tts`：播放生命周期控制
- `llm`：情绪或文本展示信息
- `mcp`：下发工具调用或返回结果

## 7. 二进制音频

WebSocket 二进制帧用于承载 Opus 音频数据。

在参考协议中支持多种二进制版本：

- 版本 1：直接发送 Opus 数据
- 版本 2：带时间戳等扩展头
- 版本 3：更紧凑的头部结构

`apps/xiaozhi` 当前重点是完成 WebSocket 文本协商和音频帧收发钩子，并在 Linux 目标上打通语音链路。

## 8. Linux 场景建议流程

```bash
xz_ota check
xz_ws show
xz_proto websocket
xz_reconnect
xz_status
xz_listen start manual
```

服务端正常时，预期时序为：

1. 建立 WebSocket 握手
2. 发送 `hello`
3. 收到 server `hello`
4. 发送 `listen start`
5. 上传麦克风音频
6. 收到 `tts start`
7. 收到二进制音频
8. 收到 `tts stop`

## 9. 适合 WebSocket 的场景

- 快速联调
- Linux 本地验证
- 不需要单独分离控制与音频链路时
- 服务端已经直接暴露 WebSocket 接口时
