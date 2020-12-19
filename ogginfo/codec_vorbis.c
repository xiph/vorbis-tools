/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * This file handles vorbis streams.
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2020      Philipp Schafft <lion@lion.leolix.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "i18n.h"

#include "private.h"

typedef struct {
    vorbis_info vi;
    vorbis_comment vc;

    ogg_int64_t bytes;
    ogg_int64_t lastgranulepos;
    ogg_int64_t firstgranulepos;

    int doneheaders;
} misc_vorbis_info;

static const struct vorbis_release {
    const char *vendor_string;
    const char *desc;
} releases[] = {
    {"Xiphophorus libVorbis I 20000508", "1.0 beta 1 or beta 2"},
    {"Xiphophorus libVorbis I 20001031", "1.0 beta 3"},
    {"Xiphophorus libVorbis I 20010225", "1.0 beta 4"},
    {"Xiphophorus libVorbis I 20010615", "1.0 rc1"},
    {"Xiphophorus libVorbis I 20010813", "1.0 rc2"},
    {"Xiphophorus libVorbis I 20011217", "1.0 rc3"},
    {"Xiphophorus libVorbis I 20011231", "1.0 rc3"},
    {"Xiph.Org libVorbis I 20020717", "1.0"},
    {"Xiph.Org libVorbis I 20030909", "1.0.1"},
    {"Xiph.Org libVorbis I 20040629", "1.1.0"},
    {"Xiph.Org libVorbis I 20050304", "1.1.1"},
    {"Xiph.Org libVorbis I 20050304", "1.1.2"},
    {"Xiph.Org libVorbis I 20070622", "1.2.0"},
    {"Xiph.Org libVorbis I 20080501", "1.2.1"},
    {NULL, NULL},
};


static void vorbis_process(stream_processor *stream, ogg_page *page )
{
    ogg_packet packet;
    misc_vorbis_info *inf = stream->data;
    int i, header=0, packets=0;
    int k;
    int res;

    ogg_stream_pagein(&stream->os, page);
    if (inf->doneheaders < 3)
        header = 1;

    while (1) {
        res = ogg_stream_packetout(&stream->os, &packet);
        if (res < 0) {
           warn(_("WARNING: discontinuity in stream (%d)\n"), stream->num);
           continue;
        }
        else if (res == 0) {
            break;
        }

        packets++;
        if (inf->doneheaders < 3) {
            if (vorbis_synthesis_headerin(&inf->vi, &inf->vc, &packet) < 0) {
                warn(_("WARNING: Could not decode Vorbis header "
                            "packet %d - invalid Vorbis stream (%d)\n"),
                        inf->doneheaders, stream->num);
                continue;
            }
            inf->doneheaders++;
            if (inf->doneheaders == 3) {
                if (ogg_page_granulepos(page) != 0 || ogg_stream_packetpeek(&stream->os, NULL) == 1)
                    warn(_("WARNING: Vorbis stream %d does not have headers "
                                "correctly framed. Terminal header page contains "
                                "additional packets or has non-zero granulepos\n"),
                            stream->num);
                info(_("Vorbis headers parsed for stream %d, "
                            "information follows...\n"), stream->num);

                info(_("Version: %d\n"), inf->vi.version);
                k = 0;
                while (releases[k].vendor_string) {
                    if (!strcmp(inf->vc.vendor, releases[k].vendor_string)) {
                        info(_("Vendor: %s (%s)\n"), inf->vc.vendor,
                                releases[k].desc);
                        break;
                    }
                    k++;
                }
                if (!releases[k].vendor_string)
                    info(_("Vendor: %s\n"), inf->vc.vendor);
                info(_("Channels: %d\n"), inf->vi.channels);
                info(_("Rate: %ld\n\n"), inf->vi.rate);

                if (inf->vi.bitrate_nominal > 0) {
                    info(_("Nominal bitrate: %f kb/s\n"),
                            (double)inf->vi.bitrate_nominal / 1000.0);
                } else {
                    info(_("Nominal bitrate not set\n"));
                }

                if (inf->vi.bitrate_upper > 0) {
                    info(_("Upper bitrate: %f kb/s\n"),
                            (double)inf->vi.bitrate_upper / 1000.0);
                } else {
                    info(_("Upper bitrate not set\n"));
                }

                if (inf->vi.bitrate_lower > 0) {
                    info(_("Lower bitrate: %f kb/s\n"),
                            (double)inf->vi.bitrate_lower / 1000.0);
                } else {
                    info(_("Lower bitrate not set\n"));
                }

                if (inf->vc.comments > 0)
                    info(_("User comments section follows...\n"));

                for (i=0; i < inf->vc.comments; i++) {
                    char *comment = inf->vc.user_comments[i];
                    check_xiph_comment(stream, i, comment,
                            inf->vc.comment_lengths[i]);
                }
            }
        }
    }

    if (!header) {
        ogg_int64_t gp = ogg_page_granulepos(page);
        if (gp > 0) {
            if (gp < inf->lastgranulepos)
                warn(_("WARNING: granulepos in stream %d decreases from %"
                        PRId64 " to %" PRId64 "\n" ),
                        stream->num, inf->lastgranulepos, gp);
            inf->lastgranulepos = gp;
        } else if (packets) {
            /* Only do this if we saw at least one packet ending on this page.
             * It's legal (though very unusual) to have no packets in a page at
             * all - this is occasionally used to have an empty EOS page */
            warn(_("Negative or zero granulepos (%" PRId64 ") on Vorbis stream outside of headers. This file was created by a buggy encoder\n"), gp);
        }
        if (inf->firstgranulepos < 0) { /* Not set yet */
        }
        inf->bytes += page->header_len + page->body_len;
    }
}

static void vorbis_end(stream_processor *stream)
{
    misc_vorbis_info *inf = stream->data;
    long minutes, seconds, milliseconds;
    double bitrate, time;

    /* This should be lastgranulepos - startgranulepos, or something like that*/
    time = (double)inf->lastgranulepos / inf->vi.rate;
    minutes = (long)time / 60;
    seconds = (long)time - minutes*60;
    milliseconds = (long)((time - minutes*60 - seconds)*1000);
    bitrate = inf->bytes*8 / time / 1000.0;

    info(_("Vorbis stream %d:\n"
           "\tTotal data length: %" PRId64 " bytes\n"
           "\tPlayback length: %ldm:%02ld.%03lds\n"
           "\tAverage bitrate: %f kb/s\n"),
            stream->num,inf->bytes, minutes, seconds, milliseconds, bitrate);

    vorbis_comment_clear(&inf->vc);
    vorbis_info_clear(&inf->vi);

    free(stream->data);
}

void vorbis_start(stream_processor *stream)
{
    misc_vorbis_info *info;

    stream->type = "vorbis";
    stream->process_page = vorbis_process;
    stream->process_end = vorbis_end;

    stream->data = calloc(1, sizeof(misc_vorbis_info));

    info = stream->data;

    vorbis_comment_init(&info->vc);
    vorbis_info_init(&info->vi);

}

