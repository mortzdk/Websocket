#ifndef wss_string_h
#define wss_string_h

#include <stddef.h>

/**
 * Function that converts a binary representation into a hexidecimal one.
 *
 * @param 	bin	    [const unsigned char *]     "The binary value"
 * @param 	len     [size_t] 	                "The length of the binary value"
 * @return 	        [char *]                    "The hexidecimal representation of the binary value in a new memory block"
 */
char *bin2hex(const unsigned char *bin, size_t len);

/**
 * Function that prints a string as hex to stdout.
 *
 * @param 	s	    [const unsigned char *]     "The binary value"
 * @param 	len     [size_t] 	                "The length of the binary value"
 * @return 	        [char *]                    "The hexidecimal representation of the text string in a new memory block"
 */
char *str2hex(const unsigned char *s, size_t len);

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
int strinarray(const char *needle, const char *haystack[], size_t size);

/**
 * Function that loads the content of a file into memory.
 *
 * @param 	path	[char *] 	"The path to the subprotocols folder"
 * @param 	str	    [char **] 	"Will be filled with the content of the file"
 * @return 	      	[size_t]    "The length of the string in memory"
 */
size_t strload(char *path, char **str);

#endif
