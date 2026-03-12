set(TUYA_CLOUD_SERVICE_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(MOCK_DIR ${GEN_DIR}/mocks)

add_library(tuya_cloud_service_test_support INTERFACE)

target_include_directories(tuya_cloud_service_test_support
    INTERFACE
        ${TUYA_CLOUD_SERVICE_TEST_DIR}/support
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/cloud
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/authorize
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/lan
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/netcfg
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/netmgr
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/transport
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/protocol
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/schema
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/tls
        ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/weather
        ${CMAKE_SOURCE_DIR}/../../src/common/include
        ${CMAKE_SOURCE_DIR}/../../src/common/base/include
        ${CMAKE_SOURCE_DIR}/../../src/common/utilities
        ${CMAKE_SOURCE_DIR}/../../src/common/backoffAlgorithm/source/include
        ${CMAKE_SOURCE_DIR}/../../src/libcjson/cJSON
        ${CMAKE_SOURCE_DIR}/../../src/libmqtt/include
        ${CMAKE_SOURCE_DIR}/../../src/libmqtt/coreMQTT/source/include
        ${CMAKE_SOURCE_DIR}/../../src/libhttp/include
        ${CMAKE_SOURCE_DIR}/../../src/libhttp/coreHTTP/source/dependency/3rdparty/http_parser
        ${CMAKE_SOURCE_DIR}/../../src/libtls/include
        ${CMAKE_SOURCE_DIR}/../../src/libtls/port
        ${CMAKE_SOURCE_DIR}/../../src/libtls/mbedtls-3.1.0/include
        ${CMAKE_SOURCE_DIR}/../../src/tal_kv/include
        ${CMAKE_SOURCE_DIR}/../../src/tal_kv/littlefs
        ${CMAKE_SOURCE_DIR}/../../src/tal_kv/port
        ${CMAKE_SOURCE_DIR}/../../src/tal_system/include
        ${CMAKE_SOURCE_DIR}/../../src/tal_network/include
        ${CMAKE_SOURCE_DIR}/../../src/tal_cli/include
        ${CMAKE_SOURCE_DIR}/../../src/tal_wifi/include
        ${CMAKE_SOURCE_DIR}/../../src/tal_driver/include
        ${CMAKE_SOURCE_DIR}/../../src/tal_driver/uart
        ${CMAKE_SOURCE_DIR}/../../src/tal_security/include
        ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/security
        ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/network
        ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/system
        ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/wifi
        ${CMAKE_SOURCE_DIR}/../../tools/porting/adapter/utilities/include
)

tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../src/tal_kv/include/tal_kv.h ${GEN_DIR} MOCK_TAL_KV_C)
tuya_generate_mock(${CMAKE_SOURCE_DIR}/../../src/tal_system/include/tal_memory.h ${GEN_DIR} MOCK_TAL_MEMORY_C)

add_custom_target(test_tuya_cloud_service_endpoint_codegen DEPENDS
    ${MOCK_TAL_KV_C}
    ${MOCK_TAL_MEMORY_C}
)
add_dependencies(tuya_codegen test_tuya_cloud_service_endpoint_codegen)

tuya_add_host_test(test_tuya_cloud_service_endpoint_host
    test_tuya_endpoint.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/cloud/tuya_endpoint.c
    ${MOCK_TAL_KV_C}
    ${MOCK_TAL_MEMORY_C}
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

add_dependencies(test_tuya_cloud_service_endpoint_host test_tuya_cloud_service_endpoint_codegen)

target_link_libraries(test_tuya_cloud_service_endpoint_host PRIVATE tuya_cloud_service_test_support)

target_include_directories(test_tuya_cloud_service_endpoint_host PRIVATE
    ${MOCK_DIR}
)

tuya_add_host_test(test_tuya_cloud_service_transport_host
    test_tuya_transport.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/transport/tuya_transport.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_transport_host PRIVATE tuya_cloud_service_test_support)

tuya_add_host_test(test_tuya_cloud_service_protocol_host
    test_tuya_protocol.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/protocol/tuya_protocol.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_protocol_host PRIVATE tuya_cloud_service_test_support)

tuya_add_host_test(test_tuya_cloud_service_health_host
    test_tuya_health.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/cloud/tuya_health.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_health_host PRIVATE tuya_cloud_service_test_support)

tuya_add_host_test(test_tuya_cloud_service_iot_dp_host
    test_tuya_iot_dp.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/schema/tuya_iot_dp.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_iot_dp_host PRIVATE tuya_cloud_service_test_support)

tuya_add_host_test(test_tuya_cloud_service_weather_host
    test_tuya_weather.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/weather/tuya_weather.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_weather_host PRIVATE tuya_cloud_service_test_support)

tuya_add_host_test(test_tuya_cloud_service_http_host
    test_tuya_http.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/cloud/tuya_http.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_http_host PRIVATE tuya_cloud_service_test_support)

tuya_add_host_test(test_tuya_cloud_service_tls_host
    test_tuya_tls.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/tls/tuya_tls.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_tls_host PRIVATE tuya_cloud_service_test_support)

tuya_add_host_test(test_tuya_cloud_service_iot_host
    test_tuya_iot.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/cloud/tuya_iot.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_iot_host PRIVATE tuya_cloud_service_test_support)
target_compile_options(test_tuya_cloud_service_iot_host PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_tuya_cloud_service_iot_host PRIVATE -Wl,--gc-sections)

tuya_add_host_test(test_tuya_cloud_service_mqtt_host
    test_tuya_mqtt.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/cloud/mqtt_service.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_mqtt_host PRIVATE tuya_cloud_service_test_support)
target_compile_options(test_tuya_cloud_service_mqtt_host PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_tuya_cloud_service_mqtt_host PRIVATE -Wl,--gc-sections)

tuya_add_host_test(test_tuya_cloud_service_atop_host
    test_tuya_atop_service.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/cloud/atop_service.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_atop_host PRIVATE tuya_cloud_service_test_support)
target_compile_options(test_tuya_cloud_service_atop_host PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_tuya_cloud_service_atop_host PRIVATE -Wl,--gc-sections)

tuya_add_host_test(test_tuya_cloud_service_matop_host
    test_tuya_matop_service.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/cloud/matop_service.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_matop_host PRIVATE tuya_cloud_service_test_support)
target_compile_options(test_tuya_cloud_service_matop_host PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_tuya_cloud_service_matop_host PRIVATE -Wl,--gc-sections)

tuya_add_host_test(test_tuya_cloud_service_lan_host
    test_tuya_lan.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/lan/tuya_lan.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_lan_host PRIVATE tuya_cloud_service_test_support)
target_compile_options(test_tuya_cloud_service_lan_host PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_tuya_cloud_service_lan_host PRIVATE -Wl,--gc-sections)
target_compile_definitions(test_tuya_cloud_service_lan_host PRIVATE TUYA_SECURITY_LEVEL=1)

tuya_add_host_test(test_tuya_cloud_service_authorize_host
    test_tuya_authorize.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/authorize/tuya_authorize.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_authorize_host PRIVATE tuya_cloud_service_test_support)
target_compile_options(test_tuya_cloud_service_authorize_host PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_tuya_cloud_service_authorize_host PRIVATE -Wl,--gc-sections)

tuya_add_host_test(test_tuya_cloud_service_netcfg_host
    test_tuya_netcfg.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/netcfg/netcfg.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_netcfg_host PRIVATE tuya_cloud_service_test_support)
target_compile_options(test_tuya_cloud_service_netcfg_host PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_tuya_cloud_service_netcfg_host PRIVATE -Wl,--gc-sections)

tuya_add_host_test(test_tuya_cloud_service_netmgr_host
    test_tuya_netmgr.c
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/netmgr/netmgr.c
    ${CMAKE_SOURCE_DIR}/../../tests/host/support/stubs/tal_log_stub.c
)

target_link_libraries(test_tuya_cloud_service_netmgr_host PRIVATE tuya_cloud_service_test_support)
target_compile_options(test_tuya_cloud_service_netmgr_host PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_tuya_cloud_service_netmgr_host PRIVATE -Wl,--gc-sections)
