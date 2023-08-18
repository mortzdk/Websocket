#ifndef wss_subprotocol_echo_h
#define wss_subprotocol_echo_h

#include <stdlib.h>

#include "subprotocol.h"
#include "core.h"
#include "uthash.h"

typedef struct {
    // The file descriptor of the client
    int fd;
    // Used for client hash table
    UT_hash_handle hh;
} wss_client_t;

/**
 * Event called when subprotocol is initialized.
 *
 * @param 	config	    [char *]            "The configuration of the subprotocol"
 * @param 	send        [WSS_send]          "Function that send message to a single recipient"
 * @param 	servers     [void *]            "Server structure used internally"
 * @return 	            [void]
 */
void visible onInit(char *config, WSS_send send);

/**
 * Sets the allocators to use instead of the default ones
 *
 * @param 	submalloc	[WSS_malloc_t]     "The malloc function"
 * @param 	subrealloc	[WSS_realloc_t]    "The realloc function"
 * @param 	subfree	    [WSS_free_t]       "The free function"
 * @return 	            [void]
 */
void visible setAllocators(WSS_malloc_t submalloc, WSS_realloc_t subrealloc, WSS_free_t subfree);

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
void visible onConnect(int fd, char *ip, int port, char *path, char *cookies);

/**
 * Event called when a client has received new data.
 *
 * @param 	fd	            [int]     "A filedescriptor of the client receiving the data"
 * @param 	message	        [char *]  "The message received"
 * @param 	message_length	[size_t]  "The length of the message"
 * @return 	                [void]
 */
void visible onMessage(int fd, wss_opcode_t opcode, char *message, size_t message_length);

/**
 * Event called when a client are about to perform a write.
 *
 * @param 	fd	            [int]     "A filedescriptor the client about to receive the message"
 * @param 	message	        [char *]  "The message that should be sent"
 * @param 	message_length	[size_t]  "The length of the message"
 * @return 	                [void]
 */
void visible onWrite(int fd, char *message, size_t message_length);

/**
 * Event called when a client disconnects from the WSS server.
 *
 * @param 	fd	[int]     "A filedescriptor of the disconnecting client"
 * @return 	    [void]
 */
void visible onClose(int fd);

/**
 * Event called when the subprotocol should be destroyed.
 *
 * @return 	    [void]
 */
void visible onDestroy();

#endif
