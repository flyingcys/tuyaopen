/**
 * @file test_protocol.c
 * @brief Target unit test dispatch protocol implementation.
 *
 * This file implements a tiny suite-addressable protocol for the Target unit
 * test harness. On Linux it parses command-line arguments; on future serial
 * targets the same operations are exposed as CLI commands.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <string.h>

#include "tal_cli.h"

#include "test_protocol.h"
#include "test_registry.h"

static void tuya_unit_test_print_help(void)
{
    printf("Usage:\r\n");
    printf("  --list            List registered target suites\r\n");
    printf("  --all             Run all registered target suites\r\n");
    printf("  --suite <name>    Run one named target suite\r\n");
    fflush(stdout);
}

static int tuya_unit_test_dispatch_named(const char *suite_name)
{
    if (suite_name == NULL) {
        tuya_unit_test_print_help();
        return -1;
    }

    return tuya_unit_test_run_suite(suite_name);
}

static void tuya_unit_test_cli_list_cmd(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    tuya_unit_test_print_suite_list();
}

static void tuya_unit_test_cli_run_cmd(int argc, char *argv[])
{
    if (argc < 2) {
        char help[] = "Usage: ut_run <suite>";
        tal_cli_echo(help);
        return;
    }

    (void)tuya_unit_test_dispatch_named(argv[1]);
}

static void tuya_unit_test_cli_help_cmd(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    tuya_unit_test_print_help();
}

void tuya_unit_test_cli_register(void)
{
#if OPERATING_SYSTEM != SYSTEM_LINUX
    static cli_cmd_t s_ut_cli_cmds[] = {
        {.name = "ut_list", .help = "list target unit-test suites", .func = tuya_unit_test_cli_list_cmd},
        {.name = "ut_run", .help = "run one target unit-test suite", .func = tuya_unit_test_cli_run_cmd},
        {.name = "ut_help", .help = "print target unit-test help", .func = tuya_unit_test_cli_help_cmd},
    };

    tal_cli_init();
    tal_cli_cmd_register(s_ut_cli_cmds, sizeof(s_ut_cli_cmds) / sizeof(s_ut_cli_cmds[0]));
#endif
}

int tuya_unit_test_dispatch(int argc, char *argv[])
{
    if (argc <= 1) {
        printf("UNITY_BEGIN\r\n");
        fflush(stdout);
        return tuya_unit_test_dispatch_named("smoke");
    }

    if (strcmp(argv[1], "--list") == 0) {
        tuya_unit_test_print_suite_list();
        return 0;
    }

    if (strcmp(argv[1], "--all") == 0) {
        printf("UNITY_BEGIN\r\n");
        fflush(stdout);
        return tuya_unit_test_run_all();
    }

    if ((strcmp(argv[1], "--suite") == 0) && (argc >= 3)) {
        printf("UNITY_BEGIN\r\n");
        fflush(stdout);
        return tuya_unit_test_dispatch_named(argv[2]);
    }

    tuya_unit_test_print_help();
    return -1;
}
