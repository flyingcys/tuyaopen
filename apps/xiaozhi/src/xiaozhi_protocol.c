/**
 * @file xiaozhi_protocol.c
 * @brief Shared protocol helpers aligned with xiaozhi-esp32 docs.
 */

#include "xiaozhi_protocol.h"

#ifdef XZ_PROTOCOL_TEST_ONLY
extern uint32_t tal_system_get_millisecond(void);
extern void    *tal_malloc(size_t size);
#else
#include "tal_api.h"
#endif

#include <stdio.h>
#include <string.h>

void xz_channel_reset(xz_channel_t *ch)
{
    if (!ch) {
        return;
    }
    memset(ch, 0, sizeof(*ch));
    ch->server_sample_rate    = 24000;
    ch->server_frame_duration = XZ_AUDIO_FRAME_DURATION;
}

BOOL_T xz_channel_is_timeout(const xz_channel_t *ch, uint32_t now_ms)
{
    if (!ch || ch->last_rx_ms == 0) {
        return FALSE;
    }
    uint32_t diff = now_ms - ch->last_rx_ms;
    return (diff > XZ_CHANNEL_TIMEOUT_SEC * 1000) ? TRUE : FALSE;
}

static char *xz_print_json(cJSON *root)
{
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

char *xz_build_hello(const char *transport, int version, BOOL_T enable_mcp, BOOL_T enable_aec)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", version);
    cJSON_AddStringToObject(root, "transport", transport ? transport : "websocket");

    cJSON *features = cJSON_CreateObject();
    if (!features) {
        cJSON_Delete(root);
        return NULL;
    }
    if (enable_mcp) {
        cJSON_AddBoolToObject(features, "mcp", true);
    }
    if (enable_aec) {
        cJSON_AddBoolToObject(features, "aec", true);
    }
    cJSON_AddItemToObject(root, "features", features);

    cJSON *audio = cJSON_CreateObject();
    if (!audio) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddStringToObject(audio, "format", "opus");
    cJSON_AddNumberToObject(audio, "sample_rate", XZ_AUDIO_SAMPLE_RATE);
    cJSON_AddNumberToObject(audio, "channels", XZ_AUDIO_CHANNELS);
    cJSON_AddNumberToObject(audio, "frame_duration", XZ_AUDIO_FRAME_DURATION);
    cJSON_AddItemToObject(root, "audio_params", audio);

    return xz_print_json(root);
}

char *xz_build_listen(const char *session_id, const char *state, const char *mode, const char *text)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "session_id", session_id ? session_id : "");
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", state ? state : "start");
    if (mode && mode[0] != '\0') {
        cJSON_AddStringToObject(root, "mode", mode);
    }
    if (text && text[0] != '\0') {
        cJSON_AddStringToObject(root, "text", text);
    }
    return xz_print_json(root);
}

char *xz_build_abort(const char *session_id, const char *reason)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "session_id", session_id ? session_id : "");
    cJSON_AddStringToObject(root, "type", "abort");
    if (reason && reason[0] != '\0') {
        cJSON_AddStringToObject(root, "reason", reason);
    }
    return xz_print_json(root);
}

char *xz_build_mcp(const char *session_id, const char *payload_json)
{
    const char *sid     = session_id ? session_id : "";
    const char *payload = (payload_json && payload_json[0] != '\0') ? payload_json : "{}";
    size_t      need    = strlen(sid) + strlen(payload) + 64;
    char       *out     = tal_malloc(need);
    if (!out) {
        return NULL;
    }
    (void)snprintf(out, need, "{\"session_id\":\"%s\",\"type\":\"mcp\",\"payload\":%s}", sid, payload);
    return out;
}

char *xz_build_goodbye(const char *session_id)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "session_id", session_id ? session_id : "");
    cJSON_AddStringToObject(root, "type", "goodbye");
    return xz_print_json(root);
}

OPERATE_RET xz_parse_server_hello(const cJSON *root, const char *expect_transport, xz_channel_t *out)
{
    if (!root || !out || !expect_transport) {
        return OPRT_INVALID_PARM;
    }

    cJSON *type      = cJSON_GetObjectItem((cJSON *)root, "type");
    cJSON *transport = cJSON_GetObjectItem((cJSON *)root, "transport");
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "hello") != 0) {
        return OPRT_INVALID_PARM;
    }
    if (!cJSON_IsString(transport) || strcmp(transport->valuestring, expect_transport) != 0) {
        return OPRT_INVALID_PARM;
    }

    cJSON *session_id = cJSON_GetObjectItem((cJSON *)root, "session_id");
    if (cJSON_IsString(session_id) && session_id->valuestring) {
        (void)snprintf(out->session_id, sizeof(out->session_id), "%s", session_id->valuestring);
    }

    cJSON *audio = cJSON_GetObjectItem((cJSON *)root, "audio_params");
    if (cJSON_IsObject(audio)) {
        cJSON *sample_rate    = cJSON_GetObjectItem(audio, "sample_rate");
        cJSON *frame_duration = cJSON_GetObjectItem(audio, "frame_duration");
        if (cJSON_IsNumber(sample_rate)) {
            out->server_sample_rate = sample_rate->valueint;
        }
        if (cJSON_IsNumber(frame_duration)) {
            out->server_frame_duration = frame_duration->valueint;
        }
    }

    out->ready      = TRUE;
    out->error      = FALSE;
    out->last_rx_ms = tal_system_get_millisecond();
    return OPRT_OK;
}
