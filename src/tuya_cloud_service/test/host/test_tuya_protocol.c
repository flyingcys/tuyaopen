#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "tuya_error_code.h"
#include "tuya_protocol.h"
#include "unity.h"

uint32_t tuya_pack_protocol_serial_no(void);

TIME_T tal_time_get_posix(void)
{
    return 1700000000;
}

int uni_random_bytes(unsigned char *output, size_t output_len)
{
    if (output != NULL) {
        memset(output, 0x11, output_len);
    }
    return 0;
}

int uni_random_string(char *dst, int size)
{
    if (dst == NULL || size <= 0) {
        return -1;
    }

    memset(dst, 'a', (size_t)size);
    return 0;
}

int uni_random_range(unsigned int range)
{
    (void)range;
    return 1;
}

int mbedtls_cipher_auth_encrypt_wrapper(const cipher_params_t *input, unsigned char *output, size_t *olen,
                                        unsigned char *tag, size_t tag_len)
{
    (void)input;
    (void)output;
    (void)olen;
    (void)tag;
    (void)tag_len;
    return OPRT_OK;
}

int mbedtls_cipher_auth_decrypt_wrapper(const cipher_params_t *input, unsigned char *output, size_t *olen,
                                        unsigned char *tag, size_t tag_len)
{
    (void)input;
    (void)output;
    (void)olen;
    (void)tag;
    (void)tag_len;
    return OPRT_OK;
}

void *tal_malloc(size_t size)
{
    return malloc(size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

void setUp(void) {}

void tearDown(void) {}

void test_tuya_parse_protocol_data_rejects_null_out_data(void)
{
    uint8_t data[16] = {'3', '5', '0'};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM,
                      tuya_parse_protocol_data(DP_CMD_LAN, data, sizeof(data), "1234567890123456", NULL));
}

void test_tuya_pack_protocol_serial_no_increments(void)
{
    uint32_t first  = tuya_pack_protocol_serial_no();
    uint32_t second = tuya_pack_protocol_serial_no();

    TEST_ASSERT_EQUAL_UINT32(first + 1, second);
}

void test_tuya_pack_protocol_data_rejects_invalid_command(void)
{
    char    *out     = NULL;
    uint32_t out_len = 0;

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_pack_protocol_data((DP_CMD_TYPE_E)0xff, "{}", 5,
                                                              (uint8_t *)"1234567890123456", &out, &out_len));
}

void test_lpv35_frame_buffer_size_get_returns_zero_for_null(void)
{
    TEST_ASSERT_EQUAL_INT(0, lpv35_frame_buffer_size_get(NULL));
}

void test_lpv35_frame_buffer_size_get_matches_payload_length(void)
{
    lpv35_frame_object_t frame = {
        .data_len = 8,
    };

    TEST_ASSERT_EQUAL_INT(LPV35_FRAME_MINI_SIZE + 8, lpv35_frame_buffer_size_get(&frame));
}

void test_lpv35_frame_serialize_rejects_null_input(void)
{
    uint8_t output[64] = {0};
    int     olen       = 0;

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM,
                      lpv35_frame_serialize((const uint8_t *)"1234567890123456", 16, NULL, output, &olen));
}

void test_lpv35_frame_parse_rejects_invalid_head_tail(void)
{
    uint8_t              input[LPV35_FRAME_MINI_SIZE] = {0};
    lpv35_frame_object_t output                       = {0};

    TEST_ASSERT_EQUAL(OPRT_VERSION_FMT_ERR,
                      lpv35_frame_parse((const uint8_t *)"1234567890123456", 16, input, sizeof(input), &output));
}

void test_tuya_parse_protocol_data_rejects_invalid_command(void)
{
    uint8_t data[16] = {'3', '5', '0'};
    char   *out_data = NULL;

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR,
                      tuya_parse_protocol_data((DP_CMD_TYPE_E)0xff, data, sizeof(data), "1234567890123456", &out_data));
}

void test_lpv35_frame_serialize_rejects_null_output(void)
{
    lpv35_frame_object_t frame = {
        .data     = (uint8_t *)"abc",
        .data_len = 3,
    };
    int olen = 0;

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM,
                      lpv35_frame_serialize((const uint8_t *)"1234567890123456", 16, &frame, NULL, &olen));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_parse_protocol_data_rejects_null_out_data);
    RUN_TEST(test_tuya_pack_protocol_serial_no_increments);
    RUN_TEST(test_tuya_pack_protocol_data_rejects_invalid_command);
    RUN_TEST(test_lpv35_frame_buffer_size_get_returns_zero_for_null);
    RUN_TEST(test_lpv35_frame_buffer_size_get_matches_payload_length);
    RUN_TEST(test_lpv35_frame_serialize_rejects_null_input);
    RUN_TEST(test_lpv35_frame_parse_rejects_invalid_head_tail);
    RUN_TEST(test_tuya_parse_protocol_data_rejects_invalid_command);
    RUN_TEST(test_lpv35_frame_serialize_rejects_null_output);
    return UNITY_END();
}
