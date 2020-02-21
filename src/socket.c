#include <errno.h> 				/* errno */
#include <string.h>             /* strerror, memset, strncpy, memcpy */
#include <unistd.h>             /* close */
#include <fcntl.h> 				/* fcntl */

#include <sys/epoll.h> 			/* epoll_event, epoll_create1, epoll_ctl */
#include <sys/types.h>          /* socket, setsockopt, accept, send, recv */
#include <sys/socket.h>         /* socket, setsockopt, inet_ntoa, accept */
#include <netinet/in.h>         /* sockaddr_in, inet_ntoa */
#include <arpa/inet.h>          /* htonl, htons, inet_ntoa */

#include "socket.h"
#include "log.h"
#include "error.h"
#include "pool.h"
#include "alloc.h"
#include "predict.h"

/**
 * Function that initializes a socket and store the filedescriptor.
 *
 * @param 	server	[server_t  **]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_create(server_t *server) {
    if ( unlikely((server->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) ) {
        WSS_log_fatal("Unable to create server filedescriptor: %s", strerror(errno));
        return SOCKET_CREATE_ERROR;
    }
    return SUCCESS;
}

/**
 * Function that enables reuse of the port if the server shuts down.
 *
 * @param 	fd		[int]		    "The filedescriptor associated to some user"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_reuse(int fd) {
    int reuse = 1;
    if ( unlikely((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) < 0) ){
        WSS_log_fatal("Unable to reuse port: %s", strerror(errno));
        return SOCKET_REUSE_ERROR;
    }
    return SUCCESS;
}

/**
 * Function that binds the socket to a specific port and chooses IPV4.
 *
 * @param 	server	[server_t *]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_bind(server_t *server) {
    /**
     * Setting values of our server structure
     */
    memset((char *) &server->info, '\0', sizeof(server->info));
    server->info.sin_family = AF_INET;
    server->info.sin_port = htons(server->port);
    server->info.sin_addr.s_addr = htonl(INADDR_ANY);

    /**
     * Binding address.
     */
    if ( unlikely((bind(server->fd, (struct sockaddr *) &server->info,
                    sizeof(server->info))) < 0) ) {
        WSS_log_fatal("Unable to bind socket to port: %s", strerror(errno));
        server->fd = -1;
        return SOCKET_BIND_ERROR;
    }
    return SUCCESS;
}

/**
 * Function that makes the socket non-blocking for both reads and writes.
 *
 * @param 	fd		[int]		    "The filedescriptor associated to some user"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_non_blocking(int fd) {
    int flags;

    if ( unlikely((flags = fcntl(fd, F_GETFL, 0)) < 0)) {
        WSS_log_fatal("Unable to fetch current filedescriptor flags: %s", strerror(errno));
        return SOCKET_NONBLOCKED_ERROR;
    }

    flags |= O_NONBLOCK;

    if ( unlikely((flags = fcntl(fd, F_SETFL, flags)) < 0) ) {
        WSS_log_fatal("Unable to set flags for filedescriptor: %s", strerror(errno));
        return SOCKET_NONBLOCKED_ERROR;
    }

    return SUCCESS;
}

/**
 * Function that makes the server start listening on the socket.
 *
 * @param 	fd		[int]	    	"The filedescriptor associated to some user"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_listen(int fd) {
    /**
     * Listen on the server socket for connections
     */
    if ( unlikely((listen(fd, SOMAXCONN)) < 0) ) {
        WSS_log_fatal("Unable to listen on filedescriptor: %s", strerror(errno));
        return SOCKET_LISTEN_ERROR;
    }
    return SUCCESS;
}

/**
 * Function that creates epoll instance and adding the filedescriptor of the
 * servers socket to it.
 *
 * @param 	server	[server_t *]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_epoll(server_t *server) {
    struct epoll_event event;

    memset(&event, 0, sizeof(event));

    /**
     * Creating epoll instance.
     */
    if ( unlikely((server->epoll_fd = epoll_create1(0)) < 0) ) {
        WSS_log_fatal("Unable to create server epoll structure: %s", strerror(errno));
        server->epoll_fd = -1;
        return EPOLL_ERROR;
    }

    /**
     * Setting values of our epoll structure
     */
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    event.data.fd = server->fd;

    if ( unlikely((epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, server->fd, &event)) < 0) ) {
        WSS_log_fatal("Unable to add servers filedescriptor to epoll: %s", strerror(errno));
        return EPOLL_ERROR;
    }

    return SUCCESS;
}

/**
 * Function that creates a threadpool which can be used handle traffic.
 *
 * @param 	server	[server_t *]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_threadpool(server_t *server) {
    unsigned int i;

    
    if ( unlikely(NULL == (server->pool = WSS_calloc(server->config->pool_queues, sizeof(threadpool_t *)))) ) {
        return MEMORY_ERROR;
    }

    /**
     * Creating threadpool
     */
    for (i = 0; likely(i < server->config->pool_queues); i++) {
        if ( unlikely(NULL == (server->pool[i] = threadpool_create(server->config->pool_workers,
                        server->config->pool_size, server->config->size_thread, i*server->config->pool_workers))) ) {
            WSS_log_fatal("The threadpool failed to initialize");
            return THREADPOOL_ERROR;
        }
    }

    return SUCCESS;
}

/**
 * Function that waits for new events on the filedescriptors associated to the
 * epoll instance.
 *
 * @param 	server	[server_t *]	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t socket_wait(server_t *server) {
    int n = epoll_pwait(server->epoll_fd, server->events, server->config->pool_size, -1, &server->mask);

    // Wait for 1000 milliseconds
    //int n = epoll_wait(server->epoll_fd, server->events, server->config->pool_size, 1000);

    if ( unlikely(n < 0) ) {
        if (errno != EINTR) {
            WSS_log_fatal("Failed waiting for epoll: %s", strerror(errno));
            return SOCKET_WAIT_ERROR;
        }

        return 0;
    }

    return n;
}
