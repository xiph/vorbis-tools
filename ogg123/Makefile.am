## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

mans = ogg123.1
docs = ogg123rc-example
ogg123sources = audio.c buffer.c callbacks.c \
                cfgfile_options.c cmdline_options.c \
                file_transport.c format.c http_transport.c \
                ogg123.c oggvorbis_format.c playlist.c \
                status.c transport.c  \
                audio.h buffer.h callbacks.h compat.h \
                cfgfile_options.h cmdline_options.h \
                format.h ogg123.h playlist.h status.h \
                transport.h


if BUILD_OGG123

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = ogg123
docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
mandir = @MANDIR@

INCLUDES = @OGG_CFLAGS@ @VORBIS_CFLAGS@ @AO_CFLAGS@ @CURL_CFLAGS@ \
           @PTHREAD_CFLAGS@ @SHARE_CFLAGS@ @I18N_CFLAGS@

ogg123_LDADD = @SHARE_LIBS@ \
	       @VORBISFILE_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ @AO_LIBS@ \
               @SOCKET_LIBS@ @LIBICONV@ @CURL_LIBS@ @PTHREAD_CFLAGS@ \
               @PTHREAD_LIBS@ @I18N_LIBS@
ogg123_DEPENDENCIES = @SHARE_LIBS@
ogg123_SOURCES = $(ogg123sources)

man_MANS = $(mans) 
doc_DATA = $(docs)

endif

EXTRA_DIST = $(ogg123sources) $(mans) $(docs)


debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
