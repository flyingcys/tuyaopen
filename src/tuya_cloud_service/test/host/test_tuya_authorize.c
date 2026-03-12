#include <stdlib.h>
#include <string.h>

#include "tal_cli.h"
#include "tuya_authorize.h"
#include "tuya_error_code.h"
#include "unity.h"

static OPERATE_RET g_cli_register_ret = OPRT_OK;
static OPERATE_RET g_license_read_ret = OPRT_COM_ERROR;

int tal_kv_set(const char *key, const uint8_t *value, size_t length)
{
    (void)key;
    (void)value;
    (void)length;
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

int tal_kv_free(uint8_t *value)
{
    (void)value;
    return OPRT_OK;
}

int tal_kv_del(const char *key)
{
    (void)key;
    return OPRT_OK;
}

int tal_cli_cmd_register(const cli_cmd_t *cmd, uint8_t num)
{
    (void)cmd;
    (void)num;
    return g_cli_register_ret;
}

int tuya_iot_license_read(tuya_iot_license_t *license)
{
    if (g_license_read_ret == OPRT_OK && license != NULL) {
        license->uuid    = "12345678901234567890";
        license->authkey = "12345678901234567890123456789012";
    }
    return g_license_read_ret;
}

void tal_system_reset(void) {}

void tal_cli_echo(char *string)
{
    (void)string;
}

void setUp(void)
{
    g_cli_register_ret = OPRT_OK;
    g_license_read_ret = OPRT_COM_ERROR;
}

void tearDown(void) {}

void test_tuya_authorize_init_propagates_cli_register_result(void)
{
    g_cli_register_ret = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_authorize_init());
}

void test_tuya_authorize_read_rejects_null_license(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_authorize_read(NULL));
}

void test_tuya_authorize_read_falls_back_to_license_read(void)
{
    tuya_iot_license_t license = {0};
    g_license_read_ret         = OPRT_OK;

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_authorize_read(&license));
    TEST_ASSERT_EQUAL_STRING("12345678901234567890", license.uuid);
    TEST_ASSERT_EQUAL_STRING("12345678901234567890123456789012", license.authkey);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_authorize_init_propagates_cli_register_result);
    RUN_TEST(test_tuya_authorize_read_rejects_null_license);
    RUN_TEST(test_tuya_authorize_read_falls_back_to_license_read);
    return UNITY_END();
}
