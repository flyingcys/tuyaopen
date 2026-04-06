#ifndef XZ_SNOWBOY_DETECT_C_WRAPPER_H
#define XZ_SNOWBOY_DETECT_C_WRAPPER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SnowboyDetect SnowboyDetect;

SnowboyDetect *SnowboyDetectConstructor(const char *resource_filename, const char *model_str);
bool           SnowboyDetectReset(SnowboyDetect *detector);
int            SnowboyDetectRunDetection(SnowboyDetect *detector, const int16_t *data, int array_length, bool is_end);
void           SnowboyDetectSetSensitivity(SnowboyDetect *detector, const char *sensitivity_str);
void           SnowboyDetectSetAudioGain(SnowboyDetect *detector, float audio_gain);
void           SnowboyDetectUpdateModel(SnowboyDetect *detector);
void           SnowboyDetectApplyFrontend(SnowboyDetect *detector, bool apply_frontend);
int            SnowboyDetectNumHotwords(SnowboyDetect *detector);
int            SnowboyDetectSampleRate(SnowboyDetect *detector);
int            SnowboyDetectNumChannels(SnowboyDetect *detector);
int            SnowboyDetectBitsPerSample(SnowboyDetect *detector);
void           SnowboyDetectDestructor(SnowboyDetect *detector);

#ifdef __cplusplus
}
#endif

#endif
