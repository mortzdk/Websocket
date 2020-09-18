#include <errno.h> 				/* errno */
#include <unistd.h>             /* close */
#include <stdlib.h>             /* EXIT_SUCCESS, EXIT_FAILURE */
#include <signal.h>             /* signal */
#include <stdio.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "server.h"
#include "frame.h"
#include "event.h"               
#include "log.h"
#include "session.h"
#include "alloc.h"
#include "socket.h"
#include "pool.h"               /* threadpool_add, threadpool_strerror */
#include "worker.h"
#include "http.h"
#include "error.h"
#include "config.h"
#include "subprotocols.h"
#include "extensions.h"
#include "predict.h"

/**
 * Global state of server
 */
wss_server_state_t state;

/**
 * Global servers available
 */
wss_servers_t servers;

/**
 * Global variable holding current timestamp 
 */
struct timespec now;

static inline void write_control_frame(wss_frame_t *frame, wss_session_t *session) {
    size_t j;
    char *message;
    size_t message_length;
    int n = 0;
    size_t written = 0;
    int fd = session->fd;

    // Use extensions
    for (j = 0; likely(j < session->header->ws_extensions_count); j++) {
        session->header->ws_extensions[j]->ext->outframes(
                fd,
                &frame,
                1);

        session->header->ws_extensions[j]->ext->outframe(
                fd,
                frame);
    }

    if (0 == (message_length = WSS_stringify_frame(frame, &message))) {
        WSS_log_fatal("Unable to represent frame as bytes");
        WSS_free_frame(frame);
        return;
    }

    WSS_free_frame(frame);

#ifdef USE_OPENSSL
    if (session->ssl_connected) {
        do {
            n = SSL_write(session->ssl, message+written, message_length-written);
            if (n < 0) {
                break;
            }
            written += n;
        } while ( written < message_length );
    } else {
#endif
        do {
            n = write(fd, message+written, message_length-written);
            if ( unlikely(n < 0) ) {
                if ( likely(errno == EINTR) ) {
                    errno = 0;
                    n = 0;
                    continue;
                }
                break;
            }
            written += n;
        } while ( written < message_length );
#ifdef USE_OPENSSL
    }
#endif

    WSS_free((void **) &message);
}

static void cleanup_session(wss_session_t *session) {
    wss_error_t err;
    wss_frame_t *frame;
    long unsigned int ms;
    int fd = session->fd;
    wss_server_t *server = servers.http;
    bool dc;

    if (! session->handshaked) {
        return;
    }

#ifdef USE_OPENSSL
    if (session->ssl && session->ssl_connected) {
        server = servers.https;
    }
#endif

    ms = (((now.tv_sec - session->alive.tv_sec)*1000)+(now.tv_nsec/1000000)) - (session->alive.tv_nsec/1000000);

    WSS_log_info("Check timeout %d ms", ms);

    // Session timed out
    if ( unlikely(server->config->timeout_client >= 0 && ms >= (long unsigned int)server->config->timeout_client) ) {
        WSS_session_is_disconnecting(session, &dc);
        if (dc) {
            return;
        }

        WSS_session_jobs_wait(session);

        WSS_log_info("Session %d has timedout last active %d ms ago", fd, ms);

        session->closing = true;
        session->state = CLOSING;

#ifdef USE_OPENSSL
        if (! session->ssl_connected) {
#endif
            WSS_log_trace("Session %d disconnected from ip: %s:%d using HTTP request, due to timing out", session->fd, session->ip, session->port);
#ifdef USE_OPENSSL
        } else {
            WSS_log_trace("Session %d disconnected from ip: %s:%d using HTTPS request, due to timing out", session->fd, session->ip, session->port);
        }
#endif

        WSS_log_trace("Informing subprotocol of client with file descriptor %d disconnecting", session->fd);

        if ( NULL != session->header && session->header->ws_protocol != NULL ) {
            WSS_log_trace("Informing subprotocol about session close");
            session->header->ws_protocol->close(session->fd);
        }

        WSS_log_trace("Removing poll filedescriptor from eventlist");

        WSS_poll_remove(server, fd);

        WSS_log_trace("Sending close frame", fd);

        frame = WSS_closing_frame(CLOSE_TRY_AGAIN, NULL);

        write_control_frame(frame, session);

        WSS_log_trace("Deleting client session");

        if ( unlikely(WSS_SUCCESS != (err = WSS_session_delete_no_lock(session))) ) {
            switch (err) {
                case WSS_SSL_SHUTDOWN_READ_ERROR:
                        WSS_poll_set_read(server, fd);
                    return;
                case WSS_SSL_SHUTDOWN_WRITE_ERROR:
                        WSS_poll_set_write(server, fd);
                    return;
                default:
                    break;
            }
            WSS_log_error("Unable to delete client session, received error code: %d", err);
            return;
        }

        WSS_log_info("Client with session %d disconnected", fd);

    // Ping session to keep it alive
    } else if (server->config->timeout_pings > 0 ) {
        WSS_log_info("Pinging session %d", fd);

        frame = WSS_ping_frame();
        if (session->pong != NULL) {
            WSS_free((void **) &session->pong);
            session->pong_length = 0;
        }

        if (NULL == (session->pong = WSS_malloc(frame->applicationDataLength))) {
            WSS_free_frame(frame);
            return;
        }
        session->pong_length = frame->applicationDataLength;

        memcpy(session->pong, frame->payload+frame->extensionDataLength, frame->applicationDataLength);

        write_control_frame(frame, session);
    }
}

/**
 * Function that updates the state of the server.
 *
 * @param 	s	[wss_state_t *]     "A state_t value describing the servers state"
 * @return 	    [void]
 */
void WSS_server_set_state(wss_state_t s) {
    pthread_mutex_lock(&state.lock);
    state.state = s;
    pthread_mutex_unlock(&state.lock);
}

/**
 * Sets the maximal file descriptor seen by the server
 *
 * @param 	server	[wss_server_t *]    "The server object"
 * @param 	fd	    [int]               "The file descriptor seen"
 * @return 	        [void]
 */
void WSS_server_set_max_fd(wss_server_t *server, int fd) {
    pthread_mutex_lock(&server->lock);
    if (server->max_fd < fd) {
        server->max_fd = fd;
    }
    pthread_mutex_unlock(&server->lock);
}

/**
 * Cleanup client sessions, that is, closes connections to clients which is 
 * timedout based on the servers timeout_client setting.
 *
 * @return 	pthread_exit 	[void *] 	"0 if successfull and otherwise <0"
 */
void *WSS_cleanup() {
    int n;
    wss_error_t err;
    struct pollfd fds[1];
    wss_server_t *server = servers.http;
    long int timeout = server->config->timeout_client;

#ifdef USE_RPMALLOC
    rpmalloc_thread_initialize();
#endif

    fds[0].fd = close_pipefd[0];
    fds[0].events = POLLIN | POLLPRI;

    WSS_log_info("Cleanup thread started with %ld", timeout);

    if (timeout >= 0 && server->config->timeout_pings > 0) {
        timeout = timeout/server->config->timeout_pings;
        WSS_log_info("Cleanup thread will run every %ld milliseconds", timeout);
    }

    while ( likely(state.state == RUNNING) ) {
        n = poll(fds, 1, timeout);
        if ( unlikely(n < 0 && errno != EINTR) ) {
#ifdef USE_RPMALLOC
            rpmalloc_thread_finalize();
#endif
            pthread_exit( ((void *) ((uintptr_t)WSS_CLEANUP_ERROR)) );
        } else if ( likely(n == 0) ) {
            WSS_log_info("Pings clients and cleanup timedout ones");
            
            clock_gettime(CLOCK_MONOTONIC, &now);
            if ( unlikely(WSS_SUCCESS != (err = WSS_session_all(cleanup_session))) ) {
#ifdef USE_RPMALLOC
                rpmalloc_thread_finalize();
#endif
                pthread_exit( ((void *) &err) );
            }
        }
    }

    WSS_log_info("Cleanup thread shutting down");

#ifdef USE_RPMALLOC
    rpmalloc_thread_finalize();
#endif
    pthread_exit( ((void *) ((uintptr_t)WSS_SUCCESS)) );
}

/**
 * Function that loops over poll events and distribute the events to different
 * threadpools.
 *
 * @param 	arg				[void *] 	"Is in fact a wss_server_t instance"
 * @return 	pthread_exit 	[void *] 	"0 if successfull and otherwise <0"
 */
void *WSS_server_run(void *arg) {
    wss_error_t err;
    wss_server_t *server = (wss_server_t *) arg;

#ifdef USE_RPMALLOC
    rpmalloc_thread_initialize();
#endif

#ifdef USE_OPENSSL
    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Running HTTPS server");
    } else {
#endif
        WSS_log_trace("Running HTTP server");
#ifdef USE_OPENSSL
    }
#endif

    if ( unlikely(WSS_SUCCESS != (err = WSS_poll_init(server))) ) {
#ifdef USE_RPMALLOC
        rpmalloc_thread_finalize();
#endif
        pthread_exit( ((void *) err) );
    }

    // Listen for poll events
    while ( likely(state.state == RUNNING) ) {
        err = WSS_poll_delegate(server);
        if ( unlikely(err != WSS_SUCCESS) ) {
#ifdef USE_RPMALLOC
            rpmalloc_thread_finalize();
#endif
            pthread_exit( ((void *) err) );
        }
    }

#ifdef USE_OPENSSL
    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Stopping HTTPS server");
    } else {
#endif
        WSS_log_trace("Stopping HTTP server");
#ifdef USE_OPENSSL
    }
#endif

#ifdef USE_RPMALLOC
    rpmalloc_thread_finalize();
#endif
    pthread_exit( ((void *) ((uintptr_t)WSS_SUCCESS)) );
}

/**
 * Handler to call when some specific interrupts are happening. This function
 * shuts down the server in a safe way or ignore certain interrupts.
 *
 * @param   sig     [int]   "The integer value of the interrupt"
 * @return 	        [void]
 */
static void WSS_server_interrupt(int sig) {
    int n;
    switch (sig) {
        case SIGINT:
        case SIGSEGV:
        case SIGILL:
        case SIGFPE:
            if (state.state != HALTING && state.state != HALT_ERROR) {
                WSS_log_trace("Server is shutting down gracefully");
                WSS_server_set_state(HALTING);
                if (close_pipefd[0] != -1 && close_pipefd[1] != -1) {
                    do {
                        errno = 0;
                        n = write(close_pipefd[1], "HALT", 5);
                        if ( unlikely(n < 0) ) {
                            if ( unlikely(errno != EINTR) ) {
                                WSS_log_fatal("Unable to write halting message through pipe");
                            }
                        }
                    } while ( unlikely(errno == EINTR) );
                }
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
static int WSS_server_free(wss_server_t *server) {
    int result = EXIT_SUCCESS;

    if ( unlikely(WSS_SUCCESS != WSS_http_server_free(server)) ) {
        result = EXIT_FAILURE;
    }

    if ( unlikely(WSS_SUCCESS != pthread_mutex_destroy(&server->lock)) ) {
        result = EXIT_FAILURE;
    }

    /**
     * Freeing memory from server instance
     */
    if ( likely(NULL != server) ) {
        WSS_free((void **) &server);
    }

    return result;
}

/**
 * Starts the websocket server.
 *
 * @param   config  [wss_config_t *] "The configuration of the server"
 * @return 	        [int]            "EXIT_SUCCESS if successfull or EXIT_FAILURE on error"
 */
int WSS_server_start(wss_config_t *config) {
    int err;
    struct rlimit limits;
    struct sigaction sa;
    int ret = EXIT_SUCCESS;
    wss_server_t *http = NULL;
    pthread_t cleanup_thread_id;
#ifdef USE_OPENSSL
    wss_server_t *https = NULL;
    bool ssl = NULL != config->ssl_cert && NULL != config->ssl_key && (NULL != config->ssl_ca_file || NULL != config->ssl_ca_path);

    WSS_log_info("OpenSSL is available");
#endif

    if ( unlikely(0 != (err = pthread_mutex_init(&state.lock, NULL))) ) {
        WSS_log_fatal("Failed initializing state lock: %s", strerror(err));

        return EXIT_FAILURE;
    }

    if ( getrlimit(RLIMIT_NOFILE, &limits) < 0 ) {
        WSS_log_fatal("Failed to get kernel file descriptor limits: %s", strerror(err));

        return EXIT_FAILURE;
    }

    WSS_log_info("Setting max amount of filedescriptors available for server to: %d", limits.rlim_max);
    limits.rlim_cur = limits.rlim_max;

    if ( setrlimit(RLIMIT_NOFILE, &limits) < 0 ) {
        WSS_log_fatal("Failed to set kernel file descriptor limits: %s", strerror(err));

        return EXIT_FAILURE;
    }

    // Load extensions available
    WSS_load_extensions(config);

    // Load subprotocols available
    WSS_load_subprotocols(config);

    // Setting starting state
    WSS_log_trace("Starting server");

    WSS_server_set_state(STARTING);

    // Listening for signals that could terminate program
    WSS_log_trace("Listening for signals");

    // Set up the structure to specify the new action
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = WSS_server_interrupt;
    sa.sa_flags = 0;

    if (sigaction (SIGINT, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGINT signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if (sigaction (SIGSEGV, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGSEGV signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if (sigaction (SIGILL, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGILL signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if (sigaction (SIGHUP, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGHUB signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if (sigaction (SIGFPE, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGFPE signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if (sigaction (SIGPIPE, &sa, NULL) == -1) {
        WSS_log_fatal("Unable to listen for SIGPIPE signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    WSS_log_trace("Initializing sessions");
    if ( unlikely(WSS_SUCCESS != WSS_session_init_lock()) ) {
        WSS_log_fatal("Unable to initialize session mutex");

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    WSS_log_trace("Allocating memory for HTTP instance");

    if ( unlikely(NULL == (http = WSS_malloc(sizeof(wss_server_t)))) ) {
        WSS_log_fatal("Unable to allocate server structure");

        WSS_session_destroy_lock();
        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }
    servers.http = http;
    servers.https = NULL;

    if ( unlikely(0 != (err = pthread_mutex_init(&http->lock, NULL))) ) {
        WSS_server_free(http);
        WSS_session_destroy_lock();
        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);
    }

    WSS_log_trace("Setting to running state");

    WSS_server_set_state(RUNNING);

    WSS_log_trace("Creating HTTP Instance");

    http->config       = config;
    http->port         = config->port_http;
    if ( unlikely(WSS_SUCCESS != WSS_http_server(http)) ) {
        WSS_log_fatal("Unable to initialize http server");

        WSS_server_free(http);
        WSS_session_destroy_lock();
        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

#ifdef USE_OPENSSL
    if (ssl) {
        WSS_log_trace("Allocating memory for HTTPS instance");

        if ( unlikely(NULL == (https = (wss_server_t *) WSS_malloc(sizeof(wss_server_t)))) ) {
            WSS_server_free(http);
            WSS_session_destroy_lock();
            WSS_destroy_subprotocols();
            WSS_destroy_extensions();
            pthread_mutex_destroy(&state.lock);

            WSS_log_fatal("Unable to allocate https server structure");

            return EXIT_FAILURE;
        }

        servers.https = https;

        WSS_log_trace("Creating HTTPS Instance");

        https->config = config;
        https->port   = config->port_https;
        if ( unlikely(WSS_SUCCESS != WSS_http_ssl(https)) ) {
            WSS_server_free(https);
            WSS_server_free(http);
            WSS_session_destroy_lock();
            WSS_destroy_subprotocols();
            WSS_destroy_extensions();
            pthread_mutex_destroy(&state.lock);

            WSS_log_fatal("Unable to establish ssl context");

            return EXIT_FAILURE;
        }

        if ( unlikely(WSS_SUCCESS != WSS_http_server(https)) ) {
            WSS_server_free(https);
            WSS_server_free(http);
            WSS_session_destroy_lock();
            WSS_destroy_subprotocols();
            WSS_destroy_extensions();
            pthread_mutex_destroy(&state.lock);

            WSS_log_fatal("Unable to initialize https server");

            return EXIT_FAILURE;
        }
    }
#endif

    WSS_log_trace("Creating HTTP cleanup thread");
    
    if ( unlikely(pthread_create(&cleanup_thread_id, NULL, WSS_cleanup, NULL) != 0) ) {
#ifdef USE_OPENSSL
        if (ssl) {
            WSS_server_free(https);
        }
#endif
        WSS_server_free(http);
        WSS_session_destroy_lock();
        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        WSS_log_fatal("Unable to create cleanup thread");
        return EXIT_FAILURE;
    }

    WSS_log_trace("Joining server threads");

    pthread_join(cleanup_thread_id, (void **) &err);
    if ( unlikely(WSS_SUCCESS != err) ) {
        WSS_log_error("Cleanup thread returned with error: %s", strerror(err));
        WSS_server_set_state(HALT_ERROR);
    }

    WSS_log_trace("Cleanup thread has shutdown");

    pthread_join(http->thread_id, (void **) &err);
    if ( unlikely(WSS_SUCCESS != err) ) {
        WSS_log_error("HTTP Server thread returned with error: %s", strerror(err));
        WSS_server_set_state(HALT_ERROR);
    }

    WSS_log_trace("HTTP server thread has shutdown");

#ifdef USE_OPENSSL
    if (ssl) {
        pthread_join(https->thread_id, (void **) &err);
        if ( unlikely(WSS_SUCCESS != err) ) {
            WSS_log_error("HTTPS Server thread returned with error: %s", strerror(err));
            WSS_server_set_state(HALT_ERROR);
        }

        WSS_log_trace("HTTPS server thread has shutdown");
    }
#endif

    pthread_join(http->thread_id, (void **) &err);

    if ( unlikely(WSS_poll_close(http) != WSS_SUCCESS) ) {
        WSS_server_set_state(HALT_ERROR);
    }

    if ( unlikely(WSS_server_free(http) != 0) ) {
        WSS_server_set_state(HALT_ERROR);
    }

    WSS_log_trace("Freed memory associated with HTTP server instance");

#ifdef USE_OPENSSL
    if (ssl) {
        if ( unlikely(WSS_poll_close(https) != WSS_SUCCESS) ) {
            WSS_server_set_state(HALT_ERROR);
        }

        if ( unlikely(WSS_server_free(https) != 0) ) {
            WSS_server_set_state(HALT_ERROR);
        }

        WSS_log_trace("Freed memory associated with HTTPS server instance");
    }
#endif

    if ( unlikely(WSS_SUCCESS != WSS_session_delete_all()) ) {
        WSS_server_set_state(HALT_ERROR);
    }

    WSS_log_trace("Freed all sessions");

    if ( unlikely(WSS_SUCCESS != WSS_session_destroy_lock()) ) {
        WSS_server_set_state(HALT_ERROR);
    }

    WSS_log_trace("Destroyed session lock");

    if ( unlikely(HALT_ERROR == state.state) ) {
        ret = EXIT_FAILURE;
    }

    WSS_destroy_subprotocols();

    WSS_destroy_extensions();
    
    pthread_mutex_destroy(&state.lock);

    return ret;
}
