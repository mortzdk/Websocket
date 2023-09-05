#include <errno.h> 				/* errno */
#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen */
#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <stdbool.h>            
#include <math.h> 				/* log10 */
#include <unistd.h> 			/* close */
#include <regex.h>              /* regex_t, regcomp, regexec */

#include <sys/socket.h>         /* socket, setsockopt, inet_ntoa, accept */
#include <netinet/in.h>         /* sockaddr_in, inet_ntoa */
#include <arpa/inet.h>          /* htonl, htons, inet_ntoa */

#include "http.h"
#include "event.h"
#include "alloc.h"
#include "config.h"
#include "error.h"
#include "log.h"
#include "socket.h"
#include "core.h"
#include "ssl.h"

/**
 * Generates a regular expression pattern to match the request uri of the header.
 *
 * @param   config    [wss_config_t *]  "The configuration of the server"
 * @param   ssl       [bool]            "Whether server uses SSL"
 * @param   port      [int]             "The server port"
 * @return            [char *]          "The request uri regex pattern"
 */
static char *generate_request_uri(wss_config_t * config, bool ssl, int port) {
    int i, j, k;
    size_t iw = 0, jw = 0, kw = 0;
    size_t request_uri_length = 0;
    size_t sum_host_length = 0; 
    size_t sum_path_length = 0; 
    size_t sum_query_length = 0; 
    char *request_uri;
    char *host = "";
    char *path = "";
    char *query = "";
    char *s = "";

    for (i = 0; i < config->hosts_length; i++) {
        sum_host_length += strlen(config->hosts[i]); 
    }
    sum_host_length += WSS_MAX(config->hosts_length-1, 0);

    for (j = 0; j < config->paths_length; j++) {
        sum_path_length += strlen(config->paths[j]); 
    }
    sum_path_length += WSS_MAX(config->paths_length-1, 0);

    for (k = 0; k < config->queries_length; k++) {
        sum_query_length += strlen(config->queries[k]); 
    }
    sum_query_length += WSS_MAX(config->queries_length-1, 0);

    if (sum_host_length+sum_path_length+sum_query_length == 0) {
        return NULL;
    }

    request_uri_length += strlen(REQUEST_URI)*sizeof(char)-12*sizeof(char);
    request_uri_length += ssl*sizeof(char);
    request_uri_length += (log10(port)+1)*sizeof(char);
    request_uri_length += sum_host_length*sizeof(char);
    request_uri_length += sum_path_length*sizeof(char);
    request_uri_length += sum_query_length*sizeof(char);
    request_uri_length += sum_query_length*sizeof(char);
    request_uri = (char *) WSS_malloc(request_uri_length+1*sizeof(char));

    if (ssl) {
        s = "s";
    }

    if ( unlikely(sum_host_length > 0 && NULL == (host = WSS_malloc(sum_host_length+1))) ) {
        return NULL;
    }

    if ( unlikely(sum_path_length > 0 && NULL == (path = WSS_malloc(sum_path_length+1))) ) {
        return NULL;
    }

    if ( unlikely(sum_query_length > 0 && NULL == (query = WSS_malloc(sum_query_length+1))) ) {
        return NULL;
    }

    for (i = 0; likely(i < config->hosts_length); i++) {
        if ( unlikely(i+1 == config->hosts_length) ) {
            sprintf(host+iw, "%s", config->hosts[i]);
        } else {
            sprintf(host+iw, "%s|", config->hosts[i]);
            iw++;
        }
        iw += strlen(config->hosts[i]);
    }

    for (j = 0; likely(j < config->paths_length); j++) {
        if ( unlikely(j+1 == config->paths_length) ) {
            sprintf(path+jw, "%s", config->paths[j]);
        } else {
            sprintf(path+jw, "%s|", config->paths[j]);
            jw++;
        }
        jw += strlen(config->paths[j]);
    }

    for (k = 0; likely(k < config->queries_length); k++) {
        if ( unlikely(k+1 == config->queries_length) ) {
            sprintf(query+kw, "%s", config->queries[k]);
        } else {
            sprintf(query+kw, "%s|", config->queries[k]);
            kw++;
        }
        kw += strlen(config->queries[k]);
    }

    sprintf(request_uri, REQUEST_URI, s, host, port, path, query, query);

    if ( likely(strlen(host) > 0) ) {
        WSS_free((void **) &host);
    }

    if ( likely(strlen(path) > 0) ) {
        WSS_free((void **) &path);
    }

    if ( likely(strlen(query) > 0) ) {
        WSS_free((void **) &query);
    }

    return request_uri;
}

/**
 * Function that initializes a regex for the server instance that can be used 
 * to validate the connecting path of the client.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_http_regex_init(wss_server_t *server) {
    int err;
    char *request_uri;

    request_uri = generate_request_uri(server->config, (NULL != server->ssl_ctx), server->port);
    if ( likely(request_uri != NULL) ) {
        if ( unlikely((err = regcomp(&server->re, request_uri, REG_EXTENDED|REG_NOSUB)) != 0) ) {
            WSS_free((void **) &request_uri);
            return WSS_REGEX_ERROR;
        }

        WSS_free((void **) &request_uri);
    }

    return WSS_SUCCESS;
}

/**
 * Function that initializes a http server instance and creating thread where
 * the instance is being run.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_http_server(wss_server_t *server) {
    int err;
    char straddr[INET6_ADDRSTRLEN];

    if (NULL == server->ssl_ctx) {
        WSS_log_trace("Starting HTTP instance");
    } else {
        WSS_log_trace("Starting HTTPS instance");
    }

    WSS_log_trace("Assigning server to port %d", server->port);

    /**
     * Setting port and initializes filedescriptors to -1 in the server
     * structure.
     */
    server->fd = -1;
    server->poll_fd = -1;

    WSS_log_trace("Creating socket filedescriptor");
    if ( unlikely((err = WSS_socket_create(server)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_log_trace("Allowing reuse of socket");
    if ( unlikely((err = WSS_socket_reuse(server->fd)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_log_trace("Creating socket structure for instance");
    if ( unlikely((err = WSS_socket_bind(server)) != WSS_SUCCESS) ) {
        return err;
    }

    if ( likely(NULL != inet_ntop(AF_INET6, (const void *)&server->info, straddr, sizeof(straddr)))) {
        WSS_log_trace("Binding address of server to: ", straddr, server->port);
    }

    WSS_log_trace("Making server socket non-blocking");
    if ( unlikely((err = WSS_socket_non_blocking(server->fd)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_log_trace("Starts listening to the server socket");
    if ( unlikely((err = WSS_socket_listen(server->fd)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_log_trace("Creating wss_thread_args_t memory pool");
    server->thread_args_pool = WSS_memorypool_create(
            server->config->pool_connect_tasks*server->config->pool_connect_workers+
            server->config->pool_io_tasks*server->config->pool_io_workers,
            sizeof(wss_thread_args_t)
            );
    if (unlikely(NULL == server->thread_args_pool)) {
        return WSS_MEMORY_ERROR;
    }

    WSS_log_trace("Creating threadpool");
    if ( unlikely((err = WSS_socket_threadpool(server->config->pool_connect_workers, 
                        server->config->pool_connect_tasks, 
                        server->config->size_thread, 
                        &server->pool_connect)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_log_trace("Creating IO threadpool");
    if ( unlikely((err = WSS_socket_threadpool(server->config->pool_io_workers, 
                        server->config->pool_io_tasks, 
                        server->config->size_thread, 
                        &server->pool_io)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_log_trace("Initializing server regexp");
    if ( unlikely((err = WSS_http_regex_init(server)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_log_trace("Creating wss_frame_t memory pool");
    server->frame_pool = WSS_memorypool_create(server->config->pool_io_workers * server->config->max_frames, sizeof(wss_frame_t));
    if (unlikely(NULL == server->frame_pool)) {
        return WSS_MEMORY_ERROR;
    }

    WSS_log_trace("Creating wss_message_t memory pool");
    server->message_pool = WSS_memorypool_create(
            server->config->size_ringbuffer*server->config->pool_io_workers,
            sizeof(wss_message_t)
            );
    if (unlikely(NULL == server->message_pool)) {
        return WSS_MEMORY_ERROR;
    }

    WSS_log_trace("Initializing server poll");
    if ( unlikely(WSS_SUCCESS != (err = WSS_poll_init(server))) ) {
        return err;
    }

    WSS_log_trace("Creating server thread");
    if ( unlikely(pthread_create(&server->thread_id, NULL, WSS_server_run, (void *) server) != 0) ) {
        WSS_log_error("Unable to create server thread", strerror(errno));
        return WSS_THREAD_CREATE_ERROR;
    }

    return WSS_SUCCESS;
}

/**
 * Function that free op space allocated for the http server and closes the
 * filedescriptors in use..
 *
 * @param   server	[wss_server_t *] 	"The http server"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_http_server_free(wss_server_t *server) {
    int err;
    int res = WSS_SUCCESS;

    if ( likely(NULL != server) ) {
        /**
         * Shutting down socket, such that no more reads is allowed.
         */
        if ( likely(server->fd > -1) ) {
            WSS_log_trace("Shutting down reads for server socket");
            if ( unlikely(shutdown(server->fd, SHUT_RD) != 0) ) {
                WSS_log_error("Unable to shutdown for reads of server socket: %s", strerror(errno));
                res = WSS_SOCKET_SHUTDOWN_ERROR;
            }
        }

        /**
         * Shutting down threadpool gracefully
         */
        if ( likely(NULL != server->pool_io) ) {
            WSS_log_trace("Shutting down io threadpool gracefully");
            if ( unlikely((err = threadpool_destroy(server->pool_io, threadpool_graceful)) != 0) ) {
                WSS_log_error("Unable to destroy threadpool gracefully: %s", threadpool_strerror(errno));

                switch (err) {
                    case threadpool_invalid:
                        res = WSS_THREADPOOL_LOCK_ERROR;
                        break;
                    case threadpool_lock_failure:
                        res = WSS_THREADPOOL_LOCK_ERROR;
                        break;
                    case threadpool_queue_full:
                        res = WSS_THREADPOOL_FULL_ERROR;
                        break;
                    case threadpool_shutdown:
                        res = WSS_THREADPOOL_SHUTDOWN_ERROR;
                        break;
                    case threadpool_thread_failure:
                        res = WSS_THREADPOOL_THREAD_ERROR;
                        break;
                    default:
                        res = WSS_THREADPOOL_ERROR;
                        break;
                }
            }
        }

        /**
         * Shutting down threadpool gracefully
         */
        if ( likely(NULL != server->pool_connect) ) {
            WSS_log_trace("Shutting down connect threadpool gracefully");
            if ( unlikely((err = threadpool_destroy(server->pool_connect, threadpool_graceful)) != 0) ) {
                WSS_log_error("Unable to destroy threadpool gracefully: %s", threadpool_strerror(errno));

                switch (err) {
                    case threadpool_invalid:
                        res = WSS_THREADPOOL_LOCK_ERROR;
                        break;
                    case threadpool_lock_failure:
                        res = WSS_THREADPOOL_LOCK_ERROR;
                        break;
                    case threadpool_queue_full:
                        res = WSS_THREADPOOL_FULL_ERROR;
                        break;
                    case threadpool_shutdown:
                        res = WSS_THREADPOOL_SHUTDOWN_ERROR;
                        break;
                    case threadpool_thread_failure:
                        res = WSS_THREADPOOL_THREAD_ERROR;
                        break;
                    default:
                        res = WSS_THREADPOOL_ERROR;
                        break;
                }
            }
        }

        /**
         * Destroying thread_args memory pool
         */
        WSS_log_trace("Waiting for thread args memory pool to be empty");
        WSS_memorypool_destroy(server->thread_args_pool);

        /**
         * Destroying frame memory pool
         */
        WSS_log_trace("Waiting for frame memory pool to be empty");
        WSS_memorypool_destroy(server->frame_pool);

        /**
         * Destroying message memory pool
         */
        WSS_log_trace("Waiting for message memory pool to be empty");
        WSS_memorypool_destroy(server->message_pool);

        /**
         * Freeing regexp
         */
        WSS_log_trace("Free server regexp");
        regfree(&server->re);

        /**
         * Freeing epoll structures
         */
        WSS_log_trace("Free server events");
        WSS_free((void **) &server->events);

        /**
         * Closing epoll
         */
        WSS_log_trace("Closing poll file descriptors");
        if ( likely(server->poll_fd > -1) ) {
            if ( unlikely(close(server->poll_fd) != 0) ) {
                WSS_log_error("Unable to close servers epoll filedescriptor: %s", strerror(errno));
                res = WSS_SOCKET_CLOSE_ERROR;
            }

            server->poll_fd = -1;
        }

        /**
         * Closing socket
         */
        WSS_log_trace("Closing socket");
        if ( likely(server->fd > -1)) {
            if ( unlikely(close(server->fd) != 0) ) {
                WSS_log_error("Unable to close servers filedescriptor: %s", strerror(errno));
                res = WSS_SOCKET_CLOSE_ERROR;
            }
            server->fd = -1;
        }

        if (NULL != server->ssl_ctx) {
            WSS_log_trace("Cleaning up SSL");
            WSS_http_ssl_free(server);
        }
    }

    return res;
}
