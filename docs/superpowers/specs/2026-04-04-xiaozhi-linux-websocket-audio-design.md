# xiaozhi Linux WebSocket 语音链路设计

> 日期：2026-04-04  
> 范围：`TuyaOpen/apps/xiaozhi`  
> 目标平台：`Linux / Ubuntu`  
> 音频接口：`ALSA PCM`  
> 对齐目标：`xiaozhi-esp32` 的 WebSocket 语音协议与核心状态流转

## 1. 目标

为 `apps/xiaozhi` 补齐 Linux 端的 WebSocket 语音上行与下行链路，使其在不引入 VAD、唤醒词和 `ai_audio_input/ai_audio_player` 的前提下，实现以下能力：

- 手动进入 `listening` 后，从 ALSA 麦克风采集 PCM 音频。
- 将 PCM 按固定帧长编码为 Opus，并通过 WebSocket binary 帧上传。
- 收到服务端 TTS binary 帧后，解码为 PCM 并通过 ALSA 播放。
- 文本控制面继续沿用现有 `hello/listen/abort/mcp` 行为。
- 状态机尽量对齐 `xiaozhi-esp32`，但首版不实现自动回听、VAD 自动停录、唤醒词。

## 2. 非目标

本次设计明确不包含以下内容：

- MQTT+UDP 音频通道。
- VAD、唤醒词、自动停录、自动回听。
- `snow-boy` 集成。
- 复用 `ai_audio_input` 或 `ai_audio_player` 业务模块。
- GUI、屏幕、提示音或显示层对齐。

## 3. 设计约束

- 仅面向 Linux 平台生效，其他平台行为不变。
- 音频格式固定为 `16kHz / mono / S16_LE`。
- WebSocket 音频帧首版按 `xiaozhi-esp32` 的 `version=1` 语义处理：
  - 上行 binary 直接发送 Opus 包体。
  - 下行 binary 直接视为 Opus 包体。
- 为后续接入 `snow-boy` 预留清晰边界，音频采集/编码/播放不能直接塞进协议文件。

## 4. 总体方案

新增一个 Linux 音频运行时模块，作为 `xiaozhi_app` 与底层 ALSA/Opus 之间的桥接层。

职责分配如下：

- `xiaozhi_ws.c`
  - 补齐 WebSocket binary 发送接口。
  - 补齐 WebSocket binary 接收回调接口。
  - 保持文本消息逻辑不变。
- `xiaozhi_app.c`
  - 继续作为上层状态机与协议协调入口。
  - 在 `listen start/stop`、`tts start/stop`、连接异常等事件上驱动音频模块。
  - 在音频模块和 WebSocket 模块之间转发上行 Opus 包与下行 Opus 包。
- `xiaozhi_audio_linux.c/.h`
  - 负责 ALSA 设备注册与打开。
  - 负责 PCM 采集缓冲、Opus 编码、Opus 解码、PCM 播放。
  - 通过回调把编码后的 Opus 包交给 `xiaozhi_app` 发送。
  - 暴露简单的 `start_capture/stop_capture/feed_opus/reset` 接口给应用层。

## 5. 数据流

### 5.1 上行语音

1. 用户通过 CLI 执行 `xz_listen start [mode]`。
2. `xiaozhi_app` 切换到 `listening` 状态，并启动 Linux 音频模块的采集上传。
3. Linux 音频模块通过 `board_register_hardware()` 注册 ALSA 设备，并使用 `tdl_audio_find/open` 获取麦克风 PCM。
4. 麦克风回调持续收到 `PCM / 16kHz / mono / S16_LE` 数据。
5. 音频模块将零散 PCM 聚合为固定 20ms 帧，即 320 samples / 640 bytes。
6. 每帧通过 Opus 编码得到一个压缩包。
7. 编码后包体通过音频模块的发送回调交给 `xiaozhi_app`。
8. `xiaozhi_app` 调用新的 `xz_ws_send_audio()`，以 WebSocket binary 形式发送给服务端。

### 5.2 下行语音

1. WebSocket 收到 binary 帧。
2. `xiaozhi_ws` 不再只打印日志，而是通过新的 binary 回调把原始负载交给 `xiaozhi_app`。
3. `xiaozhi_app` 将 Opus 包交给 Linux 音频模块。
4. Linux 音频模块使用 Opus 解码器将包体还原为 PCM。
5. 解码后的 PCM 通过 `tdl_audio_play()` 输出到 ALSA 播放设备。

### 5.3 文本控制

- `tts start`
  - `xiaozhi_app` 切换到 `speaking`。
  - 停止上行录音上传。
  - 清理上行聚合缓存，避免残留音频继续发送。
- `tts stop`
  - `xiaozhi_app` 切换回 `idle`。
  - 首版不自动回到 `listening`。
- `listen stop`
  - 停止录音上传。
  - 切换回 `idle`。
- 连接关闭或异常
  - 停止录音。
  - 停止播放。
  - 清空音频运行时缓存与编解码状态。

## 6. 状态机

首版保留 3 个有效状态：

- `idle`
  - 默认态。
  - 不主动录音，不主动播放。
- `listening`
  - 通过手动 `xz_listen start` 进入。
  - 持续采集、编码、上传麦克风音频。
- `speaking`
  - 收到 `tts start` 进入。
  - 停止录音上传。
  - 接收并播放后续 WebSocket binary 音频。

关键状态转换：

- `idle -> listening`
  - 条件：手动 `listen start` 成功发送，且当前连接处于 ready。
- `listening -> idle`
  - 条件：手动 `listen stop`，或连接异常，或连接关闭。
- `listening -> speaking`
  - 条件：收到服务端 `tts start`。
- `speaking -> idle`
  - 条件：收到服务端 `tts stop`，或 `abort`，或连接异常。

与 `xiaozhi-esp32` 的差异保留为显式约束：

- 不实现 `speaking -> listening` 自动回听。
- 不实现基于 VAD 的自动停录。
- 不实现唤醒词驱动的 `idle -> connecting -> listening`。

## 7. 模块边界

### 7.1 `xiaozhi_audio_linux`

建议新增接口：

- `xiaozhi_audio_linux_init(...)`
- `xiaozhi_audio_linux_deinit(...)`
- `xiaozhi_audio_linux_start_capture(...)`
- `xiaozhi_audio_linux_stop_capture(...)`
- `xiaozhi_audio_linux_feed_opus(...)`
- `xiaozhi_audio_linux_abort_playback(...)`
- `xiaozhi_audio_linux_reset(...)`

内部职责：

- 注册并持有 `TDL_AUDIO_HANDLE_T`。
- 管理 Opus encoder / decoder 生命周期。
- 管理 PCM 聚合缓存。
- 在 speaking/listening 切换时执行缓冲清理和状态重置。

### 7.2 `xiaozhi_ws`

建议新增接口：

- `xz_ws_send_audio(...)`
- `xz_ws_set_binary_message_callback(...)`

要求：

- 不在 `xiaozhi_ws.c` 内实现编解码。
- 仅负责 binary 帧收发和回调分发。
- 文本与二进制回调分开，避免音频逻辑进入协议文件。

### 7.3 `xiaozhi_app`

新增职责：

- 在初始化阶段初始化 Linux 音频模块。
- 在 `listen start/stop` 时调用音频模块启停采集。
- 在 `tts start/stop` 时控制 speaking/listening 切换。
- 将上行 Opus 回调桥接到 `xz_ws_send_audio()`。
- 将 WebSocket binary 回调桥接到 `xiaozhi_audio_linux_feed_opus()`。

## 8. 配置方案

首版建议补充 Linux 配置项：

- `CONFIG_ENABLE_AUDIO_ALSA=y`
- `CONFIG_ALSA_DEVICE_CAPTURE="default"`
- `CONFIG_ALSA_DEVICE_PLAYBACK="default"`
- `CONFIG_AUDIO_CODEC_NAME="alsa_audio"`

同时保留环境变量覆盖能力，便于 Ubuntu 现场调试：

- `XZ_ALSA_CAPTURE`
- `XZ_ALSA_PLAYBACK`

环境变量优先级高于编译期默认值，用于快速切换设备而无需重新编译。

## 9. 错误处理

- ALSA 注册失败：
  - 应用继续启动，但 `xiaozhi` 语音功能不可用。
  - `xz_status` 可看到连接正常但语音不可用。
- Opus encoder / decoder 初始化失败：
  - 明确打印错误日志。
  - 阻止进入 listening 或 speaking 音频路径。
- WebSocket binary 发送失败：
  - 记录错误并触发连接错误处理。
- 采集回调收到非 PCM 格式或长度异常：
  - 丢弃该帧并打日志。
- 解码失败：
  - 丢弃该包，不中断整条连接。

## 10. 测试策略

### 10.1 单元测试

优先补充纯函数或可隔离逻辑的测试：

- PCM 聚合逻辑：
  - 输入多段碎片 PCM，验证能按 20ms 正确拼帧。
- WebSocket binary 回调分发：
  - 验证 text 与 binary 不混淆。
- 状态切换：
  - `listen start -> listening`
  - `tts start -> speaking`
  - `tts stop -> idle`

### 10.2 Linux 集成验证

在 Ubuntu 环境上执行最小验证：

1. 编译 `apps/xiaozhi` Linux 目标。
2. 设置 WebSocket 地址与 token。
3. 启动应用并确认 `hello` 握手成功。
4. 手动执行 `xz_listen start manual`。
5. 确认麦克风音频通过 WebSocket binary 持续发送。
6. 触发服务端返回 `tts start + binary + tts stop`。
7. 确认本地能听到播放，且状态回到 `idle`。

### 10.3 回归检查

- 原有 `hello/listen/abort/mcp` 文本通道不回退。
- MQTT+UDP 代码路径不受影响。
- 无音频设备场景下，应用仍能正常启动和连网。

## 11. 后续扩展位

该设计为后续能力预留以下扩展点：

- `snow-boy` 唤醒词：
  - 复用 ALSA 采集侧数据源。
  - 在 `idle` 态增设唤醒词检测链路。
- VAD：
  - 在 PCM 采集侧加入软件 VAD。
  - 再把 `listening -> stop` 的时机从手动扩展为自动。
- MQTT+UDP：
  - 复用同一套 Linux 音频运行时。
  - 仅替换“编码包的发送路径”和“下行包的接收路径”。

## 12. 推荐实施顺序

1. 补 `xiaozhi_ws` binary 收发接口。
2. 新增 `xiaozhi_audio_linux` 模块并打通 ALSA 采集与播放。
3. 在 `xiaozhi_app` 中接入状态机与音频回调桥接。
4. 补最小单元测试与 Linux 端集成验证。

