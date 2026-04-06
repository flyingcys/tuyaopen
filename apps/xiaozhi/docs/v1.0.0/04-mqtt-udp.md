# v1.0.0 MQTT + UDP 通道

## 1. 适用范围

代码位置：`apps/xiaozhi/src/xiaozhi_mqtt_udp.c`、`apps/xiaozhi/src/xiaozhi_protocol.c`

该模式下：

- MQTT 负责控制消息
- UDP 负责音频数据

## 2. 配置项

可通过 CLI 设置：

```bash
xz_mqtt endpoint your.mqtt.server:8883
xz_mqtt client_id your_client_id
xz_mqtt username your_user
xz_mqtt password your_pass
xz_mqtt publish_topic your/pub/topic
xz_mqtt subscribe_topic your/sub/topic
xz_mqtt keepalive 240
xz_proto mqtt-udp
```

查看当前配置：

```bash
xz_mqtt show
```

## 3. 建链总流程

典型流程如下：

1. 连接 MQTT broker
2. 完成订阅
3. 发送 `hello`
4. 服务端通过 MQTT 返回 `hello`
5. `hello` 中携带 UDP 服务器、端口、密钥和随机数
6. 本地初始化 AES-CTR
7. 建立 UDP 连接
8. 开始发送和接收音频

## 4. MQTT 连接参数

关键字段如下：

- `endpoint`
- `client_id`
- `username`
- `password`
- `publish_topic`
- `subscribe_topic`
- `keepalive`

端点格式为：

```text
host:port
```

如果未显式指定端口，默认使用 `8883`。

## 5. MQTT hello 交换

### 5.1 设备端发送 hello

当前实现中，MQTT 模式会发送：

```json
{
  "type": "hello",
  "version": 3,
  "transport": "udp",
  "features": {
    "mcp": true
  },
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  }
}
```

### 5.2 服务端返回 hello

服务端返回中除基础字段外，还必须带 `udp` 对象：

```json
{
  "type": "hello",
  "transport": "udp",
  "session_id": "xxx",
  "audio_params": {
    "format": "opus",
    "sample_rate": 24000,
    "channels": 1,
    "frame_duration": 60
  },
  "udp": {
    "server": "192.168.1.100",
    "port": 8888,
    "key": "0123456789ABCDEF0123456789ABCDEF",
    "nonce": "0123456789ABCDEF0123456789ABCDEF"
  }
}
```

本地会从中提取：

- `udp.server`
- `udp.port`
- `udp.key`
- `udp.nonce`

## 6. MQTT 控制消息

通过 MQTT 传输的文本 JSON 消息包括：

- `hello`
- `listen`
- `abort`
- `mcp`
- `goodbye`
- 以及服务端返回的 `stt`、`tts`、`llm`、`system` 等

### 6.1 goodbye

客户端主动关闭音频通道时，会发送：

```json
{
  "session_id": "xxx",
  "type": "goodbye"
}
```

服务端主动发来 `goodbye` 时，客户端会关闭 UDP 通道并将当前连接标记为异常结束。

## 7. UDP 音频通道

### 7.1 建立条件

只有在以下条件都满足时，才会打开 UDP：

- 已成功连接 MQTT
- 已收到并解析 server `hello`
- 已拿到合法的 `key` 和 `nonce`
- 已初始化 AES 上下文

### 7.2 音频包结构

当前实现使用固定 16 字节头：

```text
|type 1byte|flags 1byte|payload_len 2bytes|ssrc 4bytes|timestamp 4bytes|sequence 4bytes|
|payload payload_len bytes|
```

字段含义：

- `type`：当前为音频包类型
- `flags`：保留
- `payload_len`：音频负载长度
- `ssrc`：同步源标识
- `timestamp`：时间戳
- `sequence`：单调递增序列号

### 7.3 加密方式

UDP 音频负载使用 AES-CTR：

- 密钥长度：128 位
- 密钥来源：服务端 `udp.key`
- 随机数来源：服务端 `udp.nonce`

### 7.4 序列号处理

实现中维护两组序列号：

- `local_sequence`
- `remote_sequence`

用途：

- 标记本地发送顺序
- 检测接收顺序异常
- 降低重放和乱序影响

## 8. 适合 MQTT + UDP 的场景

- 音频实时性优先
- 服务端控制通道和媒体通道分离
- 需要与上游 MQTT 协议部署保持一致

## 9. 联调建议

```bash
xz_ota check
xz_mqtt show
xz_proto mqtt-udp
xz_reconnect
xz_status
```

若 `xz_status` 显示已建立 MQTT 但没有音频通道，优先检查：

- `subscribe_topic` 是否正确
- server `hello` 是否返回了 `udp` 对象
- `key`、`nonce` 是否为合法十六进制字符串
- UDP 服务器端口是否可达
