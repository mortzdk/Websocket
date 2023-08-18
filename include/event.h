#pragma once 

#ifndef WSS_EVENT_H
#define WSS_EVENT_H

#include <errno.h>

#include "core.h"
#include "error.h"
#include "server.h"
#include "session.h"
#include "threadpool.h"
#include "worker.h" 
#include "socket.h" 
#include "log.h" 
#include "core.h" 

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

/**
 * Function that adds function to threadpools task queue.
 *
 * @param 	pool	[threadpool_t *] 	"A threadpool instance"
 * @param 	func	[void (*)(void *)] 	"A function pointer to add to task queue"
 * @param 	args	[void *] 	        "Arguments to be served to the function"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_add_task_to_threadpool(threadpool_t *pool, void (*func)(void *), void *args);

/**
 * Pipes used to inform the event loop to stop since the application is closing
 */
extern int close_pipefd[2];

#endif
