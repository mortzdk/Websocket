#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen */
#include <malloc.h>             

#include "alloc.h"
#include "log.h"
#include "predict.h"

/**
 * Function that allocates memory.  *
 * @param 	size	[size_t] 	"The size of the memory that should be allocated"
 * @return 	buffer	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_malloc(size_t size) {
	void *buffer;

    if ( unlikely(size == 0) ) {
        return NULL;
    }

	buffer = malloc( size );

	if ( unlikely(NULL == buffer) ) {
        WSS_log(
                SERVER_ERROR,
                "Failed to allocate memory: OUT OF MEMORY",
                __FILE__,
                __LINE__
               );
		return NULL;
	}

	memset(buffer, '\0', size);

	return buffer;
}

/**
 * Function that allocates an array of memory.
 *
 * @param 	nmemb	[size_t] 	"The size of the array i.e. indexes"
 * @param 	size	[size_t] 	"The size of the memory that should be allocated"
 * @return 	buffer	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_calloc(size_t memb, size_t size) {
	void *buffer;

    if ( unlikely(size == 0 || memb == 0) ) {
        return NULL;
    }

	buffer = calloc(memb, size);

	if ( unlikely(NULL == buffer) ) {
        WSS_log(
                SERVER_ERROR,
                "Failed to allocate memory: OUT OF MEMORY",
                __FILE__,
                __LINE__
               );
		return NULL;
	}

	return buffer;
}

/**
 * Function that re-allocates some memory.
 *
 * @param 	ptr		[void **] 	"The memory that needs to be rearranged"
 * @param 	oldSize	[size_t] 	"The size of the memory that is already allocated"
 * @param 	newSize	[size_t] 	"The size of the memory that should be allocated"
 * @return 	buffer	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_realloc(void **ptr, size_t oldSize, size_t newSize) {
	void *buffer;

    if ( unlikely(newSize == 0) ) {
        WSS_free(ptr);
        return NULL;
    }
    
    if ( unlikely(ptr == NULL) ) {
	    buffer = realloc(NULL, newSize);
    } else {
	    buffer = realloc(*ptr, newSize);
    }

	if ( unlikely(NULL == buffer) ) {
        WSS_free(ptr);
        WSS_log(
                SERVER_ERROR,
                "Failed to re-allocate memory: OUT OF MEMORY",
                __FILE__,
                __LINE__
               );
		return NULL;
	}

    if ( likely(oldSize < newSize) ) {
        memset(((char *) buffer)+oldSize, '\0', (newSize-oldSize));
    }

	return buffer;
}

#ifndef NDEBUG
/**
 * Function that re-allocates some memory.
 *
 * @param 	ptr		[void **] 	"The memory that needs to be freed"
 * @return 		    [void]
 */
void WSS_free(void **ptr) {
    if (NULL != *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}
#endif
