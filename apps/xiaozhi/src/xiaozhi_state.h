#ifndef XIAOZHI_STATE_H
#define XIAOZHI_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    XZ_CHAT_IDLE = 0,
    XZ_CHAT_CONNECTING,
    XZ_CHAT_LISTENING,
    XZ_CHAT_SPEAKING,
} xz_chat_state_t;

xz_chat_state_t xz_state_after_listen_start(xz_chat_state_t current);
xz_chat_state_t xz_state_after_tts_start(xz_chat_state_t current);
xz_chat_state_t xz_state_after_tts_stop(xz_chat_state_t current);
xz_chat_state_t xz_state_after_tts_stop_mode(xz_chat_state_t current, int continue_listening);
xz_chat_state_t xz_state_after_abort(xz_chat_state_t current);

#ifdef __cplusplus
}
#endif

#endif
