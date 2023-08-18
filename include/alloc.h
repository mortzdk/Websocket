#pragma once 

#ifndef WSS_ALLOC_H
#define WSS_ALLOC_H

#include <stdlib.h>
#include <stddef.h>

#include "rpmalloc.h"

/**
 * Function that allocates memory.
 * @param 	ptr		[void *] 	"The memory that needs to be copies"
 * @param 	void	[size_t] 	"The size of the memory that should be copied"
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_copy(void *ptr, size_t size);

/**
 * Function that allocates memory.
 * @param 	size	[size_t] 	"The size of the memory that should be allocated"
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_malloc(size_t size);

/**
 * Function that allocates an array of memory.
 *
 * @param 	nmemb	[size_t] 	"The size of the array i.e. indexes"
 * @param 	size	[size_t] 	"The size of the memory that should be allocated"
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_calloc(size_t memb, size_t size);

/**
 * Function that re-allocates some memory.
 *
 * @param 	ptr		[void **] 	"The memory that needs to be rearranged"
 * @param 	oldSize	[size_t] 	"The size of the memory that is already allocated"
 * @param 	newSize	[size_t] 	"The size of the memory that should be allocated"
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_realloc(void **ptr, size_t oldSize, size_t newSize);

/**
 * Function that re-allocates some memory. The interface is of this function is
 * similar to the realloc(3) function.
 *
 * @param 	ptr		[void **] 	"The memory that needs to be rearranged"
 * @param 	newSize	[size_t] 	"The size of the memory that should be allocated"
 * @return 	      	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_realloc_normal(void *ptr, size_t newSize);

/**
 * Function that frees some memory.
 *
 * @param 	ptr		[void **] 	"The memory that needs to be freed"
 * @return 		    [void]
 */
#ifndef NDEBUG
void WSS_free(void **ptr);
#else
#ifdef USE_RPMALLOC
#define WSS_free(p) do { void ** __p = (p); rpfree(*(__p)); *(__p) = NULL; } while (0)
#else
#define WSS_free(p) do { void ** __p = (p); free(*(__p)); *(__p) = NULL; } while (0)
#endif
#endif

/**
 * Function that frees some memory. The interface is of this function is
 * similar to the free(3) function.
 *
 * @param 	ptr		[void *] 	"The memory that needs to be freed"
 * @return 		    [void]
 */
void WSS_free_normal(void *ptr);

#endif
