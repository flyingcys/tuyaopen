# MimiClaw 时间处理机制分析（中文）

本文针对 `mimiclaw` 主工程，分析在“无 RTC、无断电保持”前提下，系统时间是如何获取、何时设置、有哪些风险，以及是否通过网络同步时间。

---

## 1. 结论先行

`mimiclaw` **不是**通过常驻 NTP/SNTP 服务自动同步时间，而是通过一个工具 `get_current_time` 在需要时发起 **HTTP 请求读取 `Date` 响应头**，再调用 `settimeofday()` 设置系统时钟。

- 是网络校时：**是**
- 方式：**HTTP Date 头（api.telegram.org）**
- NTP/SNTP 自动同步：**否（当前代码未实现）**
- 开机自动校时：**否（当前代码未实现）**
- 断电后时间保持：**否（无持久化恢复逻辑）**

---

## 2. 关键代码证据

### 2.1 唯一的“设系统时间”入口

在 `main/tools/tool_get_time.c` 中：

- `parse_and_set_time(...)` 解析时间字符串并调用 `settimeofday(&tv, NULL)`  
  位置：`main/tools/tool_get_time.c:21`、`main/tools/tool_get_time.c:54`

这基本是主工程里唯一明确修改系统时钟的路径。

### 2.2 时间来源不是 NTP，而是 HTTP Date 头

同文件中有两种拉取方式：

1. 代理路径：通过 CONNECT 隧道对 `api.telegram.org:443` 发 `HEAD /`，从响应头提取 `Date`  
   位置：`main/tools/tool_get_time.c:64`、`main/tools/tool_get_time.c:91`
2. 直连路径：`esp_http_client` 访问 `https://api.telegram.org/`，读取 `Date` 头  
   位置：`main/tools/tool_get_time.c:109`、`main/tools/tool_get_time.c:133`

### 2.3 时区处理

- 先临时 `TZ=UTC0`，把 GMT 时间转 epoch
- 再恢复到 `MIMI_TIMEZONE`

位置：
- `main/tools/tool_get_time.c:43`、`main/tools/tool_get_time.c:48`
- 默认时区常量：`main/mimi_config.h:59`

### 2.4 工具触发方式（按需，不是开机自动）

- 工具注册名：`get_current_time`  
  位置：`main/tools/tool_registry.c:72`
- system prompt 明确要求“需要时间时调用该工具”  
  位置：`main/agent/context_builder.c:41`、`main/agent/context_builder.c:56`
- Agent 每轮都会构建 system prompt，并在 LLM 返回 `tool_use` 后执行 `tool_registry_execute(...)`，这就是“提示词触发校时”的实际执行链路  
  位置：`main/agent/agent_loop.c:105`、`main/agent/agent_loop.c:150`、`main/agent/agent_loop.c:65`

这说明“校时触发”是 **system prompt -> LLM 决策 tool_use -> 工具执行** 的软触发链路，而不是系统守护线程定时同步。

### 2.5 启动流程未见自动校时

- `app_main` 负责 NVS/SPIFFS/WiFi/Telegram/Agent 启动，但没有 `sntp` 或 `settimeofday` 初始化逻辑  
  位置：`main/mimi.c:83` 起
- WiFi 事件回调中也没有“连网后自动校时”  
  位置：`main/wifi/wifi_manager.c:36` 起

---

## 3. 运行时行为（无 RTC、无断电保持场景）

### 3.1 冷启动/断电重启后

设备上电后，系统时间先处于“未校时”状态（通常会表现为早期 epoch 时间）。  
当前工程没有“从 Flash/NVS 恢复上次已知时间”的逻辑。

### 3.2 当模型调用 `get_current_time`

1. 发起网络请求到 `api.telegram.org`
2. 读取 `Date` 头（GMT）
3. 解析为 `struct tm`
4. 转 epoch 并 `settimeofday`
5. 返回本地时区格式化时间字符串

### 3.3 未触发该工具时的影响

依赖 `time(NULL)` 的业务会使用未校时的时间：

- 会话时间戳 `ts`：`main/memory/session_mgr.c:39`
- 每日记忆文件名日期：`main/memory/memory_store.c:12`

因此可能出现会话时间戳异常、daily 文件日期不准等问题。

---

## 4. “是不是通过网络同步时间？”的精确回答

是，但当前实现是：

- **通过 HTTPS 响应头 Date 做按需校时**
- **不是 SNTP/NTP 后台自动同步**
- **不是开机即自动校时**

---

## 5. 当前实现的风险与边界

### 5.1 无周期性重同步

校时成功后依赖本地计时继续走时；长时间运行可能有漂移，且不会自动矫正。

### 5.2 首次校时依赖模型是否触发工具

如果 Agent 当次未调用 `get_current_time`，系统可能一直保持未校时状态。

### 5.3 网络不可用时无法校时

在离线/代理不可用/Telegram 域名不可达时，工具会失败。

### 5.4 代码层面一个实现风险（直连分支）

`fetch_time_direct()` 中先拿 `Date` 头指针，再 `esp_http_client_cleanup(client)`，之后继续使用该指针。  
这存在生命周期风险（悬挂指针可能性），建议在 cleanup 前拷贝字符串。

位置：`main/tools/tool_get_time.c:133`、`main/tools/tool_get_time.c:134`、`main/tools/tool_get_time.c:138`

---

## 6. 与 `tuyaopen/apps/mimiclaw` 版本的差异（避免混淆）

`tuyaopen/apps/mimiclaw/tools/tool_get_time.c` 当前只做：

- `time(NULL)` + `localtime_r()` + `strftime()`
- **不拉取网络时间，不调用 settimeofday**

位置：`tuyaopen/apps/mimiclaw/tools/tool_get_time.c:13`、`:17`

所以你要分析“网络校时”应以 `mimiclaw/main/tools/tool_get_time.c` 为准。

---

## 7. 建议的改进方案（按优先级）

### P0（建议立即）

1. 修复 `fetch_time_direct()` 的 `Date` 指针生命周期问题（cleanup 前复制到本地缓冲区）。
2. 在 WiFi 成功连网后主动触发一次时间同步（不依赖 Agent 是否调用工具）。

### P1（稳定性）

1. 增加周期性重同步（例如每 6~24 小时）。
2. 增加多时间源回退（如多个 HTTPS 域名，或可选 SNTP）。
3. 增加“时间有效性门限”（例如小于某年视为未校时，阻止写入关键时间戳）。

### P2（断电体验）

1. 持久化 `last_known_epoch + monotonic_reference` 到 NVS。
2. 重启后先恢复近似时间，再等待网络校准覆盖。

---

## 8. 一句话总结

MimiClaw 在“无 RTC、无断电保持”条件下，当前采用的是**按需 HTTP 校时**，而非自动 NTP 同步；只要 `get_current_time` 未被调用，就可能长期处于未校时状态。

---

## 9. TuyaOpen 版补齐实现（已完成）

> 补充说明：本节描述的是 `tuyaopen/apps/mimiclaw` 当前已实现状态，用于对齐主工程时间能力。

### 9.1 已补齐的核心能力

`tuyaopen/apps/mimiclaw` 的 `get_current_time` 已从“仅读本地 `time()`”升级为“网络取时 + 写入系统时钟（Tuya 时间服务）”：

1. 直连路径：
   - 使用 `http_client_interface` 对 `MIMI_TG_API_HOST:443` 发 `HEAD /`
   - 从 `http_client_response_t.headers` 解析 `Date` 头
2. 代理路径：
   - 复用 `http_proxy` CONNECT+TLS 隧道
   - 从原始响应头解析 `Date` 头
3. 设时路径：
   - 将 `Date` 解析为 UTC epoch 后，调用 `tal_time_set_posix(epoch, 2)` 写入 TuyaOpen 时间服务

关键位置：
- `tuyaopen/apps/mimiclaw/tools/tool_get_time.c:271`（直连取 `Date`）
- `tuyaopen/apps/mimiclaw/tools/tool_get_time.c:322`（代理取 `Date`）
- `tuyaopen/apps/mimiclaw/tools/tool_get_time.c:397`（`tal_time_set_posix`）

### 9.2 TuyaOpen 接口映射（替代 ESP-IDF 方案）

对齐时做了接口层替换，避免继续依赖 ESP-IDF 风格 API：

- 时间设置：`settimeofday()` -> `tal_time_set_posix(...)`
- 时间读取：`time(NULL)` -> `tal_time_get_posix()`
- 本地时间转换：`localtime_r/strftime` -> `tal_time_get_local_time_custom(...)` + 手动格式化
- 时区初始化：基于 `MIMI_TIMEZONE` 解析基础偏移，调用 `tal_time_set_time_zone_seconds(...)`

关键位置：
- `tuyaopen/apps/mimiclaw/tools/tool_get_time.c:79`（时区初始化）
- `tuyaopen/apps/mimiclaw/tools/tool_get_time.c:223`（本地时间格式化）

### 9.3 时间一致性链路已一并对齐

不仅工具本身对齐，时间消费侧也已同步切换到 Tuya 时间服务：

1. 会话时间戳：
   - `session_mgr.c` 中 `ts` 改为 `tal_time_get_posix()`
   - 位置：`tuyaopen/apps/mimiclaw/memory/session_mgr.c:42`
2. 每日记忆文件名日期：
   - `memory_store.c` 改为 `tal_time_get_posix()` + `tal_time_get_local_time_custom(...)`
   - 位置：`tuyaopen/apps/mimiclaw/memory/memory_store.c:14`、`:23`

这样可以避免“工具已校时但会话/日记仍按旧接口取时”导致的不一致问题。

### 9.4 系统提示词触发链路（TuyaOpen）

TuyaOpen 版同样通过 system prompt 引导模型调用 `get_current_time`，并在 Agent 工具循环中执行：

- system prompt 中明确写有“没有内部时钟，需使用 get_current_time”  
  位置：`tuyaopen/apps/mimiclaw/agent/context_builder.c:53`、`:68`
- Agent 每轮构建 system prompt，LLM 返回 `tool_use` 后通过 `tool_registry_execute(...)` 真正执行校时工具  
  位置：`tuyaopen/apps/mimiclaw/agent/agent_loop.c:127`、`:171`、`:75`

因此，TuyaOpen 版的“提示词触发校时”机制也已补齐。

### 9.5 当前边界（仍与主工程一致）

虽然 TuyaOpen 版已补齐网络校时能力，但仍与主工程一样是“按需校时”：

- 不是 SNTP/NTP 常驻后台自动同步
- 不是开机自动强制校时
- 断电后依旧需要再次网络取时恢复准确时间

换言之，TuyaOpen 版现在已经补齐到“功能等价于主工程的 `get_current_time` 路线（并使用 TuyaOpen 接口）”。
