#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xiaozhi_protocol.h"

uint32_t tal_system_get_millisecond(void)
{
    return 1234;
}

void *tal_malloc(size_t size)
{
    return malloc(size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

static void test_build_listen_detect_contains_text_without_mode(void)
{
    char *json = xz_build_listen("sid-1", "detect", NULL, "echo");
    assert(json != NULL);
    assert(strstr(json, "\"type\":\"listen\"") != NULL);
    assert(strstr(json, "\"state\":\"detect\"") != NULL);
    assert(strstr(json, "\"text\":\"echo\"") != NULL);
    assert(strstr(json, "\"mode\"") == NULL);
    tal_free(json);
}

static void test_build_abort_includes_reason(void)
{
    char *json = xz_build_abort("sid-2", "wake_word_detected");
    assert(json != NULL);
    assert(strstr(json, "\"type\":\"abort\"") != NULL);
    assert(strstr(json, "\"reason\":\"wake_word_detected\"") != NULL);
    tal_free(json);
}

int main(void)
{
    test_build_listen_detect_contains_text_without_mode();
    test_build_abort_includes_reason();

    puts("xiaozhi_protocol tests passed");
    return 0;
}
