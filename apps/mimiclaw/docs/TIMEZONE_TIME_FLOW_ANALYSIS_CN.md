# MimiClaw 时间与时区处理详解（含 UTC-8 现象与东八区配置）

## 1. 你遇到的问题（现象）

- HTTP 取到的时间看起来是标准时间（UTC/GMT）。
- 但在 Telegram/Feishu 对话里，助手会说当前在 `UTC-8`。
- 你当前配置是：

```c
#define MIMI_TIMEZONE "PST8PDT,M3.2.0,M11.1.0"
```

这份文档解释该现象的代码链路，并给出改成东八区（UTC+8）的正确写法。

---

## 2. 结论先行

1. `get_current_time` 的时间源确实来自 HTTP 响应头 `Date`（标准 UTC/GMT 时间）。
2. 工具会把 UTC 时间写入系统时钟，然后按本地时区格式化输出。
3. 你现在看到的 `UTC-8` 来自全局时区配置 `MIMI_TIMEZONE`，不是 Telegram 或 Feishu 单独处理出来的。
4. Telegram 和 Feishu 共享同一个 Agent + 同一个 `get_current_time` 工具，因此时区表现一致。
5. 如果要改东八区，推荐把 `MIMI_TIMEZONE` 改为 `CST-8`（固定 UTC+8，无夏令时）。

---

## 3. 端到端时间处理链路

## 3.1 通道层只负责收发，不做时区换算

- Telegram 收到消息后只把文本投递到统一消息总线：
  - `tuyaopen/apps/mimiclaw/channels/telegram_bot.c`（`process_updates` -> `message_bus_push_inbound`）
- Feishu 收到消息后同样只投递到统一消息总线：
  - `tuyaopen/apps/mimiclaw/channels/feishu_bot.c`（`handle_event_payload` -> `publish_inbound_feishu` -> `message_bus_push_inbound`）

结论：两个通道都没有“按平台单独转换时区”的逻辑。

## 3.2 Agent 统一调用工具

- `agent_loop` 在模型发出 `tool_use` 后，统一执行工具：
  - `tuyaopen/apps/mimiclaw/agent/agent_loop.c`（`tool_registry_execute(...)`）
- `get_current_time` 工具注册位置：
  - `tuyaopen/apps/mimiclaw/tools/tool_registry.c`

结论：无论消息来自 Telegram 还是 Feishu，时间工具逻辑完全一致。

## 3.3 `get_current_time` 的实际行为

文件：`tuyaopen/apps/mimiclaw/tools/tool_get_time.c`

执行步骤：

1. 初始化时间服务并设置时区（只做一次）  
   - `tal_time_service_init()`  
   - `ensure_timezone_from_config()`（读取 `MIMI_TIMEZONE` 并设置时区秒偏移）
2. 从网络获取 `Date` 头（直连或代理）  
   - 直连：`fetch_date_direct`  
   - 代理：`fetch_date_via_proxy`
3. 解析 `Date` 为 UTC epoch  
   - `parse_http_date_to_epoch`
4. 写入系统时间  
   - `tal_time_set_posix(epoch, 2)`
5. 按本地时区格式化输出  
   - `tal_time_get_local_time_custom(...)`  
   - 输出如：`YYYY-MM-DD HH:MM:SS UTC±HH:MM (Weekday)`

---

## 4. 为什么会显示 UTC-8（你的当前配置）

关键配置在：

- `tuyaopen/apps/mimiclaw/mimi_config.h`

当前为：

```c
#define MIMI_TIMEZONE "PST8PDT,M3.2.0,M11.1.0"
```

`tool_get_time.c` 中的解析函数会取这个 TZ 字符串的“基础偏移”并设置：

- `parse_posix_tz_base_offset(...)`
- 最终执行 `tal_time_set_time_zone_seconds(tz_sec)`

因此最终格式化出的标签就会是 `UTC-08:00`（即你看到的 `UTC-8`）。

注意：在当前 `mimiclaw` 代码里，只设置了基础秒偏移，并没有看到为该 TZ 字符串配套配置 summer time 表的调用，所以运行上通常表现为固定偏移。

---

## 5. `MIMI_TIMEZONE` 的“符号方向”要特别注意

当前实现遵循 POSIX TZ 习惯：`STD<offset>` 里的 offset 与“常见 UTC±”直觉是反向的。

在 `parse_posix_tz_base_offset` 中可简化理解为：

```text
utc_offset_sec = - posix_offset_sec
```

这会导致以下结果：

| 配置字符串 | 结果时区偏移 |
|---|---|
| `PST8PDT,...` | `UTC-08:00` |
| `CST-8` | `UTC+08:00` |
| `UTC-8` | `UTC+08:00` |
| `UTC+8` | `UTC-08:00`（很多人会配错） |
| `CST8` | `UTC-08:00`（很多人会配错） |

---

## 5.1 `PST8PDT,M3.2.0,M11.1.0` 三段具体怎么理解

这个字符串按逗号分成 3 段：

1. `PST8PDT`
- `PST`：标准时名称（Pacific Standard Time）
- `8`：POSIX TZ 写法里的标准时偏移，表示标准时是 `UTC-8`
- `PDT`：夏令时名称（Pacific Daylight Time，通常对应 `UTC-7`）

2. `M3.2.0`
- `M3`：3 月
- `.2`：第 2 个
- `.0`：星期日（0=周日，1=周一，...，6=周六）
- 含义：夏令时从每年 3 月第 2 个星期日开始（默认切换时刻通常是 02:00）

3. `M11.1.0`
- `M11`：11 月
- `.1`：第 1 个
- `.0`：星期日
- 含义：夏令时在每年 11 月第 1 个星期日结束（默认通常 02:00）

整体语义：
- 非夏令时时段：`PST (UTC-8)`
- 夏令时时段（3 月第 2 个周日 到 11 月第 1 个周日）：`PDT (UTC-7)`

注意：以上是该 TZ 字符串的标准语义；当前 `mimiclaw` 代码主要使用了基础偏移解析，未完整接入这一套 DST 规则计算流程。

---

## 6. 改成东八区（UTC+8）怎么改

## 6.1 推荐配置（中国大陆常用，无夏令时）

把 `mimi_config.h` 改成：

```c
#define MIMI_TIMEZONE "CST-8"
```

这会让工具输出中的时区标签变成 `UTC+08:00`。

## 6.2 可选配置

也可以写：

```c
#define MIMI_TIMEZONE "UTC-8"
```

结果同样是 `UTC+08:00`，但语义上不如 `CST-8` 直观。

---

## 6.3 代码级确认：`CST-8` 在当前实现中可以正常生效

针对你关心的 `tuyaopen/apps/mimiclaw/tools/tool_get_time.c`，当前代码链路如下：

1. 时区字符串解析入口  
- `parse_posix_tz_base_offset(...)` 会读取 `MIMI_TIMEZONE`（`tool_get_time.c`）。

2. 关键换算公式  
- 代码是：
  - `posix_offset_sec = sign * (hours * 3600 + mins * 60)`
  - `utc_offset_sec = -posix_offset_sec`
- 所以 `CST-8` 会得到：
  - `posix_offset_sec = -8 * 3600`
  - `utc_offset_sec = +8 * 3600 = 28800`

3. 设置到系统时区  
- 解析结果会传给 `tal_time_set_time_zone_seconds(tz_sec)`，写入 `s_time_tz`（Tuya 时间服务）。

4. 本地时间与标签输出  
- `tal_time_get_local_time_custom(...)` 使用 `in_time + s_time_tz` 做本地时间换算。  
- `format_utc_offset(...)` 会把 `+28800` 输出为 `UTC+08:00`。

结论：在当前实现下，把 `MIMI_TIMEZONE` 配成 `CST-8`，会稳定得到东八区输出（`UTC+08:00`）。

---

## 7. 修改后的验证步骤

1. 修改配置并重新编译/烧录运行。
2. 在 Telegram 或 Feishu 里让助手执行“当前时间”相关请求（触发 `get_current_time`）。
3. 观察工具输出或日志，应出现类似：
   - `... UTC+08:00 (...)`
4. 再分别从 Telegram、Feishu 触发一次，结果应一致（因为共享同一时间链路）。

---

## 8. 你这个场景的直接回答

- “HTTP 里拿到的是标准时间，为什么 Telegram/Feishu 告诉我 UTC-8？”  
  - 因为 HTTP 取到的是 UTC 源时间；最终回复前会按 `MIMI_TIMEZONE` 做本地化格式化。你当前配置是西八区基准，所以显示 `UTC-8`。
- “Feishu 里也是 UTC-8，怎么处理的？”  
  - Feishu 与 Telegram 走的是同一套 Agent + 时间工具，不存在通道级差异。

---

## 9. 相关代码索引（便于二次学习）

- 时间工具：
  - `tuyaopen/apps/mimiclaw/tools/tool_get_time.c`
- 工具注册：
  - `tuyaopen/apps/mimiclaw/tools/tool_registry.c`
- Agent 工具执行入口：
  - `tuyaopen/apps/mimiclaw/agent/agent_loop.c`
- Telegram 入站处理：
  - `tuyaopen/apps/mimiclaw/channels/telegram_bot.c`
- Feishu 入站处理：
  - `tuyaopen/apps/mimiclaw/channels/feishu_bot.c`
- 时区配置宏：
  - `tuyaopen/apps/mimiclaw/mimi_config.h`
- Tuya 时间服务（时区/本地时间实现）：
  - `tuyaopen/src/tal_system/src/tal_time_serivce.c`
