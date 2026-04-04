#include "xiaozhi_audio_linux.h"

#include <string.h>

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
    /* TODO: add ALSA/Opus bindings */
    return -1;
}

int xiaozhi_audio_linux_deinit(void)
{
    return -1;
}

int xiaozhi_audio_linux_start_capture(void)
{
    return -1;
}

int xiaozhi_audio_linux_stop_capture(void)
{
    return -1;
}

int xiaozhi_audio_linux_feed_opus(const void *data, size_t size)
{
    (void)data;
    (void)size;
    return -1;
}

int xiaozhi_audio_linux_abort_playback(void)
{
    return -1;
}

void xiaozhi_audio_linux_reset(void) {}

#endif
