#ifndef wss_header_h
#define wss_header_h

#include <stdbool.h>

#include "config.h"
#include "httpstatuscodes.h"
#include "subprotocols.h"

#define REQUEST_URI "^(ws%s://(%s)(:%d)?)?/(%s)?(\\?(%s)?)?$"
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(a,b) ((a) < (b) ? a : b)

#define ASCII_WEBSOCKET_STRING "websocket"
#define ASCII_CONNECTION_STRING "Upgrade"
#define SEC_WEBSOCKET_KEY_LENGTH 16

typedef enum {
    UNKNOWN  = 0,
    HIXIE    = 1,
    HYBI     = 2,
    HYBI04   = 4,
    HYBI05   = 5,
    HYBI06   = 6,
    HYBI07   = 7,
    HYBI10   = 8,
    RFC6455  = 13
} ws_type_t;

typedef struct {
    unsigned int length;
    char *content;
    char *method;
    char *version;
    char *path;
    char *host;
    char *payload;
    int ws_version;
    int ws_type;
    wss_subprotocol_t *ws_protocol;
    char *ws_upgrade;
    char *ws_connection;
    char *ws_extension;
    char *ws_origin;
    char *ws_key;
    char *ws_key1;
    char *ws_key2;
    char *ws_key3;
} header_t;

/**
 * Parses a HTTP header into a header structure and returns the status code
 * appropriate.
 *
 * @param   header    [header_t *]             "The header structure to fill"
 * @param   config    [config_t *]             "The configuration of the server"
 * @return            [enum HttpStatus_Code]   "The status code to return to the client"
 */
enum HttpStatus_Code WSS_parse_header(header_t *header, config_t *config);

/**
 * Upgrades a HTTP header, that is returns switching protocols response if
 * the header contains the required options.
 *
 * @param   header    [header_t *]             "The header structure to fill"
 * @param   config    [config_t *]             "The configuration of the server"
 * @param   ssl       [bool]                   "Whether server uses SSL"
 * @param   port      [int]                    "The server port"
 * @return            [enum HttpStatus_Code]   "The status code to return to the client"
 */
enum HttpStatus_Code WSS_upgrade_header(header_t *header, config_t *config, bool ssl, int port);

/**
 * Frees a HTTP header structure
 *
 * @param   header    [header_t *]  "The HTTP header to free"
 * @return            [void]
 */
void WSS_free_header(header_t *header);

#endif
