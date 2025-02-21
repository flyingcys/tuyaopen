#include <stdio.h>

#include "cJSON.h"

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_kv.h"

#include "tkl_output.h"

#include "http_client_interface.h"

#define URL  "httpbin.org"
#define PATH "/get"
int main(int argc, char *argv[])
{
    printf("Hello, World!\n");
    printf("cJSON_Version: %s\n", cJSON_Version());

    OPERATE_RET rt = OPRT_OK;
    uint16_t cacert_len = 0;
    uint8_t *cacert = NULL;
    
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();
    tuya_tls_init();
    tuya_register_center_init();
    
    /* HTTP Response */
    http_client_response_t http_response = {0};

    /* HTTPS cert */
    TUYA_CALL_ERR_RETURN(tuya_iotdns_query_domain_certs(URL, &cacert, &cacert_len));

    /* HTTP headers */
    http_client_header_t headers[] = {{.key = "Content-Type", .value = "application/json"}};

    /* HTTP Request send */
    PR_DEBUG("http request send!");
    http_client_status_t http_status = http_client_request(
        &(const http_client_request_t){
                                        .cacert = cacert,
                                        .cacert_len = cacert_len,
                                        .host = URL,
                                        .port = 443,
                                        .method = "GET",
                                        .path = PATH,
                                        .headers = headers,
                                        .headers_count = sizeof(headers) / sizeof(http_client_header_t),
                                        .body = "",
                                        .body_length = 0,
                                        .timeout_ms = 10},
        &http_response);

    if (HTTP_CLIENT_SUCCESS != http_status) {
        PR_ERR("http_request_send error:%d", http_status);
        rt = OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
        goto err_exit;
    }

    PR_DEBUG((char *)http_response.body);
err_exit:
    http_client_free(&http_response);

    return 0;
}