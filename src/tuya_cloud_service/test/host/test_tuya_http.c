#include <stdlib.h>
#include <string.h>

#include "http_client_interface.h"
#include "http_parser.h"
#include "tuya_error_code.h"
#include "unity.h"

int tuya_http_cert_save(char *host, uint16_t port, uint8_t *cacert, uint16_t cacert_len);
int tuya_http_cert_load(char *host, uint16_t port, uint8_t **cacert, uint16_t *cacert_len);
int tuya_http_client_post_simple(char *url, char *body, http_client_header_t *headers, uint8_t headers_count,
                                 http_client_response_t *response);
int tuya_http_free(http_client_response_t *response);

static http_client_response_t *g_last_http_free;

void *tal_malloc(size_t size)
{
    return calloc(1, size);
}

void *tal_calloc(size_t nitems, size_t size)
{
    return calloc(nitems, size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

TIME_T tal_time_get_posix(void)
{
    return 1700000000;
}

int tuya_iotdns_query_host_certs(const char *host, uint16_t port, uint8_t **cacert, uint16_t *cacert_len)
{
    (void)host;
    (void)port;
    if (cacert != NULL) {
        *cacert = NULL;
    }
    if (cacert_len != NULL) {
        *cacert_len = 0;
    }
    return OPRT_COM_ERROR;
}

void http_parser_url_init(struct http_parser_url *u)
{
    memset(u, 0, sizeof(*u));
}

int http_parser_parse_url(const char *buf, size_t buflen, int is_connect, struct http_parser_url *u)
{
    (void)buf;
    (void)buflen;
    (void)is_connect;
    (void)u;
    return -1;
}

http_client_status_t http_client_request(const http_client_request_t *request, http_client_response_t *response)
{
    (void)request;
    (void)response;
    return HTTP_CLIENT_SEND_FAULT;
}

int http_client_free(http_client_response_t *response)
{
    g_last_http_free = response;
    return OPRT_OK;
}

void setUp(void)
{
    g_last_http_free = NULL;
}

void tearDown(void) {}

void test_tuya_http_cert_save_rejects_invalid_parameters(void)
{
    uint8_t cert[4] = {1, 2, 3, 4};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_http_cert_save(NULL, 443, cert, sizeof(cert)));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_http_cert_save("example.com", 443, NULL, sizeof(cert)));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_http_cert_save("example.com", 443, cert, 0));
}

void test_tuya_http_cert_load_rejects_null_outputs(void)
{
    uint8_t *cacert     = NULL;
    uint16_t cacert_len = 0;

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_http_cert_load("example.com", 443, NULL, &cacert_len));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_http_cert_load("example.com", 443, &cacert, NULL));
}

void test_tuya_http_client_post_simple_maps_parse_error(void)
{
    http_client_response_t response = {0};

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_http_client_post_simple("not a url", NULL, NULL, 0, &response));
}

void test_tuya_http_free_forwards_to_http_client_free(void)
{
    http_client_response_t response = {0};

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_http_free(&response));
    TEST_ASSERT_EQUAL_PTR(&response, g_last_http_free);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_http_cert_save_rejects_invalid_parameters);
    RUN_TEST(test_tuya_http_cert_load_rejects_null_outputs);
    RUN_TEST(test_tuya_http_client_post_simple_maps_parse_error);
    RUN_TEST(test_tuya_http_free_forwards_to_http_client_free);
    return UNITY_END();
}
