MQTT与云端交互流程
===================

.. contents:: 目录
   :depth: 3

1. MQTT连接流程
---------------

1.1 连接流程图
~~~~~~~~~~~~~
.. graphviz::

   digraph mqtt_flow {
       rankdir=TB;
       node [shape=box];
       
       初始化MQTT客户端 -> 生成连接凭证;
       生成连接凭证 -> 建立MQTT连接;
       建立MQTT连接 -> 订阅主题;
       订阅主题 -> 处理消息;
       处理消息 -> [连接断开];
       连接断开 -> 自动重连;
   }

1.2 核心API说明
~~~~~~~~~~~~~~
.. list-table:: MQTT连接管理API
   :widths: 25 25 30 20
   :header-rows: 1

   * - 函数名
     - 所在文件
     - 功能描述
     - 返回值
   * - tuya_mqtt_init()
     - mqtt_service.c
     - 初始化MQTT客户端
     - OPRT_OK(0)成功
   * - tuya_mqtt_start()
     - mqtt_service.c
     - 启动MQTT连接
     - OPRT_OK(0)成功
   * - tuya_mqtt_stop()
     - mqtt_service.c
     - 停止MQTT连接
     - OPRT_OK(0)成功
   * - tuya_mqtt_loop()
     - mqtt_service.c
     - 处理MQTT事件循环
     - OPRT_OK(0)成功

2. 消息发布与订阅
-----------------

2.1 主题管理
~~~~~~~~~~~
.. code-block:: c
   :caption: 主题订阅示例

   // 订阅主题回调
   void message_callback(uint16_t msgid, const mqtt_client_message_t *msg, void *userdata) {
       printf("收到消息: %.*s\n", msg->length, msg->payload);
   }

   // 订阅主题
   tuya_mqtt_subscribe_message_callback_register(context, "smart/device/in/device123", 
                                               message_callback, userdata);

2.2 消息发布
~~~~~~~~~~~
.. code-block:: c
   :caption: 消息发布示例

   // 发布简单消息
   tuya_mqtt_client_publish_common(context, "smart/device/out/device123", 
                                  (uint8_t*)"hello", 5, NULL, NULL, 0, false);

   // 发布协议消息
   tuya_mqtt_protocol_data_publish(context, PRO_DP_QUERY, (uint8_t*)"{\"status\":1}", 11);

3. 协议消息处理
--------------

3.1 消息格式
~~~~~~~~~~~
.. code-block:: json
   :caption: 协议消息格式示例

   {
       "protocol": 20,
       "t": 123456789,
       "data": {
           "devId": "device123",
           "dps": {
               "1": true,
               "2": 25
           }
       }
   }

3.2 协议处理API
~~~~~~~~~~~~~
.. list-table:: 协议处理API
   :widths: 25 25 30 20
   :header-rows: 1

   * - 函数名
     - 所在文件
     - 功能描述
     - 返回值
   * - tuya_mqtt_protocol_register()
     - mqtt_service.c
     - 注册协议处理器
     - OPRT_OK(0)成功
   * - tuya_protocol_message_parse_process()
     - mqtt_service.c
     - 解析协议消息
     - OPRT_OK(0)成功

4. 完整交互示例
--------------

4.1 初始化与连接
~~~~~~~~~~~~~~
.. code-block:: c
   :caption: MQTT初始化示例

   tuya_mqtt_config_t config = {
       .host = "mqtt.tuya.com",
       .port = 8883,
       .cacert = tuya_ca_cert,
       .cacert_len = sizeof(tuya_ca_cert),
       .devid = "device123",
       .seckey = "your_secret_key",
       .localkey = "your_local_key"
   };

   tuya_mqtt_init(&mqtt_ctx, &config);
   tuya_mqtt_start(&mqtt_ctx);

4.2 消息处理循环
~~~~~~~~~~~~~~
.. code-block:: c
   :caption: 主事件循环示例

   while (1) {
       tuya_mqtt_loop(&mqtt_ctx);
       tal_system_sleep(100);
   }

5. 错误处理
-----------

5.1 常见错误码
~~~~~~~~~~~~~
.. list-table:: MQTT错误代码
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
   * - 40
     - TUS_DOWNLOAD_ERROR_UNKONW
     - 检查网络连接

6. 安全机制
----------

6.1 连接凭证
~~~~~~~~~~~
- 使用设备密钥生成连接凭证
- 包含clientid、username、password
- 使用TLS加密通信

6.2 消息加密
~~~~~~~~~~~
- 使用AES加密协议消息
- 每个消息单独签名
- 支持消息完整性校验

附录
----

- 关键数据结构定义
- 完整API参考手册
- 性能优化建议