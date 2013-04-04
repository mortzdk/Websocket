/*
 * File:   utf8.c
 * Author: Pontus Östlund <spam@poppa.se>
 *
 * UTF-8 encoding/decoding functions from/to ISO-8859-1 and US-ASCII.
 * The encoding/decoding functions here are taken from xml.c in PHP
 *
 * Copyright (c) 1999 - 2009 The PHP Group. All rights reserved.
 * Copyright (c) 2009        Pontus Östlund <spam@poppa.se>
 */

#include "utf8.h"

inline static unsigned short xml_encode_iso_8859_1 (unsigned char);
inline static char           xml_decode_iso_8859_1 (unsigned short);
inline static unsigned short xml_encode_us_ascii (unsigned char);
inline static char           xml_decode_us_ascii (unsigned short);

static void *emalloc (size_t size)
{
  void *p = malloc(size);
  if (p == NULL) {
    fprintf(stderr, "Out of memory!\n");
    exit(1);
  }

  return p;
}

static void *erealloc (void *ptr, size_t size)
{
  void *p = realloc(ptr, size);
  if (p == NULL) {
    fprintf(stderr, "Out of memory!\n");
    exit(1);
  }

  return p;
}

/* All the encoding functions are set to NULL right now, since all
 * the encoding is currently done internally by expat/xmltok.
 */
xml_encoding xml_encodings[] = {
  { "ISO-8859-1", xml_decode_iso_8859_1, xml_encode_iso_8859_1 },
  { "US-ASCII",   xml_decode_us_ascii,   xml_encode_us_ascii   },
  { "UTF-8",      NULL,                  NULL                  },
  { NULL,         NULL,                  NULL                  }
};

inline static unsigned short xml_encode_iso_8859_1 (unsigned char c)
{
  return (unsigned short)c;
}

inline static char xml_decode_iso_8859_1 (unsigned short c)
{
  return (char)(c > 0xff ? '?' : c);
}

inline static unsigned short xml_encode_us_ascii (unsigned char c)
{
  return (unsigned short)c;
}

inline static char xml_decode_us_ascii (unsigned short c)
{
  return (char)(c > 0x7f ? '?' : c);
}

static xml_encoding *xml_get_encoding (const XML_Char *name)
{
  xml_encoding *enc = &xml_encodings[0];

  while (enc && enc->name) {
    if (strcasecmp(name, enc->name) == 0)
      return enc;
    enc++;
  }
  return NULL;
}

static char *xml_utf8_encode (const char *s, int len, int *newlen,
                              const XML_Char *encoding)
{
	int pos = len;
	int size;
	char *newbuf;
	unsigned int c;
	unsigned short (*encoder)(unsigned char) = NULL;
	xml_encoding *enc = xml_get_encoding (encoding);

	*newlen = 0;

	if (enc)
		encoder = enc->encoding_function;
	else
		/* If the target encoding was unknown, fail */
		return NULL;

	if (encoder == NULL) {
		/* If no encoder function was specified, return the data as-is. */
		newbuf = (char*)emalloc (len + 1);
		memcpy (newbuf, s, len);
		*newlen = len;
		newbuf[*newlen] = '\0';
		return newbuf;
	}

	/* This is the theoretical max (will never get beyond len * 2 as long
	* as we are converting from single-byte characters, though) */
	size = len;
	newbuf = emalloc (size);

	while (pos > 0) {
		c = encoder ? encoder ((unsigned char)(*s)) : (unsigned short)(*s);

		size += 16; /* add 16 bytes in new buffer */
		newbuf = (char*)erealloc (newbuf, size);

		if (c < 0x80)
			newbuf[(*newlen)++] = (char) c;
		else if (c > 6) {
			newbuf[(*newlen)++] = (0x80 | (c & 0x3f));
		}
		else if (c > 12) {
			newbuf[(*newlen)++] = (0xc0 | ((c >> 6) & 0x3f));
			newbuf[(*newlen)++] = (0x80 | (c & 0x3f));
		}
		else if (c > 18) {
			newbuf[(*newlen)++] = (0xe0 | ((c >> 12) & 0x3f));
			newbuf[(*newlen)++] = (0xc0 | ((c >> 6) & 0x3f));
			newbuf[(*newlen)++] = (0x80 | (c & 0x3f));
		}

		pos--;
		s++;
	}

	newbuf[*newlen] = '\0';

	return newbuf;
}

static char *xml_utf8_decode (const XML_Char *s, int len, int *newlen,
		                          const XML_Char *encoding)
{
  int pos = len;
  char *newbuf = emalloc(len + 1);
  unsigned short c;
  char (*decoder)(unsigned short) = NULL;
  xml_encoding *enc = xml_get_encoding(encoding);

  *newlen = 0;
  if (enc)
    decoder = enc->decoding_function;

  if (decoder == NULL) {
    /* If the target encoding was unknown, or no decoder function
     * was specified, return the UTF-8-encoded data as-is.
     */
    memcpy(newbuf, s, len);
    *newlen = len;
    newbuf[*newlen] = '\0';
    return newbuf;
  }

  while (pos > 0) {
    c = (unsigned char)(*s);
    if (c >= 0xf0) { /* four bytes encoded, 21 bits */
      if(pos-4 >= 0)
				c = ((s[0]&7)<<18) | ((s[1]&63)<<12) | ((s[2]&63)<<6) | (s[3]&63);
      else
				c = '?';

      s += 4;
      pos -= 4;
    }
    else if (c >= 0xe0) { /* three bytes encoded, 16 bits */
      if (pos-3 >= 0)
				c = ((s[0]&63)<<12) | ((s[1]&63)<<6) | (s[2]&63);
      else
				c = '?';

      s += 3;
      pos -= 3;
    }
    else if (c >= 0xc0) { /* two bytes encoded, 11 bits */
      if (pos-2 >= 0)
				c = ((s[0]&63)<<6) | (s[1]&63);
      else
				c = '?';

      s += 2;
      pos -= 2;
    }
    else {
      s++;
      pos--;
    }

    newbuf[*newlen] = decoder ? decoder (c) : c;
    ++*newlen;
  }

  if (*newlen < len)
    newbuf = erealloc (newbuf, *newlen + 1);

  newbuf[*newlen] = '\0';
  return newbuf;
}

/* Public function  */

char *utf8_encode (const char *str)
{
  char *out;
  if (strlen (str)) {
    int alen, len;
    alen = strlen (str);
    out = xml_utf8_encode (str, alen, &len, "ISO-8859-1");
  }

  return out;
}

char *utf8_decode (const char *str)
{
  char *out;
  if (strlen(str)) {
    int alen, len;
    alen = strlen(str);
    out = xml_utf8_decode (str, alen, &len, "ISO-8859-1");
  }

  return out;
}

void utf8_clean (void *str)
{
	if (str) 
		free (str);
}

