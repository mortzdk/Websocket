#include <errno.h> 				/* errno */
#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <stdlib.h>				/* atoi, malloc, free, realloc, calloc */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen,
                                   strtok, strtok_r */
#include "str.h"
#include "alloc.h"
#include "log.h"
#include "core.h"

/**
 * Function that converts a binary representation into a hexidecimal one. 
 *
 * Output must be of size len*2+1.
 *
 * @param 	bin	    [const unsigned char *]     "The binary value"
 * @param 	len     [size_t] 	                "The length of the binary value"
 * @param 	output  [char **]                   "The output string to write to"
 * @return          [size_t]                    "The length of the output string"
 */
inline size_t bin2hex(const unsigned char *bin, size_t len, char **output)
{
    char *out = *output;
    size_t i;

    for (i = 0; likely(i < len); i++) {
        out[i*2]   = "0123456789ABCDEF"[bin[i] >> 4];
        out[i*2+1] = "0123456789ABCDEF"[bin[i] & 0x0F];
    }

    out[len*2] = '\0';

    return len*2;
}

/**
 * Function that looks for a value in a char * array.
 *
 * A value is said to be found if the prefix of the needle is contained in the
 * whole array value.
 *
 * E.g
 *
 * haystack[0] = "testing"
 * needle = "testing123"
 *
 * will return 0, since all of the string in haystack[0] is present in needle.
 *
 * @param 	needle	    [const char *] 	    "The value to look for"
 * @param 	haystack    [const char **] 	"The array to look in"
 * @param 	size        [size_t] 	        "The amount of values in the array"
 * @return 	      	    [int]               "Will return 0 if present and -1 if not"
 */
int strinarray(const char *needle, const char **haystack, size_t size) {
    if ( unlikely(NULL == needle) ) {
        return -1;
    }

    int i;
    unsigned long length = strlen(needle);

    for (i = 0; likely(i < (int) size); i++) {
        if ( unlikely(NULL != haystack[i] && length == strlen(haystack[i]) && strncmp(needle, haystack[i], length) == 0) ) {
            return 0;
        }
    }

    return -1;
}

/**
 * Function that loads the content of a file into memory.
 *
 * @param 	path	[char *] 	"The path to the subprotocols folder"
 * @param 	str	    [char **] 	"Will be filled with the content of the file"
 * @return 	      	[size_t]    "The length of the string in memory"
 */
size_t strload(char *path, char **str) {
    int n;
    long size;
    FILE *file;

    if ( unlikely(NULL == (file = fopen(path, "rb"))) ) {
        WSS_log_error("Unable to open file: %s", strerror(errno));
        *str = NULL;
        return 0;
    }

    if ( unlikely(fseek(file, 0, SEEK_END) != 0) ) {
        WSS_log_error("Unable to seek to the end of file: %s", strerror(errno));
        fclose(file);
        *str = NULL;
        return 0;
    }

    size = ftell(file);

    if ( unlikely(fseek(file, 0, SEEK_SET)) ) {
        WSS_log_error("Unable to seek back to the start of file: %s", strerror(errno));
        fclose(file);
        *str = NULL;
        return 0;
    }


    if ( unlikely(NULL == (*str = (char *)WSS_malloc(size + 1))) ) {
        WSS_log_error("Unable to allocate for string");
        fclose(file);
        *str = NULL;
        return 0;
    }

    if ( unlikely(fread(*str, sizeof(char), size, file) != (unsigned long)size) ) {
        WSS_log_error("Didn't read what was expected");
        fclose(file);
        WSS_free((void **) &str);
        return 0;
    }

    n = fclose(file);
    if ( unlikely(n != 0) ) {
        WSS_log_error("Unable to close file: %s", strerror(errno));
        WSS_free((void **) &str);
        return 0;
    }

    return size;
}
