/**
 * @file xiaozhi_identity.c
 * @brief 设备身份与 MQTT topic 纯逻辑辅助函数。
 */

#include "xiaozhi_identity.h"

#include <stdio.h>
#include <string.h>

static bool xz_identity_is_empty_or_null(const char *value)
{
    return (value == NULL || value[0] == '\0' || strcmp(value, "null") == 0 || strcmp(value, "NULL") == 0);
}

bool xz_identity_select_device_id(const char *stored_device_id, const uint8_t fallback_mac[6], char *out,
                                  size_t out_size)
{
    if (!out || out_size < 18) {
        return false;
    }

    if (stored_device_id && stored_device_id[0] != '\0') {
        (void)snprintf(out, out_size, "%s", stored_device_id);
        return true;
    }

    if (!fallback_mac) {
        out[0] = '\0';
        return false;
    }

    (void)snprintf(out, out_size, "%02x:%02x:%02x:%02x:%02x:%02x", fallback_mac[0], fallback_mac[1], fallback_mac[2],
                   fallback_mac[3], fallback_mac[4], fallback_mac[5]);
    return true;
}

bool xz_identity_resolve_subscribe_topic(const char *publish_topic, const char *subscribe_topic, const char *device_id,
                                         char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return false;
    }

    out[0] = '\0';
    if (!xz_identity_is_empty_or_null(subscribe_topic)) {
        (void)snprintf(out, out_size, "%s", subscribe_topic);
        return true;
    }

    if (!publish_topic || publish_topic[0] == '\0' || !device_id || device_id[0] == '\0') {
        return false;
    }

    char        prefix[160] = {0};
    const char *slash       = strchr(publish_topic, '/');
    size_t      prefix_len  = slash ? (size_t)(slash - publish_topic) : strlen(publish_topic);
    if (prefix_len == 0 || prefix_len >= sizeof(prefix)) {
        return false;
    }

    memcpy(prefix, publish_topic, prefix_len);
    prefix[prefix_len] = '\0';

    char   device_key[64] = {0};
    size_t device_len     = strlen(device_id);
    if (device_len == 0 || device_len >= sizeof(device_key)) {
        return false;
    }

    for (size_t i = 0; i < device_len; ++i) {
        device_key[i] = (device_id[i] == ':') ? '_' : device_id[i];
    }
    device_key[device_len] = '\0';

    int written = snprintf(out, out_size, "%s/p2p/GID_test@@@%s", prefix, device_key);
    return (written > 0 && (size_t)written < out_size);
}
