
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "clipboard_html.h"

#include "test_chansrv.h"

/* As Windows puts it on the clipboard (offsets counted for this text) */
static const char cf_html_doc[] =
    "Version:0.9\r\n"
    "StartHTML:0000000105\r\n"
    "EndHTML:0000000191\r\n"
    "StartFragment:0000000139\r\n"
    "EndFragment:0000000157\r\n"
    "<html><body>\r\n"
    "<!--StartFragment--><b>Hello</b> world<!--EndFragment-->\r\n"
    "</body></html>";

/*****************************************************************************/
/* The whole document: StartHTML to EndHTML */
START_TEST(test_cf_html_get_document)
{
    int start = -1;
    int len = -1;

    ck_assert_int_eq(clipboard_cf_html_get(cf_html_doc,
                                           (int)strlen(cf_html_doc) + 1,
                                           &start, &len), 0);
    ck_assert_int_eq(start, 105);
    ck_assert_int_eq(len, 191 - 105);
    ck_assert_int_eq(strncmp(cf_html_doc + 139, "<b>Hello</b> world", 18), 0);
    ck_assert_int_eq(strncmp(cf_html_doc + start, "<html><body>", 12), 0);
    ck_assert_int_eq(strncmp(cf_html_doc + start + len - 7, "</html>", 7), 0);
}
END_TEST

/*****************************************************************************/
/* StartHTML -1 (allowed): the fragment */
START_TEST(test_cf_html_get_fragment_only)
{
    static const char data[] =
        "Version:1.0\r\n"
        "StartHTML:-1\r\n"
        "EndHTML:-1\r\n"
        "StartFragment:0000000089\r\n"
        "EndFragment:0000000104\r\n"
        "<i>fragment</i>";
    int start = -1;
    int len = -1;

    ck_assert_int_eq(clipboard_cf_html_get(data, (int)strlen(data),
                                           &start, &len), 0);
    ck_assert_int_eq(start, 89);
    ck_assert_int_eq(len, 15);
    ck_assert_int_eq(strncmp(data + start, "<i>fragment", 11), 0);
}
END_TEST

/*****************************************************************************/
START_TEST(test_cf_html_get_rejects)
{
    static const char beyond[] =
        "Version:0.9\r\n"
        "StartHTML:0000000050\r\n"
        "EndHTML:0000009999\r\n"
        "<p>x</p>";
    static const char no_header[] = "<p>just html</p>";
    int start;
    int len;

    ck_assert_int_eq(clipboard_cf_html_get(beyond, (int)strlen(beyond),
                                           &start, &len), -1);
    ck_assert_int_eq(clipboard_cf_html_get(no_header, (int)strlen(no_header),
                                           &start, &len), -1);
    ck_assert_int_eq(clipboard_cf_html_get(NULL, 10, &start, &len), -1);
    ck_assert_int_eq(clipboard_cf_html_get("\0\0", 2, &start, &len), -1);
}
END_TEST

/*****************************************************************************/
/* A fragment is wrapped, and the offsets lead back to it */
START_TEST(test_cf_html_make_fragment)
{
    static const char html[] = "<b>Tere</b> maailm";
    char *out = NULL;
    int out_len = 0;
    int start;
    int len;
    const char *frag;

    ck_assert_int_eq(clipboard_cf_html_make(html, (int)strlen(html),
                                            &out, &out_len), 0);
    ck_assert_int_eq(out[out_len - 1], '\0');
    ck_assert_int_eq((int)strlen(out), out_len - 1);
    ck_assert_ptr_nonnull(strstr(out, "StartFragment:"));

    /* What a Windows application reads back */
    ck_assert_int_eq(clipboard_cf_html_get(out, out_len, &start, &len), 0);
    ck_assert_int_eq(strncmp(out + start, "<html><body>", 12), 0);
    frag = strstr(out, "<!--StartFragment-->") + 20;
    ck_assert_int_eq(strncmp(frag, html, strlen(html)), 0);
    ck_assert_int_eq(atoi(strstr(out, "StartFragment:") + 14),
                     (int)(frag - out));
    ck_assert_int_eq(atoi(strstr(out, "EndFragment:") + 12),
                     (int)(frag - out + strlen(html)));
    free(out);
}
END_TEST

/*****************************************************************************/
/* A complete document keeps its structure; its body is the fragment */
START_TEST(test_cf_html_make_document)
{
    static const char html[] =
        "<html><head><title>t</title></head>"
        "<BODY class=\"x\"><p>body</p></BODY></html>";
    char *out = NULL;
    int out_len = 0;
    int start_fragment;
    int end_fragment;
    int start;
    int len;

    ck_assert_int_eq(clipboard_cf_html_make(html, (int)strlen(html),
                                            &out, &out_len), 0);
    ck_assert_int_eq(clipboard_cf_html_get(out, out_len, &start, &len), 0);
    ck_assert_int_eq(len, (int)strlen(html));
    ck_assert_int_eq(memcmp(out + start, html, len), 0);

    start_fragment = atoi(strstr(out, "StartFragment:") + 14);
    end_fragment = atoi(strstr(out, "EndFragment:") + 12);
    ck_assert_int_eq(end_fragment - start_fragment, 11);
    ck_assert_int_eq(strncmp(out + start_fragment, "<p>body</p>", 11), 0);
    free(out);
}
END_TEST

/*****************************************************************************/
static void
check_utf8(const char *data, int data_len, const char *expect)
{
    char *out = NULL;
    int out_len = -1;

    ck_assert_int_eq(clipboard_html_to_utf8(data, data_len, &out, &out_len), 0);
    ck_assert_int_eq(out_len, (int)strlen(expect));
    ck_assert_int_eq(memcmp(out, expect, out_len), 0);
    free(out);
}

/*****************************************************************************/
START_TEST(test_html_to_utf8)
{
    /* UTF-16LE with BOM: "<p>õ</p>" */
    static const char le_bom[] =
    {
        '\xff', '\xfe', '<', 0, 'p', 0, '>', 0, '\xf5', 0,
        '<', 0, '/', 0, 'p', 0, '>', 0
    };
    /* UTF-16BE with BOM: "<b>" */
    static const char be_bom[] = { '\xfe', '\xff', 0, '<', 0, 'b', 0, '>' };
    /* UTF-16LE without BOM, with a surrogate pair (U+1F600) */
    static const char le_plain[] =
    {
        '<', 0, 'i', 0, '>', 0, '\x3d', '\xd8', '\x00', '\xde'
    };
    /* UTF-8 with BOM */
    static const char utf8_bom[] = "\xef\xbb\xbf<p>x</p>";

    check_utf8(le_bom, sizeof(le_bom), "<p>\xc3\xb5</p>");
    check_utf8(be_bom, sizeof(be_bom), "<b>");
    check_utf8(le_plain, sizeof(le_plain), "<i>\xf0\x9f\x98\x80");
    check_utf8(utf8_bom, (int)strlen(utf8_bom), "<p>x</p>");
    check_utf8("<p>plain</p>", 12, "<p>plain</p>");
}
END_TEST

/*****************************************************************************/
Suite *
make_suite_test_clipboard_html(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("clipboard_html");

    tc = tcase_create("cf_html");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_cf_html_get_document);
    tcase_add_test(tc, test_cf_html_get_fragment_only);
    tcase_add_test(tc, test_cf_html_get_rejects);
    tcase_add_test(tc, test_cf_html_make_fragment);
    tcase_add_test(tc, test_cf_html_make_document);

    tc = tcase_create("utf8");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_html_to_utf8);

    return s;
}
