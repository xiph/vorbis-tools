/* OggEnc
 *
 * This program is distributed under the GNU General Public License, version 2.
 * A copy of this license is included with this source.
 *
 * Copyright © 2001, Daniel Resare <noa@metamatrix.se>
 */

typedef struct
{
	char* name;
	int mapping[256];
} charset_map;

charset_map *get_map(const char *encoding);
char *make_utf8_string(const unsigned short *unicode);
int simple_utf8_encode(const char *from, char **to, const char *encoding);
int utf8_encode(char *from, char **to, const char *encoding);
