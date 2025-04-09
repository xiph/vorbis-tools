## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libutf8.a libgetopt.a libbase64.a libpicture.a

libutf8_a_SOURCES = charset.c charset.h iconvert.c utf8.c

libgetopt_a_SOURCES = getopt.c getopt1.c

libbase64_a_SOURCES = base64.c

libpicture_a_SOURCES = picture.c

EXTRA_DIST = charmaps.h makemap.c charset_test.c charsetmap.h

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
