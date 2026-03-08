# mimiclaw 迁移验收清单（TuyaOpen）

## 范围
- 目标目录：`TuyaOpen/apps/mimiclaw`
- 目标要求：
- 使用 TuyaOpen 接口
- 不依赖 `esp_*` / `freertos/*` / `nvs_*`
- 不使用 `compat` 兼容层参与编译
- 无法直接迁移的接口允许空实现，后续手工补齐

## 当前状态
- `CMakeLists` 已改为 TuyaOpen `EXAMPLE_LIB` 构建方式
- 业务源码统一切到 `OPERATE_RET` + `tal_*` 类型体系
- `compat/` 目录未参与编译
- 非兼容层源码中已无 `esp_*` / `freertos/*` / `nvs_*` 引用
- `wifi_manager` 已切到 `netmgr` 链路（状态查询、IP 获取、SSID/PASS 下发）
- `wifi_manager_scan_and_print` 已接入 `tal_wifi_all_ap_scan`（AP 扫描与结果打印）
- `llm_proxy` 已接入 TuyaOpen `http_client_interface`（含证书查询、Anthropic/OpenAI 基础请求与解析）
- `telegram_bot` 已接入 TuyaOpen `http_client_interface`（`getUpdates` 长轮询 + `sendMessage` 发送）
- `serial_cli` 已接入 TuyaOpen `tal_cli`（配置/会话/内存/网络命令集）
- `http_proxy` 已接入 TuyaOpen `tuya_transporter`（基础 CONNECT 隧道读写）
- `ws_server` 已接入 TuyaOpen `tal_net`（WebSocket 握手、收发帧、消息总线入站/出站）
- `tool_web_search_execute` 已接入 TuyaOpen `http_client_interface`（Brave Search API 基础查询）
- `agent_loop` 已对齐原版 ReAct 工具循环（`llm_chat_tools` + `tool_use/tool_result` 多轮迭代）
- `context_builder` 已对齐原版系统提示词策略（工具规则 + memory 持久化策略）
- `mimi.c` 启动时序已对齐（仅 WiFi 成功后启动 Telegram/Agent/WS/Outbound）

## 编译验证
- 验证命令：
```bash
cd /home/share/samba/openclaw/TuyaOpen
source ./export.sh
cd /home/share/samba/openclaw/TuyaOpen/apps/mimiclaw
tos.py build
```
- 结果：`BUILD SUCCESS`
- 产物目录：`TuyaOpen/apps/mimiclaw/dist/mimiclaw_1.0.0`

## 空实现说明
- 当前仍为占位实现的函数清单见：
- `TuyaOpen/apps/mimiclaw/STUBS_TODO.md`
- 全量差异与缺口说明见：
- `TuyaOpen/apps/mimiclaw/DIFF_GAP_REPORT.md`

## 后续联调建议
- WiFi 真实联网流程（连接、重连、扫描）
- Telegram 边界场景（媒体消息、429 限流、网络抖动重试）
- LLM 工具调用链与错误重试策略
- Proxy 隧道上的 TLS 升级策略
