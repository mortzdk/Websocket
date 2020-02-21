#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <criterion/criterion.h>

#include "alloc.h"
#include "header.h"
#include "config.h"
#include "error.h"

#define HEADER_WRONG_HTTP_METHOD "POST / HTTP/1.1\r\n\r\n"
#define HEADER_WRONG_HTTP_PATH "GET * HTTP/1.1\r\n\r\n"
#define HEADER_WRONG_HTTP_VERSION "GET / HTTP/0.8\r\n\r\n"

Test(WSS_parse_header, unsupported_http_method) {
    config_t *conf = (config_t *) WSS_malloc(sizeof(config_t));
    header_t *header = (header_t *) WSS_malloc(sizeof(header_t));
    size_t l = strlen(HEADER_WRONG_HTTP_METHOD);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_WRONG_HTTP_METHOD);

    cr_assert(SUCCESS == config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_MethodNotAllowed);

    config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header);
    WSS_free((void**) &h);
}

Test(WSS_parse_header, unsupported_http_path) {
    config_t *conf = (config_t *) WSS_malloc(sizeof(config_t));
    header_t *header = (header_t *) WSS_malloc(sizeof(header_t));
    size_t l = strlen(HEADER_WRONG_HTTP_PATH);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_WRONG_HTTP_PATH);

    cr_assert(SUCCESS == config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_NotFound);

    config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header);
    WSS_free((void**) &h);
}

Test(WSS_parse_header, unsupported_http_version) {
    config_t *conf = (config_t *) WSS_malloc(sizeof(config_t));
    header_t *header = (header_t *) WSS_malloc(sizeof(header_t));
    size_t l = strlen(HEADER_WRONG_HTTP_VERSION);
    char *h = WSS_malloc(l*sizeof(char)+1);
    enum HttpStatus_Code code;
    int fd = -1;

    sprintf(h, "%s", HEADER_WRONG_HTTP_VERSION);

    cr_assert(SUCCESS == config_load(conf, "resources/test_wss.json"));

    header->content = h;
    header->length = l;

    code = WSS_parse_header(fd, header, conf);
    
    cr_assert(code == HttpStatus_HTTPVersionNotSupported);

    config_free(conf);

    WSS_free((void**) &conf);
    WSS_free((void**) &header);
    WSS_free((void**) &h);
}

