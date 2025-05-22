TCP局域网交互流程
=================

.. contents:: 目录
   :depth: 3

1. TCP局域网交互概述
-------------------

1.1 交互流程图
~~~~~~~~~~~~~
.. graphviz::

   digraph lan_flow {
       rankdir=TB;
       node [shape=box];
       
       服务端初始化 -> 监听客户端连接;
       监听客户端连接 -> 建立TCP连接;
       建立TCP连接 -> 三阶段密钥交换;
       三阶段密钥交换 -> 加密数据传输;
       加密数据传输 -> [连接断开];
       连接断开 -> 自动重连;
   }

1.2 核心概念
~~~~~~~~~~~
- 会话管理(lan_session_t)
- 帧类型(FRM_TYPE_*)
- 安全等级(TUYA_SECURITY_LEVEL)
- 本地密钥(localkey)

2. 核心API说明
--------------

2.1 初始化API
~~~~~~~~~~~~
.. list-table:: 局域网服务API
   :widths: 25 25 30 20
   :header-rows: 1

   * - 函数名
     - 所在文件
     - 功能描述
     - 返回值
   * - tuya_lan_init()
     - tuya_lan.c
     - 初始化局域网服务
     - OPRT_OK(0)成功
   * - tuya_lan_exit()
     - tuya_lan.c
     - 停止局域网服务
     - OPRT_OK(0)成功
   * - tuya_lan_data_report()
     - tuya_lan.c
     - 发送数据到所有客户端
     - 发送结果

2.2 密钥交换API
~~~~~~~~~~~~~~
.. code-block:: c
   :caption: 密钥交换处理流程

   // 阶段1: 客户端发送随机数A
   case FRM_SECURITY_TYPE3:
       memcpy(session->randA, data, RAND_LEN);
       // 计算HMAC
       tal_sha256_mac(localkey, strlen(localkey), randA, RAND_LEN, hmac);
       // 返回随机数B和HMAC
       lan_send(session, FRM_SECURITY_TYPE4, randB, hmac);

   // 阶段2: 客户端验证HMAC并发送自己的HMAC
   case FRM_SECURITY_TYPE5:
       // 验证HMAC
       if(memcmp(hmac, data, HMAC_LEN) != 0) {
           // 验证失败处理
       }
       // 生成会话密钥
       for(i=0; i<SESSIONKEY_LEN; i++) {
           session->secret_key[i] = randA[i] ^ randB[i];
       }

3. 完整交互示例
--------------

3.1 服务端初始化
~~~~~~~~~~~~~~
.. code-block:: c
   :caption: 服务端初始化示例

   // 初始化配置
   tuya_iot_config_t config = {
       .productkey = "your_product_key",
       .uuid = "device_uuid", 
       .authkey = "device_auth_key"
   };

   // 初始化局域网服务
   int ret = tuya_lan_init(&config);
   if(ret != OPRT_OK) {
       printf("LAN init failed: %d\n", ret);
       return;
   }

3.2 数据处理
~~~~~~~~~~~
.. code-block:: c
   :caption: 数据处理示例

   void handle_client_data(lan_session_t *session, uint8_t *data, uint32_t len) {
       // 解密数据
       lpv35_frame_object_t frame;
       int ret = lpv35_frame_parse(session->secret_key, data, len, &frame);
       
       // 处理不同类型帧
       switch(frame.type) {
           case FRM_TP_CMD:
               // 处理控制命令
               break;
           case FRM_TP_HB:
               // 心跳处理
               break;
       }
   }

4. 安全机制
----------

4.1 三阶段密钥交换
~~~~~~~~~~~~~~~
1. 客户端发送随机数A(FRM_SECURITY_TYPE3)
2. 服务端返回随机数B和HMAC(FRM_SECURITY_TYPE4)
3. 客户端验证并发送HMAC(FRM_SECURITY_TYPE5)

4.2 数据加密
~~~~~~~~~~~
- 使用AES-128-GCM加密
- 每个会话独立密钥
- 消息完整性校验

5. 错误处理
----------

5.1 常见错误码
~~~~~~~~~~~~~
.. list-table:: 局域网错误代码
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
     - 检查数据格式
   * - -3005
     - OPRT_KVS_WR_FAIL
     - 检查存储空间

附录
----

- 关键数据结构定义
- 完整API参考手册
- 性能优化建议