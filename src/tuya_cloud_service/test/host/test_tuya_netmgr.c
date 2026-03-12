#include "netmgr.h"
#include "tuya_error_code.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

void test_netmgr_conn_set_reports_not_ready_before_init(void)
{
    TEST_ASSERT_EQUAL(OPRT_RESOURCE_NOT_READY, netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_STATUS, NULL));
}

void test_netmgr_conn_get_reports_not_ready_before_init(void)
{
    TEST_ASSERT_EQUAL(OPRT_RESOURCE_NOT_READY, netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_netmgr_conn_set_reports_not_ready_before_init);
    RUN_TEST(test_netmgr_conn_get_reports_not_ready_before_init);
    return UNITY_END();
}
