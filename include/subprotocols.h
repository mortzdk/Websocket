#ifndef wss_subprotocols_h
#define wss_subprotocols_h

#include "uthash.h"

#define PATH_LENGTH 256

/**
 * The subprotocol API calls
 */
typedef void (*onConnect)(int fd);
typedef void (*onMessage)(int fd, char *message, size_t message_length, int **receivers, size_t *receiver_count);
typedef void (*onWrite)(int fd, char *message, size_t message_length);
typedef void (*onClose)(int fd);

typedef struct {
    int *handle;
    char *name;
    onConnect connect; 
    onMessage message;
    onWrite write; 
    onClose close;
    UT_hash_handle hh;
} wss_subprotocol_t;

/**
 * Function that loads subprotocol implementations into memory, by loading a
 * shared object by the name of the folder it lies in.
 *
 * E.g.
 *
 * subprotocols/echo/echo.so 
 *
 * @param 	path	[const char *] 	"The path to the subprotocols folder"
 * @return 	      	[void]
 */
void load_subprotocols(const char *path);

/**
 * Function that looks for a subprotocol implementation of the name given.
 *
 * @param 	name	[char *] 	            "The name of the subprotocol"
 * @return 	      	[wss_subprotocol_t *]   "The subprotocol or NULL"
 */
wss_subprotocol_t *find_subprotocol(char *name);

/**
 * Destroys all memory used to load and store the subprotocols 
 *
 * @return 	[void]
 */
void destroy_subprotocols();

/**
 * Global hashtable of subprotocols
 */
wss_subprotocol_t *subprotocols;

#endif
