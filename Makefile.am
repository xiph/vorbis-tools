## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign dist-zip

SUBDIRS = oggenc ogg123 vorbiscomment vcut ogginfo debian

EXTRA_DIST = README AUTHORS COPYING vorbis-tools.spec acinclude.m4

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
