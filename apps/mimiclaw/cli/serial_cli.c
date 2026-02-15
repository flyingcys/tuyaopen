#include "serial_cli.h"

#include "llm/llm_proxy.h"
#include "memory/memory_store.h"
#include "memory/session_mgr.h"
#include "mimi_config.h"
#include "proxy/http_proxy.h"
#include "tal_cli.h"
#include "telegram/telegram_bot.h"
#include "tools/tool_web_search.h"
#include "wifi/wifi_manager.h"

#include <stdarg.h>

static const char *TAG = "cli";
static bool s_cli_inited = false;

static void cli_echof(const char *fmt, ...)
{
    char line[512] = {0};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    tal_cli_echo(line);
}

static void mask_copy(const char *src, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    if (!src || src[0] == '\0') {
        snprintf(out, out_size, "(empty)");
        return;
    }

    size_t len = strlen(src);
    if (len <= 4) {
        snprintf(out, out_size, "****");
    } else {
        snprintf(out, out_size, "%.4s****", src);
    }
}

static void print_config_item(const char *label, const char *ns, const char *key,
                              const char *build_val, bool mask)
{
    char kv_val[128] = {0};
    const char *src = "not set";
    const char *val = "(empty)";

    if (mimi_kv_get_string(ns, key, kv_val, sizeof(kv_val)) == OPRT_OK && kv_val[0] != '\0') {
        src = "kv";
        val = kv_val;
    } else if (build_val && build_val[0] != '\0') {
        src = "build";
        val = build_val;
    }

    char masked[64] = {0};
    if (mask) {
        mask_copy(val, masked, sizeof(masked));
        cli_echof("%-14s: %s [%s]", label, masked, src);
    } else {
        cli_echof("%-14s: %s [%s]", label, val, src);
    }
}

static void cmd_wifi_set(int argc, char *argv[])
{
    if (argc < 3) {
        cli_echof("usage: wifi_set <ssid> <password>");
        return;
    }

    OPERATE_RET rt = wifi_manager_set_credentials(argv[1], argv[2]);
    cli_echof("wifi_set rt=%d", rt);
}

static void cmd_wifi_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("connected: %s", wifi_manager_is_connected() ? "yes" : "no");
    cli_echof("ip: %s", wifi_manager_get_ip());
}

static void cmd_wifi_scan(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    wifi_manager_scan_and_print();
}

static void cmd_set_tg_token(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_tg_token <token>");
        return;
    }
    OPERATE_RET rt = telegram_set_token(argv[1]);
    cli_echof("set_tg_token rt=%d", rt);
}

static void cmd_set_api_key(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_api_key <api_key>");
        return;
    }
    OPERATE_RET rt = llm_set_api_key(argv[1]);
    cli_echof("set_api_key rt=%d", rt);
}

static void cmd_set_model(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_model <model>");
        return;
    }
    OPERATE_RET rt = llm_set_model(argv[1]);
    cli_echof("set_model rt=%d", rt);
}

static void cmd_set_model_provider(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_model_provider <provider>");
        return;
    }
    OPERATE_RET rt = llm_set_provider(argv[1]);
    cli_echof("set_model_provider rt=%d", rt);
}

static void cmd_set_proxy(int argc, char *argv[])
{
    if (argc < 3) {
        cli_echof("usage: set_proxy <host> <port>");
        return;
    }

    long port = strtol(argv[2], NULL, 10);
    if (port <= 0 || port > 65535) {
        cli_echof("invalid port: %s", argv[2]);
        return;
    }

    OPERATE_RET rt = http_proxy_set(argv[1], (uint16_t)port);
    cli_echof("set_proxy rt=%d", rt);
}

static void cmd_clear_proxy(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    OPERATE_RET rt = http_proxy_clear();
    cli_echof("clear_proxy rt=%d", rt);
}

static void cmd_set_search_key(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_search_key <api_key>");
        return;
    }
    OPERATE_RET rt = tool_web_search_set_key(argv[1]);
    cli_echof("set_search_key rt=%d", rt);
}

static void cmd_memory_read(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    char buf[4096] = {0};
    OPERATE_RET rt = memory_read_long_term(buf, sizeof(buf));
    if (rt != OPRT_OK || buf[0] == '\0') {
        cli_echof("memory empty rt=%d", rt);
        return;
    }

    cli_echof("=== MEMORY.md ===");
    cli_echof("%s", buf);
    cli_echof("=================");
}

static void cmd_memory_write(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: memory_write <content>");
        return;
    }

    char content[2048] = {0};
    size_t off = 0;
    for (int i = 1; i < argc && off < sizeof(content) - 1; i++) {
        int n = snprintf(content + off, sizeof(content) - off, "%s%s", (i == 1) ? "" : " ", argv[i]);
        if (n < 0 || (size_t)n >= sizeof(content) - off) {
            break;
        }
        off += (size_t)n;
    }

    OPERATE_RET rt = memory_write_long_term(content);
    cli_echof("memory_write rt=%d", rt);
}

static void cmd_session_list(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    session_list();
    cli_echof("session_list done");
}

static void cmd_session_clear(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: session_clear <chat_id>");
        return;
    }

    OPERATE_RET rt = session_clear(argv[1]);
    cli_echof("session_clear rt=%d", rt);
}

static void cmd_heap_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("free heap: %d", tal_system_get_free_heap_size());
}

static void cmd_config_show(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("=== Current Configuration ===");
    print_config_item("WiFi SSID", MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID, MIMI_SECRET_WIFI_SSID, false);
    print_config_item("WiFi Pass", MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS, MIMI_SECRET_WIFI_PASS, true);
    print_config_item("TG Token", MIMI_NVS_TG, MIMI_NVS_KEY_TG_TOKEN, MIMI_SECRET_TG_TOKEN, true);
    print_config_item("API Key", MIMI_NVS_LLM, MIMI_NVS_KEY_API_KEY, MIMI_SECRET_API_KEY, true);
    print_config_item("Model", MIMI_NVS_LLM, MIMI_NVS_KEY_MODEL, MIMI_SECRET_MODEL, false);
    print_config_item("Provider", MIMI_NVS_LLM, MIMI_NVS_KEY_PROVIDER, MIMI_SECRET_MODEL_PROVIDER, false);
    print_config_item("Proxy Host", MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_HOST, MIMI_SECRET_PROXY_HOST, false);
    print_config_item("Proxy Port", MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_PORT, MIMI_SECRET_PROXY_PORT, false);
    print_config_item("Search Key", MIMI_NVS_SEARCH, MIMI_NVS_KEY_API_KEY, MIMI_SECRET_SEARCH_KEY, true);
    cli_echof("=============================");
}

static void cmd_config_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    (void)mimi_kv_del(MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID);
    (void)mimi_kv_del(MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS);
    (void)mimi_kv_del(MIMI_NVS_TG, MIMI_NVS_KEY_TG_TOKEN);
    (void)mimi_kv_del(MIMI_NVS_LLM, MIMI_NVS_KEY_API_KEY);
    (void)mimi_kv_del(MIMI_NVS_LLM, MIMI_NVS_KEY_MODEL);
    (void)mimi_kv_del(MIMI_NVS_LLM, MIMI_NVS_KEY_PROVIDER);
    (void)mimi_kv_del(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_HOST);
    (void)mimi_kv_del(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_PORT);
    (void)mimi_kv_del(MIMI_NVS_SEARCH, MIMI_NVS_KEY_API_KEY);
    cli_echof("config reset done");
}

static const cli_cmd_t s_mimi_cli_cmds[] = {
    {.name = "wifi_set", .help = "wifi_set <ssid> <password>", .func = cmd_wifi_set},
    {.name = "wifi_status", .help = "show wifi status", .func = cmd_wifi_status},
    {.name = "wifi_scan", .help = "scan wifi ap", .func = cmd_wifi_scan},
    {.name = "set_tg_token", .help = "set_tg_token <token>", .func = cmd_set_tg_token},
    {.name = "set_api_key", .help = "set_api_key <api_key>", .func = cmd_set_api_key},
    {.name = "set_model", .help = "set_model <model>", .func = cmd_set_model},
    {.name = "set_model_provider", .help = "set_model_provider <provider>", .func = cmd_set_model_provider},
    {.name = "set_proxy", .help = "set_proxy <host> <port>", .func = cmd_set_proxy},
    {.name = "clear_proxy", .help = "clear proxy config", .func = cmd_clear_proxy},
    {.name = "set_search_key", .help = "set_search_key <api_key>", .func = cmd_set_search_key},
    {.name = "memory_read", .help = "read MEMORY.md", .func = cmd_memory_read},
    {.name = "memory_write", .help = "memory_write <content>", .func = cmd_memory_write},
    {.name = "session_list", .help = "list sessions", .func = cmd_session_list},
    {.name = "session_clear", .help = "session_clear <chat_id>", .func = cmd_session_clear},
    {.name = "heap_info", .help = "show free heap", .func = cmd_heap_info},
    {.name = "config_show", .help = "show current config source", .func = cmd_config_show},
    {.name = "config_reset", .help = "clear all mimiclaw kv config", .func = cmd_config_reset},
};

OPERATE_RET serial_cli_init(void)
{
    if (s_cli_inited) {
        return OPRT_OK;
    }

    OPERATE_RET rt = tal_cli_init();
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "tal_cli_init failed: %d", rt);
        return rt;
    }

    rt = tal_cli_cmd_register(s_mimi_cli_cmds, sizeof(s_mimi_cli_cmds) / sizeof(s_mimi_cli_cmds[0]));
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "tal_cli_cmd_register failed: %d", rt);
        return rt;
    }

    s_cli_inited = true;
    MIMI_LOGI(TAG, "serial cli initialized, cmds=%u", (unsigned)(sizeof(s_mimi_cli_cmds) / sizeof(s_mimi_cli_cmds[0])));
    return OPRT_OK;
}
