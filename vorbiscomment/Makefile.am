## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

EXTRA_PROGRAMS = vorbiscomment

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@

vorbiscomment_LDFLAGS = @OGG_LIBS@ @VORBIS_LIBS@
vorbiscomment_SOURCES = vcedit.c vcomment.c

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
