/**
 * @file test_registry.h
 * @brief Target unit test registry declarations.
 *
 * This header declares the suite registry interface used by the Target unit
 * test harness.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef TEST_APP_UNIT_TEST_APP_SRC_TEST_REGISTRY_H_
#define TEST_APP_UNIT_TEST_APP_SRC_TEST_REGISTRY_H_

#include <stddef.h>

typedef int (*tuya_unit_test_run_fn_t)(void);

typedef struct {
    const char             *name;
    const char             *help;
    tuya_unit_test_run_fn_t run;
} tuya_unit_test_suite_t;

const tuya_unit_test_suite_t *tuya_unit_test_get_suites(size_t *count);
const tuya_unit_test_suite_t *tuya_unit_test_find_suite(const char *name);
int                           tuya_unit_test_run_suite(const char *name);
int                           tuya_unit_test_run_all(void);
void                          tuya_unit_test_print_suite_list(void);

int tuya_unit_test_run_smoke_suite(void);
int tuya_unit_test_run_tkl_gpio_suite(void);

#endif /* TEST_APP_UNIT_TEST_APP_SRC_TEST_REGISTRY_H_ */
