## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = ogginfo

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@

ogginfo_LDADD = @VORBISFILE_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@

ogginfo_SOURCES = ogginfo.c

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
