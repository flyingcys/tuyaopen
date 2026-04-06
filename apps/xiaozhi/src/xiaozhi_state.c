#include "xiaozhi_state.h"

xz_chat_state_t xz_state_after_listen_start(xz_chat_state_t current)
{
    (void)current;
    return XZ_CHAT_LISTENING;
}

xz_chat_state_t xz_state_after_tts_start(xz_chat_state_t current)
{
    (void)current;
    return XZ_CHAT_SPEAKING;
}

xz_chat_state_t xz_state_after_tts_stop(xz_chat_state_t current)
{
    return xz_state_after_tts_stop_mode(current, 0);
}

xz_chat_state_t xz_state_after_tts_stop_mode(xz_chat_state_t current, int continue_listening)
{
    if (current != XZ_CHAT_SPEAKING) {
        return current;
    }

    return continue_listening ? XZ_CHAT_LISTENING : XZ_CHAT_IDLE;
}

xz_chat_state_t xz_state_after_abort(xz_chat_state_t current)
{
    if (current == XZ_CHAT_SPEAKING || current == XZ_CHAT_LISTENING) {
        return XZ_CHAT_IDLE;
    }
    return current;
}

int xz_state_accepts_tts_binary(xz_chat_state_t current)
{
    return (current == XZ_CHAT_SPEAKING) ? 1 : 0;
}
