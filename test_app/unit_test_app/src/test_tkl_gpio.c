/**
 * @file test_tkl_gpio.c
 * @brief Target-side tkl_gpio suite.
 *
 * This file contains the first real Target-side suite for the test harness. It
 * focuses on API contract checks that are valid even when the host machine does
 * not expose a usable GPIO chip device.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_gpio.h"
#include "tuya_error_code.h"
#include "unity.h"

static void dummy_gpio_irq_cb(void *arg)
{
    (void)arg;
}

static void test_tkl_gpio_init_rejects_null_cfg(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_init(TUYA_GPIO_NUM_0, NULL));
}

static void test_tkl_gpio_init_rejects_invalid_pin(void)
{
    TUYA_GPIO_BASE_CFG_T cfg = {
        .mode   = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
        .level  = TUYA_GPIO_LEVEL_LOW,
    };

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_init(TUYA_GPIO_NUM_MAX, &cfg));
}

static void test_tkl_gpio_read_rejects_null_level(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_read(TUYA_GPIO_NUM_0, NULL));
}

static void test_tkl_gpio_read_rejects_invalid_pin(void)
{
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_NONE;

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_read(TUYA_GPIO_NUM_MAX, &level));
}

static void test_tkl_gpio_write_rejects_invalid_pin(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_write(TUYA_GPIO_NUM_MAX, TUYA_GPIO_LEVEL_HIGH));
}

static void test_tkl_gpio_irq_init_rejects_null_cfg(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_irq_init(TUYA_GPIO_NUM_0, NULL));
}

static void test_tkl_gpio_irq_init_rejects_invalid_pin(void)
{
    TUYA_GPIO_IRQ_T irq_cfg = {
        .mode = TUYA_GPIO_IRQ_RISE,
        .cb   = dummy_gpio_irq_cb,
        .arg  = NULL,
    };

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_irq_init(TUYA_GPIO_NUM_MAX, &irq_cfg));
}

static void test_tkl_gpio_irq_enable_disable_rejects_invalid_pin(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_irq_enable(TUYA_GPIO_NUM_MAX));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_irq_disable(TUYA_GPIO_NUM_MAX));
}

static void test_tkl_gpio_deinit_rejects_invalid_pin(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tkl_gpio_deinit(TUYA_GPIO_NUM_MAX));
}

int tuya_unit_test_run_tkl_gpio_suite(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tkl_gpio_init_rejects_null_cfg);
    RUN_TEST(test_tkl_gpio_init_rejects_invalid_pin);
    RUN_TEST(test_tkl_gpio_read_rejects_null_level);
    RUN_TEST(test_tkl_gpio_read_rejects_invalid_pin);
    RUN_TEST(test_tkl_gpio_write_rejects_invalid_pin);
    RUN_TEST(test_tkl_gpio_irq_init_rejects_null_cfg);
    RUN_TEST(test_tkl_gpio_irq_init_rejects_invalid_pin);
    RUN_TEST(test_tkl_gpio_irq_enable_disable_rejects_invalid_pin);
    RUN_TEST(test_tkl_gpio_deinit_rejects_invalid_pin);
    return UNITY_END();
}
