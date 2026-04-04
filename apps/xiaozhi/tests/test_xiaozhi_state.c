#include <assert.h>
#include <stdio.h>

#include "xiaozhi_state.h"

static void test_idle_to_listening(void)
{
    xz_chat_state_t next = xz_state_after_listen_start(XZ_CHAT_IDLE);
    assert(next == XZ_CHAT_LISTENING);
}

static void test_listening_to_speaking(void)
{
    xz_chat_state_t next = xz_state_after_tts_start(XZ_CHAT_LISTENING);
    assert(next == XZ_CHAT_SPEAKING);
}

static void test_speaking_to_idle_on_tts_stop(void)
{
    xz_chat_state_t next = xz_state_after_tts_stop(XZ_CHAT_SPEAKING);
    assert(next == XZ_CHAT_IDLE);
}

static void test_speaking_to_idle_on_abort(void)
{
    xz_chat_state_t next = xz_state_after_abort(XZ_CHAT_SPEAKING);
    assert(next == XZ_CHAT_IDLE);
}

int main(void)
{
    test_idle_to_listening();
    test_listening_to_speaking();
    test_speaking_to_idle_on_tts_stop();
    test_speaking_to_idle_on_abort();

    puts("xiaozhi_state tests passed");
    return 0;
}
