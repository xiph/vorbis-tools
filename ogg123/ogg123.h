/* This file is part of ogg123, an Ogg Vorbis player. See ogg123.c
 * for copyright information. */
#ifndef __OGG123_H
#define __OGG123_H

/* Common includes */
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <ao/ao.h>

/* For facilitating output to multiple devices */
typedef struct devices_s {
  int driver_id;
  ao_device_t *device;
  ao_option_t *options;
  struct devices_s *next_device;
} devices_t;

typedef struct ogg123_options_s {
  char *read_file;            /* File to decode */
  char shuffle;               /* Should we shuffle playing? */
  signed short int verbose;   /* Verbose output if > 0, quiet if < 0 */
  signed short int quiet;     /* Be quiet (no title) */
  double seekpos;             /* Amount to seek by */
  FILE *instream;             /* Stream to read from. */
  devices_t *outdevices;      /* Streams to write to. */
  int buffer_size;            /* Size of the buffer in chunks. */
} ogg123_options_t;           /* Changed in 0.6 to be non-static */

/* This goes here because it relies on some of the above. */
#include "buffer.h"

devices_t *append_device(devices_t * devices_list, int driver_id,
                         ao_option_t * options);
void devices_write(void *ptr, size_t size, devices_t * d);
void usage(void);
int add_option(ao_option_t ** op_h, const char *optstring);
int get_default_device(void);
void play_file(ogg123_options_t opt, buf_t *buffer);
int get_tcp_socket(void); /* Will be going soon. */
FILE *http_open(char *server, int port, char *path); /* ditto */

#endif /* !defined(__OGG123_H) */
