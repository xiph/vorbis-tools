/* Common things between reader and writer threads */

#ifndef __BUFFER_H
#define __BUFFER_H

#include "ogg123.h"

typedef struct chunk_s
{
  long len; /* Length of the chunk (for if we only got partial data) */
  char data[4096]; /* Data. 4096 is the chunk size we request from libvorbis. */
} chunk_t;

typedef struct buf_s
{
  char status;       /* Status. See STAT_* below. */
  int fds[2];        /* Pipe file descriptors. */
  chunk_t *reader;   /* Chunk the reader is busy with */
  chunk_t *writer;   /* Chunk the writer is busy with */
  chunk_t *end;      /* Last chunk in the buffer (for convenience) */
  chunk_t buffer[1]; /* The buffer itself. It's more than one chunk. */
} buf_t;

buf_t *fork_writer (long size, devices_t *d);
void submit_chunk (buf_t *buf, chunk_t chunk);
void buffer_shutdown (buf_t *buf);
void buffer_flush (buf_t *buf);

#define STAT_FLUSH 1
#define STAT_SHUTDOWN 2

#endif /* !defined (__BUFFER_H) */



