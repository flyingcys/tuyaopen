/**
 * @file test_registry.c
 * @brief Target unit test suite registry.
 *
 * This file owns the first target-side Unity smoke test and the suite
 * registration entry used by `test_main.c`. New target test suites should be
 * added here incrementally as the runner infrastructure expands.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */
/**
 * @file test_registry.c
 * @brief Target unit test registration and smoke cases.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include <stdio.h>

#include "unity.h"

static void test_target_smoke_hello(void)
{
    printf("UT_TARGET_SMOKE_OK\r\n");
    TEST_ASSERT_TRUE(1);
}

void tuya_unit_test_register_all(void)
{
    RUN_TEST(test_target_smoke_hello);
}
