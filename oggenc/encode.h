#ifndef __ENCODE_H
#define __ENCODE_H

#include <stdio.h>
#include <vorbis/codec.h>

typedef  long (*audio_read_func)(void *src, float **buffer, int samples);
void *timer_start(void);
double timer_time(void *);
void timer_clear(void *);


typedef struct
{
	char **title;
	int title_count;
	char **artist;
	int artist_count;
	char **album;
	int album_count;
	char **comments;
	int comment_count;

	int quiet;
	int rawmode;

	char *namefmt;
	char *outfile;
	int modenum;
} oe_options;

typedef struct
{
	vorbis_comment *comments;
	vorbis_info    *mode;

	audio_read_func read_samples;
	void *readdata;
	long total_samples_per_channel;
	int channels;
	long rate;

	FILE *out;
	int quiet;
	long serialno;
	char *filename;

	char *artist;
	char *album;
	char *title;
} oe_enc_opt;


int oe_encode(oe_enc_opt *opt);

typedef struct
{
	int (*open_func)(FILE *in, oe_enc_opt *opt);
	void (*close_func)(void *);
	char *format;
	char *description;
} input_format;

#endif /* __ENCODE_H */
