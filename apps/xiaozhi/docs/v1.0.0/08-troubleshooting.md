# v1.0.0 常见问题与排障

## 1. `xz_ota check` 失败

优先检查：

- OTA 地址是否正确
- 设备网络是否已通
- `device_id`、`client_id` 是否为空
- 服务端是否要求 `serial_number` 或 `activation_secret`

建议执行：

```bash
xz_sys show
xz_wifi show
```

## 2. OTA 成功但没有连接到语音服务

可能原因：

- OTA 仅下发了配置，但应用尚未重连
- 当前协议不是服务端下发的协议
- WebSocket 或 MQTT 参数仍为空

建议执行：

```bash
xz_ws show
xz_mqtt show
xz_status
xz_reconnect
```

## 3. WebSocket 握手失败

重点检查：

- URL 是否为 `ws://` 或 `wss://`
- 域名和端口是否正确
- TLS 证书是否可用
- token 是否正确

如果是 `wss://`，当前实现会优先尝试通过 IoT DNS 获取证书；找不到证书时会降级为不校验证书的 TLS。

## 4. MQTT 已连接但没有音频

优先检查：

- 是否收到了 server `hello`
- `subscribe_topic` 是否正确
- `udp.server`、`udp.port` 是否可达
- `udp.key`、`udp.nonce` 是否为合法 32 字符十六进制串

## 5. challenge 激活失败

重点检查：

- 是否配置了 `serial_number`
- 是否配置了 `activation_secret`
- 服务端返回的 `challenge` 是否为空

如果没有 `serial_number`，则不会走激活 v2 语义。

## 6. Linux 启动即失败

重点检查 ALSA 默认设备：

- `aplay -L`
- `arecord -L`

当前 Linux 实现默认绑定 `default`，若 `default` 不存在或不可用，应用可能直接启动失败。

## 7. 能收到文本消息但没有音频播放

可能原因：

- 服务端没有发送二进制音频帧
- 本地音频输出设备异常
- 会话提前被 `abort` 或 `goodbye` 中断

## 8. MCP 调用没有结果

重点检查：

- 外层消息是否使用 `type = "mcp"`
- `payload` 是否为合法 JSON-RPC 2.0
- `params` 是否为 object

注意：当前实现对 `params` 非 object 的行为是静默丢弃，不回错误。

## 9. 配置写入后似乎没生效

重点检查：

- 是否执行了 `xz_reconnect`
- 是否被环境变量覆盖
- 是否有 OTA check 再次覆盖本地配置

## 10. 最小排查顺序

建议按下面顺序排查：

1. `xz_status`
2. `xz_sys show`
3. `xz_ws show` 或 `xz_mqtt show`
4. `xz_ota check`
5. `xz_reconnect`
6. 再次观察服务端日志和本地日志
