/**
 * @file xiaozhi_ws.c
 * @brief WebSocket implementation aligned with xiaozhi-esp32 websocket protocol.
 */

#include "xiaozhi_ws.h"

#include "iotdns.h"
#include "tal_api.h"
#include "xiaozhi_ws_handshake.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define XZ_WS_RX_BUF_SIZE   8192
#define XZ_WS_IO_TIMEOUT_MS 5000

static int xz_find_header_end(const uint8_t *buf, size_t len)
{
    if (!buf || len < 4) {
        return -1;
    }

    for (size_t i = 0; i + 3 < len; ++i) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return (int)(i + 4);
        }
    }

    return -1;
}

static uint16_t xz_parse_http_status(const char *header)
{
    if (!header) {
        return 0;
    }

    int status = 0;
    (void)sscanf(header, "HTTP/%*d.%*d %d", &status);
    return (uint16_t)status;
}

static OPERATE_RET xz_parse_ws_url(const char *url, char *host, size_t host_size, uint16_t *port, char *path,
                                   size_t path_size, BOOL_T *use_tls)
{
    if (!url || !host || !port || !path || !use_tls || host_size == 0 || path_size == 0) {
        return OPRT_INVALID_PARM;
    }

    const char *p            = NULL;
    uint16_t    default_port = 0;
    if (strncmp(url, "wss://", 6) == 0) {
        p            = url + 6;
        default_port = 443;
        *use_tls     = TRUE;
    } else if (strncmp(url, "ws://", 5) == 0) {
        p            = url + 5;
        default_port = 80;
        *use_tls     = FALSE;
    } else {
        return OPRT_INVALID_PARM;
    }

    const char *host_begin = p;
    while (*p && *p != '/' && *p != '?') {
        p++;
    }

    const char *host_end = p;
    const char *colon    = NULL;
    for (const char *q = host_begin; q < host_end; ++q) {
        if (*q == ':') {
            colon = q;
            break;
        }
    }

    if (colon) {
        size_t host_len = (size_t)(colon - host_begin);
        if (host_len == 0 || host_len >= host_size) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }

        memcpy(host, host_begin, host_len);
        host[host_len] = '\0';

        int parsed_port = atoi(colon + 1);
        if (parsed_port <= 0 || parsed_port > 65535) {
            return OPRT_INVALID_PARM;
        }
        *port = (uint16_t)parsed_port;
    } else {
        size_t host_len = (size_t)(host_end - host_begin);
        if (host_len == 0 || host_len >= host_size) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }

        memcpy(host, host_begin, host_len);
        host[host_len] = '\0';
        *port          = default_port;
    }

    if (*p == '\0') {
        (void)snprintf(path, path_size, "/");
    } else {
        (void)snprintf(path, path_size, "%s", p);
    }

    return OPRT_OK;
}

static OPERATE_RET xz_ws_generate_client_key(char out[29])
{
    if (!out) {
        return OPRT_INVALID_PARM;
    }

    uint8_t random_key[16] = {0};
    for (size_t i = 0; i < sizeof(random_key); ++i) {
        random_key[i] = (uint8_t)(tal_system_get_random(0xFFU) & 0xFFU);
    }

    size_t olen = 0;
    int    rc   = mbedtls_base64_encode((unsigned char *)out, 29, &olen, random_key, sizeof(random_key));
    if (rc != 0 || olen == 0 || olen >= 29) {
        return OPRT_COM_ERROR;
    }

    out[olen] = '\0';
    return OPRT_OK;
}

static OPERATE_RET xz_ws_compute_accept_key(const char *client_key, char out[64])
{
    if (!client_key || !out) {
        return OPRT_INVALID_PARM;
    }

    const char magic[]     = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char       merged[128] = {0};
    int        n           = snprintf(merged, sizeof(merged), "%s%s", client_key, magic);
    if (n <= 0 || n >= (int)sizeof(merged)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    unsigned char sha1[20] = {0};
    if (mbedtls_sha1((const unsigned char *)merged, (size_t)n, sha1) != 0) {
        return OPRT_COM_ERROR;
    }

    size_t olen = 0;
    int    rc   = mbedtls_base64_encode((unsigned char *)out, 64, &olen, sha1, sizeof(sha1));
    if (rc != 0 || olen == 0 || olen >= 64) {
        return OPRT_COM_ERROR;
    }

    out[olen] = '\0';
    return OPRT_OK;
}

static OPERATE_RET xz_http_get_header_value(const char *headers, const char *name, char *out, size_t out_size)
{
    if (!headers || !name || !out || out_size == 0) {
        return OPRT_INVALID_PARM;
    }

    const size_t name_len = strlen(name);
    const char  *p        = headers;
    while ((p = strstr(p, name)) != NULL) {
        if (p == headers || *(p - 1) == '\n') {
            const char *v = p + name_len;
            while (*v == ' ' || *v == '\t') {
                v++;
            }
            const char *end = strstr(v, "\r\n");
            if (!end) {
                return OPRT_COM_ERROR;
            }
            size_t len = (size_t)(end - v);
            if (len >= out_size) {
                return OPRT_BUFFER_NOT_ENOUGH;
            }
            memcpy(out, v, len);
            out[len] = '\0';
            return OPRT_OK;
        }
        p += name_len;
    }

    return OPRT_NOT_FOUND;
}

static void xz_ws_conn_cleanup(xz_ws_client_t *ws)
{
    if (!ws) {
        return;
    }

    if (ws->tls) {
        (void)tuya_tls_disconnect(ws->tls);
        tuya_tls_connect_destroy(ws->tls);
        ws->tls = NULL;
    }

    if (ws->tcp) {
        (void)tuya_transporter_close(ws->tcp);
        (void)tuya_transporter_destroy(ws->tcp);
        ws->tcp = NULL;
    }

    ws->socket_fd = -1;
    ws->rx_len    = 0;
}

static OPERATE_RET xz_ws_conn_open(xz_ws_client_t *ws, const char *host, uint16_t port, BOOL_T use_tls)
{
    if (!ws || !host || port == 0) {
        return OPRT_INVALID_PARM;
    }

    ws->tcp = tuya_transporter_create(TRANSPORT_TYPE_TCP, NULL);
    if (!ws->tcp) {
        return OPRT_MALLOC_FAILED;
    }

    tuya_tcp_config_t tcp_cfg = {0};
    tcp_cfg.isReuse           = TRUE;
    tcp_cfg.isDisableNagle    = TRUE;
    tcp_cfg.recvTimeoutMs     = XZ_WS_IO_TIMEOUT_MS;
    tcp_cfg.sendTimeoutMs     = XZ_WS_IO_TIMEOUT_MS;
    (void)tuya_transporter_ctrl(ws->tcp, TUYA_TRANSPORTER_SET_TCP_CONFIG, &tcp_cfg);

    OPERATE_RET rt = tuya_transporter_connect(ws->tcp, host, port, XZ_WS_IO_TIMEOUT_MS);
    if (rt != OPRT_OK) {
        xz_ws_conn_cleanup(ws);
        return rt;
    }

    rt = tuya_transporter_ctrl(ws->tcp, TUYA_TRANSPORTER_GET_TCP_SOCKET, &ws->socket_fd);
    if (rt != OPRT_OK || ws->socket_fd < 0) {
        xz_ws_conn_cleanup(ws);
        return OPRT_SOCK_ERR;
    }

    ws->use_tls = use_tls;
    if (!use_tls) {
        return OPRT_OK;
    }

    uint8_t *cacert     = NULL;
    uint16_t cacert_len = 0;
    BOOL_T   verify     = FALSE;
    if (tuya_iotdns_query_domain_certs((char *)host, &cacert, &cacert_len) == OPRT_OK && cacert && cacert_len > 0) {
        verify = TRUE;
    } else {
        PR_WARN("iotdns cert not found for host=%s, fallback to insecure tls", host);
    }

    ws->tls = tuya_tls_connect_create();
    if (!ws->tls) {
        if (cacert) {
            tal_free(cacert);
        }
        xz_ws_conn_cleanup(ws);
        return OPRT_MALLOC_FAILED;
    }

    tuya_tls_config_t tls_cfg = {
        .mode         = TUYA_TLS_SERVER_CERT_MODE,
        .hostname     = (char *)host,
        .port         = port,
        .timeout      = XZ_WS_IO_TIMEOUT_MS / 1000,
        .verify       = verify ? true : false,
        .ca_cert      = verify ? (char *)cacert : NULL,
        .ca_cert_size = verify ? (int)cacert_len : 0,
    };
    (void)tuya_tls_config_set(ws->tls, &tls_cfg);

    rt = tuya_tls_connect(ws->tls, (char *)host, port, ws->socket_fd, XZ_WS_IO_TIMEOUT_MS / 1000);
    if (cacert) {
        tal_free(cacert);
    }
    if (rt != OPRT_OK) {
        xz_ws_conn_cleanup(ws);
        return rt;
    }

    return OPRT_OK;
}

static int xz_ws_conn_write(xz_ws_client_t *ws, const uint8_t *data, int len)
{
    if (!ws || !data || len <= 0) {
        return -1;
    }

    int sent = 0;
    while (sent < len) {
        int n = 0;
        if (ws->use_tls) {
            n = tuya_tls_write(ws->tls, (uint8_t *)data + sent, (uint32_t)(len - sent));
        } else {
            n = tuya_transporter_write(ws->tcp, (uint8_t *)data + sent, len - sent, XZ_WS_IO_TIMEOUT_MS);
        }

        if (n <= 0) {
            return -1;
        }
        sent += n;
    }

    return sent;
}

static int xz_ws_conn_read(xz_ws_client_t *ws, uint8_t *buf, int len, int timeout_ms)
{
    if (!ws || !buf || len <= 0 || timeout_ms < 0) {
        return -1;
    }

    if (!ws->use_tls) {
        return tuya_transporter_read(ws->tcp, buf, len, timeout_ms);
    }

    TUYA_FD_SET_T readfds;
    tal_net_fd_zero(&readfds);
    tal_net_fd_set(ws->socket_fd, &readfds);

    int ready = tal_net_select(ws->socket_fd + 1, &readfds, NULL, NULL, timeout_ms);
    if (ready < 0) {
        return -1;
    }
    if (ready == 0) {
        return OPRT_RESOURCE_NOT_READY;
    }

    int n = tuya_tls_read(ws->tls, buf, (uint32_t)len);
    if (n == OPRT_RESOURCE_NOT_READY) {
        return OPRT_RESOURCE_NOT_READY;
    }

    return n;
}

static OPERATE_RET xz_ws_send_frame(xz_ws_client_t *ws, uint8_t opcode, const uint8_t *payload, size_t payload_len)
{
    if (!ws || (payload_len > 0 && !payload)) {
        return OPRT_INVALID_PARM;
    }

    uint8_t header[14] = {0};
    size_t  header_len = 0;

    header[0] = (uint8_t)(0x80 | (opcode & 0x0F));
    if (payload_len <= 125) {
        header[1]  = (uint8_t)(0x80 | payload_len);
        header_len = 2;
    } else if (payload_len <= 0xFFFF) {
        header[1]  = (uint8_t)(0x80 | 126);
        header[2]  = (uint8_t)((payload_len >> 8) & 0xFF);
        header[3]  = (uint8_t)(payload_len & 0xFF);
        header_len = 4;
    } else {
        header[1]       = (uint8_t)(0x80 | 127);
        uint64_t plen64 = (uint64_t)payload_len;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (uint8_t)((plen64 >> (56 - i * 8)) & 0xFF);
        }
        header_len = 10;
    }

    uint32_t m       = (uint32_t)tal_system_get_random(0xFFFFFFFFU);
    uint8_t  mask[4] = {
         (uint8_t)(m & 0xFF),
         (uint8_t)((m >> 8) & 0xFF),
         (uint8_t)((m >> 16) & 0xFF),
         (uint8_t)((m >> 24) & 0xFF),
    };

    size_t   frame_len = header_len + 4 + payload_len;
    uint8_t *frame     = tal_malloc(frame_len);
    if (!frame) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(frame, header, header_len);
    memcpy(frame + header_len, mask, sizeof(mask));
    for (size_t i = 0; i < payload_len; ++i) {
        frame[header_len + 4 + i] = (uint8_t)(payload[i] ^ mask[i % 4]);
    }

    int n = xz_ws_conn_write(ws, frame, (int)frame_len);
    tal_free(frame);
    if (n != (int)frame_len) {
        return OPRT_SEND_ERR;
    }

    return OPRT_OK;
}

static OPERATE_RET xz_ws_handshake(xz_ws_client_t *ws)
{
    char req[1400]  = {0};
    char ws_key[29] = {0};
    if (xz_ws_generate_client_key(ws_key) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }

    int req_len = xz_ws_format_handshake_request(req, sizeof(req), ws->path, ws->host, ws->port, ws_key, ws->version,
                                                 ws->device_id, ws->client_id, ws->token);
    if (req_len <= 0 || req_len >= (int)sizeof(req)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    if (xz_ws_conn_write(ws, (const uint8_t *)req, req_len) != req_len) {
        return OPRT_SEND_ERR;
    }

    char     header[4096] = {0};
    int      total        = 0;
    int      header_end   = -1;
    uint32_t begin_ms     = tal_system_get_millisecond();
    while ((int32_t)(tal_system_get_millisecond() - begin_ms) < XZ_WS_IO_TIMEOUT_MS &&
           total < (int)sizeof(header) - 1) {
        int n = xz_ws_conn_read(ws, (uint8_t *)header + total, (int)sizeof(header) - total - 1, 500);
        if (n == OPRT_RESOURCE_NOT_READY) {
            continue;
        }
        if (n <= 0) {
            return OPRT_COM_ERROR;
        }

        total += n;
        header[total] = '\0';
        header_end    = xz_find_header_end((const uint8_t *)header, (size_t)total);
        if (header_end > 0) {
            break;
        }
    }

    if (header_end <= 0) {
        return OPRT_TIMEOUT;
    }

    uint16_t status = xz_parse_http_status(header);
    if (status != 101) {
        PR_ERR("websocket handshake failed, http=%u", status);
        return OPRT_COM_ERROR;
    }

    char server_accept[96] = {0};
    if (xz_http_get_header_value(header, "Sec-WebSocket-Accept:", server_accept, sizeof(server_accept)) != OPRT_OK) {
        PR_ERR("websocket handshake failed, missing Sec-WebSocket-Accept");
        return OPRT_COM_ERROR;
    }

    char expect_accept[64] = {0};
    if (xz_ws_compute_accept_key(ws_key, expect_accept) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    if (strcmp(server_accept, expect_accept) != 0) {
        PR_ERR("websocket handshake failed, invalid accept key");
        return OPRT_COM_ERROR;
    }

    size_t remain = (size_t)(total - header_end);
    ws->rx_len    = 0;
    if (remain > 0) {
        if (remain > ws->rx_cap) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        memcpy(ws->rx_buf, header + header_end, remain);
        ws->rx_len = remain;
    }

    return OPRT_OK;
}

static void xz_ws_consume_rx(xz_ws_client_t *ws, size_t consumed)
{
    if (!ws || consumed == 0 || consumed > ws->rx_len) {
        return;
    }

    if (consumed < ws->rx_len) {
        memmove(ws->rx_buf, ws->rx_buf + consumed, ws->rx_len - consumed);
    }
    ws->rx_len -= consumed;
}

static OPERATE_RET xz_ws_decode_one_frame(xz_ws_client_t *ws, uint8_t *opcode, uint8_t **payload, size_t *payload_len,
                                          size_t *consumed)
{
    if (!ws || !opcode || !payload || !payload_len || !consumed) {
        return OPRT_INVALID_PARM;
    }

    if (ws->rx_len < 2) {
        return OPRT_RESOURCE_NOT_READY;
    }

    const uint8_t *buf    = ws->rx_buf;
    BOOL_T         masked = ((buf[1] & 0x80) != 0) ? TRUE : FALSE;
    uint64_t       plen   = (uint64_t)(buf[1] & 0x7F);
    size_t         off    = 2;

    if (plen == 126) {
        if (ws->rx_len < off + 2) {
            return OPRT_RESOURCE_NOT_READY;
        }
        plen = (uint64_t)((buf[off] << 8) | buf[off + 1]);
        off += 2;
    } else if (plen == 127) {
        if (ws->rx_len < off + 8) {
            return OPRT_RESOURCE_NOT_READY;
        }
        plen = 0;
        for (int i = 0; i < 8; ++i) {
            plen = (plen << 8) | buf[off + i];
        }
        off += 8;
    }

    if (plen > (uint64_t)(ws->rx_cap - 16)) {
        return OPRT_MSG_OUT_OF_LIMIT;
    }

    if (masked && ws->rx_len < off + 4) {
        return OPRT_RESOURCE_NOT_READY;
    }

    size_t frame_len = off + (masked ? 4 : 0) + (size_t)plen;
    if (ws->rx_len < frame_len) {
        return OPRT_RESOURCE_NOT_READY;
    }

    uint8_t mask[4] = {0};
    if (masked) {
        memcpy(mask, buf + off, sizeof(mask));
        off += sizeof(mask);
    }

    uint8_t *data = tal_malloc((size_t)plen + 1);
    if (!data) {
        return OPRT_MALLOC_FAILED;
    }

    if (plen > 0) {
        memcpy(data, buf + off, (size_t)plen);
        if (masked) {
            for (size_t i = 0; i < (size_t)plen; ++i) {
                data[i] ^= mask[i % 4];
            }
        }
    }
    data[plen] = '\0';

    *opcode      = (uint8_t)(buf[0] & 0x0F);
    *payload     = data;
    *payload_len = (size_t)plen;
    *consumed    = frame_len;

    return OPRT_OK;
}

static OPERATE_RET xz_ws_handle_text(xz_ws_client_t *ws, const uint8_t *payload, size_t payload_len)
{
    if (!ws || !payload || payload_len == 0) {
        return OPRT_INVALID_PARM;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    if (!root) {
        return OPRT_CR_CJSON_ERR;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(type) && type->valuestring) {
        if (strcmp(type->valuestring, "hello") == 0) {
            (void)xz_parse_server_hello(root, "websocket", &ws->channel);
            PR_NOTICE("websocket hello ok, sid=%s sr=%d frame=%d", ws->channel.session_id,
                      ws->channel.server_sample_rate, ws->channel.server_frame_duration);
        } else {
            ws->channel.last_rx_ms = tal_system_get_millisecond();
            PR_DEBUG("websocket text type=%s", type->valuestring);
            if (ws->on_text_message) {
                ws->on_text_message(ws->on_text_userdata, payload, payload_len);
            }
        }
    }

    cJSON_Delete(root);
    return OPRT_OK;
}

OPERATE_RET xz_ws_init(xz_ws_client_t *ws)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    memset(ws, 0, sizeof(*ws));
    ws->socket_fd = -1;
    ws->version   = 1;
    ws->rx_cap    = XZ_WS_RX_BUF_SIZE;
    ws->rx_buf    = tal_malloc(ws->rx_cap);
    if (!ws->rx_buf) {
        return OPRT_MALLOC_FAILED;
    }

    ws->state = XZ_CONN_IDLE;
    xz_channel_reset(&ws->channel);

    return OPRT_OK;
}

OPERATE_RET xz_ws_deinit(xz_ws_client_t *ws)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    (void)xz_ws_close(ws);
    if (ws->rx_buf) {
        tal_free(ws->rx_buf);
        ws->rx_buf = NULL;
    }
    ws->rx_cap = 0;

    return OPRT_OK;
}

OPERATE_RET xz_ws_connect(xz_ws_client_t *ws, const char *url, const char *token, int version, const char *device_id,
                          const char *client_id)
{
    if (!ws || !url || !device_id || !client_id) {
        return OPRT_INVALID_PARM;
    }

    (void)xz_ws_close(ws);
    ws->state = XZ_CONN_CONNECTING;
    xz_channel_reset(&ws->channel);

    (void)snprintf(ws->url, sizeof(ws->url), "%s", url);
    (void)snprintf(ws->token, sizeof(ws->token), "%s", token ? token : "");
    (void)snprintf(ws->device_id, sizeof(ws->device_id), "%s", device_id);
    (void)snprintf(ws->client_id, sizeof(ws->client_id), "%s", client_id);
    ws->version = (version > 0) ? version : 1;

    OPERATE_RET rt =
        xz_parse_ws_url(ws->url, ws->host, sizeof(ws->host), &ws->port, ws->path, sizeof(ws->path), &ws->use_tls);
    if (rt != OPRT_OK) {
        ws->state = XZ_CONN_ERROR;
        return rt;
    }

    rt = xz_ws_conn_open(ws, ws->host, ws->port, ws->use_tls);
    if (rt != OPRT_OK) {
        ws->state = XZ_CONN_ERROR;
        return rt;
    }

    rt = xz_ws_handshake(ws);
    if (rt != OPRT_OK) {
        xz_ws_conn_cleanup(ws);
        ws->state = XZ_CONN_ERROR;
        return rt;
    }

    PR_NOTICE("websocket connected %s:%u%s", ws->host, ws->port, ws->path);
    return OPRT_OK;
}

OPERATE_RET xz_ws_open_audio_channel(xz_ws_client_t *ws, uint32_t timeout_ms)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    xz_channel_reset(&ws->channel);
#if defined(CONFIG_USE_SERVER_AEC) && (CONFIG_USE_SERVER_AEC == 1)
    char *hello = xz_build_hello("websocket", ws->version, TRUE, TRUE);
#else
    char *hello = xz_build_hello("websocket", ws->version, TRUE, FALSE);
#endif
    if (!hello) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = xz_ws_send_text(ws, hello);
    cJSON_free(hello);
    if (rt != OPRT_OK) {
        ws->state = XZ_CONN_ERROR;
        return rt;
    }

    uint32_t begin_ms = tal_system_get_millisecond();
    while ((uint32_t)(tal_system_get_millisecond() - begin_ms) < timeout_ms) {
        rt = xz_ws_poll(ws, 200);
        if (rt != OPRT_OK && rt != OPRT_RESOURCE_NOT_READY) {
            ws->state = XZ_CONN_ERROR;
            return rt;
        }
        if (ws->channel.ready) {
            ws->state = XZ_CONN_READY;
            return OPRT_OK;
        }
    }

    ws->state = XZ_CONN_ERROR;
    return OPRT_TIMEOUT;
}

OPERATE_RET xz_ws_poll(xz_ws_client_t *ws, int wait_ms)
{
    if (!ws || !ws->tcp) {
        return OPRT_INVALID_PARM;
    }

    uint8_t  opcode      = 0;
    uint8_t *payload     = NULL;
    size_t   payload_len = 0;
    size_t   consumed    = 0;

    OPERATE_RET rt = xz_ws_decode_one_frame(ws, &opcode, &payload, &payload_len, &consumed);
    if (rt == OPRT_OK) {
        xz_ws_consume_rx(ws, consumed);
    } else if (rt == OPRT_RESOURCE_NOT_READY) {
        uint8_t temp[1024] = {0};
        int     n          = xz_ws_conn_read(ws, temp, sizeof(temp), wait_ms);
        if (n == OPRT_RESOURCE_NOT_READY) {
            return OPRT_RESOURCE_NOT_READY;
        }
        if (n <= 0) {
            ws->state = XZ_CONN_ERROR;
            return OPRT_COM_ERROR;
        }
        if (ws->rx_len + (size_t)n > ws->rx_cap) {
            ws->state = XZ_CONN_ERROR;
            return OPRT_BUFFER_NOT_ENOUGH;
        }

        memcpy(ws->rx_buf + ws->rx_len, temp, (size_t)n);
        ws->rx_len += (size_t)n;

        rt = xz_ws_decode_one_frame(ws, &opcode, &payload, &payload_len, &consumed);
        if (rt != OPRT_OK) {
            return rt;
        }
        xz_ws_consume_rx(ws, consumed);
    } else {
        ws->state = XZ_CONN_ERROR;
        return rt;
    }

    ws->channel.last_rx_ms = tal_system_get_millisecond();

    if (opcode == 0x1) {
        (void)xz_ws_handle_text(ws, payload, payload_len);
    } else if (opcode == 0x2) {
        if (ws->on_binary_message) {
            ws->on_binary_message(ws->on_binary_userdata, payload, payload_len);
        } else {
            PR_DEBUG("websocket binary len=%u", (unsigned)payload_len);
        }
    } else if (opcode == 0x8) {
        (void)xz_ws_send_frame(ws, 0x8, payload, payload_len);
        if (payload) {
            tal_free(payload);
        }
        ws->state = XZ_CONN_ERROR;
        return OPRT_COM_ERROR;
    } else if (opcode == 0x9) {
        (void)xz_ws_send_frame(ws, 0xA, payload, payload_len);
    }

    if (payload) {
        tal_free(payload);
    }

    return OPRT_OK;
}

OPERATE_RET xz_ws_close(xz_ws_client_t *ws)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    xz_ws_conn_cleanup(ws);
    ws->state = XZ_CONN_IDLE;
    xz_channel_reset(&ws->channel);

    return OPRT_OK;
}

OPERATE_RET xz_ws_send_text(xz_ws_client_t *ws, const char *text)
{
    if (!ws || !text) {
        return OPRT_INVALID_PARM;
    }

    return xz_ws_send_frame(ws, 0x1, (const uint8_t *)text, strlen(text));
}

OPERATE_RET xz_ws_send_listen(xz_ws_client_t *ws, const char *state, const char *mode, const char *text)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    char *msg = xz_build_listen(ws->channel.session_id, state, mode, text);
    if (!msg) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = xz_ws_send_text(ws, msg);
    cJSON_free(msg);
    return rt;
}

OPERATE_RET xz_ws_send_abort(xz_ws_client_t *ws, const char *reason)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    char *msg = xz_build_abort(ws->channel.session_id, reason);
    if (!msg) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = xz_ws_send_text(ws, msg);
    cJSON_free(msg);
    return rt;
}

OPERATE_RET xz_ws_send_mcp(xz_ws_client_t *ws, const char *payload_json)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    char *msg = xz_build_mcp(ws->channel.session_id, payload_json);
    if (!msg) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = xz_ws_send_text(ws, msg);
    cJSON_free(msg);
    return rt;
}

OPERATE_RET xz_ws_set_text_message_callback(xz_ws_client_t *ws, xz_text_message_cb_t cb, void *userdata)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    ws->on_text_message  = cb;
    ws->on_text_userdata = userdata;
    return OPRT_OK;
}

OPERATE_RET xz_ws_send_audio(xz_ws_client_t *ws, const uint8_t *payload, size_t payload_len)
{
    if (!ws || !payload) {
        return OPRT_INVALID_PARM;
    }

    return xz_ws_send_frame(ws, 0x2, payload, payload_len);
}

OPERATE_RET xz_ws_set_binary_message_callback(xz_ws_client_t *ws, xz_binary_message_cb_t cb, void *userdata)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    ws->on_binary_message  = cb;
    ws->on_binary_userdata = userdata;
    return OPRT_OK;
}
