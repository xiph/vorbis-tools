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
#include "i18n.h"
#include <stdlib.h>
#if defined(_WIN32) || defined(__EMX__) || defined(__WATCOMC__)
#include <fcntl.h>
#include <io.h>
#include <time.h>
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

#if defined(__WATCOMC__) || defined(__BORLANDC__)
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

int create_directories(char *fn) 
{
    /* FIXME: Please implement me */
    return 0;
}

#else /* unix. Or at least win32 */

#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

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

int create_directories(char *fn)
{
    char *end, *start;
    struct stat statbuf;
    char segment[FILENAME_MAX];

    start = fn;

    while((end = strchr(start+1, '/')) != NULL)
    {
        memcpy(segment, fn, end-fn);
        segment[end-fn+1] = 0;

        if(stat(segment,&statbuf)) {
            if(errno == ENOENT) {
                if(mkdir(segment, 0777)) {
                    fprintf(stderr, _("Couldn't create directory \"%s\": %s\n"),
                                segment, strerror(errno));
                    return -1;
                }
            }
            else {
                fprintf(stderr, _("Error checking for existence of directory %s: %s\n"), 
                            segment, strerror(errno));
                return -1;
            }
        }
        else if(!S_ISDIR(statbuf.st_mode)) {
            fprintf(stderr, _("Error: path segment \"%s\" is not a directory\n"),
                    segment);
        }

        start = end+1;
    }

    return 0;

}

#endif


