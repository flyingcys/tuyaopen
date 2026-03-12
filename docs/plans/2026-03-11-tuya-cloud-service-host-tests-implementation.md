# Tuya Cloud Service Host Tests Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 为 `src/tuya_cloud_service` 建立分阶段可扩展的 Host 单元测试入口，并先完成 Phase 1 的 `tuya_endpoint`、`tuya_transport`、`tuya_protocol` 三个模块测试。

**Architecture:** 沿用现有 `src/<component>/test/host/` 结构，在 `src/tuya_cloud_service/test/host/` 下集中维护该组件的 Host 单测。首批通过 CMock + stub 隔离 KV、时间、随机数、transport factory 和加解密 wrapper，优先验证参数校验、错误传播和资源管理逻辑。

**Tech Stack:** C, CMake, CTest, Unity, CMock, TuyaOpen Host test helpers

---

## 执行状态（2026-03-11）

- [x] Task 1: 建立 `tuya_cloud_service` Host 测试目录和构建入口
- [x] Task 2: 建立 `tuya_endpoint` Host 单元测试
- [x] Task 3: 建立 `tuya_transport` Host 单元测试
- [x] Task 4: 建立 `tuya_protocol` Host 单元测试
- [x] Task 5: 文档收口并记录 Phase 2-5 待办

### 当前进度备注

- `Task 2` 已建立 `test_tuya_cloud_service_endpoint_host`，覆盖参数校验、KV 错误传播、默认环境回填与证书释放路径。
- `Task 3` 已建立 `test_tuya_cloud_service_transport_host`，覆盖 array 边界、callback forwarding 和空指针保护。
- `Task 4` 已建立 `test_tuya_cloud_service_protocol_host`，覆盖关键参数校验、invalid command 路由和 LPV35 边界 helper。
- 当前全量 Host 基线为 `10/10` suites 通过。

---

### Task 1: 建立 `tuya_cloud_service` Host 测试目录和构建入口

**Files:**
- Modify: `tests/host/CMakeLists.txt`
- Create: `src/tuya_cloud_service/test/host/CMakeLists.txt`
- Create: `src/tuya_cloud_service/test/host/support/README.md`

**Step 1: 先创建测试入口目录**

Run:

```bash
mkdir -p src/tuya_cloud_service/test/host/support
```

Expected: `src/tuya_cloud_service/test/host/` 目录存在。

**Step 2: 在 `tests/host/CMakeLists.txt` 注册组件测试目录**

Add:

```cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/test/host tuya_cloud_service_host)
```

**Step 3: 写组件测试入口 `CMakeLists.txt`**

Start with an empty-but-buildable skeleton:

```cmake
set(TUYA_CLOUD_SERVICE_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})

add_library(tuya_cloud_service_test_support INTERFACE)

target_include_directories(tuya_cloud_service_test_support
    INTERFACE
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/cloud
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/transport
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/protocol
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/schema
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/tls
        ${CMAKE_SOURCE_DIR}/../../src/common/base/include
        ${CMAKE_SOURCE_DIR}/../../src/common/cJSON
)
```

**Step 4: 跑配置验证新入口可被发现**

Run:

```bash
cmake -S tests/host -B build/tests/host
```

Expected: 配置成功，新的 `tuya_cloud_service_host` 子目录被包含。

**Step 5: Commit**

```bash
git add tests/host/CMakeLists.txt src/tuya_cloud_service/test/host
git commit -m "build: add host test entry for tuya_cloud_service"
```

### Task 2: 建立 `tuya_endpoint` Host 单元测试

**Files:**
- Modify: `src/tuya_cloud_service/test/host/CMakeLists.txt`
- Create: `src/tuya_cloud_service/test/host/test_tuya_endpoint.c`
- Create: `src/tuya_cloud_service/test/host/support/tuya_cloud_service_test_hooks.h`

**Step 1: 先写失败测试，覆盖空参数和 KV 读写错误**

Add tests like:

```c
void test_tuya_endpoint_cert_get_rejects_null(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_endpoint_cert_get(NULL));
}

void test_tuya_endpoint_region_regist_set_maps_kv_write_failure(void)
{
    tal_kv_set_ExpectAndReturn("region", NULL, 2, OPRT_COM_ERROR);
    tal_kv_set_IgnoreArg_value();
    TEST_ASSERT_EQUAL(OPRT_KVS_WR_FAIL, tuya_endpoint_region_regist_set("cn", "pro"));
}
```

**Step 2: 让测试先失败**

Run:

```bash
cmake -S tests/host -B build/tests/host
cmake --build build/tests/host -j
ctest --test-dir build/tests/host -R test_tuya_cloud_service_endpoint_host --output-on-failure
```

Expected: 初次因测试文件未完全接线、mock 缺失或当前实现行为不符而失败。

**Step 3: 为 `tuya_endpoint.c` 接入最小 mocks 和构建文件**

Generate or provide mocks for:

- `tal_kv.h`
- `tal_api.h` 中实际依赖的 KV/free 接口
- `iotdns_cloud_endpoint_get`

Add a dedicated target:

```cmake
tuya_add_host_test(test_tuya_cloud_service_endpoint_host
    test_tuya_endpoint.c
    ../../cloud/tuya_endpoint.c
    ...
)
```

**Step 4: 补齐第一批断言**

Cover:

- `tuya_endpoint_cert_get/set` 参数和错误传播
- `tuya_endpoint_domain_get/set` 参数和 serialize 错误传播
- `tuya_endpoint_region_regist_set` 成功写入后更新内存中的 region/regist_key
- `tuya_endpoint_init` 在 `regist_key` 为空时补默认 `"pro"`
- `tuya_endpoint_remove` 删除固定 key
- `tuya_endpoint_update` / `update_auto_region` 释放旧证书后调用 `iotdns_cloud_endpoint_get`

**Step 5: 跑目标测试转绿**

Run:

```bash
ctest --test-dir build/tests/host -R test_tuya_cloud_service_endpoint_host --output-on-failure
```

Expected: `PASS`

**Step 6: 跑全量 Host 基线**

Run:

```bash
bash tools/test/run_host_tests.sh
```

Expected: 现有 suites + `test_tuya_cloud_service_endpoint_host` 全部通过。

**Step 7: Commit**

```bash
git add src/tuya_cloud_service/test/host
git commit -m "test: add host unit tests for tuya_endpoint"
```

### Task 3: 建立 `tuya_transport` Host 单元测试

**Files:**
- Modify: `src/tuya_cloud_service/test/host/CMakeLists.txt`
- Create: `src/tuya_cloud_service/test/host/test_tuya_transport.c`

**Step 1: 先写失败测试，覆盖 wrapper 和 array 边界**

Add tests like:

```c
void test_transport_array_add_rejects_over_capacity(void)
{
    TEST_ASSERT_EQUAL(OPRT_INDEX_OUT_OF_BOUND,
                      tuya_transport_array_add_transporter(array, transporter3, "ws"));
}

void test_transporter_read_returns_invalid_param_without_callback(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transporter_read(transporter, buffer, sizeof(buffer), 1000));
}
```

**Step 2: 跑测试验证红灯**

Run:

```bash
ctest --test-dir build/tests/host -R test_tuya_cloud_service_transport_host --output-on-failure
```

Expected: FAIL

**Step 3: 接入构建和 fake transporter 支撑**

Add:

- fake `tuya_transporter_t` instances
- mocks for `tal_malloc/free`
- mock `mm_strdup`
- mocks for `tuya_tcp_transporter_create`
- mock `tuya_tls_transporter_create`
- optional websocket mock guarded by compile macro

**Step 4: 补齐断言**

Cover:

- array create/destroy
- add/get by scheme
- `MAX_TRANSPORTER_NUM` 上限
- `tuya_transporter_create` factory 分发
- connect/read/write/poll/close/ctrl wrapper 的成功转发
- callback 缺失时统一返回 `OPRT_INVALID_PARM`
- `tuya_transporter_destroy` 在 `f_destroy` 缺失时仍返回 `OPRT_OK`

**Step 5: 跑目标测试转绿**

Run:

```bash
ctest --test-dir build/tests/host -R test_tuya_cloud_service_transport_host --output-on-failure
```

Expected: `PASS`

**Step 6: 跑全量 Host 基线**

Run:

```bash
bash tools/test/run_host_tests.sh
```

Expected: 全量通过。

**Step 7: Commit**

```bash
git add src/tuya_cloud_service/test/host
git commit -m "test: add host unit tests for tuya_transport"
```

### Task 4: 建立 `tuya_protocol` Host 单元测试

**Files:**
- Modify: `src/tuya_cloud_service/test/host/CMakeLists.txt`
- Create: `src/tuya_cloud_service/test/host/test_tuya_protocol.c`

**Step 1: 先写失败测试，覆盖参数校验和路由**

Add tests like:

```c
void test_parse_protocol_data_rejects_short_input(void)
{
    char *out = NULL;
    uint8_t data[4] = {0};
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM,
                      tuya_parse_protocol_data(DP_CMD_LAN, data, sizeof(data), "1234567890123456", &out));
}

void test_pack_protocol_data_rejects_null_src(void)
{
    char *out = NULL;
    uint32_t out_len = 0;
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM,
                      tuya_pack_protocol_data(DP_CMD_LAN, NULL, 5, (uint8_t *)"1234567890123456", &out, &out_len));
}
```

**Step 2: 跑测试验证红灯**

Run:

```bash
ctest --test-dir build/tests/host -R test_tuya_cloud_service_protocol_host --output-on-failure
```

Expected: FAIL

**Step 3: 接入 mocks**

Generate or provide mocks for:

- `uni_random.h`
- `tal_api.h` 中 `tal_malloc/free/time`
- TLS/GCM wrapper declarations used by the file

**Step 4: 补齐第一批断言**

Cover:

- `tuya_pack_protocol_serial_no` 首次随机种子 + 递增
- `tuya_parse_protocol_data` 对 LAN/MQ 路由的选择
- decrypt/encrypt wrapper 返回错误时的错误传播
- `tuya_pack_protocol_data` 对无效 `cmd` 的返回
- `lpv35_frame_buffer_size_get`
- `lpv35_frame_serialize` / `deserialize` 的参数校验

**Step 5: 如果公开 API 无法稳定触达某个目标分支，再加最小 seam**

Rules:

- 只允许暴露必要的依赖替换点
- 不允许改变生产行为
- 先在 plan 执行时确认确实被阻塞，再补 seam

**Step 6: 跑目标测试转绿**

Run:

```bash
ctest --test-dir build/tests/host -R test_tuya_cloud_service_protocol_host --output-on-failure
```

Expected: `PASS`

**Step 7: 跑全量 Host 基线**

Run:

```bash
bash tools/test/run_host_tests.sh
```

Expected: 全量通过。

**Step 8: Commit**

```bash
git add src/tuya_cloud_service/test/host
git commit -m "test: add host unit tests for tuya_protocol"
```

### Task 5: 文档收口并记录 Phase 2-5 待办

**Files:**
- Modify: `docs/testing/unit-test-guide.md`
- Modify: `tuyaopen_unit_test_scheme.md`
- Modify: `docs/plans/2026-03-11-tuya-cloud-service-host-tests-design.md`

**Step 1: 更新当前覆盖范围**

Document:

- `src/tuya_cloud_service/test/host`
- 已落地的 `tuya_endpoint`、`tuya_transport`、`tuya_protocol`

**Step 2: 把后续阶段记录到文档**

Add a short backlog for:

- `tuya_health`
- `tuya_iot_dp`
- `tuya_weather`
- `tuya_http`
- `tuya_tls`
- `tuya_iot` / `mqtt_service` / `atop_service`

**Step 3: 验证文档 diff 无格式问题**

Run:

```bash
git diff --check -- docs/testing/unit-test-guide.md tuyaopen_unit_test_scheme.md \
  docs/plans/2026-03-11-tuya-cloud-service-host-tests-design.md
```

Expected: no output

**Step 4: Commit**

```bash
git add docs/testing/unit-test-guide.md tuyaopen_unit_test_scheme.md docs/plans/2026-03-11-tuya-cloud-service-host-tests-design.md
git commit -m "docs: record tuya_cloud_service host test phases"
```
