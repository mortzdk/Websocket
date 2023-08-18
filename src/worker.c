
#include <errno.h> 				/* errno */
#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen */
#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <unistd.h>             /* close, read, write */
#include <math.h> 				/* log10 */
#include <time.h>
#include <locale.h>
#include <libgen.h>

#include <sys/types.h>          /* socket, setsockopt, accept, send, recv */
#include <sys/stat.h> 			/* stat */
#include <sys/socket.h>         /* socket, setsockopt, inet_ntoa, accept */
#include <sys/select.h>         /* socket, setsockopt, inet_ntoa, accept */
#include <netinet/in.h>         /* sockaddr_in, inet_ntoa */
#include <arpa/inet.h>          /* htonl, htons, inet_ntoa */

#include <pthread.h> 			/* pthread_create, pthread_t, pthread_attr_t
                                   pthread_mutex_init */

#include "alloc.h"
#include "worker.h"
#include "server.h"
#include "event.h"
#include "log.h"
#include "session.h"
#include "socket.h"
#include "error.h"
#include "header.h"
#include "frame.h"
#include "message.h"
#include "config.h"
#include "httpstatuscodes.h"
#include "str.h"
#include "utf8.h"
#include "error.h"
#include "core.h"
#include "ssl.h"
#include "memorypool.h"

/**
 * Function that generates a handshake response, used to authorize a websocket
 * session.
 *
 * @param   header  [wss_header_t*]         "The http header obtained from the session"
 * @param   code    [enum HttpStatus_Code]  "The http status code to return"
 * @param   msg     [wss_message_t *]       "The message structure to be filled with the response"
 * @return          [wss_error_t]           "WSS_SUCCESS on success, otherwise some error"
 */
static wss_error_t handshake_response(wss_header_t *header, enum HttpStatus_Code code, wss_message_t *msg) {
    int length;
    size_t j;
    size_t offset = 0;
    char *message;
    char *version = "HTTP/1.1";
    const char *reason = HttpStatus_reasonPhrase(code);
    int line = 0, headers = 0;
    char sha1Key[SHA_DIGEST_LENGTH];
    int accept_length = (SHA_DIGEST_LENGTH * 4) / 3 + 2;
    char acceptKey[accept_length];
    char *acceptKeyPtr = &acceptKey[0];
    int magic_length = strlen(MAGIC_WEBSOCKET_KEY);
    int key_length = strlen(header->ws_key) + magic_length;
    char key[key_length];
    size_t acceptKeyLength;

    if ( likely(NULL != header->version) ) {
        version = header->version;
    }

    line += strlen(HTTP_STATUS_LINE)*sizeof(char)-6*sizeof(char);
    line += strlen(version)*sizeof(char);
    line += (log10(code)+1)*sizeof(char);
    line += strlen(reason)*sizeof(char);

    // Generate accept key
    memset(key, '\0', key_length);
    memset(sha1Key, '\0', SHA_DIGEST_LENGTH);
    memset(acceptKey, '\0', (SHA_DIGEST_LENGTH * 4) / 3 + 2);
    memcpy(key+(key_length-magic_length), MAGIC_WEBSOCKET_KEY, magic_length);
    memcpy(key, header->ws_key, (key_length-magic_length));

    acceptKeyLength = WSS_base64_encode_sha1(key, key_length, &acceptKeyPtr);
    if ( unlikely(acceptKeyLength == 0) ) {
        return WSS_MEMORY_ERROR;
    }

    if ( NULL != header->ws_extensions ) {
        headers += strlen(HTTP_HANDSHAKE_EXTENSIONS)*sizeof(char);
        for (j = 0; likely(j < header->ws_extensions_count); j++) {
            headers += (strlen(header->ws_extensions[j]->name))*sizeof(char);
            if ( likely(NULL != header->ws_extensions[j]->accepted) ) {
                headers += strlen(header->ws_extensions[j]->accepted)*sizeof(char);
            }
            headers += 2*sizeof(char);
        }
        headers += (header->ws_extensions_count)*sizeof(char);
    }
    
    if ( NULL != header->ws_protocol ) {
        headers += strlen(HTTP_HANDSHAKE_SUBPROTOCOL)*sizeof(char);
        headers += strlen(header->ws_protocol->name)*sizeof(char);
        headers += 2*sizeof(char);
    }

    headers += strlen(HTTP_HANDSHAKE_ACCEPT)*sizeof(char);
    headers += acceptKeyLength*sizeof(char);
    headers += 2*sizeof(char);
    headers += strlen(HTTP_HANDSHAKE_HEADERS)*sizeof(char)-2*sizeof(char);
    headers += strlen(WSS_SERVER_VERSION)*sizeof(char);

    length = line + headers;
    if ( unlikely(NULL == (message = (char *) WSS_malloc((length+1)*sizeof(char)))) ) {
        return WSS_MEMORY_ERROR;
    }

    sprintf(message+offset, HTTP_STATUS_LINE, version, code, reason);
    offset += line;
    if ( NULL != header->ws_extensions ) {
        sprintf(message+offset, HTTP_HANDSHAKE_EXTENSIONS);
        offset += strlen(HTTP_HANDSHAKE_EXTENSIONS);
        for (j = 0; likely(j < header->ws_extensions_count); j++) {
            memcpy(message+offset, header->ws_extensions[j]->name, strlen(header->ws_extensions[j]->name));
            offset += strlen(header->ws_extensions[j]->name);

            if ( likely(NULL != header->ws_extensions[j]->accepted) ) {
                message[offset] = ';';
                offset += 1;

                memcpy(message+offset, header->ws_extensions[j]->accepted, strlen(header->ws_extensions[j]->accepted));
                offset += strlen(header->ws_extensions[j]->accepted);
            }

            if ( likely(j+1 != header->ws_extensions_count) ) {
                message[offset] = ',';
                offset += 1;
            }
        }
        sprintf(message+offset, "\r\n");
        offset += 2;
    }
    if (NULL != header->ws_protocol) {
        sprintf(message+offset, HTTP_HANDSHAKE_SUBPROTOCOL);
        offset += strlen(HTTP_HANDSHAKE_SUBPROTOCOL);
        memcpy(message+offset, header->ws_protocol->name, strlen(header->ws_protocol->name));
        offset += strlen(header->ws_protocol->name);
        sprintf(message+offset, "\r\n");
        offset += 2;
    }
    sprintf(message+offset, HTTP_HANDSHAKE_ACCEPT);
    offset += strlen(HTTP_HANDSHAKE_ACCEPT);
    memcpy(message+offset, acceptKey, acceptKeyLength);
    offset += acceptKeyLength;
    sprintf(message+offset, "\r\n");
    offset += 2;
    sprintf(message+offset, HTTP_HANDSHAKE_HEADERS, WSS_SERVER_VERSION);

    WSS_log_debug("Handshake Response: \n%s", message);

    msg->msg = message;
    msg->length = length;
    msg->framed = 0;

    return WSS_SUCCESS;
}

/**
 * Function that generates a favicon response, used to present a favicon on the
 * HTTP landing page of the WSS server.
 *
 * @param   header  [wss_header_t *]        "The http header obtained from the session"
 * @param   code    [enum HttpStatus_Code]  "The http status code to return"
 * @param   config  [wss_config_t *]        "The configuration of the server"
 * @param   msg     [wss_message_t *]       "The message structure to be filled with the response"
 * @return          [wss_error_t]           "WSS_SUCCESS on success, otherwise some error"
 */
static wss_error_t favicon_response(wss_header_t *header, enum HttpStatus_Code code, wss_config_t *config, wss_message_t *msg) {
    time_t now;
    struct tm tm;
    char *ico;
    char *message;
    size_t length, etag_length;
    struct stat filestat;
    char sha1Key[SHA_DIGEST_LENGTH];
    char *sha1key_ptr = &sha1Key[0];
    char date[GMT_FORMAT_LENGTH];
    char modified[GMT_FORMAT_LENGTH];
    char etag[SHA_DIGEST_LENGTH];
    char *etag_pointer = &etag[0];
    char savedlocale[256];
    char *version = "HTTP/1.1";
    const char *reason  = HttpStatus_reasonPhrase(code);
    int icon = 0, line = 0, headers = 0;

    // Get GMT current time
    strncpy(savedlocale, setlocale(LC_ALL, NULL), 255);
    setlocale(LC_ALL, "C");
    now = time(0);
    tm = *gmtime(&now);
    memset(date, '\0', GMT_FORMAT_LENGTH);
    memset(modified, '\0', GMT_FORMAT_LENGTH);
    strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    setlocale(LC_ALL, savedlocale);

    if ( likely(NULL != header->version) ) {
        version = header->version;
    }

    line += strlen(HTTP_STATUS_LINE)*sizeof(char)-6*sizeof(char);
    line += strlen(version)*sizeof(char);
    line += (log10(code)+1)*sizeof(char);
    line += strlen(reason)*sizeof(char);

    // Read the content of the favicon file
    if ( NULL != config->favicon && access(config->favicon, F_OK) != -1 ) {
        // Get last modified
        stat(config->favicon, &filestat);
        setlocale(LC_ALL, "C");
        tm = *gmtime(&filestat.st_mtime);
        strftime(modified, sizeof modified, "%a, %d %b %Y %H:%M:%S %Z", &tm);
        setlocale(LC_ALL, savedlocale);

        icon = strload(config->favicon, &ico);
    } else {
        return WSS_FILE_ACCESS_ERROR;
    }

    // Generate etag from favicon content
    WSS_sha1(ico, icon, (char **)&sha1key_ptr);

    // Convert etag binary values to hex values
    etag_length = bin2hex((const unsigned char *)sha1Key, SHA_DIGEST_LENGTH, &etag_pointer);

    headers += strlen(HTTP_ICO_HEADERS)*sizeof(char)-10*sizeof(char);
    headers += (log10(icon)+1);
    headers += strlen(date)*sizeof(char);
    headers += strlen(WSS_SERVER_VERSION)*sizeof(char);
    headers += etag_length*sizeof(char);
    headers += strlen(modified)*sizeof(char);

    length = line + headers + icon + 1;
    if ( unlikely(NULL == (message = (char *) WSS_malloc(length*sizeof(char)))) ) {
        WSS_free((void **) &ico);
        return WSS_MEMORY_ERROR;
    }

    sprintf(message, HTTP_STATUS_LINE, version, code, reason);
    sprintf(message+line, HTTP_ICO_HEADERS, icon, date, WSS_SERVER_VERSION, etag, modified);
    memcpy(message+(line+headers), ico, icon);

    msg->msg = message;
    msg->length = length;
    msg->framed = 0;

    WSS_free((void **) &ico);

    return WSS_SUCCESS;
}

/**
 * Function that generates an HTTP upgrade response, used to tell the connecting
 * client that an upgrade is needed in order to use the server.
 *
 * @param   header  [wss_header_t *]        "The http header obtained from the session"
 * @param   code    [enum HttpStatus_Code]  "The http status code to return"
 * @param   exp     [char *]                "An explanation of what caused this response"
 * @param   msg     [wss_message_t *]       "The message structure to be filled with the response"
 * @return          [wss_error_t]           "WSS_SUCCESS on success, otherwise some error"
 */
static wss_error_t upgrade_response(wss_header_t *header, enum HttpStatus_Code code, char *exp, wss_message_t *msg) {
    int length;
    char *message;
    char *version = "HTTP/1.1";
    const char *reason = HttpStatus_reasonPhrase(code);
    int body = 0, line = 0, headers = 0;

    if ( likely(NULL != header->version) ) {
        version = header->version;
    }

    line += strlen(HTTP_STATUS_LINE)*sizeof(char)-6*sizeof(char);
    line += strlen(version)*sizeof(char);
    line += (log10(code)+1)*sizeof(char);
    line += strlen(reason)*sizeof(char);

    body += strlen(exp)*sizeof(char);

    headers += strlen(HTTP_UPGRADE_HEADERS)*sizeof(char)-4*sizeof(char);
    headers += strlen(WSS_SERVER_VERSION)*sizeof(char);
    headers += (log10(body)+1)*sizeof(char);

    length = line + headers + body + 1;
    if ( unlikely(NULL == (message = (char *) WSS_malloc(length*sizeof(char)))) ) {
        return WSS_MEMORY_ERROR;
    }

    sprintf(message, HTTP_STATUS_LINE, version, code, reason);
    sprintf(message+line, HTTP_UPGRADE_HEADERS, body, WSS_SERVER_VERSION);
    sprintf(message+line+headers, "%s", exp);

    msg->msg = message;
    msg->length = length;
    msg->framed = 0;

    return WSS_SUCCESS;
}

/**
 * Function that generates a HTTP error response, used to tell the connecting
 * client that an error occured.
 *
 * @param   header  [wss_header_t *]        "The http header obtained from the session"
 * @param   code    [enum HttpStatus_Code]  "The http status code to return"
 * @param   exp     [char *]                "An explanation of what caused this response"
 * @param   msg     [wss_message_t *]       "The message structure to be filled with the response"
 * @return          [wss_error_t]           "WSS_SUCCESS on success, otherwise some error"
 */
static wss_error_t http_response(wss_header_t *header, enum HttpStatus_Code code, char *exp, wss_message_t *msg) {
    time_t now;
    struct tm tm;
    int length;
    char *message;
    char date[GMT_FORMAT_LENGTH];
    char savedlocale[256];
    char *version = "HTTP/1.1";
    const char *reason  = HttpStatus_reasonPhrase(code);
    int body = 0, line = 0, headers = 0, support = 0;

    savedlocale[255] = '\0';

    // Get GMT current time
    strncpy(savedlocale, setlocale(LC_ALL, NULL), 255);
    setlocale(LC_ALL, "C");
    now = time(0);
    tm = *gmtime(&now);
    strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    setlocale(LC_ALL, savedlocale);

    if ( likely(NULL != header->version) ) {
        version = header->version;
    }

    line += strlen(HTTP_STATUS_LINE)*sizeof(char)-6*sizeof(char);
    line += strlen(version)*sizeof(char);
    line += (log10(code)+1)*sizeof(char);
    line += strlen(reason)*sizeof(char);

    body += strlen(HTTP_BODY)*sizeof(char)-8*sizeof(char);
    body += (log10(code)+1)*sizeof(char);
    body += strlen(reason)*sizeof(char);
    body += strlen(reason)*sizeof(char);
    body += strlen(exp)*sizeof(char);

    if ( unlikely(code == HttpStatus_NotImplemented) ) {
        support = strlen(HTTP_WS_VERSION_HEADER)*sizeof(char)-6*sizeof(char);
    }
    headers += strlen(HTTP_HTML_HEADERS)*sizeof(char)-6*sizeof(char);
    headers += strlen(WSS_SERVER_VERSION)*sizeof(char);
    headers += strlen(date)*sizeof(char);
    headers += (log10(body)+1)*sizeof(char);

    length = line + support + headers + body + 1;
    if ( unlikely(NULL == (message = (char *) WSS_malloc(length*sizeof(char)))) ) {
        return WSS_MEMORY_ERROR;
    }

    sprintf(message, HTTP_STATUS_LINE, version, code, reason);
    if ( unlikely(code == HttpStatus_NotImplemented) ) {
        sprintf(message+support, HTTP_WS_VERSION_HEADER, RFC6455, HYBI10, HYBI07); 
    }
    sprintf(message+support+line, HTTP_HTML_HEADERS, body, date, WSS_SERVER_VERSION);
    sprintf(message+support+line+headers, HTTP_BODY, code, reason, reason, exp);

    msg->msg = message;
    msg->length = length;
    msg->framed = 0;

    return WSS_SUCCESS;
}

/**
 * Function that disconnects a session and freeing any allocated memory used by
 * the session.
 *
 * @param 	server	[wss_server_t *] 	"The server structure"
 * @param 	session	[wss_session_t *] 	"The session structure"
 * @return          [void]
 */
void WSS_disconnect(wss_server_t *server, wss_session_t *session) {
    int i;
    wss_error_t err;
    bool dc;

    // If we are already closing
    WSS_session_is_disconnecting(session, &dc);
    if (dc) {
        return;
    }

    WSS_session_jobs_wait(session);

    session->state = CLOSING;

    WSS_log_trace("Informing subprotocol of client with file descriptor %d disconnecting", session->fd);

    if ( likely(session->header.ws_protocol != NULL) ) {
        WSS_log_trace("Informing subprotocol about session close");
        session->header.ws_protocol->close(session->fd);
    }

    WSS_log_trace("Removing poll filedescriptor from eventlist");

    WSS_poll_remove(server, session->fd);

    WSS_log_trace("Deleting client session");

    if (NULL == server->ssl_ctx) {
        WSS_log_info("Session %d disconnected from ip: %s:%d using HTTP request", session->fd, session->ip, session->port);
    } else {
        WSS_log_info("Session %d disconnected from ip: %s:%d using HTTPS request", session->fd, session->ip, session->port);
    }

    for (i = 0; i < session->jobs; i++) {
        pthread_mutex_unlock(&session->lock);
    }

    if ( unlikely(WSS_SUCCESS != (err = WSS_session_delete(session))) ) {
        switch (err) {
            case WSS_SSL_SHUTDOWN_READ_ERROR:
                session->event = READ;
                WSS_log_error("Unable to delete client session, must wait to read more", err);
                return;
            case WSS_SSL_SHUTDOWN_WRITE_ERROR:
                session->event = WRITE;
                WSS_log_error("Unable to delete client session, must wait to write more", err);
                return;
            default:
                break;
        }
        WSS_log_error("Unable to delete client session, received error code: %d", err);
        return;
    }
}

/**
 * Function that handles new connections. This function creates a new session and
 * associates the sessions filedescriptor to the epoll instance such that we can
 * start communicating with the session.
 *
 * @param 	server	[wss_server_t *] 	"The server structure"
 * @return          [void]
 */
void WSS_connect(void *args) {
    int client_fd;
    struct sockaddr_in client;
    size_t ringbuf_obj_size;
    wss_session_t *session;
    ringbuf_t *ringbuf;
    wss_thread_args_t *arguments = (wss_thread_args_t *) args;
    wss_server_t *server = (wss_server_t *) arguments->server;
    size_t workers = server->config->pool_connect_workers+server->config->pool_io_workers+1;
    socklen_t client_size = sizeof(client);

    // Free arguments structure as this won't be needed no more
    WSS_memorypool_dealloc(server->thread_args_pool, arguments);

    memset((char *) &client, '\0', sizeof(client));

    WSS_log_trace("Accepting incoming connection");

    if ( (client_fd = accept(server->fd, (struct sockaddr *) &client, &client_size)) < 0 ) {
        if ( unlikely(EAGAIN != errno && EWOULDBLOCK != errno) ) {
            WSS_log_error("Accept failed: %s", strerror(errno));
        }

        return;
    }

    WSS_log_trace("Received incoming connection");

    WSS_socket_non_blocking(client_fd);

    WSS_log_trace("Client filedescriptor was set to non-blocking");

    if ( unlikely(NULL == (session = WSS_session_add(client_fd,
                    inet_ntoa(client.sin_addr), ntohs(client.sin_port)))) ) {
        return;
    }

    pthread_mutex_lock(&session->lock);
    WSS_session_jobs_inc(session);

    session->state = CONNECTING;
    WSS_log_trace("Created client session: %d", client_fd);

    // Creating ringbuffer for session
    ringbuf_get_sizes(0, workers, &ringbuf_obj_size, NULL);
    if ( unlikely(NULL == (ringbuf = WSS_malloc(ringbuf_obj_size))) ) {
        WSS_log_error("Failed to allocate memory for ringbuffer");
        WSS_disconnect(server, session);
        return;
    }

    session->message_pool = server->message_pool;
    if ( unlikely(NULL == (session->messages = WSS_malloc(server->config->size_ringbuffer*sizeof(wss_message_t *)))) ) {
        WSS_log_error("Failed to allocate memory for ringbuffer messages");
        WSS_free((void **)&ringbuf);
        WSS_disconnect(server, session);
        return;
    }
    session->messages_count = server->config->size_ringbuffer;

    ringbuf_setup(ringbuf, 0, workers, server->config->size_ringbuffer);
    session->ringbuf = ringbuf;

    if (NULL == server->ssl_ctx) {
        WSS_log_trace("User connected from ip: %s:%d using HTTP request", session->ip, session->port);
    } else {
        WSS_log_trace("User connected from ip: %s:%d using HTTPS request", session->ip, session->port);
    }

    if (NULL != server->ssl_ctx) {
        if (unlikely(! WSS_session_ssl(server, session))) {
            WSS_free((void **)&ringbuf);
            return;
        }

        WSS_ssl_handshake(server, session);
    } else {
        session->state = IDLE;

        clock_gettime(CLOCK_MONOTONIC, &session->alive);

        WSS_poll_set_read(server, session->fd);

        WSS_log_info("Client with session %d connected", session->fd);
    }

    WSS_session_jobs_dec(session);
    pthread_mutex_unlock(&session->lock);

    WSS_log_trace("Finished accepting incoming connections");
}

/**
 * Puts a message in the client sessions writing buffer.
 *
 * @param 	session     [wss_session_t *] 	 "The client session"
 * @param 	pool        [wss_memorypool_t *] "The message memory pool"
 * @param 	message     [wss_message_t *] 	 "The message to send"
 * @return              [wss_error_t]        "The error status"
 */
static wss_error_t write_internal(wss_session_t *session, wss_memorypool_t *pool, wss_message_t *mes) {
    ssize_t off;
    ringbuf_worker_t *w = NULL;

    WSS_log_trace("Putting message into ringbuffer");

    if ( unlikely(-1 == (off = ringbuf_acquire(session->ringbuf, &w, 1))) ) {
        WSS_message_free(mes);
        WSS_memorypool_dealloc(pool, mes);

        WSS_log_error("Failed to acquire space in ringbuffer");

        return WSS_RINGBUFFER_ERROR;
    }
    session->messages[off] = mes;
    ringbuf_produce(session->ringbuf, &w);

    return WSS_SUCCESS;
}

/**
 * Performs the actual IO read operation using either the read or SSL_read
 * system calls.
 *
 * @param 	server      [wss_server_t *] 	"A server instance"
 * @param 	session     [wss_session_t *] 	"The client session"
 * @param 	buffer      [char *] 		    "The buffer to put the data into"
 * @return              [int]               "The amount of bytes read or -1 for error or -2 for wait for IO"
 */
static int read_internal(wss_server_t *server, wss_session_t *session, char *buffer) {
    int n;

    if ( NULL != session->ssl ) {
        n = WSS_ssl_read(server, session, buffer);
    } else {
        do {
            n = read(session->fd, buffer, server->config->size_buffer);
            if ( unlikely(n == -1) ) {
                if ( unlikely(errno == EINTR) ) {
                    errno = 0;
                    continue;
                } else if ( unlikely(errno != EAGAIN && errno != EWOULDBLOCK) ) {
                    WSS_log_error("Read failed: %s", strerror(errno));
                    session->closing = true;
                } else {
                    n = 0;
                }
            }
        } while ( unlikely(0) );
    }

    return n;
}

/**
 * Performs a websocket handshake with the client session
 *
 * @param 	server      [wss_server_t *] 	"A server instance"
 * @param 	session     [wss_session_t *] 	"The client session"
 * @return              [void]
 */
static void handshake(wss_server_t *server, wss_session_t *session) {
    int n, j;
    char *name;
    wss_error_t err;
    size_t name_length = 0;
    wss_header_t *header = &session->header;
    enum HttpStatus_Code code;
    char buffer[server->config->size_buffer];
    char default_name[MAX_SUBPROTOCOL_NAME_LENGTH];
    wss_message_t *message = WSS_memorypool_alloc(server->message_pool);

    WSS_log_info("Performing handshake for session %d", session->fd);

    WSS_log_trace("Reading headers");

    // Continue reading until we get no bytes back
    do {
        n = read_internal(server, session, buffer);

        switch (n) {
            // Wait for IO for either read or write on the filedescriptor
            case -2:
                //session->header = header;
                session->event = WRITE;

                return;
            // An error occured, notify client by writing back to it
            case -1:
                WSS_log_trace("Rejecting HTTP request due to being unable to read from client");

                err = http_response(header, HttpStatus_InternalServerError, "Unable to read from client", message);

                WSS_free_header(header);

                if ( unlikely(err != WSS_SUCCESS) ) {
                    WSS_log_error("Unable to create HTTP response");

                    WSS_message_free(message);
                    WSS_memorypool_dealloc(server->message_pool, message);

                    return;
                }

                if ( likely(WSS_SUCCESS == write_internal(session, server->message_pool, message)) ) {
                    session->state = WRITING;
                    session->event = READ;
                    WSS_write(server, session);
                }

                return;
            case 0:
                break;
            default:
                // Reallocate space for the header and copy buffer into it
                if ( unlikely(NULL == (header->content = WSS_realloc((void **) &header->content, header->length*sizeof(char), (header->length+n+1)*sizeof(char)))) ) {
                    WSS_log_error("Unable to realloc header content");
                    return;
                }
                memcpy(header->content+header->length, buffer, n);
                header->length += n;
                memset(buffer, '\0', server->config->size_buffer);

                // Check if payload from client is too large for the server to handle.
                // If so write error back to the client
                if ( unlikely(header->length > (server->config->size_header+server->config->size_uri+server->config->size_payload)) ) {
                    WSS_log_trace("Rejecting HTTP request as client payload is too large for the server to handle");

                    err = http_response(header, HttpStatus_PayloadTooLarge,
                            "The given payload is too large for the server to handle", message);
                    WSS_free_header(header);

                    if ( unlikely(err != WSS_SUCCESS) ) {
                        WSS_log_error("Unable to create HTTP response");

                        WSS_message_free(message);
                        WSS_memorypool_dealloc(server->message_pool, message);

                        return;
                    }

                    if ( likely(WSS_SUCCESS == write_internal(session, server->message_pool, message)) ) {
                        session->state = WRITING;
                        session->event = READ;
                        WSS_write(server, session);
                    }

                    return;
                }
                break;
        }
    } while ( likely(n != 0) );

    WSS_log_debug("Client header: \n%s", header->content);

    WSS_log_trace("Starting parsing header received from client");

    // Parsing HTTP header
    // If header could not be parsed correctly, notify client with response.
    if ( unlikely(HttpStatus_OK != (code = WSS_parse_header(session->fd, header, server->config))) ) {
        WSS_log_trace("Rejecting HTTP request due to header not being correct");

        err = http_response(header, code, (char *)HttpStatus_reasonPhrase(code), message);
        WSS_free_header(header);

        if ( unlikely(err != WSS_SUCCESS) ) {
            WSS_log_error("Unable to create HTTP response");

            WSS_message_free(message);
            WSS_memorypool_dealloc(server->message_pool, message);

            return;
        }

        if ( likely(WSS_SUCCESS == write_internal(session, server->message_pool, message)) ) {
            session->state = WRITING;
            session->event = READ;
            WSS_write(server, session);
        }

        return;
    }

    // Serve favicon
    if (strlen(header->path) == 12 && strncmp(header->path, "/favicon.ico", 12) == 0) {
        err = favicon_response(header, code, server->config, message);
        if ( err == WSS_SUCCESS ) {
            WSS_free_header(header);

            WSS_log_trace("Serving a favicon to the client");

            // Find and serve favicon.
            if ( likely(WSS_SUCCESS == write_internal(session, server->message_pool, message)) ) {
                session->state = WRITING;
                session->event = READ;
                WSS_write(server, session);
            }
        } else {
            WSS_log_trace("Rejecting HTTP request due to favicon not being available");

            // Else notify client that favicon could not be found
            code = HttpStatus_NotFound;
            err = http_response(header, code, (char *)HttpStatus_reasonPhrase(code), message);
            WSS_free_header(header);

            if ( unlikely(err != WSS_SUCCESS) ) {
                WSS_log_error("Unable to create HTTP response");

                WSS_message_free(message);
                WSS_memorypool_dealloc(server->message_pool, message);

                return;
            }

            if ( likely(NULL != message && WSS_SUCCESS == write_internal(session, server->message_pool, message)) ) {
                session->state = WRITING;
                session->event = READ;
                WSS_write(server, session);
            }
        }

        return;
    }

    WSS_log_trace("Header successfully parsed");

    // Create Upgrade HTTP header based on clients header
    code = WSS_upgrade_header(header, server->config, &server->re);
    switch (code) {
        case HttpStatus_UpgradeRequired:
            WSS_log_trace("Rejecting HTTP request as the service requires use of the Websocket protocol.");
            err = upgrade_response(header, code,
                    "This service requires use of the Websocket protocol.", message);
            break;
        case HttpStatus_NotImplemented:
            WSS_log_trace("Rejecting HTTP request as Websocket protocol is not yet implemented");
            err = http_response(header, code,
                    "Websocket protocol is not yet implemented", message);
            break;
        case HttpStatus_SwitchingProtocols:
            err = handshake_response(header, code, message);
            break;
        case HttpStatus_NotFound:
            WSS_log_trace("Rejecting HTTP request as the page requested was not found.");
            err = http_response(header, code,
                    (char *)HttpStatus_reasonPhrase(code), message);
            break;
        case HttpStatus_Forbidden:
            WSS_log_trace("Rejecting HTTP request as the origin is not allowed to establish a websocket connection.");
            err = http_response(header, code,
                    "The origin is not allowed to establish a websocket connection.", message);
            break;
        default:
            WSS_log_trace("Rejecting HTTP request as server was unable to parse http header as websocket request");
            err = http_response(header, code,
                    (char *)HttpStatus_reasonPhrase(code), message);
            break;
    }

    if ( unlikely(err != WSS_SUCCESS) ) {
        WSS_log_error("Unable to create HTTP response");

        WSS_message_free(message);
        WSS_memorypool_dealloc(server->message_pool, message);

        return;
    }

    // If code status isnt switching protocols, we notify client with a HTTP error
    if ( unlikely(code != HttpStatus_SwitchingProtocols) ) {
        WSS_log_trace("Unable to establish websocket connection");

        if ( likely(WSS_SUCCESS == write_internal(session, server->message_pool, message)) ) {
            session->state = WRITING;
            session->event = READ;
            WSS_write(server, session);
        }

        WSS_free_header(header);

        return;
    }

    // Use default protocol if none was chosen
    if (NULL == header->ws_protocol) {
        name = basename(server->config->subprotocols[server->config->subprotocols_default]);
        for (j = 0; name[j] != '.' && name[j] != '\0'; j++) {
            name_length++;
        }
        memcpy(&default_name[0], name, name_length);
        default_name[name_length] = '\0';

        header->ws_protocol = WSS_find_subprotocol(default_name);
    }

    // Notify websocket protocol of the connection
    header->ws_protocol->connect(session->fd, session->ip, session->port, header->path, header->cookies);

    // Set session as fully handshaked
    session->handshaked = true;

    if ( likely(WSS_SUCCESS == write_internal(session, server->message_pool, message)) ) {
        session->state = WRITING;
        session->event = READ;
        WSS_write(server, session);
    }
}

static void WSS_read_close(wss_server_t *server, wss_session_t *session, char *payload, wss_frame_t **frames, size_t frames_length) {
    uint64_t j;
    WSS_free((void **) &payload);
    for (j = 0; likely(j < frames_length); j++) {
        WSS_free_frame(frames[j]);
        WSS_memorypool_dealloc(server->frame_pool, frames[j]);
    }
    WSS_free((void **) &frames);
    session->closing = true;
}

/**
 * Function that reads information from a session.
 *
 * @param 	server	[wss_server_t *] 	"The server structure"
 * @param 	session	[wss_session_t *] 	"The session structure"
 * @return          [void]
 */
static void WSS_read(wss_server_t *server, wss_session_t *session) {
    int n;
    size_t len;
    uint16_t code;
    wss_frame_t *frame;
    char *msg = NULL;
    bool closing = false;
    size_t offset = 0, prev_offset = 0;
    wss_frame_t **tmp_frames, **frames = NULL;
    char *tmp_payload, *payload = NULL;
    size_t payload_length = 0;
    size_t frames_length = 0;
    char *message;
    size_t message_length;
    wss_message_t *m;
    size_t i, j/*, k*/;
    size_t msg_length = 0;
    size_t msg_offset = 0;
    size_t starting_frame = 0;
    bool fragmented = false;
    char buffer[server->config->size_buffer];

    WSS_log_info("Reading from session %d", session->fd);

    // If no initial header has been seen for the session, the websocket
    // handshake is yet to be made.
    if ( unlikely(! session->handshaked) ){
        WSS_log_trace("Doing websocket handshake");
        handshake(server, session);
        return;
    }

    WSS_log_trace("Starting initial steps to read from client");

    // Use earlier payload
    payload = session->payload;
    payload_length = session->payload_length;
    offset = session->offset;
    frames = session->frames;
    frames_length = session->frames_length;
    session->payload = NULL;
    session->payload_length = 0;
    session->offset = 0;
    session->frames = NULL;
    session->frames_length = 0;

    // If handshake has been made, we can read the websocket frames from
    // the connection
    do {
        n = read_internal(server, session, buffer);

        switch (n) {
            // Wait for IO for either read or write on the filedescriptor
            case -2:
                WSS_log_trace("Detected that server needs further IO to complete the reading");

                session->payload = payload;
                session->payload_length = payload_length;
                session->offset = offset;
                session->frames = frames;
                session->frames_length = frames_length;
                session->event = WRITE;

                return;
            // An error occured
            case -1:
                WSS_free((void **) &payload);

                frame = WSS_memorypool_alloc(server->frame_pool);
                if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frame)) ) {
                    WSS_log_error("Unable to create closing frame");

                    WSS_free_frame(frame);
                    WSS_memorypool_dealloc(server->frame_pool, frame);

                    WSS_read_close(server, session, payload, frames, frames_length);
                    return;
                }

                message_length = WSS_stringify_frame(frame, &message);

                WSS_free_frame(frame);
                WSS_memorypool_dealloc(server->frame_pool, frame);

                m         = WSS_memorypool_alloc(server->message_pool);
                m->msg    = message;
                m->length = message_length;
                m->framed = 1;

                if ( likely(WSS_SUCCESS == write_internal(session, server->message_pool, m)) ) {
                    session->state = WRITING;
                    session->event = READ;
                    WSS_write(server, session);
                }
                return;
            // No new data received
            case 0:
                break;
            // Empty buffer into payload
            default:
                if ( unlikely(NULL == (tmp_payload = WSS_realloc((void **) &payload, payload_length*sizeof(char), (payload_length+n+1)*sizeof(char)))) ) {
                    WSS_log_error("Unable to reallocate the payload");

                    WSS_read_close(server, session, payload, frames, frames_length);
                    return;
                }

                payload = tmp_payload;
                memcpy(payload+payload_length, buffer, n);
                payload_length += n;
                memset(buffer, '\0', server->config->size_buffer);
                break;
        }
    } while ( likely(n != 0) );

    WSS_log_trace("Payload from client was read. Continues flow by parsing frames.");

    // Parse the payload into websocket frames
    do {
        prev_offset = offset;

        frame = WSS_memorypool_alloc(server->frame_pool);
        if ( unlikely(WSS_SUCCESS != WSS_parse_frame(payload, payload_length, &offset, frame)) ) {
            WSS_log_trace("Unable to parse frame");

            WSS_free_frame(frame);
            WSS_memorypool_dealloc(server->frame_pool, frame);

            WSS_read_close(server, session, payload, frames, frames_length);
            return;
        }

        // Check if we were forced to read beyond the payload to create a full frame
        if ( unlikely(offset > payload_length) ) {
            WSS_log_trace("Detected that data was missing in order to complete frame, will wait for more");

            WSS_free_frame(frame);
            WSS_memorypool_dealloc(server->frame_pool, frame);

            session->payload = payload;
            session->payload_length = payload_length;
            session->offset = prev_offset;
            session->frames = frames;
            session->frames_length = frames_length;
            session->event = READ;

            return;
        }

        // If no extension is negotiated, the rsv bits must not be used
        if ( unlikely(NULL == session->header.ws_extensions && (frame->rsv1 || frame->rsv2 || frame->rsv3)) ) {
            WSS_log_trace("Protocol Error: rsv bits must not be set without using extensions");
            if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frame)) ) {
                WSS_log_error("Unable to create closing frame");

                WSS_free_frame(frame);
                WSS_memorypool_dealloc(server->frame_pool, frame);

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }
        } else

        // If opcode is unknown
        if ( unlikely((frame->opcode >= 0x3 && frame->opcode <= 0x7) ||
                (frame->opcode >= 0xB && frame->opcode <= 0xF)) ) {
            WSS_log_trace("Type Error: Unknown upcode");
            if (WSS_SUCCESS != WSS_closing_frame(CLOSE_TYPE, NULL, frame)) {
                WSS_log_error("Unable to create closing frame");

                WSS_free_frame(frame);
                WSS_memorypool_dealloc(server->frame_pool, frame);

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }
        } else

        // Server expects all received data to be masked
        if ( unlikely(! frame->mask) ) {
            WSS_log_trace("Protocol Error: Client message should always be masked");
            if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frame)) ) {
                WSS_log_error("Unable to create closing frame");

                WSS_free_frame(frame);
                WSS_memorypool_dealloc(server->frame_pool, frame);

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }
        } else

        // Control frames cannot be fragmented
        if ( unlikely(! frame->fin && frame->opcode >= 0x8 && frame->opcode <= 0xA) ) {
            WSS_log_trace("Protocol Error: Control frames cannot be fragmented");
            if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frame)) ) {
                WSS_log_error("Unable to create closing frame");

                WSS_free_frame(frame);
                WSS_memorypool_dealloc(server->frame_pool, frame);

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }
        } else

        // Check that frame is not too large
        if ( unlikely(frame->payloadLength > server->config->size_frame) ) {
            WSS_log_trace("Protocol Error: Control frames cannot be fragmented");
            if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_BIG, NULL, frame)) ) {
                WSS_log_error("Unable to create closing frame");

                WSS_free_frame(frame);
                WSS_memorypool_dealloc(server->frame_pool, frame);

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }
        } else 

        // Control frames cannot have a payload length larger than 125 bytes
        if ( unlikely(frame->opcode >= 0x8 && frame->opcode <= 0xA && frame->payloadLength > 125) ) {
            WSS_log_trace("Protocol Error: Control frames cannot have payload larger than 125 bytes");
            if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frame)) ) {
                WSS_log_error("Unable to create closing frame");

                WSS_free_frame(frame);
                WSS_memorypool_dealloc(server->frame_pool, frame);

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }
        } else

        // In HYBI10 specification the most significant bit must not be set
        if ( unlikely((session->header.ws_type == HYBI10 || session->header.ws_type == HYBI07) && frame->payloadLength & ((uint64_t)1 << (sizeof(uint64_t)*8-1))) ) {
            WSS_log_trace("Protocol Error: Frame payload length must not use MSB");
            if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frame)) ) {
                WSS_log_error("Unable to create closing frame");

                WSS_free_frame(frame);
                WSS_memorypool_dealloc(server->frame_pool, frame);

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }
        } else

        // Close frame
        if ( unlikely(frame->opcode == CLOSE_FRAME) ) {
            // A code of 2 byte must be present if there is any application data for closing frame
            if ( unlikely(frame->applicationDataLength > 0 && frame->applicationDataLength < 2) ) {
                WSS_log_trace("Protocol Error: Closing frame with payload too small bytewise error code");
                if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frame)) ) {
                    WSS_log_error("Unable to create closing frame");

                    WSS_free_frame(frame);
                    WSS_memorypool_dealloc(server->frame_pool, frame);

                    WSS_read_close(server, session, payload, frames, frames_length);
                    return;
                }
            } else 

                
            // The payload after the code, must be valid UTF8
            if ( unlikely(frame->applicationDataLength >= 2 && ! utf8_check(frame->payload+2, frame->applicationDataLength-2)) ) {
                WSS_log_trace("Protocol Error: Payload of error frame must be valid UTF8.");
                if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_UTF8, NULL, frame)) ) {
                    WSS_log_error("Unable to create closing frame");

                    WSS_free_frame(frame);
                    WSS_memorypool_dealloc(server->frame_pool, frame);

                    WSS_read_close(server, session, payload, frames, frames_length);
                    return;
                }
            } else 
            
            // Check status code is within valid range
            if (frame->applicationDataLength >= 2) {
                // Copy code
                memcpy(&code, frame->payload, sizeof(uint16_t));
                code = ntohs(code);

                WSS_log_debug("Closing frame code: %d", code);

                // Current rfc6455 codes
                if ( unlikely(code < 1000 || (code >= 1004 && code <= 1006) || (code >= 1015 && code < 3000) || code >= 5000) ) {
                    WSS_log_trace("Protocol Error: Closing frame has invalid error code");
                    if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frame)) ) {
                        WSS_log_error("Unable to create closing frame");

                        WSS_free_frame(frame);
                        WSS_memorypool_dealloc(server->frame_pool, frame);

                        WSS_read_close(server, session, payload, frames, frames_length);
                        return;
                    }
                }
            }
        } else

        // Pong
        if ( unlikely(frame->opcode == PONG_FRAME) ) {
            WSS_log_trace("Pong received");

            // We do not care about the contents of pong frames since they
            // indicate that the connection is still alive. In case we should
            // we would have to store a list of ping payloads in order to match
            // them with the pong payloads, since pongs are not necessarily 
            // ordered.

            WSS_free_frame(frame);
            WSS_memorypool_dealloc(server->frame_pool, frame);

            continue;
        } else

        // Ping
        if ( unlikely(frame->opcode == PING_FRAME) ) {
            WSS_log_trace("Ping received");

            WSS_pong_frame(frame);
        }

        if ( unlikely(NULL == (tmp_frames = WSS_realloc((void **) &frames, frames_length*sizeof(wss_frame_t *),
                        (frames_length+1)*sizeof(wss_frame_t *)))) ) {
            WSS_log_error("Unable to reallocate frames");
            
            WSS_free_frame(frame);
            WSS_memorypool_dealloc(server->frame_pool, frame);

            WSS_read_close(server, session, payload, frames, frames_length);
            return;
        }
        frames = tmp_frames;
        frames[frames_length] = frame;
        frames_length += 1;

        // Check if frame count is exceeded
        if ( unlikely(frames_length > server->config->max_frames) ) {
            //WSS_free_frame(frame);
            //frame = WSS_closing_frame(CLOSE_BIG, NULL);
            if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_BIG, NULL, frame)) ) {
                WSS_log_error("Unable to create closing frame");

                WSS_free_frame(frame);
                WSS_memorypool_dealloc(server->frame_pool, frame);

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }

            frames[frames_length-1] = frame;
        }

        // Close
        if ( unlikely(frame->opcode == CLOSE_FRAME) ) {
            closing = true;
            WSS_log_trace("Stopping frame validation as closing frame was parsed");
            break;
        }
    } while ( likely(offset < payload_length) );

    WSS_free((void **) &payload);

    WSS_log_trace("A total of %lu frames was parsed.", frames_length);

    WSS_log_trace("Starting frame validation");

    // Validating frames.
    for (i = 0; likely(i < frames_length); i++) {
        // If we are not processing a fragmented set of frames, expect the
        // opcode different from the continuation frame.
        if ( unlikely(! fragmented && (frames[i]->opcode == CONTINUATION_FRAME)) ) {
            WSS_log_trace("Protocol Error: continuation opcode used in non-fragmented message");
            for (j = i+1; likely(j < frames_length); j++) {
                WSS_free_frame(frames[j]);
                WSS_memorypool_dealloc(server->frame_pool, frames[j]);
            }

            if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frames[i])) ) {
                WSS_log_error("Unable to send closing frame");

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }

            frames_length = i+1;
            closing = true;
            break;
        }

        // If we receive control frame within fragmented frames.
        if ( unlikely(fragmented && (frames[i]->opcode >= 0x8 && frames[i]->opcode <= 0xA)) ) {
            WSS_log_trace("Received control frame within fragmented message");

            frame = frames[i];
            // If we received a closing frame substitute the fragment with the
            // closing frame and end validation
            if (frames[i]->opcode == CLOSE_FRAME) {
                WSS_log_trace("Stopping further validation of fragmented message as a closing frame was detected");

                for (j = starting_frame; likely(j < frames_length); j++) {
                    if ( likely(j != i) ) {
                        WSS_free_frame(frames[j]);
                        WSS_memorypool_dealloc(server->frame_pool, frames[j]);
                    } else {
                        frames[j] = NULL;
                    }
                }
                frames[starting_frame] = frame;
                frames_length = starting_frame+1;
                fragmented = false;
                closing = true;
                break;
            // Else rearrange the frames, such that control frame is first
            } else {
                WSS_log_trace("Rearranging frames such that control frame will be written before fragmented frames");
                for (j = i; likely(j > starting_frame); j--) {
                    frames[j] = frames[j-1];
                }
                frames[starting_frame] = frame;
                starting_frame++;
            }
        } else

        // If we are processing a fragmented set of frames, expect the opcode
        // to be a contination frame.
        if ( unlikely(fragmented && frames[i]->opcode != CONTINUATION_FRAME) ) {
            WSS_log_trace("Protocol Error: during fragmented message received other opcode than continuation");
            for (j = starting_frame+1; likely(j < frames_length); j++) {
                WSS_free_frame(frames[j]);
                WSS_memorypool_dealloc(server->frame_pool, frames[j]);
            }
            
            if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_PROTOCOL, NULL, frames[starting_frame])) ) {
                WSS_log_error("Unable to send closing frame");

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }

            frames_length = starting_frame+1;
            fragmented = false;
            closing = true;
            break;
        }

        // If message consists of single frame or we have the starting frame of
        // a multiframe message, store the starting index
        if ( frames[i]->fin && frames[i]->opcode != CONTINUATION_FRAME ) {
            starting_frame = i;
        } else if ( ! frames[i]->fin && frames[i]->opcode != CONTINUATION_FRAME ) {
            starting_frame = i;
            fragmented = true;
        }

        if (frames[i]->fin) {
            fragmented = false;
        }
    }

    // If fragmented is still true, we did not receive the whole message, and
    // we hence want to wait until we get the rest.
    if ( unlikely(fragmented) ) {
        WSS_log_trace("Detected missing frames in fragmented message, will wait for further IO");

        session->frames = frames;
        session->frames_length = frames_length;
        session->event = READ;

        return;
    }

    WSS_log_trace("Frames was validated");
    
    session->state = IDLE;

    WSS_log_trace("Sending frames to receivers");

    // Sending message to subprotocol and sending it to the filedescriptors
    // returned by the subprotocol.
    for (i = 0; likely(i < frames_length); i++) {
        // If message consists of single frame or we have the starting frame of
        // a multiframe message, store the starting index
        if ( (frames[i]->fin && !(frames[i]->opcode == 0x0)) ||
             (! frames[i]->fin && frames[i]->opcode != 0x0) ) {
            starting_frame = i;
        }

        if (frames[i]->fin) {
            len = i-starting_frame+1;

            WSS_log_trace("Applying %d extensions on input", session->header.ws_extensions_count);

            // Apply extensions to collection of frames (message)
            for (j = 0; likely(j < session->header.ws_extensions_count); j++) {
                // Apply extension for set of frames
                session->header.ws_extensions[j]->ext->inframes(
                        session->fd,
                        frames+starting_frame,
                        len);
            }

            WSS_log_trace("Assembling message");

            for (j = starting_frame; likely(j <= i); j++) {
                msg_length += frames[j]->applicationDataLength + frames[j]->extensionDataLength;
            }
            
            if ( unlikely(NULL == (msg = WSS_malloc((msg_length+1)*sizeof(char)))) ) {
                WSS_log_error("Unable to allocate message");

                WSS_read_close(server, session, payload, frames, frames_length);
                return;
            }

            for (j = starting_frame; likely(j <= i); j++) {
                memcpy(msg+msg_offset, frames[j]->payload+frames[j]->extensionDataLength, frames[j]->applicationDataLength);
                msg_offset += frames[j]->applicationDataLength;
            }

            WSS_log_debug("Unmasked message (%d bytes): %s\n", msg_length, msg);

            // Check utf8 for text frames
            if ( unlikely(frames[starting_frame]->opcode == TEXT_FRAME && ! utf8_check(msg, msg_length)) ) {
                WSS_log_trace("UTF8 Error: the text was not UTF8 encoded correctly");

                WSS_free((void **) &msg);
                msg_length = 0;

                for (j = starting_frame + 1; likely(j < frames_length); j++) {
                    WSS_free_frame(frames[j]);
                    WSS_memorypool_dealloc(server->frame_pool, frames[j]);
                }

                if ( unlikely(WSS_SUCCESS != WSS_closing_frame(CLOSE_UTF8, NULL, frames[starting_frame])) ) {
                    WSS_log_error("Unable to send closing frame");

                    WSS_read_close(server, session, payload, frames, frames_length);
                    return;
                }

                frames_length = starting_frame+1;
                i = starting_frame;
                len = 1;
                closing = true;
            }

            if ( likely(frames[starting_frame]->opcode == TEXT_FRAME || frames[starting_frame]->opcode == BINARY_FRAME) ) {
                WSS_log_trace("Notifying subprotocol of message");

                // Use subprotocol
                session->header.ws_protocol->message(session->fd, frames[starting_frame]->opcode, msg, msg_length);
            } else {
                WSS_log_trace("Writing control frame message");

                WSS_session_jobs_inc(session);
                WSS_message_send_frames((void *)server, (void *)session, &frames[starting_frame], len);
            }

            WSS_free((void **) &msg);
            msg_length = 0;
            msg_offset = 0;
        }
    }

    if ( likely(frames_length > 0) ) {
        for (i = 0; likely(i < frames_length); i++) {
            WSS_free_frame(frames[i]);
            WSS_memorypool_dealloc(server->frame_pool, frames[i]);
        }
        WSS_free((void **) &frames);
    }

    if (! closing && session->event == NONE) {
        WSS_log_trace("Set epoll file descriptor to read mode after finishing read");
        session->event = READ;
    }
}

/**
 * Function that writes information to a session and decides wether event poll
 * should be rearmed and whether a session lock should be performed.
 *
 * @param 	server	[wss_server_t *] 	"The server structure"
 * @param 	session	[wss_session_t *] 	"The session structure"
 * @return          [void]
 */
void WSS_write(wss_server_t *server, wss_session_t *session) {
    int n;
    wss_message_t *message;
    unsigned int i;
    unsigned int message_length;
    unsigned int bytes_sent;
    size_t len, off;
    bool closing = false;

    WSS_log_info("Writing to session %d", session->fd);

    WSS_log_trace("Performing write by popping messages from ringbuffer");

    while ( likely(0 != (len = ringbuf_consume(session->ringbuf, &off))) ) {
        for (i = 0; likely(i < len); i++) {
            if ( unlikely(session->handshaked && closing) ) {
                WSS_log_trace("No further messages are necessary as client connection is closing");
                break;
            }

            bytes_sent = session->written;
            session->written = 0;
            message = session->messages[off+i];
            message_length = message->length;

            while ( likely(bytes_sent < message_length) ) {
                // Check if message contains closing byte
                if ( unlikely(message->framed && bytes_sent == 0 &&
                     ((message->msg[0] & 0xF) & 0x8) == (message->msg[0] & 0xF)) ) {
                    closing = true;
                }

                if (NULL != session->ssl) {
                    if (! WSS_ssl_write_partial(session, i, message, &bytes_sent)) {
                        return;
                    }
                } else {
                    do {
                        n = write(session->fd, message->msg+bytes_sent, message_length-bytes_sent);
                        if (unlikely(n == -1)) {
                            if ( unlikely(errno == EINTR) ) {
                                errno = 0;
                                continue;
                            } else if ( unlikely(errno != EAGAIN && errno != EWOULDBLOCK) ) {
                                WSS_log_error("Write failed: %s", strerror(errno));
                                session->closing = true;
                                return;
                            }

                            session->written = bytes_sent;
                            ringbuf_release(session->ringbuf, i);

                            session->event = WRITE;

                            return;
                        } else {
                            bytes_sent += n;
                        }
                    } while ( unlikely(0) );
                }
            }

            if ( likely(session->messages != NULL) ) {
                if ( likely(session->messages[off+i] != NULL) ) {
                    WSS_message_free(session->messages[off+i]);
                    WSS_memorypool_dealloc(server->message_pool, session->messages[off+i]);
                    session->messages[off+i] = NULL;
                }
            }
        }

        ringbuf_release(session->ringbuf, len);
    }

    WSS_log_trace("Done writing to filedescriptors");

    session->state = IDLE;

    if (closing) {
        WSS_log_trace("Closing connection, since closing frame has been sent");

        session->closing = true;

        return;
    }
}

/**
 * Function that performs and distributes the IO work.
 *
 * @param 	args	[void *] 	"Is a args_t structure holding server_t, filedescriptor, and the state"
 * @return          [void]
 */
void WSS_work(void *args) {
    wss_session_t *session;
    long unsigned int ms;
    struct timespec now;
    wss_thread_args_t *arguments = (wss_thread_args_t *) args;
    wss_server_t *server = (wss_server_t *) arguments->server;
    wss_session_state_t session_state = arguments->state;
    int fd = arguments->fd;

    // Free arguments structure as this won't be needed no more
    WSS_memorypool_dealloc(server->thread_args_pool, arguments);

    if ( unlikely(NULL == (session = WSS_session_find(fd))) ) {
        WSS_log_trace("Unable to find client with session %d", fd);
        return;
    }

    WSS_log_trace("Incrementing session jobs");

    WSS_session_jobs_inc(session);
    pthread_mutex_lock(&session->lock);

    session->event = NONE;

    WSS_log_trace("Checking session state");

    // We first check if session was in the middle of performing an IO task but
    // needed to wait for further IO
    switch (session->state) {
        case WRITING:
            clock_gettime(CLOCK_MONOTONIC, &now);
            ms = (((now.tv_sec - session->alive.tv_sec)*1000)+(now.tv_nsec/1000000)) - (session->alive.tv_nsec/1000000);
            if ( unlikely(server->config->timeout_write >= 0 && ms >= (long unsigned int)server->config->timeout_write) ) {
                WSS_log_trace("Write timeout detected for session %d", fd);
                session->closing = true;
                break;
            }

            session->event = READ;
            WSS_write(server, session);
            break;
        case CLOSING:
            session->closing = true;
            break;
        case CONNECTING:
            if (NULL != server->ssl_ctx) {
                WSS_ssl_handshake(server, session);
                WSS_session_jobs_dec(session);
                pthread_mutex_unlock(&session->lock);
                return;
            }

            WSS_read(server, session);
            break;
        case READING:
            clock_gettime(CLOCK_MONOTONIC, &now);
            ms = (((now.tv_sec - session->alive.tv_sec)*1000)+(now.tv_nsec/1000000)) - (session->alive.tv_nsec/1000000);
            if ( unlikely(server->config->timeout_read >= 0 && ms >= (long unsigned int)server->config->timeout_read) ) {
                WSS_log_trace("Read timeout detected for session %d", fd);
                session->closing = true;
                break;
            }

            WSS_read(server, session);
            break;
        case IDLE:
            session->state = session_state;

            switch (session_state) {
                case WRITING:
                    WSS_log_trace("Handling write event");
                    session->event = READ;
                    WSS_write(server, session);
                    break;
                case CLOSING:
                    session->closing = true;
                    WSS_log_trace("Handling close event");
                    break;
                case READING:
                    WSS_log_trace("Handling read event");
                    WSS_read(server, session);
                    break;
                default:
                    WSS_log_error("State that should not be possible at this point was encountered %s", state);
                    break;
            }
    }

    WSS_session_jobs_dec(session);
    pthread_mutex_unlock(&session->lock);

    if ( unlikely(session->closing) ) {
        session->event = NONE;
        WSS_disconnect(server, session);
    }

    switch (session->event) {
        case WRITE: 
            clock_gettime(CLOCK_MONOTONIC, &session->alive);
            WSS_poll_set_write(server, session->fd);
            break;
        case READ: 
            clock_gettime(CLOCK_MONOTONIC, &session->alive);
            WSS_poll_set_read(server, session->fd);
            break;
        case NONE: 
            break;
    }
}
