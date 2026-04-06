/**
 * @file cli_cmd.c
 * @brief CLI commands for xiaozhi app.
 */

#include "xiaozhi_app.h"
#include "xiaozhi_settings.h"

#include "tal_api.h"
#include "tal_cli.h"

#include <stdarg.h>
#include <stdlib.h>

#ifndef WIFI_SSID_LEN
#define WIFI_SSID_LEN 32
#endif

#ifndef WIFI_PASSWD_LEN
#define WIFI_PASSWD_LEN 64
#endif

extern void tal_kv_cmd(int argc, char *argv[]);
extern void netmgr_cmd(int argc, char *argv[]);

static BOOL_T s_cli_inited = FALSE;

static void cli_echof(const char *fmt, ...)
{
    char line[512] = {0};

    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    tal_cli_echo(line);
}

static void mask_copy(const char *src, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }

    if (!src || src[0] == '\0') {
        (void)snprintf(out, out_size, "(empty)");
        return;
    }

    size_t len = strlen(src);
    if (len <= 4) {
        (void)snprintf(out, out_size, "****");
    } else {
        (void)snprintf(out, out_size, "%.4s****", src);
    }
}

static void join_args(int argc, char *argv[], int start_idx, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }

    out[0]     = '\0';
    size_t off = 0;
    for (int i = start_idx; i < argc; ++i) {
        int n = snprintf(out + off, out_size - off, "%s%s", (i == start_idx) ? "" : " ", argv[i]);
        if (n <= 0 || (size_t)n >= out_size - off) {
            break;
        }
        off += (size_t)n;
    }
}

static void cmd_xz_status(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        cli_echof("usage: xz_status");
        return;
    }

    char status[256] = {0};
    (void)xiaozhi_app_get_status(status, sizeof(status));
    cli_echof("%s", status);
}

static void cmd_xz_start(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        cli_echof("usage: xz_start");
        return;
    }

    OPERATE_RET rt = xiaozhi_app_start();
    cli_echof("xz_start rt=%d", rt);
}

static void cmd_xz_stop(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        cli_echof("usage: xz_stop");
        return;
    }

    OPERATE_RET rt = xiaozhi_app_stop();
    cli_echof("xz_stop rt=%d", rt);
}

static void cmd_xz_reconnect(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        cli_echof("usage: xz_reconnect");
        return;
    }

    OPERATE_RET rt = xiaozhi_app_reconnect();
    cli_echof("xz_reconnect rt=%d", rt);
}

static void cmd_xz_proto(int argc, char *argv[])
{
    if (argc != 2) {
        cli_echof("usage: xz_proto <websocket|mqtt-udp>");
        return;
    }

    OPERATE_RET rt = xiaozhi_settings_set_proto(argv[1]);
    cli_echof("xz_proto set rt=%d", rt);
    if (rt == OPRT_OK) {
        (void)xiaozhi_app_reconnect();
    }
}

static void cmd_xz_ws(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: xz_ws <url|token|version> <value>");
        return;
    }

    if (argc == 2 && strcmp(argv[1], "show") == 0) {
        char url[320]   = {0};
        char token[256] = {0};
        char masked[64] = {0};
        int  version    = xiaozhi_settings_get_int(XZ_NS_WS, "version", 1);

        (void)xiaozhi_settings_get_string(XZ_NS_WS, "url", url, sizeof(url), "");
        (void)xiaozhi_settings_get_string(XZ_NS_WS, "token", token, sizeof(token), "");
        mask_copy(token, masked, sizeof(masked));

        cli_echof("ws.url=%s", url[0] ? url : "(empty)");
        cli_echof("ws.token=%s", masked);
        cli_echof("ws.version=%d", version);
        return;
    }

    if (argc < 3) {
        cli_echof("usage: xz_ws <url|token|version> <value>");
        return;
    }

    OPERATE_RET rt = OPRT_INVALID_PARM;
    if (strcmp(argv[1], "url") == 0 || strcmp(argv[1], "token") == 0) {
        rt = xiaozhi_settings_set_string(XZ_NS_WS, argv[1], argv[2]);
    } else if (strcmp(argv[1], "version") == 0) {
        rt = xiaozhi_settings_set_int(XZ_NS_WS, "version", atoi(argv[2]));
    }

    cli_echof("xz_ws set rt=%d", rt);
}

static void cmd_xz_mqtt(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: xz_mqtt <key> <value>");
        cli_echof("key: endpoint client_id username password publish_topic subscribe_topic keepalive");
        return;
    }

    if (argc == 2 && strcmp(argv[1], "show") == 0) {
        char endpoint[160]  = {0};
        char client_id[96]  = {0};
        char username[160]  = {0};
        char password[160]  = {0};
        char pub_topic[160] = {0};
        char sub_topic[160] = {0};
        char mask_user[64]  = {0};
        char mask_pwd[64]   = {0};

        (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "endpoint", endpoint, sizeof(endpoint), "");
        (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "client_id", client_id, sizeof(client_id), "");
        (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "username", username, sizeof(username), "");
        (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "password", password, sizeof(password), "");
        (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "publish_topic", pub_topic, sizeof(pub_topic), "");
        (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "subscribe_topic", sub_topic, sizeof(sub_topic), "");

        mask_copy(username, mask_user, sizeof(mask_user));
        mask_copy(password, mask_pwd, sizeof(mask_pwd));

        cli_echof("mqtt.endpoint=%s", endpoint[0] ? endpoint : "(empty)");
        cli_echof("mqtt.client_id=%s", client_id[0] ? client_id : "(empty)");
        cli_echof("mqtt.username=%s", mask_user);
        cli_echof("mqtt.password=%s", mask_pwd);
        cli_echof("mqtt.publish_topic=%s", pub_topic[0] ? pub_topic : "(empty)");
        cli_echof("mqtt.subscribe_topic=%s", sub_topic[0] ? sub_topic : "(empty)");
        cli_echof("mqtt.keepalive=%d", xiaozhi_settings_get_int(XZ_NS_MQTT, "keepalive", 240));
        return;
    }

    if (argc < 3) {
        cli_echof("usage: xz_mqtt <key> <value>");
        return;
    }

    OPERATE_RET rt = OPRT_INVALID_PARM;
    if (strcmp(argv[1], "endpoint") == 0 || strcmp(argv[1], "client_id") == 0 || strcmp(argv[1], "username") == 0 ||
        strcmp(argv[1], "password") == 0 || strcmp(argv[1], "publish_topic") == 0 ||
        strcmp(argv[1], "subscribe_topic") == 0) {
        rt = xiaozhi_settings_set_string(XZ_NS_MQTT, argv[1], argv[2]);
    } else if (strcmp(argv[1], "keepalive") == 0) {
        rt = xiaozhi_settings_set_int(XZ_NS_MQTT, "keepalive", atoi(argv[2]));
    }

    cli_echof("xz_mqtt set rt=%d", rt);
}

static void cmd_xz_sys(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: xz_sys <device_id|client_id|serial_number|activation_secret|show> [value]");
        return;
    }

    if (argc == 2 && strcmp(argv[1], "show") == 0) {
        char device_id[32]          = {0};
        char client_id[96]          = {0};
        char serial_number[96]      = {0};
        char activation_secret[128] = {0};
        char masked_secret[64]      = {0};

        (void)xiaozhi_settings_get_string(XZ_NS_SYS, "device_id", device_id, sizeof(device_id), "");
        (void)xiaozhi_settings_get_string(XZ_NS_SYS, "client_id", client_id, sizeof(client_id), "");
        (void)xiaozhi_settings_get_string(XZ_NS_SYS, "serial_number", serial_number, sizeof(serial_number), "");
        (void)xiaozhi_settings_get_string(XZ_NS_SYS, "activation_secret", activation_secret, sizeof(activation_secret),
                                          "");
        mask_copy(activation_secret, masked_secret, sizeof(masked_secret));

        cli_echof("sys.device_id=%s", device_id[0] ? device_id : "(empty)");
        cli_echof("sys.client_id=%s", client_id[0] ? client_id : "(empty)");
        cli_echof("sys.serial_number=%s", serial_number[0] ? serial_number : "(empty)");
        cli_echof("sys.activation_secret=%s", masked_secret);
        return;
    }

    if (argc < 3) {
        cli_echof("usage: xz_sys <device_id|client_id|serial_number|activation_secret> <value>");
        return;
    }

    OPERATE_RET rt = OPRT_INVALID_PARM;
    if (strcmp(argv[1], "device_id") == 0 || strcmp(argv[1], "client_id") == 0 ||
        strcmp(argv[1], "serial_number") == 0 || strcmp(argv[1], "activation_secret") == 0) {
        rt = xiaozhi_settings_set_string(XZ_NS_SYS, argv[1], argv[2]);
    }

    cli_echof("xz_sys set rt=%d", rt);
}

static void cmd_xz_wifi(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: xz_wifi <ssid|password|ota_url|apply|show> [value]");
        return;
    }

    if (strcmp(argv[1], "apply") == 0) {
        OPERATE_RET rt = xiaozhi_app_apply_wifi_settings();
        cli_echof("xz_wifi apply rt=%d", rt);
        return;
    }

    if (strcmp(argv[1], "show") == 0) {
        char ssid[WIFI_SSID_LEN + 1]   = {0};
        char pass[WIFI_PASSWD_LEN + 1] = {0};
        char ota_url[256]              = {0};
        char masked[64]                = {0};
        (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ssid", ssid, sizeof(ssid), "");
        (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "password", pass, sizeof(pass), "");
        (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ota_url", ota_url, sizeof(ota_url), XZ_DEFAULT_OTA_URL);
        mask_copy(pass, masked, sizeof(masked));

        cli_echof("wifi.ssid=%s", ssid[0] ? ssid : "(empty)");
        cli_echof("wifi.password=%s", masked);
        cli_echof("wifi.ota_url=%s", ota_url[0] ? ota_url : "(empty)");
        return;
    }

    if (argc < 3) {
        cli_echof("usage: xz_wifi <ssid|password|ota_url> <value>");
        return;
    }

    OPERATE_RET rt = OPRT_INVALID_PARM;
    if (strcmp(argv[1], "ssid") == 0 || strcmp(argv[1], "password") == 0 || strcmp(argv[1], "ota_url") == 0) {
        rt = xiaozhi_settings_set_string(XZ_NS_WIFI, argv[1], argv[2]);
        if (rt == OPRT_OK && (strcmp(argv[1], "ssid") == 0 || strcmp(argv[1], "password") == 0)) {
            (void)xiaozhi_app_apply_wifi_settings();
        }
    }

    cli_echof("xz_wifi set rt=%d", rt);
}

static void cmd_xz_listen(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: xz_listen <start|stop|detect> [mode] [text]");
        return;
    }

    OPERATE_RET rt = OPRT_INVALID_PARM;
    if (strcmp(argv[1], "start") == 0) {
        const char *mode = (argc >= 3) ? argv[2] : "manual";
        rt               = xiaozhi_app_send_listen("start", mode, NULL);
    } else if (strcmp(argv[1], "stop") == 0) {
        (void)xiaozhi_app_stop_detect();
        rt = xiaozhi_app_send_listen("stop", NULL, NULL);
    } else if (strcmp(argv[1], "detect") == 0) {
        rt = xiaozhi_app_start_detect();
    }

    cli_echof("xz_listen rt=%d", rt);
}

static void cmd_xz_abort(int argc, char *argv[])
{
    const char *reason = NULL;
    if (argc >= 2) {
        reason = argv[1];
    }

    OPERATE_RET rt = xiaozhi_app_send_abort(reason);
    cli_echof("xz_abort rt=%d", rt);
}

static void cmd_xz_mcp(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: xz_mcp <json_payload>");
        return;
    }

    char payload[512] = {0};
    join_args(argc, argv, 1, payload, sizeof(payload));

    OPERATE_RET rt = xiaozhi_app_send_mcp(payload);
    cli_echof("xz_mcp rt=%d", rt);
}

static void cmd_xz_ota(int argc, char *argv[])
{
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "check") == 0)) {
        OPERATE_RET rt = xiaozhi_app_ota_bootstrap();
        cli_echof("xz_ota check rt=%d", rt);
        return;
    }

    if (argc == 3 && strcmp(argv[1], "url") == 0) {
        OPERATE_RET rt = xiaozhi_settings_set_string(XZ_NS_WIFI, "ota_url", argv[2]);
        cli_echof("xz_ota url rt=%d", rt);
        return;
    }

    cli_echof("usage: xz_ota [check] | xz_ota url <url>");
}

static const cli_cmd_t s_xz_cli_cmds[] = {
    {.name = "xz_start", .func = cmd_xz_start, .help = "start xiaozhi worker"},
    {.name = "xz_stop", .func = cmd_xz_stop, .help = "stop xiaozhi worker"},
    {.name = "xz_status", .func = cmd_xz_status, .help = "show xiaozhi status"},
    {.name = "xz_reconnect", .func = cmd_xz_reconnect, .help = "trigger reconnect"},
    {.name = "xz_proto", .func = cmd_xz_proto, .help = "set protocol websocket|mqtt-udp"},
    {.name = "xz_ws", .func = cmd_xz_ws, .help = "set websocket config"},
    {.name = "xz_mqtt", .func = cmd_xz_mqtt, .help = "set mqtt config"},
    {.name = "xz_sys", .func = cmd_xz_sys, .help = "set system identity and activation config"},
    {.name = "xz_wifi", .func = cmd_xz_wifi, .help = "set wifi config"},
    {.name = "xz_ota", .func = cmd_xz_ota, .help = "ota bootstrap check"},
    {.name = "xz_listen", .func = cmd_xz_listen, .help = "send listen control"},
    {.name = "xz_abort", .func = cmd_xz_abort, .help = "send abort"},
    {.name = "xz_mcp", .func = cmd_xz_mcp, .help = "send mcp payload"},
    {.name = "kv", .func = tal_kv_cmd, .help = "kv command"},
    {.name = "netmgr", .func = netmgr_cmd, .help = "netmgr command"},
};

void tuya_app_cli_init(void)
{
    if (s_cli_inited) {
        return;
    }

    OPERATE_RET rt = tal_cli_init();
    if (rt != OPRT_OK) {
        PR_ERR("tal_cli_init failed: %d", rt);
        return;
    }

    rt = tal_cli_cmd_register(s_xz_cli_cmds, sizeof(s_xz_cli_cmds) / sizeof(s_xz_cli_cmds[0]));
    if (rt != OPRT_OK) {
        PR_ERR("tal_cli_cmd_register failed: %d", rt);
        return;
    }

    s_cli_inited = TRUE;
    PR_NOTICE("xiaozhi cli ready, cmd_count=%u", (unsigned)(sizeof(s_xz_cli_cmds) / sizeof(s_xz_cli_cmds[0])));
}
