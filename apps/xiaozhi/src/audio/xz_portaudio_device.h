#ifndef XZ_PORTAUDIO_DEVICE_H
#define XZ_PORTAUDIO_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xz_pa_device xz_pa_device_t;

typedef struct {
    int sample_rate;
    int channels;
    int frames_per_buffer;
} xz_pa_config_t;

int xz_pa_runtime_init(void);
int xz_pa_runtime_deinit(void);

int xz_pa_capture_open(xz_pa_device_t **out, const xz_pa_config_t *config);
int xz_pa_capture_read(xz_pa_device_t *device, int16_t *pcm, size_t frames);

int xz_pa_playback_open(xz_pa_device_t **out, const xz_pa_config_t *config);
int xz_pa_playback_write(xz_pa_device_t *device, const int16_t *pcm, size_t frames);
int xz_pa_playback_abort(xz_pa_device_t *device);

int xz_pa_device_close(xz_pa_device_t *device);

#ifdef __cplusplus
}
#endif

#endif
