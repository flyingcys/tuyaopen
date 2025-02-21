#ifndef BLUETOOTH_API_H__
#define BLUETOOTH_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tkl_bluetooth.h"

typedef struct {
    char name[16];
    char bd_addr[6];
    int8_t rssi;
} ble_device_t;

// SD_BUS
OPERATE_RET sd_bus_init(void);
OPERATE_RET sd_bus_loop_start(void);
OPERATE_RET sd_bus_deinit(void);
OPERATE_RET sd_bus_gatts_service_add(TKL_BLE_GATTS_PARAMS_T *p_service);
OPERATE_RET sd_bus_gatts_value_set(uint16_t conn_handle, uint16_t char_handle, uint8_t *p_data, uint16_t length);
OPERATE_RET sd_bus_gatts_value_notify(uint16_t conn_handle, uint16_t char_handle, uint8_t *p_data, uint16_t length);
OPERATE_RET sd_bus_gatt_callback_register(const TKL_BLE_GATT_EVT_FUNC_CB gatt_evt);

// HCI
OPERATE_RET hci_dev_up(void);
OPERATE_RET hci_dev_down(void);
OPERATE_RET hci_dev_get_addr(uint8_t *addr);
OPERATE_RET hci_dev_set_scan_enable(uint8_t enable);
OPERATE_RET hci_dev_set_advertise_enable(uint8_t enable);
OPERATE_RET hci_dev_set_adv_parameters(TKL_BLE_GAP_ADV_PARAMS_T const *p_adv_params);
OPERATE_RET hci_dev_set_adv_data(TKL_BLE_DATA_T const *p_adv);
OPERATE_RET hci_dev_set_scan_parameters(TKL_BLE_GAP_SCAN_PARAMS_T const *p_scan_params);
OPERATE_RET hci_dev_set_scan_rsp_data(TKL_BLE_DATA_T const *p_scan_rsp);
OPERATE_RET hci_dev_create_conn(TKL_BLE_GAP_ADDR_T const *p_peer_addr, TKL_BLE_GAP_SCAN_PARAMS_T const *p_scan_params, TKL_BLE_GAP_CONN_PARAMS_T const *p_conn_params);
OPERATE_RET hci_dev_disconnect(uint16_t conn_handle, uint8_t hci_reason);
OPERATE_RET hci_dev_conn_update(uint16_t conn_handle, TKL_BLE_GAP_CONN_PARAMS_T const *p_conn_params);
OPERATE_RET hci_dev_read_rssi(uint16_t handle);
OPERATE_RET hci_dev_gap_callback_register(const TKL_BLE_GAP_EVT_FUNC_CB gap_evt);

#define IF_FAIL_RETURN(x)       \
    do {                        \
        if ((op_ret = x) < 0) { \
            return op_ret;      \
        }                       \
    } while (0)

#define IF_FAIL_RETURN_FREE(x, y) \
    do {                          \
        if ((op_ret = x) < 0) {   \
            free(y);              \
            y = NULL;             \
            return op_ret;        \
        }                         \
    } while (0)

#define IF_FAIL_RETURN_INFO(x, info) \
    do {                             \
        if ((op_ret = x) < 0) {      \
            printf(info);            \
            return op_ret;           \
        }                            \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
