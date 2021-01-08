## Process this file with automake to produce Makefile.in

mans = ogginfo.1
ogginfosources = \
	ogginfo2.c \
	metadata.c \
	theora.c \
	codec_vorbis.c \
	codec_theora.c \
	codec_kate.c \
	codec_opus.c \
	codec_speex.c \
	codec_flac.c \
	codec_skeleton.c \
	codec_other.c \
	codec_invalid.c

noinst_HEADERS = \
	private.h \
	theora.h

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = ogginfo
mandir = @MANDIR@
man_MANS = $(mans)

AM_CPPFLAGS = @SHARE_CFLAGS@ @OGG_CFLAGS@ @VORBIS_CFLAGS@ @KATE_CFLAGS@ @I18N_CFLAGS@

ogginfo_LDADD = @SHARE_LIBS@ @VORBIS_LIBS@ @KATE_LIBS@ @OGG_LIBS@ @LIBICONV@ @I18N_LIBS@ \
		$(top_builddir)/share/libpicture.a $(top_builddir)/share/libbase64.a
ogginfo_DEPENDENCIES = @SHARE_LIBS@ \
		       $(top_builddir)/share/libpicture.a $(top_builddir)/share/libbase64.a

ogginfo_SOURCES = $(ogginfosources)
EXTRA_ogginfo_SOURCES = $(man_MANS) 

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
