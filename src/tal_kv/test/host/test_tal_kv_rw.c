#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "tal_kv.h"
#include "mock_lfs.h"
#include "mock_tal_memory.h"
#include "mock_tal_mutex.h"
#include "mock_tal_symmetry.h"

static uint8_t g_encoded[16];
static uint8_t g_decoded[16];

static OPERATE_RET tal_aes128_cbc_encode_cb(uint8_t *data, uint32_t len, uint8_t *key, uint8_t *iv, uint8_t **ec_data,
                                            uint32_t *ec_len, int num_calls)
{
    (void)data;
    (void)len;
    (void)key;
    (void)iv;
    (void)num_calls;
    memset(g_encoded, 0xAB, sizeof(g_encoded));
    *ec_data = g_encoded;
    *ec_len = 10;
    return OPRT_OK;
}

static OPERATE_RET tal_aes128_cbc_decode_cb(uint8_t *data, uint32_t len, uint8_t *key, uint8_t *iv, uint8_t **dec_data,
                                            uint32_t *dec_len, int num_calls)
{
    (void)data;
    (void)len;
    (void)key;
    (void)iv;
    (void)num_calls;
    memset(g_decoded, 0, sizeof(g_decoded));
    memcpy(g_decoded, "hello", 5);
    *dec_data = g_decoded;
    *dec_len = 5;
    return OPRT_OK;
}

void test_tal_kv_set_success(void)
{
    tal_mutex_lock_IgnoreAndReturn(OPRT_OK);
    lfs_file_open_ExpectAnyArgsAndReturn(LFS_ERR_OK);
    tal_aes128_cbc_encode_StubWithCallback(tal_aes128_cbc_encode_cb);
    lfs_file_rewind_ExpectAnyArgsAndReturn(0);
    lfs_file_write_ExpectAnyArgsAndReturn(10);
    lfs_file_close_ExpectAnyArgsAndReturn(0);
    tal_aes_free_data_IgnoreAndReturn(OPRT_OK);
    tal_mutex_unlock_IgnoreAndReturn(OPRT_OK);

    TEST_ASSERT_EQUAL(OPRT_OK, tal_kv_set("key", (const uint8_t *)"value", 5));
}

void test_tal_kv_get_success(void)
{
    uint8_t *value = NULL;
    size_t length = 0;

    tal_mutex_lock_IgnoreAndReturn(OPRT_OK);
    lfs_file_open_ExpectAnyArgsAndReturn(LFS_ERR_OK);
    lfs_file_size_ExpectAnyArgsAndReturn(10);
    tal_malloc_ExpectAnyArgsAndReturn((void *)malloc(11));
    lfs_file_read_ExpectAnyArgsAndReturn(10);
    lfs_file_close_ExpectAnyArgsAndReturn(0);
    tal_mutex_unlock_IgnoreAndReturn(OPRT_OK);

    tal_aes128_cbc_decode_StubWithCallback(tal_aes128_cbc_decode_cb);
    tal_aes_get_actual_length_ExpectAnyArgsAndReturn(5);
    tal_free_Ignore();

    TEST_ASSERT_EQUAL(OPRT_OK, tal_kv_get("key", &value, &length));
    TEST_ASSERT_EQUAL(5, (int)length);
    TEST_ASSERT_EQUAL_STRING("hello", (char *)value);
}

void test_tal_kv_del_success(void)
{
    tal_mutex_lock_IgnoreAndReturn(OPRT_OK);
    lfs_remove_ExpectAnyArgsAndReturn(LFS_ERR_OK);
    tal_mutex_unlock_IgnoreAndReturn(OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_kv_del("key"));
}
