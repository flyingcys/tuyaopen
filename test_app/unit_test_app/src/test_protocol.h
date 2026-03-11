/**
 * @file test_protocol.h
 * @brief Target unit test dispatch protocol declarations.
 *
 * This header exposes the small command protocol used by the target unit test
 * harness for both local process execution and future CLI-driven board
 * execution.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef TEST_APP_UNIT_TEST_APP_SRC_TEST_PROTOCOL_H_
#define TEST_APP_UNIT_TEST_APP_SRC_TEST_PROTOCOL_H_

int  tuya_unit_test_dispatch(int argc, char *argv[]);
void tuya_unit_test_cli_register(void);

#endif /* TEST_APP_UNIT_TEST_APP_SRC_TEST_PROTOCOL_H_ */
