## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = ogginfo
mandir = @MANDIR@
man_MANS = ogginfo.1

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @SHARE_CFLAGS@ @I18N_CFLAGS@

ogginfo_LDADD = @VORBIS_LIBS@ @OGG_LIBS@ @LIBICONV@ @SHARE_LIBS@ @I18N_LIBS@
ogginfo_DEPENDENCIES = @SHARE_LIBS@

ogginfo_SOURCES = ogginfo.c

EXTRA_DIST = $(man_MANS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
