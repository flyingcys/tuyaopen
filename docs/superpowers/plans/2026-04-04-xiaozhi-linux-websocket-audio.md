# xiaozhi Linux WebSocket 语音链路实现计划

> **给代理执行者：** 必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 按任务逐项执行。本计划使用复选框 `- [ ]` 追踪步骤。

**目标：** 为 `apps/xiaozhi` 打通 Linux/Ubuntu 下基于 ALSA 的 WebSocket 语音上传与下发链路，在不接入 VAD、唤醒词和 `ai_audio_input/ai_audio_player` 的前提下，实现手动 `listen start/stop` 录音上传与 TTS binary 播放。

**架构：** 新增独立的 Linux 音频运行时模块，负责 `ALSA PCM <-> Opus <-> 应用回调`。`xiaozhi_ws` 仅负责 WebSocket binary 收发与回调分发，`xiaozhi_app` 负责状态机和音频运行时的桥接，尽量保持与 `xiaozhi-esp32` WebSocket 语义一致。

**技术栈：** C、TuyaOpen `tdl_audio_manage` / `tdd_audio_alsa`、WebSocket、Opus、Linux ALSA、`cc` 单文件测试、`tos.py build`

---

## 文件结构

- 新建：`apps/xiaozhi/src/xiaozhi_audio_linux.h`
  - Linux 音频运行时对外接口定义。
- 新建：`apps/xiaozhi/src/xiaozhi_audio_linux.c`
  - ALSA 设备注册、PCM 聚合、Opus 编解码、播放与采集控制。
- 新建：`apps/xiaozhi/tests/test_xiaozhi_audio_linux.c`
  - 纯聚合逻辑与状态辅助逻辑单元测试。
- 新建：`apps/xiaozhi/src/xiaozhi_state.h`
  - `idle/listening/speaking` 纯状态转换辅助接口。
- 新建：`apps/xiaozhi/src/xiaozhi_state.c`
  - 可独立测试的状态转换逻辑。
- 新建：`apps/xiaozhi/tests/test_xiaozhi_state.c`
  - 应用状态转换单元测试。
- 新建：`apps/xiaozhi/tests/test_xiaozhi_ws_binary.c`
  - WebSocket binary 回调分发与发送接口最小测试。
- 修改：`apps/xiaozhi/src/xiaozhi_ws.h`
  - 增加 binary 回调声明与 `xz_ws_send_audio()` 声明。
- 修改：`apps/xiaozhi/src/xiaozhi_ws.c`
  - 增加 WebSocket binary 收发桥接。
- 修改：`apps/xiaozhi/src/xiaozhi_app.c`
  - 初始化音频运行时，接入 `listen/tts/abort` 状态切换与音频桥接。
- 修改：`apps/xiaozhi/src/tuya_main.c`
  - Linux 平台初始化时注册板级 ALSA 硬件。
- 修改：`apps/xiaozhi/CMakeLists.txt`
  - 编入新增音频源文件。
- 修改：`apps/xiaozhi/config/Linux.config`
  - 开启 ALSA 音频能力与默认音频设备配置。
- 修改：`apps/xiaozhi/app_default.config`
  - 补充 Linux 构建时需要的默认配置项。
- 修改：`apps/xiaozhi/README_CN.md`
  - 增加 Linux 音频配置、运行和验证说明。
- 修改：`apps/xiaozhi/README.md`
  - 同步英文使用说明。

## 任务 1：补齐 WebSocket binary 接口与最小测试

**文件：**
- 新建：`apps/xiaozhi/tests/test_xiaozhi_ws_binary.c`
- 修改：`apps/xiaozhi/src/xiaozhi_ws.h`
- 修改：`apps/xiaozhi/src/xiaozhi_ws.c`
- 测试：`apps/xiaozhi/tests/test_xiaozhi_ws_binary.c`

- [ ] **步骤 1：先写失败测试，定义 binary 回调接口预期**

```c
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "xiaozhi_ws.h"

static int s_binary_called = 0;
static size_t s_last_len = 0;

static void on_binary(void *userdata, const uint8_t *payload, size_t len)
{
    (void)userdata;
    s_binary_called++;
    s_last_len = len;
    assert(payload != NULL);
}

int main(void)
{
    xz_ws_client_t ws;
    memset(&ws, 0, sizeof(ws));

    assert(xz_ws_set_binary_message_callback(&ws, on_binary, NULL) == OPRT_OK);
    assert(ws.on_binary_message == on_binary);

    puts("test_xiaozhi_ws_binary: PASS");
    return 0;
}
```

- [ ] **步骤 2：运行测试，确认它因缺少接口而失败**

运行：

```bash
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_ws_binary.c -o /tmp/test_xz_ws_binary
```

预期：

- 编译失败，提示 `xz_ws_set_binary_message_callback` 或 `on_binary_message` 尚不存在。

- [ ] **步骤 3：在头文件补充 binary 回调类型和发送接口声明**

修改 `[xiaozhi_ws.h](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_ws.h)`：

```c
typedef void (*xz_binary_message_cb_t)(void *userdata, const uint8_t *payload, size_t payload_len);

typedef struct {
    xz_text_message_cb_t on_text_message;
    void *on_text_userdata;
    xz_binary_message_cb_t on_binary_message;
    void *on_binary_userdata;
    xz_channel_t channel;
    xz_conn_state_t state;
} xz_ws_client_t;

OPERATE_RET xz_ws_send_audio(xz_ws_client_t *ws, const uint8_t *payload, size_t payload_len);
OPERATE_RET xz_ws_set_binary_message_callback(xz_ws_client_t *ws, xz_binary_message_cb_t cb, void *userdata);
```

- [ ] **步骤 4：在实现文件增加最小可用实现**

修改 `[xiaozhi_ws.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_ws.c)`：

```c
OPERATE_RET xz_ws_send_audio(xz_ws_client_t *ws, const uint8_t *payload, size_t payload_len)
{
    if (!ws || !payload || payload_len == 0) {
        return OPRT_INVALID_PARM;
    }

    return xz_ws_send_frame(ws, 0x2, payload, payload_len);
}

OPERATE_RET xz_ws_set_binary_message_callback(xz_ws_client_t *ws, xz_binary_message_cb_t cb, void *userdata)
{
    if (!ws) {
        return OPRT_INVALID_PARM;
    }

    ws->on_binary_message = cb;
    ws->on_binary_userdata = userdata;
    return OPRT_OK;
}
```

- [ ] **步骤 5：让 `xz_ws_poll()` 在收到 binary 帧时走回调，而不是只打日志**

修改 `[xiaozhi_ws.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_ws.c)` 中 `opcode == 0x2` 分支：

```c
    } else if (opcode == 0x2) {
        if (ws->on_binary_message) {
            ws->on_binary_message(ws->on_binary_userdata, payload, payload_len);
        } else {
            PR_DEBUG("websocket binary len=%u", (unsigned)payload_len);
        }
```

- [ ] **步骤 6：重新运行测试，确认通过**

运行：

```bash
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_ws_binary.c -o /tmp/test_xz_ws_binary && /tmp/test_xz_ws_binary
```

预期：

- 输出 `test_xiaozhi_ws_binary: PASS`

- [ ] **步骤 7：提交本任务**

```bash
git add apps/xiaozhi/src/xiaozhi_ws.h apps/xiaozhi/src/xiaozhi_ws.c apps/xiaozhi/tests/test_xiaozhi_ws_binary.c
git commit -m "feat: add websocket binary audio hooks"
```

## 任务 2：新增 Linux 音频运行时与 PCM 聚合测试

**文件：**
- 新建：`apps/xiaozhi/src/xiaozhi_audio_linux.h`
- 新建：`apps/xiaozhi/src/xiaozhi_audio_linux.c`
- 新建：`apps/xiaozhi/tests/test_xiaozhi_audio_linux.c`
- 测试：`apps/xiaozhi/tests/test_xiaozhi_audio_linux.c`

- [ ] **步骤 1：先写失败测试，锁定 PCM 聚合规则**

```c
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "xiaozhi_audio_linux.h"

int main(void)
{
    xz_audio_pcm_accum_t accum;
    uint8_t frame[640];
    uint8_t chunk1[160] = {0};
    uint8_t chunk2[480] = {0};

    xz_audio_pcm_accum_reset(&accum);
    assert(xz_audio_pcm_accum_push(&accum, chunk1, sizeof(chunk1), frame, sizeof(frame)) == 0);
    assert(xz_audio_pcm_accum_push(&accum, chunk2, sizeof(chunk2), frame, sizeof(frame)) == 640);

    puts("test_xiaozhi_audio_linux: PASS");
    return 0;
}
```

- [ ] **步骤 2：运行测试，确认因缺少聚合结构和接口而失败**

运行：

```bash
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_audio_linux.c -o /tmp/test_xz_audio_linux
```

预期：

- 编译失败，提示 `xz_audio_pcm_accum_t` 或相关函数不存在。

- [ ] **步骤 3：在头文件定义最小的 Linux 音频运行时接口**

新建 `[xiaozhi_audio_linux.h](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_audio_linux.h)`：

```c
#ifndef __XIAOZHI_AUDIO_LINUX_H__
#define __XIAOZHI_AUDIO_LINUX_H__

#include "tuya_cloud_types.h"

#include <stddef.h>
#include <stdint.h>

#define XZ_AUDIO_PCM_FRAME_BYTES 640

typedef void (*xz_audio_opus_tx_cb_t)(void *userdata, const uint8_t *data, size_t len);

typedef struct {
    uint8_t buf[XZ_AUDIO_PCM_FRAME_BYTES];
    size_t used;
} xz_audio_pcm_accum_t;

void xz_audio_pcm_accum_reset(xz_audio_pcm_accum_t *accum);
size_t xz_audio_pcm_accum_push(xz_audio_pcm_accum_t *accum, const uint8_t *data, size_t len, uint8_t *out_frame,
                               size_t out_size);

OPERATE_RET xiaozhi_audio_linux_init(xz_audio_opus_tx_cb_t cb, void *userdata);
OPERATE_RET xiaozhi_audio_linux_deinit(void);
OPERATE_RET xiaozhi_audio_linux_start_capture(void);
OPERATE_RET xiaozhi_audio_linux_stop_capture(void);
OPERATE_RET xiaozhi_audio_linux_feed_opus(const uint8_t *data, size_t len);
OPERATE_RET xiaozhi_audio_linux_abort_playback(void);
OPERATE_RET xiaozhi_audio_linux_reset(void);

#endif
```

- [ ] **步骤 4：在实现文件先只完成纯聚合逻辑和空壳接口**

新建 `[xiaozhi_audio_linux.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_audio_linux.c)`：

```c
#include "xiaozhi_audio_linux.h"

#include <string.h>

void xz_audio_pcm_accum_reset(xz_audio_pcm_accum_t *accum)
{
    if (!accum) {
        return;
    }
    memset(accum, 0, sizeof(*accum));
}

size_t xz_audio_pcm_accum_push(xz_audio_pcm_accum_t *accum, const uint8_t *data, size_t len, uint8_t *out_frame,
                               size_t out_size)
{
    size_t need = XZ_AUDIO_PCM_FRAME_BYTES - accum->used;
    size_t copy = (len < need) ? len : need;

    if (!accum || !data || !out_frame || out_size < XZ_AUDIO_PCM_FRAME_BYTES) {
        return 0;
    }

    memcpy(accum->buf + accum->used, data, copy);
    accum->used += copy;
    if (accum->used < XZ_AUDIO_PCM_FRAME_BYTES) {
        return 0;
    }

    memcpy(out_frame, accum->buf, XZ_AUDIO_PCM_FRAME_BYTES);
    accum->used = 0;
    return XZ_AUDIO_PCM_FRAME_BYTES;
}

OPERATE_RET xiaozhi_audio_linux_init(xz_audio_opus_tx_cb_t cb, void *userdata) { (void)cb; (void)userdata; return OPRT_OK; }
OPERATE_RET xiaozhi_audio_linux_deinit(void) { return OPRT_OK; }
OPERATE_RET xiaozhi_audio_linux_start_capture(void) { return OPRT_OK; }
OPERATE_RET xiaozhi_audio_linux_stop_capture(void) { return OPRT_OK; }
OPERATE_RET xiaozhi_audio_linux_feed_opus(const uint8_t *data, size_t len) { (void)data; (void)len; return OPRT_OK; }
OPERATE_RET xiaozhi_audio_linux_abort_playback(void) { return OPRT_OK; }
OPERATE_RET xiaozhi_audio_linux_reset(void) { return OPRT_OK; }
```

同时在实现文件开头加一个测试裁剪宏，确保纯测试不会被 ALSA/Opus 链接拖住：

```c
#ifdef XZ_AUDIO_LINUX_TEST_ONLY
/* 仅编译 PCM 聚合等纯辅助逻辑 */
#else
/* 编译 ALSA / Opus / TDL 真正运行时代码 */
#endif
```

- [ ] **步骤 5：运行测试，确认聚合逻辑通过**

运行：

```bash
cc -DXZ_AUDIO_LINUX_TEST_ONLY -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_audio_linux.c apps/xiaozhi/src/xiaozhi_audio_linux.c -o /tmp/test_xz_audio_linux && /tmp/test_xz_audio_linux
```

预期：

- 输出 `test_xiaozhi_audio_linux: PASS`

- [ ] **步骤 6：提交本任务**

```bash
git add apps/xiaozhi/src/xiaozhi_audio_linux.h apps/xiaozhi/src/xiaozhi_audio_linux.c apps/xiaozhi/tests/test_xiaozhi_audio_linux.c
git commit -m "feat: add linux audio runtime skeleton"
```

## 任务 3：接入 ALSA 设备注册、采集回调和 Opus 编解码

**文件：**
- 修改：`apps/xiaozhi/src/xiaozhi_audio_linux.c`
- 修改：`apps/xiaozhi/src/tuya_main.c`
- 修改：`apps/xiaozhi/CMakeLists.txt`
- 修改：`apps/xiaozhi/config/Linux.config`
- 修改：`apps/xiaozhi/app_default.config`
- 测试：Linux 手工集成验证

- [ ] **步骤 1：先写一个最小编译失败检查，确保 CMake/配置已包含新文件**

先在 `[apps/xiaozhi/CMakeLists.txt](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/CMakeLists.txt)` 加入新源文件路径后运行构建：

```cmake
    ${APP_PATH}/src/xiaozhi_audio_linux.c
```

运行：

```bash
. ./export.sh
cd apps/xiaozhi
tos.py build
```

预期：

- 先因 Linux 音频配置或缺少板级注册而失败，确认构建路径已覆盖新模块。

- [ ] **步骤 2：在 Linux 配置中开启 ALSA 能力和默认设备名**

修改 `[Linux.config](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/config/Linux.config)` 与 `[app_default.config](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/app_default.config)`：

```config
CONFIG_ENABLE_AUDIO_ALSA=y
CONFIG_AUDIO_CODEC_NAME="alsa_audio"
CONFIG_ALSA_DEVICE_CAPTURE="default"
CONFIG_ALSA_DEVICE_PLAYBACK="default"
```

- [ ] **步骤 3：在 Linux 启动路径注册板级硬件**

修改 `[tuya_main.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/tuya_main.c)`：

```c
#if OPERATING_SYSTEM == SYSTEM_LINUX
#include "board_com_api.h"
#endif

#if OPERATING_SYSTEM == SYSTEM_LINUX
    (void)board_register_hardware();
#endif
```

- [ ] **步骤 4：在音频运行时中接入 `tdl_audio_find/open/play/close`**

修改 `[xiaozhi_audio_linux.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_audio_linux.c)`，引入核心上下文：

```c
typedef struct {
    BOOL_T inited;
    BOOL_T capture_enabled;
    TDL_AUDIO_HANDLE_T audio;
    xz_audio_opus_tx_cb_t tx_cb;
    void *tx_userdata;
    xz_audio_pcm_accum_t accum;
    OpusEncoder *enc;
    OpusDecoder *dec;
    int16_t decode_pcm[320];
} xz_audio_linux_ctx_t;

static xz_audio_linux_ctx_t s_audio = {0};
```

并在 `init/deinit` 中完成：

```c
tdl_audio_find(AUDIO_CODEC_NAME, &s_audio.audio);
tdl_audio_open(s_audio.audio, xz_audio_mic_cb);
```

注意：

- 如果 `tdl_audio_open()` 只能打开一次，则在 `init` 时打开、`start_capture` 时仅切换 `capture_enabled`。
- `feed_opus()` 内部在解码成功后调用 `tdl_audio_play()`。

- [ ] **步骤 5：使用仓库内现有 Opus 头文件接入最小编解码**

修改 `[xiaozhi_audio_linux.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_audio_linux.c)`：

```c
#include "opus/opus.h"
int err = 0;
s_audio.enc = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &err);
s_audio.dec = opus_decoder_create(16000, 1, &err);
int out_len = opus_encode(s_audio.enc, (const opus_int16 *)frame_pcm, 320, opus_buf, sizeof(opus_buf));
int samples = opus_decode(s_audio.dec, data, (opus_int32)len, s_audio.decode_pcm, 320, 0);
```

- [ ] **步骤 6：在麦克风回调中把 PCM 聚合后编码并上送**

修改 `[xiaozhi_audio_linux.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_audio_linux.c)`：

```c
static void xz_audio_mic_cb(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len)
{
    uint8_t frame[XZ_AUDIO_PCM_FRAME_BYTES];

    if (!s_audio.capture_enabled || type != TDL_AUDIO_FRAME_FORMAT_PCM || status != TDL_AUDIO_STATUS_RECEIVING) {
        return;
    }

    if (xz_audio_pcm_accum_push(&s_audio.accum, data, len, frame, sizeof(frame)) != XZ_AUDIO_PCM_FRAME_BYTES) {
        return;
    }

    if (s_audio.tx_cb) {
        uint8_t opus_buf[256];
        int out_len = opus_encode(s_audio.enc, (const opus_int16 *)frame, 320, opus_buf, sizeof(opus_buf));
        if (out_len > 0) {
            s_audio.tx_cb(s_audio.tx_userdata, opus_buf, (size_t)out_len);
        }
    }
}
```

- [ ] **步骤 7：重新构建，确认 Linux 目标能编过**

运行：

```bash
. ./export.sh
cd apps/xiaozhi
mkdir -p ../../.cache && touch ../../.cache/.dont_prompt_update_platform
tos.py build
```

预期：

- 构建成功，生成 `apps/xiaozhi/dist/` 下 Linux 可执行物。

- [ ] **步骤 8：提交本任务**

```bash
git add apps/xiaozhi/src/xiaozhi_audio_linux.c apps/xiaozhi/src/tuya_main.c apps/xiaozhi/CMakeLists.txt apps/xiaozhi/config/Linux.config apps/xiaozhi/app_default.config
git commit -m "feat: add linux alsa opus audio runtime"
```

## 任务 4：抽离可测试的状态转换逻辑并接入 `xiaozhi_app`

**文件：**
- 新建：`apps/xiaozhi/src/xiaozhi_state.h`
- 新建：`apps/xiaozhi/src/xiaozhi_state.c`
- 新建：`apps/xiaozhi/tests/test_xiaozhi_state.c`
- 修改：`apps/xiaozhi/src/xiaozhi_app.c`
- 测试：`apps/xiaozhi/tests/test_xiaozhi_state.c`

- [ ] **步骤 1：先写失败测试，锁定状态切换语义**

新建 `[test_xiaozhi_state.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/tests/test_xiaozhi_state.c)`：

```c
#include <assert.h>
#include <stdio.h>

#include "xiaozhi_state.h"

int main(void)
{
    assert(xz_state_after_listen_start(XZ_CHAT_IDLE) == XZ_CHAT_LISTENING);
    assert(xz_state_after_tts_start(XZ_CHAT_LISTENING) == XZ_CHAT_SPEAKING);
    assert(xz_state_after_tts_stop(XZ_CHAT_SPEAKING) == XZ_CHAT_IDLE);
    assert(xz_state_after_abort(XZ_CHAT_SPEAKING) == XZ_CHAT_IDLE);

    puts("test_xiaozhi_state: PASS");
    return 0;
}
```

- [ ] **步骤 2：运行测试，确认它因缺少状态辅助文件而失败**

```bash
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_state.c -o /tmp/test_xz_state
```

预期：

- 编译失败，提示 `xiaozhi_state.h` 或状态辅助函数不存在。

- [ ] **步骤 3：实现最小状态辅助文件**

新建 `[xiaozhi_state.h](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_state.h)` 和 `[xiaozhi_state.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_state.c)`：

```c
typedef enum {
    XZ_CHAT_IDLE = 0,
    XZ_CHAT_CONNECTING,
    XZ_CHAT_LISTENING,
    XZ_CHAT_SPEAKING,
} xz_chat_state_t;

xz_chat_state_t xz_state_after_listen_start(xz_chat_state_t current);
xz_chat_state_t xz_state_after_tts_start(xz_chat_state_t current);
xz_chat_state_t xz_state_after_tts_stop(xz_chat_state_t current);
xz_chat_state_t xz_state_after_abort(xz_chat_state_t current);
```

```c
xz_chat_state_t xz_state_after_listen_start(xz_chat_state_t current)
{
    (void)current;
    return XZ_CHAT_LISTENING;
}

xz_chat_state_t xz_state_after_tts_start(xz_chat_state_t current)
{
    (void)current;
    return XZ_CHAT_SPEAKING;
}

xz_chat_state_t xz_state_after_tts_stop(xz_chat_state_t current)
{
    return (current == XZ_CHAT_SPEAKING) ? XZ_CHAT_IDLE : current;
}

xz_chat_state_t xz_state_after_abort(xz_chat_state_t current)
{
    return (current == XZ_CHAT_SPEAKING) ? XZ_CHAT_IDLE : current;
}
```

- [ ] **步骤 4：运行状态测试，确认通过**

```bash
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_state.c apps/xiaozhi/src/xiaozhi_state.c -o /tmp/test_xz_state && /tmp/test_xz_state
```

预期：

- 输出 `test_xiaozhi_state: PASS`

- [ ] **步骤 5：在应用初始化阶段初始化音频运行时并注册回调**

修改 `[xiaozhi_app.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_app.c)`：

```c
static void xz_on_audio_opus_frame(void *userdata, const uint8_t *data, size_t len)
{
    (void)userdata;
    if (s_app.active == XZ_ACTIVE_WS && s_app.chat_state == XZ_CHAT_LISTENING) {
        (void)xz_ws_send_audio(&s_app.ws, data, len);
    }
}

    rt = xiaozhi_audio_linux_init(xz_on_audio_opus_frame, &s_app);
    if (rt != OPRT_OK) {
        return rt;
    }
```

- [ ] **步骤 6：注册 WebSocket binary 回调，把下行 Opus 包喂给音频运行时**

修改 `[xiaozhi_app.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_app.c)`：

```c
static void xz_on_transport_binary_message(void *userdata, const uint8_t *payload, size_t payload_len)
{
    (void)userdata;
    if (s_app.chat_state == XZ_CHAT_SPEAKING) {
        (void)xiaozhi_audio_linux_feed_opus(payload, payload_len);
    }
}

    (void)xz_ws_set_binary_message_callback(&s_app.ws, xz_on_transport_binary_message, &s_app);
```

- [ ] **步骤 7：在 `listen start/stop` 里驱动录音启停，并使用状态辅助函数**

修改 `[xiaozhi_app.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_app.c)` 中 `xiaozhi_app_send_listen()`：

```c
    if (rt == OPRT_OK) {
        if (strcmp(state, "start") == 0) {
            s_app.listen_mode = xz_listen_mode_from_text(mode);
            s_app.chat_state = xz_state_after_listen_start(s_app.chat_state);
            (void)xiaozhi_audio_linux_reset();
            (void)xiaozhi_audio_linux_start_capture();
        } else if (strcmp(state, "stop") == 0) {
            (void)xiaozhi_audio_linux_stop_capture();
            s_app.chat_state = XZ_CHAT_IDLE;
        }
    }
```

- [ ] **步骤 8：在 `tts start/stop`、`abort` 和断线时同步音频状态**

修改 `[xiaozhi_app.c](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/src/xiaozhi_app.c)`：

```c
/* tts start */
(void)xiaozhi_audio_linux_stop_capture();
(void)xiaozhi_audio_linux_reset();
s_app.chat_state = xz_state_after_tts_start(s_app.chat_state);

/* tts stop */
(void)xiaozhi_audio_linux_abort_playback();
s_app.chat_state = xz_state_after_tts_stop(s_app.chat_state);

/* disconnect / poll error */
(void)xiaozhi_audio_linux_reset();

/* abort */
(void)xiaozhi_audio_linux_abort_playback();
s_app.chat_state = xz_state_after_abort(s_app.chat_state);
```

- [ ] **步骤 9：重新运行纯测试，并做一次最小构建**

运行：

```bash
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_state.c apps/xiaozhi/src/xiaozhi_state.c -o /tmp/test_xz_state && /tmp/test_xz_state
cc -DXZ_AUDIO_LINUX_TEST_ONLY -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_audio_linux.c apps/xiaozhi/src/xiaozhi_audio_linux.c -o /tmp/test_xz_audio_linux && /tmp/test_xz_audio_linux
. ./export.sh
cd apps/xiaozhi
tos.py build
```

预期：

- 聚合测试通过。
- 状态测试通过。
- Linux 目标能继续构建成功。

- [ ] **步骤 10：提交本任务**

```bash
git add apps/xiaozhi/src/xiaozhi_state.h apps/xiaozhi/src/xiaozhi_state.c apps/xiaozhi/tests/test_xiaozhi_state.c apps/xiaozhi/src/xiaozhi_app.c
git commit -m "feat: wire linux audio into xiaozhi app state"
```

## 任务 5：补充文档和 Linux 端验证步骤

**文件：**
- 修改：`apps/xiaozhi/README_CN.md`
- 修改：`apps/xiaozhi/README.md`
- 测试：Linux 运行验证

- [ ] **步骤 1：先写文档内容草稿，覆盖 ALSA 设备和运行方法**

在中文文档中增加：

```md
## Linux 语音链路

首版仅支持 WebSocket 语音通道。

默认 ALSA 设备：
- capture: `default`
- playback: `default`

可用环境变量覆盖：
- `XZ_ALSA_CAPTURE`
- `XZ_ALSA_PLAYBACK`
```

- [ ] **步骤 2：补充完整的 Linux 运行命令**

写入 `[README_CN.md](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/README_CN.md)`：

```bash
. ./export.sh
cd apps/xiaozhi
tos.py build

export XZ_WS_URL=wss://your.server/ws
export XZ_WS_TOKEN=your_token
export XZ_PROTOCOL=websocket
export XZ_ALSA_CAPTURE=default
export XZ_ALSA_PLAYBACK=default

./dist/xiaozhi
```

并明确手动验证流程：

```text
1. 执行 xz_status 确认 active=websocket state=ready
2. 执行 xz_listen start manual
3. 对麦克风讲话，确认服务端收到 Opus binary
4. 服务端返回 tts start + binary + tts stop
5. 本地确认能听到语音播报
6. 执行 xz_listen stop，确认回到 idle
```

- [ ] **步骤 3：同步更新英文文档**

在 `[README.md](/home/share/samba/xiaozhi/xiaozhi-codex-2026移植/TuyaOpen/apps/xiaozhi/README.md)` 中同步 Linux audio notes，保持参数名和命令一致。

- [ ] **步骤 4：执行最小相关验证**

运行：

```bash
python tools/check_format.py --debug --files apps/xiaozhi/src/xiaozhi_ws.c
python tools/check_format.py --debug --files apps/xiaozhi/src/xiaozhi_app.c
python tools/check_format.py --debug --files apps/xiaozhi/src/xiaozhi_audio_linux.c
python tools/check_format.py --debug --files apps/xiaozhi/README_CN.md
python tools/check_format.py --debug --files apps/xiaozhi/README.md
```

预期：

- 相关文件格式检查通过。

- [ ] **步骤 5：提交本任务**

```bash
git add apps/xiaozhi/README.md apps/xiaozhi/README_CN.md
git commit -m "docs: describe linux websocket audio usage"
```

## 任务 6：最终回归验证

**文件：**
- 测试：`apps/xiaozhi/tests/test_xiaozhi_ws_handshake.c`
- 测试：`apps/xiaozhi/tests/test_xiaozhi_identity.c`
- 测试：`apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c`
- 测试：`apps/xiaozhi/tests/test_xiaozhi_ws_binary.c`
- 测试：`apps/xiaozhi/tests/test_xiaozhi_audio_linux.c`
- 测试：`apps/xiaozhi/tests/test_xiaozhi_state.c`

- [ ] **步骤 1：运行现有纯测试，确认旧逻辑未回退**

```bash
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_ws_handshake.c apps/xiaozhi/src/xiaozhi_ws_handshake.c -o /tmp/test_xz_ws_handshake && /tmp/test_xz_ws_handshake
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_identity.c -o /tmp/test_xz_identity && /tmp/test_xz_identity
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_mqtt_topic.c -o /tmp/test_xz_mqtt_topic && /tmp/test_xz_mqtt_topic
```

预期：

- 三个测试均输出 `PASS`。

- [ ] **步骤 2：运行新增纯测试**

```bash
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_ws_binary.c -o /tmp/test_xz_ws_binary && /tmp/test_xz_ws_binary
cc -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_state.c apps/xiaozhi/src/xiaozhi_state.c -o /tmp/test_xz_state && /tmp/test_xz_state
cc -DXZ_AUDIO_LINUX_TEST_ONLY -I apps/xiaozhi/src apps/xiaozhi/tests/test_xiaozhi_audio_linux.c apps/xiaozhi/src/xiaozhi_audio_linux.c -o /tmp/test_xz_audio_linux && /tmp/test_xz_audio_linux
```

预期：

- 三个新增测试输出 `PASS`。

- [ ] **步骤 3：执行 Linux 最终构建**

```bash
. ./export.sh
mkdir -p .cache && touch .cache/.dont_prompt_update_platform
cd apps/xiaozhi
tos.py build
```

预期：

- 构建成功。
- `dist/` 下产出 Linux 可执行物。

- [ ] **步骤 4：执行人工联调**

```text
1. 配置 WebSocket URL 和 token
2. 启动应用
3. 执行 xz_listen start manual
4. 讲话并确认服务端接收音频
5. 让服务端返回 TTS 音频
6. 确认本地播放正常
7. 执行 xz_listen stop
8. 断网或关闭连接，确认状态回到 idle
```

- [ ] **步骤 5：整理并提交最终改动**

```bash
git add apps/xiaozhi
git add docs/superpowers/specs/2026-04-04-xiaozhi-linux-websocket-audio-design.md
git commit -m "feat: add linux websocket audio path for xiaozhi"
```
