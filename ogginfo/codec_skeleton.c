/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * This file handles skeleton streams.
 * See also:
 *  https://wiki.xiph.org/Ogg_Skeleton
 *  https://wiki.xiph.org/Ogg_Skeleton_3
 *  https://wiki.xiph.org/SkeletonHeaders
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2020-2021 Philipp Schafft <lion@lion.leolix.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <ogg/ogg.h>

#include "i18n.h"
#include "utf8.h"

#include "private.h"

typedef struct {
    bool supported;
    uint16_t version_major;
    uint16_t version_minor;
    uint64_t presentationtime_numerator;
    uint64_t presentationtime_denominator;
    uint64_t basetime_numerator;
    uint64_t basetime_denominator;
} misc_skeleton_info;

static const char hexchar[] = "0123456789abcdef";

static inline uint16_t read16le(unsigned char *in)
{
    return (((uint16_t)in[1]) << 8) | ((uint16_t)in[0]);
}

static inline uint32_t read32le(unsigned char *in)
{
        return (((uint32_t)in[3]) << 24) | (((uint32_t)in[2]) << 16) | (((uint32_t)in[1]) << 8) | ((uint32_t)in[0]);
}

static inline uint64_t read64le(unsigned char *in)
{
        return (((uint64_t)in[7]) << 56) | (((uint64_t)in[6]) << 48) | (((uint64_t)in[5]) << 40) | (((uint64_t)in[4]) << 32) |
               (((uint64_t)in[3]) << 24) | (((uint64_t)in[2]) << 16) | (((uint64_t)in[1]) << 8) | ((uint64_t)in[0]);
}

static void skeleton_process_fishead_utc(ogg_packet *packet)
{
    bool is_null = true;
    size_t i, j;
    char buf[2 + 2*20 + 1] = "0x";

    for (i = 44, j = 2; i < 64; i++) {
        if (packet->packet[i])
            is_null = false;

        buf[j++] = hexchar[(packet->packet[i] >> 4) & 0x0F];
        buf[j++] = hexchar[packet->packet[i] & 0x0F];
    }

    buf[j] = 0;

    if (is_null) {
        info(_("Wall clock time (UTC) unset.\n"));
    } else {
        info(_("Wall clock time (UTC): %s\n"), buf);
    }
}

static void skeleton_process_fishead(misc_skeleton_info *self, ogg_packet *packet)
{
    if (packet->bytes < 64) {
        warn(_("WARNING: Invalid short packet in %s stream.\n"), "skeleton");
        return;
    }

    self->version_major = read16le(&(packet->packet[8]));
    self->version_minor = read16le(&(packet->packet[10]));

    info(_("Version: %d.%d\n"), (int)self->version_major, (int)self->version_minor);

    self->supported = (self->version_major == 3 && self->version_minor == 0) || (self->version_major == 4 && self->version_minor == 0);
    if (!self->supported) {
        warn(_("WARNING: %s version not supported.\n"), "skeleton");
        return;
    }

    self->presentationtime_numerator = read64le(&(packet->packet[12]));
    self->presentationtime_denominator = read64le(&(packet->packet[20]));
    self->basetime_numerator = read64le(&(packet->packet[28]));
    self->basetime_denominator = read64le(&(packet->packet[36]));

    if (!self->presentationtime_denominator)
        warn(_("WARNING: Presentation time denominator is zero.\n"));

    if (!self->basetime_denominator)
        warn(_("WARNING: Presentation time denominator is zero.\n"));

    skeleton_process_fishead_utc(packet);
}

static void skeleton_process_fisbone_message_header(char *header)
{
    char *decoded;
    char *sep;

    if (utf8_decode(header, &decoded) < 0) {
        warn(_("WARNING: Invalid fishbone message header field.\n"));
        return;
    }

    sep = strstr(decoded, ": ");
    if (sep) {
        *sep = 0;
        sep += 2;
        info("\t%s=%s\n", decoded, sep);
    } else {
        warn(_("WARNING: Invalid fishbone message header field.\n"));
    }

    free(decoded);
}

static void skeleton_process_fisbone_message_headers(misc_skeleton_info *self, ogg_packet *packet, size_t offset)
{
    size_t left = packet->bytes - offset;
    const char *p = (const char *)&(packet->packet[offset]);

    while (left) {
        const char *end;
        size_t len;
        bool seen_cr = false;

        for (end = p, len = 0; len < left; len++, end++) {
            if (seen_cr) {
                if (*end == '\n') {
                    char *tmp = malloc(len);
                    if (!tmp) {
                        error(_("ERROR: Out of memory.\n"));
                        return;
                    }
                    memcpy(tmp, p, len - 1);
                    tmp[len - 1] = 0;
                    skeleton_process_fisbone_message_header(tmp);
                    free(tmp);
                    // found one.
                } else {
                    warn(_("WARNING: Invalid fishbone message header field.\n"));
                    return;
                }
            } else {
                if (*end == '\r') {
                    seen_cr = true;
                } else if (*end == '\n') {
                    warn(_("WARNING: Invalid fishbone message header field.\n"));
                    return;
                }
            }
        }

        left -= len;
        p += len;
    }
}

static void skeleton_process_fisbone(misc_skeleton_info *self, ogg_packet *packet)
{
    uint32_t offset_message_headers;

    if (packet->bytes < 52) {
        warn(_("WARNING: Invalid short packet in %s stream.\n"), "skeleton");
        return;
    }

    offset_message_headers = read32le(&(packet->packet[8]));

    if (packet->bytes < (offset_message_headers + 8)) {
        warn(_("WARNING: Invalid short packet in %s stream.\n"), "skeleton");
        return;
    }

    info(_("Fishbone: For stream %08x\n"), read32le(&(packet->packet[12])));

    skeleton_process_fisbone_message_headers(self, packet, offset_message_headers + 8);
}

static void skeleton_process_index(misc_skeleton_info *self, ogg_packet *packet)
{
    if (self->version_major == 3) {
        // Index packets are not yet defined in version 3.
        warn(_("Invalid packet in %s stream.\n"), "skeleton");
    }

    if (packet->bytes < 42) {
        warn(_("WARNING: Invalid short packet in %s stream.\n"), "skeleton");
        return;
    }
}

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

        if (!self->supported)
            continue;

        if (packet.bytes == 0 && packet.e_o_s)
            continue;

        if (packet.bytes < 8) {
            warn(_("WARNING: Invalid short packet in %s stream.\n"), "skeleton");
            continue;
        }

        if (memcmp(packet.packet, "fishead\0", 8) == 0) {
            skeleton_process_fishead(self, &packet);
        } else if (memcmp(packet.packet, "fisbone\0", 8) == 0) {
            skeleton_process_fisbone(self, &packet);
        } else if (memcmp(packet.packet, "index\0", 6) == 0) {
            skeleton_process_index(self, &packet);
        } else {
            warn(_("Invalid packet in %s stream.\n"), "skeleton");
        }
    }
}

static void skeleton_end(stream_processor *stream)
{
    free(stream->data);
}

void skeleton_start(stream_processor *stream)
{
    misc_skeleton_info *self;

    stream->type = "skeleton";
    stream->process_page = skeleton_process;
    stream->process_end = skeleton_end;
    stream->data = calloc(1, sizeof(misc_skeleton_info));

    self = stream->data;
    self->supported = true;
}
