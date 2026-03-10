#include "unity.h"

#include "tal_network.h"
#include "mock_tkl_network.h"

void test_tal_net_set_block(void)
{
    tkl_net_set_block_ExpectAndReturn(10, TRUE, OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_net_set_block(10, TRUE));
}

void test_tal_net_set_timeout(void)
{
    tkl_net_set_timeout_ExpectAndReturn(10, 1000, TRANS_RECV, OPRT_OK);
    TEST_ASSERT_EQUAL(OPRT_OK, tal_net_set_timeout(10, 1000, TRANS_RECV));
}

void test_tal_net_invalid_fd_returns_error(void)
{
    TEST_ASSERT_EQUAL(-3001, tal_net_set_block(-1, TRUE));
    TEST_ASSERT_EQUAL(-3001, tal_net_close(-1));
}
