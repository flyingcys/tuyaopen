#include <stdlib.h>
#include <string.h>

#include "tuya_error_code.h"
#include "tuya_iot_dp.h"
#include "unity.h"

static tuya_iot_client_t g_client;
static bool              g_iot_connected;

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

bool tuya_iot_is_connected(void)
{
    return g_iot_connected;
}

OPERATE_RET tal_workq_schedule(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data)
{
    (void)service;
    (void)cb;
    (void)data;
    return OPRT_OK;
}

OPERATE_RET tal_workq_init_delayed(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data,
                                   DELAYED_WORK_HANDLE *delayed_work)
{
    (void)service;
    (void)cb;
    (void)data;
    if (delayed_work != NULL) {
        *delayed_work = (DELAYED_WORK_HANDLE)0x1;
    }
    return OPRT_OK;
}

OPERATE_RET tal_workq_start_delayed(DELAYED_WORK_HANDLE delayed_work, TIME_MS interval, LOOP_TYPE type)
{
    (void)delayed_work;
    (void)interval;
    (void)type;
    return OPRT_OK;
}

int dp_obj_dump_stat_local_json(char *devid, dp_rept_valid_t **dpvalid, char **dpsjson, int flags)
{
    (void)devid;
    (void)dpvalid;
    (void)dpsjson;
    (void)flags;
    return OPRT_OK;
}

int tuya_iot_dp_report_json_async(tuya_iot_client_t *client, const char *dps, const char *time, tuya_dp_notify_cb_t cb,
                                  void *user_data, int timeout)
{
    (void)client;
    (void)dps;
    (void)time;
    (void)cb;
    (void)user_data;
    (void)timeout;
    return OPRT_OK;
}

int tuya_iot_dp_report_json_with_notify(tuya_iot_client_t *client, const char *dps, const char *time,
                                        tuya_dp_notify_cb_t cb, void *user_data, int timeout)
{
    (void)client;
    (void)dps;
    (void)time;
    (void)cb;
    (void)user_data;
    (void)timeout;
    return OPRT_OK;
}

int dp_data_recv_parse(dp_recv_msg_t *msg, dp_recv_cb_t cb)
{
    (void)msg;
    (void)cb;
    return OPRT_OK;
}

void dp_pv_stat_set(dp_schema_t *schema, uint8_t dpid, dp_pv_stat_t stat)
{
    (void)schema;
    (void)dpid;
    (void)stat;
}

dp_schema_t *dp_schema_find(const char *devid)
{
    (void)devid;
    return (dp_schema_t *)0x1;
}

int dp_rept_valid_check(dp_schema_t *schema, dp_rept_in_t *input, dp_rept_valid_t *valid)
{
    (void)schema;
    (void)input;
    (void)valid;
    return OPRT_OK;
}

int dp_rept_json_output(dp_schema_t *schema, dp_rept_in_t *input, dp_rept_valid_t *valid, dp_rept_out_t *out)
{
    (void)schema;
    (void)input;
    (void)valid;
    (void)out;
    return OPRT_OK;
}

int dp_rept_json_append(dp_schema_t *schema, char *dpsjson, char *timejson, char *type, uint8_t rept_seq, char **out)
{
    (void)schema;
    (void)dpsjson;
    (void)timejson;
    (void)type;
    (void)rept_seq;
    if (out != NULL) {
        *out = NULL;
    }
    return OPRT_OK;
}

char *dp_obj_dump_all_json(char *devid, int flags)
{
    (void)devid;
    (void)flags;
    return NULL;
}

int tuya_lan_dp_report(const char *out)
{
    (void)out;
    return OPRT_OK;
}

int tuya_lan_get_connect_client_num(void)
{
    return 0;
}

void tuya_base64_encode(const uint8_t *input, char *output, int input_len)
{
    (void)input;
    (void)input_len;
    if (output != NULL) {
        output[0] = '\0';
    }
}

void cJSON_Delete(cJSON *item)
{
    (void)item;
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
    g_iot_connected = false;
}

void tearDown(void) {}

void test_tuya_iot_dp_obj_dump_rejects_null_client(void)
{
    TEST_ASSERT_NULL(tuya_iot_dp_obj_dump(NULL, "dev1", 0));
}

void test_tuya_iot_dp_parse_rejects_null_json(void)
{
    TEST_ASSERT_EQUAL(OPRT_CJSON_GET_ERR, tuya_iot_dp_parse(&g_client, DP_CMD_LAN, NULL));
}

void test_tuya_iot_dp_raw_report_rejects_null_inputs(void)
{
    dp_raw_t raw = {0};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_iot_dp_raw_report(NULL, "dev1", &raw, 1000));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_iot_dp_raw_report(&g_client, "dev1", NULL, 1000));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_iot_dp_obj_dump_rejects_null_client);
    RUN_TEST(test_tuya_iot_dp_parse_rejects_null_json);
    RUN_TEST(test_tuya_iot_dp_raw_report_rejects_null_inputs);
    return UNITY_END();
}
