#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "xiaozhi_audio_linux.h"
#include "audio/xz_audio_opus_bridge.h"
#include "audio/xz_portaudio_device.h"
#include "wakeword/xz_snowboy_runner.h"

static int g_hotword_calls = 0;

static void on_hotword(const char *wake_word, void *ctx)
{
    (void)wake_word;
    (void)ctx;
    g_hotword_calls++;
}

int xz_pa_capture_open(xz_pa_device_t **out, const xz_pa_config_t *config)
{
    (void)out;
    (void)config;
    return 0;
}

int xz_pa_capture_read(xz_pa_device_t *device, int16_t *pcm, size_t frames)
{
    (void)device;
    (void)pcm;
    (void)frames;
    return 0;
}

int xz_pa_playback_open(xz_pa_device_t **out, const xz_pa_config_t *config)
{
    (void)out;
    (void)config;
    return 0;
}

int xz_pa_playback_write(xz_pa_device_t *device, const int16_t *pcm, size_t frames)
{
    (void)device;
    (void)pcm;
    (void)frames;
    return 0;
}

int xz_pa_playback_abort(xz_pa_device_t *device)
{
    (void)device;
    return 0;
}

int xz_pa_device_close(xz_pa_device_t *device)
{
    (void)device;
    return 0;
}

int xz_audio_opus_bridge_init(xz_audio_opus_bridge_t *bridge)
{
    (void)bridge;
    return 0;
}

void xz_audio_opus_bridge_deinit(xz_audio_opus_bridge_t *bridge)
{
    (void)bridge;
}

int xz_audio_opus_bridge_encode_pcm(xz_audio_opus_bridge_t *bridge, const uint8_t *pcm, size_t bytes,
                                    xz_opus_frame_cb_t cb, void *userdata)
{
    (void)bridge;
    (void)pcm;
    (void)bytes;
    (void)cb;
    (void)userdata;
    return 0;
}

int xz_audio_opus_bridge_decode_opus(xz_audio_opus_bridge_t *bridge, const void *opus, size_t len, int16_t **pcm,
                                     size_t *samples)
{
    static int16_t dummy[320];
    (void)bridge;
    (void)opus;
    (void)len;
    *pcm     = dummy;
    *samples = 320;
    return 0;
}

void xz_audio_opus_bridge_reset(xz_audio_opus_bridge_t *bridge)
{
    (void)bridge;
}

int xz_snowboy_runner_create(xz_snowboy_runner_t **out, const char *resource_path, const char *model_path,
                             xz_snowboy_detected_cb_t cb, void *userdata)
{
    (void)out;
    (void)resource_path;
    (void)model_path;
    (void)cb;
    (void)userdata;
    return 0;
}

int xz_snowboy_runner_feed_pcm(xz_snowboy_runner_t *runner, const int16_t *pcm, size_t samples)
{
    (void)runner;
    (void)pcm;
    (void)samples;
    return 0;
}

int xz_snowboy_runner_reset(xz_snowboy_runner_t *runner)
{
    (void)runner;
    return 0;
}

void xz_snowboy_runner_destroy(xz_snowboy_runner_t *runner)
{
    (void)runner;
}

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
    xiaozhi_audio_linux_set_hotword_callback(on_hotword, NULL);
    assert(xiaozhi_audio_linux_start_capture() == 0);
    assert(xiaozhi_audio_linux_stop_capture() == 0);
    assert(xiaozhi_audio_linux_play_wakeup_prompt() == 0);
    assert(xiaozhi_audio_linux_feed_opus(NULL, 0) != 0);
    assert(xiaozhi_audio_linux_abort_playback() == 0);
    xiaozhi_audio_linux_reset();
    assert(xiaozhi_audio_linux_deinit() == 0);

    puts("xiaozhi_audio_linux tests passed");
    return 0;
}
