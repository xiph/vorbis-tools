## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

SUBDIRS = man

bin_PROGRAMS = oggenc

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@

oggenc_LDADD = @VORBISENC_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ @LIBICONV@
oggenc_SOURCES = oggenc.c audio.c encode.c getopt.c getopt1.c\
		platform.c utf8.c utf8.h\
		audio.h encode.h platform.h getopt.h

MAP_FILES = 8859-1.map 8859-2.map
EXTRA_DIST = oggenc.dsp build_oggenc.bat $(MAP_FILES) make_code_map.pl charsetmap.h

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
