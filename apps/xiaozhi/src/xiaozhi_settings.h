/**
 * @file xiaozhi_settings.h
 * @brief xiaozhi app settings stored in TAL KV.
 */

#ifndef __XIAOZHI_SETTINGS_H__
#define __XIAOZHI_SETTINGS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XZ_NS_WS     "xz.ws"
#define XZ_NS_MQTT   "xz.mqtt"
#define XZ_NS_SYS    "xz.sys"
#define XZ_NS_WIFI   "xz.wifi"
#define XZ_NS_COMMON "xz.common"
#define XZ_NS_ASSETS "xz.assets"

#define XZ_PROTOCOL_WEBSOCKET "websocket"
#define XZ_PROTOCOL_MQTT_UDP  "mqtt-udp"
#define XZ_DEFAULT_OTA_URL    "https://api.tenclass.net/xiaozhi/ota/"

OPERATE_RET xiaozhi_settings_init_defaults(void);

OPERATE_RET xiaozhi_settings_set_string(const char *ns, const char *key, const char *value);
OPERATE_RET xiaozhi_settings_get_string(const char *ns, const char *key, char *out, size_t out_size,
                                        const char *default_value);

OPERATE_RET xiaozhi_settings_set_int(const char *ns, const char *key, int value);
int         xiaozhi_settings_get_int(const char *ns, const char *key, int default_value);

OPERATE_RET xiaozhi_settings_set_proto(const char *proto);
OPERATE_RET xiaozhi_settings_get_proto(char *proto, size_t proto_size);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_SETTINGS_H__ */
