## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

if PREFIX_IS_USR
mandir = $(datadir)/man
else
mandir = $(prefix)/man
endif

man_MANS = oggenc.1

EXTRA_DIST = $(man_MANS)
