#ifndef wss_extensions_h
#define wss_extensions_h

#if !defined(uthash_malloc) || !defined(uthash_free)
#include "alloc.h"

#ifndef uthash_malloc
#define uthash_malloc(sz) WSS_malloc(sz)   /* malloc fcn */
#endif

#ifndef uthash_free
#define uthash_free(ptr,sz) WSS_free((void **)&ptr) /* free fcn */
#endif
#endif

#include "uthash.h"
#include "config.h"

#include <stdbool.h>

/**
 * The extension API calls
 */
typedef void (*extInit)(char *config);
typedef void (*extOpen)(int fd, char *param, char **accepted, bool *valid);
typedef void (*extInFrame)(int fd, void *frame);
typedef void (*extInFrames)(int fd, void **frames, size_t len);
typedef void (*extOutFrame)(int fd, void *frame);
typedef void (*extOutFrames)(int fd,void **frames, size_t len);
typedef void (*extClose)(int fd);
typedef void (*extDestroy)();

typedef struct {
    int *handle;
    char *name;
    extInit init; 
    extOpen open; 
    extInFrame inframe; 
    extInFrames inframes;
    extOutFrame outframe;
    extOutFrames outframes;
    extClose close;
    extDestroy destroy;
    UT_hash_handle hh;
} wss_extension_t;

typedef struct {
    wss_extension_t *ext;
    char *name;
    char *accepted;
} wss_ext_t;

/**
 * Function that loads extension implementations into memory, by loading
 * shared objects from the server configuration.
 *
 * E.g.
 *
 * extensions/permessage-deflate/permessage-deflate.so 
 *
 * @param 	config	[wss_config_t *] 	"The configuration of the server"
 * @return 	      	[void]
 */
void WSS_load_extensions(wss_config_t *config);

/**
 * Function that looks for a extension implementation of the name given.
 *
 * @param 	name	[char *] 	          "The name of the extension"
 * @return 	      	[wss_extension_t *]   "The extension or NULL"
 */
wss_extension_t *WSS_find_extension(char *name);

/**
 * Destroys all memory used to load and store the extensions 
 *
 * @return 	[void]
 */
void WSS_destroy_extensions();

/**
 * Global hashtable of extensions
 */
wss_extension_t *extensions;

#endif
