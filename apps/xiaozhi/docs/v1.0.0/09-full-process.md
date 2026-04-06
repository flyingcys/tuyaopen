# v1.0.0 完整流程总览

## 1. 全局流程

`apps/xiaozhi` 在 `v1.0.0` 中的完整运行链路可以概括为：

1. 应用启动
2. 初始化设备身份
3. 进行 OTA 配置检查
4. 如有需要，执行激活
5. 选择通信协议
6. 建立控制通道
7. 发送 `hello`
8. 收到服务端 `hello`
9. 进入 listen / tts / mcp 等正常交互
10. 会话结束或异常断开
11. 回到空闲态或准备重连

## 2. 从上电到协议选择

### 2.1 启动阶段

应用启动后会完成：

- 基础运行时初始化
- 配置加载
- 身份准备
- 网络准备

### 2.2 OTA 引导阶段

若执行 `xz_ota check`，客户端会向 OTA 服务报告当前环境信息，并获取：

- 协议配置
- 激活信息
- 固件升级信息

### 2.3 激活阶段

如果返回激活码或 challenge，客户端会继续完成激活流程。  
激活成功后，设备才算完成注册闭环。

## 3. WebSocket 流程时序

```text
客户端 -> WebSocket 服务端: 建立握手连接
客户端 -> WebSocket 服务端: hello
服务端 -> 客户端: hello
客户端 -> 服务端: listen start / detect / stop
客户端 -> 服务端: 二进制音频
服务端 -> 客户端: stt / tts / llm / mcp
客户端 -> 服务端: abort / mcp
连接关闭 -> 返回空闲
```

特点：

- 单连接承载控制和音频
- Linux 场景联调最直接

## 4. MQTT + UDP 流程时序

```text
客户端 -> MQTT 服务端: MQTT Connect
客户端 -> MQTT 服务端: Subscribe
客户端 -> MQTT 服务端: hello
服务端 -> 客户端: hello(含 udp server/key/nonce)
客户端 -> UDP 服务端: 建立 UDP 通道
客户端 <-> UDP 服务端: 音频数据
客户端 <-> MQTT 服务端: listen / tts / stt / mcp / goodbye
断开 UDP
必要时断开或重连 MQTT
```

特点：

- 控制面和媒体面分离
- 更贴近上游 MQTT 部署方式

## 5. 典型会话流程

### 5.1 用户开始说话

触发方式可以是：

- 手动命令 `xz_listen start manual`
- 自动模式
- 唤醒词检测后进入 `detect`

### 5.2 设备上行

设备会：

- 发送 `listen start`
- 持续上传音频
- 必要时发送 `mcp`

### 5.3 服务端下行

服务端通常会依次返回：

- `stt`
- `llm`
- `tts start`
- 二进制音频
- `tts stop`

### 5.4 会话结束

结束方式包括：

- 客户端发送 `listen stop`
- 客户端发送 `abort`
- MQTT 模式下发送 `goodbye`
- 服务端主动断开

## 6. 固件升级流程

如果 OTA 返回 `firmware` 且存在新版本，应用可获取：

- `version`
- `url`

后续升级既可以由 OTA 引导完成，也可以通过 MCP 工具 `self.upgrade_firmware` 发起。

## 7. 推荐联调闭环

### 7.1 首次接入

```bash
xz_sys show
xz_ota check
xz_status
```

### 7.2 验证 WebSocket

```bash
xz_proto websocket
xz_reconnect
xz_listen start manual
```

### 7.3 验证 MQTT + UDP

```bash
xz_proto mqtt-udp
xz_reconnect
xz_status
```

## 8. 一句话理解整个系统

`apps/xiaozhi v1.0.0` 的核心流程就是：  
先通过 OTA 完成设备注册与配置拉取，再按所选协议建立语音控制链路，围绕 `session_id` 进行 listen、音频传输、TTS/STT、MCP 调用和会话收尾。
