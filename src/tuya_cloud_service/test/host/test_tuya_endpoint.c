#include "tuya_cloud_service_test_hooks.h"

#include "mock_tal_kv.h"
#include "mock_tal_memory.h"
#include "tuya_endpoint.h"
#include "tuya_error_code.h"
#include "unity.h"

tuya_endpoint_iotdns_fake_t g_tuya_endpoint_iotdns_fake;

void tuya_endpoint_iotdns_fake_reset(void)
{
    g_tuya_endpoint_iotdns_fake.calls         = 0;
    g_tuya_endpoint_iotdns_fake.last_region   = NULL;
    g_tuya_endpoint_iotdns_fake.last_env      = NULL;
    g_tuya_endpoint_iotdns_fake.last_endpoint = NULL;
    g_tuya_endpoint_iotdns_fake.return_value  = OPRT_OK;
}

int iotdns_cloud_endpoint_get(const char *region, const char *env, tuya_endpoint_t *endpoint)
{
    g_tuya_endpoint_iotdns_fake.calls++;
    g_tuya_endpoint_iotdns_fake.last_region   = region;
    g_tuya_endpoint_iotdns_fake.last_env      = env;
    g_tuya_endpoint_iotdns_fake.last_endpoint = endpoint;
    return g_tuya_endpoint_iotdns_fake.return_value;
}

void setUp(void)
{
    tuya_endpoint_iotdns_fake_reset();
}

void tearDown(void) {}

void test_tuya_endpoint_region_regist_set_rejects_null_region(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_endpoint_region_regist_set(NULL, "pro"));
}

void test_tuya_endpoint_cert_get_rejects_null(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_endpoint_cert_get(NULL));
}

void test_tuya_endpoint_cert_set_rejects_null(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_endpoint_cert_set(NULL));
}

void test_tuya_endpoint_domain_get_rejects_null(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_endpoint_domain_get(NULL));
}

void test_tuya_endpoint_domain_set_rejects_null(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_endpoint_domain_set(NULL));
}

void test_tuya_endpoint_remove_deletes_known_keys(void)
{
    tal_kv_del_ExpectAndReturn("region", OPRT_OK);
    tal_kv_del_ExpectAndReturn("regist_key", OPRT_OK);
    tal_kv_del_ExpectAndReturn("endpoint.cert", OPRT_OK);
    tal_kv_del_ExpectAndReturn("endpoint.domain", OPRT_OK);

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_endpoint_remove());
}

void test_tuya_endpoint_init_defaults_empty_regist_key_to_pro(void)
{
    static uint8_t region_value[]      = "cn";
    static uint8_t regist_key_value[4] = {0};
    uint8_t       *region_ptr          = region_value;
    uint8_t       *regist_key_ptr      = regist_key_value;
    size_t         region_len          = 2;
    size_t         regist_key_len      = 0;

    tal_kv_get_ExpectAnyArgsAndReturn(OPRT_OK);
    tal_kv_get_ReturnThruPtr_value(&region_ptr);
    tal_kv_get_ReturnThruPtr_length(&region_len);
    tal_kv_free_ExpectAndReturn(region_value, OPRT_OK);

    tal_kv_get_ExpectAnyArgsAndReturn(OPRT_OK);
    tal_kv_get_ReturnThruPtr_value(&regist_key_ptr);
    tal_kv_get_ReturnThruPtr_length(&regist_key_len);
    tal_kv_free_ExpectAndReturn(regist_key_value, OPRT_OK);

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_endpoint_init());
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_endpoint_update());
    TEST_ASSERT_EQUAL(1, g_tuya_endpoint_iotdns_fake.calls);
    TEST_ASSERT_EQUAL_STRING("cn", g_tuya_endpoint_iotdns_fake.last_region);
    TEST_ASSERT_EQUAL_STRING("pro", g_tuya_endpoint_iotdns_fake.last_env);
}

void test_tuya_endpoint_update_frees_existing_cert_before_lookup(void)
{
    static uint8_t   old_cert[] = "old";
    tuya_endpoint_t *endpoint   = (tuya_endpoint_t *)tuya_endpoint_get();

    endpoint->cert     = old_cert;
    endpoint->cert_len = sizeof(old_cert);

    tal_free_Expect(old_cert);

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_endpoint_update());
    TEST_ASSERT_EQUAL(1, g_tuya_endpoint_iotdns_fake.calls);
    TEST_ASSERT_NULL(endpoint->cert);
    TEST_ASSERT_EQUAL_UINT(0, endpoint->cert_len);
}

void test_tuya_endpoint_update_auto_region_uses_null_region_and_saved_env(void)
{
    tal_kv_set_ExpectAndReturn("region", (const uint8_t *)"eu", 2, OPRT_OK);
    tal_kv_set_ExpectAndReturn("regist_key", (const uint8_t *)"prod", 4, OPRT_OK);

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_endpoint_region_regist_set("eu", "prod"));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_endpoint_update_auto_region());
    TEST_ASSERT_EQUAL(1, g_tuya_endpoint_iotdns_fake.calls);
    TEST_ASSERT_NULL(g_tuya_endpoint_iotdns_fake.last_region);
    TEST_ASSERT_EQUAL_STRING("prod", g_tuya_endpoint_iotdns_fake.last_env);
}

void test_tuya_endpoint_cert_get_returns_value_from_kv(void)
{
    static uint8_t  cert_value[] = "cert";
    uint8_t        *cert_ptr     = cert_value;
    size_t          cert_len     = sizeof(cert_value) - 1;
    tuya_endpoint_t endpoint     = {0};

    tal_kv_get_ExpectAnyArgsAndReturn(OPRT_OK);
    tal_kv_get_ReturnThruPtr_value(&cert_ptr);
    tal_kv_get_ReturnThruPtr_length(&cert_len);

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_endpoint_cert_get(&endpoint));
    TEST_ASSERT_EQUAL_PTR(cert_value, endpoint.cert);
    TEST_ASSERT_EQUAL_UINT(cert_len, endpoint.cert_len);
}

void test_tuya_endpoint_cert_set_propagates_kv_error(void)
{
    tuya_endpoint_t endpoint = {
        .cert     = (uint8_t *)"abc",
        .cert_len = 3,
    };

    tal_kv_set_ExpectAndReturn("endpoint.cert", endpoint.cert, endpoint.cert_len, OPRT_COM_ERROR);

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_endpoint_cert_set(&endpoint));
}

void test_tuya_endpoint_domain_set_propagates_serialize_error(void)
{
    tuya_endpoint_t endpoint = {0};

    tal_kv_serialize_set_ExpectAnyArgsAndReturn(OPRT_COM_ERROR);

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_endpoint_domain_set(&endpoint));
}

void test_tuya_endpoint_init_propagates_kv_read_failure(void)
{
    tal_kv_get_ExpectAnyArgsAndReturn(OPRT_COM_ERROR);

    TEST_ASSERT_EQUAL(OPRT_KVS_RD_FAIL, tuya_endpoint_init());
    TEST_ASSERT_EQUAL(0, g_tuya_endpoint_iotdns_fake.calls);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_endpoint_region_regist_set_rejects_null_region);
    RUN_TEST(test_tuya_endpoint_cert_get_rejects_null);
    RUN_TEST(test_tuya_endpoint_cert_set_rejects_null);
    RUN_TEST(test_tuya_endpoint_domain_get_rejects_null);
    RUN_TEST(test_tuya_endpoint_domain_set_rejects_null);
    RUN_TEST(test_tuya_endpoint_remove_deletes_known_keys);
    RUN_TEST(test_tuya_endpoint_init_defaults_empty_regist_key_to_pro);
    RUN_TEST(test_tuya_endpoint_update_frees_existing_cert_before_lookup);
    RUN_TEST(test_tuya_endpoint_update_auto_region_uses_null_region_and_saved_env);
    RUN_TEST(test_tuya_endpoint_cert_get_returns_value_from_kv);
    RUN_TEST(test_tuya_endpoint_cert_set_propagates_kv_error);
    RUN_TEST(test_tuya_endpoint_domain_set_propagates_serialize_error);
    RUN_TEST(test_tuya_endpoint_init_propagates_kv_read_failure);
    return UNITY_END();
}
