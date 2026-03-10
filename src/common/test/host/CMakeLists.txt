tuya_add_host_test(test_common_host
    test_crc_and_mix.c
    test_uni_random.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilities/crc32i.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilities/crc_16.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilities/mix_method.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilities/uni_random.c
)

target_include_directories(test_common_host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilities
    ${CMAKE_SOURCE_DIR}/../../src/tuya_cloud_service/tls
    ${CMAKE_SOURCE_DIR}/../../src/libtls/mbedtls-3.1.0/include
    ${CMAKE_SOURCE_DIR}/../../src/libtls/port
)
