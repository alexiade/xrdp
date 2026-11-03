
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <string.h>

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

    return s;
}
