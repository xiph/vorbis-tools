## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

mans = oggdec.1
oggdecsources = oggdec.c

if BUILD_OGGDEC

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = oggdec

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @SHARE_CFLAGS@

oggdec_LDADD = @LIBICONV@ @SHARE_LIBS@ @VORBISFILE_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@
oggdec_DEPENDENCIES = @SHARE_LIBS@

oggdec_SOURCES = $(oggdecsources)

man_MANS = $(mans)
mandir = @MANDIR@

endif


EXTRA_DIST = $(oggdecsources) $(mans)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"


