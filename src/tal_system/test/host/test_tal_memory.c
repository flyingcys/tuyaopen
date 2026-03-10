#include "unity.h"

#include "tal_memory.h"
#include "mock_tkl_memory.h"
#include "mock_tkl_system.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_tal_malloc_zero_returns_null(void)
{
    TEST_ASSERT_NULL(tal_malloc(0));
}

void test_tal_malloc_success(void)
{
    void *dummy = (void *)0x12345678;
    tkl_system_malloc_ExpectAndReturn(16, dummy);
    TEST_ASSERT_EQUAL_PTR(dummy, tal_malloc(16));
}

void test_tal_malloc_fail_returns_null(void)
{
    tkl_system_malloc_ExpectAndReturn(16, NULL);
    tkl_system_get_free_heap_size_ExpectAndReturn(1024);
    TEST_ASSERT_NULL(tal_malloc(16));
}

void test_tal_free_null_noop(void)
{
    tal_free(NULL);
}

void test_tal_free_calls_tkl_free(void)
{
    void *dummy = (void *)0x13572468;
    tkl_system_free_Expect(dummy);
    tal_free(dummy);
}

void test_tal_calloc_and_realloc(void)
{
    void *dummy1 = (void *)0x10203040;
    void *dummy2 = (void *)0x99887766;
    tkl_system_calloc_ExpectAndReturn(2, 8, dummy1);
    tkl_system_realloc_ExpectAndReturn(dummy1, 64, dummy2);
    TEST_ASSERT_EQUAL_PTR(dummy1, tal_calloc(2, 8));
    TEST_ASSERT_EQUAL_PTR(dummy2, tal_realloc(dummy1, 64));
}
