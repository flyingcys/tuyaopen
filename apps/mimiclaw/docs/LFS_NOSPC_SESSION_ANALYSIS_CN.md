# MimicLaw LFS 空间耗尽（`No more free space 43`）分析与处理建议

## 1. 现象与问题

日志片段：

```text
[02-24 18:33:43 ty E][lfs.c:672] No more free space 43
[02-24 18:33:43 ty E][lfs.c:672] No more free space 43
[02-24 18:33:43 ty I][agent_loop.c:335] [agent] free heap=536841664
```

核心问题：`littlefs` 返回无可用块（`LFS_ERR_NOSPC`），不是内存（heap）不足。

## 2. 结论摘要

1. `lfs.c:672` 是 littlefs 分配块失败分支，明确代表文件系统空间耗尽。  
2. `agent_loop` 每轮会话固定写两次 session（`user` 和 `assistant`），且是追加写。  
3. session 文件读取时虽然只取最近 `N` 条消息给 LLM，但文件本体不会自动裁剪。  
4. 现有机制依赖手动清理（`session_clear <chat_id>`），长期运行后高概率再次打满 LFS。  

## 3. 证据链（代码定位）

### 3.1 littlefs 报错含义

- 文件：`tuyaopen/src/tal_kv/littlefs/lfs.c`
- 位置：`lfs_alloc()` 中
- 关键逻辑：当已扫描完可分配块仍无空闲块时打印
  `No more free space` 并返回 `LFS_ERR_NOSPC`。

对应定义：

- 文件：`tuyaopen/src/tal_kv/littlefs/lfs.h`
- `LFS_ERR_NOSPC = -28`（No space left on device）

### 3.2 会话写入路径（持续增长源）

- 文件：`tuyaopen/apps/mimiclaw/agent/agent_loop.c`
- 每轮最终回复后执行：
  - `session_append(chat_id, "user", user_text)`
  - `session_append(chat_id, "assistant", final_text)`

- 文件：`tuyaopen/apps/mimiclaw/memory/session_mgr.c`
  - `session_append()` 使用 `tal_fopen(path, "a")` 追加 JSONL
  - 每条写入 `{role, content, ts}` + `\n`

### 3.3 读取与“保留条数”仅作用于上下文，不作用于文件大小

- 同文件 `session_get_history_json()`
  - 仅在内存 ring buffer 中保留最近 `max_msgs`
  - `max_msgs` 上限来自 `MIMI_SESSION_MAX_MSGS`（当前为 20）
  - 不会回写/重写 session 文件以裁剪历史

### 3.4 当前清理能力

- 文件：`tuyaopen/apps/mimiclaw/cli/serial_cli.c`
  - `session_list`
  - `session_clear <chat_id>`
- 结论：有人工清理入口，但无自动回收策略。

## 4. `mimiclaw/main` 对照结果

`mimiclaw/main/memory/session_mgr.c` 与 `tuyaopen/apps/mimiclaw/memory/session_mgr.c` 在会话管理模式上是同一思路：

1. 每 chat 一个 `tg_<chat_id>.jsonl`
2. 追加写 session
3. 读取只取最近 `N` 条
4. 无自动裁剪/TTL/按容量回收

因此该问题不是单点偶发，更像当前会话存储策略的系统性结果。

## 5. `/spiffs` 落盘文件清单（mimiclaw）

### 5.1 高风险增长源（最可能导致 LFS 满）

1. 会话文件：`/spiffs/sessions/tg_<chat_id>.jsonl`
   - 代码：`session_append()` 以 `"a"` 追加写
   - 位置：`tuyaopen/apps/mimiclaw/memory/session_mgr.c`
2. 写入触发频率
   - 代码：每轮对话至少写两次（user + assistant）
   - 位置：`tuyaopen/apps/mimiclaw/agent/agent_loop.c`

结论：这是当前最主要的持续膨胀来源。

### 5.2 中低风险文件（有上限或非持续追加）

1. `/spiffs/cron.json`
   - 代码：`"w"` 覆盖写，不是无上限追加
   - 位置：`tuyaopen/apps/mimiclaw/cron/cron_service.c`
   - 限制：`CRON_FILE_MAX_BYTES = 8192`
2. `/spiffs/memory/MEMORY.md`
   - 代码：`"w"` 覆盖写
   - 位置：`tuyaopen/apps/mimiclaw/memory/memory_store.c`
3. `/spiffs/memory/<YYYY-MM-DD>.md`
   - 代码：`"a"` 追加写（增长速度通常低于会话）
   - 位置：`tuyaopen/apps/mimiclaw/memory/memory_store.c`
4. `/spiffs/skills/*.md`
   - 代码：内建技能仅在不存在时写入；用户也可通过 `write_file` 创建/覆盖
   - 位置：`tuyaopen/apps/mimiclaw/skills/skill_loader.c`、`tools/tool_files.c`
5. `/spiffs/config/SOUL.md`、`/spiffs/config/USER.md`、`/spiffs/HEARTBEAT.md`
   - 在当前代码中主要作为读取输入，不是高频增长点

## 6. “是不是 NVS 导致”专项结论

### 6.1 关键事实

1. `mimi_kv_set_string/get/del` 最终调用 `tal_kv_set/get/del`
   - 位置：`tuyaopen/apps/mimiclaw/mimi_base.h`
2. `tal_kv_set` 内部使用 littlefs 文件写（`LFS_O_CREAT | LFS_O_TRUNC`）
   - 位置：`tuyaopen/src/tal_kv/src/tal_kv.c`
3. `tal_fs_*` 文件 API 也使用 `tal_lfs_get()` 返回的同一个 `lfs` 实例
   - 位置：`tuyaopen/src/tal_system/src/tal_fs.c`
   - 位置：`tuyaopen/src/tal_kv/src/tal_kv.c` (`tal_lfs_get`)

### 6.2 解释

1. 在这个工程里，业务口径叫“NVS”的配置存储并非传统 ESP-IDF 独立 NVS 分区语义。
2. 它与 `/spiffs` 文件写入在实现上共享同一个 littlefs 存储池。
3. 因此从底层看都在消耗 LFS；区别在于写入模式：
   - KV 配置多为小值、覆盖写
   - session 为高频、无上限追加写

### 6.3 判定

更可能是 **session 文件持续增长** 导致 `No more free space`，而不是配置类 KV（NVS 键值）本身导致。

## 7. 如何快速验证“谁是主因”

1. 先执行 `session_clear_all`（或针对大 chat 执行 `session_clear`）
   - 命令入口：`tuyaopen/apps/mimiclaw/cli/serial_cli.c`
2. 继续跑同样消息流量，观察 `lfs.c:672` 是否消失/明显后延。
3. 如果症状显著缓解，可基本坐实 session 是主因。

## 8. 影响面评估

当 LFS 接近或达到满载时：

1. session 持久化会失败（会话上下文逐步退化）。
2. 其他依赖 `/spiffs` 的写操作也可能失败（memory、skills、cron 等）。
3. 由于 KV 与文件共享同一 LFS，配置写入也会受到牵连。
4. 运行看起来“还能回复”，但稳定性和可恢复性变差（隐性故障）。

## 9. 处理意见（分阶段）

### 9.1 立即止血（运维层）

1. 执行 `session_list` 查看历史会话文件。
2. 优先执行 `session_clear_all`；或按需执行 `session_clear <chat_id>`。
3. 清理后观察是否仍出现 `No more free space`。

适用场景：线上已告警、需要先恢复写入能力。

### 9.2 短期修复（代码层，建议优先）

在 `session_append()` 增加“写前裁剪”机制（推荐）：

1. 设定单会话文件上限（例如 64KB 或 128KB）。
2. 超限时仅保留最近 `N` 条（例如 40~100 条）重写文件，再追加新消息。
3. 写失败且判定为 NOSPC 时，触发一次“紧急裁剪后重试 1 次”。

优点：改动小、见效快、与当前 JSONL 结构兼容。

### 9.3 中期治理（策略层）

1. 增加全局 session 目录配额（总大小上限）。
2. 引入 LRU 清理（先删最久未活跃会话文件）。
3. 增加周期性维护任务（定时扫描与回收）。

### 9.4 长期优化（架构层）

1. 将“可再生历史数据”与“关键配置/技能”隔离存储（不同分区/不同介质）。
2. 对会话采用摘要化存储（长期保存 summary，原文短保留）。
3. 建立空间水位监控日志（如 >80% warn，>90% trigger cleanup）。

## 10. 建议的验收标准

1. 连续多轮对话后 session 文件大小不再无限增长。
2. 在高频对话场景下不再复现 `lfs.c:672 No more free space`。
3. 清理与裁剪策略触发时有明确日志（包含 chat_id、回收字节数）。
4. 功能回归通过：历史上下文质量不明显下降，普通消息链路正常。

## 11. 一句话建议

先用 `session_clear_all` 做止血，再把 `session_append` 改为“上限裁剪 + 失败重试”是当前成本最低、收益最高的处理路径。
