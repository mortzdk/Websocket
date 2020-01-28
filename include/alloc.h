#ifndef wss_alloc_h
#define wss_alloc_h

#include <stdlib.h>
#include <stddef.h>

/**
 * Function that allocates memory.  *
 * @param 	size	[size_t] 	"The size of the memory that should be allocated"
 * @return 	buffer	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_malloc(size_t size);

/**
 * Function that allocates an array of memory.
 *
 * @param 	nmemb	[size_t] 	"The size of the array i.e. indexes"
 * @param 	size	[size_t] 	"The size of the memory that should be allocated"
 * @return 	buffer	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_calloc(size_t memb, size_t size);

/**
 * Function that re-allocates some memory.
 *
 * @param 	ptr		[void **] 	"The memory that needs to be rearranged"
 * @param 	oldSize	[size_t] 	"The size of the memory that is already allocated"
 * @param 	newSize	[size_t] 	"The size of the memory that should be allocated"
 * @return 	buffer	[void *] 	"Returns pointer to allocated memory if successful, otherwise NULL"
 */
void *WSS_realloc(void **ptr, size_t oldSize, size_t newSize);

/**
 * Function that re-allocates some memory.
 *
 * @param 	ptr		[void **] 	"The memory that needs to be freed"
 * @return 		    [void]
 */
#ifndef NDEBUG
void WSS_free(void **ptr);
#else
#define WSS_free(p) do { void ** __p = (p); free(*(__p)); *(__p) = NULL; } while (0)
#endif

#endif
