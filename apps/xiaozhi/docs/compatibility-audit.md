# TuyaOpen xiaozhi 与 xiaozhi-esp32 功能对齐审计（软件层，重新审计）

> 审计时间：2026-02-27  
> 审计方法：不沿用历史结论，重新逐文件对照 `xiaozhi-esp32/main` 与 `TuyaOpen/apps/xiaozhi/src`。  
> 审计范围：软件协议/流程/URL/参数/请求响应 JSON；按你要求，不把音频播放、显示、硬件控制作为阻塞项。

## 一、本轮已完成并确认对齐的关键修复

1. WebSocket 握手对齐：
   - 对齐了 `Host: host:port`、`User-Agent` 默认值、随机 `Sec-WebSocket-Key`、`Sec-WebSocket-Accept` 校验。
   - 对齐文件：`xiaozhi_ws.c`。
2. OTA 激活语义对齐：
   - `Activation-Version` 与 `Serial-Number` 判定改为“有 serial 即 v2”，与上游一致。
   - 激活 payload 对齐为：有 serial 时发送 `algorithm/serial_number/challenge/hmac`，无 serial 时发送 `{}`。
   - ESP 目标补齐 `efuse user_data` 序列号回退读取逻辑。
   - 对齐文件：`xiaozhi_ota.c`。
3. MCP `params` 异常行为对齐：
   - 当 `params` 不是 object 时，按上游行为静默丢弃（不回 RPC error）。
   - 对齐文件：`xiaozhi_mcp.c`。
4. `minimum_free_heap_size` 语义对齐：
   - ESP 目标改为 `esp_get_minimum_free_heap_size()`，与上游语义一致。
   - 对齐文件：`xiaozhi_ota.c`、`xiaozhi_app.c`。

## 二、本轮明确暂缓（由你后续处理）

1. `vision` 能力到真实相机运行时能力的 1:1 绑定（当前为配置持久化 + 回调接口）。
2. `assets partition_valid` 的真实分区探测（当前为 KV/环境变量控制）。

## 三、审计结论

按你定义的“严格 1:1 功能兼容（流程 + URL + 参数 + JSON 字段级）”口径，在你排除的范围之外：

- 结论：**已达到 1:1 对齐**（以 `xiaozhi-esp32` 为准）。
- 仅剩上述 2 个你已确认暂缓项未纳入本轮闭环。
