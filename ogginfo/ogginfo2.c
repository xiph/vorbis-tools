/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2020      Philipp Schafft <lion@lion.leolix.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <math.h>
#include <inttypes.h>

#include <ogg/ogg.h>

#include <locale.h>
#include "utf8.h"
#include "i18n.h"

#include "private.h"

#define CHUNK 4500

/* TODO:
 *
 * - detect violations of muxing constraints
 * - detect granulepos 'gaps' (possibly vorbis-specific). (seperate from
 *   serial-number gaps)
 */

typedef struct {
    stream_processor *streams;
    int allocated;
    int used;

    int in_headers;
} stream_set;

static int printlots = 0;
static int printinfo = 1;
static int printwarn = 1;
static int verbose = 1;

static int flawed;

#define CONSTRAINT_PAGE_AFTER_EOS   1
#define CONSTRAINT_MUXING_VIOLATED  2

static stream_set *create_stream_set(void) {
    stream_set *set = calloc(1, sizeof(stream_set));

    set->streams = calloc(5, sizeof(stream_processor));
    set->allocated = 5;
    set->used = 0;

    return set;
}

void info(const char *format, ...)
{
    va_list ap;

    if (!printinfo)
        return;

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
}

void warn(const char *format, ...)
{
    va_list ap;

    flawed = 1;
    if (!printwarn)
        return;

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
}

void error(const char *format, ...)
{
    va_list ap;

    flawed = 1;

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
}

static void print_vendor(const unsigned char *str, size_t len)
{
    char *buf = malloc(len + 1);
    if (buf) {
        memcpy(buf, str, len);
        buf[len] = 0;
        info(_("Vendor: %s\n"), buf);
        free(buf);
    }
}

int handle_vorbis_comments(stream_processor *stream, const unsigned char *in, size_t length, size_t *end)
{
    ogg_uint32_t vendor_string_length;
    ogg_uint32_t user_comment_list_length;
    ogg_uint32_t i;

    if (length < 8)
        return -1;

    vendor_string_length = read_u32le(in);
    if (length < (8 + vendor_string_length))
        return -1;

    print_vendor(in + 4, vendor_string_length);

    user_comment_list_length = read_u32le(in + 4 + vendor_string_length);

    if (user_comment_list_length)
        info(_("User comments section follows...\n"));

    *end = 8 + vendor_string_length;
    for (i = 0; i < user_comment_list_length; i++) {
        ogg_uint32_t user_comment_string_length = read_u32le(in + *end);
        char *buf;

        (*end) += 4;

        if (length < (*end + user_comment_string_length))
            return -1;

        buf = malloc(user_comment_string_length);
        if (buf) {
            memcpy(buf, in + *end, user_comment_string_length);
            buf[user_comment_string_length] = 0;
            check_xiph_comment(stream, i, buf, user_comment_string_length);
            free(buf);
        }

        (*end) += user_comment_string_length;
    }

    return 0;
}

void check_xiph_comment(stream_processor *stream, int i, const char *comment, int comment_length)
{
    char *sep = strchr(comment, '=');
    char *decoded;
    int j;
    int broken = 0;
    unsigned char *val;
    int bytes;
    int remaining;

    if (sep == NULL) {
        warn(_("WARNING: Comment %d in stream %d has invalid "
               "format, does not contain '=': \"%s\"\n"),
               i, stream->num, comment);
        return;
    }

    for (j=0; j < sep-comment; j++) {
        if (comment[j] < 0x20 || comment[j] > 0x7D) {
            warn(_("WARNING: Invalid comment fieldname in "
                   "comment %d (stream %d): \"%s\"\n"),
                   i, stream->num, comment);
            broken = 1;
            break;
        }
    }

    if (broken)
        return;

    val = (unsigned char *)comment;

    j = sep-comment+1;
    while (j < comment_length)
    {
        remaining = comment_length - j;
        if ((val[j] & 0x80) == 0) {
            bytes = 1;
        } else if ((val[j] & 0x40) == 0x40) {
            if ((val[j] & 0x20) == 0)
                bytes = 2;
            else if ((val[j] & 0x10) == 0)
                bytes = 3;
            else if ((val[j] & 0x08) == 0)
                bytes = 4;
            else if ((val[j] & 0x04) == 0)
                bytes = 5;
            else if ((val[j] & 0x02) == 0)
                bytes = 6;
            else {
                warn(_("WARNING: Illegal UTF-8 sequence in "
                    "comment %d (stream %d): length marker wrong\n"),
                    i, stream->num);
                broken = 1;
                break;
            }
        } else {
            warn(_("WARNING: Illegal UTF-8 sequence in comment "
                "%d (stream %d): length marker wrong\n"), i, stream->num);
            broken = 1;
            break;
        }

        if (bytes > remaining) {
            warn(_("WARNING: Illegal UTF-8 sequence in comment "
                "%d (stream %d): too few bytes\n"), i, stream->num);
            broken = 1;
            break;
        }

        switch(bytes) {
            case 1:
                /* No more checks needed */
                break;
            case 2:
                if ((val[j+1] & 0xC0) != 0x80)
                    broken = 1;
                if ((val[j] & 0xFE) == 0xC0)
                    broken = 1;
                break;
            case 3:
                if (!((val[j] == 0xE0 && val[j+1] >= 0xA0 && val[j+1] <= 0xBF &&
                         (val[j+2] & 0xC0) == 0x80) ||
                     (val[j] >= 0xE1 && val[j] <= 0xEC &&
                         (val[j+1] & 0xC0) == 0x80 &&
                         (val[j+2] & 0xC0) == 0x80) ||
                     (val[j] == 0xED && val[j+1] >= 0x80 &&
                         val[j+1] <= 0x9F &&
                         (val[j+2] & 0xC0) == 0x80) ||
                     (val[j] >= 0xEE && val[j] <= 0xEF &&
                         (val[j+1] & 0xC0) == 0x80 &&
                         (val[j+2] & 0xC0) == 0x80)))
                     broken = 1;
                 if (val[j] == 0xE0 && (val[j+1] & 0xE0) == 0x80)
                     broken = 1;
                 break;
            case 4:
                 if (!((val[j] == 0xF0 && val[j+1] >= 0x90 &&
                         val[j+1] <= 0xBF &&
                         (val[j+2] & 0xC0) == 0x80 &&
                         (val[j+3] & 0xC0) == 0x80) ||
                     (val[j] >= 0xF1 && val[j] <= 0xF3 &&
                         (val[j+1] & 0xC0) == 0x80 &&
                         (val[j+2] & 0xC0) == 0x80 &&
                         (val[j+3] & 0xC0) == 0x80) ||
                     (val[j] == 0xF4 && val[j+1] >= 0x80 &&
                         val[j+1] <= 0x8F &&
                         (val[j+2] & 0xC0) == 0x80 &&
                         (val[j+3] & 0xC0) == 0x80)))
                     broken = 1;
                 if (val[j] == 0xF0 && (val[j+1] & 0xF0) == 0x80)
                     broken = 1;
                 break;
             /* 5 and 6 aren't actually allowed at this point */
             case 5:
                 broken = 1;
                 break;
             case 6:
                 broken = 1;
                 break;
         }

         if (broken) {
             char *simple = malloc (comment_length + 1);
             char *seq = malloc (comment_length * 3 + 1);
             static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
             int i, c1 = 0, c2 = 0;
             for (i = 0; i < comment_length; i++) {
               seq[c1++] = hex[((unsigned char)comment[i]) >> 4];
               seq[c1++] = hex[((unsigned char)comment[i]) & 0xf];
               seq[c1++] = ' ';

               if (comment[i] < 0x20 || comment[i] > 0x7D) {
                 simple[c2++] = '?';
               } else {
                 simple[c2++] = comment[i];
               }
             }
             seq[c1] = 0;
             simple[c2] = 0;
             warn(_("WARNING: Illegal UTF-8 sequence in comment "
                   "%d (stream %d): invalid sequence \"%s\": %s\n"), i,
                   stream->num, simple, seq);
             broken = 1;
             free (simple);
             free (seq);
             break;
         }

         j += bytes;
     }

    if (!broken) {
        if (utf8_decode(sep+1, &decoded) < 0) {
            warn(_("WARNING: Failure in UTF-8 decoder. This should not be possible\n"));
            return;
        }
        *sep = 0;
        if (!broken) {
            info("\t%s=%s\n", comment, decoded);
            free(decoded);
        }
    }
}

static void free_stream_set(stream_set *set)
{
    int i;
    for (i=0; i < set->used; i++) {
        if (!set->streams[i].end) {
            warn(_("WARNING: EOS not set on stream %d\n"),
                    set->streams[i].num);
            if (set->streams[i].process_end)
                set->streams[i].process_end(&set->streams[i]);
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
    for (i=0; i < set->used; i++) {
        if (!set->streams[i].end)
            res++;
    }

    return res;
}

static stream_processor *find_stream_processor(stream_set *set, ogg_page *page)
{
    ogg_uint32_t serial = ogg_page_serialno(page);
    int i;
    int invalid = 0;
    int constraint = 0;
    stream_processor *stream;

    for (i=0; i < set->used; i++) {
        if (serial == set->streams[i].serial) {
            /* We have a match! */
            stream = &(set->streams[i]);

            set->in_headers = 0;
            /* if we have detected EOS, then this can't occur here. */
            if (stream->end) {
                stream->isillegal = 1;
                stream->constraint_violated = CONSTRAINT_PAGE_AFTER_EOS;
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
    if (streams_open(set) && !set->in_headers) {
        constraint = CONSTRAINT_MUXING_VIOLATED;
        invalid = 1;
    }

    set->in_headers = 1;

    if (set->allocated < set->used) {
        stream = &set->streams[set->used];
    } else {
        set->allocated += 5;
        set->streams = realloc(set->streams, sizeof(stream_processor)*
                set->allocated);
        stream = &set->streams[set->used];
    }
    set->used++;
    stream->num = set->used; /* We count from 1 */

    stream->isnew = 1;
    stream->isillegal = invalid;
    stream->constraint_violated = constraint;

    {
        int res;
        ogg_packet packet;

        /* We end up processing the header page twice, but that's ok. */
        ogg_stream_init(&stream->os, serial);
        ogg_stream_pagein(&stream->os, page);
        res = ogg_stream_packetout(&stream->os, &packet);
        if (res <= 0) {
            warn(_("WARNING: Invalid header page, no packet found\n"));
            invalid_start(stream);
        } else if (packet.bytes >= 7 && memcmp(packet.packet, "\x01vorbis", 7)==0) {
            vorbis_start(stream);
        } else if (packet.bytes >= 7 && memcmp(packet.packet, "\x80theora", 7)==0) {
            theora_start(stream);
        } else if (packet.bytes >= 8 && memcmp(packet.packet, "OggMIDI\0", 8)==0) {
            other_start(stream, "MIDI");
        } else if (packet.bytes >= 5 && memcmp(packet.packet, "\177FLAC", 5)==0) {
            other_start(stream, "FLAC");
        } else if (packet.bytes == 4 && memcmp(packet.packet, "fLaC", 4)==0) {
            other_start(stream, "FLAC (legacy)");
        } else if (packet.bytes >= 8 && memcmp(packet.packet, "Speex   ", 8)==0) {
            speex_start(stream);
        } else if (packet.bytes >= 8 && memcmp(packet.packet, "fishead\0", 8)==0) {
            other_start(stream, "skeleton");
        } else if (packet.bytes >= 5 && memcmp(packet.packet, "BBCD\0", 5)==0) {
            other_start(stream, "dirac");
        } else if (packet.bytes >= 8 && memcmp(packet.packet, "KW-DIRAC", 8)==0) {
            other_start(stream, "dirac (legacy)");
        } else if (packet.bytes >= 8 && memcmp(packet.packet, "OpusHead", 8)==0) {
            opus_start(stream);
        } else if (packet.bytes >= 8 && memcmp(packet.packet, "\x80kate\0\0\0", 8)==0) {
            kate_start(stream);
        } else {
            other_start(stream, NULL);
        }

        res = ogg_stream_packetout(&stream->os, &packet);
        if (res > 0) {
            warn(_("WARNING: Invalid header page in stream %d, "
                              "contains multiple packets\n"), stream->num);
        }

        /* re-init, ready for processing */
        ogg_stream_clear(&stream->os);
        ogg_stream_init(&stream->os, serial);
   }

   stream->start = ogg_page_bos(page);
   stream->end = ogg_page_eos(page);
   stream->serial = serial;

   if (stream->serial == 0 || stream->serial == -1) {
       info(_("Note: Stream %d has serial number %d, which is legal but may "
              "cause problems with some tools.\n"), stream->num,
               stream->serial);
   }

   return stream;
}

static int get_next_page(FILE *f, ogg_sync_state *sync, ogg_page *page,
        ogg_int64_t *written)
{
    int ret;
    char *buffer;
    int bytes;

    while ((ret = ogg_sync_pageseek(sync, page)) <= 0) {
        if (ret < 0) {
            /* unsynced, we jump over bytes to a possible capture - we don't need to read more just yet */
            warn(_("WARNING: Hole in data (%d bytes) found at approximate offset %" PRId64 " bytes. Corrupted Ogg.\n"), -ret, *written);
            continue;
        }

        /* zero return, we didn't have enough data to find a whole page, read */
        buffer = ogg_sync_buffer(sync, CHUNK);
        bytes = fread(buffer, 1, CHUNK, f);
        if (bytes <= 0) {
            ogg_sync_wrote(sync, 0);
            return 0;
        }
        ogg_sync_wrote(sync, bytes);
        *written += bytes;
    }

    return 1;
}

static void process_file(char *filename) {
    FILE *file = fopen(filename, "rb");
    ogg_sync_state sync;
    ogg_page page;
    stream_set *processors = create_stream_set();
    int gotpage = 0;
    ogg_int64_t written = 0;

    if (!file) {
        error(_("Error opening input file \"%s\": %s\n"), filename,
                    strerror(errno));
        return;
    }

    printf(_("Processing file \"%s\"...\n\n"), filename);

    ogg_sync_init(&sync);

    while (get_next_page(file, &sync, &page, &written)) {
        stream_processor *p = find_stream_processor(processors, &page);
        gotpage = 1;

        if (!p) {
            error(_("Could not find a processor for stream, bailing\n"));
            return;
        }

        if (p->isillegal && !p->shownillegal) {
            char *constraint;
            switch(p->constraint_violated) {
                case CONSTRAINT_PAGE_AFTER_EOS:
                    constraint = _("Page found for stream after EOS flag");
                    break;
                case CONSTRAINT_MUXING_VIOLATED:
                    constraint = _("Ogg muxing constraints violated, new "
                                   "stream before EOS of all previous streams");
                    break;
                default:
                    constraint = _("Error unknown.");
            }

            warn(_("WARNING: illegally placed page(s) for logical stream %d\n"
                   "This indicates a corrupt Ogg file: %s.\n"),
                    p->num, constraint);
            p->shownillegal = 1;
            /* If it's a new stream, we want to continue processing this page
             * anyway to suppress additional spurious errors
             */
            if (!p->isnew)
                continue;
        }

        if (p->isnew) {
            info(_("New logical stream (#%d, serial: %08x): type %s\n"),
                    p->num, p->serial, p->type);
            if (!p->start)
                warn(_("WARNING: stream start flag not set on stream %d\n"),
                        p->num);
        } else if (p->start) {
            warn(_("WARNING: stream start flag found in mid-stream "
                      "on stream %d\n"), p->num);
        }

        if (p->seqno++ != ogg_page_pageno(&page)) {
            if (!p->lostseq)
                warn(_("WARNING: sequence number gap in stream %d. Got page "
                       "%ld when expecting page %ld. Indicates missing data.\n"
                       ), p->num, ogg_page_pageno(&page), p->seqno - 1);
            p->seqno = ogg_page_pageno(&page);
            p->lostseq = 1;
        } else {
            p->lostseq = 0;
        }

        if (!p->isillegal) {
            p->process_page(p, &page);

            if (p->end) {
                if (p->process_end)
                    p->process_end(p);
                info(_("Logical stream %d ended\n"), p->num);
                p->isillegal = 1;
                p->constraint_violated = CONSTRAINT_PAGE_AFTER_EOS;
            }
        }
    }

    if (!gotpage)
        error(_("ERROR: No Ogg data found in file \"%s\".\n"
                "Input probably not Ogg.\n"), filename);

    free_stream_set(processors);

    ogg_sync_clear(&sync);

    fclose(file);
}

static void version (void) {
    printf (_("ogginfo from %s %s\n"), PACKAGE, VERSION);
}

static void usage(void) {
    version ();
    printf (_(" by the Xiph.Org Foundation (https://www.xiph.org/)\n\n"));
    printf(_("(c) 2003-2005 Michael Smith <msmith@xiph.org>\n"
             "\n"
             "Usage: ogginfo [flags] file1.ogg [file2.ogx ... fileN.ogv]\n"
             "Flags supported:\n"
             "\t-h Show this help message\n"
             "\t-q Make less verbose. Once will remove detailed informative\n"
             "\t   messages, two will remove warnings\n"
             "\t-v Make more verbose. This may enable more detailed checks\n"
             "\t   for some stream types.\n"));
    printf (_("\t-V Output version information and exit\n"));
}

int main(int argc, char **argv) {
    int f, ret;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    if (argc < 2) {
        fprintf(stdout,
                _("Usage: ogginfo [flags] file1.ogg [file2.ogx ... fileN.ogv]\n"
                  "\n"
                  "ogginfo is a tool for printing information about Ogg files\n"
                  "and for diagnosing problems with them.\n"
                  "Full help shown with \"ogginfo -h\".\n"));
        exit(1);
    }

    while ((ret = getopt(argc, argv, "hqvV")) >= 0) {
        switch(ret) {
            case 'h':
                usage();
                return 0;
            case 'V':
                version();
                return 0;
            case 'v':
                verbose++;
                break;
            case 'q':
                verbose--;
                break;
        }
    }

    if (verbose > 1)
        printlots = 1;
    if (verbose < 1)
        printinfo = 0;
    if (verbose < 0)
        printwarn = 0;

    if (optind >= argc) {
        fprintf(stderr,
                _("No input files specified. \"ogginfo -h\" for help\n"));
        return 1;
    }

    ret = 0;

    for (f=optind; f < argc; f++) {
        flawed = 0;
        process_file(argv[f]);
        if (flawed != 0)
            ret = flawed;
    }

    return ret;
}
