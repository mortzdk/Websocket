#ifndef wss_socket_h
#define wss_socket_h

#include "server.h"
#include "error.h"

/**
 * Function that initializes a socket and store the filedescriptor.
 *
 * @param 	server	[server_t *]    "The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_create(server_t *server);

/**
 * Function that enables reuse of the port if the server shuts down.
 *
 * @param 	fd		[int]		    "The filedescriptor associated to some user"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_reuse(int fd);

/**
 * Function that binds the socket to a specific port and chooses IPV4.
 *
 * @param 	server	[server_t *]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_bind(server_t *server);

/**
 * Function that makes the socket non-blocking for both reads and writes.
 *
 * @param 	fd		[int]		    "The filedescriptor associated to some user"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_non_blocking(int fd);

/**
 * Function that makes the server start listening on the socket.
 *
 * @param 	fd		[int]		    "The filedescriptor associated to some user"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_listen(int fd);

/**
 * Function that creates epoll instance and adding the filedescriptor of the
 * servers socket to it.
 *
 * @param 	server	[server_t *]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_epoll(server_t *server);

/**
 * Function that creates a threadpool which can be used handle traffic.
 *
 * @param 	server	[server_t *]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_threadpool(server_t *server);

/**
 * Function that waits for new events on the filedescriptors associated to the
 * epoll instance.
 *
 * @param 	server	[server_t *]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_wait(server_t *server);

#endif
