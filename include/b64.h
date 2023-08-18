#pragma once 

/**
 * `b64.h' - b64
 *
 * copyright (c) 2014 joseph werle
 */

#ifndef B64_H
#define B64_H 1

#include <stdlib.h>

#include "alloc.h"

/**
 *  Memory allocation functions to use. You can define b64_malloc and
 * b64_realloc to custom functions if you want.
 */

#ifndef b64_malloc
#  define b64_malloc(size) WSS_malloc(size)
#endif
#ifndef b64_realloc
#  define b64_realloc(ptr, size) WSS_realloc((void **)&ptr, size, size)
#endif

/**
 * Base64 index table.
 */

static const char b64_table[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/'
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encode `unsigned char *' source with `size_t' size.
 * Returns a `char *' base64 encoded string.
 */

size_t
b64_encode (const unsigned char *, size_t, char **out);

/**
 * Decode `char *' source with `size_t' len into out.
 * Returns a `size_t' size of decoded string.
 */
size_t b64_decode_ex (const char *src, size_t len, unsigned char **out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif
