#include "telegram_bot.h"

#include "bus/message_bus.h"
#include "cJSON.h"
#include "http_client_interface.h"
#include "iotdns.h"
#include "mimi_config.h"

static const char *TAG = "telegram";
static char s_bot_token[128] = {0};
static int64_t s_update_offset = 0;
static THREAD_HANDLE s_poll_thread = NULL;
static uint8_t *s_tg_cacert = NULL;
static uint16_t s_tg_cacert_len = 0;

#define TG_HOST "api.telegram.org"
#define TG_HTTP_TIMEOUT_MS ((MIMI_TG_POLL_TIMEOUT_S + 5) * 1000)
#define TG_HTTP_RESP_BUF_SIZE (16 * 1024)

static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

static OPERATE_RET ensure_tg_cert(void)
{
    if (s_tg_cacert && s_tg_cacert_len > 0) {
        return OPRT_OK;
    }

    OPERATE_RET rt = tuya_iotdns_query_domain_certs((char *)TG_HOST, &s_tg_cacert, &s_tg_cacert_len);
    if (rt != OPRT_OK || !s_tg_cacert || s_tg_cacert_len == 0) {
        MIMI_LOGE(TAG, "query cert failed rt=%d", rt);
        return (rt == OPRT_OK) ? OPRT_COM_ERROR : rt;
    }

    return OPRT_OK;
}

static OPERATE_RET tg_http_call(const char *path, const char *post_data,
                                char *resp_buf, size_t resp_buf_size, uint16_t *status_code)
{
    if (!path || !resp_buf || resp_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = ensure_tg_cert();
    if (rt != OPRT_OK) {
        return rt;
    }

    http_client_header_t headers[1] = {0};
    uint8_t header_count = 0;
    if (post_data) {
        headers[header_count++] = (http_client_header_t){
            .key = "Content-Type",
            .value = "application/json",
        };
    }

    http_client_response_t response = {0};
    http_client_status_t http_rt = http_client_request(
        &(const http_client_request_t){
            .cacert = s_tg_cacert,
            .cacert_len = s_tg_cacert_len,
            .host = TG_HOST,
            .port = 443,
            .method = post_data ? "POST" : "GET",
            .path = path,
            .headers = headers,
            .headers_count = header_count,
            .body = (const uint8_t *)(post_data ? post_data : ""),
            .body_length = post_data ? strlen(post_data) : 0,
            .timeout_ms = TG_HTTP_TIMEOUT_MS,
        },
        &response);
    if (http_rt != HTTP_CLIENT_SUCCESS) {
        MIMI_LOGE(TAG, "http request failed: %d path=%s", http_rt, path);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (status_code) {
        *status_code = response.status_code;
    }

    resp_buf[0] = '\0';
    if (response.body && response.body_length > 0) {
        size_t copy = (response.body_length < resp_buf_size - 1) ? response.body_length : (resp_buf_size - 1);
        memcpy(resp_buf, response.body, copy);
        resp_buf[copy] = '\0';
    }

    http_client_free(&response);
    return OPRT_OK;
}

static bool tg_response_ok(const char *json_str)
{
    if (!json_str || json_str[0] == '\0') {
        return false;
    }

    bool ok = false;
    cJSON *root = cJSON_Parse(json_str);
    if (root) {
        cJSON *ok_field = cJSON_GetObjectItem(root, "ok");
        ok = cJSON_IsTrue(ok_field);
        cJSON_Delete(root);
    }
    return ok;
}

static void process_updates(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return;
    }

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsTrue(ok) || !cJSON_IsArray(result)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *update = NULL;
    cJSON_ArrayForEach(update, result) {
        cJSON *update_id = cJSON_GetObjectItem(update, "update_id");
        if (cJSON_IsNumber(update_id)) {
            int64_t uid = (int64_t)update_id->valuedouble;
            if (uid >= s_update_offset) {
                s_update_offset = uid + 1;
            }
        }

        cJSON *message = cJSON_GetObjectItem(update, "message");
        cJSON *text = message ? cJSON_GetObjectItem(message, "text") : NULL;
        cJSON *chat = message ? cJSON_GetObjectItem(message, "chat") : NULL;
        cJSON *chat_id = chat ? cJSON_GetObjectItem(chat, "id") : NULL;
        if (!cJSON_IsString(text) || !text->valuestring || !chat_id) {
            continue;
        }

        char chat_id_str[32] = {0};
        if (cJSON_IsString(chat_id) && chat_id->valuestring) {
            safe_copy(chat_id_str, sizeof(chat_id_str), chat_id->valuestring);
        } else if (cJSON_IsNumber(chat_id)) {
            snprintf(chat_id_str, sizeof(chat_id_str), "%.0f", chat_id->valuedouble);
        } else {
            continue;
        }

        mimi_msg_t msg = {0};
        strncpy(msg.channel, MIMI_CHAN_TELEGRAM, sizeof(msg.channel) - 1);
        strncpy(msg.chat_id, chat_id_str, sizeof(msg.chat_id) - 1);
        msg.content = strdup(text->valuestring);
        if (!msg.content) {
            continue;
        }

        OPERATE_RET rt = message_bus_push_inbound(&msg);
        if (rt != OPRT_OK) {
            MIMI_LOGW(TAG, "push inbound failed rt=%d", rt);
            free(msg.content);
        }
    }

    cJSON_Delete(root);
}

static void telegram_poll_task(void *arg)
{
    (void)arg;
    MIMI_LOGI(TAG, "telegram poll task started");

    while (1) {
        if (s_bot_token[0] == '\0') {
            tal_system_sleep(3000);
            continue;
        }

        char path[320] = {0};
        int n = snprintf(path, sizeof(path), "/bot%s/getUpdates?offset=%lld&timeout=%d",
                         s_bot_token, (long long)s_update_offset, MIMI_TG_POLL_TIMEOUT_S);
        if (n <= 0 || (size_t)n >= sizeof(path)) {
            MIMI_LOGE(TAG, "getUpdates path too long");
            tal_system_sleep(3000);
            continue;
        }

        char resp[TG_HTTP_RESP_BUF_SIZE] = {0};
        uint16_t status = 0;
        OPERATE_RET rt = tg_http_call(path, NULL, resp, sizeof(resp), &status);
        if (rt != OPRT_OK || status != 200) {
            MIMI_LOGW(TAG, "getUpdates failed rt=%d http=%u", rt, status);
            tal_system_sleep(2000);
            continue;
        }

        process_updates(resp);
    }
}

OPERATE_RET telegram_bot_init(void)
{
    if (MIMI_SECRET_TG_TOKEN[0] != '\0') {
        safe_copy(s_bot_token, sizeof(s_bot_token), MIMI_SECRET_TG_TOKEN);
    }

    char tmp[128] = {0};
    if (mimi_kv_get_string(MIMI_NVS_TG, MIMI_NVS_KEY_TG_TOKEN, tmp, sizeof(tmp)) == OPRT_OK) {
        safe_copy(s_bot_token, sizeof(s_bot_token), tmp);
    }

    MIMI_LOGI(TAG, "telegram init token=%s", s_bot_token[0] ? "configured" : "empty");
    return OPRT_OK;
}

OPERATE_RET telegram_bot_start(void)
{
    if (s_bot_token[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    if (s_poll_thread) {
        return OPRT_OK;
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth = MIMI_TG_POLL_STACK;
    cfg.priority = THREAD_PRIO_1;
    cfg.thrdname = "mimi_tg_poll";

    OPERATE_RET rt = tal_thread_create_and_start(&s_poll_thread, NULL, NULL,
                                                 telegram_poll_task, NULL, &cfg);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "create poll thread failed: %d", rt);
        return rt;
    }

    return OPRT_OK;
}

OPERATE_RET telegram_send_message(const char *chat_id, const char *text)
{
    if (!chat_id || !text) {
        return OPRT_INVALID_PARM;
    }

    if (s_bot_token[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    size_t text_len = strlen(text);
    size_t offset = 0;

    while (offset < text_len || (text_len == 0 && offset == 0)) {
        size_t chunk = text_len - offset;
        if (chunk > MIMI_TG_MAX_MSG_LEN) {
            chunk = MIMI_TG_MAX_MSG_LEN;
        }
        if (text_len == 0) {
            chunk = 0;
        }

        char *segment = calloc(1, chunk + 1);
        if (!segment) {
            return OPRT_MALLOC_FAILED;
        }
        if (chunk > 0) {
            memcpy(segment, text + offset, chunk);
        }
        segment[chunk] = '\0';

        cJSON *body = cJSON_CreateObject();
        if (!body) {
            free(segment);
            return OPRT_MALLOC_FAILED;
        }
        cJSON_AddStringToObject(body, "chat_id", chat_id);
        cJSON_AddStringToObject(body, "text", segment);
        cJSON_AddStringToObject(body, "parse_mode", "Markdown");
        char *json = cJSON_PrintUnformatted(body);
        cJSON_Delete(body);

        char path[256] = {0};
        int n = snprintf(path, sizeof(path), "/bot%s/sendMessage", s_bot_token);
        if (n <= 0 || (size_t)n >= sizeof(path)) {
            free(segment);
            free(json);
            return OPRT_BUFFER_NOT_ENOUGH;
        }

        char resp[TG_HTTP_RESP_BUF_SIZE] = {0};
        uint16_t status = 0;
        OPERATE_RET rt = OPRT_MALLOC_FAILED;
        if (json) {
            rt = tg_http_call(path, json, resp, sizeof(resp), &status);
        }
        free(json);

        if (rt != OPRT_OK || status != 200 || !tg_response_ok(resp)) {
            cJSON *body2 = cJSON_CreateObject();
            if (!body2) {
                free(segment);
                return OPRT_MALLOC_FAILED;
            }
            cJSON_AddStringToObject(body2, "chat_id", chat_id);
            cJSON_AddStringToObject(body2, "text", segment);
            char *json2 = cJSON_PrintUnformatted(body2);
            cJSON_Delete(body2);

            if (!json2) {
                free(segment);
                return OPRT_MALLOC_FAILED;
            }

            memset(resp, 0, sizeof(resp));
            status = 0;
            rt = tg_http_call(path, json2, resp, sizeof(resp), &status);
            free(json2);
            if (rt != OPRT_OK || status != 200 || !tg_response_ok(resp)) {
                free(segment);
                return OPRT_COM_ERROR;
            }
        }

        free(segment);
        if (text_len == 0) {
            break;
        }
        offset += chunk;
    }

    return OPRT_OK;
}

OPERATE_RET telegram_set_token(const char *token)
{
    if (!token) {
        return OPRT_INVALID_PARM;
    }

    safe_copy(s_bot_token, sizeof(s_bot_token), token);
    s_update_offset = 0;
    return mimi_kv_set_string(MIMI_NVS_TG, MIMI_NVS_KEY_TG_TOKEN, token);
}
