设备激活与云端交互流程
=======================

.. contents:: 目录
   :depth: 3

1. 设备激活流程概述
-------------------

1.1 激活流程图
~~~~~~~~~~~~~
.. graphviz::

   digraph activation_flow {
       rankdir=TB;
       node [shape=box];
       
       设备初始化 -> 获取激活令牌;
       获取激活令牌 -> 发送激活请求;
       发送激活请求 -> 验证激活响应;
       验证激活响应 -> 保存设备凭证;
       保存设备凭证 -> 建立MQTT连接;
   }

1.2 激活状态机
~~~~~~~~~~~~~
.. list-table:: 激活状态说明
   :widths: 20 30 50
   :header-rows: 1

   * - 状态
     - 宏定义
     - 说明
   * - STATE_IDLE
     - 0
     - 初始空闲状态
   * - STATE_START
     - 1
     - 启动激活流程
   * - STATE_TOKEN_PENDING
     - 5
     - 等待获取激活令牌
   * - STATE_ACTIVATING
     - 6
     - 正在激活中
   * - STATE_MQTT_CONNECTING
     - 11
     - 正在连接MQTT

2. 核心API说明
--------------

2.1 初始化API
~~~~~~~~~~~~
.. code-block:: c
   :caption: tuya_iot_init 初始化设备

   int tuya_iot_init(tuya_iot_client_t *client, const tuya_iot_config_t *config);
   
   参数说明:
   - client: IoT客户端实例
   - config: 配置参数，包含:
     * productkey: 产品KEY
     * uuid: 设备UUID
     * authkey: 设备授权KEY
     * software_ver: 软件版本

2.2 激活请求API
~~~~~~~~~~~~~~
.. code-block:: c
   :caption: client_activate_process 激活处理

   static int client_activate_process(tuya_iot_client_t *client, const char *token);
   
   参数说明:
   - client: IoT客户端实例
   - token: 激活令牌

2.3 激活响应处理
~~~~~~~~~~~~~~~
.. code-block:: c
   :caption: activate_response_parse 响应解析

   static int activate_response_parse(atop_base_response_t *response);
   
   关键响应字段:
   - devId: 设备ID
   - secKey: 安全密钥
   - localKey: 本地密钥
   - schemaId: 数据模型ID

3. 激活令牌获取
--------------

3.1 令牌获取流程
~~~~~~~~~~~~~~~
1. 通过事件订阅等待令牌:
   ```c
   tal_event_subscribe(EVENT_LINK_ACTIVATE, "iot", tuya_iot_token_activate_evt, SUBSCRIBE_TYPE_ONETIME);
   ```

2. 令牌回调处理:
   ```c
   static int tuya_iot_token_activate_evt(void *data) {
       memcpy(client->binding, (tuya_binding_info_t *)data, sizeof(tuya_binding_info_t));
       // 触发信号量通知
   }
   ```

3. 等待令牌获取:
   ```c
   tal_semaphore_wait_forever(client->token_get.sem);
   ```

4. 完整激活示例
--------------

4.1 设备初始化
~~~~~~~~~~~~~
.. code-block:: c
   :caption: 设备初始化示例

   tuya_iot_config_t config = {
       .productkey = "your_product_key",
       .uuid = "device_uuid",
       .authkey = "device_auth_key",
       .software_ver = "1.0.0"
   };
   tuya_iot_init(&client, &config);

4.2 激活流程
~~~~~~~~~~
.. code-block:: c
   :caption: 完整激活流程示例

   // 1. 启动激活
   tuya_iot_start(&client);
   
   // 2. 处理状态机
   while (1) {
       tuya_iot_yield(&client);
       
       if (client.state == STATE_TOKEN_PENDING) {
           // 获取到令牌后自动继续流程
       }
       
       if (tuya_iot_activated(&client)) {
           break; // 激活完成
       }
   }

5. 错误处理
----------

5.1 常见错误码
~~~~~~~~~~~~~
.. list-table:: 激活错误代码
   :widths: 20 30 50
   :header-rows: 1

   * - 错误码
     - 宏定义
     - 处理建议
   * - -1002
     - OPRT_INVALID_PARM
     - 检查输入参数
   * - -2001
     - OPRT_CJSON_PARSE_ERR
     - 检查JSON格式
   * - -3005
     - OPRT_KVS_WR_FAIL
     - 检查存储空间

6. 安全机制
----------

6.1 凭证存储
~~~~~~~~~~~
- 使用tal_kv_set加密存储:
  ```c
  tal_kv_set(activate_data_key, (const uint8_t *)result_string, strlen(result_string));
  ```

6.2 通信安全
~~~~~~~~~~~
- 使用TLS加密通信
- 设备凭证加密存储
- 激活令牌一次性使用

附录
----

- 关键数据结构定义
- 完整API参考手册
- 性能优化建议