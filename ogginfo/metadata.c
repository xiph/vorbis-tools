/* Ogginfo
 *
 * A tool to describe ogg file contents and metadata.
 *
 * Copyright 2002-2005 Michael Smith <msmith@xiph.org>
 * Copyright 2020-2021 Philipp Schafft <lion@lion.leolix.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <ogg/ogg.h>

#include "utf8.h"
#include "i18n.h"

#include "private.h"

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

        buf = malloc(user_comment_string_length + 1);
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
            if (strcasecmp(comment, "METADATA_BLOCK_PICTURE") == 0) {
                flac_picture_t *picture = flac_picture_parse_from_base64(decoded);
                check_flac_picture(picture, "\t");
                flac_picture_free(picture);
            }
            free(decoded);
        }
    }
}

void check_flac_picture(flac_picture_t *picture, const char *prefix)
{
    if (!prefix)
        prefix = "";

    if (!picture) {
        warn("%s%s\n", prefix, _("Picture: <corrupted>"));
        return;
    }

    info("%s", prefix);
    info(_("Picture: %d (%s)\n"), (int)picture->type, flac_picture_type_string(picture->type));

    if (picture->media_type) {
        info("%s", prefix);
        info(_("\tMIME-Type: %s\n"), picture->media_type);
    }

    if (picture->description) {
        info("%s", prefix);
        info(_("\tDescription: %s\n"), picture->description);
    }

    info("%s", prefix);
    info(_("\tWidth: %ld\n"), (long int)picture->width);
    info("%s", prefix);
    info(_("\tHeight: %ld\n"), (long int)picture->height);
    info("%s", prefix);
    info(_("\tColor depth: %ld\n"), (long int)picture->depth);
    if (picture->colors) {
        info("%s", prefix);
        info(_("\tUsed colors: %ld\n"), (long int)picture->colors);
    }

    if (picture->uri) {
        info("%s", prefix);
        info(_("\tURL: %s\n"), picture->uri);
    }

    if (picture->binary_length) {
        info("%s", prefix);
        info(_("\tSize: %ld bytes\n"), (long int)picture->binary_length);
    }
}

