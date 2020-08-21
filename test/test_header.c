#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <criterion/criterion.h>

#include "alloc.h"
#include "header.h"
#include "config.h"
#include "subprotocols.h"
#include "extensions.h"
#include "error.h"

#define HEADER_BAD_NEWLINE "\r\a\r\a"
#define HEADER_BAD_NEWLINE_PAYLOAD "GET / HTTP/1.1\r\n"\
                                   "test: test\r\a\r\n"
#define HEADER_BAD_NEWLINE_PAYLOAD_MULTIPLE_HEADERS "GET / HTTP/1.1\r\n"\
                                   "test: test\r\n"\
                                   "test2: test2\r\a\r\n"
#define HEADER_NEWLINE "\r\n\r\n"
#define HEADER_SPACE_NEWLINE " \r\n\r\n"
#define HEADER_INVALID_HTTP_PATH "GET * HTTP/1.1\r\n\r\n"
#define HEADER_NO_PATH "GET\r\n\r\n"
#define HEADER_NO_VERSION "GET /another/test\r\n\r\n"
#define HEADER_PATH_BIG "GET /another/test/path/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/test/?access_token=ACCESS&csrf_token=CSRF\r\n\r\n"
#define HEADER_WRONG_HTTP_METHOD "POST / HTTP/1.1\r\n\r\n"
#define HEADER_WRONG_HTTP_PATH "GET /invalid HTTP/1.1\r\n\r\n"
#define HEADER_HTTPS_HTTP_PATH "GET https://mortz.dk HTTP/1.1\r\n\r\n"
#define HEADER_WRONG_HTTP_PATH_QUERY "GET /test/path?access_token=test&invalid=yes HTTP/1.1\r\n\r\n"
#define HEADER_WRONG_HTTP_VERSION "GET / HTTP/0.8\r\n\r\n"
#define HEADER_HTTP_PATH "GET /test/path HTTP/1.1\r\n\r\n"
#define HEADER_HTTP_PATH_QUERY "GET /another/test/path?access_token=ACCESS&csrf_token=CSRF HTTP/1.1\r\n\r\n"
#define HEADER_HTTP "GET / HTTP/1.1\r\n\r\n"
#define HEADER_DUPLICATE_VERSION "GET / HTTP/1.1\r\n"\
                                 "Sec-WebSocket-Version: 13\r\n"\
                                 "Sec-WebSocket-Version: 13\r\n\r\n"
#define HEADER_DUPLICATE_KEY "GET / HTTP/1.1\r\n"\
                             "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"\
                             "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"
#define HEADER_BIG "GET / HTTP/1.1\r\n"\
                   "Lorem: Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam ac nisi risus. Fusce ante mauris, placerat vel ex eget, ultrices varius arcu. Fusce nec tincidunt mi. Curabitur gravida orci a odio vulputate, eu molestie enim auctor. Fusce viverra viverra est vel sagittis. Ut lacinia lacinia enim sit amet porta. Proin sit amet elementum tellus, a egestas ipsum. Nulla eu interdum odio. Nulla sodales enim eu urna vestibulum, id mattis justo euismod. Etiam mattis diam non ante ullamcorper, ut facilisis tortor interdum. Nunc sit amet auctor lacus, eget dictum diam. Fusce vel tellus consequat, malesuada elit ac, posuere erat. Nullam vel congue nisi, non imperdiet massa. Praesent sollicitudin massa ex, sollicitudin suscipit neque fringilla ac. Proin congue erat in semper ultrices. Praesent at odio sit amet metus condimentum elementum. Vivamus ullamcorper malesuada facilisis. Cras interdum ante egestas magna scelerisque, vel consectetur lectus dapibus. Ut vitae semper eros. Vivamus in augue vel ligula blandit tempor eu. This is tooo large\r\n\r\n"
#define HEADER_PAYLOAD_BIG "GET / HTTP/1.1\r\n"\
                           "\r\n"\
                           "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam ac nisi risus. Fusce ante mauris, placerat vel ex eget, ultrices varius arcu. Fusce nec tincidunt mi. Curabitur gravida orci a odio vulputate, eu molestie enim auctor. Fusce viverra viverra est vel sagittis. Ut lacinia lacinia enim sit amet porta. Proin sit amet elementum tellus, a egestas ipsum. Nulla eu interdum odio. Nulla sodales enim eu urna vestibulum, id mattis justo euismod. Etiam mattis diam non ante ullamcorper, ut facilisis tortor interdum. Nunc sit amet auctor lacus, eget dictum diam. Fusce vel tellus consequat, malesuada elit ac, posuere erat. Nullam vel congue nisi, non imperdiet massa. Praesent sollicitudin massa ex, sollicitudin suscipit neque fringilla ac. Proin congue erat in semper ultrices. Praesent at odio sit amet metus condimentum elementum. Vivamus ullamcorper malesuada facilisis. Cras interdum ante egestas magna scelerisque, vel consectetur lectus dapibus. Ut vitae semper eros. Vivamus in augue vel ligula blandit tempor eu. This is tooo large."
#define HEADER_PAYLOAD_BIG_WITH_HEADERS "GET / HTTP/1.1\r\n"\
                           "Test: test\r\n"\
                           "\r\n"\
                           "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam ac nisi risus. Fusce ante mauris, placerat vel ex eget, ultrices varius arcu. Fusce nec tincidunt mi. Curabitur gravida orci a odio vulputate, eu molestie enim auctor. Fusce viverra viverra est vel sagittis. Ut lacinia lacinia enim sit amet porta. Proin sit amet elementum tellus, a egestas ipsum. Nulla eu interdum odio. Nulla sodales enim eu urna vestibulum, id mattis justo euismod. Etiam mattis diam non ante ullamcorper, ut facilisis tortor interdum. Nunc sit amet auctor lacus, eget dictum diam. Fusce vel tellus consequat, malesuada elit ac, posuere erat. Nullam vel congue nisi, non imperdiet massa. Praesent sollicitudin massa ex, sollicitudin suscipit neque fringilla ac. Proin congue erat in semper ultrices. Praesent at odio sit amet metus condimentum elementum. Vivamus ullamcorper malesuada facilisis. Cras interdum ante egestas magna scelerisque, vel consectetur lectus dapibus. Ut vitae semper eros. Vivamus in augue vel ligula blandit tempor eu. This is tooo large."
#define HEADER_HTTP "GET / HTTP/1.1\r\n\r\n"
#define HEADER_RFC6455 "GET /test/path HTTP/1.1\r\n"\
                       "Host: 127.0.0.1\r\n"\
                       "Upgrade: websocket\r\n"\
                       "Connection: Upgrade\r\n"\
                       "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"\
                       "Origin: 127.0.0.1\r\n"\
                       "Sec-WebSocket-Protocol: chat, broadcast\r\n"\
                       "Cookie: test=test;\r\n"\
                       "Sec-WebSocket-Extensions: imaginary-extension ,\r\n"\
                       "Sec-WebSocket-Extensions: permessage-deflate;\r\n"\
                       "Sec-WebSocket-Extensions: client_max_window_bits=15 ,\r\n"\
                       "Sec-WebSocket-Extensions: permessage-deflate\r\n"\
                       "Sec-WebSocket-Version: 13\r\n\r\n"

#define HEADER_HYBI10 "GET /test/path HTTP/1.1\r\n"\
                      "Host: 127.0.0.1\r\n"\
                      "Upgrade: websocket\r\n"\
                      "Connection: Upgrade\r\n"\
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"\
                      "Origin: 127.0.0.1\r\n"\
                      "Sec-WebSocket-Protocol: broadcast, superchat\r\n"\
                      "Sec-WebSocket-Version: 8\r\n\r\n"

#define HEADER_HYBI07 "GET /test/path HTTP/1.1\r\n"\
                      "Host: 127.0.0.1\r\n"\
                      "Upgrade: websocket\r\n"\
                      "Connection: Upgrade\r\n"\
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"\
                      "Origin: 127.0.0.1\r\n"\
                      "Sec-WebSocket-Protocol: chat, superchat, echo\r\n"\
                      "Sec-WebSocket-Version: 7\r\n\r\n"

#define HEADER_HYBI06 "GET /test/path HTTP/1.1\r\n"\
                      "Host: 127.0.0.1\r\n"\
                      "Upgrade: websocket\r\n"\
                      "Connection: Upgrade\r\n"\
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"\
                      "Sec-WebSocket-Origin: 127.0.0.1\r\n"\
                      "Sec-WebSocket-Protocol: chat, echo, superchat\r\n"\
                      "Sec-WebSocket-Version: 6\r\n\r\n"

#define HEADER_HYBI05 "GET /test/path HTTP/1.1\r\n"\
                      "Host: 127.0.0.1\r\n"\
                      "Upgrade: websocket\r\n"\
                      "Connection: Upgrade\r\n"\
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"\
                      "Sec-WebSocket-Origin: 127.0.0.1\r\n"\
                      "Sec-WebSocket-Protocol: chat, superchat\r\n"\
                      "Sec-WebSocket-Version: 5\r\n\r\n"

#define HEADER_HYBI04 "GET /test/path HTTP/1.1\r\n"\
                      "Host: 127.0.0.1\r\n"\
                      "Upgrade: websocket\r\n"\
                      "Connection: Upgrade\r\n"\
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"\
                      "Sec-WebSocket-Origin: 127.0.0.1\r\n"\
                      "Sec-WebSocket-Protocol: chat, superchat\r\n"\
                      "Sec-WebSocket-Version: 4\r\n\r\n"

#define HEADER_HIXIE76 "GET /test/path HTTP/1.1\r\n"\
                       "Host: 127.0.0.1\r\n"\
                       "Connection: Upgrade\r\n"\
                       "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"\
                       "Sec-WebSocket-Protocol: sample\r\n"\
                       "Upgrade: WebSocket\r\n"\
                       "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"\
                       "Origin: 127.0.0.1\r\n"\
                       "\r\n"\
                       "^n:ds[4U"

#define HEADER_HIXIE75 "GET /test/path HTTP/1.1\r\n"\
                       "Upgrade: WebSocket\r\n"\
                       "Connection: Upgrade\r\n"\
                       "Host: 127.0.0.1\r\n"\
                       "Origin: 127.0.0.1\r\n"\
                       "WebSocket-Protocol: sample, echo\r\n\r\n"

#define HEADER_HIXIE75_REVERSE_PROTOCOL "GET /test/path HTTP/1.1\r\n"\
                                        "Upgrade: WebSocket\r\n"\
                                        "Connection: Upgrade\r\n"\
                                        "Host: 127.0.0.1\r\n"\
                                        "Origin: 127.0.0.1\r\n"\
                                        "WebSocket-Protocol: echo, sample\r\n\r\n"


#define HEADER_HIXIE75_NO_PROTOCOL "GET /test/path HTTP/1.1\r\n"\
                                   "Upgrade: WebSocket\r\n"\
                                   "Connection: Upgrade\r\n"\
                                   "Host: 127.0.0.1\r\n"\
                                   "Origin: 127.0.0.1\r\n\r\n"

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

TestSuite(WSS_parse_header, .init = setup, .fini = teardown);

Test(WSS_parse_header, no_header_content_data) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    enum HttpStatus_Code code;
    int fd = -1;

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = NULL;
    header->length = 0;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_BadRequest);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, bad_newline) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_BAD_NEWLINE);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_BAD_NEWLINE);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);

    cr_assert(code == HttpStatus_BadRequest);
    
    WSS_config_free(conf);
    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, bad_newline_payload) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_BAD_NEWLINE_PAYLOAD);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_BAD_NEWLINE_PAYLOAD);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);

    cr_assert(code == HttpStatus_BadRequest);
    
    WSS_config_free(conf);
    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, bad_newline_payload_multiple_headers) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_BAD_NEWLINE_PAYLOAD_MULTIPLE_HEADERS);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_BAD_NEWLINE_PAYLOAD_MULTIPLE_HEADERS);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);

    cr_assert(code == HttpStatus_BadRequest);
    
    WSS_config_free(conf);
    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, only_newline) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_NEWLINE);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_NEWLINE);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);

    cr_assert(code == HttpStatus_MethodNotAllowed);
    
    WSS_config_free(conf);
    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, space_and_newline) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_SPACE_NEWLINE);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_SPACE_NEWLINE);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);

    cr_assert(code == HttpStatus_BadRequest);
    
    WSS_config_free(conf);
    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, unsupported_http_method) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_WRONG_HTTP_METHOD);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_WRONG_HTTP_METHOD);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_MethodNotAllowed);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, no_http_path) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_NO_PATH);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_NO_PATH);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_BadRequest);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, too_big_path) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_PATH_BIG);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_PATH_BIG);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_URITooLong);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, invalid_http_path) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_INVALID_HTTP_PATH);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_INVALID_HTTP_PATH);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_BadRequest);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, unsupported_http_path) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_WRONG_HTTP_PATH);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_WRONG_HTTP_PATH);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, unsupported_http_path_query) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_WRONG_HTTP_PATH_QUERY);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_WRONG_HTTP_PATH_QUERY);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, no_http_version) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_NO_VERSION);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_NO_VERSION);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_BadRequest);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, unsupported_http_version) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_WRONG_HTTP_VERSION);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_WRONG_HTTP_VERSION);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_HTTPVersionNotSupported);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, payload_too_big) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_PAYLOAD_BIG);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_PAYLOAD_BIG);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_PayloadTooLarge);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, payload_too_big_with_headers) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_PAYLOAD_BIG_WITH_HEADERS);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_PAYLOAD_BIG_WITH_HEADERS);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_PayloadTooLarge);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, duplicate_version) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_DUPLICATE_VERSION);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_DUPLICATE_VERSION);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_BadRequest);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, duplicate_key) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_DUPLICATE_KEY);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_DUPLICATE_KEY);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_BadRequest);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, header_too_big) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_BIG);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_BIG);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_RequestHeaderFieldsTooLarge);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_rfc6455) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_RFC6455);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_RFC6455);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);
    WSS_load_extensions(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(NULL != header->ws_extensions);
    cr_assert(NULL != header->ws_protocol);

    WSS_destroy_subprotocols();
    WSS_destroy_extensions();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_hybi10) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI10);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI10);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(NULL != header->ws_protocol);

    WSS_destroy_subprotocols();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_hybi07) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI07);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI07);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(NULL != header->ws_protocol);

    WSS_destroy_subprotocols();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_hybi06) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI06);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI06);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(NULL != header->ws_protocol);

    WSS_destroy_subprotocols();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_hybi05) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI05);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI05);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_hybi04) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI04);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI04);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_hixie76) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HIXIE76);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HIXIE76);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_hixie75) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HIXIE75);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HIXIE75);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(NULL != header->ws_protocol);

    WSS_config_free(conf);

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_hixie75_reverse_protocol) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HIXIE75_REVERSE_PROTOCOL);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HIXIE75_REVERSE_PROTOCOL);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    WSS_destroy_subprotocols();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_parse_header, valid_hixie75_with_no_protocol) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HIXIE75_NO_PROTOCOL);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HIXIE75_NO_PROTOCOL);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

TestSuite(WSS_upgrade_header, .init = setup, .fini = teardown);

Test(WSS_upgrade_header, upgrade_required_for_http_path) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTPS_HTTP_PATH);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTPS_HTTP_PATH);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_UpgradeRequired);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, unsupported_http_path) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_WRONG_HTTP_PATH);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_WRONG_HTTP_PATH);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotFound);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotFound);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, unsupported_http_path_query) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_WRONG_HTTP_PATH_QUERY);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_WRONG_HTTP_PATH_QUERY);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotFound);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotFound);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, supported_http_path_missing_host) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP_PATH);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP_PATH);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_BadRequest);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_BadRequest);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, supported_http_path_query_missing_host) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP_PATH_QUERY);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP_PATH_QUERY);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_BadRequest);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_BadRequest);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, wrong_host) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "mortz.dk";

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_BadRequest);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_BadRequest);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, supported_host_missing_upgrade) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_UpgradeRequired);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_UpgradeRequired);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, too_short_upgrade) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "short";

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_UpgradeRequired);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_UpgradeRequired);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, supported_upgrade_missing_connection) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_UpgradeRequired);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_UpgradeRequired);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, too_short_connection) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = "short";

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_UpgradeRequired);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_UpgradeRequired);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, connection_comma) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = ",,,,,,,,,,";

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_UpgradeRequired);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_UpgradeRequired);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, wrong_connection) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = (char *) WSS_malloc(20);
    sprintf(header->ws_connection, "%s", "tralalala, 42, test");

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_UpgradeRequired);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_UpgradeRequired);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header->ws_connection);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, supported_connection_missing_origin) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = (char *) WSS_malloc(8);
    sprintf(header->ws_connection, "%s", "Upgrade");

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_Forbidden);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_Forbidden);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header->ws_connection);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, wrong_origin) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = (char *) WSS_malloc(8);
    sprintf(header->ws_connection, "%s", "Upgrade");
    header->ws_origin = "127.1.1.1";

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_Forbidden);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_Forbidden);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header->ws_connection);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, supported_origin_missing_type) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = (char *) WSS_malloc(8);
    sprintf(header->ws_connection, "%s", "Upgrade");
    header->ws_origin = "127.0.0.1";

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotImplemented);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotImplemented);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header->ws_connection);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, wrong_type) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = (char *) WSS_malloc(8);
    sprintf(header->ws_connection, "%s", "Upgrade");
    header->ws_origin = "127.0.0.1";
    header->ws_type = HIXIE76;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotImplemented);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotImplemented);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header->ws_connection);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, supported_type_missing_key) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = (char *) WSS_malloc(8);
    sprintf(header->ws_connection, "%s", "Upgrade");
    header->ws_origin = "127.0.0.1";
    header->ws_type = HYBI10;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_UpgradeRequired);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_UpgradeRequired);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header->ws_connection);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, wrong_key_length) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = (char *) WSS_malloc(8);
    sprintf(header->ws_connection, "%s", "Upgrade");
    header->ws_origin = "127.0.0.1";
    header->ws_key = "key";
    header->ws_type = RFC6455;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_UpgradeRequired);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_UpgradeRequired);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header->ws_connection);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, supported_key_switching_protocols) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HTTP);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HTTP);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;
    header->host = "127.0.0.1";
    header->ws_upgrade = "websocket";
    header->ws_connection = (char *) WSS_malloc(8);
    sprintf(header->ws_connection, "%s", "Upgrade");
    header->ws_origin = "127.0.0.1";
    header->ws_type = RFC6455;
    header->ws_key = "dGhlIHNhbXBsZSBub25jZQ==";

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_SwitchingProtocols);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_SwitchingProtocols);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header->ws_connection);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_rfc6455) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_RFC6455);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_RFC6455);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);
    WSS_load_extensions(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(1 == header->ws_extensions_count);
    cr_assert(strncmp("permessage-deflate", header->ws_extensions[0]->name, 18) == 0);
    cr_assert(NULL != header->ws_extensions);
    cr_assert(NULL != header->ws_protocol);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_SwitchingProtocols);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_SwitchingProtocols);

    WSS_destroy_subprotocols();
    WSS_destroy_extensions();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_hybi10) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI10);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI10);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(NULL != header->ws_protocol);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_SwitchingProtocols);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_SwitchingProtocols);

    WSS_destroy_subprotocols();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_hybi07) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI07);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI07);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(NULL != header->ws_protocol);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_SwitchingProtocols);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_SwitchingProtocols);

    WSS_destroy_subprotocols();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_hybi06) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI06);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI06);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(NULL != header->ws_protocol);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotImplemented);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotImplemented);

    WSS_destroy_subprotocols();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_hybi05) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI05);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI05);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotImplemented);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotImplemented);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_hybi04) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HYBI04);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HYBI04);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotImplemented);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotImplemented);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_hixie76) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HIXIE76);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HIXIE76);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotImplemented);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotImplemented);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_hixie75) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HIXIE75);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HIXIE75);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);
    cr_assert(NULL != header->ws_protocol);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotImplemented);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotImplemented);

    WSS_config_free(conf);

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_hixie75_reverse_protocol) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HIXIE75_REVERSE_PROTOCOL);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HIXIE75_REVERSE_PROTOCOL);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotImplemented);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotImplemented);

    WSS_destroy_subprotocols();
    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}

Test(WSS_upgrade_header, valid_hixie75_with_no_protocol_not_impletented) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    wss_header_t *header = (wss_header_t *) WSS_malloc(sizeof(wss_header_t));
    size_t l = strlen(HEADER_HIXIE75_NO_PROTOCOL);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_HIXIE75_NO_PROTOCOL);

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_OK);

    code = WSS_upgrade_header(header, conf, true, conf->port_http);
    cr_assert(code == HttpStatus_NotImplemented);

    code = WSS_upgrade_header(header, conf, true, conf->port_https);
    cr_assert(code == HttpStatus_NotImplemented);

    WSS_config_free(conf);

    WSS_free((void**) &conf);
    WSS_free_header(header);
}
