#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "crc32i.h"
#include "crc_16.h"
#include "mix_method.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void *tal_malloc(size_t size)
{
    return malloc(size);
}

void test_crc32_known_vector(void)
{
    const unsigned char data[] = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, hash_crc32i_total(data, 9));
}

void test_crc16_known_vector(void)
{
    unsigned char data[] = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x4B37u, get_crc_16(data, 9));
}

void test_mix_method_hex_conversion(void)
{
    unsigned char hex[2] = {0};
    unsigned char src[] = "1A2b";
    unsigned char out[5] = {0};

    ascs2hex(hex, src, 4);
    TEST_ASSERT_EQUAL_HEX8(0x1A, hex[0]);
    TEST_ASSERT_EQUAL_HEX8(0x2B, hex[1]);

    hex2str(out, hex, 2);
    TEST_ASSERT_EQUAL_STRING("1A2B", (char *)out);
}

void test_mix_method_sort_and_case(void)
{
    unsigned char buf[] = {3, 1, 2};
    byte_sort(1, buf, 3);
    TEST_ASSERT_EQUAL_UINT8(1, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(2, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(3, buf[2]);

    TEST_ASSERT_EQUAL('a', tuya_tolower('A'));
    TEST_ASSERT_EQUAL('A', tuya_toupper('a'));
}

void test_mix_method_mm_strdup(void)
{
    char *dup = mm_strdup("abc");
    TEST_ASSERT_NOT_NULL(dup);
    TEST_ASSERT_EQUAL_STRING("abc", dup);
    free(dup);
}

int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen)
{
    (void)dlen;
    if (slen == 5 && memcmp(src, "hello", 5) == 0) {
        memcpy(dst, "aGVsbG8=", 9);
        *olen = 8;
        return 0;
    }
    *olen = 0;
    return -1;
}

int mbedtls_base64_decode(unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen)
{
    (void)dlen;
    if (slen == 8 && memcmp(src, "aGVsbG8=", 8) == 0) {
        memcpy(dst, "hello", 6);
        *olen = 5;
        return 0;
    }
    *olen = 0;
    return -1;
}
