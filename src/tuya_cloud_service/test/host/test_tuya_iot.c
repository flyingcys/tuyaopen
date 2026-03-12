#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "tuya_error_code.h"
#include "tuya_iot.h"
#include "unity.h"

void *tal_malloc(size_t size)
{
    return calloc(1, size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

int tuya_mqtt_protocol_data_publish_common(tuya_mqtt_context_t *context, uint16_t protocol, const uint8_t *data,
                                           uint16_t len, mqtt_publish_notify_cb_t cb, void *user_data, int timeout_ms,
                                           bool async)
{
    (void)context;
    (void)protocol;
    (void)data;
    (void)len;
    (void)cb;
    (void)user_data;
    (void)timeout_ms;
    (void)async;
    return OPRT_OK;
}

int tal_kv_get(const char *key, uint8_t **value, size_t *length)
{
    (void)key;
    if (value != NULL) {
        *value = NULL;
    }
    if (length != NULL) {
        *length = 0;
    }
    return OPRT_COM_ERROR;
}

int tal_kv_set(const char *key, const uint8_t *value, size_t length)
{
    (void)key;
    (void)value;
    (void)length;
    return OPRT_OK;
}

int tal_kv_del(const char *key)
{
    (void)key;
    return OPRT_OK;
}

int tal_kv_free(uint8_t *value)
{
    (void)value;
    return OPRT_OK;
}

int tuya_lan_disable(void)
{
    return OPRT_OK;
}

int dp_schema_delete(char *devid)
{
    (void)devid;
    return OPRT_OK;
}

int tuya_endpoint_remove(void)
{
    return OPRT_OK;
}

bool tuya_mqtt_connected(tuya_mqtt_context_t *mqtt)
{
    (void)mqtt;
    return false;
}

int atop_service_version_update_v41(const char *devid, const char *seckey, const char *versions)
{
    (void)devid;
    (void)seckey;
    (void)versions;
    return OPRT_OK;
}

static int dummy_token_get(tuya_iot_config_t *config)
{
    (void)config;
    return OPRT_OK;
}

static int dummy_token_get_2(tuya_iot_config_t *config)
{
    (void)config;
    return OPRT_OK;
}

void setUp(void) {}

void tearDown(void) {}

void test_tuya_iot_activated_returns_false_for_null_client(void)
{
    TEST_ASSERT_FALSE(tuya_iot_activated(NULL));
}

void test_tuya_iot_token_get_port_register_rejects_invalid_parameters(void)
{
    tuya_iot_client_t client = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_iot_token_get_port_register(NULL, dummy_token_get));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_iot_token_get_port_register(&client, NULL));
}

void test_tuya_iot_token_get_port_register_deduplicates_and_enforces_capacity(void)
{
    tuya_iot_client_t client = {0};

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_iot_token_get_port_register(&client, dummy_token_get));
    TEST_ASSERT_EQUAL(1, client.token_get.count);
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_iot_token_get_port_register(&client, dummy_token_get));
    TEST_ASSERT_EQUAL(1, client.token_get.count);
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_iot_token_get_port_register(&client, dummy_token_get_2));
    TEST_ASSERT_EQUAL(2, client.token_get.count);
    TEST_ASSERT_EQUAL(OPRT_MSG_OUT_OF_LIMIT, tuya_iot_token_get_port_register(&client, (tuya_token_get_cb_t)0x3));
}

void test_tuya_iot_dp_report_json_async_rejects_invalid_parameters(void)
{
    tuya_iot_client_t client = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_iot_dp_report_json_async(NULL, "{}", NULL, NULL, NULL, 1000));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_iot_dp_report_json_async(&client, NULL, NULL, NULL, NULL, 1000));
}

void test_tuya_iot_extension_modules_version_update_rejects_invalid_parameters(void)
{
    tuya_iot_client_t client = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_iot_extension_modules_version_update(NULL, "[]"));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_iot_extension_modules_version_update(&client, NULL));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_iot_extension_modules_version_update(&client, ""));
}

void test_tuya_iot_getters_return_null_for_null_client(void)
{
    TEST_ASSERT_NULL(tuya_iot_devid_get(NULL));
    TEST_ASSERT_NULL(tuya_iot_localkey_get(NULL));
    TEST_ASSERT_NULL(tuya_iot_seckey_get(NULL));
    TEST_ASSERT_NULL(tuya_iot_timezone_get(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_iot_activated_returns_false_for_null_client);
    RUN_TEST(test_tuya_iot_token_get_port_register_rejects_invalid_parameters);
    RUN_TEST(test_tuya_iot_token_get_port_register_deduplicates_and_enforces_capacity);
    RUN_TEST(test_tuya_iot_dp_report_json_async_rejects_invalid_parameters);
    RUN_TEST(test_tuya_iot_extension_modules_version_update_rejects_invalid_parameters);
    RUN_TEST(test_tuya_iot_getters_return_null_for_null_client);
    return UNITY_END();
}
