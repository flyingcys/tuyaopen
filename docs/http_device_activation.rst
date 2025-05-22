HTTP设备激活绑定流程
====================

.. contents:: 目录
   :depth: 3

1. 概述
-------

1.1 流程图
~~~~~~~~~

.. graphviz::

   digraph activation_flow {
       rankdir=TB;
       node [shape=box];
       
       设备初始化 -> 获取激活令牌;
       获取激活令牌 -> 发送HTTP激活请求;
       发送HTTP激活请求 -> 验证服务器响应;
       验证服务器响应 -> 保存设备凭证;
       保存设备凭证 -> 完成绑定;
   }

1.2 核心概念
~~~~~~~~~~~
- **激活令牌**：一次性使用的认证令牌
- **设备凭证**：包括devId、secKey等关键信息
- **安全通信**：基于TLS的HTTPS连接
- **响应验证**：验证服务器签名和响应完整性

2. 核心API说明
-------------

2.1 HTTP通信API
~~~~~~~~~~~~~~~

.. code-block:: c
   :caption: tuya_http_client_post_simple 函数定义

   /**
    * @brief 发送HTTP POST请求
    * @param url 请求URL
    * @param body 请求体内容
    * @param headers 请求头数组
    * @param headers_count 请求头数量
    * @param response 响应结构体指针
    * @return 操作结果
    */
   int tuya_http_client_post_simple(
       char *url, 
       char *body,
       http_client_header_t *headers,
       uint8_t headers_count,
       http_client_response_t *response
   );

2.2 证书管理API
~~~~~~~~~~~~~~~

.. code-block:: c
   :caption: tuya_http_cert_load 函数定义

   /**
    * @brief 加载SSL证书
    * @param host 主机名
    * @param port 端口号
    * @param cacert 证书数据指针
    * @param cacert_len 证书长度
    * @return 操作结果
    */
   int tuya_http_cert_load(
       char *host,
       uint16_t port,
       uint8_t **cacert,
       uint16_t *cacert_len
   );

3. 完整激活示例
--------------

3.1 设备激活请求
~~~~~~~~~~~~~~~

.. code-block:: c
   :caption: 设备激活示例代码

   #include "tuya_http.h"
   #include "tuya_register_center.h"

   // 激活请求示例
   int device_activate(const char *token) {
       char url[256] = {0};
       char body[512] = {0};
       http_client_response_t response = {0};
       http_client_header_t headers[] = {
           {"Content-Type", "application/json"},
           {"Authorization", "Bearer your_api_key"}
       };
       
       // 构造激活URL
       snprintf(url, sizeof(url), "https://a1.tuyacn.com/api/device/activate");
       
       // 构造请求体
       snprintf(body, sizeof(body), 
                "{\"token\":\"%s\",\"productKey\":\"your_product_key\",\"uuid\":\"device_uuid\"}", 
                token);
       
       // 发送HTTP请求
       int ret = tuya_http_client_post_simple(url, body, headers, 2, &response);
       if (ret != OPRT_OK) {
           printf("Activation failed: %d\n", ret);
           return ret;
       }
       
       // 处理响应
       if (response.status_code == 200) {
           printf("Activation success! Response: %.*s\n", 
                  response.content_length, response.content);
           // 解析并保存凭证
           tuya_register_center_save(response.content);
       } else {
           printf("Activation failed with status: %d\n", response.status_code);
       }
       
       // 释放资源
       tuya_http_free(&response);
       return OPRT_OK;
   }

4. 安全机制
----------

4.1 证书管理
~~~~~~~~~~~~
- 证书缓存机制（最多缓存3个证书）
- 自动证书更新
- 证书有效期验证

4.2 密钥交换流程
~~~~~~~~~~~~~~~
1. 设备生成临时密钥对
2. 使用服务器公钥加密临时公钥
3. 通过HTTPS发送加密后的公钥
4. 服务器返回用设备公钥加密的会话密钥
5. 后续通信使用会话密钥加密

5. 错误处理
----------

5.1 常见错误码
~~~~~~~~~~~~~

.. list-table:: 错误代码表
   :widths: 20 30 50
   :header-rows: 1

   * - 错误码
     - 说明
     - 处理建议
   * - OPRT_OK (0)
     - 操作成功
     - -
   * - OPRT_COM_ERROR (-1002)
     - 通信错误
     - 检查网络连接
   * - OPRT_MALLOC_FAILED (-1003)
     - 内存分配失败
     - 检查系统内存
   * - OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR (-2001)
     - HTTP发送错误
     - 检查请求参数

6. 最佳实践
----------

6.1 实施建议
~~~~~~~~~~~~
1. **重试机制**：对临时性错误实现自动重试
2. **超时设置**：合理设置HTTP请求超时
3. **证书验证**：严格验证服务器证书
4. **敏感信息保护**：妥善保管设备凭证
5. **日志记录**：记录完整的激活过程

附录
----

A.1 关键数据结构
~~~~~~~~~~~~~~~~

.. code-block:: c
   :caption: 关键数据结构定义

   typedef struct {
       char *host;
       uint16_t port;
       uint8_t *cacert;
       uint16_t cacert_len;
       TIME_T timeposix;
   } tuya_cert_cache_t;

   typedef struct {
       uint16_t status_code;
       uint32_t content_length;
       char *content;
       http_client_header_t *headers;
       uint8_t headers_count;
   } http_client_response_t;

A.2 性能优化建议
~~~~~~~~~~~~~~~
1. 复用HTTP连接
2. 合理设置证书缓存
3. 异步处理激活流程
4. 压缩HTTP请求体