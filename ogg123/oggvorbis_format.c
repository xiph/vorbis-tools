/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS SOURCE IS GOVERNED BY *
 * THE GNU PUBLIC LICENSE 2, WHICH IS INCLUDED WITH THIS SOURCE.    *
 * PLEASE READ THESE TERMS BEFORE DISTRIBUTING.                     *
 *                                                                  *
 * THE Ogg123 SOURCE CODE IS (C) COPYRIGHT 2000-2001                *
 * by Stan Seibert <volsung@xiph.org> AND OTHER CONTRIBUTORS        *
 * http://www.xiph.org/                                             *
 *                                                                  *
 ********************************************************************

 last mod: $Id: oggvorbis_format.c,v 1.7 2002/04/10 02:40:42 volsung Exp $

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include "transport.h"
#include "format.h"
#include "utf8.h"
#include "i18n.h"


typedef struct ovf_private_t {
  OggVorbis_File vf;
  vorbis_comment *vc;
  vorbis_info *vi;
  int current_section;

  int bos; /* At beginning of logical bitstream */

  decoder_stats_t stats;
} ovf_private_t;

/* Forward declarations */
format_t oggvorbis_format;
ov_callbacks vorbisfile_callbacks;


/* Vorbis comment keys that need special formatting. */
struct {
  char *key;         /* includes the '=' for programming convenience */
  char *formatstr;   /* formatted output */
} vorbis_comment_keys[] = {
  {"TRACKNUMBER=", N_("Track number: %s")},
  {"RG_RADIO=", N_("ReplayGain (Track): %s")},
  {"RG_AUDIOPHILE=", N_("ReplayGain (Album): %s")},
  {"RG_PEAK=", N_("ReplayGain Peak: %s")},
  {"TRACKNUMBER=", N_("Track number: %s")},  
  {"COPYRIGHT=", N_("Copyright %s")},
  {NULL, N_("Comment: %s")}
};


/* Private functions declarations */
char *lookup_comment_formatstr (char *comment, int *offset);
void print_stream_comments (decoder_t *decoder);
void print_stream_info (decoder_t *decoder);


/* ----------------------------------------------------------- */


int ovf_can_decode (data_source_t *source)
{
  return 1;  /* The file transport is tested last, so always try it */
}


decoder_t* ovf_init (data_source_t *source, ogg123_options_t *ogg123_opts,
		     audio_format_t *audio_fmt,
		     decoder_callbacks_t *callbacks, void *callback_arg)
{
  decoder_t *decoder;
  ovf_private_t *private;
  int ret;


  /* Allocate data source structures */
  decoder = malloc(sizeof(decoder_t));
  private = malloc(sizeof(ovf_private_t));

  if (decoder != NULL && private != NULL) {
    decoder->source = source;
    decoder->actual_fmt = decoder->request_fmt = *audio_fmt;
    decoder->format = &oggvorbis_format;
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
    fprintf(stderr, _("Error: Out of memory.\n"));
    exit(1);
  }

  /* Initialize vorbisfile decoder */
  
  ret = ov_open_callbacks (decoder, &private->vf, NULL, 0, 
			   vorbisfile_callbacks);

  if (ret < 0) {
    free(private);
/*    free(source);     nope.  caller frees. */ 
    return NULL;
  }

  return decoder;
}


int ovf_read (decoder_t *decoder, void *ptr, int nbytes, int *eos,
	      audio_format_t *audio_fmt)
{
  ovf_private_t *priv = decoder->private;
  decoder_callbacks_t *cb = decoder->callbacks;
  int bytes_read = 0;
  int ret;
  int old_section;

  /* Read comments and audio info at the start of a logical bitstream */
  if (priv->bos) {
    priv->vc = ov_comment(&priv->vf, -1);
    priv->vi = ov_info(&priv->vf, -1);

    decoder->actual_fmt.rate = priv->vi->rate;
    decoder->actual_fmt.channels = priv->vi->channels;

    print_stream_comments(decoder);
    print_stream_info(decoder);
    priv->bos = 0;
  }

  *audio_fmt = decoder->actual_fmt;

  /* Attempt to read as much audio as is requested */
  while (nbytes > 0) {

    old_section = priv->current_section;
    ret = ov_read(&priv->vf, ptr, nbytes, audio_fmt->big_endian,
		  audio_fmt->word_size, audio_fmt->signed_sample,
		  &priv->current_section);

    if (ret == 0) {

      /* EOF */
      *eos = 1;
      break;

    } else if (ret == OV_HOLE) {
      
      if (cb->printf_error != NULL)
	cb->printf_error(decoder->callback_arg, INFO,
			   _("--- Hole in the stream; probably harmless\n"));
    
    } else if (ret < 0) {
      
      if (cb->printf_error != NULL)
	cb->printf_error(decoder->callback_arg, ERROR,
			 _("=== Vorbis library reported a stream error.\n"));
      
    } else {
      
      bytes_read += ret;
      ptr = (void *)((unsigned char *)ptr + ret);
      nbytes -= ret;

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


int ovf_seek (decoder_t *decoder, double offset, int whence)
{
  ovf_private_t *priv = decoder->private;
  int ret;
  double cur;

  if (whence == DECODER_SEEK_CUR) {
    cur = ov_time_tell(&priv->vf);
    if (cur >= 0.0)
      offset += cur;
    else
      return 0;
  }

  ret = ov_time_seek(&priv->vf, offset);
  if (ret == 0)
    return 1;
  else
    return 0;
}


decoder_stats_t *ovf_statistics (decoder_t *decoder)
{
  ovf_private_t *priv = decoder->private;
  long instant_bitrate;
  long avg_bitrate;

  /* ov_time_tell() doesn't work on non-seekable streams, so we use
     ov_pcm_tell()  */
  priv->stats.total_time = (double) ov_pcm_total(&priv->vf, -1) /
    (double) decoder->actual_fmt.rate;
  priv->stats.current_time = (double) ov_pcm_tell(&priv->vf) / 
    (double) decoder->actual_fmt.rate;

  /* vorbisfile returns 0 when no bitrate change has occurred */
  instant_bitrate = ov_bitrate_instant(&priv->vf);
  if (instant_bitrate > 0)
    priv->stats.instant_bitrate = instant_bitrate;

  avg_bitrate = ov_bitrate(&priv->vf, priv->current_section);
  /* Catch error case caused by non-seekable stream */
  priv->stats.avg_bitrate = avg_bitrate > 0 ? avg_bitrate : 0;


  return malloc_decoder_stats(&priv->stats);
}


void ovf_cleanup (decoder_t *decoder)
{
  ovf_private_t *priv = decoder->private;

  ov_clear(&priv->vf);

  free(decoder->private);
  free(decoder);
}


format_t oggvorbis_format = {
  "oggvorbis",
  &ovf_can_decode,
  &ovf_init,
  &ovf_read,
  &ovf_seek,
  &ovf_statistics,
  &ovf_cleanup,
};


/* ------------------- Vorbisfile Callbacks ----------------- */

size_t vorbisfile_cb_read (void *ptr, size_t size, size_t nmemb, void *arg)
{
  decoder_t *decoder = arg;

  return decoder->source->transport->read(decoder->source, ptr, size, nmemb);
}

int vorbisfile_cb_seek (void *arg, ogg_int64_t offset, int whence)
{
  decoder_t *decoder = arg;

  return decoder->source->transport->seek(decoder->source, offset, whence);
}

int vorbisfile_cb_close (void *arg)
{
  return 1; /* Ignore close request so transport can be closed later */
}

long vorbisfile_cb_tell (void *arg)
{
  decoder_t *decoder = arg;

  return decoder->source->transport->tell(decoder->source);
}


ov_callbacks vorbisfile_callbacks = {
  &vorbisfile_cb_read,
  &vorbisfile_cb_seek,
  &vorbisfile_cb_close,
  &vorbisfile_cb_tell
};


/* ------------------- Private functions -------------------- */


char *lookup_comment_formatstr (char *comment, int *offset)
{
  int i, j;
  char *s;

  /* Search for special-case formatting */
  for (i = 0; vorbis_comment_keys[i].key != NULL; i++) {

    if ( !strncasecmp (vorbis_comment_keys[i].key, comment,
		       strlen(vorbis_comment_keys[i].key)) ) {

      *offset = strlen(vorbis_comment_keys[i].key);
      s = malloc(strlen(vorbis_comment_keys[i].formatstr) + 1);
      if (s == NULL) {
	fprintf(stderr, _("Error: Out of memory.\n"));
	exit(1);
      };
      strcpy(s, vorbis_comment_keys[i].formatstr);
      return s;
    }

  }

  /* Use default formatting */
  *offset = 0;
  if (i = strcspn(comment, "=")) {
    s = malloc(strlen(comment) + 2);
    if (s == NULL) {
      fprintf(stderr, _("Error: Out of memory.\n"));
      exit(1);
    };
    strncpy(s, comment, i);
    strncpy(s + i, ": ", 2);
    strcpy(s+i+2, comment+i+1);

    /* Capitalize */
    s[0] = toupper(s[0]);
    for (j = 1; j < i; j++) {
      s[j] = tolower(s[j]);
    };
    return s;
  }

  /* Unrecognized comment, use last format string */
  s = malloc(strlen(vorbis_comment_keys[i].formatstr) + 1);
  if (s == NULL) {
    fprintf(stderr, _("Error: Out of memory.\n"));
    exit(1);
  };
  strcpy(s, vorbis_comment_keys[i].formatstr);
  return s;
}


void print_stream_comments (decoder_t *decoder)
{
  ovf_private_t *priv = decoder->private;
  decoder_callbacks_t *cb = decoder->callbacks;
  char *comment, *comment_formatstr;
  int offset;
  int i;

  
  if (cb == NULL || cb->printf_metadata == NULL)
    return;

  for (i = 0; i < priv->vc->comments; i++) {
    char *decoded_value;

    comment = priv->vc->user_comments[i];
    comment_formatstr = lookup_comment_formatstr(comment, &offset);

    if (utf8_decode(comment + offset, &decoded_value) >= 0) {
      cb->printf_metadata(decoder->callback_arg, 1,
			       comment_formatstr, decoded_value);
      free(decoded_value);
    } else
      cb->printf_metadata(decoder->callback_arg, 1,
			       comment_formatstr, comment + offset);
    free(comment_formatstr);
  }
}

void print_stream_info (decoder_t *decoder)
{
  ovf_private_t *priv = decoder->private;
  decoder_callbacks_t *cb = decoder->callbacks;


  if (cb == NULL || cb->printf_metadata == NULL)
    return;
    

  cb->printf_metadata(decoder->callback_arg, 3,
		      _("Version is %d"), 
		      priv->vi->version);
  
  cb->printf_metadata(decoder->callback_arg, 3,
		      _("Bitrate hints: upper=%ld nominal=%ld lower=%ld "
		      "window=%ld"), 
		      priv->vi->bitrate_upper,
		      priv->vi->bitrate_nominal,
		      priv->vi->bitrate_lower,
		      priv->vi->bitrate_window);
  
  cb->printf_metadata(decoder->callback_arg, 2,
		      _("Bitstream is %d channel, %ldHz"),
		      priv->vi->channels,
		      priv->vi->rate);
  
  cb->printf_metadata(decoder->callback_arg, 3,
		      _("Encoded by: %s"), priv->vc->vendor);
}

