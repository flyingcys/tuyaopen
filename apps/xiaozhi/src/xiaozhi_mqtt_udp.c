/**
 * @file xiaozhi_mqtt_udp.c
 * @brief MQTT + UDP implementation aligned with xiaozhi-esp32 mqtt protocol.
 */

#include "xiaozhi_mqtt_udp.h"

#include "iotdns.h"
#include "tal_api.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define XZ_UDP_NONCE_LEN           16
#define XZ_UDP_HDR_LEN             16
#define XZ_MQTT_TIMEOUT_MS_DEFAULT 3000

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

static OPERATE_RET xz_parse_endpoint(const char *endpoint, char *host, size_t host_size, uint16_t *port)
{
    if (!endpoint || !host || !port || host_size == 0) {
        return OPRT_INVALID_PARM;
    }

    const char *colon = strchr(endpoint, ':');
    if (!colon) {
        if (strlen(endpoint) >= host_size) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        (void)snprintf(host, host_size, "%s", endpoint);
        *port = 8883;
        return OPRT_OK;
    }

    size_t host_len = (size_t)(colon - endpoint);
    if (host_len == 0 || host_len >= host_size) {
        return OPRT_INVALID_PARM;
    }

    memcpy(host, endpoint, host_len);
    host[host_len] = '\0';

    int p = atoi(colon + 1);
    if (p <= 0 || p > 65535) {
        return OPRT_INVALID_PARM;
    }
    *port = (uint16_t)p;

    return OPRT_OK;
}

static uint8_t xz_hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return (uint8_t)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (uint8_t)(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return (uint8_t)(c - 'A' + 10);
    }
    return 0xFF;
}

static BOOL_T xz_hex_decode(const char *hex, uint8_t *out, size_t out_len)
{
    if (!hex || !out || out_len == 0) {
        return FALSE;
    }

    size_t hex_len = strlen(hex);
    if (hex_len != out_len * 2) {
        return FALSE;
    }

    for (size_t i = 0; i < out_len; ++i) {
        uint8_t hi = xz_hex_nibble(hex[i * 2]);
        uint8_t lo = xz_hex_nibble(hex[i * 2 + 1]);
        if (hi == 0xFF || lo == 0xFF) {
            return FALSE;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return TRUE;
}

static void xz_mqtt_udp_close_udp(xz_mqtt_udp_client_t *client)
{
    if (!client) {
        return;
    }

    if (client->udp_fd >= 0) {
        (void)tal_net_close(client->udp_fd);
        client->udp_fd = -1;
    }

    client->udp_server[0] = '\0';
    client->udp_port      = 0;
    client->aes_ready     = FALSE;
    memset(client->aes_nonce, 0, sizeof(client->aes_nonce));
    client->local_sequence  = 0;
    client->remote_sequence = 0;
    mbedtls_aes_free(&client->aes_ctx);
}

static void xz_mqtt_udp_close_mqtt(xz_mqtt_udp_client_t *client)
{
    if (!client || !client->mqtt) {
        return;
    }

    (void)mqtt_client_disconnect(client->mqtt);
    (void)mqtt_client_deinit(client->mqtt);
    mqtt_client_free(client->mqtt);
    client->mqtt = NULL;

    client->mqtt_connected  = FALSE;
    client->mqtt_subscribed = FALSE;

    if (client->cacert) {
        tal_free(client->cacert);
        client->cacert     = NULL;
        client->cacert_len = 0;
    }
}

static OPERATE_RET xz_mqtt_udp_send_goodbye(xz_mqtt_udp_client_t *client)
{
    if (!client || !client->mqtt_connected || client->channel.session_id[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    char *msg = xz_build_goodbye(client->channel.session_id);
    if (!msg) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = xz_mqtt_udp_send_text(client, msg);
    cJSON_free(msg);
    return rt;
}

static OPERATE_RET xz_mqtt_udp_parse_hello(xz_mqtt_udp_client_t *client, cJSON *root)
{
    if (!client || !root) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = xz_parse_server_hello(root, "udp", &client->channel);
    if (rt != OPRT_OK) {
        return rt;
    }

    cJSON *udp = cJSON_GetObjectItem(root, "udp");
    if (!cJSON_IsObject(udp)) {
        return OPRT_INVALID_PARM;
    }

    cJSON *server = cJSON_GetObjectItem(udp, "server");
    cJSON *port   = cJSON_GetObjectItem(udp, "port");
    cJSON *key    = cJSON_GetObjectItem(udp, "key");
    cJSON *nonce  = cJSON_GetObjectItem(udp, "nonce");

    if (!cJSON_IsString(server) || !cJSON_IsNumber(port) || !cJSON_IsString(key) || !cJSON_IsString(nonce)) {
        return OPRT_INVALID_PARM;
    }

    if ((size_t)snprintf(client->udp_server, sizeof(client->udp_server), "%s", server->valuestring) >=
        sizeof(client->udp_server)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    client->udp_port = (uint16_t)port->valueint;

    uint8_t aes_key[16] = {0};
    if (!xz_hex_decode(key->valuestring, aes_key, sizeof(aes_key))) {
        return OPRT_INVALID_PARM;
    }
    if (!xz_hex_decode(nonce->valuestring, client->aes_nonce, sizeof(client->aes_nonce))) {
        return OPRT_INVALID_PARM;
    }

    mbedtls_aes_free(&client->aes_ctx);
    mbedtls_aes_init(&client->aes_ctx);
    if (mbedtls_aes_setkey_enc(&client->aes_ctx, aes_key, 128) != 0) {
        return OPRT_COM_ERROR;
    }

    client->local_sequence  = 0;
    client->remote_sequence = 0;
    client->aes_ready       = TRUE;

    return OPRT_OK;
}

static void xz_mqtt_on_connected(void *mqtt_client, void *userdata)
{
    (void)mqtt_client;
    xz_mqtt_udp_client_t *client = (xz_mqtt_udp_client_t *)userdata;
    if (!client) {
        return;
    }
    client->mqtt_connected = TRUE;
    PR_NOTICE("mqtt connected");
}

static void xz_mqtt_on_disconnected(void *mqtt_client, void *userdata)
{
    (void)mqtt_client;
    xz_mqtt_udp_client_t *client = (xz_mqtt_udp_client_t *)userdata;
    if (!client) {
        return;
    }

    client->mqtt_connected  = FALSE;
    client->mqtt_subscribed = FALSE;
    client->state           = XZ_CONN_ERROR;
    PR_WARN("mqtt disconnected");
}

static void xz_mqtt_on_message(void *mqtt_client, uint16_t msgid, const mqtt_client_message_t *msg, void *userdata)
{
    (void)mqtt_client;
    (void)msgid;

    xz_mqtt_udp_client_t *client = (xz_mqtt_udp_client_t *)userdata;
    if (!client || !msg || !msg->payload || msg->length == 0) {
        return;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)msg->payload, msg->length);
    if (!root) {
        PR_ERR("mqtt parse json failed");
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type) || !type->valuestring) {
        cJSON_Delete(root);
        return;
    }

    if (strcmp(type->valuestring, "hello") == 0) {
        if (xz_mqtt_udp_parse_hello(client, root) == OPRT_OK) {
            PR_NOTICE("mqtt hello ok sid=%s udp=%s:%u", client->channel.session_id, client->udp_server,
                      client->udp_port);
        }
    } else if (strcmp(type->valuestring, "goodbye") == 0) {
        cJSON *sid                 = cJSON_GetObjectItem(root, "session_id");
        client->channel.last_rx_ms = tal_system_get_millisecond();
        PR_NOTICE("mqtt recv goodbye sid=%s", cJSON_IsString(sid) ? sid->valuestring : "null");
        if (!cJSON_IsString(sid) || strcmp(client->channel.session_id, sid->valuestring) == 0) {
            xz_mqtt_udp_close_udp(client);
            client->channel.ready = FALSE;
            client->state         = XZ_CONN_ERROR;
        }
    } else {
        client->channel.last_rx_ms = tal_system_get_millisecond();
        PR_NOTICE("mqtt recv text type=%s", type->valuestring);
        if (client->on_text_message) {
            client->on_text_message(client->on_text_userdata, (const uint8_t *)msg->payload, msg->length);
        }
    }

    cJSON_Delete(root);
}

static void xz_mqtt_on_subscribed(void *mqtt_client, uint16_t msgid, void *userdata)
{
    (void)mqtt_client;
    (void)msgid;
    xz_mqtt_udp_client_t *client = (xz_mqtt_udp_client_t *)userdata;
    if (!client) {
        return;
    }

    client->mqtt_subscribed = TRUE;
    PR_NOTICE("mqtt subscribed topic=%s", client->subscribe_topic);
}

static OPERATE_RET xz_mqtt_udp_open_udp(xz_mqtt_udp_client_t *client)
{
    if (!client || !client->aes_ready || client->udp_server[0] == '\0' || client->udp_port == 0) {
        return OPRT_INVALID_PARM;
    }

    if (client->udp_fd >= 0) {
        (void)tal_net_close(client->udp_fd);
        client->udp_fd = -1;
    }

    client->udp_fd = tal_net_socket_create(PROTOCOL_UDP);
    if (client->udp_fd < 0) {
        client->udp_fd = -1;
        return OPRT_SOCK_ERR;
    }

    (void)tal_net_set_timeout(client->udp_fd, 20, TRANS_RECV);
    (void)tal_net_set_timeout(client->udp_fd, 1000, TRANS_SEND);

    TUYA_IP_ADDR_T addr     = tal_net_str2addr(client->udp_server);
    BOOL_T         need_dns = FALSE;
#if defined(ENABLE_IPv6) && (ENABLE_IPv6 == 1)
    if (addr.ipaddr4 == 0 && addr.type == TY_AF_INET) {
        need_dns = TRUE;
    }
#else
    if (addr == 0) {
        need_dns = TRUE;
    }
#endif

    if (need_dns) {
        if (tal_net_gethostbyname(client->udp_server, &addr) != OPRT_OK) {
            (void)tal_net_close(client->udp_fd);
            client->udp_fd = -1;
            return OPRT_COM_ERROR;
        }
    }

    if (tal_net_connect(client->udp_fd, addr, client->udp_port) != 0) {
        (void)tal_net_close(client->udp_fd);
        client->udp_fd = -1;
        return OPRT_COM_ERROR;
    }

    PR_NOTICE("udp connected %s:%u", client->udp_server, client->udp_port);
    return OPRT_OK;
}

OPERATE_RET xz_mqtt_udp_init(xz_mqtt_udp_client_t *client)
{
    if (!client) {
        return OPRT_INVALID_PARM;
    }

    memset(client, 0, sizeof(*client));
    client->udp_fd    = -1;
    client->keepalive = 240;
    client->port      = 8883;
    client->state     = XZ_CONN_IDLE;
    xz_channel_reset(&client->channel);
    mbedtls_aes_init(&client->aes_ctx);

    return OPRT_OK;
}

OPERATE_RET xz_mqtt_udp_deinit(xz_mqtt_udp_client_t *client)
{
    if (!client) {
        return OPRT_INVALID_PARM;
    }

    (void)xz_mqtt_udp_close(client, FALSE);
    mbedtls_aes_free(&client->aes_ctx);
    return OPRT_OK;
}

OPERATE_RET xz_mqtt_udp_connect(xz_mqtt_udp_client_t *client, const char *endpoint, const char *client_id,
                                const char *username, const char *password, const char *publish_topic,
                                const char *subscribe_topic, int keepalive)
{
    if (!client || !endpoint || !client_id || !publish_topic) {
        return OPRT_INVALID_PARM;
    }

    (void)xz_mqtt_udp_close(client, FALSE);

    if (xz_parse_endpoint(endpoint, client->host, sizeof(client->host), &client->port) != OPRT_OK) {
        client->state = XZ_CONN_ERROR;
        return OPRT_INVALID_PARM;
    }

    (void)snprintf(client->endpoint, sizeof(client->endpoint), "%s", endpoint);
    (void)snprintf(client->client_id, sizeof(client->client_id), "%s", client_id);
    (void)snprintf(client->username, sizeof(client->username), "%s", username ? username : "");
    (void)snprintf(client->password, sizeof(client->password), "%s", password ? password : "");
    (void)snprintf(client->publish_topic, sizeof(client->publish_topic), "%s", publish_topic);
    if (subscribe_topic && subscribe_topic[0] != '\0') {
        (void)snprintf(client->subscribe_topic, sizeof(client->subscribe_topic), "%s", subscribe_topic);
    } else {
        (void)snprintf(client->subscribe_topic, sizeof(client->subscribe_topic), "%s", publish_topic);
    }
    client->keepalive = (keepalive > 0) ? (uint16_t)keepalive : 240;

    client->mqtt = mqtt_client_new();
    if (!client->mqtt) {
        client->state = XZ_CONN_ERROR;
        return OPRT_MALLOC_FAILED;
    }

    client->cacert     = NULL;
    client->cacert_len = 0;
    if (tuya_iotdns_query_host_certs(client->host, client->port, &client->cacert, &client->cacert_len) != OPRT_OK) {
        client->cacert     = NULL;
        client->cacert_len = 0;
    }

    mqtt_client_config_t cfg = {
        .cacert          = client->cacert,
        .cacert_len      = client->cacert_len,
        .host            = client->host,
        .port            = client->port,
        .keepalive       = client->keepalive,
        .timeout_ms      = (uint32_t)xz_env_int("XZ_MQTT_TIMEOUT_MS", XZ_MQTT_TIMEOUT_MS_DEFAULT),
        .clientid        = client->client_id,
        .username        = client->username,
        .password        = client->password,
        .userdata        = client,
        .on_connected    = xz_mqtt_on_connected,
        .on_disconnected = xz_mqtt_on_disconnected,
        .on_message      = xz_mqtt_on_message,
        .on_subscribed   = xz_mqtt_on_subscribed,
    };

    mqtt_client_status_t ms = mqtt_client_init(client->mqtt, &cfg);
    if (ms != MQTT_STATUS_SUCCESS) {
        xz_mqtt_udp_close_mqtt(client);
        client->state = XZ_CONN_ERROR;
        return OPRT_COM_ERROR;
    }

    ms = mqtt_client_connect(client->mqtt);
    if (ms != MQTT_STATUS_SUCCESS) {
        xz_mqtt_udp_close_mqtt(client);
        client->state = XZ_CONN_ERROR;
        return OPRT_COM_ERROR;
    }

    uint16_t sub_id = mqtt_client_subscribe(client->mqtt, client->subscribe_topic, MQTT_QOS_0);
    if (sub_id == 0) {
        xz_mqtt_udp_close_mqtt(client);
        client->state = XZ_CONN_ERROR;
        return OPRT_COM_ERROR;
    }

    client->state = XZ_CONN_CONNECTING;
    xz_channel_reset(&client->channel);

    PR_NOTICE("mqtt connected host=%s port=%u timeout_ms=%u pub=%s sub=%s", client->host, client->port,
              (unsigned)cfg.timeout_ms, client->publish_topic, client->subscribe_topic);
    return OPRT_OK;
}

OPERATE_RET xz_mqtt_udp_open_audio_channel(xz_mqtt_udp_client_t *client, uint32_t timeout_ms)
{
    if (!client || !client->mqtt || !client->mqtt_connected) {
        return OPRT_INVALID_PARM;
    }

    xz_channel_reset(&client->channel);
#if defined(CONFIG_USE_SERVER_AEC) && (CONFIG_USE_SERVER_AEC == 1)
    char *hello = xz_build_hello("udp", 3, TRUE, TRUE);
#else
    char *hello = xz_build_hello("udp", 3, TRUE, FALSE);
#endif
    if (!hello) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = xz_mqtt_udp_send_text(client, hello);
    cJSON_free(hello);
    if (rt != OPRT_OK) {
        client->state = XZ_CONN_ERROR;
        return rt;
    }

    uint32_t begin_ms = tal_system_get_millisecond();
    while ((uint32_t)(tal_system_get_millisecond() - begin_ms) < timeout_ms) {
        rt = xz_mqtt_udp_poll(client, 100);
        if (rt != OPRT_OK && rt != OPRT_RESOURCE_NOT_READY) {
            client->state = XZ_CONN_ERROR;
            return rt;
        }
        if (client->channel.ready && client->aes_ready) {
            rt = xz_mqtt_udp_open_udp(client);
            if (rt != OPRT_OK) {
                client->state = XZ_CONN_ERROR;
                return rt;
            }
            client->state = XZ_CONN_READY;
            return OPRT_OK;
        }
    }

    client->state = XZ_CONN_ERROR;
    return OPRT_TIMEOUT;
}

OPERATE_RET xz_mqtt_udp_poll(xz_mqtt_udp_client_t *client, int wait_ms)
{
    if (!client || !client->mqtt) {
        return OPRT_INVALID_PARM;
    }

    mqtt_client_status_t ms = mqtt_client_yield(client->mqtt);
    if (ms != MQTT_STATUS_SUCCESS) {
        client->state = XZ_CONN_ERROR;
        return OPRT_COM_ERROR;
    }

    if (client->udp_fd < 0 || !client->aes_ready) {
        return OPRT_OK;
    }

    if (wait_ms > 0) {
        tal_system_sleep(wait_ms);
    }

    uint8_t packet[2048] = {0};
    int     n            = tal_net_recv(client->udp_fd, packet, sizeof(packet));
    if (n <= 0) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (n < XZ_UDP_HDR_LEN) {
        return OPRT_OK;
    }
    if (packet[0] != 0x01) {
        return OPRT_OK;
    }

    uint16_t payload_len = (uint16_t)(((uint16_t)packet[2] << 8) | packet[3]);
    if ((int)(XZ_UDP_HDR_LEN + payload_len) > n || payload_len == 0) {
        return OPRT_OK;
    }

    uint32_t sequence = ((uint32_t)packet[12] << 24) | ((uint32_t)packet[13] << 16) | ((uint32_t)packet[14] << 8) |
                        (uint32_t)packet[15];
    if (sequence < client->remote_sequence) {
        PR_WARN("udp old seq=%u expect>=%u", sequence, client->remote_sequence);
        return OPRT_OK;
    }

    uint8_t *plain = tal_malloc(payload_len);
    if (!plain) {
        return OPRT_MALLOC_FAILED;
    }

    size_t  nc_off           = 0;
    uint8_t stream_block[16] = {0};
    int     aes_rt           = mbedtls_aes_crypt_ctr(&client->aes_ctx, payload_len, &nc_off, packet, stream_block,
                                                     packet + XZ_UDP_HDR_LEN, plain);
    if (aes_rt != 0) {
        tal_free(plain);
        return OPRT_COM_ERROR;
    }

    tal_free(plain);
    client->remote_sequence    = sequence;
    client->channel.last_rx_ms = tal_system_get_millisecond();
    return OPRT_OK;
}

OPERATE_RET xz_mqtt_udp_close(xz_mqtt_udp_client_t *client, BOOL_T send_goodbye)
{
    if (!client) {
        return OPRT_INVALID_PARM;
    }

    if (send_goodbye) {
        (void)xz_mqtt_udp_send_goodbye(client);
    }

    xz_mqtt_udp_close_udp(client);
    xz_mqtt_udp_close_mqtt(client);

    client->state = XZ_CONN_IDLE;
    xz_channel_reset(&client->channel);
    return OPRT_OK;
}

OPERATE_RET xz_mqtt_udp_send_text(xz_mqtt_udp_client_t *client, const char *text)
{
    if (!client || !client->mqtt || !client->mqtt_connected || !text || client->publish_topic[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    uint16_t msgid =
        mqtt_client_publish(client->mqtt, client->publish_topic, (const uint8_t *)text, strlen(text), MQTT_QOS_0);
    if (msgid == 0) {
        return OPRT_SEND_ERR;
    }

    return OPRT_OK;
}

OPERATE_RET xz_mqtt_udp_send_listen(xz_mqtt_udp_client_t *client, const char *state, const char *mode, const char *text)
{
    if (!client) {
        return OPRT_INVALID_PARM;
    }

    char *msg = xz_build_listen(client->channel.session_id, state, mode, text);
    if (!msg) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = xz_mqtt_udp_send_text(client, msg);
    cJSON_free(msg);
    return rt;
}

OPERATE_RET xz_mqtt_udp_send_abort(xz_mqtt_udp_client_t *client, const char *reason)
{
    if (!client) {
        return OPRT_INVALID_PARM;
    }

    char *msg = xz_build_abort(client->channel.session_id, reason);
    if (!msg) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = xz_mqtt_udp_send_text(client, msg);
    cJSON_free(msg);
    return rt;
}

OPERATE_RET xz_mqtt_udp_send_mcp(xz_mqtt_udp_client_t *client, const char *payload_json)
{
    if (!client) {
        return OPRT_INVALID_PARM;
    }

    char *msg = xz_build_mcp(client->channel.session_id, payload_json);
    if (!msg) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = xz_mqtt_udp_send_text(client, msg);
    cJSON_free(msg);
    return rt;
}

OPERATE_RET xz_mqtt_udp_send_audio(xz_mqtt_udp_client_t *client, const uint8_t *opus_data, size_t opus_len,
                                   uint32_t timestamp)
{
    if (!client || client->udp_fd < 0 || !client->aes_ready || !opus_data || opus_len == 0 || opus_len > 0xFFFF) {
        return OPRT_INVALID_PARM;
    }

    size_t   total  = XZ_UDP_HDR_LEN + opus_len;
    uint8_t *packet = tal_malloc(total);
    if (!packet) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(packet, client->aes_nonce, XZ_UDP_NONCE_LEN);
    packet[2] = (uint8_t)((opus_len >> 8) & 0xFF);
    packet[3] = (uint8_t)(opus_len & 0xFF);

    uint32_t ts = timestamp;
    packet[8]   = (uint8_t)((ts >> 24) & 0xFF);
    packet[9]   = (uint8_t)((ts >> 16) & 0xFF);
    packet[10]  = (uint8_t)((ts >> 8) & 0xFF);
    packet[11]  = (uint8_t)(ts & 0xFF);

    uint32_t seq = ++client->local_sequence;
    packet[12]   = (uint8_t)((seq >> 24) & 0xFF);
    packet[13]   = (uint8_t)((seq >> 16) & 0xFF);
    packet[14]   = (uint8_t)((seq >> 8) & 0xFF);
    packet[15]   = (uint8_t)(seq & 0xFF);

    size_t  nc_off           = 0;
    uint8_t stream_block[16] = {0};
    int     aes_rt = mbedtls_aes_crypt_ctr(&client->aes_ctx, opus_len, &nc_off, packet, stream_block, opus_data,
                                           packet + XZ_UDP_HDR_LEN);
    if (aes_rt != 0) {
        tal_free(packet);
        return OPRT_COM_ERROR;
    }

    int n = tal_net_send(client->udp_fd, packet, total);
    tal_free(packet);
    if (n != (int)total) {
        return OPRT_SEND_ERR;
    }

    return OPRT_OK;
}

OPERATE_RET xz_mqtt_udp_set_text_message_callback(xz_mqtt_udp_client_t *client, xz_text_message_cb_t cb, void *userdata)
{
    if (!client) {
        return OPRT_INVALID_PARM;
    }

    client->on_text_message  = cb;
    client->on_text_userdata = userdata;
    return OPRT_OK;
}
