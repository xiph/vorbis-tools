## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign dist-zip

SUBDIRS = po intl include share win32 @OPT_SUBDIRS@ tests
DIST_SUBDIRS = po intl include share win32 ogg123 oggenc oggdec ogginfo \
	vcut vorbiscomment m4 tests

EXTRA_DIST = config.rpath README AUTHORS COPYING CHANGES

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"

ACLOCAL_AMFLAGS = -I m4
