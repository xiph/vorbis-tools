
#ifndef __AUDIO_H
#define __AUDIO_H

#include "encode.h"
#include <stdio.h>

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

int wav_open(FILE *in, oe_enc_opt *opt);
int raw_open(FILE *in, oe_enc_opt *opt);
void wav_close(void *);
void raw_close(void *);

long wav_read_stereo(void *, float **buffer, int samples);
long wav_read_mono(void *, float **buffer, int samples);
long raw_read_stereo(void *, float **buffer, int samples);

#endif /* __AUDIO_H */

