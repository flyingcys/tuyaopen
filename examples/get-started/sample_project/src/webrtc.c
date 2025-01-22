#include "stdio.h"

#include "peer_connection.h"
#include "platform.h"
#include "media.h"

#define TICK_INTERVAL 15

static PeerConnection *peer_connection = NULL;

static void oai_onconnectionstatechange_task(PeerConnectionState state,
                                             void *user_data)
{
  printf("PeerConnectionState: %s\n",
           peer_connection_state_to_string(state));

  if (state == PEER_CONNECTION_DISCONNECTED ||
      state == PEER_CONNECTION_CLOSED) {
    oai_platform_restart();
  } else if (state == PEER_CONNECTION_CONNECTED) {
    oai_init_audio_encoder();
    oai_platform_send_audio_task(peer_connection);
  }
}

char local_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
static void oai_on_icecandidate_task(char *description, void *user_data) {
  oai_http_request(description, local_buffer);
  peer_connection_set_remote_description(peer_connection, local_buffer);
}

void oai_webrtc() {
  PeerConfiguration peer_connection_config = {
      .ice_servers = {},
      .audio_codec = CODEC_OPUS,
      .video_codec = CODEC_NONE,
      .datachannel = DATA_CHANNEL_NONE,
      .onaudiotrack = oai_audio_decode,
      .onvideotrack = NULL,
      .on_request_keyframe = NULL,
      .user_data = NULL,
  };

  peer_connection = peer_connection_create(&peer_connection_config);
  if (peer_connection == NULL) {
    printf("Failed to create peer connection\n");
    oai_platform_restart();
  }

  peer_connection_oniceconnectionstatechange(peer_connection,
                                             oai_onconnectionstatechange_task);
  peer_connection_onicecandidate(peer_connection, oai_on_icecandidate_task);
  peer_connection_create_offer(peer_connection);

  while (1) {
    peer_connection_loop(peer_connection);
    // vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL));
    tal_system_sleep(TICK_INTERVAL);

  }
}
