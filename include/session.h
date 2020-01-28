#ifndef wss_session_h
#define wss_session_h

#include <stdbool.h>
#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#endif
#include <pthread.h> 			/* pthread_create, pthread_t, pthread_attr_t
                                   pthread_mutex_init */
#include "uthash.h"
#include "header.h"
#include "ringbuf.h"
#include "frame.h"
#include "error.h"

typedef enum {
    STALE,
    READING,
    WRITING,
    CLOSING,
    CONNECTING
} session_state_t;

/**
 * Structure stored in ringbuffer
 */
typedef struct {
    size_t length;
    char *msg;
    bool framed;
} message_t;

typedef struct {
    int fd;
    int port;
    char *ip;
    bool handshaked;
#ifdef USE_OPENSSL
    SSL *ssl;
    bool ssl_connected;
#endif
    session_state_t state;
    header_t *header;
    ringbuf_t *ringbuf;
    message_t **messages;
    int messages_count;
    char *payload;
    size_t payload_length;
    size_t offset;
    frame_t **frames;
    size_t frames_length;
    unsigned int written;
    UT_hash_handle hh;
} session_t;

/**
 * Function that initialize a mutex lock.
 *
 * @return 		[wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_init_lock();

/**
 * Function that destroy a mutex lock.
 *
 * @return 		[wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_destroy_lock();

/**
 * Function that allocates and creates a new session.
 *
 * @param 	fd 		[int] 		    "The filedescriptor associated to the session"
 * @param 	ip 		[char *] 	    "The ip-address of the session"
 * @param 	port 	[int] 	        "The port"
 * @return 		    [session_t *] 	"Returns session if successful, otherwise NULL"
 */
session_t *WSS_session_add(int fd, char* ip, int port);

/**
 * Function that frees the allocated memory and removes the session from the 
 * hashtable.
 *
 * @param 	session	[session_t *] 	"The session to be deleted"
 * @return 		    [wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_delete(session_t *session);

/**
 * Function that frees the allocated memory and deletes all sessions.
 *
 * @return  [wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_delete_all();

/**
 * Function that finds a session using the filedescriptor of the session.
 *
 * @param 	fd 	[int] 		    "The filedescriptor associated to some session"
 * @return 		[session_t *] 	"Returns session if successful, otherwise NULL"
 */
session_t *WSS_session_find(int fd);

#endif
