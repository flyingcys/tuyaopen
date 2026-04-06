#ifndef XZ_SNOWBOY_RUNNER_H
#define XZ_SNOWBOY_RUNNER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*xz_snowboy_detected_cb_t)(void *userdata);

typedef struct xz_snowboy_runner xz_snowboy_runner_t;

int  xz_snowboy_runner_create(xz_snowboy_runner_t **out, const char *resource_path, const char *model_path,
                              xz_snowboy_detected_cb_t cb, void *userdata);
int  xz_snowboy_runner_feed_pcm(xz_snowboy_runner_t *runner, const int16_t *pcm, size_t samples);
int  xz_snowboy_runner_reset(xz_snowboy_runner_t *runner);
void xz_snowboy_runner_destroy(xz_snowboy_runner_t *runner);

#ifdef __cplusplus
}
#endif

#endif
