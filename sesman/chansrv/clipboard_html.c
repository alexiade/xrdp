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
 * @file sesman/chansrv/clipboard_html.c
 * @brief HTML on the clipboard: Windows' "HTML Format" and X's text/html
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clipboard_html.h"

/* The header is short - a few lines of key:value before the first '<' */
#define CF_HTML_MAX_HEADER  4096

/* Largest HTML handled, well below INT_MAX so offsets cannot overflow */
#define HTML_MAX_LEN        (256 * 1024 * 1024)

/* The CF_HTML header written by clipboard_cf_html_make(). Fixed-width
 * unsigned numbers: the header is the same length whatever the offsets
 * are (10 digits hold any unsigned int) */
#define CF_HTML_HEADER_FORMAT \
    "Version:0.9\r\n" \
    "StartHTML:%010u\r\n" \
    "EndHTML:%010u\r\n" \
    "StartFragment:%010u\r\n" \
    "EndFragment:%010u\r\n"

/*****************************************************************************/
/* Value of "key:" at the start of a line of header (NUL terminated).
 * returns 0 if found */
static int
cf_html_header_value(const char *header, const char *key, long *value)
{
    const char *line = header;
    size_t key_len = strlen(key);
    char *end;
    long v;

    while (line != NULL && *line != '\0')
    {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':')
        {
            v = strtol(line + key_len + 1, &end, 10);
            if (end == line + key_len + 1)
            {
                return -1;
            }
            *value = v;
            return 0;
        }
        line = strchr(line, '\n');
        if (line != NULL)
        {
            ++line;
        }
    }
    return -1;
}

/*****************************************************************************/
int
clipboard_cf_html_get(const char *data, int data_len,
                      int *html_start, int *html_len)
{
    char header[CF_HTML_MAX_HEADER + 1];
    const char *first_tag;
    int header_len;
    long start_html = -1;
    long end_html = -1;
    long start_fragment = -1;
    long end_fragment = -1;

    if (data == NULL || html_start == NULL || html_len == NULL)
    {
        return -1;
    }
    /* Windows terminates the item with a NUL - not part of the HTML */
    while (data_len > 0 && data[data_len - 1] == '\0')
    {
        --data_len;
    }
    if (data_len < 1)
    {
        return -1;
    }

    first_tag = (const char *)memchr(data, '<', data_len);
    header_len = (first_tag != NULL) ? (int)(first_tag - data) : data_len;
    if (header_len > CF_HTML_MAX_HEADER)
    {
        header_len = CF_HTML_MAX_HEADER;
    }
    memcpy(header, data, header_len);
    header[header_len] = '\0';

    cf_html_header_value(header, "StartHTML", &start_html);
    cf_html_header_value(header, "EndHTML", &end_html);
    cf_html_header_value(header, "StartFragment", &start_fragment);
    cf_html_header_value(header, "EndFragment", &end_fragment);

    if (start_html >= 0 && end_html > start_html && end_html <= data_len)
    {
        *html_start = (int)start_html;
        *html_len = (int)(end_html - start_html);
        return 0;
    }
    if (start_fragment >= 0 && end_fragment > start_fragment &&
            end_fragment <= data_len)
    {
        *html_start = (int)start_fragment;
        *html_len = (int)(end_fragment - start_fragment);
        return 0;
    }
    return -1;
}

/*****************************************************************************/
/* Case-insensitive search for needle (lower case) in hay[0..hay_len).
 * last: the last occurrence instead of the first */
static const char *
find_tag(const char *hay, int hay_len, const char *needle, int last)
{
    int needle_len = (int)strlen(needle);
    const char *found = NULL;
    int i;
    int j;

    for (i = 0; i + needle_len <= hay_len; ++i)
    {
        for (j = 0; j < needle_len; ++j)
        {
            if (tolower((unsigned char)hay[i + j]) != needle[j])
            {
                break;
            }
        }
        if (j == needle_len)
        {
            found = hay + i;
            if (!last)
            {
                break;
            }
        }
    }
    return found;
}

/*****************************************************************************/
int
clipboard_cf_html_make(const char *html, int html_len,
                       char **out, int *out_len)
{
    static const char prefix[] = "<html><body>\r\n<!--StartFragment-->";
    static const char suffix[] = "<!--EndFragment-->\r\n</body></html>";
    const char *body;
    const char *body_open_end = NULL;
    const char *body_close = NULL;
    char *buf;
    int header_len;
    int doc_len;
    int start_fragment;
    int end_fragment;
    int wrap;
    int total;

    if (html == NULL || out == NULL || out_len == NULL ||
            html_len < 0 || html_len > HTML_MAX_LEN)
    {
        return -1;
    }

    /* A complete document: its body's content is the fragment */
    body = find_tag(html, html_len, "<body", 0);
    if (body != NULL)
    {
        body_open_end = (const char *)memchr(body, '>',
                                             html_len - (int)(body - html));
        body_close = find_tag(html, html_len, "</body", 1);
    }
    wrap = (body_open_end == NULL || body_close == NULL ||
            body_close <= body_open_end);

    header_len = snprintf(NULL, 0, CF_HTML_HEADER_FORMAT, 0u, 0u, 0u, 0u);
    if (wrap)
    {
        doc_len = (int)(sizeof(prefix) - 1) + html_len +
                  (int)(sizeof(suffix) - 1);
        start_fragment = header_len + (int)(sizeof(prefix) - 1);
        end_fragment = start_fragment + html_len;
    }
    else
    {
        doc_len = html_len;
        start_fragment = header_len + (int)(body_open_end + 1 - html);
        end_fragment = header_len + (int)(body_close - html);
    }
    total = header_len + doc_len + 1;

    buf = (char *)malloc(total);
    if (buf == NULL)
    {
        return -1;
    }
    snprintf(buf, header_len + 1, CF_HTML_HEADER_FORMAT,
             (unsigned int)header_len, (unsigned int)(header_len + doc_len),
             (unsigned int)start_fragment, (unsigned int)end_fragment);
    if (wrap)
    {
        memcpy(buf + header_len, prefix, sizeof(prefix) - 1);
        memcpy(buf + header_len + sizeof(prefix) - 1, html, html_len);
        memcpy(buf + header_len + sizeof(prefix) - 1 + html_len,
               suffix, sizeof(suffix) - 1);
    }
    else
    {
        memcpy(buf + header_len, html, html_len);
    }
    buf[total - 1] = '\0';

    *out = buf;
    *out_len = total;
    return 0;
}

/*****************************************************************************/
/* Write c as UTF-8, return the number of bytes */
static int
put_utf8(unsigned int c, char *p)
{
    if (c < 0x80)
    {
        p[0] = (char)c;
        return 1;
    }
    if (c < 0x800)
    {
        p[0] = (char)(0xc0 | (c >> 6));
        p[1] = (char)(0x80 | (c & 0x3f));
        return 2;
    }
    if (c < 0x10000)
    {
        p[0] = (char)(0xe0 | (c >> 12));
        p[1] = (char)(0x80 | ((c >> 6) & 0x3f));
        p[2] = (char)(0x80 | (c & 0x3f));
        return 3;
    }
    p[0] = (char)(0xf0 | (c >> 18));
    p[1] = (char)(0x80 | ((c >> 12) & 0x3f));
    p[2] = (char)(0x80 | ((c >> 6) & 0x3f));
    p[3] = (char)(0x80 | (c & 0x3f));
    return 4;
}

/*****************************************************************************/
int
clipboard_html_to_utf8(const char *data, int data_len,
                       char **out, int *out_len)
{
    const unsigned char *p = (const unsigned char *)data;
    int utf16 = 0;
    int big_endian = 0;
    int skip = 0;
    int units;
    int i;
    int n;
    unsigned int c;
    unsigned int c2;
    char *buf;

    if (data == NULL || out == NULL || out_len == NULL ||
            data_len < 0 || data_len > HTML_MAX_LEN)
    {
        return -1;
    }

    if (data_len >= 2 && p[0] == 0xff && p[1] == 0xfe)
    {
        utf16 = 1;
        skip = 2;
    }
    else if (data_len >= 2 && p[0] == 0xfe && p[1] == 0xff)
    {
        utf16 = 1;
        big_endian = 1;
        skip = 2;
    }
    else if (data_len >= 4 && p[0] != 0 && p[1] == 0 &&
             p[2] != 0 && p[3] == 0)
    {
        /* Little-endian UTF-16 without a byte order mark: HTML starts
         * with ASCII, so every second byte is zero */
        utf16 = 1;
    }
    else if (data_len >= 3 && p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf)
    {
        skip = 3;
    }

    if (!utf16)
    {
        buf = (char *)malloc(data_len - skip + 1);
        if (buf == NULL)
        {
            return -1;
        }
        memcpy(buf, data + skip, data_len - skip);
        *out = buf;
        *out_len = data_len - skip;
        return 0;
    }

    /* At most 3 bytes of UTF-8 per UTF-16 code unit (a surrogate pair is
     * 2 units and 4 bytes) */
    units = (data_len - skip) / 2;
    buf = (char *)malloc((size_t)units * 3 + 1);
    if (buf == NULL)
    {
        return -1;
    }
    n = 0;
    for (i = 0; i < units; ++i)
    {
        const unsigned char *u = p + skip + 2 * i;
        c = big_endian ? ((unsigned int)u[0] << 8) | u[1]
            : u[0] | ((unsigned int)u[1] << 8);
        if (c == 0)
        {
            break;
        }
        if (c >= 0xd800 && c < 0xdc00 && i + 1 < units)
        {
            u += 2;
            c2 = big_endian ? ((unsigned int)u[0] << 8) | u[1]
                 : u[0] | ((unsigned int)u[1] << 8);
            if (c2 >= 0xdc00 && c2 < 0xe000)
            {
                c = 0x10000 + ((c - 0xd800) << 10) + (c2 - 0xdc00);
                ++i;
            }
            else
            {
                c = 0xfffd;
            }
        }
        else if (c >= 0xd800 && c < 0xe000)
        {
            c = 0xfffd;
        }
        n += put_utf8(c, buf + n);
    }
    *out = buf;
    *out_len = n;
    return 0;
}
