#include "mimi_base.h"

#include "agent/agent_loop.h"
#include "bus/message_bus.h"
#include "cli/serial_cli.h"
#include "gateway/ws_server.h"
#include "llm/llm_proxy.h"
#include "memory/memory_store.h"
#include "memory/session_mgr.h"
#include "mimi_config.h"
#include "proxy/http_proxy.h"
#include "telegram/telegram_bot.h"
#include "tools/tool_registry.h"
#include "wifi/wifi_manager.h"

#include "tal_fs.h"

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

static const char *TAG = "mimi";
static THREAD_HANDLE s_outbound_thread = NULL;

static OPERATE_RET ensure_dir(const char *path)
{
    BOOL_T exists = FALSE;
    if (tal_fs_is_exist(path, &exists) == OPRT_OK && exists) {
        return OPRT_OK;
    }

    OPERATE_RET rt = tal_fs_mkdir(path);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "mkdir failed: %s rt=%d", path, rt);
        return rt;
    }

    return OPRT_OK;
}

static OPERATE_RET init_storage(void)
{
    OPERATE_RET rt = ensure_dir(MIMI_SPIFFS_BASE);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = ensure_dir(MIMI_SPIFFS_CONFIG_DIR);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = ensure_dir(MIMI_SPIFFS_MEMORY_DIR);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = ensure_dir(MIMI_SPIFFS_SESSION_DIR);
    if (rt != OPRT_OK) {
        return rt;
    }

    return OPRT_OK;
}

static void outbound_dispatch_task(void *arg)
{
    (void)arg;

    MIMI_LOGI(TAG, "outbound dispatcher started");

    while (1) {
        mimi_msg_t msg = {0};
        if (message_bus_pop_outbound(&msg, MIMI_WAIT_FOREVER) != OPRT_OK) {
            continue;
        }

        if (strcmp(msg.channel, MIMI_CHAN_TELEGRAM) == 0) {
            (void)telegram_send_message(msg.chat_id, msg.content ? msg.content : "");
        } else if (strcmp(msg.channel, MIMI_CHAN_WEBSOCKET) == 0) {
            (void)ws_server_send(msg.chat_id, msg.content ? msg.content : "");
        } else {
            MIMI_LOGW(TAG, "unknown outbound channel: %s", msg.channel);
        }

        free(msg.content);
    }
}

static OPERATE_RET start_outbound_dispatcher(void)
{
    if (s_outbound_thread) {
        return OPRT_OK;
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth = MIMI_OUTBOUND_STACK;
    cfg.priority = THREAD_PRIO_1;
    cfg.thrdname = "mimi_outbound";

    return tal_thread_create_and_start(&s_outbound_thread, NULL, NULL,
                                       outbound_dispatch_task, NULL, &cfg);
}

void mimi_app_main(void)
{
    MIMI_LOGI(TAG, "MimiClaw TuyaOpen app start");
    MIMI_LOGI(TAG, "free heap: %d", tal_system_get_free_heap_size());

    (void)init_storage();
    (void)message_bus_init();
    (void)memory_store_init();
    (void)session_mgr_init();
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    (void)wifi_manager_init();
#endif
    (void)http_proxy_init();
    (void)telegram_bot_init();
    (void)llm_proxy_init();
    (void)tool_registry_init();
    (void)agent_loop_init();
    (void)serial_cli_init();

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    OPERATE_RET wifi_rt = wifi_manager_start();
    if (wifi_rt == OPRT_OK) {
        MIMI_LOGI(TAG, "scanning nearby APs on boot...");
        wifi_manager_scan_and_print();
        MIMI_LOGI(TAG, "waiting for WiFi connection...");

        if (wifi_manager_wait_connected(30000) == OPRT_OK) {
            MIMI_LOGI(TAG, "WiFi connected: %s", wifi_manager_get_ip());

            OPERATE_RET rt = telegram_bot_start();
            if (rt != OPRT_OK) {
                MIMI_LOGW(TAG, "telegram_bot_start failed: %d", rt);
            }

            rt = agent_loop_start();
            if (rt != OPRT_OK) {
                MIMI_LOGW(TAG, "agent_loop_start failed: %d", rt);
            }

            rt = ws_server_start();
            if (rt != OPRT_OK) {
                MIMI_LOGW(TAG, "ws_server_start failed: %d", rt);
            }

            rt = start_outbound_dispatcher();
            if (rt != OPRT_OK) {
                MIMI_LOGW(TAG, "start_outbound_dispatcher failed: %d", rt);
            }
        } else {
            MIMI_LOGW(TAG, "WiFi connection timeout. Check MIMI_SECRET_WIFI_SSID in mimi_secrets.h");
        }
    } else {
        if (wifi_rt == OPRT_NOT_FOUND) {
            MIMI_LOGW(TAG, "No WiFi credentials. Set MIMI_SECRET_WIFI_SSID in mimi_secrets.h");
        } else {
            MIMI_LOGW(TAG, "wifi_manager_start failed: %d", wifi_rt);
        }
    }
#else
    MIMI_LOGW(TAG, "ENABLE_WIFI disabled, skip WiFi connect and network services");
#endif

    MIMI_LOGI(TAG, "MimiClaw started");
}

int user_main(void)
{
    mimi_app_main();
    while (1) {
        tal_system_sleep(1000);
    }
    return 0;
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    return user_main();
}
#else
static THREAD_HANDLE s_main_thread = NULL;

static void mimi_main_thread(void *arg)
{
    (void)arg;
    user_main();
}

void tuya_app_main(void)
{
    THREAD_CFG_T cfg = {0};
    cfg.stackDepth = 1024 * 6;
    cfg.priority = THREAD_PRIO_1;
    cfg.thrdname = "mimi_main";
    (void)tal_thread_create_and_start(&s_main_thread, NULL, NULL, mimi_main_thread, NULL, &cfg);
}
#endif
