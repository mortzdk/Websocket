#ifndef wss_subprotocol_echo_h
#define wss_subprotocol_echo_h

#include <stdlib.h>
#include "subprotocol.h"

/**
 * Event called when subprotocol is initialized.
 *
 * @param 	config	    [char *]            "The configuration of the subprotocol"
 * @param 	send        [WSS_send]          "Function that send message to a single recipient"
 * @return 	            [void]
 */
void __attribute__((visibility("default"))) onInit(char *config, WSS_send send);

/**
 * Sets the allocators to use instead of the default ones
 *
 * @param 	f_malloc	[void *(*f_malloc)(size_t)]             "The malloc function"
 * @param 	f_realloc	[void *(*f_realloc)(void *, size_t)]    "The realloc function"
 * @param 	f_free	    [void *(*f_free)(void *)]               "The free function"
 * @return 	            [void]
 */
void __attribute__((visibility("default"))) setAllocators(void *(*f_malloc)(size_t), void *(*f_realloc)(void *, size_t), void (*f_free)(void *));

/**
 * Event called when a new session has handshaked and hence connects to the WSS server.
 *
 * @param 	fd	[int]     "A filedescriptor of a connecting session"
 * @return 	    [void]
 */
void __attribute__((visibility("default"))) onConnect(int fd);

/**
 * Event called when a session has received new data.
 *
 * @param 	fd	            [int]     "A filedescriptor of the session receiving the data"
 * @param 	message	        [char *]  "The message received"
 * @param 	message_length	[size_t]  "The length of the message"
 * @param 	receivers	    [int **]  "A pointer which should be filled with the filedescriptors of those who should receive this message"
 * @param 	receiver_count	[size_t]  "The amount of receivers"
 * @return 	                [void]
 */
void __attribute__((visibility("default"))) onMessage(int fd, wss_opcode_t opcode, char *message, size_t message_length);

/**
 * Event called when a session are about to perform a write.
 *
 * @param 	fd	            [int]     "A filedescriptor the session about to receive the message"
 * @param 	message	        [char *]  "The message that should be sent"
 * @param 	message_length	[size_t]  "The length of the message"
 * @return 	                [void]
 */
void __attribute__((visibility("default"))) onWrite(int fd, char *message, size_t message_length);

/**
 * Event called when a session disconnects from the WSS server.
 *
 * @param 	fd	[int]     "A filedescriptor of the disconnecting session"
 * @return 	    [void]
 */
void __attribute__((visibility("default"))) onClose(int fd);

/**
 * Event called when the subprotocol should be destroyed.
 *
 * @return 	    [void]
 */
void __attribute__((visibility("default"))) onDestroy();

#endif
