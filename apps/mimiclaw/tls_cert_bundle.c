#include "tls_cert_bundle.h"

#include <string.h>

#include "iotdns.h"
#include "tal_memory.h"

static const char *TAG = "tls_bundle";

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

    if (strcmp(host, "api.telegram.org") != 0) {
        return OPRT_NOT_FOUND;
    }

    size_t pem_len = strlen(TG_GODADDY_G2_CA_PEM) + 1;
    uint8_t *buf = tal_malloc(pem_len);
    if (!buf) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(buf, TG_GODADDY_G2_CA_PEM, pem_len);

    *cacert = buf;
    *cacert_len = (uint16_t)pem_len;
    return OPRT_OK;
}

OPERATE_RET mimi_tls_query_domain_certs(const char *host_or_url, uint8_t **cacert, uint16_t *cacert_len)
{
    if (!host_or_url || !cacert || !cacert_len) {
        return OPRT_INVALID_PARM;
    }

    *cacert = NULL;
    *cacert_len = 0;

    OPERATE_RET rt = tuya_iotdns_query_domain_certs((char *)host_or_url, cacert, cacert_len);
    if (rt == OPRT_OK && *cacert && *cacert_len > 0) {
        return OPRT_OK;
    }

    if (*cacert) {
        tal_free(*cacert);
        *cacert = NULL;
        *cacert_len = 0;
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
