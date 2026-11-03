/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2026 Alexia Liiv, all xrdp contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file sesman/chansrv/clipboard_dib.c
 * @brief Helpers for device-independent bitmaps on the clipboard
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stddef.h>

#if defined(XRDP_PNG)
#include <stdlib.h>
#include <string.h>
#include <png.h>
#endif

#include "clipboard_dib.h"

/* Header sizes. BITMAPV2INFOHEADER (52) to BITMAPV5HEADER (124) are all
 * extensions of BITMAPINFOHEADER */
#define BITMAPCOREHEADER_SIZE       12
#define BITMAPINFOHEADER_SIZE       40
#define BITMAPV3INFOHEADER_SIZE     56
#define BITMAPV5HEADER_SIZE         124

/* biCompression values */
#define DIB_BI_RGB                  0
#define DIB_BI_BITFIELDS            3
#define DIB_BI_PNG                  5
#define DIB_BI_ALPHABITFIELDS       6

/* Largest bitmap converted: a 16K screen, or 2^26 pixels */
#define DIB_MAX_DIMENSION           32768
#define DIB_MAX_PIXELS              (1u << 26)

/*****************************************************************************/
static unsigned int
get_uint16_le(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

/*****************************************************************************/
static unsigned int
get_uint32_le(const unsigned char *p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

/*****************************************************************************/
int
clipboard_dib_pixel_offset(const char *dib, int dib_len)
{
    const unsigned char *p = (const unsigned char *)dib;
    unsigned int header_size;
    unsigned int bit_count;
    unsigned int compression;
    unsigned int colors_used;
    unsigned int color_entry_size;
    unsigned int masks_size;
    unsigned int len;
    unsigned int offset;

    if (dib == NULL || dib_len < 4)
    {
        return -1;
    }
    len = (unsigned int)dib_len;
    header_size = get_uint32_le(p);

    if (header_size == BITMAPCOREHEADER_SIZE)
    {
        if (len < BITMAPCOREHEADER_SIZE)
        {
            return -1;
        }
        bit_count = get_uint16_le(p + 10);
        compression = 0;
        colors_used = 0;
        color_entry_size = 3; /* RGBTRIPLE */
    }
    else if (header_size >= BITMAPINFOHEADER_SIZE &&
             header_size <= BITMAPV5HEADER_SIZE)
    {
        if (len < header_size)
        {
            return -1;
        }
        bit_count = get_uint16_le(p + 14);
        compression = get_uint32_le(p + 16);
        colors_used = get_uint32_le(p + 32);
        color_entry_size = 4; /* RGBQUAD */
    }
    else
    {
        return -1;
    }

    masks_size = 0;
    if (header_size == BITMAPINFOHEADER_SIZE)
    {
        if (compression == DIB_BI_BITFIELDS)
        {
            masks_size = 3 * 4;
        }
        else if (compression == DIB_BI_ALPHABITFIELDS)
        {
            masks_size = 4 * 4;
        }
    }

    /* Without a count, a palette-based bitmap has a full colour table */
    if (colors_used == 0 && bit_count >= 1 && bit_count <= 8)
    {
        colors_used = 1u << bit_count;
    }

    /* A colour table cannot be larger than the data. This also keeps the
     * arithmetic below from overflowing */
    if (colors_used > len / color_entry_size)
    {
        return -1;
    }

    offset = header_size + masks_size + colors_used * color_entry_size;
    if (offset > len)
    {
        return -1;
    }

    return (int)offset;
}

#if defined(XRDP_PNG)

/* What a DIB's pixels are, as needed to read them */
struct dib_format
{
    int width;
    int height;              /* rows, always positive */
    int top_down;            /* first row in the data is the top row */
    unsigned int bit_count;
    unsigned int masks[4];   /* red, green, blue, alpha (0 = none) */
    const unsigned char *palette;
    unsigned int colors;     /* entries in the colour table */
    unsigned int color_entry_size;
    const unsigned char *pixels;
    unsigned int stride;     /* bytes per row, rows are 4-byte aligned */
};

/*****************************************************************************/
/* Value of one colour channel of a masked pixel, scaled to 0..255 */
static unsigned int
dib_channel(unsigned int pixel, unsigned int mask)
{
    unsigned int shift;
    unsigned int bits;
    unsigned int value;

    if (mask == 0)
    {
        return 0;
    }
    for (shift = 0; (mask & (1u << shift)) == 0; ++shift)
    {
    }
    value = (pixel & mask) >> shift;
    mask >>= shift;
    for (bits = 0; bits < 32 && (mask & (1u << bits)) != 0; ++bits)
    {
    }
    if (bits >= 8)
    {
        return value >> (bits - 8);
    }
    return (value * 255) / ((1u << bits) - 1);
}

/*****************************************************************************/
/* Index or packed value of pixel x in a row, for all but 24 bits */
static unsigned int
dib_pixel(const unsigned char *row, int x, unsigned int bit_count)
{
    switch (bit_count)
    {
        case 1:
            return (row[x >> 3] >> (7 - (x & 7))) & 0x01;
        case 2:
            return (row[x >> 2] >> (6 - 2 * (x & 3))) & 0x03;
        case 4:
            return (row[x >> 1] >> ((x & 1) ? 0 : 4)) & 0x0f;
        case 8:
            return row[x];
        case 16:
            return get_uint16_le(row + 2 * x);
        default: /* 32 */
            return get_uint32_le(row + 4 * x);
    }
}

/*****************************************************************************/
/* returns 0 if the DIB can be converted, filling in f */
static int
dib_get_format(const unsigned char *p, unsigned int len, struct dib_format *f)
{
    unsigned int header_size;
    unsigned int compression;
    unsigned int masks_size;
    unsigned int pixel_offset;
    unsigned int pixel_count;
    long long width;
    long long height;
    unsigned long long data_size;
    int offset;

    offset = clipboard_dib_pixel_offset((const char *)p, (int)len);
    if (offset < 0)
    {
        return -1;
    }
    pixel_offset = (unsigned int)offset;
    header_size = get_uint32_le(p);
    masks_size = 0;
    f->masks[0] = f->masks[1] = f->masks[2] = f->masks[3] = 0;

    if (header_size == BITMAPCOREHEADER_SIZE)
    {
        width = get_uint16_le(p + 4);
        height = get_uint16_le(p + 6);
        f->bit_count = get_uint16_le(p + 10);
        compression = DIB_BI_RGB;
        f->color_entry_size = 3;
    }
    else
    {
        width = (int)get_uint32_le(p + 4);
        height = (int)get_uint32_le(p + 8);
        f->bit_count = get_uint16_le(p + 14);
        compression = get_uint32_le(p + 16);
        f->color_entry_size = 4;
    }

    f->top_down = (height < 0);
    if (height < 0)
    {
        height = -height;
    }
    if (width < 1 || height < 1 ||
            width > DIB_MAX_DIMENSION || height > DIB_MAX_DIMENSION)
    {
        return -1;
    }
    pixel_count = (unsigned int)(width * height);
    if (pixel_count > DIB_MAX_PIXELS)
    {
        return -1;
    }
    f->width = (int)width;
    f->height = (int)height;

    switch (f->bit_count)
    {
        case 1:
        case 2:
        case 4:
        case 8:
        case 24:
            if (compression != DIB_BI_RGB)
            {
                return -1;
            }
            break;
        case 16:
        case 32:
            if (compression == DIB_BI_BITFIELDS ||
                    compression == DIB_BI_ALPHABITFIELDS)
            {
                /* After a BITMAPINFOHEADER, or inside a larger header -
                 * either way at the same place */
                f->masks[0] = get_uint32_le(p + 40);
                f->masks[1] = get_uint32_le(p + 44);
                f->masks[2] = get_uint32_le(p + 48);
                if (compression == DIB_BI_ALPHABITFIELDS ||
                        header_size >= BITMAPV3INFOHEADER_SIZE)
                {
                    f->masks[3] = get_uint32_le(p + 52);
                }
                if (header_size == BITMAPINFOHEADER_SIZE)
                {
                    masks_size = (compression == DIB_BI_BITFIELDS) ? 12 : 16;
                }
            }
            else if (compression == DIB_BI_RGB)
            {
                /* No alpha: BI_RGB leaves the fourth byte undefined */
                if (f->bit_count == 16)
                {
                    f->masks[0] = 0x7c00;
                    f->masks[1] = 0x03e0;
                    f->masks[2] = 0x001f;
                }
                else
                {
                    f->masks[0] = 0x00ff0000;
                    f->masks[1] = 0x0000ff00;
                    f->masks[2] = 0x000000ff;
                }
            }
            else
            {
                return -1;
            }
            break;
        default:
            return -1;
    }

    /* The colour table is whatever lies between the header (and masks)
     * and the pixels */
    f->palette = p + header_size + masks_size;
    f->colors = (pixel_offset - header_size - masks_size) /
                f->color_entry_size;

    f->stride = (unsigned int)((((unsigned long long)f->width *
                                 f->bit_count + 31) / 32) * 4);
    data_size = (unsigned long long)f->stride * (unsigned int)f->height;
    if (pixel_offset + data_size > len)
    {
        return -1;
    }
    f->pixels = p + pixel_offset;
    return 0;
}

/*****************************************************************************/
/* Fill in one row of 8-bit RGBA from a DIB row */
static void
dib_row_to_rgba(const struct dib_format *f, const unsigned char *src,
                unsigned char *dst)
{
    unsigned int pixel;
    unsigned int index;
    const unsigned char *entry;
    int x;

    for (x = 0; x < f->width; ++x)
    {
        if (f->bit_count == 24)
        {
            dst[0] = src[3 * x + 2];
            dst[1] = src[3 * x + 1];
            dst[2] = src[3 * x];
            dst[3] = 0xff;
        }
        else if (f->bit_count <= 8)
        {
            index = dib_pixel(src, x, f->bit_count);
            if (index < f->colors)
            {
                entry = f->palette + index * f->color_entry_size;
                dst[0] = entry[2];
                dst[1] = entry[1];
                dst[2] = entry[0];
            }
            else
            {
                dst[0] = dst[1] = dst[2] = 0;
            }
            dst[3] = 0xff;
        }
        else
        {
            pixel = dib_pixel(src, x, f->bit_count);
            dst[0] = (unsigned char)dib_channel(pixel, f->masks[0]);
            dst[1] = (unsigned char)dib_channel(pixel, f->masks[1]);
            dst[2] = (unsigned char)dib_channel(pixel, f->masks[2]);
            dst[3] = f->masks[3] ?
                     (unsigned char)dib_channel(pixel, f->masks[3]) : 0xff;
        }
        dst += 4;
    }
}

/*****************************************************************************/
int
clipboard_dib_to_png(const char *dib, int dib_len, char **png, int *png_len)
{
    const unsigned char *p = (const unsigned char *)dib;
    struct dib_format f;
    png_image image;
    png_alloc_size_t png_size;
    unsigned char *rgba;
    unsigned char *row;
    const unsigned char *src;
    int has_alpha;
    int y;
    int i;
    int rv;

    if (dib == NULL || png == NULL || png_len == NULL || dib_len < 20)
    {
        return -1;
    }

    /* A BI_PNG bitmap carries a PNG file as its pixels */
    if (get_uint32_le(p) >= BITMAPINFOHEADER_SIZE &&
            get_uint32_le(p + 16) == DIB_BI_PNG)
    {
        int offset = clipboard_dib_pixel_offset(dib, dib_len);
        if (offset < 0 || dib_len - offset < 8 ||
                png_sig_cmp((png_const_bytep)(p + offset), 0, 8) != 0)
        {
            return -1;
        }
        *png = (char *)malloc(dib_len - offset);
        if (*png == NULL)
        {
            return -1;
        }
        memcpy(*png, p + offset, dib_len - offset);
        *png_len = dib_len - offset;
        return 0;
    }

    if (dib_get_format(p, (unsigned int)dib_len, &f) != 0)
    {
        return -1;
    }

    rgba = (unsigned char *)malloc((size_t)f.width * f.height * 4);
    if (rgba == NULL)
    {
        return -1;
    }
    for (y = 0; y < f.height; ++y)
    {
        /* PNG rows go top to bottom; a bottom-up DIB starts at the bottom */
        src = f.pixels + (size_t)f.stride *
              (unsigned int)(f.top_down ? y : f.height - 1 - y);
        row = rgba + (size_t)y * f.width * 4;
        dib_row_to_rgba(&f, src, row);
    }

    /* Windows often leaves the alpha of a 32-bit bitmap at 0 throughout -
     * that is an opaque image, not an invisible one */
    has_alpha = 0;
    if (f.masks[3] != 0)
    {
        for (i = 3; i < f.width * f.height * 4; i += 4)
        {
            if (rgba[i] != 0)
            {
                has_alpha = 1;
                break;
            }
        }
        if (!has_alpha)
        {
            for (i = 3; i < f.width * f.height * 4; i += 4)
            {
                rgba[i] = 0xff;
            }
        }
    }

    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    image.width = (png_uint_32)f.width;
    image.height = (png_uint_32)f.height;
    image.format = PNG_FORMAT_RGBA;

    rv = -1;
    *png = NULL;
    png_size = 0;
    /* First call: how big the PNG will be. Second: write it */
    if (png_image_write_get_memory_size(image, png_size, 0, rgba, 0, NULL) &&
            png_size <= 0x7fffffff)
    {
        *png = (char *)malloc(png_size);
        if (*png != NULL &&
                png_image_write_to_memory(&image, *png, &png_size, 0,
                                          rgba, 0, NULL))
        {
            *png_len = (int)png_size;
            rv = 0;
        }
        else
        {
            free(*png);
            *png = NULL;
        }
    }
    png_image_free(&image);
    free(rgba);
    return rv;
}

#endif /* XRDP_PNG */
