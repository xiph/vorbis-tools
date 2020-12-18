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

void info(const char *format, ...);
void warn(const char *format, ...);
void error(const char *format, ...);

void check_xiph_comment(stream_processor *stream, int i, const char *comment, int comment_length);

void vorbis_start(stream_processor *stream);
void theora_start(stream_processor *stream);
void kate_start(stream_processor *stream);
void other_start(stream_processor *stream, const char *type);
void invalid_start(stream_processor *stream);

#endif
