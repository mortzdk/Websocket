#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen */

#include "alloc.h"
#include "core.h"

/**
 * Function that allocates memory.
 * @param 	ptr		[void *] 	"The memory that needs to be copies"
 * @param 	void	[size_t] 	"The size of the memory that should be copied"
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_copy(void *ptr, size_t size) {
	void *buffer;

    if ( unlikely(ptr == NULL) ) {

        return NULL;
    }

    if ( unlikely(NULL == (buffer = WSS_malloc(size))) ) {
        return NULL;
    }

    return memcpy(buffer, ptr, size);
}

/**
 * Function that allocates memory.  *
 * @param 	size	[size_t] 	"The size of the memory that should be allocated"
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_malloc(size_t size) {
	void *buffer;

    if ( unlikely(size == 0) ) {
        return NULL;
    }

#ifdef USE_RPMALLOC
	if ( unlikely(NULL == (buffer = rpmalloc( size ))) ) {
		return NULL;
	}
#else
	if ( unlikely(NULL == (buffer = malloc( size ))) ) {
		return NULL;
	}
#endif


	memset(buffer, '\0', size);

	return buffer;
}

/**
 * Function that allocates an array of memory.
 *
 * @param 	nmemb	[size_t] 	"The size of the array i.e. indexes"
 * @param 	size	[size_t] 	"The size of the memory that should be allocated"
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_calloc(size_t memb, size_t size) {
	void *buffer;

    if ( unlikely(size == 0 || memb == 0) ) {
        return NULL;
    }

#ifdef USE_RPMALLOC
	if ( unlikely(NULL == (buffer = rpcalloc(memb, size))) ) {
		return NULL;
	}
#else
	if ( unlikely(NULL == (buffer = calloc(memb, size))) ) {
		return NULL;
	}
#endif


	memset(buffer, '\0', size*memb);

	return buffer;
}

/**
 * Function that re-allocates some memory.
 *
 * @param 	ptr		[void **] 	"The memory that needs to be rearranged"
 * @param 	oldSize	[size_t] 	"The size of the memory that is already allocated"
 * @param 	newSize	[size_t] 	"The size of the memory that should be allocated"
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_realloc(void **ptr, size_t oldSize, size_t newSize) {
	void *buffer;

    if ( unlikely(newSize == 0) ) {
        WSS_free(ptr);
        return NULL;
    }
    
    if ( unlikely(ptr == NULL) ) {
#ifdef USE_RPMALLOC
        if ( unlikely(NULL == (buffer = rprealloc(NULL, newSize))) ) {
            return NULL;
        }
#else
        if ( unlikely(NULL == (buffer = realloc(NULL, newSize))) ) {
            return NULL;
        }
#endif
    } else {
#ifdef USE_RPMALLOC
        if ( unlikely(NULL == (buffer = rprealloc(*ptr, newSize))) ) {
            WSS_free(ptr);
            return NULL;
        }
#else
        if ( unlikely(NULL == (buffer = realloc(*ptr, newSize))) ) {
            WSS_free(ptr);
            return NULL;
        }
#endif
    }


    if ( likely(oldSize < newSize) ) {
        memset(((char *) buffer)+oldSize, '\0', (newSize-oldSize));
    }

	return buffer;
}

/**
 * Function that re-allocates some memory. The interface is of this function is
 * similar to the realloc(3) function.
 *
 * @param 	ptr		[void *] 	"The memory that needs to be rearranged"
 * @param 	newSize	[size_t] 	"The size of the memory that should be allocated"
 * @return 		    [void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_realloc_normal(void *ptr, size_t newSize) {
    return WSS_realloc(&ptr, newSize, newSize);
}

#ifndef NDEBUG
/**
 * Function that re-allocates some memory.
 *
 * @param 	ptr		[void **] 	"The memory that needs to be freed"
 * @return 		    [void]
 */
void WSS_free(void **ptr) {
    if ( likely(NULL != *ptr) ) {
#ifdef USE_RPMALLOC
        rpfree(*ptr);
#else
        free(*ptr);
#endif
        *ptr = NULL;
    }
}
#endif

/**
 * Function that re-allocates some memory. The interface is of this function is
 * similar to the free(3) function.
 *
 * @param 	ptr		[void *] 	"The memory that needs to be freed"
 * @return 		    [void]
 */
void WSS_free_normal(void *ptr) {
    WSS_free(&ptr);
}
