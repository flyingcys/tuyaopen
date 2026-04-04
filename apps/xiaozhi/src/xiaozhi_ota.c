/**
 * @file xiaozhi_ota.c
 * @brief OTA bootstrap implementation aligned with xiaozhi-esp32 flow.
 */

#include "xiaozhi_ota.h"

#include "cJSON.h"
#include "http_client_interface.h"
#include "iotdns.h"
#include "netmgr.h"
#include "tal_api.h"
#include "tkl_flash.h"
#include "xiaozhi_settings.h"
#include "xiaozhi_system.h"

#if defined(ESP_PLATFORM)
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_flash.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#ifdef SOC_HMAC_SUPPORTED
#include "esp_hmac.h"
#endif
#endif

#include "mbedtls/md.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tal_wifi.h"
#endif

#ifndef PROJECT_NAME
#define PROJECT_NAME "xiaozhi"
#endif

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#ifndef PLATFORM_BOARD
#define PLATFORM_BOARD "unknown"
#endif

#ifndef PLATFORM_CHIP
#define PLATFORM_CHIP "unknown"
#endif

static void xz_env_override(char *buf, size_t buf_size, const char *env_key)
{
    if (!buf || buf_size == 0 || !env_key) {
        return;
    }

    const char *val = getenv(env_key);
    if (val && val[0] != '\0') {
        (void)snprintf(buf, buf_size, "%s", val);
    }
}

static int xz_env_int(const char *env_key, int default_value)
{
    if (!env_key) {
        return default_value;
    }

    const char *val = getenv(env_key);
    if (!val || val[0] == '\0') {
        return default_value;
    }

    return atoi(val);
}

static int xz_detect_flash_size_from_tkl(void)
{
    uint32_t max_end = 0;

    for (int type = 0; type < (int)TUYA_FLASH_TYPE_MAX; ++type) {
        TUYA_FLASH_BASE_INFO_T info = {0};
        if (tkl_flash_get_one_type_info((TUYA_FLASH_TYPE_E)type, &info) != OPRT_OK) {
            continue;
        }

        for (uint32_t i = 0; i < info.partition_num && i < TUYA_FLASH_TYPE_MAX_PARTITION_NUM; ++i) {
            uint32_t start = info.partition[i].start_addr;
            uint32_t size  = info.partition[i].size;
            if (size == 0) {
                continue;
            }
            uint32_t end = start + size;
            if (end > max_end) {
                max_end = end;
            }
        }
    }

    return (int)max_end;
}

static cJSON *xz_load_partition_table_from_tkl(void)
{
    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        return NULL;
    }

    uint32_t added = 0;
    for (int type = 0; type < (int)TUYA_FLASH_TYPE_MAX; ++type) {
        TUYA_FLASH_BASE_INFO_T info = {0};
        if (tkl_flash_get_one_type_info((TUYA_FLASH_TYPE_E)type, &info) != OPRT_OK) {
            continue;
        }

        for (uint32_t i = 0; i < info.partition_num && i < TUYA_FLASH_TYPE_MAX_PARTITION_NUM; ++i) {
            uint32_t size = info.partition[i].size;
            if (size == 0) {
                continue;
            }

            cJSON *item = cJSON_CreateObject();
            if (!item) {
                cJSON_Delete(arr);
                return NULL;
            }

            char label[32] = {0};
            (void)snprintf(label, sizeof(label), "type_%d_%u", type, (unsigned)i);
            cJSON_AddStringToObject(item, "label", label);
            cJSON_AddNumberToObject(item, "type", type);
            cJSON_AddNumberToObject(item, "subtype", (int)i);
            cJSON_AddNumberToObject(item, "address", (double)info.partition[i].start_addr);
            cJSON_AddNumberToObject(item, "size", (double)size);
            cJSON_AddItemToArray(arr, item);
            added++;
        }
    }

    if (added == 0) {
        cJSON_Delete(arr);
        return NULL;
    }

    return arr;
}

#if defined(ESP_PLATFORM)
static void xz_fill_esp_runtime_fields(int *flash_size, int *chip_model, int *chip_revision, int *chip_features,
                                       char *idf_version, size_t idf_version_size, char *elf_sha256,
                                       size_t elf_sha256_size, char *ota_label, size_t ota_label_size)
{
    if (flash_size) {
        uint32_t size = 0;
        if (esp_flash_get_size(NULL, &size) == ESP_OK) {
            *flash_size = (int)size;
        }
    }

    esp_chip_info_t chip = {0};
    esp_chip_info(&chip);
    if (chip_model) {
        *chip_model = chip.model;
    }
    if (chip_revision) {
        *chip_revision = chip.revision;
    }
    if (chip_features) {
        *chip_features = chip.features;
    }

    const esp_app_desc_t *app_desc = esp_app_get_description();
    if (app_desc) {
        if (idf_version && idf_version_size > 0) {
            (void)snprintf(idf_version, idf_version_size, "%s", app_desc->idf_ver);
        }
        if (elf_sha256 && elf_sha256_size > 0) {
            elf_sha256[0] = '\0';
            for (int i = 0; i < 32 && (size_t)(i * 2 + 1) < elf_sha256_size; ++i) {
                (void)snprintf(elf_sha256 + i * 2, elf_sha256_size - (size_t)i * 2, "%02x",
                               app_desc->app_elf_sha256[i]);
            }
        }
    }

    if (ota_label && ota_label_size > 0) {
        const esp_partition_t *part = esp_ota_get_running_partition();
        if (part) {
            (void)snprintf(ota_label, ota_label_size, "%s", part->label);
        }
    }
}

static cJSON *xz_load_partition_table_from_esp(void)
{
    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        return NULL;
    }

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (!it) {
        cJSON_Delete(arr);
        return NULL;
    }

    while (it) {
        const esp_partition_t   *part = esp_partition_get(it);
        esp_partition_iterator_t next = esp_partition_next(it);

        if (!part) {
            it = next;
            continue;
        }

        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(arr);
            return NULL;
        }

        cJSON_AddStringToObject(item, "label", part->label);
        cJSON_AddNumberToObject(item, "type", (int)part->type);
        cJSON_AddNumberToObject(item, "subtype", (int)part->subtype);
        cJSON_AddNumberToObject(item, "address", (double)part->address);
        cJSON_AddNumberToObject(item, "size", (double)part->size);
        cJSON_AddItemToArray(arr, item);

        it = next;
    }

    return arr;
}
#endif

static cJSON *xz_load_partition_table_json(void)
{
    const char *raw = getenv("XZ_PARTITION_TABLE_JSON");
    if (raw && raw[0] != '\0') {
        cJSON *arr = cJSON_Parse(raw);
        if (cJSON_IsArray(arr)) {
            return arr;
        }
        if (arr) {
            cJSON_Delete(arr);
        }
    }

#if defined(ESP_PLATFORM)
    cJSON *esp_arr = xz_load_partition_table_from_esp();
    if (esp_arr) {
        return esp_arr;
    }
#endif

    cJSON *tkl_arr = xz_load_partition_table_from_tkl();
    if (tkl_arr) {
        return tkl_arr;
    }

    cJSON *arr  = cJSON_CreateArray();
    cJSON *item = cJSON_CreateObject();
    if (!arr || !item) {
        if (arr) {
            cJSON_Delete(arr);
        }
        if (item) {
            cJSON_Delete(item);
        }
        return NULL;
    }
    cJSON_AddStringToObject(item, "label", "ota_0");
    cJSON_AddNumberToObject(item, "type", 0);
    cJSON_AddNumberToObject(item, "subtype", 0);
    cJSON_AddNumberToObject(item, "address", 0);
    cJSON_AddNumberToObject(item, "size", 0);
    cJSON_AddItemToArray(arr, item);
    return arr;
}

static void xz_load_http_common(char *device_id, size_t device_id_size, char *client_id, size_t client_id_size,
                                char *user_agent, size_t user_agent_size, char *accept_language,
                                size_t accept_language_size)
{
    if (device_id && device_id_size > 0) {
        (void)xiaozhi_system_get_device_id(device_id, device_id_size);
    }
    if (client_id && client_id_size > 0) {
        (void)xiaozhi_system_get_client_id(client_id, client_id_size);
    }
    if (user_agent && user_agent_size > 0) {
        if (xiaozhi_system_get_user_agent(user_agent, user_agent_size) != OPRT_OK || user_agent[0] == '\0') {
            (void)snprintf(user_agent, user_agent_size, "%s/%s", PLATFORM_BOARD, PROJECT_VERSION);
        }
        xz_env_override(user_agent, user_agent_size, "XZ_USER_AGENT");
    }
    if (accept_language && accept_language_size > 0) {
        (void)snprintf(accept_language, accept_language_size, "%s", "zh-CN");
        xz_env_override(accept_language, accept_language_size, "XZ_ACCEPT_LANGUAGE");
    }
}

static void xz_load_serial_number(char *serial, size_t serial_size)
{
    if (!serial || serial_size == 0) {
        return;
    }

    serial[0] = '\0';
    (void)xiaozhi_settings_get_string(XZ_NS_SYS, "serial_number", serial, serial_size, "");

#if defined(ESP_PLATFORM) && defined(ESP_EFUSE_BLOCK_USR_DATA)
    if (serial[0] == '\0') {
        uint8_t efuse_serial[33] = {0};
        if (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, efuse_serial, 32 * 8) == ESP_OK) {
            if (efuse_serial[0] != 0) {
                (void)snprintf(serial, serial_size, "%.*s", 32, (const char *)efuse_serial);
            }
        }
    }
#endif
}

static void xz_fill_chip_runtime_info(char *chip_model_name, size_t chip_model_name_size, int *chip_cores)
{
    if (chip_cores) {
        *chip_cores = 0;
    }
    if (!chip_model_name || chip_model_name_size == 0) {
        return;
    }

    TUYA_CPU_INFO_T *cpu_ary = NULL;
    int32_t          cpu_cnt = 0;
    if (tal_system_get_cpu_info(&cpu_ary, &cpu_cnt) != OPRT_OK || !cpu_ary || cpu_cnt <= 0) {
        return;
    }

    if (chip_cores) {
        *chip_cores = cpu_cnt;
    }
    if (cpu_ary[0].chipidlen > 0) {
        (void)snprintf(chip_model_name, chip_model_name_size, "%.*s", (int)cpu_ary[0].chipidlen,
                       (const char *)cpu_ary[0].chipid);
    }
}

static BOOL_T xz_get_wifi_rssi(int *rssi_out)
{
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    if (!rssi_out) {
        return FALSE;
    }

    int8_t rssi = 0;
    if (tal_wifi_station_get_conn_ap_rssi(&rssi) != OPRT_OK) {
        return FALSE;
    }

    *rssi_out = (int)rssi;
    return TRUE;
#else
    (void)rssi_out;
    return FALSE;
#endif
}

static uint32_t xz_get_minimum_free_heap_size(void)
{
#if defined(ESP_PLATFORM)
    return (uint32_t)esp_get_minimum_free_heap_size();
#else
    return (uint32_t)tal_system_get_free_heap_size();
#endif
}

static OPERATE_RET xz_parse_http_url(const char *url, BOOL_T *tls, char *host, size_t host_size, uint16_t *port,
                                     char *path, size_t path_size)
{
    if (!url || !tls || !host || host_size == 0 || !port || !path || path_size == 0) {
        return OPRT_INVALID_PARM;
    }

    const char *p            = NULL;
    uint16_t    default_port = 0;

    if (strncmp(url, "https://", 8) == 0) {
        *tls         = TRUE;
        p            = url + 8;
        default_port = 443;
    } else if (strncmp(url, "http://", 7) == 0) {
        *tls         = FALSE;
        p            = url + 7;
        default_port = 80;
    } else {
        return OPRT_INVALID_PARM;
    }

    const char *host_begin = p;
    while (*p && *p != '/' && *p != '?') {
        p++;
    }

    const char *host_end = p;
    const char *colon    = NULL;
    for (const char *q = host_begin; q < host_end; ++q) {
        if (*q == ':') {
            colon = q;
            break;
        }
    }

    if (colon) {
        size_t host_len = (size_t)(colon - host_begin);
        if (host_len == 0 || host_len >= host_size) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        memcpy(host, host_begin, host_len);
        host[host_len] = '\0';

        int pnum = atoi(colon + 1);
        if (pnum <= 0 || pnum > 65535) {
            return OPRT_INVALID_PARM;
        }
        *port = (uint16_t)pnum;
    } else {
        size_t host_len = (size_t)(host_end - host_begin);
        if (host_len == 0 || host_len >= host_size) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        memcpy(host, host_begin, host_len);
        host[host_len] = '\0';
        *port          = default_port;
    }

    if (*p == '\0') {
        (void)snprintf(path, path_size, "/");
    } else {
        (void)snprintf(path, path_size, "%s", p);
    }

    return OPRT_OK;
}

static char *xz_make_activate_url(const char *base_url)
{
    if (!base_url || base_url[0] == '\0') {
        return NULL;
    }

    size_t      n      = strlen(base_url);
    const char *suffix = (base_url[n - 1] == '/') ? "activate" : "/activate";

    size_t need = n + strlen(suffix) + 1;
    char  *out  = tal_malloc(need);
    if (!out) {
        return NULL;
    }

    (void)snprintf(out, need, "%s%s", base_url, suffix);
    return out;
}

static OPERATE_RET xz_apply_config_object(const cJSON *obj, const char *ns)
{
    if (!obj || !ns) {
        return OPRT_INVALID_PARM;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, (cJSON *)obj)
    {
        if (!item->string || item->string[0] == '\0') {
            continue;
        }

        if (cJSON_IsString(item) && item->valuestring) {
            (void)xiaozhi_settings_set_string(ns, item->string, item->valuestring);
        } else if (cJSON_IsNumber(item)) {
            (void)xiaozhi_settings_set_int(ns, item->string, item->valueint);
        } else if (cJSON_IsBool(item)) {
            (void)xiaozhi_settings_set_int(ns, item->string, cJSON_IsTrue(item) ? 1 : 0);
        }
    }

    return OPRT_OK;
}

static char *xz_build_check_payload(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    char app_name[64]     = {0};
    char app_version[32]  = {0};
    char app_chip[32]     = {0};
    char app_board[64]    = {0};
    char language[16]     = {0};
    char device_id[32]    = {0};
    char client_id[96]    = {0};
    char min_heap[32]     = {0};
    char ssid[64]         = {0};
    char ip_str[16]       = {0};
    int  chip_cores       = 0;
    int  flash_size       = 0;
    int  wifi_channel     = 0;
    int  wifi_rssi        = 0;
    int  chip_model       = 0;
    int  chip_revision    = 0;
    int  chip_features    = 0;
    char compile_time[64] = {0};
    char idf_version[64]  = {0};
    char elf_sha256[128]  = {0};
    char ota_label[32]    = {0};

    (void)snprintf(app_name, sizeof(app_name), "%s", PROJECT_NAME);
    (void)snprintf(app_version, sizeof(app_version), "%s", PROJECT_VERSION);
    (void)snprintf(app_chip, sizeof(app_chip), "%s", PLATFORM_CHIP);
    (void)snprintf(app_board, sizeof(app_board), "%s", PLATFORM_BOARD);
    (void)snprintf(language, sizeof(language), "%s", "zh-CN");
    (void)snprintf(compile_time, sizeof(compile_time), "%s", __DATE__ "T" __TIME__ "Z");
    (void)snprintf(idf_version, sizeof(idf_version), "%s", "tuyaopen");
    (void)snprintf(ota_label, sizeof(ota_label), "%s", "ota_0");
    (void)snprintf(min_heap, sizeof(min_heap), "%u", (unsigned)xz_get_minimum_free_heap_size());
    xz_fill_chip_runtime_info(app_chip, sizeof(app_chip), &chip_cores);
#if defined(ESP_PLATFORM)
    xz_fill_esp_runtime_fields(&flash_size, &chip_model, &chip_revision, &chip_features, idf_version,
                               sizeof(idf_version), elf_sha256, sizeof(elf_sha256), ota_label, sizeof(ota_label));
#else
    flash_size = xz_detect_flash_size_from_tkl();
#endif
    flash_size    = xz_env_int("XZ_OTA_FLASH_SIZE", flash_size);
    wifi_channel  = xz_env_int("XZ_WIFI_CHANNEL", 0);
    chip_model    = xz_env_int("XZ_CHIP_MODEL", chip_model);
    chip_revision = xz_env_int("XZ_CHIP_REVISION", chip_revision);
    chip_features = xz_env_int("XZ_CHIP_FEATURES", chip_features);
    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ssid", ssid, sizeof(ssid), "");
    if (xz_get_wifi_rssi(&wifi_rssi) == FALSE) {
        wifi_rssi = 0;
    }
    NW_IP_S ip = {0};
    if (netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_IP, &ip) == OPRT_OK) {
        (void)snprintf(ip_str, sizeof(ip_str), "%s", ip.ip);
    }

    xz_env_override(app_name, sizeof(app_name), "XZ_OTA_APP_NAME");
    xz_env_override(app_version, sizeof(app_version), "XZ_OTA_APP_VERSION");
    xz_env_override(app_chip, sizeof(app_chip), "XZ_OTA_APP_CHIP");
    xz_env_override(app_board, sizeof(app_board), "XZ_OTA_APP_BOARD");
    xz_env_override(language, sizeof(language), "XZ_ACCEPT_LANGUAGE");
    xz_env_override(compile_time, sizeof(compile_time), "XZ_APP_COMPILE_TIME");
    xz_env_override(idf_version, sizeof(idf_version), "XZ_APP_IDF_VERSION");
    xz_env_override(elf_sha256, sizeof(elf_sha256), "XZ_APP_ELF_SHA256");
    xz_env_override(ota_label, sizeof(ota_label), "XZ_OTA_LABEL");

    (void)xiaozhi_system_get_device_id(device_id, sizeof(device_id));
    (void)xiaozhi_system_get_client_id(client_id, sizeof(client_id));

    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddStringToObject(root, "language", language);
    cJSON_AddNumberToObject(root, "flash_size", flash_size);
    cJSON_AddStringToObject(root, "minimum_free_heap_size", min_heap);
    cJSON_AddStringToObject(root, "mac_address", device_id);
    cJSON_AddStringToObject(root, "uuid", client_id);
    cJSON_AddStringToObject(root, "chip_model_name", app_chip);

    cJSON *chip_info = cJSON_CreateObject();
    if (!chip_info) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddNumberToObject(chip_info, "model", chip_model);
    cJSON_AddNumberToObject(chip_info, "cores", chip_cores);
    cJSON_AddNumberToObject(chip_info, "revision", chip_revision);
    cJSON_AddNumberToObject(chip_info, "features", chip_features);
    cJSON_AddItemToObject(root, "chip_info", chip_info);

    cJSON *app = cJSON_CreateObject();
    if (!app) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddStringToObject(app, "name", app_name);
    cJSON_AddStringToObject(app, "version", app_version);
    cJSON_AddStringToObject(app, "compile_time", compile_time);
    cJSON_AddStringToObject(app, "idf_version", idf_version);
    cJSON_AddStringToObject(app, "elf_sha256", elf_sha256);
    cJSON_AddItemToObject(root, "application", app);

    cJSON *partition_table = xz_load_partition_table_json();
    if (!partition_table) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "partition_table", partition_table);

    cJSON *ota = cJSON_CreateObject();
    if (!ota) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddStringToObject(ota, "label", ota_label);
    cJSON_AddItemToObject(root, "ota", ota);

    cJSON *display = cJSON_CreateObject();
    if (!display) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddBoolToObject(display, "monochrome", 0);
    cJSON_AddNumberToObject(display, "width", 0);
    cJSON_AddNumberToObject(display, "height", 0);
    cJSON_AddItemToObject(root, "display", display);

    cJSON *board = cJSON_CreateObject();
    if (!board) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddStringToObject(board, "type", app_board);
    cJSON_AddStringToObject(board, "name", app_board);
    cJSON_AddStringToObject(board, "ssid", ssid);
    cJSON_AddNumberToObject(board, "rssi", wifi_rssi);
    cJSON_AddNumberToObject(board, "channel", wifi_channel);
    cJSON_AddStringToObject(board, "ip", ip_str);
    cJSON_AddStringToObject(board, "mac", device_id);
    cJSON_AddItemToObject(root, "board", board);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static OPERATE_RET xz_http_request_json(const char *url, const char *method, http_client_header_t *headers,
                                        uint8_t headers_count, const uint8_t *body, size_t body_len, cJSON **json_out,
                                        uint16_t *http_status_out)
{
    if (!url || !method || !json_out || !http_status_out) {
        return OPRT_INVALID_PARM;
    }

    *json_out        = NULL;
    *http_status_out = 0;

    BOOL_T   use_tls   = FALSE;
    char     host[160] = {0};
    uint16_t port      = 0;
    char     path[320] = {0};

    OPERATE_RET rt = xz_parse_http_url(url, &use_tls, host, sizeof(host), &port, path, sizeof(path));
    if (rt != OPRT_OK) {
        return rt;
    }

    uint8_t *cacert     = NULL;
    uint16_t cacert_len = 0;
    BOOL_T   no_verify  = FALSE;
    if (use_tls) {
        if (tuya_iotdns_query_domain_certs(host, &cacert, &cacert_len) != OPRT_OK || !cacert || cacert_len == 0) {
            no_verify = TRUE;
        }
    }

    PR_NOTICE("ota http transport: host=%s port=%u tls=%d no_verify=%d cacert_len=%u", host, port, use_tls, no_verify,
              (unsigned)cacert_len);

    http_client_response_t response = {0};
    http_client_request_t  req      = {
              .host          = host,
              .port          = port,
              .path          = path,
              .cacert        = cacert,
              .cacert_len    = cacert_len,
              .tls_no_verify = no_verify ? true : false,
              .method        = method,
              .headers       = headers,
              .headers_count = headers_count,
              .body          = body,
              .body_length   = body_len,
              .timeout_ms    = 15000,
    };

    http_client_status_t hs = http_client_request(&req, &response);
    if (cacert) {
        tal_free(cacert);
    }

    if (hs != HTTP_CLIENT_SUCCESS) {
        PR_ERR("ota http request failed: host=%s port=%u path=%s method=%s status=%d", host, port, path, method, hs);
        (void)http_client_free(&response);
        return OPRT_COM_ERROR;
    }

    *http_status_out = response.status_code;
    if (response.body && response.body_length > 0) {
        *json_out = cJSON_ParseWithLength((const char *)response.body, response.body_length);
    }

    if (*http_status_out != 200) {
        size_t preview_len = response.body_length > 256 ? 256 : response.body_length;
        PR_WARN("ota http status=%u body=%.*s", *http_status_out, (int)preview_len,
                response.body ? (const char *)response.body : "");
    }

    (void)http_client_free(&response);
    return OPRT_OK;
}

static OPERATE_RET xz_make_activation_hmac(const char *secret, const char *challenge, char *out, size_t out_size)
{
    if (!secret || !challenge || !out || out_size < 65) {
        return OPRT_INVALID_PARM;
    }

    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md_info) {
        return OPRT_COM_ERROR;
    }

    unsigned char digest[32] = {0};
    int ret = mbedtls_md_hmac(md_info, (const unsigned char *)secret, strlen(secret), (const unsigned char *)challenge,
                              strlen(challenge), digest);
    if (ret != 0) {
        return OPRT_COM_ERROR;
    }

    for (size_t i = 0; i < sizeof(digest); ++i) {
        (void)snprintf(out + i * 2, out_size - i * 2, "%02x", digest[i]);
    }
    out[64] = '\0';

    return OPRT_OK;
}

OPERATE_RET xiaozhi_ota_check_and_apply(xz_ota_result_t *result)
{
    if (!result) {
        return OPRT_INVALID_PARM;
    }

    memset(result, 0, sizeof(*result));

    char ota_url[256] = {0};
    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ota_url", ota_url, sizeof(ota_url), XZ_DEFAULT_OTA_URL);
    if (ota_url[0] == '\0') {
        (void)snprintf(ota_url, sizeof(ota_url), "%s", XZ_DEFAULT_OTA_URL);
    }
    xz_env_override(ota_url, sizeof(ota_url), "XZ_OTA_URL");

    char device_id[32]       = {0};
    char client_id[96]       = {0};
    char user_agent[128]     = {0};
    char accept_language[32] = {0};
    xz_load_http_common(device_id, sizeof(device_id), client_id, sizeof(client_id), user_agent, sizeof(user_agent),
                        accept_language, sizeof(accept_language));

    char serial[96] = {0};
    xz_load_serial_number(serial, sizeof(serial));
    xz_env_override(serial, sizeof(serial), "XZ_ACT_SERIAL");
    const char *activation_version = (serial[0] != '\0') ? "2" : "1";

    http_client_header_t headers[8]    = {0};
    uint8_t              headers_count = 0;
    headers[headers_count++] = (http_client_header_t){.key = "Activation-Version", .value = activation_version};
    headers[headers_count++] = (http_client_header_t){.key = "Device-Id", .value = device_id};
    headers[headers_count++] = (http_client_header_t){.key = "Client-Id", .value = client_id};
    if (serial[0] != '\0') {
        headers[headers_count++] = (http_client_header_t){.key = "Serial-Number", .value = serial};
    }
    headers[headers_count++] = (http_client_header_t){.key = "User-Agent", .value = user_agent};
    headers[headers_count++] = (http_client_header_t){.key = "Accept-Language", .value = accept_language};
    headers[headers_count++] = (http_client_header_t){.key = "Content-Type", .value = "application/json"};

    PR_NOTICE("ota check request: url=%s device_id=%s client_id=%s activation_version=%s", ota_url, device_id,
              client_id, activation_version);

    char *payload = xz_build_check_payload();
    if (!payload) {
        return OPRT_MALLOC_FAILED;
    }

    PR_NOTICE("ota check payload=%s", payload);

    cJSON      *root   = NULL;
    uint16_t    status = 0;
    OPERATE_RET rt     = xz_http_request_json(ota_url, "POST", headers, headers_count, (const uint8_t *)payload,
                                              strlen(payload), &root, &status);
    cJSON_free(payload);
    if (rt != OPRT_OK) {
        return rt;
    }

    if (status != 200 || !root) {
        if (root) {
            cJSON_Delete(root);
            root = NULL;
        }

        PR_WARN("ota check primary request failed(status=%u), retry with empty body", status);
        status = 0;
        rt     = xz_http_request_json(ota_url, "POST", headers, headers_count, NULL, 0, &root, &status);
        if (rt != OPRT_OK || status != 200 || !root) {
            if (root) {
                cJSON_Delete(root);
            }
            return OPRT_COM_ERROR;
        }
    }

    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (cJSON_IsObject(activation)) {
        cJSON *message    = cJSON_GetObjectItem(activation, "message");
        cJSON *code       = cJSON_GetObjectItem(activation, "code");
        cJSON *challenge  = cJSON_GetObjectItem(activation, "challenge");
        cJSON *timeout_ms = cJSON_GetObjectItem(activation, "timeout_ms");

        if (cJSON_IsString(message) && message->valuestring) {
            (void)snprintf(result->activation_message, sizeof(result->activation_message), "%s", message->valuestring);
        }
        if (cJSON_IsString(code) && code->valuestring) {
            result->has_activation_code = TRUE;
            (void)snprintf(result->activation_code, sizeof(result->activation_code), "%s", code->valuestring);
        }
        if (cJSON_IsString(challenge) && challenge->valuestring) {
            result->has_activation_challenge = TRUE;
            (void)snprintf(result->activation_challenge, sizeof(result->activation_challenge), "%s",
                           challenge->valuestring);
        }
        if (cJSON_IsNumber(timeout_ms)) {
            result->activation_timeout_ms = timeout_ms->valueint;
        }
    }

    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (cJSON_IsObject(mqtt)) {
        (void)xz_apply_config_object(mqtt, XZ_NS_MQTT);
        result->has_mqtt = TRUE;
    }

    cJSON *ws = cJSON_GetObjectItem(root, "websocket");
    if (cJSON_IsObject(ws)) {
        (void)xz_apply_config_object(ws, XZ_NS_WS);
        result->has_websocket = TRUE;
    }

    cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
    if (cJSON_IsObject(server_time)) {
        cJSON *timestamp       = cJSON_GetObjectItem(server_time, "timestamp");
        cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");
        if (cJSON_IsNumber(timestamp)) {
            result->has_server_time     = TRUE;
            result->server_timestamp_ms = (int64_t)timestamp->valuedouble;
            if (cJSON_IsNumber(timezone_offset)) {
                result->timezone_offset_min = timezone_offset->valueint;
            }
        }
    }

    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (cJSON_IsObject(firmware)) {
        cJSON *version = cJSON_GetObjectItem(firmware, "version");
        cJSON *url     = cJSON_GetObjectItem(firmware, "url");
        cJSON *force   = cJSON_GetObjectItem(firmware, "force");
        if (cJSON_IsString(version) && version->valuestring) {
            (void)snprintf(result->firmware_version, sizeof(result->firmware_version), "%s", version->valuestring);
        }
        if (cJSON_IsString(url) && url->valuestring) {
            (void)snprintf(result->firmware_url, sizeof(result->firmware_url), "%s", url->valuestring);
        }
        result->firmware_force = (cJSON_IsNumber(force) && force->valueint == 1) ? TRUE : FALSE;
        if (result->firmware_version[0] != '\0' && result->firmware_url[0] != '\0') {
            result->has_firmware = TRUE;
        }
    }

    if (result->has_mqtt) {
        (void)xiaozhi_settings_set_proto(XZ_PROTOCOL_MQTT_UDP);
    } else if (result->has_websocket) {
        (void)xiaozhi_settings_set_proto(XZ_PROTOCOL_WEBSOCKET);
    } else {
        // Align with xiaozhi-esp32 fallback: no server config -> MQTT first
        (void)xiaozhi_settings_set_proto(XZ_PROTOCOL_MQTT_UDP);
    }

    cJSON_Delete(root);
    return OPRT_OK;
}

OPERATE_RET xiaozhi_ota_activate_if_needed(const xz_ota_result_t *result)
{
    if (!result) {
        return OPRT_INVALID_PARM;
    }

    if (!result->has_activation_challenge || result->activation_challenge[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    char ota_url[256] = {0};
    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ota_url", ota_url, sizeof(ota_url), XZ_DEFAULT_OTA_URL);
    if (ota_url[0] == '\0') {
        (void)snprintf(ota_url, sizeof(ota_url), "%s", XZ_DEFAULT_OTA_URL);
    }
    xz_env_override(ota_url, sizeof(ota_url), "XZ_OTA_URL");

    char serial[96]  = {0};
    char secret[128] = {0};
    xz_load_serial_number(serial, sizeof(serial));
    (void)xiaozhi_settings_get_string(XZ_NS_SYS, "activation_secret", secret, sizeof(secret), "");
    xz_env_override(serial, sizeof(serial), "XZ_ACT_SERIAL");
    xz_env_override(secret, sizeof(secret), "XZ_ACT_HMAC_SECRET");

    BOOL_T has_serial = (serial[0] != '\0') ? TRUE : FALSE;

    char       *json = NULL;
    OPERATE_RET rt   = OPRT_OK;
    if (has_serial) {
        char   hmac_hex[65] = {0};
        BOOL_T hmac_ready   = FALSE;

#if defined(ESP_PLATFORM) && defined(SOC_HMAC_SUPPORTED)
        {
            uint8_t   digest[32] = {0};
            esp_err_t esp_rt     = esp_hmac_calculate(HMAC_KEY0, (const uint8_t *)result->activation_challenge,
                                                      strlen(result->activation_challenge), digest);
            if (esp_rt == ESP_OK) {
                for (size_t i = 0; i < sizeof(digest); ++i) {
                    (void)snprintf(hmac_hex + i * 2, sizeof(hmac_hex) - i * 2, "%02x", digest[i]);
                }
                hmac_hex[64] = '\0';
                hmac_ready   = TRUE;
            } else {
                return OPRT_COM_ERROR;
            }
        }
#endif

        if (!hmac_ready && secret[0] != '\0') {
            rt = xz_make_activation_hmac(secret, result->activation_challenge, hmac_hex, sizeof(hmac_hex));
            if (rt != OPRT_OK) {
                return rt;
            }
            hmac_ready = TRUE;
        }

        cJSON *payload = cJSON_CreateObject();
        if (!payload) {
            return OPRT_MALLOC_FAILED;
        }

        cJSON_AddStringToObject(payload, "algorithm", "hmac-sha256");
        cJSON_AddStringToObject(payload, "serial_number", serial);
        cJSON_AddStringToObject(payload, "challenge", result->activation_challenge);
        cJSON_AddStringToObject(payload, "hmac", hmac_hex);

        json = cJSON_PrintUnformatted(payload);
        cJSON_Delete(payload);
        if (!json) {
            return OPRT_MALLOC_FAILED;
        }
    } else {
        json = tal_malloc(3);
        if (!json) {
            return OPRT_MALLOC_FAILED;
        }
        (void)snprintf(json, 3, "{}");
    }

    char *act_url = xz_make_activate_url(ota_url);
    if (!act_url) {
        cJSON_free(json);
        return OPRT_MALLOC_FAILED;
    }

    char device_id[32]       = {0};
    char client_id[96]       = {0};
    char user_agent[128]     = {0};
    char accept_language[32] = {0};
    xz_load_http_common(device_id, sizeof(device_id), client_id, sizeof(client_id), user_agent, sizeof(user_agent),
                        accept_language, sizeof(accept_language));

    const char          *activation_version = has_serial ? "2" : "1";
    http_client_header_t headers[8]         = {0};
    uint8_t              headers_count      = 0;
    headers[headers_count++] = (http_client_header_t){.key = "Activation-Version", .value = activation_version};
    headers[headers_count++] = (http_client_header_t){.key = "Device-Id", .value = device_id};
    headers[headers_count++] = (http_client_header_t){.key = "Client-Id", .value = client_id};
    if (has_serial) {
        headers[headers_count++] = (http_client_header_t){.key = "Serial-Number", .value = serial};
    }
    headers[headers_count++] = (http_client_header_t){.key = "User-Agent", .value = user_agent};
    headers[headers_count++] = (http_client_header_t){.key = "Accept-Language", .value = accept_language};
    headers[headers_count++] = (http_client_header_t){.key = "Content-Type", .value = "application/json"};

    cJSON   *root   = NULL;
    uint16_t status = 0;
    rt = xz_http_request_json(act_url, "POST", headers, headers_count, (const uint8_t *)json, strlen(json), &root,
                              &status);
    if (root) {
        cJSON_Delete(root);
    }
    cJSON_free(json);
    tal_free(act_url);

    if (rt != OPRT_OK) {
        return rt;
    }

    if (status == 200) {
        return OPRT_OK;
    }
    if (status == 202) {
        return OPRT_TIMEOUT;
    }

    return OPRT_COM_ERROR;
}
