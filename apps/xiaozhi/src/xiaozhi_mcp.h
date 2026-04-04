/**
 * @file xiaozhi_mcp.h
 * @brief Minimal MCP JSON-RPC handler for xiaozhi TuyaOpen app.
 */

#ifndef __XIAOZHI_MCP_H__
#define __XIAOZHI_MCP_H__

#include "cJSON.h"
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    cJSON *(*get_device_status)(void *userdata);
    cJSON *(*get_system_info)(void *userdata);
    OPERATE_RET (*request_reboot)(void *userdata);
    OPERATE_RET (*request_upgrade)(void *userdata, const char *url);
    void (*set_vision_capabilities)(void *userdata, const char *url, const char *token);
    void *userdata;
} xz_mcp_ops_t;

OPERATE_RET xiaozhi_mcp_init(const xz_mcp_ops_t *ops);
OPERATE_RET xiaozhi_mcp_handle_payload(const cJSON *payload, char **response_json_out);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_MCP_H__ */
