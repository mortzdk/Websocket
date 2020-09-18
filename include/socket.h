#ifndef wss_socket_h
#define wss_socket_h

#include "server.h"
#include "error.h"

/**
 * Function that initializes a socket and store the filedescriptor.
 *
 * @param 	server	[wss_server_t *]    "The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_socket_create(wss_server_t *server);

/**
 * Function that enables reuse of the port if the server shuts down.
 *
 * @param 	fd		[int]		        "The filedescriptor associated to some user"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_socket_reuse(int fd);

/**
 * Function that binds the socket to a specific port and chooses IPV4.
 *
 * @param 	server	[wss_server_t *]	"The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_socket_bind(wss_server_t *server);

/**
 * Function that makes the socket non-blocking for both reads and writes.
 *
 * @param 	fd		[int]		        "The filedescriptor associated to some user"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_socket_non_blocking(int fd);

/**
 * Function that makes the server start listening on the socket.
 *
 * @param 	fd		[int]		        "The filedescriptor associated to some user"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_socket_listen(int fd);

/**
 * Function that creates a threadpool which can be used handle traffic.
 *
 * @param 	server	[wss_server_t *]	"The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_socket_threadpool(wss_server_t *server);

#endif
