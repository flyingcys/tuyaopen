#include "unity.h"

#include "tal_wifi.h"
#include "mock_tal_mutex.h"
#include "mock_tkl_wifi.h"

static void dummy_wifi_event_cb(WF_EVENT_E event, void *arg)
{
    (void)event;
    (void)arg;
}

void setUp(void)
{
    tal_mutex_lock_IgnoreAndReturn(OPRT_OK);
    tal_mutex_unlock_IgnoreAndReturn(OPRT_OK);
}

void tearDown(void)
{
}

void test_tal_wifi_init_invalid_param(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_wifi_init(NULL));
}

void test_tal_wifi_init_success(void)
{
    tal_mutex_create_init_ExpectAnyArgsAndReturn(OPRT_OK);
    tkl_wifi_init_ExpectAnyArgsAndReturn(OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_init(dummy_wifi_event_cb));
}

void test_tal_wifi_get_work_mode_before_set_returns_unknown(void)
{
    WF_WK_MD_E mode = WWM_STATION;
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_get_work_mode(&mode));
    TEST_ASSERT_EQUAL(WWM_UNKNOWN, mode);
}

void test_tal_wifi_set_work_mode_and_get_mode(void)
{
    WF_WK_MD_E mode = WWM_UNKNOWN;
    tkl_wifi_set_work_mode_ExpectAndReturn(WWM_STATION, OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_set_work_mode(WWM_STATION));

    tkl_wifi_get_work_mode_ExpectAnyArgsAndReturn(OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_wifi_get_work_mode(&mode));
}
