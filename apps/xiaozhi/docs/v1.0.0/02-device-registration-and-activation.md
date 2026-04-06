# v1.0.0 设备注册与激活

## 1. 目标

本章节说明 `apps/xiaozhi` 如何完成以下工作：

- 生成或读取设备身份
- 向 OTA 服务发起配置检查
- 根据服务端返回内容执行激活
- 将返回的 WebSocket 或 MQTT 配置落地到本地设置

## 2. 设备注册所需身份字段

代码位置：`apps/xiaozhi/src/xiaozhi_ota.c`、`apps/xiaozhi/src/cli_cmd.c`

主要字段如下：

- `Device-Id`
  - 设备标识
  - 会放在 OTA 请求头中
  - 也可通过 `xz_sys device_id <value>` 手动写入
- `Client-Id`
  - 客户端实例标识
  - 会放在 OTA 请求头中
  - 也可通过 `xz_sys client_id <value>` 手动写入
- `Serial-Number`
  - 可选
  - 用于切换到激活 v2 逻辑
  - 可通过 `xz_sys serial_number <value>` 写入
- `activation_secret`
  - 可选
  - 在服务端下发 `challenge` 时用来计算 HMAC
  - 可通过 `xz_sys activation_secret <value>` 写入

查看当前身份配置：

```bash
xz_sys show
```

## 3. OTA 配置检查流程

代码位置：`apps/xiaozhi/src/xiaozhi_ota.c`

默认 OTA 地址：

```text
https://api.tenclass.net/xiaozhi/ota/
```

也可以通过以下方式覆盖：

```bash
xz_ota url https://your.server/xiaozhi/ota/
```

或使用环境变量：

```bash
XZ_OTA_URL=https://your.server/xiaozhi/ota/
```

触发配置检查：

```bash
xz_ota check
```

## 4. OTA check 请求内容

### 4.1 HTTP 头

当前实现会发送以下关键请求头：

- `Activation-Version`
- `Device-Id`
- `Client-Id`
- `Serial-Number`
- `User-Agent`
- `Accept-Language`
- `Content-Type`

其中 `Activation-Version` 的判断规则是：

- 有 `serial_number` 时，使用 `2`
- 没有 `serial_number` 时，使用 `1`

### 4.2 请求体

请求体为 JSON，主要包含：

- 顶层基础字段：
  - `version`
  - `language`
  - `flash_size`
  - `minimum_free_heap_size`
  - `mac_address`
  - `uuid`
  - `chip_model_name`
- 子对象：
  - `chip_info`
  - `application`
  - `partition_table`
  - `ota`
  - `display`
  - `board`

这些字段用于让 OTA 服务判断：

- 设备型号和板型
- 当前运行版本
- 是否需要升级
- 是否应该下发 MQTT 或 WebSocket 配置
- 是否需要激活

## 5. OTA 返回内容与本地落地

服务端可能返回以下对象：

- `mqtt`
- `websocket`
- `activation`
- `firmware`

应用会根据返回内容写入本地设置空间：

- `mqtt` 写入 `XZ_NS_MQTT`
- `websocket` 写入 `XZ_NS_WS`
- 选中的协议写入协议设置

协议选择规则：

- 如果返回 `mqtt`，优先使用 `mqtt-udp`
- 否则如果返回 `websocket`，使用 `websocket`

## 6. 激活流程

### 6.1 两种激活模式

服务端可返回 `activation` 对象，其中可能包括：

- `message`
- `code`
- `challenge`
- `timeout_ms`

当前支持两种激活方式：

1. 激活码展示
   - 服务端下发 `code`
   - 设备向用户展示激活码
   - 用户在配套平台完成绑定

2. challenge 应答激活
   - 服务端下发 `challenge`
   - 设备使用 `activation_secret` 计算 HMAC
   - 再向 OTA 服务发起 activate 请求

### 6.2 激活 v1 与 v2

- v1：没有 `serial_number`
  - 激活请求体通常为空对象 `{}`
- v2：有 `serial_number`
  - 激活请求体包含：
    - `algorithm`
    - `serial_number`
    - `challenge`
    - `hmac`

## 7. 典型注册与激活操作步骤

### 7.1 手动注入身份并发起检查

```bash
xz_sys device_id your_device_id
xz_sys client_id your_client_id
xz_sys serial_number your_serial
xz_sys activation_secret your_secret
xz_ota check
```

### 7.2 仅依赖 OTA 自动下发协议配置

```bash
xz_ota check
xz_status
```

如果 OTA 已返回 MQTT 或 WebSocket 配置，本地就会得到对应参数。

## 8. 注册成功后会发生什么

完成 OTA 检查和激活后，应用进入可连接状态，后续通常会：

1. 依据协议设置选择 `websocket` 或 `mqtt-udp`
2. 创建控制通道
3. 发起 `hello`
4. 收到服务端 `hello`
5. 进入 listen / tts / mcp 正常交互

## 9. 常见注意事项

- 没有 `serial_number` 时，不会走激活 v2。
- 没有 `activation_secret` 时，challenge 激活无法完成。
- OTA 只负责下发配置和升级信息，不等于语音会话已建立。
- OTA 成功后仍需执行协议连接流程。
