#include "unity.h"

#include <string.h>

#include "tal_network.h"
#include "tal_network_register.h"
#include "mock_tkl_network.h"

extern void test_tal_net_socket_create(void);
extern void test_tal_net_connect(void);
extern void test_tal_net_send_recv_close(void);
extern void test_tal_net_set_block(void);
extern void test_tal_net_set_timeout(void);
extern void test_tal_net_invalid_fd_returns_error(void);

void setUp(void)
{
    tal_network_card_init();
}

void tearDown(void)
{
}

TAL_NETWORK_CARD_T tal_network_card_posix = {
    .name = "mock-posix",
    .type = TAL_NET_TYPE_POSIX,
    .ipaddr = 0,
    .ops =
        {
            .get_errno = tkl_net_get_errno,
            .set_block = tkl_net_set_block,
            .close = tkl_net_close,
            .socket_create = tkl_net_socket_create,
            .connect = tkl_net_connect,
            .send = tkl_net_send,
            .recv = tkl_net_recv,
            .set_timeout = tkl_net_set_timeout,
        },
};

void test_tal_network_register_active_type(void)
{
    TEST_ASSERT_EQUAL(TAL_NET_TYPE_POSIX, tal_network_card_get_active_type());
    TEST_ASSERT_EQUAL(OPRT_OK, tal_network_card_set_active(TAL_NET_TYPE_POSIX));
    TEST_ASSERT_EQUAL(OPRT_INVALID_PARM, tal_network_card_set_active(TAL_NET_TYPE_MAX));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_tal_network_register_active_type);
    RUN_TEST(test_tal_net_socket_create);
    RUN_TEST(test_tal_net_connect);
    RUN_TEST(test_tal_net_send_recv_close);
    RUN_TEST(test_tal_net_set_block);
    RUN_TEST(test_tal_net_set_timeout);
    RUN_TEST(test_tal_net_invalid_fd_returns_error);

    return UNITY_END();
}
