/**
 * @file xiaozhi_upgrade.h
 * @brief Firmware upgrade helper.
 */

#ifndef __XIAOZHI_UPGRADE_H__
#define __XIAOZHI_UPGRADE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET xiaozhi_upgrade_from_url(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_UPGRADE_H__ */
