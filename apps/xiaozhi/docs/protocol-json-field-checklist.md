# 协议 JSON 字段级 1:1 对照清单（TuyaOpen xiaozhi vs xiaozhi-esp32，重新审计）

> 时间：2026-02-27  
> 口径：请求/响应 JSON 字段结构与语义一致；你暂缓的 2 项单独列出，不计阻塞。

## 1. 音频通道消息（WS / MQTT-UDP）

### 1.1 `hello`

- 字段：`type/version/transport/features/audio_params`
- 结论：已对齐

### 1.2 `listen/abort/mcp/goodbye`

- 字段：
  - `listen`: `session_id/type/state` + 可选 `mode/text`
  - `abort`: `session_id/type` + 可选 `reason`
  - `mcp`: `session_id/type/payload`
  - `goodbye`: `session_id/type`
- 结论：已对齐

### 1.3 服务端文本消息分发（`tts/stt/llm/alert/custom/mcp/system`）

- 字段：`type` 主分发 + 对应子字段
- 结论：软件路径已对齐（显示/硬件动作不作为本轮阻塞）

## 2. MCP JSON-RPC

### 2.1 RPC 外层

- 字段：`jsonrpc/id/method/params`
- 结论：已对齐（含 `params` 非 object 时与上游一致为静默丢弃）

### 2.2 `initialize` / `tools/list` / `tools/call`

- 字段：`protocolVersion/capabilities/serverInfo/tools/nextCursor/annotations`
- 结论：已对齐

### 2.3 `self.upgrade_firmware` schema

- 字段：`inputSchema.properties.url.type=string` 且带 `default`、不在 `required`
- 结论：已对齐

## 3. OTA check / activate

### 3.1 HTTP 头

- 字段：`Activation-Version/Device-Id/Client-Id/Serial-Number/User-Agent/Accept-Language/Content-Type`
- 结论：已对齐（含激活阶段 v1/v2 判定语义）

### 3.2 check payload JSON

- 字段：
  - 顶层：`version/language/flash_size/minimum_free_heap_size/mac_address/uuid/chip_model_name`
  - `chip_info/application/partition_table/ota/display/board`
- 结论：字段结构已对齐；ESP 目标下关键值语义已对齐（`minimum_free_heap_size` 语义已修正）

### 3.3 activate payload JSON

- 字段：`algorithm/serial_number/challenge/hmac`（v2）或 `{}`（v1）
- 结论：已对齐（ESP 目标支持硬件 HMAC 路径）

## 4. 非 JSON 但与协议流程相关的对齐项（本轮已修复）

1. WS 握手：`Host: host:port`、默认 `User-Agent`、随机 `Sec-WebSocket-Key`。
2. WS 握手：`Sec-WebSocket-Accept` 校验。

## 5. 仍在你“暂缓”范围内的 2 项

1. `vision` 能力到真实相机运行时绑定（当前为配置持久化 + 回调接口）
2. `assets partition_valid` 真实分区探测（当前为 KV/环境变量控制）

## 6. 当前结论

- 按“JSON 字段级 + 流程参数一致”口径：**除你暂缓的 2 项外，已完成 1:1 对齐**。
