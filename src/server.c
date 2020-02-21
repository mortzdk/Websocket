#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <errno.h> 				/* errno */
#include <unistd.h>             /* close */
#include <stdlib.h>             /* EXIT_SUCCESS, EXIT_FAILURE */
#include <signal.h>             /* signal */
#include <sys/epoll.h> 			/* EPOLLHUB, EPOLLERR, EPOLL, EPOLLRDHUB, EPOLLIN EPOLLOUT */
#include <stdio.h>

#include "server.h"
#include "log.h"
#include "session.h"
#include "alloc.h"
#include "socket.h"
#include "pool.h"               /* threadpool_add, threadpool_strerror */
#include "comm.h"
#include "http.h"
#include "error.h"
#include "config.h"
#include "subprotocols.h"
#include "predict.h"

/**
 * Global state of server
 */
server_state_t state;

/**
 * Function that updates the state of the server.
 *
 * @param 	s	[state_t *]     "A state_t value describing the servers state"
 * @return 	    [void]
 */
void server_set_state(state_t s) {
    pthread_mutex_lock(&state.lock);
    state.state = s;
    pthread_mutex_unlock(&state.lock);
}

/**
 * Function that adds task-function and data instance to worker pool.
 *
 * @param 	server	[server_t *] 	    "A server_t instance"
 * @param 	func	[void (*)(void *)] 	"A function pointer"
 * @param 	args	[void *] 	        "Arguments to be served to the function"
 * @return 			[wss_error_t]       "The error status"
 */
static wss_error_t server_add_to_pool(server_t *server, void (*func)(void *, int), void *args, int queues) {
    int err, c = rand() % queues;

    do {
        err = threadpool_add(
                server->pool[c],
                func,
                args,
                0
                );

        if (unlikely(err != 0 && err != threadpool_queue_full)) {
            WSS_log_fatal("Threadpool returned with error: %s", threadpool_strerror(err));
            return THREADPOOL_ERROR;
        }

        c++;
        c = c % queues;
    } while (unlikely(err != 0));

    return SUCCESS;
}

/**
 * Function that loops over epoll events and distribute the events to different
 * threadpools.
 *
 * @param 	arg				[void *] 	"Is in fact a server_t instance"
 * @return 	pthread_exit 	[void *] 	"0 if successfull and otherwise <0"
 */
void *server_run(void *arg) {
    wss_error_t err;
    server_t *server = (server_t *) arg;
    int queues = server->config->pool_queues;
    struct epoll_event event;
    int n = 0, i = 0;

    if ( unlikely(NULL == (server->events = WSS_calloc(server->config->pool_size, sizeof(event)))) ) {
        WSS_log_fatal("Unable to calloc server epoll events");
        pthread_exit( ((void *) MEMORY_ERROR) );
    }

    // Listen for epoll events
    while (1) {
        if (unlikely(state.state != RUNNING)) {
#ifdef USE_OPENSSL
            if (server->ssl_ctx != NULL) {
                WSS_log_trace("Stopping HTTPS server");
            } else {
#endif
                WSS_log_trace("Stopping HTTP server");
#ifdef USE_OPENSSL
            }
#endif
            break;
        }

        if ( unlikely((n = socket_wait(server)) < 0) ) {
            pthread_exit( ((void *) SOCKET_WAIT_ERROR) );
        } 
        
        for (i = 0; i < n; i++) {
            if ( unlikely((server->events[i].events & EPOLLHUP) ||
                    (server->events[i].events & EPOLLERR) ||
                    (server->events[i].events & EPOLLRDHUP)) ) {
                if ( unlikely(server->events[i].data.fd == server->fd) ) {
                    WSS_log_fatal("Epoll reported error on the servers filedescriptor");
                    pthread_exit( ((void *) EPOLL_ERROR) );
                } else {
                    args_t *args = (args_t *) WSS_malloc(sizeof(args_t));
                    if ( unlikely(NULL == args) ) {
                        WSS_log_fatal("Unable to allocate args structure");
                        pthread_exit( ((void *) MEMORY_ERROR) );
                    }
                    args->server = server;
                    args->fd = server->events[i].data.fd;

                    if ( unlikely((err = server_add_to_pool(server, &WSS_disconnect,
                                    (void *)args, queues)) != 0) ) {
                        WSS_log_fatal("Failed adding job to worker pool");
                        WSS_free((void **)&args);
                        pthread_exit( ((void *) EPOLL_CONN_ERROR) );
                    }
                }
            } else if (server->events[i].data.fd == server->fd) {
                /**
                 * If new session has connected
                 */
                if ( unlikely((err = server_add_to_pool(server, &WSS_connect,
                                (void *)server, queues)) != 0) ) {
                    WSS_log_fatal("Failed adding job to worker pool");
                    pthread_exit( ((void *) EPOLL_CONN_ERROR) );
                }
            } else {
                if ( server->events[i].events & EPOLLIN ) {
                    /**
                     * If new reads are ready
                     */
                    args_t *args = (args_t *) WSS_malloc(sizeof(args_t));
                    if ( unlikely(NULL == args) ) {
                        WSS_log_fatal("Unable to allocate args structure");
                        pthread_exit( ((void *) MEMORY_ERROR) );
                    }
                    args->server = server;
                    args->fd = server->events[i].data.fd;

                    if ( unlikely((err = server_add_to_pool(server, &WSS_read,
                                    (void *)args, queues)) != 0) ) {
                        WSS_log_fatal("Failed adding job to worker pool");
                        WSS_free((void **)&args);
                        pthread_exit( ((void *) EPOLL_READ_ERROR) );
                    }
                }

                if ( server->events[i].events & EPOLLOUT ) {
                    /**
                     * If new writes are ready
                     */
                    args_t *args = (args_t *) WSS_malloc(sizeof(args_t));
                    if ( unlikely(NULL == args) ) {
                        WSS_log_fatal("Unable to allocate args structure");
                        pthread_exit( ((void *) MEMORY_ERROR) );
                    }
                    args->server = server;
                    args->fd = server->events[i].data.fd;

                    if ( unlikely((err = server_add_to_pool(server, &WSS_write,
                                    (void *)args, queues)) != 0) ) {
                        WSS_log_fatal("Failed adding job to worker pool");
                        free(args);
                        pthread_exit( ((void *) EPOLL_WRITE_ERROR) );
                    }
                }
            }
        }
    }

    pthread_exit( ((void *) ((uintptr_t)SUCCESS)) );
}

/**
 * Handler to call when some specific interrupts are happening. This function
 * shuts down the server in a safe way or ignore certain interrupts.
 *
 * @param   sig     [int]   "The integer value of the interrupt"
 * @return 	        [void]
 */
static void server_interrupt(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGSEGV:
        case SIGILL:
        case SIGFPE:
            if (state.state != SHUTDOWN) {
                WSS_log_trace("Server is shutting down gracefully");
                server_set_state(SHUTDOWN);

                // To signal second server thread
                kill(getpid(), SIGINT);
            }
            break;
        case SIGPIPE:
            break;
        default:
            return;
    }
}

/**
 * Function that frees all memory that the server has allocated
 *
 * @return 	        [int]        "EXIT_SUCCESS if successfull or EXIT_FAILURE on error"
 */
static int server_free(server_t *server) {
    int result = EXIT_SUCCESS;

    if (http_server_free(server) != SUCCESS) {
        result = EXIT_FAILURE;
    }

    /**
     * Freeing memory from server instance
     */
    if (NULL != server) {
        free(server);
    }

    return result;
}

/**
 * Starts the websocket server.
 *
 * @param   config  [config_t *] "The configuration of the server"
 * @return 	        [int]        "EXIT_SUCCESS if successfull or EXIT_FAILURE on error"
 */
int server_start(config_t *config) {
    int err;
    server_t *http = NULL;
    server_t *https = NULL;
    int ret = EXIT_SUCCESS;
    struct sigaction sa;
    sigset_t mask, orig_mask;

    if ( unlikely((err = pthread_mutex_init(&state.lock, NULL)) != 0) ) {
        server_free(http);
        server_free(https);

        WSS_log_fatal("Failed initializing state lock: %s", strerror(err));

        return EXIT_FAILURE;
    }

    // Load extensions available
    load_extensions(config);

    // Load subprotocols available
    load_subprotocols(config);

    // Setting starting state
    WSS_log_trace("Starting server");
    server_set_state(STARTING);

    // Listening for signals that could terminate program
    WSS_log_trace("Listening for signals");

    // Blocking signals
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGSEGV);
    sigaddset(&mask, SIGFPE);
    sigaddset(&mask, SIGPIPE);

    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
        exit(EXIT_FAILURE);
    }

    // Set up the structure to specify the new action
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = server_interrupt;
    sa.sa_flags = 0;

    if (sigaction (SIGINT, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGINT signal: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction (SIGSEGV, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGSEGV signal: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction (SIGILL, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGILL signal: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction (SIGHUP, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGHUB signal: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction (SIGFPE, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGFPE signal: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction (SIGPIPE, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGPIPE signal: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    /*
    (void) signal(SIGINT , &server_interrupt);
    (void) signal(SIGSEGV, &server_interrupt);
    (void) signal(SIGILL, &server_interrupt);
    (void) signal(SIGFPE, &server_interrupt);
    (void) signal(SIGPIPE, &server_interrupt);
    */

    WSS_log_trace("Initializing sessions");
    if ( unlikely(SUCCESS != WSS_session_init_lock()) ) {
        WSS_log_fatal("Unable to initialize session mutex");

        return EXIT_FAILURE;
    }

    WSS_log_trace("Allocating memory for HTTP instance");

    if ( unlikely(NULL == (http = WSS_malloc(sizeof(server_t)))) ) {
        WSS_log_fatal("Unable to allocate server structure");

        return EXIT_FAILURE;
    }
    http->mask = orig_mask;

    WSS_log_trace("Setting to running state");

    server_set_state(RUNNING);

    WSS_log_trace("Creating HTTP Instance");

    http->config       = config;
    http->port         = config->port_http;
    if ( unlikely(SUCCESS != http_server(http)) ) {
        server_free(http);

        WSS_log_fatal("Unable to initialize http server");

        return EXIT_FAILURE;
    }

#ifdef USE_OPENSSL
    bool ssl = NULL != config->ssl_cert && NULL != config->ssl_key && (NULL != config->ssl_ca_file || NULL != config->ssl_ca_path);
    if (ssl) {
        WSS_log_trace("Allocating memory for HTTPS instance");

        if ( unlikely(NULL == (https = (server_t *) WSS_malloc(sizeof(server_t)))) ) {
            server_free(http);

            WSS_log_fatal("Unable to allocate https server structure");

            return EXIT_FAILURE;
        }

        https->mask = orig_mask;

        WSS_log_trace("Creating HTTPS Instance");

        https->config       = config;
        https->port         = config->port_https;
        if ( unlikely(SUCCESS != http_ssl(https)) ) {
            server_free(http);
            server_free(https);

            WSS_log_fatal("Unable to establish ssl context");

            return EXIT_FAILURE;
        }

        if ( unlikely(SUCCESS != http_server(https)) ) {
            server_free(http);
            server_free(https);

            WSS_log_fatal("Unable to initialize https server");

            return EXIT_FAILURE;
        }
    }
#endif

    WSS_log_trace("Joining server threads");

    pthread_join(http->thread_id, (void **) &err);
    if ( unlikely(SUCCESS != err) ) {
        WSS_log_error("HTTP Server thread returned with error: %s", strerror(err));
        server_set_state(SHUTDOWN_ERROR);
    }

    WSS_log_trace("HTTP server thread has shutdown");

#ifdef USE_OPENSSL
    if (ssl) {
        pthread_join(https->thread_id, (void **) &err);
        if ( unlikely(SUCCESS != err) ) {
            WSS_log_error("HTTPS Server thread returned with error: %s", strerror(err));
            server_set_state(SHUTDOWN_ERROR);
        }

        WSS_log_trace("HTTPS server thread has shutdown");
    }
#endif

    if ( unlikely(server_free(http) != 0) ) {
        server_set_state(SHUTDOWN_ERROR);
    }

    WSS_log_trace("Freed memory associated with HTTP server instance");

#ifdef USE_OPENSSL
    if (ssl) {
        if ( unlikely(server_free(https) != 0) ) {
            server_set_state(SHUTDOWN_ERROR);
        }

        WSS_log_trace("Freed memory associated with HTTPS server instance");
    }
#endif

    if ( unlikely(WSS_session_delete_all() != SUCCESS) ) {
        server_set_state(SHUTDOWN_ERROR);
    }

    WSS_log_trace("Freed all sessions");

    if ( unlikely(WSS_session_destroy_lock() != SUCCESS) ) {
        server_set_state(SHUTDOWN_ERROR);
    }

    WSS_log_trace("Destroyed session lock");

    if ( unlikely(state.state == SHUTDOWN_ERROR) ) {
        ret = EXIT_FAILURE;
    }

    destroy_subprotocols();

    destroy_extensions();
    
    pthread_mutex_destroy(&state.lock);

    return ret;
}
