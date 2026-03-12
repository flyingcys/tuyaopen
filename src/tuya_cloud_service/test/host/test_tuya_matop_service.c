#include <stdlib.h>
#include <string.h>

#include "matop_service.h"
#include "tuya_error_code.h"
#include "unity.h"

int tuya_mqtt_subscribe_message_callback_register(tuya_mqtt_context_t *context, const char *topic,
                                                  mqtt_subscribe_message_cb_t cb, void *userdata)
{
    (void)context;
    (void)topic;
    (void)cb;
    (void)userdata;
    return OPRT_OK;
}

int tuya_mqtt_protocol_data_publish_with_topic_common(tuya_mqtt_context_t *context, const char *topic,
                                                      uint16_t protocol_id, const uint8_t *data, uint16_t length,
                                                      mqtt_publish_notify_cb_t cb, void *user_data, int timeout_ms,
                                                      bool async)
{
    (void)context;
    (void)topic;
    (void)protocol_id;
    (void)data;
    (void)length;
    (void)cb;
    (void)user_data;
    (void)timeout_ms;
    (void)async;
    return OPRT_OK;
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

SYS_TIME_T tal_system_get_millisecond(void)
{
    return 0;
}

int atop_base_response_parse(const char *input, atop_base_response_t *response)
{
    (void)input;
    (void)response;
    return OPRT_OK;
}

cJSON *cJSON_Parse(const char *value)
{
    (void)value;
    return NULL;
}

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string)
{
    (void)object;
    (void)string;
    return NULL;
}

void cJSON_Delete(cJSON *item)
{
    (void)item;
}

cJSON_bool cJSON_IsTrue(const cJSON *item)
{
    (void)item;
    return false;
}

void setUp(void) {}

void tearDown(void) {}

void test_matop_service_init_rejects_null_inputs(void)
{
    matop_context_t context = {0};
    matop_config_t  config  = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, matop_serice_init(NULL, &config));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, matop_serice_init(&context, NULL));
}

void test_matop_service_client_reset_rejects_null_context(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, matop_service_client_reset(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_matop_service_init_rejects_null_inputs);
    RUN_TEST(test_matop_service_client_reset_rejects_null_context);
    return UNITY_END();
}
