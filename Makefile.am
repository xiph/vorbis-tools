## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign dist-zip

SUBDIRS = po intl include share oggenc ogg123 vorbiscomment vcut oggdec ogginfo debian win32 

EXTRA_DIST = README AUTHORS COPYING vorbis-tools.spec acinclude.m4 config.h

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
