#ifndef wss_session_h
#define wss_session_h

#if !defined(uthash_malloc) || !defined(uthash_free)
#include "alloc.h"

#ifndef uthash_malloc
#define uthash_malloc(sz) WSS_malloc(sz)   /* malloc fcn */
#endif

#ifndef uthash_free
#define uthash_free(ptr,sz) WSS_free((void **)&ptr) /* free fcn */
#endif
#endif

#include <time.h>
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
#include "message.h"
#include "error.h"

typedef enum {
    NONE,
    READ,
    WRITE,
} wss_session_event_t;

typedef enum {
    IDLE,
    READING,
    WRITING,
    CLOSING,
    CONNECTING
} wss_session_state_t;

typedef struct {
    // The file descriptor of the session
    int fd;
    // The port of the session
    int port;
    // The IP of the session
    char *ip;
    // Whether session has been WSS handshaked
    bool handshaked;
#ifdef USE_OPENSSL
    // The ssl object used to communicate with session
    SSL *ssl;
    // Whether session has been SSL handshaked
    bool ssl_connected;
#endif
    // Whether the session is closing
    bool closing;
    // Whether the session has begun disconnecting
    bool disconnecting;
    // Lock that ensures disconnecting check is done atomically
    pthread_mutex_t lock_disconnecting;
    // Lock that ensures only one thread can perform IO at a time
    pthread_mutex_t lock;
    // Attributes for the lock
    pthread_mutexattr_t lock_attr;
    // Jobs to be performed
    int jobs;
    // Lock that ensures jobs are incremented/decremented atomically
    pthread_mutex_t lock_jobs;
    // Conditional variable that ensures that close call is only performed when no other IO is waiting
    pthread_cond_t cond_jobs;
    // Which state the session is currently in
    wss_session_state_t state;
    // Which event the session should continue listening for
    wss_session_event_t event;
    // The HTTP header of the session
    wss_header_t *header;
    // A ringbuffer containing references to the messages that the session shall receive
    ringbuf_t *ringbuf;
    // The actual messages
    wss_message_t **messages;
    // The size the messages/ringbuffer
    int messages_count;
    // Store the lastest activity of the session
    struct timespec alive;
    // Store pong application data if a ping was sent to the session
    char *pong;
    // Length of the pong application data
    unsigned int pong_length;
    // If not all data was read, store the payload temporarily
    char *payload;
    // The size of the temporarily payload
    size_t payload_length;
    // The offset into the payload where the next frame should be read from
    size_t offset;
    // If not all frames was read, store the frames temporarily
    wss_frame_t **frames;
    // The size of the temporarily frames
    size_t frames_length;
    // If not all data was written, store many bytes currently written
    unsigned int written;
    // Used for session hash table
    UT_hash_handle hh;
} wss_session_t;

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
 * @param 	fd 		[int] 		        "The filedescriptor associated to the session"
 * @param 	ip 		[char *] 	        "The ip-address of the session"
 * @param 	port 	[int] 	            "The port"
 * @return 		    [wss_session_t *] 	"Returns session if successful, otherwise NULL"
 */
wss_session_t *WSS_session_add(int fd, char* ip, int port);

/**
 * Function that frees the allocated memory and removes the session from the 
 * hashtable but without locking the hashtable. This should only be used in
 * conjunction with the WSS_session_all call.
 *
 * @param 	session	[wss_session_t *] 	"The session to be deleted"
 * @return 		    [wss_error_t] 	    "The error status"
 */
wss_error_t WSS_session_delete_no_lock(wss_session_t *session);

/**
 * Function that frees the allocated memory and removes the session from the 
 * hashtable.
 *
 * @param 	session	[wss_session_t *] 	"The session to be deleted"
 * @return 		    [wss_error_t] 	    "The error status"
 */
wss_error_t WSS_session_delete(wss_session_t *session);

/**
 * Function that frees the allocated memory and deletes all sessions.
 *
 * @return  [wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_delete_all();

/**
 * Function that finds all sessions and calls callback for each of them.
 *
 * @param   callback       [void (*callback)(wss_session_t *, void *)]    "A callback to be called for each session"
 * @return                 [wss_error_t] 	                              "The error status"
 */
wss_error_t WSS_session_all(void (*callback)(wss_session_t *));

/**
 * Function that finds a session using the filedescriptor of the session.
 *
 * @param 	fd 	[int] 		        "The filedescriptor associated to some session"
 * @return 		[wss_session_t *] 	"Returns session if successful, otherwise NULL"
 */
wss_session_t *WSS_session_find(int fd);

/**
 * Function that increments the job counter atomically.
 *
 * @param   session   [wss_session_t *]  "The session"
 * @return  [wss_error_t] 	"The error status"
 */
wss_error_t WSS_session_jobs_inc(wss_session_t *session);

/**
 * Function that decrements the job counter atomically and signals the
 * condition if no jobs are left.
 *
 * @param   session   [wss_session_t *]  "The session"
 * @return            [wss_error_t] 	 "The error status"
 */
wss_error_t WSS_session_jobs_dec(wss_session_t *session);

/**
 * Function that waits for all jobs to be done.
 *
 * @param   session   [wss_session_t *]  "The session"
 * @return            [wss_error_t]  	 "The error status"
 */
wss_error_t WSS_session_jobs_wait(wss_session_t *session);

/**
 * Function that determines atomically if session is already disconnecting.
 *
 * @param   session   [wss_session_t *]  "The session"
 * @param   dc        [bool *]           "Whether session is already disconnecting or not"
 *
 * @return       [wss_error_t]  "The error status"
 */
wss_error_t WSS_session_is_disconnecting(wss_session_t *session, bool *dc);

#endif
