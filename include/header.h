#pragma once

#ifndef WSS_HEADER_H
#define WSS_HEADER_H

#include <stdbool.h>
#include <regex.h>              /* regex_t, regcomp, regexec */

#include "config.h"
#include "httpstatuscodes.h"
#include "subprotocols.h"
#include "extensions.h"

#define WEBSOCKET_STRING "websocket"
#define WEBSOCKET_UPPERCASE_STRING "WebSocket"
#define WEBSOCKET_PROTOCOL_STRING "WebSocket-Protocol"
#define UPGRADE_STRING "Upgrade"
#define ORIGIN_STRING "Origin"
#define CONNECTION_STRING "Connection"
#define COOKIE_STRING "Cookie"
#define HOST_STRING "Host"
#define SEC_WEBSOCKET_KEY_LENGTH 16
#define SEC_WEBSOCKET_VERSION "Sec-WebSocket-Version"
#define SEC_WEBSOCKET_PROTOCOL "Sec-WebSocket-Protocol"
#define SEC_WEBSOCKET_KEY "Sec-WebSocket-Key"
#define SEC_WEBSOCKET_KEY1 "Sec-WebSocket-Key1"
#define SEC_WEBSOCKET_KEY2 "Sec-WebSocket-Key2"
#define SEC_WEBSOCKET_ORIGIN "Sec-WebSocket-Origin"
#define SEC_WEBSOCKET_EXTENSIONS "Sec-WebSocket-Extensions"

typedef enum {
    UNKNOWN  = 0,
    HIXIE75  = 1,
    HIXIE76  = 2,
    HYBI04   = 4,
    HYBI05   = 5,
    HYBI06   = 6,
    HYBI07   = 7,
    HYBI10   = 8,
    RFC6455  = 13
} wss_type_t;

typedef struct {
    unsigned int length;
    char *content;
    char *method;
    char *version;
    char *path;
    char *host;
    char *payload;
    char *cookies;
    int ws_version;
    wss_type_t ws_type;
    wss_subprotocol_t *ws_protocol;
    char *ws_upgrade;
    char *ws_connection;
    wss_ext_t **ws_extensions;
    unsigned int ws_extensions_count;
    char *ws_origin;
    char *ws_key;
    char *ws_key1;
    char *ws_key2;
    char *ws_key3;
} wss_header_t;

/**
 * Parses a HTTP header into a header structure and returns the status code
 * appropriate.
 *
 * @param   fd        [int]                    "The filedescriptor"
 * @param   header    [wss_header_t *]         "The header structure to fill"
 * @param   config    [wss_config_t *]         "The configuration of the server"
 * @return            [enum HttpStatus_Code]   "The status code to return to the client"
 */
enum HttpStatus_Code WSS_parse_header(int fd, wss_header_t *header, wss_config_t *config);

/**
 * Upgrades a HTTP header, that is returns switching protocols response if
 * the header contains the required options.
 *
 * @param   header    [wss_header_t *]         "The header structure to fill"
 * @param   config    [wss_config_t *]         "The configuration of the server"
 * @param   re        [regex_t *]              "The regex to validate path"
 * @return            [enum HttpStatus_Code]   "The status code to return to the client"
 */
enum HttpStatus_Code WSS_upgrade_header(wss_header_t *header, wss_config_t *config, regex_t *re);

/**
 * Frees a HTTP header structure
 *
 * @param   header    [wss_header_t *]  "The HTTP header to free"
 * @return            [void]
 */
void WSS_free_header(wss_header_t *header);

#endif
