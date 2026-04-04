/**
 * @file xiaozhi_system.c
 * @brief Device id / client id helpers.
 */

#include "xiaozhi_system.h"

#include "xiaozhi_identity.h"
#include "netmgr.h"
#include "tal_api.h"
#include "xiaozhi_settings.h"

#include <stdio.h>
#include <string.h>

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.1.0"
#endif

#ifndef PLATFORM_BOARD
#define PLATFORM_BOARD "xiaozhi"
#endif

static void xz_generate_uuid_v4(char *out, size_t out_size)
{
    if (!out || out_size < 37) {
        return;
    }

    uint8_t b[16] = {0};
    for (int i = 0; i < 16; ++i) {
        b[i] = (uint8_t)(tal_system_get_random(0xFF) & 0xFF);
    }
    b[6] = (uint8_t)((b[6] & 0x0F) | 0x40);
    b[8] = (uint8_t)((b[8] & 0x3F) | 0x80);

    (void)snprintf(out, out_size, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0], b[1],
                   b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

OPERATE_RET xiaozhi_system_get_device_id(char *buf, size_t buf_size)
{
    if (!buf || buf_size < 18) {
        return OPRT_INVALID_PARM;
    }

    NW_MAC_S    mac = {0};
    OPERATE_RET rt  = netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_MAC, &mac);
    if (rt == OPRT_OK) {
        (void)snprintf(buf, buf_size, "%02x:%02x:%02x:%02x:%02x:%02x", mac.mac[0], mac.mac[1], mac.mac[2], mac.mac[3],
                       mac.mac[4], mac.mac[5]);
        (void)xiaozhi_settings_set_string(XZ_NS_SYS, "device_id", buf);
        return OPRT_OK;
    }

    char stored[18] = {0};
    if (xiaozhi_settings_get_string(XZ_NS_SYS, "device_id", stored, sizeof(stored), "") == OPRT_OK &&
        stored[0] != '\0') {
        (void)snprintf(buf, buf_size, "%s", stored);
        return OPRT_OK;
    }

    uint8_t fallback[6] = {0};
    for (size_t i = 0; i < CNTSOF(fallback); ++i) {
        fallback[i] = (uint8_t)(tal_system_get_random(0xFF) & 0xFF);
    }
    if (!xz_identity_select_device_id(NULL, fallback, buf, buf_size)) {
        return OPRT_COM_ERROR;
    }
    (void)xiaozhi_settings_set_string(XZ_NS_SYS, "device_id", buf);
    return OPRT_COM_ERROR;
}

OPERATE_RET xiaozhi_system_get_client_id(char *buf, size_t buf_size)
{
    if (!buf || buf_size < 37) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = xiaozhi_settings_get_string(XZ_NS_SYS, "client_id", buf, buf_size, "");
    if (rt == OPRT_OK && buf[0] != '\0') {
        return OPRT_OK;
    }

    xz_generate_uuid_v4(buf, buf_size);
    return xiaozhi_settings_set_string(XZ_NS_SYS, "client_id", buf);
}

OPERATE_RET xiaozhi_system_get_user_agent(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    (void)snprintf(buf, buf_size, "%s/%s", PLATFORM_BOARD, PROJECT_VERSION);
    return OPRT_OK;
}
