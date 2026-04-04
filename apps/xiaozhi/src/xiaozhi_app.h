/**
 * @file xiaozhi_app.h
 * @brief High-level app controller for xiaozhi.
 */

#ifndef __XIAOZHI_APP_H__
#define __XIAOZHI_APP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET xiaozhi_app_init(void);
OPERATE_RET xiaozhi_app_start(void);
OPERATE_RET xiaozhi_app_stop(void);
OPERATE_RET xiaozhi_app_reconnect(void);
OPERATE_RET xiaozhi_app_ota_bootstrap(void);
OPERATE_RET xiaozhi_app_upgrade_firmware(const char *url);

OPERATE_RET xiaozhi_app_apply_wifi_settings(void);
OPERATE_RET xiaozhi_app_get_status(char *buf, size_t buf_size);

OPERATE_RET xiaozhi_app_send_listen(const char *state, const char *mode, const char *text);
OPERATE_RET xiaozhi_app_send_abort(const char *reason);
OPERATE_RET xiaozhi_app_send_mcp(const char *payload_json);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_APP_H__ */
