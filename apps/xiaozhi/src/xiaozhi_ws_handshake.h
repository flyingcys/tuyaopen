/**
 * @file xiaozhi_ws_handshake.h
 * @brief Pure helpers for websocket handshake request formatting.
 */

#ifndef __XIAOZHI_WS_HANDSHAKE_H__
#define __XIAOZHI_WS_HANDSHAKE_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int xz_ws_format_handshake_request(char *out, size_t out_size, const char *path, const char *host, unsigned int port,
                                   const char *ws_key, int version, const char *device_id, const char *client_id,
                                   const char *token);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_WS_HANDSHAKE_H__ */
