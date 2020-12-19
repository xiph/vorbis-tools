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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <ogg/ogg.h>

#include "i18n.h"

#include "private.h"

typedef struct {
    bool seen_speex_header;
    bool seen_vorbis_comments;
    bool seen_data;
} misc_speex_info;

static inline const char *mode_name(const unsigned char *in)
{
    switch (read_u32le(in)) {
        case 0:
            return "narrowband";
            break;
        case 1:
            return "wideband";
            break;
        case 2:
            return "ultra-wideband";
            break;
        default:
            return "<invalid>";
            break;
    }
}

static void speex_process_header(stream_processor *stream, misc_speex_info *self, ogg_packet *packet)
{
    if (packet->bytes == 80) {
        if (read_u32le(&(packet->packet[32])) == 80) {
            char version[21];
            memcpy(version, &(packet->packet[8]), 20);
            version[20] = 0;
            info(_("Version: %d (%s)\n"), (int)read_u32le(&(packet->packet[28])), version);
            info(_("Mode: %ld (%s)\n"), (long int)read_u32le(&(packet->packet[40])), mode_name(&(packet->packet[40])));
            info(_("Channels: %d\n"), (int)read_u32le(&(packet->packet[48])));
            info(_("Rate: %ld\n\n"), (long int)read_u32le(&(packet->packet[36])));
        } else {
            warn(_("WARNING: invalid Speex header on stream %d: header size does not match packet size\n"), stream->num);
        }
    } else {
        warn(_("WARNING: invalid Speex header on stream %d: packet of wrong size\n"), stream->num);
    }
    self->seen_speex_header = true;
}

static void speex_process_vorbis_comments(stream_processor *stream, misc_speex_info *self, ogg_packet *packet)
{
    size_t end;

    if (handle_vorbis_comments(stream, packet->packet, packet->bytes, &end) == -1)
        warn(_("WARNING: invalid Vorbis comments on stream %d: packet too short\n"), stream->num);

    self->seen_vorbis_comments = true;
}

static void speex_process_data(stream_processor *stream, misc_speex_info *self, ogg_packet *packet)
{
    self->seen_data = true;
}

static void speex_process(stream_processor *stream, ogg_page *page)
{
    misc_speex_info *self = stream->data;

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

        switch (packet.packetno) {
            case 0:
                speex_process_header(stream, self, &packet);
                break;
            case 1:
                speex_process_vorbis_comments(stream, self, &packet);
                break;
            default:
                speex_process_data(stream, self, &packet);
                break;
        }
    }
}

static void speex_end(stream_processor *stream)
{
    misc_speex_info *self = stream->data;

    if (!self->seen_speex_header)
        warn(_("WARNING: stream (%d) did not contain Speex header\n"), stream->num);

    if (!self->seen_vorbis_comments)
        warn(_("WARNING: stream (%d) did not contain Vorbis comments header\n"), stream->num);

    if (!self->seen_data)
        warn(_("WARNING: stream (%d) did not contain data packets\n"), stream->num);

    free(stream->data);
}

void speex_start(stream_processor *stream)
{
    stream->type = "speex";
    stream->process_page = speex_process;
    stream->process_end = speex_end;
    stream->data = calloc(1, sizeof(misc_speex_info));
}
