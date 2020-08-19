#include <stddef.h>             /* size_t */
#include <string.h>             /* strerror */
#include <errno.h> 				/* errno */
#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <unistd.h>             /* close */
#include <stdio.h>              /* close */

#include <pthread.h> 			/* pthread_create, pthread_t, pthread_attr_t
                                   pthread_rwlock_init */

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
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
wss_session_t * volatile sessions = NULL;

/**
 * A lock that ensures the hash table is update atomically
 */
pthread_rwlock_t lock;

/**
 * Function that initialize a rwlock lock.
 *
 * @return 		[wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_init_lock() {
    int err;
    if ( unlikely((err = pthread_rwlock_init(&lock, NULL)) != 0) ) {
        WSS_log_error("Unable to initialize session lock: %s", strerror(err));
        return WSS_SESSION_LOCK_CREATE_ERROR;
    }
    return WSS_SUCCESS;
}

/**
 * Function that destroy a rwlock lock.
 *
 * @return 		[wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_destroy_lock() {
    int err;
    if ( unlikely((err = pthread_rwlock_destroy(&lock)) != 0) ) {
        WSS_log_error("Unable to destroy session lock: %s", strerror(err));
        return WSS_SESSION_LOCK_DESTROY_ERROR;
    }
    return WSS_SUCCESS;
}

/**
 * Function that allocates and creates a new session.
 *
 * @param 	fd 		[int] 		        "The filedescriptor associated to the session"
 * @param 	ip 		[char *] 	        "The ip-address of the session"
 * @param 	port 	[int] 	            "The port"
 * @return 		    [wss_session_t *] 	"Returns session if successful, otherwise NULL"
 */
wss_session_t *WSS_session_add(int fd, char* ip, int port) {
    int length, err;
    wss_session_t *session = NULL;

    WSS_log_trace("Adding session");

    if ( unlikely(NULL != (session = WSS_session_find(fd))) ) {
        return NULL;
    }

    if ( unlikely((err = pthread_rwlock_wrlock(&lock)) != 0) ) {
        WSS_log_error("Unable to lock session lock: %s", strerror(err));
        return NULL;
    }

    if ( unlikely(NULL == (session = (wss_session_t *) WSS_malloc(sizeof(wss_session_t)))) ) {
        WSS_log_error("Unable to allocate memory for session");
        pthread_rwlock_unlock(&lock);
        return NULL;
    }

    pthread_mutexattr_init(&session->lock_attr);
    
    if ( unlikely((err = pthread_mutexattr_settype(&session->lock_attr, PTHREAD_MUTEX_RECURSIVE)) != 0) ) {
        WSS_log_error("Unable to initialize session locks attributes: %s", strerror(err));
        WSS_free((void **) &session);
    }

    if ( unlikely((err = pthread_mutex_init(&session->lock, &session->lock_attr)) != 0) ) {
        WSS_log_error("Unable to initialize session lock: %s", strerror(err));
        pthread_mutexattr_destroy(&session->lock_attr);
        WSS_free((void **) &session);
        pthread_rwlock_unlock(&lock);
        return NULL;
    }

    if ( unlikely((err = pthread_mutex_init(&session->lock_jobs, NULL)) != 0) ) {
        WSS_log_error("Unable to initialize session job lock: %s", strerror(err));
        pthread_mutex_destroy(&session->lock);
        pthread_mutexattr_destroy(&session->lock_attr);
        WSS_free((void **) &session);
        pthread_rwlock_unlock(&lock);
        return NULL;
    }

    if ( unlikely((err = pthread_mutex_init(&session->lock_disconnecting, NULL)) != 0) ) {
        WSS_log_error("Unable to initialize session disconnecting lock: %s", strerror(err));
        pthread_mutex_destroy(&session->lock);
        pthread_mutex_destroy(&session->lock_jobs);
        pthread_mutexattr_destroy(&session->lock_attr);
        WSS_free((void **) &session);
        pthread_rwlock_unlock(&lock);
        return NULL;
    }

    session->fd = fd;
    session->port = port;
    session->header = NULL;

    length = strlen(ip);
    if ( unlikely(NULL == (session->ip = (char *) WSS_malloc( (length+1)*sizeof(char)))) ) {
        WSS_log_error("Unable to allocate memory for IP");
        pthread_mutex_destroy(&session->lock);
        pthread_mutex_destroy(&session->lock_jobs);
        pthread_mutex_destroy(&session->lock_disconnecting);
        pthread_mutexattr_destroy(&session->lock_attr);
        WSS_free((void **) &session);
        pthread_rwlock_unlock(&lock);
        return NULL;
    }

    memcpy(session->ip, ip, length);

    HASH_ADD_INT(sessions, fd, session);

    pthread_rwlock_unlock(&lock);

    return session;
}

static wss_error_t session_delete(wss_session_t *session) {
    int i, err = WSS_SUCCESS;
    size_t j;

    if ( likely(NULL != session) ) {
        pthread_mutex_unlock(&session->lock);
        if ( unlikely((err = pthread_mutex_destroy(&session->lock)) != 0) ) {
            err = WSS_SESSION_LOCK_DESTROY_ERROR;
        }

        if ( unlikely((err = pthread_mutexattr_destroy(&session->lock_attr)) != 0) ) {
            err = WSS_SESSION_LOCK_DESTROY_ERROR;
        }

        if ( unlikely((err = pthread_mutex_destroy(&session->lock_jobs)) != 0) ) {
            err = WSS_SESSION_LOCK_DESTROY_ERROR;
        }

        if ( unlikely((err = pthread_mutex_destroy(&session->lock_disconnecting)) != 0) ) {
            err = WSS_SESSION_LOCK_DESTROY_ERROR;
        }

        WSS_log_trace("Free ip string");
        WSS_free((void **) &session->ip);

        WSS_log_trace("Free pong string");
        WSS_free((void **) &session->pong);

        WSS_log_trace("Free payload");
        WSS_free((void **) &session->payload);

        WSS_log_trace("Free frames");
        for (j = 0; likely(j < session->frames_length); j++) {
            WSS_free((void **) &session->frames[j]);
        }
        WSS_free((void **) &session->frames);

        WSS_log_trace("Free session header structure");
        if ( likely(NULL != session->header) ) {
            for (j = 0; j < session->header->ws_extensions_count; j++) {
                session->header->ws_extensions[j]->ext->close(session->fd);
            }

            session->header->ws_protocol->close(session->fd);

            WSS_free_header(session->header);
        }

        WSS_log_trace("Free ringbuf");
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
        WSS_log_trace("Free ssl structure");
        if (NULL != session->ssl) {
            if ( unlikely((err = SSL_shutdown(session->ssl)) < 0) ) {
                err = SSL_get_error(session->ssl, err);
                switch (err) {
                    case SSL_ERROR_WANT_READ:
                        pthread_rwlock_unlock(&lock);
                        return WSS_SSL_SHUTDOWN_READ_ERROR;
                    case SSL_ERROR_WANT_WRITE:
                        pthread_rwlock_unlock(&lock);
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
        }
#endif

        WSS_log_trace("Deleting client from hash table");

        HASH_DEL(sessions, session);

        WSS_log_trace("Closing client filedescriptor");
        if ( unlikely(close(session->fd) < 0) ) {
            WSS_log_error("Unable to close clients filedescriptor: %s", strerror(err));
            err = WSS_SOCKET_CLOSE_ERROR;
        }

        WSS_log_trace("Free session structure");
        WSS_free((void **) &session);
    }

    return WSS_SUCCESS;
}

/**
 * Function that frees the allocated memory and removes the session from the 
 * hashtable but without locking the hashtable. This should only be used in
 * conjunction with the WSS_session_all call.
 *
 * @param 	session	[wss_session_t *] 	"The session to be deleted"
 * @return 		    [wss_error_t] 	    "The error status"
 */
wss_error_t WSS_session_delete_no_lock(wss_session_t *session) {
    wss_error_t err;

    WSS_log_trace("Deleting session");

    err = session_delete(session);

    return err;
}

/**
 * Function that frees the allocated memory and removes the session from the 
 * hashtable.
 *
 * @param 	session	[wss_session_t *] 	"The session to be deleted"
 * @return 		    [wss_error_t] 	    "The error status"
 */
wss_error_t WSS_session_delete(wss_session_t *session) {
    wss_error_t err;

    WSS_log_trace("Deleting session");

    if ( unlikely((err = pthread_rwlock_wrlock(&lock)) != 0) ) {
        WSS_log_error("Unable to lock session lock: %s", strerror(err));
        return WSS_SESSION_LOCK_ERROR;
    }

    err = session_delete(session);

    pthread_rwlock_unlock(&lock);

    return err;
}

/**
 * Function that frees the allocated memory and deletes all sessions.
 *
 * @return  [wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_delete_all() {
    wss_error_t err, tmp_err;
    wss_session_t *session, *tmp;

    WSS_log_trace("Deleting all sessions");

    if ( unlikely((err = pthread_rwlock_wrlock(&lock)) != 0) ) {
        WSS_log_error("Unable to lock session lock: %s", strerror(err));
        return WSS_SESSION_LOCK_ERROR;
    }

    err = WSS_SUCCESS;

    HASH_ITER(hh, sessions, session, tmp) {
        tmp_err = session_delete(session);
        if ( likely(err == WSS_SUCCESS) ) {
            err = tmp_err;
        }
    }
    pthread_rwlock_unlock(&lock);

    return err;
}

/**
 * Function that increments the job counter atomically.
 *
 * @param   session   [wss_session_t *]  "The session"
 * @return  [wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_jobs_inc(wss_session_t *session) {
    wss_error_t err;

    if ( unlikely((err = pthread_mutex_lock(&session->lock_jobs)) != 0) ) {
        WSS_log_error("Unable to lock session jobs lock: %s", strerror(err));
        return WSS_SESSION_LOCK_ERROR;
    }
    session->jobs++;

    WSS_log_trace("session %d incremented to %d jobs", session->fd, session->jobs);

    pthread_mutex_unlock(&session->lock_jobs);

    return err;
}

/**
 * Function that decrements the job counter atomically and signals the
 * condition if no jobs are left.
 *
 * @param   session   [wss_session_t *]  "The session"
 * @return            [wss_error_t] 	 "The error status"
 */
wss_error_t WSS_session_jobs_dec(wss_session_t *session) {
    wss_error_t err;

    if ( unlikely((err = pthread_mutex_lock(&session->lock_jobs)) != 0) ) {
        WSS_log_error("Unable to lock session jobs lock: %s", strerror(err));
        return WSS_SESSION_LOCK_ERROR;
    }
    session->jobs--;

    WSS_log_trace("session %d decremented to %d jobs", session->fd, session->jobs);

    if (session->jobs == 0) {
        pthread_cond_signal(&session->cond_jobs);
    }

    pthread_mutex_unlock(&session->lock_jobs);

    return err;
}

/**
 * Function that waits for all jobs to be done.
 *
 * @param   session   [wss_session_t *]  "The session"
 * @return            [wss_error_t]  	 "The error status"
 */
wss_error_t WSS_session_jobs_wait(wss_session_t *session) {
    wss_error_t err;

    if ( unlikely((err = pthread_mutex_lock(&session->lock_jobs)) != 0) ) {
        WSS_log_error("Unable to lock session jobs lock: %s", strerror(err));
        return WSS_SESSION_LOCK_ERROR;
    }

    WSS_log_trace("session %d waiting to close", session->fd);

    // Wait for all jobs on session to be finished
    while (session->jobs > 0) {
        pthread_cond_wait(&session->cond_jobs, &session->lock_jobs);
    }

    pthread_mutex_unlock(&session->lock_jobs);

    return err;
}

/**
 * Function that determines atomically if session is already disconnecting.
 *
 * @param   session   [wss_session_t *]  "The session"
 * @param   dc        [bool *]           "Whether session is already disconnecting or not"
 *
 * @return       [wss_error_t]  "The error status"
 */
wss_error_t WSS_session_is_disconnecting(wss_session_t *session, bool *dc) {
    wss_error_t err;

    *dc = true;

    if ( unlikely((err = pthread_mutex_lock(&session->lock_disconnecting)) != 0) ) {
        WSS_log_error("Unable to lock session disconnecting lock: %s", strerror(err));
        return WSS_SESSION_LOCK_ERROR;
    }

    if (! session->disconnecting) {
        *dc = false;
        session->disconnecting = true;
    }

    pthread_mutex_unlock(&session->lock_disconnecting);

    return err;
}

/**
 * Function that finds all sessions and calls callback for each of them.
 *
 * @param   callback       [void (*callback)(wss_session_t *)]    "A callback to be called for each session"
 * @return                 [wss_error_t] 	                      "The error status"
 */
wss_error_t WSS_session_all(void (*callback)(wss_session_t *)) {
    int err;
    wss_session_t *session, *tmp;

    WSS_log_trace("Finding all sessions");

    if ( unlikely((err = pthread_rwlock_rdlock(&lock)) != 0) ) {
        WSS_log_error("Unable to lock session lock: %s", strerror(err));
        return WSS_SESSION_LOCK_ERROR;
    }

    HASH_ITER(hh, sessions, session, tmp) {
        if ( likely(NULL != session) ) {
            callback(session);
        }
    }

    pthread_rwlock_unlock(&lock);

    return WSS_SUCCESS;
}

/**
 * Function that finds a session using the filedescriptor of the session.
 *
 * @param 	fd 	[int] 		        "The filedescriptor associated to some session"
 * @return 		[wss_session_t *] 	"Returns session if successful, otherwise NULL"
 */
wss_session_t *WSS_session_find(int fd) {
    int err;
    wss_session_t *session = NULL;

    WSS_log_trace("Finding session");

    if ( unlikely((err = pthread_rwlock_rdlock(&lock)) != 0) ) {
        WSS_log_error("Unable to lock session lock", strerror(err));
        return NULL;
    }

    HASH_FIND_INT(sessions, &fd, session);

    pthread_rwlock_unlock(&lock);

    return session;
}
