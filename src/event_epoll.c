#include "event_epoll.h" 

#if defined(WSS_EPOLL)

/******************************************************************************
 *                                   EPOLL                                    *
 ******************************************************************************/

static inline wss_error_t WSS_poll_add(int poll_fd, int fd, uint32_t events) {
    struct epoll_event event;
    int ret;

    WSS_log_trace("Modifying filedescriptor on epoll eventlist");

    do {
        memset(&event, 0, sizeof(event));

        event.events = events;
        event.data.fd = fd;

        if ( unlikely((ret = epoll_ctl(poll_fd, EPOLL_CTL_MOD, fd, &event)) < 0) ) {
            if ( likely(errno == ENOENT) ) {
                WSS_log_trace("Adding file descriptor to epoll eventlist");
                errno = 0;

                memset(&event, 0, sizeof(event));

                event.events = events;
                event.data.fd = fd;

                ret = epoll_ctl(poll_fd, EPOLL_CTL_ADD, fd, &event);
            }
        }
    } while ( unlikely(errno == EINTR) );

    if ( unlikely(ret < 0) ) {
        WSS_log_error("Failed to (re)arm file descriptor to epoll: %s", strerror(errno));
        return WSS_POLL_SET_ERROR;
    }

    return WSS_SUCCESS;
}

/**
 * Function that creates poll instance and adding the filedescriptor of the
 * servers socket to it.
 *
 * @param 	server	    [wss_server_t *]    "A pointer to a server structure"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_init(wss_server_t *server) {
    wss_error_t err;
    struct epoll_event event;

    memset(&event, 0, sizeof(event));

    WSS_log_info("Using EPOLL");
    
    WSS_log_trace("Initializing epoll instance");

    /**
     * Creating epoll instance.
     */
    if ( unlikely((server->poll_fd = epoll_create1(0)) < 0) ) {
        WSS_log_fatal("Unable to create server epoll structure: %s", strerror(errno));
        server->poll_fd = -1;
        return WSS_POLL_CREATE_ERROR;
    }
    WSS_server_set_max_fd(server, server->poll_fd);

    WSS_log_trace("Initializing epoll events");

    if ( unlikely(NULL == (server->events = WSS_calloc(server->config->pool_io_workers+server->config->pool_connect_workers, sizeof(event)))) ) {
        WSS_log_fatal("Unable to calloc server epoll events");
        return WSS_MEMORY_ERROR;
    }

    if ( close_pipefd[0] == -1 && close_pipefd[1] == -1 ) {
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

    WSS_log_trace("Arms close pipe file descriptor to epoll instance");

    if ( unlikely((err = WSS_poll_add(server->poll_fd, close_pipefd[0], EPOLLIN | EPOLLET | EPOLLRDHUP)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_server_set_max_fd(server, close_pipefd[0]);

    WSS_log_trace("Arms server file descriptor to epoll instance");

    if ( unlikely((err = WSS_poll_add(server->poll_fd, server->fd, EPOLLIN | EPOLLET | EPOLLRDHUP)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_server_set_max_fd(server, server->fd);

    return WSS_SUCCESS;
}

/**
 * Function that rearms the poll instance for write events with the clients
 * filedescriptor
 *
 * @param 	server      [wss_server_t *server]      "A pointer to a server structure"
 * @param 	fd	        [int]	                    "The clients file descriptor"
 * @return 			    [wss_error_t]               "The error status"
 */
wss_error_t WSS_poll_set_write(wss_server_t *server, int fd) {
    wss_error_t err;

    WSS_log_trace("Rearms session %d for write epoll events", fd);

    if ( unlikely((err = WSS_poll_add(server->poll_fd, fd, EPOLLOUT | EPOLLET | EPOLLONESHOT)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_server_set_max_fd(server, fd);

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
    wss_error_t err;

    WSS_log_trace("Rearms session %d for read epoll events", fd);

    if ( unlikely((err = WSS_poll_add(server->poll_fd, fd, EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP)) != WSS_SUCCESS) ) {
        return err;
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
    struct epoll_event event;
    int ret;

    WSS_log_trace("Removing session %d from epoll events", fd);

    do {
        memset(&event, 0, sizeof(event));

        event.events = EPOLLIN | EPOLLOUT;
        event.data.fd = fd;

        ret = epoll_ctl(server->poll_fd, EPOLL_CTL_DEL, fd, &event);
    } while ( unlikely(errno == EINTR) );

    if ( unlikely(ret < 0) ) {
        WSS_log_error("Failed to remove session %d from epoll: %s", fd, strerror(errno));
        return WSS_POLL_REMOVE_ERROR;
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
    int i, n;
    wss_error_t err;
    struct epoll_event *events = server->events;

    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Listening for HTTPS epoll events");
    } else {
        WSS_log_trace("Listening for HTTP epoll events");
    }

    do {
        errno = 0;
        n = epoll_wait(server->poll_fd, events, server->config->pool_io_workers+server->config->pool_connect_workers, server->config->timeout_poll);
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
        if ( unlikely((events[i].events & EPOLLHUP) ||
                    (events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLRDHUP)) ) {
            if ( unlikely(events[i].data.fd == server->fd) ) {
                WSS_log_fatal("A server error occured upon epoll");
                return WSS_POLL_WAIT_ERROR;
            }

            WSS_log_trace("Session %d disconnecting", events[i].data.fd);

            wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
            if ( unlikely(NULL == args) ) {
                WSS_log_fatal("Failed allocating threadpool argument");
                return WSS_MEMORY_ERROR;
            }
            args->server = server;
            args->fd = events[i].data.fd;
            args->state = CLOSING;

            err = WSS_poll_add_task_to_threadpool(server->pool_io, &WSS_work, (void *)args);
            if ( unlikely(err != WSS_SUCCESS) ) {
                WSS_log_fatal("Failed adding disconnect job to worker pool");
                WSS_free((void **)&args);
                return err;
            }
        } else if ( events[i].data.fd == server->fd ) {
            WSS_log_trace("New session connecting");

            wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
            if ( unlikely(NULL == args) ) {
                WSS_log_fatal("Failed allocating threadpool argument");
                return WSS_MEMORY_ERROR;
            }
            args->server = server;
            args->state = CONNECTING;
            args->fd = events[i].data.fd;

            /**
             * If new session has connected
             */
            err = WSS_poll_add_task_to_threadpool(server->pool_connect, &WSS_connect, (void *)args);
            if ( unlikely(err != WSS_SUCCESS) ) {
                WSS_log_fatal("Failed adding connect job to worker pool");
                return err;
            }
        } else if ( unlikely(events[i].data.fd == close_pipefd[0]) ) {
            WSS_log_trace("Close pipe invoked");
            // Pipe file descriptor is used to interrupt blocking wait
            continue;
        } else {
            if ( events[i].events & EPOLLIN ) {
                WSS_log_trace("Session %d begins to read", events[i].data.fd);

                /**
                 * If new reads are ready
                 */
                wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->fd = events[i].data.fd;
                args->state = READING;

                err = WSS_poll_add_task_to_threadpool(server->pool_io, &WSS_work, (void *)args);
                if ( unlikely(err != WSS_SUCCESS) ) {
                    WSS_log_fatal("Failed adding read job to worker pool");
                    WSS_free((void **)&args);
                    return err;
                }
            }

            if ( events[i].events & EPOLLOUT ) {
                WSS_log_trace("Session %d begins to write", events[i].data.fd);

                /**
                 * If new writes are ready
                 */
                wss_thread_args_t *args = (wss_thread_args_t *) WSS_memorypool_alloc(server->thread_args_pool);
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->fd = events[i].data.fd;
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

#endif //WSS_EPOLL
