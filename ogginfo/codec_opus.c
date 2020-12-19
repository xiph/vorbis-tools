/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * This file handles opus streams.
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2020      Philipp Schafft <lion@lion.leolix.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <ogg/ogg.h>

#include "i18n.h"

#include "private.h"

typedef struct {
    bool seen_opushead;
    bool seen_opustags;
    bool seen_data;

    ogg_int64_t bytes;

    /* from OpusHead */
    int version;
    int channel_count;
    ogg_uint16_t pre_skip;
    ogg_uint32_t input_sample_rate;
    ogg_int16_t output_gain;
    int mapping_family;

    /* from OpusTags */

    /* from data */
    ogg_int64_t lastgranulepos;
    ogg_int64_t firstgranulepos;
} misc_opus_info;

static inline const char * mapping_family_name(int mapping_family)
{
    switch (mapping_family) {
        case 0:
            return "RTP mapping";
            break;
        case 1:
            return "Vorbis mapping";
            break;
        case 2:
            return "Ambisonic mapping 2";
            break;
        case 3:
            return "Ambisonic mapping 3";
            break;
        case 255:
            return "unidentified mapping";
            break;
        default:
            return "<unknown>";
            break;
    }
}

static void opus_process_opushead(stream_processor *stream, misc_opus_info *self, ogg_packet *packet)
{
    if (packet->bytes >= 19) {
        self->version = (unsigned int)packet->packet[8];
        if (self->version < 16) {
            self->channel_count = (unsigned int)packet->packet[9];
            self->pre_skip = read_u16le(&(packet->packet[10]));
            self->input_sample_rate = read_u16le(&(packet->packet[12]));
            self->output_gain = read_u16le(&(packet->packet[16]));
            self->mapping_family = (unsigned int)packet->packet[18];
            info(_("Version: %d\n"), self->version);
            info(_("Channels: %d\n"), self->channel_count);
            info(_("Preskip: %d (%.1fms)\n"), (int)self->pre_skip, (self->pre_skip / 48.));
            info(_("Output gain: %.1fdB\n"), self->output_gain / 256.);
            info(_("Mapping family: %d (%s)\n"), self->mapping_family, mapping_family_name(self->mapping_family));
            /*
             * Not reported as per discussion in #xiph on 2020-12-19 -- phschafft
            if (self->input_sample_rate)
                info(_("Input rate: %ld\n"), (long int)self->input_sample_rate);
            info(_("Rate: %ld\n\n"), (long int)48000);
            */
        } else {
            warn(_("WARNING: invalid OpusHead version %d on stream %d\n"), self->version, stream->num);
        }
    } else {
        warn(_("WARNING: invalid OpusHead on stream %d: packet too short\n"), stream->num);
    }
    self->seen_opushead = true;
}

static void opus_process_opustags(stream_processor *stream, misc_opus_info *self, ogg_packet *packet)
{
    bool too_short = false;

    do {
        size_t offset;

        if (packet->bytes < 16) {
            too_short = true;
            break;
        }

        if (handle_vorbis_comments(stream, &(packet->packet[8]), packet->bytes - 8, &offset) == -1) {
            too_short = true;
            break;
        }

        offset += 8;

        if (packet->bytes - offset - 1) {
            if (packet->packet[offset] & 0x1) {
                info(_("Extra metadata: %ld bytes\n"), (long int)(packet->bytes - offset - 1));
            } else {
                info(_("Padding: %ld bytes\n"), (long int)(packet->bytes - offset - 1));
            }
        }
    } while (0);

    if (too_short)
        warn(_("WARNING: invalid OpusTags on stream %d: packet too short\n"), stream->num);

    self->seen_opustags = true;
}

static void opus_process_data(stream_processor *stream, misc_opus_info *self, ogg_packet *packet)
{
    if (packet->granulepos != -1) {
        if (!self->seen_data)
            self->firstgranulepos = packet->granulepos;

        self->lastgranulepos = packet->granulepos;
    }

    self->seen_data = true;
}

static void opus_process(stream_processor *stream, ogg_page *page)
{
    misc_opus_info *self = stream->data;

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

        if (packet.bytes >= 8 && strncmp((const char *)packet.packet, "OpusHead", 8) == 0) {
            opus_process_opushead(stream, self, &packet);
        } else if (packet.bytes >= 8 && strncmp((const char *)packet.packet, "OpusTags", 8) == 0) {
            opus_process_opustags(stream, self, &packet);
        } else {
            opus_process_data(stream, self, &packet);
        }
    }

    if (self->seen_data)
        self->bytes += page->header_len + page->body_len;
}

static void opus_end(stream_processor *stream)
{
    misc_opus_info *self = stream->data;

    if (!self->seen_opushead)
        warn(_("WARNING: stream (%d) did not contain OpusHead header\n"), stream->num);

    if (!self->seen_opustags)
        warn(_("WARNING: stream (%d) did not contain OpusTags header\n"), stream->num);

    if (!self->seen_data)
        warn(_("WARNING: stream (%d) did not contain data packets\n"), stream->num);

    print_summary(stream, self->bytes, (double)(self->lastgranulepos - self->firstgranulepos - self->pre_skip) / 48000.);

    free(stream->data);
}

void opus_start(stream_processor *stream)
{
    misc_opus_info *self;

    stream->type = "Opus";
    stream->process_page = opus_process;
    stream->process_end = opus_end;

    stream->data = calloc(1, sizeof(misc_opus_info));
    self = stream->data;

    self->version = -1;
}
