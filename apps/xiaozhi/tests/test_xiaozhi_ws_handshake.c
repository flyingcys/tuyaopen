#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "xiaozhi_ws_handshake.h"

static void test_format_handshake_uses_real_crlf(void)
{
    char req[1400] = {0};
    int  n = xz_ws_format_handshake_request(req, sizeof(req), "/xiaozhi/v1/", "api.tenclass.net", 443, "abc123", 1,
                                            "aa:bb:cc:dd:ee:ff", "client-id", "test-token");

    assert(n > 0);
    assert(strstr(req, "GET /xiaozhi/v1/ HTTP/1.1\r\n") != NULL);
    assert(strstr(req, "Authorization: Bearer test-token\r\n") != NULL);
    assert(strstr(req, "\r\n\r\n") != NULL);
    assert(strstr(req, "\\r\\n") == NULL);
}

int main(void)
{
    test_format_handshake_uses_real_crlf();
    puts("test_xiaozhi_ws_handshake: PASS");
    return 0;
}
