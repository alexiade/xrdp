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
 * @file sesman/chansrv/clipboard_dib.h
 * @brief Helpers for device-independent bitmaps on the clipboard
 */

#if !defined(CLIPBOARD_DIB_H)
#define CLIPBOARD_DIB_H

/**
 * Find the start of the pixel array in a packed DIB
 *
 * A packed DIB (the CF_DIB and CF_DIBV5 clipboard formats) is a bitmap
 * header followed by optional colour masks, an optional colour table and
 * then the pixels. Turning it into a BMP file means prepending a
 * BITMAPFILEHEADER, whose bfOffBits must point at the pixels - that is
 * this offset plus the size of the file header.
 *
 * The header may be a BITMAPCOREHEADER (12 bytes), a BITMAPINFOHEADER (40)
 * or one of its larger successors up to BITMAPV5HEADER (124). Colour masks
 * follow a BITMAPINFOHEADER only for BI_BITFIELDS (3 masks) and
 * BI_ALPHABITFIELDS (4 masks); the later headers contain them.
 *
 * @param dib Packed DIB
 * @param dib_len Number of bytes at dib
 * @return Offset of the pixel array from the start of the DIB, or -1 if
 *         the data does not start with a valid bitmap header, or the
 *         offset would lie beyond the data
 */
int
clipboard_dib_pixel_offset(const char *dib, int dib_len);

#endif /* CLIPBOARD_DIB_H */
