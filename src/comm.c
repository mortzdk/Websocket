#include <stddef.h> 		    /* size_t */
#include <errno.h> 				/* errno */
#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen */
#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <unistd.h>             /* close, read, write */
#include <math.h> 				/* log10 */
#include <time.h>
#include <locale.h>

#include <sys/types.h>          /* socket, setsockopt, accept, send, recv */
#include <sys/stat.h> 			/* stat */
#include <sys/epoll.h> 			/* epoll_event, epoll_create, epoll_ctl */
#include <sys/socket.h>         /* socket, setsockopt, inet_ntoa, accept */
#include <sys/select.h>         /* socket, setsockopt, inet_ntoa, accept */
#include <netinet/in.h>         /* sockaddr_in, inet_ntoa */
#include <arpa/inet.h>          /* htonl, htons, inet_ntoa */

#include <pthread.h> 			/* pthread_create, pthread_t, pthread_attr_t
                                   pthread_mutex_init */
#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/buffer.h>
#else
#define SHA_DIGEST_LENGTH 20
#include "sha1.h"
#include "base64.h"
#endif

#include "comm.h"
#include "server.h"
#include "log.h"
#include "session.h"
#include "socket.h"
#include "alloc.h"
#include "error.h"
#include "header.h"
#include "frame.h"
#include "config.h"
#include "httpstatuscodes.h"
#include "str.h"
#include "utf8.h"
#include "error.h"
#include "predict.h"

/**
 * Function that generates a handshake response, used to authorize a websocket
 * session.
 *
 * @param   header  [header_t*]             "The http header obtained from the session"
 * @param   code    [enum HttpStatus_Code]  "The http status code to return"
 * @return          [message_t *]           "A message structure that can be passed through ringbuffer"
 */
static message_t * WSS_handshake_response(header_t *header, enum HttpStatus_Code code) {
    message_t *msg;
    int length;
    size_t j;
    size_t offset = 0;
    char *message;
    char *version = "HTTP/1.1";
    const char *reason = HttpStatus_reasonPhrase(code);
    int line = 0, headers = 0;
    char sha1Key[SHA_DIGEST_LENGTH];
    int magic_length = strlen(MAGIC_WEBSOCKET_KEY);
    int key_length = strlen(header->ws_key) + magic_length;
    char key[key_length];
    char *acceptKey;
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
    memset(sha1Key, '\0', 20);
    memcpy(key+(key_length-magic_length), MAGIC_WEBSOCKET_KEY, magic_length);
    memcpy(key, header->ws_key, (key_length-magic_length));
#ifdef USE_OPENSSL
    SHA1((const unsigned char *)key, key_length, (unsigned char*) sha1Key);

    BIO *b64_bio, *mem_bio;                         // Declares two OpenSSL BIOs: a base64 filter and a memory BIO.
    BUF_MEM *mem_bio_mem_ptr;                       // Pointer to a "memory BIO" structure holding our base64 data.
    b64_bio = BIO_new(BIO_f_base64());              // Initialize our base64 filter BIO.
    mem_bio = BIO_new(BIO_s_mem());                 // Initialize our memory sink BIO.
    b64_bio = BIO_push(b64_bio, mem_bio);           // Link the BIOs by creating a filter-sink BIO chain.
    BIO_set_flags(b64_bio, BIO_FLAGS_BASE64_NO_NL); // No newlines every 64 characters or less.
    BIO_set_close(mem_bio, BIO_CLOSE);              // Permit access to mem_ptr after BIOs are destroyed.
    BIO_write(b64_bio, sha1Key, SHA_DIGEST_LENGTH); // Records base64 encoded data.
    BIO_flush(b64_bio);                             // Flush data.  Necessary for b64 encoding, because of pad characters.
    BIO_get_mem_ptr(mem_bio, &mem_bio_mem_ptr);     // Store address of mem_bio's memory structure.

    acceptKeyLength = mem_bio_mem_ptr->length;
    if ( unlikely(NULL == (acceptKey = WSS_malloc(acceptKeyLength))) ) {
        return NULL;
    }
    memcpy(acceptKey, (*mem_bio_mem_ptr).data, acceptKeyLength);

    BIO_free_all(b64_bio);                          // Destroys all BIOs in chain, starting with b64 (i.e. the 1st one).
#else
    SHA1Context sha;
    int i, b;
    memset(sha1Key, '\0', SHA_DIGEST_LENGTH);

    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char*) key, key_length);
    if ( likely(SHA1Result(&sha)) ) {
        for (i = 0; likely(i < 5); i++) {
            b = htonl(sha.Message_Digest[i]);
            memcpy(sha1Key+(4*i), (unsigned char *) &b, 4);
        }
    } else {
        return NULL;
    }

    acceptKeyLength = base64_encode_alloc((const char *) sha1Key, SHA_DIGEST_LENGTH, &acceptKey);
#endif

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
        return NULL;
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
    WSS_free((void **) &acceptKey);

    if ( unlikely(NULL == (msg = (message_t *) WSS_malloc(sizeof(message_t)))) ) {
        WSS_free((void **) &message);
        return NULL;
    }

    WSS_log_debug("Handshake Response: \n%s", message);

    msg->msg = message;
    msg->length = length;

    return msg;
}

/**
 * Function that generates a favicon response, used to present a favicon on the
 * HTTP landing page of the WSS server.
 *
 * @param   header  [header_t *]            "The http header obtained from the session"
 * @param   code    [enum HttpStatus_Code]  "The http status code to return"
 * @param   config  [config_t *]            "The configuration of the server"
 * @return          [message_t *]           "A message structure that can be passed through ringbuffer"
 */
static message_t * WSS_favicon_response(header_t *header, enum HttpStatus_Code code, config_t *config) {
    time_t now;
    struct tm tm;
    char *ico;
    char *etag;
    char *message;
    message_t *msg;
    size_t length;
    struct stat filestat;
    char sha1Key[SHA_DIGEST_LENGTH];
    char date[GMT_FORMAT_LENGTH];
    char modified[GMT_FORMAT_LENGTH];
    char savedlocale[256];
    char *version = "HTTP/1.1";
    const char *reason  = HttpStatus_reasonPhrase(code);
    int icon = 0, line = 0, headers = 0;

    // Get GMT current time
    strcpy(savedlocale, setlocale(LC_ALL, NULL));
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
    if (NULL != config->favicon && access(config->favicon, F_OK) != -1) {
        // Get last modified
        stat(config->favicon, &filestat);
        setlocale(LC_ALL, "C");
        tm = *gmtime(&filestat.st_mtime);
        strftime(modified, sizeof modified, "%a, %d %b %Y %H:%M:%S %Z", &tm);
        setlocale(LC_ALL, savedlocale);

        icon = strload(config->favicon, &ico);
    } else {
        memcpy(modified, date, strlen(date));
    }

    // Generate etag from favicon content
#ifdef USE_OPENSSL
    SHA1((const unsigned char *)ico, icon, (unsigned char*) sha1Key);
#else
    SHA1Context sha;
    int i, b;
    memset(sha1Key, '\0', SHA_DIGEST_LENGTH);

    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char*) ico, icon);
    if ( likely(SHA1Result(&sha)) ) {
        for (i = 0; likely(i < 5); i++) {
            b = htonl(sha.Message_Digest[i]);
            memcpy(sha1Key+(4*i), (unsigned char *) &b, 4);
        }
    }
#endif

    // Convert etag binary values to hex values
    if ( unlikely(NULL == (etag = bin2hex((const unsigned char *)sha1Key, SHA_DIGEST_LENGTH))) ) {
        WSS_free((void **) &ico);
        return NULL;
    }

    headers += strlen(HTTP_ICO_HEADERS)*sizeof(char)-10*sizeof(char);
    headers += (log10(icon)+1);
    headers += strlen(date)*sizeof(char);
    headers += strlen(WSS_SERVER_VERSION)*sizeof(char);
    headers += strlen(etag)*sizeof(char);
    headers += strlen(modified)*sizeof(char);

    length = line + headers + icon + 1;
    if ( unlikely(NULL == (message = (char *) WSS_malloc(length*sizeof(char)))) ) {
        WSS_free((void **) &etag);
        WSS_free((void **) &ico);
        return NULL;
    }

    sprintf(message, HTTP_STATUS_LINE, version, code, reason);
    sprintf(message+line, HTTP_ICO_HEADERS, icon, date, WSS_SERVER_VERSION, etag, modified);
    memcpy(message+(line+headers), ico, icon);

    msg = (message_t *) WSS_malloc(sizeof(message_t));
    msg->msg = message;
    msg->length = length;

    WSS_free((void **) &etag);
    WSS_free((void **) &ico);

    return msg;
}

/**
 * Function that generates an HTTP upgrade response, used to tell the connecting
 * client that an upgrade is needed in order to use the server.
 *
 * @param   header  [header_t *]            "The http header obtained from the session"
 * @param   code    [enum HttpStatus_Code]  "The http status code to return"
 * @param   exp     [char *]                "An explanation of what caused this response"
 * @return          [message_t *]           "A message structure that can be passed through ringbuffer"
 */
static message_t * WSS_upgrade_response(header_t *header, enum HttpStatus_Code code, char *exp) {
    message_t *msg;
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
        return NULL;
    }

    sprintf(message, HTTP_STATUS_LINE, version, code, reason);
    sprintf(message+line, HTTP_UPGRADE_HEADERS, body, WSS_SERVER_VERSION);
    sprintf(message+line+headers, "%s", exp);

    if ( unlikely(NULL == (msg = (message_t *) WSS_malloc(sizeof(message_t)))) ) {
        WSS_free((void **) &message);
        return NULL;
    }
    msg->msg = message;
    msg->length = length;

    return msg;
}

/**
 * Function that generates a HTTP error response, used to tell the connecting
 * client that an error occured.
 *
 * @param   header  [header_t *]            "The http header obtained from the session"
 * @param   code    [enum HttpStatus_Code]  "The http status code to return"
 * @param   exp     [char *]                "An explanation of what caused this response"
 * @return          [message_t *]           "A message structure that can be passed through ringbuffer"
 */
static message_t * WSS_http_response(header_t *header, enum HttpStatus_Code code, char *exp) {
    time_t now;
    struct tm tm;
    message_t *msg;
    int length;
    char *message;
    char date[GMT_FORMAT_LENGTH];
    char savedlocale[256];
    char *version = "HTTP/1.1";
    const char *reason  = HttpStatus_reasonPhrase(code);
    int body = 0, line = 0, headers = 0;

    // Get GMT current time
    strcpy(savedlocale, setlocale(LC_ALL, NULL));
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

    headers += strlen(HTTP_HTML_HEADERS)*sizeof(char)-6*sizeof(char);
    headers += strlen(WSS_SERVER_VERSION)*sizeof(char);
    headers += strlen(date)*sizeof(char);
    headers += (log10(body)+1)*sizeof(char);

    length = line + headers + body + 1;
    if ( unlikely(NULL == (message = (char *) WSS_malloc(length*sizeof(char)))) ) {
        return NULL;
    }

    sprintf(message, HTTP_STATUS_LINE, version, code, reason);
    sprintf(message+line, HTTP_HTML_HEADERS, body, date, WSS_SERVER_VERSION);
    sprintf(message+line+headers, HTTP_BODY, code, reason, reason, exp);

    if ( unlikely(NULL == (msg = (message_t *) WSS_malloc(sizeof(message_t)))) ) {
        WSS_free((void **) &message);
        return NULL;
    }
    msg->msg = message;
    msg->length = length;

    return msg;
}

inline static void WSS_disconnect_internal(server_t *server, session_t *session) {
    int fd = session->fd;
    struct epoll_event event;
    memset(&event, 0, sizeof(event));

    session->state = CLOSING;

#ifdef USE_OPENSSL
    if (NULL == server->ssl_ctx) {
#endif
        WSS_log_trace("User disconnected from ip: %s:%d using HTTP request", session->ip, session->port);
#ifdef USE_OPENSSL
    } else {
        WSS_log_trace("User disconnected from ip: %s:%d using HTTPS request", session->ip, session->port);
    }
#endif


    WSS_log_trace("Informing subprotocol about session close");

    session->header->ws_protocol->close(session->fd);

    WSS_log_trace("Removing clients filedescriptor (%d) from the epoll queue", session->fd);
    if ( unlikely((epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, session->fd, &event)) < 0) ) {
        WSS_log_error("Unable to remove epoll from queue: %s", strerror(errno));
        return;
    }

    WSS_log_trace("Deleting client session");

    if ( unlikely(SUCCESS != WSS_session_delete(session)) ) {
        WSS_log_error("Unable to delete client session");
        return;
    }

    WSS_log_info("Client with session %d disconnected", fd);
}


#ifdef USE_OPENSSL
/**
 * Function that performs a ssl handshake with the connecting client.
 *
 * @param   server      [server_t *]    "The server implementation"
 * @param   session     [session_t *]   "The connecting client session"
 * @return              [void]
 */
static void WSS_ssl_handshake(server_t *server, session_t *session) {
    int ret, n;
    unsigned long err;
    struct epoll_event event;

    memset(&event, 0, sizeof(event));

    WSS_log_trace("Performing SSL handshake");

    if (NULL != server->ssl_ctx) {
        ret = SSL_do_handshake(session->ssl);
        err = SSL_get_error(session->ssl, ret);

        // If something more needs to be read in order for the handshake to finish
        if (err == SSL_ERROR_WANT_READ) {
            WSS_log_trace("Need to wait for further reads");

            event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
            event.data.fd = session->fd;

            if ( unlikely((n = epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, session->fd, &event)) < 0) ) {
                // File descriptor could already have been added to epoll due to SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
                if ( likely(errno == EEXIST) ) {
                    n = epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, session->fd, &event);
                }

                if ( unlikely(n < 0) ) {
                    WSS_log_error("Unable to rearm epoll: %s", strerror(errno));
                    WSS_disconnect_internal(server, session);
                    return;
                }
            }

            return;
        }

        // If something more needs to be written in order for the handshake to finish
        if (err == SSL_ERROR_WANT_WRITE) {
            WSS_log_trace("Need to wait for further writes");

            event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
            event.data.fd = session->fd;

            if ( unlikely((n = epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, session->fd, &event)) < 0) ) {
                // File descriptor could already have been added to epoll due to SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
                if ( likely(errno == EEXIST) ) {
                    n = epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, session->fd, &event);
                }

                if ( unlikely(n < 0) ) {
                    WSS_log_error("Unable to rearm epoll: %s", strerror(errno));
                    WSS_disconnect_internal(server, session);
                    return;
                }
            }

            return;
        }

        if ( unlikely(err != SSL_ERROR_NONE && err != SSL_ERROR_ZERO_RETURN) ) {
            char message[1024];
            ERR_error_string_n(err, message, 1024);
            WSS_log_error("Handshake error: %s", message);

            while ( (err = ERR_get_error()) != 0 ) {
                memset(message, '\0', 1024);
                ERR_error_string_n(err, message, 1024);
                WSS_log_error("Handshake error: %s", message);
            }

            WSS_disconnect_internal(server, session);
            return;
        }

        WSS_log_trace("SSL handshake was successfull");

        session->ssl_connected = true;

        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
        event.data.fd = session->fd;

        if ( unlikely((n = epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, session->fd, &event)) < 0) ) {
            // File descriptor could already have been added to epoll due to SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
            if ( likely(errno == EEXIST) ) {
                n = epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, session->fd, &event);
            }

            if ( unlikely(n < 0) ) {
                WSS_log_error("Unable to rearm epoll: %s", strerror(errno));
                WSS_disconnect_internal(server, session);
                return;
            }
        }

        WSS_log_trace("Added client to pool of filedescriptors");
    }

    session->state = STALE;
}
#endif

/**
 * Function that disconnects a session and freeing any allocated memory used by
 * the session.
 *
 * @param 	args	[void *] "Is a args_t structure holding server_t and filedescriptor"
 * @return          [void]
 */
void WSS_disconnect(void *args, int id) {
    session_t *session;
    args_t *arguments = (args_t *) args;
    server_t *server = arguments->server;
    int fd = arguments->fd;

    WSS_log_trace("Disconnecting client");

    WSS_free((void **) &arguments);

    WSS_log_trace("Finding session");

    if ( unlikely(NULL == (session = WSS_session_find(fd))) ) {
        WSS_log_trace("Unable to find client with session %d", fd);
        return;
    }

    WSS_log_trace("Session found: %d", session->fd);

    WSS_disconnect_internal(server, session);
}

/**
 * Function that handles new connections. This function creates a new session and
 * associates the sessions filedescriptor to the epoll instance such that we can
 * start communicating with the session.
 *
 * @param 	arg     [void *] 		"Is in fact a server_t instance"
 * @return          [void]
 */
void WSS_connect(void *arg, int id) {
#ifdef USE_OPENSSL
    char ssl_msg[1024];
#endif
    int client_fd;
    struct epoll_event event;
    struct sockaddr_in client;
    size_t ringbuf_obj_size;
    socklen_t client_size;
    session_t *session;
    server_t *server = (server_t *) arg;
    ringbuf_t *ringbuf;
    size_t workers = server->config->pool_workers*server->config->pool_queues;

    memset(&event, 0, sizeof(event));

    client_size	= sizeof(client);
    memset((char *) &client, '\0', sizeof(client));

    while (1) {
        if ( (client_fd = accept(server->fd, (struct sockaddr *) &client,
                        &client_size)) < 0 ) {
            if ( likely(EAGAIN == errno || EWOULDBLOCK == errno) ) {
                break;
            }

            WSS_log_fatal("Accept failed: %s", strerror(errno));
            break;
        }

        WSS_log_trace("Received incoming connection");

        socket_non_blocking(client_fd);

        WSS_log_trace("Client filedescriptor was set to non-blocking");

        if ( unlikely(NULL == (session = WSS_session_add(client_fd,
                        inet_ntoa(client.sin_addr), ntohs(client.sin_port)))) ) {
            continue;
        }
        session->state = CONNECTING;
        WSS_log_trace("Created client session: %d", client_fd);

        // Creating ringbuffer for session
        ringbuf_get_sizes(workers, &ringbuf_obj_size, NULL);
        if ( unlikely(NULL == (ringbuf = WSS_malloc(ringbuf_obj_size))) ) {
            WSS_log_fatal("Failed to allocate memory for ringbuffer");
            return;
        }

        if ( unlikely(NULL == (session->messages = WSS_malloc(server->config->size_ringbuffer*sizeof(message_t *)))) ) {
            WSS_log_fatal("Failed to allocate memory for ringbuffer messages");
            WSS_free((void **)&ringbuf);
            return;
        }
        session->messages_count = server->config->size_ringbuffer;

        ringbuf_setup(ringbuf, workers, server->config->size_ringbuffer);
        session->ringbuf = ringbuf;

#ifdef USE_OPENSSL
        if (NULL == server->ssl_ctx) {
#endif
            WSS_log_trace("User connected from ip: %s:%d using HTTP request", session->ip, session->port);
#ifdef USE_OPENSSL
        } else {
            WSS_log_trace("User connected from ip: %s:%d using HTTPS request", session->ip, session->port);
        }
#endif

#ifdef USE_OPENSSL
        if (NULL != server->ssl_ctx) {
            WSS_log_trace("Creating ssl client structure");
            if ( unlikely(NULL == (session->ssl = SSL_new(server->ssl_ctx))) ) {
                ERR_error_string_n(ERR_get_error(), ssl_msg, 1024);
                WSS_log_error("Unable to create SSL structure: %s", ssl_msg);
                return;
            }

            WSS_log_trace("Associating structure with client filedescriptor");
            if ( unlikely(SSL_set_fd(session->ssl, session->fd) != 1) ) {
                ERR_error_string_n(ERR_get_error(), ssl_msg, 1024);
                WSS_log_error("Unable to bind filedescriptor to SSL structure: %s", ssl_msg);
                return;
            }

            WSS_log_trace("Allow writes to be partial");
            SSL_set_mode(session->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);

            //WSS_log_trace("Allow write buffer to be moving as it is allocated on the heap");
            //SSL_set_mode(session->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

            WSS_log_trace("Allow read and write buffers to be released when they are no longer needed");
            SSL_set_mode(session->ssl, SSL_MODE_RELEASE_BUFFERS);

            WSS_log_trace("Setting accept state");
            SSL_set_accept_state(session->ssl);

            WSS_ssl_handshake(server, session);
        } else {
#endif
            event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
            event.data.fd = session->fd;

            if ( unlikely((epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, session->fd,
                            &event)) < 0) ) {
                WSS_log_error("Unable to rearm epoll: %s", strerror(errno));
                return;
            }

            WSS_log_trace("Added client to pool of filedescriptors");
            session->state = STALE;
#ifdef USE_OPENSSL
        }

        WSS_log_info("Client with session %d connected", session->fd);
#endif
    }

}

/**
 * Puts a message in the client sessions writing buffer.
 *
 * @param 	session     [session_t *] 	"The client session"
 * @param 	message     [message_t *] 	"The message to send"
 * @param 	id          [int] 		    "The thread ID"
 * @return              [void]
 */
static void WSS_write_internal(session_t *session, message_t *mes, int id) {
    ringbuf_worker_t *w;
    ssize_t off;

    WSS_log_trace("Putting message into ringbuffer");

    w = ringbuf_register(session->ringbuf, id);
    if ( unlikely(-1 == (off = ringbuf_acquire(session->ringbuf, w, 1))) ) {
        WSS_free((void **) &mes->msg);
        WSS_free((void **) &mes);

        WSS_log_error("Failed to aquire space in ringbuffer");
        return;
    }
    session->messages[off] = mes;
    ringbuf_produce(session->ringbuf, w);
    ringbuf_unregister(session->ringbuf, w);
}

/**
 * Notifies the epoll filedescriptor for the specific session about being
 * ready to write.
 *
 * @param 	server      [server_t *] 	"A server instance"
 * @param 	fd          [int] 	        "The client filedescriptor"
 * @param 	closing     [bool] 		    "Whether the message is used to close the connection"
 * @return              [void]
 */
static void WSS_write_notify(server_t *server, int fd, bool closing) {
    struct epoll_event event;

    memset(&event, 0, sizeof(event));

    WSS_log_trace("Notifying client, that it needs to do WRITE");

    event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    if (! closing) {
        event.events |= EPOLLRDHUP;
    }
    event.data.fd = fd;

    if ( unlikely(epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0) ) {
        WSS_log_error("Unable to rearm epoll: %s", strerror(errno));
    }
}

/**
 * Notifies the epoll filedescriptor for the specific session about being
 * ready to write.
 *
 * @param 	server      [server_t *] 	"A server instance"
 * @param 	fd          [int] 	        "The client filedescriptor"
 * @return              [void]
 */
static void WSS_read_notify(server_t *server, int fd) {
    struct epoll_event event;

    memset(&event, 0, sizeof(event));

    WSS_log_trace("Notifying client, that it needs to do READ");

    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    event.data.fd = fd;

    if ( unlikely(epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0) ) {
        WSS_log_error("Unable to rearm epoll: %s", strerror(errno));
    }
}

/**
 * Performs the actual IO read operation using either the read or SSL_read
 * system calls.
 *
 * @param 	server      [server_t *] 	"A server instance"
 * @param 	session     [session_t *] 	"The client session"
 * @param 	buffer      [char *] 		"The buffer to put the data into"
 * @return              [int]           "The amount of bytes read or -1 for error or -2 for wait for IO"
 */
static int WSS_read_internal(server_t *server, session_t *session, char *buffer) {
    int n;

#ifdef USE_OPENSSL
    if ( NULL != server->ssl_ctx ) {
        unsigned long err;

        n = SSL_read(session->ssl, buffer, server->config->size_buffer);
        err = SSL_get_error(session->ssl, n);

        // There's no more to read from the kernel
        if (err == SSL_ERROR_WANT_READ) {
            n = 0;
        } else
            
        // There's no space to write to the kernel, wait for filedescriptor.
        if (err == SSL_ERROR_WANT_WRITE) {
            WSS_log_debug("SSL_ERROR_WANT_WRITE");

            WSS_write_notify(server, session->fd, false);

            n = -2;
        } else
            
        // Some error has occured.
        if ( unlikely(err != SSL_ERROR_NONE && err != SSL_ERROR_ZERO_RETURN) ) {
            char msg[1024];
            ERR_error_string_n(err, msg, 1024);
            WSS_log_error("SSL read failed: %s", msg);

            while ( (err = ERR_get_error()) != 0 ) {
                memset(msg, '\0', 1024);
                ERR_error_string_n(err, msg, 1024);
                WSS_log_error("SSL read failed: %s", msg);
            }

            n = -1;
        } else 
            
        // If this was one of the error codes that we do accept, return 0
        if ( unlikely(n < 0) ) {
            n = 0;
        }
    } else {
#endif
        n = read(session->fd, buffer, server->config->size_buffer);
        if (n == -1) {
            if ( unlikely(errno != EAGAIN && errno != EWOULDBLOCK) ) {
                WSS_log_error("Read failed: %s", strerror(errno));
            } else {
                n = 0;
            }
        }
#ifdef USE_OPENSSL
    }
#endif

    return n;
}

/**
 * Performs a websocket handshake with the client session
 *
 * @param 	server      [server_t *] 	"A server instance"
 * @param 	session     [session_t *] 	"The client session"
 * @return              [void]
 */
static void WSS_handshake(server_t *server, session_t *session, int id) {
    int n;
    header_t *header;
    struct epoll_event event;
    char buffer[server->config->size_buffer];
    enum HttpStatus_Code code;
    message_t *message;
    bool ssl = false;

    memset(buffer, '\0', server->config->size_buffer);
    memset(&event, 0, sizeof(event));

#ifdef USE_OPENSSL
    if (NULL != server->ssl_ctx) {
        ssl = true;
    }
#endif

    WSS_log_trace("Preparing client header");

    if ( unlikely(NULL != session->header) ) {
        header = session->header;
        session->header = NULL;
    } else {
        if ( unlikely(NULL == (header = WSS_malloc(sizeof(header_t)))) ) {
            return;
        }

        header->content             = NULL;
        header->method              = NULL;
        header->version             = NULL;
        header->path                = NULL;
        header->host                = NULL;
        header->payload             = NULL;
        header->length              = 0;
        header->ws_version          = 0;
        header->ws_type             = UNKNOWN;
        header->ws_protocol         = NULL;
        header->ws_upgrade          = NULL;
        header->ws_connection       = NULL;
        header->ws_extensions       = NULL;
        header->ws_extensions_count = 0;
        header->ws_origin           = NULL;
        header->ws_key              = NULL;
        header->ws_key1             = NULL;
        header->ws_key2             = NULL;
        header->ws_key3             = NULL;
    }

    WSS_log_trace("Reading headers");

    // Continue reading until we get no bytes back
    do {
        n = WSS_read_internal(server, session, buffer);

        switch (n) {
            // Wait for IO for either read or write on the filedescriptor
            case -2:
                session->header = header;
                return;
            // An error occured, notify client by writing back to it
            case -1:
                WSS_log_trace("Rejecting HTTP request due to being unable to read from client");

                if ( likely(NULL != (message = WSS_http_response(header,
                                HttpStatus_InternalServerError, "Unable to read from client"))) ) {
                    WSS_write_internal(session, message, id);
                    WSS_write_notify(server, session->fd, true);
                }

                WSS_free_header(header);

                return;
            case 0:
                break;
            default:
                // Reallocate space for the header and copy buffer into it
                if ( unlikely(NULL == (header->content = WSS_realloc((void **) &header->content, header->length*sizeof(char), (header->length+n+1)*sizeof(char)))) ) {
                    WSS_log_fatal("Unable to realloc header content");
                    return;
                }
                memcpy(header->content+header->length, buffer, n);
                header->length += n;
                memset(buffer, '\0', server->config->size_buffer);

                // Check if payload from client is too large for the server to handle.
                // If so write error back to the client
                if ( unlikely(header->length > (server->config->size_header+server->config->size_uri+server->config->size_payload)) ) {
                    WSS_log_trace("Rejecting HTTP request as client payload is too large for the server to handle");

                    if ( likely(NULL != (message = WSS_http_response(header, HttpStatus_PayloadTooLarge,
                                    "The given payload is too large for the server to handle"))) ) {
                        WSS_write_internal(session, message, id);
                        WSS_write_notify(server, session->fd, true);
                    }

                    WSS_free_header(header);

                    return;
                }
                break;
        }
    } while ( likely(n != 0) );

    session->state = STALE;

    WSS_log_debug("Client header: \n%s", header->content);

    WSS_log_trace("Starting parsing header received from client");

    // Parsing HTTP header
    // If header could not be parsed correctly, notify client with response.
    if ( (code = WSS_parse_header(session->fd, header, server->config)) != HttpStatus_OK ) {
        WSS_log_trace("Rejecting HTTP request due to header not being correct");

        if ( likely(NULL != (message = WSS_http_response(header, code, "Unable to parse header"))) ) {
            WSS_write_internal(session, message, id);
            WSS_write_notify(server, session->fd, true);
        }

        WSS_free_header(header);

        return;
    }

    // Serve favicon
    if (strlen(header->path) == 12 && strncmp(header->path, "/favicon.ico", 12) == 0) {
        if (NULL != (message = WSS_favicon_response(header, code, server->config))) {
            WSS_log_trace("Serving a favicon to the client");

            // Find and serve favicon.
            WSS_write_internal(session, message, id);
            WSS_write_notify(server, session->fd, true);
        } else {
            WSS_log_trace("Rejecting HTTP request due to favicon not being available");

            // Else notify client that favicon could not be found
            code = HttpStatus_NotFound;
            if ( likely(NULL != (message = WSS_http_response(header, code, "File not found"))) ) {
                WSS_write_internal(session, message, id);
                WSS_write_notify(server, session->fd, true);
            }
        }

        WSS_free_header(header);

        return;
    }

    WSS_log_trace("Header successfully parsed");

    // Create Upgrade HTTP header based on clients header
    code = WSS_upgrade_header(header, server->config, ssl, server->port);
    switch (code) {
        case HttpStatus_UpgradeRequired:
            WSS_log_trace("Rejecting HTTP request as the service requires use of the Websocket protocol.");
            message = WSS_upgrade_response(header, code,
                    "This service requires use of the Websocket protocol.");
            break;
        case HttpStatus_NotImplemented:
            WSS_log_trace("Rejecting HTTP request as Websocket protocol is not yet implemented");
            message = WSS_http_response(header, code,
                    "Websocket protocol is not yet implemented");
            break;
        case HttpStatus_SwitchingProtocols:
            message = WSS_handshake_response(header, code);
            break;
        case HttpStatus_NotFound:
            WSS_log_trace("Rejecting HTTP request as the page requested was not found.");
            message = WSS_http_response(header, code,
                    "The page requested was not found.");
            break;
        case HttpStatus_Forbidden:
            WSS_log_trace("Rejecting HTTP request as the origin is not allowed to establish a websocket connection.");
            message = WSS_http_response(header, code,
                    "The origin is not allowed to establish a websocket connection.");
            break;
        default:
            WSS_log_trace("Rejecting HTTP request as server was unable to parse http header as websocket request");
            message = WSS_http_response(header, code,
                    "Unable to parse http header as websocket request");
            break;
    }

    // If code status isnt switching protocols, we notify client with a HTTP error
    if (code != HttpStatus_SwitchingProtocols) {
        WSS_log_trace("Unable to establish websocket connection");

        WSS_write_internal(session, message, id);
        WSS_write_notify(server, session->fd, true);

        WSS_free_header(header);

        return;
    }

    // Use echo protocol if none was chosen
    if (NULL == header->ws_protocol) {
        header->ws_protocol = find_subprotocol("echo");
    }

    // Notify websocket protocol of the connection
    header->ws_protocol->connect(session->fd);

    // Set session as fully handshaked
    session->handshaked = true;
    session->header = header;

    WSS_write_internal(session, message, id);
    WSS_write_notify(server, session->fd, false);
}

/**
 * Function that reads information from a session.
 *
 * @param 	args	[void *] 	"Is a args_t structure holding server_t and filedescriptor"
 * @return          [void]
 */
void WSS_read(void *args, int id) {
    int n;
    size_t len;
    uint16_t code;
    frame_t *frame;
    char *msg;
    size_t offset = 0, prev_offset = 0;
    frame_t **frames = NULL;
    char *payload = NULL;
    size_t payload_length = 0;
    size_t frames_length = 0;
    char *message;
    size_t message_length;
    message_t *m;
    size_t i, j, k;
    int *receivers;
    receiver_t *r, *tmp;
    session_t *receiver;
    size_t receivers_count = 0;
    size_t msg_length = 0;
    size_t msg_offset = 0;
    size_t starting_frame = 0;
    bool rearmed = false;
    bool fragmented = false;
    receiver_t *total_receivers = NULL;
    args_t *arguments = (args_t *) args;
    server_t *server = arguments->server;
    int fd = arguments->fd;
    session_t *session;

    WSS_log_trace("Starting initial steps to read from client");

    if ( unlikely(NULL == (session = WSS_session_find(fd))) ) {
        WSS_log_trace("Unable to find client with session %d", fd);

        WSS_free((void **) &arguments);
        return;
    }

    switch (session->state) {
        case WRITING:
            WSS_write(arguments, id);
            return;
        case CLOSING:
            return;
        case CONNECTING:
#ifdef USE_OPENSSL
            if (NULL != server->ssl_ctx) {
                WSS_ssl_handshake(server, session);
                WSS_free((void **) &arguments);
                return;
            }
#endif
            WSS_free((void **) &arguments);
            break;
        case READING:
        case STALE:
            WSS_free((void **) &arguments);
            break;
    }

    session->state = READING;

    WSS_log_trace("Found client session");

    // If no initial header has been seen for the session, the websocket
    // handshake is yet to be made.
    if ( unlikely(! session->handshaked) ){
        WSS_log_trace("Doing websocket handshake");
        WSS_handshake(server, session, id);
        return;
    }

    char buffer[server->config->size_buffer];
    memset(buffer, '\0', server->config->size_buffer);

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

    //TODO: Have upper limit on payload, frame size and amount of frames to
    // avoid clients from exhausting the server

    // If handshake has been made, we can read the websocket frames from
    // the connection
    do {
        n = WSS_read_internal(server, session, buffer);

        switch (n) {
            // Wait for IO for either read or write on the filedescriptor
            case -2:
                WSS_log_trace("Detected that server needs further IO to complete the reading");
                session->payload = payload;
                session->payload_length = payload_length;
                return;
            // An error occured
            case -1:
                WSS_free((void **) &payload);

                frame = WSS_closing_frame(session->header, CLOSE_UNEXPECTED);
                message_length = WSS_stringify_frame(frame, &message);
                WSS_free_frame(frame);

                if ( unlikely(NULL == (m = WSS_malloc(sizeof(message_t)))) ) {
                    WSS_log_error("Unable to allocate the message structure");
                    return;
                }
                m->msg = message;
                m->length = message_length;
                m->framed = true;

                WSS_write_internal(session, m, id);
                WSS_write_notify(server, session->fd, true);
                return;
            // No new data received
            case 0:
                break;
            // Empty buffer into payload
            default:
                if ( unlikely(NULL == (payload = WSS_realloc((void **) &payload, payload_length*sizeof(char), (payload_length+n+1)*sizeof(char)))) ) {
                    WSS_log_error("Unable to reallocate the payload");
                    return;
                }

                memcpy(payload+payload_length, buffer, n);
                payload_length += n;
                memset(buffer, '\0', server->config->size_buffer);

        }
    } while ( likely(n != 0) );

    WSS_log_trace("Payload from client was read, parsing frames...");
    WSS_log_debug("Payload: %s (%lu bytes)", payload, payload_length);

    // Parse the payload into websocket frames
    do {
        prev_offset = offset;

        if ( unlikely(NULL == (frame = WSS_parse_frame(session->header, payload, payload_length, &offset))) ) {
            WSS_log_trace("Unable to parse frame");
            WSS_free((void **) &payload);
            return;
        }

        // Check if we were forced to read beyond the payload to create a full frame
        if ( unlikely(offset > payload_length) ) {
            WSS_log_trace("Detected that data was missing in order to complete frame, will wait for more");

            WSS_free_frame(frame);

            session->payload = payload;
            session->payload_length = payload_length;
            session->offset = prev_offset;
            session->frames = frames;
            session->frames_length = frames_length;

            WSS_read_notify(server, session->fd);
            return;
        }

        // If no extension is negotiated, the rsv bits must not be used
        if ( unlikely(NULL == session->header->ws_extensions && (frame->rsv1 || frame->rsv2 || frame->rsv3)) ) {
            WSS_log_trace("Protocol Error: rsv bits must not be set without using extensions");
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
        } else

        // If opcode is unknown
        if ( unlikely((frame->opcode >= 0x3 && frame->opcode <= 0x7) ||
                (frame->opcode >= 0xB && frame->opcode <= 0xF)) ) {
            WSS_log_trace("Type Error: Unknown upcode");
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_TYPE);
        } else

        // Server expects all received data to be masked
        if ( unlikely(! frame->mask) ) {
            WSS_log_trace("Protocol Error: Client message should always be masked");
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
        } else

        // Control frames cannot be fragmented
        if ( unlikely(! frame->fin && frame->opcode >= 0x8 && frame->opcode <= 0xA) ) {
            WSS_log_trace("Protocol Error: Control frames cannot be fragmented");
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
        } else

        // Control frames cannot have a payload length larger than 125 bytes
        if ( unlikely(frame->opcode >= 0x8 && frame->opcode <= 0xA && frame->payloadLength > 125) ) {
            WSS_log_trace("Protocol Error: Control frames cannot have payload larger than 125 bytes");
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
        } else

        // Close frame
        if ( unlikely(frame->opcode == 0x8) ) {
            // A code of 2 byte must be present if there is any application data for closing frame
            if ( unlikely(frame->applicationDataLength > 0 && frame->applicationDataLength < 2) ) {
                WSS_log_trace("Protocol Error: Closing frame with payload too small bytewise error code");
                WSS_free_frame(frame);
                frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
            } else 
                
            // The payload after the code, must be valid UTF8
            if ( unlikely(frame->applicationDataLength >= 2 && ! utf8_check(frame->payload+2, frame->applicationDataLength-2)) ) {
                WSS_log_trace("Protocol Error: Payload of error frame must be valid UTF8.");
                WSS_free_frame(frame);
                frame = WSS_closing_frame(session->header, CLOSE_UTF8);
            } else 
            
            // Check status code is within valid range
            if (frame->applicationDataLength >= 2) {
                // Copy code
                memcpy(&code, frame->payload, sizeof(uint16_t));
                code = ntohs(code);

                // Current rfc6455 codes
                if ( unlikely(code < 1000 || (code >= 1004 && code <= 1006) || (code >= 1015 && code < 3000) || code >= 5000) ) {
                    WSS_log_trace("Protocol Error: Closing frame has invalid error code");
                    WSS_free_frame(frame);
                    frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
                }
            }
        } else

        // Pong
        if ( unlikely(frame->opcode == 0xA) ) {
            WSS_log_trace("Pong received");
            // TODO:
            // Have cleanup thread, which loops through session list and removes
            // those that has not responded with a pong
            WSS_free_frame(frame);
            continue;
        } else

        // Ping
        if ( unlikely(frame->opcode == 0x9) ) {
            WSS_log_trace("Ping received");
            frame = WSS_pong_frame(session->header, frame);
        }

        if ( unlikely(NULL == (frames = WSS_realloc((void **) &frames, frames_length*sizeof(frame_t *),
                        (frames_length+1)*sizeof(frame_t *)))) ) {
            WSS_log_error("Unable to reallocate frames");
            return;
        }
        frames[frames_length] = frame;
        frames_length += 1;

        // Close
        if ( unlikely(frame->opcode == 0x8) ) {
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
        if ( unlikely(! fragmented && (frames[i]->opcode == 0x0)) ) {
            WSS_log_trace("Protocol Error: continuation opcode used in non-fragmented message");
            for (j = i; likely(j < frames_length); j++) {
                WSS_free_frame(frames[j]);
            }
            frames[i] = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
            frames_length = i+1;
            break;
        }

        // If we receive control frame within fragmented frames.
        if ( unlikely(fragmented && (frames[i]->opcode >= 0x8 && frames[i]->opcode <= 0xA)) ) {
            WSS_log_trace("Received control frame within fragmented message");

            frame = frames[i];
            // If we received a closing frame substitue the fragment with the
            // closing frame and end validation
            if (frames[i]->opcode == 0x8) {
                WSS_log_trace("Stopping further validation of fragmented message as a closing frame was detected");

                for (j = starting_frame; likely(j < frames_length); j++) {
                    if ( unlikely(j != i) ) {
                        WSS_free_frame(frames[j]);
                    } else {
                        frames[j] = NULL;
                    }
                }
                frames[starting_frame] = frame;
                frames_length = starting_frame+1;
                fragmented = false;
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
        if ( unlikely(fragmented && frames[i]->opcode != 0x0) ) {
            WSS_log_trace("Protocol Error: during fragmented message received other opcode than continuation");
            for (j = starting_frame; likely(j < frames_length); j++) {
                WSS_free_frame(frames[j]);
            }
            frames[starting_frame] = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
            frames_length = starting_frame+1;
            fragmented = false;
            break;
        }

        // If message consists of single frame or we have the starting frame of
        // a multiframe message, store the starting index
        if (frames[i]->fin && frames[i]->opcode != 0x0) {
            starting_frame = i;
        } else if ( ! frames[i]->fin && frames[i]->opcode != 0x0 ) {
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

        WSS_read_notify(server, session->fd);
        return;
    }

    session->state = STALE;

    WSS_log_trace("Frames was validated");
    WSS_log_trace("Starting sending frames to receivers based on the subprotocol");

    // Sending message to subprotocol and sending it to the filedescriptors
    // returned by the subprotocol.
    for (i = 0; likely(i < frames_length); i++) {
        // If message consists of single frame or we have the starting frame of
        // a multiframe message, store the starting index
        if ( (frames[i]->fin && !(frames[i]->opcode == 0x0)) || (! frames[i]->fin && frames[i]->opcode != 0x0) ) {
            starting_frame = i;
        }

        if (frames[i]->fin) {
            len = i-starting_frame+1;

            WSS_log_trace("Applying extensions on input");

            // Apply extensions to collection of frames (message)
            for (j = 0; likely(j < session->header->ws_extensions_count); j++) {
                // Apply extension perframe
                for (k = 0; likely(k < len); k++) {
                    session->header->ws_extensions[j]->ext->inframe(
                            session->fd,
                            (void *)frames[k+starting_frame]);
                }

                // Apply extension for set of frames
                session->header->ws_extensions[j]->ext->inframes(
                        session->fd,
                        (void **)frames+starting_frame,
                        len);
            }

            WSS_log_trace("Assembling message");

            for (j = starting_frame; likely(j <= i); j++) {
                msg_length += frames[j]->applicationDataLength;
            }

            if ( unlikely(NULL == (msg = WSS_malloc((msg_length+1)*sizeof(char)))) ) {
                WSS_log_error("Unable to allocate message");
                return;
            }

            for (j = starting_frame; likely(j <= i); j++) {
                memcpy(msg+msg_offset, frames[j]->payload+frames[j]->extensionDataLength, frames[j]->applicationDataLength);
                msg_offset += frames[j]->applicationDataLength;
            }

            // Check utf8 for text frames
            if ( unlikely(frames[starting_frame]->opcode == 0x1 && ! utf8_check(msg, msg_length)) ) {
                WSS_log_trace("UTF8 Error: the text was not UTF8 encoded correctly");

                for (j = starting_frame; likely(j < frames_length); j++) {
                    WSS_free_frame(frames[j]);
                }
                frames[starting_frame] = WSS_closing_frame(session->header, CLOSE_UTF8);
                frames_length = starting_frame+1;
                i = starting_frame;
                len = 1;
            }
            
            WSS_log_trace("Applying extensions to output");

            // Use extensions
            for (j = 0; likely(j < session->header->ws_extensions_count); j++) {
                session->header->ws_extensions[j]->ext->outframes(
                        session->fd,
                        (void **)frames+starting_frame,
                        len);

                for (k = 0; likely(k < len); k++) {
                    session->header->ws_extensions[j]->ext->outframe(
                            session->fd,
                            (void *)frames[starting_frame+k]);
                }
            }

            message_length = WSS_stringify_frames(&frames[starting_frame], len, &message);

            WSS_log_debug("Replying with message (%lu bytes): \n%s", message_length, message);

            if ( unlikely(NULL == (m = WSS_malloc(sizeof(message_t)))) ) {
                WSS_log_error("Unable to allocate message structure");
                return;
            }
            m->msg = message;
            m->length = message_length;
            m->framed = true;

            WSS_log_trace("Building list of receivers");

            // If data frame, get receivers from subprotocol 
            if ( likely(frames[starting_frame]->opcode == 0x1 || frames[starting_frame]->opcode == 0x2) ) {
                WSS_log_trace("Notifying subprotocol of message");

                // Use subprotocol
                session->header->ws_protocol->message(session->fd, msg, msg_length, &receivers, &receivers_count);

                WSS_log_debug("Received following message (%lu bytes) from client: \n%s", msg_length, msg);

                for (j = 0; likely(j < receivers_count); j++) {
                    if (receivers[j] == session->fd) {
                        receiver = session;
                        rearmed = true;
                    } else {
                        if ( NULL == (receiver = WSS_session_find(receivers[j])) ) {
                            continue;
                        }
                    }

                    HASH_FIND_INT(total_receivers, &receivers[j], r);
                    if ( likely(NULL == r) ) {
                        if ( unlikely(NULL == (r = malloc(sizeof(receiver_t)))) ) {
                            WSS_log_error("Unable to allocate receiver structure");
                            return;
                        }
                        r->fd = receivers[j];

                        HASH_ADD_INT( total_receivers, fd, r );
                    }

                    WSS_write_internal(receiver, m, id);
                }

                WSS_free((void **) &receivers);
            } else {
                rearmed = true;
                HASH_FIND_INT(total_receivers, &session->fd, r);
                if ( likely(NULL == r) ) {
                    if ( unlikely(NULL == (r = malloc(sizeof(receiver_t)))) ) {
                        WSS_log_error("Unable to allocate receiver structure");
                        return;
                    }
                    r->fd = session->fd;
                    HASH_ADD_INT( total_receivers, fd, r );
                }
                WSS_write_internal(session, m, id);
            }

            msg_length = 0;
            msg_offset = 0;
            WSS_free((void **) &msg);
        }
    }
        
    WSS_log_trace("Notifying receivers about incoming message");

    HASH_ITER(hh, total_receivers, r, tmp) {
        if ( unlikely(r != NULL) ) {
            HASH_DEL(total_receivers, r);

            WSS_log_trace("Sending message to session %d", r->fd);
            WSS_write_notify(server, r->fd, false);

            WSS_free((void **) &r);
        }
    }

    for (k = 0; likely(k < frames_length); k++) {
        WSS_free_frame(frames[k]);
    }
    WSS_free((void **) &frames);

    if (!rearmed) {
        WSS_read_notify(server, session->fd);
    }
}

/**
 * Function that writes information to a session.
 *
 * @param 	args	[void *] 	"Is a args_t structure holding server_t and filedescriptor"
 * @return          [void]
 */
void WSS_write(void *args, int id) {
    int n;
    args_t *arguments = (args_t *) args;
    int fd = arguments->fd;
    server_t *server = arguments->server;
    session_t *session;
    message_t *message;
    unsigned int i;
    unsigned int message_length;
    unsigned int bytes_sent;
    size_t len, off;
    bool closing = false;

    if ( unlikely(NULL == (session = WSS_session_find(fd))) ) {
        WSS_log_trace("Unable to find client with session %d", fd);
        WSS_free((void **) &arguments);
        return;
    }

    switch (session->state) {
        case READING:
            WSS_read(arguments, id);
            return;
        case CLOSING:
            return;
        case CONNECTING:
#ifdef USE_OPENSSL
            if (NULL != server->ssl_ctx) {
                WSS_ssl_handshake(server, session);
                WSS_free((void **) &arguments);
                return;
            }
#endif
            WSS_free((void **) &arguments);
            break;
        case WRITING:
        case STALE:
            WSS_free((void **) &arguments);
            break;
    }

    WSS_log_trace("Starting initial steps to write to client");

    session->state = WRITING;

    WSS_log_trace("Popping messages from ringbuffer");

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

#ifdef USE_OPENSSL
                if (NULL != server->ssl_ctx) {
                    unsigned long err;

                    n = SSL_write(session->ssl, message->msg+bytes_sent, message_length-bytes_sent);
                    err = SSL_get_error(session->ssl, n);

                    // If something more needs to be read in order for the handshake to finish
                    if (err == SSL_ERROR_WANT_READ) {
                        WSS_log_trace("Need to wait for further reads");

                        session->written = bytes_sent;
                        ringbuf_release(session->ringbuf, i);

                        WSS_read_notify(server, session->fd);
                        return;
                    }

                    // If something more needs to be written in order for the handshake to finish
                    if (err == SSL_ERROR_WANT_WRITE) {
                        WSS_log_trace("Need to wait for further writes");

                        session->written = bytes_sent;

                        ringbuf_release(session->ringbuf, i);

                        WSS_write_notify(server, session->fd, false);
                        return;
                    }

                    if ( unlikely(err != SSL_ERROR_NONE && err != SSL_ERROR_ZERO_RETURN) ) {
                        char msg[1024];
                        ERR_error_string_n(err, msg, 1024);
                        WSS_log_error("SSL write failed: %s", msg);

                        while ( (err = ERR_get_error()) != 0 ) {
                            ERR_error_string_n(err, msg, 1024);
                            WSS_log_error("SSL write failed: %s", msg);
                        }

                        WSS_disconnect_internal(server, session);

                        return;
                    } else {
                        if (unlikely(n < 0)) {
                            n = 0;
                        }
                        bytes_sent += n;
                    }
                } else {
#endif
                    n = write(session->fd, message->msg+bytes_sent, message_length-bytes_sent);
                    if (unlikely(n == -1)) {
                        if ( unlikely(errno != EAGAIN && errno != EWOULDBLOCK) ) {
                            WSS_log_error("Write failed: %s", strerror(errno));
                            WSS_disconnect_internal(server, session);
                            return;
                        }

                        session->written = bytes_sent;
                        ringbuf_release(session->ringbuf, i);

                        WSS_write_notify(server, session->fd, false);
                        return;
                    } else {
                        bytes_sent += n;
                    }

#ifdef USE_OPENSSL
                }
#endif
            }

            if ( likely(session->messages != NULL) ) {
                if ( likely(session->messages[off+i] != NULL) ) {
                    if ( likely(session->messages[off+i]->msg != NULL) ) {
                        WSS_free((void **)&session->messages[off+i]->msg);
                    }
                    WSS_free((void **)&session->messages[off+i]);
                }
            }
        }

        ringbuf_release(session->ringbuf, len);
    }

    WSS_log_trace("Done writing to filedescriptors");

    session->state = STALE;

    if (closing) {
        WSS_log_trace("Closing connection, since closing frame has been sent");
        WSS_disconnect_internal(server, session);
    } else {
        WSS_log_trace("Set epoll file descriptor to read mode");

        WSS_read_notify(server, session->fd);
    }
}
