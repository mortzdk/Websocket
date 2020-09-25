#include <stdlib.h>             /* atoi, malloc, free, realloc */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen */

#include "alloc.h"
#include "predict.h"

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
	buffer = rpmalloc( size );
#else
	buffer = malloc( size );
#endif

	if ( unlikely(NULL == buffer) ) {
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
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_calloc(size_t memb, size_t size) {
	void *buffer;

    if ( unlikely(size == 0 || memb == 0) ) {
        return NULL;
    }

#ifdef USE_RPMALLOC
	buffer = rpcalloc(memb, size);
#else
	buffer = calloc(memb, size);
#endif

	if ( unlikely(NULL == buffer) ) {
		return NULL;
	}

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
	    buffer = rprealloc(NULL, newSize);
#else
	    buffer = realloc(NULL, newSize);
#endif
    } else {
#ifdef USE_RPMALLOC
	    buffer = rprealloc(*ptr, newSize);
#else
	    buffer = realloc(*ptr, newSize);
#endif
    }

	if ( unlikely(NULL == buffer) ) {
        WSS_free(ptr);
		return NULL;
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
