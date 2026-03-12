#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mqtt_service.h"
#include "tuya_cloud_types.h"
#include "tuya_error_code.h"
#include "tuya_protocol.h"
#include "unity.h"

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

TIME_T tal_time_get_posix(void)
{
    return 1700000000;
}

uint16_t mqtt_client_publish(void *client, const char *topic, const uint8_t *payload, size_t length, uint8_t qos)
{
    (void)client;
    (void)topic;
    (void)payload;
    (void)length;
    (void)qos;
    return 1;
}

mqtt_client_status_t mqtt_client_deinit(void *client)
{
    (void)client;
    return MQTT_STATUS_SUCCESS;
}

void mqtt_client_free(void *client)
{
    (void)client;
}

int tuya_pack_protocol_data(const DP_CMD_TYPE_E cmd, const char *src, const uint32_t pro, uint8_t *key, char **out,
                            uint32_t *out_len)
{
    (void)cmd;
    (void)src;
    (void)pro;
    (void)key;
    if (out != NULL) {
        *out = NULL;
    }
    if (out_len != NULL) {
        *out_len = 0;
    }
    return OPRT_OK;
}

static void dummy_protocol_cb(tuya_protocol_event_t *event)
{
    (void)event;
}

void setUp(void) {}

void tearDown(void) {}

void test_tuya_mqtt_connected_returns_false_for_null_context(void)
{
    TEST_ASSERT_FALSE(tuya_mqtt_connected(NULL));
}

void test_tuya_mqtt_protocol_register_rejects_invalid_parameters(void)
{
    tuya_mqtt_context_t context = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_mqtt_protocol_register(NULL, 1, dummy_protocol_cb, NULL));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_mqtt_protocol_register(&context, 1, dummy_protocol_cb, NULL));
    context.is_inited = true;
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_mqtt_protocol_register(&context, 1, NULL, NULL));
}

void test_tuya_mqtt_protocol_data_publish_common_rejects_null_context(void)
{
    uint8_t payload[] = "abc";

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_mqtt_protocol_data_publish_common(NULL, 1, payload, sizeof(payload) - 1,
                                                                                NULL, NULL, 0, false));
}

void test_tuya_mqtt_destory_rejects_invalid_context(void)
{
    tuya_mqtt_context_t context = {0};

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_mqtt_destory(NULL));
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_mqtt_destory(&context));
}

void test_tuya_mqtt_upgrade_progress_report_rejects_invalid_percent(void)
{
    tuya_mqtt_context_t context = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_mqtt_upgrade_progress_report(&context, 0, 101));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_mqtt_connected_returns_false_for_null_context);
    RUN_TEST(test_tuya_mqtt_protocol_register_rejects_invalid_parameters);
    RUN_TEST(test_tuya_mqtt_protocol_data_publish_common_rejects_null_context);
    RUN_TEST(test_tuya_mqtt_destory_rejects_invalid_context);
    RUN_TEST(test_tuya_mqtt_upgrade_progress_report_rejects_invalid_percent);
    return UNITY_END();
}
