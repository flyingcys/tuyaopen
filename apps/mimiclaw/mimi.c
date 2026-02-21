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
#include "tuya_register_center.h"
#include "tuya_tls.h"
#include "wifi/wifi_manager.h"

#include "cJSON.h"
#include "netmgr.h"
#include "tal_fs.h"
#include "tkl_output.h"

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

static const char *TAG = "mimi";
static THREAD_HANDLE s_outbound_thread = NULL;

static void mimi_runtime_init(void)
{
    static bool s_inited = false;
    if (s_inited) {
        return;
    }

    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
    (void)tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    // LittleFS mount happens inside tal_kv_init(). We must call this before any tal_fs_* APIs.
    (void)tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });

    (void)tal_sw_timer_init();
    (void)tal_workq_init();
    (void)tuya_tls_init();
    (void)tuya_register_center_init();

    s_inited = true;
}

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

static void mimi_network_init(void)
{
    netmgr_type_e type = 0;
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
    type |= NETCONN_CELLULAR;
#endif

    if (type == 0) {
        MIMI_LOGI(TAG, "skip netmgr init: wifi handled by wifi_manager");
        return;
    }

    OPERATE_RET rt = netmgr_init(type);
    if (rt == OPRT_OK) {
        MIMI_LOGI(TAG, "netmgr initialized (non-wifi), type=0x%x", type);
    } else {
        MIMI_LOGW(TAG, "netmgr_init failed: %d", rt);
    }
}

static void start_online_services(const char *mode)
{
    OPERATE_RET rt = telegram_bot_start();
    if (rt == OPRT_NOT_FOUND) {
        MIMI_LOGW(TAG, "telegram token missing, telegram service disabled");
    } else if (rt != OPRT_OK) {
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

    MIMI_LOGI(TAG, "online services started in %s mode", mode ? mode : "unknown");
}

void mimi_app_main(void)
{
    mimi_runtime_init();

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

#if OPERATING_SYSTEM == SYSTEM_LINUX
    MIMI_LOGI(TAG, "serial CLI disabled on Linux host");
#else
    (void)serial_cli_init();
#endif

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif
    mimi_network_init();

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    OPERATE_RET wifi_rt = wifi_manager_start();
    if (wifi_rt == OPRT_OK) {
        MIMI_LOGI(TAG, "current target ssid: %s", wifi_manager_get_target_ssid());
        MIMI_LOGI(TAG, "waiting for WiFi connection...");

        if (wifi_manager_wait_connected(30000) == OPRT_OK) {
            MIMI_LOGI(TAG, "WiFi connected: %s", wifi_manager_get_ip());
            start_online_services("wifi");
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
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    MIMI_LOGI(TAG, "ENABLE_WIFI disabled, start online services with wired network");
    start_online_services("wired");
#else
    MIMI_LOGW(TAG, "both WiFi and wired are disabled, skip network services");
#endif
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
