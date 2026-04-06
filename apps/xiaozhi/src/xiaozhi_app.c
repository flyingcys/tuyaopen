/**
 * @file xiaozhi_app.c
 * @brief High-level runtime controller for xiaozhi protocols.
 */

#include "xiaozhi_app.h"

#include "cJSON.h"
#include "netmgr.h"
#include "tal_api.h"
#include "tkl_flash.h"
#include "xiaozhi_identity.h"
#include "xiaozhi_mcp.h"
#include "xiaozhi_mqtt_udp.h"
#include "xiaozhi_ota.h"
#include "xiaozhi_settings.h"
#include "xiaozhi_state.h"
#include "xiaozhi_system.h"
#include "xiaozhi_upgrade.h"
#include "xiaozhi_ws.h"

#if defined(OPERATING_SYSTEM) && defined(SYSTEM_LINUX) && (OPERATING_SYSTEM == SYSTEM_LINUX)
#define XZ_APP_ENABLE_LINUX_AUDIO 1
#include "xiaozhi_audio_linux.h"
#else
#define XZ_APP_ENABLE_LINUX_AUDIO 0
#endif

#if defined(ESP_PLATFORM)
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#endif

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#include "tal_wifi.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.1.0"
#endif

#ifndef PROJECT_NAME
#define PROJECT_NAME "xiaozhi"
#endif

#ifndef PLATFORM_CHIP
#define PLATFORM_CHIP "unknown"
#endif

#ifndef PLATFORM_BOARD
#define PLATFORM_BOARD "unknown"
#endif

typedef enum {
    XZ_ACTIVE_NONE = 0,
    XZ_ACTIVE_WS,
    XZ_ACTIVE_MQTT_UDP,
} xz_active_proto_t;

typedef enum {
    XZ_LISTEN_MODE_AUTO = 0,
    XZ_LISTEN_MODE_MANUAL,
    XZ_LISTEN_MODE_REALTIME,
} xz_listen_mode_t;

#define XZ_UPLINK_AUDIO_QUEUE_DEPTH 16U
#define XZ_UPLINK_AUDIO_FRAME_MAX   1024U

typedef struct {
    size_t  length;
    uint8_t data[XZ_UPLINK_AUDIO_FRAME_MAX];
} xz_uplink_audio_frame_t;

typedef struct {
    MUTEX_HANDLE            lock;
    uint32_t                head;
    uint32_t                tail;
    uint32_t                count;
    xz_uplink_audio_frame_t frames[XZ_UPLINK_AUDIO_QUEUE_DEPTH];
} xz_uplink_audio_queue_t;

typedef struct {
    BOOL_T  inited;
    BOOL_T  running;
    BOOL_T  stop_flag;
    BOOL_T  reconnect_flag;
    BOOL_T  ota_checked;
    uint8_t ota_retry_count;

    MUTEX_HANDLE            lock;
    THREAD_HANDLE           worker;
    xz_uplink_audio_queue_t uplink_audio_queue;

    xz_active_proto_t    active;
    xz_chat_state_t      chat_state;
    xz_listen_mode_t     listen_mode;
    BOOL_T               pending_reboot;
    char                 pending_upgrade_url[384];
    char                 last_stt[256];
    char                 last_tts_sentence[256];
    char                 last_alert[256];
    char                 last_custom[256];
    char                 last_emotion[64];
    xz_ws_client_t       ws;
    xz_mqtt_udp_client_t mqtt_udp;
} xz_app_ctx_t;

static xz_app_ctx_t s_app = {0};

static void        xz_on_transport_text_message(void *userdata, const uint8_t *payload, size_t payload_len);
static void        xz_on_transport_binary_message(void *userdata, const uint8_t *payload, size_t payload_len);
static void        xz_handle_transport_text_locked(const uint8_t *payload, size_t payload_len);
extern int         xz_state_accepts_tts_binary(xz_chat_state_t current);
static OPERATE_RET xz_send_mcp_payload_locked(const char *payload_json);
static OPERATE_RET xz_audio_start_capture_locked(void);
static OPERATE_RET xz_audio_stop_capture_locked(void);
static OPERATE_RET xz_audio_start_detect_locked(void);
static OPERATE_RET xz_audio_stop_detect_locked(void);
static OPERATE_RET xz_audio_resume_detect_locked(void);
static OPERATE_RET xz_audio_abort_playback_locked(void);
static OPERATE_RET xz_audio_reset_locked(void);
static BOOL_T      xz_is_connected_locked(void);
static void        xz_set_chat_state_locked(xz_chat_state_t state);
static OPERATE_RET xz_uplink_audio_queue_init(void);
static void        xz_uplink_audio_queue_clear(void);
static BOOL_T      xz_uplink_audio_queue_push(const void *frame, size_t length);
static BOOL_T      xz_uplink_audio_queue_pop(uint8_t *out, size_t *out_len, size_t out_cap);
static OPERATE_RET xz_drain_uplink_audio_queue_locked(void);
static OPERATE_RET xz_send_listen_transport_locked(const char *state, const char *mode, const char *text);
static OPERATE_RET xz_send_abort_transport_locked(const char *reason);
static OPERATE_RET xz_transition_to_idle_locked(BOOL_T abort_playback, BOOL_T resume_detect);
static OPERATE_RET xz_restart_listening_locked(xz_listen_mode_t mode);
static OPERATE_RET xz_handle_hotword_detected_locked(const char *wake_word);
static OPERATE_RET xz_on_listen_start_locked(const char *mode);
static OPERATE_RET xz_on_listen_stop_locked(void);
static OPERATE_RET xz_on_tts_start_locked(void);
static OPERATE_RET xz_on_tts_stop_locked(void);
static OPERATE_RET xz_on_abort_locked(void);

static OPERATE_RET xz_audio_start_capture_locked(void)
{
#if XZ_APP_ENABLE_LINUX_AUDIO
    int rt = xiaozhi_audio_linux_start_capture();
    if (rt != OPRT_OK) {
        PR_WARN("audio start capture failed: %d", rt);
        return rt;
    }
#endif
    return OPRT_OK;
}

static OPERATE_RET xz_audio_stop_capture_locked(void)
{
#if XZ_APP_ENABLE_LINUX_AUDIO
    int rt = xiaozhi_audio_linux_stop_capture();
    if (rt != OPRT_OK) {
        PR_WARN("audio stop capture failed: %d", rt);
        return rt;
    }
#endif
    return OPRT_OK;
}

static OPERATE_RET xz_audio_start_detect_locked(void)
{
#if XZ_APP_ENABLE_LINUX_AUDIO
    int rt = xiaozhi_audio_linux_start_detect();
    if (rt != OPRT_OK) {
        PR_WARN("audio start detect failed: %d", rt);
        return rt;
    }
#endif
    return OPRT_OK;
}

static OPERATE_RET xz_audio_stop_detect_locked(void)
{
#if XZ_APP_ENABLE_LINUX_AUDIO
    int rt = xiaozhi_audio_linux_stop_detect();
    if (rt != OPRT_OK) {
        PR_WARN("audio stop detect failed: %d", rt);
        return rt;
    }
#endif
    return OPRT_OK;
}

static OPERATE_RET xz_audio_resume_detect_locked(void)
{
#if XZ_APP_ENABLE_LINUX_AUDIO
    int rt = xiaozhi_audio_linux_resume_detect();
    if (rt != OPRT_OK) {
        PR_WARN("audio resume detect failed: %d", rt);
        return rt;
    }
#endif
    return OPRT_OK;
}

static OPERATE_RET xz_audio_abort_playback_locked(void)
{
#if XZ_APP_ENABLE_LINUX_AUDIO
    int rt = xiaozhi_audio_linux_abort_playback();
    if (rt != OPRT_OK) {
        PR_WARN("audio abort playback failed: %d", rt);
        return rt;
    }
#endif
    return OPRT_OK;
}

static OPERATE_RET xz_audio_reset_locked(void)
{
#if XZ_APP_ENABLE_LINUX_AUDIO
    xiaozhi_audio_linux_reset();
#endif
    return OPRT_OK;
}

static OPERATE_RET xz_uplink_audio_queue_init(void)
{
    if (s_app.uplink_audio_queue.lock) {
        return OPRT_OK;
    }

    OPERATE_RET rt = tal_mutex_create_init(&s_app.uplink_audio_queue.lock);
    if (rt != OPRT_OK) {
        return rt;
    }

    s_app.uplink_audio_queue.head  = 0;
    s_app.uplink_audio_queue.tail  = 0;
    s_app.uplink_audio_queue.count = 0;
    return OPRT_OK;
}

static void xz_uplink_audio_queue_clear(void)
{
    if (!s_app.uplink_audio_queue.lock) {
        return;
    }

    if (tal_mutex_lock(s_app.uplink_audio_queue.lock) != OPRT_OK) {
        return;
    }

    s_app.uplink_audio_queue.head  = 0;
    s_app.uplink_audio_queue.tail  = 0;
    s_app.uplink_audio_queue.count = 0;

    (void)tal_mutex_unlock(s_app.uplink_audio_queue.lock);
}

static BOOL_T xz_uplink_audio_queue_push(const void *frame, size_t length)
{
    if (!frame || length == 0 || length > XZ_UPLINK_AUDIO_FRAME_MAX || !s_app.uplink_audio_queue.lock) {
        return FALSE;
    }

    if (tal_mutex_lock(s_app.uplink_audio_queue.lock) != OPRT_OK) {
        return FALSE;
    }

    if (s_app.uplink_audio_queue.count == XZ_UPLINK_AUDIO_QUEUE_DEPTH) {
        (void)tal_mutex_unlock(s_app.uplink_audio_queue.lock);
        return FALSE;
    }

    xz_uplink_audio_frame_t *slot = &s_app.uplink_audio_queue.frames[s_app.uplink_audio_queue.tail];
    (void)memcpy(slot->data, frame, length);
    slot->length = length;

    s_app.uplink_audio_queue.tail = (s_app.uplink_audio_queue.tail + 1U) % XZ_UPLINK_AUDIO_QUEUE_DEPTH;
    s_app.uplink_audio_queue.count++;

    (void)tal_mutex_unlock(s_app.uplink_audio_queue.lock);
    return TRUE;
}

static BOOL_T xz_uplink_audio_queue_pop(uint8_t *out, size_t *out_len, size_t out_cap)
{
    if (!out || !out_len || out_cap == 0 || !s_app.uplink_audio_queue.lock) {
        return FALSE;
    }

    if (tal_mutex_lock(s_app.uplink_audio_queue.lock) != OPRT_OK) {
        return FALSE;
    }

    if (s_app.uplink_audio_queue.count == 0) {
        (void)tal_mutex_unlock(s_app.uplink_audio_queue.lock);
        return FALSE;
    }

    xz_uplink_audio_frame_t *slot = &s_app.uplink_audio_queue.frames[s_app.uplink_audio_queue.head];
    if (slot->length > out_cap) {
        s_app.uplink_audio_queue.head = (s_app.uplink_audio_queue.head + 1U) % XZ_UPLINK_AUDIO_QUEUE_DEPTH;
        s_app.uplink_audio_queue.count--;
        (void)tal_mutex_unlock(s_app.uplink_audio_queue.lock);
        return FALSE;
    }

    (void)memcpy(out, slot->data, slot->length);
    *out_len = slot->length;

    s_app.uplink_audio_queue.head = (s_app.uplink_audio_queue.head + 1U) % XZ_UPLINK_AUDIO_QUEUE_DEPTH;
    s_app.uplink_audio_queue.count--;

    (void)tal_mutex_unlock(s_app.uplink_audio_queue.lock);
    return TRUE;
}

static OPERATE_RET xz_drain_uplink_audio_queue_locked(void)
{
    uint8_t frame[XZ_UPLINK_AUDIO_FRAME_MAX];
    size_t  frame_len = 0;

    while (xz_uplink_audio_queue_pop(frame, &frame_len, sizeof(frame))) {
        OPERATE_RET rt = OPRT_INVALID_PARM;
        if (s_app.active == XZ_ACTIVE_WS) {
            rt = xz_ws_send_audio(&s_app.ws, frame, frame_len);
        } else if (s_app.active == XZ_ACTIVE_MQTT_UDP) {
            rt = xz_mqtt_udp_send_audio(&s_app.mqtt_udp, frame, frame_len, tal_system_get_millisecond());
        }

        if (rt != OPRT_OK && rt != OPRT_INVALID_PARM) {
            PR_WARN("audio uplink send failed: %d", rt);
            return rt;
        }
    }

    return OPRT_OK;
}

#if XZ_APP_ENABLE_LINUX_AUDIO
void xiaozhi_audio_linux_on_tx_opus_frame(const void *frame, size_t length, void *ctx)
{
    (void)ctx;
    if (!frame || length == 0 || !s_app.inited) {
        return;
    }

    (void)xz_uplink_audio_queue_push(frame, length);
}

static void xz_on_linux_hotword_detected(const char *wake_word, void *ctx)
{
    (void)ctx;
    if (!s_app.inited) {
        return;
    }

    if (tal_mutex_lock(s_app.lock) != OPRT_OK) {
        return;
    }

    OPERATE_RET rt = xz_handle_hotword_detected_locked(wake_word);
    (void)tal_mutex_unlock(s_app.lock);

    if (rt != OPRT_OK && rt != OPRT_RESOURCE_NOT_READY && rt != OPRT_NOT_FOUND) {
        PR_WARN("hotword flow failed: %d", rt);
    }
}
#endif

static void xz_copy_str(char *out, size_t out_size, const char *in)
{
    if (!out || out_size == 0) {
        return;
    }
    (void)snprintf(out, out_size, "%s", in ? in : "");
}

static xz_listen_mode_t xz_listen_mode_from_text(const char *mode)
{
    if (!mode || mode[0] == '\0') {
        return XZ_LISTEN_MODE_MANUAL;
    }
    if (strcmp(mode, "auto") == 0) {
        return XZ_LISTEN_MODE_AUTO;
    }
    if (strcmp(mode, "realtime") == 0) {
        return XZ_LISTEN_MODE_REALTIME;
    }
    return XZ_LISTEN_MODE_MANUAL;
}

static const char *xz_listen_mode_to_text(xz_listen_mode_t mode)
{
    if (mode == XZ_LISTEN_MODE_AUTO) {
        return "auto";
    }
    if (mode == XZ_LISTEN_MODE_REALTIME) {
        return "realtime";
    }
    return "manual";
}

static const char *xz_chat_state_to_text(xz_chat_state_t state)
{
    if (state == XZ_CHAT_CONNECTING) {
        return "connecting";
    }
    if (state == XZ_CHAT_LISTENING) {
        return "listening";
    }
    if (state == XZ_CHAT_SPEAKING) {
        return "speaking";
    }
    return "idle";
}

static void xz_set_chat_state_locked(xz_chat_state_t state)
{
    if (s_app.chat_state != state) {
        PR_NOTICE("chat state: %s -> %s", xz_chat_state_to_text(s_app.chat_state), xz_chat_state_to_text(state));
    }
    s_app.chat_state = state;
}

static OPERATE_RET xz_send_listen_transport_locked(const char *state, const char *mode, const char *text)
{
    if (!state || state[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("send listen: state=%s mode=%s text=%s active=%d", state, (mode && mode[0] != '\0') ? mode : "(none)",
              (text && text[0] != '\0') ? text : "(none)", (int)s_app.active);

    if (s_app.active == XZ_ACTIVE_WS) {
        return xz_ws_send_listen(&s_app.ws, state, mode, text);
    }
    if (s_app.active == XZ_ACTIVE_MQTT_UDP) {
        return xz_mqtt_udp_send_listen(&s_app.mqtt_udp, state, mode, text);
    }

    return OPRT_COM_ERROR;
}

static OPERATE_RET xz_send_abort_transport_locked(const char *reason)
{
    PR_NOTICE("send abort: reason=%s active=%d", (reason && reason[0] != '\0') ? reason : "(none)", (int)s_app.active);

    if (s_app.active == XZ_ACTIVE_WS) {
        return xz_ws_send_abort(&s_app.ws, reason);
    }
    if (s_app.active == XZ_ACTIVE_MQTT_UDP) {
        return xz_mqtt_udp_send_abort(&s_app.mqtt_udp, reason);
    }

    return OPRT_COM_ERROR;
}

static OPERATE_RET xz_transition_to_idle_locked(BOOL_T abort_playback, BOOL_T resume_detect)
{
    PR_NOTICE("transition to idle: abort_playback=%d resume_detect=%d", abort_playback, resume_detect);
    OPERATE_RET rt = xz_audio_stop_capture_locked();
    if (rt != OPRT_OK) {
        return rt;
    }

    xz_uplink_audio_queue_clear();

    if (abort_playback) {
        rt = xz_audio_abort_playback_locked();
        if (rt != OPRT_OK) {
            return rt;
        }
        (void)xz_audio_reset_locked();
    }

    xz_set_chat_state_locked(XZ_CHAT_IDLE);
    if (resume_detect) {
        (void)xz_audio_resume_detect_locked();
    }
    return OPRT_OK;
}

static OPERATE_RET xz_restart_listening_locked(xz_listen_mode_t mode)
{
    const char *mode_text = xz_listen_mode_to_text(mode);
    PR_NOTICE("restart listening: mode=%s", mode_text);
    OPERATE_RET rt        = xz_send_listen_transport_locked("start", mode_text, NULL);
    if (rt != OPRT_OK) {
        return rt;
    }

    return xz_on_listen_start_locked(mode_text);
}

static OPERATE_RET xz_on_listen_start_locked(const char *mode)
{
    PR_NOTICE("enter listening: mode=%s", (mode && mode[0] != '\0') ? mode : "manual");
    xz_uplink_audio_queue_clear();
    (void)xz_audio_abort_playback_locked();
    OPERATE_RET rt = xz_audio_start_capture_locked();
    if (rt != OPRT_OK) {
        return rt;
    }
    s_app.listen_mode = xz_listen_mode_from_text(mode);
    xz_set_chat_state_locked(xz_state_after_listen_start(s_app.chat_state));
    return OPRT_OK;
}

static OPERATE_RET xz_on_listen_stop_locked(void)
{
    PR_NOTICE("leave listening");
    return xz_transition_to_idle_locked(FALSE, TRUE);
}

static OPERATE_RET xz_on_tts_start_locked(void)
{
    PR_NOTICE("enter speaking");
    OPERATE_RET rt = xz_audio_stop_capture_locked();
    if (rt != OPRT_OK) {
        return rt;
    }
    xz_uplink_audio_queue_clear();
    (void)xz_audio_reset_locked();
    xz_set_chat_state_locked(xz_state_after_tts_start(s_app.chat_state));
    return OPRT_OK;
}

static OPERATE_RET xz_on_tts_stop_locked(void)
{
    BOOL_T continue_listening = (s_app.listen_mode != XZ_LISTEN_MODE_MANUAL) ? TRUE : FALSE;
    PR_NOTICE("leave speaking: continue_listening=%d mode=%s", continue_listening, xz_listen_mode_to_text(s_app.listen_mode));
    OPERATE_RET rt           = xz_audio_stop_capture_locked();
    if (rt != OPRT_OK) {
        return rt;
    }

    xz_set_chat_state_locked(xz_state_after_tts_stop_mode(s_app.chat_state, continue_listening));
    if (!continue_listening) {
        (void)xz_audio_resume_detect_locked();
        return OPRT_OK;
    }

    return xz_restart_listening_locked(s_app.listen_mode);
}

static OPERATE_RET xz_on_abort_locked(void)
{
    PR_NOTICE("handle abort");
    OPERATE_RET rt = xz_transition_to_idle_locked(TRUE, TRUE);
    if (rt != OPRT_OK) {
        return rt;
    }

    xz_set_chat_state_locked(xz_state_after_abort(s_app.chat_state));
    return OPRT_OK;
}

static OPERATE_RET xz_handle_hotword_detected_locked(const char *wake_word)
{
    const char *hotword = (wake_word && wake_word[0] != '\0') ? wake_word : "wake";
    OPERATE_RET rt;

    PR_NOTICE("hotword detected: word=%s chat=%s connected=%d", hotword, xz_chat_state_to_text(s_app.chat_state),
              xz_is_connected_locked());

    if (!xz_is_connected_locked()) {
        return OPRT_RESOURCE_NOT_READY;
    }

    if (s_app.chat_state == XZ_CHAT_SPEAKING || s_app.chat_state == XZ_CHAT_LISTENING) {
        (void)xz_send_abort_transport_locked("wake_word_detected");
        rt = xz_transition_to_idle_locked(TRUE, FALSE);
        if (rt != OPRT_OK) {
            return rt;
        }
    } else if (s_app.chat_state == XZ_CHAT_CONNECTING) {
        return OPRT_RESOURCE_NOT_READY;
    }

    rt = xz_send_listen_transport_locked("detect", NULL, hotword);
    if (rt != OPRT_OK) {
        return rt;
    }

#if XZ_APP_ENABLE_LINUX_AUDIO
    rt = xiaozhi_audio_linux_play_wakeup_prompt();
    if (rt != OPRT_OK && rt != OPRT_NOT_FOUND) {
        return rt;
    }
#endif

    s_app.listen_mode = XZ_LISTEN_MODE_AUTO;
    return xz_restart_listening_locked(s_app.listen_mode);
}

static int xz_parse_version_part(const char **pp)
{
    if (!pp || !*pp) {
        return 0;
    }
    const char *p = *pp;
    int         v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        ++p;
    }
    if (*p == '.') {
        ++p;
    }
    *pp = p;
    return v;
}

static BOOL_T xz_is_new_version(const char *current, const char *incoming)
{
    if (!incoming || incoming[0] == '\0') {
        return FALSE;
    }
    if (!current || current[0] == '\0') {
        return TRUE;
    }
    const char *a = current;
    const char *b = incoming;
    for (int i = 0; i < 4; ++i) {
        int av = xz_parse_version_part(&a);
        int bv = xz_parse_version_part(&b);
        if (bv > av) {
            return TRUE;
        }
        if (bv < av) {
            return FALSE;
        }
    }
    return FALSE;
}

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

static double xz_env_double(const char *env_key, double default_value, BOOL_T *has_value)
{
    if (has_value) {
        *has_value = FALSE;
    }
    if (!env_key) {
        return default_value;
    }

    const char *val = getenv(env_key);
    if (!val || val[0] == '\0') {
        return default_value;
    }

    if (has_value) {
        *has_value = TRUE;
    }
    return atof(val);
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

static BOOL_T xz_net_is_ready(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    if (netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status) != OPRT_OK) {
        return FALSE;
    }

    return status != NETMGR_LINK_DOWN;
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

static OPERATE_RET xz_netmgr_init_once(void)
{
    static BOOL_T s_net_inited = FALSE;
    if (s_net_inited) {
        return OPRT_OK;
    }

    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
    type |= NETCONN_CELLULAR;
#endif

    if (type == 0) {
        type = NETCONN_AUTO;
    }

    OPERATE_RET rt = netmgr_init(type);
    if (rt == OPRT_OK) {
        s_net_inited = TRUE;
    }

    return rt;
}

static cJSON *xz_mcp_make_status_json(void *userdata)
{
    (void)userdata;
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    cJSON *audio_speaker = cJSON_CreateObject();
    cJSON *screen        = cJSON_CreateObject();
    cJSON *network       = cJSON_CreateObject();
    if (!audio_speaker || !screen || !network) {
        if (audio_speaker) {
            cJSON_Delete(audio_speaker);
        }
        if (screen) {
            cJSON_Delete(screen);
        }
        if (network) {
            cJSON_Delete(network);
        }
        cJSON_Delete(root);
        return NULL;
    }

    cJSON_AddNumberToObject(audio_speaker, "volume", 100);
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

    cJSON_AddNumberToObject(screen, "brightness", 100);
    cJSON_AddStringToObject(screen, "theme", "light");
    cJSON_AddItemToObject(root, "screen", screen);

    char        ssid[64]          = {0};
    int         rssi              = 0;
    const char *signal            = "weak";
    BOOL_T      has_battery_level = FALSE;
    BOOL_T      has_chip_temp     = FALSE;
    int         battery_level     = xz_env_int("XZ_BATTERY_LEVEL", 0);
    int         battery_charging  = xz_env_int("XZ_BATTERY_CHARGING", 0);
    double      chip_temp         = xz_env_double("XZ_CHIP_TEMPERATURE", 0.0, &has_chip_temp);
    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ssid", ssid, sizeof(ssid), "");
    has_battery_level = (getenv("XZ_BATTERY_LEVEL") != NULL) ? TRUE : FALSE;
    if (xz_get_wifi_rssi(&rssi)) {
        signal = (rssi >= -60) ? "strong" : ((rssi >= -70) ? "medium" : "weak");
    } else if (xz_net_is_ready()) {
        signal = "strong";
    }
    cJSON_AddStringToObject(network, "type", "wifi");
    cJSON_AddStringToObject(network, "ssid", ssid);
    cJSON_AddStringToObject(network, "signal", signal);
    cJSON_AddItemToObject(root, "network", network);

    if (has_battery_level) {
        cJSON *battery = cJSON_CreateObject();
        if (battery) {
            cJSON_AddNumberToObject(battery, "level", battery_level);
            cJSON_AddBoolToObject(battery, "charging", (battery_charging == 1) ? true : false);
            cJSON_AddItemToObject(root, "battery", battery);
        }
    }

    if (has_chip_temp) {
        cJSON *chip = cJSON_CreateObject();
        if (chip) {
            cJSON_AddNumberToObject(chip, "temperature", chip_temp);
            cJSON_AddItemToObject(root, "chip", chip);
        }
    }

    return root;
}

static cJSON *xz_mcp_make_system_info_json(void *userdata)
{
    (void)userdata;
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        return NULL;
    }

    char language[16]        = {0};
    char device_id[32]       = {0};
    char client_id[96]       = {0};
    char chip_model_name[32] = {0};
    char min_heap[32]        = {0};
    char ssid[64]            = {0};
    char ip_str[16]          = {0};
    int  chip_cores          = 0;
    int  flash_size          = 0;
    int  wifi_channel        = 0;
    int  wifi_rssi           = 0;
    int  chip_model          = 0;
    int  chip_revision       = 0;
    int  chip_features       = 0;
    char compile_time[64]    = {0};
    char idf_version[64]     = {0};
    char elf_sha256[128]     = {0};
    char ota_label[32]       = {0};
    (void)snprintf(language, sizeof(language), "%s", "zh-CN");
    (void)snprintf(chip_model_name, sizeof(chip_model_name), "%s", PLATFORM_CHIP);
    (void)snprintf(compile_time, sizeof(compile_time), "%s", __DATE__ "T" __TIME__ "Z");
    (void)snprintf(idf_version, sizeof(idf_version), "%s", "tuyaopen");
    (void)snprintf(ota_label, sizeof(ota_label), "%s", "ota_0");
#if defined(ESP_PLATFORM)
    xz_fill_esp_runtime_fields(&flash_size, &chip_model, &chip_revision, &chip_features, idf_version,
                               sizeof(idf_version), elf_sha256, sizeof(elf_sha256), ota_label, sizeof(ota_label));
#else
    flash_size = xz_detect_flash_size_from_tkl();
#endif

    xz_env_override(language, sizeof(language), "XZ_ACCEPT_LANGUAGE");
    xz_env_override(compile_time, sizeof(compile_time), "XZ_APP_COMPILE_TIME");
    xz_env_override(idf_version, sizeof(idf_version), "XZ_APP_IDF_VERSION");
    xz_env_override(elf_sha256, sizeof(elf_sha256), "XZ_APP_ELF_SHA256");
    xz_env_override(ota_label, sizeof(ota_label), "XZ_OTA_LABEL");
    xz_fill_chip_runtime_info(chip_model_name, sizeof(chip_model_name), &chip_cores);
    (void)xiaozhi_system_get_device_id(device_id, sizeof(device_id));
    (void)xiaozhi_system_get_client_id(client_id, sizeof(client_id));
    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ssid", ssid, sizeof(ssid), "");
    (void)snprintf(min_heap, sizeof(min_heap), "%u", (unsigned)xz_get_minimum_free_heap_size());
    flash_size    = xz_env_int("XZ_OTA_FLASH_SIZE", flash_size);
    wifi_channel  = xz_env_int("XZ_WIFI_CHANNEL", 0);
    chip_model    = xz_env_int("XZ_CHIP_MODEL", chip_model);
    chip_revision = xz_env_int("XZ_CHIP_REVISION", chip_revision);
    chip_features = xz_env_int("XZ_CHIP_FEATURES", chip_features);
    if (xz_get_wifi_rssi(&wifi_rssi) == FALSE) {
        wifi_rssi = 0;
    }
    NW_IP_S ip = {0};
    if (netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_IP, &ip) == OPRT_OK) {
        (void)snprintf(ip_str, sizeof(ip_str), "%s", ip.ip);
    }

    cJSON_AddNumberToObject(obj, "version", 2);
    cJSON_AddStringToObject(obj, "language", language);
    cJSON_AddNumberToObject(obj, "flash_size", flash_size);
    cJSON_AddStringToObject(obj, "minimum_free_heap_size", min_heap);
    cJSON_AddStringToObject(obj, "mac_address", device_id);
    cJSON_AddStringToObject(obj, "uuid", client_id);
    cJSON_AddStringToObject(obj, "chip_model_name", chip_model_name);

    cJSON *chip_info = cJSON_CreateObject();
    if (!chip_info) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddNumberToObject(chip_info, "model", chip_model);
    cJSON_AddNumberToObject(chip_info, "cores", chip_cores);
    cJSON_AddNumberToObject(chip_info, "revision", chip_revision);
    cJSON_AddNumberToObject(chip_info, "features", chip_features);
    cJSON_AddItemToObject(obj, "chip_info", chip_info);

    cJSON *application = cJSON_CreateObject();
    if (!application) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddStringToObject(application, "name", PROJECT_NAME);
    cJSON_AddStringToObject(application, "version", PROJECT_VERSION);
    cJSON_AddStringToObject(application, "compile_time", compile_time);
    cJSON_AddStringToObject(application, "idf_version", idf_version);
    cJSON_AddStringToObject(application, "elf_sha256", elf_sha256);
    cJSON_AddItemToObject(obj, "application", application);

    cJSON *partition_table = xz_load_partition_table_json();
    if (!partition_table) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddItemToObject(obj, "partition_table", partition_table);

    cJSON *ota = cJSON_CreateObject();
    if (!ota) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddStringToObject(ota, "label", ota_label);
    cJSON_AddItemToObject(obj, "ota", ota);

    cJSON *display = cJSON_CreateObject();
    if (!display) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddBoolToObject(display, "monochrome", 0);
    cJSON_AddNumberToObject(display, "width", 0);
    cJSON_AddNumberToObject(display, "height", 0);
    cJSON_AddItemToObject(obj, "display", display);

    cJSON *board = cJSON_CreateObject();
    if (!board) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddStringToObject(board, "type", PLATFORM_BOARD);
    cJSON_AddStringToObject(board, "name", PLATFORM_BOARD);
    cJSON_AddStringToObject(board, "ssid", ssid);
    cJSON_AddNumberToObject(board, "rssi", wifi_rssi);
    cJSON_AddNumberToObject(board, "channel", wifi_channel);
    cJSON_AddStringToObject(board, "ip", ip_str);
    cJSON_AddStringToObject(board, "mac", device_id);
    cJSON_AddItemToObject(obj, "board", board);

    return obj;
}

static OPERATE_RET xz_mcp_request_reboot(void *userdata)
{
    (void)userdata;
    s_app.pending_reboot = TRUE;
    return OPRT_OK;
}

static OPERATE_RET xz_mcp_request_upgrade(void *userdata, const char *url)
{
    (void)userdata;
    if (!url || url[0] == '\0') {
        return OPRT_INVALID_PARM;
    }
    xz_copy_str(s_app.pending_upgrade_url, sizeof(s_app.pending_upgrade_url), url);
    return OPRT_OK;
}

static void xz_mcp_set_vision_capabilities(void *userdata, const char *url, const char *token)
{
    (void)userdata;
    if (url && url[0] != '\0') {
        (void)xiaozhi_settings_set_string(XZ_NS_SYS, "vision_url", url);
    }
    (void)xiaozhi_settings_set_string(XZ_NS_SYS, "vision_token", (token && token[0] != '\0') ? token : "");
}

static BOOL_T xz_is_connected_locked(void)
{
    if (s_app.active == XZ_ACTIVE_WS) {
        return (s_app.ws.state == XZ_CONN_READY && s_app.ws.channel.ready);
    }

    if (s_app.active == XZ_ACTIVE_MQTT_UDP) {
        return (s_app.mqtt_udp.state == XZ_CONN_READY && s_app.mqtt_udp.channel.ready && s_app.mqtt_udp.udp_fd >= 0);
    }

    return FALSE;
}

static OPERATE_RET xz_disconnect_locked(BOOL_T send_goodbye)
{
    if (s_app.active == XZ_ACTIVE_WS) {
        (void)xz_ws_close(&s_app.ws);
    } else if (s_app.active == XZ_ACTIVE_MQTT_UDP) {
        (void)xz_mqtt_udp_close(&s_app.mqtt_udp, send_goodbye);
    }

    s_app.active = XZ_ACTIVE_NONE;
    (void)xz_audio_stop_detect_locked();
    xz_set_chat_state_locked(XZ_CHAT_IDLE);
    (void)xz_audio_reset_locked();
    xz_uplink_audio_queue_clear();
    return OPRT_OK;
}

static OPERATE_RET xz_connect_websocket_locked(void)
{
    char ws_url[320]   = {0};
    char ws_token[256] = {0};
    char device_id[32] = {0};
    char client_id[96] = {0};

    (void)xiaozhi_settings_get_string(XZ_NS_WS, "url", ws_url, sizeof(ws_url), "");
    (void)xiaozhi_settings_get_string(XZ_NS_WS, "token", ws_token, sizeof(ws_token), "");
    int ws_version = xiaozhi_settings_get_int(XZ_NS_WS, "version", 1);

    xz_env_override(ws_url, sizeof(ws_url), "XZ_WS_URL");
    xz_env_override(ws_token, sizeof(ws_token), "XZ_WS_TOKEN");
    ws_version = xz_env_int("XZ_WS_VERSION", ws_version);

    if (ws_url[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    (void)xiaozhi_system_get_device_id(device_id, sizeof(device_id));
    (void)xiaozhi_system_get_client_id(client_id, sizeof(client_id));

    xz_set_chat_state_locked(XZ_CHAT_CONNECTING);
    (void)xz_audio_stop_detect_locked();
    (void)xz_audio_stop_capture_locked();
    (void)xz_audio_abort_playback_locked();
    OPERATE_RET rt = xz_ws_connect(&s_app.ws, ws_url, ws_token, ws_version, device_id, client_id);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = xz_ws_open_audio_channel(&s_app.ws, 10000);
    if (rt != OPRT_OK) {
        (void)xz_ws_close(&s_app.ws);
        return rt;
    }

    s_app.active = XZ_ACTIVE_WS;
    xz_set_chat_state_locked(XZ_CHAT_IDLE);
    (void)xz_audio_start_detect_locked();
    return OPRT_OK;
}

static OPERATE_RET xz_connect_mqtt_udp_locked(void)
{
    char endpoint[160]        = {0};
    char device_id[32]        = {0};
    char client_id[96]        = {0};
    char username[160]        = {0};
    char password[160]        = {0};
    char publish_topic[160]   = {0};
    char subscribe_topic[160] = {0};

    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "endpoint", endpoint, sizeof(endpoint), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "client_id", client_id, sizeof(client_id), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "username", username, sizeof(username), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "password", password, sizeof(password), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "publish_topic", publish_topic, sizeof(publish_topic), "");
    (void)xiaozhi_settings_get_string(XZ_NS_MQTT, "subscribe_topic", subscribe_topic, sizeof(subscribe_topic), "");
    int keepalive = xiaozhi_settings_get_int(XZ_NS_MQTT, "keepalive", 240);

    xz_env_override(endpoint, sizeof(endpoint), "XZ_MQTT_ENDPOINT");
    xz_env_override(client_id, sizeof(client_id), "XZ_MQTT_CLIENT_ID");
    xz_env_override(username, sizeof(username), "XZ_MQTT_USERNAME");
    xz_env_override(password, sizeof(password), "XZ_MQTT_PASSWORD");
    xz_env_override(publish_topic, sizeof(publish_topic), "XZ_MQTT_PUB_TOPIC");
    xz_env_override(subscribe_topic, sizeof(subscribe_topic), "XZ_MQTT_SUB_TOPIC");
    keepalive = xz_env_int("XZ_MQTT_KEEPALIVE", keepalive);

    if (endpoint[0] == '\0' || publish_topic[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    (void)xiaozhi_system_get_device_id(device_id, sizeof(device_id));
    if (client_id[0] == '\0') {
        (void)xiaozhi_system_get_client_id(client_id, sizeof(client_id));
    }
    if (!xz_identity_resolve_subscribe_topic(publish_topic, subscribe_topic, device_id, subscribe_topic,
                                             sizeof(subscribe_topic))) {
        return OPRT_INVALID_PARM;
    }

    xz_set_chat_state_locked(XZ_CHAT_CONNECTING);
    (void)xz_audio_stop_detect_locked();
    (void)xz_audio_stop_capture_locked();
    (void)xz_audio_abort_playback_locked();
    OPERATE_RET rt = xz_mqtt_udp_connect(&s_app.mqtt_udp, endpoint, client_id, username, password, publish_topic,
                                         subscribe_topic, keepalive);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = xz_mqtt_udp_open_audio_channel(&s_app.mqtt_udp, 10000);
    if (rt != OPRT_OK) {
        (void)xz_mqtt_udp_close(&s_app.mqtt_udp, FALSE);
        return rt;
    }

    s_app.active = XZ_ACTIVE_MQTT_UDP;
    xz_set_chat_state_locked(XZ_CHAT_IDLE);
    (void)xz_audio_start_detect_locked();
    return OPRT_OK;
}

static OPERATE_RET xz_connect_locked(void)
{
    char proto[24] = {0};
    (void)xiaozhi_settings_get_proto(proto, sizeof(proto));
    xz_env_override(proto, sizeof(proto), "XZ_PROTOCOL");

    if (strcmp(proto, XZ_PROTOCOL_MQTT_UDP) == 0) {
        return xz_connect_mqtt_udp_locked();
    }

    return xz_connect_websocket_locked();
}

static OPERATE_RET xz_ota_bootstrap_once(void)
{
    char disable_ota[8] = {0};
    xz_env_override(disable_ota, sizeof(disable_ota), "XZ_DISABLE_OTA");
    if (strcmp(disable_ota, "1") == 0 || strcmp(disable_ota, "true") == 0 || strcmp(disable_ota, "TRUE") == 0) {
        PR_WARN("ota bootstrap disabled by env XZ_DISABLE_OTA=%s", disable_ota);
        return OPRT_OK;
    }
    const int max_retry     = 10;
    int       retry_count   = 0;
    uint32_t  retry_delay_s = 10;

    while (!s_app.stop_flag) {
        xz_ota_result_t result = {0};
        OPERATE_RET     rt     = xiaozhi_ota_check_and_apply(&result);
        if (rt != OPRT_OK) {
            retry_count++;
            if (retry_count >= max_retry) {
                PR_WARN("ota check failed, retries exhausted: rt=%d", rt);
                return rt;
            }

            PR_WARN("ota check failed: %d, retry in %u seconds (%d/%d)", rt, (unsigned)retry_delay_s, retry_count,
                    max_retry);
            tal_system_sleep(retry_delay_s * 1000U);
            retry_delay_s *= 2;
            continue;
        }

        retry_count   = 0;
        retry_delay_s = 10;

        PR_NOTICE("ota check ok: mqtt=%d ws=%d activation_code=%d activation_challenge=%d server_time=%d firmware=%d",
                  result.has_mqtt, result.has_websocket, result.has_activation_code, result.has_activation_challenge,
                  result.has_server_time, result.has_firmware);

        if (result.has_server_time) {
            int64_t local_ms = result.server_timestamp_ms + (int64_t)result.timezone_offset_min * 60 * 1000;
            TIME_T  posix    = (TIME_T)(local_ms / 1000);
            (void)tal_time_set_posix(posix, 1);
        }

        if (result.has_firmware) {
            (void)xiaozhi_settings_set_string(XZ_NS_SYS, "firmware_version", result.firmware_version);
            (void)xiaozhi_settings_set_string(XZ_NS_SYS, "firmware_url", result.firmware_url);

            BOOL_T should_upgrade = result.firmware_force;
            if (!should_upgrade) {
                should_upgrade = xz_is_new_version(PROJECT_VERSION, result.firmware_version);
            }

            if (should_upgrade) {
                PR_NOTICE("new firmware detected version=%s force=%d url=%s", result.firmware_version,
                          result.firmware_force, result.firmware_url);
                return xiaozhi_app_upgrade_firmware(result.firmware_url);
            }
        }

        if (!result.has_activation_code && !result.has_activation_challenge) {
            return OPRT_OK;
        }

        if (result.has_activation_code) {
            PR_NOTICE("activation code=%s message=%s", result.activation_code, result.activation_message);
        }

        for (int i = 0; i < 10 && !s_app.stop_flag; ++i) {
            OPERATE_RET art = xiaozhi_ota_activate_if_needed(&result);
            if (art == OPRT_OK) {
                break;
            }

            if (art == OPRT_TIMEOUT) {
                PR_NOTICE("activation pending: retry %d/10 after 3s", i + 1);
                tal_system_sleep(3000);
            } else {
                PR_WARN("activation failed: %d retry %d/10 after 10s", art, i + 1);
                tal_system_sleep(10000);
            }
        }
    }

    return OPRT_COM_ERROR;
}

static OPERATE_RET xz_poll_locked(int wait_ms)
{
    OPERATE_RET rt = OPRT_OK;

    rt = xz_drain_uplink_audio_queue_locked();
    if (rt != OPRT_OK) {
        return rt;
    }

    if (s_app.active == XZ_ACTIVE_WS) {
        rt = xz_ws_poll(&s_app.ws, wait_ms);
        if (rt == OPRT_OK && xz_channel_is_timeout(&s_app.ws.channel, tal_system_get_millisecond())) {
            return OPRT_TIMEOUT;
        }
        return rt;
    }

    if (s_app.active == XZ_ACTIVE_MQTT_UDP) {
        rt = xz_mqtt_udp_poll(&s_app.mqtt_udp, wait_ms);
        if (rt == OPRT_RESOURCE_NOT_READY) {
            rt = OPRT_OK;
        }
        if (rt == OPRT_OK && xz_channel_is_timeout(&s_app.mqtt_udp.channel, tal_system_get_millisecond())) {
            return OPRT_TIMEOUT;
        }
        return rt;
    }

    return OPRT_OK;
}

static OPERATE_RET xz_send_mcp_payload_locked(const char *payload_json)
{
    if (!payload_json || payload_json[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    if (s_app.active == XZ_ACTIVE_WS) {
        return xz_ws_send_mcp(&s_app.ws, payload_json);
    }
    if (s_app.active == XZ_ACTIVE_MQTT_UDP) {
        return xz_mqtt_udp_send_mcp(&s_app.mqtt_udp, payload_json);
    }
    return OPRT_COM_ERROR;
}

static void xz_handle_transport_text_locked(const uint8_t *payload, size_t payload_len)
{
    if (!payload || payload_len == 0) {
        return;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    if (!root) {
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type) || !type->valuestring) {
        cJSON_Delete(root);
        return;
    }

    PR_NOTICE("recv text: type=%s chat=%s bytes=%u", type->valuestring, xz_chat_state_to_text(s_app.chat_state),
              (unsigned)payload_len);

    if (strcmp(type->valuestring, "tts") == 0) {
        cJSON *state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state) && state->valuestring) {
            PR_NOTICE("recv tts: state=%s", state->valuestring);
            if (strcmp(state->valuestring, "start") == 0) {
                OPERATE_RET rt = xz_on_tts_start_locked();
                if (rt != OPRT_OK) {
                    PR_WARN("tts start audio sync failed: %d", rt);
                }
            } else if (strcmp(state->valuestring, "stop") == 0) {
                OPERATE_RET rt = xz_on_tts_stop_locked();
                if (rt != OPRT_OK) {
                    PR_WARN("tts stop audio sync failed: %d", rt);
                }
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                cJSON *text = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(text) && text->valuestring) {
                    xz_copy_str(s_app.last_tts_sentence, sizeof(s_app.last_tts_sentence), text->valuestring);
                    PR_NOTICE("recv tts sentence: %s", s_app.last_tts_sentence);
                }
            }
        }
    } else if (strcmp(type->valuestring, "stt") == 0) {
        cJSON *text = cJSON_GetObjectItem(root, "text");
        if (cJSON_IsString(text) && text->valuestring) {
            xz_copy_str(s_app.last_stt, sizeof(s_app.last_stt), text->valuestring);
            PR_NOTICE("recv stt: %s", s_app.last_stt);
        }
    } else if (strcmp(type->valuestring, "llm") == 0) {
        cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
        if (cJSON_IsString(emotion) && emotion->valuestring) {
            xz_copy_str(s_app.last_emotion, sizeof(s_app.last_emotion), emotion->valuestring);
            PR_NOTICE("recv llm emotion: %s", s_app.last_emotion);
        }
    } else if (strcmp(type->valuestring, "alert") == 0) {
        cJSON *status  = cJSON_GetObjectItem(root, "status");
        cJSON *message = cJSON_GetObjectItem(root, "message");
        cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
        if (cJSON_IsString(status) && cJSON_IsString(message) && cJSON_IsString(emotion)) {
            char alert[256] = {0};
            (void)snprintf(alert, sizeof(alert), "%s:%s:%s", status->valuestring, message->valuestring,
                           emotion->valuestring);
            xz_copy_str(s_app.last_alert, sizeof(s_app.last_alert), alert);
            PR_NOTICE("recv alert: %s", s_app.last_alert);
        }
    } else if (strcmp(type->valuestring, "custom") == 0) {
        cJSON *custom_payload = cJSON_GetObjectItem(root, "payload");
        if (cJSON_IsObject(custom_payload)) {
            char *json = cJSON_PrintUnformatted(custom_payload);
            if (json) {
                xz_copy_str(s_app.last_custom, sizeof(s_app.last_custom), json);
                cJSON_free(json);
                PR_NOTICE("recv custom payload");
            }
        }
    } else if (strcmp(type->valuestring, "system") == 0) {
        cJSON *command = cJSON_GetObjectItem(root, "command");
        if (cJSON_IsString(command) && command->valuestring && strcmp(command->valuestring, "reboot") == 0) {
            s_app.pending_reboot = TRUE;
            PR_NOTICE("recv system command: reboot");
        }
    } else if (strcmp(type->valuestring, "mcp") == 0) {
        cJSON *mcp_payload = cJSON_GetObjectItem(root, "payload");
        if (cJSON_IsObject(mcp_payload)) {
            PR_NOTICE("recv mcp payload");
            char *reply_json = NULL;
            if (xiaozhi_mcp_handle_payload(mcp_payload, &reply_json) == OPRT_OK && reply_json) {
                (void)xz_send_mcp_payload_locked(reply_json);
                cJSON_free(reply_json);
            }
        }
    }

    cJSON_Delete(root);
}

static void xz_on_transport_text_message(void *userdata, const uint8_t *payload, size_t payload_len)
{
    (void)userdata;
    if (tal_mutex_lock(s_app.lock) != OPRT_OK) {
        return;
    }
    xz_handle_transport_text_locked(payload, payload_len);
    (void)tal_mutex_unlock(s_app.lock);
}

static void xz_on_transport_binary_message(void *userdata, const uint8_t *payload, size_t payload_len)
{
    (void)userdata;
#if XZ_APP_ENABLE_LINUX_AUDIO
    if (!payload || payload_len == 0) {
        return;
    }
    if (tal_mutex_lock(s_app.lock) != OPRT_OK) {
        return;
    }
    if (!xz_state_accepts_tts_binary(s_app.chat_state)) {
        PR_NOTICE("drop binary audio: chat=%s len=%u", xz_chat_state_to_text(s_app.chat_state), (unsigned)payload_len);
        (void)tal_mutex_unlock(s_app.lock);
        return;
    }

    PR_NOTICE("recv binary audio: chat=%s len=%u", xz_chat_state_to_text(s_app.chat_state), (unsigned)payload_len);
    int rt = xiaozhi_audio_linux_feed_opus(payload, payload_len);
    (void)tal_mutex_unlock(s_app.lock);
    if (rt != OPRT_OK) {
        PR_WARN("audio feed downlink opus failed: %d len=%u", rt, (unsigned)payload_len);
    }
#else
    (void)payload;
    (void)payload_len;
#endif
}

static void xz_worker_thread(void *arg)
{
    (void)arg;

    uint32_t retry_delay_ms = 1000;
    while (!s_app.stop_flag) {
        if (!xz_net_is_ready()) {
            (void)tal_mutex_lock(s_app.lock);
            (void)xz_disconnect_locked(FALSE);
            (void)tal_mutex_unlock(s_app.lock);
            tal_system_sleep(500);
            continue;
        }

        (void)tal_mutex_lock(s_app.lock);
        if (!s_app.ota_checked) {
            (void)tal_mutex_unlock(s_app.lock);
            OPERATE_RET ort = xz_ota_bootstrap_once();
            (void)tal_mutex_lock(s_app.lock);

            s_app.ota_checked     = TRUE;
            s_app.ota_retry_count = 0;
            s_app.reconnect_flag  = TRUE;

            if (ort != OPRT_OK) {
                PR_WARN("ota bootstrap failed: %d, continue with local config", ort);
            }
        }

        if (s_app.reconnect_flag) {
            s_app.reconnect_flag = FALSE;
            (void)xz_disconnect_locked(FALSE);
        }

        BOOL_T connected = xz_is_connected_locked();
        (void)tal_mutex_unlock(s_app.lock);

        OPERATE_RET rt = OPRT_OK;
        if (!connected) {
            (void)tal_mutex_lock(s_app.lock);
            rt = xz_connect_locked();
            (void)tal_mutex_unlock(s_app.lock);

            if (rt != OPRT_OK) {
                PR_WARN("xiaozhi connect failed: %d", rt);
                tal_system_sleep(retry_delay_ms);
                if (retry_delay_ms < 10000) {
                    retry_delay_ms += 1000;
                }
                continue;
            }

            retry_delay_ms = 1000;
            continue;
        }

        (void)tal_mutex_lock(s_app.lock);
        rt = xz_poll_locked(50);
        if (rt == OPRT_RESOURCE_NOT_READY) {
            rt = OPRT_OK;
        }

        if (rt != OPRT_OK) {
            PR_WARN("xiaozhi poll error: %d", rt);
            (void)xz_disconnect_locked(FALSE);
        }
        BOOL_T do_reboot = s_app.pending_reboot;
        if (do_reboot) {
            s_app.pending_reboot = FALSE;
        }
        char upgrade_url[384] = {0};
        if (s_app.pending_upgrade_url[0] != '\0') {
            xz_copy_str(upgrade_url, sizeof(upgrade_url), s_app.pending_upgrade_url);
            s_app.pending_upgrade_url[0] = '\0';
        }
        (void)tal_mutex_unlock(s_app.lock);

        if (upgrade_url[0] != '\0') {
            OPERATE_RET urt = xiaozhi_app_upgrade_firmware(upgrade_url);
            if (urt != OPRT_OK) {
                PR_WARN("mcp upgrade failed: %d url=%s", urt, upgrade_url);
            }
        }

        if (do_reboot) {
            tal_system_sleep(300);
            tal_system_reset();
        }

        if (rt != OPRT_OK) {
            tal_system_sleep(retry_delay_ms);
            if (retry_delay_ms < 10000) {
                retry_delay_ms += 1000;
            }
        } else {
            retry_delay_ms = 1000;
            tal_system_sleep(20);
        }
    }

    (void)tal_mutex_lock(s_app.lock);
    (void)xz_disconnect_locked(TRUE);
    (void)tal_mutex_unlock(s_app.lock);

    s_app.running = FALSE;
    s_app.worker  = NULL;
}

OPERATE_RET xiaozhi_app_apply_wifi_settings(void)
{
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    char ssid[WIFI_SSID_LEN + 1]   = {0};
    char pass[WIFI_PASSWD_LEN + 1] = {0};

    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "ssid", ssid, sizeof(ssid), "");
    (void)xiaozhi_settings_get_string(XZ_NS_WIFI, "password", pass, sizeof(pass), "");
    if (ssid[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    netconn_wifi_info_t info = {0};
    (void)snprintf(info.ssid, sizeof(info.ssid), "%s", ssid);
    (void)snprintf(info.pswd, sizeof(info.pswd), "%s", pass);
    return netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &info);
#else
    return OPRT_NOT_SUPPORTED;
#endif
}

OPERATE_RET xiaozhi_app_init(void)
{
    if (s_app.inited) {
        return OPRT_OK;
    }

    OPERATE_RET rt = xz_netmgr_init_once();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = tal_mutex_create_init(&s_app.lock);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = xz_uplink_audio_queue_init();
    if (rt != OPRT_OK) {
        return rt;
    }

#if XZ_APP_ENABLE_LINUX_AUDIO
    rt = xiaozhi_audio_linux_init();
    if (rt != OPRT_OK) {
        PR_ERR("audio runtime init failed: %d", rt);
        return rt;
    }
    xiaozhi_audio_linux_set_hotword_callback(xz_on_linux_hotword_detected, &s_app);
    (void)xz_audio_reset_locked();
#endif

    (void)xiaozhi_settings_init_defaults();

    char cid[96] = {0};
    (void)xiaozhi_system_get_client_id(cid, sizeof(cid));

    rt = xz_ws_init(&s_app.ws);
    if (rt != OPRT_OK) {
        return rt;
    }
    (void)xz_ws_set_text_message_callback(&s_app.ws, xz_on_transport_text_message, &s_app);
    (void)xz_ws_set_binary_message_callback(&s_app.ws, xz_on_transport_binary_message, &s_app);

    rt = xz_mqtt_udp_init(&s_app.mqtt_udp);
    if (rt != OPRT_OK) {
        return rt;
    }
    (void)xz_mqtt_udp_set_text_message_callback(&s_app.mqtt_udp, xz_on_transport_text_message, &s_app);

    (void)xiaozhi_mcp_init(&(xz_mcp_ops_t){
        .get_device_status       = xz_mcp_make_status_json,
        .get_system_info         = xz_mcp_make_system_info_json,
        .request_reboot          = xz_mcp_request_reboot,
        .request_upgrade         = xz_mcp_request_upgrade,
        .set_vision_capabilities = xz_mcp_set_vision_capabilities,
        .userdata                = &s_app,
    });

    (void)xiaozhi_app_apply_wifi_settings();

    xz_set_chat_state_locked(XZ_CHAT_IDLE);
    s_app.listen_mode            = XZ_LISTEN_MODE_MANUAL;
    s_app.pending_reboot         = FALSE;
    s_app.pending_upgrade_url[0] = '\0';

    s_app.inited = TRUE;
    return OPRT_OK;
}

OPERATE_RET xiaozhi_app_start(void)
{
    if (!s_app.inited) {
        OPERATE_RET rt = xiaozhi_app_init();
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    if (s_app.running) {
        return OPRT_OK;
    }

    s_app.stop_flag       = FALSE;
    s_app.reconnect_flag  = FALSE;
    s_app.ota_checked     = FALSE;
    s_app.ota_retry_count = 0;
    xz_set_chat_state_locked(XZ_CHAT_IDLE);
    s_app.listen_mode            = XZ_LISTEN_MODE_MANUAL;
    s_app.pending_reboot         = FALSE;
    s_app.pending_upgrade_url[0] = '\0';
    (void)xz_audio_reset_locked();
    xz_uplink_audio_queue_clear();

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth   = 1024 * 8;
    cfg.priority     = THREAD_PRIO_1;
    cfg.thrdname     = "xz_worker";

    OPERATE_RET rt = tal_thread_create_and_start(&s_app.worker, NULL, NULL, xz_worker_thread, NULL, &cfg);
    if (rt == OPRT_OK) {
        s_app.running = TRUE;
    }

    return rt;
}

OPERATE_RET xiaozhi_app_stop(void)
{
    if (!s_app.running) {
        return OPRT_OK;
    }

    s_app.stop_flag = TRUE;
    for (int i = 0; i < 50; ++i) {
        if (!s_app.running) {
            return OPRT_OK;
        }
        tal_system_sleep(100);
    }

    return OPRT_TIMEOUT;
}

OPERATE_RET xiaozhi_app_reconnect(void)
{
    if (!s_app.inited) {
        return OPRT_COM_ERROR;
    }

    (void)tal_mutex_lock(s_app.lock);
    s_app.reconnect_flag = TRUE;
    (void)tal_mutex_unlock(s_app.lock);
    return OPRT_OK;
}

OPERATE_RET xiaozhi_app_ota_bootstrap(void)
{
    OPERATE_RET rt = xz_ota_bootstrap_once();
    if (rt != OPRT_OK) {
        return rt;
    }

    (void)tal_mutex_lock(s_app.lock);
    s_app.ota_checked     = TRUE;
    s_app.ota_retry_count = 0;
    s_app.reconnect_flag  = TRUE;
    (void)tal_mutex_unlock(s_app.lock);

    return OPRT_OK;
}

OPERATE_RET xiaozhi_app_get_status(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    char proto[24] = {0};
    (void)xiaozhi_settings_get_proto(proto, sizeof(proto));

    BOOL_T net_ready = xz_net_is_ready();

    const char *active      = "none";
    const char *state       = "idle";
    const char *sid         = "";
    const char *chat_state  = "idle";
    const char *listen_mode = "manual";

    (void)tal_mutex_lock(s_app.lock);
    if (s_app.active == XZ_ACTIVE_WS) {
        active = "websocket";
        state  = (s_app.ws.state == XZ_CONN_READY) ? "ready"
                                                   : ((s_app.ws.state == XZ_CONN_CONNECTING) ? "connecting" : "idle");
        sid    = s_app.ws.channel.session_id;
    } else if (s_app.active == XZ_ACTIVE_MQTT_UDP) {
        active = "mqtt-udp";
        state  = (s_app.mqtt_udp.state == XZ_CONN_READY)
                     ? "ready"
                     : ((s_app.mqtt_udp.state == XZ_CONN_CONNECTING) ? "connecting" : "idle");
        sid    = s_app.mqtt_udp.channel.session_id;
    }
    chat_state  = xz_chat_state_to_text(s_app.chat_state);
    listen_mode = xz_listen_mode_to_text(s_app.listen_mode);
    (void)tal_mutex_unlock(s_app.lock);

    (void)snprintf(buf, buf_size,
                   "running=%d net=%s ota_checked=%d config_proto=%s active=%s state=%s chat=%s mode=%s session_id=%s",
                   s_app.running, net_ready ? "up" : "down", s_app.ota_checked, proto, active, state, chat_state,
                   listen_mode, sid ? sid : "");
    return OPRT_OK;
}

OPERATE_RET xiaozhi_app_send_listen(const char *state, const char *mode, const char *text)
{
    if (!state || state[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = OPRT_INVALID_PARM;
    (void)tal_mutex_lock(s_app.lock);
    rt = xz_send_listen_transport_locked(state, mode, text);
    if (rt == OPRT_OK) {
        if (strcmp(state, "start") == 0) {
            rt = xz_on_listen_start_locked(mode);
        } else if (strcmp(state, "stop") == 0) {
            rt = xz_on_listen_stop_locked();
        }
    }
    (void)tal_mutex_unlock(s_app.lock);

    return rt;
}

OPERATE_RET xiaozhi_app_send_abort(const char *reason)
{
    OPERATE_RET rt = OPRT_INVALID_PARM;
    (void)tal_mutex_lock(s_app.lock);
    rt = xz_send_abort_transport_locked(reason);
    if (rt == OPRT_OK) {
        rt = xz_on_abort_locked();
    }
    (void)tal_mutex_unlock(s_app.lock);

    return rt;
}

OPERATE_RET xiaozhi_app_send_mcp(const char *payload_json)
{
    OPERATE_RET rt = OPRT_INVALID_PARM;
    (void)tal_mutex_lock(s_app.lock);
    if (s_app.active == XZ_ACTIVE_WS) {
        rt = xz_ws_send_mcp(&s_app.ws, payload_json);
    } else if (s_app.active == XZ_ACTIVE_MQTT_UDP) {
        rt = xz_mqtt_udp_send_mcp(&s_app.mqtt_udp, payload_json);
    }
    (void)tal_mutex_unlock(s_app.lock);

    return rt;
}

OPERATE_RET xiaozhi_app_upgrade_firmware(const char *url)
{
    if (!url || url[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("firmware upgrade start url=%s", url);
    (void)tal_mutex_lock(s_app.lock);
    (void)xz_disconnect_locked(TRUE);
    (void)tal_mutex_unlock(s_app.lock);

    OPERATE_RET rt = xiaozhi_upgrade_from_url(url);
    if (rt != OPRT_OK) {
        PR_ERR("firmware upgrade failed: %d", rt);
        return rt;
    }

    PR_NOTICE("firmware upgrade success, rebooting");
    tal_system_sleep(300);
    tal_system_reset();

    return OPRT_OK;
}

OPERATE_RET xiaozhi_app_start_detect(void)
{
    if (!s_app.inited) {
        return OPRT_COM_ERROR;
    }

    (void)tal_mutex_lock(s_app.lock);
    xz_set_chat_state_locked(XZ_CHAT_IDLE);
    OPERATE_RET rt = xz_audio_start_detect_locked();
    (void)tal_mutex_unlock(s_app.lock);
    return rt;
}

OPERATE_RET xiaozhi_app_stop_detect(void)
{
    if (!s_app.inited) {
        return OPRT_COM_ERROR;
    }

    (void)tal_mutex_lock(s_app.lock);
    OPERATE_RET rt = xz_audio_stop_detect_locked();
    (void)tal_mutex_unlock(s_app.lock);
    return rt;
}
