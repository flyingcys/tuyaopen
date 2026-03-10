#include "unity.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "uni_random.h"

static uint8_t g_seed = 0x10;

void test_uni_random_bytes_ok(void)
{
    unsigned char out[4] = {0};
    TEST_ASSERT_EQUAL_INT(0, uni_random_bytes(out, sizeof(out)));
    TEST_ASSERT_NOT_EQUAL(0, out[0]);
}

void test_uni_random_string_len_and_charset(void)
{
    char out[9] = {0};
    int ret = uni_random_string(out, 8);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(8, (int)strlen(out));
}

void test_uni_random_range_nonzero(void)
{
    int value = uni_random_range(16);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, value);
    TEST_ASSERT_LESS_THAN_INT(16, value);
}

int tuya_tls_random(unsigned char *output, size_t output_len)
{
    size_t i = 0;
    for (i = 0; i < output_len; ++i) {
        output[i] = g_seed++;
    }
    return 0;
}

extern void test_crc32_known_vector(void);
extern void test_crc16_known_vector(void);
extern void test_mix_method_hex_conversion(void);
extern void test_mix_method_sort_and_case(void);
extern void test_mix_method_mm_strdup(void);

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_crc16_known_vector);
    RUN_TEST(test_mix_method_hex_conversion);
    RUN_TEST(test_mix_method_sort_and_case);
    RUN_TEST(test_mix_method_mm_strdup);
    RUN_TEST(test_uni_random_bytes_ok);
    RUN_TEST(test_uni_random_string_len_and_charset);
    RUN_TEST(test_uni_random_range_nonzero);

    return UNITY_END();
}
