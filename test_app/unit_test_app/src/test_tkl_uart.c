/**
 * @file test_tkl_uart.c
 * @brief Target-side tkl_uart suite.
 *
 * This file keeps the UART Target suite Linux-safe by covering only contract
 * paths that do not require opening real UART endpoints on the local machine.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_uart.h"
#include "tuya_error_code.h"
#include "unity.h"

static void test_tkl_uart_write_rejects_unsupported_port(void)
{
    unsigned char data = 0x5a;

    TEST_ASSERT_LESS_THAN(0, tkl_uart_write(TUYA_UART_NUM_2, &data, 1));
}

static void test_tkl_uart_set_tx_int_reports_not_supported(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_uart_set_tx_int(TUYA_UART_NUM_2, TRUE));
}

static void test_tkl_uart_set_rx_flowctrl_reports_not_supported(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_uart_set_rx_flowctrl(TUYA_UART_NUM_2, TRUE));
}

static void test_tkl_uart_wait_for_data_reports_not_supported(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_uart_wait_for_data(TUYA_UART_NUM_2, 0));
}

static void test_tkl_uart_ioctl_reports_not_supported(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_uart_ioctl(TUYA_UART_NUM_2, 0, NULL));
}

int tuya_unit_test_run_tkl_uart_suite(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tkl_uart_write_rejects_unsupported_port);
    RUN_TEST(test_tkl_uart_set_tx_int_reports_not_supported);
    RUN_TEST(test_tkl_uart_set_rx_flowctrl_reports_not_supported);
    RUN_TEST(test_tkl_uart_wait_for_data_reports_not_supported);
    RUN_TEST(test_tkl_uart_ioctl_reports_not_supported);
    return UNITY_END();
}
