#include <stdlib.h>
#include <pthread.h>

#include "broadcast.h"
#include "core.h"

/**
 * A hashtable containing all active sessions
 */
wss_client_t * volatile clients = NULL;

/**
 * Structure containing allocators
 */
typedef struct {
    void *(*malloc)(size_t);
    void *(*realloc)(void *, size_t);
    void (*free)(void *);
} allocators;

/**
 * Global allocators
 */
allocators allocs = {
    malloc,
    realloc,
    free
};

WSS_send send = NULL;

/**
 * A lock that ensures the hash table is update atomically
 */
pthread_rwlock_t lock;

/**
 * Event called when subprotocol is initialized.
 *
 * @param 	config	    [char *]            "The configuration of the subprotocol"
 * @param 	s	        [WSS_send]          "Function that send message to a single recipient"
 * @return 	            [void]
 */
void onInit(char *config, WSS_send s) {
    WSS_UNUSED(config);
    send = s;
    return;
}

/**
 * Sets the allocators to use instead of the default ones
 *
 * @param 	submalloc	[WSS_malloc_t]     "The malloc function"
 * @param 	subrealloc	[WSS_realloc_t]    "The realloc function"
 * @param 	subfree	    [WSS_free_t]       "The free function"
 * @return 	            [void]
 */
void setAllocators(WSS_malloc_t submalloc, WSS_realloc_t subrealloc, WSS_free_t subfree) {
    allocs.malloc = submalloc;
    allocs.realloc = subrealloc;
    allocs.free = subfree;
}

/**
 * Function that finds a client using the filedescriptor of the client.
 *
 * @param 	fd 	[int] 		        "The filedescriptor associated to some client"
 * @return 		[wss_session_t *] 	"Returns client if successful, otherwise NULL"
 */
inline static wss_client_t *WSS_client_find(int fd) {
    wss_client_t *client = NULL;

    if ( unlikely(pthread_rwlock_rdlock(&lock) != 0) ) {
        return NULL;
    }

    HASH_FIND_INT(clients, &fd, client);

    pthread_rwlock_unlock(&lock);

    return client;
}

/**
 * Event called when a new client has handshaked and hence connects to the WSS server.
 *
 * @param 	fd	     [int]     "A filedescriptor of a connecting client"
 * @param 	ip       [char *]  "The ip address of the connecting session"
 * @param 	port     [int]     "The port of the connecting session"
 * @param 	path     [char *]  "The connection path. This can hold HTTP parameters such as access_token, csrf_token etc. that can be used to authentication"
 * @param 	cookies  [char *]  "The cookies received from the client. This can be used to do authentication."
 * @return 	         [void]
 */
void onConnect(int fd, char *ip, int port, char *path, char *cookies) {
    WSS_UNUSED(ip);
    WSS_UNUSED(port);
    WSS_UNUSED(path);
    WSS_UNUSED(cookies);

    wss_client_t *client;

    if ( unlikely(NULL != (client = WSS_client_find(fd))) ) {
        return;
    }

    if ( unlikely(pthread_rwlock_wrlock(&lock) != 0) ) {
        return;
    }

    if ( unlikely(NULL == (client = (wss_client_t *) allocs.malloc(sizeof(wss_client_t)))) ) {
        pthread_rwlock_unlock(&lock);
        return;
    }

    client->fd = fd;

    HASH_ADD_INT(clients, fd, client);

    pthread_rwlock_unlock(&lock);

    return;
}

/**
 * Event called when a client has received new data.
 *
 * @param 	fd	            [int]     "A filedescriptor of the client receiving the data"
 * @param 	message	        [char *]  "The message received"
 * @param 	message_length	[size_t]  "The length of the message"
 * @return 	                [void]
 */
void onMessage(int fd, wss_opcode_t opcode, char *message, size_t message_length) {
    wss_client_t *client, *tmp;

    if ( unlikely(pthread_rwlock_rdlock(&lock) != 0) ) {
        return;
    }

    HASH_ITER(hh, clients, client, tmp) {
        if (client->fd != fd) {
            send(client->fd, opcode, message, message_length);
        }
    }

    pthread_rwlock_unlock(&lock);
}

/**
 * Event called when a client are about to perform a write.
 *
 * @param 	fd	            [int]     "A filedescriptor the client about to receive the message"
 * @param 	message	        [char *]  "The message that should be sent"
 * @param 	message_length	[size_t]  "The length of the message"
 * @return 	                [void]
 */
void onWrite(int fd, char *message, size_t message_length) {
    WSS_UNUSED(fd);
    WSS_UNUSED(message);
    WSS_UNUSED(message_length);
    return;
}

/**
 * Event called when a client disconnects from the WSS server.
 *
 * @param 	fd	[int]     "A filedescriptor of the disconnecting client"
 * @return 	    [void]
 */
void onClose(int fd) {
    wss_client_t *client;

    if ( unlikely(NULL == (client = WSS_client_find(fd))) ) {
        return;
    }

    if ( unlikely(pthread_rwlock_wrlock(&lock) != 0) ) {
        return;
    }

    HASH_DEL(clients, client);

    allocs.free(client);

    pthread_rwlock_unlock(&lock);

    return;
}

/**
 * Event called when the subprotocol should be destroyed.
 *
 * @return 	    [void]
 */
void onDestroy() {
    wss_client_t *client, *tmp;

    if ( unlikely(pthread_rwlock_wrlock(&lock) != 0) ) {
        return;
    }

    HASH_ITER(hh, clients, client, tmp) {
        if (client != NULL) {
            HASH_DEL(clients, client);

            allocs.free(client);
        }
    }

    pthread_rwlock_unlock(&lock);
    pthread_rwlock_destroy(&lock);
}
