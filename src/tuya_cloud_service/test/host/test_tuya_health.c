#include <stdlib.h>

#include "tal_event.h"
#include "tal_system.h"
#include "tal_workq_service.h"
#include "tuya_error_code.h"
#include "tuya_health.h"
#include "unity.h"

static OPERATE_RET g_tal_mutex_create_init_ret = OPRT_OK;
static OPERATE_RET g_tal_event_subscribe_ret   = OPRT_OK;
static OPERATE_RET g_tal_thread_create_ret     = OPRT_OK;

void *tal_malloc(size_t size)
{
    return calloc(1, size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

OPERATE_RET tal_mutex_create_init(MUTEX_HANDLE *handle)
{
    if (handle != NULL) {
        *handle = (MUTEX_HANDLE)0x1;
    }
    return g_tal_mutex_create_init_ret;
}

OPERATE_RET tal_mutex_lock(const MUTEX_HANDLE handle)
{
    (void)handle;
    return OPRT_OK;
}

OPERATE_RET tal_mutex_unlock(const MUTEX_HANDLE handle)
{
    (void)handle;
    return OPRT_OK;
}

OPERATE_RET tal_mutex_release(const MUTEX_HANDLE handle)
{
    (void)handle;
    return OPRT_OK;
}

OPERATE_RET tal_event_publish(const char *name, void *data)
{
    (void)name;
    (void)data;
    return OPRT_OK;
}

OPERATE_RET tal_event_subscribe(const char *name, const char *desc, const EVENT_SUBSCRIBE_CB cb, SUBSCRIBE_TYPE_E type)
{
    (void)name;
    (void)desc;
    (void)cb;
    (void)type;
    return g_tal_event_subscribe_ret;
}

OPERATE_RET tal_event_unsubscribe(const char *name, const char *desc, EVENT_SUBSCRIBE_CB cb)
{
    (void)name;
    (void)desc;
    (void)cb;
    return OPRT_OK;
}

OPERATE_RET tal_thread_create_and_start(THREAD_HANDLE *handle, const THREAD_ENTER_CB enter, const THREAD_EXIT_CB exit,
                                        const THREAD_FUNC_CB func, const void *func_args, const THREAD_CFG_T *cfg)
{
    (void)enter;
    (void)exit;
    (void)func;
    (void)func_args;
    (void)cfg;
    if (handle != NULL) {
        *handle = (THREAD_HANDLE)0x2;
    }
    return g_tal_thread_create_ret;
}

OPERATE_RET tal_thread_delete(const THREAD_HANDLE handle)
{
    (void)handle;
    return OPRT_OK;
}

OPERATE_RET tal_workq_schedule(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data)
{
    (void)service;
    (void)cb;
    (void)data;
    return OPRT_OK;
}

uint16_t tal_workq_get_num(WORKQ_SERVICE_E service)
{
    (void)service;
    return 0;
}

void tal_workq_dump(WORKQ_SERVICE_E service)
{
    (void)service;
}

int tal_sw_timer_get_num(void)
{
    return 0;
}

TIME_T tal_time_get_posix(void)
{
    return 1700000000;
}

void tal_system_reset(void) {}

void tal_system_sleep(uint32_t time_ms)
{
    (void)time_ms;
}

SYS_TIME_T tal_system_get_millisecond(void)
{
    return 0;
}

int tal_system_get_free_heap_size(void)
{
    return 1024 * 64;
}

void tal_thread_dump_watermark(void) {}

void tuya_list_add(const P_LIST_HEAD pNew, const P_LIST_HEAD pHead)
{
    (void)pNew;
    (void)pHead;
}

void tuya_list_del(const P_LIST_HEAD pEntry)
{
    (void)pEntry;
}

int tuya_list_empty(const P_LIST_HEAD pHead)
{
    (void)pHead;
    return 1;
}

void setUp(void)
{
    g_tal_mutex_create_init_ret = OPRT_OK;
    g_tal_event_subscribe_ret   = OPRT_OK;
    g_tal_thread_create_ret     = OPRT_OK;
}

void tearDown(void) {}

void test_tuya_health_item_add_rejects_uninitialized_monitor(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_health_item_add(1, 60, NULL, NULL));
}

void test_tuya_health_item_del_ignores_uninitialized_monitor(void)
{
    tuya_health_item_del(1);
    TEST_PASS();
}

void test_tuya_health_update_item_period_ignores_uninitialized_monitor(void)
{
    tuya_health_update_item_period(1, 30);
    TEST_PASS();
}

void test_tuya_health_update_item_threshold_ignores_uninitialized_monitor(void)
{
    tuya_health_update_item_threshold(1, 10);
    TEST_PASS();
}

void test_tuya_health_disable_watchdog_ignores_uninitialized_monitor(void)
{
    tuya_health_disable_watchdog();
    TEST_PASS();
}

void test_tuya_health_monitor_init_propagates_mutex_create_failure(void)
{
    g_tal_mutex_create_init_ret = OPRT_COM_ERROR;

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_health_monitor_init());
}

void test_tuya_health_monitor_init_returns_ok_when_dependencies_succeed(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_health_monitor_init());
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_health_monitor_init());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_health_item_add_rejects_uninitialized_monitor);
    RUN_TEST(test_tuya_health_item_del_ignores_uninitialized_monitor);
    RUN_TEST(test_tuya_health_update_item_period_ignores_uninitialized_monitor);
    RUN_TEST(test_tuya_health_update_item_threshold_ignores_uninitialized_monitor);
    RUN_TEST(test_tuya_health_disable_watchdog_ignores_uninitialized_monitor);
    RUN_TEST(test_tuya_health_monitor_init_propagates_mutex_create_failure);
    RUN_TEST(test_tuya_health_monitor_init_returns_ok_when_dependencies_succeed);
    return UNITY_END();
}
