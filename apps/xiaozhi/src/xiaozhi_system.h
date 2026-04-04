/**
 * @file xiaozhi_system.h
 * @brief System identity helpers for xiaozhi app.
 */

#ifndef __XIAOZHI_SYSTEM_H__
#define __XIAOZHI_SYSTEM_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET xiaozhi_system_get_device_id(char *buf, size_t buf_size);
OPERATE_RET xiaozhi_system_get_client_id(char *buf, size_t buf_size);
OPERATE_RET xiaozhi_system_get_user_agent(char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_SYSTEM_H__ */
