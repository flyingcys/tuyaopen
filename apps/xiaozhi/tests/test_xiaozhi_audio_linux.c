#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "xiaozhi_audio_linux.h"

static void test_pcm_accum(void)
{
    xz_audio_pcm_accum_t accum;
    xz_audio_pcm_accum_reset(&accum);

    assert(accum.tail == 0);

    uint8_t chunk[XZ_AUDIO_PCM_FRAME_BYTES];
    memset(chunk, 0x5A, sizeof(chunk));

    size_t consumed = xz_audio_pcm_accum_push(&accum, chunk, 300);
    assert(consumed == 300);
    assert(accum.tail == 300);

    consumed = xz_audio_pcm_accum_push(&accum, chunk, 500);
    assert(consumed == 340);
    assert(accum.tail == 0);

    consumed = xz_audio_pcm_accum_push(&accum, chunk, XZ_AUDIO_PCM_FRAME_BYTES + 10);
    assert(consumed == XZ_AUDIO_PCM_FRAME_BYTES);
    assert(accum.tail == 0);
}

static void test_pcm_accum_overflow(void)
{
    xz_audio_pcm_accum_t accum;
    enum { kExtraBytes = 128 };
    const size_t total = XZ_AUDIO_PCM_FRAME_BYTES + kExtraBytes;
    uint8_t      payload[total];

    xz_audio_pcm_accum_reset(&accum);

    for (size_t i = 0; i < total; ++i) {
        payload[i] = (uint8_t)(i & 0xFF);
    }

    size_t first_consumed = xz_audio_pcm_accum_push(&accum, payload, total);
    assert(first_consumed == XZ_AUDIO_PCM_FRAME_BYTES);
    assert(accum.tail == 0);

    size_t remainder       = total - first_consumed;
    size_t second_consumed = xz_audio_pcm_accum_push(&accum, payload + first_consumed, remainder);
    assert(second_consumed == remainder);
    assert(accum.tail == remainder);

    for (size_t i = 0; i < remainder; ++i) {
        assert(accum.buffer[i] == payload[first_consumed + i]);
    }
}

int main(void)
{
    test_pcm_accum();
    test_pcm_accum_overflow();

    assert(xiaozhi_audio_linux_init() == 0);
    assert(xiaozhi_audio_linux_start_capture() == 0);
    assert(xiaozhi_audio_linux_stop_capture() == 0);
    assert(xiaozhi_audio_linux_feed_opus(NULL, 0) == 0);
    assert(xiaozhi_audio_linux_abort_playback() == 0);
    xiaozhi_audio_linux_reset();
    assert(xiaozhi_audio_linux_deinit() == 0);

    puts("xiaozhi_audio_linux tests passed");
    return 0;
}
