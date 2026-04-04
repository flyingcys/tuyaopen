#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "xiaozhi_identity.h"

static void test_generate_subscribe_topic_from_publish_topic(void)
{
    char out[160] = {0};

    assert(xz_identity_resolve_subscribe_topic("device-server", "null", "02:11:22:33:44:55", out, sizeof(out)));
    assert(strcmp(out, "device-server/p2p/GID_test@@@02_11_22_33_44_55") == 0);
}

static void test_keep_explicit_subscribe_topic(void)
{
    char out[160] = {0};

    assert(xz_identity_resolve_subscribe_topic("device-server", "custom/topic", "02:11:22:33:44:55", out, sizeof(out)));
    assert(strcmp(out, "custom/topic") == 0);
}

static void test_require_publish_topic_when_subscribe_topic_is_missing(void)
{
    char out[160] = {0};

    assert(!xz_identity_resolve_subscribe_topic("", "", "02:11:22:33:44:55", out, sizeof(out)));
}

int main(void)
{
    test_generate_subscribe_topic_from_publish_topic();
    test_keep_explicit_subscribe_topic();
    test_require_publish_topic_when_subscribe_topic_is_missing();
    puts("test_xiaozhi_mqtt_topic: PASS");
    return 0;
}
