#include <stdbool.h>
#include <stdlib.h>

#include "netcfg.h"
#include "tal_workq_service.h"
#include "tuya_error_code.h"
#include "unity.h"

int    netcfg_get_register_count(void);
int    netcfg_get_register_started_count(void);
BOOL_T is_netcfg_inited(void);

static int                g_start_count[8];
static int                g_stop_count[8];
static int                g_start_ret[8];
static int                g_stop_ret[8];
static int                g_last_start_type;
static int                g_last_stop_type;
static netcfg_finish_cb_t g_last_finish_cb;
static void              *g_last_args;

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
    cb(data);
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

static int finish_cb(int type, netcfg_info_t *info)
{
    (void)type;
    (void)info;
    return OPRT_OK;
}

static int handler_index_from_type(int type)
{
    switch (type) {
    case NETCFG_TUYA_WIFI_AP:
        return 0;
    case NETCFG_TUYA_BLE:
        return 1;
    case NETCFG_TUYA_API_USER:
        return 2;
    case NETCFG_TUYA_WIFI_PEGASUS:
        return 3;
    case NETCFG_AMAZON_WIFI_FFS:
        return 4;
    default:
        return 7;
    }
}

static int dummy_start(int type, netcfg_finish_cb_t cb, void *args)
{
    int index = handler_index_from_type(type);
    g_start_count[index]++;
    g_last_start_type = type;
    g_last_finish_cb  = cb;
    g_last_args       = args;
    return g_start_ret[index];
}

static int dummy_stop(int type)
{
    int index = handler_index_from_type(type);
    g_stop_count[index]++;
    g_last_stop_type = type;
    return g_stop_ret[index];
}

static void register_default_handlers(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_WIFI_AP, dummy_start, dummy_stop));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_BLE, dummy_start, dummy_stop));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_API_USER, dummy_start, dummy_stop));
}

void setUp(void)
{
    for (size_t i = 0; i < sizeof(g_start_count) / sizeof(g_start_count[0]); ++i) {
        g_start_count[i] = 0;
        g_stop_count[i]  = 0;
        g_start_ret[i]   = OPRT_OK;
        g_stop_ret[i]    = OPRT_OK;
    }

    g_last_start_type = -1;
    g_last_stop_type  = -1;
    g_last_finish_cb  = NULL;
    g_last_args       = NULL;

    netcfg_uninit();
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_init());
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

void test_netcfg_register_and_unregister_update_registered_count(void)
{
    TEST_ASSERT_EQUAL(0, netcfg_get_register_count());

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_WIFI_AP, dummy_start, dummy_stop));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_BLE, dummy_start, dummy_stop));
    TEST_ASSERT_EQUAL(2, netcfg_get_register_count());

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_unregister(NETCFG_TUYA_WIFI_AP));
    TEST_ASSERT_EQUAL(1, netcfg_get_register_count());
}

void test_netcfg_register_rejects_duplicate_type(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_WIFI_AP, dummy_start, dummy_stop));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netcfg_register(NETCFG_TUYA_WIFI_AP, dummy_start, dummy_stop));
}

void test_netcfg_start_marks_handler_started_and_records_finish_context(void)
{
    int args = 42;

    TEST_ASSERT_FALSE(is_netcfg_inited());
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_WIFI_AP, dummy_start, dummy_stop));
    TEST_ASSERT_EQUAL(0, netcfg_get_register_started_count());

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_WIFI_AP, finish_cb, &args));
    TEST_ASSERT_TRUE(is_netcfg_inited());
    TEST_ASSERT_EQUAL(1, netcfg_get_register_started_count());
    TEST_ASSERT_EQUAL(1, g_start_count[0]);
    TEST_ASSERT_EQUAL(NETCFG_TUYA_WIFI_AP, g_last_start_type);
    TEST_ASSERT_EQUAL_PTR(finish_cb, g_last_finish_cb);
    TEST_ASSERT_EQUAL_PTR(&args, g_last_args);
}

void test_netcfg_stop_clears_started_state_and_calls_stop_handler(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_WIFI_AP, dummy_start, dummy_stop));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_WIFI_AP, finish_cb, NULL));
    TEST_ASSERT_EQUAL(1, netcfg_get_register_started_count());

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_stop(NETCFG_TUYA_WIFI_AP));
    TEST_ASSERT_EQUAL(0, netcfg_get_register_started_count());
    TEST_ASSERT_EQUAL(1, g_stop_count[0]);
    TEST_ASSERT_EQUAL(NETCFG_TUYA_WIFI_AP, g_last_stop_type);
}

void test_netcfg_stop_rejects_unknown_type_and_propagates_stop_failure(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netcfg_stop(NETCFG_TUYA_WIFI_AP));

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_WIFI_AP, dummy_start, dummy_stop));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_WIFI_AP, finish_cb, NULL));
    g_stop_ret[0] = OPRT_COM_ERROR;

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, netcfg_stop(NETCFG_TUYA_WIFI_AP));
}

void test_netcfg_stop_all_requires_session_and_uninitializes_handlers(void)
{
    register_default_handlers();
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_WIFI_AP, finish_cb, NULL));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_BLE, finish_cb, NULL));

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_stop(0));
    TEST_ASSERT_EQUAL(0, netcfg_get_register_count());

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, netcfg_stop(0));
}

void test_netcfg_start_other_all_starts_pending_handlers_with_saved_callbacks(void)
{
    register_default_handlers();

    g_start_ret[1] = OPRT_COM_ERROR;
    g_start_ret[2] = OPRT_COM_ERROR;

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_WIFI_AP, finish_cb, (void *)0x1));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_BLE, finish_cb, (void *)0x2));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_API_USER, finish_cb, (void *)0x3));
    TEST_ASSERT_EQUAL(1, netcfg_get_register_started_count());

    g_start_ret[1] = OPRT_OK;
    g_start_ret[2] = OPRT_OK;

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start_other_all(NETCFG_TUYA_WIFI_AP));
    TEST_ASSERT_EQUAL(3, netcfg_get_register_started_count());
    TEST_ASSERT_EQUAL(2, g_start_count[1]);
    TEST_ASSERT_EQUAL(2, g_start_count[2]);
}

void test_netcfg_start_other_all_requires_session_and_tolerates_start_failures(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_WIFI_AP, dummy_start, dummy_stop));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_register(NETCFG_TUYA_BLE, dummy_start, dummy_stop));

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_WIFI_AP, finish_cb, (void *)0x1));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_BLE, finish_cb, (void *)0x2));
    TEST_ASSERT_EQUAL(2, netcfg_get_register_started_count());

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_stop(NETCFG_TUYA_BLE));
    g_start_ret[1] = OPRT_COM_ERROR;

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start_other_all(NETCFG_TUYA_WIFI_AP));
    TEST_ASSERT_EQUAL(1, netcfg_get_register_started_count());
    TEST_ASSERT_EQUAL(2, g_start_count[1]);

    netcfg_uninit();
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, netcfg_start_other_all(NETCFG_TUYA_WIFI_AP));
}

void test_netcfg_stop_other_all_stops_started_handlers_except_selected_type(void)
{
    register_default_handlers();

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_WIFI_AP, finish_cb, NULL));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_BLE, finish_cb, NULL));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_API_USER, finish_cb, NULL));
    TEST_ASSERT_EQUAL(3, netcfg_get_register_started_count());

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_stop_other_all(NETCFG_TUYA_WIFI_AP));
    TEST_ASSERT_EQUAL(1, netcfg_get_register_started_count());
    TEST_ASSERT_EQUAL(0, g_stop_count[0]);
    TEST_ASSERT_EQUAL(1, g_stop_count[1]);
    TEST_ASSERT_EQUAL(1, g_stop_count[2]);
}

void test_netcfg_stop_other_all_requires_session_and_tolerates_stop_failures(void)
{
    register_default_handlers();

    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_WIFI_AP, finish_cb, NULL));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_BLE, finish_cb, NULL));
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_start(NETCFG_TUYA_API_USER, finish_cb, NULL));

    g_stop_ret[1] = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_OK, netcfg_stop_other_all(NETCFG_TUYA_WIFI_AP));
    TEST_ASSERT_EQUAL(1, netcfg_get_register_started_count());

    netcfg_uninit();
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, netcfg_stop_other_all(NETCFG_TUYA_WIFI_AP));
}

void test_netcfg_start_rejects_unregistered_type(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netcfg_start(NETCFG_TUYA_WIFI_AP, NULL, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_netcfg_register_rejects_null_callbacks);
    RUN_TEST(test_netcfg_register_and_unregister_update_registered_count);
    RUN_TEST(test_netcfg_register_rejects_duplicate_type);
    RUN_TEST(test_netcfg_start_marks_handler_started_and_records_finish_context);
    RUN_TEST(test_netcfg_stop_clears_started_state_and_calls_stop_handler);
    RUN_TEST(test_netcfg_stop_rejects_unknown_type_and_propagates_stop_failure);
    RUN_TEST(test_netcfg_stop_all_requires_session_and_uninitializes_handlers);
    RUN_TEST(test_netcfg_start_other_all_starts_pending_handlers_with_saved_callbacks);
    RUN_TEST(test_netcfg_start_other_all_requires_session_and_tolerates_start_failures);
    RUN_TEST(test_netcfg_stop_other_all_stops_started_handlers_except_selected_type);
    RUN_TEST(test_netcfg_stop_other_all_requires_session_and_tolerates_stop_failures);
    RUN_TEST(test_netcfg_start_rejects_unregistered_type);
    return UNITY_END();
}
