#include "unity.h"

#include "tal_wifi.h"
#include "mock_tkl_wifi.h"

extern void test_tal_wifi_init_invalid_param(void);
extern void test_tal_wifi_init_success(void);
extern void test_tal_wifi_get_work_mode_before_set_returns_unknown(void);
extern void test_tal_wifi_set_work_mode_and_get_mode(void);
extern void test_tal_wifi_station_connect_invalid_ssid(void);
extern void test_tal_wifi_station_connect_success(void);
extern void test_tal_wifi_station_disconnect_success(void);
extern void test_tal_wifi_all_ap_scan_success(void);
extern void test_tal_wifi_sniffer_set_success(void);

static OPERATE_RET g_lp_enable_ret = OPRT_OK;
static OPERATE_RET g_lp_disable_ret = OPRT_OK;

OPERATE_RET tal_cpu_lp_enable(void)
{
    return g_lp_enable_ret;
}

OPERATE_RET tal_cpu_lp_disable(void)
{
    return g_lp_disable_ret;
}

void test_tal_wifi_ap_start_invalid_param(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_wifi_ap_start(NULL));
}

void test_tal_wifi_ap_start_stop_success(void)
{
    WF_AP_CFG_IF_S cfg;
    tkl_wifi_start_ap_ExpectAnyArgsAndReturn(OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_ap_start(&cfg));

    tkl_wifi_stop_ap_ExpectAndReturn(OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_ap_stop());
}

void test_tal_wifi_set_country_code_success(void)
{
    tkl_wifi_set_country_code_ExpectAndReturn(COUNTRY_CODE_US, OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_set_country_code("US"));
}

void test_tal_wifi_lp_disable_then_enable(void)
{
    tkl_wifi_set_lp_mode_ExpectAndReturn(FALSE, 0, OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_lp_disable());

    tkl_wifi_set_lp_mode_ExpectAndReturn(TRUE, 1, OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_lp_enable());
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_tal_wifi_init_invalid_param);
    RUN_TEST(test_tal_wifi_init_success);
    RUN_TEST(test_tal_wifi_get_work_mode_before_set_returns_unknown);
    RUN_TEST(test_tal_wifi_set_work_mode_and_get_mode);
    RUN_TEST(test_tal_wifi_station_connect_invalid_ssid);
    RUN_TEST(test_tal_wifi_station_connect_success);
    RUN_TEST(test_tal_wifi_station_disconnect_success);
    RUN_TEST(test_tal_wifi_all_ap_scan_success);
    RUN_TEST(test_tal_wifi_sniffer_set_success);
    RUN_TEST(test_tal_wifi_ap_start_invalid_param);
    RUN_TEST(test_tal_wifi_ap_start_stop_success);
    RUN_TEST(test_tal_wifi_set_country_code_success);
    RUN_TEST(test_tal_wifi_lp_disable_then_enable);

    return UNITY_END();
}
