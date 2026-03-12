#include "tuya_error_code.h"
#include "tuya_lan.h"
#include "unity.h"

static int dummy_lan_handler(const uint8_t *data, uint8_t **out)
{
    (void)data;
    (void)out;
    return OPRT_OK;
}

void setUp(void) {}

void tearDown(void) {}

void test_tuya_lan_register_cb_rejects_null_handler(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_lan_register_cb(FRM_TP_CMD, NULL));
}

void test_tuya_lan_unregister_cb_reports_not_found_for_unknown_frame(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_FOUND, tuya_lan_unregister_cb(0x9999));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_lan_register_cb_rejects_null_handler);
    RUN_TEST(test_tuya_lan_unregister_cb_reports_not_found_for_unknown_frame);
    return UNITY_END();
}
