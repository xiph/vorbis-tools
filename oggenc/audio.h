
#ifndef __AUDIO_H
#define __AUDIO_H

#include "encode.h"
#include <stdio.h>

typedef struct
{
	int (*id_func)(unsigned char *buf, int len); /* Returns true if can load file */
	int id_data_len; /* Amount of data needed to id whether this can load the file */
	int (*open_func)(FILE *in, oe_enc_opt *opt);
	void (*close_func)(void *);
	char *format;
	char *description;
} input_format;


typedef struct {
	short format;
	short channels;
	int samplerate;
	int bytespersec;
	short align;
	short samplesize;
} wav_fmt;

typedef struct {
	long totalsamples;
	long samplesread;
	FILE *f;
} wavfile;

input_format *open_audio_file(FILE *in, oe_enc_opt *opt);

int wav_open(FILE *in, oe_enc_opt *opt);
int raw_open(FILE *in, oe_enc_opt *opt);
int wav_id(unsigned char *buf, int len);
void wav_close(void *);
void raw_close(void *);

long wav_read_stereo(void *, float **buffer, int samples);
long wav_read_mono(void *, float **buffer, int samples);
long raw_read_stereo(void *, float **buffer, int samples);

#endif /* __AUDIO_H */

