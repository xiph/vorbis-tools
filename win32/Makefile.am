## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign dist-zip

EXTRA_DIST = oggenc.dsp oggenc_dynamic.dsp ogginfo.dsp tools.dsp tools.dsw\
	tools.opt vcut.dsp vorbiscomment.dsp

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
