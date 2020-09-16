#ifndef wss_subprotocol_h
#define wss_subprotocol_h

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONTINUATION_FRAME = 0x0,
    TEXT_FRAME         = 0x1,
    BINARY_FRAME       = 0x2,
    CLOSE_FRAME        = 0x8,
    PING_FRAME         = 0x9,
    PONG_FRAME         = 0xA,
} wss_opcode_t;

/**
 * A function that the subprotocol can use to send a message to a client of the
 * server.
 */
typedef void (*WSS_send)(int fd, wss_opcode_t opcode, char *message, uint64_t message_length);
typedef void *(*WSS_malloc_t)(size_t size);
typedef void *(*WSS_realloc_t)(void *ptr, size_t size);
typedef void (*WSS_free_t)(void *ptr);

/**
 * The subprotocol API calls
 */
typedef void (*subAlloc)(WSS_malloc_t submalloc, WSS_realloc_t subrealloc, WSS_free_t subfree);
typedef void (*subInit)(char *config, WSS_send send);
typedef void (*subConnect)(int fd, char *ip, int port, char *path, char *cookies);
typedef void (*subMessage)(int fd, wss_opcode_t opcode, char *message, size_t message_length);
typedef void (*subWrite)(int fd, char *message, size_t message_length);
typedef void (*subClose)(int fd);
typedef void (*subDestroy)();

#ifdef __cplusplus
}
#endif

#endif
