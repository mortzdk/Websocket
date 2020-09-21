#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <math.h>

#include "frame.h"
#include "str.h"
#include "alloc.h"
#include "log.h"
#include "predict.h"

#if defined(_MSC_VER)
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
/* GCC-compatible compiler, targeting x86/x86-64 */
#include <x86intrin.h>
#elif defined(__GNUC__) && defined(__ARM_NEON__)
/* GCC-compatible compiler, targeting ARM with NEON */
#include <arm_neon.h>
#elif defined(__GNUC__) && defined(__IWMMXT__)
/* GCC-compatible compiler, targeting ARM with WMMX */
#include <mmintrin.h>
#elif (defined(__GNUC__) || defined(__xlC__)) &&                               \
    (defined(__VEC__) || defined(__ALTIVEC__))
/* XLC or GCC-compatible compiler, targeting PowerPC with VMX/VSX */
#include <altivec.h>
#elif defined(__GNUC__) && defined(__SPE__)
/* GCC-compatible compiler, targeting PowerPC with SPE */
#include <spe.h>
#endif

/**
 * Converts the unsigned 64 bit integer from host byte order to network byte
 * order.
 *
 * @param   length [size_t]   "The length of the random byte string"
 * @return 		   [char *]   "Random byte string of the given length"
 */
static char *random_bytes(int length) {
    char *res = WSS_malloc(length);
    /* Seed number for rand() */
    srand((unsigned int) time(0) + getpid());

    while (likely(length--)) {
        res[length] = rand();
        srand(rand());
    }

    return res;
}

/**
 * Converts the unsigned 64 bit integer from host byte order to network byte
 * order.
 *
 * @param   value  [uint64_t]   "A 64 bit unsigned integer"
 * @return 		   [uint64_t]   "The 64 bit unsigned integer in network byte order"
 */
static inline uint64_t htonl64(uint64_t value) {
	static const int num = 42;

	/**
	 * If this check is true, the system is using the little endian
	 * convention. Else the system is using the big endian convention, which
	 * means that we do not have to represent our integers in another way.
	 */
	if (*(char *)&num == 42) {
		const uint32_t high = (uint32_t)(value >> 32);
		const uint32_t low = (uint32_t)(value & 0xFFFFFFFF);

		return (((uint64_t)(htonl(low))) << 32) | htonl(high);
	} else {
		return value;
	}
}

/**
 * Converts the unsigned 16 bit integer from host byte order to network byte
 * order.
 *
 * @param   value  [uint64_t]   "A 16 bit unsigned integer"
 * @return 		   [uint64_t]   "The 16 bit unsigned integer in network byte order"
 */
static inline uint64_t htons16(uint64_t value) {
	static const int num = 42;

	/**
	 * If this check is true, the system is using the little endian
	 * convention. Else the system is using the big endian convention, which
	 * means that we do not have to represent our integers in another way.
	 */
	if (*(char *)&num == 42) {
        return ntohs(value);
	} else {
		return value;
	}
}

static void unmask(wss_frame_t *frame) {
    char *applicationData = frame->payload+frame->extensionDataLength;
#if defined(__AVX512F__) && false
    uint64_t i = 0;
    __m512i masked_data;
    int mask = (frame->maskingKey[3] << 24) | (frame->maskingKey[2] << 16) | (frame->maskingKey[1] << 8) | frame->maskingKey[0];
    __m512i maskingKey = _mm512_setr_epi32(
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask,
            mask
            );

    if ( frame->applicationDataLength > 64 ) {
        for (; likely(i <= frame->applicationDataLength - 64); i += 64) {
            masked_data = _mm512_loadu_si512((const __m512i *)(applicationData+i));
            _mm512_storeu_si512((__m512i *)(applicationData+i), _mm512_xor_si512 (masked_data, maskingKey));
        }
    }

    // last part
    if ( likely(i < frame->applicationDataLength) ) {
        char buffer[64];
        memset(buffer, '\0', 64);
        memcpy(buffer, applicationData + i, frame->applicationDataLength - i);
        masked_data = _mm512_loadu_si512((const __m512i *)buffer);
        _mm512_storeu_si512((__m512i *)buffer, _mm512_xor_si512 (masked_data, maskingKey));
        memcpy(applicationData + i, buffer, (frame->applicationDataLength - i));
    }
#elif defined(__AVX2__) && defined(__AVX__)
    uint64_t i = 0;
    __m256i masked_data;
    __m256i maskingKey = _mm256_setr_epi8(
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3]
                );

    if ( frame->applicationDataLength > 32 ) {
        for (; likely(i <= frame->applicationDataLength - 32); i += 32) {
            masked_data = _mm256_loadu_si256((const __m256i *)(applicationData+i));
            _mm256_storeu_si256((__m256i *)(applicationData+i), _mm256_xor_si256 (masked_data, maskingKey));
        }
    }

    // last part
    if ( likely(i < frame->applicationDataLength) ) {
        char buffer[32];
        memset(buffer, '\0', 32);
        memcpy(buffer, applicationData + i, frame->applicationDataLength - i);
        masked_data = _mm256_loadu_si256((const __m256i *)buffer);
        _mm256_storeu_si256((__m256i *)buffer, _mm256_xor_si256 (masked_data, maskingKey));
        memcpy(applicationData + i, buffer, (frame->applicationDataLength - i));
    }
#elif defined(__SSE2__)
    uint64_t i = 0;
    __m128i masked_data;
    __m128i maskingKey = _mm_setr_epi8(
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3],
            frame->maskingKey[0],
            frame->maskingKey[1],
            frame->maskingKey[2],
            frame->maskingKey[3]
            );

    if ( frame->applicationDataLength > 16 ) {
        for (; likely(i <= frame->applicationDataLength - 16); i += 16) {
            masked_data = _mm_loadu_si128((const __m128i *)(applicationData+i));
            _mm_storeu_si128((__m128i *)(applicationData+i), _mm_xor_si128 (masked_data, maskingKey));
        }
    }

    if ( likely(i < frame->applicationDataLength) ) {
        char buffer[16];
        memset(buffer, '\0', 16);
        memcpy(buffer, applicationData + i, frame->applicationDataLength - i);
        masked_data = _mm_loadu_si128((const __m128i *)buffer);
        _mm_storeu_si128((__m128i *)buffer, _mm_xor_si128 (masked_data, maskingKey));
        memcpy(applicationData + i, buffer, (frame->applicationDataLength - i));
    }
#else
    uint64_t i, j;
    for (i = 0, j = 0; likely(i < frame->applicationDataLength); i++, j++){
        applicationData[j] = applicationData[i] ^ frame->maskingKey[j % 4];
    }
#endif
}

/**
 * Parses a payload of data into a websocket frame. Returns the frame and
 * corrects the offset pointer in order for multiple frames to be processed 
 * from the same payload.
 *
 * @param   payload [char *]           "The payload to be processed"
 * @param   length  [size_t]           "The length of the payload"
 * @param   offset  [size_t *]         "A pointer to an offset"
 * @return 		    [wss_frame_t *]    "A websocket frame"
 */
wss_frame_t *WSS_parse_frame(char *payload, size_t length, size_t *offset) {
    wss_frame_t *frame;

    if ( unlikely(NULL == payload) ) {
        WSS_log_error("Payload cannot be NULL");
        return NULL;
    }

    if ( unlikely(NULL == (frame = WSS_malloc(sizeof(wss_frame_t)))) ) {
        WSS_log_error("Unable to allocate frame");
        return NULL;
    }

    WSS_log_trace("Parsing frame starting from offset %lu", *offset);

    frame->mask = false;
    frame->payloadLength = 0;
    frame->applicationDataLength = 0;
    frame->extensionDataLength = 0;

    frame->fin    = 0x80 & payload[*offset];
    frame->rsv1   = 0x40 & payload[*offset];
    frame->rsv2   = 0x20 & payload[*offset];
    frame->rsv3   = 0x10 & payload[*offset];
    frame->opcode = 0x0F & payload[*offset];

    *offset += 1;

    if ( likely(*offset < length) ) {
        frame->mask          = 0x80 & payload[*offset];
        frame->payloadLength = 0x7F & payload[*offset];
    }
    *offset += 1;

    switch (frame->payloadLength) { 
        case 126:
            if ( likely(*offset+sizeof(uint16_t) <= length) ) {
                memcpy(&frame->payloadLength, payload+*offset, sizeof(uint16_t));
                frame->payloadLength = htons16(frame->payloadLength);
            }
            *offset += sizeof(uint16_t);
            break;
        case 127:
            if ( likely(*offset+sizeof(uint64_t) <= length) ) {
                memcpy(&frame->payloadLength, payload+*offset, sizeof(uint64_t));
                frame->payloadLength = htonl64(frame->payloadLength);
            }
            *offset += sizeof(uint64_t);
            break;
    }

    if ( likely(frame->mask) ) {
        if ( likely(*offset+sizeof(uint32_t) <= length) ) {
            memcpy(frame->maskingKey, payload+*offset, sizeof(uint32_t));
        }
        *offset += sizeof(uint32_t);
    }

    frame->applicationDataLength = frame->payloadLength-frame->extensionDataLength;
    if ( likely(frame->applicationDataLength > 0) ) {
        if ( likely(*offset+frame->applicationDataLength <= length) ) {
            if ( unlikely(NULL == (frame->payload = WSS_malloc(frame->applicationDataLength))) ) {
                WSS_log_error("Unable to allocate frame application data");
                return NULL;
            }

            memcpy(frame->payload, payload+*offset, frame->applicationDataLength);
        }
        *offset += frame->applicationDataLength;
    }

    if ( likely(frame->mask && *offset <= length) ) {
        unmask(frame);
    }

    return frame;
}

/**
 * Converts a single frame into a char array.
 *
 * @param   frame    [wss_frame_t *]  "The frame"
 * @param   message  [char **]        "A pointer to a char array which should be filled with the frame data"
 * @return 		     [size_t]         "The size of the frame data"
 */
size_t WSS_stringify_frame(wss_frame_t *frame, char **message) {
    size_t offset = 0;
    size_t len = 2;
    char *mes;

    if ( unlikely(NULL == frame) ) {
        *message = NULL;
        return 0;
    }

    WSS_log_trace("Creating byte message from frame");

    if ( likely(frame->payloadLength > 125) ) {
        if ( likely(frame->payloadLength <= 65535) ) {
            len += sizeof(uint16_t);
        } else {
            len += sizeof(uint64_t);
        }
    }

    len += frame->payloadLength;

    if ( unlikely(NULL == (mes = WSS_malloc(len*sizeof(char)))) ) {
        WSS_log_error("Unable to allocate return message");
        *message = NULL;
        return 0;
    }

    if (frame->fin) {
        mes[offset] |= 0x80;
    }

    if (frame->rsv1) {
        mes[offset] |= 0x40;
    }

    if ( unlikely(frame->rsv2) ) {
        mes[offset] |= 0x20;
    }

    if ( unlikely(frame->rsv3) ) {
        mes[offset] |= 0x10;
    }

    mes[offset++] |= 0xF & frame->opcode;

    if ( unlikely(frame->payloadLength <= 125) ) {
        mes[offset++] = frame->payloadLength;
    } else if ( likely(frame->payloadLength <= 65535) ) {
        uint16_t plen;
        mes[offset++] = 126;
        plen = htons16(frame->payloadLength);
        memcpy(mes+offset, &plen, sizeof(plen));
        offset += sizeof(plen);
    } else {
        uint64_t plen;
        mes[offset++] = 127;
        plen = htonl64(frame->payloadLength);
        memcpy(mes+offset, &plen, sizeof(plen));
        offset += sizeof(plen);
    }

    if ( unlikely(frame->extensionDataLength > 0) ) {
        memcpy(mes+offset, frame->payload, frame->extensionDataLength);
        offset += frame->extensionDataLength;
    }

    if ( likely(frame->applicationDataLength > 0) ) {
        memcpy(mes+offset, frame->payload+frame->extensionDataLength, frame->applicationDataLength);
        offset += frame->applicationDataLength;
    }

    *message = mes;

    return offset;
}

/**
 * Converts an array of frames into a char array that can be written to others.
 *
 * @param   frames   [wss_frame_t **]  "The frames to be converted"
 * @param   size     [size_t]          "The amount of frames"
 * @param   message  [char **]         "A pointer to a char array which should be filled with the frame data"
 * @return 		     [size_t]          "The size of the frame data"
 */
size_t WSS_stringify_frames(wss_frame_t **frames, size_t size, char **message) {
    size_t i, n;
    char *f;
    char *msg = NULL;
    size_t message_length = 0;

    WSS_log_trace("Creating byte message from frames");

    for (i = 0; likely(i < size); i++) {
        n = WSS_stringify_frame(frames[i], &f);

        // If we receive less than two bytes, we did not receive a valid frame
        if ( unlikely(n < 2) ) {
            WSS_log_error("Received invalid frame");
            *message = NULL;
            WSS_free((void **)&f);
            WSS_free((void **)&msg);
            return 0;
        }

        if ( unlikely(NULL == (msg = WSS_realloc((void **) &msg, 
                        message_length*sizeof(char), (message_length+n+1)*sizeof(char)))) ) {
            WSS_log_error("Unable to allocate message string");
            *message = NULL;
            WSS_free((void **)&f);
            WSS_free((void **)&msg);
            return 0;
        }

        memcpy(msg+message_length, f, n);
        message_length += n;

        WSS_free((void **) &f);
    }

    *message = msg;

    return message_length;
}

/**
 * Creates a series of frames from a message.
 *
 * @param   config          [wss_config_t *]   "The server configuration"
 * @param   opcode          [wss_opcode_t]     "The opcode that the frames should be"
 * @param   message         [char *]           "The message to be converted into frames"
 * @param   message_length  [size_t]           "The length of the message"
 * @param   fs              [wss_frame_t ***]  "The frames created from the message"
 * @return 		            [size_t]           "The amount of frames created"
 */
size_t WSS_create_frames(wss_config_t *config, wss_opcode_t opcode, char *message, size_t message_length, wss_frame_t ***fs) {
    size_t i, j, offset = 0;
    wss_frame_t *frame;
    size_t frames_count;
    wss_frame_t **frames;
    wss_close_t code;
    char *msg = message;

    if ( unlikely(NULL == config) ) {
        *fs = NULL;
        return 0;
    }

    if ( unlikely(NULL == message && message_length != 0) ) {
        *fs = NULL;
        return 0;
    }

    frames_count = MAX(1, (size_t)ceil((double)message_length/(double)config->size_frame));

    if ( unlikely(NULL == (*fs = WSS_malloc(frames_count*sizeof(wss_frame_t *)))) ) {
        WSS_log_error("Unable to allocate closing frame");
        *fs = NULL;
        return 0;
    }

    frames = *fs;

    if (opcode == CLOSE_FRAME) {
        if ( likely(message_length >= sizeof(uint16_t)) ) {
            memcpy(&code, msg, sizeof(uint16_t));
            code = ntohs(code);
            msg = msg + sizeof(uint16_t);
        } else if ( unlikely(message_length == 1) ) {
            code = CLOSE_PROTOCOL;
            msg = msg + 1;
        } else {
            code = CLOSE_NORMAL;
        }

        frame = WSS_closing_frame(code, msg);
        frames[0] = frame;
        return 1;
    }

    for (i = 0; i < frames_count; i++) {
        // Always allocate one frame
        if ( unlikely(NULL == (frame = WSS_malloc(sizeof(wss_frame_t)))) ) {
            WSS_log_error("Unable to allocate frame");
            for (j = 0; j < i; j++) {
                WSS_free_frame(frames[j]);
            }
            WSS_free((void **)&frames);
            *fs = NULL;
            return 0;
        }

        frame->fin = 0;
        frame->opcode = opcode;
        frame->mask = 0;

        frame->applicationDataLength = MIN(message_length-(config->size_frame*i), config->size_frame);
        if ( unlikely(NULL == (frame->payload = WSS_malloc(frame->applicationDataLength+1))) ) {
            WSS_log_error("Unable to allocate frame application data");
            for (j = 0; j < i; j++) {
                WSS_free_frame(frames[j]);
            }
            WSS_free((void **)&frame);
            WSS_free((void **)&frames);
            *fs = NULL;
            return 0;
        }
        memcpy(frame->payload, msg+offset, frame->applicationDataLength);
        frame->payloadLength += frame->extensionDataLength;
        frame->payloadLength += frame->applicationDataLength;
        offset += frame->payloadLength;

        frames[i] = frame;
    }

    frames[frames_count-1]->fin = 1;

    return frames_count;
}

/**
 * Creates a closing frame given a reason for the closure.
 *
 * @param   reason   [wss_close_t]      "The reason for the closure"
 * @return 		     [wss_frame_t *]    "A websocket frame"
 */
wss_frame_t *WSS_closing_frame(wss_close_t reason, char *message) {
    wss_frame_t *frame;
    uint16_t nbo_reason;
    char *reason_str = message;

    WSS_log_trace("Creating closing frame");

    if ( unlikely(NULL == (frame = WSS_malloc(sizeof(wss_frame_t)))) ) {
        WSS_log_error("Unable to allocate closing frame");
        return NULL;
    }

    frame->fin = 1;
    frame->opcode = CLOSE_FRAME;
    frame->mask = 0;

    if (NULL == reason_str) {
        switch (reason) {
            case CLOSE_NORMAL:
                reason_str = "Normal close";
                break;
            case CLOSE_SHUTDOWN:
                reason_str = "Server is shutting down";
                break;
            case CLOSE_PROTOCOL:
                reason_str = "Experienced a protocol error";
                break;
            case CLOSE_TYPE:
                reason_str = "Unsupported data type";
                break;
            case CLOSE_NO_STATUS_CODE:
                reason_str = "No status received";
                break;
            case CLOSE_ABNORMAL:
                reason_str = "Abnormal closure";
                break;
            case CLOSE_UTF8:
                reason_str = "Invalid frame payload data";
                break;
            case CLOSE_POLICY:
                reason_str = "Policy Violation";
                break;
            case CLOSE_BIG:
                reason_str = "Message is too big";
                break;
            case CLOSE_EXTENSION:
                reason_str = "Mandatory extension";
                break;
            case CLOSE_UNEXPECTED:
                reason_str = "Internal server error";
                break;
            case CLOSE_RESTARTING:
                reason_str = "Server is restarting";
                break;
            case CLOSE_TRY_AGAIN:
                reason_str = "Try again later";
                break;
            case CLOSE_INVALID_PROXY_RESPONSE:
                reason_str = "The server was acting as a gateway or proxy and received an invalid response from the upstream server.";
                break;
            case CLOSE_FAILED_TLS_HANDSHAKE:
                reason_str = "Failed TLS Handshake";
                break;
            default:
                WSS_log_error("Unknown closing reason");
                WSS_free_frame(frame);
                return NULL;
        }
    }
    frame->applicationDataLength = strlen(reason_str)+sizeof(uint16_t);
    if ( unlikely(NULL == (frame->payload = WSS_malloc(frame->applicationDataLength+1))) ) {
        WSS_log_error("Unable to allocate closing frame application data");
        WSS_free_frame(frame);
        return NULL;
    }
    nbo_reason = htons16(reason);
    memcpy(frame->payload, &nbo_reason, sizeof(uint16_t));
    memcpy(frame->payload+sizeof(uint16_t), reason_str, strlen(reason_str));

    frame->payloadLength += frame->extensionDataLength;
    frame->payloadLength += frame->applicationDataLength;

    return frame;
}

/**
 * Creates a ping frame.
 *
 * @return 		     [wss_frame_t *]    "A websocket frame"
 */
wss_frame_t *WSS_ping_frame() {
    WSS_log_trace("Creating ping frame");

    wss_frame_t *frame;

    if ( unlikely(NULL == (frame = WSS_malloc(sizeof(wss_frame_t)))) ) {
        WSS_log_error("Unable to allocate ping frame");
        return NULL;
    }

    frame->fin = 1;
    frame->opcode = PING_FRAME;
    frame->mask = 0;

    frame->applicationDataLength = 120;
    if ( unlikely(NULL == (frame->payload = random_bytes(frame->applicationDataLength))) ) {
        WSS_log_error("Unable to allocate ping frame application data");
        WSS_free_frame(frame);
        return NULL;
    }

    frame->payloadLength += frame->extensionDataLength;
    frame->payloadLength += frame->applicationDataLength;

    return frame;
}

/**
 * Creates a pong frame from a received ping frame.
 *
 * @param   ping     [wss_frame_t *]    "A ping frame"
 * @return 		     [wss_frame_t *]    "A websocket frame"
 */
wss_frame_t *WSS_pong_frame(wss_frame_t *ping) {
    WSS_log_trace("Converting ping frame to pong frame");
    
    if ( NULL == ping ) {
        return NULL;
    }

    ping->fin = 1;
    ping->rsv1 = 0;
    ping->rsv2 = 0;
    ping->rsv3 = 0;
    ping->opcode = PONG_FRAME;
    ping->mask = 0;

    memset(ping->maskingKey, '\0', sizeof(uint32_t));

    return ping;
}

/**
 * Releases memory used by a frame.
 *
 * @param   ping     [wss_frame_t *]    "The frame that should be freed"
 * @return 		     [void]         
 */
void WSS_free_frame(wss_frame_t *frame) {
    if ( likely(NULL != frame) ) {
        WSS_free((void **) &frame->payload);
    }
    WSS_free((void **) &frame);
}
