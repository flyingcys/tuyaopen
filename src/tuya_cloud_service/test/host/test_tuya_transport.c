#include <stdlib.h>
#include <string.h>

#include "tuya_error_code.h"
#include "tuya_transporter.h"
#include "unity.h"

void *tal_malloc(size_t size)
{
    return calloc(1, size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

char *mm_strdup(const char *str)
{
    if (str == NULL) {
        return NULL;
    }

    size_t len  = strlen(str) + 1;
    char  *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, str, len);
    }

    return copy;
}

static struct tuya_transporter_inter_t g_tcp_transporter;
static struct tuya_transporter_inter_t g_tls_transporter;
static int                             g_destroy_calls;

tuya_transporter_t tuya_tcp_transporter_create(void)
{
    memset(&g_tcp_transporter, 0, sizeof(g_tcp_transporter));
    return &g_tcp_transporter;
}

tuya_transporter_t tuya_tls_transporter_create(void)
{
    memset(&g_tls_transporter, 0, sizeof(g_tls_transporter));
    return &g_tls_transporter;
}

#if defined(ENABLE_WEBSOCKET) && (ENABLE_WEBSOCKET == 1)
static struct tuya_transporter_inter_t g_websocket_transporter;

tuya_transporter_t tuya_websocket_transporter_create(void)
{
    memset(&g_websocket_transporter, 0, sizeof(g_websocket_transporter));
    return &g_websocket_transporter;
}
#endif

void setUp(void)
{
    g_destroy_calls = 0;
}

void tearDown(void) {}

static OPERATE_RET fake_destroy(tuya_transporter_t transporter)
{
    (void)transporter;
    g_destroy_calls++;
    return OPRT_OK;
}

static OPERATE_RET fake_connect(tuya_transporter_t transporter, const char *host, int port, int timeout_ms)
{
    (void)transporter;
    (void)host;
    (void)port;
    (void)timeout_ms;
    return OPRT_COM_ERROR;
}

static OPERATE_RET fake_close(tuya_transporter_t transporter)
{
    (void)transporter;
    return OPRT_TIMEOUT;
}

static OPERATE_RET fake_read(tuya_transporter_t transporter, uint8_t *buf, int len, int timeout_ms)
{
    (void)transporter;
    (void)buf;
    (void)len;
    (void)timeout_ms;
    return OPRT_OK;
}

static OPERATE_RET fake_write(tuya_transporter_t transporter, uint8_t *buf, int len, int timeout_ms)
{
    (void)transporter;
    (void)buf;
    (void)len;
    (void)timeout_ms;
    return OPRT_NOT_SUPPORTED;
}

static OPERATE_RET fake_poll_read(tuya_transporter_t transporter, int timeout_ms)
{
    (void)transporter;
    (void)timeout_ms;
    return OPRT_OS_ADAPTER_COM_ERROR;
}

static OPERATE_RET fake_poll_write(tuya_transporter_t transporter, int timeout_ms)
{
    (void)transporter;
    (void)timeout_ms;
    return OPRT_INVALID_PARM;
}

static OPERATE_RET fake_ctrl(tuya_transporter_t transporter, uint32_t cmd, void *args)
{
    (void)transporter;
    (void)cmd;
    (void)args;
    return OPRT_OS_ADAPTER_INVALID_PARM;
}

void test_tuya_transport_array_add_transporter_rejects_null_scheme(void)
{
    tuya_transport_array_handle_t   handle      = tuya_transport_array_create();
    struct tuya_transporter_inter_t transporter = {0};

    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transport_array_add_transporter(handle, &transporter, NULL));

    tuya_transport_array_destroy(handle);
}

void test_tuya_transport_array_get_transporter_returns_null_for_empty_array(void)
{
    tuya_transport_array_handle_t handle = tuya_transport_array_create();

    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NULL(tuya_transport_array_get_transporter(handle, "tls"));

    tuya_transport_array_destroy(handle);
}

void test_tuya_transporter_set_func_rejects_null_transporter(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM,
                      tuya_transporter_set_func(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
}

void test_tuya_transport_array_destroy_rejects_null_handle(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transport_array_destroy(NULL));
}

void test_tuya_transport_array_add_and_get_transporter_by_scheme(void)
{
    tuya_transport_array_handle_t   handle      = tuya_transport_array_create();
    struct tuya_transporter_inter_t transporter = {0};

    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_transport_array_add_transporter(handle, &transporter, "tls"));
    TEST_ASSERT_EQUAL_PTR(&transporter, tuya_transport_array_get_transporter(handle, "tls"));

    tuya_transport_array_destroy(handle);
}

void test_tuya_transport_array_add_rejects_over_capacity(void)
{
    tuya_transport_array_handle_t   handle = tuya_transport_array_create();
    struct tuya_transporter_inter_t first  = {0};
    struct tuya_transporter_inter_t second = {0};
    struct tuya_transporter_inter_t third  = {0};

    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_transport_array_add_transporter(handle, &first, "tcp"));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_transport_array_add_transporter(handle, &second, "tls"));
    TEST_ASSERT_EQUAL(OPRT_INDEX_OUT_OF_BOUND, tuya_transport_array_add_transporter(handle, &third, "ws"));

    tuya_transport_array_destroy(handle);
}

void test_tuya_transporter_wrappers_forward_to_callbacks(void)
{
    struct tuya_transporter_inter_t transporter = {0};
    uint8_t                         buffer[4]   = {0};

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_transporter_set_func(&transporter, fake_connect, fake_close, fake_read, fake_write,
                                                         fake_poll_read, fake_poll_write, fake_destroy, fake_ctrl));
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_transporter_connect(&transporter, "host", 443, 1000));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_transporter_read(&transporter, buffer, sizeof(buffer), 1000));
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tuya_transporter_write(&transporter, buffer, sizeof(buffer), 1000));
    TEST_ASSERT_EQUAL(OPRT_OS_ADAPTER_COM_ERROR, tuya_transporter_poll_read(&transporter, 1000));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transporter_poll_write(&transporter, 1000));
    TEST_ASSERT_EQUAL(OPRT_TIMEOUT, tuya_transporter_close(&transporter));
    TEST_ASSERT_EQUAL(OPRT_OS_ADAPTER_INVALID_PARM, tuya_transporter_ctrl(&transporter, 1, NULL));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_transporter_destroy(&transporter));
    TEST_ASSERT_EQUAL(1, g_destroy_calls);
}

void test_tuya_transporter_create_routes_known_types(void)
{
    TEST_ASSERT_EQUAL_PTR(&g_tcp_transporter, tuya_transporter_create(TRANSPORT_TYPE_TCP, NULL));
    TEST_ASSERT_EQUAL_PTR(&g_tls_transporter, tuya_transporter_create(TRANSPORT_TYPE_TLS, NULL));
    TEST_ASSERT_NULL(tuya_transporter_create(0xff, NULL));
}

void test_tuya_transporter_wrappers_reject_missing_callbacks(void)
{
    struct tuya_transporter_inter_t transporter = {0};
    uint8_t                         buffer[4]   = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transporter_connect(&transporter, "host", 80, 1000));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transporter_read(&transporter, buffer, sizeof(buffer), 1000));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transporter_write(&transporter, buffer, sizeof(buffer), 1000));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transporter_poll_read(&transporter, 1000));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transporter_poll_write(&transporter, 1000));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transporter_close(&transporter));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_transporter_ctrl(&transporter, 1, NULL));
}

void test_tuya_transport_array_destroy_calls_destroy_callback_for_members(void)
{
    tuya_transport_array_handle_t   handle      = tuya_transport_array_create();
    struct tuya_transporter_inter_t transporter = {
        .f_destroy = fake_destroy,
    };

    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_transport_array_add_transporter(handle, &transporter, "tcp"));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_transport_array_destroy(handle));
    TEST_ASSERT_EQUAL(1, g_destroy_calls);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_transport_array_add_transporter_rejects_null_scheme);
    RUN_TEST(test_tuya_transport_array_get_transporter_returns_null_for_empty_array);
    RUN_TEST(test_tuya_transporter_set_func_rejects_null_transporter);
    RUN_TEST(test_tuya_transport_array_destroy_rejects_null_handle);
    RUN_TEST(test_tuya_transport_array_add_and_get_transporter_by_scheme);
    RUN_TEST(test_tuya_transport_array_add_rejects_over_capacity);
    RUN_TEST(test_tuya_transporter_wrappers_forward_to_callbacks);
    RUN_TEST(test_tuya_transporter_create_routes_known_types);
    RUN_TEST(test_tuya_transporter_wrappers_reject_missing_callbacks);
    RUN_TEST(test_tuya_transport_array_destroy_calls_destroy_callback_for_members);
    return UNITY_END();
}
