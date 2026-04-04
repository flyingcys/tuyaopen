#ifndef XIAOZHI_AUDIO_LINUX_H
#define XIAOZHI_AUDIO_LINUX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XZ_AUDIO_PCM_FRAME_BYTES 640

typedef void (*xz_audio_opus_tx_cb_t)(const void *frame, size_t length, void *ctx);

typedef struct {
    uint8_t buffer[XZ_AUDIO_PCM_FRAME_BYTES];
    size_t  tail;
} xz_audio_pcm_accum_t;

void   xz_audio_pcm_accum_reset(xz_audio_pcm_accum_t *accum);
size_t xz_audio_pcm_accum_push(xz_audio_pcm_accum_t *accum, const void *pcm, size_t bytes);

int  xiaozhi_audio_linux_init(void);
int  xiaozhi_audio_linux_deinit(void);
int  xiaozhi_audio_linux_start_capture(void);
int  xiaozhi_audio_linux_stop_capture(void);
int  xiaozhi_audio_linux_feed_opus(const void *data, size_t size);
int  xiaozhi_audio_linux_abort_playback(void);
void xiaozhi_audio_linux_reset(void);

#ifdef __cplusplus
}
#endif

#endif
