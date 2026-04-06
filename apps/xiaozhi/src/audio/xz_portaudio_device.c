#include "xz_portaudio_device.h"

#include <stdlib.h>
#include <string.h>

#include <portaudio.h>

struct xz_pa_device {
    PaStream *stream;
    int       is_input;
};

static int s_runtime_refs = 0;

static int xz_pa_translate(PaError err)
{
    return (err == paNoError) ? 0 : -1;
}

int xz_pa_runtime_init(void)
{
    if (s_runtime_refs == 0) {
        if (Pa_Initialize() != paNoError) {
            return -1;
        }
    }

    s_runtime_refs++;
    return 0;
}

int xz_pa_runtime_deinit(void)
{
    if (s_runtime_refs <= 0) {
        s_runtime_refs = 0;
        return 0;
    }

    s_runtime_refs--;
    if (s_runtime_refs == 0) {
        return xz_pa_translate(Pa_Terminate());
    }

    return 0;
}

static int xz_pa_open_common(xz_pa_device_t **out, const xz_pa_config_t *config, int is_input)
{
    if (!out || !config) {
        return -1;
    }

    if (xz_pa_runtime_init() != 0) {
        return -1;
    }

    xz_pa_device_t *device = calloc(1, sizeof(*device));
    if (!device) {
        (void)xz_pa_runtime_deinit();
        return -1;
    }

    PaStreamParameters params;
    memset(&params, 0, sizeof(params));

    params.device = is_input ? Pa_GetDefaultInputDevice() : Pa_GetDefaultOutputDevice();
    if (params.device == paNoDevice) {
        free(device);
        (void)xz_pa_runtime_deinit();
        return -1;
    }

    params.channelCount = config->channels;
    params.sampleFormat = paInt16;
    params.suggestedLatency =
        is_input ? Pa_GetDeviceInfo(params.device)->defaultLowInputLatency : Pa_GetDeviceInfo(params.device)->defaultLowOutputLatency;

    PaError err = Pa_OpenStream(&device->stream, is_input ? &params : NULL, is_input ? NULL : &params, config->sample_rate,
                                (unsigned long)config->frames_per_buffer, paClipOff, NULL, NULL);
    if (err != paNoError) {
        free(device);
        (void)xz_pa_runtime_deinit();
        return -1;
    }

    err = Pa_StartStream(device->stream);
    if (err != paNoError) {
        (void)Pa_CloseStream(device->stream);
        free(device);
        (void)xz_pa_runtime_deinit();
        return -1;
    }

    device->is_input = is_input;
    *out             = device;
    return 0;
}

int xz_pa_capture_open(xz_pa_device_t **out, const xz_pa_config_t *config)
{
    return xz_pa_open_common(out, config, 1);
}

int xz_pa_capture_read(xz_pa_device_t *device, int16_t *pcm, size_t frames)
{
    if (!device || !device->is_input || !pcm || frames == 0) {
        return -1;
    }

    return xz_pa_translate(Pa_ReadStream(device->stream, pcm, (unsigned long)frames));
}

int xz_pa_playback_open(xz_pa_device_t **out, const xz_pa_config_t *config)
{
    return xz_pa_open_common(out, config, 0);
}

int xz_pa_playback_write(xz_pa_device_t *device, const int16_t *pcm, size_t frames)
{
    if (!device || device->is_input || !pcm || frames == 0) {
        return -1;
    }

    return xz_pa_translate(Pa_WriteStream(device->stream, pcm, (unsigned long)frames));
}

int xz_pa_playback_abort(xz_pa_device_t *device)
{
    if (!device || device->is_input) {
        return 0;
    }

    return xz_pa_translate(Pa_AbortStream(device->stream));
}

int xz_pa_device_close(xz_pa_device_t *device)
{
    if (!device) {
        return 0;
    }

    (void)Pa_StopStream(device->stream);
    (void)Pa_CloseStream(device->stream);
    free(device);
    return xz_pa_runtime_deinit();
}
