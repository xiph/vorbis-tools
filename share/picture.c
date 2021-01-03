/*
 * Copyright (C) 2021 Philipp Schafft <lion@lion.leolix.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "base64.h"
#include "picture.h"

static uint32_t read32be(unsigned char *buf)
{
    uint32_t ret = 0;

    ret = (ret << 8) | *buf++;
    ret = (ret << 8) | *buf++;
    ret = (ret << 8) | *buf++;
    ret = (ret << 8) | *buf++;

    return ret;
}

static flac_picture_t * flac_picture_parse_eat(void *data, size_t len)
{
    size_t expected_length = 32; // 8*32 bit
    size_t offset = 0;
    flac_picture_t *ret;
    uint32_t tmp;

    if (len < expected_length)
        return NULL;

    ret = calloc(1, sizeof(*ret));
    if (!ret)
        return NULL;

    ret->private_data = data;
    ret->private_data_length = len;

    ret->type = read32be(data);

    /*
    const char *media_type;
    const char *description;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
    unsigned int colors;
    const void *binary;
    size_t binary_length;
    const char *uri;
    */
    tmp = read32be(data+4);
    expected_length += tmp;
    if (len < expected_length) {
        free(ret);
        return NULL;
    }

    ret->media_type = data + 8;
    offset = 8 + tmp;
    tmp = read32be(data + offset);
    expected_length += tmp;
    if (len < expected_length) {
        free(ret);
        return NULL;
    }

    *(char*)(data + offset) = 0;
    offset += 4;
    ret->description = data + offset;
    offset += tmp;
    ret->width = read32be(data + offset);
    *(char*)(data + offset) = 0;
    offset += 4;
    ret->height = read32be(data + offset);
    offset += 4;
    ret->depth = read32be(data + offset);
    offset += 4;
    ret->colors = read32be(data + offset);
    offset += 4;
    ret->binary_length = read32be(data + offset);
    expected_length += ret->binary_length;
    if (len < expected_length) {
        free(ret);
        return NULL;
    }
    offset += 4;
    ret->binary = data + offset;

    if (strcmp(ret->media_type, "-->") == 0) {
        // Note: it is ensured ret->binary[ret->binary_length] == 0.
        ret->media_type = NULL;
        ret->uri = ret->binary;
        ret->binary = NULL;
        ret->binary_length = 0;
    }

    return ret;
}

flac_picture_t * flac_picture_parse_from_base64(const char *str)
{
    flac_picture_t *ret;
    void *data;
    size_t len;
    
    if (!str || !*str)
        return NULL;

    if (base64_decode(str, &data, &len) != 0)
        return NULL;

    ret = flac_picture_parse_eat(data, len);

    if (!ret) {
        free(data);
        return NULL;
    }

    return ret;
}

void flac_picture_free(flac_picture_t *picture)
{
    if (!picture)
        return;

    free(picture->private_data);
    free(picture);
}
