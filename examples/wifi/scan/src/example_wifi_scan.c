/**
 * @file example_wifi_scan.c
 * @brief WiFi scan example for SDK.
 *
 * This file demonstrates how to perform a WiFi scan using the Tuya IoT SDK.
 * It includes initializing the WiFi module, triggering a scan for available WiFi networks, and processing the scan
 * results. The example covers setting up a task for WiFi scanning, handling the scan results to list available
 * networks, and cleaning up resources upon completion. This example is useful for IoT devices that need to list or
 * connect to WiFi networks based on signal strength or other criteria.
 *
 * The code is structured to provide a clear example of using Tuya's WiFi APIs for scanning nearby WiFi networks, which
 * can be integrated into IoT solutions that require WiFi connectivity setup or network management functionalities.
 *
 * @note This example is designed for educational purposes and may need to be adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_wifi.h"
#include "tkl_output.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
char info_ssid[64 + 1];

/***********************************************************
***********************function define**********************
***********************************************************/
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

/**
 * @brief WiFi scanf task
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main()
{
    OPERATE_RET rt = OPRT_OK;
    AP_IF_S *ap_info;
    uint32_t ap_info_nums;
    int i = 0;
    PR_NOTICE("------ wifi scan example start ------");

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();

    /*WiFi init*/
    TUYA_CALL_ERR_GOTO(tal_wifi_init(wifi_event_callback), __EXIT);

    /*Set WiFi mode to station*/
    TUYA_CALL_ERR_GOTO(tal_wifi_set_work_mode(WWM_STATION), __EXIT);

    /*Scan WiFi information in the current environment*/
    TUYA_CALL_ERR_GOTO(tal_wifi_all_ap_scan(&ap_info, &ap_info_nums), __EXIT);
    PR_DEBUG("Scanf to %d wifi signals", ap_info_nums);
    for (i = 0; i < ap_info_nums; i++) {
        strcpy((char *)info_ssid, (const char *)ap_info[i].ssid);
        PR_DEBUG("channel:%d, ssid:%s", ap_info[i].channel, info_ssid);
    }

    /*Release the acquired WiFi information in the current environment*/
    TUYA_CALL_ERR_LOG(tal_wifi_release_ap(ap_info));

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
