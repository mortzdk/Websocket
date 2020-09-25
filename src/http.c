#include <errno.h> 				/* errno */
#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen */
#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <stdbool.h>            
#include <math.h> 				/* log10 */
#include <unistd.h> 			/* close */
#include <regex.h>              /* regex_t, regcomp, regexec */

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#endif

#include <sys/socket.h>         /* socket, setsockopt, inet_ntoa, accept */
#include <netinet/in.h>         /* sockaddr_in, inet_ntoa */
#include <arpa/inet.h>          /* htonl, htons, inet_ntoa */

#include "http.h"
#include "alloc.h"
#include "config.h"
#include "error.h"
#include "log.h"
#include "socket.h"
#include "predict.h"

#ifdef USE_OPENSSL
/**
 * Function initializes SSL context that can be used to serve over https.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_http_ssl(wss_server_t *server) {
    FILE *f;
    DH *dh;
    const SSL_METHOD *method;
    int error_size = 1024;
    char error[error_size];

    SSL_library_init();
    FIPS_mode_set(1);
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    OpenSSL_add_all_algorithms();

    method = TLS_method();
    server->ssl_ctx = SSL_CTX_new(method);
    if ( unlikely(!server->ssl_ctx) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to create ssl context: %s", error);
        return WSS_SSL_CTX_ERROR;
    }

    WSS_log_trace("Assign CA certificate(s) to ssl context");
    if ( unlikely(!SSL_CTX_load_verify_locations(server->ssl_ctx, server->config->ssl_ca_file, server->config->ssl_ca_path)) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load CA certificates: %s", error);
        return WSS_SSL_CA_ERROR;
    }

    WSS_log_trace("Assign certificate to ssl context");
    if ( unlikely(SSL_CTX_use_certificate_file(server->ssl_ctx, server->config->ssl_cert, SSL_FILETYPE_PEM) <= 0) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load server certificate: %s", error);
        return WSS_SSL_CERT_ERROR;
    }

    WSS_log_trace("Assign private key to ssl context");
    if ( unlikely(SSL_CTX_use_PrivateKey_file(server->ssl_ctx, server->config->ssl_key, SSL_FILETYPE_PEM) <= 0) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load server private key: %s", error);
        return WSS_SSL_KEY_ERROR;
    }

    WSS_log_trace("Validate private key");
    if ( unlikely(!SSL_CTX_check_private_key(server->ssl_ctx)) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Failed check of private key: %s", error);
        return WSS_SSL_KEY_ERROR;
    }

    WSS_log_trace("Use most appropriate client curve");
    SSL_CTX_set_options(server->ssl_ctx, SSL_OP_SINGLE_ECDH_USE);
    SSL_CTX_set_ecdh_auto(server->ssl_ctx, 1);

    if (! server->config->ssl_peer_cert) {
        SSL_CTX_set_verify(server->ssl_ctx, SSL_VERIFY_NONE, 0);
    } else {
        SSL_CTX_set_verify(server->ssl_ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);
    }

    WSS_log_trace("Allow writes to be partial");
    SSL_CTX_set_mode(server->ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);

    //WSS_log_trace("Allow write buffer to be moving as it is allocated on the heap");
    //SSL_CTX_set_mode(server->ssl_ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

    WSS_log_trace("Allow read and write buffers to be released when they are no longer needed");
    SSL_CTX_set_mode(server->ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

    if (! server->config->ssl_compression) {
        WSS_log_trace("Do not use compression even if it is supported.");
        SSL_CTX_set_mode(server->ssl_ctx, SSL_OP_NO_COMPRESSION);
    }

    WSS_log_trace("When choosing a cipher, use the server's preferences instead of the client preferences.");
    SSL_CTX_set_mode(server->ssl_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

    WSS_log_trace("Disable use of session and ticket cache and resumption");
    SSL_CTX_set_session_cache_mode(server->ssl_ctx, SSL_SESS_CACHE_OFF);
    SSL_CTX_set_options(server->ssl_ctx, SSL_OP_NO_TICKET);
    if (! SSL_CTX_set_num_tickets(server->ssl_ctx, 0)) {
        WSS_log_error("Failed to set number of ticket to zero");
        return WSS_SSL_CTX_ERROR;
    }
    SSL_CTX_set_options(server->ssl_ctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

    if ( NULL != server->config->ssl_cipher_list ) {
        WSS_log_trace("Setting cipher list");
        SSL_CTX_set_cipher_list(server->ssl_ctx, server->config->ssl_cipher_list);
    }

    if ( NULL != server->config->ssl_cipher_suites ) {
        WSS_log_trace("Setting cipher suites");
        SSL_CTX_set_ciphersuites(server->ssl_ctx, server->config->ssl_cipher_suites);
    }

    if ( NULL != server->config->ssl_dhparam ) {
        if ( NULL != (f = fopen(server->config->ssl_dhparam, "r")) ) {
            if ( NULL != (dh = PEM_read_DHparams(f, NULL, NULL, NULL)) ) {
                SSL_CTX_set_options(server->ssl_ctx, SSL_OP_SINGLE_DH_USE);
                if ( SSL_CTX_set_tmp_dh(server->ssl_ctx, dh) == 0 ) {
                    ERR_error_string_n(ERR_get_error(), error, error_size);
                    WSS_log_error("Setting dhparam failed: %s", error);
                }
                DH_free(dh);
            } else {
                ERR_error_string_n(ERR_get_error(), error, error_size);
                WSS_log_error("Unable load dhparam: %s", error);
            }

            fclose(f);
        } else {
            WSS_log_error("Unable to open dhparam file: %s", strerror(errno));
        }
    }

    return WSS_SUCCESS;
}
#endif

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
    sum_host_length += MAX(config->hosts_length-1, 0);

    for (j = 0; j < config->paths_length; j++) {
        sum_path_length += strlen(config->paths[j]); 
    }
    sum_path_length += MAX(config->paths_length-1, 0);

    for (k = 0; k < config->queries_length; k++) {
        sum_query_length += strlen(config->queries[k]); 
    }
    sum_query_length += MAX(config->queries_length-1, 0);

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

    WSS_log_error("Require_uri: %s", request_uri);

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
    bool ssl = false;

#ifdef USE_OPENSSL
    if (NULL != server->ssl_ctx) {
        ssl = true;
    }
#endif

    request_uri = generate_request_uri(server->config, ssl, server->port);
    if ( likely(request_uri != NULL) ) {
        if ( NULL == (server->re = WSS_malloc(sizeof(regex_t)))) {
            return WSS_MEMORY_ERROR;
        }

        if ( unlikely((err = regcomp(server->re, request_uri, REG_EXTENDED|REG_NOSUB)) != 0) ) {
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

    WSS_log_trace("Creating threadpool");
    if ( unlikely((err = WSS_socket_threadpool(server)) != WSS_SUCCESS) ) {
        return err;
    }

    WSS_log_trace("Initializing server regexp");
    if ( unlikely((err = WSS_http_regex_init(server)) != WSS_SUCCESS) ) {
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
            if ( unlikely(shutdown(server->fd, SHUT_RD) != 0) ) {
                WSS_log_error("Unable to shutdown for reads of server socket: %s", strerror(errno));
                res = WSS_SOCKET_SHUTDOWN_ERROR;
            }
        }

        /**
         * Shutting down threadpool gracefully
         */
        if ( likely(NULL != server->pool) ) {
            if ( unlikely((err = threadpool_destroy(server->pool, threadpool_graceful)) != 0) ) {
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

        if ( NULL != server->re ) {
            regfree(server->re);
            WSS_free((void **) &server->re);
        }

        /**
         * Freeing epoll structures
         */
        WSS_free((void **) &server->events);

        /**
         * Closing epoll
         */
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
        if ( likely(server->fd > -1)) {
            if ( unlikely(close(server->fd) != 0) ) {
                WSS_log_error("Unable to close servers filedescriptor: %s", strerror(errno));
                res = WSS_SOCKET_CLOSE_ERROR;
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
