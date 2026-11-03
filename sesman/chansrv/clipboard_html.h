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
 * @file sesman/chansrv/clipboard_html.h
 * @brief HTML on the clipboard: Windows' "HTML Format" and X's text/html
 *
 * Windows puts HTML on the clipboard as CF_HTML: UTF-8, behind a header
 * of byte offsets to the HTML and to the fragment that was copied
 * (https://learn.microsoft.com/windows/win32/dataxchg/html-clipboard-format).
 * X applications put the bare HTML up as text/html - UTF-8, or UTF-16
 * from some applications.
 */

#if !defined(CLIPBOARD_HTML_H)
#define CLIPBOARD_HTML_H

/**
 * Find the HTML in a CF_HTML clipboard item
 *
 * The whole HTML document (StartHTML to EndHTML) if the header gives it,
 * else the fragment (StartFragment to EndFragment). Every offset is
 * checked against the data.
 *
 * @param data CF_HTML data, as received from the client
 * @param data_len Number of bytes at data
 * @param[out] html_start Offset of the HTML in data
 * @param[out] html_len Number of bytes of HTML
 * @return 0 on success, -1 if data is not CF_HTML with valid offsets
 */
int
clipboard_cf_html_get(const char *data, int data_len,
                      int *html_start, int *html_len);

/**
 * Make a CF_HTML clipboard item of UTF-8 HTML
 *
 * A complete document keeps its own structure, with the content of its
 * body as the fragment. Anything else is the fragment of a minimal
 * document wrapped around it.
 *
 * @param html UTF-8 HTML
 * @param html_len Number of bytes at html
 * @param[out] out CF_HTML, NUL terminated, to be freed with g_free()
 * @param[out] out_len Number of bytes at *out, including the NUL
 * @return 0 on success, -1 on error
 */
int
clipboard_cf_html_make(const char *html, int html_len,
                       char **out, int *out_len);

/**
 * Bring X text/html to UTF-8
 *
 * UTF-16 (with a byte order mark, or little-endian without one, as some
 * applications offer it) is converted; a UTF-8 byte order mark is
 * dropped; anything else is taken to be UTF-8 already.
 *
 * @param data text/html as received from the X selection owner
 * @param data_len Number of bytes at data
 * @param[out] out UTF-8 HTML, to be freed with g_free()
 * @param[out] out_len Number of bytes at *out
 * @return 0 on success, -1 on error
 */
int
clipboard_html_to_utf8(const char *data, int data_len,
                       char **out, int *out_len);

#endif /* CLIPBOARD_HTML_H */
