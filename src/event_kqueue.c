#include "event_kqueue.h" 

#if defined(WSS_KQUEUE)

/******************************************************************************
 *                                   KQUEUE                                   *
 ******************************************************************************/

static struct timespec timeout;

/**
 * Function that creates poll instance and adding the filedescriptor of the
 * servers socket to it.
 *
 * @param 	server	    [wss_server_t *]    "A pointer to a server structure"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_init(wss_server_t *server) {
    struct kevent event;

    WSS_log_info("Using KQUEUE");

    WSS_log_trace("Initializing kqueue instance");

    /**
     * Creating epoll instance.
     */
    if ( unlikely((server->poll_fd = kqueue()) < 0) ) {
        WSS_log_fatal("Unable to create server kqueue structure: %s", strerror(errno));
        server->poll_fd = -1;
        return WSS_POLL_CREATE_ERROR;
    }
    WSS_server_set_max_fd(server, server->poll_fd);

    WSS_log_trace("Initializing kqueue events");

    if ( unlikely(NULL == (server->events = WSS_calloc(server->config->pool_connect_workers+server->config->pool_io_workers, sizeof(event)))) ) {
        WSS_log_fatal("Unable to calloc server kqueue events");
        return WSS_MEMORY_ERROR;
    }

    if ( likely(close_pipefd[0] == -1 && close_pipefd[1] == -1) ) {
        WSS_log_trace("Creating close pipe file descriptors");

        if ( unlikely(pipe(close_pipefd) == -1) ) {
            WSS_log_trace("Unable to create close pipe file descriptors: %s", strerror(errno));
            return WSS_POLL_PIPE_ERROR;
        }

        if ( unlikely((err = WSS_socket_non_blocking(close_pipefd[0])) != WSS_SUCCESS) ) {
            return err;
        }

        if ( unlikely((err = WSS_socket_non_blocking(close_pipefd[1])) != WSS_SUCCESS) ) {
            return err;
        }
    }

    WSS_log_trace("Arms close pipe file descriptor to kqueue instance");

    do {
        errno = 0;
        EV_SET(&event, close_pipefd[0], EVFILT_READ, EV_ADD, 0, 0, ((void *) &close_pipefd[0])); 
        ret = kevent(server->poll_fd, &event, 1, NULL, 0, NULL);
    } while ( unlikely(errno == EINTR) );

    if ( unlikely(ret < 0) ) {
        WSS_log_error("Failed to (re)arm close pipe file descriptor to kqueue: %s", strerror(errno));
        return WSS_POLL_SET_ERROR;
    }

    WSS_server_set_max_fd(server, close_pipefd[0]);

    WSS_log_trace("Arms server file descriptor to kqueue instance");

    do {
        errno = 0;
        EV_SET(&event, server->fd, EVFILT_READ, EV_ADD, 0, 0, ((void *) &server->fd)); 
        ret = kevent(server->poll_fd, &event, 1, NULL, 0, NULL);
    } while ( unlikely(errno == EINTR) );

    if ( unlikely(ret < 0) ) {
        WSS_log_error("Failed to (re)arm file descriptor to kqueue: %s", strerror(errno));
        return WSS_POLL_SET_ERROR;
    }

    WSS_server_set_max_fd(server, server->fd);

    if (server->config->timeout_poll >= 0) {
        timeout = { server->config->timeout_poll/1000, (server->config->timeout_poll%1000)*1000000 }
    }

    return WSS_SUCCESS;
}

/**
 * Function that rearms the poll instance for write events with the clients
 * filedescriptor
 *
 * @param 	server      [wss_server_t *]	"A pointer to a server structure"
 * @param 	fd	        [int]	            "The clients file descriptor"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_set_write(wss_server_t *server, int fd) {
    int ret;
    struct kevent event;

    WSS_log_trace("Rearms session %d for write events on kqueue", fd);

    do {
        errno = 0;
        EV_SET(&event, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT, 0, 0, ((void *) &fd)); 
        ret = kevent(server->poll_fd, &event, 1, NULL, 0, NULL);
    } while ( unlikely(errno == EINTR) );

    if ( unlikely(ret < 0) ) {
        WSS_log_error("Failed to (re)arm file descriptor to kqueue: %s", strerror(errno));
        return WSS_POLL_SET_ERROR;
    }

    WSS_server_set_max_fd(server, fd);

    return WSS_SUCCESS;
}

/**
 * Function that rearms the poll instance for read events with the clients
 * filedescriptor
 *
 * @param 	server      [wss_server_t *server]   "A pointer to a server structure"
 * @param 	fd	        [int]	                 "The clients file descriptor"
 * @return 			    [wss_error_t]            "The error status"
 */
wss_error_t WSS_poll_set_read(wss_server_t *server, int fd) {
    int ret;
    struct kevent event;

    WSS_log_trace("Rearms session %d for read events on kqueue", fd);

    do {
        errno = 0;
        EV_SET(&event, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT, 0, 0, ((void *) &fd)); 
        ret = kevent(server->poll_fd, &event, 1, NULL, 0, NULL);
    } while ( unlikely(errno == EINTR) );

    if ( unlikely(ret < 0) ) {
        WSS_log_error("Failed to (re)arm file descriptor to kqueue: %s", strerror(errno));
        return WSS_POLL_SET_ERROR;
    }

    WSS_server_set_max_fd(server, fd);

    return WSS_SUCCESS;
}

/**
 * Function removes the client filedescriptor from the poll instance 
 *
 * @param 	server      [wss_server_t *]    "A pointer to a server structure"
 * @param 	fd	        [int]	            "The clients file descriptor"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_remove(wss_server_t *server, int fd) {
    int ret;
    struct kevent event;

    WSS_log_trace("Removes session %d from kqueue", fd);

    do {
        errno = 0;
        EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        ret = kevent(server->poll_fd, &event, 1, NULL, 0, NULL);
    } while ( unlikely(errno == EINTR) );

    if ( unlikely(ret < 0) ) {
        WSS_log_error("Failed to delete file descriptor from kqueue: %s", strerror(errno));
        return WSS_POLL_REMOVE_ERROR;
    } 

    do {
        errno = 0;
        EV_SET(&event, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        ret = kevent(server->poll_fd, &event, 1, NULL, 0, NULL);
    } while ( unlikely(errno == EINTR) );

    if ( unlikely(ret < 0) ) {
        WSS_log_error("Failed to delete file descriptor from kqueue: %s", strerror(errno));
        return WSS_POLL_REMOVE_ERROR;
    } 

    return WSS_SUCCESS;
} 

/**
 * Function that listens for new events on the servers file descriptor 
 *
 * @param 	server      [wss_server_t *]    "A pointer to a server structure"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_delegate(wss_server_t *server) {
    int i, n, fd;
    wss_error_t err;
    struct kevent *events = (struct kevent *) server->events;


    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Listening for HTTPS kqueue events");
    } else {
        WSS_log_trace("Listening for HTTP kqueue events");
    }
    
    do {
        errno = 0;

        if (server->config->timeout_poll >= 0) {
            n = kevent(server->poll_fd, NULL, 0, events, server->config->pool_connect_workers+server->config->pool_io_workers, &timeout);
        } else {
            n = kevent(server->poll_fd, NULL, 0, events, server->config->pool_connect_workers+server->config->pool_io_workers, NULL);
        }
        
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

    for (i = 0; i < n; i++) {
        fd = *((int *)events[i].udata);
        if (events[i].flags & (EV_EOF | EV_ERROR)) {
            if ( unlikely(fd == server->fd) ) {
                WSS_log_fatal("A server error occured upon kqueue");
                return WSS_POLL_WAIT_ERROR;
            }

            WSS_log_trace("Session %d disconnecting", fd);

            wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
            if ( unlikely(NULL == args) ) {
                WSS_log_fatal("Failed allocating threadpool argument");
                return WSS_MEMORY_ERROR;
            }
            args->server = server;
            args->fd = fd;
            args->state = CLOSING

            err = WSS_poll_add_task_to_threadpool(server->pool_io, &WSS_work, (void *)args);
            if ( unlikely(err != WSS_SUCCESS) ) {
                WSS_log_fatal("Failed adding disconnect job to worker pool");
                WSS_free((void **)&args);
                return err;
            }
        } else if ( fd == server->fd ) {
            WSS_log_trace("New session connecting");

            wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
            if ( unlikely(NULL == args) ) {
                WSS_log_fatal("Failed allocating threadpool argument");
                return WSS_MEMORY_ERROR;
            }
            args->server = server;
            args->state = CONNECTING
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
            // Pipe file descriptor is used to interrupt blocking wait
            continue;
        } else {
            if (events[i].filter == EVFILT_READ) {
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
                args->state = READING

                err = WSS_poll_add_task_to_threadpool(server->pool_io, &WSS_work, (void *)args);
                if ( unlikely(err != WSS_SUCCESS) ) {
                    WSS_log_fatal("Failed adding read job to worker pool");
                    WSS_free((void **)&args);
                    return err;
                }
            }

            if (events[i].filter == EVFILT_WRITE) {
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
                args->state = WRITING

                err = WSS_poll_add_task_to_threadpool(server->pool_io, &WSS_work, (void *)args);
                if ( unlikely(err != WSS_SUCCESS) ) {
                    WSS_log_fatal("Failed adding write job to worker pool");
                    WSS_free((void **)&args);
                    return err;
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
    WSS_UNUSED(server);

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

#endif //WSS_KQUEUE
