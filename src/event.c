#include "event.h" 

#if defined(WSS_EPOLL)
#include <sys/epoll.h>
#elif defined(WSS_KQUEUE)
#include <sys/event.h>
#include <sys/time.h>
#elif defined(WSS_POLL)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef POLLRDHUP
#define POLLRDHUP  0x2000
#endif

#include <poll.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "server.h" 
#include "alloc.h" 
#include "worker.h" 
#include "socket.h" 
#include "pool.h" 
#include "log.h" 
#include "predict.h" 

int close_pipefd[2] = {-1, -1};

/**
 * Writes a rearm message on the rearm pipe.
 *
 * @param 	server	[wss_server_t *] 	"A wss_server_t instance"
 * @return 			[void]
 */
static inline void rearm(wss_server_t *server) {
    int n;

    WSS_log_trace("Notify about rearm");

    if (server->rearm_pipefd[0] != -1 && server->rearm_pipefd[1] != -1) {
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

    if (server->rearm_pipefd[0] != -1 && server->rearm_pipefd[1] != -1) {
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
 * Function that adds task-function and data instance to worker pool.
 *
 * @param 	server	[wss_server_t *] 	"A wss_server_t instance"
 * @param 	func	[void (*)(void *)] 	"A function pointer"
 * @param 	args	[void *] 	        "Arguments to be served to the function"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_add_to_threadpool(wss_server_t *server, void (*func)(void *), void *args) {
    int err, retries = 0;
    struct timespec tim;

    tim.tv_sec = 0;
    tim.tv_nsec = 100000000;

    do {
        if ( unlikely((err = threadpool_add(server->pool, func, args, 0) != 0) < 0) ) {
            switch (err) {
                case threadpool_invalid:
                    WSS_log_fatal("Threadpool was served with invalid data");
                    return WSS_THREADPOOL_INVALID_ERROR;
                case threadpool_lock_failure:
                    WSS_log_fatal("Locking in thread failed");
                    return WSS_THREADPOOL_LOCK_ERROR;
                case threadpool_queue_full:
                    if (retries < 5) {
                        retries += 1;
                        WSS_log_trace("Threadpool full, will retry shortly. Retry number: %d", retries);
                        nanosleep(&tim, NULL);
                        continue;
                    }

                    // Currently we treat a full threadpool as an error, but
                    // we could try to handle this by dynamically increasing size
                    // of threadpool, and maybe reset thread count to that of the
                    // configuration when the hot load is over
                    WSS_log_error("Threadpool queue is full");
                    return WSS_THREADPOOL_FULL_ERROR;
                case threadpool_shutdown:
                    WSS_log_error("Threadpool is shutting down");
                    return WSS_THREADPOOL_SHUTDOWN_ERROR;
                case threadpool_thread_failure:
                    WSS_log_fatal("Threadpool thread return an error");
                    return WSS_THREADPOOL_THREAD_ERROR;
                default:
                    WSS_log_fatal("Unknown error occured with threadpool");
                    return WSS_THREADPOOL_ERROR;
            }
        }
    } while (0);

    return WSS_SUCCESS;
}

/******************************************************************************
 *                                   KQUEUE                                   *
 ******************************************************************************/

#if defined(WSS_KQUEUE)

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

    if ( unlikely(NULL == (server->events = WSS_calloc(server->config->pool_workers, sizeof(event)))) ) {
        WSS_log_fatal("Unable to calloc server kqueue events");
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

    WSS_log_trace("Creating arm pipe file descriptors");

    if ( unlikely(pipe(server->rearm_pipefd) == -1) ) {
        WSS_log_trace("Unable to create arm pipe file descriptors: %s", strerror(errno));
        return WSS_POLL_PIPE_ERROR;
    }

    if ( (err = WSS_socket_non_blocking(server->rearm_pipefd[0])) != WSS_SUCCESS ) {
        return err;
    }

    if ( (err = WSS_socket_non_blocking(server->rearm_pipefd[1])) != WSS_SUCCESS ) {
        return err;
    }

    WSS_log_trace("Arms rearm pipe file descriptor to kqueue instance");

    do {
        errno = 0;
        EV_SET(&event, server->rearm_pipefd[0], EVFILT_READ, EV_ADD, 0, 0, ((void *) &server->rearm_pipefd[0])); 
        ret = kevent(server->poll_fd, &event, 1, NULL, 0, NULL);
    } while ( unlikely(errno == EINTR) );

    if ( unlikely(ret < 0) ) {
        WSS_log_error("Failed to (re)arm arm pipe file descriptor to kqueue: %s", strerror(errno));
        return WSS_POLL_SET_ERROR;
    }

    WSS_server_set_max_fd(server, server->rearm_pipefd[0]);

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

#ifdef USE_OPENSSL
    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Listening for HTTPS kqueue events");
    } else {
#endif
        WSS_log_trace("Listening for HTTP kqueue events");
#ifdef USE_OPENSSL
    }
#endif
    
    do {
        errno = 0;

        if (server->config->timeout_poll >= 0) {
            n = kevent(server->poll_fd, NULL, 0, events, server->config->pool_workers, &timeout);
        } else {
            n = kevent(server->poll_fd, NULL, 0, events, server->config->pool_workers, NULL);
        }
        
        if ( unlikely(n < 0) ) {
            if ( unlikely(errno != EINTR) ) {
                return WSS_POLL_WAIT_ERROR;
            }
        }
    } while ( unlikely(errno == EINTR) );

#ifdef USE_OPENSSL
    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Received %d HTTPS events", n);
    } else {
#endif
        WSS_log_trace("Received %d HTTP events", n);
#ifdef USE_OPENSSL
    }
#endif

    for (i = 0; i < n; i++) {
        fd = *((int *)events[i].udata);
        if (events[i].flags & (EV_EOF | EV_ERROR)) {
            if ( unlikely(fd == server->fd) ) {
                WSS_log_fatal("A server error occured upon kqueue");
                return WSS_POLL_WAIT_ERROR;
            }

            WSS_log_trace("Session %d disconnecting", fd);

            wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
            if ( unlikely(NULL == args) ) {
                WSS_log_fatal("Failed allocating threadpool argument");
                return WSS_MEMORY_ERROR;
            }
            args->server = server;
            args->fd = fd;
            args->state = CLOSING

            if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
                WSS_log_fatal("Failed adding disconnect job to worker pool");
                WSS_free((void **)&args);
                return err;
            }
        } else if ( fd == server->fd ) {
            WSS_log_trace("New session connecting");

            wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
            if ( unlikely(NULL == args) ) {
                WSS_log_fatal("Failed allocating threadpool argument");
                return WSS_MEMORY_ERROR;
            }
            args->server = server;
            args->state = CONNECTING

            /**
             * If new session has connected
             */
            if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
                WSS_log_fatal("Failed adding connect job to worker pool");
                return err;
            }
        } else if ( unlikely(fd == close_pipefd[0]) ) {
            // Pipe file descriptor is used to interrupt blocking wait
            continue;
        } else if ( likely(fd == server->rearm_pipefd[0]) ) {
            handle_rearm(server);
            continue;
        } else {
            if (events[i].filter == EVFILT_READ) {
                WSS_log_trace("Session %d begins to read", fd);

                /**
                 * If new reads are ready
                 */
                wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->fd = fd;
                args->state = READING

                if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
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
                wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->fd = fd;
                args->state = WRITING

                if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
                    WSS_log_fatal("Failed adding write job to worker pool");
                    WSS_free((void **)&args);
                    return err;
                }
            }
        }
    }

    return WSS_SUCCESS;
}

/******************************************************************************
 *                                   EPOLL                                    *
 ******************************************************************************/

#elif defined(WSS_EPOLL)
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

    if ( unlikely(NULL == (server->events = WSS_calloc(server->config->pool_workers, sizeof(event)))) ) {
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

    WSS_log_trace("Creating arm pipe file descriptors");

    if ( unlikely(pipe(server->rearm_pipefd) == -1) ) {
        WSS_log_trace("Unable to create arm pipe file descriptors: %s", strerror(errno));
        return WSS_POLL_PIPE_ERROR;
    }

    if ( (err = WSS_socket_non_blocking(server->rearm_pipefd[0])) != WSS_SUCCESS ) {
        return err;
    }

    if ( (err = WSS_socket_non_blocking(server->rearm_pipefd[1])) != WSS_SUCCESS ) {
        return err;
    }

    WSS_log_trace("Arms arm pipe file descriptor to poll instance");

    if ( unlikely((err = WSS_poll_add(server->poll_fd, server->rearm_pipefd[0], EPOLLIN | EPOLLET | EPOLLRDHUP)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_server_set_max_fd(server, server->rearm_pipefd[0]);

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

#ifdef USE_OPENSSL
    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Listening for HTTPS epoll events");
    } else {
#endif
        WSS_log_trace("Listening for HTTP epoll events");
#ifdef USE_OPENSSL
    }
#endif

    do {
        errno = 0;
        n = epoll_wait(server->poll_fd, events, server->config->pool_workers, server->config->timeout_poll);
        if ( unlikely(n < 0) ) {
            if ( unlikely(errno != EINTR) ) {
                return WSS_POLL_WAIT_ERROR;
            }
        }
    } while ( unlikely(errno == EINTR) );

#ifdef USE_OPENSSL
    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Received %d HTTPS events", n);
    } else {
#endif
        WSS_log_trace("Received %d HTTP events", n);
#ifdef USE_OPENSSL
    }
#endif

    for (i = 0; i < n; i++) {
        if ( unlikely((events[i].events & EPOLLHUP) ||
                    (events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLRDHUP)) ) {
            if ( unlikely(events[i].data.fd == server->fd) ) {
                WSS_log_fatal("A server error occured upon epoll");
                return WSS_POLL_WAIT_ERROR;
            }

            WSS_log_trace("Session %d disconnecting", events[i].data.fd);

            wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
            if ( unlikely(NULL == args) ) {
                WSS_log_fatal("Failed allocating threadpool argument");
                return WSS_MEMORY_ERROR;
            }
            args->server = server;
            args->fd = events[i].data.fd;
            args->state = CLOSING;

            if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
                WSS_log_fatal("Failed adding disconnect job to worker pool");
                WSS_free((void **)&args);
                return err;
            }
        } else if ( events[i].data.fd == server->fd ) {
            WSS_log_trace("New session connecting");

            wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
            if ( unlikely(NULL == args) ) {
                WSS_log_fatal("Failed allocating threadpool argument");
                return WSS_MEMORY_ERROR;
            }
            args->server = server;
            args->state = CONNECTING;

            /**
             * If new session has connected
             */
            if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
                WSS_log_fatal("Failed adding connect job to worker pool");
                return err;
            }
        } else if ( unlikely(events[i].data.fd == close_pipefd[0]) ) {
            // Pipe file descriptor is used to interrupt blocking wait
            continue;
        } else if ( likely(events[i].data.fd == server->rearm_pipefd[0]) ) {
            handle_rearm(server);
            continue;
        } else {
            if ( events[i].events & EPOLLIN ) {
                WSS_log_trace("Session %d begins to read", events[i].data.fd);

                /**
                 * If new reads are ready
                 */
                wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->fd = events[i].data.fd;
                args->state = READING;

                if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
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
                wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->fd = events[i].data.fd;
                args->state = WRITING;

                if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
                    WSS_log_fatal("Failed adding write job to worker pool");
                    WSS_free((void **)&args);
                    return err;
                }
            }
        }
    }
    
    return WSS_SUCCESS;
}

/******************************************************************************
 *                                    POLL                                    *
 ******************************************************************************/

#elif defined(WSS_POLL)
static unsigned int MAXEVENTS;

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
    wss_server_t *server = (wss_server_t *) s;

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

    if ( (err = WSS_socket_non_blocking(server->rearm_pipefd[0])) != WSS_SUCCESS ) {
        return err;
    }

    if ( (err = WSS_socket_non_blocking(server->rearm_pipefd[1])) != WSS_SUCCESS ) {
        return err;
    }

    WSS_log_trace("Arms arm pipe file descriptor to poll instance");

    if ((err = WSS_poll_set_read(server, server->rearm_pipefd[0])) != WSS_SUCCESS) {
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

#ifdef USE_OPENSSL
    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Listening for HTTPS poll events");
    } else {
#endif
        WSS_log_trace("Listening for HTTP poll events");
#ifdef USE_OPENSSL
    }
#endif
    
    do {
        errno = 0;

        n = poll(&events[start], end-start+1, server->config->timeout_poll);

        if ( unlikely(n < 0) ) {
            if ( unlikely(errno != EINTR) ) {
                return WSS_POLL_WAIT_ERROR;
            }
        }
    } while ( unlikely(errno == EINTR) );

#ifdef USE_OPENSSL
    if (server->ssl_ctx != NULL) {
        WSS_log_trace("Received %d HTTPS events", n);
    } else {
#endif
        WSS_log_trace("Received %d HTTP events", n);
#ifdef USE_OPENSSL
    }
#endif

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

                wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->fd = fd;
                args->state = CLOSING;

                if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
                    WSS_log_fatal("Failed adding disconnect job to worker pool");
                    WSS_free((void **)&args);
                    return err;
                }
            } else if (fd == server->fd) {
                WSS_log_trace("New session connecting");

                wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
                if ( unlikely(NULL == args) ) {
                    WSS_log_fatal("Failed allocating threadpool argument");
                    return WSS_MEMORY_ERROR;
                }
                args->server = server;
                args->state = CONNECTING;

                /**
                 * If new session has connected
                 */
                if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
                    WSS_log_fatal("Failed adding connect job to worker pool");
                    return err;
                }
            } else if ( unlikely(fd == close_pipefd[0]) ) {
                continue;
            } else if ( likely(fd == server->rearm_pipefd[0]) ) {
                handle_rearm(server);
                continue;
            } else {
                WSS_poll_remove(server, fd);

                if (events[i].revents & (POLLIN | POLLPRI)) {
                    WSS_log_trace("Session %d begins to read", fd);

                    /**
                     * If new reads are ready
                     */
                    wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
                    if ( unlikely(NULL == args) ) {
                        WSS_log_fatal("Failed allocating threadpool argument");
                        return WSS_MEMORY_ERROR;
                    }
                    args->server = server;
                    args->fd = fd;
                    args->state = READING;

                    if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
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
                    wss_thread_args_t *args = (wss_thread_args_t *) WSS_malloc(sizeof(wss_thread_args_t));
                    if ( unlikely(NULL == args) ) {
                        WSS_log_fatal("Failed allocating threadpool argument");
                        return WSS_MEMORY_ERROR;
                    }
                    args->server = server;
                    args->fd = fd;
                    args->state = WRITING;

                    if ( unlikely((err = WSS_add_to_threadpool(server, &WSS_work, (void *)args)) != WSS_SUCCESS) ) {
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
#endif

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
