#include "unity.h"

#include <string.h>

#include "tal_kv.h"
#include "mock_lfs.h"
#include "mock_tal_hash.h"
#include "mock_tal_mutex.h"
#include "mock_tkl_flash.h"

void test_tal_kv_init_success(void)
{
    tal_kv_cfg_t cfg = {
        .seed = "1234567890123456",
        .key = "1234567890123456",
    };

    tal_sha256_ret_IgnoreAndReturn(OPRT_OK);
    tal_mutex_create_init_IgnoreAndReturn(OPRT_OK);

    TUYA_FLASH_BASE_INFO_T info;
    memset(&info, 0, sizeof(info));
    info.partition[0].block_size = 4096;
    info.partition[0].size = 4096 * 10;
    info.partition[0].start_addr = 0x10000;

    tkl_flash_get_one_type_info_ExpectAnyArgsAndReturn(OPRT_OK);
    tkl_flash_get_one_type_info_ReturnThruPtr_info(&info);

    lfs_mount_ExpectAnyArgsAndReturn(0);

    TEST_ASSERT_EQUAL(0, tal_kv_init(&cfg));
}

void test_tal_kv_init_mount_fail_then_format_success(void)
{
    tal_kv_cfg_t cfg = {
        .seed = "1234567890123456",
        .key = "1234567890123456",
    };

    tal_sha256_ret_IgnoreAndReturn(OPRT_OK);
    tal_mutex_create_init_IgnoreAndReturn(OPRT_OK);

    TUYA_FLASH_BASE_INFO_T info;
    memset(&info, 0, sizeof(info));
    info.partition[0].block_size = 4096;
    info.partition[0].size = 4096 * 10;
    info.partition[0].start_addr = 0x10000;

    tkl_flash_get_one_type_info_ExpectAnyArgsAndReturn(OPRT_OK);
    tkl_flash_get_one_type_info_ReturnThruPtr_info(&info);

    lfs_mount_ExpectAnyArgsAndReturn(LFS_ERR_CORRUPT);
    lfs_format_ExpectAnyArgsAndReturn(0);
    lfs_mount_ExpectAnyArgsAndReturn(0);

    TEST_ASSERT_EQUAL(0, tal_kv_init(&cfg));
}
