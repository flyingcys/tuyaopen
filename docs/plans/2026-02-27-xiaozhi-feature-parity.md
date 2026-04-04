# Xiaozhi Non-HW Feature Parity Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 在不涉及音频播放、显示、硬件控制的前提下，使 `apps/xiaozhi` 的功能行为与 `xiaozhi-esp32` 在协议与业务控制层完全对齐。

**Architecture:** 采用“协议层对齐 + 业务事件层补齐 + MCP JSON-RPC 内核补齐 + OTA 元数据补齐”的增量方式。先补可独立验证的纯逻辑模块，再接入 `xiaozhi_app` 工作线程与 CLI，最后做端到端编译与行为验证。对于硬件相关 MCP 工具仅保留非硬件工具（系统信息、重启、固件升级入口）。

**Tech Stack:** C (TuyaOpen TAL/cJSON/http/mqtt/tls), TuyaOpen build (`tos.py`), Linux CLI 验证

---

### Task 1: 建立功能对齐清单与基线接口

**Files:**
- Modify: `apps/xiaozhi/README_CN.md`
- Create: `apps/xiaozhi/src/xiaozhi_events.h`

**Step 1: 写“预期行为测试清单”（文档化断言）**

- 在 README_CN 增加“非硬件功能对齐矩阵”：
  - 协议报文（hello/listen/abort/mcp/goodbye）
  - 下行消息类型（tts/stt/llm/system/alert/custom）
  - MCP JSON-RPC（initialize/tools/list/tools/call）
  - OTA 元数据（activation/mqtt/websocket/server_time/firmware）
  - 状态流转（connecting/listening/speaking/idle）

**Step 2: 运行静态检查（文档存在且可读）**

Run: `rg -n "功能对齐矩阵|MCP JSON-RPC|server_time|firmware" apps/xiaozhi/README_CN.md`
Expected: 能命中新增条目。

**Step 3: 新增事件枚举头文件**

- 定义统一事件类型与事件 payload 结构，供 WS/MQTT 解析后上抛。

**Step 4: 编译验证**

Run: `cd apps/xiaozhi && tos.py build`
Expected: 编译通过。

### Task 2: 补齐下行消息解析与业务分发

**Files:**
- Create: `apps/xiaozhi/src/xiaozhi_message.h`
- Create: `apps/xiaozhi/src/xiaozhi_message.c`
- Modify: `apps/xiaozhi/src/xiaozhi_ws.c`
- Modify: `apps/xiaozhi/src/xiaozhi_mqtt_udp.c`
- Modify: `apps/xiaozhi/src/xiaozhi_app.c`

**Step 1: 先写失败用例（最小化）**

- 为 `xiaozhi_message_parse_type` 写纯逻辑断言（tts/stt/llm/system/alert/custom/mcp/unknown）。

**Step 2: 运行用例确认失败**

Run: `cc -I src -I ../../src/libcjson/cJSON tests/test_xiaozhi_message.c src/xiaozhi_message.c ../../src/libcjson/cJSON/cJSON.c -o /tmp/test_xz_msg`
Expected: 初始失败（函数未实现或断言失败）。

**Step 3: 最小实现并接入 WS/MQTT**

- 抽出统一解析函数，WS/MQTT 收到文本后均复用。
- 在 `xiaozhi_app` 中增加事件回调：
  - `tts.start/stop/sentence_start`
  - `stt.text`
  - `llm.emotion`
  - `system.command(reboot)`
  - `alert`
  - `custom`
- 非硬件范围内，至少保证状态机语义与日志语义一致，不依赖显示/音频渲染。

**Step 4: 重新运行用例确认通过**

Run: 同 Step 2
Expected: 通过。

**Step 5: 全量构建**

Run: `cd apps/xiaozhi && tos.py build`
Expected: 通过。

### Task 3: 补齐 MCP JSON-RPC 内核（非硬件工具）

**Files:**
- Create: `apps/xiaozhi/src/xiaozhi_mcp.h`
- Create: `apps/xiaozhi/src/xiaozhi_mcp.c`
- Modify: `apps/xiaozhi/src/xiaozhi_app.h`
- Modify: `apps/xiaozhi/src/xiaozhi_app.c`
- Modify: `apps/xiaozhi/src/cli_cmd.c`

**Step 1: 写失败用例**

- `initialize` 返回 `protocolVersion/capabilities/serverInfo`
- `tools/list` 分页与 `nextCursor`
- `tools/call` 参数校验与错误回包

**Step 2: 跑失败用例**

Run: `cc -I src -I ../../src/libcjson/cJSON tests/test_xiaozhi_mcp.c src/xiaozhi_mcp.c ../../src/libcjson/cJSON/cJSON.c -o /tmp/test_xz_mcp`
Expected: 失败。

**Step 3: 实现最小可用 MCP 内核**

- 支持 JSON-RPC 2.0 基础校验。
- 支持方法：`initialize/tools/list/tools/call`。
- 内置非硬件工具：
  - `self.get_system_info`
  - `self.reboot`
  - `self.upgrade_firmware`（触发升级入口）
- 回包通过 `mcp` 上行发送。

**Step 4: 通过用例 + 集成编译**

Run:
- `cc .../test_xiaozhi_mcp.c ...`
- `cd apps/xiaozhi && tos.py build`
Expected: 均通过。

### Task 4: 补齐 OTA server_time/firmware 元数据语义

**Files:**
- Modify: `apps/xiaozhi/src/xiaozhi_ota.h`
- Modify: `apps/xiaozhi/src/xiaozhi_ota.c`
- Modify: `apps/xiaozhi/src/xiaozhi_app.c`
- Modify: `apps/xiaozhi/src/cli_cmd.c`

**Step 1: 写失败用例**

- OTA JSON 包含 `server_time` 时应解析并保存。
- OTA JSON 包含 `firmware` 时应解析版本/URL/force。

**Step 2: 跑失败用例**

Run: `cc -I src -I ../../src/libcjson/cJSON tests/test_xiaozhi_ota_parse.c src/xiaozhi_ota.c src/xiaozhi_settings.c src/xiaozhi_system.c ../../src/libcjson/cJSON/cJSON.c -o /tmp/test_xz_ota`
Expected: 失败。

**Step 3: 最小实现**

- 新增 OTA 结果字段：`has_server_time/server_timestamp_ms/timezone_offset_min/has_firmware/...`
- 保持现有 activation/config 逻辑不回归。
- 新增 CLI：查询 firmware 元信息、触发升级入口（非硬件）。

**Step 4: 回归构建**

Run: `cd apps/xiaozhi && tos.py build`
Expected: 通过。

### Task 5: 端到端验收与文档收敛

**Files:**
- Modify: `apps/xiaozhi/README_CN.md`

**Step 1: 运行最终验证命令**

Run:
1. `cd apps/xiaozhi && tos.py build`
2. `cd apps/xiaozhi && rg -n "initialize|tools/list|tools/call|tts|stt|llm|system|alert|custom|server_time|firmware" src`

Expected:
- 构建通过。
- 关键能力均存在代码实现引用。

**Step 2: 更新 README 的“已对齐能力”与“不在范围内能力”**

- 明确本次对齐范围：不含音频播放/显示/硬件控制。

**Step 3: 完整性复查**

- 对照 `xiaozhi-esp32/main` 的同类能力点逐条核对。

