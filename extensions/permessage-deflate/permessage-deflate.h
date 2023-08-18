#ifndef wss_extension_permessage_deflate_h
#define wss_extension_permessage_deflate_h

#define WSS_PERMESSAGE_DEFLATE_VERSION_MAJOR 1
#define WSS_PERMESSAGE_DEFLATE_VERSION_MINOR 0
#define WSS_PERMESSAGE_DEFLATE_VERSION ((WSS_PERMESSAGE_DEFLATE_VERSION_MAJOR << 16) | WSS_PERMESSAGE_DEFLATE_VERSION_MINOR)

#include <stdlib.h>
#include <stdbool.h>
#include "extension.h"
#include "core.h"

/**
 * Event called when extension is initialized.
 *
 * @param 	config	[char *]     "The configuration of the extension"
 * @return 	        [void]
 */
void visible onInit(char *config);

/**
 * Sets the allocators to use instead of the default ones
 *
 * @param 	extmalloc	[WSS_malloc_t]     "The malloc function"
 * @param 	extrealloc	[WSS_realloc_t]    "The realloc function"
 * @param 	extfree	    [WSS_free_t]       "The free function"
 * @return 	            [void]
 */
void visible setAllocators(WSS_malloc_t extmalloc, WSS_realloc_t extrealloc, WSS_free_t extfree);

/**
 * Event called when parameters are available for the pcme i.e. when the
 * connection is opened.
 *
 * @param 	fd	        [int]        "The filedescriptor of the session"
 * @param 	param	    [char *]     "The parameters to the PCME"
 * @param 	accepted	[char *]     "The accepted parameters to the PCME"
 * @param 	valid	    [bool *]     "A pointer to a boolean, that should state whether the parameters are accepted"
 * @return 	            [void]
 */
void visible onOpen(int fd, char *param, char **accepted, bool *valid);

/**
 * Event called when a full set of frames are received.
 *
 * @param 	fd	            [int]             "The filedescriptor of the session"
 * @param 	frames	        [wss_frame_t **]  "The websocket frames received"
 * @param 	frames_count    [size_t]          "The amount of frames"
 * @return 	                [void]
 */
void visible inFrames(int fd, wss_frame_t **frames, size_t frames_count);

/**
 * Event called when a full set of frames are about to be sent.
 *
 * @param 	fd	            [int]             "The filedescriptor of the session"
 * @param 	frames	        [wss_frame_t **]  "The websocket frames received"
 * @param 	frames_count	[size_t]          "The amount of frames"
 * @return 	                [void]
 */
void visible outFrames(int fd, wss_frame_t **frames, size_t frames_count);

/**
 * Event called when a session disconnects from the WSS server.
 *
 * @param 	fd	[int]     "A filedescriptor of the disconnecting session"
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
