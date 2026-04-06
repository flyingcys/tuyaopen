#include "xz_snowboy_runner.h"

#include "../third_party/snowboy_wrapper/snowboy-detect-c-wrapper.h"

#include <new>

struct xz_snowboy_runner {
    SnowboyDetect            *detector;
    xz_snowboy_detected_cb_t  cb;
    void                     *userdata;
};

int xz_snowboy_runner_create(xz_snowboy_runner_t **out, const char *resource_path, const char *model_path,
                             xz_snowboy_detected_cb_t cb, void *userdata)
{
    xz_snowboy_runner_t *runner;

    if (!out || !resource_path || !model_path) {
        return -1;
    }

    runner = new (std::nothrow) xz_snowboy_runner_t();
    if (!runner) {
        return -1;
    }

    runner->detector = SnowboyDetectConstructor(resource_path, model_path);
    if (!runner->detector) {
        delete runner;
        return -1;
    }

    SnowboyDetectSetSensitivity(runner->detector, "0.5");
    SnowboyDetectSetAudioGain(runner->detector, 1.0f);
    SnowboyDetectApplyFrontend(runner->detector, false);

    runner->cb       = cb;
    runner->userdata = userdata;
    *out             = runner;
    return 0;
}

int xz_snowboy_runner_feed_pcm(xz_snowboy_runner_t *runner, const int16_t *pcm, size_t samples)
{
    int result;

    if (!runner || !runner->detector || !pcm || samples == 0) {
        return -1;
    }

    result = SnowboyDetectRunDetection(runner->detector, pcm, (int)samples, false);
    if (result > 0 && runner->cb) {
        runner->cb(runner->userdata);
    }
    return result;
}

int xz_snowboy_runner_reset(xz_snowboy_runner_t *runner)
{
    if (!runner || !runner->detector) {
        return -1;
    }

    return SnowboyDetectReset(runner->detector) ? 0 : -1;
}

void xz_snowboy_runner_destroy(xz_snowboy_runner_t *runner)
{
    if (!runner) {
        return;
    }

    if (runner->detector) {
        SnowboyDetectDestructor(runner->detector);
        runner->detector = NULL;
    }

    delete runner;
}
