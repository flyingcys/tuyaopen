#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "tuya_error_code.h"
#include "tuya_iot.h"
#include "tuya_weather.h"
#include "unity.h"

static tuya_iot_client_t g_client;
static bool              g_is_activated;
static bool              g_network_connected;

void *tal_malloc(size_t size)
{
    return calloc(1, size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

tuya_iot_client_t *tuya_iot_client_get(void)
{
    return &g_client;
}

bool tuya_iot_activated(tuya_iot_client_t *client)
{
    (void)client;
    return g_is_activated;
}

OPERATE_RET tal_time_check_time_sync(void)
{
    return OPRT_OK;
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

int cJSON_HasObjectItem(const cJSON *object, const char *string)
{
    (void)object;
    (void)string;
    return 0;
}

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string)
{
    (void)object;
    (void)string;
    return NULL;
}

void setUp(void)
{
    memset(&g_client, 0, sizeof(g_client));
    g_is_activated                = false;
    g_network_connected           = false;
    g_client.config.network_check = NULL;
}

void tearDown(void) {}

static bool fake_network_check(void)
{
    return g_network_connected;
}

void test_tuya_weather_get_current_conditions_rejects_null_output(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_weather_get_current_conditions(NULL));
}

void test_tuya_weather_allow_update_requires_activation(void)
{
    g_client.config.network_check = fake_network_check;
    g_network_connected           = true;

    TEST_ASSERT_FALSE(tuya_weather_allow_update());
}

void test_tuya_weather_allow_update_requires_connected_network(void)
{
    g_is_activated                = true;
    g_client.config.network_check = fake_network_check;
    g_network_connected           = false;

    TEST_ASSERT_FALSE(tuya_weather_allow_update());
}

void test_tuya_weather_allow_update_accepts_activated_connected_client(void)
{
    g_is_activated                = true;
    g_client.config.network_check = fake_network_check;
    g_network_connected           = true;

    TEST_ASSERT_TRUE(tuya_weather_allow_update());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_weather_get_current_conditions_rejects_null_output);
    RUN_TEST(test_tuya_weather_allow_update_requires_activation);
    RUN_TEST(test_tuya_weather_allow_update_requires_connected_network);
    RUN_TEST(test_tuya_weather_allow_update_accepts_activated_connected_client);
    return UNITY_END();
}
