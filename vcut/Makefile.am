## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

mans = vcut.1
vcutsources = vcut.c vcut.h

if BUILD_VCUT

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = vcut
mandir = @MANDIR@
man_MANS = $(mans)

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @SHARE_CFLAGS@ @I18N_CFLAGS@

vcut_LDADD = @VORBIS_LIBS@ @OGG_LIBS@ @I18N_LIBS@

vcut_SOURCES = $(vcutsources)

endif

EXTRA_DIST = $(vcutsources) $(mans)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
