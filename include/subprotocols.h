#pragma once

#ifndef WSS_SUBPROTOCOLS_H
#define WSS_SUBPROTOCOLS_H

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
#include "subprotocol.h"
#include "message.h"
#include "config.h"

//typedef void (*pyInit)(void);
//
#define MAX_SUBPROTOCOL_NAME_LENGTH 256

typedef struct {
    int *handle;
    char name[MAX_SUBPROTOCOL_NAME_LENGTH];
    subAlloc alloc; 
    subInit init; 
    subConnect connect; 
    subMessage message;
    subWrite write; 
    subClose close;
    subDestroy destroy;
    //pyInit pyinit;
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
 * @param 	config	[wss_config_t *config] 	"The configuration of the server"
 * @return 	      	[unsigned int]  "The number of subprotocols loaded"
 */
unsigned int WSS_load_subprotocols(wss_config_t *config);

/**
 * Function that looks for a subprotocol implementation of the name given.
 *
 * @param 	name	[char *] 	            "The name of the subprotocol"
 * @return 	      	[wss_subprotocol_t *]   "The subprotocol or NULL"
 */
wss_subprotocol_t *WSS_find_subprotocol(char *name);

/**
 * Destroys all memory used to load and store the subprotocols 
 *
 * @return 	[void]
 */
void WSS_destroy_subprotocols();

/**
 * Global hashtable of subprotocols
 */
extern wss_subprotocol_t *subprotocols;

#endif
