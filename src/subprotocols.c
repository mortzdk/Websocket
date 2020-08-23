#include <dlfcn.h>
#include <stdio.h>
#include <libgen.h>
#include <unistd.h>

#include "subprotocols.h"
#include "subprotocol.h"
#include "uthash.h"
#include "message.h"
#include "alloc.h"
#include "log.h"
#include "predict.h"

/**
 * Global hashtable of subprotocols
 */
wss_subprotocol_t *subprotocols = NULL;

/**
 * Function that loads subprotocol implementations into memory, by loading the
 * shared objects defined in the configuration.
 *
 * E.g.
 *
 * subprotocols/echo/echo.so 
 * subprotocols/broadcast/broadcast.so 
 *
 * @param 	config	[config_t *] 	"The configuration of the server"
 * @return 	      	[void]
 */
void WSS_load_subprotocols(wss_config_t *config)
{
    size_t i, j;
    char *name;
    int *handle;
    wss_subprotocol_t* proto;
    int name_length;

    if ( unlikely(NULL == config) ) {
        return;
    }

    WSS_log_trace("Loading %d subprotocols", config->subprotocols_length);

    for (i = 0; i < config->subprotocols_length; i++) {
        name_length = 0;

        WSS_log_trace("Loading subprotocol %s", config->subprotocols[i]);

        if ( unlikely(NULL == (handle = dlopen(config->subprotocols[i], RTLD_LAZY))) ) {
            WSS_log_error("Failed to load shared object: %s", dlerror());
            continue;
        }

        if ( unlikely(NULL == (proto = WSS_malloc(sizeof(wss_subprotocol_t)))) ) {
            WSS_log_error("Unable to allocate subprotocol structure");
            dlclose(proto->handle);
            return;
        }
        proto->handle = handle;

        if ( unlikely((*(void**)(&proto->alloc) = dlsym(proto->handle, "setAllocators")) == NULL) ) {
            WSS_log_error("Failed to find 'setAllocators' function: %s", dlerror());
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->init) = dlsym(proto->handle, "onInit")) == NULL) ) {
            WSS_log_error("Failed to find 'onInit' function: %s", dlerror());
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->connect) = dlsym(proto->handle, "onConnect")) == NULL) ) {
            WSS_log_error("Failed to find 'onConnect' function: %s", dlerror());
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->message) = dlsym(proto->handle, "onMessage")) == NULL) ) {
            WSS_log_error("Failed to find 'onMessage' function: %s", dlerror());
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->write) = dlsym(proto->handle, "onWrite")) == NULL) ) {
            WSS_log_error("Failed to find 'onWrite' function: %s", dlerror());
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->close) = dlsym(proto->handle, "onClose")) == NULL) ) {
            WSS_log_error("Failed to find 'onClose' function: %s", dlerror());
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->destroy) = dlsym(proto->handle, "onDestroy")) == NULL) ) {
            WSS_log_error("Failed to find 'onDestroy' function: %s", dlerror());
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        name = basename(config->subprotocols[i]);
        for (j = 0; name[j] != '.' && name[j] != '\0'; j++) {
            name_length++;
        }

        if ( unlikely(NULL == (proto->name = WSS_malloc(name_length+1))) ) {
            WSS_log_error("Unable to allocate name");
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            return;
        }

        memcpy(proto->name, name, name_length);

        HASH_ADD_KEYPTR(hh, subprotocols, proto->name, name_length, proto);

        WSS_log_trace("Setting custom allocators for subprotocol %s", proto->name);

        // Set custom allocators
        proto->alloc(WSS_malloc, WSS_realloc_normal, WSS_free_normal);

        WSS_log_trace("Initializing subprotocol %s", proto->name);

        // Initialize subprotocol
        proto->init(config->subprotocols_config[i], WSS_message_send);

        WSS_log_info("Successfully loaded %s extension", proto->name);
    }
}

/**
 * Function that looks for a subprotocol implementation of the name given.
 *
 * @param 	name	[char *] 	            "The name of the subprotocol"
 * @return 	      	[wss_subprotocol_t *]   "The subprotocol or NULL"
 */
wss_subprotocol_t *WSS_find_subprotocol(char *name) {
    wss_subprotocol_t *proto;

    if ( unlikely(NULL == name) ) {
        return NULL;
    }

    HASH_FIND_STR(subprotocols, name, proto);

    return proto;
}

/**
 * Destroys all memory used to load and store the subprotocols 
 *
 * @return 	[void]
 */
void WSS_destroy_subprotocols() {
    wss_subprotocol_t *proto, *tmp;

    HASH_ITER(hh, subprotocols, proto, tmp) {
        HASH_DEL(subprotocols, proto);
        proto->destroy();
        dlclose(proto->handle);
        WSS_free((void **) &proto->name);
        WSS_free((void **) &proto);
    }
}
