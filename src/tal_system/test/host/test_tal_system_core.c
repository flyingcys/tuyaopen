#include "unity.h"

#include "tal_system.h"
#include "mock_tkl_system.h"
#include "mock_tkl_memory.h"

extern void test_tal_malloc_zero_returns_null(void);
extern void test_tal_malloc_success(void);
extern void test_tal_malloc_fail_returns_null(void);
extern void test_tal_free_null_noop(void);
extern void test_tal_free_calls_tkl_free(void);
extern void test_tal_calloc_and_realloc(void);

extern SYS_TIME_T g_sys_time_offset;

void test_tal_system_enter_exit_critical(void)
{
    tkl_system_enter_critical_ExpectAndReturn(0x1234);
    TEST_ASSERT_EQUAL_HEX32(0x1234, tal_system_enter_critical());
    tkl_system_exit_critical_Expect(0x1234);
    tal_system_exit_critical(0x1234);
}

void test_tal_system_sleep_and_reset(void)
{
    tkl_system_sleep_Expect(10);
    tal_system_sleep(10);
    tkl_system_reset_Expect();
    tal_system_reset();
}

void test_tal_system_tick_ms_random(void)
{
    tkl_system_get_tick_count_ExpectAndReturn(100);
    TEST_ASSERT_EQUAL(100, tal_system_get_tick_count());

    g_sys_time_offset = 7;
    tkl_system_get_millisecond_ExpectAndReturn(1000);
    TEST_ASSERT_EQUAL(1007, tal_system_get_millisecond());

    tkl_system_get_random_ExpectAndReturn(16, 3);
    TEST_ASSERT_EQUAL(3, tal_system_get_random(16));
}

void test_tal_system_reset_reason_and_cpu_info(void)
{
    char *desc = NULL;
    tkl_system_get_reset_reason_ExpectAndReturn(&desc, TUYA_RESET_REASON_EXTERNAL);
    TEST_ASSERT_EQUAL(TUYA_RESET_REASON_EXTERNAL, tal_system_get_reset_reason(&desc));

    TUYA_CPU_INFO_T *cpu_ary = NULL;
    int32_t cpu_cnt = 0;
    tkl_system_get_cpu_info_ExpectAndReturn(&cpu_ary, (int *)&cpu_cnt, OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_system_get_cpu_info(&cpu_ary, &cpu_cnt));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_tal_malloc_zero_returns_null);
    RUN_TEST(test_tal_malloc_success);
    RUN_TEST(test_tal_malloc_fail_returns_null);
    RUN_TEST(test_tal_free_null_noop);
    RUN_TEST(test_tal_free_calls_tkl_free);
    RUN_TEST(test_tal_calloc_and_realloc);
    RUN_TEST(test_tal_system_enter_exit_critical);
    RUN_TEST(test_tal_system_sleep_and_reset);
    RUN_TEST(test_tal_system_tick_ms_random);
    RUN_TEST(test_tal_system_reset_reason_and_cpu_info);

    return UNITY_END();
}
