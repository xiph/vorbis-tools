/* Link this with ogg123 and the buffer support will be disabled. */
#include "ogg123.h"
#include "buffer.h"

buf_t *fork_writer (long size, devices_t *d)
{
  fprintf (stderr, "This ogg123 was not compiled with buffer support.\n");
  return NULL;
}

void submit_chunk (buf_t *buf, chunk_t chunk)
{
  fprintf (stderr, "Error: internal error in submit_chunk: no buffer support.\n");
}

void buffer_shutdown (buf_t *buf)
{
  fprintf (stderr, "Error: internal error in buffer_shutdown: no buffer support.\n");
}
