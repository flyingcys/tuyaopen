# xiaozhi PortAudio + Snowboy 设计

日期：2026-04-06
范围：`apps/xiaozhi`

## 背景

`apps/xiaozhi` 当前已经具备：

- 通过 OTA check 获取服务器下发的 WebSocket / MQTT 配置与激活信息
- 通过 MQTT 和 WebSocket 连接服务器
- Linux 下 WebSocket 语音链路的基础实现

当前问题有两类：

1. 需要先拿到验证码，让设备可以加入目标服务器完成激活。
2. Linux 语音链路仍依赖 `tdl_audio` 注册的 `alsa_audio`，实际效果不理想，需要替换为：
   - `PortAudio` 作为采集与播放后端
   - `Snowboy` 作为本地热词唤醒

用户要求：

- 保留现有手动听写能力
- 新增 `Snowboy` 自动唤醒
- 参考 `/home/share/samba/Demo4Echo` 的适配
- 代码复制到 `apps/xiaozhi/src` 下并分目录管理
- 在当前 Ubuntu 上可编译运行
- 在 `apps/xiaozhi` 目录下执行 `tos.py build` 完成编译

## 目标

本次改造的可交付结果是：

1. 能通过现有 OTA/check 流程获取激活验证码，并把验证码输出给用户用于服务器侧绑定。
2. 服务器绑定完成后，`apps/xiaozhi` 能继续使用现有 WebSocket 协议逻辑建立连接。
3. Linux 语音上下行切换到 `PortAudio + Opus`。
4. 保留 `xz_listen start manual` 手动模式。
5. 新增后台热词检测，空闲态由 `Snowboy` 唤醒后进入与手动模式相同的上行/下行语音会话流程。
6. `tos.py build` 在当前 Ubuntu 环境可通过。

## 非目标

本次不做以下事项：

- 不改动 MQTT/UDP 音频链路
- 不改动云端协议格式
- 不引入独立守护进程或额外 IPC
- 不对 Tuya 平台通用音频框架做大规模重构
- 不处理多热词、在线模型更新、AEC/NS/AGC 等增强能力

## 设计方案

采用“替换 Linux 音频后端、协议层基本不动”的方案。

核心原则：

- 保持 `xiaozhi_ws.c`、`xiaozhi_protocol.c` 的协议行为稳定
- 将 Linux 音频实现收敛到 `xiaozhi_audio_linux.*` 门面后面
- 仅为 Linux 目标引入 C/C++ 混编、PortAudio 和 Snowboy 依赖

### 目录结构

在 `apps/xiaozhi/src` 下新增分目录：

- `audio/`
  - `xz_portaudio_device.c/.h`
  - `xz_audio_opus_bridge.c/.h`
- `wakeword/`
  - `xz_snowboy_runner.cc/.h`
- `third_party/snowboy_wrapper/`
  - 从 `Demo4Echo` 复制并裁剪的 C wrapper 头文件与实现
- `resources/`
  - `common.res`
  - `models/echo.pmdl`

保留现有 `xiaozhi_audio_linux.c/.h` 作为统一入口，对外接口不大改。

### 模块职责

`xiaozhi_audio_linux.*`

- 对上层继续提供：
  - `init/deinit`
  - `start_capture/stop_capture`
  - `feed_opus`
  - `abort_playback`
  - `reset`
- 管理 Linux 音频全局状态
- 协调 PortAudio、Opus 编解码和 Snowboy 生命周期

`audio/xz_portaudio_device.*`

- 封装 PortAudio 初始化与清理
- 打开默认输入/输出设备
- 以 16kHz、单声道、16-bit PCM 工作
- 提供阻塞式或回调式读写接口给上层
- 保证手动采集、播放和热词检测对同一设备的访问有明确时序

`audio/xz_audio_opus_bridge.*`

- 负责 PCM 累积成 20ms 帧
- 负责 Opus 编码与解码
- 为 `xiaozhi_audio_linux` 提供“PCM in / Opus out”与“Opus in / PCM out”能力

`wakeword/xz_snowboy_runner.*`

- 封装 Snowboy detector 的创建、销毁、重置和检测调用
- 使用复制进仓库的 C wrapper，避免上层 C 代码直接依赖复杂 C++ API
- 提供后台检测线程控制接口

### 数据流

手动模式：

1. 用户执行 `xz_listen start manual`
2. 现有应用状态机进入 listen
3. `xiaozhi_audio_linux_start_capture()` 启动 PortAudio 采集
4. 采集 PCM 经 Opus 编码后，沿用当前 WebSocket 音频发送回调上传
5. 服务端返回 Opus 音频包
6. `xiaozhi_audio_linux_feed_opus()` 解码后通过 PortAudio 播放

自动唤醒模式：

1. 应用空闲时启动后台唤醒线程
2. 唤醒线程持续从输入设备读取 16k PCM
3. PCM 送入 Snowboy 检测
4. 命中热词后：
   - 停止或暂停唤醒检测
   - 触发与手动模式相同的 listen 启动路径
5. 会话结束后恢复唤醒检测

### 状态与并发约束

需要避免一个输入设备被多个逻辑同时消费，因此状态约束如下：

- `idle_detecting`
  - 仅唤醒线程占用输入设备
- `manual_listening`
  - 仅语音采集链路占用输入设备
- `session_playing`
  - 输出设备用于 TTS 播放
- `session_active`
  - 禁止热词检测

切换规则：

- 从 `idle_detecting` 进入 listen 前，必须先停止唤醒线程对输入流的占用
- 会话结束后，再重新启动唤醒线程
- 若用户手动触发 `manual` 模式，则显式暂停唤醒检测

### 命令行为

保留：

- `xz_listen start manual`
- `xz_listen stop`
- `xz_listen detect`

扩展语义：

- `xz_listen detect` 在 Linux 下进入“待机热词唤醒”模式
- 若应用启动即希望进入待机，可在初始化阶段自动开启 detect
- `manual` 和 `detect` 共存，但同一时刻只能有一种输入模式占用麦克风

### 资源与路径

Snowboy 资源文件与模型文件复制到 `apps/xiaozhi/src/resources/` 下。

运行时优先按以下顺序定位：

1. 环境变量覆盖路径
2. 可执行文件相对路径
3. 源码树内默认路径

这样可以兼容 `dist/` 产物运行与开发态直接运行。

### 构建设计

`apps/xiaozhi/CMakeLists.txt` 需要调整为：

- 允许混编 `.c` 与 `.cc`
- 新增 `audio/`、`wakeword/`、`third_party/snowboy_wrapper/` 源文件
- Linux 目标下通过 `pkg-config` 引入 `portaudio-2.0`
- 链接 Snowboy 静态库及其依赖
- 将 Snowboy 资源文件复制或安装到最终运行目录

当前 `xiaozhi_audio_linux.c` 会移除对 `tdl_audio_manage` 和 `alsa_audio` 的强依赖，避免继续走 `tdl_audio_find(AUDIO_CODEC_NAME, ...)`。

### 验证码获取设计

不新增新的验证码接口，继续复用当前 OTA/check 流程：

1. 初始化环境
2. 执行 OTA check
3. 从返回 JSON 中的 `activation.code` 提取验证码
4. 通过日志输出给用户
5. 由用户在服务器侧完成绑定

若返回的是 `activation.challenge` 而不是 `activation.code`，则保持现有激活逻辑，不额外发明新流程。

### 错误处理

需要明确处理以下错误：

- PortAudio 初始化失败
- 默认输入或输出设备不存在
- Snowboy 模型或资源文件缺失
- Snowboy detector 初始化失败
- Opus 编解码失败
- 会话状态切换期间重复启动/停止采集

错误策略：

- 初始化错误直接返回失败并打印清晰日志
- 会话中播放/采集错误尽量中止当前链路并恢复到空闲态
- 热词检测错误不应导致进程死循环或忙等

## 测试与验证

最小验证链路如下：

1. `tos.py check`
2. 获取 OTA 返回的激活验证码
3. 用户完成服务器绑定
4. `tos.py build`
5. 启动 Linux 可执行文件并确认成功连接 WebSocket 服务器
6. 执行 `xz_listen start manual` 验证手动上行和下行
7. 执行 `xz_listen detect` 验证热词唤醒后进入相同语音链路

补充验证：

- 对新增或修改的测试运行最小相关测试
- 若无法做真实音频端到端自动化测试，至少保证编译通过、已有单元测试不回归，并记录人工验证步骤

## 实施顺序

1. 跑通 OTA/check，获取验证码
2. 梳理并复制 `Demo4Echo` 中最小可复用的 PortAudio 与 Snowboy 代码
3. 在 `apps/xiaozhi/src` 下建立分目录并接入 CMake
4. 用 PortAudio 重写 Linux 音频采集与播放门面
5. 接入 Snowboy 唤醒线程
6. 保持 `manual` 模式，同时补充 `detect` 模式
7. 编译并做最小链路验证

## 风险

- Snowboy 自带静态库与当前 Ubuntu/编译器 ABI 不兼容
- `tos.py build` 的 Linux 工具链环境里缺少 `portaudio` 开发包
- 运行时资源路径与 `dist/` 产物布局不匹配
- 现有 listen 状态机与自动唤醒存在竞态

对应缓解：

- 优先复用 `Demo4Echo` 的 Ubuntu64 Snowboy 库并尽早做链接验证
- 通过 `pkg-config --modversion portaudio-2.0` 提前确认依赖
- 在代码里支持环境变量覆盖资源路径
- 先保留手动模式回退路径，保证自动唤醒问题不阻塞基础语音链路
