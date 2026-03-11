# TuyaOpen Unit Test Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 为 `tuyaopen` 建立一套可持续维护的单元测试体系，先落地 `Host` 单元测试基建和首批试点组件，再补齐 `Target/HIL` 测试骨架与 CI 门禁。

**Architecture:** 采用“组件内测试代码 + 根目录统一调度”的结构。`Host` 测试使用独立原生 CMake 入口，不耦合产品工程；`Target` 测试使用 `test_app/unit_test_app` 通过现有 `tos.py` 构建，并用 `pytest` 统一调度真实设备测试。实现顺序先 `Host` 再 `CI`，最后 `Target/HIL`。

**Tech Stack:** C, CMake, CTest, Unity, CMock, gcov/lcov 或 gcovr, AddressSanitizer, UndefinedBehaviorSanitizer, Python, pytest, TuyaOpen `tos.py`

---

## 执行状态（2026-03-11）

- [x] Task 1: 建立测试目录和第三方依赖基线
- [x] Task 2: 建立 Host 测试公共构建能力
- [x] Task 3: 建立 `src/common` Host 单元测试
- [x] Task 4: 建立 `src/tal_system` Host 单元测试
- [x] Task 5: 建立 `src/tal_kv` Host 单元测试
- [x] Task 6: 建立 `src/tal_network` Host 单元测试
- [x] Task 7: 建立 `src/tal_wifi` Host 单元测试
- [x] Task 8: 接入 Host 覆盖率、Sanitizer 和 CI
- [x] Task 9: 建立 Target 单元测试应用骨架
- [x] Task 10: 建立 Target `pytest` 调度骨架
- [x] Task 11: 文档与贡献规范收口
- [x] Task 12: 执行顺序与停靠点回收

### 本轮已落地提交

- `a9d7245a` `build: add unit test directory and vendor baseline`
- `48a7d3c0` `build: add host test helpers and options`
- `04da0538` `test: add host unit tests for common utilities`
- `a5465d96` `test: add host unit tests for tal_system`
- `cc7666ce` `test: add host unit tests for tal_kv`
- `c986d3c5` `test: add host unit tests for tal_network`
- `c096f13f` `test: add host unit tests for tal_wifi`
- `0971dbba` `ci: add host unit test workflow`

### 执行偏差记录（与原计划相比）

- `codegen` 目标名改为 `tuya_codegen`，避免 CMake 保留目标名冲突。
- `tal_system` 实现集中在 `src/tal_system/src/tal_system.c`，不存在 `tal_memory.c`。
- 为了执行 `tal_kv` Host 单测，初始化了 `src/tal_kv/littlefs` 子模块。
- `CMock` 插件增加了 `expect_any_args` 与 `return_thru_ptr`，用于生成当前测试依赖的 API。
- `tests/vendor/unity` 与 `tests/vendor/cmock` 作为 Host/Target 共用测试依赖，需要在新 worktree 中显式初始化。
- Host 基线当前为 `5/5` suites 通过，后续阶段以 `Task 9-12` 为主。
- `tos.py check` 原先假设 `.git/hooks` 位于工作区目录下；为兼容 git worktree，已改为通过 `git rev-parse --git-path hooks` 解析真实 hooks 路径。
- 当前环境仓库根目录即项目根，不存在历史说明中的 `/workspace` 路径，相关命令已统一为仓库根执行。

---

### Task 1: 建立测试目录和第三方依赖基线

**Files:**
- Create: `tests/host/CMakeLists.txt`
- Create: `tests/host/cmake/AddTuyaHostTest.cmake`
- Create: `tests/host/support/cmock/cmock_config.yml`
- Create: `tests/target/pytest/.gitkeep`
- Create: `tests/target/runners/.gitkeep`
- Modify: `/home/share/samba/open-github/open-utest/tuyaopen/.gitmodules`
- Modify: `/home/share/samba/open-github/open-utest/tuyaopen/.gitignore`
- Create: `tests/vendor/unity/`
- Create: `tests/vendor/cmock/`

**Step 1: 创建空目录和基础入口**

```bash
mkdir -p tests/host/cmake tests/host/support/cmock tests/target/pytest tests/target/runners tests/vendor
```

**Step 2: 固定 Unity/CMock 版本**

```bash
git submodule add https://github.com/ThrowTheSwitch/Unity.git tests/vendor/unity
git submodule add https://github.com/ThrowTheSwitch/CMock.git tests/vendor/cmock
```

Expected: `.gitmodules` 出现两个新 submodule，`git status --short` 显示新路径。

**Step 3: 写 Host 顶层 CMake 和通用函数**

```cmake
# tests/host/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(tuyaopen_host_tests C)
enable_testing()
add_subdirectory(../vendor/unity tests_vendor_unity)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/AddTuyaHostTest.cmake)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../src/common/test/host common_host)
```

```cmake
# tests/host/cmake/AddTuyaHostTest.cmake
function(tuya_add_host_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE unity)
    add_test(NAME ${target} COMMAND ${target})
endfunction()
```

**Step 4: 写 CMock 配置**

```yaml
# tests/host/support/cmock/cmock_config.yml
:cmock:
  :mock_path: mocks
  :plugins:
    - :ignore
    - :expect
    - :callback
```

**Step 5: 验证基线**

Run:

```bash
cmake -S tests/host -B build/tests/host
```

Expected: 配置成功，失败点只允许来自尚未创建的组件测试子目录。

**Step 6: Commit**

```bash
git add .gitmodules .gitignore tests/host tests/vendor tests/target
git commit -m "build: add unit test directory and vendor baseline"
```

### Task 2: 建立 Host 测试公共构建能力

**Files:**
- Modify: `tests/host/CMakeLists.txt`
- Modify: `tests/host/cmake/AddTuyaHostTest.cmake`
- Create: `tests/host/cmake/GenerateMocks.cmake`
- Create: `tests/host/support/stubs/tal_log_stub.c`
- Create: `tests/host/support/stubs/test_clock_stub.c`
- Create: `tests/host/support/README.md`

**Step 1: 增加 Coverage 和 Sanitizer 选项**

```cmake
option(TUYA_ENABLE_COVERAGE "Enable coverage" OFF)
option(TUYA_ENABLE_ASAN "Enable ASAN" OFF)
option(TUYA_ENABLE_UBSAN "Enable UBSAN" OFF)
```

**Step 2: 增加 mock 生成函数**

```cmake
# tests/host/cmake/GenerateMocks.cmake
function(tuya_generate_mock header out_dir)
    # 调用 tests/vendor/cmock/lib/cmock.rb
endfunction()
```

**Step 3: 增加通用 stub**

```c
/* tests/host/support/stubs/tal_log_stub.c */
OPERATE_RET tal_log_print_raw(const char *fmt, ...) { return OPRT_OK; }
```

**Step 4: 把公共能力接入顶层入口**

Run:

```bash
cmake -S tests/host -B build/tests/host -DTUYA_ENABLE_COVERAGE=ON
cmake --build build/tests/host -j
```

Expected: 顶层基建可编译，若没有测试目标，`ctest` 返回 `No tests were found`。

**Step 5: Commit**

```bash
git add tests/host
git commit -m "build: add host test helpers and options"
```

### Task 3: 建立 `src/common` Host 单元测试

**Files:**
- Create: `src/common/test/host/CMakeLists.txt`
- Create: `src/common/test/host/test_crc_and_mix.c`
- Create: `src/common/test/host/test_uni_random.c`
- Modify: `tests/host/CMakeLists.txt`

**Step 1: 先写失败用例**

```c
void test_crc32_known_vector(void) {
    const unsigned char data[] = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, hash_crc32i_total(data, 9));
}
```

**Step 2: 接入 `src/common` 测试子目录**

```cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/../../src/common/test/host common_host)
```

**Step 3: 写最小可运行构建文件**

```cmake
tuya_add_host_test(test_common_host
    test_crc_and_mix.c
    test_uni_random.c
    ../../utilities/crc32i.c
    ../../utilities/crc_16.c
    ../../utilities/mix_method.c
    ../../utilities/uni_random.c
)
```

**Step 4: 运行并让它先失败一次**

Run:

```bash
cmake -S tests/host -B build/tests/host
cmake --build build/tests/host -j
ctest --test-dir build/tests/host -R test_common_host --output-on-failure
```

Expected: 初次因为 stub 或 include 缺失失败，补齐后转为 PASS。

**Step 5: 完成 `common` 第一批断言**

覆盖：

- CRC16/CRC32 标准向量
- `mix_method` 的大小写转换、hex 编解码、排序、版本解析
- `uni_random` 的依赖注入和错误路径

**Step 6: Commit**

```bash
git add tests/host src/common/test/host
git commit -m "test: add host unit tests for common utilities"
```

### Task 4: 建立 `src/tal_system` Host 单元测试

**Files:**
- Create: `src/tal_system/test/host/CMakeLists.txt`
- Create: `src/tal_system/test/host/test_tal_memory.c`
- Create: `src/tal_system/test/host/test_tal_system_core.c`
- Modify: `tests/host/CMakeLists.txt`

**Step 1: 写失败用例，覆盖内存接口边界**

```c
void test_tal_malloc_zero_returns_null(void) {
    TEST_ASSERT_NULL(tal_malloc(0));
}
```

**Step 2: 为 `tkl_system.h`、`tkl_memory.h` 生成 mocks**

Run:

```bash
cmake --build build/tests/host --target codegen -j
```

Expected: `build/tests/host/.../mocks/` 下出现 mock 文件。

**Step 3: 写构建脚本**

```cmake
tuya_add_host_test(test_tal_system_host
    test_tal_memory.c
    test_tal_system_core.c
    ../../src/tal_memory.c
    ../../src/tal_system.c
)
```

**Step 4: 跑单测并补齐 stub**

Run:

```bash
ctest --test-dir build/tests/host -R test_tal_system_host --output-on-failure
```

Expected: 覆盖 `malloc/calloc/realloc/free`、`sleep/reset/tick/random` 的正常与失败路径。

**Step 5: Commit**

```bash
git add src/tal_system/test/host tests/host
git commit -m "test: add host unit tests for tal_system"
```

### Task 5: 建立 `src/tal_kv` Host 单元测试

**Files:**
- Create: `src/tal_kv/test/host/CMakeLists.txt`
- Create: `src/tal_kv/test/host/test_tal_kv_init.c`
- Create: `src/tal_kv/test/host/test_tal_kv_rw.c`
- Create: `src/tal_kv/test/host/test_tal_kv_error_paths.c`
- Modify: `tests/host/CMakeLists.txt`

**Step 1: 先写初始化失败用例**

```c
void test_tal_kv_init_returns_error_when_flash_info_fails(void) {
    tkl_flash_get_one_type_info_ExpectAnyArgsAndReturn(OPRT_COM_ERROR);
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tal_kv_init(&cfg));
}
```

**Step 2: 为 `tkl_flash.h`、`tal_mutex.h`、`tal_hash.h`、`tal_symmetry.h`、`lfs.h` 建 mock 或 fake**

原则：

- `lfs` 优先用 fake，不要把所有行为都 mock 成“调用即成功”

**Step 3: 写构建脚本并接入**

Run:

```bash
cmake --build build/tests/host -j
ctest --test-dir build/tests/host -R test_tal_kv_host --output-on-failure
```

Expected: 至少有 3 组 suite，覆盖 `init`、`set/get/del`、异常路径。

**Step 4: 增加内存失败和加密失败注入**

覆盖：

- `tal_malloc` 失败
- `lfs_mount` 返回损坏
- `tal_aes128_cbc_encode/decode` 失败

**Step 5: Commit**

```bash
git add src/tal_kv/test/host tests/host
git commit -m "test: add host unit tests for tal_kv"
```

### Task 6: 建立 `src/tal_network` Host 单元测试

**Files:**
- Create: `src/tal_network/test/host/CMakeLists.txt`
- Create: `src/tal_network/test/host/test_tal_network_socket.c`
- Create: `src/tal_network/test/host/test_tal_network_timeout.c`
- Create: `src/tal_network/test/host/test_tal_network_register.c`
- Modify: `tests/host/CMakeLists.txt`

**Step 1: 写 socket/create/connect 的失败用例**

```c
void test_tal_net_connect_maps_platform_error(void) {
    tkl_net_connect_ExpectAndReturn(10, 0x12345678, 80, OPRT_COM_ERROR);
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tal_net_connect(10, 0x12345678, 80));
}
```

**Step 2: 生成 `tkl_network.h`、`tal_mutex.h`、`tal_memory.h` mocks**

**Step 3: 写构建脚本**

```cmake
tuya_add_host_test(test_tal_network_host
    test_tal_network_socket.c
    test_tal_network_timeout.c
    test_tal_network_register.c
    ../../src/tal_network.c
    ../../src/tal_network_register.c
    ../../src/tal_platform.c
)
```

**Step 4: 跑测试**

Run:

```bash
ctest --test-dir build/tests/host -R test_tal_network_host --output-on-failure
```

Expected: 覆盖创建、连接、收发、超时、阻塞模式和注册流程。

**Step 5: Commit**

```bash
git add src/tal_network/test/host tests/host
git commit -m "test: add host unit tests for tal_network"
```

### Task 7: 建立 `src/tal_wifi` Host 单元测试

**Files:**
- Create: `src/tal_wifi/test/host/CMakeLists.txt`
- Create: `src/tal_wifi/test/host/test_tal_wifi_init.c`
- Create: `src/tal_wifi/test/host/test_tal_wifi_station.c`
- Create: `src/tal_wifi/test/host/test_tal_wifi_ap.c`
- Modify: `tests/host/CMakeLists.txt`

**Step 1: 写失败用例，先覆盖初始化和 station 连接**

```c
void test_tal_wifi_init_returns_error_when_tkl_init_fails(void) {
    tkl_wifi_init_ExpectAnyArgsAndReturn(OPRT_COM_ERROR);
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tal_wifi_init(cb));
}
```

**Step 2: 生成 `tkl_wifi.h` 和系统相关 mocks**

**Step 3: 写构建脚本**

**Step 4: 跑测试**

Run:

```bash
ctest --test-dir build/tests/host -R test_tal_wifi_host --output-on-failure
```

Expected: 覆盖 `init/scan/connect/disconnect/get_ip/set_ip/get_mac/set_mac/lp_mode` 的成功与错误路径。

**Step 5: Commit**

```bash
git add src/tal_wifi/test/host tests/host
git commit -m "test: add host unit tests for tal_wifi"
```

### Task 8: 接入 Host 覆盖率、Sanitizer 和 CI

**Files:**
- Create: `.github/workflows/unit_tests_host.yml`
- Modify: `tests/host/CMakeLists.txt`
- Create: `tools/test/run_host_tests.sh`
- Create: `tools/test/run_host_coverage.sh`
- Create: `tools/test/README.md`

**Step 1: 增加覆盖率汇总目标**

```cmake
add_custom_target(coverage
    COMMAND ctest --output-on-failure
)
```

**Step 2: 增加本地执行脚本**

```bash
#!/usr/bin/env bash
cmake -S tests/host -B build/tests/host -DTUYA_ENABLE_COVERAGE=ON
cmake --build build/tests/host -j
ctest --test-dir build/tests/host --output-on-failure
```

**Step 3: 增加 GitHub Actions**

lane:

- format
- host build + unit tests
- coverage artifact
- asan/ubsan

**Step 4: 运行最小验证**

Run:

```bash
bash tools/test/run_host_tests.sh
bash tools/test/run_host_coverage.sh
```

Expected: Host 试点组件全部 PASS，生成覆盖率报告目录。

**Step 5: Commit**

```bash
git add .github/workflows/unit_tests_host.yml tests/host tools/test
git commit -m "ci: add host unit test workflow"
```

### Task 9: 建立 Target 单元测试应用骨架

**Files:**
- Create: `test_app/unit_test_app/CMakeLists.txt`
- Create: `test_app/unit_test_app/app_default.config`
- Create: `test_app/unit_test_app/src/test_main.c`
- Create: `test_app/unit_test_app/src/test_registry.c`
- Create: `test_app/unit_test_app/src/test_registry.h`

**Step 1: 建立最小测试 app**

```c
int main(void) {
    tuya_unit_test_main();
    return 0;
}
```

**Step 2: 先注册一个 smoke test**

```c
void register_all_tests(void) {
    /* 先只接一个 hello test，验证固件能跑 */
}
```

**Step 3: 通过 `tos.py` 走通构建**

Run:

```bash
cd /workspace && . ./export.sh
mkdir -p .cache && touch .cache/.dont_prompt_update_platform
cd /home/share/samba/open-github/open-utest/tuyaopen/test_app/unit_test_app
tos.py check
tos.py build
```

Expected: `dist/` 下产出目标文件，至少 `LINUX` 目标能编过。

**Step 4: Commit**

```bash
git add test_app/unit_test_app
git commit -m "build: add target unit test app skeleton"
```

### Task 10: 建立 Target `pytest` 调度骨架

**Files:**
- Create: `tests/target/pytest/conftest.py`
- Create: `tests/target/pytest/test_smoke.py`
- Create: `tests/target/pytest/test_tkl_gpio.py`
- Create: `tests/target/runners/serial_runner.py`
- Create: `tests/target/runners/flash_runner.py`
- Create: `pytest.ini`

**Step 1: 先做最小串口 runner**

```python
class SerialRunner:
    def expect(self, pattern, timeout=10): ...
```

**Step 2: 写 smoke test**

```python
def test_target_smoke(dut):
    dut.expect("UNITY_BEGIN")
    dut.expect("OK")
```

**Step 3: 写首个 TKL GPIO 占位测试**

```python
@pytest.mark.target
def test_tkl_gpio_port(dut):
    dut.write("ut_run tkl_gpio")
    dut.expect("PASS")
```

**Step 4: 本地验证**

Run:

```bash
pytest tests/target/pytest -m target -v
```

Expected: 在无设备环境可通过 mock runner 或 skip；接设备后能抓到串口日志。

**Step 5: Commit**

```bash
git add tests/target pytest.ini
git commit -m "test: add target pytest runner skeleton"
```

### Task 11: 文档与贡献规范收口

**Files:**
- Modify: `/home/share/samba/open-github/open-utest/tuyaopen/README_zh.md`
- Modify: `/home/share/samba/open-github/open-utest/tuyaopen/README.md`
- Modify: `/home/share/samba/open-github/open-utest/tuyaopen/tuyaopen_unit_test_scheme.md`
- Modify: `/home/share/samba/open-github/open-utest/tuyaopen/tools/test/README.md`
- Modify: `/home/share/samba/open-github/open-utest/tuyaopen/.github/pull_request_template.md`
- Create: `docs/testing/unit-test-guide.md`
- Create: `docs/testing/component-test-template.md`

**Step 1: 写开发者入口文档**

内容包括：

- 如何跑 Host 单测
- 如何看覆盖率
- 如何给组件新增测试
- 如何跑 Target/HIL 测试

**Step 2: 写测试模板**

模板包括：

- `src/<component>/test/host/CMakeLists.txt`
- `mock_list.txt`
- `test_xxx.c`

**Step 3: 最终验证**

Run:

```bash
python tools/check_format.py --debug --files README.md README_zh.md tuyaopen_unit_test_scheme.md tools/test/README.md .github/pull_request_template.md docs/testing/unit-test-guide.md docs/testing/component-test-template.md
```

Expected: 文档格式检查通过。

**Step 4: Commit**

```bash
git add README.md README_zh.md tuyaopen_unit_test_scheme.md tools/test/README.md .github/pull_request_template.md docs/testing
git commit -m "docs: add unit test developer guide"
```

### Task 12: 执行顺序与停靠点

**Files:**
- Reference only: `docs/plans/2026-03-10-tuyaopen-unit-test-implementation.md`

**Step 1: Phase 0**

依次执行 Task 1, Task 2。

Exit criteria:

- `cmake -S tests/host -B build/tests/host` 成功
- 本地 `ctest` 可运行

**Step 2: Phase 1**

依次执行 Task 3, Task 4, Task 5, Task 6。

Exit criteria:

- `common/tal_system/tal_kv/tal_network` Host 单测全部 PASS
- 有覆盖率报告

**Step 3: Phase 2**

执行 Task 7, Task 8。

Exit criteria:

- `tal_wifi` Host 单测 PASS
- PR 级 CI 跑通

**Step 4: Phase 3**

执行 Task 9, Task 10。

Exit criteria:

- `test_app/unit_test_app` 可构建
- `pytest` Target 骨架可运行

**Step 5: Phase 4**

执行 Task 11。

Exit criteria:

- 文档、模板、贡献入口完整
