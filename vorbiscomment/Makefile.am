## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = vorbiscomment
mandir = @MANDIR@
man_MANS = vorbiscomment.1

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @SHARE_CFLAGS@

vorbiscomment_LDADD = @VORBIS_LIBS@ @OGG_LIBS@ @LIBICONV@ @SHARE_LIBS@
vorbiscomment_DEPENDENCIES = @SHARE_LIBS@

vorbiscomment_SOURCES = vcedit.c vcedit.h vcomment.c

EXTRA_DIST = $(man_MANS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
