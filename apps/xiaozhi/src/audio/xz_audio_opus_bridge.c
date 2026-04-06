#include "xz_audio_opus_bridge.h"

#include <string.h>

#include "../../../../src/tuya_ai_service/svc_ai_codec/src/opus/opus.h"

#define XZ_AUDIO_SAMPLE_RATE_HZ     16000
#define XZ_AUDIO_CHANNELS           1
#define XZ_AUDIO_PCM_FRAME_BYTES    640
#define XZ_AUDIO_FRAME_SAMPLES      (XZ_AUDIO_PCM_FRAME_BYTES / (sizeof(int16_t) * XZ_AUDIO_CHANNELS))
#define XZ_AUDIO_OPUS_MAX_PACKET    256
#define XZ_AUDIO_DECODE_MAX_SAMPLES 1920

int xz_audio_opus_bridge_init(xz_audio_opus_bridge_t *bridge)
{
    int err = OPUS_OK;

    if (!bridge) {
        return -1;
    }

    memset(bridge, 0, sizeof(*bridge));

    bridge->enc = (void *)opus_encoder_create(XZ_AUDIO_SAMPLE_RATE_HZ, XZ_AUDIO_CHANNELS, OPUS_APPLICATION_VOIP, &err);
    if (!bridge->enc || err != OPUS_OK) {
        return -1;
    }

    bridge->dec = (void *)opus_decoder_create(XZ_AUDIO_SAMPLE_RATE_HZ, XZ_AUDIO_CHANNELS, &err);
    if (!bridge->dec || err != OPUS_OK) {
        opus_encoder_destroy((OpusEncoder *)bridge->enc);
        bridge->enc = NULL;
        return -1;
    }

    return 0;
}

void xz_audio_opus_bridge_deinit(xz_audio_opus_bridge_t *bridge)
{
    if (!bridge) {
        return;
    }

    if (bridge->enc) {
        opus_encoder_destroy((OpusEncoder *)bridge->enc);
        bridge->enc = NULL;
    }
    if (bridge->dec) {
        opus_decoder_destroy((OpusDecoder *)bridge->dec);
        bridge->dec = NULL;
    }
    bridge->buffer_tail = 0;
}

int xz_audio_opus_bridge_encode_pcm(xz_audio_opus_bridge_t *bridge, const uint8_t *pcm, size_t bytes, xz_opus_frame_cb_t cb,
                                    void *userdata)
{
    if (!bridge || !bridge->enc || !pcm || bytes == 0 || !cb) {
        return -1;
    }

    while (bytes > 0) {
        size_t space   = XZ_AUDIO_PCM_FRAME_BYTES - bridge->buffer_tail;
        size_t to_copy = (bytes < space) ? bytes : space;

        memcpy(bridge->buffer + bridge->buffer_tail, pcm, to_copy);
        bridge->buffer_tail += to_copy;
        pcm += to_copy;
        bytes -= to_copy;

        if (bridge->buffer_tail == XZ_AUDIO_PCM_FRAME_BYTES) {
            uint8_t opus_buf[XZ_AUDIO_OPUS_MAX_PACKET] = {0};
            int     out_len =
                opus_encode((OpusEncoder *)bridge->enc, (const opus_int16 *)bridge->buffer, XZ_AUDIO_FRAME_SAMPLES, opus_buf,
                            sizeof(opus_buf));
            if (out_len <= 0) {
                bridge->buffer_tail = 0;
                return -1;
            }
            cb(opus_buf, (size_t)out_len, userdata);
            bridge->buffer_tail = 0;
        }
    }

    return 0;
}

int xz_audio_opus_bridge_decode_opus(xz_audio_opus_bridge_t *bridge, const void *opus, size_t len, int16_t **pcm,
                                     size_t *samples)
{
    int out_samples;

    if (!bridge || !bridge->dec || !opus || len == 0 || !pcm || !samples) {
        return -1;
    }

    out_samples =
        opus_decode((OpusDecoder *)bridge->dec, (const uint8_t *)opus, (opus_int32)len, bridge->decode_pcm,
                    XZ_AUDIO_DECODE_MAX_SAMPLES, 0);
    if (out_samples <= 0) {
        return -1;
    }

    *pcm     = bridge->decode_pcm;
    *samples = (size_t)out_samples;
    return 0;
}

void xz_audio_opus_bridge_reset(xz_audio_opus_bridge_t *bridge)
{
    if (!bridge) {
        return;
    }

    bridge->buffer_tail = 0;
    if (bridge->enc) {
        (void)opus_encoder_ctl((OpusEncoder *)bridge->enc, OPUS_RESET_STATE);
    }
    if (bridge->dec) {
        (void)opus_decoder_ctl((OpusDecoder *)bridge->dec, OPUS_RESET_STATE);
    }
}
