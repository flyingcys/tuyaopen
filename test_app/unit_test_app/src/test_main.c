/**
 * @file test_main.c
 * @brief Target unit test application entry.
 *
 * This file provides the application entry for the Target-side unit test
 * harness. It supports local `LINUX` suite selection through command-line
 * arguments and exposes the same suite execution protocol to future serial/CLI
 * flows on non-Linux targets.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <string.h>

#include "tal_api.h"
#include "tal_cli.h"
#include "tkl_output.h"

#include "test_protocol.h"

void setUp(void) {}

void tearDown(void) {}

static int user_main(int argc, char *argv[])
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Target unit test app: %s", PROJECT_NAME);
    return tuya_unit_test_dispatch(argc, argv);
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
int main(int argc, char *argv[])
{
    return user_main(argc, argv);
}
#else
static THREAD_HANDLE ty_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    char *argv[] = {"unit_test_app"};

    (void)arg;
    (void)user_main(1, argv);

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};

    tuya_unit_test_cli_register();

    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority   = THREAD_PRIO_1;
    thrd_param.thrdname   = "tuya_app_main";

    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
