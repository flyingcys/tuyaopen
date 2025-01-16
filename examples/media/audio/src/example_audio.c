/**
 * @file example_ble_central.c
 * @brief Bluetooth Low Energy (BLE) central role implementation example.
 *
 * This file demonstrates the implementation of a BLE central device using Tuya's APIs.
 * It includes initializing the BLE stack, scanning for BLE advertisements, parsing advertisement data, and managing BLE
 * connections. The example showcases how to set up a BLE central device to interact with BLE peripheral devices,
 * facilitating communication and data exchange in IoT applications.
 *
 * The implementation covers essential BLE operations such as starting and stopping scans, handling advertisement
 * reports, and initiating connections with peripheral devices. It also demonstrates how to configure scanning
 * parameters and process advertisement packets to extract useful information such as device addresses, advertisement
 * types, and signal strength (RSSI).
 *
 * @note This example is designed to provide a basic understanding of operating a BLE central device using Tuya's BLE
 * APIs and should be adapted to fit specific application requirements.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_bluetooth.h"
#include "tkl_output.h"

#include "tkl_audio.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AUDIO_SAMPLE_RATE          16000
#define SPK_SAMPLE_RATE            16000
#define AUDIO_SAMPLE_BITS          16
#define AUDIO_CHANNEL              1

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/

static int __audio_mic_cb(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    OPERATE_RET ret = OPRT_OK;

    char *data = pframe->pbuf;
    uint32_t len = pframe->buf_size;

    /* write a fram speaker data to speaker_ring_buff */
    TKL_AUDIO_FRAME_INFO_T ao_frame;
    ao_frame.pbuf = data;
    ao_frame.used_size = len;
    ret = tkl_ao_put_frame(TKL_AUDIO_TYPE_BOARD, 0, NULL, &ao_frame);
    if (ret != OPRT_OK) {
        PR_ERR("tkl_ao_put_frame failed, ret=%d", ret);
        return -1;
    }
    
    return len;
}

void audio_example_deinit(void)
{
    tkl_ai_stop(0, 0);
    tkl_ai_uninit();
}

OPERATE_RET audio_example_init(void)
{
    OPERATE_RET ret = OPRT_OK;

    TKL_AUDIO_CONFIG_T config;
    config.enable = 0;
    config.ai_chn = 0;
    config.sample = AUDIO_SAMPLE_RATE;
    config.spk_sample = SPK_SAMPLE_RATE;
    config.datebits = AUDIO_SAMPLE_BITS;
    config.channel = AUDIO_CHANNEL;
    config.codectype = TKL_CODEC_AUDIO_PCM;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.spk_gpio = TUYA_GPIO_NUM_27;
    config.spk_gpio_polarity = TUYA_GPIO_LEVEL_LOW;
    config.put_cb = __audio_mic_cb;

    // open audio
    ret = tkl_ai_init(&config, 0);
    if (ret != OPRT_OK) {
        PR_ERR("tkl_ai_init fail");
        goto err;
    }

    PR_NOTICE("tkl_ai_start...");
    ret = tkl_ai_start(0, 0);
    if (ret != OPRT_OK) {
        PR_ERR("tkl_ai_start fail");
        goto err;
    }

    // set mic volume
    tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, 100);

    // set spk volume
    // tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, 100);

    return OPRT_OK;
err:
    tkl_ai_stop(TKL_AUDIO_TYPE_BOARD, 0);
    tkl_ai_uninit();
    return ret;
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main()
{
    OPERATE_RET rt = OPRT_OK;
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    PR_DEBUG("hello world\r\n");
    
    rt = audio_example_init();
    if (rt != OPRT_OK) {
        PR_ERR("_ty_audio_init fail");
    }

    while (1) {
        tal_system_sleep(1000);
    }
__EXIT:
    audio_example_deinit();

    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif