#ifndef wss_message_h
#define wss_message_h

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "subprotocol.h"
#include "extension.h"

/**
 * Structure containing a message that should be sent to a client
 */
typedef struct {
    size_t length;
    char *msg;
    bool framed;
} wss_message_t;

void WSS_message_send_frames(void *server, void *session, wss_frame_t **frames, size_t frames_count);

void WSS_message_send(int fd, wss_opcode_t opcode, char *message, uint64_t message_length);

void WSS_message_free(wss_message_t *msg);

#endif
