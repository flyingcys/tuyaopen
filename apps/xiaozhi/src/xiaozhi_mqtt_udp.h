/**
 * @file xiaozhi_mqtt_udp.h
 * @brief MQTT + UDP protocol client for xiaozhi.
 */

#ifndef __XIAOZHI_MQTT_UDP_H__
#define __XIAOZHI_MQTT_UDP_H__

#include "xiaozhi_protocol.h"

#include "mqtt_client_interface.h"
#include "tuya_cloud_types.h"

#include "mbedtls/aes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void  *mqtt;
    BOOL_T mqtt_connected;
    BOOL_T mqtt_subscribed;

    char     endpoint[160];
    char     host[128];
    uint16_t port;
    uint16_t keepalive;

    char client_id[96];
    char username[160];
    char password[160];
    char publish_topic[160];
    char subscribe_topic[160];

    uint8_t *cacert;
    uint16_t cacert_len;

    int      udp_fd;
    char     udp_server[128];
    uint16_t udp_port;

    BOOL_T               aes_ready;
    uint8_t              aes_nonce[16];
    mbedtls_aes_context  aes_ctx;
    uint32_t             local_sequence;
    uint32_t             remote_sequence;
    xz_text_message_cb_t on_text_message;
    void                *on_text_userdata;

    xz_channel_t    channel;
    xz_conn_state_t state;
} xz_mqtt_udp_client_t;

OPERATE_RET xz_mqtt_udp_init(xz_mqtt_udp_client_t *client);
OPERATE_RET xz_mqtt_udp_deinit(xz_mqtt_udp_client_t *client);

OPERATE_RET xz_mqtt_udp_connect(xz_mqtt_udp_client_t *client, const char *endpoint, const char *client_id,
                                const char *username, const char *password, const char *publish_topic,
                                const char *subscribe_topic, int keepalive);
OPERATE_RET xz_mqtt_udp_open_audio_channel(xz_mqtt_udp_client_t *client, uint32_t timeout_ms);
OPERATE_RET xz_mqtt_udp_poll(xz_mqtt_udp_client_t *client, int wait_ms);
OPERATE_RET xz_mqtt_udp_close(xz_mqtt_udp_client_t *client, BOOL_T send_goodbye);

OPERATE_RET xz_mqtt_udp_send_text(xz_mqtt_udp_client_t *client, const char *text);
OPERATE_RET xz_mqtt_udp_send_listen(xz_mqtt_udp_client_t *client, const char *state, const char *mode,
                                    const char *text);
OPERATE_RET xz_mqtt_udp_send_abort(xz_mqtt_udp_client_t *client, const char *reason);
OPERATE_RET xz_mqtt_udp_send_mcp(xz_mqtt_udp_client_t *client, const char *payload_json);
OPERATE_RET xz_mqtt_udp_send_audio(xz_mqtt_udp_client_t *client, const uint8_t *opus_data, size_t opus_len,
                                   uint32_t timestamp);
OPERATE_RET xz_mqtt_udp_set_text_message_callback(xz_mqtt_udp_client_t *client, xz_text_message_cb_t cb,
                                                  void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_MQTT_UDP_H__ */
