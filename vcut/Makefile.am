## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = vcut
mandir = @MANDIR@
man_MANS = vcut.1

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@

vcut_LDADD = @OGG_LIBS@ @VORBIS_LIBS@

vcut_SOURCES = vcut.c vcut.h

EXTRA_DIST = $(man_MANS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
