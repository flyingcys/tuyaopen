#include "tls_cert_bundle.h"

#include <string.h>

#include "iotdns.h"
#include "tal_memory.h"
#include "tal_system.h"

static const char *TAG = "tls_bundle";

#define MIMI_TLS_CERT_QUERY_RETRY_COUNT    3
#define MIMI_TLS_CERT_QUERY_RETRY_BASE_MS  400
#define MIMI_TLS_CERT_QUERY_RETRY_MAX_MS   1600

/* Go Daddy Secure Certificate Authority - G2, expires 2031-05-03. */
static const char TG_GODADDY_G2_CA_PEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIE0DCCA7igAwIBAgIBBzANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx\n"
    "EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT\n"
    "EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp\n"
    "ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTExMDUwMzA3MDAwMFoXDTMxMDUwMzA3\n"
    "MDAwMFowgbQxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH\n"
    "EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjEtMCsGA1UE\n"
    "CxMkaHR0cDovL2NlcnRzLmdvZGFkZHkuY29tL3JlcG9zaXRvcnkvMTMwMQYDVQQD\n"
    "EypHbyBEYWRkeSBTZWN1cmUgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi\n"
    "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC54MsQ1K92vdSTYuswZLiBCGzD\n"
    "BNliF44v/z5lz4/OYuY8UhzaFkVLVat4a2ODYpDOD2lsmcgaFItMzEUz6ojcnqOv\n"
    "K/6AYZ15V8TPLvQ/MDxdR/yaFrzDN5ZBUY4RS1T4KL7QjL7wMDge87Am+GZHY23e\n"
    "cSZHjzhHU9FGHbTj3ADqRay9vHHZqm8A29vNMDp5T19MR/gd71vCxJ1gO7GyQ5HY\n"
    "pDNO6rPWJ0+tJYqlxvTV0KaudAVkV4i1RFXULSo6Pvi4vekyCgKUZMQWOlDxSq7n\n"
    "eTOvDCAHf+jfBDnCaQJsY1L6d8EbyHSHyLmTGFBUNUtpTrw700kuH9zB0lL7AgMB\n"
    "AAGjggEaMIIBFjAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAdBgNV\n"
    "HQ4EFgQUQMK9J47MNIMwojPX+2yz8LQsgM4wHwYDVR0jBBgwFoAUOpqFBxBnKLbv\n"
    "9r0FQW4gwZTaD94wNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8v\n"
    "b2NzcC5nb2RhZGR5LmNvbS8wNQYDVR0fBC4wLDAqoCigJoYkaHR0cDovL2NybC5n\n"
    "b2RhZGR5LmNvbS9nZHJvb3QtZzIuY3JsMEYGA1UdIAQ/MD0wOwYEVR0gADAzMDEG\n"
    "CCsGAQUFBwIBFiVodHRwczovL2NlcnRzLmdvZGFkZHkuY29tL3JlcG9zaXRvcnkv\n"
    "MA0GCSqGSIb3DQEBCwUAA4IBAQAIfmyTEMg4uJapkEv/oV9PBO9sPpyIBslQj6Zz\n"
    "91cxG7685C/b+LrTW+C05+Z5Yg4MotdqY3MxtfWoSKQ7CC2iXZDXtHwlTxFWMMS2\n"
    "RJ17LJ3lXubvDGGqv+QqG+6EnriDfcFDzkSnE3ANkR/0yBOtg2DZ2HKocyQetawi\n"
    "DsoXiWJYRBuriSUBAA/NxBti21G00w9RKpv0vHP8ds42pM3Z2Czqrpv1KrKQ0U11\n"
    "GIo/ikGQI31bS/6kA1ibRrLDYGCD+H1QQc7CoZDDu+8CL9IVVO5EFdkKrqeKM+2x\n"
    "LXY2JtwE65/3YR8V3Idv7kaWKK2hJn0KCacuBKONvPi8BDAB\n"
    "-----END CERTIFICATE-----\n";

/*
 * Google Trust Services WE1 intermediate CA (P-256), currently used in
 * api.openai.com certificate chain. Expires at 2029-02-20.
 */
static const char OPENAI_WE1_CA_PEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICnzCCAiWgAwIBAgIQf/MZd5csIkp2FV0TttaF4zAKBggqhkjOPQQDAzBHMQsw\n"
    "CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n"
    "MBIGA1UEAxMLR1RTIFJvb3QgUjQwHhcNMjMxMjEzMDkwMDAwWhcNMjkwMjIwMTQw\n"
    "MDAwWjA7MQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZp\n"
    "Y2VzMQwwCgYDVQQDEwNXRTEwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARvzTr+\n"
    "Z1dHTCEDhUDCR127WEcPQMFcF4XGGTfn1XzthkubgdnXGhOlCgP4mMTG6J7/EFmP\n"
    "LCaY9eYmJbsPAvpWo4H+MIH7MA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggr\n"
    "BgEFBQcDAQYIKwYBBQUHAwIwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHQ4EFgQU\n"
    "kHeSNWfE/6jMqeZ72YB5e8yT+TgwHwYDVR0jBBgwFoAUgEzW63T/STaj1dj8tT7F\n"
    "avCUHYwwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzAChhhodHRwOi8vaS5wa2ku\n"
    "Z29vZy9yNC5jcnQwKwYDVR0fBCQwIjAgoB6gHIYaaHR0cDovL2MucGtpLmdvb2cv\n"
    "ci9yNC5jcmwwEwYDVR0gBAwwCjAIBgZngQwBAgEwCgYIKoZIzj0EAwMDaAAwZQIx\n"
    "AOcCq1HW90OVznX+0RGU1cxAQXomvtgM8zItPZCuFQ8jSBJSjz5keROv9aYsAm5V\n"
    "sQIwJonMaAFi54mrfhfoFNZEfuNMSQ6/bIBiNLiyoX46FohQvKeIoJ99cx7sUkFN\n"
    "7uJW\n"
    "-----END CERTIFICATE-----\n";

/*
 * DigiCert Global Root G2 (cross-signed), used by api.deepseek.com chain.
 * Expires at 2031-11-09.
 */
static const char DEEPSEEK_DIGICERT_G2_CA_PEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEfjCCA2agAwIBAgIQD+Ayq4RNAzEGxQyOE8iwaDANBgkqhkiG9w0BAQsFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
    "QTAeFw0yNDAxMTgwMDAwMDBaFw0zMTExMDkyMzU5NTlaMGExCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
    "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
    "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
    "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
    "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
    "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo4IBMDCC\n"
    "ASwwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUTiJUIBiV5uNu5g/6+rkS7QYX\n"
    "jzkwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUwDgYDVR0PAQH/BAQD\n"
    "AgGGMHQGCCsGAQUFBwEBBGgwZjAjBggrBgEFBQcwAYYXaHR0cDovL29jc3AuZGln\n"
    "aWNlcnQuY24wPwYIKwYBBQUHMAKGM2h0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNu\n"
    "L0RpZ2lDZXJ0R2xvYmFsUm9vdENBLmNydDBABgNVHR8EOTA3MDWgM6Axhi9odHRw\n"
    "Oi8vY3JsLmRpZ2ljZXJ0LmNuL0RpZ2lDZXJ0R2xvYmFsUm9vdENBLmNybDARBgNV\n"
    "HSAECjAIMAYGBFUdIAAwDQYJKoZIhvcNAQELBQADggEBAHRBl3jN7+XHBUK0dZnu\n"
    "hMdoNwD1nCROU3BTIh1TNzRI0bQ0m5+C/dCRzzlqoSAFHUlOi+OiDltWkXTzmQn6\n"
    "Z8bH5PFBy5sYpc/8cNPoSzhyqcpvvEZvv/Ivc0Up+dzma7vBDJC9WrMRUUlSFSQp\n"
    "kdXSmphDNkXJsgARmxzc18IN6LYMRiOWlY7RE2F900pPW60BvJHHNCX0bbSRj/Ql\n"
    "bmVq8wuftBD++D+RS8K++ujpMjFBROyWfBX+woQDGsMazkmgulQdnZrdj476elOL\n"
    "axRvrSgEorju1kJM7M65z2RUZrfzQYW/1rs8mRUXin6iEtad/Rv1ZI1WGYmWPyBm\n"
    "pbo=\n"
    "-----END CERTIFICATE-----\n";

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

static OPERATE_RET query_builtin_cert(const char *host, uint8_t **cacert, uint16_t *cacert_len)
{
    if (!host || !cacert || !cacert_len) {
        return OPRT_INVALID_PARM;
    }

    const char *pem = NULL;
    if (strcmp(host, "api.telegram.org") == 0) {
        pem = TG_GODADDY_G2_CA_PEM;
    } else if (strcmp(host, "api.openai.com") == 0) {
        pem = OPENAI_WE1_CA_PEM;
    } else if (strcmp(host, "api.deepseek.com") == 0) {
        pem = DEEPSEEK_DIGICERT_G2_CA_PEM;
    } else {
        return OPRT_NOT_FOUND;
    }

    size_t pem_len = strlen(pem) + 1;
    uint8_t *buf = tal_malloc(pem_len);
    if (!buf) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(buf, pem, pem_len);

    *cacert = buf;
    *cacert_len = (uint16_t)pem_len;
    return OPRT_OK;
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

    OPERATE_RET brt = query_builtin_cert(host, cacert, cacert_len);
    if (brt == OPRT_OK) {
        MIMI_LOGW(TAG, "iotdns cert unavailable host=%s rt=%d, fallback to built-in CA", host, rt);
        return OPRT_OK;
    }
    if (brt != OPRT_NOT_FOUND) {
        return brt;
    }

    return (rt == OPRT_OK) ? OPRT_COM_ERROR : rt;
}
