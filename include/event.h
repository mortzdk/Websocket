#ifndef WSS_EVENT_H
#define WSS_EVENT_H

#if defined(__linux__) && !defined(USE_POLL)
#define WSS_EPOLL 1
#elif defined(__APPLE__)   || defined(__FreeBSD__) || defined(__NetBSD__) || \
      defined(__OpenBSD__) || defined(__bsdi__)    || defined(__DragonFly__)
#define WSS_KQUEUE 1
#else
#define WSS_POLL 1
#endif

#include "server.h"
#include "session.h"
#include "error.h"

/**
 * Structure used to send from event loop to threadpool
 */
typedef struct {
    int fd;
    wss_server_t *server;
    wss_session_state_t state;
} wss_thread_args_t;

/**
 * Function that creates poll instance and adding the filedescriptor of the
 * servers socket to it.
 *
 * @param 	server	    [wss_server_t *]	"A pointer to a server structure"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_init(wss_server_t *server);

/**
 * Function that rearms the poll instance for write events with the clients
 * filedescriptor
 *
 * @param 	server	    [wss_server_t *]	"A pointer to a server structure"
 * @param 	fd	        [int]	            "The clients file descriptor"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_set_write(wss_server_t *server, int fd);

/**
 * Function that rearms the poll instance for read events with the clients
 * filedescriptor
 *
 * @param 	server	    [wss_server_t *]	"A pointer to a server structure"
 * @param 	fd	        [int]	            "The clients file descriptor"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_set_read(wss_server_t *server, int fd);

/**
 * Function removes the client filedescriptor from the poll instance 
 *
 * @param 	server	    [wss_server_t *]	"A pointer to a server structure"
 * @param 	fd	        [int]	            "The clients file descriptor"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_remove(wss_server_t *server, int fd);

/**
 * Function that listens for new events on the servers file descriptor 
 *
 * @param 	server	    [wss_server_t *]	"A pointer to a server structure"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_delegate(wss_server_t *server);

/**
 * Function that cleanup poll function when closing
 *
 * @param 	server	    [wss_server_t *]	"A pointer to a server structure"
 * @return 			    [wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_close(wss_server_t *server);

extern int close_pipefd[2];

#endif
