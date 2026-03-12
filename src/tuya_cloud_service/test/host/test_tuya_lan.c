#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lan_sock.h"
#include "netmgr.h"
#include "tal_hash.h"
#include "tal_memory.h"
#include "tal_mutex.h"
#include "tal_network.h"
#include "tal_time_service.h"
#include "tuya_error_code.h"
#include "tuya_iot_dp.h"
#include "tuya_protocol.h"
#include "tuya_lan.h"
#include "uni_random.h"
#include "unity.h"

static tuya_iot_client_t *g_client;
static tuya_iot_client_t  g_test_client;
static int                g_tal_free_count;
static int                g_sock_loop_init_count;
static int                g_sock_loop_disable_count;
static int                g_mutex_create_count;
static int                g_mutex_release_count;
static int                g_net_close_count;
static int                g_reg_sock_count;
static int                g_unreg_sock_count;
static int                g_net_send_count;
static int                g_net_send_to_count;
static TUYA_ERRNO         g_net_send_ret;
static TUYA_ERRNO         g_net_send_to_ret;
static TUYA_ERRNO         g_net_errno;
static int                g_socket_create_tcp_ret;
static int                g_socket_create_udp_ret;
static OPERATE_RET        g_set_reuse_ret;
static TUYA_ERRNO         g_bind_ret;
static TUYA_ERRNO         g_listen_ret;
static OPERATE_RET        g_reg_sock_ret;
static TUYA_ERRNO         g_recv_ret;
static int                g_recv_nd_size_ret;
static int                g_sock_loop_is_inited_cycles;
static OPERATE_RET        g_sock_loop_init_ret;
static int                g_mutex_create_fail_on_call;
static int                g_recv_raw_len;
static uint8_t            g_recv_raw_buf[1024];
static int                g_recv_nd_raw_len;
static uint8_t            g_recv_nd_raw_buf[1024];
static int                g_dp_parse_count;
static int                g_malloc_fail_on_call;
static int                g_malloc_call_count;
static OPERATE_RET        g_parse_protocol_ret;
static OPERATE_RET        g_pack_protocol_ret;
static OPERATE_RET        g_frame_serialize_ret;
static OPERATE_RET        g_frame_parse_ret;
static int                g_encrypt_ret;
static bool               g_missing_ip_item;
static bool               g_missing_from_item;
static bool               g_missing_dps_item;
static char               g_parse_protocol_json[128];
static char               g_dp_dump_json[128];
static cJSON             *g_detach_item;
static bool               g_cjson_root_valid;
static sloop_sock_t       g_tcp_server_sock;
static sloop_sock_t       g_udp_server_sock;
static sloop_sock_t       g_client_sock;
static int                g_accept_fd;
static TUYA_IP_ADDR_T     g_accept_addr;
static uint32_t           g_next_tcp_seq;
static uint32_t           g_next_tcp_type;
static uint32_t           g_next_tcp_payload_len;
static uint8_t            g_next_tcp_payload[64];
static int                g_udp_mode;
static cJSON              g_json_root;
static cJSON              g_json_data;
static cJSON              g_json_dps;
static cJSON              g_json_ip;
static cJSON              g_json_from;

static uint32_t test_be32(uint32_t v)
{
    return __builtin_bswap32(v);
}

static void fill_frame_buffer(uint8_t *buf, uint32_t sequence, uint32_t type, uint32_t payload_len)
{
    const uint32_t     encoded_len = LPV35_FRAME_NONCE_SIZE + payload_len + LPV35_FRAME_TAG_SIZE;
    lpv35_fixed_head_t head        = {0};

    memcpy(buf, LPV35_FRAME_HEAD, LPV35_FRAME_HEAD_SIZE);
    head.sequence = test_be32(sequence);
    head.type     = test_be32(type);
    head.length   = test_be32(encoded_len);
    memcpy(buf + LPV35_FRAME_HEAD_SIZE, &head, sizeof(head));
    memset(buf + LPV35_FRAME_HEAD_SIZE + sizeof(head), 0, encoded_len);
    memcpy(buf + LPV35_FRAME_HEAD_SIZE + sizeof(head) + encoded_len, LPV35_FRAME_TAIL, LPV35_FRAME_TAIL_SIZE);
}

static int dummy_lan_handler_a(const uint8_t *data, uint8_t **out)
{
    (void)data;
    (void)out;
    return OPRT_OK;
}

static int dummy_lan_handler_b(const uint8_t *data, uint8_t **out)
{
    (void)data;
    (void)out;
    return OPRT_OK;
}

static int dummy_lan_handler_c(const uint8_t *data, uint8_t **out)
{
    (void)data;
    (void)out;
    return OPRT_OK;
}

static int dummy_lan_handler_d(const uint8_t *data, uint8_t **out)
{
    (void)data;
    (void)out;
    return OPRT_OK;
}

static int dummy_lan_handler_e(const uint8_t *data, uint8_t **out)
{
    (void)data;
    (void)out;
    return OPRT_OK;
}

static int dummy_lan_handler_f(const uint8_t *data, uint8_t **out)
{
    (void)data;
    (void)out;
    return OPRT_OK;
}

void *tal_malloc(size_t size)
{
    g_malloc_call_count++;
    if (g_malloc_fail_on_call != 0 && g_malloc_call_count == g_malloc_fail_on_call) {
        return NULL;
    }
    return calloc(1, size);
}

void tal_free(void *ptr)
{
    g_tal_free_count++;
    free(ptr);
}

tuya_iot_client_t *tuya_iot_client_get(void)
{
    return g_client;
}

OPERATE_RET tuya_sock_loop_init(void)
{
    g_sock_loop_init_count++;
    return g_sock_loop_init_ret;
}

void tuya_sock_loop_disable(void)
{
    g_sock_loop_disable_count++;
}

BOOL_T tuya_sock_loop_is_inited(void)
{
    if (g_sock_loop_is_inited_cycles > 0) {
        g_sock_loop_is_inited_cycles--;
        return TRUE;
    }
    return FALSE;
}

OPERATE_RET tal_mutex_create_init(MUTEX_HANDLE *handle)
{
    g_mutex_create_count++;
    if (g_mutex_create_fail_on_call != 0 && g_mutex_create_count == g_mutex_create_fail_on_call) {
        return OPRT_COM_ERROR;
    }
    if (handle != NULL) {
        *handle = (MUTEX_HANDLE)0x1;
    }
    return OPRT_OK;
}

TUYA_ERRNO tal_net_close(const int fd)
{
    (void)fd;
    g_net_close_count++;
    return OPRT_OK;
}

OPERATE_RET tuya_unreg_lan_sock(int fd)
{
    (void)fd;
    g_unreg_sock_count++;
    return OPRT_OK;
}

void tal_system_sleep(uint32_t time_ms)
{
    (void)time_ms;
}

OPERATE_RET tal_mutex_lock(const MUTEX_HANDLE mutex_handle)
{
    (void)mutex_handle;
    return OPRT_OK;
}

OPERATE_RET tal_mutex_unlock(const MUTEX_HANDLE mutex_handle)
{
    (void)mutex_handle;
    return OPRT_OK;
}

OPERATE_RET tal_mutex_release(const MUTEX_HANDLE mutex_handle)
{
    (void)mutex_handle;
    g_mutex_release_count++;
    return OPRT_OK;
}

OPERATE_RET tal_event_publish(const char *name, void *data)
{
    (void)name;
    (void)data;
    return OPRT_OK;
}

OPERATE_RET tal_md5_ret(const uint8_t *input, size_t ilen, uint8_t output[16])
{
    (void)input;
    (void)ilen;
    memset(output, 0x5A, 16);
    return OPRT_OK;
}

int uni_random_range(unsigned int range)
{
    (void)range;
    return 7;
}

int uni_random_string(char *dst, int size)
{
    memset(dst, 'r', (size_t)size);
    return size;
}

TIME_T tal_time_get_posix(void)
{
    return 100;
}

int tal_net_socket_create(const TUYA_PROTOCOL_TYPE_E type)
{
    return (type == PROTOCOL_TCP) ? g_socket_create_tcp_ret : g_socket_create_udp_ret;
}

TUYA_ERRNO tal_net_get_errno(void)
{
    return g_net_errno;
}

OPERATE_RET tal_net_set_reuse(const int fd)
{
    (void)fd;
    return g_set_reuse_ret;
}

TUYA_ERRNO tal_net_bind(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    (void)fd;
    (void)addr;
    (void)port;
    return g_bind_ret;
}

TUYA_ERRNO tal_net_listen(const int fd, const int backlog)
{
    (void)fd;
    (void)backlog;
    return g_listen_ret;
}

TUYA_ERRNO tal_net_send(const int fd, const void *buf, const uint32_t nbytes)
{
    (void)fd;
    (void)buf;
    g_net_send_count++;
    return (g_net_send_ret == INT32_MIN) ? (TUYA_ERRNO)nbytes : g_net_send_ret;
}

OPERATE_RET netmgr_conn_get(netmgr_type_e type, netmgr_conn_config_type_e cmd, void *param)
{
    (void)type;
    (void)cmd;
    if (param != NULL) {
        memset(param, 0, sizeof(NW_IP_S));
        strcpy(((NW_IP_S *)param)->ip, "192.168.0.2");
    }
    return OPRT_OK;
}

int lpv35_frame_buffer_size_get(lpv35_frame_object_t *frame_obj)
{
    return frame_obj == NULL ? 0 : (int)(LPV35_FRAME_MINI_SIZE + frame_obj->data_len + sizeof(uint32_t));
}

OPERATE_RET lpv35_frame_serialize(const uint8_t *key, int key_len, const lpv35_frame_object_t *input, uint8_t *output,
                                  int *olen)
{
    (void)key;
    (void)key_len;
    (void)input;
    if (olen != NULL) {
        *olen = 16;
    }
    if (output != NULL) {
        memset(output, 0, 16);
    }
    return g_frame_serialize_ret;
}

OPERATE_RET lpv35_frame_parse(const uint8_t *key, int key_len, const uint8_t *input, int ilen,
                              lpv35_frame_object_t *output)
{
    (void)key;
    (void)key_len;
    (void)input;
    (void)ilen;
    if (output != NULL) {
        output->type     = g_next_tcp_type;
        output->data_len = g_next_tcp_payload_len;
        output->data     = tal_malloc(g_next_tcp_payload_len ? g_next_tcp_payload_len : 1);
        if (g_next_tcp_payload_len != 0) {
            memcpy(output->data, g_next_tcp_payload, g_next_tcp_payload_len);
        } else {
            output->data[0] = 0;
        }
    }
    return g_frame_parse_ret;
}

OPERATE_RET tuya_parse_protocol_data(const DP_CMD_TYPE_E cmd, uint8_t *data, const int len, const char *key,
                                     char **out_data)
{
    (void)cmd;
    (void)data;
    (void)len;
    (void)key;
    if (out_data != NULL) {
        *out_data = NULL;
        if (g_parse_protocol_ret == OPRT_OK) {
            size_t json_len = strlen(g_parse_protocol_json) + 1;
            *out_data       = tal_malloc(json_len);
            memcpy(*out_data, g_parse_protocol_json, json_len);
        }
    }
    return g_parse_protocol_ret;
}

int tuya_iot_dp_parse(tuya_iot_client_t *client, dp_cmd_type_t tp, cJSON *cmd_js)
{
    (void)client;
    (void)tp;
    (void)cmd_js;
    g_dp_parse_count++;
    return OPRT_OK;
}

char *tuya_iot_dp_obj_dump(tuya_iot_client_t *client, char *devid, int flags)
{
    (void)client;
    (void)devid;
    (void)flags;
    if (g_dp_dump_json[0] == '\0') {
        return NULL;
    }
    char *out = tal_malloc(strlen(g_dp_dump_json) + 1);
    strcpy(out, g_dp_dump_json);
    return out;
}

OPERATE_RET tuya_pack_protocol_data(const DP_CMD_TYPE_E cmd, const char *src, const uint32_t pro, uint8_t *key,
                                    char **out, uint32_t *out_len)
{
    (void)cmd;
    (void)src;
    (void)pro;
    (void)key;
    if (out != NULL) {
        *out = NULL;
        if (g_pack_protocol_ret == OPRT_OK) {
            *out = tal_malloc(3);
            memcpy(*out, "{}", 3);
        }
    }
    if (out_len != NULL) {
        *out_len = 2;
    }
    return g_pack_protocol_ret;
}

OPERATE_RET tal_sha256_mac(const uint8_t *key, size_t keylen, const uint8_t *input, size_t ilen, uint8_t *output)
{
    (void)key;
    (void)keylen;
    (void)input;
    (void)ilen;
    memset(output, 0, 32);
    return OPRT_OK;
}

int mbedtls_cipher_auth_encrypt_wrapper(const cipher_params_t *input, unsigned char *output, size_t *olen,
                                        unsigned char *tag, size_t tag_len)
{
    (void)input;
    (void)output;
    (void)tag;
    (void)tag_len;
    if (olen != NULL) {
        *olen = 0;
    }
    return g_encrypt_ret;
}

cJSON *cJSON_Parse(const char *value)
{
    (void)value;
    return g_cjson_root_valid ? &g_json_root : NULL;
}

cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string)
{
    (void)object;
    (void)string;
    return g_detach_item;
}

cJSON *cJSON_GetObjectItem(const cJSON *const object, const char *const string)
{
    if (object == &g_json_root) {
        if (strcmp(string, "ip") == 0) {
            if (g_missing_ip_item) {
                return NULL;
            }
            return &g_json_ip;
        }
        if (strcmp(string, "from") == 0) {
            if (g_missing_from_item) {
                return NULL;
            }
            return &g_json_from;
        }
    }
    if (object == &g_json_data && strcmp(string, "dps") == 0) {
        if (g_missing_dps_item) {
            return NULL;
        }
        return &g_json_dps;
    }
    if (strcmp(string, "ip") == 0) {
        return &g_json_ip;
    }
    if (strcmp(string, "from") == 0) {
        return &g_json_from;
    }
    return NULL;
}

void cJSON_Delete(cJSON *item)
{
    (void)item;
}

TUYA_ERRNO tal_net_recv(const int fd, void *buf, const uint32_t nbytes)
{
    (void)fd;
    (void)nbytes;
    if (g_recv_raw_len > 0) {
        memcpy(buf, g_recv_raw_buf, (size_t)g_recv_raw_len);
        int ret        = g_recv_raw_len;
        g_recv_raw_len = 0;
        return ret;
    }
    if (g_recv_ret != INT32_MIN) {
        if (g_recv_ret > 0) {
            memset(buf, 0, (size_t)g_recv_ret);
        }
        return g_recv_ret;
    }
    fill_frame_buffer(buf, g_next_tcp_seq, g_next_tcp_type, g_next_tcp_payload_len);
    return (TUYA_ERRNO)(LPV35_FRAME_MINI_SIZE + g_next_tcp_payload_len);
}

int tal_net_recv_nd_size(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size)
{
    (void)fd;
    (void)buf_size;
    (void)nd_size;
    if (g_recv_nd_raw_len > 0) {
        memcpy(buf, g_recv_nd_raw_buf, (size_t)g_recv_nd_raw_len);
        int ret           = g_recv_nd_raw_len;
        g_recv_nd_raw_len = 0;
        return ret;
    }
    return g_recv_nd_size_ret;
}

int tal_net_accept(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    (void)fd;
    if (addr != NULL) {
        *addr = g_accept_addr;
    }
    if (port != NULL) {
        *port = 0;
    }
    return g_accept_fd;
}

OPERATE_RET tal_net_set_block(const int fd, const BOOL_T block)
{
    (void)fd;
    (void)block;
    return OPRT_OK;
}

OPERATE_RET tuya_reg_lan_sock(sloop_sock_t sock_info)
{
    g_reg_sock_count++;
    if (sock_info.pre_select != NULL) {
        g_tcp_server_sock = sock_info;
    } else if (sock_info.read != NULL && sock_info.err != NULL && sock_info.quit == NULL) {
        if (sock_info.sock == 12) {
            g_udp_server_sock = sock_info;
        } else {
            g_client_sock = sock_info;
        }
    }
    return g_reg_sock_ret;
}

TUYA_ERRNO tal_net_recvfrom(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    (void)fd;
    (void)nbytes;
    if (addr != NULL) {
        *addr = 0;
    }
    if (port != NULL) {
        *port = 0;
    }
    if (g_udp_mode == 0) {
        memset(buf, 0, 8);
        return 8;
    }
    if (g_udp_mode == -1) {
        return -1;
    }

    g_next_tcp_type        = FRM_TYPE_APP_UDP_BOARDCAST;
    g_next_tcp_payload_len = 2;
    g_next_tcp_payload[0]  = '{';
    g_next_tcp_payload[1]  = '}';
    fill_frame_buffer(buf, 1, FRM_TYPE_APP_UDP_BOARDCAST, g_next_tcp_payload_len);
    return (TUYA_ERRNO)(LPV35_FRAME_MINI_SIZE + g_next_tcp_payload_len);
}

TUYA_IP_ADDR_T tal_net_str2addr(const char *ip_str)
{
    (void)ip_str;
    return 0;
}

TUYA_ERRNO tal_net_send_to(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                           const uint16_t port)
{
    (void)fd;
    (void)buf;
    (void)addr;
    (void)port;
    g_net_send_to_count++;
    return (g_net_send_to_ret == INT32_MIN) ? (TUYA_ERRNO)nbytes : g_net_send_to_ret;
}

#include "../../lan/tuya_lan.c"

void setUp(void)
{
    tuya_lan_exit();
    g_client = NULL;
    memset(&g_test_client, 0, sizeof(g_test_client));
    g_tal_free_count             = 0;
    g_sock_loop_init_count       = 0;
    g_sock_loop_disable_count    = 0;
    g_mutex_create_count         = 0;
    g_mutex_release_count        = 0;
    g_net_close_count            = 0;
    g_reg_sock_count             = 0;
    g_unreg_sock_count           = 0;
    g_net_send_count             = 0;
    g_net_send_to_count          = 0;
    g_net_send_ret               = INT32_MIN;
    g_net_send_to_ret            = INT32_MIN;
    g_net_errno                  = OPRT_OK;
    g_socket_create_tcp_ret      = 11;
    g_socket_create_udp_ret      = 12;
    g_set_reuse_ret              = OPRT_OK;
    g_bind_ret                   = OPRT_OK;
    g_listen_ret                 = OPRT_OK;
    g_reg_sock_ret               = OPRT_OK;
    g_recv_ret                   = INT32_MIN;
    g_recv_nd_size_ret           = OPRT_OK;
    g_sock_loop_is_inited_cycles = 0;
    g_sock_loop_init_ret         = OPRT_OK;
    g_mutex_create_fail_on_call  = 0;
    g_recv_raw_len               = 0;
    memset(g_recv_raw_buf, 0, sizeof(g_recv_raw_buf));
    g_recv_nd_raw_len = 0;
    memset(g_recv_nd_raw_buf, 0, sizeof(g_recv_nd_raw_buf));
    g_dp_parse_count      = 0;
    g_encrypt_ret         = OPRT_OK;
    g_missing_ip_item     = false;
    g_missing_from_item   = false;
    g_missing_dps_item    = false;
    g_malloc_fail_on_call = 0;
    g_malloc_call_count   = 0;
    g_parse_protocol_ret  = OPRT_COM_ERROR;
    g_pack_protocol_ret   = OPRT_OK;
    g_frame_serialize_ret = OPRT_OK;
    g_frame_parse_ret     = OPRT_OK;
    strcpy(g_parse_protocol_json, "{\"data\":{\"dps\":{\"1\":true}}}");
    g_dp_dump_json[0]  = '\0';
    g_detach_item      = NULL;
    g_cjson_root_valid = true;
    memset(&g_tcp_server_sock, 0, sizeof(g_tcp_server_sock));
    memset(&g_udp_server_sock, 0, sizeof(g_udp_server_sock));
    memset(&g_client_sock, 0, sizeof(g_client_sock));
    g_accept_fd            = 21;
    g_accept_addr          = 0x01020304;
    g_next_tcp_seq         = 1;
    g_next_tcp_type        = FRM_SECURITY_TYPE3;
    g_next_tcp_payload_len = 0;
    memset(g_next_tcp_payload, 0, sizeof(g_next_tcp_payload));
    g_udp_mode = 0;
    memset(&g_json_root, 0, sizeof(g_json_root));
    memset(&g_json_ip, 0, sizeof(g_json_ip));
    memset(&g_json_from, 0, sizeof(g_json_from));
    g_json_data.valuestring = NULL;
    g_json_dps.valuestring  = NULL;
    g_json_ip.valuestring   = "192.168.0.10";
    g_json_from.valuestring = "app";
    strcpy(g_test_client.activate.localkey, "0123456789abcdef");
    strcpy(g_test_client.activate.devid, "deviceid1234567890");
    g_test_client.config.uuid       = "12345678901234567890";
    g_test_client.config.productkey = "productkey123456";

    tuya_lan_unregister_cb(0x1001);
    tuya_lan_unregister_cb(0x1002);
    tuya_lan_unregister_cb(0x1003);
    tuya_lan_unregister_cb(0x1004);
    tuya_lan_unregister_cb(0x1005);
    tuya_lan_unregister_cb(0x1006);
    tuya_lan_unregister_cb(FRM_TP_CMD);
}

void tearDown(void)
{
    tuya_lan_exit();
}

void test_tuya_lan_register_cb_rejects_null_handler(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_lan_register_cb(FRM_TP_CMD, NULL));
}

void test_tuya_lan_register_cb_accepts_duplicate_pair_and_unregisters_once(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_register_cb(FRM_TP_CMD, dummy_lan_handler_a));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_register_cb(FRM_TP_CMD, dummy_lan_handler_a));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_unregister_cb(FRM_TP_CMD));
    TEST_ASSERT_EQUAL(OPRT_NOT_FOUND, tuya_lan_unregister_cb(FRM_TP_CMD));
}

void test_tuya_lan_register_cb_rejects_capacity_overflow(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_register_cb(0x1001, dummy_lan_handler_a));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_register_cb(0x1002, dummy_lan_handler_b));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_register_cb(0x1003, dummy_lan_handler_c));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_register_cb(0x1004, dummy_lan_handler_d));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_register_cb(0x1005, dummy_lan_handler_e));
    TEST_ASSERT_EQUAL(OPRT_EXCEED_UPPER_LIMIT, tuya_lan_register_cb(0x1006, dummy_lan_handler_f));
}

void test_tuya_lan_enable_rejects_missing_or_unactivated_client(void)
{
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_lan_enable());

    g_client                   = &g_test_client;
    g_test_client.is_activated = false;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_lan_enable());
}

void test_tuya_lan_disable_and_disconnect_all_return_ok_without_manager(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_disable());
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_disconnect_all());
}

void test_tuya_lan_init_and_exit_manage_resources(void)
{
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_lan_init(NULL));

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    TEST_ASSERT_EQUAL(1, g_sock_loop_init_count);
    TEST_ASSERT_EQUAL(2, g_mutex_create_count);
    TEST_ASSERT_EQUAL(2, g_reg_sock_count);

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_exit());
    TEST_ASSERT_EQUAL(2, g_mutex_release_count);
    TEST_ASSERT_TRUE(g_tal_free_count >= 2);
}

void test_tuya_lan_enable_and_disable_work_for_activated_client(void)
{
    g_client                   = &g_test_client;
    g_test_client.is_activated = true;

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_enable());
    TEST_ASSERT_EQUAL(1, g_sock_loop_init_count);

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_enable());
    TEST_ASSERT_EQUAL(1, g_sock_loop_init_count);

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_disable());
    TEST_ASSERT_EQUAL(1, g_sock_loop_disable_count);
    TEST_ASSERT_TRUE(g_unreg_sock_count >= 2);
}

void test_tuya_lan_data_report_requires_initialized_manager(void)
{
    uint8_t payload[] = {0x01, 0x02};

    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_lan_data_report(FRM_TP_CMD, OPRT_OK, payload, sizeof(payload)));
}

void test_tuya_lan_initialized_without_sessions_reports_no_connected_clients(void)
{
    uint8_t payload[] = {0x01};

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    TEST_ASSERT_EQUAL(OPRT_SVC_LAN_NO_CLIENT_CONNECTED,
                      tuya_lan_data_report(FRM_TP_CMD, OPRT_OK, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(0, tuya_lan_get_connect_client_num());
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_disconnect_all());
}

void test_tuya_lan_tcp_server_client_callbacks_manage_session_lifecycle(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    TEST_ASSERT_NOT_NULL(g_tcp_server_sock.read);

    g_tcp_server_sock.read(g_tcp_server_sock.sock);
    TEST_ASSERT_NOT_NULL(g_client_sock.read);
    TEST_ASSERT_EQUAL(1, tuya_lan_get_connect_client_num());

    g_client_sock.err(g_accept_fd);
    g_tcp_server_sock.pre_select();
    TEST_ASSERT_EQUAL(0, tuya_lan_get_connect_client_num());
}

void test_tuya_lan_security_handshake_enables_dp_report(void)
{
    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    g_tcp_server_sock.read(g_tcp_server_sock.sock);

    g_next_tcp_seq         = 1;
    g_next_tcp_type        = FRM_SECURITY_TYPE3;
    g_next_tcp_payload_len = RAND_LEN;
    memset(g_next_tcp_payload, 0, RAND_LEN);
    g_client_sock.read(g_accept_fd);

    g_next_tcp_seq         = 2;
    g_next_tcp_type        = FRM_SECURITY_TYPE5;
    g_next_tcp_payload_len = HMAC_LEN;
    memset(g_next_tcp_payload, 0, HMAC_LEN);
    g_client_sock.read(g_accept_fd);

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_dp_report("{}"));
    TEST_ASSERT_TRUE(g_net_send_count >= 1);
}

void test_tuya_lan_udp_callbacks_handle_invalid_and_valid_packets(void)
{
    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    TEST_ASSERT_NOT_NULL(g_udp_server_sock.read);

    g_udp_mode = 0;
    g_udp_server_sock.read(g_udp_server_sock.sock);
    TEST_ASSERT_EQUAL(0, g_net_send_to_count);

    g_udp_mode = 1;
    g_udp_server_sock.read(g_udp_server_sock.sock);
    TEST_ASSERT_EQUAL(1, g_net_send_to_count);

    g_udp_server_sock.err(g_udp_server_sock.sock);
    TEST_ASSERT_TRUE(g_unreg_sock_count >= 1);
}

void test_tuya_lan_server_quit_callback_exits_manager(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    TEST_ASSERT_NOT_NULL(g_tcp_server_sock.quit);

    g_tcp_server_sock.quit();
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_disable());
}

void test_lan_msg_gcm_encrypt_returns_serialized_buffer(void)
{
    uint8_t  data[]           = {0x11, 0x22, 0x33};
    uint8_t  key[APP_KEY_LEN] = {0};
    uint8_t *out              = NULL;
    uint32_t out_len          = 0;

    TEST_ASSERT_EQUAL(OPRT_OK, lan_msg_gcm_encrpt(data, sizeof(data), &out, &out_len, key, FRM_TP_CMD));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_UINT32(16, out_len);
    tal_free(out);
}

void test_lan_session_helpers_manage_state_directly(void)
{
    lan_session_t *session = NULL;

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));

    lan_sesison_add(33, 10);
    session = lan_session_get_by_fd(33);
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_EQUAL(1, lan_session_active_num_get());
    TEST_ASSERT_EQUAL_PTR(s_lan_mgr->session, lan_sessions_get());

    lan_session_time_update(session, 22);
    TEST_ASSERT_EQUAL(22, session->time);

    lan_session_fault_set(session);
    TEST_ASSERT_TRUE(session->fault);

    lan_session_close(session);
    TEST_ASSERT_EQUAL(0, lan_session_active_num_get());
    TEST_ASSERT_FALSE(session->active);
    TEST_ASSERT_EQUAL(-1, session->fd);
}

void test_lan_session_time_check_closes_expired_sessions(void)
{
    lan_session_t *session = NULL;

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_sesison_add(44, 0);
    session = lan_session_get_by_fd(44);
    TEST_ASSERT_NOT_NULL(session);

    lan_session_time_check(HEART_BEAT_TIMEOUT + 1);
    TEST_ASSERT_FALSE(session->active);
    TEST_ASSERT_EQUAL(0, lan_session_active_num_get());

    lan_sesison_add(45, 0);
    session = lan_session_get_by_fd(45);
    TEST_ASSERT_NOT_NULL(session);
    lan_session_time_check(2592000 + 1);
    TEST_ASSERT_TRUE(session->active);
}

void test_lan_send_handles_success_and_failure(void)
{
    lan_session_t *session   = NULL;
    uint8_t        payload[] = {0x01, 0x02};

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_sesison_add(46, 0);
    session = lan_session_get_by_fd(46);
    TEST_ASSERT_NOT_NULL(session);

    TEST_ASSERT_EQUAL(OPRT_OK, lan_send(session, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));

    g_net_send_ret = 0;
    TEST_ASSERT_EQUAL(OPRT_SVC_LAN_SEND_ERR, lan_send(session, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));
    TEST_ASSERT_TRUE(session->fault);
}

void test_lan_make_udp_packets_and_dp_report_error_paths(void)
{
    uint8_t *out    = NULL;
    int      outlen = 0;

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));

    lan_make_udp_packets(&out, &outlen);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL(16, outlen);
    tal_free(out);

    lan_sesison_add(47, 0);
    g_pack_protocol_ret = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_lan_dp_report("{}"));
}

void test_lan_protocol_process_covers_cmd_and_query_paths(void)
{
    lan_session_t       *session = NULL;
    lpv35_frame_object_t frame   = {0};
    uint8_t              data[]  = {0x01};

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_sesison_add(48, 0);
    session = lan_session_get_by_fd(48);
    TEST_ASSERT_NOT_NULL(session);

    frame.type           = FRM_TP_CMD;
    frame.data           = data;
    frame.data_len       = sizeof(data);
    g_parse_protocol_ret = OPRT_COM_ERROR;
    lan_protocol_process(s_lan_mgr, session, &frame);

    g_parse_protocol_ret = OPRT_OK;
    g_detach_item        = &g_json_data;
    lan_protocol_process(s_lan_mgr, session, &frame);
    TEST_ASSERT_EQUAL(1, g_dp_parse_count);

    strcpy(g_dp_dump_json, "{\"dps\":{\"1\":true}}");
    frame.type = FRM_QUERY_STAT;
    lan_protocol_process(s_lan_mgr, session, &frame);
    TEST_ASSERT_TRUE(g_net_send_count >= 1);
}

void test_lan_server_and_udp_helpers_cover_error_paths(void)
{
    uint8_t frame_buf[LPV35_FRAME_MINI_SIZE + 8] = {0};

    TEST_ASSERT_FALSE(__udp_serv_is_in_packet_vaild(frame_buf, sizeof(frame_buf)));

    fill_frame_buffer(frame_buf, 1, FRM_TYPE_APP_UDP_BOARDCAST, 2);
    TEST_ASSERT_TRUE(__udp_serv_is_in_packet_vaild(frame_buf, LPV35_FRAME_MINI_SIZE + 2));

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    g_accept_fd = -1;
    lan_tcp_serv_sock_read(s_lan_mgr->tcp_serv_fd);
    lan_tcp_serv_sock_err(s_lan_mgr->tcp_serv_fd);
    lan_udp_serv_sock_err(s_lan_mgr->udp_serv_fd);
}

void test_lan_low_level_error_paths_cover_socket_and_memory_failures(void)
{
    uint8_t  data[] = {0xAA};
    uint8_t *out    = NULL;
    uint32_t outlen = 0;

    g_malloc_fail_on_call = 1;
    TEST_ASSERT_EQUAL(OPRT_MALLOC_FAILED, lan_msg_gcm_encrpt(data, sizeof(data), &out, &outlen, data, FRM_TP_CMD));

    g_malloc_fail_on_call   = 0;
    g_socket_create_tcp_ret = -1;
    TEST_ASSERT_EQUAL(-1, lan_tcp_setup_serv_socket(6668));

    g_socket_create_tcp_ret = 11;
    g_listen_ret            = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_SOCK_ERR, lan_tcp_setup_serv_socket(6668));

    g_socket_create_udp_ret = -1;
    TEST_ASSERT_EQUAL(-1, lan_setup_udp_serv_socket(7000));

    g_socket_create_udp_ret = 12;
    g_bind_ret              = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_SOCK_ERR, lan_setup_udp_serv_socket(7000));
}

void test_lan_send_and_udp_packet_build_cover_additional_failures(void)
{
    lan_session_t *session   = NULL;
    uint8_t        payload[] = {0x01};
    uint8_t       *out       = NULL;
    int            olen      = 0;

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_sesison_add(49, 0);
    session = lan_session_get_by_fd(49);
    TEST_ASSERT_NOT_NULL(session);

    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, lan_send(NULL, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));
    session->active = false;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, lan_send(session, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));
    session->active = true;
    session->fault  = true;
    TEST_ASSERT_EQUAL(OPRT_SVC_LAN_SOCKET_FAULT, lan_send(session, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));
    session->fault             = false;
    g_test_client.is_activated = false;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, lan_send(session, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));
    g_test_client.is_activated = true;

    g_malloc_fail_on_call = g_malloc_call_count + 1;
    TEST_ASSERT_EQUAL(OPRT_MALLOC_FAILED, lan_send(session, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));
    g_malloc_fail_on_call = g_malloc_call_count + 2;
    TEST_ASSERT_EQUAL(OPRT_MALLOC_FAILED, lan_send(session, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));
    g_malloc_fail_on_call = 0;

    g_frame_serialize_ret = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, lan_send(session, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));
    g_frame_serialize_ret = OPRT_OK;

    g_net_send_ret = -1;
    g_net_errno    = UNW_EAGAIN;
    TEST_ASSERT_EQUAL(OPRT_SVC_LAN_SEND_ERR, lan_send(session, 0, FRM_TP_CMD, 0, payload, sizeof(payload), true));
    session->fault = false;
    g_net_send_ret = INT32_MIN;
    g_net_errno    = OPRT_OK;

    s_lan_mgr->iot_client = NULL;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);
    s_lan_mgr->iot_client = &g_test_client;

    g_malloc_fail_on_call = g_malloc_call_count + 1;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);
    g_malloc_fail_on_call = 0;
}

void test_lan_protocol_process_covers_remaining_error_branches(void)
{
    lan_session_t       *session        = NULL;
    lpv35_frame_object_t frame          = {0};
    uint8_t              data[HMAC_LEN] = {0};

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_sesison_add(50, 0);
    session = lan_session_get_by_fd(50);
    TEST_ASSERT_NOT_NULL(session);

    frame.type           = FRM_TP_NEW_CMD;
    frame.data           = data;
    frame.data_len       = sizeof(data);
    g_parse_protocol_ret = OPRT_OK;
    g_cjson_root_valid   = false;
    lan_protocol_process(s_lan_mgr, session, &frame);

    g_cjson_root_valid = true;
    g_detach_item      = NULL;
    lan_protocol_process(s_lan_mgr, session, &frame);

    g_detach_item  = &g_json_data;
    frame.type     = FRM_SECURITY_TYPE3;
    frame.data_len = RAND_LEN - 1;
    lan_protocol_process(s_lan_mgr, session, &frame);

    frame.data_len        = RAND_LEN;
    g_malloc_fail_on_call = g_malloc_call_count + 1;
    lan_protocol_process(s_lan_mgr, session, &frame);
    g_malloc_fail_on_call = 0;

    frame.type     = FRM_SECURITY_TYPE5;
    frame.data_len = HMAC_LEN - 1;
    lan_protocol_process(s_lan_mgr, session, &frame);

    frame.data_len = HMAC_LEN;
    memset(session->hmac, 0x11, sizeof(session->hmac));
    memset(frame.data, 0x22, HMAC_LEN);
    lan_protocol_process(s_lan_mgr, session, &frame);
    session->fault = false;
    memset(session->hmac, 0, sizeof(session->hmac));
    memset(frame.data, 0, HMAC_LEN);
    frame.type         = FRM_QUERY_STAT_NEW;
    g_cjson_root_valid = false;
    lan_protocol_process(s_lan_mgr, session, &frame);
    g_cjson_root_valid = true;
    g_dp_dump_json[0]  = '\0';
    lan_protocol_process(s_lan_mgr, session, &frame);
}

void test_lan_tcp_client_read_and_server_create_cover_edge_cases(void)
{
    lan_session_t *session = NULL;

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_sesison_add(51, 0);
    session = lan_session_get_by_fd(51);
    TEST_ASSERT_NOT_NULL(session);

    g_recv_ret = -1;
    lan_tcp_client_sock_read(51);
    TEST_ASSERT_TRUE(session->fault);

    session->fault = false;
    g_recv_ret     = 8;
    lan_tcp_client_sock_read(51);

    g_recv_ret  = INT32_MIN;
    g_accept_fd = 52;
    lan_sesison_add(53, 0);
    lan_sesison_add(54, 0);
    lan_tcp_serv_sock_read(s_lan_mgr->tcp_serv_fd);
    TEST_ASSERT_TRUE(g_net_close_count >= 1);

    g_reg_sock_ret = OPRT_COM_ERROR;
    memset(s_lan_mgr->session, 0, sizeof(lan_session_t) * s_lan_mgr->cfg->client_num);
    s_lan_mgr->fd_num = 0;
    lan_tcp_serv_sock_read(s_lan_mgr->tcp_serv_fd);
    g_reg_sock_ret = OPRT_OK;
}

void test_lan_udp_helpers_cover_more_error_paths(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));

    g_udp_mode        = 1;
    g_frame_parse_ret = OPRT_COM_ERROR;
    lan_udp_serv_sock_read(s_lan_mgr->udp_serv_fd);

    g_frame_parse_ret  = OPRT_OK;
    g_cjson_root_valid = false;
    lan_udp_serv_sock_read(s_lan_mgr->udp_serv_fd);

    g_cjson_root_valid    = true;
    g_json_ip.valuestring = NULL;
    lan_udp_serv_sock_read(s_lan_mgr->udp_serv_fd);
    g_json_ip.valuestring = "192.168.0.10";

    g_net_send_to_ret = -1;
    g_net_errno       = UNW_EAGAIN;
    lan_udp_serv_sock_read(s_lan_mgr->udp_serv_fd);
    g_net_send_to_ret = INT32_MIN;
    g_net_errno       = OPRT_OK;

    s_lan_mgr->udp_serv_fd = 77;
    TEST_ASSERT_EQUAL(77, lan_udp_create_serv_socket());
}

void test_lan_direct_invalid_parameter_branches(void)
{
    lan_session_t session = {0};

    lan_session_close(NULL);
    lan_sesison_add(-1, 0);
    lan_session_fault_set(NULL);
    lan_session_time_update(NULL, 1);
    TEST_ASSERT_NULL(lan_session_get_by_fd(999));
    TEST_ASSERT_EQUAL(0, lan_session_active_num_get());
    TEST_ASSERT_NULL(lan_sessions_get());

    TEST_ASSERT_EQUAL(0, lan_get_valid_socket_num(NULL));

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_session_close(&session);
    lan_session_fault_set(&session);
    lan_session_time_update(&session, 1);
    TEST_ASSERT_NULL(lan_session_get_by_fd(999));
}

void test_lan_get_valid_socket_num_all_fault_and_report_error_branch(void)
{
    uint8_t payload[] = {1};

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_sesison_add(61, 0);
    lan_sesison_add(62, 0);
    s_lan_mgr->session[0].fault = true;
    s_lan_mgr->session[1].fault = true;
    TEST_ASSERT_EQUAL(0, lan_get_valid_socket_num(s_lan_mgr));

    s_lan_mgr->session[0].fault = false;
    strcpy((char *)s_lan_mgr->session[0].secret_key, "secret");
    g_net_send_ret = 0;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_data_report(FRM_TP_CMD, 0, payload, sizeof(payload)));
}

void test_lan_make_udp_packets_covers_remaining_failure_branches(void)
{
    uint8_t    *out  = NULL;
    int         olen = 0;
    static char long_uuid[400];
    static char long_pk[400];
    static char uuid_active_overflow[175];
    static char uuid_encrypt_overflow[160];
    static char pk_version_overflow[108];
    static char pk_sl_overflow[101];
    static char pk_finalize_overflow[100];

    memset(long_uuid, 'u', sizeof(long_uuid) - 1);
    long_uuid[sizeof(long_uuid) - 1] = '\0';
    memset(long_pk, 'p', sizeof(long_pk) - 1);
    long_pk[sizeof(long_pk) - 1] = '\0';
    memset(uuid_active_overflow, 'a', sizeof(uuid_active_overflow) - 1);
    uuid_active_overflow[sizeof(uuid_active_overflow) - 1] = '\0';
    memset(uuid_encrypt_overflow, 'b', sizeof(uuid_encrypt_overflow) - 1);
    uuid_encrypt_overflow[sizeof(uuid_encrypt_overflow) - 1] = '\0';
    memset(pk_version_overflow, 'c', sizeof(pk_version_overflow) - 1);
    pk_version_overflow[sizeof(pk_version_overflow) - 1] = '\0';
    memset(pk_sl_overflow, 'd', sizeof(pk_sl_overflow) - 1);
    pk_sl_overflow[sizeof(pk_sl_overflow) - 1] = '\0';
    memset(pk_finalize_overflow, 'e', sizeof(pk_finalize_overflow) - 1);
    pk_finalize_overflow[sizeof(pk_finalize_overflow) - 1] = '\0';

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));

    g_test_client.config.uuid = long_uuid;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);

    g_test_client.config.uuid       = "12345678901234567890";
    g_test_client.config.productkey = long_pk;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);
    g_test_client.config.productkey = "productkey123456";

    g_test_client.config.uuid = uuid_active_overflow;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);

    g_test_client.config.uuid = uuid_encrypt_overflow;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);
    g_test_client.config.uuid = "12345678901234567890";

    g_test_client.config.productkey = pk_version_overflow;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);

    g_test_client.config.productkey = pk_sl_overflow;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);

    g_test_client.config.productkey = pk_finalize_overflow;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);
    g_test_client.config.productkey = "productkey123456";

    g_malloc_fail_on_call = g_malloc_call_count + 2;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);
    g_malloc_fail_on_call = g_malloc_call_count + 3;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);
    g_malloc_fail_on_call = 0;

    g_frame_serialize_ret = OPRT_COM_ERROR;
    lan_make_udp_packets(&out, &olen);
    TEST_ASSERT_NULL(out);
    g_frame_serialize_ret = OPRT_OK;
}

void test_lan_protocol_process_covers_dps_missing_and_encrypt_failure(void)
{
    lan_session_t       *session        = NULL;
    lpv35_frame_object_t frame          = {0};
    uint8_t              data[HMAC_LEN] = {0};

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_sesison_add(63, 0);
    session = lan_session_get_by_fd(63);
    TEST_ASSERT_NOT_NULL(session);

    frame.type           = FRM_TP_CMD;
    frame.data           = data;
    frame.data_len       = sizeof(data);
    g_parse_protocol_ret = OPRT_OK;
    g_detach_item        = &g_json_data;
    g_missing_dps_item   = true;
    lan_protocol_process(s_lan_mgr, session, &frame);
    TEST_ASSERT_EQUAL(0, g_dp_parse_count);
    g_missing_dps_item = false;

    frame.type     = FRM_SECURITY_TYPE5;
    frame.data_len = HMAC_LEN;
    memset(frame.data, 0, HMAC_LEN);
    memset(session->hmac, 0, sizeof(session->hmac));
    g_encrypt_ret = OPRT_COM_ERROR;
    lan_protocol_process(s_lan_mgr, session, &frame);
    TEST_ASSERT_TRUE(session->fault);
    g_encrypt_ret = OPRT_OK;
}

void test_tuya_lan_dp_report_covers_no_session_and_send_error_log_branch(void)
{
    uint8_t dummy[] = {0x01};

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tuya_lan_dp_report("{}"));

    lan_sesison_add(65, 0);
    strcpy((char *)s_lan_mgr->session[0].secret_key, "secret");
    g_net_send_ret = 0;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_dp_report("{}"));
    g_net_send_ret = INT32_MIN;

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_data_report(FRM_TP_CMD, 0, dummy, sizeof(dummy)));
}

void test_lan_tcp_client_error_and_read_additional_edges(void)
{
    uint8_t        bad_head[LPV35_FRAME_MINI_SIZE + 2]  = {0};
    uint8_t        big_frame[LPV35_FRAME_MINI_SIZE + 4] = {0};
    lan_session_t *session                              = NULL;

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));

    lan_tcp_client_sock_err(999);

    lan_sesison_add(66, 0);
    session = lan_session_get_by_fd(66);
    TEST_ASSERT_NOT_NULL(session);
    session->active = false;
    lan_tcp_client_sock_err(66);
    session->active = true;

    lan_tcp_client_sock_read(999);

    fill_frame_buffer(bad_head, 2, FRM_TP_CMD, 2);
    bad_head[0] = 0xAA;
    memcpy(g_recv_raw_buf, bad_head, sizeof(bad_head));
    g_recv_raw_len = (int)sizeof(bad_head);
    g_recv_ret     = -1;
    lan_tcp_client_sock_read(66);

    fill_frame_buffer(big_frame, 1, FRM_TP_CMD, LAN_FRAME_MAX_LEN);
    memcpy(g_recv_raw_buf, big_frame, sizeof(big_frame));
    g_recv_raw_len = (int)sizeof(big_frame);
    g_recv_ret     = -1;
    lan_tcp_client_sock_read(66);

    session->sequence_in = 10;
    fill_frame_buffer(big_frame, 1, FRM_TP_CMD, 2);
    memcpy(g_recv_raw_buf, big_frame, sizeof(big_frame));
    g_recv_raw_len = (int)sizeof(big_frame);
    g_recv_ret     = -1;
    lan_tcp_client_sock_read(66);
    g_recv_ret = INT32_MIN;
}

void test_lan_tcp_client_read_covers_tmp_buffer_and_runtime_paths(void)
{
    uint8_t        partial_frame[LPV35_FRAME_MINI_SIZE + 4] = {0};
    lan_session_t *session                                  = NULL;

    g_test_client.is_activated = true;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    lan_sesison_add(67, 0);
    session = lan_session_get_by_fd(67);
    TEST_ASSERT_NOT_NULL(session);

    fill_frame_buffer(partial_frame, 11, FRM_TP_CMD, 20);
    memcpy(g_recv_raw_buf, partial_frame, sizeof(partial_frame));
    g_recv_raw_len     = (int)sizeof(partial_frame);
    g_recv_nd_size_ret = -1;
    g_recv_ret         = -1;
    lan_tcp_client_sock_read(67);

    g_recv_nd_size_ret = 20;
    memset(g_recv_nd_raw_buf, 0, 20);
    g_recv_nd_raw_len = 20;
    memcpy(g_recv_raw_buf, partial_frame, sizeof(partial_frame));
    g_recv_raw_len    = (int)sizeof(partial_frame);
    g_frame_parse_ret = OPRT_COM_ERROR;
    lan_tcp_client_sock_read(67);
    g_recv_nd_raw_len = 0;
    g_frame_parse_ret = OPRT_OK;

    strcpy((char *)session->secret_key, "secret");
    g_next_tcp_type = FRM_TP_HB;
    g_recv_ret      = INT32_MIN;
    lan_tcp_client_sock_read(67);

    session->secret_key[0]                   = '\0';
    s_lan_mgr->cfg->allow_no_session_key_num = 1;
    g_next_tcp_type                          = FRM_TP_CMD;
    g_recv_ret                               = -1;
    lan_tcp_client_sock_read(67);
    s_lan_mgr->cfg->allow_no_session_key_num = 0;
    g_recv_raw_len                           = (int)sizeof(partial_frame);
    memcpy(g_recv_raw_buf, partial_frame, sizeof(partial_frame));
    lan_tcp_client_sock_read(67);

    session->active            = true;
    session->fd                = 67;
    session->sequence_in       = 0;
    g_test_client.is_activated = false;
    memcpy(g_recv_raw_buf, partial_frame, sizeof(partial_frame));
    g_recv_raw_len = (int)sizeof(partial_frame);
    lan_tcp_client_sock_read(67);
    g_test_client.is_activated = true;
    g_recv_ret                 = INT32_MIN;

    strcpy((char *)session->secret_key, "secret");
    g_next_tcp_type = FRM_SECURITY_TYPE3;
    memcpy(g_recv_raw_buf, partial_frame, sizeof(partial_frame));
    g_recv_raw_len = (int)sizeof(partial_frame);
    g_recv_ret     = -1;
    lan_tcp_client_sock_read(67);
    g_recv_ret = INT32_MIN;
}

void test_lan_udp_send_non_retry_error_and_exit_disable_resource_branches(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));

    g_udp_mode        = 1;
    g_net_send_to_ret = -1;
    g_net_errno       = OPRT_COM_ERROR;
    lan_udp_serv_sock_read(s_lan_mgr->udp_serv_fd);
    g_net_send_to_ret = INT32_MIN;
    g_net_errno       = OPRT_OK;

    s_lan_mgr->udp_client_fd = 88;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_exit());

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    s_lan_mgr->tcp_serv_fd   = 11;
    s_lan_mgr->udp_serv_fd   = 12;
    s_lan_mgr->udp_client_fd = 13;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_disable());
}

void test_tuya_lan_init_duplicate_and_failure_branches(void)
{
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    tuya_lan_exit();

    g_mutex_create_fail_on_call = g_mutex_create_count + 2;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_lan_init(&g_test_client));
    g_mutex_create_fail_on_call = 0;
}

void test_lan_udp_and_server_creation_failure_paths(void)
{
    uint8_t frame_buf[LPV35_FRAME_MINI_SIZE + 8] = {0};

    fill_frame_buffer(frame_buf, 1, FRM_TYPE_APP_UDP_BOARDCAST, 2);
    frame_buf[LPV35_FRAME_MINI_SIZE + 2 - 1] = 0xAA;
    TEST_ASSERT_FALSE(__udp_serv_is_in_packet_vaild(frame_buf, LPV35_FRAME_MINI_SIZE + 2));

    fill_frame_buffer(frame_buf, 1, FRM_TP_CMD, 2);
    TEST_ASSERT_FALSE(__udp_serv_is_in_packet_vaild(frame_buf, LPV35_FRAME_MINI_SIZE + 2));

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    g_udp_mode = -1;
    lan_udp_serv_sock_read(s_lan_mgr->udp_serv_fd);

    g_udp_mode        = 1;
    g_missing_ip_item = true;
    lan_udp_serv_sock_read(s_lan_mgr->udp_serv_fd);
    g_missing_ip_item = false;

    g_socket_create_udp_ret = -1;
    s_lan_mgr->udp_serv_fd  = -1;
    TEST_ASSERT_EQUAL(-1, lan_udp_create_serv_socket());

    g_socket_create_udp_ret = 12;
    g_reg_sock_ret          = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(-2, lan_udp_create_serv_socket());
    g_reg_sock_ret = OPRT_OK;

    s_lan_mgr->serv_fd_switch = true;
    lan_tcp_serv_sock_pre_select();

    g_socket_create_tcp_ret = -1;
    TEST_ASSERT_EQUAL(-1, lan_tcp_create_serv_socket(s_lan_mgr));
    g_socket_create_tcp_ret = 11;
    g_reg_sock_ret          = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(-2, lan_tcp_create_serv_socket(s_lan_mgr));
    g_reg_sock_ret = OPRT_OK;
}

void test_tuya_lan_init_and_disable_failure_paths(void)
{
    g_sock_loop_init_ret = OPRT_COM_ERROR;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_lan_init(&g_test_client));
    g_sock_loop_init_ret = OPRT_OK;

    g_malloc_fail_on_call = g_malloc_call_count + 1;
    TEST_ASSERT_EQUAL(OPRT_MALLOC_FAILED, tuya_lan_init(&g_test_client));
    g_malloc_fail_on_call = 0;

    g_mutex_create_fail_on_call = 1;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_lan_init(&g_test_client));
    g_mutex_create_fail_on_call = 2;
    TEST_ASSERT_EQUAL(OPRT_COM_ERROR, tuya_lan_init(&g_test_client));
    g_mutex_create_fail_on_call = 0;

    g_socket_create_tcp_ret = -1;
    TEST_ASSERT_EQUAL(-1, tuya_lan_init(&g_test_client));
    g_socket_create_tcp_ret = 11;
    g_socket_create_udp_ret = -1;
    TEST_ASSERT_EQUAL(-1, tuya_lan_init(&g_test_client));
    g_socket_create_udp_ret = 12;

    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_init(&g_test_client));
    s_lan_mgr->udp_client_fd     = 66;
    s_lan_mgr->tcp_serv_fd       = 11;
    s_lan_mgr->udp_serv_fd       = 12;
    g_sock_loop_is_inited_cycles = 2;
    TEST_ASSERT_EQUAL(OPRT_OK, tuya_lan_disable());
}

void test_tuya_lan_get_client_num_returns_default_limit(void)
{
    TEST_ASSERT_EQUAL(3u, tuya_lan_get_client_num());
}

void test_tuya_lan_get_connect_client_num_returns_zero_without_manager(void)
{
    TEST_ASSERT_EQUAL(0, tuya_lan_get_connect_client_num());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuya_lan_register_cb_rejects_null_handler);
    RUN_TEST(test_tuya_lan_register_cb_accepts_duplicate_pair_and_unregisters_once);
    RUN_TEST(test_tuya_lan_register_cb_rejects_capacity_overflow);
    RUN_TEST(test_tuya_lan_enable_rejects_missing_or_unactivated_client);
    RUN_TEST(test_tuya_lan_disable_and_disconnect_all_return_ok_without_manager);
    RUN_TEST(test_tuya_lan_init_and_exit_manage_resources);
    RUN_TEST(test_tuya_lan_enable_and_disable_work_for_activated_client);
    RUN_TEST(test_tuya_lan_data_report_requires_initialized_manager);
    RUN_TEST(test_tuya_lan_initialized_without_sessions_reports_no_connected_clients);
    RUN_TEST(test_tuya_lan_tcp_server_client_callbacks_manage_session_lifecycle);
    RUN_TEST(test_tuya_lan_security_handshake_enables_dp_report);
    RUN_TEST(test_tuya_lan_udp_callbacks_handle_invalid_and_valid_packets);
    RUN_TEST(test_tuya_lan_server_quit_callback_exits_manager);
    RUN_TEST(test_lan_msg_gcm_encrypt_returns_serialized_buffer);
    RUN_TEST(test_lan_session_helpers_manage_state_directly);
    RUN_TEST(test_lan_session_time_check_closes_expired_sessions);
    RUN_TEST(test_lan_send_handles_success_and_failure);
    RUN_TEST(test_lan_make_udp_packets_and_dp_report_error_paths);
    RUN_TEST(test_lan_protocol_process_covers_cmd_and_query_paths);
    RUN_TEST(test_lan_server_and_udp_helpers_cover_error_paths);
    RUN_TEST(test_lan_low_level_error_paths_cover_socket_and_memory_failures);
    RUN_TEST(test_lan_send_and_udp_packet_build_cover_additional_failures);
    RUN_TEST(test_lan_protocol_process_covers_remaining_error_branches);
    RUN_TEST(test_lan_tcp_client_read_and_server_create_cover_edge_cases);
    RUN_TEST(test_lan_udp_helpers_cover_more_error_paths);
    RUN_TEST(test_lan_direct_invalid_parameter_branches);
    RUN_TEST(test_lan_get_valid_socket_num_all_fault_and_report_error_branch);
    RUN_TEST(test_lan_make_udp_packets_covers_remaining_failure_branches);
    RUN_TEST(test_lan_protocol_process_covers_dps_missing_and_encrypt_failure);
    RUN_TEST(test_tuya_lan_dp_report_covers_no_session_and_send_error_log_branch);
    RUN_TEST(test_lan_udp_send_non_retry_error_and_exit_disable_resource_branches);
    RUN_TEST(test_tuya_lan_init_duplicate_and_failure_branches);
    RUN_TEST(test_lan_udp_and_server_creation_failure_paths);
    RUN_TEST(test_tuya_lan_init_and_disable_failure_paths);
    RUN_TEST(test_tuya_lan_get_client_num_returns_default_limit);
    RUN_TEST(test_tuya_lan_get_connect_client_num_returns_zero_without_manager);
    return UNITY_END();
}
