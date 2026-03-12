/**
 * @file test_tkl_flash.c
 * @brief Target-side tkl_flash suite.
 *
 * This file validates the Linux file-backed flash adapter through deterministic
 * contract checks that remain stable on local `LINUX` execution.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include <string.h>

#include "tkl_flash.h"
#include "tuya_error_code.h"
#include "unity.h"

static void test_tkl_flash_read_requires_init(void)
{
    unsigned char data = 0;

    TEST_ASSERT_EQUAL(OPRT_RESOURCE_NOT_READY, tkl_flash_read(0, &data, 1));
}

static void test_tkl_flash_write_requires_init(void)
{
    const unsigned char data = 0xa5;

    TEST_ASSERT_EQUAL(OPRT_RESOURCE_NOT_READY, tkl_flash_write(0, &data, 1));
}

static void test_tkl_flash_get_info_rejects_null_output(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_KV_DATA, NULL));
}

static void test_tkl_flash_get_info_rejects_invalid_type(void)
{
    TUYA_FLASH_BASE_INFO_T info = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM,
                      tkl_flash_get_one_type_info((TUYA_FLASH_TYPE_E)(TUYA_FLASH_TYPE_MAX + 1), &info));
}

static void test_tkl_flash_get_info_reports_kv_partition(void)
{
    TUYA_FLASH_BASE_INFO_T info = {0};

    TEST_ASSERT_EQUAL(OPRT_OK, tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_KV_DATA, &info));
    TEST_ASSERT_EQUAL_UINT32(1, info.partition_num);
    TEST_ASSERT_EQUAL_HEX32(0x1000, info.partition[0].start_addr);
    TEST_ASSERT_EQUAL_HEX32(0x1000, info.partition[0].block_size);
    TEST_ASSERT_EQUAL_HEX32(0x8000, info.partition[0].size);
}

static void test_tkl_flash_get_info_reports_uf_partition(void)
{
    TUYA_FLASH_BASE_INFO_T info = {0};

    TEST_ASSERT_EQUAL(OPRT_OK, tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_UF, &info));
    TEST_ASSERT_EQUAL_UINT32(1, info.partition_num);
    TEST_ASSERT_EQUAL_HEX32(0x9000, info.partition[0].start_addr);
    TEST_ASSERT_EQUAL_HEX32(0x1000, info.partition[0].block_size);
    TEST_ASSERT_EQUAL_HEX32(0x18000, info.partition[0].size);
}

static void test_tkl_flash_write_read_round_trip_after_init(void)
{
    TUYA_FLASH_BASE_INFO_T info                        = {0};
    const unsigned char    write_buf[]                 = {0x10, 0x32, 0x54, 0x76};
    unsigned char          read_buf[sizeof(write_buf)] = {0};
    unsigned int           addr                        = 0;

    TEST_ASSERT_EQUAL(OPRT_OK, tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_KV_DATA, &info));
    addr = info.partition[0].start_addr + info.partition[0].block_size;

    TEST_ASSERT_EQUAL(OPRT_OK, tkl_flash_write(addr, write_buf, sizeof(write_buf)));
    TEST_ASSERT_EQUAL(OPRT_OK, tkl_flash_read(addr, read_buf, sizeof(read_buf)));
    TEST_ASSERT_EQUAL_MEMORY(write_buf, read_buf, sizeof(write_buf));
}

static void test_tkl_flash_lock_unlock_report_not_supported(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_flash_lock(0, 0x1000));
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_flash_unlock(0, 0x1000));
}

int tuya_unit_test_run_tkl_flash_suite(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tkl_flash_read_requires_init);
    RUN_TEST(test_tkl_flash_write_requires_init);
    RUN_TEST(test_tkl_flash_get_info_rejects_null_output);
    RUN_TEST(test_tkl_flash_get_info_rejects_invalid_type);
    RUN_TEST(test_tkl_flash_get_info_reports_kv_partition);
    RUN_TEST(test_tkl_flash_get_info_reports_uf_partition);
    RUN_TEST(test_tkl_flash_write_read_round_trip_after_init);
    RUN_TEST(test_tkl_flash_lock_unlock_report_not_supported);
    return UNITY_END();
}
