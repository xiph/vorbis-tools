## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = ogg123
man_MANS = ogg123.1

docdir = $(prefix)/doc/$(PACKAGE)-$(VERSION)
doc_DATA = ogg123rc-example

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @AO_CFLAGS@

ogg123_LDFLAGS = @OGG_LIBS@ @VORBIS_LIBS@ @VORBISFILE_LIBS@ @AO_LIBS@

ogg123_SOURCES = ogg123.c

EXTRA_DIST = $(man_MANS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
