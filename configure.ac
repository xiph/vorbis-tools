dnl Process this file with autoconf to produce a configure script

dnl ------------------------------------------------
dnl Initialization
dnl ------------------------------------------------

AC_INIT(oggenc/encode.c)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_PREREQ(2.53)

AM_INIT_AUTOMAKE([vorbis-tools],[1.1.1])
AM_MAINTAINER_MODE

AC_CONFIG_HEADER(config.h)

dnl --------------------------------------------------  
dnl Check for programs
dnl --------------------------------------------------  

dnl save $CFLAGS since AC_PROG_CC likes to insert "-g -O2"
dnl if $CFLAGS is blank
cflags_save="$CFLAGS"
AC_PROG_CC
CFLAGS="$cflags_save"

AC_PROG_LIBTOOL

ALL_LINGUAS="be cs da es fr hr hu nl ro ru sv uk"
AM_GNU_GETTEXT

dnl --------------------------------------------------
dnl Set build flags based on environment
dnl --------------------------------------------------

cflags_save="$CFLAGS"
ldflags_save="$LDFLAGS"
if test -z "$GCC"; then
        case $host in
        *-*-irix*)
                DEBUG="-g -signed"
                CFLAGS="-O2 -w -signed"
                PROFILE="-p -g3 -O2 -signed"
                ;;
        sparc-sun-solaris*)
                DEBUG="-v -g"
                CFLAGS="-xO4 -fast -w -fsimple -native -xcg92"
                PROFILE="-v -xpg -g -xO4 -fast -native -fsimple -xcg92 -Dsuncc"
                ;;
        *)
                DEBUG="-g"
                CFLAGS="-O"
                PROFILE="-g -p"
                ;;
        esac
else
        case $host in
        *-*-linux*)
                DEBUG="-g -Wall -fsigned-char"
                CFLAGS="-O20 -ffast-math -fsigned-char"
                PROFILE="-Wall -W -pg -g -O20 -ffast-math -fsigned-char"
                ;;
        sparc-sun-*)
                DEBUG="-g -Wall -fsigned-char -mv8"
                CFLAGS="-O20 -ffast-math -fsigned-char -mv8"
                PROFILE="-pg -g -O20 -fsigned-char -mv8"
                ;;
        *-*-darwin*)
                DEBUG="-fno-common -g -Wall -fsigned-char"
                CFLAGS="-fno-common -O4 -Wall -fsigned-char -ffast-math"
                PROFILE="-fno-common -O4 -Wall -pg -g -fsigned-char -ffast-math"
                ;;
        *)
                DEBUG="-g -Wall -fsigned-char"
                CFLAGS="-O20 -fsigned-char"
                PROFILE="-O20 -g -pg -fsigned-char"
                ;;
        esac
fi
CFLAGS="$CFLAGS $cflags_save"
DEBUG="$DEBUG $cflags_save"
PROFILE="$PROFILE $cflags_save"
LDFLAGS="$LDFLAGS $ldflags_save"

dnl --------------------------------------------------
dnl Allow tools to be selectively built
dnl --------------------------------------------------
AC_ARG_ENABLE(ogg123, [  --disable-ogg123   Skip building ogg123], build_ogg123="$enableval", build_ogg123="yes")
AC_ARG_ENABLE(oggdec, [  --disable-oggdec   Skip building oggdec], build_oggdec="$enableval", build_oggdec="yes")
AC_ARG_ENABLE(oggenc, [  --disable-oggenc   Skip building oggenc], build_oggenc="$enableval", build_oggenc="yes")
AC_ARG_ENABLE(ogginfo,[  --disable-ogginfo  Skip building ogginfo], build_ogginfo="$enableval", build_ogginfo="yes")
AC_ARG_ENABLE(vcut,   [  --disable-vcut     Skip building vcut], build_vcut="$enableval", build_vcut="no")
AC_ARG_ENABLE(vorbiscomment, [  --disable-vorbiscomment   Skip building vorbiscomment], build_vorbiscomment="$enableval", build_vorbiscomment="yes")
AC_ARG_WITH(flac,     [  --without-flac     Do not compile FLAC support], build_flac=no, build_flac="yes")
AC_ARG_WITH(speex,    [  --without-speex    Do not compile Speex support], build_speex=no, build_speex="yes")

dnl --------------------------------------------------
dnl Check for generally needed libraries
dnl --------------------------------------------------

HAVE_OGG=no
dnl first check through pkg-config
dnl check for pkg-config itself so we don't try the m4 macro without pkg-config
AC_CHECK_PROG(HAVE_PKG_CONFIG, pkg-config, yes)
if test "x$HAVE_PKG_CONFIG" = "xyes"
then
  PKG_CHECK_MODULES(OGG, ogg >= 1.0, HAVE_OGG=yes, HAVE_OGG=no)
fi
if test "x$HAVE_OGG" = "xno"
then
  dnl fall back to the old school test
  XIPH_PATH_OGG(,AC_MSG_ERROR(Ogg needed!))
  libs_save=$LIBS
  LIBS="$OGG_LIBS $VORBIS_LIBS"
  AC_CHECK_FUNC(oggpack_writealign, , AC_MSG_ERROR(Ogg >= 1.0 required !))
  LIBS=$libs_save
fi

dnl check for Vorbis
HAVE_VORBIS=no

dnl first check through pkg-config since it's more flexible

if test "x$HAVE_PKG_CONFIG" = "xyes"
then
  PKG_CHECK_MODULES(VORBIS, vorbis >= 1.0.1, HAVE_VORBIS=yes, HAVE_VORBIS=no)
  dnl also set VORBISENC_LIBS since an examples needs it
  dnl the old .m4 sets this to a value to use on top of VORBIS_LIBS,
  dnl so we do the same here.
  VORBISENC_LIBS="-lvorbisenc"
  VORBISFILE_LIBS="-lvorbisfile"
  AC_SUBST(VORBISENC_LIBS)
  AC_SUBST(VORBISFILE_LIBS)
fi
if test "x$HAVE_VORBIS" = "xno"
then
  dnl fall back to the old school test
  XIPH_PATH_VORBIS(,AC_MSG_ERROR(Vorbis needed!))
fi



SHARE_LIBS='$(top_builddir)/share/libutf8.a $(top_builddir)/share/libgetopt.a'
SHARE_CFLAGS='-I$(top_srcdir)/include'

I18N_CFLAGS='-I$(top_srcdir)/intl'
I18N_LIBS=$INTLLIBS

SOCKET_LIBS=
AC_CHECK_LIB(socket, socket, SOCKET_LIBS="-lsocket")
AC_CHECK_LIB(nsl, gethostbyname, SOCKET_LIBS="-lnsl $SOCKET_LIBS")


dnl --------------------------------------------------
dnl Check for ogg123 critical libraries and other optional libraries
dnl --------------------------------------------------

if test "x$build_ogg123" = xyes; then
  AC_MSG_RESULT([checking for ogg123 requirements])
  XIPH_PATH_AO(,build_ogg123=no; AC_MSG_WARN(libao missing))
  AM_PATH_CURL(,build_ogg123=no; AC_MSG_WARN(libcurl missing))

  ACX_PTHREAD(,build_ogg123=no; AC_MSG_WARN(POSIX threads missing))
fi

dnl -------------------- FLAC ----------------------

FLAC_LIBS=""
if test "x$build_flac" = xyes; then
  AC_CHECK_LIB(m,log,FLAC_LIBS="-lm")
  AC_CHECK_LIB(FLAC, [FLAC__stream_decoder_process_single],
    [have_libFLAC=yes; FLAC_LIBS="-lFLAC $FLAC_LIBS"],
    AC_MSG_WARN([libFLAC missing])
    have_libFLAC=no, [$FLAC_LIBS]
  )
  AC_CHECK_LIB(OggFLAC, [OggFLAC__stream_decoder_new],
    [FLAC_LIBS="-lOggFLAC $FLAC_LIBS $OGG_LIBS"],
    AC_MSG_WARN([libOggFLAC missing])
    have_libFLAC=no, [$FLAC_LIBS $OGG_LIBS]
  )
  AC_CHECK_HEADER(FLAC/stream_decoder.h,,
    AC_MSG_WARN(libFLAC headers missing)
    have_libFLAC=no,[ ])

  if test "x$have_libFLAC" = xyes; then
     AC_DEFINE(HAVE_LIBFLAC, 1, [Defined if we have libFLAC])
  else
     build_flac="no"
     FLAC_LIBS=""
  fi
fi
AM_CONDITIONAL(HAVE_LIBFLAC, test "x$have_libFLAC" = "xyes")
AC_SUBST(FLAC_LIBS)

dnl ------------------- Speex ------------------------

SPEEX_LIBS=""
if test "x$build_speex" = xyes; then
  AC_CHECK_LIB(m,log,SPEEX_LIBS="-lm")
  AC_CHECK_LIB(speex, [speex_decoder_init],
    [have_libspeex=yes; SPEEX_LIBS="-lspeex $SPEEX_LIBS"],
    AC_MSG_WARN(libspeex missing)
    have_libspeex=no, [$SPEEX_LIBS]
  )
  AC_CHECK_HEADER(speex/speex.h,,
    AC_MSG_WARN(libspeex headers missing)
    have_libspeex=no,[ ])

  if test "x$have_libspeex" = xyes; then
    AC_DEFINE(HAVE_LIBSPEEX, 1, [Defined if we have libspeex])
  else
    build_speex="no"
    SPEEX_LIBS=""
  fi
fi
AM_CONDITIONAL(HAVE_LIBSPEEX, test "x$have_libspeex" = "xyes")
AC_SUBST(SPEEX_LIBS)

dnl --------------------------------------------------
dnl Check for library functions
dnl --------------------------------------------------

AC_FUNC_ALLOCA
AM_ICONV
AC_CHECK_FUNCS(atexit on_exit)
AM_LANGINFO_CODESET

dnl --------------------------------------------------
dnl Work around FHS stupidity
dnl --------------------------------------------------

if test -z "$mandir"; then
    if test "$prefix" = "/usr"; then
        MANDIR='$(datadir)/man'
    else
        MANDIR='$(prefix)/man'
    fi
else
    MANDIR=$mandir
fi
AC_SUBST(MANDIR)


dnl --------------------------------------------------
dnl Do substitutions
dnl --------------------------------------------------

# add optional subdirs to the build
OPT_SUBDIRS=""
if test "x$build_ogg123" = xyes; then
  OPT_SUBDIRS="$OPT_SUBDIRS ogg123"
fi
if test "x$build_oggenc" = xyes; then
  OPT_SUBDIRS="$OPT_SUBDIRS oggenc"
fi
if test "x$build_oggdec" = xyes; then
  OPT_SUBDIRS="$OPT_SUBDIRS oggdec"
fi
if test "x$build_ogginfo" = xyes; then
  OPT_SUBDIRS="$OPT_SUBDIRS ogginfo"
fi
if test "x$build_vcut" = xyes; then
  OPT_SUBDIRS="$OPT_SUBDIRS vcut"
fi
if test "x$build_vorbiscomment" = xyes; then
  OPT_SUBDIRS="$OPT_SUBDIRS vorbiscomment"
fi
AC_SUBST(OPT_SUBDIRS)

AC_SUBST(DEBUG)
AC_SUBST(PROFILE)
AC_SUBST(SOCKET_LIBS)
AC_SUBST(SHARE_CFLAGS)
AC_SUBST(SHARE_LIBS)
AC_SUBST(CURL_CFLAGS)
AC_SUBST(CURL_LIBS)
AC_SUBST(I18N_CFLAGS)
AC_SUBST(I18N_LIBS)

AC_OUTPUT([
Makefile
m4/Makefile
po/Makefile.in
intl/Makefile
include/Makefile
share/Makefile
win32/Makefile
oggdec/Makefile
oggenc/Makefile
oggenc/man/Makefile
ogg123/Makefile
vorbiscomment/Makefile
vcut/Makefile
ogginfo/Makefile
debian/Makefile
])

if test "x$build_oggenc" = xyes -a "x$have_libFLAC" != xyes; then
    AC_MSG_WARN([FLAC and/on OggFLAC libraries or headers missing, oggenc 
will NOT be built with FLAC read support.])
fi

if test "x$build_ogg123" != xyes; then
    AC_MSG_WARN([Prerequisites for ogg123 not met, ogg123 will be skipped.
Please ensure that you have POSIX threads, libcurl and libao libraries and
headers present if you would like to build ogg123.])
else
    if test "x$have_libFLAC" != xyes; then
        AC_MSG_WARN([FLAC and/or OggFLAC libraries or headers missing, ogg123 
will NOT be built with FLAC read support.])
    fi
    if test "x$have_libspeex" != xyes; then
        AC_MSG_WARN([Speex libraries and/or headers missing, ogg123 
will NOT be built with Speex read support.])
    fi         
fi
