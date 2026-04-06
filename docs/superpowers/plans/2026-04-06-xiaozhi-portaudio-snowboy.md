# xiaozhi PortAudio + Snowboy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `apps/xiaozhi` 中获取 OTA 激活验证码，并把 Linux 语音链路从 `alsa_audio/tdl_audio` 切换到 `PortAudio + Opus`，同时新增 `Snowboy` 自动唤醒且保留手动听写模式。

**Architecture:** 保持 `xiaozhi_app.c`、`xiaozhi_ws.c` 的协议与状态机主路径不变，只替换 `xiaozhi_audio_linux.*` 背后的 Linux 音频实现。新增 `audio/`、`wakeword/` 和 `third_party/snowboy_wrapper/` 子目录，使用 PortAudio 统一输入输出设备，用 Snowboy 后台线程在空闲态触发与 `manual` 模式相同的 listen 路径。

**Tech Stack:** C、C++、CMake、PortAudio、Opus、Snowboy、TuyaOpen `tos.py`

---

### 文件结构

**Create:**
- `apps/xiaozhi/src/audio/xz_portaudio_device.h`
- `apps/xiaozhi/src/audio/xz_portaudio_device.c`
- `apps/xiaozhi/src/audio/xz_audio_opus_bridge.h`
- `apps/xiaozhi/src/audio/xz_audio_opus_bridge.c`
- `apps/xiaozhi/src/wakeword/xz_snowboy_runner.h`
- `apps/xiaozhi/src/wakeword/xz_snowboy_runner.cc`
- `apps/xiaozhi/src/third_party/snowboy_wrapper/snowboy-detect-c-wrapper.h`
- `apps/xiaozhi/src/third_party/snowboy_wrapper/snowboy-detect-c-wrapper.cc`
- `apps/xiaozhi/src/resources/common.res`
- `apps/xiaozhi/src/resources/models/echo.pmdl`

**Modify:**
- `apps/xiaozhi/CMakeLists.txt`
- `apps/xiaozhi/src/xiaozhi_audio_linux.h`
- `apps/xiaozhi/src/xiaozhi_audio_linux.c`
- `apps/xiaozhi/src/xiaozhi_app.c`
- `apps/xiaozhi/src/cli_cmd.c`
- `apps/xiaozhi/README_CN.md`
- `apps/xiaozhi/README.md`

**Inspect while implementing:**
- `apps/xiaozhi/src/xiaozhi_state.h`
- `apps/xiaozhi/src/xiaozhi_state.c`
- `apps/xiaozhi/src/xiaozhi_ws.c`
- `/home/share/samba/Demo4Echo/AIChat_demo/Client/Audio/AudioProcess.cc`
- `/home/share/samba/Demo4Echo/AIChat_demo/Client/Application/UserStates/Idle.cc`
- `/home/share/samba/Demo4Echo/AIChat_demo/Client/third_party/snowboy/CMakeLists.txt`

**Test / Verify:**
- `apps/xiaozhi/tests/test_xiaozhi_audio_linux.c`
- `apps/xiaozhi/tests/test_xiaozhi_state.c`

### Task 1: 环境检查并获取激活验证码

**Files:**
- Modify: `apps/xiaozhi/README_CN.md`
- Modify: `apps/xiaozhi/README.md`

- [ ] **Step 1: 初始化构建环境**

Run: `cd /home/share/samba/tuyaopen && . ./export.sh`
Expected: shell 导出 `OPEN_SDK_ROOT`、`OPEN_SDK_PYTHON`、`OPEN_SDK_PIP`，并可执行 `tos.py`

- [ ] **Step 2: 检查 Linux 构建依赖**

Run: `cd /home/share/samba/tuyaopen && . ./export.sh && pkg-config --modversion portaudio-2.0 && python3 -V && cd apps/xiaozhi && tos.py check`
Expected: 输出 PortAudio 版本、Python 版本，并且 `tos.py check` 通过；若 PortAudio 缺失，记录为明确阻塞

- [ ] **Step 3: 执行 OTA check 获取验证码**

Run: `cd /home/share/samba/tuyaopen && . ./export.sh && mkdir -p .cache && touch .cache/.dont_prompt_update_platform && cd apps/xiaozhi && python3 ../tools/port/linux/run_app.py 2>/dev/null`
Fallback Run: `cd /home/share/samba/tuyaopen && . ./export.sh && cd apps/xiaozhi && tos.py build`
Expected: 通过运行日志或构建后应用日志看到 `activation code=`；若没有，检查是否返回 `activation.challenge`

- [ ] **Step 4: 记录激活验证手册**

在 `apps/xiaozhi/README_CN.md` 和 `apps/xiaozhi/README.md` 增加“获取激活码 / 绑定服务器”小节，写明：

```text
1. . ./export.sh
2. cd apps/xiaozhi
3. mkdir -p ../../.cache && touch ../../.cache/.dont_prompt_update_platform
4. tos.py build
5. 运行 dist/xiaozhi_1.0.0/xiaozhi_1.0.0.elf
6. 查看日志中的 activation code=xxxx
7. 在服务器侧完成绑定后再进行 xz_reconnect / 语音验证
```

- [ ] **Step 5: 运行格式检查**

Run: `cd /home/share/samba/tuyaopen && python tools/check_format.py --debug --files apps/xiaozhi/README_CN.md`
Expected: 无格式错误

### Task 2: 建立 PortAudio 设备封装和 Opus 桥接

**Files:**
- Create: `apps/xiaozhi/src/audio/xz_portaudio_device.h`
- Create: `apps/xiaozhi/src/audio/xz_portaudio_device.c`
- Create: `apps/xiaozhi/src/audio/xz_audio_opus_bridge.h`
- Create: `apps/xiaozhi/src/audio/xz_audio_opus_bridge.c`
- Modify: `apps/xiaozhi/tests/test_xiaozhi_audio_linux.c`

- [ ] **Step 1: 写 PortAudio 设备层失败测试**

在 `apps/xiaozhi/tests/test_xiaozhi_audio_linux.c` 新增一个仅测试 PCM 累积与门面转发的用例，避免真实设备依赖：

```c
static void test_pcm_accum_push_emits_complete_20ms_frame(void)
{
    xz_audio_pcm_accum_t accum = {0};
    uint8_t pcm[640] = {0};

    assert_int_equal(xz_audio_pcm_accum_push(&accum, pcm, sizeof(pcm)), 640);
    assert_int_equal(accum.tail, 0);
}
```

- [ ] **Step 2: 运行测试确认基线仍然失败或缺少新接口**

Run: `cd /home/share/samba/tuyaopen && . ./export.sh && cd apps/xiaozhi && ctest --test-dir build --output-on-failure -R xiaozhi_audio_linux`
Expected: 若当前尚未配置 `build/`，记录需要先完成 `tos.py build`；若已配置，测试应因新接口缺失或未接线而失败

- [ ] **Step 3: 创建 PortAudio 设备头文件**

在 `apps/xiaozhi/src/audio/xz_portaudio_device.h` 写入：

```c
#ifndef XZ_PORTAUDIO_DEVICE_H
#define XZ_PORTAUDIO_DEVICE_H

#include <stddef.h>
#include <stdint.h>

typedef struct xz_pa_device xz_pa_device_t;

typedef struct {
    int sample_rate;
    int channels;
    int frames_per_buffer;
} xz_pa_config_t;

int xz_pa_init(void);
int xz_pa_deinit(void);
int xz_pa_capture_open(xz_pa_device_t **handle, const xz_pa_config_t *config);
int xz_pa_capture_read(xz_pa_device_t *handle, int16_t *pcm, size_t frames);
int xz_pa_capture_close(xz_pa_device_t *handle);
int xz_pa_playback_open(xz_pa_device_t **handle, const xz_pa_config_t *config);
int xz_pa_playback_write(xz_pa_device_t *handle, const int16_t *pcm, size_t frames);
int xz_pa_playback_close(xz_pa_device_t *handle);

#endif
```

- [ ] **Step 4: 写最小 PortAudio 实现**

在 `apps/xiaozhi/src/audio/xz_portaudio_device.c` 实现 `Pa_Initialize`、默认输入输出设备打开、阻塞读写与关闭逻辑，统一参数使用：

```c
#define XZ_PA_SAMPLE_RATE 16000
#define XZ_PA_CHANNELS 1
#define XZ_PA_FRAMES_PER_BUFFER 320
```

错误返回统一为 0 成功、负数失败，并把 `Pa_GetErrorText(err)` 打到日志。

- [ ] **Step 5: 创建 Opus 桥接层**

在 `apps/xiaozhi/src/audio/xz_audio_opus_bridge.h` 和 `.c` 中实现：

```c
typedef void (*xz_opus_frame_cb_t)(const uint8_t *data, size_t len, void *userdata);

typedef struct {
    xz_audio_pcm_accum_t accum;
    OpusEncoder *enc;
    OpusDecoder *dec;
    int16_t decode_pcm[1920];
} xz_audio_opus_bridge_t;

int xz_audio_opus_bridge_init(xz_audio_opus_bridge_t *bridge);
void xz_audio_opus_bridge_deinit(xz_audio_opus_bridge_t *bridge);
int xz_audio_opus_bridge_encode_pcm(xz_audio_opus_bridge_t *bridge, const uint8_t *pcm, size_t bytes,
                                    xz_opus_frame_cb_t cb, void *userdata);
int xz_audio_opus_bridge_decode_opus(xz_audio_opus_bridge_t *bridge, const void *opus, size_t len,
                                     int16_t **pcm, size_t *frames);
void xz_audio_opus_bridge_reset(xz_audio_opus_bridge_t *bridge);
```

- [ ] **Step 6: 运行最小相关测试**

Run: `cd /home/share/samba/tuyaopen && . ./export.sh && cd apps/xiaozhi && tos.py build`
Expected: 目前可能仍失败，但失败点应已推进到 `xiaozhi_audio_linux` 尚未完成接入，而不是头文件或符号缺失

### Task 3: 用 PortAudio 重写 Linux 音频门面

**Files:**
- Modify: `apps/xiaozhi/src/xiaozhi_audio_linux.h`
- Modify: `apps/xiaozhi/src/xiaozhi_audio_linux.c`
- Modify: `apps/xiaozhi/src/tuya_main.c`

- [ ] **Step 1: 写音频门面接口失败测试**

在 `apps/xiaozhi/tests/test_xiaozhi_audio_linux.c` 增加：

```c
static void test_feed_opus_rejects_empty_packet(void)
{
    assert_int_not_equal(xiaozhi_audio_linux_feed_opus(NULL, 0), 0);
}
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cd /home/share/samba/tuyaopen && . ./export.sh && cd apps/xiaozhi && tos.py build`
Expected: 当前实现仍绑定 `tdl_audio`，后续切换后重新验证

- [ ] **Step 3: 修改 `xiaozhi_audio_linux.c` 的上下文结构**

把当前依赖 `TDL_AUDIO_HANDLE_T` 的上下文替换为：

```c
typedef struct {
    BOOL_T inited;
    BOOL_T capture_enabled;
    BOOL_T playback_opened;
    THREAD_HANDLE capture_thread;
    MUTEX_HANDLE lock;
    xz_pa_device_t *capture_dev;
    xz_pa_device_t *playback_dev;
    xz_audio_opus_bridge_t bridge;
    xz_audio_opus_tx_cb_t tx_cb;
    void *tx_userdata;
} xz_audio_linux_ctx_t;
```

- [ ] **Step 4: 实现 PortAudio 采集线程**

在 `xiaozhi_audio_linux.c` 中新增线程函数：

```c
static void xz_audio_capture_thread(void *arg)
{
    int16_t pcm[320] = {0};
    while (s_audio.capture_enabled) {
        if (xz_pa_capture_read(s_audio.capture_dev, pcm, 320) != 0) {
            PR_ERR("portaudio capture read failed");
            break;
        }
        (void)xz_audio_opus_bridge_encode_pcm(&s_audio.bridge, (const uint8_t *)pcm, sizeof(pcm),
                                              xz_audio_emit_opus_frame, NULL);
    }
}
```

- [ ] **Step 5: 重写初始化与播放路径**

把 `xiaozhi_audio_linux_init()` 改为：

```c
int xiaozhi_audio_linux_init(void)
{
    xz_pa_config_t cfg = {.sample_rate = 16000, .channels = 1, .frames_per_buffer = 320};
    if (xz_pa_init() != 0) return OPRT_COM_ERROR;
    if (xz_pa_playback_open(&s_audio.playback_dev, &cfg) != 0) return OPRT_COM_ERROR;
    if (xz_audio_opus_bridge_init(&s_audio.bridge) != 0) return OPRT_COM_ERROR;
    s_audio.tx_cb = xiaozhi_audio_linux_on_tx_opus_frame;
    s_audio.inited = TRUE;
    return OPRT_OK;
}
```

把 `xiaozhi_audio_linux_feed_opus()` 改为先解码，再调用 `xz_pa_playback_write()`。

- [ ] **Step 6: 移除 ALSA fallback 注册依赖**

在 `apps/xiaozhi/src/tuya_main.c` 中删除或 `#if 0` 掉仅为 Linux 注册 `alsa_audio` 的 fallback 逻辑，避免构建继续要求旧音频后端。

- [ ] **Step 7: 运行构建验证**

Run: `cd /home/share/samba/tuyaopen && . ./export.sh && cd apps/xiaozhi && tos.py build`
Expected: 编译通过或只剩 Snowboy/C++ 混编相关错误

### Task 4: 接入 Snowboy 后台唤醒

**Files:**
- Create: `apps/xiaozhi/src/wakeword/xz_snowboy_runner.h`
- Create: `apps/xiaozhi/src/wakeword/xz_snowboy_runner.cc`
- Create: `apps/xiaozhi/src/third_party/snowboy_wrapper/snowboy-detect-c-wrapper.h`
- Create: `apps/xiaozhi/src/third_party/snowboy_wrapper/snowboy-detect-c-wrapper.cc`
- Modify: `apps/xiaozhi/src/xiaozhi_audio_linux.h`
- Modify: `apps/xiaozhi/src/xiaozhi_audio_linux.c`
- Modify: `apps/xiaozhi/src/xiaozhi_app.c`
- Modify: `apps/xiaozhi/src/cli_cmd.c`

- [ ] **Step 1: 从 `Demo4Echo` 复制最小 Snowboy wrapper**

从以下来源复制最小实现，不带 demo：

```text
/home/share/samba/Demo4Echo/AIChat_demo/Client/third_party/snowboy/include/snowboy-detect-c-wrapper.h
/home/share/samba/Demo4Echo/AIChat_demo/Client/third_party/snowboy/snowboy-detect-c-wrapper.cc
```

复制后把 include 路径调整为 `apps/xiaozhi/src/third_party/snowboy_wrapper/...` 可独立编译。

- [ ] **Step 2: 创建 Snowboy runner 头文件**

在 `apps/xiaozhi/src/wakeword/xz_snowboy_runner.h` 写入：

```c
#ifndef XZ_SNOWBOY_RUNNER_H
#define XZ_SNOWBOY_RUNNER_H

#include <stddef.h>
#include <stdint.h>

typedef void (*xz_snowboy_detected_cb_t)(void *userdata);

typedef struct xz_snowboy_runner xz_snowboy_runner_t;

int xz_snowboy_runner_create(xz_snowboy_runner_t **runner, const char *resource_path, const char *model_path,
                             xz_snowboy_detected_cb_t cb, void *userdata);
int xz_snowboy_runner_start(xz_snowboy_runner_t *runner);
int xz_snowboy_runner_feed_pcm(xz_snowboy_runner_t *runner, const int16_t *pcm, size_t samples);
int xz_snowboy_runner_reset(xz_snowboy_runner_t *runner);
int xz_snowboy_runner_stop(xz_snowboy_runner_t *runner);
void xz_snowboy_runner_destroy(xz_snowboy_runner_t *runner);

#endif
```

- [ ] **Step 3: 实现 C++ runner**

在 `.cc` 里用 wrapper 创建 detector，设定单热词灵敏度，`feed_pcm()` 内部调用 `SnowboyDetectRunDetectionShort()`，命中后回调。

- [ ] **Step 4: 在音频门面里接入 detect 线程**

在 `xiaozhi_audio_linux.c` 中新增：

```c
int xiaozhi_audio_linux_start_detect(void);
int xiaozhi_audio_linux_stop_detect(void);
```

detect 线程逻辑：

```c
while (detect_enabled) {
    if (xz_pa_capture_read(capture_dev, pcm, 320) != 0) break;
    if (xz_snowboy_runner_feed_pcm(runner, pcm, 320) > 0) {
        detect_enabled = FALSE;
        xz_audio_notify_hotword();
    }
}
```

- [ ] **Step 5: 把热词命中接到现有 listen 路径**

在 `apps/xiaozhi/src/xiaozhi_app.c` 中新增一个仅 Linux 使用的内部入口，例如：

```c
OPERATE_RET xiaozhi_app_start_detected_listen(void)
{
    return xz_start_listen_with_mode(XZ_LISTEN_MODE_AUTO, NULL);
}
```

由热词命中回调调用它，复用现有发送 `listen` 的逻辑，不新造协议。

- [ ] **Step 6: 调整 CLI**

在 `apps/xiaozhi/src/cli_cmd.c` 中把 `xz_listen detect` 接到 `xiaozhi_audio_linux_start_detect()` 或统一应用层封装。

- [ ] **Step 7: 运行构建验证**

Run: `cd /home/share/samba/tuyaopen && . ./export.sh && cd apps/xiaozhi && tos.py build`
Expected: C/C++ 混编和 Snowboy 链接通过

### Task 5: 资源路径、CMake 与文档收尾

**Files:**
- Modify: `apps/xiaozhi/CMakeLists.txt`
- Modify: `apps/xiaozhi/README_CN.md`
- Modify: `apps/xiaozhi/README.md`
- Create: `apps/xiaozhi/src/resources/common.res`
- Create: `apps/xiaozhi/src/resources/models/echo.pmdl`

- [ ] **Step 1: 修改 CMake 接入 C++、PortAudio 和 Snowboy**

在 `apps/xiaozhi/CMakeLists.txt` 增加：

```cmake
enable_language(CXX)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

list(APPEND APP_SRCS
    ${APP_PATH}/src/audio/xz_portaudio_device.c
    ${APP_PATH}/src/audio/xz_audio_opus_bridge.c
    ${APP_PATH}/src/wakeword/xz_snowboy_runner.cc
    ${APP_PATH}/src/third_party/snowboy_wrapper/snowboy-detect-c-wrapper.cc
)

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
        ${PORTAUDIO_INCLUDE_DIRS}
        ${APP_PATH}/src/third_party/snowboy_wrapper
)

target_link_libraries(${EXAMPLE_LIB} PRIVATE ${PORTAUDIO_LIBRARIES})
```

并按 Ubuntu64 目标链接 `Demo4Echo` 里的 Snowboy 静态库。

- [ ] **Step 2: 拷贝资源文件**

复制：

```text
/home/share/samba/Demo4Echo/AIChat_demo/Client/third_party/snowboy/resources/common.res
/home/share/samba/Demo4Echo/AIChat_demo/Client/third_party/snowboy/resources/models/echo.pmdl
```

到 `apps/xiaozhi/src/resources/...`

- [ ] **Step 3: 在文档中补充运行示例**

在 `README_CN.md` / `README.md` 增加：

```text
XZ_SNOWBOY_RESOURCE=/abs/path/common.res
XZ_SNOWBOY_MODEL=/abs/path/echo.pmdl
XZ_PROTOCOL=websocket
./dist/xiaozhi_1.0.0/xiaozhi_1.0.0.elf
```

并写明：

- `xz_listen start manual` 用于手动模式
- `xz_listen detect` 用于热词待机模式

- [ ] **Step 4: 运行格式检查**

Run: `cd /home/share/samba/tuyaopen && python tools/check_format.py --debug --dir apps/xiaozhi/src`
Expected: 无格式错误或只报告第三方复制代码需要最小格式调整

### Task 6: 最终验证

**Files:**
- Verify only

- [ ] **Step 1: 全量构建**

Run: `cd /home/share/samba/tuyaopen && . ./export.sh && cd apps/xiaozhi && tos.py build`
Expected: exit 0，生成 `apps/xiaozhi/dist/xiaozhi_1.0.0/`

- [ ] **Step 2: 获取并确认验证码**

Run: `cd /home/share/samba/tuyaopen/apps/xiaozhi && ./dist/xiaozhi_1.0.0/xiaozhi_1.0.0.elf`
Expected: 日志包含 `activation code=...` 或显示已激活；把验证码提供给用户绑定服务器

- [ ] **Step 3: 手动模式验证**

Run:

```text
xz_status
xz_listen start manual
```

Expected: 讲话时产生上行音频，服务端回 `tts start` / `binary` / `tts stop`，本地可听到下行语音

- [ ] **Step 4: 热词模式验证**

Run:

```text
xz_listen detect
```

Expected: 待机时监听热词，命中后自动进入 listen，会话结束后恢复 detect

- [ ] **Step 5: 记录无法自动化的残余风险**

在最终汇报中明确：

- 是否实际拿到验证码
- 是否完成服务器绑定后的真实语音验证
- 如果缺少真实音频设备或服务器环境，哪些步骤只能人工补测
