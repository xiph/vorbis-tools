/* OggEnc
 *
 * This program is distributed under the GNU General Public License, version 2.
 * A copy of this license is included with this source.
 *
 * (C) 2001 Michael Smith <msmith@labyrinth.net.au>
 *
 * UTF-8 Conversion routines
 *   Copyright (C) 2001, Daniel Resare <noa@metamatrix.se>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
int utf8_encode(const char *from, char **to, const char *encoding)
{
	fprintf(stderr, "Sorry, not implemented currently on win32\n");
	return 1;
}

#else /* End win32. Rest is for real operating systems */

#ifdef HAVE_ICONV
#include <iconv.h>
#include <errno.h>
#endif

#include "utf8.h"
#include "charsetmap.h"

#define BUFSIZE 256

/*
 Converts the string FROM from the encoding specified in ENCODING
 to UTF-8. The resulting string i pointed to by *TO.

 Return values:
 0 indicates a successfully converted string.
 1 indicates that the given encoding is not available.
 2 indicates that the given string is bigger than BUFSIZE and can therefore
   not be encoded.
 3 indicates that given string could not be parsed.
*/
int utf8_encode(char *from, char **to, const char *encoding)
{
#ifdef HAVE_ICONV
	static unsigned char buffer[BUFSIZE];
    char *from_p, *to_p;
	size_t from_left, to_left, ret;
	iconv_t cd;
#endif

	if (!strcasecmp(encoding, "UTF-8")) {
	    /* ideally some checking of the given string should be done */
		*to = malloc(strlen(from) + 1);
		strcpy(*to, from);
		return 0;
	}

#ifdef HAVE_ICONV
	cd = iconv_open("UTF-8", encoding);
	if(cd == (iconv_t)(-1))
	{
		if(errno == EINVAL) {
			/* if iconv can't encode from this encoding, try
			 * simple_utf8_encode()
			 */
			return simple_utf8_encode(from, to, encoding);
		} else {
			perror("iconv_open");
		}
	}
	
	from_left = strlen(from);
	to_left = BUFSIZE;
	from_p = from;
	to_p = buffer;
	
	if(iconv(cd, &from_p, &from_left, &to_p, &to_left) == (size_t)-1)
	{
		iconv_close(cd);
		switch(errno)
		{
		case E2BIG:
			/* if the buffer is too small, try simple_utf8_encode()
			 */
			return simple_utf8_encode(from, to, encoding);
		case EILSEQ:
		case EINVAL:
			return 3;
		default:
			perror("iconv");
		}
	}
	else
	{
		iconv_close(cd);
	}
	*to = malloc(BUFSIZE - to_left + 1);
	buffer[BUFSIZE - to_left] = 0;
	strcpy(*to, buffer);
	return 0;
#else
	return simple_utf8_encode(from, to, encoding);
#endif
}

/*
 This implementation has the following limitations: The given charset must
 represent each glyph with exactly one (1) byte. No multi byte or variable
 width charsets are allowed. (An exception to this i UTF-8 that is passed
 right through.) The glyhps in the charsets must have a unicode value equal
 to or less than 0xFFFF (this inclues pretty much everything). For a complete,
 free conversion implementation please have a look at libiconv.
*/
int simple_utf8_encode(const char *from, char **to, const char *encoding)
{
	// can you always know this will be 16 bit?
	unsigned short *unicode;
	charset_map *map;
	int index = 0;
	unsigned char c;
	
	unicode = malloc((strlen(from) * sizeof(int)) + 1);

	map = get_map(encoding);
	
	if (map == NULL) 
		return 1;

	c = from[index];
	while(c)
	{
		unicode[index] = map->mapping[c];
		index++;
		c = from[index];
	}

	*to =  make_utf8_string(unicode);
	free(unicode);
	return 0;
}
	
charset_map *get_map(const char *encoding)
{
	charset_map *map_p = maps;
	while(map_p->name != NULL)
	{
		if(!strcasecmp(map_p->name, encoding))
		{
			return map_p;
		}
		map_p++;
	}
	return NULL;
}

char *make_utf8_string(const unsigned short *unicode)
{
	int size = 0, index = 0, out_index = 0;
	unsigned char *out;
	unsigned short c;

        /* first calculate the size of the target string */
	c = unicode[index++];
	while(c) {
		if(c < 0x0080) {
			size += 1;
		} else if(c < 0x8000) {
			size += 2;
		} else {
			size += 3;
		}
		c = unicode[index++];
	}	

	out = malloc(size);
	index = 0;

	c = unicode[index++];
	while(c)
	{
		if(c < 0x080) {
			out[out_index++] = c;
		} else if(c < 0x800) {
			out[out_index++] = 0xc0 | (c >> 6);
			out[out_index++] = 0x80 | (c & 0x3f);
		} else {
			out[out_index++] = 0xe0 | (c >> 12);
			out[out_index++] = 0x80 | ((c >> 6) & 0x3f);
			out[out_index++] = 0x80 | (c & 0x3f);
		}
		c = unicode[index++];
	}
	out[out_index] = 0x0000;

	return out;
}

#endif

