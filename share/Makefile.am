## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

INCLUDES = -I$(top_srcdir)/include

noinst_LIBRARIES = libutf8.a libgetopt.a

libutf8_a_SOURCES = utf8.c
MAP_FILES = 8859-1.map 8859-2.map

libgetopt_a_SOURCES = getopt.c getopt1.c

EXTRA_DIST = $(MAP_FILES) charsetmap.h make_code_map.pl

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
