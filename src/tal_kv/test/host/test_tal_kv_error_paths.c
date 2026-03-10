#include "unity.h"

#include <string.h>

#include "lfs.h"
#include "lfs_config.h"
#include "tal_kv.h"
#include "mock_tal_mutex.h"
#include "mock_tkl_flash.h"

extern void test_tal_kv_init_success(void);
extern void test_tal_kv_init_mount_fail_then_format_success(void);
extern void test_tal_kv_set_success(void);
extern void test_tal_kv_get_success(void);
extern void test_tal_kv_del_success(void);

void setUp(void)
{
}

void tearDown(void)
{
}

int kv_serialize(const kv_db_t *db, const uint32_t dbcnt, char **out, uint32_t *out_len)
{
    (void)db;
    (void)dbcnt;
    if (out) {
        *out = NULL;
    }
    if (out_len) {
        *out_len = 0;
    }
    return OPRT_OK;
}

int kv_deserialize(const char *in, kv_db_t *db, const uint32_t dbcnt)
{
    (void)in;
    (void)db;
    (void)dbcnt;
    return OPRT_OK;
}

extern int user_provided_block_device_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer,
                                           lfs_size_t size);
extern int user_provided_block_device_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                                           const void *buffer, lfs_size_t size);
extern int user_provided_block_device_erase(const struct lfs_config *c, lfs_block_t block);
extern int user_provided_block_device_sync(const struct lfs_config *c);

void test_tal_kv_set_invalid_params(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_kv_set(NULL, (const uint8_t *)"v", 1));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_kv_set("k", NULL, 1));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_kv_set("k", (const uint8_t *)"v", 0));
}

void test_tal_kv_get_invalid_params(void)
{
    uint8_t *value = NULL;
    size_t length = 0;
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_kv_get(NULL, &value, &length));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_kv_get("k", NULL, &length));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_kv_get("k", &value, NULL));
}

void test_user_block_device_io_path(void)
{
    struct lfs_config c;
    memset(&c, 0, sizeof(c));
    c.block_size = 4096;

    tkl_flash_read_ExpectAndReturn(0x10000, NULL, 16, OPRT_OK);
    TEST_ASSERT_EQUAL(LFS_ERR_OK, user_provided_block_device_read(&c, 0, 0, NULL, 16));
    tkl_flash_read_ExpectAndReturn(0x10000, NULL, 16, OPRT_COM_ERROR);
    TEST_ASSERT_EQUAL(LFS_ERR_IO, user_provided_block_device_read(&c, 0, 0, NULL, 16));

    tkl_flash_write_ExpectAndReturn(0x10000, NULL, 16, OPRT_OK);
    TEST_ASSERT_EQUAL(LFS_ERR_OK, user_provided_block_device_prog(&c, 0, 0, NULL, 16));
    tkl_flash_write_ExpectAndReturn(0x10000, NULL, 16, OPRT_COM_ERROR);
    TEST_ASSERT_EQUAL(LFS_ERR_IO, user_provided_block_device_prog(&c, 0, 0, NULL, 16));

    tkl_flash_erase_ExpectAndReturn(0x10000, 4096, OPRT_OK);
    TEST_ASSERT_EQUAL(LFS_ERR_OK, user_provided_block_device_erase(&c, 0));
    tkl_flash_erase_ExpectAndReturn(0x10000, 4096, OPRT_COM_ERROR);
    TEST_ASSERT_EQUAL(LFS_ERR_IO, user_provided_block_device_erase(&c, 0));

    TEST_ASSERT_EQUAL(LFS_ERR_OK, user_provided_block_device_sync(&c));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_tal_kv_init_success);
    RUN_TEST(test_tal_kv_init_mount_fail_then_format_success);
    RUN_TEST(test_tal_kv_set_success);
    RUN_TEST(test_tal_kv_get_success);
    RUN_TEST(test_tal_kv_del_success);
    RUN_TEST(test_tal_kv_set_invalid_params);
    RUN_TEST(test_tal_kv_get_invalid_params);
    RUN_TEST(test_user_block_device_io_path);

    return UNITY_END();
}
