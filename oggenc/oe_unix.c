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


#include "platform.h"
#include "encode.h"

#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

void *timer_start(void)
{
	struct timeval *start = malloc(sizeof(struct timeval));
	gettimeofday(start,NULL);
	return (void *)start;
}

double timer_time(void *timer)
{
	struct timeval now;
	struct timeval start = *((struct timeval *)timer);

	gettimeofday(&now,NULL);

	return (double)now.tv_sec - (double)start.tv_sec + 
		((double)now.tv_usec - (double)start.tv_usec)/1000000.0;
}


void timer_clear(void *timer)
{
	free((time_t *)timer);
}


