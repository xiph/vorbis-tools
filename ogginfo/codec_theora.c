/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * This file handles theora streams.
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2020      Philipp Schafft <lion@lion.leolix.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

#include <ogg/ogg.h>

#include "i18n.h"

#include "theora.h"
#include "private.h"

typedef struct {
    theora_info ti;
    theora_comment tc;

    ogg_int64_t bytes;
    ogg_int64_t lastgranulepos;
    ogg_int64_t firstgranulepos;

    int doneheaders;

    ogg_int64_t framenum_expected;
} misc_theora_info;


static void theora_process(stream_processor *stream, ogg_page *page)
{
    ogg_packet packet;
    misc_theora_info *inf = stream->data;
    int i, header=0;
    int res;

    ogg_stream_pagein(&stream->os, page);
    if (inf->doneheaders < 3)
        header = 1;

    while (1) {
        res = ogg_stream_packetout(&stream->os, &packet);
        if (res < 0) {
            warn(_("WARNING: discontinuity in stream (%d)\n"), stream->num);
            continue;
        } else if (res == 0) {
            break;
        }

        if (inf->doneheaders < 3) {
            if (theora_decode_header(&inf->ti, &inf->tc, &packet) < 0) {
                warn(_("WARNING: Could not decode Theora header "
                            "packet - invalid Theora stream (%d)\n"), stream->num);
                continue;
            }
            inf->doneheaders++;
            if (inf->doneheaders == 3) {
                if (ogg_page_granulepos(page) != 0 || ogg_stream_packetpeek(&stream->os, NULL) == 1)
                    warn(_("WARNING: Theora stream %d does not have headers "
                                "correctly framed. Terminal header page contains "
                                "additional packets or has non-zero granulepos\n"),
                            stream->num);
                info(_("Theora headers parsed for stream %d, "
                            "information follows...\n"), stream->num);

                info(_("Version: %d.%d.%d\n"), inf->ti.version_major, inf->ti.version_minor, inf->ti.version_subminor);

                info(_("Vendor: %s\n"), inf->tc.vendor);
                info(_("Width: %d\n"), inf->ti.frame_width);
                info(_("Height: %d\n"), inf->ti.frame_height);
                info(_("Total image: %d by %d, crop offset (%d, %d)\n"),
                        inf->ti.width, inf->ti.height, inf->ti.offset_x, inf->ti.offset_y);
                if (inf->ti.offset_x + inf->ti.frame_width > inf->ti.width)
                    warn(_("Frame offset/size invalid: width incorrect\n"));
                if (inf->ti.offset_y + inf->ti.frame_height > inf->ti.height)
                    warn(_("Frame offset/size invalid: height incorrect\n"));

                if (inf->ti.fps_numerator == 0 || inf->ti.fps_denominator == 0) {
                    warn(_("Invalid zero framerate\n"));
                } else {
                    info(_("Framerate %d/%d (%.02f fps)\n"), inf->ti.fps_numerator, inf->ti.fps_denominator, (float)inf->ti.fps_numerator/(float)inf->ti.fps_denominator);
                }

                if (inf->ti.aspect_numerator == 0 || inf->ti.aspect_denominator == 0) {
                    info(_("Aspect ratio undefined\n"));
                } else {
                    float frameaspect = (float)inf->ti.frame_width/(float)inf->ti.frame_height * (float)inf->ti.aspect_numerator/(float)inf->ti.aspect_denominator;
                    info(_("Pixel aspect ratio %d:%d (%f:1)\n"), inf->ti.aspect_numerator, inf->ti.aspect_denominator, (float)inf->ti.aspect_numerator/(float)inf->ti.aspect_denominator);
                    if (fabs(frameaspect - 4.0/3.0) < 0.02)
                        info(_("Frame aspect 4:3\n"));
                    else if (fabs(frameaspect - 16.0/9.0) < 0.02)
                        info(_("Frame aspect 16:9\n"));
                    else
                        info(_("Frame aspect %f:1\n"), frameaspect);
                }

                if (inf->ti.colorspace == OC_CS_ITU_REC_470M)
                    info(_("Colourspace: Rec. ITU-R BT.470-6 System M (NTSC)\n"));
                else if (inf->ti.colorspace == OC_CS_ITU_REC_470BG)
                    info(_("Colourspace: Rec. ITU-R BT.470-6 Systems B and G (PAL)\n"));
                else
                    info(_("Colourspace unspecified\n"));

                if (inf->ti.pixelformat == OC_PF_420)
                    info(_("Pixel format 4:2:0\n"));
                else if (inf->ti.pixelformat == OC_PF_422)
                    info(_("Pixel format 4:2:2\n"));
                else if (inf->ti.pixelformat == OC_PF_444)
                    info(_("Pixel format 4:4:4\n"));
                else
                    warn(_("Pixel format invalid\n"));

                info(_("Target bitrate: %d kbps\n"), inf->ti.target_bitrate/1000);
                info(_("Nominal quality setting (0-63): %d\n"), inf->ti.quality);

                if (inf->tc.comments > 0)
                    info(_("User comments section follows...\n"));

                for (i=0; i < inf->tc.comments; i++) {
                    char *comment = inf->tc.user_comments[i];
                    check_xiph_comment(stream, i, comment,
                            inf->tc.comment_lengths[i]);
                }
            }
        }
        else {
            ogg_int64_t framenum;
            ogg_int64_t iframe,pframe;
            ogg_int64_t gp = packet.granulepos;

            if (gp > 0) {
                iframe=gp>>inf->ti.granule_shift;
                pframe=gp-(iframe<<inf->ti.granule_shift);
                framenum = iframe+pframe;
                if (inf->framenum_expected >= 0 &&
                        inf->framenum_expected != framenum)
                {
                    warn(_("WARNING: Expected frame %" PRId64
                                ", got %" PRId64 "\n"),
                            inf->framenum_expected, framenum);
                }
                inf->framenum_expected = framenum + 1;
            } else if (inf->framenum_expected >= 0) {
                inf->framenum_expected++;
            }
        }
    }

    if (!header) {
        ogg_int64_t gp = ogg_page_granulepos(page);
        if (gp > 0) {
            if (gp < inf->lastgranulepos)
                warn(_("WARNING: granulepos in stream %d decreases from %"
                            PRId64 " to %" PRId64 "\n"),
                        stream->num, inf->lastgranulepos, gp);
            inf->lastgranulepos = gp;
        }
        if (inf->firstgranulepos < 0) { /* Not set yet */
        }
        inf->bytes += page->header_len + page->body_len;
    }
}

static void theora_end(stream_processor *stream)
{
    misc_theora_info *inf = stream->data;
    long minutes, seconds, milliseconds;
    double bitrate, time;
    int new_gp;
    new_gp = inf->ti.version_major > 3
       || (inf->ti.version_major == 3 && (inf->ti.version_minor > 2
       || (inf->ti.version_minor == 2 && inf->ti.version_subminor > 0)));

    /* This should be lastgranulepos - startgranulepos, or something like that*/
    ogg_int64_t iframe=inf->lastgranulepos>>inf->ti.granule_shift;
    ogg_int64_t pframe=inf->lastgranulepos-(iframe<<inf->ti.granule_shift);
    /* The granule position starts at 0 for stream version 3.2.0, but starts at
       1 for version 3.2.1 and above. In the former case, we need to add one
       to the final granule position to get the frame count. */
    time = (double)(iframe+pframe+!new_gp) /
	((float)inf->ti.fps_numerator/(float)inf->ti.fps_denominator);
    minutes = (long)time / 60;
    seconds = (long)time - minutes*60;
    milliseconds = (long)((time - minutes*60 - seconds)*1000);
    bitrate = inf->bytes*8 / time / 1000.0;

    info(_("Theora stream %d:\n"
           "\tTotal data length: %" PRId64 " bytes\n"
           "\tPlayback length: %ldm:%02ld.%03lds\n"
           "\tAverage bitrate: %f kb/s\n"),
            stream->num,inf->bytes, minutes, seconds, milliseconds, bitrate);

    theora_comment_clear(&inf->tc);
    theora_info_clear(&inf->ti);

    free(stream->data);
}


void theora_start(stream_processor *stream)
{
    misc_theora_info *info;

    stream->type = "theora";
    stream->process_page = theora_process;
    stream->process_end = theora_end;

    stream->data = calloc(1, sizeof(misc_theora_info));
    info = stream->data;
    info->framenum_expected = -1;
}
