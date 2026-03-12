#include <stddef.h>
#include <string.h>

#include "netmgr.h"
#include "tal_event.h"
#include "tal_mutex.h"
#include "tal_network_register.h"
#include "tal_sw_timer.h"
#include "tuya_error_code.h"
#include "tuya_iot.h"
#include "unity.h"

OPERATE_RET __netmgr_conn_register(netmgr_type_e type, netmgr_conn_base_t *conn);
void        __tuya_lan_init_tm_cb(TIMER_ID timer_id, void *arg);
void        netmgr_cmd(int argc, char *argv[]);

static int                g_network_card_init_count;
static int                g_mutex_create_count;
static int                g_network_card_set_active_count;
static int                g_event_publish_count;
static int                g_last_event_name_matches;
static int                g_timer_create_count;
static int                g_timer_start_count;
static int                g_timer_stop_count;
static int                g_open_count[3];
static int                g_set_count[3];
static int                g_get_count[3];
static int                g_last_set_cmd[3];
static int                g_last_get_cmd[3];
static void              *g_last_set_param[3];
static void              *g_last_get_param[3];
static netmgr_status_e    g_fake_status[3];
static OPERATE_RET        g_fake_set_ret[3];
static OPERATE_RET        g_fake_get_ret[3];
static int                g_last_lan_init_called;
static TIMER_ID           g_fake_timer = (TIMER_ID)0x1001;
static tuya_iot_client_t *g_client_ptr;
static tuya_iot_client_t  g_test_client;

static netmgr_conn_base_t g_wifi_conn;
static netmgr_conn_base_t g_wired_conn;
static netmgr_conn_base_t g_cellular_conn;

static int conn_index(netmgr_type_e type)
{
    switch (type) {
    case NETCONN_WIFI:
        return 0;
    case NETCONN_WIRED:
        return 1;
    case NETCONN_CELLULAR:
        return 2;
    default:
        return 2;
    }
}

static void reset_fake_connection(netmgr_conn_base_t *conn, netmgr_type_e type, uint8_t pri,
                                  TAL_NETWORK_CARD_TYPE_E card_type)
{
    conn->pri       = pri;
    conn->type      = type;
    conn->status    = NETMGR_LINK_DOWN;
    conn->card_type = card_type;
    conn->next      = NULL;
}

OPERATE_RET tal_network_card_init(void)
{
    g_network_card_init_count++;
    return OPRT_OK;
}

OPERATE_RET tal_mutex_create_init(MUTEX_HANDLE *handle)
{
    g_mutex_create_count++;
    if (handle != NULL) {
        *handle = (MUTEX_HANDLE)0x2002;
    }
    return OPRT_OK;
}

OPERATE_RET tal_sw_timer_create(TAL_TIMER_CB func, void *arg, TIMER_ID *timer_id)
{
    (void)func;
    (void)arg;
    g_timer_create_count++;
    if (timer_id != NULL) {
        *timer_id = g_fake_timer;
    }
    return OPRT_OK;
}

OPERATE_RET tal_sw_timer_start(TIMER_ID timer_id, TIME_MS time_ms, TIMER_TYPE timer_type)
{
    (void)timer_id;
    (void)time_ms;
    (void)timer_type;
    g_timer_start_count++;
    return OPRT_OK;
}

OPERATE_RET tal_sw_timer_stop(TIMER_ID timer_id)
{
    (void)timer_id;
    g_timer_stop_count++;
    return OPRT_OK;
}

OPERATE_RET tal_event_publish(const char *name, void *data)
{
    (void)data;
    g_event_publish_count++;
    g_last_event_name_matches = (strcmp(name, EVENT_LINK_TYPE_CHG) == 0) || (strcmp(name, EVENT_LINK_STATUS_CHG) == 0);
    return OPRT_OK;
}

OPERATE_RET tal_network_card_set_active(TAL_NETWORK_CARD_TYPE_E type)
{
    (void)type;
    g_network_card_set_active_count++;
    return OPRT_OK;
}

tuya_iot_client_t *tuya_iot_client_get(void)
{
    return g_client_ptr;
}

int tuya_lan_init(tuya_iot_client_t *client)
{
    (void)client;
    g_last_lan_init_called++;
    return OPRT_OK;
}

static OPERATE_RET fake_conn_open_wifi(void *config)
{
    (void)config;
    g_open_count[0]++;
    return OPRT_OK;
}

static OPERATE_RET fake_conn_open_wired(void *config)
{
    (void)config;
    g_open_count[1]++;
    return OPRT_OK;
}

static OPERATE_RET fake_conn_open_cellular(void *config)
{
    (void)config;
    g_open_count[2]++;
    return OPRT_OK;
}

static OPERATE_RET fake_wifi_set(netmgr_conn_config_type_e cmd, void *param)
{
    (void)param;
    g_set_count[0]++;
    g_last_set_cmd[0] = cmd;
    return g_fake_set_ret[0];
}

static OPERATE_RET fake_wired_set(netmgr_conn_config_type_e cmd, void *param)
{
    (void)param;
    g_set_count[1]++;
    g_last_set_cmd[1] = cmd;
    return g_fake_set_ret[1];
}

static OPERATE_RET fake_wifi_get(netmgr_conn_config_type_e cmd, void *param)
{
    g_get_count[0]++;
    g_last_get_cmd[0]   = cmd;
    g_last_get_param[0] = param;
    if (cmd == NETCONN_CMD_STATUS && param != NULL) {
        *((netmgr_status_e *)param) = g_fake_status[0];
    }
    return g_fake_get_ret[0];
}

static OPERATE_RET fake_wired_get(netmgr_conn_config_type_e cmd, void *param)
{
    g_get_count[1]++;
    g_last_get_cmd[1]   = cmd;
    g_last_get_param[1] = param;
    if (cmd == NETCONN_CMD_STATUS && param != NULL) {
        *((netmgr_status_e *)param) = g_fake_status[1];
    }
    return g_fake_get_ret[1];
}

void setUp(void)
{
    g_network_card_init_count       = 0;
    g_mutex_create_count            = 0;
    g_network_card_set_active_count = 0;
    g_event_publish_count           = 0;
    g_last_event_name_matches       = 0;
    g_timer_create_count            = 0;
    g_timer_start_count             = 0;
    g_timer_stop_count              = 0;
    g_last_lan_init_called          = 0;
    g_client_ptr                    = NULL;
    memset(&g_test_client, 0, sizeof(g_test_client));

    memset(g_open_count, 0, sizeof(g_open_count));
    memset(g_set_count, 0, sizeof(g_set_count));
    memset(g_get_count, 0, sizeof(g_get_count));
    memset(g_last_set_cmd, 0, sizeof(g_last_set_cmd));
    memset(g_last_get_cmd, 0, sizeof(g_last_get_cmd));
    memset(g_last_set_param, 0, sizeof(g_last_set_param));
    memset(g_last_get_param, 0, sizeof(g_last_get_param));

    g_fake_status[0]  = NETMGR_LINK_DOWN;
    g_fake_status[1]  = NETMGR_LINK_DOWN;
    g_fake_status[2]  = NETMGR_LINK_DOWN;
    g_fake_set_ret[0] = OPRT_OK;
    g_fake_set_ret[1] = OPRT_OK;
    g_fake_set_ret[2] = OPRT_OK;
    g_fake_get_ret[0] = OPRT_OK;
    g_fake_get_ret[1] = OPRT_OK;
    g_fake_get_ret[2] = OPRT_OK;
}

void tearDown(void) {}

void test_netmgr_conn_set_reports_not_ready_before_init(void)
{
    TEST_ASSERT_EQUAL(OPRT_RESOURCE_NOT_READY, netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_STATUS, NULL));
}

void test_netmgr_conn_get_reports_not_ready_before_init(void)
{
    TEST_ASSERT_EQUAL(OPRT_RESOURCE_NOT_READY, netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, NULL));
}

void test_netmgr_cmd_returns_immediately_when_not_ready(void)
{
    char *argv[] = {"netmgr"};

    netmgr_cmd(1, argv);
    TEST_ASSERT_EQUAL(0, g_network_card_set_active_count);
}

void test_netmgr_init_returns_invalid_param_when_no_connections_are_registered(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netmgr_init(NETCONN_WIFI));
    TEST_ASSERT_EQUAL(1, g_network_card_init_count);
    TEST_ASSERT_EQUAL(1, g_mutex_create_count);
}

void test_netmgr_conn_register_rejects_null_connection(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, __netmgr_conn_register(NETCONN_WIFI, NULL));
}

void test_netmgr_conn_register_orders_connections_and_rejects_duplicates(void)
{
    reset_fake_connection(&g_wifi_conn, NETCONN_WIFI, 1, TAL_NET_TYPE_POSIX);
    reset_fake_connection(&g_wired_conn, NETCONN_WIRED, 2, TAL_NET_TYPE_PLATFORM);
    g_wifi_conn.open  = fake_conn_open_wifi;
    g_wifi_conn.set   = fake_wifi_set;
    g_wifi_conn.get   = fake_wifi_get;
    g_wired_conn.open = fake_conn_open_wired;
    g_wired_conn.set  = fake_wired_set;
    g_wired_conn.get  = fake_wired_get;
    g_fake_status[0]  = NETMGR_LINK_DOWN;
    g_fake_status[1]  = NETMGR_LINK_UP;
    g_fake_set_ret[0] = OPRT_OK;
    g_fake_set_ret[1] = OPRT_OK;
    g_fake_get_ret[0] = OPRT_OK;
    g_fake_get_ret[1] = OPRT_OK;

    TEST_ASSERT_EQUAL(OPRT_OK, __netmgr_conn_register(NETCONN_WIFI, &g_wifi_conn));
    TEST_ASSERT_EQUAL(OPRT_OK, __netmgr_conn_register(NETCONN_WIRED, &g_wired_conn));
    TEST_ASSERT_EQUAL(1, g_open_count[0]);
    TEST_ASSERT_EQUAL(1, g_open_count[1]);
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, __netmgr_conn_register(NETCONN_WIFI, &g_wifi_conn));
}

void test_netmgr_init_and_auto_routing_use_active_connection(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    g_fake_status[0]       = NETMGR_LINK_DOWN;
    g_fake_status[1]       = NETMGR_LINK_UP;

    TEST_ASSERT_EQUAL(OPRT_OK, netmgr_init(NETCONN_WIFI | NETCONN_WIRED));
    TEST_ASSERT_EQUAL(1, g_network_card_init_count);
    TEST_ASSERT_EQUAL(1, g_mutex_create_count);
    TEST_ASSERT_EQUAL(1, g_timer_create_count);
    TEST_ASSERT_EQUAL(1, g_timer_start_count);

    TEST_ASSERT_EQUAL(OPRT_OK, netmgr_conn_set(NETCONN_AUTO, NETCONN_CMD_STATUS, NULL));
    TEST_ASSERT_EQUAL(0, g_set_count[0]);
    TEST_ASSERT_EQUAL(1, g_set_count[1]);
    TEST_ASSERT_EQUAL(NETCONN_CMD_STATUS, g_last_set_cmd[1]);

    TEST_ASSERT_EQUAL(OPRT_OK, netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status));
    TEST_ASSERT_EQUAL(NETMGR_LINK_UP, status);
    TEST_ASSERT_TRUE(g_get_count[1] >= 2);
}

void test_netmgr_event_callback_switches_active_connection(void)
{
    g_fake_status[1] = NETMGR_LINK_DOWN;
    g_fake_status[0] = NETMGR_LINK_UP;

    TEST_ASSERT_NOT_NULL(g_wifi_conn.event_cb);
    g_wifi_conn.event_cb(NETCONN_WIFI, NETMGR_LINK_UP);

    TEST_ASSERT_EQUAL(OPRT_OK, netmgr_conn_set(NETCONN_AUTO, NETCONN_CMD_STATUS, NULL));
    TEST_ASSERT_EQUAL(1, g_set_count[0]);
    TEST_ASSERT_TRUE(g_network_card_set_active_count >= 1);
    TEST_ASSERT_TRUE(g_event_publish_count >= 1);
    TEST_ASSERT_TRUE(g_last_event_name_matches);
}

void test_netmgr_conn_get_propagates_connection_error(void)
{
    g_fake_get_ret[0] = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, NULL));
}

void test_netmgr_lan_timer_callback_skips_missing_client_then_starts_for_activated_client(void)
{
    __tuya_lan_init_tm_cb(g_fake_timer, NULL);
    TEST_ASSERT_EQUAL(0, g_last_lan_init_called);

    g_client_ptr               = &g_test_client;
    g_test_client.is_activated = true;
    __tuya_lan_init_tm_cb(g_fake_timer, NULL);

    TEST_ASSERT_EQUAL(1, g_last_lan_init_called);
    TEST_ASSERT_EQUAL(1, g_timer_stop_count);
}

void test_netmgr_conn_set_and_get_reject_missing_callbacks(void)
{
    reset_fake_connection(&g_cellular_conn, NETCONN_CELLULAR, 0, TAL_NET_TYPE_AT_MODEM);
    g_cellular_conn.open = fake_conn_open_cellular;
    g_cellular_conn.set  = NULL;
    g_cellular_conn.get  = NULL;

    TEST_ASSERT_EQUAL(OPRT_OK, __netmgr_conn_register(NETCONN_CELLULAR, &g_cellular_conn));
    TEST_ASSERT_EQUAL(1, g_open_count[2]);
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netmgr_conn_set(NETCONN_CELLULAR, NETCONN_CMD_STATUS, NULL));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, netmgr_conn_get(NETCONN_CELLULAR, NETCONN_CMD_STATUS, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_netmgr_conn_set_reports_not_ready_before_init);
    RUN_TEST(test_netmgr_conn_get_reports_not_ready_before_init);
    RUN_TEST(test_netmgr_cmd_returns_immediately_when_not_ready);
    RUN_TEST(test_netmgr_init_returns_invalid_param_when_no_connections_are_registered);
    RUN_TEST(test_netmgr_conn_register_rejects_null_connection);
    RUN_TEST(test_netmgr_conn_register_orders_connections_and_rejects_duplicates);
    RUN_TEST(test_netmgr_init_and_auto_routing_use_active_connection);
    RUN_TEST(test_netmgr_event_callback_switches_active_connection);
    RUN_TEST(test_netmgr_conn_get_propagates_connection_error);
    RUN_TEST(test_netmgr_lan_timer_callback_skips_missing_client_then_starts_for_activated_client);
    RUN_TEST(test_netmgr_conn_set_and_get_reject_missing_callbacks);
    return UNITY_END();
}
