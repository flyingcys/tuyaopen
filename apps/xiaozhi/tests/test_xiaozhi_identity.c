#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "xiaozhi_identity.h"

static void test_reuse_stored_device_id(void)
{
    const uint8_t fallback[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    char          out[18]     = {0};

    assert(xz_identity_select_device_id("aa:bb:cc:dd:ee:ff", fallback, out, sizeof(out)));
    assert(strcmp(out, "aa:bb:cc:dd:ee:ff") == 0);
}

static void test_generate_fallback_device_id(void)
{
    const uint8_t fallback[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    char          out[18]     = {0};

    assert(xz_identity_select_device_id(NULL, fallback, out, sizeof(out)));
    assert(strcmp(out, "00:11:22:33:44:55") == 0);
}

int main(void)
{
    test_reuse_stored_device_id();
    test_generate_fallback_device_id();
    puts("test_xiaozhi_identity: PASS");
    return 0;
}
