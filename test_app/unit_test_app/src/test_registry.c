/**
 * @file test_registry.c
 * @brief Target unit test suite registry.
 *
 * This file owns the suite registry used by the Target-side unit test harness.
 * Suites are registered by deterministic names so both local process execution
 * and future serial-driven runners can target the same interface.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <string.h>

#include "test_registry.h"

static const tuya_unit_test_suite_t s_target_suites[] = {
    {.name = "smoke", .help = "run the target smoke suite", .run = tuya_unit_test_run_smoke_suite},
    {.name = "tkl_gpio", .help = "run target-side tkl_gpio adapter tests", .run = tuya_unit_test_run_tkl_gpio_suite},
};

const tuya_unit_test_suite_t *tuya_unit_test_get_suites(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(s_target_suites) / sizeof(s_target_suites[0]);
    }

    return s_target_suites;
}

const tuya_unit_test_suite_t *tuya_unit_test_find_suite(const char *name)
{
    size_t                        count  = 0;
    const tuya_unit_test_suite_t *suites = tuya_unit_test_get_suites(&count);

    if (name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < count; i++) {
        if (strcmp(suites[i].name, name) == 0) {
            return &suites[i];
        }
    }

    return NULL;
}

int tuya_unit_test_run_suite(const char *name)
{
    const tuya_unit_test_suite_t *suite = tuya_unit_test_find_suite(name);

    if (suite == NULL) {
        printf("UT_SUITE_ERROR:%s:not_found\r\n", (name != NULL) ? name : "(null)");
        return -1;
    }

    printf("UT_SUITE_BEGIN:%s\r\n", suite->name);
    fflush(stdout);

    int rc = suite->run();

    printf("UT_SUITE_END:%s:%s\r\n", suite->name, (rc == 0) ? "OK" : "FAIL");
    fflush(stdout);
    return rc;
}

int tuya_unit_test_run_all(void)
{
    size_t                        count  = 0;
    const tuya_unit_test_suite_t *suites = tuya_unit_test_get_suites(&count);
    int                           failed = 0;

    for (size_t i = 0; i < count; i++) {
        if (tuya_unit_test_run_suite(suites[i].name) != 0) {
            failed++;
        }
    }

    return (failed == 0) ? 0 : -1;
}

void tuya_unit_test_print_suite_list(void)
{
    size_t                        count  = 0;
    const tuya_unit_test_suite_t *suites = tuya_unit_test_get_suites(&count);

    printf("UT_SUITE_LIST_BEGIN\r\n");
    for (size_t i = 0; i < count; i++) {
        printf("UT_SUITE_ITEM:%s:%s\r\n", suites[i].name, suites[i].help);
    }
    printf("UT_SUITE_LIST_END\r\n");
    fflush(stdout);
}
