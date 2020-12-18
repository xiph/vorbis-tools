/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS SOURCE IS GOVERNED BY *
 * THE GNU PUBLIC LICENSE 2, WHICH IS INCLUDED WITH THIS SOURCE.    *
 * PLEASE READ THESE TERMS BEFORE DISTRIBUTING.                     *
 *                                                                  *
 * THE Ogg123 SOURCE CODE IS (C) COPYRIGHT 2000-2003                *
 * by Stan Seibert <volsung@xiph.org> AND OTHER CONTRIBUTORS        *
 * http://www.xiph.org/                                             *
 *                                                                  *
 ********************************************************************

 last mod: $Id: opus_format.c 16825 2010-01-27 04:14:08Z xiphmont $

 ********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ogg/ogg.h>
#include <opus/opusfile.h>
#include "transport.h"
#include "format.h"
#include "vorbis_comments.h"
#include "utf8.h"
#include "i18n.h"

typedef struct opf_private_t {
  OggOpusFile *of;
  const OpusTags *ot;
  const OpusHead *oh;
  int current_section;

  int bos; /* At beginning of logical bitstream */

  decoder_stats_t stats;
} opf_private_t;

/* Forward declarations */
format_t opus_format;
OpusFileCallbacks opusfile_callbacks;


void print_opus_stream_info (decoder_t *decoder);
void print_opus_comments (const OpusTags *ot, decoder_callbacks_t *cb, 
			    void *callback_arg);


/* ----------------------------------------------------------- */


int opf_can_decode (data_source_t *source)
{
  char buf[36];
  int len;

  len = source->transport->peek(source, buf, sizeof(char), 36);

  if (len >= 32 && memcmp(buf, "OggS", 4) == 0
      && memcmp(buf+28, "OpusHead", 8) == 0) /* 3 trailing spaces */
    return 1;
  else
    return 0;
}


decoder_t* opf_init (data_source_t *source, ogg123_options_t *ogg123_opts,
		     audio_format_t *audio_fmt,
		     decoder_callbacks_t *callbacks, void *callback_arg)
{
  decoder_t *decoder;
  opf_private_t *private;
  int ret;


  /* Allocate data source structures */
  decoder = malloc(sizeof(decoder_t));
  private = malloc(sizeof(opf_private_t));

  if (decoder != NULL && private != NULL) {
    decoder->source = source;
    decoder->actual_fmt = decoder->request_fmt = *audio_fmt;
    decoder->format = &opus_format;
    decoder->callbacks = callbacks;
    decoder->callback_arg = callback_arg;
    decoder->private = private;

    private->bos = 1;
    private->current_section = -1;

    private->stats.total_time = 0.0;
    private->stats.current_time = 0.0;
    private->stats.instant_bitrate = 0;
    private->stats.avg_bitrate = 0;

  } else {
    fprintf(stderr, _("ERROR: Out of memory.\n"));
    exit(1);
  }

  /* Initialize opusfile decoder */

  private->of = op_open_callbacks (decoder, &opusfile_callbacks, NULL, 0, &ret);

  if (private->of == NULL) {
    free(private);
/*    free(source);     nope.  caller frees. */
    return NULL;
  }

  return decoder;
}


int opf_read (decoder_t *decoder, void *ptr, int nbytes, int *eos,
	      audio_format_t *audio_fmt)
{
  opf_private_t *priv = decoder->private;
  decoder_callbacks_t *cb = decoder->callbacks;
  int bytes_read = 0;
  int ret;
  int old_section;

  /* Read comments and audio info at the start of a logical bitstream */
  if (priv->bos) {
    priv->ot = op_tags(priv->of, -1);
    priv->oh = op_head(priv->of, -1);

    decoder->actual_fmt.channels = priv->oh->channel_count;
    decoder->actual_fmt.rate = 48000;

    switch(decoder->actual_fmt.channels){
    case 1:
      decoder->actual_fmt.matrix="M";
      break;
    case 2:
      decoder->actual_fmt.matrix="L,R";
      break;
    case 3:
      decoder->actual_fmt.matrix="L,C,R";
      break;
    case 4:
      decoder->actual_fmt.matrix="L,R,BL,BR";
      break;
    case 5:
      decoder->actual_fmt.matrix="L,C,R,BL,BR";
      break;
    case 6:
      decoder->actual_fmt.matrix="L,C,R,BL,BR,LFE";
      break;
    case 7:
      decoder->actual_fmt.matrix="L,C,R,SL,SR,BC,LFE";
      break;
    case 8:
      decoder->actual_fmt.matrix="L,C,R,SL,SR,BL,BR,LFE";
      break;
    default:
      decoder->actual_fmt.matrix=NULL;
      break;
    }


    print_opus_stream_info(decoder);
    print_opus_comments(priv->ot, cb, decoder->callback_arg);
    priv->bos = 0;
  }

  *audio_fmt = decoder->actual_fmt;

  /* Attempt to read as much audio as is requested */
  while (nbytes >= audio_fmt->word_size * audio_fmt->channels) {

    old_section = priv->current_section;
    ret = op_read(priv->of, ptr, nbytes/2, NULL);

    if (ret == 0) {

      /* EOF */
      *eos = 1;
      break;

    } else if (ret == OP_HOLE) {

      if (cb->printf_error != NULL)
	cb->printf_error(decoder->callback_arg, INFO,
			   _("--- Hole in the stream; probably harmless\n"));

    } else if (ret < 0) {

      if (cb->printf_error != NULL)
	cb->printf_error(decoder->callback_arg, ERROR,
			 _("=== Vorbis library reported a stream error.\n"));

      /* EOF */
      *eos = 1;
      break;
    } else {

      bytes_read += ret*2*audio_fmt->channels;
      ptr = (void *)((unsigned char *)ptr + ret*2*audio_fmt->channels);
      nbytes -= ret*2*audio_fmt->channels;

      /* did we enter a new logical bitstream? */
      if (old_section != priv->current_section && old_section != -1) {
	
	*eos = 1;
	priv->bos = 1; /* Read new headers next time through */
	break;
      }
    }

  }

  return bytes_read;
}


int opf_seek (decoder_t *decoder, double offset, int whence)
{
  opf_private_t *priv = decoder->private;
  int ret;
  int cur;
  int samples = offset * 48000;

  if (whence == DECODER_SEEK_CUR) {
    cur = op_pcm_tell(priv->of);
    if (cur >= 0)
      samples += cur;
    else
      return 0;
  }

  ret = op_pcm_seek(priv->of, samples);
  if (ret == 0)
    return 1;
  else
    return 0;
}


decoder_stats_t *opf_statistics (decoder_t *decoder)
{
  opf_private_t *priv = decoder->private;
  long instant_bitrate;
  long avg_bitrate;

  /* ov_time_tell() doesn't work on non-seekable streams, so we use
     ov_pcm_tell()  */
  priv->stats.total_time = (double) op_pcm_total(priv->of, -1) /
    (double) decoder->actual_fmt.rate;
  priv->stats.current_time = (double) op_pcm_tell(priv->of) / 
    (double) decoder->actual_fmt.rate;

  /* opusfile returns 0 when no bitrate change has occurred */
  instant_bitrate = op_bitrate_instant(priv->of);
  if (instant_bitrate > 0)
    priv->stats.instant_bitrate = instant_bitrate;

  avg_bitrate = op_bitrate(priv->of, priv->current_section);
  /* Catch error case caused by non-seekable stream */
  priv->stats.avg_bitrate = avg_bitrate > 0 ? avg_bitrate : 0;


  return malloc_decoder_stats(&priv->stats);
}


void opf_cleanup (decoder_t *decoder)
{
  opf_private_t *priv = decoder->private;

  op_free(priv->of);

  free(decoder->private);
  free(decoder);
}


format_t opus_format = {
  "oggopus",
  &opf_can_decode,
  &opf_init,
  &opf_read,
  &opf_seek,
  &opf_statistics,
  &opf_cleanup,
};


/* ------------------- Opusfile Callbacks ----------------- */

int opusfile_cb_read (void *stream, unsigned char *ptr, int nbytes)
{
  decoder_t *decoder = stream;

  return decoder->source->transport->read(decoder->source, ptr, 1, nbytes);
}

int opusfile_cb_seek (void *arg, opus_int64 offset, int whence)
{
  decoder_t *decoder = arg;

  return decoder->source->transport->seek(decoder->source, offset, whence);
}

int opusfile_cb_close (void *arg)
{
  return 1; /* Ignore close request so transport can be closed later */
}

opus_int64 opusfile_cb_tell (void *arg)
{
  decoder_t *decoder = arg;

  return decoder->source->transport->tell(decoder->source);
}


OpusFileCallbacks opusfile_callbacks = {
  &opusfile_cb_read,
  &opusfile_cb_seek,
  &opusfile_cb_tell,
  &opusfile_cb_close
};


/* ------------------- Private functions -------------------- */


void print_opus_stream_info (decoder_t *decoder)
{
  opf_private_t *priv = decoder->private;
  decoder_callbacks_t *cb = decoder->callbacks;


  if (cb == NULL || cb->printf_metadata == NULL)
    return;

  cb->printf_metadata(decoder->callback_arg, 2,
		      _("Ogg Opus stream: %d channel, 48000 Hz"),
		      priv->oh->channel_count);

  cb->printf_metadata(decoder->callback_arg, 3,
		      _("Vorbis format: Version %d"), 
		      priv->oh->version);

  cb->printf_metadata(decoder->callback_arg, 3,
		      _("Encoded by: %s"), priv->ot->vendor);
}

void print_opus_comments (const OpusTags *ot, decoder_callbacks_t *cb, 
			    void *callback_arg)
{
  int i;
  char *temp = NULL;
  int temp_len = 0;

  for (i = 0; i < ot->comments; i++) {

    /* Gotta null terminate these things */
    if (temp_len < ot->comment_lengths[i] + 1) {
      temp_len = ot->comment_lengths[i] + 1;
      temp = realloc(temp, sizeof(char) * temp_len);
    }

    strncpy(temp, ot->user_comments[i], ot->comment_lengths[i]);
    temp[ot->comment_lengths[i]] = '\0';

    print_vorbis_comment(temp, cb, callback_arg);
  }

  free(temp);
}
