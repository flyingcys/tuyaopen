#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xiaozhi_ws.h"

static uint8_t  g_last_frame[512];
static size_t   g_last_frame_len;
static uint32_t g_next_random_value = 0x01020304;

static void reset_last_frame(void)
{
    g_last_frame_len = 0;
    memset(g_last_frame, 0, sizeof(g_last_frame));
}

static void set_next_random(uint32_t value)
{
    g_next_random_value = value;
}

void *tal_malloc(size_t size)
{
    if (size == 0) {
        size = 1;
    }
    return malloc(size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

SYS_TIME_T tal_system_get_millisecond(void)
{
    static SYS_TIME_T ts = 1;
    return ts++;
}

int tal_system_get_random(uint32_t range)
{
    (void)range;
    return (int)(g_next_random_value & 0x7FFFFFFF);
}

OPERATE_RET tal_net_fd_zero(TUYA_FD_SET_T *fds)
{
    (void)fds;
    return OPRT_OK;
}

OPERATE_RET tal_net_fd_set(int fd, TUYA_FD_SET_T *fds)
{
    (void)fd;
    (void)fds;
    return OPRT_OK;
}

int tal_net_select(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                   const uint32_t ms_timeout)
{
    (void)maxfd;
    (void)readfds;
    (void)writefds;
    (void)errorfds;
    (void)ms_timeout;
    return 0;
}

tuya_transporter_t tuya_transporter_create(TUYA_TRANSPORT_TYPE_E transport_type, tuya_transporter_t dependency)
{
    (void)transport_type;
    (void)dependency;
    return (tuya_transporter_t)0x1;
}

OPERATE_RET tuya_transporter_destroy(tuya_transporter_t transporter)
{
    (void)transporter;
    return OPRT_OK;
}

OPERATE_RET tuya_transporter_connect(tuya_transporter_t transporter, const char *host, int port, int timeout_ms)
{
    (void)transporter;
    (void)host;
    (void)port;
    (void)timeout_ms;
    return OPRT_OK;
}

OPERATE_RET tuya_transporter_read(tuya_transporter_t transporter, uint8_t *buf, int len, int timeout_ms)
{
    (void)transporter;
    (void)buf;
    (void)len;
    (void)timeout_ms;
    return 0;
}

OPERATE_RET tuya_transporter_write(tuya_transporter_t transporter, uint8_t *buf, int len, int timeout_ms)
{
    (void)transporter;
    (void)timeout_ms;
    size_t copy_len = (size_t)len;
    if (copy_len > sizeof(g_last_frame)) {
        copy_len = sizeof(g_last_frame);
    }
    memcpy(g_last_frame, buf, copy_len);
    g_last_frame_len = (size_t)len;
    return len;
}

OPERATE_RET tuya_transporter_close(tuya_transporter_t transporter)
{
    (void)transporter;
    return OPRT_OK;
}

OPERATE_RET tuya_transporter_ctrl(tuya_transporter_t transporter, uint32_t cmd, void *args)
{
    (void)transporter;
    (void)cmd;
    (void)args;
    return OPRT_OK;
}

tuya_tls_hander *tuya_tls_connect_create(void)
{
    return (tuya_tls_hander *)0x1;
}

void tuya_tls_connect_destroy(tuya_tls_hander p_tls_hander)
{
    (void)p_tls_hander;
}

OPERATE_RET tuya_tls_config_set(tuya_tls_hander p_tls_handler, tuya_tls_config_t *config)
{
    (void)p_tls_handler;
    (void)config;
    return OPRT_OK;
}

OPERATE_RET tuya_tls_connect(tuya_tls_hander p_tls_handler, char *hostname, int port_num, int socket_fd, int overtime_s)
{
    (void)p_tls_handler;
    (void)hostname;
    (void)port_num;
    (void)socket_fd;
    (void)overtime_s;
    return OPRT_OK;
}

int tuya_tls_write(tuya_tls_hander tls_handler, uint8_t *buf, uint32_t len)
{
    (void)tls_handler;
    (void)buf;
    (void)len;
    return (int)len;
}

int tuya_tls_read(tuya_tls_hander tls_handler, uint8_t *buf, uint32_t len)
{
    (void)tls_handler;
    (void)buf;
    (void)len;
    return 0;
}

OPERATE_RET tuya_tls_disconnect(tuya_tls_hander tls_handler)
{
    (void)tls_handler;
    return OPRT_OK;
}

OPERATE_RET tuya_iotdns_query_domain_certs(char *domain, uint8_t **cacert, uint16_t *cacert_len)
{
    (void)domain;
    if (cacert) {
        *cacert = NULL;
    }
    if (cacert_len) {
        *cacert_len = 0;
    }
    return OPRT_COM_ERROR;
}

int xz_ws_format_handshake_request(char *out, size_t out_size, const char *path, const char *host, unsigned int port,
                                   const char *ws_key, int version, const char *device_id, const char *client_id,
                                   const char *token)
{
    (void)out;
    (void)out_size;
    (void)path;
    (void)host;
    (void)port;
    (void)ws_key;
    (void)version;
    (void)device_id;
    (void)client_id;
    (void)token;
    return 0;
}

void xz_channel_reset(xz_channel_t *ch)
{
    if (ch) {
        memset(ch, 0, sizeof(*ch));
    }
}

char *xz_build_hello(const char *transport, int version, BOOL_T enable_mcp, BOOL_T enable_aec)
{
    (void)transport;
    (void)version;
    (void)enable_mcp;
    (void)enable_aec;
    return NULL;
}

char *xz_build_listen(const char *session_id, const char *state, const char *mode, const char *text)
{
    (void)session_id;
    (void)state;
    (void)mode;
    (void)text;
    return NULL;
}

char *xz_build_abort(const char *session_id, const char *reason)
{
    (void)session_id;
    (void)reason;
    return NULL;
}

char *xz_build_mcp(const char *session_id, const char *payload_json)
{
    (void)session_id;
    (void)payload_json;
    return NULL;
}

char *xz_build_goodbye(const char *session_id)
{
    (void)session_id;
    return NULL;
}

OPERATE_RET xz_parse_server_hello(const cJSON *root, const char *expect_transport, xz_channel_t *out)
{
    (void)root;
    (void)expect_transport;
    (void)out;
    return OPRT_OK;
}

int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen)
{
    (void)dst;
    (void)dlen;
    (void)src;
    (void)slen;
    if (olen) {
        *olen = 0;
    }
    return 0;
}

int mbedtls_sha1(const unsigned char *input, size_t ilen, unsigned char output[20])
{
    (void)input;
    (void)ilen;
    if (output) {
        memset(output, 0, 20);
    }
    return 0;
}

void tal_log_print_secure(const char *fmt, ...)
{
    (void)fmt;
}

static int     g_binary_calls;
static size_t  g_binary_payload_len;
static uint8_t g_binary_payload[64];

static void binary_cb(void *userdata, const uint8_t *payload, size_t payload_len)
{
    assert(userdata == (void *)0xBADC0DE);
    assert(payload);
    assert(payload_len <= sizeof(g_binary_payload));

    memcpy(g_binary_payload, payload, payload_len);
    g_binary_payload_len = payload_len;
    g_binary_calls += 1;
}

static size_t build_unmasked_binary_frame(uint8_t *out, size_t cap, const uint8_t *payload, size_t payload_len)
{
    if (!out || !payload) {
        return 0;
    }
    if (payload_len > 125 || cap < payload_len + 2) {
        return 0;
    }

    out[0] = 0x80 | 0x02;
    out[1] = (uint8_t)payload_len;
    memcpy(out + 2, payload, payload_len);
    return 2 + payload_len;
}

static void reset_binary_state(void)
{
    g_binary_calls       = 0;
    g_binary_payload_len = 0;
    memset(g_binary_payload, 0, sizeof(g_binary_payload));
}

static void assert_masked_frame(const uint8_t *payload, size_t payload_len)
{
    assert(g_last_frame_len == payload_len + 6);
    uint8_t opcode = g_last_frame[0] & 0x0F;
    assert(opcode == 0x02);
    uint8_t len_byte = g_last_frame[1];
    assert((len_byte & 0x80) == 0x80);
    assert((len_byte & 0x7F) == payload_len);

    uint8_t mask[4];
    memcpy(mask, g_last_frame + 2, sizeof(mask));
    for (size_t i = 0; i < payload_len; ++i) {
        uint8_t expected = payload[i] ^ mask[i % 4];
        assert(g_last_frame[6 + i] == expected);
    }
}

static void test_binary_callback_flow(void)
{
    reset_binary_state();
    xz_ws_client_t ws;
    assert(xz_ws_init(&ws) == OPRT_OK);
    ws.tcp = (tuya_transporter_t *)0x1;

    const uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t       frame[64] = {0};
    size_t        f_len     = build_unmasked_binary_frame(frame, sizeof(frame), payload, sizeof(payload));
    assert(f_len > 0);
    assert(f_len <= ws.rx_cap);
    memcpy(ws.rx_buf, frame, f_len);
    ws.rx_len = f_len;

    assert(xz_ws_set_binary_message_callback(&ws, binary_cb, (void *)0xBADC0DE) == OPRT_OK);
    assert(xz_ws_poll(&ws, 0) == OPRT_OK);
    assert(g_binary_calls == 1);
    assert(g_binary_payload_len == sizeof(payload));
    assert(memcmp(g_binary_payload, payload, sizeof(payload)) == 0);

    assert(xz_ws_send_audio(NULL, payload, sizeof(payload)) == OPRT_INVALID_PARM);
    xz_ws_deinit(&ws);
}

static void test_xz_ws_send_audio_masks_payload(void)
{
    xz_ws_client_t ws;
    reset_last_frame();
    set_next_random(0x01020304);
    assert(xz_ws_init(&ws) == OPRT_OK);
    ws.tcp = (tuya_transporter_t *)0x1;

    const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    assert(xz_ws_send_audio(&ws, payload, sizeof(payload)) == OPRT_OK);
    assert_masked_frame(payload, sizeof(payload));

    xz_ws_deinit(&ws);
}

static void test_poll_without_binary_callback_consumes_frames(void)
{
    reset_binary_state();
    xz_ws_client_t ws;
    assert(xz_ws_init(&ws) == OPRT_OK);
    ws.tcp = (tuya_transporter_t *)0x1;

    const uint8_t payload[] = {0x9A, 0xBC};
    uint8_t       frame[64] = {0};
    size_t        f_len     = build_unmasked_binary_frame(frame, sizeof(frame), payload, sizeof(payload));
    assert(f_len > 0);
    assert(f_len <= ws.rx_cap);
    memcpy(ws.rx_buf, frame, f_len);
    ws.rx_len = f_len;

    assert(xz_ws_poll(&ws, 0) == OPRT_OK);
    assert(ws.rx_len == 0);
    assert(g_binary_calls == 0);

    xz_ws_deinit(&ws);
}

int main(void)
{
    test_binary_callback_flow();
    test_xz_ws_send_audio_masks_payload();
    test_poll_without_binary_callback_consumes_frames();
    puts("test_xiaozhi_ws_binary: PASS");
    return 0;
}
