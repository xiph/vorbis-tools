## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = vorbiscomment
mandir = @MANDIR@
man_MANS = vorbiscomment.1

INCLUDES = @SHARE_CFLAGS@ @I18N_CFLAGS@ @OGG_CFLAGS@ @VORBIS_CFLAGS@

vorbiscomment_LDADD = @SHARE_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ @LIBICONV@ @I18N_LIBS@
vorbiscomment_DEPENDENCIES = @SHARE_LIBS@

vorbiscomment_SOURCES = vcedit.c vcedit.h vcomment.c

EXTRA_DIST = $(man_MANS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
