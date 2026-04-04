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

### Linux Voice Pipeline (WebSocket)

The Linux build currently supports only the WebSocket voice channel. ALSA capture and playback devices default to `default`, and their names are sourced from the `CONFIG_ALSA_DEVICE_CAPTURE` and `CONFIG_ALSA_DEVICE_PLAYBACK` entries in `config/Linux.config` / `app_default.config`. After a `listen` request, the client uploads microphone data, waits for `tts start`/`binary`/`tts stop` from the server, and plays back the returned audio.

### Running the WebSocket Executable

1. `. ./export.sh`
2. `cd apps/xiaozhi`
3. `tos.py build`
4. Set the environment variables:
   - `XZ_WS_URL`
   - `XZ_WS_TOKEN`
   - `XZ_PROTOCOL=websocket`
   - If you need to change the ALSA device names, update `CONFIG_ALSA_DEVICE_CAPTURE` and `CONFIG_ALSA_DEVICE_PLAYBACK` in `config/Linux.config` / `app_default.config` before building.
5. Execute the generated binary: `./dist/xiaozhi_1.0.0/xiaozhi_1.0.0.elf`

### Manual Voice Validation

1. Run `xz_status` to ensure the network/session is ready.
2. Start manual listening with `xz_listen start manual`.
3. Speak so the client uploads microphone data to the server.
4. The server should return `tts start`, `binary`, and `tts stop` in sequence; the client plays the returned audio locally.
5. Finish by running `xz_listen stop`.

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
