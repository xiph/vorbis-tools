## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

SUBDIRS = man

bin_PROGRAMS = oggenc

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @SHARE_CFLAGS@

oggenc_LDADD = @VORBISENC_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ @LIBICONV@ \
               @SHARE_LIBS@
oggenc_DEPENDENCIES = @SHARE_LIBS@

oggenc_SOURCES = oggenc.c audio.c encode.c platform.c \
                 audio.h encode.h platform.h

#EXTRA_DIST = oggenc.dsp build_oggenc.bat

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
