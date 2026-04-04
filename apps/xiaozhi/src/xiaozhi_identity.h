/**
 * @file xiaozhi_identity.h
 * @brief 设备身份与 MQTT topic 纯逻辑辅助函数。
 */

#ifndef __XIAOZHI_IDENTITY_H__
#define __XIAOZHI_IDENTITY_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool xz_identity_select_device_id(const char *stored_device_id, const uint8_t fallback_mac[6], char *out,
                                  size_t out_size);

bool xz_identity_resolve_subscribe_topic(const char *publish_topic, const char *subscribe_topic, const char *device_id,
                                         char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_IDENTITY_H__ */
