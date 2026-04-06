#include "xiaozhi_audio_linux.h"

#include "audio/xz_audio_opus_bridge.h"
#include "audio/xz_portaudio_device.h"

#ifdef XZ_AUDIO_LINUX_TEST_ONLY
typedef int BOOL_T;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef OPRT_OK
#define OPRT_OK 0
#endif
#ifndef OPRT_COM_ERROR
#define OPRT_COM_ERROR (-1)
#endif
#ifndef OPRT_INVALID_PARM
#define OPRT_INVALID_PARM (-2)
#endif
#ifndef OPRT_NOT_FOUND
#define OPRT_NOT_FOUND (-3)
#endif
#else
#include "tal_api.h"
#include "tuya_error_code.h"
#endif
#include "wakeword/xz_snowboy_runner.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XZ_DEFAULT_SNOWBOY_RESOURCE "src/resources/common.res"
#define XZ_DEFAULT_SNOWBOY_MODEL    "src/resources/models/echo.pmdl"
#define XZ_DEFAULT_WAKE_PROMPT_PCM  "src/resources/waked.pcm"

#if defined(__GNUC__) || defined(__clang__)
#define XZ_AUDIO_WEAK __attribute__((weak))
#else
#define XZ_AUDIO_WEAK
#endif

typedef struct {
    BOOL_T                  inited;
    BOOL_T                  capture_enabled;
    BOOL_T                  detect_enabled;
    BOOL_T                  detect_requested;
    BOOL_T                  playback_opened;
    BOOL_T                  hotword_pending;
    pthread_t               capture_thread;
    pthread_t               detect_thread;
    BOOL_T                  capture_thread_started;
    BOOL_T                  detect_thread_started;
    pthread_mutex_t         lock;
    xz_pa_device_t         *capture_dev;
    xz_pa_device_t         *detect_dev;
    xz_pa_device_t         *playback_dev;
    xz_audio_opus_bridge_t  bridge;
    xz_audio_opus_tx_cb_t   tx_cb;
    void                   *tx_userdata;
    xz_audio_hotword_cb_t   hotword_cb;
    void                   *hotword_userdata;
    xz_snowboy_runner_t    *snowboy;
    char                    snowboy_resource[256];
    char                    snowboy_model[256];
    char                    wake_prompt_pcm[256];
    char                    wake_word_text[64];
} xz_audio_linux_ctx_t;

static xz_audio_linux_ctx_t s_audio = {0};

XZ_AUDIO_WEAK void xiaozhi_audio_linux_on_tx_opus_frame(const void *frame, size_t length, void *ctx)
{
    (void)frame;
    (void)length;
    (void)ctx;
}

static void xz_audio_emit_opus_frame(const uint8_t *frame, size_t length, void *userdata)
{
    (void)userdata;
    if (!frame || length == 0) {
        return;
    }

    if (s_audio.tx_cb) {
        s_audio.tx_cb(frame, length, s_audio.tx_userdata);
    }
}

static void xz_audio_copy_model_basename(char *out, size_t out_size, const char *model_path)
{
    const char *base;
    size_t      len = 0;

    if (!out || out_size == 0) {
        return;
    }

    base = model_path ? strrchr(model_path, '/') : NULL;
    base = base ? (base + 1) : (model_path ? model_path : "wake");

    while (base[len] != '\0' && base[len] != '.' && len + 1 < out_size) {
        out[len] = base[len];
        ++len;
    }
    out[len] = '\0';

    if (len == 0) {
        (void)snprintf(out, out_size, "wake");
    }
}

static void xz_audio_copy_env_or_default(char *out, size_t out_size, const char *env_name, const char *fallback)
{
    const char *env = getenv(env_name);
    if (env && env[0] != '\0') {
        (void)snprintf(out, out_size, "%s", env);
        return;
    }
    (void)snprintf(out, out_size, "%s", fallback);
}

static void xz_audio_close_capture_locked(void)
{
    if (s_audio.capture_dev) {
        (void)xz_pa_device_close(s_audio.capture_dev);
        s_audio.capture_dev = NULL;
    }
}

static void xz_audio_close_detect_locked(void)
{
    if (s_audio.detect_dev) {
        (void)xz_pa_device_close(s_audio.detect_dev);
        s_audio.detect_dev = NULL;
    }
}

static void xz_audio_close_playback_locked(void)
{
    if (s_audio.playback_dev) {
        (void)xz_pa_device_close(s_audio.playback_dev);
        s_audio.playback_dev  = NULL;
        s_audio.playback_opened = FALSE;
    }
}

static void xz_audio_join_capture_thread_locked(void)
{
    pthread_t tid;

    if (!s_audio.capture_thread_started) {
        return;
    }

    tid                           = s_audio.capture_thread;
    s_audio.capture_thread_started = FALSE;
    pthread_mutex_unlock(&s_audio.lock);
    (void)pthread_join(tid, NULL);
    pthread_mutex_lock(&s_audio.lock);
}

static void xz_audio_join_detect_thread_locked(void)
{
    pthread_t tid;

    if (!s_audio.detect_thread_started) {
        return;
    }

    tid                          = s_audio.detect_thread;
    s_audio.detect_thread_started = FALSE;
    pthread_mutex_unlock(&s_audio.lock);
    (void)pthread_join(tid, NULL);
    pthread_mutex_lock(&s_audio.lock);
}

static void xz_audio_ensure_snowboy_locked(void)
{
    if (s_audio.snowboy) {
        return;
    }

    xz_audio_copy_env_or_default(s_audio.snowboy_resource, sizeof(s_audio.snowboy_resource), "XZ_SNOWBOY_RESOURCE",
                                 XZ_DEFAULT_SNOWBOY_RESOURCE);
    xz_audio_copy_env_or_default(s_audio.snowboy_model, sizeof(s_audio.snowboy_model), "XZ_SNOWBOY_MODEL",
                                 XZ_DEFAULT_SNOWBOY_MODEL);
    xz_audio_copy_env_or_default(s_audio.wake_prompt_pcm, sizeof(s_audio.wake_prompt_pcm), "XZ_WAKE_PROMPT_PCM",
                                 XZ_DEFAULT_WAKE_PROMPT_PCM);
    xz_audio_copy_model_basename(s_audio.wake_word_text, sizeof(s_audio.wake_word_text), s_audio.snowboy_model);
    {
        const char *wake_word_text = getenv("XZ_WAKE_WORD_TEXT");
        if (wake_word_text && wake_word_text[0] != '\0') {
            (void)snprintf(s_audio.wake_word_text, sizeof(s_audio.wake_word_text), "%s", wake_word_text);
        }
    }

    if (xz_snowboy_runner_create(&s_audio.snowboy, s_audio.snowboy_resource, s_audio.snowboy_model, NULL, NULL) != 0) {
        s_audio.snowboy = NULL;
    }
}

static void xz_audio_hotword_detected(void *userdata)
{
    (void)userdata;
    pthread_mutex_lock(&s_audio.lock);
    s_audio.hotword_pending = TRUE;
    s_audio.detect_enabled  = FALSE;
    pthread_mutex_unlock(&s_audio.lock);
}

static void *xz_audio_capture_thread_main(void *arg)
{
    xz_pa_config_t cfg = {.sample_rate = XZ_AUDIO_SAMPLE_RATE_HZ, .channels = XZ_AUDIO_CHANNELS, .frames_per_buffer = XZ_AUDIO_FRAME_SAMPLES};
    int16_t        pcm[XZ_AUDIO_FRAME_SAMPLES] = {0};

    (void)arg;

    pthread_mutex_lock(&s_audio.lock);
    if (!s_audio.capture_dev && xz_pa_capture_open(&s_audio.capture_dev, &cfg) != 0) {
        s_audio.capture_enabled = FALSE;
        pthread_mutex_unlock(&s_audio.lock);
        return NULL;
    }
    pthread_mutex_unlock(&s_audio.lock);

    while (1) {
        pthread_mutex_lock(&s_audio.lock);
        if (!s_audio.capture_enabled || !s_audio.capture_dev) {
            pthread_mutex_unlock(&s_audio.lock);
            break;
        }
        pthread_mutex_unlock(&s_audio.lock);

        if (xz_pa_capture_read(s_audio.capture_dev, pcm, XZ_AUDIO_FRAME_SAMPLES) != 0) {
            pthread_mutex_lock(&s_audio.lock);
            s_audio.capture_enabled = FALSE;
            pthread_mutex_unlock(&s_audio.lock);
            break;
        }

        (void)xz_audio_opus_bridge_encode_pcm(&s_audio.bridge, (const uint8_t *)pcm, sizeof(pcm), xz_audio_emit_opus_frame, NULL);
    }

    pthread_mutex_lock(&s_audio.lock);
    xz_audio_close_capture_locked();
    pthread_mutex_unlock(&s_audio.lock);
    return NULL;
}

static void *xz_audio_detect_thread_main(void *arg)
{
    xz_pa_config_t cfg = {.sample_rate = XZ_AUDIO_SAMPLE_RATE_HZ, .channels = XZ_AUDIO_CHANNELS, .frames_per_buffer = XZ_AUDIO_FRAME_SAMPLES};
    int16_t        pcm[XZ_AUDIO_FRAME_SAMPLES] = {0};
    int            detect_result               = 0;

    (void)arg;

    pthread_mutex_lock(&s_audio.lock);
    xz_audio_ensure_snowboy_locked();
    if (!s_audio.snowboy) {
        s_audio.detect_enabled = FALSE;
        pthread_mutex_unlock(&s_audio.lock);
        return NULL;
    }

    xz_snowboy_runner_destroy(s_audio.snowboy);
    s_audio.snowboy = NULL;
    if (xz_snowboy_runner_create(&s_audio.snowboy, s_audio.snowboy_resource, s_audio.snowboy_model, xz_audio_hotword_detected,
                                 NULL) != 0) {
        s_audio.detect_enabled = FALSE;
        pthread_mutex_unlock(&s_audio.lock);
        return NULL;
    }

    if (!s_audio.detect_dev && xz_pa_capture_open(&s_audio.detect_dev, &cfg) != 0) {
        s_audio.detect_enabled = FALSE;
        pthread_mutex_unlock(&s_audio.lock);
        return NULL;
    }
    pthread_mutex_unlock(&s_audio.lock);

    while (1) {
        pthread_mutex_lock(&s_audio.lock);
        if (!s_audio.detect_enabled || !s_audio.detect_dev || !s_audio.snowboy) {
            pthread_mutex_unlock(&s_audio.lock);
            break;
        }
        pthread_mutex_unlock(&s_audio.lock);

        if (xz_pa_capture_read(s_audio.detect_dev, pcm, XZ_AUDIO_FRAME_SAMPLES) != 0) {
            pthread_mutex_lock(&s_audio.lock);
            s_audio.detect_enabled = FALSE;
            pthread_mutex_unlock(&s_audio.lock);
            break;
        }

        detect_result = xz_snowboy_runner_feed_pcm(s_audio.snowboy, pcm, XZ_AUDIO_FRAME_SAMPLES);
        if (detect_result > 0) {
            break;
        }
    }

    pthread_mutex_lock(&s_audio.lock);
    xz_audio_close_detect_locked();
    pthread_mutex_unlock(&s_audio.lock);

    if (detect_result > 0 && s_audio.hotword_cb) {
        s_audio.hotword_cb(s_audio.wake_word_text[0] ? s_audio.wake_word_text : "wake", s_audio.hotword_userdata);
    }

    return NULL;
}

void xz_audio_pcm_accum_reset(xz_audio_pcm_accum_t *accum)
{
    if (!accum) {
        return;
    }
    accum->tail = 0;
}

size_t xz_audio_pcm_accum_push(xz_audio_pcm_accum_t *accum, const void *pcm, size_t bytes)
{
    size_t space;
    size_t to_copy;

    if (!accum || !pcm || bytes == 0) {
        return 0;
    }

    space   = XZ_AUDIO_PCM_FRAME_BYTES - accum->tail;
    to_copy = (bytes < space) ? bytes : space;

    memcpy(accum->buffer + accum->tail, pcm, to_copy);
    accum->tail += to_copy;

    if (accum->tail >= XZ_AUDIO_PCM_FRAME_BYTES) {
        accum->tail = 0;
    }

    return to_copy;
}

#ifdef XZ_AUDIO_LINUX_TEST_ONLY

int xiaozhi_audio_linux_init(void) { return 0; }
int xiaozhi_audio_linux_deinit(void) { return 0; }
int xiaozhi_audio_linux_start_capture(void) { return 0; }
int xiaozhi_audio_linux_stop_capture(void) { return 0; }
int xiaozhi_audio_linux_start_detect(void) { return 0; }
int xiaozhi_audio_linux_stop_detect(void) { return 0; }
int xiaozhi_audio_linux_resume_detect(void) { return 0; }
void xiaozhi_audio_linux_set_hotword_callback(xz_audio_hotword_cb_t cb, void *ctx)
{
    (void)cb;
    (void)ctx;
}
int xiaozhi_audio_linux_play_wakeup_prompt(void) { return 0; }
int xiaozhi_audio_linux_feed_opus(const void *data, size_t size)
{
    return (!data || size == 0) ? OPRT_INVALID_PARM : OPRT_OK;
}
int xiaozhi_audio_linux_abort_playback(void) { return 0; }
void xiaozhi_audio_linux_reset(void) {}

#else

static int xz_audio_ensure_playback_device_locked(void)
{
    xz_pa_config_t cfg = {.sample_rate = XZ_AUDIO_SAMPLE_RATE_HZ,
                          .channels = XZ_AUDIO_CHANNELS,
                          .frames_per_buffer = XZ_AUDIO_FRAME_SAMPLES};

    if (s_audio.playback_dev) {
        return OPRT_OK;
    }

    if (xz_pa_playback_open(&s_audio.playback_dev, &cfg) != 0) {
        return OPRT_COM_ERROR;
    }

    s_audio.playback_opened = TRUE;
    return OPRT_OK;
}

static int xz_audio_play_pcm_file_locked(const char *path)
{
    FILE   *fp;
    int16_t pcm[XZ_AUDIO_FRAME_SAMPLES];

    if (!path || path[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return OPRT_NOT_FOUND;
    }

    int rt = xz_audio_ensure_playback_device_locked();
    if (rt != OPRT_OK) {
        (void)fclose(fp);
        return rt;
    }

    while (!feof(fp)) {
        size_t samples = fread(pcm, sizeof(int16_t), XZ_AUDIO_FRAME_SAMPLES, fp);
        if (samples == 0) {
            break;
        }
        if (xz_pa_playback_write(s_audio.playback_dev, pcm, samples) != 0) {
            (void)fclose(fp);
            return OPRT_COM_ERROR;
        }
    }

    (void)fclose(fp);
    return OPRT_OK;
}

int xiaozhi_audio_linux_init(void)
{
    if (s_audio.inited) {
        return OPRT_OK;
    }

    memset(&s_audio, 0, sizeof(s_audio));
    if (pthread_mutex_init(&s_audio.lock, NULL) != 0) {
        return OPRT_COM_ERROR;
    }

    if (xz_audio_opus_bridge_init(&s_audio.bridge) != 0) {
        (void)pthread_mutex_destroy(&s_audio.lock);
        return OPRT_COM_ERROR;
    }

    xz_audio_copy_env_or_default(s_audio.snowboy_resource, sizeof(s_audio.snowboy_resource), "XZ_SNOWBOY_RESOURCE",
                                 XZ_DEFAULT_SNOWBOY_RESOURCE);
    xz_audio_copy_env_or_default(s_audio.snowboy_model, sizeof(s_audio.snowboy_model), "XZ_SNOWBOY_MODEL",
                                 XZ_DEFAULT_SNOWBOY_MODEL);
    xz_audio_copy_env_or_default(s_audio.wake_prompt_pcm, sizeof(s_audio.wake_prompt_pcm), "XZ_WAKE_PROMPT_PCM",
                                 XZ_DEFAULT_WAKE_PROMPT_PCM);
    xz_audio_copy_model_basename(s_audio.wake_word_text, sizeof(s_audio.wake_word_text), s_audio.snowboy_model);
    {
        const char *wake_word_text = getenv("XZ_WAKE_WORD_TEXT");
        if (wake_word_text && wake_word_text[0] != '\0') {
            (void)snprintf(s_audio.wake_word_text, sizeof(s_audio.wake_word_text), "%s", wake_word_text);
        }
    }

    s_audio.tx_cb  = xiaozhi_audio_linux_on_tx_opus_frame;
    s_audio.inited = TRUE;
    return OPRT_OK;
}

int xiaozhi_audio_linux_deinit(void)
{
    if (!s_audio.inited) {
        return OPRT_OK;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    s_audio.capture_enabled = FALSE;
    s_audio.detect_enabled  = FALSE;
    s_audio.detect_requested = FALSE;
    xz_audio_join_capture_thread_locked();
    xz_audio_join_detect_thread_locked();
    xz_audio_close_capture_locked();
    xz_audio_close_detect_locked();
    xz_audio_close_playback_locked();
    if (s_audio.snowboy) {
        xz_snowboy_runner_destroy(s_audio.snowboy);
        s_audio.snowboy = NULL;
    }
    (void)pthread_mutex_unlock(&s_audio.lock);

    xz_audio_opus_bridge_deinit(&s_audio.bridge);
    (void)pthread_mutex_destroy(&s_audio.lock);
    memset(&s_audio, 0, sizeof(s_audio));
    return OPRT_OK;
}

int xiaozhi_audio_linux_start_capture(void)
{
    int rc;

    if (!s_audio.inited) {
        return OPRT_COM_ERROR;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    s_audio.detect_enabled = FALSE;
    xz_audio_join_detect_thread_locked();
    xz_audio_close_detect_locked();

    if (s_audio.capture_enabled) {
        PR_NOTICE("audio capture already running");
        (void)pthread_mutex_unlock(&s_audio.lock);
        return OPRT_OK;
    }

    xz_audio_opus_bridge_reset(&s_audio.bridge);
    s_audio.capture_enabled = TRUE;
    PR_NOTICE("audio capture start");
    rc = pthread_create(&s_audio.capture_thread, NULL, xz_audio_capture_thread_main, NULL);
    if (rc != 0) {
        s_audio.capture_enabled = FALSE;
        (void)pthread_mutex_unlock(&s_audio.lock);
        return OPRT_COM_ERROR;
    }
    s_audio.capture_thread_started = TRUE;
    (void)pthread_mutex_unlock(&s_audio.lock);
    return OPRT_OK;
}

int xiaozhi_audio_linux_stop_capture(void)
{
    if (!s_audio.inited) {
        return OPRT_OK;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    if (s_audio.capture_enabled) {
        PR_NOTICE("audio capture stop");
    }
    s_audio.capture_enabled = FALSE;
    xz_audio_join_capture_thread_locked();
    xz_audio_close_capture_locked();
    (void)pthread_mutex_unlock(&s_audio.lock);
    return OPRT_OK;
}

int xiaozhi_audio_linux_start_detect(void)
{
    int rc;

    if (!s_audio.inited) {
        return OPRT_COM_ERROR;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    s_audio.detect_requested = TRUE;
    if (s_audio.capture_enabled || s_audio.detect_enabled) {
        PR_NOTICE("audio detect skip: capture=%d detect=%d", s_audio.capture_enabled, s_audio.detect_enabled);
        (void)pthread_mutex_unlock(&s_audio.lock);
        return OPRT_OK;
    }

    s_audio.detect_enabled = TRUE;
    PR_NOTICE("audio detect start");
    rc = pthread_create(&s_audio.detect_thread, NULL, xz_audio_detect_thread_main, NULL);
    if (rc != 0) {
        s_audio.detect_enabled  = FALSE;
        s_audio.detect_requested = FALSE;
        (void)pthread_mutex_unlock(&s_audio.lock);
        return OPRT_COM_ERROR;
    }
    s_audio.detect_thread_started = TRUE;
    (void)pthread_mutex_unlock(&s_audio.lock);
    return OPRT_OK;
}

int xiaozhi_audio_linux_stop_detect(void)
{
    if (!s_audio.inited) {
        return OPRT_OK;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    if (s_audio.detect_enabled || s_audio.detect_requested) {
        PR_NOTICE("audio detect stop");
    }
    s_audio.detect_requested = FALSE;
    s_audio.detect_enabled   = FALSE;
    xz_audio_join_detect_thread_locked();
    xz_audio_close_detect_locked();
    (void)pthread_mutex_unlock(&s_audio.lock);
    return OPRT_OK;
}

int xiaozhi_audio_linux_resume_detect(void)
{
    if (!s_audio.inited) {
        return OPRT_COM_ERROR;
    }

    if (!s_audio.detect_requested) {
        PR_NOTICE("audio detect resume skipped: not requested");
        return OPRT_OK;
    }

    PR_NOTICE("audio detect resume");
    return xiaozhi_audio_linux_start_detect();
}

void xiaozhi_audio_linux_set_hotword_callback(xz_audio_hotword_cb_t cb, void *ctx)
{
    if (!s_audio.inited) {
        return;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    s_audio.hotword_cb       = cb;
    s_audio.hotword_userdata = ctx;
    (void)pthread_mutex_unlock(&s_audio.lock);
}

int xiaozhi_audio_linux_play_wakeup_prompt(void)
{
    int rt;

    if (!s_audio.inited) {
        return OPRT_COM_ERROR;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    PR_NOTICE("play wakeup prompt: %s", s_audio.wake_prompt_pcm);
    rt = xz_audio_play_pcm_file_locked(s_audio.wake_prompt_pcm);
    (void)pthread_mutex_unlock(&s_audio.lock);
    return rt;
}

int xiaozhi_audio_linux_feed_opus(const void *data, size_t size)
{
    int16_t       *pcm     = NULL;
    size_t         samples = 0;

    if (!s_audio.inited) {
        return OPRT_COM_ERROR;
    }
    if (!data || size == 0) {
        return OPRT_INVALID_PARM;
    }

    if (xz_audio_opus_bridge_decode_opus(&s_audio.bridge, data, size, &pcm, &samples) != 0) {
        return OPRT_COM_ERROR;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    if (xz_audio_ensure_playback_device_locked() != OPRT_OK) {
        (void)pthread_mutex_unlock(&s_audio.lock);
        return OPRT_COM_ERROR;
    }
    if (xz_pa_playback_write(s_audio.playback_dev, pcm, samples) != 0) {
        (void)pthread_mutex_unlock(&s_audio.lock);
        return OPRT_COM_ERROR;
    }
    (void)pthread_mutex_unlock(&s_audio.lock);
    return OPRT_OK;
}

int xiaozhi_audio_linux_abort_playback(void)
{
    if (!s_audio.inited) {
        return OPRT_OK;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    if (s_audio.playback_dev) {
        (void)xz_pa_playback_abort(s_audio.playback_dev);
    }
    (void)pthread_mutex_unlock(&s_audio.lock);
    return OPRT_OK;
}

void xiaozhi_audio_linux_reset(void)
{
    if (!s_audio.inited) {
        return;
    }

    (void)pthread_mutex_lock(&s_audio.lock);
    s_audio.hotword_pending = FALSE;
    xz_audio_opus_bridge_reset(&s_audio.bridge);
    if (s_audio.playback_dev) {
        (void)xz_pa_playback_abort(s_audio.playback_dev);
    }
    if (s_audio.snowboy) {
        (void)xz_snowboy_runner_reset(s_audio.snowboy);
    }
    (void)pthread_mutex_unlock(&s_audio.lock);
}

#endif
