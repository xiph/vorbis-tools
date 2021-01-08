/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * This file is a common header for the code files.
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2020      Philipp Schafft <lion@lion.leolix.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifndef __VORBIS_TOOLS__PRIVATE_H__
#define __VORBIS_TOOLS__PRIVATE_H__

#include "picture.h"

typedef struct _stream_processor {
    void (*process_page)(struct _stream_processor *, ogg_page *);
    void (*process_end)(struct _stream_processor *);
    int isillegal;
    int constraint_violated;
    int shownillegal;
    int isnew;
    long seqno;
    int lostseq;

    int start;
    int end;

    int num;
    const char *type;

    ogg_uint32_t serial; /* must be 32 bit unsigned */
    ogg_stream_state os;
    void *data;
} stream_processor;

extern int printlots;

static inline ogg_uint16_t read_u16le(const unsigned char *in)
{
    return in[0] | (in[1] << 8);
}

static inline ogg_uint32_t read_u32le(const unsigned char *in)
{
    return in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
}

void info(const char *format, ...);
void warn(const char *format, ...);
void error(const char *format, ...);

void print_summary(stream_processor *stream, size_t bytes, double time);
int handle_vorbis_comments(stream_processor *stream, const unsigned char *in, size_t length, size_t *end);
void check_xiph_comment(stream_processor *stream, int i, const char *comment, int comment_length);
void check_flac_picture(flac_picture_t *picture, const char *prefix);

void vorbis_start(stream_processor *stream);
void theora_start(stream_processor *stream);
void kate_start(stream_processor *stream);
void opus_start(stream_processor *stream);
void speex_start(stream_processor *stream);
void flac_start(stream_processor *stream);
void skeleton_start(stream_processor *stream);
void other_start(stream_processor *stream, const char *type);
void invalid_start(stream_processor *stream);

#endif
