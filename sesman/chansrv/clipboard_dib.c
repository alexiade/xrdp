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

#include "clipboard_dib.h"

/* Header sizes. BITMAPV2INFOHEADER (52) to BITMAPV5HEADER (124) are all
 * extensions of BITMAPINFOHEADER */
#define BITMAPCOREHEADER_SIZE       12
#define BITMAPINFOHEADER_SIZE       40
#define BITMAPV5HEADER_SIZE         124

/* biCompression values which add colour masks after a BITMAPINFOHEADER */
#define DIB_BI_BITFIELDS            3
#define DIB_BI_ALPHABITFIELDS       6

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
