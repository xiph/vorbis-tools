## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

SUBDIRS = man

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = oggenc

INCLUDES = @SHARE_CFLAGS@ @OGG_CFLAGS@ @VORBIS_CFLAGS@ @I18N_CFLAGS@

oggenc_LDADD = @OGGENC_FLAC_OBJS@ @SHARE_LIBS@ \
	       @VORBISENC_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ \
	       @LIBICONV@ @I18N_LIBS@ @FLAC_LIBS@

oggenc_DEPENDENCIES = @OGGENC_FLAC_OBJS@ @SHARE_LIBS@

oggenc_SOURCES = oggenc.c audio.c encode.c platform.c \
                 audio.h encode.h platform.h resample.c resample.h

EXTRA_oggenc_SOURCES = flac.c flac.h easyflac.c easyflac.h

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
