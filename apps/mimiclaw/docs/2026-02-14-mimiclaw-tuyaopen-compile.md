# Mimiclaw TuyaOpen Compile Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 让 `TuyaOpen/apps/mimiclaw` 在 TuyaOpen 构建体系下可编译，且不再依赖 ESP 接口与兼容层。

**Architecture:** 先改造构建入口为 TuyaOpen `EXAMPLE_LIB` 模式，再将各模块统一切换到 TuyaOpen 的 `OPERATE_RET`、`tal_*` 接口。不能直接迁移的能力保留函数签名并提供空实现，保证后续可手工补功能。

**Tech Stack:** C/CMake, TuyaOpen TAL (`tal_api`, `tal_thread`, `tal_queue`, `tal_kv`, `tal_wifi`)

---

### Task 1: 修复构建入口（红灯基线）

**Files:**
- Modify: `TuyaOpen/apps/mimiclaw/CMakeLists.txt`

**Step 1: 运行失败构建**

Run: `cd /home/share/samba/openclaw/TuyaOpen && source ./export.sh && cd apps/mimiclaw && tos.py build`
Expected: FAIL，报 `Unknown CMake command "idf_component_register"`

**Step 2: 改造 CMake 为 TuyaOpen 样式**

- 使用 `add_library(${EXAMPLE_LIB})`
- 使用 `target_sources/target_include_directories`
- 明确排除 `compat/` 源文件

**Step 3: 再次运行构建**

Expected: 不再出现 `idf_component_register` 错误，进入源码级编译报错

### Task 2: 接口基线切换（ESP/FREERTOS/NVS -> TuyaOpen）

**Files:**
- Modify: `TuyaOpen/apps/mimiclaw/**/*.h`
- Modify: `TuyaOpen/apps/mimiclaw/**/*.c`

**Step 1: 统一返回值与日志**

- `esp_err_t` -> `OPERATE_RET`
- `ESP_OK/ESP_FAIL/...` -> `OPRT_*`
- `ESP_LOG*` -> `PR_*`

**Step 2: 统一线程/队列/延时接口**

- `xTaskCreate*` -> `tal_thread_create_and_start`
- `vTaskDelay/pdMS_TO_TICKS` -> `tal_system_sleep`
- `xQueue*` -> `tal_queue_*`

**Step 3: 统一 KV 与系统信息**

- `nvs_*` -> `tal_kv_*`
- `esp_get_free_heap_size/heap_caps_get_free_size` -> `tal_system_get_free_heap_size`

### Task 3: 空实现兜底不可迁移能力

**Files:**
- Modify: `TuyaOpen/apps/mimiclaw/ota/ota_manager.c`
- Modify: `TuyaOpen/apps/mimiclaw/tools/tool_get_time.c`
- Modify: `TuyaOpen/apps/mimiclaw/tools/tool_web_search.c`
- Modify: `TuyaOpen/apps/mimiclaw/llm/llm_proxy.c`
- Modify: `TuyaOpen/apps/mimiclaw/telegram/telegram_bot.c`

**Step 1: 保留函数签名与调用点**

- 对当前无法直接映射的 HTTP/OTA 细节给空实现
- 返回 `OPRT_NOT_SUPPORTED` 或 `OPRT_COM_ERROR`

**Step 2: 保持可编译和可链接**

- 确保所有对外声明函数都有定义
- 不引入 `compat/*` 或 `esp_*` 头

### Task 4: 迭代编译收敛

**Files:**
- Modify: 由编译报错驱动

**Step 1: 执行构建并记录首个错误**

Run: `tos.py build`
Expected: FAIL（新的编译错误）

**Step 2: 最小改动修复后重编译**

- 每轮只修一类错误，避免引入额外回归

**Step 3: 直到构建通过**

Expected: `tos.py build` 退出码 `0`

### Task 5: 结果核对与交付

**Files:**
- Modify: `TuyaOpen/apps/mimiclaw/PORTING_CHECKLIST.md`（可选）

**Step 1: 运行最终验证命令**

Run: `cd /home/share/samba/openclaw/TuyaOpen && source ./export.sh && cd apps/mimiclaw && tos.py build`
Expected: PASS

**Step 2: 输出遗留空实现列表**

- 标明文件、函数名、当前返回值

**Step 3: 输出变更摘要**

- 构建系统改造
- 接口替换范围
- 后续人工补实现建议
