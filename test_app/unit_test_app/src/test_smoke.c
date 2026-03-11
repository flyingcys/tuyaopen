/**
 * @file test_smoke.c
 * @brief Target-side smoke suite.
 *
 * This file contains the minimal smoke suite used to verify that the target
 * harness and runner protocol are alive.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>

#include "unity.h"

static void test_target_smoke_hello(void)
{
    printf("UT_TARGET_SMOKE_OK\r\n");
    TEST_ASSERT_TRUE(1);
}

int tuya_unit_test_run_smoke_suite(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_target_smoke_hello);
    return UNITY_END();
}
