#ifndef wss_comm_h
#define wss_comm_h

#if !defined(uthash_malloc) || !defined(uthash_free)
#include "alloc.h"

#ifndef uthash_malloc
#define uthash_malloc(sz) WSS_malloc(sz)   /* malloc fcn */
#endif

#ifndef uthash_free
#define uthash_free(ptr,sz) WSS_free((void **)&ptr) /* free fcn */
#endif
#endif

#include "server.h"
#include "session.h"
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
 * Structure used to find receivers
 */
typedef struct {
    int fd;
    UT_hash_handle hh;
} wss_receiver_t;

/**
 * Function that disconnects a session and freeing any allocated memory used by
 * the session.
 *
 * @param 	server	[wss_server_t *] 	"The server structure"
 * @param 	session	[wss_session_t *] 	"The session structure"
 * @return          [void]
 */
//void WSS_disconnect(void *args);

/**
 * Function that handles new connections. This function creates a new session and
 * associates the sessions filedescriptor to the epoll instance such that we can
 * start communicating with the session.
 *
 * @param 	server	[wss_server_t *] 	"The server structure"
 * @param 	session	[wss_session_t *] 	"The session structure"
 * @return          [void]
 */
//void WSS_connect(void *arg);

/**
 * Function that reads information from a session.
 *
 * @param 	server	[wss_server_t *] 	"The server structure"
 * @param 	session	[wss_session_t *] 	"The session structure"
 * @return          [void]
 */
//void WSS_read(void *args);

/**
 * Function that writes information to a session
 *
 * @param 	server	[wss_server_t *] 	"The server structure"
 * @param 	session	[wss_session_t *] 	"The session structure"
 * @return          [void]
 */
void WSS_write(wss_server_t *server, wss_session_t *session);

/**
 * Function that performs and distributes the IO work.
 *
 * @param 	args	[void *] 	"Is a args_t structure holding server_t, filedescriptor, and the state"
 * @return          [void]
 */
void WSS_work(void *args);

#endif
