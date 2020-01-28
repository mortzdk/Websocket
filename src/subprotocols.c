#include <unistd.h>             /* close */
#include <dlfcn.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "subprotocols.h"
#include "uthash.h"
#include "alloc.h"
#include "predict.h"

/**
 * Global hashtable of subprotocols
 */
wss_subprotocol_t *subprotocols = NULL;

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
void load_subprotocols(const char *path)
{
    char *name;
    int name_length;
    int length;
    int *handle;
    size_t path_len;
    struct stat fstat;
    wss_subprotocol_t* proto;
    struct dirent *direntp = NULL;
    DIR *dirp = NULL;
    char full_name[PATH_LENGTH];

    /* Check input parameters. */
    if ( unlikely(NULL == path) ) {
        return;
    }

    path_len = strlen(path);

    if ( unlikely(! path || ! path_len || (path_len > PATH_LENGTH)) ) {
        return;
    }

    /* Open directory */
    dirp = opendir(path);
    if ( unlikely(dirp == NULL) ) {
        return;
    }

    while ( likely((direntp = readdir(dirp)) != NULL) ) {
        handle = NULL;
        proto = NULL;
        name = direntp->d_name;
        name_length = strlen(name);
        memset(full_name, '\0', PATH_LENGTH);

        /* Calculate full name, check we are in file length limits */
        if ( unlikely((path_len + 2*name_length + 5) > PATH_LENGTH) ) {
            continue;
        }

        /* Ignore special directories. */
        if ( unlikely(strncmp(name, ".", name_length) == 0 || strncmp(name, "..", name_length) == 0) ) {
            continue;
        }

        memcpy(full_name, path, path_len);
        length = path_len;
        if (full_name[length - 1] != '/') {
            memcpy(full_name+length, "/", 1);
            length += 1;
        }
        memcpy(full_name+length, name, name_length);
        length += name_length;

        /* Print only if it is really directory. */
        if ( unlikely(stat(full_name, &fstat) < 0) ) {
            continue;
        }

        if (full_name[length - 1] != '/') {
            strncat(full_name+length, "/", 1);
            length++;
        }
        memcpy(full_name+length, name, name_length);
        length += name_length;
        memcpy(full_name+length, ".so", 3);
        length += 3;

        if (S_ISDIR(fstat.st_mode)) {
            if ( unlikely(NULL == (handle = dlopen(full_name, RTLD_LAZY))) ) {
                continue;
            }

            if ( unlikely(NULL == (proto = WSS_malloc(sizeof(wss_subprotocol_t)))) ) {
                return;
            }

            if ( unlikely(NULL == (proto->name = WSS_malloc(name_length+1))) ) {
                return;
            }

            proto->handle = handle;
            memcpy(proto->name, name, name_length);

            if ( unlikely((*(void**)(&proto->connect) = dlsym(proto->handle, "onConnect")) == NULL) ) {
                continue;
            }

            if ( unlikely((*(void**)(&proto->message) = dlsym(proto->handle, "onMessage")) == NULL) ) {
                continue;
            }

            if ( unlikely((*(void**)(&proto->write) = dlsym(proto->handle, "onWrite")) == NULL) ) {
                continue;
            }

            if ( unlikely((*(void**)(&proto->close) = dlsym(proto->handle, "onClose")) == NULL) ) {
                continue;
            }

            HASH_ADD_KEYPTR(hh, subprotocols, proto->name, name_length, proto);
        }
    }

    /* Finalize resources. */
    (void) closedir(dirp);
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
