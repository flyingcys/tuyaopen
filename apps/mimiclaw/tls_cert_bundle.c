#include "tls_cert_bundle.h"

#include <string.h>

#include "iotdns.h"
#include "tal_memory.h"
#include "tal_system.h"

static const char *TAG = "tls_bundle";

#define MIMI_TLS_CERT_QUERY_RETRY_COUNT    3
#define MIMI_TLS_CERT_QUERY_RETRY_BASE_MS  400
#define MIMI_TLS_CERT_QUERY_RETRY_MAX_MS   1600

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

OPERATE_RET mimi_tls_query_domain_certs(const char *host_or_url, uint8_t **cacert, uint16_t *cacert_len)
{
    if (!host_or_url || !cacert || !cacert_len) {
        return OPRT_INVALID_PARM;
    }

    *cacert = NULL;
    *cacert_len = 0;

    OPERATE_RET rt = OPRT_COM_ERROR;
    for (uint32_t attempt = 0; attempt < MIMI_TLS_CERT_QUERY_RETRY_COUNT; attempt++) {
        rt = tuya_iotdns_query_domain_certs((char *)host_or_url, cacert, cacert_len);
        if (rt == OPRT_OK && *cacert && *cacert_len > 0) {
            return OPRT_OK;
        }

        if (*cacert) {
            tal_free(*cacert);
            *cacert = NULL;
            *cacert_len = 0;
        }

        if (attempt + 1 >= MIMI_TLS_CERT_QUERY_RETRY_COUNT || !should_retry_iotdns_query(rt)) {
            break;
        }

        uint32_t delay_ms = cert_query_retry_delay_ms(attempt);
        MIMI_LOGW(TAG, "iotdns cert query retry %u/%u host=%s rt=%d delay=%u", (unsigned)(attempt + 1),
                  (unsigned)MIMI_TLS_CERT_QUERY_RETRY_COUNT, host_or_url, rt, (unsigned)delay_ms);
        tal_system_sleep(delay_ms);
    }

    char host[96] = {0};
    extract_host(host_or_url, host, sizeof(host));
    MIMI_LOGW(TAG, "iotdns cert unavailable host=%s rt=%d, built-in fallback disabled", host, rt);

    return (rt == OPRT_OK) ? OPRT_COM_ERROR : rt;
}
