/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * Copyright 2002 Michael Smith <msmith@layrinth.net.au>
 * Licensed under the GNU GPL, distributed with this program.
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include <locale.h>
#include "utf8.h"
#include "i18n.h"

#define CHUNK 4500

typedef struct _stream_processor {
    void (*process_page)(struct _stream_processor *, ogg_page *);
    void (*process_end)(struct _stream_processor *);
    int isillegal;
    int shownillegal;
    int isnew;

    int start;
    int end;

    int num;
    char *type;

    ogg_uint32_t serial; /* must be 32 bit unsigned */
    ogg_stream_state os;
    void *data;
} stream_processor;

typedef struct {
    stream_processor *streams;
    int allocated;
    int used;

    int in_headers;
} stream_set;

typedef struct {
    vorbis_info vi;
    vorbis_comment vc;

    long bytes;
    ogg_int64_t lastgranulepos;

    int doneheaders;
} misc_vorbis_info;

static stream_set *create_stream_set(void) {
    stream_set *set = calloc(1, sizeof(stream_set));

    set->streams = calloc(5, sizeof(stream_processor));
    set->allocated = 5;
    set->used = 0;

    return set;
}

static void vorbis_process(stream_processor *stream, ogg_page *page )
{
    ogg_packet packet;
    misc_vorbis_info *info = stream->data;
    int i, header=0;

    ogg_stream_pagein(&stream->os, page);

    while(ogg_stream_packetout(&stream->os, &packet) > 0) {
        if(info->doneheaders < 3) {
            if(vorbis_synthesis_headerin(&info->vi, &info->vc, &packet) < 0) {
                fprintf(stderr, _("Warning: Could not decode vorbis header "
                       "packet - invalid vorbis stream (%d)\n"), stream->num);
                continue;
            }
            header = 1;
            info->doneheaders++;
            if(info->doneheaders == 3) {
                fprintf(stderr, 
                        _("Vorbis headers parsed for stream %d, "
                          "information follows...\n"), stream->num);

                fprintf(stderr, _("Version: %d\n"), info->vi.version);
                fprintf(stderr, _("Vendor: %s\n"), info->vc.vendor);
                fprintf(stderr, _("Channels: %d\n"), info->vi.channels);
                fprintf(stderr, _("Rate: %ld\n\n"), info->vi.rate);

                if(info->vi.bitrate_nominal >= 0)
                    fprintf(stderr, _("Nominal bitrate: %f kb/s\n"), 
                            (double)info->vi.bitrate_nominal / 1000.0);
                else
                    fprintf(stderr, _("Nominal bitrate not set\n"));

                if(info->vi.bitrate_upper >= 0)
                    fprintf(stderr, _("Upper bitrate: %f kb/s\n"), 
                            (double)info->vi.bitrate_upper / 1000.0);
                else
                    fprintf(stderr, _("Upper bitrate not set\n"));

                if(info->vi.bitrate_lower >= 0)
                    fprintf(stderr, _("Lower bitrate: %f kb/s\n"), 
                            (double)info->vi.bitrate_lower / 1000.0);
                else
                    fprintf(stderr, _("Lower bitrate not set\n"));

                if(info->vc.comments > 0)
                    fprintf(stderr, _("User comments section follows...\n"));

                for(i=0; i < info->vc.comments; i++)
                    fprintf(stderr, "\t%s\n", info->vc.user_comments[i]);
            }
        }
    }

    if(!header) {
        info->bytes += page->header_len + page->body_len;
        info->lastgranulepos = ogg_page_granulepos(page);
    }
}

static void vorbis_end(stream_processor *stream) 
{
    misc_vorbis_info *info = stream->data;
    long minutes, seconds;
    double bitrate, time;

    time = (double)info->lastgranulepos / info->vi.rate;
    minutes = (long)time / 60;
    seconds = (long)time - minutes*60;
    bitrate = info->bytes*8 / time / 1000.0;

    fprintf(stderr, _("Vorbis stream %d:\n"
                      "\tTotal data length: %ld bytes\n"
                      "\tPlayback length: %ldm:%02lds\n"
                      "\tAverage bitrate: %f kbps\n"), 
            stream->num,info->bytes, minutes, seconds, bitrate);

    vorbis_comment_clear(&info->vc);
    vorbis_info_clear(&info->vi);

    free(stream->data);
}

static void process_other(stream_processor *stream, ogg_page *page )
{
    ogg_packet packet;

    ogg_stream_pagein(&stream->os, page);

    while(ogg_stream_packetout(&stream->os, &packet) > 0) {
        /* Should we do anything here? Currently, we don't */
    }
}
        

static void free_stream_set(stream_set *set)
{
    int i;
    for(i=0; i < set->used; i++) {
        if(!set->streams[i].end) {
            fprintf(stderr, _("Warning: EOS not set on stream %d\n"), 
                    set->streams[i].num);
        }
        ogg_stream_clear(&set->streams[i].os);
    }

    free(set->streams);
    free(set);
}

static int streams_open(stream_set *set)
{
    int i;
    int res=0;
    for(i=0; i < set->used; i++) {
        if(!set->streams[i].end)
            res++;
    }

    return res;
}

static void other_start(stream_processor *stream)
{
    stream->type = "unknown";
    stream->process_page = process_other;
    stream->process_end = NULL;
}

static void vorbis_start(stream_processor *stream)
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

static stream_processor *find_stream_processor(stream_set *set, ogg_page *page)
{
    ogg_uint32_t serial = ogg_page_serialno(page);
    int i, found = 0;
    int invalid = 0;
    stream_processor *stream;

    for(i=0; i < set->used; i++) {
        if(serial == set->streams[i].serial) {
            /* We have a match! */
            found = 1;
            stream = &(set->streams[i]);

            set->in_headers = 0;
            /* if we have detected EOS, then this can't occur here. */
            if(stream->end) {
                stream->isillegal = 1;
                return stream;
            }

            stream->isnew = 0;
            stream->start = ogg_page_bos(page);
            stream->end = ogg_page_eos(page);
            stream->serial = serial;
            return stream;
        }
    }

    /* If there are streams open, and we've reached the end of the
     * headers, then we can't be starting a new stream.
     * XXX: might this sometimes catch ok streams if EOS flag is missing,
     * but the stream is otherwise ok?
     */
    if(streams_open(set) && !set->in_headers)
        invalid = 1;

    set->in_headers = 1;

    if(set->allocated < set->used)
        stream = &set->streams[set->used];
    else {
        set->allocated += 5;
        set->streams = realloc(set->streams, sizeof(stream_processor)*
                set->allocated);
        stream = &set->streams[set->used];
    }
    set->used++;
    stream->num = set->used; /* We count from 1 */

    stream->isnew = 1;
    stream->isillegal = invalid;

    {
        int res;
        ogg_packet packet;

        /* We end up processing the header page twice, but that's ok. */
        ogg_stream_init(&stream->os, serial);
        ogg_stream_pagein(&stream->os, page);
        res = ogg_stream_packetout(&stream->os, &packet);
        if(res <= 0) {
            fprintf(stderr, _(
                    "Warning: Invalid header page, no packet found\n"));
            return NULL;
        }

        if(packet.bytes >= 7 && memcmp(packet.packet, "\001vorbis", 7)==0)
        {
            fprintf(stderr, _("Stream %d is vorbis\n"), stream->num);
            vorbis_start(stream);
        }
        else {
            fprintf(stderr, _("Stream %d is unidentified other type\n"), 
                    stream->num);
            other_start(stream);
        }

        res = ogg_stream_packetout(&stream->os, &packet);
        if(res > 0) {
            fprintf(stderr, _("Warning: Invalid header page in stream %d, "
                              "contains multiple packets\n"), stream->num);
        }

        /* re-init, ready for processing */
        ogg_stream_clear(&stream->os);
        ogg_stream_init(&stream->os, serial);
   }

   stream->start = ogg_page_bos(page);
   stream->end = ogg_page_eos(page);
   stream->serial = serial;

   return stream;
}

static int get_next_page(FILE *f, ogg_sync_state *sync, ogg_page *page)
{
    int ret;
    char *buffer;
    int bytes;

    while((ret = ogg_sync_pageout(sync, page)) <= 0) {
        if(ret < 0)
            fprintf(stderr, _("Warning: Hole in data found. Corrupted ogg\n"));

        buffer = ogg_sync_buffer(sync, CHUNK);
        bytes = fread(buffer, 1, CHUNK, f);
        ogg_sync_wrote(sync, bytes);
        if(bytes == 0)
            return 0;
    }

    return 1;
}

static void process_file(char *filename) {
    FILE *file = fopen(filename, "rb");
    ogg_sync_state sync;
    ogg_page page;
    stream_set *processors = create_stream_set();

    if(!file) {
        fprintf(stderr, _("Error opening input file \"%s\": %s\n"), filename,
                    strerror(errno));
        return;
    }

    fprintf(stderr, _("Processing file \"%s\"...\n\n"), filename);

    ogg_sync_init(&sync);

    while(get_next_page(file, &sync, &page)) {
        stream_processor *p = find_stream_processor(processors, &page);

        if(!p) {
            fprintf(stderr, 
                    _("Could not find a processor for stream, bailing\n"));
            return;
        }

        if(p->isillegal && !p->shownillegal) {
            fprintf(stderr, 
                    _("Warning: illegally placed page(s) for logical stream %d\n"
                      "This indicates a corrupt ogg file.\n"), p->num);
            p->shownillegal = 1;
            continue;
        }

        if(p->isnew) {
            fprintf(stderr, _("New logical stream (#%d, serial: %08x): type %s\n"), 
                    p->num, p->serial, p->type);
            if(!p->start)
                fprintf(stderr, 
                        _("Warning: stream start flag not set on stream %d\n"),
                        p->num);
        }
        else if(p->start)
            fprintf(stderr, 
                    _("Warning: stream start flag found in mid-stream "
                      "on stream %d\n"), p->num);

        if(!p->isillegal) {
            p->process_page(p, &page);

            if(p->end) {
                if(p->process_end)
                    p->process_end(p);
                fprintf(stderr, _("Logical stream %d ended\n"), p->num);
                p->isillegal = 1;
            }
        }
    }

    free_stream_set(processors);

    ogg_sync_clear(&sync);

    fclose(file);
}

int main(int argc, char **argv) {
    int f;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    if(argc < 2) {
        fprintf(stderr, 
                _("Usage: ogginfo file1.ogg [file2.ogg ... fileN.ogg]\n"
                  "\n"
                  "Ogginfo is a tool for printing information about ogg files\n"
                  "and for diagnosing problems with them.\n"));
        exit(1);
    }

    for(f=1; f < argc; f++) {
        process_file(argv[f]);
    }

    return 0;
}

