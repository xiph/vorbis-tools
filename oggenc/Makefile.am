## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = oggenc

oggenc_SOURCES = oggenc.c audio.c encode.c oe_unix.c\
		audio.h encode.h platform.h @GETOPT@

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
