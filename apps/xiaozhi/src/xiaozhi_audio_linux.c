#include "xiaozhi_audio_linux.h"

#include <stdint.h>
#include <string.h>

#ifndef XZ_AUDIO_LINUX_TEST_ONLY
#include "../../../src/common/include/tuya_error_code.h"
#include "../../../src/peripherals/audio_codecs/tdl_audio/include/tdl_audio_manage.h"
#include "../../../src/tuya_ai_service/svc_ai_codec/src/opus/opus.h"
#include "tal_api.h"

#ifndef AUDIO_CODEC_NAME
#define AUDIO_CODEC_NAME "alsa_audio"
#endif

#define XZ_AUDIO_SAMPLE_RATE_HZ     16000
#define XZ_AUDIO_CHANNELS           1
#define XZ_AUDIO_FRAME_SAMPLES      (XZ_AUDIO_PCM_FRAME_BYTES / (sizeof(int16_t) * XZ_AUDIO_CHANNELS))
#define XZ_AUDIO_OPUS_MAX_PACKET    256
#define XZ_AUDIO_DECODE_MAX_SAMPLES 1920

#if defined(__GNUC__) || defined(__clang__)
#define XZ_AUDIO_WEAK __attribute__((weak))
#else
#define XZ_AUDIO_WEAK
#endif

typedef struct {
    BOOL_T                inited;
    BOOL_T                capture_enabled;
    BOOL_T                audio_opened;
    TDL_AUDIO_HANDLE_T    audio;
    xz_audio_opus_tx_cb_t tx_cb;
    void                 *tx_userdata;
    xz_audio_pcm_accum_t  accum;
    OpusEncoder          *enc;
    OpusDecoder          *dec;
    int16_t               decode_pcm[XZ_AUDIO_DECODE_MAX_SAMPLES];
} xz_audio_linux_ctx_t;

static xz_audio_linux_ctx_t s_audio = {0};

XZ_AUDIO_WEAK void xiaozhi_audio_linux_on_tx_opus_frame(const void *frame, size_t length, void *ctx)
{
    (void)frame;
    (void)length;
    (void)ctx;
}

static void xz_audio_emit_opus(const uint8_t *frame, size_t length)
{
    if (!frame || length == 0) {
        return;
    }
    if (s_audio.tx_cb) {
        s_audio.tx_cb(frame, length, s_audio.tx_userdata);
    }
}

static void xz_audio_destroy_codec(void)
{
    if (s_audio.enc) {
        opus_encoder_destroy(s_audio.enc);
        s_audio.enc = NULL;
    }
    if (s_audio.dec) {
        opus_decoder_destroy(s_audio.dec);
        s_audio.dec = NULL;
    }
}

static void xz_audio_mic_cb(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len)
{
    if (!s_audio.inited || !s_audio.capture_enabled || !s_audio.enc) {
        return;
    }
    if (type != TDL_AUDIO_FRAME_FORMAT_PCM || status != TDL_AUDIO_STATUS_RECEIVING || !data || len == 0) {
        return;
    }

    const uint8_t *pcm    = data;
    size_t         remain = (size_t)len;
    while (remain > 0) {
        size_t consumed = xz_audio_pcm_accum_push(&s_audio.accum, pcm, remain);
        if (consumed == 0) {
            break;
        }

        pcm += consumed;
        remain -= consumed;

        if (s_audio.accum.tail == 0) {
            uint8_t opus_buf[XZ_AUDIO_OPUS_MAX_PACKET] = {0};
            int     out_len = opus_encode(s_audio.enc, (const opus_int16 *)s_audio.accum.buffer, XZ_AUDIO_FRAME_SAMPLES,
                                          opus_buf, (opus_int32)sizeof(opus_buf));
            if (out_len > 0) {
                xz_audio_emit_opus(opus_buf, (size_t)out_len);
            } else {
                PR_WARN("opus_encode failed: %d", out_len);
            }
        }
    }
}
#endif

void xz_audio_pcm_accum_reset(xz_audio_pcm_accum_t *accum)
{
    if (!accum) {
        return;
    }
    accum->tail = 0;
}

size_t xz_audio_pcm_accum_push(xz_audio_pcm_accum_t *accum, const void *pcm, size_t bytes)
{
    if (!accum || !pcm || bytes == 0) {
        return 0;
    }

    size_t space   = XZ_AUDIO_PCM_FRAME_BYTES - accum->tail;
    size_t to_copy = (bytes < space) ? bytes : space;

    memcpy(accum->buffer + accum->tail, pcm, to_copy);
    accum->tail += to_copy;

    if (accum->tail >= XZ_AUDIO_PCM_FRAME_BYTES) {
        accum->tail = 0;
    }

    return to_copy;
}

#ifdef XZ_AUDIO_LINUX_TEST_ONLY

int xiaozhi_audio_linux_init(void)
{
    return 0;
}
int xiaozhi_audio_linux_deinit(void)
{
    return 0;
}
int xiaozhi_audio_linux_start_capture(void)
{
    return 0;
}
int xiaozhi_audio_linux_stop_capture(void)
{
    return 0;
}
int xiaozhi_audio_linux_feed_opus(const void *data, size_t size)
{
    (void)data;
    (void)size;
    return 0;
}
int xiaozhi_audio_linux_abort_playback(void)
{
    return 0;
}
void xiaozhi_audio_linux_reset(void) {}

#else

int xiaozhi_audio_linux_init(void)
{
    if (s_audio.inited) {
        return OPRT_OK;
    }

    memset(&s_audio, 0, sizeof(s_audio));
    xz_audio_pcm_accum_reset(&s_audio.accum);
    s_audio.tx_cb = xiaozhi_audio_linux_on_tx_opus_frame;

    int err     = 0;
    s_audio.enc = opus_encoder_create(XZ_AUDIO_SAMPLE_RATE_HZ, XZ_AUDIO_CHANNELS, OPUS_APPLICATION_VOIP, &err);
    if (!s_audio.enc || err != OPUS_OK) {
        PR_ERR("opus_encoder_create failed: %d", err);
        xz_audio_destroy_codec();
        return OPRT_COM_ERROR;
    }

    s_audio.dec = opus_decoder_create(XZ_AUDIO_SAMPLE_RATE_HZ, XZ_AUDIO_CHANNELS, &err);
    if (!s_audio.dec || err != OPUS_OK) {
        PR_ERR("opus_decoder_create failed: %d", err);
        xz_audio_destroy_codec();
        return OPRT_COM_ERROR;
    }

    OPERATE_RET rt = tdl_audio_find(AUDIO_CODEC_NAME, &s_audio.audio);
    if (rt != OPRT_OK) {
        PR_ERR("tdl_audio_find failed: %d codec=%s", rt, AUDIO_CODEC_NAME);
        xz_audio_destroy_codec();
        return rt;
    }

    rt = tdl_audio_open(s_audio.audio, xz_audio_mic_cb);
    if (rt != OPRT_OK) {
        PR_ERR("tdl_audio_open failed: %d codec=%s", rt, AUDIO_CODEC_NAME);
        s_audio.audio = NULL;
        xz_audio_destroy_codec();
        return rt;
    }

    s_audio.audio_opened = TRUE;
    s_audio.inited       = TRUE;
    return OPRT_OK;
}

int xiaozhi_audio_linux_deinit(void)
{
    if (!s_audio.inited) {
        return OPRT_OK;
    }

    s_audio.capture_enabled = FALSE;
    if (s_audio.audio_opened && s_audio.audio) {
        (void)tdl_audio_close(s_audio.audio);
    }

    s_audio.audio_opened = FALSE;
    s_audio.audio        = NULL;
    s_audio.tx_cb        = NULL;
    s_audio.tx_userdata  = NULL;
    xz_audio_destroy_codec();
    xz_audio_pcm_accum_reset(&s_audio.accum);
    s_audio.inited = FALSE;
    return OPRT_OK;
}

int xiaozhi_audio_linux_start_capture(void)
{
    if (!s_audio.inited || !s_audio.audio_opened) {
        return OPRT_COM_ERROR;
    }

    xz_audio_pcm_accum_reset(&s_audio.accum);
    s_audio.capture_enabled = TRUE;
    return OPRT_OK;
}

int xiaozhi_audio_linux_stop_capture(void)
{
    if (!s_audio.inited) {
        return OPRT_COM_ERROR;
    }

    s_audio.capture_enabled = FALSE;
    xz_audio_pcm_accum_reset(&s_audio.accum);
    return OPRT_OK;
}

int xiaozhi_audio_linux_feed_opus(const void *data, size_t size)
{
    if (!s_audio.inited || !s_audio.audio_opened || !s_audio.audio || !s_audio.dec) {
        return OPRT_COM_ERROR;
    }
    if (!data || size == 0) {
        return OPRT_INVALID_PARM;
    }

    int samples = opus_decode(s_audio.dec, (const uint8_t *)data, (opus_int32)size, s_audio.decode_pcm,
                              XZ_AUDIO_DECODE_MAX_SAMPLES, 0);
    if (samples <= 0) {
        PR_WARN("opus_decode failed: %d", samples);
        return OPRT_COM_ERROR;
    }

    uint32_t pcm_bytes = (uint32_t)(samples * XZ_AUDIO_CHANNELS * (int)sizeof(int16_t));
    return tdl_audio_play(s_audio.audio, (uint8_t *)s_audio.decode_pcm, pcm_bytes);
}

int xiaozhi_audio_linux_abort_playback(void)
{
    if (!s_audio.inited || !s_audio.audio_opened || !s_audio.audio) {
        return OPRT_OK;
    }

    return tdl_audio_play_stop(s_audio.audio);
}

void xiaozhi_audio_linux_reset(void)
{
    s_audio.capture_enabled = FALSE;
    xz_audio_pcm_accum_reset(&s_audio.accum);

    if (s_audio.enc) {
        (void)opus_encoder_ctl(s_audio.enc, OPUS_RESET_STATE);
    }
    if (s_audio.dec) {
        (void)opus_decoder_ctl(s_audio.dec, OPUS_RESET_STATE);
    }
    if (s_audio.inited && s_audio.audio_opened && s_audio.audio) {
        (void)tdl_audio_play_stop(s_audio.audio);
    }
}

#endif
