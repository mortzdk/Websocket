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

    //TODO: support optional extensions
    // if ( NULL != header->ws_extension ) {
    // }
    
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

    WSS_log(
        CLIENT_DEBUG,
        message,
        __FILE__,
        __LINE__
    );

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

inline static void WSS_diconnect_internal(server_t *server, session_t *session) {
    int size;
    char *message;
    struct epoll_event event;
    memset(&event, 0, sizeof(event));

    session->state = CLOSING;

    size = strlen(STRING_DISCONNECTED)*sizeof(char) +
        strlen(session->ip)*sizeof(char) +
        (log10(session->port)+1)*sizeof(char) +
        strlen(STRING_HTTP)*sizeof(char) +
        2+1;

#ifdef USE_OPENSSL
    if (NULL == server->ssl_ctx) {
        size++;
    }
#endif

    if ( unlikely(NULL == (message = (char *) WSS_malloc(size))) ) {
        return;
    }

#ifdef USE_OPENSSL
    if (NULL == server->ssl_ctx) {
#endif
        snprintf(
                message,
                (size_t) size,
                "%s%s:%d%s",
                STRING_DISCONNECTED,
                session->ip,
                session->port,
                STRING_HTTP
                );
        WSS_log(CLIENT_TRACE, message, __FILE__, __LINE__);
#ifdef USE_OPENSSL
    } else {
        snprintf(
                message,
                (size_t) size,
                "%s%s:%d%s",
                STRING_DISCONNECTED,
                session->ip,
                session->port,
                STRING_HTTPS
                );
        WSS_log(CLIENT_TRACE, message, __FILE__, __LINE__);
    }
#endif
    WSS_free((void **) &message);

    WSS_log(CLIENT_TRACE, "Informing subprotocol about session close", __FILE__, __LINE__);

    session->header->ws_protocol->close(session->fd);

    WSS_log(CLIENT_TRACE, "Removing clients filedescriptor from the pool", __FILE__, __LINE__);

    if ( unlikely((epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, session->fd, &event)) < 0) ) {
        WSS_log(SERVER_ERROR, strerror(errno), __FILE__, __LINE__);
        return;
    }

    WSS_log(CLIENT_TRACE, "Deleting clients session", __FILE__, __LINE__);

    if ( unlikely(SUCCESS != WSS_session_delete(session)) ) {
        WSS_log(SERVER_ERROR, "Unable to delete client session", __FILE__, __LINE__);
        return;
    }
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

    WSS_log(CLIENT_TRACE, "Performing SSL handshake", __FILE__, __LINE__);

    if (NULL != server->ssl_ctx) {
        ret = SSL_do_handshake(session->ssl);
        err = SSL_get_error(session->ssl, ret);

        // If something more needs to be read in order for the handshake to finish
        if (err == SSL_ERROR_WANT_READ) {
            WSS_log(CLIENT_DEBUG, "SSL_ERROR_WANT_READ", __FILE__, __LINE__);

            event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
            event.data.fd = session->fd;

            if ( unlikely((n = epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, session->fd, &event)) < 0) ) {
                // File descriptor could already have been added to epoll due to SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
                if ( likely(errno == EEXIST) ) {
                    n = epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, session->fd, &event);
                }

                if ( unlikely(n < 0) ) {
                    WSS_log(SERVER_ERROR, strerror(errno), __FILE__, __LINE__);
                    WSS_diconnect_internal(server, session);
                    return;
                }
            }

            return;
        }

        // If something more needs to be written in order for the handshake to finish
        if (err == SSL_ERROR_WANT_WRITE) {
            WSS_log(CLIENT_DEBUG, "SSL_ERROR_WANT_WRITE", __FILE__, __LINE__);

            event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
            event.data.fd = session->fd;

            if ( unlikely((n = epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, session->fd, &event)) < 0) ) {
                // File descriptor could already have been added to epoll due to SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
                if ( likely(errno == EEXIST) ) {
                    n = epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, session->fd, &event);
                }

                if ( unlikely(n < 0) ) {
                    WSS_log(SERVER_ERROR, strerror(errno), __FILE__, __LINE__);
                    WSS_diconnect_internal(server, session);
                    return;
                }
            }

            return;
        }

        if ( unlikely(err != SSL_ERROR_NONE && err != SSL_ERROR_ZERO_RETURN) ) {
            char message[1024];
            ERR_error_string_n(err, message, 1024);
            WSS_log(CLIENT_ERROR, message, __FILE__, __LINE__);

            while ( (err = ERR_get_error()) != 0 ) {
                ERR_error_string_n(err, message, 1024);
                WSS_log(CLIENT_ERROR, message, __FILE__, __LINE__);
            }

            WSS_diconnect_internal(server, session);
            return;
        }

        WSS_log(CLIENT_TRACE, "SSL handshake was successfull", __FILE__, __LINE__);

        session->ssl_connected = true;

        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
        event.data.fd = session->fd;

        if ( unlikely((n = epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, session->fd, &event)) < 0) ) {
            // File descriptor could already have been added to epoll due to SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE
            if ( likely(errno == EEXIST) ) {
                n = epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, session->fd, &event);
            }

            if ( unlikely(n < 0) ) {
                WSS_log(SERVER_ERROR, strerror(errno), __FILE__, __LINE__);
                WSS_diconnect_internal(server, session);
                return;
            }
        }

        WSS_log(CLIENT_TRACE, "Added client to pool of filedescriptors", __FILE__, __LINE__);
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

    WSS_log(CLIENT_TRACE, "Disconnecting client", __FILE__, __LINE__);

    WSS_free((void **) &arguments);

    WSS_log(CLIENT_TRACE, "Finding session", __FILE__, __LINE__);

    if ( unlikely(NULL == (session = WSS_session_find(fd))) ) {
        return;
    }

    WSS_log(CLIENT_TRACE, "Session found", __FILE__, __LINE__);

    WSS_diconnect_internal(server, session);
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
    char *message;
    int client_fd;
    int size;
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

            WSS_log(SERVER_ERROR, strerror(errno), __FILE__, __LINE__);
            break;
        }

        WSS_log(CLIENT_TRACE, "Received incoming connection", __FILE__, __LINE__);

        socket_non_blocking(client_fd);

        WSS_log(CLIENT_TRACE, "Set client filedescriptor to non-blocking", __FILE__, __LINE__);

        if ( unlikely(NULL == (session = WSS_session_add(client_fd,
                        inet_ntoa(client.sin_addr), ntohs(client.sin_port)))) ) {
            continue;
        }
        session->state = CONNECTING;
        WSS_log(CLIENT_TRACE, "Created client session structure", __FILE__, __LINE__);

        // Creating ringbuffer for session
        ringbuf_get_sizes(workers, &ringbuf_obj_size, NULL);
        if ( unlikely(NULL == (ringbuf = WSS_malloc(ringbuf_obj_size))) ) {
            WSS_log(SERVER_ERROR, "Not enough memory to allocate ringbuf", __FILE__, __LINE__);
            return;
        }

        if ( unlikely(NULL == (session->messages = WSS_malloc(server->config->size_pipe*sizeof(message_t *)))) ) {
            WSS_log(SERVER_ERROR, "Not enough memory to allocate messages buffer", __FILE__, __LINE__);
            WSS_free((void **)&ringbuf);
            return;
        }
        session->messages_count = server->config->size_pipe;

        ringbuf_setup(ringbuf, workers, server->config->size_pipe);
        session->ringbuf = ringbuf;

        // Allocate memory to write client trace
        size = strlen(STRING_CONNECTED)*sizeof(char) +
            strlen(session->ip)*sizeof(char) +
            (log10(session->port)+1)*sizeof(char) +
            2+1;

#ifdef USE_OPENSSL
        if (NULL == server->ssl_ctx) {
            size += strlen(STRING_HTTPS)*sizeof(char);
        } else {
#endif
            size += strlen(STRING_HTTP)*sizeof(char);
#ifdef USE_OPENSSL
        }
#endif

        if ( unlikely(NULL == (message = (char *) WSS_malloc(size))) ) {
            WSS_log(SERVER_ERROR, "Not enough memory to allocate log message", __FILE__, __LINE__);
            return;
        }

#ifdef USE_OPENSSL
        if (NULL == server->ssl_ctx) {
#endif
            snprintf(
                    message,
                    (size_t) size,
                    "%s%s:%d%s",
                    STRING_CONNECTED,
                    session->ip,
                    session->port,
                    STRING_HTTP
                    );
            WSS_log(CLIENT_TRACE, message, __FILE__, __LINE__);
#ifdef USE_OPENSSL
        } else {
            snprintf(
                    message,
                    (size_t) size,
                    "%s%s:%d%s",
                    STRING_CONNECTED,
                    inet_ntoa(client.sin_addr),
                    ntohs(client.sin_port),
                    STRING_HTTPS
                    );
            WSS_log(CLIENT_TRACE, message, __FILE__, __LINE__);
        }
#endif
        WSS_free((void **) &message);

#ifdef USE_OPENSSL
        if (NULL != server->ssl_ctx) {
            WSS_log(CLIENT_TRACE, "Creating ssl client structure", __FILE__, __LINE__);
            if ( unlikely(NULL == (session->ssl = SSL_new(server->ssl_ctx))) ) {
                ERR_error_string_n(ERR_get_error(), ssl_msg, 1024);
                WSS_log(CLIENT_ERROR, ssl_msg, __FILE__, __LINE__);
                return;
            }

            WSS_log(CLIENT_TRACE, "Associating structure with client filedescriptor", __FILE__, __LINE__);
            if ( unlikely(SSL_set_fd(session->ssl, session->fd) != 1) ) {
                ERR_error_string_n(ERR_get_error(), ssl_msg, 1024);
                WSS_log(CLIENT_ERROR, ssl_msg, __FILE__, __LINE__);
                return;
            }

            WSS_log(CLIENT_TRACE, "Allow writes to be partial", __FILE__, __LINE__);
            SSL_set_mode(session->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);

            WSS_log(CLIENT_TRACE, "Allow write buffer to be moving as it is allocated on the heap", __FILE__, __LINE__);
            SSL_set_mode(session->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

            WSS_log(CLIENT_TRACE, "Allow read and write buffers to be released when they are no longer needed", __FILE__, __LINE__);
            SSL_set_mode(session->ssl, SSL_MODE_RELEASE_BUFFERS);

            WSS_log(CLIENT_TRACE, "Setting accept state", __FILE__, __LINE__);
            SSL_set_accept_state(session->ssl);

            WSS_ssl_handshake(server, session);
        } else {
#endif
            event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
            event.data.fd = session->fd;

            if ( unlikely((epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, session->fd,
                            &event)) < 0) ) {
                WSS_log(SERVER_ERROR, strerror(errno), __FILE__, __LINE__);
                return;
            }

            WSS_log(CLIENT_TRACE, "Added client to pool of filedescriptors", __FILE__, __LINE__);
            session->state = STALE;
#ifdef USE_OPENSSL
        }
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

    WSS_log(CLIENT_TRACE, "Putting message into ringbuffer", __FILE__, __LINE__);

    w = ringbuf_register(session->ringbuf, id);
    if ( unlikely(-1 == (off = ringbuf_acquire(session->ringbuf, w, 1))) ) {
        WSS_free((void **) &mes->msg);
        WSS_free((void **) &mes);
        WSS_log(CLIENT_ERROR, "Failed to aquire space in ringbuffer", __FILE__, __LINE__);
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

    WSS_log(CLIENT_TRACE, "Notifying client, that it needs to do WRITE", __FILE__, __LINE__);

    event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    if (! closing) {
        event.events |= EPOLLRDHUP;
    }
    event.data.fd = fd;

    if ( unlikely(epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0) ) {
        WSS_log(SERVER_ERROR, strerror(errno), __FILE__, __LINE__);
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

    WSS_log(CLIENT_TRACE, "Notifying client, that it needs to do READ", __FILE__, __LINE__);

    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    event.data.fd = fd;

    if ( unlikely(epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0) ) {
        WSS_log(SERVER_ERROR, strerror(errno), __FILE__, __LINE__);
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
            WSS_log(CLIENT_DEBUG, "SSL_ERROR_WANT_WRITE", __FILE__, __LINE__);

            WSS_write_notify(server, session->fd, false);

            n = -2;
        } else
            
        // Some error has occured.
        if ( unlikely(err != SSL_ERROR_NONE && err != SSL_ERROR_ZERO_RETURN) ) {
            char msg[1024];
            ERR_error_string_n(err, msg, 1024);
            WSS_log(CLIENT_ERROR, msg, __FILE__, __LINE__);

            while ( (err = ERR_get_error()) != 0 ) {
                ERR_error_string_n(err, msg, 1024);
                WSS_log(CLIENT_ERROR, msg, __FILE__, __LINE__);
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
                WSS_log(CLIENT_ERROR, strerror(errno), __FILE__, __LINE__);
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

    WSS_log(CLIENT_TRACE, "Preparing client header", __FILE__, __LINE__);
    if ( unlikely(NULL != session->header) ) {
        header = session->header;
        session->header = NULL;
    } else {
        if ( unlikely(NULL == (header = WSS_malloc(sizeof(header_t)))) ) {
            return;
        }

        header->content       = NULL;
        header->method        = NULL;
        header->version       = NULL;
        header->path          = NULL;
        header->host          = NULL;
        header->payload       = NULL;
        header->length        = 0;
        header->ws_version    = 0;
        header->ws_type       = UNKNOWN;
        header->ws_protocol   = NULL;
        header->ws_upgrade    = NULL;
        header->ws_connection = NULL;
        header->ws_extension  = NULL;
        header->ws_origin     = NULL;
        header->ws_key        = NULL;
        header->ws_key1       = NULL;
        header->ws_key2       = NULL;
        header->ws_key3       = NULL;
    }

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
                    return;
                }
                memcpy(header->content+header->length, buffer, n);
                header->length += n;
                memset(buffer, '\0', server->config->size_buffer);

                // Check if payload from client is too large for the server to handle.
                // If so write error back to the client
                if ( unlikely(header->length > (server->config->size_header+server->config->size_uri+server->config->size_payload)) ) {
                    WSS_log(CLIENT_ERROR, "The client payload is too large for the server to handle", __FILE__, __LINE__);

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

    WSS_log(CLIENT_DEBUG, header->content, __FILE__, __LINE__);
    WSS_log(CLIENT_TRACE, "Starting parsing content received from client", __FILE__, __LINE__);

    // Parsing HTTP header
    // If header could not be parsed correctly, notify client with response.
    if ( (code = WSS_parse_header(header, server->config)) != HttpStatus_OK ) {
        WSS_log(CLIENT_TRACE, "Unable to parse clients http header", __FILE__, __LINE__);

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
            // Find and serve favicon.
            WSS_write_internal(session, message, id);
            WSS_write_notify(server, session->fd, true);
        } else {
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

    WSS_log(CLIENT_TRACE, "Parsed header", __FILE__, __LINE__);

    // Create Upgrade HTTP header based on clients header
    code = WSS_upgrade_header(header, server->config, ssl, server->port);
    switch (code) {
        case HttpStatus_UpgradeRequired:
            message = WSS_upgrade_response(header, code,
                    "This service requires use of the Websocket protocol.");
            break;
        case HttpStatus_NotImplemented:
            message = WSS_http_response(header, code,
                    "Websocket protocol is not yet implemented");
            break;
        case HttpStatus_SwitchingProtocols:
            message = WSS_handshake_response(header, code);
            break;
        case HttpStatus_NotFound:
            message = WSS_http_response(header, code,
                    "The page requested was not found.");
            break;
        case HttpStatus_Forbidden:
            message = WSS_http_response(header, code,
                    "The origin is not allowed to establish a websocket connection.");
            break;
        default:
            message = WSS_http_response(header, code,
                    "Unable to parse http header as websocket request");
            break;
    }

    // If code status isnt switching protocols, we notify client with a HTTP error
    if (code != HttpStatus_SwitchingProtocols) {
        WSS_log(
                CLIENT_TRACE,
                "Unable to establish websocket connection",
                __FILE__,
                __LINE__
               );

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
    uint16_t code;
    struct epoll_event event;
    frame_t *frame;
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
    char *msg = NULL;
    size_t msg_length = 0;
    size_t starting_frame = 0;
    bool rearmed = false;
    bool fragmented = false;
    receiver_t *total_receivers = NULL;
    args_t *arguments = (args_t *) args;
    server_t *server = arguments->server;
    int fd = arguments->fd;
    session_t *session;

    WSS_log(CLIENT_TRACE, "Starting initial steps to read from client", __FILE__, __LINE__);

    if ( unlikely(NULL == (session = WSS_session_find(fd))) ) {
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

    WSS_log(CLIENT_TRACE, "Found client session", __FILE__, __LINE__);

    // If no initial header has been seen for the session, the websocket
    // handshake is yet to be made.
    if ( unlikely(! session->handshaked) ){
        WSS_log(CLIENT_TRACE, "Doing websocket handshake", __FILE__, __LINE__);
        WSS_handshake(server, session, id);
        return;
    }

    char buffer[server->config->size_buffer];
    memset(buffer, '\0', server->config->size_buffer);

    memset(&event, 0, sizeof(event));

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
        n = WSS_read_internal(server, session, buffer);

        switch (n) {
            // Wait for IO for either read or write on the filedescriptor
            case -2:
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
                    return;
                }

                memcpy(payload+payload_length, buffer, n);
                payload_length += n;
                memset(buffer, '\0', server->config->size_buffer);

        }
    } while ( likely(n != 0) );

    WSS_log(CLIENT_TRACE, "Payload from client was read, parsing frames...", __FILE__, __LINE__);

    // Parse the payload into websocket frames
    do {
        prev_offset = offset;

        if ( unlikely(NULL == (frame = WSS_parse_frame(session->header, payload, payload_length, &offset))) ) {
            WSS_free((void **) &payload);
            return;
        }

        // Check if we were forced to read beyond the payload to create a full frame
        if ( unlikely(offset > payload_length) ) {
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
        if ( unlikely(NULL == session->header->ws_extension && (frame->rsv1 || frame->rsv2 || frame->rsv3)) ) {
            WSS_log(CLIENT_TRACE, "Protocol Error: rsv bits must not be set without using extensions", __FILE__, __LINE__);
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
        } else

        // If opcode is unknown
        if ( unlikely((frame->opcode >= 0x3 && frame->opcode <= 0x7) ||
                (frame->opcode >= 0xB && frame->opcode <= 0xF)) ) {
            WSS_log(CLIENT_TRACE, "Type Error: Unknown upcode", __FILE__, __LINE__);
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_TYPE);
        } else

        // Server expects all received data to be masked
        if ( unlikely(! frame->mask) ) {
            WSS_log(CLIENT_TRACE, "Protocol Error: Client message should always be masked", __FILE__, __LINE__);
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
        } else

        // Control frames cannot be fragmented
        if ( unlikely(! frame->fin && frame->opcode >= 0x8 && frame->opcode <= 0xA) ) {
            WSS_log(CLIENT_TRACE, "Protocol Error: Control frames cannot be fragmented", __FILE__, __LINE__);
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
        } else

        // Control frames cannot have a payload length larger than 125 bytes
        if ( unlikely(frame->opcode >= 0x8 && frame->opcode <= 0xA && frame->payloadLength > 125) ) {
            WSS_log(CLIENT_TRACE, "Protocol Error: Control frames cannot have payload larger than 125 bytes", __FILE__, __LINE__);
            WSS_free_frame(frame);
            frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
        } else

        // Close frame
        if ( unlikely(frame->opcode == 0x8) ) {
            // A code of 2 byte must be present if there is any application data for closing frame
            if ( unlikely(frame->applicationDataLength > 0 && frame->applicationDataLength < 2) ) {
                WSS_log(CLIENT_TRACE, "Protocol Error: Closing frame with payload have 2 byte error code", __FILE__, __LINE__);
                WSS_free_frame(frame);
                frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
            } else 
                
            // The payload after the code, must be valid UTF8
            if ( unlikely(frame->applicationDataLength >= 2 && ! utf8_check(frame->applicationData+2, frame->applicationDataLength-2)) ) {
                WSS_log(CLIENT_TRACE, "UTF8 Error: Closing frame with payload must contain UTF8 data", __FILE__, __LINE__);
                WSS_free_frame(frame);
                frame = WSS_closing_frame(session->header, CLOSE_UTF8);
            } else 
            
            // Check status code is within valid range
            if (frame->applicationDataLength >= 2) {
                // Copy code
                memcpy(&code, frame->applicationData, sizeof(uint16_t));
                code = ntohs(code);

                // Current rfc6455 codes
                if ( unlikely(code < 1000 || (code >= 1004 && code <= 1006) || (code >= 1015 && code < 3000) || code >= 5000) ) {
                    WSS_log(CLIENT_TRACE, "Protocol Error: Closing frame has error code less than 1000", __FILE__, __LINE__);
                    WSS_free_frame(frame);
                    frame = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
                }
            }
        } else

        // Pong
        if ( unlikely(frame->opcode == 0xA) ) {
            WSS_log(CLIENT_TRACE, "Pong received", __FILE__, __LINE__);
            // TODO:
            // Have cleanup thread, which loops through session list and removes
            // those that has not responded with a pong
            WSS_free_frame(frame);
            continue;
        } else

        // Ping
        if ( unlikely(frame->opcode == 0x9) ) {
            WSS_log(CLIENT_TRACE, "Ping received, replying with a pong", __FILE__, __LINE__);
            frame = WSS_pong_frame(session->header, frame);
        }

        frames_length += 1;
        if ( unlikely(NULL == (frames = WSS_realloc((void **) &frames, (frames_length-1)*sizeof(frame_t *),
                        frames_length*sizeof(frame_t *)))) ) {
            return;
        }

        frames[frames_length-1] = frame;

        // Close
        if ( unlikely(frame->opcode == 0x8) ) {
            break;
        }
    } while ( likely(offset < payload_length) );

    WSS_free((void **) &payload);

    WSS_log(CLIENT_TRACE, "Frames was parsed, validating frames...", __FILE__, __LINE__);

    // Validating frames.
    for (i = 0; likely(i < frames_length); i++) {
        // If we are not processing a fragmented set of frames, expect the
        // opcode different from the continuation frame.
        if ( unlikely(! fragmented && (frames[i]->opcode == 0x0)) ) {
            WSS_log(CLIENT_TRACE, "Protocol Error: continuation opcode used in non-fragmented message", __FILE__, __LINE__);
            for (j = i; likely(j < frames_length); j++) {
                WSS_free_frame(frames[j]);
            }
            frames[i] = WSS_closing_frame(session->header, CLOSE_PROTOCOL);
            frames_length = i+1;
            break;
        }

        // If we receive control frame within fragmented frames.
        if ( unlikely(fragmented && (frames[i]->opcode >= 0x8 && frames[i]->opcode <= 0xA)) ) {
            frame = frames[i];
            // If we received a closing frame substitue the fragment with the
            // closing frame and end validation
            if (frames[i]->opcode == 0x8) {
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
            WSS_log(CLIENT_TRACE, "Protocol Error: during fragmented message received other opcode than continuation", __FILE__, __LINE__);
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
            // If message consist of text frames, validate the text as UTF8
            if (frames[starting_frame]->opcode == 0x1) {
                for (j = starting_frame; likely(j <= i); j++) {
                    if ( unlikely(NULL == (msg = WSS_realloc((void **) &msg, msg_length*sizeof(char), (msg_length+frames[j]->applicationDataLength+1)*sizeof(char)))) ) {
                        return;
                    }
                    memcpy(msg+msg_length, frames[j]->applicationData, frames[j]->applicationDataLength);
                    msg_length += frames[j]->applicationDataLength;
                }

                if ( unlikely(! utf8_check(msg, msg_length)) ) {
                    WSS_log(CLIENT_TRACE, "UTF8 Error: the text was not UTF8 encoded correctly", __FILE__, __LINE__);

                    for (j = starting_frame; likely(j <= i); j++) {
                        WSS_free_frame(frames[j]);
                    }
                    frames[starting_frame] = WSS_closing_frame(session->header, CLOSE_UTF8);
                    frames_length = starting_frame+1;
                    WSS_free((void **) &msg);
                    msg_length = 0;
                    fragmented = false;
                    break;
                }

                WSS_free((void **) &msg);
            }

            msg = NULL;
            msg_length = 0;
            fragmented = false;
        }
    }

    // If fragmented is still true, we did not receive the whole message, and
    // we hence want to wait until we get the rest.
    if ( unlikely(fragmented) ) {
        WSS_log(CLIENT_TRACE, "Missing frames in fragmented message, will wait for READ.", __FILE__, __LINE__);

        session->frames = frames;
        session->frames_length = frames_length;

        WSS_read_notify(server, session->fd);
        return;
    }

    session->state = STALE;

    WSS_log(CLIENT_TRACE, "Frames was validated, sending frames...", __FILE__, __LINE__);

    // Sending message to subprotocol and sending it to the filedescriptors
    // returned by the subprotocol.
    for (i = 0; likely(i < frames_length); i++) {
        if ( unlikely(NULL == (msg = WSS_realloc((void **) &msg, msg_length*sizeof(char), (msg_length+frames[i]->applicationDataLength+1)*sizeof(char)))) ) {
            return;
        }
        memcpy(msg+msg_length, frames[i]->applicationData, frames[i]->applicationDataLength);
        msg_length += frames[i]->applicationDataLength;

        // If message consists of single frame or we have the starting frame of
        // a multiframe message, store the starting index
        if (frames[i]->fin && !(frames[i]->opcode == 0x0)) {
            starting_frame = i;
        } else if (! frames[i]->fin && frames[i]->opcode == 0x1) {
            starting_frame = i;
            fragmented = true;
        }

        if (frames[i]->fin) {
            WSS_log(CLIENT_DEBUG, "Received following message from client:", __FILE__, __LINE__);
            WSS_log(CLIENT_DEBUG, msg, __FILE__, __LINE__);

            // Use subprotocol
            session->header->ws_protocol->message(session->fd, msg, msg_length, &receivers, &receivers_count);

            message_length = WSS_stringify_frames(&frames[starting_frame],
                    i-starting_frame+1, &message);

            WSS_log(CLIENT_DEBUG, "Replying with message: ", __FILE__, __LINE__);
            WSS_log(CLIENT_DEBUG, message, __FILE__, __LINE__);

            if ( unlikely(NULL == (m = WSS_malloc(sizeof(message_t)))) ) {
                return;
            }
            m->msg = message;
            m->length = message_length;
            m->framed = true;

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
                if (NULL == r) {
                    if ( unlikely(NULL == (r = malloc(sizeof(receiver_t)))) ) {
                        return;
                    }
                    r->fd = receivers[j];
                    HASH_ADD_INT( total_receivers, fd, r );
                }

                WSS_write_internal(session, m, id);
            }

            WSS_free((void **) &receivers);
            WSS_free((void **) &msg);

            msg = NULL;
            msg_length = 0;
            fragmented = false;
        }
    }

    HASH_ITER(hh, total_receivers, r, tmp) {
        if ( unlikely(r != NULL) ) {
            HASH_DEL(total_receivers, r);

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
    char buffer[server->config->size_buffer];
    struct epoll_event event;
    session_t *session;
    message_t *message;
    unsigned int i;
    unsigned int message_length;
    unsigned int bytes_sent;
    unsigned int sending;
    size_t len, off;
    bool closing = false;

    if ( unlikely(NULL == (session = WSS_session_find(fd))) ) {
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

    WSS_log(CLIENT_TRACE, "Starting initial steps to write to client", __FILE__, __LINE__);

    session->state = WRITING;

    memset(&event, 0, sizeof(event));

    WSS_log(CLIENT_TRACE, "Popping messages from ringbuffer", __FILE__, __LINE__);

    while ( likely(0 != (len = ringbuf_consume(session->ringbuf, &off))) ) {
        for (i = 0; likely(i < len); i++) {
            if ( unlikely(session->handshaked && closing) ) {
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

                memset(buffer, '\0', server->config->size_buffer);
                sending = MIN(server->config->size_buffer, message_length-bytes_sent);
                memcpy(buffer, message->msg+bytes_sent, sending);

#ifdef USE_OPENSSL
                if (NULL != server->ssl_ctx) {
                    unsigned long err;

                    n = SSL_write(session->ssl, buffer, sending);
                    err = SSL_get_error(session->ssl, n);

                    // If something more needs to be read in order for the handshake to finish
                    if (err == SSL_ERROR_WANT_READ) {
                        WSS_log(CLIENT_DEBUG, "SSL_ERROR_WANT_READ", __FILE__, __LINE__);

                        session->written = bytes_sent;
                        ringbuf_release(session->ringbuf, i);

                        WSS_read_notify(server, session->fd);
                        return;
                    }

                    // If something more needs to be written in order for the handshake to finish
                    if (err == SSL_ERROR_WANT_WRITE) {
                        WSS_log(CLIENT_DEBUG, "SSL_ERROR_WANT_WRITE", __FILE__, __LINE__);

                        session->written = bytes_sent;

                        ringbuf_release(session->ringbuf, i);

                        WSS_write_notify(server, session->fd, false);
                        return;
                    }

                    if ( unlikely(err != SSL_ERROR_NONE && err != SSL_ERROR_ZERO_RETURN) ) {
                        char msg[1024];
                        ERR_error_string_n(err, msg, 1024);
                        WSS_log(CLIENT_ERROR, msg, __FILE__, __LINE__);

                        while ( (err = ERR_get_error()) != 0 ) {
                            ERR_error_string_n(err, msg, 1024);
                            WSS_log(CLIENT_ERROR, msg, __FILE__, __LINE__);
                        }

                        WSS_diconnect_internal(server, session);

                        return;
                    } else {
                        if (unlikely(n < 0)) {
                            n = 0;
                        }
                        bytes_sent += n;
                    }
                } else {
#endif
                    n = write(session->fd, buffer, sending);
                    if (unlikely(n == -1)) {
                        if ( unlikely(errno != EAGAIN && errno != EWOULDBLOCK) ) {
                            WSS_log(CLIENT_ERROR, strerror(errno), __FILE__, __LINE__);
                            WSS_diconnect_internal(server, session);
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

    WSS_log(CLIENT_TRACE, "Done writing to filedescriptor.", __FILE__, __LINE__);

    session->state = STALE;

    if (closing) {
        WSS_log(CLIENT_TRACE, "Closing connection, since closing frame has been sent.", __FILE__, __LINE__);
        WSS_diconnect_internal(server, session);
    } else {
        WSS_log(CLIENT_TRACE, "Set epoll file descriptor to read mode", __FILE__, __LINE__);

        WSS_read_notify(server, session->fd);
    }
}
