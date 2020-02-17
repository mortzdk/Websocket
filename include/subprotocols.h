#ifndef wss_subprotocols_h
#define wss_subprotocols_h

#include "uthash.h"
#include "config.h"

/**
 * The subprotocol API calls
 */
typedef void (*subInit)(char *config);
typedef void (*subConnect)(int fd);
typedef void (*subMessage)(int fd, char *message, size_t message_length, int **receivers, size_t *receiver_count);
typedef void (*subWrite)(int fd, char *message, size_t message_length);
typedef void (*subClose)(int fd);

typedef struct {
    int *handle;
    char *name;
    subInit init; 
    subConnect connect; 
    subMessage message;
    subWrite write; 
    subClose close;
    UT_hash_handle hh;
} wss_subprotocol_t;

/**
 * Function that loads subprotocol implementations into memory, by loading the
 * shared objects defined in the configuration.
 *
 * E.g.
 *
 * subprotocols/echo/echo.so 
 *
 * @param 	config	[config_t *config] 	"The configuration of the server"
 * @return 	      	[void]
 */
void load_subprotocols(config_t *config);

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
