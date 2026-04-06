# v1.0.0 Linux 构建、运行与验证

## 1. 适用范围

本章节面向 Cursor Cloud 或本机 Linux 环境，说明如何构建并运行 `apps/xiaozhi`。

## 2. 环境初始化

在仓库根目录执行：

```bash
cd /workspace && . ./export.sh
```

如果当前工作目录就是仓库根，也可以执行：

```bash
. ./export.sh
```

为避免平台更新提示阻塞，建议执行：

```bash
mkdir -p .cache && touch .cache/.dont_prompt_update_platform
```

## 3. 基本检查

建议先执行：

```bash
tos.py check
```

## 4. 构建步骤

```bash
. ./export.sh
cd apps/xiaozhi
tos.py build
```

构建产物通常位于：

- 中间产物：`apps/xiaozhi/.build/`
- 最终产物：`apps/xiaozhi/dist/`

当前仓库可见产物示例：

- `dist/xiaozhi_1.0.0/xiaozhi_1.0.0.elf`

## 5. Linux 下的 WebSocket 运行方式

Linux 场景下，优先使用 WebSocket 验证。

### 5.1 通过环境变量配置

```bash
export XZ_WS_URL=wss://your.server/ws
export XZ_WS_TOKEN=your_token
export XZ_PROTOCOL=websocket
```

### 5.2 启动程序

```bash
./dist/xiaozhi_1.0.0/xiaozhi_1.0.0.elf
```

## 6. Linux 音频说明

当前实现的 Linux 音频链路有以下特点：

- 主要支持 WebSocket 语音通道
- 采集与回放设备默认绑定 ALSA `default`
- 运行时不再通过环境变量切换音频设备
- 若 ALSA 默认设备不存在，应用会在启动阶段失败

因此在部署前应先确保：

- `arecord -L` 与 `aplay -L` 可看到默认设备
- ALSA 默认设备配置正确

## 7. 手动验证步骤

### 7.1 OTA 拉取默认配置

```bash
xz_ota check
```

### 7.2 查看当前状态

```bash
xz_status
```

### 7.3 手动启动听写

```bash
xz_listen start manual
```

### 7.4 预期行为

正常情况下应出现：

1. 客户端开始上传麦克风数据
2. 服务端返回 `tts start`
3. 服务端返回若干二进制音频帧
4. 服务端返回 `tts stop`
5. 本地听到回放音频

### 7.5 结束会话

```bash
xz_listen stop
```

## 8. 适合在 Linux 验证的内容

- WebSocket 握手
- hello 交换
- listen / abort / mcp 文本消息
- 音频上传与 TTS 播放
- OTA 配置拉取

## 9. 不建议在 Linux 首轮验证的内容

- 依赖特定板级能力的显示、硬件控制
- 与 ESP 分区、硬件 efuse 强相关的行为
