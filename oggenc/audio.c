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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "audio.h"
#include "platform.h"

#define WAV_HEADER_SIZE 44

/* Macros to read header data */
#define READ_U32(buf) \
	(((buf)[3]<<24)|((buf)[2]<<16)|((buf)[1]<<8)|((buf)[0]&0xff));

#define READ_U16(buf) \
	(((buf)[1]<<8)|((buf)[0]&0xff));

/* Define the supported formats here */
input_format formats[] = {
	{wav_id, 12, wav_open, wav_close, "wav", "WAV file reader"},
	{NULL, 0, NULL, NULL, NULL, NULL}
};

input_format *open_audio_file(FILE *in, oe_enc_opt *opt)
{
	int j=0;
	unsigned char *buf=NULL;
	int buf_size=0, buf_filled=0;
	int size,ret;

	while(formats[j].id_func)
	{
		size = formats[j].id_data_len;
		if(size >= buf_size)
		{
			buf = realloc(buf, size);
			buf_size = size;
		}

		if(buf_size > buf_filled)
		{
			ret = fread(buf+buf_filled, 1, buf_size-buf_filled, in);
			buf_filled += ret;

			if(buf_filled != buf_size)
			{ /* File truncated */
				buf_size = buf_filled;
				j++;
				continue;
			}
		}

		if(formats[j].id_func(buf, size))
		{
			/* ok, we now have something that can handle the file */
			if(formats[j].open_func(in, opt))
				return &formats[j];
		}
		j++;
	}

	return NULL;
}


static int find_chunk(FILE *in, char *type, unsigned int *len)
{
	unsigned char buf[8];

	while(1)
	{
		if(fread(buf,1,8,in) < 8) /* Suck down a chunk specifier */
		{
			fprintf(stderr, "Warning: Unexpected EOF in reading WAV header\n");
			return 0; /* EOF before reaching the appropriate chunk */
		}

		if(memcmp(buf, type, 4))
		{
			*len = READ_U32(buf+4);
			if(fseek(in, *len, SEEK_CUR)) /* Skip the rest of the chunk */
			{
				/* seek failed. Do it the hard way */
				unsigned char buf2[1024];
				int seek_needed = *len, seeked;
				while(seek_needed>0)
				{
					seeked = fread(buf2,1,seek_needed>1024?1024:seek_needed,in);
					if(!seeked)
						return 0; /* Couldn't read more, can't read file */
					else
						seek_needed -= seeked;
				}
			}
			buf[4] = 0;
			fprintf(stderr, "Skipping chunk of type \"%s\", length %d\n", buf, *len);
		}
		else
		{
			*len = READ_U32(buf+4);
			return 1;
		}
	}
}

int wav_id(unsigned char *buf, int len)
{
	unsigned int flen;
	
	if(len<12) return 0; /* Something screwed up */

	if(memcmp(buf, "RIFF", 4))
		return 0; /* Not wave */

	flen = READ_U32(buf+4); /* We don't use this */

	if(memcmp(buf+8, "WAVE",4))
		return 0; /* RIFF, but not wave */

	return 1;
}

int wav_open(FILE *in, oe_enc_opt *opt)
{
	unsigned char buf[16];
	unsigned int len;
	wav_fmt format;
	wavfile *wav = malloc(sizeof(wavfile));

	/* Ok. At this point, we know we have a WAV file. Now we have to detect
	 * whether we support the subtype, and we have to find the actual data
	 */

	if(!find_chunk(in, "fmt ", &len))
		return 0; /* EOF */

	if(len!=16) 
	{
		fprintf(stderr, "Warning: Unrecognised format chunk in WAV header\n");
		return 0; /* Weird format chunk */
	}

	if(fread(buf,1,16,in) < 16)
	{
		fprintf(stderr, "Warning: Unexpected EOF in reading WAV header\n");
		return 0;
	}

	format.format =      READ_U16(buf); 
	format.channels =    READ_U16(buf+2); 
	format.samplerate =  READ_U32(buf+4);
	format.bytespersec = READ_U32(buf+8);
	format.align =       READ_U16(buf+12);
	format.samplesize =  READ_U16(buf+14);

	if(!find_chunk(in, "data", &len))
		return 0; /* EOF */

	if( format.format == 1 &&
		(format.channels == 2 || format.channels == 1) &&
		format.align == format.channels*2 && /* We could deal with this one pretty easily */
		format.samplesize == 16)
	{
		if(format.samplerate != 44100)
			fprintf(stderr, "Warning: Vorbis is currently not tuned for input\n"
							" at other than 44.1kHz. Quality may be somewhat\n"
							" degraded.\n");
		/* OK, good - we have the one supported format,
		   now we want to find the size of the file */
		opt->rate = format.samplerate;
		opt->channels = format.channels;
		if(opt->channels ==2)
			opt->read_samples = wav_read_stereo;
		else
			opt->read_samples = wav_read_mono;

		wav->f = in;
		wav->samplesread = 0;

		if(len)
		{
			opt->total_samples_per_channel = len/(format.channels*2);
			wav->totalsamples = len/(format.channels*2);
		}
		else
		{
			long pos;
			pos = ftell(in);
			if(fseek(in, 0, SEEK_END) == -1)
			{
				opt->total_samples_per_channel = 0; /* Give up */
				wav->totalsamples = 0;
			}
			else
			{
				opt->total_samples_per_channel = (ftell(in) - pos)/(format.channels*2);
				wav->totalsamples = len/(format.channels*2);
				fseek(in,pos, SEEK_SET);
			}
		}
		opt->readdata = (void *)wav;
		return 1;
	}
	else
	{
		fprintf(stderr, "ERROR: Wav file is unsupported subformat (must be 44.1kHz/16bit, this is %f/%dbit, %s)\n", (double)format.samplerate/1000, format.samplesize, (format.channels ==1)?"mono":"stereo");
		return 0;
	}
}

long wav_read_stereo(void *in, float **buffer, int samples)
{
	signed char *buf = alloca(samples*4);
	wavfile *f = (wavfile *)in;
	long bytes_read = fread(buf, 1, samples*4, f->f);
	int i;

	if(f->totalsamples && f->samplesread + bytes_read/4 > f->totalsamples)
		bytes_read = 4*(f->totalsamples - f->samplesread);
	f->samplesread += bytes_read/4;
		

	for(i = 0; i < bytes_read/4; i++)
	{
		buffer[0][i] = ((buf[i*4+1]<<8) | (((int)buf[i*4]) & 0xff))/32768.0;
		buffer[1][i] = ((buf[i*4+3]<<8) | (((int)buf[i*4+2]) & 0xff))/32768.0;
	}

	return bytes_read/4;
}

long raw_read_stereo(void *in, float **buffer, int samples)
{
	signed char *buf = alloca(samples*4);
	long bytes_read = fread(buf,1,samples*4, (FILE *)in);
	int i;

	for(i=0;i<bytes_read/4; i++)
	{
		buffer[0][i] = ((buf[i*4+1]<<8) | (((int)buf[i*4]) & 0xff))/32768.0;
		buffer[1][i] = ((buf[i*4+3]<<8) | (((int)buf[i*4+2]) & 0xff))/32768.0;
	}

	return bytes_read/4;
}

long wav_read_mono(void *in, float **buffer, int samples)
{
	signed char *buf = alloca(samples*2);
	wavfile *f = (wavfile *)in;
	long bytes_read = fread(buf, 1, samples*2, f->f);
	int i;

	if(f->totalsamples && f->samplesread + bytes_read/2 > f->totalsamples)
		bytes_read = 2*(f->totalsamples - f->samplesread);
	f->samplesread += bytes_read/2;


	for(i=0;i<bytes_read/2; i++)
		buffer[0][i] = ((buf[i*2+1]<<8) | (((int)buf[i*2]) & 0xff))/32768.0;

	return bytes_read/2;
}

void wav_close(void *info)
{
	wavfile *f = (wavfile *)info;

	free(f);
}

int raw_open(FILE *in, oe_enc_opt *opt)
{
	opt->rate = 44100; /* we assume this */
	opt->channels = 2;
	opt->readdata = (void *)in;
	opt->read_samples = raw_read_stereo; /* it's the same, currently */
	opt->total_samples_per_channel = 0; /* raw mode, don't bother */
	return 1;
}
