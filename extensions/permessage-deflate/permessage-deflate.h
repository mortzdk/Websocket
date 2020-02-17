#ifndef wss_extension_permessage_deflate_h
#define wss_extension_permessage_deflate_h

#include <stdlib.h>
#include <stdbool.h>

/**
 * Event called when extension is initialized.
 *
 * @param 	config	[char *]     "The configuration of the extension"
 * @return 	        [void]
 */
void __attribute__((visibility("default"))) onInit(char *config);

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
void __attribute__((visibility("default"))) onOpen(int fd, char *param, char **accepted, bool *valid);

/**
 * Event called when a frame_t of data is received.
 *
 * @param 	fd	    [int]        "The filedescriptor of the session"
 * @param 	frame	[void *]     "A pointer to a frame_t structure received"
 * @return 	        [void]
 */
void __attribute__((visibility("default"))) inFrame(int fd, void *frame);

/**
 * Event called when a full set of frames are received.
 *
 * @param 	fd	      [int]      "The filedescriptor of the session"
 * @param 	frames	  [char **]  "The frames received"
 * @param 	len	      [size_t]   "The amount of frames"
 * @return 	          [void]
 */
void __attribute__((visibility("default"))) inFrames(int fd, void **frames, size_t len);

/**
 * Event called when a frame_t of data is about to be sent.
 *
 * @param 	fd	    [int]        "The filedescriptor of the session"
 * @param 	frame	[void *]     "A pointer to a frame_t structure received"
 * @return 	        [void]
 */
void __attribute__((visibility("default"))) outFrame(int fd, void *frame);

/**
 * Event called when a full set of frames are about to be sent.
 *
 * @param 	fd	      [int]      "The filedescriptor of the session"
 * @param 	frames	  [char **]  "The frames received"
 * @param 	len	      [size_t]   "The amount of frames"
 * @return 	          [void]
 */
void __attribute__((visibility("default"))) outFrames(int fd, void **frames, size_t len);

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
