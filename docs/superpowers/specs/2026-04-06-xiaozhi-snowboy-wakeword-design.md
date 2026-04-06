# xiaozhi Snowboy 唤醒设计

日期：2026-04-06  
范围：只改 `apps/xiaozhi/src` 里与 Snowboy / 唤醒相关的文件（`wakeword`、`third_party/snowboy_wrapper`、`resources`、`cli_cmd.c`、`xiaozhi_app.c`），其他代码保持不动，PortAudio 主体实现不触碰。

## 背景

`apps/xiaozhi` 当前已经具备 Linux 下通过 WebSocket/MQTT 与服务端通信、OTA 检查与音频上行/下行的基础架构，音频采集由 `xiaozhi_audio_linux` 对接 `tdl_audio`，编码后走现有队列再发送到服务器。`Demo4Echo` 里已有符合要求的 Snowboy + PortAudio 唤醒实现（`Idle.cc`/`third_party/snowboy`），可以借鉴模型、资源与检测流程。

本任务只负责 **最小迁移 Snowboy + 现有音频流程的唤醒接入**，不触及 PortAudio 续改（只做唤醒相关代码准备），最终目标是在 `apps/xiaozhi` 中添加后台热词检测，命中时走现有的 `listen start` 逻辑，并提供 CLI 级的控制/状态查看。

## 假设与范围

- 采用 **Idle 自动监听** 模式：`xiaozhi_app` 启动后默认开启 Snowboy 后台线程，处于空闲（`XZ_CHAT_IDLE`）时持续抓取麦克风 PCM；当检测到唤醒词，直接调用与手动模式一致的 `xiaozhi_app_send_listen("start","auto",NULL)`。
- 现有 `tdl_audio` 采集仍然负责获取麦克风，并通过 `xiaozhi_audio_linux` 编码。但在空闲模式下，不会立即把 Opus 数据推送给服务器，只有唤醒后才允许发送。
- 需要先在 `apps/xiaozhi/src` 下新增目录：  
  - `wakeword/`（Snowboy 线程、队列与状态）  
  - `third_party/snowboy_wrapper/`（从 Demo 里裁剪而来的 C 包装器、静态库以及必要的头文件）  
  - `resources/`（`common.res`、`models/echo.pmdl`，运行时由代码拼出绝对路径）  
- 除了以上目录，还需在 `cli_cmd.c` 增加 `xz_wake` 之类命令，在 `xiaozhi_app.c` 中维护检测状态、开启/暂停检测流程，并在检测命中时触发 listen 。
- 由于当前 `apps/xiaozhi` 专注于 Linux 目标，假定系统自带的 `libblas/liblapack` 可用，且 `libsnowboy-detect.a` 用的是 `_GLIBCXX_USE_CXX11_ABI=0`，新编译单元需要加对应编译定义。
- 如果需求后来变更（需要 CLI 手动控制或只在 `xz_listen detect` 时监听），再调整设计；当前先按照上述自动唤醒假设推进。

## 方案对比

1. **Idle 自动监听（推荐）**  
   - **描述：** `xiaozhi_app` 启动时激活唤醒线程、开始 PCM 捕获，只有在真正需要把数据发给服务器时（手动模式或唤醒命中）才允许 `xiaozhi_audio_linux_on_tx_opus_frame` 推送 Opus，命中后调用 `listen start`。检测线程在非 session 期间反复处理队列数据。  
   - **优点：** 用户体验最好，无需额外命令即可唤醒，逻辑与 Demo 中 Idle 一致；唤醒后直接走已验证的 listen 流程。  
   - **缺点：** 需要在编码后的通路里增加 streaming 过滤标记、管理检测线程与音频资源的共享，稍复杂。  

2. **CLI 手动控制唤醒线程**  
   - **描述：** 新增命令 `xz_wake start|stop|status`，唤醒线程仅在命令发出后才捕获 PCM，命中后调用 `listen start`。  
   - **优点：** 状态透明、与现有 `xz_listen` 命令保持一致；资源竞争更容易控制。  
   - **缺点：** 用户必须先执行命令，自动唤醒体验削弱，不符合 Idle 需求。  

3. **用 `xz_listen detect` 触发监听**  
   - **描述：** 保留现有 `listen detect` 命令，在服务器端发出 detect 指令后再启用 Snowboy。  
   - **优点：** 改动最小，仅复用已有命令。  
   - **缺点：** 无法做到本地自动唤醒，命令与 trigger 之前没有连接，触发链路仍是手动。  

推荐方案是方案1（Idle 自动监听），因为它直接复用了 Demo 中最成熟的模型与流程，同时提供了零交互的本地唤醒体验，后续若需要 CLI 微调可以在此基础上扩展。

## 推荐设计

### 架构总览

```
             +-------------------+
             |  Snowboy port     |
             |  (libsnowboy...)  |
             +-------------------+
                       ^
           +-----------+-----------+
           | Snowboy Wrapper (C)   |
           +-----------------------+
           | wakeword thread +     |
           | PCM queue             |
           +-----------+-----------+
                       |
             +---------v----------+
             | xiaozhi_app.c       | <-- detection callback
             | - 管理 listen/stop   |
             | - 判断 chat state    |
             +---------+----------+
                       |
           +-----------v-----------+
           | xiaozhi_audio_linux    |
           | - Opus 编码 -> 队列    |
           | - 推送到 MQTT/WS       |
           +-----------------------+
```

1. **Snowboy wrapper**：放在 `third_party/snowboy_wrapper`，包含 `snowboy-detect-c-wrapper.h/.cc`、`snowboy-detect.h`、`lib/ubuntu64/libsnowboy-detect.a`（复用 Demo 原始库），并在 CMake 中为这些文件单独指定 `-D_GLIBCXX_USE_CXX11_ABI=0`。（避免改动 Demo 里 PortAudio，直接链接预编译库）。  
2. **wakeword 模块**（`wakeword/*`）：  
   - 保持一个 PCM 队列与线程，由 `tdl_audio` 回调（或编码后的 Opus 解码）传入 `int16_t` 数据。  
   - 在线程中用 `SnowboyDetectRunDetection` 处理，每次命中通过回调通知 `xiaozhi_app`。  
   - 提供 `init`/`deinit`、`enable`/`disable`、`feed_pcm`、`is_enabled` 等 API。  
3. **`xiaozhi_app.c`**  
   - 新增 `wakeword` 状态字段、`streaming_active` 标志，初始化/启动时负责启动检测线程、监听回调。  
   - 检测到唤醒后：  
     1. 检查 `chat_state==XZ_CHAT_IDLE` 且当前未有监听，  
     2. 临时 `xiaozhi_wakeword_pause()`（避免重复触发）  
     3. 调用 `xiaozhi_app_send_listen("start","auto",NULL)`，  
     4. 结合 `xz_on_listen_stop_locked` 逻辑在会话结束后恢复检测。  
   - `xiaozhi_audio_linux_on_tx_opus_frame` 只在 `streaming_active` 为 `TRUE` 时把 Opus 推给队列；否则只是解码用于检测，并丢弃数据。  
4. **CLI 命令 (`cli_cmd.c`)**  
   - 增加 `xz_wake` 命令支持 `start|stop|status`，方便工程/调试时手动控制检测的启停与灵敏度（例如临时提高灵敏度）。  
5. **资源**  
   - 在 `apps/xiaozhi/src/resources/wakeword` 里复制 Demo 的 `common.res` 与 `models/echo.pmdl`。  
   - 编译时通过 `-DXZ_WAKEWORD_RESOURCE_DIR="${APP_PATH}/src/resources/wakeword"` 固定路径，运行时直接拼 `common.res`/`models/echo.pmdl`；也可通过环境变量覆盖。  

### 数据流与状态

1. **Idle 期间**  
   - `xiaozhi_app_start` 触发 `wakeword_init()`+`wakeword_enable(TRUE)`，调用 `xiaozhi_audio_linux_start_capture()` 确保 PCM 源源不断地到达。  
   - `wakeword_feed_pcm()` 将 `int16_t` 数据推入队列；检测线程每 10ms 处理 `SnowboyDetectRunDetection`。  
   - `streaming_active` 保持 `FALSE`，`xiaozhi_audio_linux_on_tx_opus_frame` 仅用于解码/供检测，未推网络。  
2. **唤醒命中**  
   - 检测线程通过回调调用 `xiaozhi_wakeword_triggered() -> wakeword_pause()`，然后在 `xiaozhi_app` 中调用 `xiaozhi_app_send_listen("start","auto",NULL)`。  
   - 所有唤醒后产生的 Opus 都会先进入编码队列（`streaming_active` 在 `xz_on_listen_start_locked` 中被置为 `TRUE`），开始网络上行。  
3. **会话结束**  
   - 服务器发 `listen stop`，`xz_on_listen_stop_locked` 把 `streaming_active` 置 `FALSE` 并重启检测线程（`wakeword_resume()`），准备下一次唤醒。  
4. **手动启动/停止**  
   - `xz_listen start manual` 直接走老流程，同时关闭/暂停检测（避免双重占麦）。  
   - `xz_listen stop` 之后如没有正在会话，检测恢复。

### 资源与构建

- **Snowboy 静态库**：把 `/home/share/samba/Demo4Echo/AIChat_demo/Client/third_party/snowboy/lib/ubuntu64/libsnowboy-detect.a` 拷贝到 `apps/xiaozhi/src/third_party/snowboy_wrapper/lib/ubuntu64/`；CMake 里新增 `target_link_libraries(${EXAMPLE_LIB} PRIVATE ${APP_PATH}/src/third_party/snowboy_wrapper/lib/ubuntu64/libsnowboy-detect.a)` 与 `blas/lapack`（通过 `find_package(BLAS REQUIRED)` 和 `find_package(LAPACK REQUIRED)`）。  
- **C 包装器**：在上述目录里放置 `snowboy-detect-c-wrapper.h/.cc`，再用 `target_sources` 编译 `.cc`，`target_include_directories` 指向头文件。添加 `target_compile_definitions(${EXAMPLE_LIB} PRIVATE _GLIBCXX_USE_CXX11_ABI=0)` 以匹配库编译时的 ABI。  
- **资源路径**：CMake `target_compile_definitions` 增加 `XZ_WAKEWORD_RESOURCE_DIR="${APP_PATH}/src/resources/wakeword"`，代码里可通过 `#ifdef XZ_WAKEWORD_RESOURCE_DIR` 拼接绝对路径。运行时也支持 `XZ_WAKEWORD_RESOURCE_DIR` 环境变量提前设置。  
- **其他依赖**：继续链接现有 `opus`，无需额外 PortAudio。  

### CLI 调整

- `xz_wake status`：打印当前唤醒状态（`enabled/disabled/triggering`）、灵敏度、是否正在监听、最近一次唤醒时间。  
- `xz_wake start` & `xz_wake stop`：用于调试时开/关自动监听，内部调用 `xiaozhi_wakeword_enable(TRUE/FALSE)`。  
- `xz_wake sens <value>`（可选）：调整 `SnowboyDetectSetSensitivity` 的值，加大/降低误触概率。  
- 这些命令与 `xz_listen` 共存互补，方便保持自动唤醒的同时也能手动控制。  

### 错误与恢复

- Snowboy 初始化失败（缺资源/模型/ABI） → 日志提示并将检测模块置为 disabled；之后 `xz_wake status` 报告失败原因，仍保留手动 listen 能力。  
- Opus 编码/解码错误 → 严重则 `xiaozhi_audio_linux_reset()` 复位后重新初始化检测线程。  
- 检测命中后 `xiaozhi_app_send_listen` 返回非 `OPRT_OK` → 再次恢复检测并记录失败码，避免死锁。  

### 验证计划

1. `cd /workspace && . ./export.sh && tos.py check`，确认基础依赖无误（如 Opus、BLAS/LAPACK、Snowboy 资源）。  
2. `xiaozhi_app_start` 后观察日志，确认 `SnowboyDetectConstructor` 成功且线程进入 `Waiting for PCM`。  
3. 在 `Idle` 期间说出 “Echo” 模型唤醒词，验证：  
   - Detection thread log → `xiaozhi_app_send_listen("start", "auto", NULL)` 执行；  
   - `streaming_active` 置 `TRUE` 且 Opus 数据开始走队列（`xz_drain_uplink_audio_queue_locked`）。  
4. `xz_listen stop` 或 `listen` session 结束后，确认检测再次 resume。  
5. `xz_wake status/start/stop` 命令按预期工作。  
6. 编译 `apps/xiaozhi`，运行 `./dist/...` 手动测试 `xz_listen start manual`，确保新检测逻辑不影响旧流程。  

### 风险与缓解

- **Snowboy 静态库与当前 ABI 不匹配** → 通过 `_GLIBCXX_USE_CXX11_ABI=0` 编译定义并尽早尝试链接；若仍失败，可考虑重新编译 wrapper 或引入官方源。  
- **资源路径在 `dist`/运行时找不到** → 代码里默认使用 `XZ_WAKEWORD_RESOURCE_DIR` 宏，优先从环境变量读取；若路径缺失提示日志并关闭检测。  
- **检测线程与音频会话抢占麦克风** → `streaming_active` 变量保证只有真正的 `listen start` 才开始发给服务器，检测线程在会话期间通过 `wakeword_pause()` 暂停。  
- **CPU 加载或误触** → CLI `xz_wake sens` 暴露灵敏度，必要时在 `wakeword` 模块做滑动平均/最短时间窗口限制；检测命中后立即暂停避免重复触发。  

### 后续

- 本设计完成后再根据用户反馈决定是否把检测状态暴露给 `xiaozhi_mcp` 或 UI 层。  
- 若未来需要支持多个模型，只需在 `wakeword` 模块中把 `model_str` 扩展为 `model1.pmdl,model2.pmdl` 并在 CLI 里新增选择逻辑。
