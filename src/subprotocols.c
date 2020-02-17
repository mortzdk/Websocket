#include <dlfcn.h>
#include <stdio.h>
#include <libgen.h>

#include "subprotocols.h"
#include "uthash.h"
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
 *
 * @param 	config	[config_t *config] 	"The configuration of the server"
 * @return 	      	[void]
 */
void load_subprotocols(config_t *config)
{
    size_t i, j;
    char *name;
    int *handle;
    wss_subprotocol_t* proto;
    int name_length = 0;

    WSS_log(
            SERVER_TRACE,
            "Loading subprotocols",
            __FILE__,
            __LINE__
           );
    for (i = 0; i < config->subprotocols_length; i++) {
        WSS_log(
                SERVER_TRACE,
                config->subprotocols[i],
                __FILE__,
                __LINE__
               );

        if ( unlikely(NULL == (handle = dlopen(config->subprotocols[i], RTLD_LAZY))) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            continue;
        }

        if ( unlikely(NULL == (proto = WSS_malloc(sizeof(wss_subprotocol_t)))) ) {
            (void) dlclose(proto->handle);
            return;
        }
        proto->handle = handle;

        if ( unlikely((*(void**)(&proto->init) = dlsym(proto->handle, "onInit")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void) dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->connect) = dlsym(proto->handle, "onConnect")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void) dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->message) = dlsym(proto->handle, "onMessage")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void) dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->write) = dlsym(proto->handle, "onWrite")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void) dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->close) = dlsym(proto->handle, "onClose")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void) dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        name = basename(config->subprotocols[i]);
        for (j = 0; name[j] != '.' && name[j] != '\0'; j++) {
            name_length++;
        }

        if ( unlikely(NULL == (proto->name = WSS_malloc(name_length+1))) ) {
            (void) dlclose(proto->handle);
            WSS_free((void **) &proto);
            return;
        }

        memcpy(proto->name, name, name_length);

        HASH_ADD_KEYPTR(hh, subprotocols, proto->name, name_length, proto);

        proto->init(config->subprotocols_config[i]);

        WSS_log(
                SERVER_TRACE,
                "Successfully loaded",
                __FILE__,
                __LINE__
               );
    }
}

/**
 * Function that looks for a subprotocol implementation of the name given.
 *
 * @param 	name	[char *] 	            "The name of the subprotocol"
 * @return 	      	[wss_subprotocol_t *]   "The subprotocol or NULL"
 */
wss_subprotocol_t *find_subprotocol(char *name) {
    wss_subprotocol_t *proto;

    HASH_FIND_STR(subprotocols, name, proto);

    return proto;
}

/**
 * Destroys all memory used to load and store the subprotocols 
 *
 * @return 	[void]
 */
void destroy_subprotocols() {
    wss_subprotocol_t *proto, *tmp;

    HASH_ITER(hh, subprotocols, proto, tmp) {
        HASH_DEL(subprotocols, proto);
        (void)dlclose(proto->handle);
        WSS_free((void **) &proto->name);
        WSS_free((void **) &proto);
    }
}
