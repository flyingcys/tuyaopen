#pragma once

#include "mimi_base.h"

OPERATE_RET wifi_manager_init(void);
OPERATE_RET wifi_manager_start(void);
OPERATE_RET wifi_manager_wait_connected(uint32_t timeout_ms);
bool wifi_manager_is_connected(void);
const char *wifi_manager_get_ip(void);
const char *wifi_manager_get_target_ssid(void);
OPERATE_RET wifi_manager_set_credentials(const char *ssid, const char *password);
void wifi_manager_scan_and_print(void);
