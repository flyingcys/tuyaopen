# xiaozhi (TuyaOpen)

`apps/xiaozhi` is a TuyaOpen port aligned with `xiaozhi-esp32` protocol behavior:
- WebSocket control channel: `hello/listen/abort/mcp`
- MQTT+UDP hybrid channel: MQTT control + UDP audio (AES-CTR packet flow)

## Build

From TuyaOpen repository root:

```bash
. ./export.sh
cd apps/xiaozhi
tos.py build
```

## Linux Quick Start

Linux target typically runs with wired networking.

### OTA Bootstrap (aligned with xiaozhi-esp32)

```bash
xz_ota check
# or set custom OTA URL
xz_ota url https://api.tenclass.net/xiaozhi/ota/
```

### WebSocket

```bash
xz_ws url wss://your.server/ws
xz_ws token your_token
xz_ws version 1
xz_proto websocket
xz_reconnect
xz_status
```

### MQTT+UDP

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

## Main CLI Commands

- `xz_start` / `xz_stop` / `xz_reconnect` / `xz_status`
- `xz_proto <websocket|mqtt-udp>`
- `xz_ws <url|token|version|show> [value]`
- `xz_mqtt <endpoint|client_id|username|password|publish_topic|subscribe_topic|keepalive|show> [value]`
- `xz_wifi <ssid|password|ota_url|apply|show> [value]`
- `xz_listen <start|stop|detect> [mode] [text]`
- `xz_abort [reason]`
- `xz_mcp <json_payload>`
- `xz_ota [check] | xz_ota url <url>`

## Protocol References

- `xiaozhi-esp32/docs/websocket.md`
- `xiaozhi-esp32/docs/mqtt-udp.md`
