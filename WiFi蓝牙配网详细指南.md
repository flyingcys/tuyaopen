# TuyaOpen WiFi和蓝牙配网详细指南

## 目录
1. [概述](#概述)
2. [WiFi AP配网](#wifi-ap配网)
3. [蓝牙BLE配网](#蓝牙ble配网) 
4. [配网管理器](#配网管理器)
5. [完整API参考](#完整api参考)
6. [实际应用示例](#实际应用示例)

## 概述

TuyaOpen提供两种主要的设备配网方式：
- **WiFi AP配网**：设备开启热点，APP连接后发送WiFi信息
- **蓝牙BLE配网**：通过蓝牙传输WiFi配置信息

两种配网方式可以同时启用，为用户提供更灵活的配网选择。

## WiFi AP配网

### 1. AP配网核心数据结构

```c
typedef struct {
    THREAD_HANDLE thread;           // 配网处理线程
    BOOL_T thread_exit_flag;        // 线程退出标志

    uint8_t recv_buf[AP_MAX_BUFSIZE]; // 接收缓冲区 (4096字节)

    netcfg_args_t netcfg_args;      // 配网参数
    netcfg_info_t netcfg_info;      // 配网结果信息  
    netcfg_finish_cb_t netcfg_finish_cb; // 配网完成回调

    TUYA_IP_ADDR_T serv_ip;         // 服务器IP地址

    int psk_fd;                     // PSK套接字
    int client_fd;                  // 客户端套接字
    int broadcast_fd;               // 广播套接字
    bool is_psk_pincode;            // 是否使用PIN码PSK

    tuya_tls_hander tls_hander;     // TLS处理句柄
    uint8_t app_key[APP_KEY_LEN];   // 应用密钥 (16字节)
    uint8_t tls_psk[AP_TLS_PSK_LEN + 1]; // TLS PSK (37字节)

    TIMER_ID broadcast_timer;       // 广播定时器
} ap_netcfg_t;
```

### 2. AP配网端口定义

```c
#define AP_BROADCAST_PORT       6667  // 广播端口
#define AP_TLS_PSK_PORT         6668  // TLS+PSK端口
#define AP_TLS_PSK_PINCODE_PORT 7001  // TLS+PSK(PIN码)端口
#define AP_MAX_BUFSIZE          4096  // 最大缓冲区大小
#define AP_TLS_PSK_LEN          37    // PSK长度
#define AP_MAX_STA_CONN         1     // 最大连接数
```

### 3. AP配网初始化

#### ap_netcfg_init()
```c
/**
 * @brief 初始化AP配网
 * 
 * @param netcfg_args 配网参数
 * @return OPRT_OK 成功，其他值表示错误
 */
int ap_netcfg_init(netcfg_args_t *netcfg_args)
{
    // 等待重置完成
    while (s_ap_netcfg != NULL) {
        tal_system_sleep(200);
    }
    
    // 分配内存
    s_ap_netcfg = tal_malloc(sizeof(ap_netcfg_t));
    if (!s_ap_netcfg) {
        return OPRT_MALLOC_FAILED;
    }
    
    memset(s_ap_netcfg, 0, sizeof(ap_netcfg_t));
    memcpy(&s_ap_netcfg->netcfg_args, netcfg_args, sizeof(netcfg_args_t));
    
    // 注册配网处理器
    return netcfg_register(NETCFG_TUYA_WIFI_AP, ap_netcfg_start, ap_netcfg_stop);
}
```

### 4. AP模式启动

#### ap_mode_start()
```c
static int ap_mode_start(ap_netcfg_t *ap)
{
    int op_ret = OPRT_OK;
    
    // 1. 设置WiFi为AP模式
    op_ret = tal_wifi_set_work_mode(WWM_SOFTAP);
    if (OPRT_OK != op_ret) {
        PR_ERR("设置AP模式失败:%d", op_ret);
        return op_ret;
    }
    
    // 2. 配置AP参数
    WF_AP_CFG_IF_S ap_cfg = {0};
    NW_MAC_S mac;
    
    // 获取MAC地址用于生成SSID
    op_ret = tal_wifi_get_mac(WF_AP, &mac);
    if (OPRT_OK != op_ret) {
        PR_ERR("获取MAC地址失败:%d", op_ret);
        return op_ret;
    }
    
    // 配置IP信息
    strcpy(ap_cfg.ip.ip, "192.168.176.1");
    strcpy(ap_cfg.ip.gw, "192.168.176.1");
    strcpy(ap_cfg.ip.mask, "255.255.255.0");
    
    // 生成AP SSID：SmartLife-XXXX (基于MAC地址后两位)
    sprintf((char *)ap_cfg.ssid, "%s-%02X%02X", 
            TUYA_AP_SSID_DEFAULT, mac.mac[4], mac.mac[5]);
    ap_cfg.s_len = strlen((char *)ap_cfg.ssid);
    ap_cfg.md = WAAM_OPEN;        // 开放模式，无密码
    ap_cfg.chan = 6;              // 信道6
    ap_cfg.max_conn = AP_MAX_STA_CONN; // 最大连接数1
    ap_cfg.ms_interval = 100;     // Beacon间隔100ms
    
    // 3. 启动AP
    op_ret = tal_wifi_ap_start(&ap_cfg);
    if (OPRT_OK != op_ret) {
        PR_ERR("启动AP失败:%d", op_ret);
        return op_ret;
    }
    
    PR_DEBUG("AP启动成功:%s", ap_cfg.ssid);
    return OPRT_OK;
}
```

### 5. 设备信息广播

#### ap_broadcast_timeout()
```c
static void ap_broadcast_timeout(TIMER_ID timerID, void *pTimerArg)
{
    ap_netcfg_t *ap = (ap_netcfg_t *)pTimerArg;
    char *json_buf = NULL;
    
    // 1. 构建设备配置信息
    int ret = ap_dev_config_make(ap, &json_buf);
    if (ret != OPRT_OK) {
        PR_ERR("构建广播数据失败");
        return;
    }
    
    // 设备信息JSON格式：
    // {
    //   "ip": "192.168.176.1",
    //   "uuid": "设备UUID", 
    //   "active": 0,
    //   "version": "3.5",
    //   "sl": 1,
    //   "apConfigType": 1,
    //   "CombosFlag": 8
    // }
    
    // 2. 封装为LPV3.5协议包
    size_t plaintext_len = sizeof(lpv35_plaintext_data_t) + strlen(json_buf);
    lpv35_plaintext_data_t *plaintext_data = tal_malloc(plaintext_len);
    if (!plaintext_data) {
        tal_free(json_buf);
        return;
    }
    
    plaintext_data->ret_code = 0;
    memcpy(plaintext_data->data, json_buf, strlen(json_buf));
    tal_free(json_buf);
    
    // 3. 构建帧对象
    lpv35_frame_object_t frame = {
        .sequence = 0,
        .type = FRM_TYPE_AP_ENCRYPTION,
        .data = (uint8_t *)plaintext_data,
        .data_len = plaintext_len,
    };
    
    // 4. 序列化并广播
    size_t olen = 0;
    uint8_t *send_buf = tal_malloc(lpv35_frame_buffer_size_get(&frame));
    if (send_buf) {
        ret = lpv35_frame_serialize(ap->app_key, APP_KEY_LEN, &frame, send_buf, (int *)&olen);
        if (ret == OPRT_OK) {
            // 广播到6667端口
            tal_net_send_to(ap->broadcast_fd, send_buf, olen, 
                           TY_IPADDR_BROADCAST, AP_BROADCAST_PORT);
        }
        tal_free(send_buf);
    }
    tal_free(plaintext_data);
}
```

### 6. 配网数据处理

#### ap_cfg_cmd_patse()
```c
static int ap_cfg_cmd_patse(ap_netcfg_t *ap, char *data)
{
    cJSON *root = cJSON_Parse(data);
    if (!root) {
        return OPRT_CJSON_PARSE_ERR;
    }
    
    // 解析配网数据
    cJSON *ssid = cJSON_GetObjectItem(root, "s");
    cJSON *passwd = cJSON_GetObjectItem(root, "p"); 
    cJSON *token = cJSON_GetObjectItem(root, "t");
    
    if (!ssid || !passwd || !token) {
        cJSON_Delete(root);
        return OPRT_CJSON_GET_ERR;
    }
    
    // 保存配网信息
    memset(&ap->netcfg_info, 0, sizeof(netcfg_info_t));
    strncpy((char *)ap->netcfg_info.ssid, ssid->valuestring, WIFI_SSID_LEN);
    ap->netcfg_info.s_len = strlen(ssid->valuestring);
    
    strncpy((char *)ap->netcfg_info.passwd, passwd->valuestring, WIFI_PASSWD_LEN);
    ap->netcfg_info.p_len = strlen(passwd->valuestring);
    
    strncpy((char *)ap->netcfg_info.token, token->valuestring, WL_TOKEN_LEN);
    ap->netcfg_info.t_len = strlen(token->valuestring);
    
    cJSON_Delete(root);
    return OPRT_OK;
}
```

### 7. AP配网启动流程

#### ap_netcfg_start()
```c
static int ap_netcfg_start(int type, netcfg_finish_cb_t cb, void *args)
{
    ap_netcfg_t *ap = ap_netcfg_get();
    int op_ret = OPRT_OK;
    
    if (!cb || !ap) {
        return OPRT_INVALID_PARM;
    }
    
    // 1. 启动AP模式
    op_ret = ap_mode_start(ap);
    if (op_ret != OPRT_OK) {
        goto __exit;
    }
    
    // 2. 获取AP IP地址
    NW_IP_S ip;
    op_ret = tal_wifi_get_ip(WF_AP, &ip);
    if (op_ret != OPRT_OK) {
        goto __exit;
    }
    
    ap->serv_ip = tal_net_str2addr(ip.ip);
    ap->netcfg_finish_cb = cb;
    ap->client_fd = -1;
    ap->psk_fd = -1;
    
    // 3. 设置广播或PIN码模式
    if (!ap->netcfg_args.pincode || strlen(ap->netcfg_args.pincode) == 0) {
        // 标准广播模式
        ap->broadcast_fd = tal_net_socket_create(PROTOCOL_UDP);
        if (ap->broadcast_fd < 0) {
            op_ret = OPRT_COM_ERROR;
            goto __exit;
        }
        
        tal_net_set_broadcast(ap->broadcast_fd);
        tal_net_bind(ap->broadcast_fd, ap->serv_ip, AP_BROADCAST_PORT);
        
        // 启动1秒周期广播定时器
        op_ret = tal_sw_timer_create((TAL_TIMER_CB)ap_broadcast_timeout, ap, &ap->broadcast_timer);
        op_ret |= tal_sw_timer_start(ap->broadcast_timer, 1000, TAL_TIMER_CYCLE);
        if (op_ret != OPRT_OK) {
            goto __exit;
        }
    } else {
        // PIN码模式
        ap->is_psk_pincode = true;
    }
    
    // 4. 创建配网处理线程
    THREAD_CFG_T thread_cfg = {
        .priority = THREAD_PRIO_2,
        .stackDepth = 4096,
        .thrdname = "ap_cfg_task"
    };
    
    op_ret = tal_thread_create_and_start(&ap->thread, NULL, NULL, 
                                        ap_netcfg_thread, ap, &thread_cfg);
    if (op_ret != OPRT_OK) {
        goto __exit;
    }
    
    return OPRT_OK;
    
__exit:
    // 清理资源
    if (ap->broadcast_timer) {
        tal_sw_timer_delete(ap->broadcast_timer);
        ap->broadcast_timer = NULL;
    }
    if (ap->broadcast_fd > 0) {
        tal_net_close(ap->broadcast_fd);
    }
    ap_netcfg_free();
    return op_ret;
}
```

## 蓝牙BLE配网

### 1. BLE配网核心数据结构

```c
typedef struct {
    netcfg_args_t netcfg_args;      // 配网参数
    netcfg_info_t netcfg_info;      // 配网结果信息
    netcfg_finish_cb_t netcfg_finish_cb; // 配网完成回调
} ble_netcfg_t;

// BLE通道类型
typedef enum {
    BLE_CHANNLE_NETCFG = 1,         // 配网通道
    BLE_CHANNEL_MAX,
} ble_channel_type_t;

// BLE通道回调函数类型
typedef void (*ble_channel_fn_t)(void *data, void *user_data);
```

### 2. BLE配网初始化

#### ble_netcfg_init()
```c
/**
 * @brief 初始化BLE配网
 * 
 * @param netcfg_args 配网参数
 * @return OPRT_OK 成功，其他值表示错误
 */
int ble_netcfg_init(netcfg_args_t *netcfg_args)
{
    // 清空配网句柄
    memset(&g_bt_netcfg_handle, 0, sizeof(g_bt_netcfg_handle));
    
    // 注册BLE配网处理器
    return netcfg_register(NETCFG_TUYA_BLE, ble_netcfg_start, ble_netcfg_stop);
}
```

### 3. BLE配网启动

#### ble_netcfg_start()
```c
/**
 * @brief 启动BLE配网
 * 
 * @param type 配网类型
 * @param cb 配网完成回调
 * @param args 附加参数
 * @return OPRT_OK 成功，其他值表示错误
 */
int ble_netcfg_start(int type, netcfg_finish_cb_t cb, void *args)
{
    int ret = OPRT_OK;
    PR_DEBUG("BLE配网启动");
    
    // 1. 设置配网完成回调
    g_bt_netcfg_handle.netcfg_finish_cb = cb;
    
    // 2. 添加BLE配网通道
    ble_channel_add(BLE_CHANNLE_NETCFG, __handle_net_cfg, NULL);
    
    // 3. 更新BLE广播数据
    tuya_ble_adv_update();
    
    return ret;
}
```

### 4. BLE配网数据处理

#### __handle_net_cfg()
```c
static void __handle_net_cfg(void *data, void *user_data)
{
    uint8_t result = 0;
    uint8_t resp[5];
    
    // 1. 解析JSON配网数据
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        PR_ERR("JSON解析失败");
        result = (uint8_t)OPRT_CJSON_PARSE_ERR;
        goto __exit;
    }
    
    // 2. 提取必要字段
    cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
    cJSON *token_item = cJSON_GetObjectItem(json, "token");
    cJSON *pwd_item = cJSON_GetObjectItem(json, "pwd");
    
    if (!ssid_item || !token_item) {
        PR_ERR("缺少必要字段");
        result = (uint8_t)OPRT_CJSON_GET_ERR;
        goto __exit;
    }
    
    // 3. 获取配网信息
    char *ssid = ssid_item->valuestring;
    char *token = token_item->valuestring;
    char *passwd = pwd_item ? pwd_item->valuestring : "";
    
    if (!passwd) {
        PR_ERR("密码字段错误");
        result = (uint8_t)OPRT_CJSON_GET_ERR;
        goto __exit;
    }
    
    PR_NOTICE("BLE配网信息 - SSID:%s, 密码:%s, Token:%s", ssid, passwd, token);
    
    // 4. 保存配网信息
    strncpy((char *)g_bt_netcfg_handle.netcfg_info.ssid, ssid, WIFI_SSID_LEN + 1);
    g_bt_netcfg_handle.netcfg_info.s_len = strlen(ssid);
    
    strncpy((char *)g_bt_netcfg_handle.netcfg_info.passwd, passwd, WIFI_PASSWD_LEN + 1);
    g_bt_netcfg_handle.netcfg_info.p_len = strlen(passwd);
    
    strncpy((char *)g_bt_netcfg_handle.netcfg_info.token, token, WL_TOKEN_LEN + 1);
    g_bt_netcfg_handle.netcfg_info.t_len = strlen(token);
    
    // 5. 调用配网完成回调
    g_bt_netcfg_handle.netcfg_finish_cb(NETCFG_TUYA_BLE, &g_bt_netcfg_handle.netcfg_info);
    
    // 6. 保存注册中心信息
    cJSON *reg = cJSON_GetObjectItem(json, "reg");
    if (reg) {
        tuya_register_center_save(RCS_APP, reg);
    }
    
__exit:
    // 清理资源
    if (json) {
        cJSON_Delete(json);
    }
    
    // 发送响应
    resp[1] = 0x00; // 非子包
    resp[2] = 0x00; // 无响应
    resp[3] = FRM_DATA_TRANS_SUBCMD_BT_NETCFG; // 配网子命令
    resp[4] = result; // 结果码：0x00成功，其他失败
    tuya_ble_send(FRM_UPLINK_TRANSPARENT_REQ, 0, resp, 5);
}
```

### 5. BLE广播数据更新

#### tuya_ble_adv_update()
```c
/**
 * @brief 更新BLE广播数据
 * 
 * @return OPRT_OK 成功，其他值表示错误
 */
int tuya_ble_adv_update(void)
{
    if (!s_ble_mgr) {
        return OPRT_INVALID_PARM;
    }
    
    ble_adv_update(s_ble_mgr);
    return OPRT_OK;
}

static void ble_adv_update(tuya_ble_mgr_t *ble)
{
    // 设置广播数据
    ble_adv_set(ble);
    
    // 停止当前广播
    tal_ble_advertising_stop();
    
    // 设置新的广播和扫描响应数据
    tal_ble_advertising_scan_response_data_set(ble->rsp_data, ble->rsp_len);
    tal_ble_advertising_data_set(ble->adv_data, ble->adv_len);
    
    // 设置广播参数
    TAL_BLE_ADV_PARAMS_T adv_params = {
        .type = TAL_BLE_ADV_TYPE_IND,
        .min_interval = 0x20,      // 20ms
        .max_interval = 0x40,      // 40ms  
        .timeout = 0,              // 无超时
    };
    tal_ble_advertising_params_set(&adv_params);
    
    // 启动广播
    tal_ble_advertising_start();
}
```

### 6. BLE配网停止

#### ble_netcfg_stop()
```c
/**
 * @brief 停止BLE配网
 * 
 * @param type 配网类型
 * @return OPRT_OK 成功，其他值表示错误
 */
int ble_netcfg_stop(int type)
{
    int ret = OPRT_OK;
    
    PR_DEBUG("BLE配网停止");
    
    // 删除BLE配网通道
    ble_channel_del(BLE_CHANNLE_NETCFG);
    
    return ret;
}
```

## 配网管理器

### 1. 配网类型定义

```c
typedef enum {
    NETCFG_TUYA_WIFI_AP = 1 << 0,    // WiFi AP配网
    NETCFG_TUYA_BLE = 1 << 1,        // BLE配网
    NETCFG_TUYA_API_USER = 1 << 2,   // 用户自定义配网
    NETCFG_TUYA_WIFI_PEGASUS = 1 << 3, // WiFi Pegasus配网
    NETCFG_AMAZON_WIFI_FFS = 1 << 4,  // Amazon FFS配网
} netcfg_type_t;
```

### 2. 配网参数结构

```c
typedef struct {
    netcfg_type_t type;             // 配网类型
    char *uuid;                     // 设备UUID
    char *pincode;                  // PIN码（可选）
} netcfg_args_t;

typedef struct {
    uint8_t ssid[WIFI_SSID_LEN + 1];      // WiFi名称
    uint8_t s_len;                        // SSID长度
    uint8_t passwd[WIFI_PASSWD_LEN + 1];  // WiFi密码
    uint8_t p_len;                        // 密码长度
    uint8_t token[WL_TOKEN_LEN + 1];      // 绑定Token
    uint8_t t_len;                        // Token长度
} netcfg_info_t;
```

### 3. 配网回调函数

```c
/**
 * @brief 配网完成回调函数类型
 * 
 * @param type 配网类型
 * @param info 配网信息
 * @return OPRT_OK 成功，其他值表示错误
 */
typedef int (*netcfg_finish_cb_t)(int type, netcfg_info_t *info);
```

### 4. 配网注册接口

#### netcfg_register()
```c
/**
 * @brief 注册配网处理器
 * 
 * @param type 配网类型
 * @param start_cb 启动回调
 * @param stop_cb 停止回调
 * @return OPRT_OK 成功，其他值表示错误
 */
int netcfg_register(int type, netcfg_start_cb_t start_cb, netcfg_stop_cb_t stop_cb);
```

### 5. 配网控制接口

#### netcfg_start()
```c
/**
 * @brief 启动网络配置
 * 
 * @param type 配网类型（可以使用位或组合多种类型）
 * @param netcfg_finish_cb 配网完成回调
 * @param args 额外参数
 * @return OPRT_OK 成功，其他值表示错误
 */
int netcfg_start(int type, netcfg_finish_cb_t netcfg_finish_cb, void *args);
```

#### netcfg_stop()
```c
/**
 * @brief 停止网络配置
 * 
 * @param type 配网类型，0表示停止所有
 * @return OPRT_OK 成功，其他值表示错误
 */
int netcfg_stop(int type);
```

## 完整API参考

### 1. WiFi管理API

#### tal_wifi_set_work_mode()
```c
/**
 * @brief 设置WiFi工作模式
 * 
 * @param mode 工作模式 (WWM_STATION/WWM_SOFTAP/WWM_STATIONAP)
 * @return OPRT_OK 成功，其他值表示错误
 */
OPERATE_RET tal_wifi_set_work_mode(WF_WK_MD_E mode);
```

#### tal_wifi_ap_start()
```c
/**
 * @brief 启动WiFi AP
 * 
 * @param cfg AP配置参数
 * @return OPRT_OK 成功，其他值表示错误
 */
OPERATE_RET tal_wifi_ap_start(WF_AP_CFG_IF_S *cfg);
```

#### tal_wifi_ap_stop()
```c
/**
 * @brief 停止WiFi AP
 * 
 * @return OPRT_OK 成功，其他值表示错误
 */
OPERATE_RET tal_wifi_ap_stop(void);
```

### 2. 网络通信API

#### tal_net_socket_create()
```c
/**
 * @brief 创建网络套接字
 * 
 * @param type 协议类型 (PROTOCOL_TCP/PROTOCOL_UDP)
 * @return 套接字描述符，失败返回负值
 */
int tal_net_socket_create(TUYA_PROTOCOL_TYPE_E type);
```

#### tal_net_send_to()
```c
/**
 * @brief UDP发送数据
 * 
 * @param fd 套接字描述符
 * @param buf 发送缓冲区
 * @param nbytes 发送字节数
 * @param addr 目标地址
 * @param port 目标端口
 * @return 实际发送字节数，失败返回负值
 */
int tal_net_send_to(const int fd, const void *buf, const size_t nbytes, 
                   const TUYA_IP_ADDR_T addr, const uint16_t port);
```

### 3. BLE管理API

#### tuya_ble_init()
```c
/**
 * @brief 初始化BLE管理器
 * 
 * @param cfg BLE配置参数
 * @return OPRT_OK 成功，其他值表示错误
 */
int tuya_ble_init(tuya_ble_cfg_t *cfg);
```

#### ble_channel_add()
```c
/**
 * @brief 添加BLE通道
 * 
 * @param type 通道类型
 * @param fn 通道处理函数
 * @param priv_data 私有数据
 * @return OPRT_OK 成功，其他值表示错误
 */
int ble_channel_add(ble_channel_type_t type, ble_channel_fn_t fn, void *priv_data);
```

#### tuya_ble_send()
```c
/**
 * @brief 发送BLE数据
 * 
 * @param type 数据类型
 * @param ack_sn 确认序号
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return OPRT_OK 成功，其他值表示错误
 */
int tuya_ble_send(uint16_t type, uint32_t ack_sn, uint8_t *data, uint32_t len);
```

## 实际应用示例

### 1. 同时启用WiFi和BLE配网

```c
#include "netmgr.h"
#include "netconn_wifi.h"

// 配网完成处理
int netcfg_finish_handler(int type, netcfg_info_t *info)
{
    printf("配网完成，类型: %s\n", 
           (type == NETCFG_TUYA_WIFI_AP) ? "WiFi AP" : "BLE");
    printf("SSID: %.*s\n", info->s_len, info->ssid);
    printf("密码: %.*s\n", info->p_len, info->passwd);
    printf("Token: %.*s\n", info->t_len, info->token);
    
    // 停止所有配网
    netcfg_stop(NETCFG_STOP_ALL_CFG_MODULE);
    
    return OPRT_OK;
}

void start_dual_netcfg(const char *uuid)
{
    // 1. 配置参数
    netcfg_args_t netcfg_args = {
        .type = NETCFG_TUYA_WIFI_AP | NETCFG_TUYA_BLE,
        .uuid = (char *)uuid,
        .pincode = NULL  // 不使用PIN码
    };
    
    // 2. 初始化AP配网
    int ret = ap_netcfg_init(&netcfg_args);
    if (ret != OPRT_OK) {
        printf("AP配网初始化失败: %d\n", ret);
        return;
    }
    
    // 3. 初始化BLE配网  
    ret = ble_netcfg_init(&netcfg_args);
    if (ret != OPRT_OK) {
        printf("BLE配网初始化失败: %d\n", ret);
        return;
    }
    
    // 4. 启动配网
    ret = netcfg_start(NETCFG_TUYA_WIFI_AP, netcfg_finish_handler, NULL);
    if (ret != OPRT_OK) {
        printf("启动AP配网失败: %d\n", ret);
        return;
    }
    
    ret = netcfg_start(NETCFG_TUYA_BLE, netcfg_finish_handler, NULL);
    if (ret != OPRT_OK) {
        printf("启动BLE配网失败: %d\n", ret);
        return;
    }
    
    printf("双模配网启动成功\n");
    printf("AP热点名称: SmartLife-XXXX\n");
    printf("BLE设备名称: TYBLE\n");
}
```

### 2. 仅WiFi AP配网

```c
void start_ap_only_netcfg(const char *uuid)
{
    netcfg_args_t netcfg_args = {
        .type = NETCFG_TUYA_WIFI_AP,
        .uuid = (char *)uuid,
        .pincode = NULL
    };
    
    int ret = ap_netcfg_init(&netcfg_args);
    if (ret == OPRT_OK) {
        ret = netcfg_start(NETCFG_TUYA_WIFI_AP, netcfg_finish_handler, NULL);
        if (ret == OPRT_OK) {
            printf("WiFi AP配网启动成功\n");
        }
    }
}
```

### 3. 仅BLE配网

```c
void start_ble_only_netcfg(const char *uuid)
{
    netcfg_args_t netcfg_args = {
        .type = NETCFG_TUYA_BLE,
        .uuid = (char *)uuid,
        .pincode = NULL
    };
    
    int ret = ble_netcfg_init(&netcfg_args);
    if (ret == OPRT_OK) {
        ret = netcfg_start(NETCFG_TUYA_BLE, netcfg_finish_handler, NULL);
        if (ret == OPRT_OK) {
            printf("BLE配网启动成功\n");
        }
    }
}
```

### 4. 配网状态监控

```c
// 配网状态枚举
typedef enum {
    NETCFG_STATE_IDLE,
    NETCFG_STATE_AP_RUNNING,
    NETCFG_STATE_BLE_RUNNING,
    NETCFG_STATE_FINISHED,
    NETCFG_STATE_FAILED
} netcfg_state_e;

static netcfg_state_e g_netcfg_state = NETCFG_STATE_IDLE;

// 配网状态变更处理
void netcfg_state_change(netcfg_state_e new_state)
{
    if (g_netcfg_state != new_state) {
        printf("配网状态变更: %d -> %d\n", g_netcfg_state, new_state);
        g_netcfg_state = new_state;
        
        switch (new_state) {
        case NETCFG_STATE_AP_RUNNING:
            // LED蓝色快闪 - AP配网中
            break;
        case NETCFG_STATE_BLE_RUNNING:
            // LED紫色快闪 - BLE配网中  
            break;
        case NETCFG_STATE_FINISHED:
            // LED绿色常亮 - 配网成功
            break;
        case NETCFG_STATE_FAILED:
            // LED红色闪烁 - 配网失败
            break;
        default:
            break;
        }
    }
}

// 增强的配网完成处理
int enhanced_netcfg_finish_handler(int type, netcfg_info_t *info)
{
    printf("========== 配网成功 ==========\n");
    printf("配网方式: %s\n", 
           (type == NETCFG_TUYA_WIFI_AP) ? "WiFi热点配网" : "蓝牙配网");
    printf("WiFi名称: %.*s\n", info->s_len, info->ssid);
    printf("WiFi密码: %.*s\n", info->p_len, info->passwd);
    printf("绑定Token: %.*s\n", info->t_len, info->token);
    printf("=============================\n");
    
    // 更新状态
    netcfg_state_change(NETCFG_STATE_FINISHED);
    
    // 停止所有配网
    netcfg_stop(NETCFG_STOP_ALL_CFG_MODULE);
    
    return OPRT_OK;
}
```

## 总结

TuyaOpen提供了完整的WiFi和蓝牙配网解决方案：

**WiFi AP配网特点：**
- 设备开启热点，无需路由器支持
- 支持TLS+PSK安全通信
- 自动广播设备信息
- 适合大部分用户场景

**BLE配网特点：**
- 功耗更低，响应更快
- 适合低功耗设备
- 支持加密通信
- 配置过程更简洁

**最佳实践：**
1. **同时启用两种配网**，给用户更多选择
2. **合理设置超时时间**，避免长时间占用资源
3. **实现状态指示**，提供清晰的用户反馈
4. **错误处理完善**，确保配网流程健壮性

通过本指南提供的API和示例代码，开发者可以根据实际需求选择合适的配网方式，快速实现设备的网络配置功能。 