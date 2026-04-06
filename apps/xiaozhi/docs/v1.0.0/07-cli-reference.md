# v1.0.0 CLI 命令参考

## 1. 总览

`apps/xiaozhi/src/cli_cmd.c` 注册了以下主要命令：

- `xz_start`
- `xz_stop`
- `xz_reconnect`
- `xz_status`
- `xz_proto`
- `xz_ws`
- `xz_mqtt`
- `xz_sys`
- `xz_wifi`
- `xz_listen`
- `xz_abort`
- `xz_mcp`
- `xz_ota`

## 2. 生命周期命令

### 2.1 启动应用

```bash
xz_start
```

### 2.2 停止应用

```bash
xz_stop
```

### 2.3 重连

```bash
xz_reconnect
```

### 2.4 查看状态

```bash
xz_status
```

## 3. 协议选择

```bash
xz_proto websocket
xz_proto mqtt-udp
```

设置成功后会触发一次重连。

## 4. WebSocket 配置

```bash
xz_ws url wss://your.server/ws
xz_ws token your_token
xz_ws version 1
xz_ws show
```

## 5. MQTT 配置

```bash
xz_mqtt endpoint your.server:8883
xz_mqtt client_id your_client_id
xz_mqtt username your_user
xz_mqtt password your_pass
xz_mqtt publish_topic your/pub/topic
xz_mqtt subscribe_topic your/sub/topic
xz_mqtt keepalive 240
xz_mqtt show
```

## 6. 系统身份与激活参数

```bash
xz_sys device_id your_device_id
xz_sys client_id your_client_id
xz_sys serial_number your_serial
xz_sys activation_secret your_secret
xz_sys show
```

## 7. Wi-Fi 与 OTA 参数

```bash
xz_wifi ssid your_ssid
xz_wifi password your_password
xz_wifi ota_url https://your.server/xiaozhi/ota/
xz_wifi apply
xz_wifi show
```

说明：

- `ota_url` 是 OTA 配置拉取地址
- `apply` 用于应用 Wi-Fi 相关配置

## 8. 语音会话命令

### 8.1 开始监听

```bash
xz_listen start auto
xz_listen start manual
xz_listen start realtime
```

### 8.2 停止监听

```bash
xz_listen stop
```

### 8.3 上报唤醒词检测

```bash
xz_listen detect auto 你好小智
```

## 9. 中断会话

```bash
xz_abort wake_word_detected
```

如果不带参数，会按默认原因处理。

## 10. 发送 MCP 负载

```bash
xz_mcp {"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}
```

## 11. OTA 检查

```bash
xz_ota check
xz_ota url https://your.server/xiaozhi/ota/
```

## 12. 推荐联调命令组合

### 12.1 WebSocket

```bash
xz_ota check
xz_ws show
xz_proto websocket
xz_reconnect
xz_status
```

### 12.2 MQTT + UDP

```bash
xz_ota check
xz_mqtt show
xz_proto mqtt-udp
xz_reconnect
xz_status
```
