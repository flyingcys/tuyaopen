#ifndef XZ_AUDIO_OPUS_BRIDGE_H
#define XZ_AUDIO_OPUS_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*xz_opus_frame_cb_t)(const uint8_t *data, size_t len, void *userdata);

typedef struct {
    uint8_t buffer[640];
    size_t  buffer_tail;
    void   *enc;
    void   *dec;
    int16_t decode_pcm[1920];
} xz_audio_opus_bridge_t;

int  xz_audio_opus_bridge_init(xz_audio_opus_bridge_t *bridge);
void xz_audio_opus_bridge_deinit(xz_audio_opus_bridge_t *bridge);
int  xz_audio_opus_bridge_encode_pcm(xz_audio_opus_bridge_t *bridge, const uint8_t *pcm, size_t bytes,
                                     xz_opus_frame_cb_t cb, void *userdata);
int  xz_audio_opus_bridge_decode_opus(xz_audio_opus_bridge_t *bridge, const void *opus, size_t len, int16_t **pcm,
                                      size_t *samples);
void xz_audio_opus_bridge_reset(xz_audio_opus_bridge_t *bridge);

#ifdef __cplusplus
}
#endif

#endif
