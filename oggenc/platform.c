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

/* Platform support routines  - win32, OS/2, unix */


#include "platform.h"
#include "encode.h"
#include <stdlib.h>
#if defined(_WIN32) || defined(__EMX__) || defined(__WATCOMC__)
#include <fcntl.h>
#include <io.h>
#include <time.h>
#endif

#if defined(_WIN32) || defined(__WATCOMC__)
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
#endif

#if defined(_WIN32) && defined(_MSC_VER)

void setbinmode(FILE *f)
{
	_setmode( _fileno(f), _O_BINARY );
}
#endif /* win32 */

#ifdef __EMX__
void setbinmode(FILE *f) 
{
	        _fsetmode( f, "b");
}
#endif

#ifdef __WATCOMC__ || __BORLANDC__
void setbinmode(FILE *f)
{
	setmode(fileno(f), O_BINARY);
}
#endif


#if defined(_WIN32) || defined(__EMX__) || defined(__WATCOMC__)
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

#else /* unix. Or at least win32 */

#include <sys/time.h>
#include <unistd.h>

void *timer_start(void)
{
	struct timeval *start = malloc(sizeof(struct timeval));
	gettimeofday(start, NULL);
	return (void *)start;
}

double timer_time(void *timer)
{
	struct timeval now;
	struct timeval start = *((struct timeval *)timer);

	gettimeofday(&now, NULL);

	return (double)now.tv_sec - (double)start.tv_sec + 
		((double)now.tv_usec - (double)start.tv_usec)/1000000.0;
}

void timer_clear(void *timer)
{
	free((time_t *)timer);
}
#endif


