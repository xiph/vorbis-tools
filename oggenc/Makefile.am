## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

SUBDIRS = man

if BUILD_OGGENC

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = oggenc

INCLUDES = @SHARE_CFLAGS@ @OGG_CFLAGS@ @VORBIS_CFLAGS@ @I18N_CFLAGS@

oggenc_LDADD = @SHARE_LIBS@ \
	       @VORBISENC_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ \
	       @LIBICONV@ @I18N_LIBS@
oggenc_DEPENDENCIES = @SHARE_LIBS@

oggenc_SOURCES = oggenc.c audio.c encode.c platform.c \
                 audio.h encode.h platform.h resample.c resample.h

endif

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
