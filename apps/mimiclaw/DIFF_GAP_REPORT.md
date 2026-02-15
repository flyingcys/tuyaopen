# mimiclaw 与 TuyaOpen/apps/mimiclaw 差异与缺口报告

## 对齐结论
- 当前实现已达到“核心原理一致、平台接口不同”的迁移目标。
- 主要运行链路（消息总线 -> Agent ReAct 工具循环 -> 会话落盘 -> Telegram/WS 出站）与 `mimiclaw/main` 保持一致。
- 保留的差异集中在平台能力差异（`esp_*` vs `tal_*`）与暂未实现功能（OTA）。

## 已修复的原理差异

### 1. Agent 工具调用链（ReAct）
- 文件：`TuyaOpen/apps/mimiclaw/agent/agent_loop.c`
- 修复前：仅单轮 `llm_chat`，没有工具迭代执行。
- 修复后：
- 使用 `llm_chat_tools` + `MIMI_AGENT_MAX_TOOL_ITER` 迭代。
- 解析并追加 `assistant/tool_use` 与 `user/tool_result` 消息块。
- 通过 `tool_registry_execute` 执行工具，并将结果回填给 LLM。
- 仅把最终 assistant 文本写入 session 并下发 outbound。

### 2. 系统提示词策略
- 文件：`TuyaOpen/apps/mimiclaw/agent/context_builder.c`
- 修复前：提示词过于简化，缺少工具使用和持久化记忆策略。
- 修复后：
- 对齐原版的工具说明（`web_search/get_current_time/read_file/write_file/edit_file/list_dir`）。
- 对齐原版 memory 规则（`MEMORY.md`、daily notes、主动写记忆策略）。

### 3. 启动时序（WiFi 门控）
- 文件：`TuyaOpen/apps/mimiclaw/mimi.c`
- 修复前：未联网也启动 Telegram/Agent/WS。
- 修复后：
- 先启动 CLI 与本地能力。
- WiFi 成功连接后再启动 Telegram/Agent/WS/Outbound。
- WiFi 失败时仅告警，不启动网络依赖服务。

## 原理一致但接口不同（符合迁移预期）
- `wifi_manager`：`esp_netif/esp_wifi` -> `netmgr/tal_wifi`。
- `ws_server`：ESP socket 体系 -> `tal_net` + WebSocket 握手/帧收发。
- `llm_proxy`、`telegram_bot`、`tool_web_search`：ESP HTTP 实现 -> Tuya `http_client_interface`。
- 线程/队列/日志：`FreeRTOS/ESP_LOG` -> `tal_thread/tal_queue/PR_*`。

## 未实现项（占位）
- OTA
- 文件：`TuyaOpen/apps/mimiclaw/ota/ota_manager.c`
- 函数：`ota_update_from_url`
- 状态：占位实现，待手工接入目标平台 OTA 流程。

