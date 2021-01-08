/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * This file handles codecs we have no specific handling for.
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2020-2021 Philipp Schafft <lion@lion.leolix.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <ogg/ogg.h>

#include "i18n.h"

#include "private.h"

typedef struct {
} misc_skeleton_info;
    
static void skeleton_process(stream_processor *stream, ogg_page *page)
{
    misc_skeleton_info *self = stream->data;

    ogg_stream_pagein(&stream->os, page);

    while (1) {
        ogg_packet packet;
        int res = ogg_stream_packetout(&stream->os, &packet);

        if (res < 0) {
           warn(_("WARNING: discontinuity in stream (%d)\n"), stream->num);
           continue;
        } else if (res == 0) {
            break;
        }

        // TODO.
    }
}

static void skeleton_end(stream_processor *stream)
{
    misc_skeleton_info *self = stream->data;

    free(stream->data);
}

void skeleton_start(stream_processor *stream)
{
    stream->type = "skeleton";
    stream->process_page = skeleton_process;
    stream->process_end = skeleton_end;
    stream->data = calloc(1, sizeof(misc_skeleton_info));
}
