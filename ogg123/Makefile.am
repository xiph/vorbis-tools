## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = ogg123
docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
doc_DATA = ogg123rc-example
mandir = @MANDIR@
man_MANS = ogg123.1

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @AO_CFLAGS@ @CURL_CFLAGS@ \
           @PTHREAD_CFLAGS@ @SHARE_CFLAGS@

ogg123_LDADD = @VORBISFILE_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ @AO_LIBS@ \
               @SOCKET_LIBS@ @SHARE_LIBS@ @CURL_LIBS@ @PTHREAD_CFLAGS@ \
               @PTHREAD_LIBS@
ogg123_DEPENDENCIES = @SHARE_LIBS@
ogg123_SOURCES = audio.c buffer.c callbacks.c \
                 cfgfile_options.c cmdline_options.c \
                 file_transport.c format.c http_transport.c \
                 ogg123.c oggvorbis_format.c status.c transport.c     \
                 audio.h buffer.h callbacks.h compat.h \
                 cfgfile_options.h cmdline_options.h \
                 format.h ogg123.h status.h transport.h

EXTRA_DIST = $(man_MANS) $(doc_DATA)

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
