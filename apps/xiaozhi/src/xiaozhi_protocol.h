/**
 * @file xiaozhi_protocol.h
 * @brief Protocol shared structs/helpers for websocket and mqtt-udp.
 */

#ifndef __XIAOZHI_PROTOCOL_H__
#define __XIAOZHI_PROTOCOL_H__

#include "cJSON.h"
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XZ_AUDIO_SAMPLE_RATE    16000
#define XZ_AUDIO_CHANNELS       1
#define XZ_AUDIO_FRAME_DURATION 60
#define XZ_CHANNEL_TIMEOUT_SEC  120

typedef enum {
    XZ_PROTO_KIND_WEBSOCKET = 1,
    XZ_PROTO_KIND_MQTT_UDP  = 2,
} xz_proto_kind_t;

typedef enum {
    XZ_CONN_IDLE       = 0,
    XZ_CONN_CONNECTING = 1,
    XZ_CONN_READY      = 2,
    XZ_CONN_ERROR      = 3,
} xz_conn_state_t;

typedef struct {
    BOOL_T   ready;
    BOOL_T   error;
    char     session_id[96];
    int      server_sample_rate;
    int      server_frame_duration;
    uint32_t last_rx_ms;
} xz_channel_t;

typedef void (*xz_text_message_cb_t)(void *userdata, const uint8_t *payload, size_t payload_len);

void   xz_channel_reset(xz_channel_t *ch);
BOOL_T xz_channel_is_timeout(const xz_channel_t *ch, uint32_t now_ms);

char *xz_build_hello(const char *transport, int version, BOOL_T enable_mcp, BOOL_T enable_aec);
char *xz_build_listen(const char *session_id, const char *state, const char *mode, const char *text);
char *xz_build_abort(const char *session_id, const char *reason);
char *xz_build_mcp(const char *session_id, const char *payload_json);
char *xz_build_goodbye(const char *session_id);

OPERATE_RET xz_parse_server_hello(const cJSON *root, const char *expect_transport, xz_channel_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_PROTOCOL_H__ */
