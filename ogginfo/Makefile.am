## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

mans = ogginfo.1
ogginfosources = ogginfo2.c

if BUILD_OGGINFO

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = ogginfo
mandir = @MANDIR@
man_MANS = $(mans)

INCLUDES = @SHARE_CFLAGS@ @OGG_CFLAGS@ @VORBIS_CFLAGS@ @I18N_CFLAGS@

ogginfo_LDADD = @SHARE_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ @LIBICONV@ @I18N_LIBS@
ogginfo_DEPENDENCIES = @SHARE_LIBS@

ogginfo_SOURCES = $(ogginfosources)

endif

EXTRA_DIST = $(ogginfosources) $(mans)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
