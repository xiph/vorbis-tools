## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = oggchain
mandir = @MANDIR@
man_MANS = #oggchain.1

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@

oggchain_LDADD = @VORBISFILE_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@

oggchain_SOURCES = oggchain.c oggchain.h

EXTRA_DIST = $(man_MANS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
