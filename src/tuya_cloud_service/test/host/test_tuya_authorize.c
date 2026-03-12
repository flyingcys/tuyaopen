#include <stdlib.h>
#include <string.h>

#include "tal_cli.h"
#include "tuya_authorize.h"
#include "tuya_error_code.h"
#include "unity.h"

#define TEST_UUID_KEY    "UUID_TUYAOPEN"
#define TEST_AUTHKEY_KEY "AUTHKEY_TUYAOPEN"
#define TEST_UUID_LEN    20
#define TEST_AUTHKEY_LEN 32

static OPERATE_RET      g_cli_register_ret = OPRT_OK;
static OPERATE_RET      g_license_read_ret = OPRT_COM_ERROR;
static OPERATE_RET      g_uuid_set_ret     = OPRT_OK;
static OPERATE_RET      g_authkey_set_ret  = OPRT_OK;
static OPERATE_RET      g_uuid_del_ret     = OPRT_OK;
static OPERATE_RET      g_authkey_del_ret  = OPRT_OK;
static int              g_license_read_count;
static int              g_system_reset_count;
static int              g_kv_set_count;
static int              g_kv_free_count;
static const cli_cmd_t *g_registered_cmds;
static uint8_t          g_registered_cmd_num;
static char             g_cli_echo_last[96];
static char             g_uuid_kv[TEST_UUID_LEN + 1];
static char             g_authkey_kv[TEST_AUTHKEY_LEN + 1];
static size_t           g_uuid_set_length;
static size_t           g_authkey_set_length;

int tal_kv_set(const char *key, const uint8_t *value, size_t length)
{
    if (strcmp(key, TEST_UUID_KEY) == 0) {
        g_uuid_set_length = length;
        memcpy(g_uuid_kv, value, length);
        g_uuid_kv[length] = '\0';
        g_kv_set_count++;
        return g_uuid_set_ret;
    }

    if (strcmp(key, TEST_AUTHKEY_KEY) == 0) {
        g_authkey_set_length = length;
        memcpy(g_authkey_kv, value, length);
        g_authkey_kv[length] = '\0';
        g_kv_set_count++;
        return g_authkey_set_ret;
    }

    return OPRT_INVALID_PARM;
}

int tal_kv_get(const char *key, uint8_t **value, size_t *length)
{
    if (value == NULL || length == NULL) {
        return OPRT_INVALID_PARM;
    }

    if ((strcmp(key, TEST_UUID_KEY) == 0) && g_uuid_kv[0] != '\0') {
        *length = strlen(g_uuid_kv);
        *value  = malloc(*length);
        memcpy(*value, g_uuid_kv, *length);
        return OPRT_OK;
    }

    if ((strcmp(key, TEST_AUTHKEY_KEY) == 0) && g_authkey_kv[0] != '\0') {
        *length = strlen(g_authkey_kv);
        *value  = malloc(*length);
        memcpy(*value, g_authkey_kv, *length);
        return OPRT_OK;
    }

    *value  = NULL;
    *length = 0;
    return OPRT_COM_ERROR;
}

int tal_kv_free(uint8_t *value)
{
    g_kv_free_count++;
    free(value);
    return OPRT_OK;
}

int tal_kv_del(const char *key)
{
    if (strcmp(key, TEST_UUID_KEY) == 0) {
        g_uuid_kv[0] = '\0';
        return g_uuid_del_ret;
    }

    if (strcmp(key, TEST_AUTHKEY_KEY) == 0) {
        g_authkey_kv[0] = '\0';
        return g_authkey_del_ret;
    }

    return OPRT_INVALID_PARM;
}

int tal_cli_cmd_register(const cli_cmd_t *cmd, uint8_t num)
{
    g_registered_cmds    = cmd;
    g_registered_cmd_num = num;
    return g_cli_register_ret;
}

int tuya_iot_license_read(tuya_iot_license_t *license)
{
    g_license_read_count++;
    if (g_license_read_ret == OPRT_OK && license != NULL) {
        license->uuid    = "12345678901234567890";
        license->authkey = "12345678901234567890123456789012";
    }
    return g_license_read_ret;
}

void tal_system_reset(void)
{
    g_system_reset_count++;
}

void tal_cli_echo(char *string)
{
    if (string == NULL) {
        g_cli_echo_last[0] = '\0';
        return;
    }

    strncpy(g_cli_echo_last, string, sizeof(g_cli_echo_last) - 1);
    g_cli_echo_last[sizeof(g_cli_echo_last) - 1] = '\0';
}

static cli_cmd_func_cb_t find_cli_handler(const char *name)
{
    for (uint8_t i = 0; i < g_registered_cmd_num; ++i) {
        if (strcmp(g_registered_cmds[i].name, name) == 0) {
            return g_registered_cmds[i].func;
        }
    }

    return NULL;
}

void setUp(void)
{
    g_cli_register_ret   = OPRT_OK;
    g_license_read_ret   = OPRT_COM_ERROR;
    g_uuid_set_ret       = OPRT_OK;
    g_authkey_set_ret    = OPRT_OK;
    g_uuid_del_ret       = OPRT_OK;
    g_authkey_del_ret    = OPRT_OK;
    g_license_read_count = 0;
    g_system_reset_count = 0;
    g_kv_set_count       = 0;
    g_kv_free_count      = 0;
    g_uuid_set_length    = 0;
    g_authkey_set_length = 0;
    g_registered_cmds    = NULL;
    g_registered_cmd_num = 0;
    g_cli_echo_last[0]   = '\0';
    g_uuid_kv[0]         = '\0';
    g_authkey_kv[0]      = '\0';
}

void tearDown(void) {}

void test_tuya_authorize_init_propagates_cli_register_result(void)
{
    g_cli_register_ret = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_authorize_init());
}

void test_tuya_authorize_init_registers_cli_commands(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_authorize_init());
    TEST_ASSERT_NOT_NULL(g_registered_cmds);
    TEST_ASSERT_EQUAL_UINT8(3, g_registered_cmd_num);
}

void test_tuya_authorize_write_saves_both_keys_and_resets_system(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_authorize_write("12345678901234567890", "12345678901234567890123456789012"));
    TEST_ASSERT_EQUAL(2, g_kv_set_count);
    TEST_ASSERT_EQUAL(1, g_system_reset_count);
    TEST_ASSERT_EQUAL(TEST_UUID_LEN, g_uuid_set_length);
    TEST_ASSERT_EQUAL(TEST_AUTHKEY_LEN, g_authkey_set_length);
    TEST_ASSERT_EQUAL_STRING("12345678901234567890", g_uuid_kv);
    TEST_ASSERT_EQUAL_STRING("12345678901234567890123456789012", g_authkey_kv);
}

void test_tuya_authorize_write_maps_kv_failures_to_kvs_write_fail(void)
{
    g_uuid_set_ret = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_KVS_WR_FAIL,
                      tuya_authorize_write("12345678901234567890", "12345678901234567890123456789012"));
    TEST_ASSERT_EQUAL(0, g_system_reset_count);

    g_uuid_set_ret    = OPRT_OK;
    g_authkey_set_ret = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_KVS_WR_FAIL,
                      tuya_authorize_write("12345678901234567890", "12345678901234567890123456789012"));
    TEST_ASSERT_EQUAL(0, g_system_reset_count);
}

void test_tuya_authorize_read_rejects_null_license(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_authorize_read(NULL));
}

void test_tuya_authorize_read_prefers_kv_over_otp(void)
{
    tuya_iot_license_t license = {0};

    memcpy(g_uuid_kv, "kv-uuid-123456789012", TEST_UUID_LEN + 1);
    memcpy(g_authkey_kv, "kv-authkey-123456789012345678901", TEST_AUTHKEY_LEN + 1);
    g_license_read_ret = OPRT_OK;

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_authorize_read(&license));
    TEST_ASSERT_EQUAL_STRING("kv-uuid-123456789012", license.uuid);
    TEST_ASSERT_EQUAL_STRING("kv-authkey-123456789012345678901", license.authkey);
    TEST_ASSERT_EQUAL(0, g_license_read_count);
    TEST_ASSERT_EQUAL(2, g_kv_free_count);
}

void test_tuya_authorize_read_falls_back_to_license_read(void)
{
    tuya_iot_license_t license = {0};
    g_license_read_ret         = OPRT_OK;

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_authorize_read(&license));
    TEST_ASSERT_EQUAL_STRING("12345678901234567890", license.uuid);
    TEST_ASSERT_EQUAL_STRING("12345678901234567890123456789012", license.authkey);
    TEST_ASSERT_EQUAL(1, g_license_read_count);
}

void test_tuya_authorize_read_returns_com_error_when_kv_and_otp_both_fail(void)
{
    tuya_iot_license_t license = {0};

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_authorize_read(&license));
    TEST_ASSERT_EQUAL(1, g_license_read_count);
}

void test_tuya_authorize_reset_reports_delete_status(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_authorize_reset());

    g_authkey_del_ret = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_KVS_WR_FAIL, tuya_authorize_reset());
}

void test_tuya_authorize_cli_authorize_validates_arguments_and_reports_result(void)
{
    cli_cmd_func_cb_t cli_authorize = NULL;
    char             *short_args[]  = {"auth", "short", "bad"};
    char             *valid_args[]  = {"auth", "12345678901234567890", "12345678901234567890123456789012"};

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_authorize_init());
    cli_authorize = find_cli_handler("auth");
    TEST_ASSERT_NOT_NULL(cli_authorize);

    cli_authorize(1, short_args);
    TEST_ASSERT_EQUAL_STRING("Use like: auth uuidxxxxxxxxxxxxxxxx keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", g_cli_echo_last);

    cli_authorize(3, short_args);
    TEST_ASSERT_EQUAL_STRING("uuid len not equal 20 or authkey len not equal 32.", g_cli_echo_last);

    cli_authorize(3, valid_args);
    TEST_ASSERT_EQUAL_STRING("Authorization write succeeds.", g_cli_echo_last);

    g_authkey_set_ret = OPRT_COM_ERROR;
    cli_authorize(3, valid_args);
    TEST_ASSERT_EQUAL_STRING("Authorization write failure.", g_cli_echo_last);
}

void test_tuya_authorize_cli_read_and_reset_forward_runtime_results(void)
{
    cli_cmd_func_cb_t cli_read  = NULL;
    cli_cmd_func_cb_t cli_reset = NULL;
    char             *argv[]    = {"auth-read"};

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_authorize_init());
    cli_read  = find_cli_handler("auth-read");
    cli_reset = find_cli_handler("auth-reset");
    TEST_ASSERT_NOT_NULL(cli_read);
    TEST_ASSERT_NOT_NULL(cli_reset);

    cli_read(1, argv);
    TEST_ASSERT_EQUAL_STRING("Authorization read failure.", g_cli_echo_last);

    memcpy(g_uuid_kv, "12345678901234567890", TEST_UUID_LEN + 1);
    memcpy(g_authkey_kv, "12345678901234567890123456789012", TEST_AUTHKEY_LEN + 1);
    cli_read(1, argv);
    TEST_ASSERT_EQUAL_STRING("12345678901234567890123456789012", g_cli_echo_last);

    cli_reset(1, argv);
    TEST_ASSERT_EQUAL_STRING("Authorization reset succeeds.", g_cli_echo_last);

    g_authkey_del_ret = OPRT_COM_ERROR;
    cli_reset(1, argv);
    TEST_ASSERT_EQUAL_STRING("Authorization reset failure.", g_cli_echo_last);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_authorize_init_propagates_cli_register_result);
    RUN_TEST(test_tuya_authorize_init_registers_cli_commands);
    RUN_TEST(test_tuya_authorize_write_saves_both_keys_and_resets_system);
    RUN_TEST(test_tuya_authorize_write_maps_kv_failures_to_kvs_write_fail);
    RUN_TEST(test_tuya_authorize_read_rejects_null_license);
    RUN_TEST(test_tuya_authorize_read_prefers_kv_over_otp);
    RUN_TEST(test_tuya_authorize_read_falls_back_to_license_read);
    RUN_TEST(test_tuya_authorize_read_returns_com_error_when_kv_and_otp_both_fail);
    RUN_TEST(test_tuya_authorize_reset_reports_delete_status);
    RUN_TEST(test_tuya_authorize_cli_authorize_validates_arguments_and_reports_result);
    RUN_TEST(test_tuya_authorize_cli_read_and_reset_forward_runtime_results);
    return UNITY_END();
}
