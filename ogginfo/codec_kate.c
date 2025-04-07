/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * This file handles kate streams.
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

#ifdef HAVE_KATE
#include <kate/oggkate.h>
#endif

#include "i18n.h"

#include "private.h"

typedef struct {
#ifdef HAVE_KATE
    kate_info ki;
    kate_comment kc;
#else
    int num_headers;
#endif

    int major;
    int minor;
    char language[16];
    char category[16];

    ogg_int64_t bytes;
    ogg_int64_t lastgranulepos;
    ogg_int64_t firstgranulepos;

    int doneheaders;
} misc_kate_info;


static void kate_process(stream_processor *stream, ogg_page *page )
{
    ogg_packet packet;
    misc_kate_info *inf = stream->data;
    int header=0, packets=0;
    int res;
#ifdef HAVE_KATE
    int i;
    const char *encoding = NULL, *directionality = NULL;
#endif

    ogg_stream_pagein(&stream->os, page);
    if (!inf->doneheaders)
        header = 1;

    while (1) {
        res = ogg_stream_packetout(&stream->os, &packet);
        if (res < 0) {
           warn(_("WARNING: discontinuity in stream (%d)\n"), stream->num);
           continue;
        } else if (res == 0) {
            break;
        }

        packets++;
        if (!inf->doneheaders) {
#ifdef HAVE_KATE
            int ret = kate_ogg_decode_headerin(&inf->ki, &inf->kc, &packet);
            if (ret < 0) {
                warn(_("WARNING: Could not decode Kate header "
                            "packet %d - invalid Kate stream (%d)\n"),
                        packet.packetno, stream->num);
                continue;
            } else if (ret > 0) {
                inf->doneheaders=1;
            }
#else
            /* if we're not building against libkate, do some limited checks */
            if (packet.bytes<64 || memcmp(packet.packet+1, "kate\0\0\0", 7)) {
                warn(_("WARNING: packet %d does not seem to be a Kate header - "
                            "invalid Kate stream (%d)\n"),
                        packet.packetno, stream->num);
                continue;
            }
            if (packet.packetno==inf->num_headers) {
                inf->doneheaders=1;
            }
#endif

            if (packet.packetno==0) {
#ifdef HAVE_KATE
                inf->major = inf->ki.bitstream_version_major;
                inf->minor = inf->ki.bitstream_version_minor;
                memcpy(inf->language, inf->ki.language, 16);
                inf->language[15] = 0;
                memcpy(inf->category, inf->ki.category, 16);
                inf->category[15] = 0;
#else
                inf->major = packet.packet[9];
                inf->minor = packet.packet[10];
                inf->num_headers = packet.packet[11];
                memcpy(inf->language, packet.packet+32, 16);
                inf->language[15] = 0;
                memcpy(inf->category, packet.packet+48, 16);
                inf->category[15] = 0;
#endif
            }

            if (inf->doneheaders) {
                if (ogg_page_granulepos(page) != 0 || ogg_stream_packetpeek(&stream->os, NULL) == 1)
                    warn(_("WARNING: Kate stream %d does not have headers "
                                "correctly framed. Terminal header page contains "
                                "additional packets or has non-zero granulepos\n"),
                            stream->num);
                info(_("Kate headers parsed for stream %d, "
                            "information follows...\n"), stream->num);

                info(_("Version: %d.%d\n"), inf->major, inf->minor);
#ifdef HAVE_KATE
                info(_("Vendor: %s\n"), inf->kc.vendor);
#endif

                if (*inf->language) {
                    info(_("Language: %s\n"), inf->language);
                } else {
                    info(_("No language set\n"));
                }

                if (*inf->category) {
                    info(_("Category: %s\n"), inf->category);
                } else {
                    info(_("No category set\n"));
                }

#ifdef HAVE_KATE
                switch (inf->ki.text_encoding) {
                    case kate_utf8: encoding=_("utf-8"); break;
                    default: encoding=NULL; break;
                }

                if (encoding) {
                    info(_("Character encoding: %s\n"),encoding);
                } else {
                    info(_("Unknown character encoding\n"));
                }

                if (printlots) {
                    switch (inf->ki.text_directionality) {
                        case kate_l2r_t2b: directionality=_("left to right, top to bottom"); break;
                        case kate_r2l_t2b: directionality=_("right to left, top to bottom"); break;
                        case kate_t2b_r2l: directionality=_("top to bottom, right to left"); break;
                        case kate_t2b_l2r: directionality=_("top to bottom, left to right"); break;
                        default: directionality=NULL; break;
                    }

                    if (directionality) {
                        info(_("Text directionality: %s\n"),directionality);
                    } else {
                        info(_("Unknown text directionality\n"));
                    }

                    info("%u regions, %u styles, %u curves, %u motions, %u palettes,\n"
                            "%u bitmaps, %u font ranges, %u font mappings\n",
                            inf->ki.nregions, inf->ki.nstyles,
                            inf->ki.ncurves, inf->ki.nmotions,
                            inf->ki.npalettes, inf->ki.nbitmaps,
                            inf->ki.nfont_ranges, inf->ki.nfont_mappings);
                }

                if (inf->ki.gps_numerator == 0 || inf->ki.gps_denominator == 0) {
                    warn(_("Invalid zero granulepos rate\n"));
                } else {
                    info(_("Granulepos rate %d/%d (%.02f gps)\n"),
                            inf->ki.gps_numerator, inf->ki.gps_denominator,
                            (float)inf->ki.gps_numerator/(float)inf->ki.gps_denominator);
                }

                if (inf->kc.comments > 0)
                    info(_("User comments section follows...\n"));

                for (i=0; i < inf->kc.comments; i++) {
                    const char *comment = inf->kc.user_comments[i];
                    check_xiph_comment(stream, i, comment,
                            inf->kc.comment_lengths[i]);
                }
#endif
                info(_("\n"));
            }
        }
    }

    if (!header) {
        ogg_int64_t gp = ogg_page_granulepos(page);
        if (gp > 0) {
            if (gp < inf->lastgranulepos) {
                warn(_("WARNING: granulepos in stream %d decreases from %"
                        PRId64 " to %" PRId64 "\n" ),
                        stream->num, inf->lastgranulepos, gp);
            }
            inf->lastgranulepos = gp;
        } else if (packets && gp<0) { /* zero granpos on data is valid for kate */
            /* Only do this if we saw at least one packet ending on this page.
             * It's legal (though very unusual) to have no packets in a page at
             * all - this is occasionally used to have an empty EOS page */
            warn(_("Negative granulepos (%" PRId64 ") on Kate stream outside of headers. This file was created by a buggy encoder\n"), gp);
        }
        if (inf->firstgranulepos < 0) { /* Not set yet */
        }
        inf->bytes += page->header_len + page->body_len;
    }
}

#ifdef HAVE_KATE
static void kate_end(stream_processor *stream)
{
    misc_kate_info *inf = stream->data;
    long minutes, seconds, milliseconds;
    double bitrate, time;

    /* This should be lastgranulepos - startgranulepos, or something like that*/
    //time = (double)(inf->lastgranulepos>>inf->ki.granule_shift) * inf->ki.gps_denominator / inf->ki.gps_numerator;
    ogg_int64_t gbase=inf->lastgranulepos>>inf->ki.granule_shift;
    ogg_int64_t goffset=inf->lastgranulepos-(gbase<<inf->ki.granule_shift);
    time = (double)(gbase+goffset) / ((float)inf->ki.gps_numerator/(float)inf->ki.gps_denominator);
    minutes = (long)time / 60;
    seconds = (long)time - minutes*60;
    milliseconds = (long)((time - minutes*60 - seconds)*1000);
    bitrate = inf->bytes*8 / time / 1000.0;

    info(_("Kate stream %d:\n"
           "\tTotal data length: %" PRId64 " bytes\n"
           "\tPlayback length: %ldm:%02ld.%03lds\n"
           "\tAverage bitrate: %f kb/s\n"),
            stream->num,inf->bytes, minutes, seconds, milliseconds, bitrate);

    kate_comment_clear(&inf->kc);
    kate_info_clear(&inf->ki);

    free(stream->data);
}
#else
static void kate_end(stream_processor *stream)
{
}
#endif

void kate_start(stream_processor *stream)
{
#ifdef HAVE_KATE
    misc_kate_info *info;
#endif /* HAVE_KATE */

    stream->type = "kate";
    stream->process_page = kate_process;
    stream->process_end = kate_end;

    stream->data = calloc(1, sizeof(misc_kate_info));

#ifdef HAVE_KATE
    info = stream->data;
    kate_comment_init(&info->kc);
    kate_info_init(&info->ki);
#endif /* HAVE_KATE */
}

