#include <dlfcn.h>
#include <stdio.h>
#include <libgen.h>

#include "extensions.h"
#include "uthash.h"
#include "alloc.h"
#include "log.h"
#include "predict.h"

/**
 * Global hashtable of extensions
 */
wss_extension_t *extensions = NULL;

/**
 * Function that loads extension implementations into memory, by loading
 * shared objects from the server configuration.
 *
 * E.g.
 *
 * extensions/permessage-deflate/permessage-deflate.so 
 *
 * @param 	config	[config_t *] 	"The configuration of the server"
 * @return 	      	[void]
 */
void load_extensions(config_t *config)
{
    size_t i, j;
    char *name;
    int *handle;
    wss_extension_t* proto;
    int name_length = 0;

    WSS_log(
            SERVER_TRACE,
            "Loading extensions",
            __FILE__,
            __LINE__
           );

    for (i = 0; i < config->extensions_length; i++) {
        WSS_log(
                SERVER_TRACE,
                config->extensions[i],
                __FILE__,
                __LINE__
               );

        if ( unlikely(NULL == (handle = dlopen(config->extensions[i], RTLD_LAZY))) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            continue;
        }

        if ( unlikely(NULL == (proto = WSS_malloc(sizeof(wss_extension_t)))) ) {
            (void)dlclose(proto->handle);
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

        if ( unlikely((*(void**)(&proto->open) = dlsym(proto->handle, "onOpen")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void)dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->inframe) = dlsym(proto->handle, "inFrame")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void)dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->inframes) = dlsym(proto->handle, "inFrames")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void)dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->outframe) = dlsym(proto->handle, "outFrame")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void)dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->outframes) = dlsym(proto->handle, "outFrames")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void)dlclose(proto->handle);
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
            (void)dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->destroy) = dlsym(proto->handle, "onDestroy")) == NULL) ) {
            WSS_log(
                    SERVER_ERROR,
                    dlerror(),
                    __FILE__,
                    __LINE__
                   );
            (void)dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        name = basename(config->extensions[i]);
        for (j = 0; name[j] != '.' && name[j] != '\0'; j++) {
            name_length++;
        }

        if ( unlikely(NULL == (proto->name = WSS_malloc(name_length+1))) ) {
            (void) dlclose(proto->handle);
            WSS_free((void **) &proto);
            return;
        }

        memcpy(proto->name, name, name_length);

        HASH_ADD_KEYPTR(hh, extensions, proto->name, name_length, proto);

        proto->init(config->extensions_config[i]);

        WSS_log(
                SERVER_TRACE,
                "Successfully loaded",
                __FILE__,
                __LINE__
               );
    }
}

/**
 * Function that looks for a extension implementation of the name given.
 *
 * @param 	name	[char *] 	            "The name of the extension"
 * @return 	      	[wss_extension_t *]   "The extension or NULL"
 */
wss_extension_t *find_extension(char *name) {
    wss_extension_t *proto;

    HASH_FIND_STR(extensions, name, proto);

    return proto;
}

/**
 * Destroys all memory used to load and store the extensions 
 *
 * @return 	[void]
 */
void destroy_extensions() {
    wss_extension_t *proto, *tmp;

    HASH_ITER(hh, extensions, proto, tmp) {
        HASH_DEL(extensions, proto);
        proto->destroy();
        (void)dlclose(proto->handle);
        WSS_free((void **) &proto->name);
        WSS_free((void **) &proto);
    }
}
