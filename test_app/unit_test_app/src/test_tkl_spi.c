/**
 * @file test_tkl_spi.c
 * @brief Target-side tkl_spi suite.
 *
 * This file keeps the SPI Target suite Linux-safe by asserting only adapter
 * contracts that are stable without requiring a live spidev endpoint.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_spi.h"
#include "tuya_error_code.h"
#include "unity.h"

static void dummy_spi_irq_cb(TUYA_SPI_NUM_E port, TUYA_SPI_IRQ_EVT_E event)
{
    (void)port;
    (void)event;
}

static TUYA_SPI_BASE_CFG_T make_spi_cfg(TUYA_SPI_ROLE_E role)
{
    TUYA_SPI_BASE_CFG_T cfg = {
        .role          = role,
        .mode          = TUYA_SPI_MODE0,
        .type          = TUYA_SPI_AUTO_TYPE,
        .databits      = TUYA_SPI_DATA_BIT8,
        .bitorder      = TUYA_SPI_ORDER_MSB2LSB,
        .freq_hz       = 1000000,
        .spi_dma_flags = 0,
    };

    return cfg;
}

static void test_tkl_spi_init_rejects_null_cfg(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_init(TUYA_SPI_NUM_0, NULL));
}

static void test_tkl_spi_init_rejects_invalid_port(void)
{
    TUYA_SPI_BASE_CFG_T cfg = make_spi_cfg(TUYA_SPI_ROLE_MASTER);

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_init(TUYA_SPI_NUM_MAX, &cfg));
}

static void test_tkl_spi_init_rejects_slave_role(void)
{
    TUYA_SPI_BASE_CFG_T cfg = make_spi_cfg(TUYA_SPI_ROLE_SLAVE);

    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_spi_init(TUYA_SPI_NUM_0, &cfg));
}

static void test_tkl_spi_send_recv_transfer_validate_parameters(void)
{
    unsigned char tx_data = 0x12;
    unsigned char rx_data = 0;

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_send(TUYA_SPI_NUM_0, NULL, 1));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_send(TUYA_SPI_NUM_0, &tx_data, 0));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_recv(TUYA_SPI_NUM_0, NULL, 1));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_recv(TUYA_SPI_NUM_0, &rx_data, 0));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_transfer(TUYA_SPI_NUM_0, NULL, &rx_data, 1));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_transfer(TUYA_SPI_NUM_0, &tx_data, NULL, 1));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_transfer(TUYA_SPI_NUM_0, &tx_data, &rx_data, 0));
}

static void test_tkl_spi_transfer_with_length_requires_initialized_port(void)
{
    unsigned char tx_data = 0x12;
    unsigned char rx_data = 0;

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tkl_spi_transfer_with_length(TUYA_SPI_NUM_0, &tx_data, 0, &rx_data, 0));
}

static void test_tkl_spi_get_status_reports_idle_zeroed_state(void)
{
    TUYA_SPI_STATUS_T status = {
        .busy       = 1,
        .data_lost  = 1,
        .mode_fault = 1,
    };

    TEST_ASSERT_EQUAL(OPRT_OK, tkl_spi_get_status(TUYA_SPI_NUM_0, &status));
    TEST_ASSERT_EQUAL_UINT32(0, status.busy);
    TEST_ASSERT_EQUAL_UINT32(0, status.data_lost);
    TEST_ASSERT_EQUAL_UINT32(0, status.mode_fault);
}

static void test_tkl_spi_get_status_rejects_null_output(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_spi_get_status(TUYA_SPI_NUM_0, NULL));
}

static void test_tkl_spi_irq_controls_report_not_supported(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_spi_irq_init(TUYA_SPI_NUM_0, dummy_spi_irq_cb));
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_spi_irq_enable(TUYA_SPI_NUM_0));
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_spi_irq_disable(TUYA_SPI_NUM_0));
}

static void test_tkl_spi_abort_transfer_reports_success(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tkl_spi_abort_transfer(TUYA_SPI_NUM_0));
}

static void test_tkl_spi_get_data_count_reports_default_zero_and_invalid_port(void)
{
    TEST_ASSERT_EQUAL_INT32(0, tkl_spi_get_data_count(TUYA_SPI_NUM_0));
    TEST_ASSERT_EQUAL_INT32(-1, tkl_spi_get_data_count(TUYA_SPI_NUM_MAX));
}

static void test_tkl_spi_ioctl_reports_not_supported(void)
{
    TEST_ASSERT_EQUAL(OPRT_NOT_SUPPORTED, tkl_spi_ioctl(TUYA_SPI_NUM_0, 0, NULL));
}

static void test_tkl_spi_get_max_dma_data_length_reports_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, tkl_spi_get_max_dma_data_length());
}

int tuya_unit_test_run_tkl_spi_suite(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tkl_spi_init_rejects_null_cfg);
    RUN_TEST(test_tkl_spi_init_rejects_invalid_port);
    RUN_TEST(test_tkl_spi_init_rejects_slave_role);
    RUN_TEST(test_tkl_spi_send_recv_transfer_validate_parameters);
    RUN_TEST(test_tkl_spi_transfer_with_length_requires_initialized_port);
    RUN_TEST(test_tkl_spi_get_status_reports_idle_zeroed_state);
    RUN_TEST(test_tkl_spi_get_status_rejects_null_output);
    RUN_TEST(test_tkl_spi_irq_controls_report_not_supported);
    RUN_TEST(test_tkl_spi_abort_transfer_reports_success);
    RUN_TEST(test_tkl_spi_get_data_count_reports_default_zero_and_invalid_port);
    RUN_TEST(test_tkl_spi_ioctl_reports_not_supported);
    RUN_TEST(test_tkl_spi_get_max_dma_data_length_reports_zero);
    return UNITY_END();
}
