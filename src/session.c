#include <stddef.h>             /* size_t */
#include <string.h>             /* strerror */
#include <errno.h> 				/* errno */
#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <unistd.h>             /* close */
#include <stdio.h>             /* close */

#include <pthread.h> 			/* pthread_create, pthread_t, pthread_attr_t
                                   pthread_mutex_init */

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#endif

#include "session.h"
#include "log.h"
#include "error.h"
#include "alloc.h"
#include "header.h"
#include "ringbuf.h"
#include "predict.h"

/**
 * A hashtable containing all active sessions
 */
session_t * volatile sessions = NULL;

/**
 * A lock that ensures the hash table is update atomically
 */
pthread_mutex_t lock;

/**
 * Function that initialize a mutex lock.
 *
 * @return 		[wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_init_lock() {
    int err;
    if ( unlikely((err = pthread_mutex_init(&lock, NULL)) != 0) ) {
        WSS_log(SERVER_ERROR, strerror(err), __FILE__, __LINE__);
        return LOCK_ERROR;
    }
    return SUCCESS;
}

/**
 * Function that destroy a mutex lock.
 *
 * @return 		[wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_destroy_lock() {
    int err;
    if ( unlikely((err = pthread_mutex_destroy(&lock)) != 0) ) {
        WSS_log(SERVER_ERROR, strerror(err), __FILE__, __LINE__);
        return LOCK_ERROR;
    }
    return SUCCESS;
}

/**
 * Function that allocates and creates a new session.
 *
 * @param 	fd 		[int] 		    "The filedescriptor associated to the session"
 * @param 	ip 		[char *] 	    "The ip-address of the session"
 * @param 	port 	[int] 	        "The port"
 * @return 		    [session_t *] 	"Returns session if successful, otherwise NULL"
 */
session_t *WSS_session_add(int fd, char* ip, int port) {
    int length, err;
    session_t *session = NULL;

    if ( unlikely(NULL != (session = WSS_session_find(fd))) ) {
        return NULL;
    }

    if ( unlikely((err = pthread_mutex_lock(&lock)) != 0) ) {
        WSS_log(SERVER_ERROR, strerror(err), __FILE__, __LINE__);
        return NULL;
    }

    if ( unlikely(NULL == (session = (session_t *) WSS_malloc(sizeof(session_t)))) ) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    session->fd = fd;
    session->port = port;
    session->header = NULL;

    length = strlen(ip);
    if ( unlikely(NULL == (session->ip = (char *) WSS_malloc( (length+1)*sizeof(char)))) ) {
        WSS_free((void **) &session);
        WSS_log(SERVER_ERROR, "Unable to allocate memory for IP", __FILE__, __LINE__);
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    memcpy(session->ip, ip, length);

    HASH_ADD_INT(sessions, fd, session);

    pthread_mutex_unlock(&lock);

    return session;
}

/**
 * Function that frees the allocated memory and removes the session from the 
 * hashtable.
 *
 * @param 	session	[session_t *] 	"The session to be deleted"
 * @return 		    [wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_delete(session_t *session) {
    int i, err = 0;
    size_t j;

    WSS_log(CLIENT_TRACE, "Locking hash table", __FILE__, __LINE__);

    if ( unlikely((err = pthread_mutex_lock(&lock)) != 0) ) {
        WSS_log(SERVER_ERROR, strerror(err), __FILE__, __LINE__);
        return LOCK_ERROR;
    }

    if ( likely(NULL != session) ) {
        WSS_log(CLIENT_TRACE, "Deleting client from hash table", __FILE__, __LINE__);

        HASH_DEL(sessions, session);

        WSS_log(CLIENT_TRACE, "Free ip string", __FILE__, __LINE__);
        WSS_free((void **) &session->ip);

        WSS_log(CLIENT_TRACE, "Free session header structure", __FILE__, __LINE__);
        if ( likely(NULL != session->header) ) {
            for (j = 0; j < session->header->ws_extensions_count; j++) {
                session->header->ws_extensions[j]->ext->close(session->fd);
            }
            WSS_free_header(session->header);
        }

        WSS_log(CLIENT_TRACE, "Free ringbuf", __FILE__, __LINE__);
        for (i = 0; likely(i < session->messages_count); i++) {
            if ( likely(NULL != session->messages[i]) ) {
                if ( likely(NULL != session->messages[i]->msg) ) {
                    WSS_free((void **) &session->messages[i]->msg);
                }
                WSS_free((void **) &session->messages[i]);
            }
        }
        WSS_free((void **) &session->messages);
        WSS_free((void **) &session->ringbuf);

#ifdef USE_OPENSSL
        WSS_log(CLIENT_TRACE, "Free ssl structure", __FILE__, __LINE__);
        if (NULL != session->ssl) {
            if ( unlikely(SSL_shutdown(session->ssl) < 0) ) {
                err = SSL_ERROR;
            }
            SSL_free(session->ssl);
            session->ssl = NULL;
        }
#endif

        WSS_log(CLIENT_TRACE, "Closing client filedescriptor", __FILE__, __LINE__);
        if ( unlikely(close(session->fd) < 0) ) {
            WSS_log(SERVER_ERROR, strerror(err), __FILE__, __LINE__);
            err = FD_ERROR;
        }

        WSS_log(CLIENT_TRACE, "Free session structure", __FILE__, __LINE__);
        WSS_free((void **) &session);
    }

    pthread_mutex_unlock(&lock);

    WSS_log(CLIENT_TRACE, "Unlocked hash table", __FILE__, __LINE__);

    return err;
}

/**
 * Function that frees the allocated memory and deletes all sessions.
 *
 * @return  [wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_delete_all() {
    int i, err = 0;
    size_t j;
    session_t *session, *tmp;

    if ( unlikely((err = pthread_mutex_lock(&lock)) != 0) ) {
        WSS_log(SERVER_ERROR, strerror(err), __FILE__, __LINE__);
        return LOCK_ERROR;
    }

    err = SUCCESS;

    HASH_ITER(hh, sessions, session, tmp) {
        if ( likely(NULL != session) ) {
            HASH_DEL(sessions, session);

            WSS_free((void **) &session->ip);

            if ( likely(NULL != session->header) ) {
                for (j = 0; j < session->header->ws_extensions_count; j++) {
                    session->header->ws_extensions[j]->ext->close(session->fd);
                }
                WSS_free_header(session->header);
            }

            for (i = 0; likely(i < session->messages_count); i++) {
                if ( likely(NULL != session->messages[i]) ) {
                    if ( likely(NULL != session->messages[i]->msg) ) {
                        WSS_free((void **) &session->messages[i]->msg);
                    }
                    WSS_free((void **) &session->messages[i]);
                }
            }
            WSS_free((void **) &session->messages);
            WSS_free((void **) &session->ringbuf);

#ifdef USE_OPENSSL
            if (NULL != session->ssl) {
                if ( unlikely(SSL_shutdown(session->ssl) < 0) ) {
                    err = SSL_ERROR;
                }
                SSL_free(session->ssl);
                session->ssl = NULL;
            }
#endif

            if ( likely(close(session->fd) < 0) ) {
                WSS_log(SERVER_ERROR, strerror(errno), __FILE__, __LINE__);
                err = FD_ERROR;
            }

            WSS_free((void **) &session);
        }
    }
    pthread_mutex_unlock(&lock);

    return err;
}

/**
 * Function that finds a session using the filedescriptor of the session.
 *
 * @param 	fd 	[int] 		    "The filedescriptor associated to some session"
 * @return 		[session_t *] 	"Returns session if successful, otherwise NULL"
 */
session_t *WSS_session_find(int fd) {
    int err;
    session_t *session = NULL;

    if ( unlikely((err = pthread_mutex_lock(&lock)) != 0) ) {
        WSS_log(SERVER_ERROR, strerror(err), __FILE__, __LINE__);
        return NULL;
    }

    HASH_FIND_INT(sessions, &fd, session);

    pthread_mutex_unlock(&lock);

    return session;
}
