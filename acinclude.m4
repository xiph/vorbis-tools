# Configure paths for libao
# Jack Moffitt <jack@icecast.org> 10-21-2000
# Shamelessly stolen from Owen Taylor and Manish Singh

dnl AM_PATH_AO([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for libao, and define AO_CFLAGS and AO_LIBS
dnl
AC_DEFUN(AM_PATH_AO,
[dnl 
dnl Get the cflags and libraries from the ao-config script
dnl
AC_ARG_WITH(ao-prefix,[  --with-ao-prefix=PFX   Prefix where libao is installed (optional)], ao_prefix="$withval", ao_prefix="")
AC_ARG_ENABLE(aotest, [  --disable-aotest       Do not try to compile and run a test ao program],, enable_aotest=yes)

  if test x$ao_prefix != x ; then
     ao_args="$ao_args --prefix=$ao_prefix"
     if test x${AO_CONFIG+set} != xset ; then
        AO_CONFIG=$ao_prefix/bin/ao-config
     fi
  fi

  AC_PATH_PROG(AO_CONFIG, ao-config, no)
  min_ao_version=ifelse([$1], ,1.0.0,$1)
  AC_MSG_CHECKING(for ao - version >= $min_ao_version)
  no_ao=""
  if test "$AO_CONFIG" = "no" ; then
    no_ao=yes
  else
    AO_CFLAGS=`$AO_CONFIG $aoconf_args --cflags`
    AO_LIBS=`$AO_CONFIG $aoconf_args --libs`

    ao_major_version=`$AO_CONFIG $ao_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    ao_minor_version=`$AO_CONFIG $ao_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    ao_micro_version=`$AO_CONFIG $ao_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_aotest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $AO_CFLAGS"
      LIBS="$LIBS $AO_LIBS"
dnl
dnl Now check if the installed ao is sufficiently new. (Also sanity
dnl checks the results of ao-config to some extent
dnl
      rm -f conf.aotest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ao/ao.h>

int main ()
{
  system("touch conf.aotest");
  return 0;
}

],, no_ao=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_ao" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$AO_CONFIG" = "no" ; then
       echo "*** The ao-config script installed by ao could not be found"
       echo "*** If ao was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the AO_CONFIG environment variable to the"
       echo "*** full path to ao-config."
     else
       if test -f conf.aotest ; then
        :
       else
          echo "*** Could not run ao test program, checking why..."
          CFLAGS="$CFLAGS $AO_CFLAGS"
          LIBS="$LIBS $AO_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <ao/ao.h>
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding ao or finding the wrong"
          echo "*** version of ao. If it is not finding ao, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means ao was incorrectly installed"
          echo "*** or that you have moved ao since it was installed. In the latter case, you"
          echo "*** may want to edit the ao-config script: $AO_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     AO_CFLAGS=""
     AO_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(AO_CFLAGS)
  AC_SUBST(AO_LIBS)
  rm -f conf.aotest
])
