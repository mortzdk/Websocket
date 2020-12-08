#ifndef wss_frame_h
#define wss_frame_h

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "extension.h"
#include "subprotocol.h"
#include "config.h"

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

typedef enum {
	CLOSE_NORMAL                 = 1000, /* The connection */
	CLOSE_SHUTDOWN               = 1001, /* Server is shutting down */
	CLOSE_PROTOCOL               = 1002, /* Some error in the protocol has happened */
	CLOSE_TYPE                   = 1003, /* The type (text, binary) was not supported */
	CLOSE_NO_STATUS_CODE         = 1005, /* No status code available */
	CLOSE_ABNORMAL               = 1006, /* Abnormal close */
	CLOSE_UTF8                   = 1007, /* The message wasn't in UTF8 */
	CLOSE_POLICY                 = 1008, /* The policy of the server has been broken */
	CLOSE_BIG                    = 1009, /* The messages received is too big */
	CLOSE_EXTENSION              = 1010, /* Mandatory extension missing */
	CLOSE_UNEXPECTED             = 1011, /* Unexpected happened */
	CLOSE_RESTARTING             = 1012, /* Service Restart */
	CLOSE_TRY_AGAIN              = 1013, /* Try Again Later */
	CLOSE_INVALID_PROXY_RESPONSE = 1014, /* Server acted as a gateway or proxy and received an invalid response from the upstream server. */
	CLOSE_FAILED_TLS_HANDSHAKE   = 1015  /* Unexpected TLS handshake failed */
} wss_close_t;

/**
 * Parses a payload of data into a websocket frame. Returns the frame and
 * corrects the offset pointer in order for multiple frames to be processed 
 * from the same payload.
 *
 * @param   payload         [char *]           "The payload to be processed"
 * @param   payload_length  [size_t]           "The length of the payload"
 * @param   offset          [size_t *]         "A pointer to an offset"
 * @return 		            [wss_frame_t *]    "A websocket frame"
 */
wss_frame_t *WSS_parse_frame(char *payload, size_t payload_length, uint64_t *offset);

/**
 * Converts a single frame into a char array.
 *
 * @param   frame    [wss_frame_t *]  "The frame"
 * @param   message  [char **]        "A pointer to a char array which should be filled with the frame data"
 * @return 		     [size_t]         "The size of the frame data"
 */
size_t WSS_stringify_frame(wss_frame_t *frame, char **message);

/**
 * Converts an array of frames into a char array that can be written to others.
 *
 * @param   frames   [wss_frame_t **]  "The frames to be converted"
 * @param   size     [size_t]           "The amount of frames"
 * @param   message  [char **]          "A pointer to a char array which should be filled with the frame data"
 * @return 		     [size_t]           "The size of the frame data"
 */
size_t WSS_stringify_frames(wss_frame_t **frames, size_t size, char **message);

/**
 * Creates a series of frames from a message.
 *
 * @param   config          [wss_config_t *]   "The server configuration"
 * @param   opcode          [wss_opcode_t]     "The opcode that the frames should be"
 * @param   message         [char *]           "The message to be converted into frames"
 * @param   message_length  [size_t]           "The length of the message"
 * @param   frames          [wss_frame_t ***]  "The frames created from the message"
 * @return 		            [size_t]           "The amount of frames created"
 */
size_t WSS_create_frames(wss_config_t *config, wss_opcode_t opcode, char *message, size_t message_length, wss_frame_t ***frames);

/**
 * Creates a closing frame given a reason for the closure.
 *
 * @param   reason   [wss_close_t]      "The reason for the closure"
 * @return 		     [wss_frame_t *]    "A websocket frame"
 */
wss_frame_t *WSS_closing_frame(wss_close_t reason, char *message);

/**
 * Creates a ping frame.
 *
 * @return 		     [wss_frame_t *]    "A websocket frame"
 */
wss_frame_t *WSS_ping_frame();

/**
 * Creates a pong frame from a received ping frame.
 *
 * @param   ping     [wss_frame_t *]    "A ping frame"
 * @return 		     [wss_frame_t *]    "A websocket frame"
 */
wss_frame_t *WSS_pong_frame(wss_frame_t *ping);

/**
 * Releases memory used by a frame.
 *
 * @param   ping     [wss_frame_t *]    "The frame that should be freed"
 * @return 		     [void]         
 */
void WSS_free_frame(wss_frame_t *frame);

#endif
