/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * This file handles invalid streams.
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2020      Philipp Schafft <lion@lion.leolix.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ogg/ogg.h>

#include "private.h"

static void process_invalid(stream_processor *stream, ogg_page *page)
{
    /* This is for invalid streams. */
}

void invalid_start(stream_processor *stream)
{
    stream->process_end = NULL;
    stream->type = "invalid";
    stream->process_page = process_invalid;
}
