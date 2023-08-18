#pragma once

#ifndef WSS_SSL_H
#define WSS_SSL_H

#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)

#include <openssl/sha.h>

#elif defined(USE_WOLFSSL)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include <wolfssl/wolfcrypt/sha.h>
#pragma GCC diagnostic pop

#define SHA_DIGEST_LENGTH 20

#else

#define SHA_DIGEST_LENGTH 20

#endif

#include <stdbool.h>
#include <pthread.h> 			/* pthread_create, pthread_t, pthread_attr_t
                                   pthread_rwlock_init */

#include "error.h"
#include "server.h"
#include "session.h"

/**
 * Function initializes SSL context that can be used to serve over https.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_http_ssl(wss_server_t *server);

/**
 * Function frees SSL context that was used to serve over https.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @return 			[void]
 */
void WSS_http_ssl_free(wss_server_t *server);

/**
 * Function that performs a ssl handshake with the connecting client.
 *
 * @param   server      [wss_server_t *]    "The server implementation"
 * @param   session     [wss_session_t *]   "The connecting client session"
 * @return              [void]
 */
void WSS_ssl_handshake(wss_server_t *server, wss_session_t *session);

/**
 * Function that performs a ssl read from the connecting client.
 *
 * @param   server      [wss_server_t *]    "The server implementation"
 * @param   session     [wss_session_t *]   "The connecting client session"
 * @param   buffer      [char *]            "The buffer to use"
 * @return              [int]
 */
int WSS_ssl_read(wss_server_t *server, wss_session_t *session, char *buffer);

/**
 * Function that performs a ssl write to the connecting client.
 *
 * @param   session       [wss_session_t *]   "The connecting client session"
 * @param   message_index [unsigned int]      "The message index"
 * @param   message       [wss_message_t *]   "The message"
 * @param   bytes_sent    [unsigned int *]    "Pointer to the amount of bytes currently sent"
 * @return                [bool]
 */
bool WSS_ssl_write_partial(wss_session_t *session, unsigned int message_index, wss_message_t *message, unsigned int* bytes_sent);

/**
 * Function that performs a ssl write to the connecting client.
 *
 * @param   session        [wss_session_t *]   "The connecting client session"
 * @param   message        [char *]            "The message"
 * @param   message_length [size_t]            "The message length"
 * @return                 [void]
 */
void WSS_ssl_write(wss_session_t *session, char *message, unsigned int message_length);

/**
 * Function initializes SSL session instance that can be used to serve over https.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @param   session	[wss_session_t *] 	"The session instance"
 * @return 			[bool]              "Whether function was successful"
 */
bool WSS_session_ssl(wss_server_t *server, wss_session_t *session);

/**
 * Function frees SSL session instance that was used to serve over https.
 *
 * @param   session	[wss_session_t *] 	    "The session instance"
 * @param   lock	[pthread_rwlock_t *] 	"The read/write session lock"
 * @return 			[wss_error_t]           "An error or success"
 */
wss_error_t WSS_session_ssl_free(wss_session_t *session, pthread_rwlock_t *lock);

/**
 * Function creates a sha1 hash of the key.
 *
 * @param   key	        [char *] 	            "The key to be hashed"
 * @param   key_length	[pthread_rwlock_t *] 	"The length of the key"
 * @param   hash	    [char **] 	            "A pointer to where the hash should be stored"
 * @return 	[size_t]                            "The length of the hash"
 */
size_t WSS_sha1(char *key, size_t key_length, char **hash);

/**
 * Function creates a sha1 hash of the key and base64 encodes it.
 *
 * @param   key	        [char *] 	            "The key to be hashed"
 * @param   key_length	[pthread_rwlock_t *] 	"The length of the key"
 * @param   accept_key  [char **] 	            "A pointer to where the hash should be stored"
 * @return 			    [size_t]                "Length of accept_key"
 */
size_t WSS_base64_encode_sha1(char *key, size_t key_length, char **accept_key);

#endif
