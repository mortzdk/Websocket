#include <errno.h> 				/* errno */
#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen */
#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <math.h> 				/* log10 */
#include <unistd.h> 			/* close */

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <sys/socket.h>         /* socket, setsockopt, inet_ntoa, accept */
#include <netinet/in.h>         /* sockaddr_in, inet_ntoa */
#include <arpa/inet.h>          /* htonl, htons, inet_ntoa */

#include "http.h"
#include "alloc.h"
#include "error.h"
#include "log.h"
#include "socket.h"
#include "predict.h"

#ifdef USE_OPENSSL
/**
 * Function initializes SSL context that can be used to serve over https.
 *
 * @param   server	[server_t *] 	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t http_ssl(server_t *server) {
    const SSL_METHOD *method;
    int error_size = 1024;
    char error[error_size];

    SSL_library_init();
    FIPS_mode_set(1);
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    OpenSSL_add_all_algorithms();

    method = SSLv23_server_method();
    server->ssl_ctx = SSL_CTX_new(method);
    if ( unlikely(!server->ssl_ctx) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to create ssl context: %s", error);
        return SSL_ERROR;
    }

    SSL_CTX_set_ecdh_auto(server->ssl_ctx, 1);

    if ( unlikely(!SSL_CTX_load_verify_locations(server->ssl_ctx, server->config->ssl_ca_file, server->config->ssl_ca_path)) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load CA certificates: %s", error);
        return SSL_ERROR;
    }

    if ( unlikely(SSL_CTX_use_certificate_file(server->ssl_ctx, server->config->ssl_cert, SSL_FILETYPE_PEM) <= 0) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load server certificate: %s", error);
        return SSL_ERROR;
    }

    if ( unlikely(SSL_CTX_use_PrivateKey_file(server->ssl_ctx, server->config->ssl_key, SSL_FILETYPE_PEM) <= 0) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load server private key: %s", error);
        return SSL_ERROR;
    }

    if ( unlikely(!SSL_CTX_check_private_key(server->ssl_ctx)) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Failed check of private key: %s", error);
        return SSL_ERROR;
    }

    SSL_CTX_set_verify(server->ssl_ctx, SSL_VERIFY_NONE, 0);

    return SUCCESS;
}
#endif

/**
 * Function initialized a http server instance and creating thread where the
 * instance is being run.
 *
 * @param   server	[server_t *] 	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t http_server(server_t *server) {
    int err;

#ifdef USE_OPENSSL
    if (NULL == server->ssl_ctx) {
#endif
        WSS_log_trace("Starting HTTP instance");
#ifdef USE_OPENSSL
    } else {
        WSS_log_trace("Starting HTTPS instance");
    }
#endif

    WSS_log_trace("Assigning server to port %d", server->port);

    /**
     * Setting port and initializes filedescriptors to -1 in the server
     * structure.
     */
    server->fd = -1;
    server->epoll_fd = -1;

    WSS_log_trace("Creating socket filedescriptor");
    if ( unlikely((err = socket_create(server)) != 0) ) {
        return err;
    }

    WSS_log_trace("Allowing reuse of socket");
    if ( unlikely((err = socket_reuse(server->fd)) != 0) ) {
        return err;
    }

    WSS_log_trace("Creating socket structure for instance");
    if ( unlikely((err = socket_bind(server)) != 0) ) {
        return err;
    }

    WSS_log_trace("Binding address of server to: ", inet_ntoa(server->info.sin_addr), server->port);

    WSS_log_trace("Making server socket non-blocking");
    if ( unlikely((err = socket_non_blocking(server->fd)) != 0) ) {
        return err;
    }

    WSS_log_trace("Starts listening to the server socket");
    if ( unlikely((err = socket_listen(server->fd)) != 0) ) {
        return err;
    }

    WSS_log_trace("Creating epoll instance and associating it with the filedescriptor");
    if ( unlikely((err = socket_epoll(server)) != 0) ) {
        return err;
    }

    WSS_log_trace("Creating threadpool");
    if ( unlikely((err = socket_threadpool(server)) != 0) ) {
        return err;
    }

    WSS_log_trace("Creating server thread");
    if ( unlikely(pthread_create(&server->thread_id, NULL, server_run, (void *) server) != 0) ) {
        WSS_log_error("Unable to create server thread", strerror(errno));
        return THREAD_ERROR;
    }

    return SUCCESS;
}

/**
 * Function that free op space allocated for the http server and closes the
 * filedescriptors in use..
 *
 * @param   server	[server_t *] 	"The http server"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t http_server_free(server_t *server) {
    unsigned int i;
    int res = SUCCESS;

    if ( likely(NULL != server) ) {
        /**
         * Shutting down socket, such that no more reads is allowed.
         */
        if ( likely(server->fd > -1) ) {
            if ( unlikely(shutdown(server->fd, SHUT_RD) != 0) ) {
                WSS_log_error("Unable to shutdown for reads of server socket: %s", strerror(errno));
                res = FD_ERROR;
            }
        }

        /**
         * Shutting down threadpool gracefully
         */
        if ( likely(NULL != server->pool) ) {
            for (i = 0; likely(i < server->config->pool_queues); i++) {
                if (unlikely(threadpool_destroy(server->pool[i], threadpool_graceful) != 0) ) {
                    WSS_log_error("Unable to destroy threadpool gracefully: %s", threadpool_strerror(errno));
                    res = THREADPOOL_ERROR;
                }
            }
            WSS_free((void **) &server->pool);
        }

        /**
         * Freeing epoll structures
         */
        WSS_free((void **) &server->events);

        /**
         * Closing epoll
         */
        if ( likely(server->epoll_fd > -1) ) {
            if ( unlikely(close(server->epoll_fd) != 0) ) {
                WSS_log_error("Unable to close servers epoll filedescriptor: %s", strerror(errno));
                res = EPOLL_ERROR;
            }

            server->epoll_fd = -1;
        }

        /**
         * Closing socket
         */
        if ( likely(server->fd > -1)) {
            if ( unlikely(close(server->fd) != 0) ) {
                WSS_log_error("Unable to close servers filedescriptor: %s", strerror(errno));
                res = FD_ERROR;
            }
            server->fd = -1;
        }

#ifdef USE_OPENSSL
        if (NULL != server->ssl_ctx) {
            SSL_CTX_free(server->ssl_ctx);
            FIPS_mode_set(0);
            EVP_cleanup();
            CRYPTO_cleanup_all_ex_data();
            ERR_free_strings();
        }
#endif
    }

    return res;
}
