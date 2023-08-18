#pragma once

#ifndef WSS_SERVER_H
#define WSS_SERVER_H

#include <signal.h>
#include <sys/socket.h>         /* socket, setsockopt, inet_ntoa, accept, shutdown */
#include <netinet/in.h>         /* sockaddr_in, inet_ntoa */
#include <regex.h>              /* regex_t, regcomp, regexec */
#include <pthread.h> 			/* pthread_create, pthread_t, pthread_attr_t
                                   pthread_mutex_init */
#include "threadpool.h"
#include "memorypool.h"
#include "config.h"

#if !defined(uthash_malloc) || !defined(uthash_free)
#include "alloc.h"

#ifndef uthash_malloc
#define uthash_malloc(sz) WSS_malloc(sz)   /* malloc fcn */
#endif

#ifndef uthash_free
#define uthash_free(ptr,sz) WSS_free((void **)&ptr) /* free fcn */
#endif
#endif

#ifndef WSS_SERVER_VERSION
#define WSS_SERVER_VERSION "v2.0.0"
#endif

#include "uthash.h"

typedef enum {
    STARTING,
    RUNNING,
    HALTING,
    HALT_ERROR
} wss_state_t;

typedef struct {
    int port;
    int fd;
    int poll_fd;
    int max_fd;
    wss_config_t *config;
    void *ssl_ctx;
    threadpool_t *pool_connect;
    threadpool_t *pool_io;
    void *events;
    struct sockaddr_in6 info;
    pthread_t thread_id;
    pthread_t cleanup_thread_id;
    pthread_mutex_t lock;
#if defined(WSS_POLL)
    int rearm_pipefd[2];
#endif //WSS_POLL
    regex_t re;
    wss_memorypool_t *thread_args_pool;
    wss_memorypool_t *frame_pool;
    wss_memorypool_t *message_pool;
} wss_server_t;

typedef struct {
    wss_state_t state;
    pthread_mutex_t lock;
} wss_server_state_t;

typedef struct {
    wss_server_t http;
    wss_server_t https;
} wss_servers_t;

/**
 * Function that updates the state of the server.
 *
 * @param 	s	[wss_state_t *]     "A state_t value describing the servers state"
 * @return 	    [void]
 */
void WSS_server_set_state(wss_state_t state);

void WSS_server_set_max_fd(wss_server_t *server, int fd);

/**
 * Function that loops over poll events and distribute the events to different
 * threadpools.
 *
 * @param 	arg				[void *] 	"Is in fact a server_t instance"
 * @return 	pthread_exit 	[void *] 	"0 if successfull and otherwise <0"
 */
void *WSS_server_run(void *arg);

/**
 * Starts the websocket server.
 *
 * @param   config  [wss_config_t *] "The configuration of the server"
 * @return 	        [int]            "EXIT_SUCCESS if successfull or EXIT_FAILURE on error"
 */
int WSS_server_start(wss_config_t *config);

extern wss_server_state_t state;

extern wss_servers_t servers;

#endif
