#ifndef XIAOZHI_AUDIO_LINUX_H
#define XIAOZHI_AUDIO_LINUX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XZ_AUDIO_SAMPLE_RATE_HZ    16000
#define XZ_AUDIO_CHANNELS          1
#define XZ_AUDIO_FRAME_DURATION_MS 20
#define XZ_AUDIO_FRAME_SAMPLES     ((XZ_AUDIO_SAMPLE_RATE_HZ / 1000) * XZ_AUDIO_FRAME_DURATION_MS)
#define XZ_AUDIO_PCM_FRAME_BYTES   (XZ_AUDIO_FRAME_SAMPLES * XZ_AUDIO_CHANNELS * sizeof(int16_t))

typedef void (*xz_audio_opus_tx_cb_t)(const void *frame, size_t length, void *ctx);
typedef void (*xz_audio_hotword_cb_t)(const char *wake_word, void *ctx);

typedef struct {
    uint8_t buffer[XZ_AUDIO_PCM_FRAME_BYTES];
    size_t  tail;
} xz_audio_pcm_accum_t;

/**
 * The accumulator must be reset via xz_audio_pcm_accum_reset() or zero-initialized
 * before the first call to xz_audio_pcm_accum_push().
 */

void xz_audio_pcm_accum_reset(xz_audio_pcm_accum_t *accum);
/**
 * Push PCM bytes into the accumulator.
 *
 * @param accum accumulator state.
 * @param pcm pointer to PCM data.
 * @param bytes number of bytes available for push.
 * @return number of bytes consumed. Callers must invoke the function again
 *         with any leftover data when not all bytes could be packed.
 */
size_t xz_audio_pcm_accum_push(xz_audio_pcm_accum_t *accum, const void *pcm, size_t bytes);

int  xiaozhi_audio_linux_init(void);
int  xiaozhi_audio_linux_deinit(void);
int  xiaozhi_audio_linux_start_capture(void);
int  xiaozhi_audio_linux_stop_capture(void);
int  xiaozhi_audio_linux_start_detect(void);
int  xiaozhi_audio_linux_stop_detect(void);
int  xiaozhi_audio_linux_resume_detect(void);
void xiaozhi_audio_linux_set_hotword_callback(xz_audio_hotword_cb_t cb, void *ctx);
int  xiaozhi_audio_linux_play_wakeup_prompt(void);
int  xiaozhi_audio_linux_feed_opus(const void *data, size_t size);
int  xiaozhi_audio_linux_abort_playback(void);
void xiaozhi_audio_linux_reset(void);

#ifdef __cplusplus
}
#endif

#endif
