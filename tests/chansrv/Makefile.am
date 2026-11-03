AM_CPPFLAGS = \
  -I$(top_builddir) \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/sesman/chansrv

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                  $(top_srcdir)/tap-driver.sh

PACKAGE_STRING = "chansrv"

TESTS = test_chansrv
check_PROGRAMS = test_chansrv

test_chansrv_SOURCES = \
    test_chansrv.h \
    test_chansrv_main.c \
    test_clipboard_dib.c \
    test_clipboard_html.c

test_chansrv_CFLAGS = \
    @CHECK_CFLAGS@

test_chansrv_LDADD = \
    $(top_builddir)/sesman/chansrv/clipboard_dib.o \
    $(top_builddir)/sesman/chansrv/clipboard_html.o \
    @CHECK_LIBS@

if XRDP_PNG
AM_CPPFLAGS += -DXRDP_PNG $(LIBPNG_CFLAGS)
test_chansrv_LDADD += $(LIBPNG_LIBS)
endif
