/* OggEnc
 **
 ** This program is distributed under the GNU General Public License, version 2.
 ** A copy of this license is included with this source.
 **
 ** Copyright 2002, Stan Seibert <volsung@xiph.org>
 **
 **/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <FLAC/metadata.h>
#include "audio.h"
#include "flac.h"
#include "i18n.h"
#include "platform.h"
#include "resample.h"

#define DEFAULT_FLAC_FRAME_SIZE 4608

FLAC__StreamDecoderReadStatus easyflac_read_callback(const EasyFLAC__StreamDecoder *decoder, FLAC__byte buffer[], unsigned *bytes, void *client_data);
FLAC__StreamDecoderWriteStatus easyflac_write_callback(const EasyFLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
void easyflac_metadata_callback(const EasyFLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
void easyflac_error_callback(const EasyFLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

void resize_buffer(flacfile *flac, int newchannels, int newsamples);
void copy_comments (vorbis_comment *v_comments, FLAC__StreamMetadata_VorbisComment *f_comments);


int flac_id(unsigned char *buf, int len)
{
	if (len < 4) return 0;

	return memcmp(buf, "fLaC", 4) == 0;
}


int oggflac_id(unsigned char *buf, int len)
{
	if (len < 32) return 0;

	return memcmp(buf, "OggS", 4) == 0 && flac_id(buf+28, len - 28);
}


int flac_open(FILE *in, oe_enc_opt *opt, unsigned char *oldbuf, int buflen)
{
	flacfile *flac = malloc(sizeof(flacfile));

	flac->decoder = NULL;
	flac->channels = 0;
	flac->rate = 0;
	flac->totalsamples = 0;
	flac->comments = NULL;
	flac->in = NULL;
	flac->eos = 0;

	/* Setup empty audio buffer that will be resized on first frame 
	   callback */
	flac->buf = NULL;
	flac->buf_len = 0;
	flac->buf_start = 0;
	flac->buf_fill = 0;

	/* Copy old input data over */
	flac->oldbuf = malloc(buflen);
	flac->oldbuf_len = buflen;
	memcpy(flac->oldbuf, oldbuf, buflen);
	flac->oldbuf_start = 0;

	/* Need to save FILE pointer for read callback */
	flac->in = in;

	/* Setup FLAC decoder */
	flac->decoder = EasyFLAC__stream_decoder_new(oggflac_id(oldbuf, buflen));
	EasyFLAC__set_client_data(flac->decoder, flac);
	EasyFLAC__set_read_callback(flac->decoder, &easyflac_read_callback);
	EasyFLAC__set_write_callback(flac->decoder, &easyflac_write_callback);
	EasyFLAC__set_metadata_callback(flac->decoder, &easyflac_metadata_callback);
	EasyFLAC__set_error_callback(flac->decoder, &easyflac_error_callback);
	EasyFLAC__set_metadata_respond(flac->decoder, FLAC__METADATA_TYPE_STREAMINFO);
	EasyFLAC__set_metadata_respond(flac->decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);
	EasyFLAC__init(flac->decoder);
	
	/* Callback will set the total samples and sample rate */
	EasyFLAC__process_until_end_of_metadata(flac->decoder);

	/* Callback will set the number of channels and resize the 
	   audio buffer */
	EasyFLAC__process_single(flac->decoder);
	
	/* Copy format info for caller */
	opt->rate = flac->rate;
	opt->channels = flac->channels;
	/* flac->total_samples_per_channel was already set by metadata
	   callback when metadata was processed. */
	opt->total_samples_per_channel = flac->totalsamples;
	/* Copy Vorbis-style comments from FLAC file (read in metadata 
	   callback)*/
	if (flac->comments != NULL && opt->copy_comments)
		copy_comments(opt->comments, &flac->comments->data.vorbis_comment);
	opt->read_samples = flac_read;
	opt->readdata = (void *)flac;

	return 1;
}


long flac_read(void *in, float **buffer, int samples)
{
	flacfile *flac = (flacfile *)in;
	long realsamples = 0;
	FLAC__bool ret;
	int i,j;
	while (realsamples < samples)
	{
		if (flac->buf_fill > 0)
		{
			int copy = flac->buf_fill < (samples - realsamples) ?
				flac->buf_fill : (samples - realsamples);
			
			for (i = 0; i < flac->channels; i++)
				for (j = 0; j < copy; j++)
					buffer[i][j+realsamples] = 
						flac->buf[i][j+flac->buf_start];
			flac->buf_start += copy;
			flac->buf_fill -= copy;
			realsamples += copy;
		}
		else if (!flac->eos)
		{
			ret = EasyFLAC__process_single(flac->decoder);
			if (!ret ||
			    EasyFLAC__get_state(flac->decoder)
			    == FLAC__STREAM_DECODER_END_OF_STREAM)
				flac->eos = 1;  /* Bail out! */
		} else
			break;
	}

	return realsamples;
}


void flac_close(void *info)
{
	int i;
	flacfile *flac =  (flacfile *) info;

	for (i = 0; i < flac->channels; i++)
		free(flac->buf[i]);

	free(flac->buf);
	free(flac->oldbuf);
	free(flac->comments);
	EasyFLAC__finish(flac->decoder);
	EasyFLAC__stream_decoder_delete(flac->decoder);
	free(flac);
}


FLAC__StreamDecoderReadStatus easyflac_read_callback(const EasyFLAC__StreamDecoder *decoder, FLAC__byte buffer[], unsigned *bytes, void *client_data)
{
	flacfile *flac = (flacfile *) client_data;
	int i = 0;
	int oldbuf_fill = flac->oldbuf_len - flac->oldbuf_start;
	
	/* Immediately return if errors occured */
	if(feof(flac->in))
	{
		*bytes = 0;
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}
	else if(ferror(flac->in))
	{
		*bytes = 0;
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}


	if(oldbuf_fill > 0) 
	{
		int copy;
		
		copy = oldbuf_fill < (*bytes - i) ? oldbuf_fill : (*bytes - i);
		memcpy(buffer + i, flac->oldbuf, copy);
		i += copy;
		flac->oldbuf_start += copy;
	}
	
	if(i < *bytes)
		i += fread(buffer+i, sizeof(FLAC__byte), *bytes - i, flac->in);

	*bytes = i;
	
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;	 
}

FLAC__StreamDecoderWriteStatus easyflac_write_callback(const EasyFLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	flacfile *flac = (flacfile *) client_data;
	int samples = frame->header.blocksize;
	int channels = frame->header.channels;
	int bits_per_sample = frame->header.bits_per_sample;
	int i, j;

	resize_buffer(flac, channels, samples);

	for (i = 0; i < channels; i++)
		for (j = 0; j < samples; j++)
			flac->buf[i][j] = buffer[i][j] / 
				 (float) (1 << (bits_per_sample - 1));

	flac->buf_start = 0;
	flac->buf_fill = samples;
 
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void easyflac_metadata_callback(const EasyFLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	flacfile *flac = (flacfile *) client_data;

	switch (metadata->type)
	{
	case FLAC__METADATA_TYPE_STREAMINFO:
		flac->totalsamples = metadata->data.stream_info.total_samples;
		flac->rate = metadata->data.stream_info.sample_rate;
		break;

	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		flac->comments = FLAC__metadata_object_clone(metadata);
		break;
	default:
		break;
	}
}

void easyflac_error_callback(const EasyFLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	flacfile *flac = (flacfile *) client_data;

}


void resize_buffer(flacfile *flac, int newchannels, int newsamples)
{
	int i;

	if (newchannels == flac->channels && newsamples == flac->buf_len)
	{
		flac->buf_start = 0;
		flac->buf_fill = 0;
		return;
	}


	/* Not the most efficient approach, but it is easy to follow */
	if(newchannels != flac->channels)
	{
		/* Deallocate all of the sample vectors */
		for (i = 0; i < flac->channels; i++)
			free(flac->buf[i]);

		flac->buf = realloc(flac->buf, sizeof(float*) * newchannels);
		flac->channels = newchannels;

	}

	for (i = 0; i < newchannels; i++)
		flac->buf[i] = malloc(sizeof(float) * newsamples);

	flac->buf_len = newsamples;
	flac->buf_start = 0;
	flac->buf_fill = 0;
}

void copy_comments (vorbis_comment *v_comments, FLAC__StreamMetadata_VorbisComment *f_comments)
{
	int i;

	for (i = 0; i < f_comments->num_comments; i++)
	{
		char *comment = malloc(f_comments->comments[i].length + 1);
		memset(comment, '\0', f_comments->comments[i].length + 1);
		strncpy(comment, f_comments->comments[i].entry, f_comments->comments[i].length);
		vorbis_comment_add(v_comments, comment);
		free(comment);
	}
}

