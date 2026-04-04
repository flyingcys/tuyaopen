# xiaozhi (TuyaOpen)

`apps/xiaozhi` 是基于 TuyaOpen 重新实现的 xiaozhi 应用，协议行为对齐 `xiaozhi-esp32`：
- WebSocket：`hello/listen/abort/mcp` 控制通道
- MQTT+UDP：MQTT 控制 + UDP 音频通道（AES-CTR 包结构）

## 目录

```text
apps/xiaozhi
├── app_default.config
├── config/
│   ├── Linux.config
│   └── T5AI.config
└── src/
    ├── tuya_main.c
    ├── cli_cmd.c
    ├── xiaozhi_app.c
    ├── xiaozhi_ws.c
    ├── xiaozhi_mqtt_udp.c
    ├── xiaozhi_protocol.c
    ├── xiaozhi_system.c
    └── xiaozhi_settings.c
```

## 构建

在 `TuyaOpen` 根目录执行：

```bash
. ./export.sh
cd apps/xiaozhi
tos.py build
```

## 运行（Linux）

Linux 默认走有线网络（wired）。启动后可通过 CLI 配置服务器并发起连接。

### OTA 拉取默认配置（与 xiaozhi-esp32 流程一致）

```bash
xz_ota check
# 或设置自定义 OTA 地址
xz_ota url https://api.tenclass.net/xiaozhi/ota/
```

### WebSocket 示例

```bash
xz_ws url wss://your.server/ws
xz_ws token your_token
xz_ws version 1
xz_proto websocket
xz_reconnect
xz_status
```

### Linux 语音链路（WebSocket）

首版 Linux 语音链路仅支持基于 WebSocket 的音频通道。默认采集和回放均使用 ALSA 设备 `default`，名称由 `config/Linux.config` / `app_default.config` 中的 `CONFIG_ALSA_DEVICE_CAPTURE` 和 `CONFIG_ALSA_DEVICE_PLAYBACK` 配置项控制。语音链路在收到 `listen` 请求后会上传麦克风流、等待服务端返回 `tts start`/`binary`/`tts stop` 并播放返回的音频。

### Linux WebSocket 可执行物运行

1. `. ./export.sh`
2. `cd apps/xiaozhi`
3. `tos.py build`
4. 设置环境变量：
   - `XZ_WS_URL`（WebSocket 服务地址）
   - `XZ_WS_TOKEN`（身份令牌）
   - `XZ_PROTOCOL=websocket`
   - 需要替换 ALSA 设备名时，请在构建前修改 `config/Linux.config` / `app_default.config` 中的 `CONFIG_ALSA_DEVICE_CAPTURE` 与 `CONFIG_ALSA_DEVICE_PLAYBACK`。
5. 启动生成的可执行文件：`./dist/xiaozhi_1.0.0/xiaozhi_1.0.0.elf`

### 手动语音链路验证

1. 运行 `xz_status` 确认网络连接正常。
2. 用 `xz_listen start manual` 进入手动听写模式。
3. 讲话期间客户端会上传麦克风数据至服务端。
4. 服务端应该依次返回 `tts start`、`binary`、`tts stop`，客户端播放时可听到返回的音频。
5. 结束后执行 `xz_listen stop` 退出语音会话。

### MQTT+UDP 示例

```bash
xz_mqtt endpoint your.mqtt.server:8883
xz_mqtt client_id your_client_id
xz_mqtt username your_user
xz_mqtt password your_pass
xz_mqtt publish_topic your/topic
xz_mqtt subscribe_topic your/topic
xz_mqtt keepalive 240
xz_proto mqtt-udp
xz_reconnect
xz_status
```

### 激活参数配置

当目标板没有可直接使用的序列号或需要手动注入激活密钥时，可通过 CLI 持久化配置：

```bash
xz_sys serial_number your_serial_number
xz_sys activation_secret your_hmac_secret
xz_sys show
```

## CLI 主要命令

- `xz_start` / `xz_stop` / `xz_reconnect` / `xz_status`
- `xz_proto <websocket|mqtt-udp>`
- `xz_ws <url|token|version|show> [value]`
- `xz_mqtt <endpoint|client_id|username|password|publish_topic|subscribe_topic|keepalive|show> [value]`
- `xz_sys <device_id|client_id|serial_number|activation_secret|show> [value]`
- `xz_wifi <ssid|password|ota_url|apply|show> [value]`
- `xz_listen <start|stop|detect> [mode] [text]`
- `xz_abort [reason]`
- `xz_mcp <json_payload>`
- `xz_ota [check] | xz_ota url <url>`

## 协议参考

- `xiaozhi-esp32/docs/websocket.md`
- `xiaozhi-esp32/docs/mqtt-udp.md`
