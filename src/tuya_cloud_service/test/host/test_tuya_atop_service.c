#include <stdlib.h>
#include <string.h>

#include "atop_service.h"
#include "tuya_error_code.h"
#include "tuya_iot.h"
#include "unity.h"

static tuya_iot_client_t *g_client_ptr;

void *tal_malloc(size_t size)
{
    return calloc(1, size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

TIME_T tal_time_get_posix(void)
{
    return 1700000000;
}

int atop_base_request(const atop_base_request_t *request, atop_base_response_t *response)
{
    (void)request;
    if (response != NULL) {
        memset(response, 0, sizeof(*response));
    }
    return OPRT_OK;
}

void atop_base_response_free(atop_base_response_t *response)
{
    (void)response;
}

cJSON *cJSON_Duplicate(const cJSON *item, cJSON_bool recurse)
{
    (void)item;
    (void)recurse;
    return NULL;
}

tuya_iot_client_t *tuya_iot_client_get(void)
{
    return g_client_ptr;
}

void setUp(void)
{
    g_client_ptr = NULL;
}

void tearDown(void) {}

void test_atop_service_activate_request_rejects_invalid_params(void)
{
    atop_base_response_t    response = {0};
    tuya_activite_request_t request  = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, atop_service_activate_request(NULL, &response));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, atop_service_activate_request(&request, NULL));
}

void test_atop_service_version_update_v41_rejects_invalid_params(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, atop_service_version_update_v41(NULL, "key", "[]"));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, atop_service_version_update_v41("id", NULL, "[]"));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, atop_service_version_update_v41("id", "key", NULL));
}

void test_atop_service_comm_post_simple_rejects_invalid_api_version(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, atop_service_comm_post_simple(NULL, "1.0", NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, atop_service_comm_post_simple("api", NULL, NULL, NULL, NULL));
}

void test_atop_service_comm_post_simple_rejects_missing_client(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, atop_service_comm_post_simple("tuya.device.test", "1.0", NULL, NULL, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_atop_service_activate_request_rejects_invalid_params);
    RUN_TEST(test_atop_service_version_update_v41_rejects_invalid_params);
    RUN_TEST(test_atop_service_comm_post_simple_rejects_invalid_api_version);
    RUN_TEST(test_atop_service_comm_post_simple_rejects_missing_client);
    return UNITY_END();
}
