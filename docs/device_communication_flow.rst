设备通信全流程文档
==================

.. contents:: 目录
   :depth: 3

1. 设备初始化流程
-----------------

1.1 硬件初始化
~~~~~~~~~~~~~~
- 外设驱动加载 (peripherals/)
- 安全芯片初始化 (tal_security/)
- 存储系统初始化 (tal_kv/)

1.2 系统服务启动
~~~~~~~~~~~~~~~~
.. graphviz::

   digraph init_flow {
       rankdir=TB;
       node [shape=box];
       
       硬件初始化 -> 系统时钟启动;
       系统时钟启动 -> 任务调度器初始化;
       任务调度器初始化 -> 网络协议栈初始化;
       网络协议栈初始化 -> 云服务初始化;
       云服务初始化 -> 应用服务启动;
   }

2. 网络连接建立
---------------

2.1 网络选择策略
~~~~~~~~~~~~~~~~
- 优先顺序：有线网络 > WiFi > 蓝牙
- 自动切换机制 (netmgr/)

2.2 连接建立流程
~~~~~~~~~~~~~~~
.. list-table:: 连接参数配置
   :widths: 20 30 50
   :header-rows: 1

   * - 网络类型
     - 配置文件
     - 关键参数
   * - 有线
     - tal_wired.h
     - MAC地址, IP获取方式
   * - WiFi
     - tal_wifi.h
     - SSID, 密码, 加密方式
   * - 蓝牙
     - tal_bluetooth.h
     - 设备名称, 服务UUID

3. 云端通信协议
---------------

3.1 MQTT通信流程
~~~~~~~~~~~~~~~~
- 连接建立 (mqtt_service.c)
- 主题订阅/发布
- 心跳保持机制

3.2 HTTP接口
~~~~~~~~~~~
- 设备激活接口 (tuya_http.c)
- OTA升级接口
- 数据统计上报

4. 局域网通信
------------

4.1 TCP通信协议
~~~~~~~~~~~~~~~
- 服务发现机制 (lan_sock.c)
- 数据加密传输
- 多设备组网

4.2 蓝牙Mesh
~~~~~~~~~~~~
- 网络拓扑结构
- 消息转发策略
- 安全加密机制

5. OTA升级流程
--------------

5.1 OTA工作流程
~~~~~~~~~~~~~~~
.. graphviz::

   digraph ota_flow {
       rankdir=TB;
       node [shape=box];
       
       初始化OTA模块 -> 接收升级通知;
       接收升级通知 -> 解析升级信息;
       解析升级信息 -> 创建下载线程;
       创建下载线程 -> 下载固件;
       下载固件 -> 验证HMAC;
       验证HMAC -> 上报完成状态;
       上报完成状态 -> 执行升级;
   }

5.2 核心API说明
~~~~~~~~~~~~~~~
.. list-table:: 真实OTA API说明
   :widths: 25 25 30 20
   :header-rows: 1

   * - 函数名
     - 所在文件
     - 功能描述
     - 返回值
   * - tuya_ota_init()
     - tuya_ota.c
     - 初始化OTA模块
     - OPRT_OK(0)表示成功
   * - tuya_ota_start()
     - tuya_ota.c
     - 开始OTA升级流程
     - 线程创建结果
   * - tuya_ota_upgrade_status_report()
     - tuya_ota.c
     - 上报升级状态
     - 云服务返回状态
   * - tuya_ota_upgrade_progress_report()
     - tuya_ota.c
     - 上报下载进度
     - 云服务返回状态

5.3 升级信息结构
~~~~~~~~~~~~~~~
.. code-block:: c
   :caption: tuya_ota_msg_t结构定义

   typedef struct {
       uint8_t channel;          // 固件类型
       char fw_url[FW_URL_LEN+1]; // 固件下载URL
       char sw_ver[SW_VER_LEN+1]; // 固件版本
       size_t file_size;          // 固件大小(字节)
       char fw_hmac[FW_HMAC_LEN+1]; // HMAC校验值
       char fw_md5[SW_MD5_LEN+1];   // MD5校验值
   } tuya_ota_msg_t;

5.4 完整OTA示例
~~~~~~~~~~~~~~~
.. code-block:: c
   :caption: 基于真实API的OTA示例

   #include "tuya_ota.h"
   
   // OTA事件回调
   void ota_event_cb(tuya_ota_msg_t *msg, tuya_ota_event_t *event) {
       switch(event->id) {
           case TUYA_OTA_EVENT_START:
               printf("OTA开始,文件大小:%d\n", event->file_size);
               break;
           case TUYA_OTA_EVENT_ON_DATA:
               printf("下载进度:%d%%\n", event->offset*100/event->file_size);
               break;
           case TUYA_OTA_EVENT_FINISH:
               printf("下载完成,准备升级\n");
               break;
           case TUYA_OTA_EVENT_FAULT:
               printf("OTA出错\n");
               break;
       }
   }

   int main() {
       // 1. 初始化OTA
       tuya_ota_config_t config = {
           .event_cb = ota_event_cb,
           .range_size = 1024,
           .timeout_ms = 30000
       };
       tuya_ota_init(&config);
       
       // 2. 解析升级信息(通常来自MQTT消息)
       cJSON *upgrade = cJSON_Parse(mqtt_message);
       
       // 3. 开始OTA升级
       tuya_ota_start(upgrade);
       
       cJSON_Delete(upgrade);
       return 0;
   }

5.5 安全机制说明
~~~~~~~~~~~~~~~
1. HMAC验证流程:
   - 下载完成后计算文件HMAC
   - 与云端下发的HMAC比对
   - 不一致则终止升级

2. 进度上报机制:
   - 每下载5%进度上报一次
   - 包含下载偏移量和文件大小
   - 异常中断会上报错误状态

5.6 错误处理
~~~~~~~~~~~~
.. list-table:: OTA错误代码
   :widths: 20 30 50
   :header-rows: 1

   * - 错误码
     - 定义
     - 处理建议
   * - 40
     - TUS_DOWNLOAD_ERROR_UNKONW
     - 检查网络连接
   * - 41
     - TUS_DOWNLOAD_ERROR_LOW_BATTERY
     - 确保电量充足
   * - 42
     - TUS_DOWNLOAD_ERROR_STORAGE_NOT_ENOUGH
     - 清理存储空间
   * - 45
     - TUS_DOWNLOAD_ERROR_HMAC
     - 重新下载固件

6. 附录
-------

6.1 关键数据结构定义
~~~~~~~~~~~~~~~~~~~~
- tuya_ota_config_t: OTA配置结构体
- tuya_ota_event_t: OTA事件结构体
- tuya_dp_data_t: 数据点结构体

6.2 完整API参考
~~~~~~~~~~~~~~~
- 参考tuya_ota.h头文件定义
- 参考mqtt_service.h网络接口
- 参考tal_kv.h存储接口

6.3 性能优化建议
~~~~~~~~~~~~~~~
- 使用多线程处理耗时操作
- 合理设置OTA超时时间
- 优化网络重试机制