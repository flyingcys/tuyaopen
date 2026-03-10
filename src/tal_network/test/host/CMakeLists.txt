set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(MOCK_DIR ${GEN_DIR}/mocks)

tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/network/tkl_network.h ${GEN_DIR} MOCK_TKL_NETWORK_C)

add_custom_target(test_tal_network_codegen DEPENDS ${MOCK_TKL_NETWORK_C})
add_dependencies(tuya_codegen test_tal_network_codegen)

tuya_add_host_test(test_tal_network_host
    test_tal_network_socket.c
    test_tal_network_timeout.c
    test_tal_network_register.c
    ${CMAKE_SOURCE_DIR}/../../src/tal_network/src/tal_network.c
    ${CMAKE_SOURCE_DIR}/../../src/tal_network/src/tal_network_register.c
    ${CMAKE_SOURCE_DIR}/../../src/tal_network/src/tal_platform.c
    ${MOCK_TKL_NETWORK_C}
    ${CMAKE_SOURCE_DIR}/support/stubs/tal_log_stub.c
)

add_dependencies(test_tal_network_host test_tal_network_codegen)

target_include_directories(test_tal_network_host PRIVATE
    ${MOCK_DIR}
    ${CMAKE_SOURCE_DIR}/../../src/tal_network/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_system/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_cli/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_driver/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_driver/uart
    ${CMAKE_SOURCE_DIR}/../../src/tal_kv/include
    ${CMAKE_SOURCE_DIR}/../../src/tal_kv/littlefs
    ${CMAKE_SOURCE_DIR}/../../src/tal_kv/port
    ${CMAKE_SOURCE_DIR}/../../src/tal_security/include
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/tls
    ${CMAKE_SOURCE_DIR}/../../src/common/include
    ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/network
    ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/security
)
