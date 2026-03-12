/**
 * @file test_tkl_i2c.c
 * @brief Target-side tkl_i2c suite.
 *
 * This file keeps the I2C Target suite Linux-safe by asserting only adapter
 * contracts that do not depend on a real `/dev/i2c-*` device being present.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include <string.h>

#include "tkl_i2c.h"
#include "tuya_error_code.h"
#include "unity.h"

static void dummy_i2c_irq_cb(TUYA_I2C_NUM_E port, TUYA_IIC_IRQ_EVT_E event)
{
    (void)port;
    (void)event;
}

static void test_tkl_i2c_init_rejects_null_cfg(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_i2c_init(TUYA_I2C_NUM_0, NULL));
}

static void test_tkl_i2c_init_rejects_invalid_port(void)
{
    TUYA_IIC_BASE_CFG_T cfg = {
        .role       = TUYA_IIC_MODE_MASTER,
        .speed      = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_i2c_init(TUYA_I2C_NUM_MAX, &cfg));
}

static void test_tkl_i2c_deinit_rejects_invalid_port(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_i2c_deinit(TUYA_I2C_NUM_MAX));
}

static void test_tkl_i2c_master_send_rejects_invalid_port(void)
{
    static const unsigned char data[] = {0x12, 0x34};

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_i2c_master_send(TUYA_I2C_NUM_MAX, 0x50, data, sizeof(data), FALSE));
}

static void test_tkl_i2c_master_receive_rejects_null_buffer(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_i2c_master_receive(TUYA_I2C_NUM_0, 0x50, NULL, 1, FALSE));
}

static void test_tkl_i2c_master_receive_rejects_zero_size(void)
{
    unsigned char data = 0;

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_i2c_master_receive(TUYA_I2C_NUM_0, 0x50, &data, 0, FALSE));
}

static void test_tkl_i2c_master_receive_rejects_invalid_port(void)
{
    unsigned char data = 0;

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_i2c_master_receive(TUYA_I2C_NUM_MAX, 0x50, &data, 1, FALSE));
}

static void test_tkl_i2c_irq_controls_report_not_supported(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_i2c_irq_init(TUYA_I2C_NUM_0, dummy_i2c_irq_cb));
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_i2c_irq_enable(TUYA_I2C_NUM_0));
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_i2c_irq_disable(TUYA_I2C_NUM_0));
}

static void test_tkl_i2c_slave_apis_report_not_supported(void)
{
    unsigned char data = 0;

    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_i2c_set_slave_addr(TUYA_I2C_NUM_0, 0x50));
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_i2c_slave_send(TUYA_I2C_NUM_0, &data, 1));
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_i2c_slave_receive(TUYA_I2C_NUM_0, &data, 1));
}

static void test_tkl_i2c_get_status_rejects_null_output(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_i2c_get_status(TUYA_I2C_NUM_0, NULL));
}

static void test_tkl_i2c_get_status_reports_not_supported_and_clears_output(void)
{
    TUYA_IIC_STATUS_T status;

    memset(&status, 0xff, sizeof(status));

    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_i2c_get_status(TUYA_I2C_NUM_0, &status));
    TEST_ASSERT_EQUAL_UINT32(0, status.busy);
    TEST_ASSERT_EQUAL_UINT32(0, status.mode);
    TEST_ASSERT_EQUAL_UINT32(0, status.direction);
    TEST_ASSERT_EQUAL_UINT32(0, status.general_call);
    TEST_ASSERT_EQUAL_UINT32(0, status.arbitration_lost);
    TEST_ASSERT_EQUAL_UINT32(0, status.bus_error);
}

static void test_tkl_i2c_reset_reports_ok(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tkl_i2c_reset(TUYA_I2C_NUM_0));
}

static void test_tkl_i2c_get_data_count_rejects_invalid_port(void)
{
    TEST_ASSERT_EQUAL(-1, tkl_i2c_get_data_count(TUYA_I2C_NUM_MAX));
}

static void test_tkl_i2c_ioctl_reports_not_supported(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_i2c_ioctl(TUYA_I2C_NUM_0, I2C_IOCTL_SET_REGADDR_WIDTH, NULL));
}

int tuya_unit_test_run_tkl_i2c_suite(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tkl_i2c_init_rejects_null_cfg);
    RUN_TEST(test_tkl_i2c_init_rejects_invalid_port);
    RUN_TEST(test_tkl_i2c_deinit_rejects_invalid_port);
    RUN_TEST(test_tkl_i2c_master_send_rejects_invalid_port);
    RUN_TEST(test_tkl_i2c_master_receive_rejects_null_buffer);
    RUN_TEST(test_tkl_i2c_master_receive_rejects_zero_size);
    RUN_TEST(test_tkl_i2c_master_receive_rejects_invalid_port);
    RUN_TEST(test_tkl_i2c_irq_controls_report_not_supported);
    RUN_TEST(test_tkl_i2c_slave_apis_report_not_supported);
    RUN_TEST(test_tkl_i2c_get_status_rejects_null_output);
    RUN_TEST(test_tkl_i2c_get_status_reports_not_supported_and_clears_output);
    RUN_TEST(test_tkl_i2c_reset_reports_ok);
    RUN_TEST(test_tkl_i2c_get_data_count_rejects_invalid_port);
    RUN_TEST(test_tkl_i2c_ioctl_reports_not_supported);
    return UNITY_END();
}
