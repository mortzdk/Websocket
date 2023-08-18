#include <dlfcn.h>
#include <stdio.h>
#include <libgen.h>

#include "extensions.h"
#include "uthash.h"
#include "alloc.h"
#include "log.h"
#include "core.h"

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
 * @return 	      	[unsigned int]      "The number of extensions loaded"
 */
unsigned int WSS_load_extensions(wss_config_t *config)
{
    size_t i, j;
    char *name;
    int *handle;
    wss_extension_t* proto;
    int name_length;

    if ( unlikely(NULL == config) ) {
        return 0;
    }

    WSS_log_trace("Loading extensions");

    for (i = 0; i < config->extensions_length; i++) {
        name_length = 0;

        WSS_log_trace("Loading extension %s", config->extensions[i]);

        if ( unlikely(NULL == (handle = dlopen(config->extensions[i], RTLD_LAZY))) ) {
            WSS_log_error("Failed to load shared object: %s", dlerror());
            continue;
        }

        if ( unlikely(NULL == (proto = WSS_malloc(sizeof(wss_extension_t)))) ) {
            WSS_log_error("Unable to allocate extension structure");
            dlclose(handle);
            return 0;
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

        if ( unlikely((*(void**)(&proto->open) = dlsym(proto->handle, "onOpen")) == NULL) ) {
            WSS_log_error("Failed to find 'onOpen' function: %s", dlerror());
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->inframes) = dlsym(proto->handle, "inFrames")) == NULL) ) {
            WSS_log_error("Failed to find 'inFrames' function: %s", dlerror());
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        if ( unlikely((*(void**)(&proto->outframes) = dlsym(proto->handle, "outFrames")) == NULL) ) {
            WSS_log_error("Failed to find 'outFrames' function: %s", dlerror());
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

        name = basename(config->extensions[i]);
        for (j = 0; name[j] != '.' && name[j] != '\0'; j++) {
            name_length++;
        }

        if (name_length > MAX_EXTENSION_NAME_LENGTH) {
            WSS_log_error("Extension name '%s' is too long", name);
            dlclose(proto->handle);
            WSS_free((void **) &proto);
            continue;
        }

        /*
         * TODO: Find out what is necessary to invoke python shared object
         *
         * char *pyname;
         * if ( unlikely(NULL == (pyname = WSS_malloc(7+name_length+1))) ) {
         *     WSS_log_error("Unable to allocate name");
         *     dlclose(proto->handle);
         *     WSS_free((void **) &proto->name);
         *     WSS_free((void **) &proto);
         *     return;
         * }

         * sprintf(pyname, "PyInit_%s", name); 
         * if ( unlikely((*(void**)(&proto->pyinit) = dlsym(proto->handle, name)) != NULL) ) {
         *     proto->pyinit(); 
         * } else {
         *     sprintf(pyname, "init%s", name); 
         *     if ( unlikely((*(void**)(&proto->pyinit) = dlsym(proto->handle, name)) != NULL) ) {
         *         proto->pyinit(); 
         *     }
         * }
         * WSS_free((void **) &pyname);
         */

        memcpy(&proto->name[0], name, name_length);

        HASH_ADD_KEYPTR(hh, extensions, proto->name, name_length, proto);

        WSS_log_trace("Setting custom allocators for extension %s", proto->name);

        proto->alloc(WSS_malloc, WSS_realloc_normal, WSS_free_normal);

        WSS_log_trace("Initializing extension %s", proto->name);

        proto->init(config->extensions_config[i]);

        WSS_log_info("Successfully loaded %s extension", proto->name);
    }

    return HASH_COUNT(extensions);
}

/**
 * Function that looks for a extension implementation of the name given.
 *
 * @param 	name	[char *] 	            "The name of the extension"
 * @return 	      	[wss_extension_t *]   "The extension or NULL"
 */
wss_extension_t *WSS_find_extension(char *name) {
    wss_extension_t *proto;

    if ( unlikely(NULL == name) ) {
        return NULL;
    }

    HASH_FIND_STR(extensions, name, proto);

    return proto;
}

/**
 * Destroys all memory used to load and store the extensions 
 *
 * @return 	[void]
 */
void WSS_destroy_extensions() {
    wss_extension_t *proto, *tmp;

    HASH_ITER(hh, extensions, proto, tmp) {
        HASH_DEL(extensions, proto);
        proto->destroy();
        dlclose(proto->handle);
        WSS_free((void **) &proto);
    }
}
