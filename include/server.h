#ifndef wss_server_h
#define wss_server_h

#ifndef WSS_SERVER_VERSION
#define WSS_SERVER_VERSION "2.0.0"
#endif

#include <signal.h>
#include <sys/epoll.h> 			/* epoll_event, epoll_create, epoll_ctl */
#include <sys/socket.h>         /* socket, setsockopt, inet_ntoa, accept, shutdown */
#include <netinet/in.h>         /* sockaddr_in, inet_ntoa */
#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#endif
#include <pthread.h> 			/* pthread_create, pthread_t, pthread_attr_t
                                   pthread_mutex_init */

#include "pool.h"
#include "config.h"
#include "uthash.h"

typedef enum {
    STARTING,
    RUNNING,
    SHUTDOWN,
    SHUTDOWN_SUCCESS,
    SHUTDOWN_ERROR
} state_t;

typedef struct {
    int port;
    int fd;
    int epoll_fd;
    config_t *config;
#ifdef USE_OPENSSL
    SSL_CTX *ssl_ctx;
#endif
    threadpool_t **pool;
    struct epoll_event *events;
    struct sockaddr_in info;
    pthread_t thread_id;
    sigset_t mask;
} server_t;

typedef struct {
    state_t state;
    pthread_mutex_t lock;
} server_state_t;

/**
 * Function that updates the state of the server.
 *
 * @param 	s	[state_t *]     "A state_t value describing the servers state"
 * @return 	    [void]
 */
void server_set_state(state_t state);

/**
 * Function that loops over epoll events and distribute the events to different
 * threadpools.
 *
 * @param 	arg				[void *] 	"Is in fact a server_t instance"
 * @return 	pthread_exit 	[void *] 	"0 if successfull and otherwise <0"
 */
void *server_run(void *arg);

/**
 * Starts the websocket server.
 *
 * @param   config  [config_t *] "The configuration of the server"
 * @return 	        [int]        "EXIT_SUCCESS if successfull or EXIT_FAILURE on error"
 */
int server_start(config_t *config);

server_state_t state;

#endif
