## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

if BUILD_OGGDEC

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = oggdec

mandir = @MANDIR@
man_MANS = oggdec.1

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @SHARE_CFLAGS@

oggdec_LDADD = @LIBICONV@ @SHARE_LIBS@ @VORBISFILE_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@
oggdec_DEPENDENCIES = @SHARE_LIBS@

oggdec_SOURCES = oggdec.c

EXTRA_DIST = $(man_MANS)

endif

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"


