/**
 * @file xiaozhi_mcp.c
 * @brief MCP JSON-RPC handling aligned with xiaozhi-esp32 core behavior.
 */

#include "xiaozhi_mcp.h"

#include "tal_api.h"
#include "xiaozhi_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROJECT_NAME
#define PROJECT_NAME "xiaozhi"
#endif

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.1.0"
#endif

#ifndef PLATFORM_BOARD
#define PLATFORM_BOARD PROJECT_NAME
#endif

typedef struct {
    const char *name;
    const char *description;
    BOOL_T      user_only;
    BOOL_T      has_url_arg;
    const char *url_default;
    BOOL_T      needs_assets_partition;
} xz_mcp_tool_t;

static const xz_mcp_tool_t s_tools[] = {
    {
        .name = "self.get_device_status",
        .description =
            "Provides the real-time information of the device, including the current status of the audio speaker, "
            "screen, battery, network, etc.\n"
            "Use this tool for: \n"
            "1. Answering questions about current condition (e.g. what is the current volume of the audio speaker?)\n"
            "2. As the first step to control the device (e.g. turn up / down the volume of the audio speaker, etc.)",
        .user_only              = FALSE,
        .has_url_arg            = FALSE,
        .needs_assets_partition = FALSE,
    },
    {
        .name                   = "self.get_system_info",
        .description            = "Get the system information",
        .user_only              = TRUE,
        .has_url_arg            = FALSE,
        .needs_assets_partition = FALSE,
    },
    {
        .name                   = "self.reboot",
        .description            = "Reboot the system",
        .user_only              = TRUE,
        .has_url_arg            = FALSE,
        .needs_assets_partition = FALSE,
    },
    {
        .name        = "self.upgrade_firmware",
        .description = "Upgrade firmware from a specific URL. This will download and install the firmware, then reboot "
                       "the device.",
        .user_only   = TRUE,
        .has_url_arg = TRUE,
        .url_default = "The URL of the firmware binary file to download and install",
        .needs_assets_partition = FALSE,
    },
    {
        .name                   = "self.assets.set_download_url",
        .description            = "Set the download url for the assets",
        .user_only              = TRUE,
        .has_url_arg            = TRUE,
        .url_default            = NULL,
        .needs_assets_partition = TRUE,
    },
};

static xz_mcp_ops_t s_ops = {0};

static cJSON *xz_mcp_make_error_obj(const char *msg)
{
    cJSON *err = cJSON_CreateObject();
    if (!err) {
        return NULL;
    }
    cJSON_AddStringToObject(err, "message", msg ? msg : "error");
    return err;
}

static cJSON *xz_mcp_make_tool_text_result(const char *text, BOOL_T is_error)
{
    cJSON *result  = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *item    = cJSON_CreateObject();
    if (!result || !content || !item) {
        if (result) {
            cJSON_Delete(result);
        }
        if (content) {
            cJSON_Delete(content);
        }
        if (item) {
            cJSON_Delete(item);
        }
        return NULL;
    }

    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", text ? text : "");
    cJSON_AddItemToArray(content, item);
    cJSON_AddItemToObject(result, "content", content);
    cJSON_AddBoolToObject(result, "isError", is_error ? true : false);
    return result;
}

static cJSON *xz_mcp_make_tool_text_result_from_json(cJSON *json_obj)
{
    if (!json_obj) {
        return xz_mcp_make_tool_text_result("{}", FALSE);
    }

    char *json = cJSON_PrintUnformatted(json_obj);
    cJSON_Delete(json_obj);
    if (!json) {
        return NULL;
    }

    cJSON *ret = xz_mcp_make_tool_text_result(json, FALSE);
    cJSON_free(json);
    return ret;
}

static char *xz_mcp_build_reply(int id, cJSON *result, cJSON *error)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        if (result) {
            cJSON_Delete(result);
        }
        if (error) {
            cJSON_Delete(error);
        }
        return NULL;
    }

    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);
    if (error) {
        cJSON_AddItemToObject(root, "error", error);
    } else if (result) {
        cJSON_AddItemToObject(root, "result", result);
    } else {
        cJSON_AddItemToObject(root, "result", cJSON_CreateObject());
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

static cJSON *xz_mcp_make_input_schema(const xz_mcp_tool_t *tool)
{
    if (!tool) {
        return NULL;
    }

    cJSON *schema = cJSON_CreateObject();
    if (!schema) {
        return NULL;
    }

    cJSON_AddStringToObject(schema, "type", "object");
    cJSON *props = cJSON_CreateObject();
    if (!props) {
        cJSON_Delete(schema);
        return NULL;
    }
    cJSON_AddItemToObject(schema, "properties", props);

    if (tool->has_url_arg) {
        cJSON *url_prop = cJSON_CreateObject();
        if (!url_prop) {
            cJSON_Delete(schema);
            return NULL;
        }
        cJSON_AddStringToObject(url_prop, "type", "string");
        if (tool->url_default && tool->url_default[0] != '\0') {
            cJSON_AddStringToObject(url_prop, "default", tool->url_default);
        }
        cJSON_AddItemToObject(props, "url", url_prop);

        if (!tool->url_default || tool->url_default[0] == '\0') {
            cJSON *required = cJSON_CreateArray();
            if (!required) {
                cJSON_Delete(schema);
                return NULL;
            }
            cJSON_AddItemToArray(required, cJSON_CreateString("url"));
            cJSON_AddItemToObject(schema, "required", required);
        }
    }

    return schema;
}

static int xz_env_int(const char *env_key, int default_value)
{
    if (!env_key) {
        return default_value;
    }

    const char *val = getenv(env_key);
    if (!val || val[0] == '\0') {
        return default_value;
    }

    return atoi(val);
}

static BOOL_T xz_mcp_is_tool_available(const xz_mcp_tool_t *tool)
{
    if (!tool) {
        return FALSE;
    }

    if (!tool->needs_assets_partition) {
        return TRUE;
    }

    int valid = xiaozhi_settings_get_int(XZ_NS_ASSETS, "partition_valid", 1);
    valid     = xz_env_int("XZ_ASSETS_PARTITION_VALID", valid);
    return (valid == 1) ? TRUE : FALSE;
}

static void xz_mcp_parse_capabilities(const cJSON *caps)
{
    if (!cJSON_IsObject((cJSON *)caps)) {
        return;
    }

    cJSON *vision = cJSON_GetObjectItem((cJSON *)caps, "vision");
    if (!cJSON_IsObject(vision)) {
        return;
    }

    cJSON *url = cJSON_GetObjectItem(vision, "url");
    if (cJSON_IsString(url) && url->valuestring) {
        const char *token_str = "";
        (void)xiaozhi_settings_set_string(XZ_NS_SYS, "vision_url", url->valuestring);

        cJSON *token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(token) && token->valuestring) {
            token_str = token->valuestring;
            (void)xiaozhi_settings_set_string(XZ_NS_SYS, "vision_token", token->valuestring);
        } else {
            (void)xiaozhi_settings_set_string(XZ_NS_SYS, "vision_token", "");
        }

        if (s_ops.set_vision_capabilities) {
            s_ops.set_vision_capabilities(s_ops.userdata, url->valuestring, token_str);
        }
    }
}

static cJSON *xz_mcp_make_tool_json(const xz_mcp_tool_t *tool)
{
    if (!tool) {
        return NULL;
    }

    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        return NULL;
    }

    cJSON_AddStringToObject(obj, "name", tool->name);
    cJSON_AddStringToObject(obj, "description", tool->description);

    cJSON *schema = xz_mcp_make_input_schema(tool);
    if (!schema) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddItemToObject(obj, "inputSchema", schema);

    if (tool->user_only) {
        cJSON *ann = cJSON_CreateObject();
        cJSON *aud = cJSON_CreateArray();
        if (!ann || !aud) {
            if (ann) {
                cJSON_Delete(ann);
            }
            if (aud) {
                cJSON_Delete(aud);
            }
            cJSON_Delete(obj);
            return NULL;
        }
        cJSON_AddItemToArray(aud, cJSON_CreateString("user"));
        cJSON_AddItemToObject(ann, "audience", aud);
        cJSON_AddItemToObject(obj, "annotations", ann);
    }

    return obj;
}

static const xz_mcp_tool_t *xz_mcp_find_tool(const char *name)
{
    if (!name || name[0] == '\0') {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(s_tools) / sizeof(s_tools[0]); ++i) {
        if (strcmp(s_tools[i].name, name) == 0) {
            if (!xz_mcp_is_tool_available(&s_tools[i])) {
                return NULL;
            }
            return &s_tools[i];
        }
    }

    return NULL;
}

static char *xz_mcp_handle_initialize(int id, const cJSON *params)
{
    if (cJSON_IsObject((cJSON *)params)) {
        cJSON *caps = cJSON_GetObjectItem((cJSON *)params, "capabilities");
        if (cJSON_IsObject(caps)) {
            xz_mcp_parse_capabilities(caps);
        }
    }

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
    }

    cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");

    cJSON *caps  = cJSON_CreateObject();
    cJSON *tools = cJSON_CreateObject();
    if (!caps || !tools) {
        if (caps) {
            cJSON_Delete(caps);
        }
        if (tools) {
            cJSON_Delete(tools);
        }
        cJSON_Delete(result);
        return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
    }
    cJSON_AddItemToObject(caps, "tools", tools);
    cJSON_AddItemToObject(result, "capabilities", caps);

    cJSON *server = cJSON_CreateObject();
    if (!server) {
        cJSON_Delete(result);
        return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
    }
    cJSON_AddStringToObject(server, "name", PLATFORM_BOARD);
    cJSON_AddStringToObject(server, "version", PROJECT_VERSION);
    cJSON_AddItemToObject(result, "serverInfo", server);

    return xz_mcp_build_reply(id, result, NULL);
}

static char *xz_mcp_handle_tools_list(int id, const cJSON *params)
{
    const char *cursor          = "";
    BOOL_T      with_user_tools = FALSE;

    if (cJSON_IsObject((cJSON *)params)) {
        cJSON *cursor_json = cJSON_GetObjectItem((cJSON *)params, "cursor");
        if (cJSON_IsString(cursor_json) && cursor_json->valuestring) {
            cursor = cursor_json->valuestring;
        }
        cJSON *with_user = cJSON_GetObjectItem((cJSON *)params, "withUserTools");
        if (cJSON_IsBool(with_user)) {
            with_user_tools = with_user->valueint ? TRUE : FALSE;
        }
    }

    cJSON *result    = cJSON_CreateObject();
    cJSON *tools_arr = cJSON_CreateArray();
    if (!result || !tools_arr) {
        if (result) {
            cJSON_Delete(result);
        }
        if (tools_arr) {
            cJSON_Delete(tools_arr);
        }
        return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
    }

    cJSON_AddItemToObject(result, "tools", tools_arr);

    BOOL_T      start          = (cursor[0] == '\0');
    const char *next_cursor    = NULL;
    size_t      max_payload    = 8000;
    BOOL_T      has_tool_added = FALSE;

    for (size_t i = 0; i < sizeof(s_tools) / sizeof(s_tools[0]); ++i) {
        const xz_mcp_tool_t *tool = &s_tools[i];
        if (!start) {
            if (strcmp(tool->name, cursor) == 0) {
                start = TRUE;
            } else {
                continue;
            }
        }

        if (!with_user_tools && tool->user_only) {
            continue;
        }
        if (!xz_mcp_is_tool_available(tool)) {
            continue;
        }

        cJSON *tool_json = xz_mcp_make_tool_json(tool);
        if (!tool_json) {
            cJSON_Delete(result);
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
        }

        char *tool_str = cJSON_PrintUnformatted(tool_json);
        if (!tool_str) {
            cJSON_Delete(tool_json);
            cJSON_Delete(result);
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
        }

        char  *curr     = cJSON_PrintUnformatted(result);
        size_t curr_len = curr ? strlen(curr) : 0;
        if (curr) {
            cJSON_free(curr);
        }

        if (curr_len + strlen(tool_str) + 32 > max_payload) {
            next_cursor = tool->name;
            cJSON_free(tool_str);
            cJSON_Delete(tool_json);
            break;
        }

        cJSON_free(tool_str);
        cJSON_AddItemToArray(tools_arr, tool_json);
        has_tool_added = TRUE;
    }

    if (!has_tool_added && (sizeof(s_tools) / sizeof(s_tools[0])) > 0) {
        char err[192] = {0};
        (void)snprintf(err, sizeof(err), "Failed to add tool %s because of payload size limit",
                       next_cursor ? next_cursor : "");
        cJSON_Delete(result);
        return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj(err));
    }

    if (next_cursor) {
        cJSON_AddStringToObject(result, "nextCursor", next_cursor);
    }

    return xz_mcp_build_reply(id, result, NULL);
}

static char *xz_mcp_tool_call(int id, const cJSON *params)
{
    if (!cJSON_IsObject((cJSON *)params)) {
        return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("Missing params"));
    }

    cJSON *name = cJSON_GetObjectItem((cJSON *)params, "name");
    if (!cJSON_IsString(name) || !name->valuestring) {
        return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("Missing name"));
    }

    cJSON *args = cJSON_GetObjectItem((cJSON *)params, "arguments");
    if (args && !cJSON_IsObject(args)) {
        return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("Invalid arguments"));
    }

    const xz_mcp_tool_t *tool = xz_mcp_find_tool(name->valuestring);
    if (!tool) {
        char err[192] = {0};
        (void)snprintf(err, sizeof(err), "Unknown tool: %s", name->valuestring);
        return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj(err));
    }

    cJSON *result = NULL;

    if (strcmp(tool->name, "self.get_device_status") == 0) {
        cJSON *status_json = NULL;
        if (s_ops.get_device_status) {
            status_json = s_ops.get_device_status(s_ops.userdata);
        }
        if (!status_json) {
            status_json = cJSON_CreateObject();
        }
        result = xz_mcp_make_tool_text_result_from_json(status_json);
        if (!result) {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
        }
        return xz_mcp_build_reply(id, result, NULL);
    }

    if (strcmp(tool->name, "self.get_system_info") == 0) {
        cJSON *info_json = NULL;
        if (s_ops.get_system_info) {
            info_json = s_ops.get_system_info(s_ops.userdata);
        }
        if (!info_json) {
            info_json = cJSON_CreateObject();
        }
        result = xz_mcp_make_tool_text_result_from_json(info_json);
        if (!result) {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
        }
        return xz_mcp_build_reply(id, result, NULL);
    }

    if (strcmp(tool->name, "self.reboot") == 0) {
        if (!s_ops.request_reboot || s_ops.request_reboot(s_ops.userdata) != OPRT_OK) {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("reboot failed"));
        }
        result = xz_mcp_make_tool_text_result("true", FALSE);
        if (!result) {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
        }
        return xz_mcp_build_reply(id, result, NULL);
    }

    if (strcmp(tool->name, "self.upgrade_firmware") == 0) {
        cJSON      *url         = args ? cJSON_GetObjectItem(args, "url") : NULL;
        const char *upgrade_url = NULL;
        if (cJSON_IsString(url) && url->valuestring && url->valuestring[0] != '\0') {
            upgrade_url = url->valuestring;
        } else if (tool->url_default && tool->url_default[0] != '\0') {
            upgrade_url = tool->url_default;
        }
        if (!upgrade_url || upgrade_url[0] == '\0') {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("Missing valid argument: url"));
        }

        if (!s_ops.request_upgrade || s_ops.request_upgrade(s_ops.userdata, upgrade_url) != OPRT_OK) {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("upgrade failed"));
        }
        result = xz_mcp_make_tool_text_result("true", FALSE);
        if (!result) {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
        }
        return xz_mcp_build_reply(id, result, NULL);
    }

    if (strcmp(tool->name, "self.assets.set_download_url") == 0) {
        cJSON *url = args ? cJSON_GetObjectItem(args, "url") : NULL;
        if (!cJSON_IsString(url) || !url->valuestring || url->valuestring[0] == '\0') {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("Missing valid argument: url"));
        }

        if (xiaozhi_settings_set_string(XZ_NS_ASSETS, "download_url", url->valuestring) != OPRT_OK) {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("set download url failed"));
        }

        result = xz_mcp_make_tool_text_result("true", FALSE);
        if (!result) {
            return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("malloc failed"));
        }
        return xz_mcp_build_reply(id, result, NULL);
    }

    return xz_mcp_build_reply(id, NULL, xz_mcp_make_error_obj("Method not implemented"));
}

OPERATE_RET xiaozhi_mcp_init(const xz_mcp_ops_t *ops)
{
    if (!ops) {
        return OPRT_INVALID_PARM;
    }
    s_ops = *ops;
    return OPRT_OK;
}

OPERATE_RET xiaozhi_mcp_handle_payload(const cJSON *payload, char **response_json_out)
{
    if (!payload || !response_json_out) {
        return OPRT_INVALID_PARM;
    }

    *response_json_out = NULL;

    cJSON *version = cJSON_GetObjectItem((cJSON *)payload, "jsonrpc");
    if (!cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        return OPRT_INVALID_PARM;
    }

    cJSON *id = cJSON_GetObjectItem((cJSON *)payload, "id");
    if (!cJSON_IsNumber(id)) {
        return OPRT_INVALID_PARM;
    }

    cJSON *method = cJSON_GetObjectItem((cJSON *)payload, "method");
    if (!cJSON_IsString(method) || !method->valuestring) {
        return OPRT_INVALID_PARM;
    }

    if (strncmp(method->valuestring, "notifications", strlen("notifications")) == 0) {
        return OPRT_OK;
    }

    cJSON *params = cJSON_GetObjectItem((cJSON *)payload, "params");
    if (params && !cJSON_IsObject(params)) {
        return OPRT_OK;
    }

    if (strcmp(method->valuestring, "initialize") == 0) {
        *response_json_out = xz_mcp_handle_initialize(id->valueint, params);
    } else if (strcmp(method->valuestring, "tools/list") == 0) {
        *response_json_out = xz_mcp_handle_tools_list(id->valueint, params);
    } else if (strcmp(method->valuestring, "tools/call") == 0) {
        *response_json_out = xz_mcp_tool_call(id->valueint, params);
    } else {
        char err[192] = {0};
        (void)snprintf(err, sizeof(err), "Method not implemented: %s", method->valuestring);
        *response_json_out = xz_mcp_build_reply(id->valueint, NULL, xz_mcp_make_error_obj(err));
    }

    return (*response_json_out != NULL) ? OPRT_OK : OPRT_MALLOC_FAILED;
}
