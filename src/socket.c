#include <errno.h> 				/* errno */
#include <string.h>             /* strerror, memset, strncpy, memcpy */
#include <unistd.h>             /* close */
#include <fcntl.h> 				/* fcntl */

#include <sys/types.h>          /* socket, setsockopt, accept, send, recv */
#include <sys/socket.h>         /* socket, setsockopt, inet_ntoa, accept */
#include <netinet/in.h>         /* sockaddr_in, inet_ntoa */
#include <arpa/inet.h>          /* htonl, htons, inet_ntoa */

#include "socket.h"
#include "log.h"
#include "error.h"
#include "threadpool.h"
#include "alloc.h"
#include "core.h"

/**
 * Function that initializes a socket and store the filedescriptor.
 *
 * @param 	server	[server_t  **]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t WSS_socket_create(wss_server_t *server) {
    if ( unlikely(NULL == server) ) {
        WSS_log_fatal("No server structure given");
        return WSS_SOCKET_CREATE_ERROR;
    }

    if ( unlikely((server->fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) ) {
        WSS_log_fatal("Unable to create server filedescriptor: %s", strerror(errno));
        return WSS_SOCKET_CREATE_ERROR;
    }

    return WSS_SUCCESS;
}

/**
 * Function that enables reuse of the port if the server shuts down.
 *
 * @param 	fd		[int]		    "The filedescriptor associated to some user"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t WSS_socket_reuse(int fd) {
    int reuse = 1;
    if ( unlikely((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) < 0) ){
        WSS_log_fatal("Unable to reuse port: %s", strerror(errno));
        return WSS_SOCKET_REUSE_ERROR;
    }
    return WSS_SUCCESS;
}

/**
 * Function that binds the socket to a specific port and chooses IPV6.
 *
 * @param 	server	[server_t *]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t WSS_socket_bind(wss_server_t *server) {
    if ( unlikely(NULL == server) ) {
        WSS_log_fatal("No server structure given");
        return WSS_SOCKET_BIND_ERROR;
    }

    if ( unlikely(server->port < 0 || server->port > 65535) ) {
        WSS_log_fatal("Server port must be within range [0, 65535]");
        server->fd = -1;
        return WSS_SOCKET_BIND_ERROR;
    }

    /**
     * Setting values of our server structure
     */
    memset((char *) &server->info, '\0', sizeof(server->info));
    server->info.sin6_family = AF_INET6;
    server->info.sin6_port   = htons(server->port);
    server->info.sin6_addr   = in6addr_any;

    /**
     * Binding address.
     */
    if ( unlikely((bind(server->fd, (struct sockaddr *) &server->info,
                    sizeof(server->info))) < 0) ) {
        WSS_log_fatal("Unable to bind socket to port: %s", strerror(errno));
        server->fd = -1;
        return WSS_SOCKET_BIND_ERROR;
    }
    return WSS_SUCCESS;
}

/**
 * Function that makes the socket non-blocking for both reads and writes.
 *
 * @param 	fd		[int]		    "The filedescriptor associated to some user"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t WSS_socket_non_blocking(int fd) {
    int flags;

    if ( unlikely((flags = fcntl(fd, F_GETFL, 0)) < 0)) {
        WSS_log_fatal("Unable to fetch current filedescriptor flags: %s", strerror(errno));
        return WSS_SOCKET_NONBLOCKED_ERROR;
    }

    flags |= O_NONBLOCK;

    if ( unlikely((flags = fcntl(fd, F_SETFL, flags)) < 0) ) {
        WSS_log_fatal("Unable to set flags for filedescriptor: %s", strerror(errno));
        return WSS_SOCKET_NONBLOCKED_ERROR;
    }

    return WSS_SUCCESS;
}

/**
 * Function that makes the server start listening on the socket.
 *
 * @param 	fd		[int]	    	"The filedescriptor associated to some user"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t WSS_socket_listen(int fd) {
    /**
     * Listen on the server socket for connections
     */
    if ( unlikely((listen(fd, SOMAXCONN)) < 0) ) {
        WSS_log_fatal("Unable to listen on filedescriptor: %s", strerror(errno));
        return WSS_SOCKET_LISTEN_ERROR;
    }
    return WSS_SUCCESS;
}

/**
 * Function that creates a threadpool which can be used handle traffic.
 *
 * @param 	workers	    [unsigned int]	    "Amount of workers"
 * @param 	tasks	    [unsigned int]	    "Amount of worker tasks"
 * @param 	stack_size	[unsigned int]	    "Stack size for each thread"
 * @param 	pool	    [threadpool_t **]	"A pointer to where the pool should be stored"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_socket_threadpool(unsigned int workers, unsigned int tasks, size_t stack_size, threadpool_t **pool) {
    size_t default_stack_size;
    pthread_attr_t attr;
    threadpool_t *p;

    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr, &default_stack_size);
    WSS_log_info("Default thread stack size %d", default_stack_size);
    pthread_attr_destroy(&attr);

    /**
     * Creating threadpool
     */
    if ( unlikely(NULL == (p = threadpool_create(workers, tasks, stack_size, 0))) ) {
        WSS_log_fatal("The threadpool failed to initialize");
        return WSS_THREADPOOL_CREATE_ERROR;
    }

    WSS_log_info("Configured thread stack size %llu", threadpool_get_stack_size(p));

    *pool = p;

    return WSS_SUCCESS;
}
