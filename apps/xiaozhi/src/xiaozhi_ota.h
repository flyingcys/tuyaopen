/**
 * @file xiaozhi_ota.h
 * @brief OTA bootstrap for fetching xiaozhi protocol config.
 */

#ifndef __XIAOZHI_OTA_H__
#define __XIAOZHI_OTA_H__

#include "tuya_cloud_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    BOOL_T  has_websocket;
    BOOL_T  has_mqtt;
    BOOL_T  has_server_time;
    int64_t server_timestamp_ms;
    int     timezone_offset_min;
    BOOL_T  has_firmware;
    BOOL_T  firmware_force;
    char    firmware_version[64];
    char    firmware_url[384];
    BOOL_T  has_activation_code;
    BOOL_T  has_activation_challenge;
    char    activation_message[256];
    char    activation_code[128];
    char    activation_challenge[128];
    int     activation_timeout_ms;
} xz_ota_result_t;

OPERATE_RET xiaozhi_ota_check_and_apply(xz_ota_result_t *result);
OPERATE_RET xiaozhi_ota_activate_if_needed(const xz_ota_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_OTA_H__ */
