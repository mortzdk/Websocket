#ifndef wss_frame_h
#define wss_frame_h

#include <stdbool.h>
#include <stdint.h>

#include "header.h"

typedef enum {
    CONTINUATION_FRAME     = 0,
    TEXT_FRAME             = 1,
    BINARY_FRAME           = 2,
    CONNECTION_CLOSE_FRAME = 8,
    PING_FRAME             = 9,
    POING_FRAME            = 10,
} wss_opcode_t;

typedef enum {
	CLOSE_NORMAL             = 1000, /* The connection */
	CLOSE_SHUTDOWN           = 1001, /* Server is shutting down */
	CLOSE_PROTOCOL           = 1002, /* Some error in the protocol has happened */
	CLOSE_TYPE               = 1003, /* The type (text, binary) was not supported */
	NO_STATUS_CODE           = 1005, /* No status code available */
	ABNORMAL_CLOSE           = 1006, /* Abnormal close */
	CLOSE_UTF8               = 1007, /* The message wasn't in UTF8 */
	CLOSE_POLICY             = 1008, /* The policy of the server has been broken */
	CLOSE_BIG                = 1009, /* The messages received is too big */
	CLOSE_EXTENSION          = 1010, /* Mandatory extension missing */
	CLOSE_UNEXPECTED         = 1011, /* Unexpected happened */
	RESTARTING               = 1012, /* Service Restart */
	TRY_AGAIN                = 1013, /* Try Again Later */
	INVALID_PROXY_RESPONSE   = 1014, /* Server acted as a gateway or proxy and received an invalid response from the upstream server. */
	FAILED_TLS_HANDSHAKE     = 1015  /* Unexpected TLS handshake failed */
} wss_close_t;

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
    char *extensionData;
    uint64_t extensionDataLength;
    char *applicationData;
    uint64_t applicationDataLength;
} frame_t;

/**
 * Parses a payload of data into a websocket frame. Returns the frame and
 * corrects the offset pointer in order for multiple frames to be processed 
 * from the same payload.
 *
 * @param   header          [header_t *]   "A HTTP header structure of the session"
 * @param   payload         [char *]       "The payload to be processed"
 * @param   payload_length  [size_t]       "The length of the payload"
 * @param   offset          [size_t *]     "A pointer to an offset"
 * @return 		            [frame_t *]    "A websocket frame"
 */
frame_t *WSS_parse_frame(header_t *header, char *payload, size_t payload_length, uint64_t *offset);

/**
 * Converts a single frame into a char array.
 *
 * @param   frame    [frame_t *]  "The frame"
 * @param   message  [char **]    "A pointer to a char array which should be filled with the frame data"
 * @return 		     [size_t]     "The size of the frame data"
 */
size_t WSS_stringify_frame(frame_t *frame, char **message);

/**
 * Converts an array of frames into a char array that can be written to others.
 *
 * @param   frames   [frames_t **]  "The frames to be converted"
 * @param   size     [size_t]       "The amount of frames"
 * @param   message  [char **]      "A pointer to a char array which should be filled with the frame data"
 * @return 		     [size_t]       "The size of the frame data"
 */
size_t WSS_stringify_frames(frame_t **frames, size_t size, char **message);

/**
 * Creates a series of frames from a message.
 *
 * @param   header   [header_t *]   "A HTTP header structure of the session"
 * @param   message  [char *]       "The message to be converted into frames"
 * @param   frames   [frame_t **]   "The frames created from the message"
 * @return 		     [size_t]       "The amount of frames created"
 */
size_t WSS_create_frames(header_t *header, size_t buffer_size, char *message, frame_t ***frames);

/**
 * Creates a closing frame given a reason for the closure.
 *
 * @param   header   [header_t *]   "A HTTP header structure of the session"
 * @param   reason   [wss_close_t]  "The reason for the closure"
 * @return 		     [frame_t *]    "A websocket frame"
 */
frame_t *WSS_closing_frame(header_t *header, wss_close_t reason);

/**
 * Creates a ping frame.
 *
 * @param   header   [header_t *]   "A HTTP header structure of the session"
 * @return 		     [frame_t *]    "A websocket frame"
 */
frame_t *WSS_ping_frame(header_t *header);

/**
 * Creates a pong frame from a received ping frame.
 *
 * @param   header   [header_t *]   "A HTTP header structure of the session"
 * @param   ping     [frame_t *]    "A ping frame"
 * @return 		     [frame_t *]    "A websocket frame"
 */
frame_t *WSS_pong_frame(header_t *header, frame_t *ping);

/**
 * Releases memory used by a frame.
 *
 * @param   ping     [frame_t *]    "The frame that should be freed"
 * @return 		     [void]         
 */
void WSS_free_frame(frame_t *frame);

#endif
