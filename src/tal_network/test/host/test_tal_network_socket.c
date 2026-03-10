#include "unity.h"

#include "tal_network.h"
#include "mock_tkl_network.h"

void test_tal_net_socket_create(void)
{
    tkl_net_socket_create_ExpectAndReturn(PROTOCOL_TCP, 10);
    TEST_ASSERT_EQUAL(10, tal_net_socket_create(PROTOCOL_TCP));
}

void test_tal_net_connect(void)
{
    tkl_net_connect_ExpectAndReturn(10, 0x12345678, 80, OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_net_connect(10, 0x12345678, 80));
}

void test_tal_net_send_recv_close(void)
{
    char buf[] = "hello";
    tkl_net_send_ExpectAndReturn(10, buf, 5, 5);
    TEST_ASSERT_EQUAL(5, tal_net_send(10, buf, 5));

    tkl_net_recv_ExpectAndReturn(10, buf, 5, 5);
    TEST_ASSERT_EQUAL(5, tal_net_recv(10, buf, 5));

    tkl_net_close_ExpectAndReturn(10, OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_net_close(10));
}
