## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = vcut

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@

vcut_LDADD = @OGG_LIBS@ @VORBIS_LIBS@

vcut_SOURCES = vcut.c vcut.h

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
