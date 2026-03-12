# Tuya Cloud Service Host Tests Phase 2 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 扩展 `src/tuya_cloud_service/test/host/`，完成 Phase 2 的 `tuya_health` 和 `tuya_iot_dp` Host 单测，并保持全量 Host 基线通过。

**Architecture:** 继续复用已建立的 `src/tuya_cloud_service/test/host/` 入口，以最小 fake/stub 驱动 `tuya_health.c` 和 `tuya_iot_dp.c` 的公开 API。优先覆盖空参数、未初始化状态、work queue 调度、事件分发和错误传播，不引入新的测试框架。

**Tech Stack:** C, CMake, CTest, Unity, CMock, existing Host test support

---

## 执行状态（2026-03-11）

- [x] Task 1: 建立 `tuya_health` Host 单元测试
- [x] Task 2: 建立 `tuya_iot_dp` Host 单元测试
- [x] Task 3: 文档收口并同步 Phase 2 完成状态

### 当前进度备注

- 已建立 `test_tuya_cloud_service_health_host`
- 已建立 `test_tuya_cloud_service_iot_dp_host`
- 当前全量 Host 基线为 `10/10` suites 通过

---

### Task 1: 建立 `tuya_health` Host 单元测试

**Files:**
- Modify: `src/tuya_cloud_service/test/host/CMakeLists.txt`
- Create: `src/tuya_cloud_service/test/host/test_tuya_health.c`

**Step 1: 先写失败测试**

优先覆盖：

- `tuya_health_item_add` 在 monitor 未初始化时返回 `OPRT_INVALID_PARM`
- `tuya_health_item_del` 在 monitor 未初始化时不崩溃
- `tuya_health_update_item_period` / `tuya_health_update_item_threshold` 在 monitor 未初始化时不崩溃
- `tuya_health_disable_watchdog` 在 monitor 未初始化时不崩溃
- `tuya_health_monitor_init` 在关键依赖失败时返回错误码

**Step 2: 跑测试验证红灯**

Run:

```bash
cmake -S tests/host -B build/tests/host
cmake --build build/tests/host -j
ctest --test-dir build/tests/host -R test_tuya_cloud_service_health_host --output-on-failure
```

Expected: FAIL

**Step 3: 接入最小构建和依赖 stub**

Provide only what `tuya_health.c` needs:

- `tal_mutex_*`
- `tal_event_subscribe/unsubscribe/publish`
- `tal_thread_create_and_start/delete`
- `tal_workq_schedule`
- `tal_system_*`
- `tal_sw_timer_get_num`

Prefer local test fakes over broad production changes.

**Step 4: 补齐断言并转绿**

Run:

```bash
ctest --test-dir build/tests/host -R test_tuya_cloud_service_health_host --output-on-failure
```

Expected: PASS

### Task 2: 建立 `tuya_iot_dp` Host 单元测试

**Files:**
- Modify: `src/tuya_cloud_service/test/host/CMakeLists.txt`
- Create: `src/tuya_cloud_service/test/host/test_tuya_iot_dp.c`

**Step 1: 先写失败测试**

优先覆盖：

- `tuya_iot_dp_parse` 在 `cmd_js == NULL` 时返回 `OPRT_CJSON_GET_ERR`
- `tuya_iot_dp_sync_process` 在未连接时重新调度 delayed work
- `tuya_iot_dp_sync_start` 首次初始化 delayed work，之后仅重复 start
- `tuya_iot_dp_obj_dump` 在 client 未激活时返回 `NULL`
- `tuya_iot_dp_raw_report` 对 `client == NULL` / `dp == NULL` 返回 `OPRT_INVALID_PARM`

**Step 2: 跑测试验证红灯**

Run:

```bash
ctest --test-dir build/tests/host -R test_tuya_cloud_service_iot_dp_host --output-on-failure
```

Expected: FAIL

**Step 3: 接入最小 fake/stub**

Provide only what this file needs:

- `tuya_iot_client_get`
- `tuya_iot_is_connected`
- `tal_workq_*`
- `dp_*` report/parse helpers
- `tuya_lan_*`
- optional BLE hooks as disabled path only

**Step 4: 补齐断言并转绿**

Run:

```bash
ctest --test-dir build/tests/host -R test_tuya_cloud_service_iot_dp_host --output-on-failure
```

Expected: PASS

### Task 3: 文档收口并同步 Phase 2 完成状态

**Files:**
- Modify: `docs/testing/unit-test-guide.md`
- Modify: `tuyaopen_unit_test_scheme.md`
- Modify: `docs/plans/2026-03-11-tuya-cloud-service-host-tests-implementation.md`
- Modify: `docs/plans/2026-03-11-tuya-cloud-service-host-tests-design.md`

**Step 1: 更新 Host suites 列表**

Add:

- `test_tuya_cloud_service_health_host`
- `test_tuya_cloud_service_iot_dp_host`

**Step 2: 更新当前基线**

Record the new full Host suite count after verification.

**Step 3: 验证文档 diff**

Run:

```bash
git diff --check -- docs/testing/unit-test-guide.md \
  tuyaopen_unit_test_scheme.md \
  docs/plans/2026-03-11-tuya-cloud-service-host-tests-implementation.md \
  docs/plans/2026-03-11-tuya-cloud-service-host-tests-design.md \
  docs/plans/2026-03-11-tuya-cloud-service-host-tests-phase2-implementation.md
```

Expected: no output
