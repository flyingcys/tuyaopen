#include <stdlib.h>
#include <string.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"
#include "mbedtls/threading.h"
#include "mbedtls/x509_crt.h"
#include "tal_network.h"
#include "tuya_error_code.h"
#include "tuya_tls.h"
#include "unity.h"

static OPERATE_RET g_mutex_create_ret = OPRT_OK;

void *tal_malloc(size_t size)
{
    return calloc(1, size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

void *tal_calloc(size_t nitems, size_t size)
{
    return calloc(nitems, size);
}

OPERATE_RET tal_mutex_create_init(MUTEX_HANDLE *handle)
{
    if (handle != NULL) {
        *handle = (MUTEX_HANDLE)0x1;
    }
    return g_mutex_create_ret;
}

OPERATE_RET tal_mutex_lock(const MUTEX_HANDLE handle)
{
    (void)handle;
    return OPRT_OK;
}

OPERATE_RET tal_mutex_unlock(const MUTEX_HANDLE handle)
{
    (void)handle;
    return OPRT_OK;
}

OPERATE_RET tal_mutex_release(const MUTEX_HANDLE handle)
{
    (void)handle;
    return OPRT_OK;
}

int tal_kv_get(const char *key, uint8_t **value, size_t *length)
{
    (void)key;
    (void)value;
    (void)length;
    return OPRT_COM_ERROR;
}

int tal_kv_set(const char *key, const uint8_t *value, size_t length)
{
    (void)key;
    (void)value;
    (void)length;
    return OPRT_OK;
}

int tal_kv_free(uint8_t *value)
{
    (void)value;
    return OPRT_OK;
}

TIME_T tal_time_get_posix(void)
{
    return 1700000000;
}

void tal_system_sleep(uint32_t time_ms)
{
    (void)time_ms;
}

TUYA_ERRNO tal_net_send(const int fd, const void *buf, const uint32_t nbytes)
{
    (void)fd;
    (void)buf;
    return (TUYA_ERRNO)nbytes;
}

TUYA_ERRNO tal_net_recv(const int fd, void *buf, const uint32_t nbytes)
{
    (void)fd;
    (void)buf;
    return (TUYA_ERRNO)nbytes;
}

TUYA_ERRNO tal_net_get_errno(void)
{
    return UNW_SUCCESS;
}

int tal_net_get_nonblock(const int fd)
{
    (void)fd;
    return 0;
}

OPERATE_RET tal_net_set_block(const int fd, const BOOL_T block)
{
    (void)fd;
    (void)block;
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
    return 1;
}

OPERATE_RET tal_net_set_timeout(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type)
{
    (void)fd;
    (void)ms_timeout;
    (void)type;
    return OPRT_OK;
}

void mbedtls_threading_set_alt(void (*mutex_init)(mbedtls_threading_mutex_t *),
                               void (*mutex_free)(mbedtls_threading_mutex_t *),
                               int (*mutex_lock)(mbedtls_threading_mutex_t *),
                               int (*mutex_unlock)(mbedtls_threading_mutex_t *))
{
    (void)mutex_init;
    (void)mutex_free;
    (void)mutex_lock;
    (void)mutex_unlock;
}

int mbedtls_platform_set_calloc_free(void *(*calloc_func)(size_t, size_t), void (*free_func)(void *))
{
    (void)calloc_func;
    (void)free_func;
    return 0;
}

void mbedtls_ctr_drbg_init(mbedtls_ctr_drbg_context *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_entropy_init(mbedtls_entropy_context *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

int mbedtls_entropy_func(void *data, unsigned char *output, size_t len)
{
    (void)data;
    memset(output, 0, len);
    return 0;
}

int mbedtls_ctr_drbg_seed(mbedtls_ctr_drbg_context *ctx, int (*f_entropy)(void *, unsigned char *, size_t),
                          void *p_entropy, const unsigned char *custom, size_t len)
{
    (void)ctx;
    (void)f_entropy;
    (void)p_entropy;
    (void)custom;
    (void)len;
    return 0;
}

void mbedtls_ctr_drbg_set_prediction_resistance(mbedtls_ctr_drbg_context *ctx, int resistance)
{
    (void)ctx;
    (void)resistance;
}

void mbedtls_ctr_drbg_free(mbedtls_ctr_drbg_context *ctx)
{
    (void)ctx;
}

void mbedtls_entropy_free(mbedtls_entropy_context *ctx)
{
    (void)ctx;
}

int mbedtls_ctr_drbg_random(void *p_rng, unsigned char *output, size_t output_len)
{
    (void)p_rng;
    memset(output, 0x11, output_len);
    return 0;
}

int mbedtls_x509_crt_parse(mbedtls_x509_crt *chain, const unsigned char *buf, size_t buflen)
{
    (void)chain;
    (void)buf;
    (void)buflen;
    return 0;
}

void mbedtls_x509_crt_free(mbedtls_x509_crt *crt)
{
    (void)crt;
}

void mbedtls_x509_crt_init(mbedtls_x509_crt *crt)
{
    memset(crt, 0, sizeof(*crt));
}

void mbedtls_pk_init(mbedtls_pk_context *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_pk_free(mbedtls_pk_context *ctx)
{
    (void)ctx;
}

int mbedtls_pk_parse_key(mbedtls_pk_context *ctx, const unsigned char *key, size_t keylen, const unsigned char *pwd,
                         size_t pwdlen, int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    (void)ctx;
    (void)key;
    (void)keylen;
    (void)pwd;
    (void)pwdlen;
    (void)f_rng;
    (void)p_rng;
    return 0;
}

void mbedtls_ssl_conf_authmode(mbedtls_ssl_config *conf, int authmode)
{
    (void)conf;
    (void)authmode;
}

void mbedtls_ssl_conf_ca_chain(mbedtls_ssl_config *conf, mbedtls_x509_crt *ca_chain, mbedtls_x509_crl *ca_crl)
{
    (void)conf;
    (void)ca_chain;
    (void)ca_crl;
}

int mbedtls_ssl_conf_own_cert(mbedtls_ssl_config *conf, mbedtls_x509_crt *own_cert, mbedtls_pk_context *pk_key)
{
    (void)conf;
    (void)own_cert;
    (void)pk_key;
    return 0;
}

void mbedtls_ssl_init(mbedtls_ssl_context *ssl)
{
    memset(ssl, 0, sizeof(*ssl));
}

void mbedtls_ssl_config_init(mbedtls_ssl_config *conf)
{
    memset(conf, 0, sizeof(*conf));
}

void mbedtls_ssl_conf_dbg(mbedtls_ssl_config *conf, void (*f_dbg)(void *, int, const char *, int, const char *),
                          void               *p_dbg)
{
    (void)conf;
    (void)f_dbg;
    (void)p_dbg;
}

void mbedtls_ssl_conf_rng(mbedtls_ssl_config *conf, int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    (void)conf;
    (void)f_rng;
    (void)p_rng;
}

void mbedtls_debug_set_threshold(int threshold)
{
    (void)threshold;
}

void mbedtls_ssl_set_export_keys_cb(mbedtls_ssl_context *ssl, mbedtls_ssl_export_keys_t *f_export_keys, void *p_expkey)
{
    (void)ssl;
    (void)f_export_keys;
    (void)p_expkey;
}

int mbedtls_ssl_config_defaults(mbedtls_ssl_config *conf, int endpoint, int transport, int preset)
{
    (void)conf;
    (void)endpoint;
    (void)transport;
    (void)preset;
    return 0;
}

int mbedtls_ssl_conf_max_frag_len(mbedtls_ssl_config *conf, unsigned char mfl_code)
{
    (void)conf;
    (void)mfl_code;
    return 0;
}

int mbedtls_ssl_conf_psk(mbedtls_ssl_config *conf, const unsigned char *psk, size_t psk_len,
                         const unsigned char *psk_identity, size_t psk_identity_len)
{
    (void)conf;
    (void)psk;
    (void)psk_len;
    (void)psk_identity;
    (void)psk_identity_len;
    return 0;
}

void mbedtls_ssl_conf_ciphersuites(mbedtls_ssl_config *conf, const int *ciphersuites)
{
    (void)conf;
    (void)ciphersuites;
}

int mbedtls_ssl_set_hostname(mbedtls_ssl_context *ssl, const char *hostname)
{
    (void)ssl;
    (void)hostname;
    return 0;
}

int mbedtls_ssl_setup(mbedtls_ssl_context *ssl, const mbedtls_ssl_config *conf)
{
    (void)ssl;
    (void)conf;
    return 0;
}

void mbedtls_ssl_set_bio(mbedtls_ssl_context *ssl, void *p_bio, int (*f_send)(void *, const unsigned char *, size_t),
                         int (*f_recv)(void *, unsigned char *, size_t),
                         int (*f_recv_timeout)(void *, unsigned char *, size_t, uint32_t))
{
    (void)ssl;
    (void)p_bio;
    (void)f_send;
    (void)f_recv;
    (void)f_recv_timeout;
}

int mbedtls_ssl_handshake(mbedtls_ssl_context *ssl)
{
    (void)ssl;
    return 0;
}

uint32_t mbedtls_ssl_get_verify_result(const mbedtls_ssl_context *ssl)
{
    (void)ssl;
    return 0;
}

const char *mbedtls_ssl_get_ciphersuite(const mbedtls_ssl_context *ssl)
{
    (void)ssl;
    return "stub";
}

int mbedtls_ssl_write(mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len)
{
    (void)ssl;
    (void)buf;
    return (int)len;
}

int mbedtls_ssl_read(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
    (void)ssl;
    memset(buf, 0, len);
    return (int)len;
}

void mbedtls_ssl_free(mbedtls_ssl_context *ssl)
{
    (void)ssl;
}

void mbedtls_ssl_config_free(mbedtls_ssl_config *conf)
{
    (void)conf;
}

void setUp(void)
{
    g_mutex_create_ret = OPRT_OK;
}

void tearDown(void) {}

void test_tuya_tls_connect_create_returns_null_when_mutex_create_fails(void)
{
    g_mutex_create_ret = OPRT_COM_ERROR;
    TEST_ASSERT_NULL(tuya_tls_connect_create());
}

void test_tuya_tls_config_roundtrip_on_created_context(void)
{
    tuya_tls_config_t config = {
        .hostname = "example.com",
        .port     = 443,
        .timeout  = 5,
    };
    tuya_tls_hander *handler = tuya_tls_connect_create();

    TEST_ASSERT_NOT_NULL(handler);
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_tls_config_set(handler, &config));
    TEST_ASSERT_EQUAL_PTR(config.hostname, tuya_tls_config_get(handler)->hostname);
    TEST_ASSERT_EQUAL_UINT16(config.port, tuya_tls_config_get(handler)->port);

    tuya_tls_connect_destroy(handler);
}

void test_tuya_tls_connect_rejects_invalid_parameters(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_tls_connect(NULL, "example.com", 443, -1, 5));
}

void test_tuya_tls_write_read_disconnect_reject_invalid_parameters(void)
{
    uint8_t buf[4] = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_tls_write(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_tls_read(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_tls_disconnect(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_tls_connect_create_returns_null_when_mutex_create_fails);
    RUN_TEST(test_tuya_tls_config_roundtrip_on_created_context);
    RUN_TEST(test_tuya_tls_connect_rejects_invalid_parameters);
    RUN_TEST(test_tuya_tls_write_read_disconnect_reject_invalid_parameters);
    return UNITY_END();
}
