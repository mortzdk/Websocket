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

#include "extension.h"
#include "uthash.h"
#include "config.h"

#include <stdbool.h>

typedef void (*pyInit)(void);

typedef struct {
    int *handle;
    char *name;
    extAlloc alloc; 
    extInit init; 
    extOpen open; 
    extInFrame inframe; 
    extInFrames inframes;
    extOutFrame outframe;
    extOutFrames outframes;
    extClose close;
    extDestroy destroy;
    pyInit pyinit;
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
extern wss_extension_t *extensions;

#endif
