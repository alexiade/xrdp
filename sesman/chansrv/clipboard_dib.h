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

#if defined(XRDP_PNG)
/**
 * Convert a packed DIB to a PNG image
 *
 * Handles what Windows puts on the clipboard: 1, 2, 4 and 8 bits per
 * pixel with a colour table, 16 and 32 bits as BI_RGB or with colour masks
 * (BI_BITFIELDS, BI_ALPHABITFIELDS), 24 bits, bottom-up and top-down. A
 * BI_PNG bitmap is returned as the PNG it contains. Alpha is kept when
 * the header has an alpha mask and at least one pixel is not fully
 * transparent - Windows often leaves the alpha bytes at zero.
 *
 * @param dib Packed DIB (CF_DIB, CF_DIBV5)
 * @param dib_len Number of bytes at dib
 * @param[out] png PNG data on success, to be freed with g_free()
 * @param[out] png_len Number of bytes at *png
 * @return 0 on success, -1 if the DIB is invalid, unsupported (RLE, JPEG)
 *         or too large, or the conversion failed
 */
int
clipboard_dib_to_png(const char *dib, int dib_len, char **png, int *png_len);

/**
 * Convert a PNG image to a packed DIB (CF_DIB)
 *
 * The DIB is 24 bits per pixel, bottom-up, with a BITMAPINFOHEADER - what
 * every Windows application reads. Transparent areas are composited onto
 * white: most Windows applications ignore the alpha of a CF_DIB, and
 * would show them black. (Alpha survives in the PNG, which is offered to
 * the client as well.)
 *
 * @param png PNG data
 * @param png_len Number of bytes at png
 * @param[out] dib DIB on success, to be freed with g_free()
 * @param[out] dib_len Number of bytes at *dib
 * @return 0 on success, -1 if the PNG is invalid or too large, or the
 *         conversion failed
 */
int
clipboard_png_to_dib(const char *png, int png_len, char **dib, int *dib_len);
#endif

#endif /* CLIPBOARD_DIB_H */
