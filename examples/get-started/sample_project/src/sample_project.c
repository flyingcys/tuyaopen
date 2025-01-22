
/**
 * @file hello_world.c
 * @brief Simple demonstration of a "Hello World" program for SDK.
 *
 * This file contains the implementation of a basic "Hello World" program
 * designed for Tuya IoT projects. It demonstrates the initialization of the
 * Tuya Abstract Layer (TAL) logging system, a simple loop to print debug
 * messages, and the use of a task thread for executing the main logic. The
 * program showcases fundamental concepts such as logging, task creation, and
 * conditional compilation based on the operating system. It serves as a
 * starting point for developers new to Tuya's IoT platform, illustrating how to
 * structure a simple application and utilize Tuya's SDK for basic operations.
 *
 * The code is structured to support different execution paths based on the
 * operating system, demonstrating the portability and flexibility of Tuya's IoT
 * SDK across various platforms.
 *
 * @note This example is designed for educational purposes and may need to be
 * adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_api.h"
#include "tal_cli.h"
#include "tkl_output.h"

#include "tal_log.h"
#include "cJSON.h"

#include "peer.h"
#include "peer_connection.h"

// #if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"

#include "tal_wifi.h"
static int connect_status = 0;
/**
 * @brief wifi Related event callback function
 *
 * @param[in] event:event
 * @param[in] arg:parameter
 * @return none
 */
static void wifi_event_callback(WF_EVENT_E event, void *arg)
{
    OPERATE_RET op_ret = OPRT_OK;
    NW_IP_S sta_info;
    memset(&sta_info, 0, sizeof(NW_IP_S));

    PR_DEBUG("-------------event callback-------------");
    switch (event) {
    case WFE_CONNECTED: {
        PR_DEBUG("connection succeeded!");

        /* output ip information */
        op_ret = tal_wifi_get_ip(WF_STATION, &sta_info);
        if (OPRT_OK != op_ret) {
            PR_ERR("get station ip error");
            return;
        }
        PR_NOTICE("gw: %s", sta_info.gw);
        PR_NOTICE("ip: %s", sta_info.ip);
        PR_NOTICE("mask: %s", sta_info.mask);
        
        connect_status = 1;

        break;
    }

    case WFE_CONNECT_FAILED: {
        PR_DEBUG("connection fail!");
        break;
    }

    case WFE_DISCONNECTED: {
        PR_DEBUG("disconnect!");
        break;
    }
    }
}
// #endif

/**
 * @brief user_main
 *
 * @return int
 */
int user_main()
{
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, tkl_log_output);
    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    // tal_sw_timer_init();
    // tal_workq_init();
    // tal_cli_init();
    tuya_tls_init();
    // tuya_register_center_init();
    // tuya_app_cli_init();

    PR_DEBUG("Hello World! %x", tal_system_get_free_heap_size());

// #if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    OPERATE_RET rt = OPRT_OK;
    char connect_ssid[] = "6F-S-03-09"; // connect wifi ssid
    char connect_pswd[] = "12345678"; // connect wifi password
    /*WiFi init*/
    TUYA_CALL_ERR_GOTO(tal_wifi_init(wifi_event_callback), __EXIT);

    /*Set WiFi mode to station*/
    TUYA_CALL_ERR_GOTO(tal_wifi_set_work_mode(WWM_STATION), __EXIT);

    /*STA mode, connect to WiFi*/
    PR_NOTICE("\r\nconnect wifi ssid: %s, password: %s\r\n", connect_ssid, connect_pswd);
    TUYA_CALL_ERR_LOG(tal_wifi_station_connect((int8_t *)connect_ssid, (int8_t *)connect_pswd));

    while (0 == connect_status) {
        tal_system_sleep(100);
    }
// #endif

    peer_init();
    oai_platform_init_audio_capture();
    oai_init_audio_decoder();

    oai_webrtc();

    while (1) {
        tal_system_sleep(10);
    }
__EXIT:
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
    THREAD_CFG_T thrd_param = {4096*10, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif