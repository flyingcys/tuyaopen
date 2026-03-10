set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(MOCK_DIR ${GEN_DIR}/mocks)

tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/flash/tkl_flash.h ${GEN_DIR} MOCK_TKL_FLASH_C)
tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../src/tal_system/include/tal_mutex.h ${GEN_DIR} MOCK_TAL_MUTEX_C)
tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../src/tal_system/include/tal_memory.h ${GEN_DIR} MOCK_TAL_MEMORY_C)
tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../src/tal_security/include/tal_hash.h ${GEN_DIR} MOCK_TAL_HASH_C)
tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../src/tal_security/include/tal_symmetry.h ${GEN_DIR} MOCK_TAL_SYMMETRY_C)
tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../src/tal_kv/littlefs/lfs.h ${GEN_DIR} MOCK_LFS_C)

add_custom_target(test_tal_kv_codegen DEPENDS
    ${MOCK_TKL_FLASH_C}
    ${MOCK_TAL_MUTEX_C}
    ${MOCK_TAL_MEMORY_C}
    ${MOCK_TAL_HASH_C}
    ${MOCK_TAL_SYMMETRY_C}
    ${MOCK_LFS_C}
)
add_dependencies(tuya_codegen test_tal_kv_codegen)

tuya_add_host_test(test_tal_kv_host
    test_tal_kv_init.c
    test_tal_kv_rw.c
    test_tal_kv_error_paths.c
    ${CMAKE_SOURCE_DIR}/../../src/tal_kv/src/tal_kv.c
    ${MOCK_TKL_FLASH_C}
    ${MOCK_TAL_MUTEX_C}
    ${MOCK_TAL_MEMORY_C}
    ${MOCK_TAL_HASH_C}
    ${MOCK_TAL_SYMMETRY_C}
    ${MOCK_LFS_C}
    ${CMAKE_SOURCE_DIR}/support/stubs/tal_log_stub.c
)

add_dependencies(test_tal_kv_host test_tal_kv_codegen)

target_include_directories(test_tal_kv_host PRIVATE
    ${MOCK_DIR}
    ${CMAKE_SOURCE_DIR}/../../src/tal_kv/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_kv/port
    ${CMAKE_SOURCE_DIR}/../../src/tal_kv/littlefs
    ${CMAKE_SOURCE_DIR}/../../src/tal_security/include
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/tls
    ${CMAKE_SOURCE_DIR}/../../src/tal_system/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_network/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_cli/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_driver/uart
    ${CMAKE_SOURCE_DIR}/../../src/tal_driver/include
    ${CMAKE_SOURCE_DIR}/../../src/common/include
    ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/flash
    ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/security
)

target_compile_definitions(test_tal_kv_host PRIVATE LFS_CONFIG=lfs_config.h)
