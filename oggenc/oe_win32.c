/* OggEnc
 **
 ** This program is distributed under the GNU General Public License, version 2.
 ** A copy of this license is included with this source.
 **
 ** Copyright 2000, Michael Smith <msmith@labyrinth.net.au>
 **
 ** Portions from Vorbize, (c) Kenneth Arnold <kcarnold@yahoo.com>
 ** and libvorbis examples, (c) Monty <monty@xiph.org>
 **/

/* Win32 support routines */


#include "platform.h"
#include "encode.h"

#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <time.h>

/* This doesn't seem to exist on windows */
char *rindex(char *s, int c)
{
	char *ret = NULL;

	while(*s)
	{
		if(*s == (char)c)
			ret=s;
		s++;
	}
	return ret;
}

void setbinmode(FILE *f)
{
	_setmode( _fileno(f), _O_BINARY );
}

void *timer_start(void)
{
	time_t *start = malloc(sizeof(time_t));
	time(start);
	return (void *)start;
}

double timer_time(void *timer)
{
	time_t now = time(NULL);
	time_t start = *((time_t *)timer);

	return (double)(now-start);
}


void timer_clear(void *timer)
{
	free((time_t *)timer);
}


