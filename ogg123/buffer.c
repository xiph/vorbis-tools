/* buffer.c
 *  buffering code for ogg123. This is Unix-specific. Other OSes anyone?
 *
 * Thanks to Lee McLouchlin's buffer(1) for inspiration; no code from
 * that program is used in this buffer.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unistd.h> /* for fork and pipe*/
#include <fcntl.h>

#ifndef DARWIN
#include <malloc.h>
#endif

#include "ogg123.h"
#include "buffer.h"

/* Initialize the buffer structure. */
void buffer_init (buf_t *buf, long size)
{
  buf->status = 0;
  buf->reader = buf->writer = buf->buffer;
  buf->end = buf->buffer + (size - 1);
}

/* Main write loop. No semaphores. No deadlock. No problem. I hope. */
void writer_main (volatile buf_t *buf, devices_t *d)
{
  devices_t *d1;
  while (! (buf->status & STAT_SHUTDOWN && buf->reader == buf->writer))
    {
      /* Writer just waits on reader to be done with buf_write.
       * Reader must ensure that we don't deadlock. */

      write (buf->fds[1], "1", 1); /* This identifier could hold a lot
				    * more detail in the future. */

      while (buf->reader == buf->writer && !(buf->status & STAT_SHUTDOWN));

      if (buf->reader == buf->writer) break;

      /* devices_write (buf->writer->data, buf->writer->len, d); */
      {
	d1 = d;
	while (d1 != NULL) {
	  ao_play(d1->device, buf->writer->data, buf->writer->len);
	  d1 = d1->next_device;
	}
      }

      if (buf->writer == buf->end)
	buf->writer = buf->buffer;
      else
	buf->writer++;
   }
  buf->status = 0;
  write (buf->fds[1], "2", 1);
  _exit(0);
}

/* fork_writer is called to create the writer process. This creates
 * the shared memory segment of 'size' chunks, and returns a pointer
 * to the buffer structure that is shared. Just pass this straight to
 * submit_chunk and all will be happy. */

buf_t *fork_writer (long size, devices_t *d)
{
  int childpid;
  buf_t *buf;

  /* Get the shared memory segment. */
  int shmid = shmget (IPC_PRIVATE,
			  sizeof(buf_t) + sizeof (chunk_t) * (size - 1),
			  IPC_CREAT|SHM_R|SHM_W);

  if (shmid == -1)
    {
      perror ("shmget");
      exit (1);
    }
  
  /* Attach the segment to us (and our kids). Get and store the pointer. */
  buf = (buf_t *) shmat (shmid, 0, 0);
  
  if (buf == NULL)
    {
      perror ("shmat");
      exit (1);
    }

  /* Remove segment after last process detaches it or terminates. */
  shmctl(shmid, IPC_RMID, 0);

  buffer_init (buf, size);
  
  /* Create a pipe for communication between the two processes. Unlike
   * the first incarnation of an ogg123 buffer, the data is not transferred
   * over this channel, only occasional "WAKE UP!"'s. */

  if (pipe (buf->fds))
    {
      perror ("pipe");
      exit (1);
    }

  fcntl (buf->fds[1], F_SETFL, O_NONBLOCK);
  /* write should never block; read should always block. */

  fflush (stdout);
  /* buffer flushes stderr, but stderr is unbuffered (*duh*!) */
  
  childpid = fork();
  
  if (childpid == -1)
    {
      perror ("fork");
      exit (1);
    }

  if (childpid == 0)
    {
      writer_main (buf, d);
      return NULL;
    }
  else
    return buf;
}

void submit_chunk (buf_t *buf, chunk_t chunk)
{
  struct timeval tv;
  static fd_set set;

  FD_ZERO(&set);
  FD_SET(buf->fds[0], &set);

  /* Wait wait, don't step on my sample! */
  while (!((buf->reader != buf->end && buf->reader + 1 != buf->writer) ||
	   (buf->reader == buf->end && buf->writer != buf->buffer)))
    {
      /* buffer overflow (yikes! no actually it's a GOOD thing) */
      int ret;
      char t;
      
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      ret = select (buf->fds[0]+1, &set, NULL, NULL, &tv);
      
      while (ret-- > 0)
	read (buf->fds[0], &t, 1);
    }
	      
  *(buf->reader) = chunk;
  /* do this atomically */
  if (buf->reader == buf->end)
    buf->reader = buf->buffer;
  else
    buf->reader++;
}

void buffer_shutdown (buf_t *buf)
{
  struct timeval tv;

  buf->status |= STAT_SHUTDOWN;
  while (buf->status != 0)
    {
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      select (0, NULL, NULL, NULL, &tv);
    } 
  /* Deallocate the shared memory segment. */
  shmdt(buf);
}
