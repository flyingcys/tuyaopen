/**
 * @file test_main.c
 * @brief Target unit test application entry.
 *
 * This file provides the minimal TuyaOpen application entry for the target-side
 * unit test skeleton. It initializes logging, prints a deterministic Unity
 * start banner for the pytest runner, and dispatches the registered suites
 * through Unity on both Linux and non-Linux targets.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */
/**
 * @file test_main.c
 * @brief Target unit test application entry.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include <stdio.h>
#include <string.h>

#include "tal_api.h"
#include "tal_cli.h"
#include "tkl_output.h"
#include "unity.h"

#include "test_registry.h"

void setUp(void) {}

void tearDown(void) {}

static int tuya_unit_test_main(void)
{
    printf("UNITY_BEGIN\r\n");
    fflush(stdout);

    UNITY_BEGIN();
    tuya_unit_test_register_all();
    return UNITY_END();
}

static int user_main(void)
{
    int result = 0;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Target unit test app: %s", PROJECT_NAME);
    result = tuya_unit_test_main();
    PR_NOTICE("Target unit test app finished with rc=%d", result);

    return result;
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    return user_main();
}
#else
static THREAD_HANDLE ty_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    (void)arg;
    (void)user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};

    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority   = THREAD_PRIO_1;
    thrd_param.thrdname   = "tuya_app_main";

    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
