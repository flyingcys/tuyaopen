#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <stdlib.h>
#include "tal_log.h"

#include "peer_connection.h"

#define kCaptureSampleRate    8000
#define kCaptureChannelCount  1
#define kPlaybackSampleRate   8000
#define kPlaybackChannelCount 2

#define MAX_HTTP_OUTPUT_BUFFER 8192

void oai_platform_init(void);
void oai_platform_restart(void);
void oai_platform_init_audio_capture(void);
void oai_platform_audio_write(char *output_buffer, size_t output_buffer_size,
                              size_t *bytes_written);
void oai_platform_audio_read(char *input_buffer, size_t input_buffer_size,
                             size_t *bytes_read);
void oai_platform_send_audio_task(PeerConnection *peer_connection);

#endif