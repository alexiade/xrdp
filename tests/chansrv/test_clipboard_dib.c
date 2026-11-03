
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdlib.h>
#include <string.h>

#if defined(XRDP_PNG)
#include <png.h>
#endif

#include "clipboard_dib.h"

#include "test_chansrv.h"

/* Room for the largest header plus a full 256 entry colour table */
#define DIB_BUF_SIZE (124 + 256 * 4 + 64)

static unsigned char dib[DIB_BUF_SIZE];

/*****************************************************************************/
static void
put_uint16_le(unsigned char *p, unsigned int v)
{
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
}

/*****************************************************************************/
static void
put_uint32_le(unsigned char *p, unsigned int v)
{
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
}

/*****************************************************************************/
/* A BITMAPINFOHEADER (or larger) with the fields that decide the offset */
static void
make_info_header(unsigned int header_size, unsigned int bit_count,
                 unsigned int compression, unsigned int colors_used)
{
    memset(dib, 0, sizeof(dib));
    put_uint32_le(dib, header_size);
    put_uint32_le(dib + 4, 16);  /* biWidth */
    put_uint32_le(dib + 8, 16);  /* biHeight */
    put_uint16_le(dib + 12, 1);  /* biPlanes */
    put_uint16_le(dib + 14, bit_count);
    put_uint32_le(dib + 16, compression);
    put_uint32_le(dib + 32, colors_used);
}

/*****************************************************************************/
static int
offset_of(int len)
{
    return clipboard_dib_pixel_offset((const char *)dib, len);
}

/*****************************************************************************/
START_TEST(test_dib_24bit_rgb)
{
    make_info_header(40, 24, 0, 0);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 40);
}
END_TEST

/*****************************************************************************/
/* What Windows puts on the clipboard for a 32-bit image: three colour masks
 * between the header and the pixels */
START_TEST(test_dib_32bit_bitfields)
{
    make_info_header(40, 32, 3, 0);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 40 + 12);
}
END_TEST

/*****************************************************************************/
START_TEST(test_dib_32bit_alphabitfields)
{
    make_info_header(40, 32, 6, 0);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 40 + 16);
}
END_TEST

/*****************************************************************************/
/* A V5 header contains its masks - nothing follows it */
START_TEST(test_dib_v5_bitfields)
{
    make_info_header(124, 32, 3, 0);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 124);
}
END_TEST

/*****************************************************************************/
START_TEST(test_dib_v4_rgb)
{
    make_info_header(108, 24, 0, 0);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 108);
}
END_TEST

/*****************************************************************************/
START_TEST(test_dib_8bit_full_palette)
{
    make_info_header(40, 8, 0, 0);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 40 + 256 * 4);
}
END_TEST

/*****************************************************************************/
START_TEST(test_dib_8bit_short_palette)
{
    make_info_header(40, 8, 0, 16);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 40 + 16 * 4);
}
END_TEST

/*****************************************************************************/
START_TEST(test_dib_1bit)
{
    make_info_header(40, 1, 0, 0);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 40 + 2 * 4);
}
END_TEST

/*****************************************************************************/
/* A colour table is allowed for >8 bits too, as a hint for display */
START_TEST(test_dib_24bit_with_colors_used)
{
    make_info_header(40, 24, 0, 4);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 40 + 4 * 4);
}
END_TEST

/*****************************************************************************/
START_TEST(test_dib_core_header)
{
    memset(dib, 0, sizeof(dib));
    put_uint32_le(dib, 12);
    put_uint16_le(dib + 4, 16);  /* bcWidth */
    put_uint16_le(dib + 6, 16);  /* bcHeight */
    put_uint16_le(dib + 8, 1);   /* bcPlanes */
    put_uint16_le(dib + 10, 8);  /* bcBitCount */
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), 12 + 256 * 3);
}
END_TEST

/*****************************************************************************/
START_TEST(test_dib_invalid_header_size)
{
    make_info_header(20, 24, 0, 0);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), -1);
    make_info_header(200, 24, 0, 0);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), -1);
}
END_TEST

/*****************************************************************************/
START_TEST(test_dib_truncated)
{
    make_info_header(40, 24, 0, 0);
    ck_assert_int_eq(offset_of(39), -1);
    ck_assert_int_eq(offset_of(3), -1);
    ck_assert_int_eq(clipboard_dib_pixel_offset(NULL, 100), -1);
}
END_TEST

/*****************************************************************************/
/* The colour table must fit in the data */
START_TEST(test_dib_palette_beyond_data)
{
    make_info_header(40, 8, 0, 0);
    ck_assert_int_eq(offset_of(40 + 255 * 4), -1);
    ck_assert_int_eq(offset_of(40 + 256 * 4), 40 + 256 * 4);
}
END_TEST

/*****************************************************************************/
/* A huge biClrUsed must not overflow the offset arithmetic */
START_TEST(test_dib_huge_colors_used)
{
    make_info_header(40, 24, 0, 0xffffffff);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), -1);
    make_info_header(40, 24, 0, 0x40000000);
    ck_assert_int_eq(offset_of(DIB_BUF_SIZE), -1);
}
END_TEST

#if defined(XRDP_PNG)

/*****************************************************************************/
/* Convert dib[0..len) to PNG and decode that again into rgba (w*h*4) */
static int
convert_and_decode(int len, int w, int h, unsigned char *rgba)
{
    char *png = NULL;
    int png_len = 0;
    png_image image;
    int rv = -1;

    if (clipboard_dib_to_png((const char *)dib, len, &png, &png_len) != 0)
    {
        return -1;
    }
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&image, png, png_len))
    {
        if ((int)image.width == w && (int)image.height == h)
        {
            image.format = PNG_FORMAT_RGBA;
            if (png_image_finish_read(&image, NULL, rgba, 0, NULL))
            {
                rv = 0;
            }
        }
        png_image_free(&image);
    }
    free(png);
    return rv;
}

/*****************************************************************************/
static void
check_rgba(const unsigned char *rgba, int w, int x, int y,
           unsigned int r, unsigned int g, unsigned int b, unsigned int a)
{
    const unsigned char *px = rgba + (y * w + x) * 4;
    ck_assert_uint_eq(px[0], r);
    ck_assert_uint_eq(px[1], g);
    ck_assert_uint_eq(px[2], b);
    ck_assert_uint_eq(px[3], a);
}

/*****************************************************************************/
/* What Windows puts on the clipboard for a screenshot: 32 bits,
 * BI_BITFIELDS, bottom-up, alpha bytes all zero - an opaque image */
START_TEST(test_png_32bit_bitfields)
{
    /* BGRA, bottom row first: red, green - then the top row: blue, white */
    static const unsigned char pixels[] =
    {
        0x00, 0x00, 0xff, 0x00,   0x00, 0xff, 0x00, 0x00,
        0xff, 0x00, 0x00, 0x00,   0xff, 0xff, 0xff, 0x00
    };
    unsigned char rgba[2 * 2 * 4];

    make_info_header(40, 32, 3, 0);
    put_uint32_le(dib + 4, 2);
    put_uint32_le(dib + 8, 2);
    put_uint32_le(dib + 40, 0x00ff0000);
    put_uint32_le(dib + 44, 0x0000ff00);
    put_uint32_le(dib + 48, 0x000000ff);
    memcpy(dib + 52, pixels, sizeof(pixels));

    ck_assert_int_eq(convert_and_decode(52 + 16, 2, 2, rgba), 0);
    check_rgba(rgba, 2, 0, 0, 0, 0, 0xff, 0xff);
    check_rgba(rgba, 2, 1, 0, 0xff, 0xff, 0xff, 0xff);
    check_rgba(rgba, 2, 0, 1, 0xff, 0, 0, 0xff);
    check_rgba(rgba, 2, 1, 1, 0, 0xff, 0, 0xff);
}
END_TEST

/*****************************************************************************/
/* 24 bits: 3 pixels make 9 bytes, padded to 12 per row */
START_TEST(test_png_24bit_padded_top_down)
{
    /* BGR, top row first: RGB (1,2,3) (4,5,6) (7,8,9), then the bottom
     * row - each padded with 3 bytes */
    static const unsigned char pixels[] =
    {
        3, 2, 1,     6, 5, 4,     9, 8, 7,      0, 0, 0,
        30, 20, 10,  60, 50, 40,  90, 80, 70,   0, 0, 0
    };
    unsigned char rgba[3 * 2 * 4];

    make_info_header(40, 24, 0, 0);
    put_uint32_le(dib + 4, 3);
    put_uint32_le(dib + 8, (unsigned int)(-2)); /* top-down */
    memcpy(dib + 40, pixels, sizeof(pixels));

    ck_assert_int_eq(convert_and_decode(40 + 24, 3, 2, rgba), 0);
    check_rgba(rgba, 3, 0, 0, 1, 2, 3, 0xff);
    check_rgba(rgba, 3, 2, 0, 7, 8, 9, 0xff);
    check_rgba(rgba, 3, 0, 1, 10, 20, 30, 0xff);
    check_rgba(rgba, 3, 2, 1, 70, 80, 90, 0xff);
}
END_TEST

/*****************************************************************************/
START_TEST(test_png_8bit_palette)
{
    /* A 2 entry colour table (BGR + reserved), then the pixels: 1, 0 */
    static const unsigned char table_and_pixels[] =
    {
        0x10, 0x20, 0x30, 0,   0x40, 0x50, 0x60, 0,
        1, 0, 0, 0
    };
    unsigned char rgba[2 * 1 * 4];

    make_info_header(40, 8, 0, 2);
    put_uint32_le(dib + 4, 2);
    put_uint32_le(dib + 8, 1);
    memcpy(dib + 40, table_and_pixels, sizeof(table_and_pixels));

    ck_assert_int_eq(convert_and_decode(48 + 4, 2, 1, rgba), 0);
    check_rgba(rgba, 2, 0, 0, 0x60, 0x50, 0x40, 0xff);
    check_rgba(rgba, 2, 1, 0, 0x30, 0x20, 0x10, 0xff);
}
END_TEST

/*****************************************************************************/
/* 1 bit: 9 pixels span two bytes */
START_TEST(test_png_1bit)
{
    unsigned char rgba[9 * 1 * 4];

    make_info_header(40, 1, 0, 0);
    put_uint32_le(dib + 4, 9);
    put_uint32_le(dib + 8, 1);
    /* palette: black, white */
    dib[44] = dib[45] = dib[46] = 0xff;
    dib[48] = 0x81;   /* pixels 0 and 7 white */
    dib[49] = 0x80;   /* pixel 8 white */

    ck_assert_int_eq(convert_and_decode(48 + 4, 9, 1, rgba), 0);
    check_rgba(rgba, 9, 0, 0, 0xff, 0xff, 0xff, 0xff);
    check_rgba(rgba, 9, 1, 0, 0, 0, 0, 0xff);
    check_rgba(rgba, 9, 7, 0, 0xff, 0xff, 0xff, 0xff);
    check_rgba(rgba, 9, 8, 0, 0xff, 0xff, 0xff, 0xff);
}
END_TEST

/*****************************************************************************/
/* 16 bits as 5-6-5 */
START_TEST(test_png_16bit_565)
{
    unsigned char rgba[2 * 1 * 4];

    make_info_header(40, 16, 3, 0);
    put_uint32_le(dib + 4, 2);
    put_uint32_le(dib + 8, 1);
    put_uint32_le(dib + 40, 0xf800);
    put_uint32_le(dib + 44, 0x07e0);
    put_uint32_le(dib + 48, 0x001f);
    put_uint16_le(dib + 52, 0xf800); /* red */
    put_uint16_le(dib + 54, 0x07e0); /* green */

    ck_assert_int_eq(convert_and_decode(52 + 4, 2, 1, rgba), 0);
    check_rgba(rgba, 2, 0, 0, 0xff, 0, 0, 0xff);
    check_rgba(rgba, 2, 1, 0, 0, 0xff, 0, 0xff);
}
END_TEST

/*****************************************************************************/
/* A V5 bitmap with real alpha keeps it */
START_TEST(test_png_v5_alpha)
{
    /* BGRA: half-transparent blue, opaque red */
    static const unsigned char pixels[] =
    {
        0xff, 0x00, 0x00, 0x80,   0x00, 0x00, 0xff, 0xff
    };
    unsigned char rgba[2 * 1 * 4];

    make_info_header(124, 32, 3, 0);
    put_uint32_le(dib + 4, 2);
    put_uint32_le(dib + 8, 1);
    put_uint32_le(dib + 40, 0x00ff0000);
    put_uint32_le(dib + 44, 0x0000ff00);
    put_uint32_le(dib + 48, 0x000000ff);
    put_uint32_le(dib + 52, 0xff000000);
    memcpy(dib + 124, pixels, sizeof(pixels));

    ck_assert_int_eq(convert_and_decode(124 + 8, 2, 1, rgba), 0);
    check_rgba(rgba, 2, 0, 0, 0, 0, 0xff, 0x80);
    check_rgba(rgba, 2, 1, 0, 0xff, 0, 0, 0xff);
}
END_TEST

/*****************************************************************************/
/* A BI_PNG bitmap is its own PNG */
START_TEST(test_png_bi_png)
{
    char *png1 = NULL;
    char *png2 = NULL;
    int len1 = 0;
    int len2 = 0;

    make_info_header(40, 24, 0, 0);
    put_uint32_le(dib + 4, 1);
    put_uint32_le(dib + 8, 1);
    ck_assert_int_eq(clipboard_dib_to_png((const char *)dib, 40 + 4,
                                          &png1, &len1), 0);
    ck_assert_int_le(40 + len1, DIB_BUF_SIZE);

    make_info_header(40, 0, 5, 0);
    put_uint32_le(dib + 4, 1);
    put_uint32_le(dib + 8, 1);
    memcpy(dib + 40, png1, len1);
    ck_assert_int_eq(clipboard_dib_to_png((const char *)dib, 40 + len1,
                                          &png2, &len2), 0);
    ck_assert_int_eq(len2, len1);
    ck_assert_int_eq(memcmp(png1, png2, len1), 0);
    free(png1);
    free(png2);
}
END_TEST

/*****************************************************************************/
START_TEST(test_png_rejects)
{
    char *png = NULL;
    int png_len = 0;

    /* pixel data cut short */
    make_info_header(40, 24, 0, 0);
    put_uint32_le(dib + 4, 3);
    put_uint32_le(dib + 8, 2);
    ck_assert_int_eq(clipboard_dib_to_png((const char *)dib, 40 + 23,
                                          &png, &png_len), -1);
    /* RLE8 is not converted */
    make_info_header(40, 8, 1, 0);
    put_uint32_le(dib + 4, 2);
    put_uint32_le(dib + 8, 2);
    ck_assert_int_eq(clipboard_dib_to_png((const char *)dib, DIB_BUF_SIZE,
                                          &png, &png_len), -1);
    /* no width */
    make_info_header(40, 24, 0, 0);
    ck_assert_int_eq(clipboard_dib_to_png((const char *)dib, DIB_BUF_SIZE,
                                          &png, &png_len), 0);
    free(png);
    put_uint32_le(dib + 4, 0);
    ck_assert_int_eq(clipboard_dib_to_png((const char *)dib, DIB_BUF_SIZE,
                                          &png, &png_len), -1);
}
END_TEST

/*****************************************************************************/
/* PNG back to a DIB: 24 bits, bottom-up, rows padded */
START_TEST(test_png_to_dib_round_trip)
{
    static const unsigned char pixels[] =
    {
        3, 2, 1,     6, 5, 4,     9, 8, 7,      0, 0, 0,
        30, 20, 10,  60, 50, 40,  90, 80, 70,   0, 0, 0
    };
    char *png = NULL;
    char *out = NULL;
    int png_len = 0;
    int out_len = 0;
    const unsigned char *o;

    make_info_header(40, 24, 0, 0);
    put_uint32_le(dib + 4, 3);
    put_uint32_le(dib + 8, (unsigned int)(-2)); /* top-down */
    memcpy(dib + 40, pixels, sizeof(pixels));
    ck_assert_int_eq(clipboard_dib_to_png((const char *)dib, 40 + 24,
                                          &png, &png_len), 0);
    ck_assert_int_eq(clipboard_png_to_dib(png, png_len, &out, &out_len), 0);
    free(png);

    o = (const unsigned char *)out;
    ck_assert_int_eq(out_len, 40 + 2 * 12);
    ck_assert_int_eq(clipboard_dib_pixel_offset(out, out_len), 40);
    ck_assert_uint_eq(o[4], 3);     /* width */
    ck_assert_uint_eq(o[8], 2);     /* height, positive: bottom-up */
    ck_assert_uint_eq(o[14], 24);   /* bits per pixel */
    /* The bottom row comes first now */
    ck_assert_int_eq(memcmp(o + 40, pixels + 12, 9), 0);
    ck_assert_int_eq(memcmp(o + 52, pixels, 9), 0);
    free(out);
}
END_TEST

/*****************************************************************************/
/* Transparency becomes white in a DIB */
START_TEST(test_png_to_dib_transparent_is_white)
{
    /* BGRA: fully transparent blue, opaque red */
    static const unsigned char pixels[] =
    {
        0xff, 0x00, 0x00, 0x00,   0x00, 0x00, 0xff, 0xff
    };
    char *png = NULL;
    char *out = NULL;
    int png_len = 0;
    int out_len = 0;
    const unsigned char *o;

    make_info_header(124, 32, 3, 0);
    put_uint32_le(dib + 4, 2);
    put_uint32_le(dib + 8, 1);
    put_uint32_le(dib + 40, 0x00ff0000);
    put_uint32_le(dib + 44, 0x0000ff00);
    put_uint32_le(dib + 48, 0x000000ff);
    put_uint32_le(dib + 52, 0xff000000);
    memcpy(dib + 124, pixels, sizeof(pixels));
    ck_assert_int_eq(clipboard_dib_to_png((const char *)dib, 124 + 8,
                                          &png, &png_len), 0);
    ck_assert_int_eq(clipboard_png_to_dib(png, png_len, &out, &out_len), 0);
    free(png);

    o = (const unsigned char *)out + 40;
    ck_assert_uint_eq(o[0], 0xff);  /* white, BGR */
    ck_assert_uint_eq(o[1], 0xff);
    ck_assert_uint_eq(o[2], 0xff);
    ck_assert_uint_eq(o[3], 0x00);  /* red */
    ck_assert_uint_eq(o[4], 0x00);
    ck_assert_uint_eq(o[5], 0xff);
    free(out);
}
END_TEST

/*****************************************************************************/
START_TEST(test_png_to_dib_rejects)
{
    static const char not_png[] = "this is not a PNG file at all";
    char *out = NULL;
    int out_len = 0;

    ck_assert_int_eq(clipboard_png_to_dib(not_png, sizeof(not_png),
                                          &out, &out_len), -1);
    ck_assert_int_eq(clipboard_png_to_dib(not_png, 4, &out, &out_len), -1);
    ck_assert_int_eq(clipboard_png_to_dib(NULL, 100, &out, &out_len), -1);
}
END_TEST

#endif /* XRDP_PNG */

/*****************************************************************************/
Suite *
make_suite_test_clipboard_dib(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("clipboard_dib");

    tc = tcase_create("pixel_offset");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_dib_24bit_rgb);
    tcase_add_test(tc, test_dib_32bit_bitfields);
    tcase_add_test(tc, test_dib_32bit_alphabitfields);
    tcase_add_test(tc, test_dib_v5_bitfields);
    tcase_add_test(tc, test_dib_v4_rgb);
    tcase_add_test(tc, test_dib_8bit_full_palette);
    tcase_add_test(tc, test_dib_8bit_short_palette);
    tcase_add_test(tc, test_dib_1bit);
    tcase_add_test(tc, test_dib_24bit_with_colors_used);
    tcase_add_test(tc, test_dib_core_header);
    tcase_add_test(tc, test_dib_invalid_header_size);
    tcase_add_test(tc, test_dib_truncated);
    tcase_add_test(tc, test_dib_palette_beyond_data);
    tcase_add_test(tc, test_dib_huge_colors_used);

#if defined(XRDP_PNG)
    tc = tcase_create("to_png");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_png_32bit_bitfields);
    tcase_add_test(tc, test_png_24bit_padded_top_down);
    tcase_add_test(tc, test_png_8bit_palette);
    tcase_add_test(tc, test_png_1bit);
    tcase_add_test(tc, test_png_16bit_565);
    tcase_add_test(tc, test_png_v5_alpha);
    tcase_add_test(tc, test_png_bi_png);
    tcase_add_test(tc, test_png_rejects);

    tc = tcase_create("png_to_dib");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_png_to_dib_round_trip);
    tcase_add_test(tc, test_png_to_dib_transparent_is_white);
    tcase_add_test(tc, test_png_to_dib_rejects);
#endif

    return s;
}
