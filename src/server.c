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
#include "worker.h"
#include "http.h"
#include "error.h"
#include "config.h"
#include "subprotocols.h"
#include "extensions.h"
#include "core.h"
#include "ssl.h"

#define B_STACKTRACE_IMPL
#include "b_stacktrace.h"

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

static inline void write_control_frame(wss_memorypool_t *frame_pool, wss_frame_t *frame, wss_session_t *session) {
    size_t j;
    char *message;
    size_t message_length;
    int n = 0;
    int fd = session->fd;

    // Use extensions
    for (j = 0; likely(j < session->header.ws_extensions_count); j++) {
        session->header.ws_extensions[j]->ext->outframes(
                fd,
                &frame,
                1);
    }

    if ( unlikely(0 == (message_length = WSS_stringify_frame(frame, &message))) ) {
        WSS_log_error("Unable to represent frame as bytes");

        WSS_free_frame(frame);
        WSS_memorypool_dealloc(frame_pool, frame);

        return;
    }

    WSS_free_frame(frame);
    WSS_memorypool_dealloc(frame_pool, frame);

    if (session->ssl_connected) {
        WSS_ssl_write(session, message, message_length);
    } else {
        size_t written = 0;
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
    }

    WSS_free((void **) &message);
}

static void cleanup_session(wss_session_t *session) {
    wss_error_t err;
    wss_frame_t *frame;
    long unsigned int ms;
    int fd = session->fd;
    wss_server_t *server = &servers.http;
    bool dc;

    if (! session->handshaked) {
        return;
    }

    if (NULL != session->ssl && session->ssl_connected) {
        server = &servers.https;
    }

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

        if (! session->ssl_connected) {
            WSS_log_trace("Session %d disconnected from ip: %s:%d using HTTP request, due to timing out", session->fd, session->ip, session->port);
        } else {
            WSS_log_trace("Session %d disconnected from ip: %s:%d using HTTPS request, due to timing out", session->fd, session->ip, session->port);
        }

        WSS_log_trace("Informing subprotocol of client with file descriptor %d disconnecting", session->fd);

        if ( likely(session->header.ws_protocol != NULL) ) {
            WSS_log_trace("Informing subprotocol about session close");
            session->header.ws_protocol->close(session->fd);
        }

        WSS_log_trace("Removing poll filedescriptor from eventlist");

        WSS_poll_remove(server, fd);

        WSS_log_trace("Sending close frame", fd);

        frame = WSS_memorypool_alloc(server->frame_pool);
        if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_TRY_AGAIN, NULL, frame)) ) {
            WSS_log_error("Unable to create closing frame");

            WSS_free_frame(frame);
            WSS_memorypool_dealloc(server->frame_pool, frame);

            return;
        }

        write_control_frame(server->frame_pool, frame, session);

        WSS_log_trace("Deleting client session");

        if ( unlikely(WSS_SUCCESS != (err = WSS_session_delete_no_lock(session))) ) {
            switch (err) {
                case WSS_SSL_SHUTDOWN_READ_ERROR:
                    WSS_poll_set_read(server, fd);

                    WSS_free_frame(frame);
                    WSS_memorypool_dealloc(server->frame_pool, frame);

                    return;
                case WSS_SSL_SHUTDOWN_WRITE_ERROR:
                    WSS_poll_set_write(server, fd);

                    WSS_free_frame(frame);
                    WSS_memorypool_dealloc(server->frame_pool, frame);

                    return;
                default:
                    break;
            }

            WSS_log_error("Unable to delete client session, received error code: %d", err);
            return;
        }

        WSS_log_info("Client with session %d disconnected", fd);

    // Ping session to keep it alive
    } else

    if (server->config->timeout_pings > 0 ) {
        WSS_log_info("Pinging session %d", fd);

        frame = WSS_memorypool_alloc(server->frame_pool);
        if ( unlikely(WSS_SUCCESS != WSS_ping_frame(frame)) ) {
            WSS_log_error("Unable to create ping frame");

            WSS_free_frame(frame);
            WSS_memorypool_dealloc(server->frame_pool, frame);

            return;
        }

        write_control_frame(server->frame_pool, frame, session);
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
    wss_server_t *server = &servers.http;
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

    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Running HTTPS server");
    } else {
        WSS_log_trace("Running HTTP server");
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

    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Stopping HTTPS server");
    } else {
        WSS_log_trace("Stopping HTTP server");
    }

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
        case SIGSEGV:
        case SIGILL:
        case SIGBUS:
        case SIGHUP:
        case SIGFPE:
            {
                char* stacktrace = b_stacktrace_get_string();
                WSS_log_fatal(stacktrace);
                free(stacktrace);
            }
            fallthrough;
        case SIGINT:
            WSS_log_trace("Received closing signal");

            if (state.state != HALTING && state.state != HALT_ERROR) {
                WSS_server_set_state(HALTING);

                WSS_log_trace("Server is shutting down gracefully");
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
            return;
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
    unsigned int loaded;
    int ret = EXIT_SUCCESS;
    pthread_t cleanup_thread_id;
    bool ssl = NULL != config->ssl_cert && NULL != config->ssl_key && (NULL != config->ssl_ca_file || NULL != config->ssl_ca_path);

    if ( unlikely(0 != (err = pthread_mutex_init(&state.lock, NULL))) ) {
        WSS_log_fatal("Failed initializing state lock: %s", strerror(err));

        return EXIT_FAILURE;
    }

    if ( unlikely(getrlimit(RLIMIT_NOFILE, &limits) < 0) ) {
        WSS_log_fatal("Failed to get kernel file descriptor limits: %s", strerror(err));

        return EXIT_FAILURE;
    }

    WSS_log_info("Setting max amount of filedescriptors available for server to: %d", limits.rlim_max);
    limits.rlim_cur = limits.rlim_max;

    if ( unlikely(setrlimit(RLIMIT_NOFILE, &limits) < 0) ) {
        WSS_log_fatal("Failed to set kernel file descriptor limits: %s", strerror(err));

        return EXIT_FAILURE;
    }

    // Load extensions available
    WSS_load_extensions(config);

    // Load subprotocols available
    loaded = WSS_load_subprotocols(config);
    if ( unlikely(loaded == 0) ) {
        WSS_log_fatal("No subprotocols loaded");

        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    // Setting starting state
    WSS_log_trace("Starting server");

    WSS_server_set_state(STARTING);

    // Listening for signals that could terminate program
    WSS_log_trace("Listening for signals");

    // Set up the structure to specify the new action
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = WSS_server_interrupt;
    sa.sa_flags = 0;

    if ( unlikely(sigaction (SIGINT, &sa, NULL) == -1) ) {
        WSS_log_fatal("Unable to listen for SIGINT signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if ( unlikely(sigaction (SIGSEGV, &sa, NULL) == -1) ) {
        WSS_log_fatal("Unable to listen for SIGSEGV signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if ( unlikely(sigaction (SIGILL, &sa, NULL) == -1) ) {
        WSS_log_fatal("Unable to listen for SIGILL signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if ( unlikely(sigaction (SIGHUP, &sa, NULL) == -1) ) {
        WSS_log_fatal("Unable to listen for SIGHUB signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if ( unlikely(sigaction (SIGBUS, &sa, NULL) == -1) ) {
        WSS_log_fatal("Unable to listen for SIGBUS signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if ( unlikely(sigaction (SIGFPE, &sa, NULL) == -1) ) {
        WSS_log_fatal("Unable to listen for SIGFPE signal: %s", strerror(errno));

        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if ( unlikely(sigaction (SIGPIPE, &sa, NULL) == -1) ) {
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

    if ( unlikely(0 != (err = pthread_mutex_init(&servers.http.lock, NULL))) ) {
        WSS_session_destroy_lock();
        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);
    }

    WSS_log_trace("Setting to running state");

    WSS_server_set_state(RUNNING);

    WSS_log_trace("Creating HTTP Instance");

    servers.http.config = config;
    servers.http.port   = config->port_http;
    if ( unlikely(WSS_SUCCESS != WSS_http_server(&servers.http)) ) {
        WSS_log_fatal("Unable to initialize http server");

        WSS_session_destroy_lock();
        WSS_destroy_subprotocols();
        WSS_destroy_extensions();
        pthread_mutex_destroy(&state.lock);

        return EXIT_FAILURE;
    }

    if (ssl) {
        WSS_log_trace("Creating HTTPS Instance");

        servers.https.config = config;
        servers.https.port   = config->port_https;
        if ( unlikely(WSS_SUCCESS != WSS_http_ssl(&servers.https)) ) {
            WSS_session_destroy_lock();
            WSS_destroy_subprotocols();
            WSS_destroy_extensions();
            pthread_mutex_destroy(&state.lock);

            WSS_log_fatal("Unable to establish ssl context");

            return EXIT_FAILURE;
        }

        if ( unlikely(WSS_SUCCESS != WSS_http_server(&servers.https)) ) {
            WSS_session_destroy_lock();
            WSS_destroy_subprotocols();
            WSS_destroy_extensions();
            pthread_mutex_destroy(&state.lock);

            WSS_log_fatal("Unable to initialize https server");

            return EXIT_FAILURE;
        }
    }

    WSS_log_trace("Creating HTTP cleanup thread");
    
    if ( unlikely(pthread_create(&cleanup_thread_id, NULL, WSS_cleanup, NULL) != 0) ) {
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

    pthread_join(servers.http.thread_id, (void **) &err);
    if ( unlikely(WSS_SUCCESS != err) ) {
        WSS_log_error("HTTP Server thread returned with error: %s", strerror(err));
        WSS_server_set_state(HALT_ERROR);
    }

    WSS_log_trace("HTTP server thread has shutdown");

    if (ssl) {
        pthread_join(servers.https.thread_id, (void **) &err);
        if ( unlikely(WSS_SUCCESS != err) ) {
            WSS_log_error("HTTPS Server thread returned with error: %s", strerror(err));
            WSS_server_set_state(HALT_ERROR);
        }

        WSS_log_trace("HTTPS server thread has shutdown");
    }

    pthread_join(servers.http.thread_id, (void **) &err);

    if ( unlikely(WSS_poll_close(&servers.http) != WSS_SUCCESS) ) {
        WSS_server_set_state(HALT_ERROR);
    }

    if ( unlikely(WSS_server_free(&servers.http) != 0) ) {
        WSS_server_set_state(HALT_ERROR);
    }

    WSS_log_trace("Freed memory associated with HTTP server instance");

    if (ssl) {
        if ( unlikely(WSS_poll_close(&servers.https) != WSS_SUCCESS) ) {
            WSS_server_set_state(HALT_ERROR);
        }

        if ( unlikely(WSS_server_free(&servers.https) != 0) ) {
            WSS_server_set_state(HALT_ERROR);
        }

        WSS_log_trace("Freed memory associated with HTTPS server instance");
    }

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

    WSS_log_trace("Destroyed subprotocols");

    WSS_destroy_extensions();

    WSS_log_trace("Destroyed extensions");
    
    pthread_mutex_destroy(&state.lock);

    WSS_log_trace("Destroyed state lock");

    return ret;
}
