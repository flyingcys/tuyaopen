# Component Host Test Template

## Purpose

Use this template when adding a new `Host` unit-test suite for a TuyaOpen component under:

```text
src/<component>/test/host/
```

The goal is to keep test code close to the component while reusing the shared `tests/host` infrastructure.

## Recommended Layout

```text
src/<component>/
├── include/
├── src/
└── test/
    └── host/
        ├── CMakeLists.txt
        ├── test_<component>_core.c
        ├── test_<component>_error_paths.c
        └── test_<component>_integration_edges.c
```

Use multiple `test_*.c` files when:

- the component has distinct responsibilities
- the runner `main()` would become too large
- different mock sets are easier to reason about in separate files

## CMakeLists Template

Adapt the include paths and mocked headers to the real component:

```cmake
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(MOCK_DIR ${GEN_DIR}/mocks)

tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/<domain>/tkl_<dep>.h ${GEN_DIR} MOCK_TKL_DEP_C)
tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../src/tal_system/include/tal_mutex.h ${GEN_DIR} MOCK_TAL_MUTEX_C)

add_custom_target(test_<component>_codegen DEPENDS
    ${MOCK_TKL_DEP_C}
    ${MOCK_TAL_MUTEX_C}
)
add_dependencies(tuya_codegen test_<component>_codegen)

tuya_add_host_test(test_<component>_host
    test_<component>_core.c
    test_<component>_error_paths.c
    ${CMAKE_SOURCE_DIR}/../../src/<component>/src/<component>.c
    ${MOCK_TKL_DEP_C}
    ${MOCK_TAL_MUTEX_C}
    ${CMAKE_SOURCE_DIR}/support/stubs/tal_log_stub.c
)

add_dependencies(test_<component>_host test_<component>_codegen)

target_include_directories(test_<component>_host PRIVATE
    ${MOCK_DIR}
    ${CMAKE_SOURCE_DIR}/../../src/<component>/include
    ${CMAKE_SOURCE_DIR}/../../src/common/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_system/include
    ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/<domain>
)
```

Notes:

- `tuya_add_host_test()` already links `unity` and `cmock`
- `tuya_generate_mock()` outputs generated files under `${GEN_DIR}/mocks`
- Put generated mock dependencies behind a `test_<component>_codegen` custom target

## Test File Template

Use one file as the runner entrypoint with `setUp`, `tearDown`, and `RUN_TEST(...)`:

```c
#include "unity.h"

#include "mock_tkl_<dep>.h"
#include "tal_api.h"
#include "<component>.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_<component>_returns_error_when_dep_fails(void)
{
    tkl_<dep>_init_ExpectAndReturn(OPRT_COM_ERROR);

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, <component>_init());
}

static void test_<component>_success_path(void)
{
    tkl_<dep>_init_ExpectAndReturn(OPRT_OK);

    TEST_ASSERT_EQUAL(OPRT_OK, <component>_init());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_<component>_returns_error_when_dep_fails);
    RUN_TEST(test_<component>_success_path);
    return UNITY_END();
}
```

## What to Test First

Prioritize these cases:

1. Parameter validation
2. Dependency failure mapping
3. Main success path
4. Resource cleanup on error
5. State transitions and edge cases

Avoid starting with:

- giant end-to-end flows
- pure call-count tests with no behavior assertion
- duplicated tests that only differ in names

## Mock Selection Guide

Use `Mock` when:

- the dependency is a TKL boundary
- you need exact return-code injection
- you need to verify retry/error handling

Use `Stub` when:

- the dependency is simple and deterministic
- you just need a harmless implementation, such as logging
- the real behavior is irrelevant to the assertion

Use `Fake` when:

- pure mocks make behavior unrealistic
- you need small but stateful storage or protocol behavior
- contract validation matters more than exact call sequence

Practical examples in the current tree:

- `tests/host/support/stubs/tal_log_stub.c` is a stub
- `src/tal_kv/test/host/` mixes generated mocks with a more behavior-aware filesystem dependency strategy

## Registration Checklist

When the new suite is ready:

1. Add `src/<component>/test/host/CMakeLists.txt`
2. Register it from `tests/host/CMakeLists.txt`
3. Run:

```bash
bash tools/test/run_host_tests.sh
```

4. If formatting changed, run:

```bash
python tools/check_format.py --debug --dir src/<component>/test/host
```

## Review Checklist

Before sending the change:

- test names describe behavior, not implementation trivia
- at least one failure path is covered
- generated mocks are not committed as source files
- includes are minimal and real
- the suite is reachable through `tests/host/CMakeLists.txt`
