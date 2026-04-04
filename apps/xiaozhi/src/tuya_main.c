/**
 * @file tuya_main.c
 * @brief xiaozhi TuyaOpen entry.
 */

#include "xiaozhi_app.h"

#include "cJSON.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tuya_tls.h"
#include "tuya_register_center.h"

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.1.0"
#endif

extern void tuya_app_cli_init(void);

static void xiaozhi_runtime_init(void)
{
    static BOOL_T s_inited = FALSE;
    if (s_inited) {
        return;
    }

    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
    (void)tal_log_init(TAL_LOG_LEVEL_INFO, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    (void)tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key  = "dflfuap134ddlduq",
    });
    (void)tal_sw_timer_init();
    (void)tal_workq_init();
    (void)tuya_tls_init();
    (void)tuya_register_center_init();

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

    s_inited = TRUE;
}

void user_main(void)
{
    xiaozhi_runtime_init();

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    OPERATE_RET rt = xiaozhi_app_init();
    if (rt != OPRT_OK) {
        PR_ERR("xiaozhi_app_init failed: %d", rt);
        return;
    }

    tuya_app_cli_init();

    rt = xiaozhi_app_start();
    if (rt != OPRT_OK) {
        PR_ERR("xiaozhi_app_start failed: %d", rt);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    user_main();
    while (1) {
        tal_system_sleep(1000);
    }
}
#else
static THREAD_HANDLE ty_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    (void)arg;
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth   = 1024 * 6;
    thrd_param.priority     = THREAD_PRIO_1;
    thrd_param.thrdname     = "tuya_app_main";
    (void)tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
