## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

SUBDIRS = man

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = oggenc

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @SHARE_CFLAGS@ @I18N_CFLAGS@

oggenc_LDADD = @VORBISENC_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ @LIBICONV@ \
               @SHARE_LIBS@ @I18N_LIBS@
oggenc_DEPENDENCIES = @SHARE_LIBS@

oggenc_SOURCES = oggenc.c audio.c encode.c platform.c \
                 audio.h encode.h platform.h resample.c resample.h

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
