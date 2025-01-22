#include <stdio.h>
#include "platform.h"

#include <core_http_client.h>
#include "ssl_transport.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

// esp_err_t oai_http_event_handler(esp_http_client_event_t *evt) {
//   static int output_len;
//   switch (evt->event_id) {
//     case HTTP_EVENT_REDIRECT:
//       ESP_LOGD(LOG_TAG, "HTTP_EVENT_REDIRECT");
//       esp_http_client_set_header(evt->client, "From", "user@example.com");
//       esp_http_client_set_header(evt->client, "Accept", "text/html");
//       esp_http_client_set_redirection(evt->client);
//       break;
//     case HTTP_EVENT_ERROR:
//       ESP_LOGD(LOG_TAG, "HTTP_EVENT_ERROR");
//       break;
//     case HTTP_EVENT_ON_CONNECTED:
//       ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_CONNECTED");
//       break;
//     case HTTP_EVENT_HEADER_SENT:
//       ESP_LOGD(LOG_TAG, "HTTP_EVENT_HEADER_SENT");
//       break;
//     case HTTP_EVENT_ON_HEADER:
//       ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
//                evt->header_key, evt->header_value);
//       break;
//     case HTTP_EVENT_ON_DATA: {
//       ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
//       if (esp_http_client_is_chunked_response(evt->client)) {
//         ESP_LOGE(LOG_TAG, "Chunked HTTP response not supported");
//         oai_platform_restart();
//       }

//       if (output_len == 0 && evt->user_data) {
//         memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
//       }

//       // If user_data buffer is configured, copy the response into the buffer
//       int copy_len = 0;
//       if (evt->user_data) {
//         // The last byte in evt->user_data is kept for the NULL character in
//         // case of out-of-bound access.
//         copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
//         if (copy_len) {
//           memcpy(((char *)evt->user_data) + output_len, evt->data, copy_len);
//         }
//       }
//       output_len += copy_len;

//       break;
//     }
//     case HTTP_EVENT_ON_FINISH:
//       ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_FINISH");
//       output_len = 0;
//       break;
//     case HTTP_EVENT_DISCONNECTED:
//       ESP_LOGI(LOG_TAG, "HTTP_EVENT_DISCONNECTED");
//       output_len = 0;
//       break;
//   }
//   return ESP_OK;
// }

const *hostname = "api.openai.com";
const *path = "/v1/realtime?model=gpt-4o-mini-realtime-preview-2024-12-17";
const *OPENAI_API_KEY = "1";

char http_buf[MAX_HTTP_OUTPUT_BUFFER];
static HTTPResponse_t _http_request(const TransportInterface_t* transport_interface,
                                           const char* method,
                                           size_t method_len,
                                           const char* host,
                                           size_t host_len,
                                           const char* path,
                                           size_t path_len,
                                           const char* auth,
                                           size_t auth_len,
                                           const char* body,
                                           size_t body_len) {
  HTTPStatus_t status = HTTPSuccess;
  HTTPRequestInfo_t request_info = {0};
  HTTPResponse_t response = {0};
  HTTPRequestHeaders_t request_headers = {0};

  request_info.pMethod = method;
  request_info.methodLen = method_len;
  request_info.pHost = host;
  request_info.hostLen = host_len;
  request_info.pPath = path;
  request_info.pathLen = path_len;
  request_info.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

  request_headers.pBuffer = http_buf;
  request_headers.bufferLen = sizeof(http_buf);

  status = HTTPClient_InitializeRequestHeaders(&request_headers, &request_info);

  if (status == HTTPSuccess) {
    HTTPClient_AddHeader(&request_headers,
                         "Content-Type", strlen("Content-Type"), "application/sdp", strlen("application/sdp"));

    if (auth_len > 0) {
      HTTPClient_AddHeader(&request_headers,
                           "Authorization", strlen("Authorization"), auth, auth_len);
    }

    response.pBuffer = http_buf;
    response.bufferLen = sizeof(http_buf);

    status = HTTPClient_Send(transport_interface,
                             &request_headers, (uint8_t*)body, body ? body_len : 0, &response, 0);

  } else {
    PR_DEBUG("Failed to initialize HTTP request headers: Error=%s.", HTTPClient_strerror(status));
  }

  return response;
}


void oai_http_request(char *offer, char *answer)
{
  int ret = 0;
  TransportInterface_t trans_if = {0};
  NetworkContext_t net_ctx;
  HTTPResponse_t res;

  memset(http_buf, 0, sizeof(http_buf));

  snprintf(answer, MAX_HTTP_OUTPUT_BUFFER, "Bearer %s", OPENAI_API_KEY);

  trans_if.recv = ssl_transport_recv;
  trans_if.send = ssl_transport_send;
  trans_if.pNetworkContext = &net_ctx;


  ret = ssl_transport_connect(&net_ctx, hostname, 443, NULL);

  if (ret < 0) {
    PR_ERR("Failed to connect to %s:%d\n", hostname, 443);
    return ret;
  }
  res = _http_request(&trans_if, "POST", 4, hostname, strlen(hostname), path,
                                    strlen(path), answer, strlen(answer), offer, strlen(offer));

  ssl_transport_disconnect(&net_ctx);

  if (res.pHeaders == NULL) {
    PR_ERR("Response headers are NULL");
    return -1;
  }

  if (res.pBody == NULL) {
    PR_ERR("Response body is NULL");
    return -1;
  }

  PR_DEBUG(
      "\nReceived HTTP response from %s%s\n"
      "Response Headers: %s\nResponse Status: %u\nResponse Body: %s\n",
      hostname, path, res.pHeaders, res.statusCode, res.pBody);

  if (res.statusCode == 201) {
    memset(answer, 0, MAX_HTTP_OUTPUT_BUFFER);
    memcpy(answer, res.pBody, res.bodyLen);
  }
}
