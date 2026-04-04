/**
 * @file xiaozhi_upgrade.c
 * @brief Download and write firmware image via TAL OTA APIs.
 */

#include "xiaozhi_upgrade.h"

#include "http_session.h"
#include "tal_api.h"

#include <string.h>

OPERATE_RET xiaozhi_upgrade_from_url(const char *url)
{
    if (!url || url[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    http_session_t session = NULL;
    http_resp_t   *resp    = NULL;
    OPERATE_RET    rt      = OPRT_OK;

    rt = http_open_session(&session, url, 15000);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = http_set_timeout(session, 15000);
    if (rt != OPRT_OK) {
        (void)http_close_session(&session);
        return rt;
    }

    http_req_t req = {
        .type            = HTTP_GET,
        .version         = HTTP_VER_1_1,
        .content         = NULL,
        .content_len     = 0,
        .download_offset = 0,
        .download_size   = 0,
    };

    rt = http_send_request(session, &req, 0);
    if (rt != OPRT_OK) {
        (void)http_close_session(&session);
        return rt;
    }

    rt = http_get_response_hdr(session, &resp);
    if (rt != OPRT_OK || !resp) {
        (void)http_close_session(&session);
        return (rt != OPRT_OK) ? rt : OPRT_COM_ERROR;
    }

    if (resp->status_code != 200) {
        (void)http_free_response_hdr(&resp);
        (void)http_close_session(&session);
        return OPRT_COM_ERROR;
    }

    if (resp->content_length == 0) {
        (void)http_free_response_hdr(&resp);
        (void)http_close_session(&session);
        return OPRT_NOT_SUPPORTED;
    }

    uint32_t        max_image_size = 0;
    TUYA_OTA_TYPE_E ota_type       = TUYA_OTA_FULL;
    if (tal_ota_get_ability(&max_image_size, &ota_type) != OPRT_OK) {
        (void)http_free_response_hdr(&resp);
        (void)http_close_session(&session);
        return OPRT_COM_ERROR;
    }

    if (max_image_size != 0 && resp->content_length > max_image_size) {
        (void)http_free_response_hdr(&resp);
        (void)http_close_session(&session);
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    rt = tal_ota_start_notify(resp->content_length, TUYA_OTA_FULL, TUYA_OTA_PATH_AIR);
    if (rt != OPRT_OK) {
        (void)http_free_response_hdr(&resp);
        (void)http_close_session(&session);
        return rt;
    }

    uint8_t  buf[2048] = {0};
    uint32_t offset    = 0;

    for (;;) {
        int n = http_read_content(session, buf, sizeof(buf));
        if (n < 0) {
            (void)http_free_response_hdr(&resp);
            (void)http_close_session(&session);
            return OPRT_COM_ERROR;
        }
        if (n == 0) {
            break;
        }

        TUYA_OTA_DATA_T pack = {
            .total_len = resp->content_length,
            .offset    = offset,
            .data      = buf,
            .len       = (uint32_t)n,
            .pri_data  = NULL,
        };
        uint32_t remain_len = 0;
        rt                  = tal_ota_data_process(&pack, &remain_len);
        if (rt != OPRT_OK) {
            (void)http_free_response_hdr(&resp);
            (void)http_close_session(&session);
            return rt;
        }

        if (remain_len > (uint32_t)n) {
            (void)http_free_response_hdr(&resp);
            (void)http_close_session(&session);
            return OPRT_COM_ERROR;
        }
        offset += (uint32_t)n - remain_len;
    }

    (void)http_free_response_hdr(&resp);
    (void)http_close_session(&session);

    if (offset == 0) {
        return OPRT_COM_ERROR;
    }

    return tal_ota_end_notify(TRUE);
}
