/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * This file handles codecs we have no specific handling for.
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

static void process_other(stream_processor *stream, ogg_page *page)
{
    ogg_packet packet;

    ogg_stream_pagein(&stream->os, page);

    while (ogg_stream_packetout(&stream->os, &packet) > 0) {
        /* Should we do anything here? Currently, we don't */
    }
}

void other_start(stream_processor *stream, const char *type)
{
    if (type) {
        stream->type = type;
    } else {
        stream->type = "unknown";
    }
    stream->process_page = process_other;
    stream->process_end = NULL;
}
