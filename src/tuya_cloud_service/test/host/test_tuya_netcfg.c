#include <stdlib.h>

#include "netcfg.h"
#include "tal_workq_service.h"
#include "tuya_error_code.h"
#include "unity.h"

void *tal_malloc(size_t size)
{
    return calloc(1, size);
}

void tal_free(void *ptr)
{
    free(ptr);
}

OPERATE_RET tal_workq_schedule(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data)
{
    (void)service;
    (void)cb;
    (void)data;
    return OPRT_OK;
}

OPERATE_RET tal_workq_cancel(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data)
{
    (void)service;
    (void)cb;
    (void)data;
    return OPRT_OK;
}

int tuya_list_empty(const P_LIST_HEAD pHead)
{
    return pHead->next == pHead;
}

void tuya_list_add(const P_LIST_HEAD pNew, const P_LIST_HEAD pHead)
{
    pNew->next        = pHead->next;
    pNew->prev        = pHead;
    pHead->next->prev = pNew;
    pHead->next       = pNew;
}

void tuya_list_del(const P_LIST_HEAD pEntry)
{
    pEntry->prev->next = pEntry->next;
    pEntry->next->prev = pEntry->prev;
    pEntry->next       = pEntry;
    pEntry->prev       = pEntry;
}

static int dummy_start(int type, netcfg_finish_cb_t cb, void *args)
{
    (void)type;
    (void)cb;
    (void)args;
    return OPRT_OK;
}

static int dummy_stop(int type)
{
    (void)type;
    return OPRT_OK;
}

void setUp(void)
{
    netcfg_uninit();
    netcfg_init();
}

void tearDown(void)
{
    netcfg_uninit();
}

void test_netcfg_register_rejects_null_callbacks(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netcfg_register(NETCFG_TUYA_WIFI_AP, NULL, dummy_stop));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netcfg_register(NETCFG_TUYA_WIFI_AP, dummy_start, NULL));
}

void test_netcfg_unregister_rejects_missing_handler(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netcfg_unregister(NETCFG_TUYA_WIFI_AP));
}

void test_netcfg_start_rejects_unregistered_type(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netcfg_start(NETCFG_TUYA_WIFI_AP, NULL, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_netcfg_register_rejects_null_callbacks);
    RUN_TEST(test_netcfg_unregister_rejects_missing_handler);
    RUN_TEST(test_netcfg_start_rejects_unregistered_type);
    return UNITY_END();
}
