/**
 * @file xiaozhi_settings.c
 * @brief KV backed settings for xiaozhi app.
 */

#include "xiaozhi_settings.h"

#include "tal_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XZ_KEY_BUF_SIZE 96

static void xz_make_key(const char *ns, const char *key, char out[XZ_KEY_BUF_SIZE])
{
    if (!ns || !key) {
        out[0] = '\0';
        return;
    }
    (void)snprintf(out, XZ_KEY_BUF_SIZE, "%s.%s", ns, key);
}

OPERATE_RET xiaozhi_settings_set_string(const char *ns, const char *key, const char *value)
{
    if (!ns || !key || !value) {
        return OPRT_INVALID_PARM;
    }
    char full_key[XZ_KEY_BUF_SIZE] = {0};
    xz_make_key(ns, key, full_key);
    if (full_key[0] == '\0') {
        return OPRT_INVALID_PARM;
    }
    return tal_kv_set(full_key, (const uint8_t *)value, strlen(value) + 1);
}

OPERATE_RET xiaozhi_settings_get_string(const char *ns, const char *key, char *out, size_t out_size,
                                        const char *default_value)
{
    if (!ns || !key || !out || out_size == 0) {
        return OPRT_INVALID_PARM;
    }

    out[0]                         = '\0';
    char full_key[XZ_KEY_BUF_SIZE] = {0};
    xz_make_key(ns, key, full_key);
    if (full_key[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    uint8_t    *read_buf = NULL;
    size_t      read_len = 0;
    OPERATE_RET rt       = tal_kv_get(full_key, &read_buf, &read_len);
    if (rt == OPRT_OK && read_buf && read_len > 0) {
        (void)snprintf(out, out_size, "%s", (const char *)read_buf);
        tal_kv_free(read_buf);
        return OPRT_OK;
    }

    if (read_buf) {
        tal_kv_free(read_buf);
    }

    if (default_value) {
        (void)snprintf(out, out_size, "%s", default_value);
    }
    return rt;
}

OPERATE_RET xiaozhi_settings_set_int(const char *ns, const char *key, int value)
{
    char buf[16] = {0};
    (void)snprintf(buf, sizeof(buf), "%d", value);
    return xiaozhi_settings_set_string(ns, key, buf);
}

int xiaozhi_settings_get_int(const char *ns, const char *key, int default_value)
{
    char buf[16] = {0};
    if (xiaozhi_settings_get_string(ns, key, buf, sizeof(buf), NULL) != OPRT_OK || buf[0] == '\0') {
        return default_value;
    }
    return atoi(buf);
}

OPERATE_RET xiaozhi_settings_set_proto(const char *proto)
{
    if (!proto || proto[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    if (strcmp(proto, XZ_PROTOCOL_WEBSOCKET) != 0 && strcmp(proto, XZ_PROTOCOL_MQTT_UDP) != 0) {
        return OPRT_INVALID_PARM;
    }

    return xiaozhi_settings_set_string(XZ_NS_COMMON, "protocol", proto);
}

OPERATE_RET xiaozhi_settings_get_proto(char *proto, size_t proto_size)
{
    OPERATE_RET rt = xiaozhi_settings_get_string(XZ_NS_COMMON, "protocol", proto, proto_size, XZ_PROTOCOL_MQTT_UDP);
    if (proto[0] == '\0') {
        (void)snprintf(proto, proto_size, "%s", XZ_PROTOCOL_MQTT_UDP);
    }

    if (strcmp(proto, XZ_PROTOCOL_WEBSOCKET) != 0 && strcmp(proto, XZ_PROTOCOL_MQTT_UDP) != 0) {
        (void)snprintf(proto, proto_size, "%s", XZ_PROTOCOL_MQTT_UDP);
    }

    return rt;
}

OPERATE_RET xiaozhi_settings_init_defaults(void)
{
    char buf[160] = {0};

    (void)xiaozhi_settings_get_proto(buf, sizeof(buf));

    (void)xiaozhi_settings_get_string(XZ_NS_WS, "url", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_WS, "token", buf, sizeof(buf), "");
    if (xiaozhi_settings_get_int(XZ_NS_WS, "version", 0) == 0) {
        (void)xiaozhi_settings_set_int(XZ_NS_WS, "version", 1);
    }

    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "endpoint", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "client_id", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "username", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "password", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "publish_topic", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "subscribe_topic", buf, sizeof(buf), "");
    if (xiaozhi_settings_get_int(XZ_NS_MQTT, "keepalive", 0) == 0) {
        (void)xiaozhi_settings_set_int(XZ_NS_MQTT, "keepalive", 240);
    }

    (void)xiaozhi_settings_get_string(XZ_NS_SYS, "client_id", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_SYS, "vision_url", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_SYS, "vision_token", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_ASSETS, "download_url", buf, sizeof(buf), "");
    if (xiaozhi_settings_get_int(XZ_NS_ASSETS, "partition_valid", -1) < 0) {
        (void)xiaozhi_settings_set_int(XZ_NS_ASSETS, "partition_valid", 1);
    }
    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ssid", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "password", buf, sizeof(buf), "");
    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ota_url", buf, sizeof(buf), XZ_DEFAULT_OTA_URL);

    return OPRT_OK;
}
