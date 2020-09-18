#include <stdio.h>
#include <arpa/inet.h>
#include <criterion/criterion.h>

#include "alloc.h"
#include "frame.h"
#include "rpmalloc.h"

static void setup(void) {
#ifdef USE_RPMALLOC
    rpmalloc_initialize();
#endif
}

static void teardown(void) {
#ifdef USE_RPMALLOC
    rpmalloc_finalize();
#endif
}

static inline char *mask(char key[4], char *payload, size_t length) {
    size_t i, j;
    for (i = 0, j = 0; i < length; i++, j++){
        payload[i] = payload[i] ^ key[j % 4];
    }
    return payload;
}

static inline uint64_t htons64(uint64_t value) {
	static const int num = 42;

	/**
	 * If this check is true, the system is using the little endian
	 * convention. Else the system is using the big endian convention, which
	 * means that we do not have to represent our integers in another way.
	 */
	if (*(char *)&num == 42) {
        return ((uint64_t)htonl((value) & 0xFFFFFFFFLL) << 32) | htonl((value) >> 32);
	} else {
        return value;
	}
}

TestSuite(WSS_parse_frame, .init = setup, .fini = teardown);

Test(WSS_parse_frame, null_payload) {
    size_t offset = 0;
    cr_assert(NULL == WSS_parse_frame(NULL, 0, &offset));
}

Test(WSS_parse_frame, empty_payload) {
    size_t offset = 0;
    cr_assert(NULL != WSS_parse_frame("", 0, &offset));
    cr_assert(offset == 2);
}

Test(WSS_parse_frame, small_client_frame) {
    size_t offset = 0;
    char *payload = "\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58";
    uint16_t length = strlen(payload);
    wss_frame_t *frame = WSS_parse_frame(payload, length, &offset);
    cr_assert(NULL != frame);
    cr_assert(offset == length);
    cr_assert(strncmp(frame->payload, "Hello", frame->payloadLength) == 0);
    WSS_free_frame(frame);
}

Test(WSS_parse_frame, medium_client_frame) {
    size_t offset = 0;
    char header[2] = "\x81\xFE";
    char key[4] = "\x37\xfa\x21\x3d";
    char *payload = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin hendrerit ornare tortor ut euismod. Nunc finibus convallis sem, at imperdiet ligula commodo id. Nam bibendum nec augue in posuere mauris.";
    uint16_t length = strlen(payload);
    char *payload_copy = WSS_copy(payload, length);
    char *payload_frame = WSS_malloc((2+2+4+length+1)*sizeof(char));
    uint16_t len = htons(length);
    memcpy(payload_frame, header, 2);
    memcpy(payload_frame+2, &len, 2);
    memcpy(payload_frame+2+2, key, 4);
    memcpy(payload_frame+2+2+4, mask(key, payload_copy, length), length);
    wss_frame_t *frame = WSS_parse_frame(payload_frame, length+8, &offset);
    cr_assert(NULL != frame);
    cr_assert(offset == 208);
    cr_assert(strncmp(frame->payload, payload, frame->payloadLength) == 0);
    WSS_free_frame(frame);
    WSS_free((void **)&payload_copy);
    WSS_free((void **)&payload_frame);
}

Test(WSS_parse_frame, large_client_frame) {
    int i;
    size_t offset = 0;
    char header[2] = "\x81\xFF";
    char key[4] = "\x37\xfa\x21\x3d";
    char *p = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin hendrerit ornare tortor ut euismod. Nunc finibus convallis sem, at imperdiet ligula commodo id. Nam bibendum nec augue in posuere mauris.";
    char *payload = WSS_malloc(66001);
    size_t plen = strlen(p);
    size_t poff = 0;
    for (i = 0; i < 330; i++) {
        memcpy(payload+poff, p, plen);
        poff+=plen;
    }
    uint64_t length = strlen(payload);
    char *payload_copy = WSS_copy(payload, length);
    char *payload_frame = WSS_malloc((2+2+4+length+1)*sizeof(char));
    uint64_t len = htons64(length);
    memcpy(payload_frame, header, 2);
    memcpy(payload_frame+2, &len, sizeof(uint64_t));
    memcpy(payload_frame+2+sizeof(uint64_t), key, 4);
    memcpy(payload_frame+2+sizeof(uint64_t)+4, mask(key, payload_copy, length), length);
    wss_frame_t *frame = WSS_parse_frame(payload_frame, length+6+sizeof(uint64_t), &offset);
    cr_assert(NULL != frame);
    cr_assert(offset == 66014);
    cr_assert(strncmp(frame->payload, payload, frame->payloadLength) == 0);
    WSS_free_frame(frame);
    WSS_free((void **)&payload);
    WSS_free((void **)&payload_copy);
    WSS_free((void **)&payload_frame);
}

TestSuite(WSS_stringify_frame, .init = setup, .fini = teardown);

Test(WSS_stringify_frame, null_frame) {
    char *message;
    cr_assert(0 == WSS_stringify_frame(NULL, &message));
    cr_assert(NULL == message);
}

Test(WSS_stringify_frame, small_client_frame) {
    size_t offset = 0;
    char *message;
    char *payload = "\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58";
    uint16_t length = strlen(payload);

    wss_frame_t *frame = WSS_parse_frame(payload, length, &offset);

    cr_assert(NULL != frame);
    cr_assert(offset == length);
    cr_assert(strncmp(frame->payload, "Hello", frame->payloadLength) == 0);

    offset = WSS_stringify_frame(frame, &message);

    cr_assert(offset == (size_t)length-4);
    cr_assert(strncmp(message, "\x81\x05", 2) == 0);
    cr_assert(strncmp(frame->payload, message+2, 5) == 0);

    WSS_free((void **)&message);
    WSS_free_frame(frame);
}

Test(WSS_stringify_frame, small_client_frame_rsv) {
    size_t offset = 0;
    char *message;
    char *payload = "\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58";
    uint16_t length = strlen(payload);

    wss_frame_t *frame = WSS_parse_frame(payload, length, &offset);

    cr_assert(NULL != frame);
    cr_assert(offset == length);
    cr_assert(strncmp(frame->payload, "Hello", frame->payloadLength) == 0);

    frame->rsv1 = true;
    frame->rsv2 = true;
    frame->rsv3 = true;

    offset = WSS_stringify_frame(frame, &message);

    cr_assert(offset == (size_t)length-4);
    cr_assert(strncmp(message, "\xF1\x05", 2) == 0);
    cr_assert(strncmp(frame->payload, message+2, 5) == 0);

    WSS_free((void **)&message);
    WSS_free_frame(frame);
}

Test(WSS_stringify_frame, medium_client_frame) {
    size_t offset = 0;
    char header[2] = "\x81\xFE";
    char *message;
    char key[4] = "\x37\xfa\x21\x3d";
    char *payload = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin hendrerit ornare tortor ut euismod. Nunc finibus convallis sem, at imperdiet ligula commodo id. Nam bibendum nec augue in posuere mauris.";
    uint16_t length = strlen(payload);
    char *payload_copy = WSS_copy(payload, length);
    char *payload_frame = WSS_malloc((2+2+4+length+1)*sizeof(char));
    uint16_t len = htons(length);
    memcpy(payload_frame, header, 2);
    memcpy(payload_frame+2, &len, 2);
    memcpy(payload_frame+2+2, key, 4);
    memcpy(payload_frame+2+2+4, mask(key, payload_copy, length), length);
    wss_frame_t *frame = WSS_parse_frame(payload_frame, length+8, &offset);
    cr_assert(NULL != frame);
    cr_assert(offset == 208);
    cr_assert(strncmp(frame->payload, payload, frame->payloadLength) == 0);

    offset = WSS_stringify_frame(frame, &message);

    cr_assert(offset == (size_t)length+2+sizeof(uint16_t));
    cr_assert(strncmp(message, "\x81\x7E", 2) == 0);
    uint16_t str_len;
    memcpy(&str_len, message+2, sizeof(uint16_t));
    cr_assert(str_len == len);
    cr_assert(strncmp(frame->payload, message+2+sizeof(uint16_t), length) == 0);

    WSS_free_frame(frame);
    WSS_free((void **)&message);
    WSS_free((void **)&payload_copy);
    WSS_free((void **)&payload_frame);
}

Test(WSS_stringify_frame, large_client_frame) {
    int i;
    size_t offset = 0;
    char *message;
    char header[2] = "\x81\xFF";
    char key[4] = "\x37\xfa\x21\x3d";
    char *p = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin hendrerit ornare tortor ut euismod. Nunc finibus convallis sem, at imperdiet ligula commodo id. Nam bibendum nec augue in posuere mauris.";
    char *payload = WSS_malloc(66001);
    size_t plen = strlen(p);
    size_t poff = 0;
    for (i = 0; i < 330; i++) {
        memcpy(payload+poff, p, plen);
        poff+=plen;
    }
    uint64_t length = strlen(payload);
    char *payload_copy = WSS_copy(payload, length);
    char *payload_frame = WSS_malloc((2+2+4+length+1)*sizeof(char));
    uint64_t len = htons64(length);
    memcpy(payload_frame, header, 2);
    memcpy(payload_frame+2, &len, sizeof(uint64_t));
    memcpy(payload_frame+2+sizeof(uint64_t), key, 4);
    memcpy(payload_frame+2+sizeof(uint64_t)+4, mask(key, payload_copy, length), length);
    wss_frame_t *frame = WSS_parse_frame(payload_frame, length+6+sizeof(uint64_t), &offset);
    cr_assert(NULL != frame);
    cr_assert(offset == 66014);
    cr_assert(strncmp(frame->payload, payload, frame->payloadLength) == 0);

    offset = WSS_stringify_frame(frame, &message);

    cr_assert(offset == (size_t)length+2+sizeof(uint64_t));
    cr_assert(strncmp(message, "\x81\x7F", 2) == 0);
    uint64_t str_len;
    memcpy(&str_len, message+2, sizeof(uint64_t));
    cr_assert(str_len == len);
    cr_assert(strncmp(frame->payload, message+2+sizeof(uint64_t), length) == 0);

    WSS_free_frame(frame);
    WSS_free((void **)&message);
    WSS_free((void **)&payload);
    WSS_free((void **)&payload_copy);
    WSS_free((void **)&payload_frame);
}

TestSuite(WSS_pong_frame, .init = setup, .fini = teardown);

Test(WSS_pong_frame, null_frame) {
    cr_assert(NULL == WSS_pong_frame(NULL));
}

Test(WSS_pong_frame, pong_from_ping) {
    wss_frame_t *ping = WSS_ping_frame();
    size_t ping_payload_len = ping->payloadLength;
    char *ping_payload = WSS_copy(ping->payload, ping_payload_len);
    wss_frame_t *pong = WSS_pong_frame(ping);

    cr_assert(NULL != pong);
    cr_assert(true == pong->fin);
    cr_assert(false == pong->rsv1);
    cr_assert(false == pong->rsv2);
    cr_assert(false == pong->rsv3);
    cr_assert(PONG_FRAME == pong->opcode);
    cr_assert(0 == pong->mask);
    cr_assert(0 == pong->maskingKey[0]);
    cr_assert(0 == pong->maskingKey[1]);
    cr_assert(0 == pong->maskingKey[2]);
    cr_assert(0 == pong->maskingKey[3]);
    cr_assert(pong->payloadLength == ping_payload_len);
    cr_assert(strncmp(pong->payload, ping_payload, ping_payload_len) == 0);
    
    WSS_free((void **)&ping_payload);
}

TestSuite(WSS_stringify_frames, .init = setup, .fini = teardown);

Test(WSS_stringify_frames, null_frame) {
    char *message;
    cr_assert(0 == WSS_stringify_frames(NULL, 0, &message));
    cr_assert(NULL == message);
}

Test(WSS_stringify_frames, null_last_frame) {
    size_t offset = 0;
    char *message;
    char *payload = "\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58";
    uint16_t length = strlen(payload);
    wss_frame_t *frames[2];
    wss_frame_t *frame = WSS_parse_frame(payload, length, &offset);

    cr_assert(NULL != frame);
    cr_assert(offset == length);
    cr_assert(strncmp(frame->payload, "Hello", frame->payloadLength) == 0);

    frames[0] = frame;
    frames[1] = NULL;

    cr_assert(0 == WSS_stringify_frames(frames, 2, &message));
    cr_assert(NULL == message);
    WSS_free_frame(frame);
}

Test(WSS_stringify_frames, small_client_frame) {
    size_t offset = 0;
    char *message;
    char *payload = "\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58";
    uint16_t length = strlen(payload);

    wss_frame_t *frame = WSS_parse_frame(payload, length, &offset);

    cr_assert(NULL != frame);
    cr_assert(offset == length);
    cr_assert(strncmp(frame->payload, "Hello", frame->payloadLength) == 0);

    offset = WSS_stringify_frames(&frame, 1, &message);

    cr_assert(offset == (size_t)length-4);
    cr_assert(strncmp(message, "\x81\x05", 2) == 0);
    cr_assert(strncmp(frame->payload, message+2, 5) == 0);

    WSS_free((void **)&message);
    WSS_free_frame(frame);
}

TestSuite(WSS_create_frames, .init = setup, .fini = teardown);

Test(WSS_create_frames, null_config) {
    wss_frame_t **frames;
    cr_assert(0 == WSS_create_frames(NULL, CLOSE_FRAME, "", 0, &frames));
    cr_assert(NULL == frames);
}

Test(WSS_create_frames, null_message_with_length) {
    wss_frame_t **frames;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    cr_assert(0 == WSS_create_frames(conf, CLOSE_FRAME, NULL, 1, &frames));
    cr_assert(NULL == frames);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_create_frames, zero_length) {
    wss_frame_t **frames;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    cr_assert(1 == WSS_create_frames(conf, TEXT_FRAME, "", 0, &frames));
    cr_assert(frames[0]->payloadLength == 0);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_create_frames, multiple_frames) {
    char *message = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin hendrerit ornare tortor ut euismod. Nunc finibus convallis sem, at imperdiet ligula commodo id. Nam bibendum nec augue in posuere mauris.";
    wss_frame_t **frames;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    cr_assert(2 == WSS_create_frames(conf, TEXT_FRAME, message, strlen(message), &frames));
    cr_expect(frames[0]->payloadLength == conf->size_frame);
    cr_expect(frames[1]->payloadLength == strlen(message)-conf->size_frame);
    cr_expect(strncmp(frames[0]->payload, message, frames[0]->payloadLength) == 0);
    cr_expect(strncmp(frames[1]->payload, message+conf->size_frame, frames[1]->payloadLength) == 0);

    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_create_frames, empty_close_frame) {
    char *message = "";
    wss_frame_t **frames;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    cr_assert(1 == WSS_create_frames(conf, CLOSE_FRAME, message, 0, &frames));
    cr_assert(frames[0]->payloadLength == 2);
    cr_assert(frames[0]->opcode == CLOSE_FRAME);
    cr_assert(memcmp(frames[0]->payload, "\x03\xE8", frames[0]->payloadLength) == 0);

    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_create_frames, protocol_error_close_frame) {
    char *message = "\x03";
    wss_frame_t **frames;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    cr_assert(1 == WSS_create_frames(conf, CLOSE_FRAME, message, strlen(message), &frames));
    cr_assert(frames[0]->payloadLength == 2);
    cr_assert(frames[0]->opcode == CLOSE_FRAME);
    cr_assert(memcmp(frames[0]->payload, "\x03\xEA", frames[0]->payloadLength) == 0);

    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_create_frames, close_frame_with_opcode) {
    char *message = "\x03\xEA";
    wss_frame_t **frames;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    cr_assert(1 == WSS_create_frames(conf, CLOSE_FRAME, message, strlen(message), &frames));
    cr_assert(frames[0]->payloadLength == 2);
    cr_assert(frames[0]->opcode == CLOSE_FRAME);
    cr_assert(memcmp(frames[0]->payload, message, strlen(message)) == 0);

    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_create_frames, close_frame_with_opcode_and_reason) {
    char *message = "\x03\xEDThis is a test";
    wss_frame_t **frames;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    cr_assert(1 == WSS_create_frames(conf, CLOSE_FRAME, message, strlen(message), &frames));
    cr_assert(frames[0]->payloadLength == 16);
    cr_assert(frames[0]->opcode == CLOSE_FRAME);
    cr_assert(memcmp(frames[0]->payload, message, strlen(message)) == 0);

    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

TestSuite(WSS_closing_frame, .init = setup, .fini = teardown);

Test(WSS_closing_frame, create_different_closing_frames) {
    uint16_t code, nw_code;
    wss_frame_t *frame;

    for (int i = 0; i <= 15; i++) {
        code = CLOSE_NORMAL+i;
        nw_code = htons(code);
        frame = WSS_closing_frame(code, NULL);

        if (i == 4) {
            cr_assert(frame == NULL);
        } else {
            cr_assert(frame != NULL);
            cr_expect(frame->opcode == CLOSE_FRAME);
            cr_expect(memcmp(frame->payload, &nw_code, sizeof(nw_code)) == 0);
            cr_expect(frame->payloadLength > 2);
        }

        WSS_free_frame(frame);
    }
}

