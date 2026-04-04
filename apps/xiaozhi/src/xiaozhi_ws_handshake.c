/**
 * @file xiaozhi_ws_handshake.c
 * @brief Pure helpers for websocket handshake request formatting.
 */

#include "xiaozhi_ws_handshake.h"

#include <stdio.h>
#include <string.h>

int xz_ws_format_handshake_request(char *out, size_t out_size, const char *path, const char *host, unsigned int port,
                                   const char *ws_key, int version, const char *device_id, const char *client_id,
                                   const char *token)
{
    char auth_line[384] = {0};

    if (!out || out_size == 0 || !path || !host || !ws_key || !device_id || !client_id) {
        return -1;
    }

    if (token && token[0] != '\0') {
        if (strchr(token, ' ') == NULL) {
            (void)snprintf(auth_line, sizeof(auth_line), "Authorization: Bearer %s\r\n", token);
        } else {
            (void)snprintf(auth_line, sizeof(auth_line), "Authorization: %s\r\n", token);
        }
    }

    return snprintf(out, out_size,
                    "GET %s HTTP/1.1\r\n"
                    "Host: %s:%u\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Key: %s\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "User-Agent: ESP32 Websocket Client\r\n"
                    "Protocol-Version: %d\r\n"
                    "Device-Id: %s\r\n"
                    "Client-Id: %s\r\n"
                    "%s"
                    "\r\n",
                    path, host, port, ws_key, version, device_id, client_id, auth_line);
}
