set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(MOCK_DIR ${GEN_DIR}/mocks)

tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/system/tkl_system.h ${GEN_DIR} MOCK_TKL_SYSTEM_C)
tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/system/tkl_memory.h ${GEN_DIR} MOCK_TKL_MEMORY_C)

add_custom_target(test_tal_system_codegen DEPENDS
    ${MOCK_TKL_SYSTEM_C}
    ${MOCK_TKL_MEMORY_C}
)
add_dependencies(tuya_codegen test_tal_system_codegen)

tuya_add_host_test(test_tal_system_host
    test_tal_memory.c
    test_tal_system_core.c
    ${CMAKE_SOURCE_DIR}/../../src/tal_system/src/tal_system.c
    ${MOCK_TKL_SYSTEM_C}
    ${MOCK_TKL_MEMORY_C}
    ${CMAKE_SOURCE_DIR}/support/stubs/tal_log_stub.c
)

add_dependencies(test_tal_system_host test_tal_system_codegen)

target_include_directories(test_tal_system_host PRIVATE
    ${MOCK_DIR}
    ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/system
    ${CMAKE_SOURCE_DIR}/../../src/tal_system/include
)
