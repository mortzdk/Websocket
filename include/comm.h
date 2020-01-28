#ifndef wss_comm_h
#define wss_comm_h

#include "server.h"
#include "uthash.h"

#define MAGIC_WEBSOCKET_KEY         "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define GMT_FORMAT_LENGTH           30
#define HTTP_STATUS_LINE            "%s %d %s\r\n"
#define HTTP_ICO_HEADERS            "Content-Type: image/x-icon\r\n"\
                                    "Content-Length: %d\r\n"\
                                    "Connection: close\r\n"\
                                    "Date: %s\r\n"\
                                    "Server: WSServer/%s\r\n"\
                                    "Etag: %s\r\n"\
                                    "Accept-Ranges: bytes\r\n"\
                                    "Last-Modified: %s\r\n"\
                                    "\r\n"
#define HTTP_HTML_HEADERS           "Content-Type: text/html; charset=utf-8\r\n"\
                                    "Content-Length: %d\r\n"\
                                    "Connection: Closed\r\n"\
                                    "Date: %s\r\n"\
                                    "Server: WSServer/%s\r\n"\
                                    "\r\n"
#define HTTP_UPGRADE_HEADERS        "Content-Type: text/plain\r\n"\
                                    "Content-Length: %d\r\n"\
                                    "Connection: Upgrade\r\n"\
                                    "Upgrade: websocket\r\n"\
                                    "Sec-WebSocket-Version: 13\r\n"\
                                    "Server: WSServer/%s\r\n"\
                                    "\r\n"
#define HTTP_HANDSHAKE_EXTENSIONS   "Sec-Websocket-Extensions: "
#define HTTP_HANDSHAKE_SUBPROTOCOL  "Sec-Websocket-Protocol: "
#define HTTP_HANDSHAKE_ACCEPT       "Sec-Websocket-Accept: "
#define HTTP_HANDSHAKE_HEADERS      "Upgrade: websocket\r\n"\
                                    "Connection: Upgrade\r\n"\
                                    "Server: WSServer/%s\r\n"\
                                    "\r\n"
#define HTTP_BODY                   "<!doctype html>"\
                                    "<html lang=\"en\">"\
                                    "<head>"\
                                    "<meta charset=\"utf-8\">"\
                                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\">"\
                                    "<title>%d %s</title>"\
                                    "</head>"\
                                    "<body>"\
                                    "<h1>%s</h1>"\
                                    "<p>%s</p>"\
                                    "</body>"\
                                    "</html>"

/**
 * Structure used to send from event loop to threadpool
 */
typedef struct {
    int fd;
    server_t *server;
} args_t;

/**
 * Structure used to find receivers
 */
typedef struct {
    int fd;
    UT_hash_handle hh;
} receiver_t;

/**
 * Function that disconnects a session and freeing any allocated memory used by
 * the session.
 *
 * @param 	args	[void *] "Is a args_t structure holding server_t and filedescriptor"
 * @return          [void]
 */
void WSS_disconnect(void *args, int id);

/**
 * Function that handles new connections. This function creates a new session and
 * associates the sessions filedescriptor to the epoll instance such that we can
 * start communicating with the session.
 *
 * @param 	arg     [void *] 		"Is in fact a server_t instance"
 * @return          [void]
 */
void WSS_connect(void *arg, int id);

/**
 * Function that reads information from a session.
 *
 * @param 	args	[void *] 	"Is a args_t structure holding server_t and filedescriptor"
 * @return          [void]
 */
void WSS_read(void *args, int id);

/**
 * Function that writes information to a session.
 *
 * @param 	args	[void *] 	"Is a args_t structure holding server_t and filedescriptor"
 * @return          [void]
 */
void WSS_write(void *args, int id);

#endif
