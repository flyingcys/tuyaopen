#include "unity.h"

#include "tal_wifi.h"
#include "mock_tal_mutex.h"
#include "mock_tkl_wifi.h"

void test_tal_wifi_station_connect_invalid_ssid(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_wifi_station_connect(NULL, (int8_t *)"pwd"));
}

void test_tal_wifi_station_connect_success(void)
{
    tkl_wifi_station_connect_ExpectAnyArgsAndReturn(OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_station_connect((int8_t *)"ssid", (int8_t *)"pwd"));
}

void test_tal_wifi_station_disconnect_success(void)
{
    tkl_wifi_station_disconnect_ExpectAndReturn(OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_station_disconnect());
}

void test_tal_wifi_all_ap_scan_success(void)
{
    AP_IF_S *ap_ary = NULL;
    uint32_t num = 0;
    tkl_wifi_scan_ap_ExpectAnyArgsAndReturn(OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_all_ap_scan(&ap_ary, &num));
}

void test_tal_wifi_sniffer_set_success(void)
{
    tkl_wifi_set_sniffer_ExpectAnyArgsAndReturn(OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_sniffer_set(TRUE, NULL));
}
