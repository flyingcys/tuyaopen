#include "wifi_manager.h"

#include "mimi_config.h"

static const char *TAG = "wifi";

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)

#include "netconn_wifi.h"
#include "netmgr.h"
#include "tal_wifi.h"

#ifndef WIFI_SSID_LEN
#define WIFI_SSID_LEN 32
#endif

#ifndef WIFI_PASSWD_LEN
#define WIFI_PASSWD_LEN 64
#endif

static bool s_connected = false;
static bool s_netmgr_ready = false;
static char s_ip_str[40] = "0.0.0.0";

static OPERATE_RET ensure_netmgr_ready(void)
{
    if (s_netmgr_ready) {
        return OPRT_OK;
    }

    netmgr_status_e status = NETMGR_LINK_DOWN;
    OPERATE_RET rt = netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    if (rt == OPRT_OK || rt == OPRT_NOT_SUPPORTED || rt == OPRT_TIMEOUT) {
        s_netmgr_ready = true;
        return OPRT_OK;
    }

    // follow TuyaOpen demos: init only enabled net types
    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
    type |= NETCONN_CELLULAR;
#endif
    if (type == 0) {
        return OPRT_NOT_SUPPORTED;
    }

    rt = netmgr_init(type);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "netmgr_init failed: %d", rt);
        return rt;
    }

    s_netmgr_ready = true;
    return OPRT_OK;
}

static void update_ip_from_netmgr(void)
{
    NW_IP_S ip = {0};
    if (netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_IP, &ip) != OPRT_OK) {
        snprintf(s_ip_str, sizeof(s_ip_str), "0.0.0.0");
        return;
    }

#ifdef nwipstr
    snprintf(s_ip_str, sizeof(s_ip_str), "%s", ip.nwipstr);
#else
    snprintf(s_ip_str, sizeof(s_ip_str), "%s", ip.ip);
#endif
}

OPERATE_RET wifi_manager_init(void)
{
    s_connected = false;
    snprintf(s_ip_str, sizeof(s_ip_str), "0.0.0.0");

    OPERATE_RET rt = ensure_netmgr_ready();
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "wifi manager init degrade: %d", rt);
    }

    return OPRT_OK;
}

OPERATE_RET wifi_manager_start(void)
{
    char ssid[WIFI_SSID_LEN + 1] = {0};
    char pass[WIFI_PASSWD_LEN + 1] = {0};

    (void)mimi_kv_get_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID, ssid, sizeof(ssid));
    (void)mimi_kv_get_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS, pass, sizeof(pass));

    if (ssid[0] == '\0' && MIMI_SECRET_WIFI_SSID[0] != '\0') {
        snprintf(ssid, sizeof(ssid), "%s", MIMI_SECRET_WIFI_SSID);
        snprintf(pass, sizeof(pass), "%s", MIMI_SECRET_WIFI_PASS);
    }

    if (ssid[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    OPERATE_RET rt = ensure_netmgr_ready();
    if (rt != OPRT_OK) {
        return rt;
    }

    netconn_wifi_info_t wifi_info = {0};
    strncpy(wifi_info.ssid, ssid, sizeof(wifi_info.ssid) - 1);
    strncpy(wifi_info.pswd, pass, sizeof(wifi_info.pswd) - 1);

    rt = netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "set wifi credentials to netmgr failed: %d", rt);
        return rt;
    }

    rt = netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_CLOSE, NULL);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "trigger wifi reconnect failed: %d", rt);
    }

    return OPRT_OK;
}

OPERATE_RET wifi_manager_wait_connected(uint32_t timeout_ms)
{
    OPERATE_RET rt = ensure_netmgr_ready();
    if (rt != OPRT_OK) {
        return rt;
    }

    uint64_t start = tal_time_get_posix_ms();
    while (1) {
        netmgr_status_e status = NETMGR_LINK_DOWN;
        rt = netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
        if (rt == OPRT_OK && status == NETMGR_LINK_UP) {
            s_connected = true;
            update_ip_from_netmgr();
            return OPRT_OK;
        }

        if (timeout_ms != UINT32_MAX) {
            uint64_t now = tal_time_get_posix_ms();
            if (now > start && (now - start) >= timeout_ms) {
                s_connected = false;
                snprintf(s_ip_str, sizeof(s_ip_str), "0.0.0.0");
                return OPRT_TIMEOUT;
            }
        }

        tal_system_sleep(200);
    }
}

bool wifi_manager_is_connected(void)
{
    if (ensure_netmgr_ready() == OPRT_OK) {
        netmgr_status_e status = NETMGR_LINK_DOWN;
        if (netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status) == OPRT_OK) {
            s_connected = (status == NETMGR_LINK_UP);
            if (s_connected) {
                update_ip_from_netmgr();
            }
        }
    }

    return s_connected;
}

const char *wifi_manager_get_ip(void)
{
    if (wifi_manager_is_connected()) {
        return s_ip_str;
    }

    return "0.0.0.0";
}

OPERATE_RET wifi_manager_set_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = mimi_kv_set_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID, ssid);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = mimi_kv_set_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS, password);
    if (rt != OPRT_OK) {
        return rt;
    }

    return OPRT_OK;
}

void wifi_manager_scan_and_print(void)
{
    OPERATE_RET rt = ensure_netmgr_ready();
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "scan unavailable, netmgr not ready: %d", rt);
        return;
    }

    AP_IF_S *ap_list = NULL;
    uint32_t ap_num = 0;

    rt = tal_wifi_all_ap_scan(&ap_list, &ap_num);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "wifi scan failed: %d", rt);
        return;
    }

    MIMI_LOGI(TAG, "wifi scan found %u ap(s)", (unsigned)ap_num);

    for (uint32_t i = 0; i < ap_num; i++) {
        AP_IF_S *ap = &ap_list[i];
        char ssid[WIFI_SSID_LEN + 1] = {0};
        size_t ssid_len = ap->s_len;
        if (ssid_len == 0) {
            ssid_len = strnlen((const char *)ap->ssid, WIFI_SSID_LEN);
        }
        if (ssid_len > WIFI_SSID_LEN) {
            ssid_len = WIFI_SSID_LEN;
        }
        if (ssid_len > 0) {
            memcpy(ssid, ap->ssid, ssid_len);
            ssid[ssid_len] = '\0';
        }

        char bssid[18] = {0};
        snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X", ap->bssid[0], ap->bssid[1], ap->bssid[2],
                 ap->bssid[3], ap->bssid[4], ap->bssid[5]);

        MIMI_LOGI(TAG, "ap[%u] ssid=%s ch=%u rssi=%d sec=%u bssid=%s", (unsigned)i, ssid[0] ? ssid : "<hidden>",
                  (unsigned)ap->channel, (int)ap->rssi, (unsigned)ap->security, bssid);
    }

    (void)tal_wifi_release_ap(ap_list);
}

#else

OPERATE_RET wifi_manager_init(void)
{
    MIMI_LOGW(TAG, "wifi disabled (ENABLE_WIFI!=1)");
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET wifi_manager_start(void)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET wifi_manager_wait_connected(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return OPRT_NOT_SUPPORTED;
}

bool wifi_manager_is_connected(void)
{
    return false;
}

const char *wifi_manager_get_ip(void)
{
    return "0.0.0.0";
}

OPERATE_RET wifi_manager_set_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return OPRT_INVALID_PARM;
    }
    // still allow persisting creds even if WiFi feature is off
    OPERATE_RET rt = mimi_kv_set_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID, ssid);
    if (rt != OPRT_OK) {
        return rt;
    }
    return mimi_kv_set_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS, password);
}

void wifi_manager_scan_and_print(void)
{
    MIMI_LOGW(TAG, "wifi scan disabled (ENABLE_WIFI!=1)");
}

#endif
