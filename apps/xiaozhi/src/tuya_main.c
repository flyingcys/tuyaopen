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

#include <string.h>

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

#if OPERATING_SYSTEM == SYSTEM_LINUX
#include "../../../src/peripherals/audio_codecs/tdd_audio/include/tdd_audio_alsa.h"
#if defined(__GNUC__) || defined(__clang__)
extern OPERATE_RET board_register_hardware(void) __attribute__((weak));
#else
extern OPERATE_RET board_register_hardware(void);
#endif

#ifndef AUDIO_CODEC_NAME
#define AUDIO_CODEC_NAME "alsa_audio"
#endif
#ifndef ALSA_DEVICE_CAPTURE
#define ALSA_DEVICE_CAPTURE "default"
#endif
#ifndef ALSA_DEVICE_PLAYBACK
#define ALSA_DEVICE_PLAYBACK "default"
#endif
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

static OPERATE_RET xiaozhi_run_app_lifecycle(void)
{
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
        return rt;
    }

    tuya_app_cli_init();

    rt = xiaozhi_app_start();
    if (rt != OPRT_OK) {
        PR_ERR("xiaozhi_app_start failed: %d", rt);
        return rt;
    }

    return OPRT_OK;
}

void user_main(void)
{
    xiaozhi_runtime_init();
    (void)xiaozhi_run_app_lifecycle();
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
static OPERATE_RET xiaozhi_linux_register_audio_fallback(void)
{
#if defined(ENABLE_AUDIO_ALSA) && (ENABLE_AUDIO_ALSA == 1)
    TDD_AUDIO_ALSA_CFG_T alsa_cfg = {0};

    (void)strncpy(alsa_cfg.capture_device, ALSA_DEVICE_CAPTURE, sizeof(alsa_cfg.capture_device) - 1);
    (void)strncpy(alsa_cfg.playback_device, ALSA_DEVICE_PLAYBACK, sizeof(alsa_cfg.playback_device) - 1);

    alsa_cfg.sample_rate     = TDD_ALSA_SAMPLE_16000;
    alsa_cfg.data_bits       = TDD_ALSA_DATABITS_16;
    alsa_cfg.channels        = TDD_ALSA_CHANNEL_MONO;
    alsa_cfg.spk_sample_rate = TDD_ALSA_SAMPLE_16000;
    alsa_cfg.buffer_frames   = 1024;
    alsa_cfg.period_frames   = 256;
    alsa_cfg.aec_enable      = 0;

    OPERATE_RET rt = tdd_audio_alsa_register((char *)AUDIO_CODEC_NAME, alsa_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("fallback tdd_audio_alsa_register failed: %d", rt);
        return rt;
    }
    PR_NOTICE("fallback ALSA registered: codec=%s cap=%s pb=%s", AUDIO_CODEC_NAME, ALSA_DEVICE_CAPTURE,
              ALSA_DEVICE_PLAYBACK);
    return OPRT_OK;
#else
    PR_ERR("ENABLE_AUDIO_ALSA is disabled, cannot fallback register ALSA");
    return OPRT_NOT_SUPPORTED;
#endif
}

static OPERATE_RET xiaozhi_linux_register_hardware(void)
{
    if (board_register_hardware) {
        OPERATE_RET rt = board_register_hardware();
        if (rt == OPRT_OK) {
            return OPRT_OK;
        }
        PR_WARN("board_register_hardware failed: %d, try ALSA fallback", rt);
    } else {
        PR_WARN("board_register_hardware not found, use ALSA fallback");
    }

    return xiaozhi_linux_register_audio_fallback();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    xiaozhi_runtime_init();

    OPERATE_RET rt = xiaozhi_linux_register_hardware();
    if (rt != OPRT_OK) {
        PR_ERR("linux hardware register failed: %d", rt);
        return 2;
    }

    rt = xiaozhi_run_app_lifecycle();
    if (rt != OPRT_OK) {
        PR_ERR("xiaozhi app lifecycle failed: %d", rt);
        return 3;
    }

    while (1) {
        tal_system_sleep(1000);
    }

    return 0;
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
