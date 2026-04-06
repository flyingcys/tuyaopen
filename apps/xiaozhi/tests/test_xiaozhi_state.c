#include <assert.h>
#include <stdio.h>

#include "xiaozhi_state.h"

extern int xz_state_accepts_tts_binary(xz_chat_state_t current);

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

static void test_speaking_to_listening_on_tts_stop_auto_mode(void)
{
    xz_chat_state_t next = xz_state_after_tts_stop_mode(XZ_CHAT_SPEAKING, 1);
    assert(next == XZ_CHAT_LISTENING);
}

static void test_tts_stop_keeps_listening_state(void)
{
    xz_chat_state_t next = xz_state_after_tts_stop(XZ_CHAT_LISTENING);
    assert(next == XZ_CHAT_LISTENING);
}

static void test_speaking_to_idle_on_abort(void)
{
    xz_chat_state_t next = xz_state_after_abort(XZ_CHAT_SPEAKING);
    assert(next == XZ_CHAT_IDLE);
}

static void test_listening_to_idle_on_abort(void)
{
    xz_chat_state_t next = xz_state_after_abort(XZ_CHAT_LISTENING);
    assert(next == XZ_CHAT_IDLE);
}

static void test_tts_binary_gating_follows_chat_state(void)
{
    assert(xz_state_accepts_tts_binary(XZ_CHAT_SPEAKING) == 1);
    assert(xz_state_accepts_tts_binary(XZ_CHAT_IDLE) == 0);
    assert(xz_state_accepts_tts_binary(XZ_CHAT_LISTENING) == 0);
    assert(xz_state_accepts_tts_binary(xz_state_after_tts_stop(XZ_CHAT_SPEAKING)) == 0);
}

int main(void)
{
    test_idle_to_listening();
    test_listening_to_speaking();
    test_speaking_to_idle_on_tts_stop();
    test_speaking_to_listening_on_tts_stop_auto_mode();
    test_tts_stop_keeps_listening_state();
    test_speaking_to_idle_on_abort();
    test_listening_to_idle_on_abort();
    test_tts_binary_gating_follows_chat_state();

    puts("xiaozhi_state tests passed");
    return 0;
}
