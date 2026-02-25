#include "tls_cert_bundle.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "http_client_interface.h"
#include "iotdns.h"
#include "mimi_config.h"
#include "tal_fs.h"
#include "tal_memory.h"
#include "tal_system.h"

static const char *TAG = "tls_bundle";

#define MIMI_TLS_CERT_QUERY_RETRY_COUNT    3
#define MIMI_TLS_CERT_QUERY_RETRY_BASE_MS  400
#define MIMI_TLS_CERT_QUERY_RETRY_MAX_MS   1600

typedef struct {
    char host[96];
    char path[192];
    uint16_t port;
    bool use_tls;
} mimi_url_t;

typedef struct {
    char host[96];
    OPERATE_RET last_rt;
    uint32_t failed_at_ms;
    uint32_t last_log_ms;
    bool used;
} mimi_tls_fail_cache_t;

static uint32_t s_global_ca_last_fetch_ms = 0;
static mimi_tls_fail_cache_t s_fail_cache[MIMI_TLS_CERT_FAIL_CACHE_SLOTS] = {0};
static uint8_t s_fail_cache_next_slot = 0;

static void extract_host(const char *host_or_url, char *host, size_t host_size)
{
    if (!host || host_size == 0) {
        return;
    }
    host[0] = '\0';
    if (!host_or_url || host_or_url[0] == '\0') {
        return;
    }

    const char *begin = host_or_url;
    const char *scheme = strstr(host_or_url, "://");
    if (scheme) {
        begin = scheme + 3;
    }

    size_t copy = strcspn(begin, "/:");
    if (copy >= host_size) {
        copy = host_size - 1;
    }
    memcpy(host, begin, copy);
    host[copy] = '\0';
}

static bool should_retry_iotdns_query(OPERATE_RET rt)
{
    return (rt == OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR || rt == OPRT_RESOURCE_NOT_READY || rt == OPRT_TIMEOUT);
}

static uint32_t cert_query_retry_delay_ms(uint32_t attempt)
{
    uint32_t delay = MIMI_TLS_CERT_QUERY_RETRY_BASE_MS;
    while (attempt > 0 && delay < MIMI_TLS_CERT_QUERY_RETRY_MAX_MS) {
        if (delay > (MIMI_TLS_CERT_QUERY_RETRY_MAX_MS / 2)) {
            delay = MIMI_TLS_CERT_QUERY_RETRY_MAX_MS;
            break;
        }
        delay <<= 1;
        attempt--;
    }
    if (delay > MIMI_TLS_CERT_QUERY_RETRY_MAX_MS) {
        delay = MIMI_TLS_CERT_QUERY_RETRY_MAX_MS;
    }
    return delay;
}

static OPERATE_RET mimi_parse_url(const char *url, mimi_url_t *out)
{
    if (!url || !out || url[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    memset(out, 0, sizeof(*out));
    out->port = 80;
    out->use_tls = false;

    const char *cursor = url;
    if (strncmp(cursor, "https://", 8) == 0) {
        out->use_tls = true;
        out->port = 443;
        cursor += 8;
    } else if (strncmp(cursor, "http://", 7) == 0) {
        cursor += 7;
    } else {
        return OPRT_INVALID_PARM;
    }

    const char *path = strchr(cursor, '/');
    const char *host_end = path ? path : (cursor + strlen(cursor));
    const char *port_sep = NULL;
    for (const char *p = cursor; p < host_end; p++) {
        if (*p == ':') {
            port_sep = p;
            break;
        }
    }

    size_t host_len = port_sep ? (size_t)(port_sep - cursor) : (size_t)(host_end - cursor);
    if (host_len == 0 || host_len >= sizeof(out->host)) {
        return OPRT_INVALID_PARM;
    }
    memcpy(out->host, cursor, host_len);
    out->host[host_len] = '\0';

    if (port_sep) {
        int p = atoi(port_sep + 1);
        if (p <= 0 || p > 65535) {
            return OPRT_INVALID_PARM;
        }
        out->port = (uint16_t)p;
    }

    if (path && path[0] != '\0') {
        size_t path_len = strlen(path);
        if (path_len >= sizeof(out->path)) {
            return OPRT_INVALID_PARM;
        }
        memcpy(out->path, path, path_len);
        out->path[path_len] = '\0';
    } else {
        out->path[0] = '/';
        out->path[1] = '\0';
    }

    return OPRT_OK;
}

static mimi_tls_fail_cache_t *fail_cache_find(const char *host)
{
    if (!host || host[0] == '\0') {
        return NULL;
    }
    for (uint32_t i = 0; i < MIMI_TLS_CERT_FAIL_CACHE_SLOTS; i++) {
        if (s_fail_cache[i].used && strcmp(s_fail_cache[i].host, host) == 0) {
            return &s_fail_cache[i];
        }
    }
    return NULL;
}

static void fail_cache_clear(const char *host)
{
    mimi_tls_fail_cache_t *slot = fail_cache_find(host);
    if (!slot) {
        return;
    }
    memset(slot, 0, sizeof(*slot));
}

static void fail_cache_save(const char *host, OPERATE_RET rt, uint32_t now_ms)
{
    if (!host || host[0] == '\0') {
        return;
    }

    mimi_tls_fail_cache_t *slot = fail_cache_find(host);
    if (!slot) {
        slot = &s_fail_cache[s_fail_cache_next_slot];
        s_fail_cache_next_slot = (uint8_t)((s_fail_cache_next_slot + 1) % MIMI_TLS_CERT_FAIL_CACHE_SLOTS);
        memset(slot, 0, sizeof(*slot));
    }

    snprintf(slot->host, sizeof(slot->host), "%s", host);
    slot->last_rt = rt;
    slot->failed_at_ms = now_ms;
    slot->used = true;
}

static bool fail_cache_hit(const char *host, OPERATE_RET *cached_rt, uint32_t now_ms)
{
    mimi_tls_fail_cache_t *slot = fail_cache_find(host);
    if (!slot) {
        return false;
    }

    if ((uint32_t)(now_ms - slot->failed_at_ms) >= MIMI_TLS_CERT_FAIL_RETRY_INTERVAL_MS) {
        memset(slot, 0, sizeof(*slot));
        return false;
    }

    if (cached_rt) {
        *cached_rt = slot->last_rt;
    }

    if ((slot->last_log_ms == 0) || ((uint32_t)(now_ms - slot->last_log_ms) >= MIMI_TLS_CERT_FAIL_LOG_INTERVAL_MS)) {
        MIMI_LOGD(TAG, "skip iotdns host=%s due to fail cache rt=%d age_ms=%u", host, slot->last_rt,
                  (unsigned)(now_ms - slot->failed_at_ms));
        slot->last_log_ms = now_ms;
    }

    return true;
}

static OPERATE_RET mimi_tls_load_global_ca_bundle(uint8_t **cacert, size_t *cacert_len)
{
#if MIMI_TLS_GLOBAL_CA_ENABLE
    if (!cacert || !cacert_len) {
        return OPRT_INVALID_PARM;
    }

    const char *path = MIMI_TLS_GLOBAL_CA_BUNDLE_PATH;
    if (!path || path[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    int file_size = tal_fgetsize(path);
    if (file_size <= 0) {
        return OPRT_FILE_NOT_FIND;
    }

    if ((size_t)file_size > (size_t)MIMI_TLS_GLOBAL_CA_BUNDLE_MAX_BYTES) {
        MIMI_LOGE(TAG, "global ca bundle too large path=%s size=%d max=%d", path, file_size,
                  (int)MIMI_TLS_GLOBAL_CA_BUNDLE_MAX_BYTES);
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    TUYA_FILE f = tal_fopen(path, "r");
    if (!f) {
        return OPRT_FILE_OPEN_FAILED;
    }

    uint8_t *buf = tal_calloc(1, (size_t)file_size + 1);
    if (!buf) {
        tal_fclose(f);
        return OPRT_MALLOC_FAILED;
    }

    int n = tal_fread(buf, file_size, f);
    tal_fclose(f);
    if (n != file_size) {
        tal_free(buf);
        return OPRT_FILE_READ_FAILED;
    }

    if (strstr((const char *)buf, "-----BEGIN CERTIFICATE-----") == NULL) {
        tal_free(buf);
        return OPRT_INVALID_PARM;
    }

    *cacert = buf;
    *cacert_len = (size_t)n + 1;
    return OPRT_OK;
#else
    (void)cacert;
    (void)cacert_len;
    return OPRT_NOT_SUPPORTED;
#endif
}

static OPERATE_RET mimi_tls_store_global_ca_bundle(const uint8_t *data, size_t len)
{
#if MIMI_TLS_GLOBAL_CA_ENABLE
    if (!data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    TUYA_FILE f = tal_fopen(MIMI_TLS_GLOBAL_CA_BUNDLE_PATH, "w");
    if (!f) {
        return OPRT_FILE_OPEN_FAILED;
    }

    if ((len - 1) > (size_t)INT_MAX) {
        tal_fclose(f);
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    int wn = tal_fwrite((void *)data, (int)(len - 1), f);
    tal_fclose(f);
    if (wn != (int)(len - 1)) {
        return OPRT_FILE_WRITE_FAILED;
    }

    return OPRT_OK;
#else
    (void)data;
    (void)len;
    return OPRT_NOT_SUPPORTED;
#endif
}

static OPERATE_RET mimi_tls_download_global_ca_bundle(uint8_t **cacert, size_t *cacert_len)
{
#if MIMI_TLS_GLOBAL_CA_ENABLE
    if (!cacert || !cacert_len) {
        return OPRT_INVALID_PARM;
    }

    const char *url = MIMI_TLS_GLOBAL_CA_BUNDLE_URL;
    if (!url || url[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    uint32_t now = tal_system_get_millisecond();
    if (s_global_ca_last_fetch_ms != 0 &&
        (uint32_t)(now - s_global_ca_last_fetch_ms) < MIMI_TLS_GLOBAL_CA_BUNDLE_FETCH_INTERVAL_MS) {
        return OPRT_RESOURCE_NOT_READY;
    }
    s_global_ca_last_fetch_ms = now;

    mimi_url_t u = {0};
    OPERATE_RET rt = mimi_parse_url(url, &u);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "invalid global ca bundle url=%s", url);
        return rt;
    }

    uint8_t *fetch_cacert = NULL;
    uint16_t fetch_cacert_len = 0;
    if (u.use_tls) {
        rt = tuya_iotdns_query_domain_certs((char *)url, &fetch_cacert, &fetch_cacert_len);
        if (rt != OPRT_OK || !fetch_cacert || fetch_cacert_len == 0) {
            if (fetch_cacert) {
                tal_free(fetch_cacert);
            }
            return (rt == OPRT_OK) ? OPRT_COM_ERROR : rt;
        }
    }

    http_client_response_t response = {0};
    http_client_status_t http_rt = http_client_request(
        &(const http_client_request_t){
            .cacert = fetch_cacert,
            .cacert_len = fetch_cacert_len,
            .host = u.host,
            .port = u.port,
            .method = "GET",
            .path = u.path,
            .headers = NULL,
            .headers_count = 0,
            .body = (const uint8_t *)"",
            .body_length = 0,
            .timeout_ms = MIMI_TLS_GLOBAL_CA_BUNDLE_FETCH_TIMEOUT_MS,
        },
        &response);

    if (fetch_cacert) {
        tal_free(fetch_cacert);
    }

    if (http_rt != HTTP_CLIENT_SUCCESS) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (response.status_code != 200 || !response.body || response.body_length == 0) {
        http_client_free(&response);
        return OPRT_COM_ERROR;
    }

    if (response.body_length > MIMI_TLS_GLOBAL_CA_BUNDLE_MAX_BYTES) {
        http_client_free(&response);
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    uint8_t *buf = tal_calloc(1, response.body_length + 1);
    if (!buf) {
        http_client_free(&response);
        return OPRT_MALLOC_FAILED;
    }

    memcpy(buf, response.body, response.body_length);
    buf[response.body_length] = '\0';
    http_client_free(&response);

    if (strstr((const char *)buf, "-----BEGIN CERTIFICATE-----") == NULL) {
        tal_free(buf);
        return OPRT_INVALID_PARM;
    }

    *cacert = buf;
    *cacert_len = response.body_length + 1;
    (void)mimi_tls_store_global_ca_bundle(*cacert, *cacert_len);

    MIMI_LOGW(TAG, "downloaded global ca bundle from %s len=%zu", url, *cacert_len);
    return OPRT_OK;
#else
    (void)cacert;
    (void)cacert_len;
    return OPRT_NOT_SUPPORTED;
#endif
}

OPERATE_RET mimi_tls_query_domain_certs(const char *host_or_url, uint8_t **cacert, size_t *cacert_len)
{
    if (!host_or_url || !cacert || !cacert_len) {
        return OPRT_INVALID_PARM;
    }

    *cacert = NULL;
    *cacert_len = 0;

    char host[96] = {0};
    extract_host(host_or_url, host, sizeof(host));
    uint32_t now_ms = tal_system_get_millisecond();

    OPERATE_RET cached_rt = OPRT_COM_ERROR;
    if (fail_cache_hit(host, &cached_rt, now_ms)) {
        return (cached_rt == OPRT_OK) ? OPRT_COM_ERROR : cached_rt;
    }

    OPERATE_RET rt = OPRT_COM_ERROR;
    for (uint32_t attempt = 0; attempt < MIMI_TLS_CERT_QUERY_RETRY_COUNT; attempt++) {
        uint8_t *iotdns_cert = NULL;
        uint16_t iotdns_cert_len = 0;

        rt = tuya_iotdns_query_domain_certs((char *)host_or_url, &iotdns_cert, &iotdns_cert_len);
        if (rt == OPRT_OK && iotdns_cert && iotdns_cert_len > 0) {
            *cacert = iotdns_cert;
            *cacert_len = iotdns_cert_len;
            fail_cache_clear(host);
            return OPRT_OK;
        }

        if (iotdns_cert) {
            tal_free(iotdns_cert);
        }

        if (attempt + 1 >= MIMI_TLS_CERT_QUERY_RETRY_COUNT || !should_retry_iotdns_query(rt)) {
            break;
        }

        uint32_t delay_ms = cert_query_retry_delay_ms(attempt);
        MIMI_LOGW(TAG, "iotdns cert query retry %u/%u host=%s rt=%d delay=%u", (unsigned)(attempt + 1),
                  (unsigned)MIMI_TLS_CERT_QUERY_RETRY_COUNT, host_or_url, rt, (unsigned)delay_ms);
        tal_system_sleep(delay_ms);
    }

    OPERATE_RET global_rt = mimi_tls_load_global_ca_bundle(cacert, cacert_len);
    if (global_rt == OPRT_OK && *cacert && *cacert_len > 0) {
        fail_cache_clear(host);
        MIMI_LOGW(TAG, "iotdns cert unavailable host=%s rt=%d, fallback to global ca bundle path=%s len=%zu", host, rt,
                  MIMI_TLS_GLOBAL_CA_BUNDLE_PATH, *cacert_len);
        return OPRT_OK;
    }

    OPERATE_RET fetch_rt = mimi_tls_download_global_ca_bundle(cacert, cacert_len);
    if (fetch_rt == OPRT_OK && *cacert && *cacert_len > 0) {
        fail_cache_clear(host);
        MIMI_LOGW(TAG, "iotdns cert unavailable host=%s rt=%d, fallback to downloaded global ca bundle len=%zu", host, rt,
                  *cacert_len);
        return OPRT_OK;
    }

    OPERATE_RET final_rt = (rt == OPRT_OK) ? OPRT_COM_ERROR : rt;
    fail_cache_save(host, final_rt, now_ms);
    MIMI_LOGD(TAG, "iotdns cert unavailable host=%s rt=%d, global_ca_rt=%d fetch_rt=%d", host, rt, global_rt, fetch_rt);
    return final_rt;
}
