HTTP OTA升级流程
================

.. contents:: 目录
   :depth: 3

1. 概述
-------

1.1 流程图
~~~~~~~~~

.. graphviz::

   digraph ota_flow {
       rankdir=TB;
       node [shape=box];
       
       OTA初始化 -> 检查升级;
       检查升级 -> 下载固件;
       下载固件 -> 验证签名;
       验证签名 -> 执行升级;
       执行升级 -> 上报结果;
   }

1.2 核心概念
~~~~~~~~~~~
- **固件包**：包含固件二进制和元数据
- **签名验证**：使用ECDSA验证固件完整性
- **断点续传**：支持下载中断恢复
- **安全存储**：升级过程中临时文件加密存储

2. 核心API说明
-------------

2.1 OTA初始化API
~~~~~~~~~~~~~~~

.. code-block:: c
   :caption: tuya_ota_init 函数定义

   /**
    * @brief 初始化OTA模块
    * @param config 配置参数
    * @param event_cb 事件回调函数
    * @return OPRT_OK 成功，其他为错误码
    */
   int tuya_ota_init(
       const tuya_ota_config_t *config,
       tuya_ota_event_cb_t event_cb
   );

2.2 固件下载API
~~~~~~~~~~~~~~

.. code-block:: c
   :caption: tuya_ota_download 函数定义

   /**
    * @brief 启动固件下载
    * @param url 固件下载URL
    * @param md5 预期的MD5校验值
    * @param file_size 固件大小(字节)
    * @return OPRT_OK 成功，其他为错误码
    */
   int tuya_ota_download(
       const char *url,
       const char *md5,
       uint32_t file_size
   );

3. 完整OTA示例
-------------

3.1 OTA升级流程
~~~~~~~~~~~~~

.. code-block:: c
   :caption: OTA升级示例代码

   #include "tuya_ota.h"
   #include "tuya_http.h"

   // OTA事件回调
   void ota_event_cb(tuya_ota_event_t *event) {
       switch(event->type) {
           case TUYA_OTA_EVT_CHECK_COMPLETE:
               if(event->result == OPRT_OK) {
                   printf("New version: %s\n", event->version);
                   // 开始下载
                   tuya_ota_download(event->url, event->md5, event->file_size);
               }
               break;
           case TUYA_OTA_EVT_DOWNLOAD_PROGRESS:
               printf("Download progress: %d%%\n", event->progress);
               break;
           case TUYA_OTA_EVT_UPGRADE_RESULT:
               printf("Upgrade result: %d\n", event->result);
               break;
       }
   }

   int start_ota_upgrade() {
       // 初始化配置
       tuya_ota_config_t config = {
           .range_size = 1024*1024, // 1MB分片
           .timeout_ms = 30000
       };
       
       // 初始化OTA
       int ret = tuya_ota_init(&config, ota_event_cb);
       if(ret != OPRT_OK) {
           printf("OTA init failed: %d\n", ret);
           return ret;
       }
       
       // 检查升级
       return tuya_ota_check();
   }

4. 安全机制
----------

4.1 签名验证流程
~~~~~~~~~~~~~~~
1. 从固件包提取签名
2. 使用设备预置公钥验证
3. 验证固件元数据完整性
4. 比对MD5校验值

4.2 密钥管理
~~~~~~~~~~~
- 使用设备唯一密钥加密通信
- 每次OTA生成临时会话密钥
- 固件包使用AES-256加密

5. 错误处理
----------

5.1 常见错误码
~~~~~~~~~~~~~

.. list-table:: OTA错误代码表
   :widths: 20 30 50
   :header-rows: 1

   * - 错误码
     - 说明
     - 处理建议
   * - OPRT_OK (0)
     - 操作成功
     - -
   * - OPRT_OTA_DOWNLOAD_FAILED (-5001)
     - 下载失败
     - 检查网络连接
   * - OPRT_OTA_VERIFY_FAILED (-5002)
     - 验证失败
     - 重新下载固件
   * - OPRT_OTA_INSUFFICIENT_SPACE (-5003)
     - 存储空间不足
     - 清理存储空间

6. 最佳实践
----------

6.1 实施建议
~~~~~~~~~~~~
1. **进度上报**：定期上报下载/升级进度
2. **断电保护**：关键操作前写入标记
3. **回滚机制**：保留上一个可用版本
4. **日志记录**：详细记录升级过程
5. **测试验证**：先在测试环境验证固件

附录
----

A.1 关键数据结构
~~~~~~~~~~~~~~~~

.. code-block:: c
   :caption: 关键数据结构定义

   typedef struct {
       uint8_t type;           // 事件类型
       uint8_t result;         // 操作结果
       uint32_t progress;      // 进度百分比
       char version[32];       // 固件版本
       char url[256];          // 下载URL
       char md5[33];           // MD5校验值
       uint32_t file_size;     // 文件大小
   } tuya_ota_event_t;

A.2 性能优化建议
~~~~~~~~~~~~~~~
1. 分片下载减少内存占用
2. 并行下载加速大文件传输
3. 压缩固件元数据
4. 增量升级减少下载量