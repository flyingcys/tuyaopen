/**
 * @file xiaozhi_ws.h
 * @brief WebSocket protocol client for xiaozhi.
 */

#ifndef __XIAOZHI_WS_H__
#define __XIAOZHI_WS_H__

#include "xiaozhi_protocol.h"

#include "tuya_cloud_types.h"
#include "tuya_tls.h"
#include "tuya_transporter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*xz_binary_message_cb_t)(void *userdata, const uint8_t *payload, size_t payload_len);

typedef struct {
    tuya_transporter_t tcp;
    tuya_tls_hander    tls;
    int                socket_fd;
    BOOL_T             use_tls;

    char     url[320];
    char     host[128];
    uint16_t port;
    char     path[256];
    char     token[256];
    int      version;
    char     device_id[32];
    char     client_id[96];

    uint8_t             *rx_buf;
    size_t               rx_len;
    size_t               rx_cap;
    xz_text_message_cb_t on_text_message;
    void                *on_text_userdata;

    xz_binary_message_cb_t on_binary_message;
    void                  *on_binary_userdata;

    xz_channel_t    channel;
    xz_conn_state_t state;
} xz_ws_client_t;

OPERATE_RET xz_ws_init(xz_ws_client_t *ws);
OPERATE_RET xz_ws_deinit(xz_ws_client_t *ws);

OPERATE_RET xz_ws_connect(xz_ws_client_t *ws, const char *url, const char *token, int version, const char *device_id,
                          const char *client_id);
OPERATE_RET xz_ws_open_audio_channel(xz_ws_client_t *ws, uint32_t timeout_ms);
OPERATE_RET xz_ws_poll(xz_ws_client_t *ws, int wait_ms);
OPERATE_RET xz_ws_close(xz_ws_client_t *ws);

OPERATE_RET xz_ws_send_text(xz_ws_client_t *ws, const char *text);
OPERATE_RET xz_ws_send_listen(xz_ws_client_t *ws, const char *state, const char *mode, const char *text);
OPERATE_RET xz_ws_send_abort(xz_ws_client_t *ws, const char *reason);
OPERATE_RET xz_ws_send_mcp(xz_ws_client_t *ws, const char *payload_json);
OPERATE_RET xz_ws_set_text_message_callback(xz_ws_client_t *ws, xz_text_message_cb_t cb, void *userdata);
OPERATE_RET xz_ws_send_audio(xz_ws_client_t *ws, const uint8_t *payload, size_t payload_len);
/* payload pointer is only valid for the scope of the binary callback; copy it if you need it later. */
OPERATE_RET xz_ws_set_binary_message_callback(xz_ws_client_t *ws, xz_binary_message_cb_t cb, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_WS_H__ */
