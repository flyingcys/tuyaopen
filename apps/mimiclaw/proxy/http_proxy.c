#include "http_proxy.h"

#include "mimi_config.h"
#include "tuya_transporter.h"

struct proxy_conn {
    tuya_transporter_t tcp;
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

static int proxy_write_all(tuya_transporter_t tcp, const char *data, int len, int timeout_ms)
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

    if (proxy_write_all(conn->tcp, req, req_len, timeout_ms) != req_len) {
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

    MIMI_LOGI(TAG, "CONNECT tunnel ready %s:%d via %s:%u", host, port, s_proxy_host, s_proxy_port);
    return conn;
}

int proxy_conn_write(proxy_conn_t *conn, const char *data, int len)
{
    if (!conn || !conn->tcp || !data || len <= 0) {
        return -1;
    }
    return proxy_write_all(conn->tcp, data, len, 5000);
}

int proxy_conn_read(proxy_conn_t *conn, char *buf, int len, int timeout_ms)
{
    if (!conn || !conn->tcp || !buf || len <= 0 || timeout_ms <= 0) {
        return -1;
    }

    int n = tuya_transporter_read(conn->tcp, (uint8_t *)buf, len, timeout_ms);
    if (n == OPRT_RESOURCE_NOT_READY) {
        return 0;
    }
    if (n <= 0) {
        return -1;
    }
    return n;
}

void proxy_conn_close(proxy_conn_t *conn)
{
    if (!conn) {
        return;
    }
    if (conn->tcp) {
        (void)tuya_transporter_close(conn->tcp);
        (void)tuya_transporter_destroy(conn->tcp);
        conn->tcp = NULL;
    }
    free(conn);
}
