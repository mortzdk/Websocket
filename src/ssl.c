#if defined(USE_OPENSSL) | defined(USE_LIBRESSL)

#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/buffer.h>

#elif defined(USE_BORINGSSL)

#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include "b64.h"

#elif defined(USE_WOLFSSL)

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include "b64.h"

#else

#include "sha1.h"
#include "b64.h"

#endif

#ifndef SSL_SUCCESS
#define SSL_SUCCESS 1
#endif

#include <errno.h>

#include "worker.h"
#include "event.h"
#include "ssl.h"
#include "log.h"
#include "predict.h"

/**
 * Function initializes SSL context that can be used to serve over https.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_http_ssl(wss_server_t *server) {
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    FILE *f;
    DH *dh;
    const SSL_METHOD *method;
    int error_size = 1024;
    char error[error_size];

    SSL_library_init();

#if defined(USE_OPENSSL)
    FIPS_mode_set(1);
#endif

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
    if ( unlikely(SSL_SUCCESS != SSL_CTX_load_verify_locations(server->ssl_ctx, server->config->ssl_ca_file, server->config->ssl_ca_path)) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load CA certificates: %s", error);
        return WSS_SSL_CA_ERROR;
    }

    WSS_log_trace("Assign certificate to ssl context");
    if ( unlikely(SSL_SUCCESS != SSL_CTX_use_certificate_file(server->ssl_ctx, server->config->ssl_cert, SSL_FILETYPE_PEM)) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load server certificate: %s", error);
        return WSS_SSL_CERT_ERROR;
    }

    WSS_log_trace("Assign private key to ssl context");
    if ( unlikely(SSL_SUCCESS != SSL_CTX_use_PrivateKey_file(server->ssl_ctx, server->config->ssl_key, SSL_FILETYPE_PEM)) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load server private key: %s", error);
        return WSS_SSL_KEY_ERROR;
    }

    WSS_log_trace("Validate private key");
    if ( unlikely(SSL_SUCCESS != SSL_CTX_check_private_key(server->ssl_ctx)) ) {
        ERR_error_string_n(ERR_get_error(), error, error_size);
        WSS_log_error("Failed check of private key: %s", error);
        return WSS_SSL_KEY_ERROR;
    }

    WSS_log_trace("Use most appropriate client curve");
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL)
    SSL_CTX_set_options(server->ssl_ctx, SSL_OP_SINGLE_ECDH_USE);
#endif

#if defined(USE_OPENSSL) | defined(USE_LIBRESSL)
    SSL_CTX_set_ecdh_auto(server->ssl_ctx, 1);
#endif

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

#if defined(USE_OPENSSL) | defined(USE_BORINGSSL)
    SSL_CTX_set_options(server->ssl_ctx, SSL_OP_NO_TICKET);
#endif

#if defined(USE_OPENSSL)
    if ( SSL_SUCCESS != SSL_CTX_set_num_tickets(server->ssl_ctx, 0) ) {
        WSS_log_error("Failed to set number of ticket to zero");
        return WSS_SSL_CTX_ERROR;
    }
#endif

#if defined(USE_OPENSSL) | defined(USE_BORINGSSL)
    SSL_CTX_set_options(server->ssl_ctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#endif

    if ( NULL != server->config->ssl_cipher_list ) {
        WSS_log_trace("Setting cipher list");
        SSL_CTX_set_cipher_list(server->ssl_ctx, server->config->ssl_cipher_list);
    }

    if ( NULL != server->config->ssl_cipher_suites ) {
#if defined(USE_OPENSSL)
        WSS_log_trace("Setting cipher suites");
        SSL_CTX_set_ciphersuites(server->ssl_ctx, server->config->ssl_cipher_suites);
#else
        if ( NULL != server->config->ssl_cipher_list ) {
            char list[strlen(server->config->ssl_cipher_list)+strlen(server->config->ssl_cipher_list)+2];
            memset(list, '\0', strlen(server->config->ssl_cipher_list)+strlen(server->config->ssl_cipher_list)+2);
            memcpy(list, server->config->ssl_cipher_list, strlen(server->config->ssl_cipher_list));
            list[strlen(server->config->ssl_cipher_list)] = ':';
            memcpy(list+(strlen(server->config->ssl_cipher_list)+1), server->config->ssl_cipher_suites, strlen(server->config->ssl_cipher_suites));
            SSL_CTX_set_cipher_list(server->ssl_ctx, list);
        } else {
            SSL_CTX_set_cipher_list(server->ssl_ctx, server->config->ssl_cipher_suites);
        }

        WSS_log_trace("Setting cipher suites");
#endif
    }

    if ( NULL != server->config->ssl_dhparam ) {
        if ( NULL != (f = fopen(server->config->ssl_dhparam, "r")) ) {
            if ( NULL != (dh = PEM_read_DHparams(f, NULL, NULL, NULL)) ) {
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL)
                SSL_CTX_set_options(server->ssl_ctx, SSL_OP_SINGLE_DH_USE);
#endif
                if ( SSL_SUCCESS != SSL_CTX_set_tmp_dh(server->ssl_ctx, dh) ) {
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
#elif defined(USE_WOLFSSL)
    WOLFSSL_METHOD *method;
    int error_size = 1024;
    char error[error_size];

    wolfSSL_Init();

    method = TLS_method();
    server->ssl_ctx = wolfSSL_CTX_new(method);
    if ( unlikely(!server->ssl_ctx) ) {
        wolfSSL_ERR_error_string_n(wolfSSL_ERR_get_error(), error, error_size);
        WSS_log_error("Unable to create ssl context: %s", error);
        return WSS_SSL_CTX_ERROR;
    }

    WSS_log_trace("Assign CA certificate(s) to ssl context");
    if ( unlikely(SSL_SUCCESS != wolfSSL_CTX_load_verify_locations(server->ssl_ctx, server->config->ssl_ca_file, server->config->ssl_ca_path)) ) {
        wolfSSL_ERR_error_string_n(wolfSSL_ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load CA certificates: %s", error);
        return WSS_SSL_CA_ERROR;
    }

    WSS_log_trace("Assign certificate to ssl context");
    if ( unlikely(SSL_SUCCESS != wolfSSL_CTX_use_certificate_file(server->ssl_ctx, server->config->ssl_cert, SSL_FILETYPE_PEM)) ) {
        wolfSSL_ERR_error_string_n(wolfSSL_ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load server certificate: %s", error);
        return WSS_SSL_CERT_ERROR;
    }

    WSS_log_trace("Assign private key to ssl context");
    if ( unlikely(SSL_SUCCESS != wolfSSL_CTX_use_PrivateKey_file(server->ssl_ctx, server->config->ssl_key, SSL_FILETYPE_PEM)) ) {
        wolfSSL_ERR_error_string_n(wolfSSL_ERR_get_error(), error, error_size);
        WSS_log_error("Unable to load server private key: %s", error);
        return WSS_SSL_KEY_ERROR;
    }

    WSS_log_trace("Validate private key");
    if ( unlikely(SSL_SUCCESS != wolfSSL_CTX_check_private_key(server->ssl_ctx)) ) {
        wolfSSL_ERR_error_string_n(wolfSSL_ERR_get_error(), error, error_size);
        WSS_log_error("Failed check of private key: %s", error);
        return WSS_SSL_KEY_ERROR;
    }

    WSS_log_trace("Use most appropriate client curve");
    wolfSSL_CTX_set_options(server->ssl_ctx, SSL_OP_SINGLE_ECDH_USE);

    if (! server->config->ssl_peer_cert) {
        wolfSSL_CTX_set_verify(server->ssl_ctx, SSL_VERIFY_NONE, 0);
    } else {
        wolfSSL_CTX_set_verify(server->ssl_ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);
    }

    WSS_log_trace("Allow writes to be partial");
    wolfSSL_CTX_set_mode(server->ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);

    //WSS_log_trace("Allow write buffer to be moving as it is allocated on the heap");
    //wolfSSL_CTX_set_mode(server->ssl_ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

    WSS_log_trace("Allow read and write buffers to be released when they are no longer needed");
    wolfSSL_CTX_set_mode(server->ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

    if (! server->config->ssl_compression) {
        WSS_log_trace("Do not use compression even if it is supported.");
        wolfSSL_CTX_set_mode(server->ssl_ctx, SSL_OP_NO_COMPRESSION);
    }

    WSS_log_trace("When choosing a cipher, use the server's preferences instead of the client preferences.");
    wolfSSL_CTX_set_mode(server->ssl_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

    WSS_log_trace("Disable use of session and ticket cache and resumption");
    wolfSSL_CTX_set_session_cache_mode(server->ssl_ctx, SSL_SESS_CACHE_OFF);
    wolfSSL_CTX_set_options(server->ssl_ctx, SSL_OP_NO_TICKET);
    wolfSSL_CTX_set_options(server->ssl_ctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

    if ( NULL != server->config->ssl_cipher_list ) {
        WSS_log_trace("Setting cipher list");
        wolfSSL_CTX_set_cipher_list(server->ssl_ctx, server->config->ssl_cipher_list);
    }

    if ( NULL != server->config->ssl_cipher_suites ) {
        if ( NULL != server->config->ssl_cipher_list ) {
            char list[strlen(server->config->ssl_cipher_list)+strlen(server->config->ssl_cipher_list)+2];
            memset(list, '\0', strlen(server->config->ssl_cipher_list)+strlen(server->config->ssl_cipher_list)+2);
            memcpy(list, server->config->ssl_cipher_list, strlen(server->config->ssl_cipher_list));
            list[strlen(server->config->ssl_cipher_list)] = ':';
            memcpy(list+(strlen(server->config->ssl_cipher_list)+1), server->config->ssl_cipher_suites, strlen(server->config->ssl_cipher_suites));
            SSL_CTX_set_cipher_list(server->ssl_ctx, list);
        } else {
            SSL_CTX_set_cipher_list(server->ssl_ctx, server->config->ssl_cipher_suites);
        }
        WSS_log_trace("Setting cipher suites");
    }

    if ( NULL != server->config->ssl_dhparam ) {
        if ( SSL_SUCCESS != wolfSSL_SetTmpDH_file(server->ssl_ctx, server->config->ssl_dhparam, SSL_FILETYPE_PEM) ) {
            wolfSSL_ERR_error_string_n(wolfSSL_ERR_get_error(), error, error_size);
            WSS_log_error("Setting dhparam failed: %s", error);
        }
    }

    return WSS_SUCCESS;
#else
    return WSS_SSL_CTX_ERROR;
#endif
}

/**
 * Function frees SSL context that was used to serve over https.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @return 			[void]
 */
void WSS_http_ssl_free(wss_server_t *server) {
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    SSL_CTX_free(server->ssl_ctx);

#if defined(USE_OPENSSL)
    FIPS_mode_set(0);
#endif

    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    ERR_free_strings();
#elif defined(USE_WOLFSSL)
    wolfSSL_CTX_free(server->ssl_ctx);
    wolfSSL_Cleanup();
#endif
}

/**
 * Function that performs a ssl read from the connecting client.
 *
 * @param   server      [wss_server_t *]    "The server implementation"
 * @param   session     [wss_session_t *]   "The connecting client session"
 * @param   buffer      [char *]            "The buffer to use"
 * @return              [int]
 */
int WSS_ssl_read(wss_server_t *server, wss_session_t *session, char *buffer) {
#if defined(USE_OPENSSL) | defined(USE_WOLFSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    int n;
    unsigned long err;
    
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    n = SSL_read(session->ssl, buffer, server->config->size_buffer);
    err = SSL_get_error(session->ssl, n);
#elif defined(USE_WOLFSSL)
    n = wolfSSL_read(session->ssl, buffer, server->config->size_buffer);
    err = SSL_get_error(session->ssl, n);
#endif
    
    // There's no more to read from the kernel
    if ( unlikely(err == SSL_ERROR_WANT_READ) ) {
        n = 0;
    } else
        
    // There's no space to write to the kernel, wait for filedescriptor.
    if ( unlikely(err == SSL_ERROR_WANT_WRITE) ) {
        WSS_log_debug("SSL_ERROR_WANT_WRITE");
    
        n = -2;
    } else
        
    // Some error has occured.
    if ( unlikely(err != SSL_ERROR_NONE && err != SSL_ERROR_ZERO_RETURN) ) {
        char msg[1024];
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
        while ( (err = ERR_get_error()) != 0 ) {
            memset(msg, '\0', 1024);
            ERR_error_string_n(err, msg, 1024);
            WSS_log_error("SSL read failed: %s", msg);
        }
#elif defined(USE_WOLFSSL)
        while ( (err = wolfSSL_ERR_get_error()) != 0 ) {
            memset(msg, '\0', 1024);
            wolfSSL_ERR_error_string_n(err, msg, 1024);
            WSS_log_error("SSL read failed: %s", msg);
        }
#endif
    
        n = -1;
    } else 
        
    // If this was one of the error codes that we do accept, return 0
    if ( unlikely(n < 0) ) {
        n = 0;
    }
    
    return n;
#else
    return 0;
#endif
}

/**
 * Function that performs a ssl write to the connecting client.
 *
 * @param   session       [wss_session_t *]   "The connecting client session"
 * @param   message_index [unsigned int]      "The message index"
 * @param   message       [wss_message_t *]   "The message"
 * @param   bytes_sent    [unsigned int]      "The amount of bytes currently sent"
 * @return                [bool]
 */
bool WSS_ssl_write_partial(wss_session_t *session, unsigned int message_index, wss_message_t *message, unsigned int *bytes_sent) {
#if defined(USE_OPENSSL) | defined(USE_WOLFSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    int n;
    unsigned long err;
    unsigned int message_length = message->length;

#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    n = SSL_write(session->ssl, message->msg+*bytes_sent, message_length-*bytes_sent);
    err = SSL_get_error(session->ssl, n);
#elif defined(USE_WOLFSSL)
    n = wolfSSL_write(session->ssl, message->msg+*bytes_sent, message_length-*bytes_sent);
    err = SSL_get_error(session->ssl, n);
#endif

    // If something more needs to be read in order for the handshake to finish
    if ( unlikely(err == SSL_ERROR_WANT_READ) ) {
        WSS_log_trace("Needs to wait for further reads");

        session->written = *bytes_sent;
        ringbuf_release(session->ringbuf, message_index);

        session->event = READ;

        return false;
    }

    // If something more needs to be written in order for the handshake to finish
    if ( unlikely(err == SSL_ERROR_WANT_WRITE) ) {
        WSS_log_trace("Needs to wait for further writes");

        session->written = *bytes_sent;

        ringbuf_release(session->ringbuf, message_index);

        session->event = WRITE;

        return false;
    }

    if ( unlikely(err != SSL_ERROR_NONE && err != SSL_ERROR_ZERO_RETURN) ) {
        char msg[1024];
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
        while ( (err = ERR_get_error()) != 0 ) {
            ERR_error_string_n(err, msg, 1024);
            WSS_log_error("SSL write failed: %s", msg);
        }
#elif defined(USE_WOLFSSL)
        while ( (err = wolfSSL_ERR_get_error()) != 0 ) {
            wolfSSL_ERR_error_string_n(err, msg, 1024);
            WSS_log_error("SSL write failed: %s", msg);
        }
#endif
        session->closing = true;

        return false;
    } else {
        if (unlikely(n < 0)) {
            n = 0;
        }
        *bytes_sent += n;
    }

    return true;
#else
    return false;
#endif
}

/**
 * Function that performs a ssl write to the connecting client.
 *
 * @param   session        [wss_session_t *]   "The connecting client session"
 * @param   message        [char *]            "The message"
 * @param   message_length [size_t]            "The message length"
 * @return                 [void]
 */
void WSS_ssl_write(wss_session_t *session, char *message, unsigned int message_length) {
#if defined(USE_OPENSSL) | defined(USE_WOLFSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    int n;
    size_t written = 0;

    do {
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
        n = SSL_write(session->ssl, message+written, message_length-written);
#elif defined(USE_WOLFSSL)
        n = wolfSSL_write(session->ssl, message+written, message_length-written);
#endif
        if (n < 0) {
            break;
        }
        written += n;
    } while ( written < message_length );
#endif
}

/**
 * Function that performs a ssl handshake with the connecting client.
 *
 * @param   server      [wss_server_t *]    "The server implementation"
 * @param   session     [wss_session_t *]   "The connecting client session"
 * @return              [void]
 */
void WSS_ssl_handshake(wss_server_t *server, wss_session_t *session) {
#if defined(USE_OPENSSL) | defined(USE_WOLFSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    int ret;
    unsigned long err;

    WSS_log_trace("Performing SSL handshake");

#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    ret = SSL_do_handshake(session->ssl);
    err = SSL_get_error(session->ssl, ret);
#elif defined(USE_WOLFSSL)
    ret = wolfSSL_SSL_do_handshake(session->ssl);
    err = wolfSSL_get_error(session->ssl, ret);
#endif

    // If something more needs to be read in order for the handshake to finish
    if ( unlikely(err == SSL_ERROR_WANT_READ) ) {
        WSS_log_trace("Need to wait for further reads");

        clock_gettime(CLOCK_MONOTONIC, &session->alive);

        WSS_poll_set_read(server, session->fd);

        return;
    }

    // If something more needs to be written in order for the handshake to finish
    if ( unlikely(err == SSL_ERROR_WANT_WRITE) ) {
        WSS_log_trace("Need to wait for further writes");

        clock_gettime(CLOCK_MONOTONIC, &session->alive);

        WSS_poll_set_write(server, session->fd);

        return;
    }

    if ( unlikely(err != SSL_ERROR_NONE && err != SSL_ERROR_ZERO_RETURN) ) {
        char message[1024];
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
        while ( (err = ERR_get_error()) != 0 ) {
            memset(message, '\0', 1024);
            ERR_error_string_n(err, message, 1024);
            WSS_log_error("Handshake error: %s", message);
        }
#elif defined(USE_WOLFSSL)
        while ( (err = wolfSSL_ERR_get_error()) != 0 ) {
            memset(message, '\0', 1024);
            wolfSSL_ERR_error_string_n(err, message, 1024);
            WSS_log_error("Handshake error: %s", message);
        }
#endif

        WSS_disconnect(server, session);
        return;
    }

    WSS_log_trace("SSL handshake was successfull");

    session->ssl_connected = true;

    WSS_log_info("Client with session %d connected", session->fd);

    session->state = IDLE;

    clock_gettime(CLOCK_MONOTONIC, &session->alive);

    WSS_poll_set_read(server, session->fd);
#endif
}

/**
 * Function initializes SSL context that can be used to serve over https.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @param   session	[wss_session_t *] 	"The session instance"
 * @return 			[bool]              "Whether function was successful"
 */
bool WSS_session_ssl(wss_server_t *server, wss_session_t *session) {
#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    char ssl_msg[1024];

    WSS_log_trace("Creating ssl client structure");
    if ( unlikely(NULL == (session->ssl = SSL_new(server->ssl_ctx))) ) {
        ERR_error_string_n(ERR_get_error(), ssl_msg, 1024);
        WSS_log_error("Unable to create SSL structure: %s", ssl_msg);
        WSS_disconnect(server, session);
        return false;
    }

    WSS_log_trace("Associating structure with client filedescriptor");
    if ( unlikely(SSL_set_fd(session->ssl, session->fd) != 1) ) {
        ERR_error_string_n(ERR_get_error(), ssl_msg, 1024);
        WSS_log_error("Unable to bind filedescriptor to SSL structure: %s", ssl_msg);
        WSS_disconnect(server, session);
        return false;
    }

    WSS_log_trace("Setting accept state");
    SSL_set_accept_state(session->ssl);

    return true;
#elif defined(USE_WOLFSSL)
    char ssl_msg[1024];

    WSS_log_trace("Creating ssl client structure");
    if ( unlikely(NULL == (session->ssl = wolfSSL_new(server->ssl_ctx))) ) {
        wolfSSL_ERR_error_string_n(wolfSSL_ERR_get_error(), ssl_msg, 1024);
        WSS_log_error("Unable to create SSL structure: %s", ssl_msg);
        WSS_disconnect(server, session);
        return false;
    }

    WSS_log_trace("Associating structure with client filedescriptor");
    if ( unlikely(wolfSSL_set_fd(session->ssl, session->fd) != 1) ) {
        wolfSSL_ERR_error_string_n(wolfSSL_ERR_get_error(), ssl_msg, 1024);
        WSS_log_error("Unable to bind filedescriptor to SSL structure: %s", ssl_msg);
        WSS_disconnect(server, session);
        return false;
    }

    WSS_log_trace("Setting accept state");
    wolfSSL_set_accept_state(session->ssl);

    return true;
#else
    return false;
#endif
}

/**
 * Function frees SSL session instance that was used to serve over https.
 *
 * @param   session	[wss_session_t *] 	    "The session instance"
 * @param   lock	[pthread_rwlock_t *] 	"The read/write session lock"
 * @return 			[wss_error_t]           "An error or success"
 */
wss_error_t WSS_session_ssl_free(wss_session_t *session, pthread_rwlock_t *lock) {
    int err = WSS_SUCCESS;

#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    WSS_log_trace("Free ssl structure");
    if ( unlikely((err = SSL_shutdown(session->ssl)) < 0) ) {
        err = SSL_get_error(session->ssl, err);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                pthread_rwlock_unlock(lock);
                return WSS_SSL_SHUTDOWN_READ_ERROR;
            case SSL_ERROR_WANT_WRITE:
                pthread_rwlock_unlock(lock);
                return WSS_SSL_SHUTDOWN_WRITE_ERROR;
            case SSL_ERROR_SYSCALL:
                WSS_log_error("SSL_shutdown failed: %s", strerror(errno));
                break;
            default:
                WSS_log_error("SSL_shutdown error code: %d", err);
                break;
        }

        char message[1024];
        while ( (err = ERR_get_error()) != 0 ) {
            memset(message, '\0', 1024);
            ERR_error_string_n(err, message, 1024);
            WSS_log_error("SSL_shutdown error: %s", message);
        }

        err = WSS_SSL_SHUTDOWN_ERROR;
    }
    SSL_free(session->ssl);
    session->ssl = NULL;
#elif defined(USE_WOLFSSL)
    if ( unlikely((err = wolfSSL_shutdown(session->ssl)) < 0) ) {
        err = SSL_get_error(session->ssl, err);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                pthread_rwlock_unlock(lock);
                return WSS_SSL_SHUTDOWN_READ_ERROR;
            case SSL_ERROR_WANT_WRITE:
                pthread_rwlock_unlock(lock);
                return WSS_SSL_SHUTDOWN_WRITE_ERROR;
            case SSL_ERROR_SYSCALL:
                WSS_log_error("SSL_shutdown failed: %s", strerror(errno));
                break;
            default:
                WSS_log_error("SSL_shutdown error code: %d", err);
                break;
        }

        char message[1024];
        while ( (err = wolfSSL_ERR_get_error()) != 0 ) {
            memset(message, '\0', 1024);
            wolfSSL_ERR_error_string_n(err, message, 1024);
            WSS_log_error("SSL_shutdown error: %s", message);
        }

        err = WSS_SSL_SHUTDOWN_ERROR;
    }
    wolfSSL_free(session->ssl);
    session->ssl = NULL;
#endif

    return err;
}

/**
 * Function creates a sha1 hash of the key.
 *
 * @param   key	        [char *] 	            "The key to be hashed"
 * @param   key_length	[pthread_rwlock_t *] 	"The length of the key"
 * @param   hash	    [hash **] 	            "A pointer to where the hash should be stored"
 * @return 	[size_t]                            "The length of the hash"
 */
size_t WSS_sha1(char *key, size_t key_length, char **hash) {
    memset(*hash, '\0', SHA_DIGEST_LENGTH);

#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)
    SHA1((const unsigned char *)key, key_length, (unsigned char*) *hash);
#elif defined(USE_WOLFSSL)
    Sha sha;
    wc_InitSha(&sha);
    wc_ShaUpdate(&sha, (const unsigned char *) key, key_length);
    wc_ShaFinal(&sha, (unsigned char *)*hash);
#else
    SHA1Context sha;
    int i, b;
    memset(*hash, '\0', SHA_DIGEST_LENGTH);

    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char*) key, key_length);
    if ( likely(SHA1Result(&sha)) ) {
        for (i = 0; likely(i < 5); i++) {
            b = htonl(sha.Message_Digest[i]);
            memcpy(*hash+(4*i), (unsigned char *) &b, 4);
        }
    }
#endif

    return SHA_DIGEST_LENGTH;
}

/**
 * Function creates a sha1 hash of the key and base64 encodes it.
 *
 * @param   key	        [char *] 	            "The key to be hashed"
 * @param   key_length	[pthread_rwlock_t *] 	"The length of the key"
 * @param   accept_key  [char **] 	            "A pointer to where the hash should be stored"
 * @return 			    [size_t]                "Length of accept_key"
 */
size_t WSS_base64_encode_sha1(char *key, size_t key_length, char **accept_key) {
    size_t acceptKeyLength;
    char sha1Key[SHA_DIGEST_LENGTH];
    memset(sha1Key, '\0', SHA_DIGEST_LENGTH);

#if defined(USE_OPENSSL) | defined(USE_LIBRESSL)
    SHA1((const unsigned char *)key, key_length, (unsigned char*) sha1Key);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
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
#pragma GCC diagnostic pop

    acceptKeyLength = mem_bio_mem_ptr->length;
    if ( unlikely(NULL == (*accept_key = WSS_malloc(acceptKeyLength))) ) {
        return 0;
    }
    memcpy(*accept_key, (*mem_bio_mem_ptr).data, acceptKeyLength);

    BIO_free_all(b64_bio);                          // Destroys all BIOs in chain, starting with b64 (i.e. the 1st one).
#elif defined(USE_BORINGSSL)
    SHA1((const unsigned char *)key, key_length, (unsigned char*) sha1Key);

    *accept_key = b64_encode((const unsigned char *) sha1Key, SHA_DIGEST_LENGTH);
    acceptKeyLength = strlen(*accept_key);
#elif defined(USE_WOLFSSL)
    Sha sha;
    wc_InitSha(&sha);
    wc_ShaUpdate(&sha, (const unsigned char *) key, key_length);
    wc_ShaFinal(&sha, (unsigned char *)sha1Key);

    if ( unlikely(NULL == (*accept_key = WSS_malloc((SHA_DIGEST_LENGTH*4)/3+1))) ) {
        return 0;
    }

    *accept_key = b64_encode((const unsigned char *) sha1Key, SHA_DIGEST_LENGTH);
    acceptKeyLength = strlen(*accept_key);
#else
    SHA1Context sha;
    int i, b;

    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char*) key, key_length);
    if ( likely(SHA1Result(&sha)) ) {
        for (i = 0; likely(i < 5); i++) {
            b = htonl(sha.Message_Digest[i]);
            memcpy(sha1Key+(4*i), (unsigned char *) &b, 4);
        }
    } else {
        return 0;
    }

    *accept_key = b64_encode((const unsigned char *) sha1Key, SHA_DIGEST_LENGTH);
    acceptKeyLength = strlen(*accept_key);
#endif

    return acceptKeyLength;
}
