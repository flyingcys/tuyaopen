#include "http_proxy.h"

#include "mimi_config.h"
#include "tal_network.h"
#include "tls_cert_bundle.h"
#include "tuya_transporter.h"
#include "tuya_tls.h"
#include "mbedtls/ssl.h"

struct proxy_conn {
    tuya_transporter_t tcp;
    tuya_tls_hander tls;
    int socket_fd;
};

static const char *TAG = "proxy";
static char s_proxy_host[64] = {0};
static uint16_t s_proxy_port = 0;

static int find_header_end(const char *buf, int len)
{
    if (!buf || len < 4) {
        return -1;
    }
    for (int i = 0; i <= len - 4; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return i + 4;
        }
    }
    return -1;
}

static int parse_http_status(const char *header)
{
    if (!header) {
        return -1;
    }
    const char *sp = strchr(header, ' ');
    if (!sp) {
        return -1;
    }
    return atoi(sp + 1);
}

static int proxy_write_all_tcp(tuya_transporter_t tcp, const char *data, int len, int timeout_ms)
{
    int sent = 0;
    while (sent < len) {
        int n = tuya_transporter_write(tcp, (uint8_t *)data + sent, len - sent, timeout_ms);
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }
    return sent;
}

static int proxy_read_headers(tuya_transporter_t tcp, char *buf, int size, int timeout_ms)
{
    if (!buf || size <= 4) {
        return -1;
    }

    int total = 0;
    uint32_t start_ms = tal_system_get_millisecond();
    while (total < size - 1) {
        uint32_t now_ms = tal_system_get_millisecond();
        if ((int)(now_ms - start_ms) >= timeout_ms) {
            break;
        }
        int remain_ms = timeout_ms - (int)(now_ms - start_ms);
        if (remain_ms < 50) {
            remain_ms = 50;
        }

        int n = tuya_transporter_read(tcp, (uint8_t *)buf + total, size - total - 1, remain_ms);
        if (n == OPRT_RESOURCE_NOT_READY) {
            tal_system_sleep(10);
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        total += n;
        buf[total] = '\0';
        if (find_header_end(buf, total) > 0) {
            return total;
        }
    }

    return -1;
}

OPERATE_RET http_proxy_init(void)
{
    if (MIMI_SECRET_PROXY_HOST[0] != '\0') {
        snprintf(s_proxy_host, sizeof(s_proxy_host), "%s", MIMI_SECRET_PROXY_HOST);
    }

    if (MIMI_SECRET_PROXY_PORT[0] != '\0') {
        s_proxy_port = (uint16_t)atoi(MIMI_SECRET_PROXY_PORT);
    }

    char tmp[64] = {0};
    if (mimi_kv_get_string(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_HOST, tmp, sizeof(tmp)) == OPRT_OK) {
        snprintf(s_proxy_host, sizeof(s_proxy_host), "%s", tmp);
    }
    if (mimi_kv_get_string(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_PORT, tmp, sizeof(tmp)) == OPRT_OK) {
        s_proxy_port = (uint16_t)atoi(tmp);
    }

    return OPRT_OK;
}

bool http_proxy_is_enabled(void)
{
    return s_proxy_host[0] != '\0' && s_proxy_port > 0;
}

OPERATE_RET http_proxy_set(const char *host, uint16_t port)
{
    if (!host || port == 0) {
        return OPRT_INVALID_PARM;
    }

    snprintf(s_proxy_host, sizeof(s_proxy_host), "%s", host);
    s_proxy_port = port;

    char port_buf[16] = {0};
    snprintf(port_buf, sizeof(port_buf), "%u", (unsigned)port);

    OPERATE_RET rt = mimi_kv_set_string(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_HOST, host);
    if (rt != OPRT_OK) {
        return rt;
    }

    return mimi_kv_set_string(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_PORT, port_buf);
}

OPERATE_RET http_proxy_clear(void)
{
    s_proxy_host[0] = '\0';
    s_proxy_port = 0;

    (void)mimi_kv_del(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_HOST);
    (void)mimi_kv_del(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_PORT);
    return OPRT_OK;
}

proxy_conn_t *proxy_conn_open(const char *host, int port, int timeout_ms)
{
    if (!host || port <= 0 || timeout_ms <= 0) {
        return NULL;
    }
    if (!http_proxy_is_enabled()) {
        MIMI_LOGW(TAG, "proxy not configured");
        return NULL;
    }

    proxy_conn_t *conn = calloc(1, sizeof(proxy_conn_t));
    if (!conn) {
        return NULL;
    }

    conn->tcp = tuya_transporter_create(TRANSPORT_TYPE_TCP, NULL);
    if (!conn->tcp) {
        MIMI_LOGE(TAG, "create tcp transporter failed");
        free(conn);
        return NULL;
    }

    tuya_tcp_config_t cfg = {0};
    cfg.isReuse = TRUE;
    cfg.isDisableNagle = TRUE;
    cfg.sendTimeoutMs = timeout_ms;
    cfg.recvTimeoutMs = timeout_ms;
    (void)tuya_transporter_ctrl(conn->tcp, TUYA_TRANSPORTER_SET_TCP_CONFIG, &cfg);

    OPERATE_RET rt = tuya_transporter_connect(conn->tcp, s_proxy_host, s_proxy_port, timeout_ms);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "connect proxy failed %s:%u rt=%d", s_proxy_host, s_proxy_port, rt);
        tuya_transporter_destroy(conn->tcp);
        free(conn);
        return NULL;
    }

    char req[512] = {0};
    int req_len = snprintf(req, sizeof(req),
                           "CONNECT %s:%d HTTP/1.1\r\n"
                           "Host: %s:%d\r\n"
                           "Proxy-Connection: Keep-Alive\r\n"
                           "Connection: Keep-Alive\r\n\r\n",
                           host, port, host, port);
    if (req_len <= 0 || req_len >= (int)sizeof(req)) {
        MIMI_LOGE(TAG, "build connect request failed");
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        free(conn);
        return NULL;
    }

    if (proxy_write_all_tcp(conn->tcp, req, req_len, timeout_ms) != req_len) {
        MIMI_LOGE(TAG, "send CONNECT failed");
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        free(conn);
        return NULL;
    }

    char header[1024] = {0};
    int hdr_len = proxy_read_headers(conn->tcp, header, sizeof(header), timeout_ms);
    if (hdr_len <= 0) {
        MIMI_LOGE(TAG, "read CONNECT response failed");
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        free(conn);
        return NULL;
    }

    int code = parse_http_status(header);
    if (code != 200) {
        MIMI_LOGE(TAG, "CONNECT rejected code=%d", code);
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        free(conn);
        return NULL;
    }

    rt = tuya_transporter_ctrl(conn->tcp, TUYA_TRANSPORTER_GET_TCP_SOCKET, &conn->socket_fd);
    if (rt != OPRT_OK || conn->socket_fd < 0) {
        MIMI_LOGE(TAG, "get proxy socket failed rt=%d fd=%d", rt, conn->socket_fd);
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        free(conn);
        return NULL;
    }

    uint8_t *cacert = NULL;
    uint16_t cacert_len = 0;
    bool verify_peer = false;
    rt = mimi_tls_query_domain_certs(host, &cacert, &cacert_len);
    if (rt == OPRT_OK && cacert && cacert_len > 0) {
        verify_peer = true;
    } else {
#if OPERATING_SYSTEM == SYSTEM_LINUX
        MIMI_LOGW(TAG, "proxy tls cert unavailable for %s, fallback to no-verify mode", host);
#else
        MIMI_LOGE(TAG, "proxy tls query cert failed host=%s rt=%d", host, rt);
        if (cacert) {
            tal_free(cacert);
        }
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        free(conn);
        return NULL;
#endif
    }

    conn->tls = tuya_tls_connect_create();
    if (!conn->tls) {
        MIMI_LOGE(TAG, "create tls handler failed");
        if (cacert) {
            tal_free(cacert);
        }
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        free(conn);
        return NULL;
    }

    int timeout_s = timeout_ms / 1000;
    if (timeout_s <= 0) {
        timeout_s = 1;
    }

    tuya_tls_config_t cfg_tls = {
        .mode = TUYA_TLS_SERVER_CERT_MODE,
        .hostname = (char *)host,
        .port = (uint32_t)port,
        .timeout = timeout_s,
        .verify = verify_peer,
        .ca_cert = verify_peer ? (char *)cacert : NULL,
        .ca_cert_size = verify_peer ? cacert_len : 0,
    };

    (void)tuya_tls_config_set(conn->tls, &cfg_tls);
    rt = tuya_tls_connect(conn->tls, (char *)host, port, conn->socket_fd, timeout_s);
    if (cacert) {
        tal_free(cacert);
    }
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "proxy tls connect failed host=%s rt=%d", host, rt);
        tuya_tls_connect_destroy(conn->tls);
        conn->tls = NULL;
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        free(conn);
        return NULL;
    }

    MIMI_LOGI(TAG, "CONNECT+TLS tunnel ready %s:%d via %s:%u", host, port, s_proxy_host, s_proxy_port);
    return conn;
}

int proxy_conn_write(proxy_conn_t *conn, const char *data, int len)
{
    if (!conn || !conn->tls || !data || len <= 0) {
        return -1;
    }

    int sent = 0;
    while (sent < len) {
        int n = tuya_tls_write(conn->tls, (uint8_t *)data + sent, (uint32_t)(len - sent));
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }

    return sent;
}

int proxy_conn_read(proxy_conn_t *conn, char *buf, int len, int timeout_ms)
{
    if (!conn || !conn->tls || conn->socket_fd < 0 || !buf || len <= 0 || timeout_ms <= 0) {
        return -1;
    }

    TUYA_FD_SET_T readfds;
    tal_net_fd_zero(&readfds);
    tal_net_fd_set(conn->socket_fd, &readfds);
    int ready = tal_net_select(conn->socket_fd + 1, &readfds, NULL, NULL, timeout_ms);
    if (ready < 0) {
        return -1;
    }
    if (ready == 0) {
        return OPRT_RESOURCE_NOT_READY;
    }

    int n = tuya_tls_read(conn->tls, (uint8_t *)buf, (uint32_t)len);
    if (n > 0) {
        return n;
    }
    if (n == 0 || n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        return 0;
    }
    if (n == OPRT_RESOURCE_NOT_READY || n == MBEDTLS_ERR_SSL_WANT_READ ||
        n == MBEDTLS_ERR_SSL_WANT_WRITE || n == MBEDTLS_ERR_SSL_TIMEOUT || n == -100) {
        return OPRT_RESOURCE_NOT_READY;
    }
    return n;
}

void proxy_conn_close(proxy_conn_t *conn)
{
    if (!conn) {
        return;
    }
    if (conn->tls) {
        (void)tuya_tls_disconnect(conn->tls);
        tuya_tls_connect_destroy(conn->tls);
        conn->tls = NULL;
    }
    if (conn->tcp) {
        (void)tuya_transporter_close(conn->tcp);
        (void)tuya_transporter_destroy(conn->tcp);
        conn->tcp = NULL;
    }
    conn->socket_fd = -1;
    free(conn);
}
