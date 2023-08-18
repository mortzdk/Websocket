#include "event_poll.h" 

#if defined(WSS_POLL)

/******************************************************************************
 *                                    POLL                                    *
 ******************************************************************************/

static unsigned int MAXEVENTS;

/**
 * Writes a rearm message on the rearm pipe.
 *
 * @param 	server	[wss_server_t *] 	"A wss_server_t instance"
 * @return 			[void]
 */
static inline void rearm(wss_server_t *server) {
    int n;

    WSS_log_trace("Notify about rearm");

    if ( likely(server->rearm_pipefd[0] != -1 && server->rearm_pipefd[1] != -1) ) {
        do {
            errno = 0;
            n = write(server->rearm_pipefd[1], "ARM", 3);
            if ( unlikely(n < 0) ) {
                if ( unlikely(errno != EINTR) ) {
                    WSS_log_fatal("Unable to write rearm message through pipe");
                }
            }
        } while ( unlikely(errno == EINTR) );
    }
}

/**
 * Reads as much as possible from rearm pipe.
 *
 * @param 	server	[wss_server_t *] 	"A wss_server_t instance"
 * @return 			[void]
 */
static inline void handle_rearm(wss_server_t *server) {
    int n;
    char buf[server->config->size_buffer];

    WSS_log_trace("Handle rearm");

    if ( likely(server->rearm_pipefd[0] != -1 && server->rearm_pipefd[1] != -1) ) {
        do {
            n = read(server->rearm_pipefd[0], buf, server->config->size_buffer);
            if ( unlikely(n < 0) ) {
                if ( likely(errno == EAGAIN || errno == EWOULDBLOCK) ) {
                    return;
                } else if ( likely(errno == EINTR) ) {
                    errno = 0;
                    continue;
                } else {
                    WSS_log_fatal("Unable to read rearm message through pipe");
                }
            }
        } while ( n > 0 );
    }
}

/**
 * Function that creates poll instance and adding the filedescriptor of the
 * servers socket to it.
 *
 * @param 	server      [wss_server_t *]    "A pointer to a server structure"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_init(wss_server_t *server) {
    unsigned int i;
    wss_error_t err;
    struct rlimit limits;
    struct pollfd event;
    struct pollfd *events;

    if ( unlikely(getrlimit(RLIMIT_NOFILE, &limits) < 0) ) {
        return WSS_RLIMIT_ERROR;
    }

    MAXEVENTS = limits.rlim_cur;

    WSS_log_info("Using POLL");

    WSS_log_trace("Initializing poll events");

    if ( unlikely(NULL == (events = WSS_calloc(MAXEVENTS, sizeof(event)))) ) {
        WSS_log_fatal("Unable to calloc server poll events");
        return WSS_MEMORY_ERROR;
    }

    for (i = 0; i < MAXEVENTS; i++) {
        events[i].fd = -1;
    }

    server->events = events;

    if ( close_pipefd[0] == -1 && close_pipefd[1] == -1 ) {
        WSS_log_trace("Creating close pipe file descriptors");

        if ( unlikely(pipe(close_pipefd) == -1) ) {
            WSS_log_trace("Unable to create close pipe file descriptors: %s", strerror(errno));
            return WSS_POLL_PIPE_ERROR;
        }

        if ( (err = WSS_socket_non_blocking(close_pipefd[0])) != WSS_SUCCESS ) {
            return err;
        }

        if ( (err = WSS_socket_non_blocking(close_pipefd[1])) != WSS_SUCCESS ) {
            return err;
        }
    }

    WSS_log_trace("Arms close pipe file descriptor to poll instance");

    if ((err = WSS_poll_set_read(server, close_pipefd[0])) != WSS_SUCCESS) {
        return err;
    }

    WSS_log_trace("Creating arm pipe file descriptors");

    if ( unlikely(pipe(server->rearm_pipefd) == -1) ) {
        WSS_log_trace("Unable to create arm pipe file descriptors: %s", strerror(errno));
        return WSS_POLL_PIPE_ERROR;
    }

    if ( unlikely((err = WSS_socket_non_blocking(server->rearm_pipefd[0])) != WSS_SUCCESS) ) {
        return err;
    }

    if ( unlikely((err = WSS_socket_non_blocking(server->rearm_pipefd[1])) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_log_trace("Arms arm pipe file descriptor to poll instance");

    if ( unlikely((err = WSS_poll_set_read(server, server->rearm_pipefd[0])) != WSS_SUCCESS)) {
        return err;
    }

    WSS_log_trace("Arms server file descriptor to poll instance");

    return WSS_poll_set_read(server, server->fd);

}

/**
 * Function that rearms the poll instance for write events with the clients
 * filedescriptor
 *
 * @param 	server      [wss_server_t *]    "A pointer to a server structure"
 * @param 	fd	        [int]	            "The clients file descriptor"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_set_write(wss_server_t *server, int fd) {
    struct pollfd *events = (struct pollfd *) server->events;

    if ( unlikely((unsigned int)fd >= MAXEVENTS) ) {
        return WSS_POLL_SET_ERROR;
    }

    WSS_log_trace("Session %d (re)armed for write events on poll", fd);

    WSS_server_set_max_fd(server, fd);

    events[fd].fd = fd;
    events[fd].events = POLLOUT;

    if (fd != server->rearm_pipefd[0] && fd != close_pipefd[0] && fd != server->fd) {
        rearm(server);
    }

    return WSS_SUCCESS;
}

/**
 * Function that rearms the poll instance for read events with the clients
 * filedescriptor
 *
 * @param 	server      [wss_server_t *]    "A pointer to a server structure"
 * @param 	fd	        [int]	            "The clients file descriptor"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_set_read(wss_server_t *server, int fd) {
    struct pollfd *events = (struct pollfd *) server->events;

    if ( unlikely((unsigned int)fd >= MAXEVENTS) ) {
        return WSS_POLL_SET_ERROR;
    }

    WSS_log_trace("Session %d (re)armed for read events on poll", fd);

    WSS_server_set_max_fd(server, fd);

    events[fd].fd = fd;
    events[fd].events = POLLPRI | POLLIN | POLLRDHUP;

    if (fd != server->rearm_pipefd[0] && fd != close_pipefd[0] && fd != server->fd) {
        rearm(server);
    }

    return WSS_SUCCESS;
}

/**
 * Function removes the client filedescriptor from the poll instance 
 *
 * @param 	server      [wss_server_t *]	"A pointer to a server structure"
 * @param 	fd	        [int]	            "The clients file descriptor"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_remove(wss_server_t *server, int fd) {
    struct pollfd *events = (struct pollfd *) server->events;

    if ( unlikely((unsigned int)fd >= MAXEVENTS) ) {
        return WSS_POLL_REMOVE_ERROR;
    }

    WSS_log_trace("Removing session %d from poll", fd);

    events[fd].fd = -1;
    events[fd].events = 0;

    if (fd != server->rearm_pipefd[0] && fd != close_pipefd[0] && fd != server->fd) {
        rearm(server);
    }

    return WSS_SUCCESS;
}

/**
 * Function that listens for new events on the servers file descriptor 
 *
 * @param 	server	    [wss_server_t *]    "A pointer to a server structure"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_delegate(wss_server_t *server) {
    int n, i;
    int start = 0;
    int fd;
    wss_error_t err;
    int end = server->max_fd;
    struct pollfd *events = (struct pollfd *) server->events;

    while ( likely(start < end && events[start].fd == -1) ) {
        ++start;
    }

    while ( likely(start < end && events[end].fd == -1) ) {
        --end;
    }

    if ( unlikely(start == end && events[start].fd == -1) ) {
        return WSS_POLL_WAIT_ERROR;
    }

    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Listening for HTTPS poll events");
    } else {
        WSS_log_trace("Listening for HTTP poll events");
    }
    
    do {
        errno = 0;

        n = poll(&events[start], end-start+1, server->config->timeout_poll);

        if ( unlikely(n < 0) ) {
            if ( unlikely(errno != EINTR) ) {
                return WSS_POLL_WAIT_ERROR;
            }
        }
    } while ( unlikely(errno == EINTR) );

    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Received %d HTTPS events", n);
    } else {
        WSS_log_trace("Received %d HTTP events", n);
    }

    for (i = start; likely(i <= end); ++i) {
        if ( likely(events[i].fd > 0 && events[i].revents) ) {
            fd = events[i].fd;
            if ( unlikely(events[i].revents & (POLLHUP | POLLERR | POLLRDHUP | POLLNVAL)) ) {
                if ( unlikely(fd == server->fd) ) {
                    WSS_log_fatal("A server error occured upon poll");
                    return WSS_POLL_WAIT_ERROR;
                }

                WSS_poll_remove(server, fd);

                WSS_log_trace("Session %d is disconnecting", fd);

                wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->fd = fd;
                args->state = CLOSING;

                err = WSS_poll_add_task_to_threadpool(server->pool_io, &WSS_work, (void *)args);
                if ( unlikely(err != WSS_SUCCESS) ) {
                    WSS_log_fatal("Failed adding disconnect job to worker pool");
                    WSS_free((void **)&args);
                    return err;
                }
            } else if (fd == server->fd) {
                WSS_log_trace("New session connecting");

                wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->state = CONNECTING;
                args->fd = fd;

                /**
                 * If new session has connected
                 */
                err = WSS_poll_add_task_to_threadpool(server->pool_connect, &WSS_connect, (void *)args);
                if ( unlikely(err != WSS_SUCCESS) ) {
                    WSS_log_fatal("Failed adding connect job to worker pool");
                    return err;
                }
            } else if ( unlikely(fd == close_pipefd[0]) ) {
                WSS_log_trace("Close pipe invoked");
                continue;
            } else if ( likely(fd == server->rearm_pipefd[0]) ) {
                WSS_log_trace("Rearm pipe invoked");
                handle_rearm(server);
                continue;
            } else {
                WSS_poll_remove(server, fd);

                if (events[i].revents & (POLLIN | POLLPRI)) {
                    WSS_log_trace("Session %d begins to read", fd);

                    /**
                     * If new reads are ready
                     */
                    wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
                    if ( unlikely(NULL == args) ) {
                        WSS_log_fatal("Failed allocating threadpool argument");
                        return WSS_MEMORY_ERROR;
                    }
                    args->server = server;
                    args->fd = fd;
                    args->state = READING;

                    err = WSS_poll_add_task_to_threadpool(server->pool_io, &WSS_work, (void *)args);
                    if ( unlikely(err != WSS_SUCCESS) ) {
                        WSS_log_fatal("Failed adding read job to worker pool");
                        WSS_free((void **)&args);
                        return err;
                    }
                }

                if (events[i].revents & POLLOUT) {
                    WSS_log_trace("Session %d begins to write", fd);

                    /**
                     * If new writes are ready
                     */
                    wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
                    if ( unlikely(NULL == args) ) {
                        WSS_log_fatal("Failed allocating threadpool argument");
                        return WSS_MEMORY_ERROR;
                    }
                    args->server = server;
                    args->fd = fd;
                    args->state = WRITING;

                    err = WSS_poll_add_task_to_threadpool(server->pool_io, &WSS_work, (void *)args);
                    if ( unlikely(err != WSS_SUCCESS) ) {
                        WSS_log_fatal("Failed adding write job to worker pool");
                        WSS_free((void **)&args);
                        return err;
                    }
                }
            }
        }
    }

    return WSS_SUCCESS;
}

/**
 * Function that cleanup poll function when closing
 *
 * @param 	server	    [wss_server_t *]	"A pointer to a server structure"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_close(wss_server_t *server) {
    if ( likely(server->rearm_pipefd[0] != -1) ) {
        close(server->rearm_pipefd[0]);
        server->rearm_pipefd[0] = -1;
    }

    if ( likely(server->rearm_pipefd[1] != -1) ) {
        close(server->rearm_pipefd[1]);
        server->rearm_pipefd[1] = -1;
    }

    if ( likely(close_pipefd[0] != -1) ) {
        close(close_pipefd[0]);
        close_pipefd[0] = -1;
    }

    if ( likely(close_pipefd[1] != -1) ) {
        close(close_pipefd[1]);
        close_pipefd[1] = -1;
    }

    return WSS_SUCCESS;
}

#endif //WSS_POLL
