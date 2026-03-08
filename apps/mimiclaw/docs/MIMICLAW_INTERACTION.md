# MimiClaw 交互机制文档

## 概述

MimiClaw 是一个运行在 TuyaOpen 设备上的 AI 助手，通过 Telegram Bot 和 WebSocket 两个渠道接收用户消息，经过 Agent 处理层调用 LLM 和工具，再将结果回写给用户。整个系统由四条独立线程和一个双向消息队列总线驱动。

---

## 系统线程一览

| 线程名 | 入口函数 | 职责 |
|---|---|---|
| `mimi_tg_poll` | `telegram_poll_task` | 长轮询 Telegram Bot API，将收到的消息写入入站队列 |
| `mimi_ws` | `ws_server_task` | WebSocket 服务器，处理 TCP 连接与消息帧，写入入站队列 |
| `mimi_agent` | `agent_loop_task` | 从入站队列消费消息，驱动 LLM + 工具调用循环，将结果写出站队列 |
| `mimi_outbound` | `outbound_dispatch_task` | 从出站队列消费消息，按渠道发送到 Telegram 或 WebSocket |

消息在线程之间以 `mimi_msg_t` 结构体传递：

```c
typedef struct {
    char  channel[16];   // "telegram" | "websocket" | "cli"
    char  chat_id[32];   // 会话唯一标识（Telegram chat id 或 ws_<fd>）
    char *content;       // heap 分配的文本内容，消费方负责 free
} mimi_msg_t;
```

消息总线内部维护两条 FIFO 队列（长度 8），分别为 `inbound`（入站）和 `outbound`（出站）。

---

## 完整交互流程

```
用户 (Telegram / WebSocket)
        │
        │  HTTP long-poll / TCP frame
        ▼
┌───────────────────┐
│  Input Adapter    │  mimi_tg_poll  /  mimi_ws
│  (接收 + 解析)    │
└────────┬──────────┘
         │  message_bus_push_inbound(msg)
         ▼
   ┌───────────┐
   │ Inbound Q │  (FIFO, 深度 8)
   └─────┬─────┘
         │  message_bus_pop_inbound(WAIT_FOREVER)
         ▼
┌──────────────────────────────────────────────────────┐
│  Agent Loop  (mimi_agent)                            │
│                                                      │
│  1. context_build_system_prompt()                    │
│     └─ SOUL.md + USER.md + MEMORY.md + 近期日记      │
│                                                      │
│  2. session_get_history_json(chat_id, max=20)        │
│     └─ 读取 /spiffs/sessions/tg_<chat_id>.jsonl      │
│                                                      │
│  3. 构造 messages = [history... , {user: text}]      │
│                                                      │
│  4. 工具迭代循环 (最多 MIMI_AGENT_MAX_TOOL_ITER=10次) │
│     ┌────────────────────────────────────────────┐   │
│     │ a. 随机选一条 working_phrase，push_outbound  │   │
│     │ b. llm_chat_tools(system, messages, tools) │   │
│     │ c. 若 tool_use == false → 保存 final_text   │   │
│     │    break                                   │   │
│     │ d. 若有 tool_calls:                         │   │
│     │    - tool_registry_execute(name, input)    │   │
│     │    - 将 assistant + tool_result 追加 messages│  │
│     │    - 继续下一轮                              │   │
│     └────────────────────────────────────────────┘   │
│                                                      │
│  5. push_outbound(final_text) 或 push error msg      │
│  6. session_append(chat_id, user/assistant)          │
└──────────────────────────────────────────────────────┘
         │  message_bus_push_outbound(msg)
         ▼
   ┌────────────┐
   │ Outbound Q │  (FIFO, 深度 8)
   └─────┬──────┘
         │  message_bus_pop_outbound(WAIT_FOREVER)
         ▼
┌───────────────────────┐
│  Outbound Dispatcher  │  mimi_outbound
│  按 channel 路由:      │
│  "telegram" → telegram_send_message()               │
│  "websocket"→ ws_server_send()                      │
└───────────────────────┘
        │
        ▼
   用户收到回复
```

---

## working_phrases 机制详解

### 作用

LLM API 调用是**同步阻塞**的（HTTP POST，超时 120 秒），每次工具迭代都需要等待 LLM 响应。为了避免用户长时间看不到任何回馈，Agent 在**每次调用 LLM 之前**立即向出站队列推送一条"正在处理"的状态消息。

### 文案池

```c
// agent_loop.c — agent_loop_task()
const char *working_phrases[] = {
    "mimi is working...",
    "mimi is thinking...",
    "mimi is pondering...",
    "mimi is on it...",
    "mimi is cooking..."
};
```

每次从中**随机**取一条（`bounded_random_index`，用硬件随机数取模），避免每次都发送相同文案。

### 触发时机

```
iteration 0:
  → push_outbound("mimi is on it...")    ← 立刻发给用户
  → llm_chat_tools(...)                  ← 等待 LLM（可能数秒）
  → LLM 返回 tool_calls
  → 执行工具

iteration 1:
  → push_outbound("mimi is cooking...")  ← 再发一条状态
  → llm_chat_tools(...)                  ← 再次等待 LLM
  → LLM 返回纯文本 (tool_use=false)
  → break

  → push_outbound(final_text)            ← 发送最终回复
```

每次进入新的迭代轮次（无论是第一轮还是工具调用后的后续轮）都会发一条 working_phrase，因此用户在复杂的多步推理过程中可能收到多条状态消息，随后才收到最终答案。

### 实现代码片段

```c
// agent_loop.c:152-162
while (iteration < MIMI_AGENT_MAX_TOOL_ITER) {
    mimi_msg_t status = {0};
    strncpy(status.channel, in_msg.channel, sizeof(status.channel) - 1);
    strncpy(status.chat_id,  in_msg.chat_id,  sizeof(status.chat_id)  - 1);
    uint32_t phrase_index = bounded_random_index((uint32_t)phrase_count);
    status.content = strdup(working_phrases[phrase_index]);
    if (status.content) {
        if (message_bus_push_outbound(&status) != OPRT_OK) {
            free(status.content);
        }
    }
    // 紧接着调用 LLM ...
```

---

## Telegram 通道细节

### 接收消息（长轮询）

`mimi_tg_poll` 线程循环调用：

```
GET /bot{token}/getUpdates?offset={s_update_offset}&timeout=30
```

- 超时窗口 30 秒（若使用 HTTP 代理则降为 20 秒）
- 请求成功后解析 JSON `result` 数组，逐条处理 `update`
- 每条 `update` 更新 `s_update_offset = update_id + 1`（去重）
- 仅处理含 `text` 字段的 `message`，忽略其他类型（如 `document`）
- 错误时指数退避（2s → 4s → … → 最大 60s）

### 发送消息

`telegram_send_message(chat_id, text)` 向出站队列消费方调用：

- 将文本按 `MIMI_TG_MAX_MSG_LEN`（4096 字节）分段
- 每段 POST `/bot{token}/sendMessage`：

```json
{
  "chat_id": "...",
  "text": "...",
  "parse_mode": "Markdown"
}
```

- 首次尝试带 Markdown，若失败（非 200 或 `ok!=true`）自动降级为纯文本重试

### TLS 证书

- 非 Linux 平台：通过 `mimi_tls_query_domain_certs` 预先查询 `api.telegram.org` 的 CA 证书并缓存
- Linux 平台：若证书不可用则跳过验证（fallback no-verify）
- 若配置了 HTTP 代理（`MIMI_SECRET_PROXY_HOST`），则通过 CONNECT 隧道转发所有 Telegram 请求

---

## WebSocket 通道细节

- 监听端口：`MIMI_WS_PORT = 18789`，最大并发连接 `MIMI_WS_MAX_CLIENTS = 4`
- 完成 RFC 6455 握手后，使用 `select` 多路复用处理所有客户端
- 入站帧格式（客户端发送）：

```json
{ "type": "message", "content": "用户文本", "chat_id": "可选自定义会话ID" }
```

- 出站帧格式（服务端发送）：

```json
{ "type": "response", "content": "回复内容", "chat_id": "..." }
```

- `chat_id` 若客户端未指定，默认为 `ws_<fd>`

---

## LLM 调用细节

支持两种 Provider，通过 `MIMI_SECRET_MODEL_PROVIDER` 配置：

| Provider | 端点 | 鉴权头 | 工具格式 |
|---|---|---|---|
| `anthropic`（默认）| `api.anthropic.com/v1/messages` | `x-api-key` + `anthropic-version` | Anthropic tools schema |
| `openai` | `api.deepseek.com/v1/chat/completions` | `Authorization: Bearer` | OpenAI function calling |

- `max_tokens`：4096
- HTTP 超时：120 秒
- 模型可通过 NVS 或编译期 `MIMI_SECRET_MODEL` 配置，运行时通过串口 CLI 的 `set_model` 命令修改

---

## 工具列表

Agent 每次迭代可调用的工具（最多 4 个 tool call / 轮，最多 10 轮）：

| 工具名 | 功能 |
|---|---|
| `web_search` | 联网搜索当前信息 |
| `get_current_time` | 获取当前时间（POSIX TZ，默认 PST8PDT） |
| `read_file` | 读取 `/spiffs/` 下的文件 |
| `write_file` | 写入/覆盖 `/spiffs/` 下的文件 |
| `edit_file` | 字符串替换编辑文件 |
| `list_dir` | 列出 `/spiffs/` 目录内容 |

---

## 会话与记忆持久化

- **会话历史**：存储在 `/spiffs/sessions/tg_<chat_id>.jsonl`，每行一条 `{role, content, ts}` JSON。每次对话读取最近 20 条作为上下文，Agent 回复后追加用户和助手两条记录。
- **长期记忆**：`/spiffs/memory/MEMORY.md`，由 LLM 自主通过 `write_file`/`edit_file` 工具维护。
- **日记**：`/spiffs/memory/daily/<YYYY-MM-DD>.md`，由 LLM 自主写入当天发生的事。
- **人格/用户信息**：`/spiffs/config/SOUL.md`（人格设定）、`/spiffs/config/USER.md`（用户信息），每次构建 system prompt 时读入。

---

## 配置优先级

所有关键配置（Token、API Key、模型等）按以下优先级生效：

1. **NVS（运行时写入）**：最高优先级，通过串口 CLI 命令持久化到 Flash
2. **`mimi_secrets.h`（编译期）**：若存在则覆盖默认值
3. **`mimi_config.h` 默认值**：兜底

---

## 关键常量速查

| 常量 | 值 | 说明 |
|---|---|---|
| `MIMI_AGENT_MAX_TOOL_ITER` | 10 | 工具调用最大迭代轮数 |
| `MIMI_MAX_TOOL_CALLS` | 4 | 单轮 LLM 响应最多解析几个 tool call |
| `MIMI_AGENT_MAX_HISTORY` | 20 | 携带的历史消息条数 |
| `MIMI_TG_POLL_TIMEOUT_S` | 30 | Telegram 长轮询超时（秒） |
| `MIMI_TG_MAX_MSG_LEN` | 4096 | Telegram 单条消息最大字节 |
| `MIMI_BUS_QUEUE_LEN` | 8 | 消息队列深度 |
| `MIMI_WS_PORT` | 18789 | WebSocket 监听端口 |
| `MIMI_WS_MAX_CLIENTS` | 4 | WebSocket 最大并发连接数 |
| `MIMI_LLM_MAX_TOKENS` | 4096 | LLM 单次响应最大 token |
