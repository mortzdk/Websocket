#ifndef wss_extension_h
#define wss_extension_h

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-------+-+-------------+-------------------------------+
 *   |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 *   |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 *   |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 *   | |1|2|3|       |K|             |                               |
 *   +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 *   |     Extended payload length continued, if payload len == 127  |
 *   + - - - - - - - - - - - - - - - +-------------------------------+
 *   |                               |Masking-key, if MASK set to 1  |
 *   +-------------------------------+-------------------------------+
 *   | Masking-key (continued)       |          Payload Data         |
 *   +-------------------------------- - - - - - - - - - - - - - - - +
 *   :                     Payload Data continued ...                :
 *   + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 *   |                     Payload Data continued ...                |
 *   +---------------------------------------------------------------+
 */

typedef struct {
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;
    uint8_t opcode;
    bool mask;
    uint64_t payloadLength;
    char maskingKey[4];
    char *payload;
    uint64_t extensionDataLength;
    uint64_t applicationDataLength;
} wss_frame_t;

typedef void *(*WSS_malloc_t)(size_t size);
typedef void *(*WSS_realloc_t)(void *ptr, size_t size);
typedef void (*WSS_free_t)(void *ptr);

/**
 * The extension API calls
 */
typedef void (*extAlloc)(WSS_malloc_t extmalloc, WSS_realloc_t extrealloc, WSS_free_t extfree);
typedef void (*extInit)(char *config);
typedef void (*extOpen)(int fd, char *param, char **accepted, bool *valid);
typedef void (*extInFrame)(int fd, wss_frame_t *frame);
typedef void (*extInFrames)(int fd, wss_frame_t **frames, size_t len);
typedef void (*extOutFrame)(int fd, wss_frame_t *frame);
typedef void (*extOutFrames)(int fd, wss_frame_t **frames, size_t len);
typedef void (*extClose)(int fd);
typedef void (*extDestroy)();

#ifdef __cplusplus
}
#endif

#endif
