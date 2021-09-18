dnl Copyright (C) 2001-2021 Artifex Software, Inc.
dnl All Rights Reserved.
dnl
dnl This software is provided AS-IS with no warranty, either express or
dnl implied.
dnl
dnl This software is distributed under license and may not be copied,
dnl modified or distributed except as expressly authorized under the terms
dnl of the license contained in the file LICENSE in this distribution.
dnl
dnl Refer to licensing information at http://www.artifex.com or contact
dnl Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
dnl CA 94945, U.S.A., +1(415)492-9861, for further information.


dnl Process this file with autoconf to produce a configure script

dnl ------------------------------------------------
dnl Initialization and Versioning
dnl ------------------------------------------------

AC_INIT
AC_PREREQ([2.63])
AC_LANG(C)
AC_CONFIG_SRCDIR(psi/gs.c)

AC_CANONICAL_HOST

dnl Inherit compiler flags from the environment...
CFLAGS="${CFLAGS:=}"
CPPFLAGS="${CPPFLAGS:=}"
CXXFLAGS="${CXXFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"

if test x"$host" != x"$build" ; then
  CFLAGSAUX="${CFLAGSAUX:=}"
  CPPFLAGSAUX="${CPPFLAGSAUX:=}"
  LDFLAGSAUX="${LDFLAGSAUX:=}"
else
  CFLAGSAUX="${CFLAGSAUX:-$CFLAGS}"
  CPPFLAGSAUX="${CPPFLAGSAUX:-$CPPFLAGS}"
  LDFLAGSAUX="${LDFLAGSAUX:-$LDFLAGS}"
fi

THEMAKEFILE="${MAKEFILE:-Makefile}"
AUXFLAGS_MAK=auxflags.mak

ARCH_AUTOCONF_HEADER=arch-config/arch_autoconf.h
ARCH_AUTOCONF_HEADER_PROTO=arch/arch_autoconf.h.in

dnl --------------------------------------------------
dnl List of non-threadsafe devices
dnl defined at the top of the file for ease of access
dnl --------------------------------------------------

NTS_DEVS='opvp oprp'
NTS_DEVS="$NTS_DEVS uniprint"
NTS_DEVS="$NTS_DEVS x11 x11alpha"
NTS_DEVS="$NTS_DEVS pcl3 hpdjplus hpdjportable hpdj310 hpdj320 hpdj340 hpdj400 hpdj500 hpdj500c hpdj510 hpdj520 hpdj540 hpdj550c hpdj560c hpdj600 hpdj660c hpdj670c hpdj680c hpdj690c hpdj850c hpdj855c hpdj870c hpdj890c hpdj1120c"

# We use this to store a list of devices we have excluded
# and emit a warning on completion.
NTS_EXCLUDES=""


dnl --------------------------------------------------
dnl Local utilities
dnl --------------------------------------------------

dnl GS_SPLIT_LIBS( LIBS, LINKLINE )
dnl Split a unix-style link line into a list of
dnl bare library names. For example, the line
dnl '-L/usr/X11R6/lib -lX11 -lXt' splits into
dnl LIB='X11 Xt'
dnl
AC_DEFUN([GS_SPLIT_LIBS], [
# the makefile wants a list of just the library names
for gs_item in $2; do
  gs_stripped_item=`echo "$gs_item" | sed -e 's/^-l//'`
  if test "x$gs_stripped_item" != "x$gs_item"; then
    $1="$[$1] $gs_stripped_item"
  fi
done
])

dnl GS_SPLIT_LIBPATHS( LIBPATHS, LINKLINE )
dnl Split a unix-style link line into a list of
dnl bare search path entries. For example,
dnl '-L/usr/X11R6/lib -lX11 -L/opt/lib -lXt'
dnl splits to LIBPATHS='/usr/X11R6/lib /opt/lib'
dnl
AC_DEFUN([GS_SPLIT_LIBPATHS], [
for gs_item in $2; do
  gs_stripped_item=`echo "$gs_item" | sed -e 's/-L//'`
  if test "x$gs_stripped_item" != "x$gs_item"; then
    $1="$[$1] $gs_stripped_item"
  fi
done
])

dnl --------------------------------------------------
dnl Check for programs
dnl --------------------------------------------------

dnl AC_PROG_CC likes to add '-g -O2' to CFLAGS. however,
dnl we ignore those flags and construct our own.
save_cflags="$CFLAGS"
AC_PROG_CC
AC_PROG_CPP
CFLAGS="$save_cflags"

AUXFLAGS_MAK_LINE00="CCAUX=@CC@"
AUXFLAGS_MAK_LINE01="GCFLAGSAUX=@CPPFLAGS@ @GCFLAGS@ @CFLAGS@"
AUXFLAGS_MAK_LINE02="CAPOPTAUX="
AUXFLAGS_MAK_LINE03="CFLAGSAUX_STANDARD=@OPT_CFLAGS@"
AUXFLAGS_MAK_LINE04="CFLAGSAUX_DEBUG=@DBG_CFLAGS@"
AUXFLAGS_MAK_LINE05="CFLAGSAUX_PROFILE=-pg @OPT_CFLAGS@"
AUXFLAGS_MAK_LINE06="LDFLAGSAUX=@LDFLAGS@"
AUXFLAGS_MAK_LINE07="AUXEXTRALIBS=@LIBS@"

GCFLAGSAUXTMP="\$(GCFLAGS)"
CAPOPTAUXTMP="\$(CAPOPT)"
CFLAGSAUX_STANDARDTMP="\$(CFLAGS_STANDARD)"
CFLAGSAUX_DEBUGTMP="\$(CFLAGS_DEBUG)"
CFLAGSAUX_PROFILETMP="\$(CFLAGS_PROFILE)"
LDFLAGSAUXTMP="\$(LDFLAGS)"
AUXEXTRALIBSTMP="\$(EXTRALIBS)"

# purposefully do not include "help" output for this
AC_ARG_ENABLE([save_confaux])
AC_ARG_ENABLE([auxtools_only])

absolute_source_path=$(cd "$(dirname "$0")" && pwd)

if test x"$host" != x"$build" ; then
  # rerun configure for the AUX tools, disabling a load of tests not relevant for CCAUX
  olddir=`pwd`
  if test x"$enable_save_confaux" = x"yes"; then
    CONFAUXDIR=auxtmp
  else
    CONFAUXDIR=auxtmp$RANDOM$RANDOM
  fi
  mkdir $CONFAUXDIR
  cd $CONFAUXDIR
  echo -e $AUXFLAGS_MAK_CONTENTS > $AUXFLAGS_MAK.in
  echo $AUXFLAGS_MAK_LINE00  > $AUXFLAGS_MAK.in
  echo $AUXFLAGS_MAK_LINE01 >> $AUXFLAGS_MAK.in
  echo $AUXFLAGS_MAK_LINE02 >> $AUXFLAGS_MAK.in
  echo $AUXFLAGS_MAK_LINE03 >> $AUXFLAGS_MAK.in
  echo $AUXFLAGS_MAK_LINE04 >> $AUXFLAGS_MAK.in
  echo $AUXFLAGS_MAK_LINE05 >> $AUXFLAGS_MAK.in
  echo $AUXFLAGS_MAK_LINE06 >> $AUXFLAGS_MAK.in
  echo $AUXFLAGS_MAK_LINE07 >> $AUXFLAGS_MAK.in

  AC_MSG_NOTICE([Begin recursive call to configure script (for auxiliary tools)])
  "$absolute_source_path/configure" CC="$CCAUX" CFLAGS="$CFLAGSAUX" CPPFLAGS="$CPPFLAGSAUX" LDFLAGS="$LDFLAGSAUX" CCAUX= CFLAGSAUX= CFLAGSAUX= MAKEFILE=$AUXFLAGS_MAK --host=$build --build=$build --enable-auxtools_only --disable-hidden-visibility --with-local-zlib --without-libtiff --disable-contrib --disable-fontconfig --disable-dbus --disable-freetype --disable-fapi --disable-cups --disable-openjpeg --disable-gtk --with-libiconv=no --without-libidn --without-libpaper --without-pdftoraster --without-ijs --without-jbig2dec --without-x --with-drivers=""
  status=$?
  cp config.log "$olddir/configaux.log"
  if test $status -eq 0 ; then
    CCAUX=$(grep CCAUX $AUXFLAGS_MAK | sed "s/CCAUX=//g")
    GCFLAGSAUXTMP=$(grep GCFLAGSAUX $AUXFLAGS_MAK | sed "s/GCFLAGSAUX=//g")
    CAPOPTAUXTMP=$(grep CAPOPTAUX $AUXFLAGS_MAK | sed "s/CAPOPTAUX=//g")
    CFLAGSAUX_STANDARDTMP=$(grep CFLAGSAUX_STANDARD $AUXFLAGS_MAK | sed "s/CFLAGSAUX_STANDARD=//g")
    CFLAGSAUX_DEBUGTMP=$(grep CFLAGSAUX_DEBUG $AUXFLAGS_MAK | sed "s/CFLAGSAUX_DEBUG=//g")
    CFLAGS_PROFILETMP=$(grep CFLAGS_PROFILE $AUXFLAGS_MAK | sed "s/CFLAGS_PROFILE=//g")
    LDFLAGSAUXTMP=$(grep LDFLAGSAUX $AUXFLAGS_MAK | sed "s/LDFLAGSAUX=//g")
    AUXEXTRALIBSTMP=$(grep AUXEXTRALIBS$ $AUXFLAGS_MAK | sed "s/AUXEXTRALIBS$=//g")
  fi
  cd "$olddir"

  if test x"$enable_save_confaux" != x"yes"; then
    rm -rf $CONFAUXDIR
  fi

  if test $status -ne 0 ; then
    AC_MSG_ERROR([Recursive call to configure script failed], $status)
  else
    AC_MSG_NOTICE([Recursive call to configure script succeeded])
  fi
fi

GCFLAGSAUX=$GCFLAGSAUXTMP
CAPOPTAUX=$CAPOPTAUXTMP
CFLAGSAUX_STANDARD=$CFLAGSAUX_STANDARDTMP
CFLAGSAUX_DEBUG=$CFLAGSAUX_DEBUGTMP
CFLAGS_PROFILE=$CFLAGS_PROFILETMP
LDFLAGSAUX=$LDFLAGSAUXTMP
AUXEXTRALIBS=$AUXEXTRALIBSTMP

# we have to do this here in case it took
# AC_PROG_CC to set CC (as it usually does)
CCAUX="${CCAUX:-$CC}"

AC_PROG_SED
dnl See if it is GNU sed or else.
dnl - need more work to tell SED features.
SED_EXTENDED_REGEX_OPT=-nre
sed_variant=`sed --version 2>&1`
sed_variant=`echo $sed_variant|sed -e 's/ .*//'`
if test "$sed_variant" != GNU ; then
SED_EXTENDED_REGEX_OPT=-nEe
fi
AC_SUBST(SED_EXTENDED_REGEX_OPT)

AC_PROG_RANLIB
#AC_PROG_INSTALL

# If the caller has gives a specific pkg-config to use, then
# use it, and skip the tests.
if test x"$PKGCONFIG" != x"" ; then
    AC_MSG_NOTICE([Using $PKGCONFIG])
else
# this is an unpleasant hack
# but if we are cross compiling, and there isn't a matching
# pkconfig for the --host setting, then don't use the 'local'
# pkconfig at all
  AC_PATH_TOOL(PKGCONFIG, pkg-config)
  if test x"$host" != x"$build" ; then
    dnl pkg-config is used for several tests now...
    AC_PATH_PROG(BUILD_PKGCONFIG, pkg-config)
    if test x"$BUILD_PKGCONFIG" = x"$PKGCONFIG" ; then
      PKGCONFIG=
    fi
  fi
fi

AC_PATH_TOOL(STRIP_XE, strip)

# this is an unpleasant hack
# but if we are cross compiling, and there isn't a matching
# pkconfig for the --host setting, then don't use the 'local'
# pkconfig at all
if test x"$host" != x"$build" ; then
  AC_PATH_PROG(BUILD_STRIP_XE, strip)
  if test x"$BUILD_STRIP_XE" = x"$STRIP_XE" ; then
    STRIP_XE=
  fi
fi

dnl --------------------------------------------------
dnl Allow excluding the contributed drivers
dnl --------------------------------------------------

AC_ARG_ENABLE([contrib], AS_HELP_STRING([--disable-contrib], [Do not include contributed drivers]))

CONTRIBINCLUDE="include $srcdir/contrib/contrib.mak"
INSTALL_CONTRIB="install-contrib-extras"

if test x"$enable_contrib" = x; then
    case $host in
      *-mingw*|*-msys*)
        AC_MSG_WARN([disabling contrib devices])
        enable_contrib=no
      ;;
      *)
      ;;
    esac
fi

if test x"$enable_contrib" != x"no"; then
#     This is just an arbitrary file in contrib to check
      if test -f $srcdir/contrib/gdevbjc_.c; then
        enable_contrib=yes
      else
        enable_contrib=no
      fi
fi

if test x$enable_contrib = xno; then
    CONTRIBINCLUDE=""
    INSTALL_CONTRIB=""
    CFLAGS="$CFLAGS -DNOCONTRIB"
fi
AC_SUBST(CONTRIBINCLUDE)
AC_SUBST(INSTALL_CONTRIB)

dnl --------------------------------------------------
dnl Set build flags based on environment
dnl --------------------------------------------------

CC_OPT_FLAGS_TO_TRY="-O -DNDEBUG"
CC_VISIBILITY_FLAGS_TO_TRY=""
SET_DT_SONAME="-soname="
CFLAGS_LARGEFILE=""

case $host in
        *-linux*|*-gnu)
        if test $GCC = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2 -DNDEBUG"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
            CC_VISIBILITY_FLAGS_TO_TRY="-fvisibility=hidden"
        fi
        ;;
        *bsd*)
        if test $GCC = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2 -DNDEBUG"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
            CC_VISIBILITY_FLAGS_TO_TRY="-fvisibility=hidden"
        fi
        ;;
        *-darwin*)
        if test $GCC = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2 -DNDEBUG"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
            CC_VISIBILITY_FLAGS_TO_TRY="-fvisibility=hidden"
        fi
        SET_DT_SONAME=""
        ;;
        *-mingw*|*-msys*)
        if test $GCC = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2 -DNDEBUG"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
            CC_VISIBILITY_FLAGS_TO_TRY="-fvisibility=hidden"
        fi
        SET_DT_SONAME=""
        ;;
        *-sun*|*-solaris*)
        CC_OPT_FLAGS_TO_TRY="-O2 -DNDEBUG"
        CFLAGS_LARGEFILE="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
        # the trailing space is required!
        if test $GCC = no; then
            SET_DT_SONAME="-h "
        fi
        if test $GCC = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2 -DNDEBUG"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
            CC_VISIBILITY_FLAGS_TO_TRY="-fvisibility=hidden"
        else
            CC_DBG_FLAGS_TO_TRY="-g -O0"
        fi
        ;;
        *-aix*)
        if test $GCC = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2 -DNDEBUG"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
            SET_DT_SONAME="so"
            CC_VISIBILITY_FLAGS_TO_TRY="-fvisibility=hidden"
        fi
        ;;
esac

AC_SUBST(SET_DT_SONAME)


if test $GCC = yes; then
    cflags_to_try="-Wall -Wstrict-prototypes -Wundef \
-Wmissing-declarations -Wmissing-prototypes -Wwrite-strings \
-fno-strict-aliasing -Werror=declaration-after-statement \
-fno-builtin -fno-common -Werror=return-type $CFLAGS_LARGEFILE"
    optflags_to_try="$CC_OPT_FLAGS_TO_TRY"
    dbgflags_to_try="$CC_DBG_FLAGS_TO_TRY"
else
    cflags_to_try="$CFLAGS_LARGEFILE"
    optflags_to_try="$CC_OPT_FLAGS_TO_TRY"
    dbgflags_to_try="$CC_DBG_FLAGS_TO_TRY"
fi

AC_MSG_CHECKING([if compiler supports restrict])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if defined(__STDC_VERSION__) && __STDC_VERSION__ == 199901L
void test (char * restrict p){}
#else
void test (char * __restrict p){}
#endif
]], [[
#if defined(__STDC_VERSION__) && __STDC_VERSION__ == 199901L
                 char *restrict t;
#else
                 char * __restrict t;
#endif
                 test(t);
                 return 1;
               ]])],[
               CFLAGS="$CFLAGS -DHAVE_RESTRICT=1"
               if test x"$host" = x"$build" ; then
                   CFLAGSAUX="$CFLAGSAUX -DHAVE_RESTRICT=1"
               fi
               AC_MSG_RESULT([yes])],[
               CFLAGS="$CFLAGS -DHAVE_RESTRICT=0"
               if test x"$host" = x"$build" ; then
                   CFLAGSAUX="$CFLAGSAUX -DHAVE_RESTRICT=0"
               fi
               AC_MSG_RESULT([no])])



AC_ARG_WITH([arch_h], AS_HELP_STRING([--with-arch_h=<arch.h to use>],
            [Use a custom arch.h (must be an absolute path)]),[], [with_arch_h=])

ARCH_CONF_HEADER=

if test x"$with_arch_h" = x""; then
    case $host in
          x86_64*-mingw*|x86_64*-msys*)
            ARCH_CONF_HEADER="\$(GLSRCDIR)/../arch/windows-x64-msvc.h"
          ;;
          *-mingw*|*-msys*)
            ARCH_CONF_HEADER="\$(GLSRCDIR)/../arch/windows-x86-msvc.h"
          ;;
          *-darwin*)
            ARCH_CONF_HEADER="\$(GLSRCDIR)/../arch/osx-x86-x86_64-ppc-gcc.h"
          ;;
          *)
            ARCH_CONF_HEADER=
            if test x"$host" != x"$build"; then
              ARCH_CONF_HEADER="\$(GLSRCDIR)/../$ARCH_AUTOCONF_HEADER"
            else
              ARCH_CONF_HEADER=
            fi
          ;;
    esac
else
  ARCH_CONF_HEADER=$with_arch_h
fi

AC_SUBST(ARCH_CONF_HEADER)

# debug configurarion is available by default with "make debug"
#AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug],
#    [turn on debugging]))
#if test x$enable_debug = xyes; then
#    optflags_to_try="-g"
#    CFLAGS="-DDEBUG $CFLAGS"
#fi

# NOTE: To correctly disable GCC's strict aliasing with '-fno-strict-aliasing'
#       option, the 'cflags_to_try' have to be checked after 'optflags_to_try'.
AC_MSG_CHECKING([supported compiler flags])
old_cflags=$CFLAGS

for flag in $optflags_to_try; do
    CFLAGS="$CFLAGS $flag"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[
        echo "   $flag"
        OPT_CFLAGS="$OPT_CFLAGS $flag"
    ],[])
    CFLAGS=$old_cflags
done
for flag in $cflags_to_try; do
        CFLAGS="$CFLAGS $flag"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[
                echo "   $flag"
                GCFLAGS="$GCFLAGS $flag"
        ],[])
        CFLAGS=$old_cflags
done
old_cflags=$CFLAGS

for flag in $dbgflags_to_try; do
    CFLAGS="$CFLAGS $flag"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[
        echo "   $flag"
        DBG_CFLAGS="$DBG_CFLAGS $flag"
    ],[])
    CFLAGS=$old_cflags
done

CFLAGS_VISIBILITY=""

for flag in $CC_VISIBILITY_FLAGS_TO_TRY; do
    CFLAGS="$CFLAGS $flag"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[
        echo "   $flag"
        CFLAGS_VISIBILITY="$CFLAGS_VISIBILITY $flag"
    ],[])
    CFLAGS=$old_cflags
done

AC_MSG_RESULT([ ...done.])

dnl ----------------------------
dnl check for sanitize support
dnl ----------------------------
AC_MSG_CHECKING([compiler/linker address santizer support])

AC_ARG_WITH([sanitizer], AS_HELP_STRING([--with-sanitizer=@<:@address/memory@:>@],
                                   [Sanitizer for 'sanitize' target (defaults to 'address')]),
            [SANITIZER=$with_sanitizer], [SANITIZER=address])

CFLAGS_SANITIZE=""
CFLAGS_SANITIZE_TRY="-fsanitize=$SANITIZER -fno-omit-frame-pointer"
CFLAGS_SAVED="$CFLAGS"
CFLAGS="$CFLAGS_SANITIZE_TRY"

AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <stdio.h>], [
  return(0);
  ])],
  [CFLAGS_SANITIZE="$CFLAGS"],
  [
    if test x"$with_sanitizer" != x; then
      AC_MSG_ERROR([--with-sanitizer=$with_sanitizer not supported by compiler])
    else
      CFLAGS_SANITIZE="'****************ADDRESS_SANITIZER_NOT_SUPPORTED*********************'"
    fi
  ]
  )

CFLAGS="$CFLAGS_SAVED"

AC_MSG_RESULT([ ...done.])


dnl ----------------------------
dnl check for sanitize build warnings support
dnl ----------------------------
AC_MSG_CHECKING([compiler/linker address santizer build warnings support])

CFLAGS_SANITIZE_TRY="$CFLAGS_SANITIZE -W -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough -Wno-missing-field-initializers -Wno-shift-negative-value -Wno-old-style-declaration -Wno-unused-but-set-parameter"
CFLAGS_SAVED="$CFLAGS"
CFLAGS="$CFLAGS_SANITIZE_TRY"

AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <stdio.h>], [
  return(0);
  ])],
  [CFLAGS_SANITIZE="$CFLAGS"], [echo 'extra warnings not supported'])

CFLAGS="$CFLAGS_SAVED"
AC_MSG_RESULT([ ...done.])


dnl ----------------------------
dnl check for big/little endian
dnl ----------------------------

AC_C_BIGENDIAN([BIGENDIAN=1], [BIGENDIAN=0],
  [AC_MSG_ERROR([Endian status cannot be determined])],
  [AC_MSG_ERROR([Building of universal binaries is not implemented.])])

dnl --------------------------------------------------
dnl check for sse2 intrinsics
dnl --------------------------------------------------

AC_MSG_CHECKING([sse2 support])
save_cflags=$CFLAGS
CFLAGS="$CFLAGS $OPT_CFLAGS"

HAVE_SSE2=""
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <emmintrin.h>], [
  __m128i input1;
  unsigned char buf1[[128]];
  input1 = _mm_loadu_si128((const __m128i *)buf1);
  return(0);
  ])],
  [HAVE_SSE2="-DHAVE_SSE2"], [HAVE_SSE2=""])

AC_ARG_ENABLE([sse2], AS_HELP_STRING([--disable-sse2],
       [Do not use sse2 instrinsics]), [
             if test "x$enable_sse2" = xno; then
                HAVE_SSE2=""
             fi])

if test "x$HAVE_SSE2" != x; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

AC_SUBST(HAVE_SSE2)
CFLAGS=$save_cflags


dnl --------------------------------------------------
dnl Enable thread safe build
dnl --------------------------------------------------
AC_ARG_ENABLE([threadsafe], AS_HELP_STRING([--enable-threadsafe],
    [enable a thread safe build - NOTE: this will exclude non-threadsafe devices]))
if test x$enable_threadsafe = xyes ; then
    CFLAGS="-DGS_THREADSAFE"
fi

dnl --------------------------------------------------
dnl Check for headers
dnl --------------------------------------------------

AC_HEADER_DIRENT
AC_CHECK_HEADERS([errno.h fcntl.h limits.h malloc.h memory.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/time.h sys/times.h syslog.h unistd.h dirent.h ndir.h sys/dir.h sys/ndir.h inttypes.h])

dnl --------------------------------------------------
dnl Sun, BSD possibly other makes don't have quite the
dnl feature set of GNU make. We still prefer GNU make,
dnl but......
dnl --------------------------------------------------

OBJDIR_BSDMAKE_WORKAROUND=obj
SUB_MAKE_OPTION=
ORDER_ONLY=""

AC_MSG_CHECKING(if make is GNU make)
if make --version 2> /dev/null | grep GNU 2>&1 > /dev/null ; then
  AC_MSG_RESULT(yes)
  SUB_MAKE_OPTION="-f \$(MAKEFILE)"
  ORDER_ONLY="|"
else
  AC_MSG_RESULT(no)
  # BSD Make treats obj special and cd into it first
  OBJDIR_BSDMAKEWORKAOROUND="notobj"
fi

AC_SUBST(OBJDIR_BSDMAKE_WORKAROUND)
AC_SUBST(ORDER_ONLY)
AC_SUBST(SUB_MAKE_OPTION)

AC_CHECK_HEADER([sys/window.h])

dnl --------------------------------------------------
dnl Check for typedefs, structures, etc
dnl --------------------------------------------------

AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLOCKS
AC_STRUCT_TM

dnl see if we're on a system that puts the *int*_t types
dnl from stdint.h in sys/types.h
if test "x$ac_cv_header_stdint_h" != xyes; then
    AC_CHECK_TYPES([int8_t, int16_t, int32_t, uint8_t, uint16_t, uint32_t],,,[#include <sys/types.h>])
    if test "$ac_cv_type_uint8_t" = yes; then
        AC_DEFINE([SYS_TYPES_HAS_STDINT_TYPES])
        GCFLAGS="$GCFLAGS -DSYS_TYPES_HAS_STDINT_TYPES"
    fi
fi

dnl we aren't interested in all of DEFS, so manually insert
dnl the flags we care about
if test "$ac_cv_c_const" != yes; then
        GCFLAGS="$GCFLAGS -Dconst="
fi
if test "x$ac_cv_header_stdint_h" = "xyes"; then
        GCFLAGS="$GCFLAGS -DHAVE_STDINT_H=1"
fi

if test "x$ac_cv_header_dirent_h" = "xyes"; then
        GCFLAGS="$GCFLAGS -DHAVE_DIRENT_H=1"
fi

if test "x$ac_cv_header_ndir_h" = xyes; then
        GCFLAGS="$GCFLAGS -DHAVE_NDIR_H=1"
fi

if test "x$ac_cv_header_sys_dir_h" = "xyes"; then
        GCFLAGS="$GCFLAGS -DHAVE_SYS_DIR_H=1"
fi

if test "x$ac_cv_header_sys_ndir_h" = "xyes"; then
        GCFLAGS="$GCFLAGS -DHAVE_SYS_NDIR_H=1"
fi

if test "x$ac_cv_header_sys_time_h" = "xyes"; then
        GCFLAGS="$GCFLAGS -DHAVE_SYS_TIME_H=1"
fi

if test "x$ac_cv_header_sys_times_h" = "xyes"; then
        GCFLAGS="$GCFLAGS -DHAVE_SYS_TIMES_H=1"
fi

if test "x$ac_cv_header_inttypes_h" = "xyes"; then
        GCFLAGS="$GCFLAGS -DHAVE_INTTYPES_H=1"
fi

AC_CHECK_LIB([dl], [dlopen],
             [AC_CHECK_HEADER([dlfcn.h], [GCFLAGS="$GCFLAGS -DHAVE_LIBDL=1";LIBS="-ldl $LIBS"])]
              )

large_color_index=1

AC_ARG_WITH([large_color_index],, large_color_index="$with_large_color_index")

if test x"$large_color_index" != "x0"; then
  dnl try to find a 64 bit type for devicen color index
  color_ind_type="none"
   AC_CHECK_SIZEOF(unsigned long long)
   if test $ac_cv_sizeof_unsigned_long_long = 8; then
          color_ind_type="unsigned long long"
          color_ind_size=$ac_cv_sizeof_unsigned_long_long
   else
    AC_CHECK_SIZEOF(unsigned long int)
    if test $ac_cv_sizeof_unsigned_long_int = 8; then
          color_ind_type="unsigned long int"
          color_ind_size=$ac_cv_sizeof_unsigned_long_int
    else
     AC_CHECK_SIZEOF(unsigned __int64)
     if test $ac_cv_sizeof_unsigned___int64 = 8; then
          color_ind_type="unsigned __int64"
          color_ind_size=$ac_cv_sizeof_unsigned___int64
     else
      AC_CHECK_SIZEOF(u_int64_t)
      if test $ac_cv_sizeof_u_int64_t = 8; then
          color_ind_type="u_int64_t"
          color_ind_size=$ac_cv_sizeof_u_int64_t
      fi
     fi
    fi
   fi
else
  dnl try to find a 32 bit type for devicen color index
  color_ind_type="none"
   AC_CHECK_SIZEOF(unsigned long int)
   if test $ac_cv_sizeof_unsigned_long_int = 4; then
          color_ind_type="unsigned long int"
          color_ind_size=$ac_cv_sizeof_unsigned_long_int
   else
    AC_CHECK_SIZEOF(unsigned int)
    if test $ac_cv_sizeof_unsigned_int = 4; then
          color_ind_type="unsigned int"
          color_ind_size=$ac_cv_sizeof_unsigned_int
    else
     AC_CHECK_SIZEOF(unsigned __int32)
     if test $ac_cv_sizeof_unsigned___int32 = 4; then
          color_ind_type="unsigned __int32"
          color_in_size=$ac_cv_sizeof_unsigned___int32
     else
      AC_CHECK_SIZEOF(u_int32_t)
      if test $ac_cv_sizeof_u_int32_t = 4; then
          color_ind_type="u_int32_t"
          color_ind_size=$ac_cv_sizeof_u_int32_t
      fi
     fi
    fi
   fi
fi

dnl if a suitable type wasn't found above, we fall back to
dnl a default in genarch.
if test "$color_ind_type" != "none"; then
        GCFLAGS="$GCFLAGS -DGX_COLOR_INDEX_TYPE=\"$color_ind_type\""
        ARCH_SIZEOF_GX_COLOR_INDEX=$color_ind_size
fi

AC_SUBST(ARCH_SIZEOF_GX_COLOR_INDEX)



dnl --------------------------------------------------
dnl Set options that we want to pass into all other
dnl configure scripts we might call
dnl --------------------------------------------------

SUBCONFIG_OPTS="--build=$build --host=$host"

#SUBCONFIG_OPTS=""
#if test x"$build_alias" != x""; then
#  SUBCONFIG_OPTS="$SUBCONFIG_OPTS --build=$build_alias"
#fi
#if test x"$host_alias" != x""; then
#  SUBCONFIG_OPTS="$SUBCONFIG_OPTS --host=$host_alias"
#fi

dnl --------------------------------------------------
dnl Check for libraries
dnl --------------------------------------------------

AC_CHECK_LIB(m, cos)

AC_CHECK_FUNCS([pread pwrite], [HAVE_PREAD_PWRITE="-DHAVE_PREAD_PWRITE=1"], [HAVE_PREAD_PWRITE=])

if test "x$HAVE_PREAD_PWRITE" != "x"; then
  save_cflags=$CFLAGS
  CFLAGS="$CFLAGS -D__USE_UNIX98=1 -D_XOPEN_SOURCE=500"
  AC_CHECK_DECLS([pwrite,pread], [], [HAVE_PREAD_PWRITE=])
  CFLAGS=$save_cflags
  if test "x$HAVE_PREAD_PWRITE" != "x"; then
    GCFLAGS="$GCFLAGS -D__USE_UNIX98=1"
  fi
fi

AC_SUBST(HAVE_PREAD_PWRITE)

AC_CHECK_DECL([popen], [HAVE_POPEN_PROTO="-DHAVE_POPEN_PROTO=1"], [AVE_POPEN_PROTO=])
AC_SUBST(HAVE_POPEN_PROTO)

SYNC="nosync"
PTHREAD_LIBS=""
RECURSIVE_MUTEXATTR=""

AC_ARG_ENABLE([threading], AS_HELP_STRING([--disable-threading],
    [disable support for multithreaded rendering]))

# if you haven't got pread/pwrite, we can't use multithreading
if test "x$HAVE_PREAD_PWRITE" != "x"; then
  if test "$enable_threading" != "no"; then
    mutex_result="no"
    AC_CHECK_LIB(pthread, pthread_create, [
      SYNC=posync;
      PTHREAD_LIBS="-lpthread"
    ])
    AC_MSG_CHECKING([for recursive mutexes])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]], [[
            static int k = PTHREAD_MUTEX_RECURSIVE;
            #ifndef pthread_mutexattr_settype
            #ifdef __cplusplus
              (void) pthread_mutexattr_settype;
            #else
              (void) pthread_mutexattr_settype;
            #endif
            #endif
            ;
            return 0;
          ]])],[RECURSIVE_MUTEXATTR="-DGS_RECURSIVE_MUTEXATTR=PTHREAD_MUTEX_RECURSIVE"
          mutex_result="PTHREAD_MUTEX_RECURSIVE"],[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]], [[
              static int k = PTHREAD_MUTEX_RECURSIVE_NP;
              #ifndef pthread_mutexattr_settype
              #ifdef __cplusplus
                (void) pthread_mutexattr_settype;
              #else
                (void) pthread_mutexattr_settype;
              #endif
              #endif
              ;
              return 0;
            ]])],[RECURSIVE_MUTEXATTR="-DGS_RECURSIVE_MUTEXATTR=PTHREAD_MUTEX_RECURSIVE_NP"
             mutex_result="PTHREAD_MUTEX_RECURSIVE_NP"],[])])
    AC_MSG_RESULT($mutex_result)
  fi
fi

AC_SUBST(SYNC)
AC_SUBST(PTHREAD_LIBS)
AC_SUBST(RECURSIVE_MUTEXATTR)

dnl Tesseract/Leptonica detection
TESSERACTDIR=tesseract
LEPTONICADIR=leptonica
OCR_VERSION=0
LEPTONICAINCLUDE=
TESSERACTINCLUDE=
TESSERACT_LIBS=
OCR_DEVS=

OCR_DEVS_WARNING_LINE1=
OCR_DEVS_WARNING_LINE2=

AC_ARG_WITH([tesseract], AS_HELP_STRING([--without-tesseract],
    [do not try to use the Tesseract library for OCR]))

if test x$with_tesseract != xno; then
  AC_MSG_CHECKING([for local Tesseract library source])
  if test -d $srcdir/tesseract && test -d $srcdir/leptonica; then
    AC_MSG_RESULT([yes])
    AC_LANG_PUSH(C++)
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[HAVE_CXX_COMPILER=yes; AC_SUBST(CXX)],[HAVE_CXX_COMPILER=no])
    if test x$HAVE_CXX_COMPILER != xyes; then
      AC_MSG_WARN([Disabling tesseract as no working C++ compiler])
    elif test x$SYNC = xnosync ; then
      AC_MSG_ERROR([Threading disabled or not available. Tesseract OCR relies on threading. Rerun configure with "--without-tesseract" to exclude OCR from the build])
    else

      save_cxxflags="$CXXFLAGS"
      cxxflags_to_try="-std=c++17 -stdlib=libstdc++"
      CXXFLAGS_TO_USE=""

      AC_MSG_CHECKING([supported C++ compiler flags])
      for flag in $cxxflags_to_try ; do
        CXXFLAGS="$CXXFLAGS $flag"

        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <cstdlib>]], [[return 0;]])],[
                echo "   $flag"; CXXFLAGS_TO_USE="$CXXFLAGS_TO_USE $flag"
        ],[])

        CXXFLAGS="$old_cflags"
      done

      if test x"$GXX" = x"no" || echo $CXXFLAGS_TO_USE | grep "std=c++17" ; then
        if test x"$GXX" = x"no" ; then
          OCR_DEVS_WARNING_LINE1="C++ compiler does not appear to be gcc or compatible, attempting to continue anyway"
          OCR_DEVS_WARNING_LINE2="NOTE: that tesseract requires a C++17 compatible C++ compiler"
        fi

        CXXFLAGS="$save_cxxflags $CXXFLAGS_TO_USE"

        dnl --------------------------------------------------
        dnl check for sse4.1, avx, avx2 or fma
        dnl --------------------------------------------------
        AC_MSG_CHECKING([sse4.1 support])
        save_cxxflags=$CXXFLAGS
        TESS_CXXFLAGS="$CXXFLAGS"
        CXXFLAGS="$CXXFLAGS -msse4.1"

        TESS_SSE4_1=""
        AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([#include <emmintrin.h>
                            #include <smmintrin.h>],
                           [__m128i input1;
                            unsigned char buf1[[128]];
                            input1 = _mm_loadu_si128((const __m128i *)buf1);
                            input1 = _mm_hadd_epi32(input1, input1);
                            return(0);
                           ])],
          [TESS_SSE4_1="-msse4.1"],
          [TESS_SSE4_1=""])

        if test "x$TESS_SSE4_1" != x; then
          AC_MSG_RESULT(yes)
          TESS_CXXFLAGS="$TESS_CXXFLAGS -DHAVE_SSE4_1"
        else
          AC_MSG_RESULT(no)
        fi

        AC_MSG_CHECKING([avx support])
        CXXFLAGS="$save_cxxflags -mavx"

        TESS_AVX=""
        AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([#include <immintrin.h>],
                           [__m256d input1 = _mm256_setzero_pd();
                            input1 = _mm256_hadd_pd(input1, input1);
                            return(0);])],
          [TESS_AVX="-mavx"],
          [TESS_AVX=""])

        if test "x$TESS_AVX" != x; then
          AC_MSG_RESULT(yes)
          TESS_CXXFLAGS="$TESS_CXXFLAGS -DHAVE_AVX"
        else
          AC_MSG_RESULT(no)
        fi

        AC_MSG_CHECKING([avx2 support])
        CXXFLAGS="$save_cxxflags -mavx2"

        TESS_AVX2=""
        AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([#include <immintrin.h>],
                           [__m256i input1;
                            unsigned char buf1[[256]];
                            input1 = _mm256_loadu_si256((const __m256i *)buf1);
                            input1 = _mm256_adds_epu8(input1, input1);
                            return(0);])],
          [TESS_AVX2="-mavx2"],
          [TESS_AVX2=""])

        if test "x$TESS_AVX2" != x; then
          AC_MSG_RESULT(yes)
          TESS_CXXFLAGS="$TESS_CXXFLAGS -DHAVE_AVX2"
        else
          AC_MSG_RESULT(no)
        fi

        AC_MSG_CHECKING([fma support])
        CXXFLAGS="$save_cxxflags -mfma"

        TESS_FMA=""
        AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([#include <immintrin.h>],
                           [__m256d input1 = _mm256_setzero_pd();
                            input1 = _mm256_fmadd_pd(input1, input1, input1);
                            return(0);])],
          [TESS_FMA="-mfma"],
          [TESS_FMA=""])

        if test "x$TESS_FMA" != x; then
          AC_MSG_RESULT(yes)
          TESS_CXXFLAGS="$TESS_CXXFLAGS -DHAVE_FMA"
        else
          AC_MSG_RESULT(no)
        fi

        AC_MSG_CHECKING([neon support])
        CXXFLAGS="$save_cxxflags -mfpu=neon -mcpu=cortex-a53"

        TESS_NEON=""
        AC_LINK_IFELSE(
         [AC_LANG_PROGRAM([#include "arm_neon.h"],
                          [int32x4_t round = vdupq_n_s32(10);
                           return(0);])],
         [TESS_NEON="-mfpu=neon -mcpu=cortex-a53 -D__ARM_NEON__"],
         [TESS_NEON=""])

        if test "x$TESS_NEON" != x; then
          AC_MSG_RESULT(yes)
          TESS_CXXFLAGS="$TESS_CXXFLAGS -DHAVE_NEON"
        else
          AC_MSG_RESULT(no)
        fi

        CXXFLAGS="$save_cxxflags"

        OCR_VERSION=1
        OCR_DEVS="\$(DD)ocr.dev \$(DD)hocr.dev \$(DD)pdfocr8.dev \$(DD)pdfocr24.dev \$(DD)pdfocr32.dev"
        LEPTONICAINCLUDE="include base/leptonica.mak"
        TESSERACTINCLUDE="include base/tesseract.mak"
        TESSERACT_LIBS="-lstdc++"
      else
        OCR_DEVS_WARNING_LINE1="OCR devices disabled due to incompatible compiler: tesseract requires a C++17 compatible compiler"
      fi
    fi
    AC_LANG_POP()
  else
    AC_MSG_RESULT([no])
  fi
fi

AC_SUBST(TESSERACT_LIBS)
AC_SUBST(LEPTONICAINCLUDE)
AC_SUBST(TESSERACTINCLUDE)
AC_SUBST(LEPTONICADIR)
AC_SUBST(TESSERACTDIR)
AC_SUBST(OCR_DEVS)
AC_SUBST(OCR_VERSION)
AC_SUBST(TESS_AVX)
AC_SUBST(TESS_AVX2)
AC_SUBST(TESS_FMA)
AC_SUBST(TESS_SSE4_1)
AC_SUBST(TESS_NEON)
AC_SUBST(TESS_CXXFLAGS)

dnl Tests for iconv (Needed for OpenPrinting Vector, "opvp" output device)
AC_ARG_WITH(libiconv,
            [AS_HELP_STRING([--with-libiconv=@<:@no/gnu/native@:>@],
                            [use the libiconv library])],,
            [with_libiconv=maybe])
found_iconv=no
case $with_libiconv in
  maybe)
    # Check in the C library first
    AC_CHECK_FUNC(iconv_open, [with_libiconv=no; found_iconv=yes])
    # Check if we have GNU libiconv
    if test $found_iconv = "no"; then
      AC_CHECK_LIB(iconv, libiconv_open, [with_libiconv=gnu; found_iconv=yes])
    fi
    # Check if we have a iconv in -liconv, possibly from vendor
    if test $found_iconv = "no"; then
      AC_CHECK_LIB(iconv, iconv_open, [with_libiconv=native; found_iconv=yes])
    fi
    ;;
  no)
    found_iconv=no
    ;;
  gnu|yes)
    AC_CHECK_LIB(iconv, libiconv_open, [with_libiconv=gnu; found_iconv=yes])
    ;;
  native)
    AC_CHECK_LIB(iconv, iconv_open, [with_libiconv=native; found_iconv=yes])
    ;;
esac
if test x$found_iconv != xno -a x$with_libiconv != xno ; then
  LIBS="$LIBS -liconv"
fi

case $with_libiconv in
  gnu)
    AC_DEFINE(USE_LIBICONV_GNU, 1, [Using GNU libiconv])
    CFLAGS="$CFLAGS -DUSE_LIBICONV_GNU"
    ;;
  native)
    AC_DEFINE(USE_LIBICONV_NATIVE, 1, [Using a native implementation of iconv in a separate library])
    ;;
esac

dnl Check for libidn (needed for Unicode password support)
AC_ARG_WITH(libidn,
            [AS_HELP_STRING([--without-libidn],
                               [Do not use libidn to support Unicode passwords])],,
            [with_libidn=maybe])

if test x$with_libidn != xno; then
  if test x"$PKGCONFIG" != x""; then
    AC_MSG_CHECKING(for libidn with pkg-config)
    if $PKGCONFIG --exists libidn; then
            AC_MSG_RESULT(yes)
            LIBS="$LIBS `$PKGCONFIG --libs libidn`"
            CFLAGS="$CFLAGS `$PKGCONFIG --cflags libidn`"
            HAVE_LIBIDN=-DHAVE_LIBIDN
    else
            AC_MSG_RESULT(no)
    fi
  fi
  if test -z "$HAVE_LIBIDN"; then
    AC_CHECK_LIB(idn, stringprep, [
        with_libidn=no
        AC_CHECK_HEADER([stringprep.h],
        [
          with_libidn=yes
          HAVE_LIBIDN="-DHAVE_LIBIDN"
          LIBS="$LIBS -lidn"
        ])
        ], [
        if test x$with_libidn != xmaybe; then
          AC_MSG_ERROR([libidn not found])
        fi
        with_libidn=no
    ])
  fi
fi

UTF8DEVS=''
if test x$with_libidn != xno; then
  if test x$found_iconv != xno; then
    UTF8DEVS='$(PSD)utf8.dev'
  fi
fi
AC_SUBST(HAVE_LIBIDN)
AC_SUBST(UTF8DEVS)

dnl Tests for libpaper (to determine system default paper size)
AC_ARG_WITH([libpaper],
            AS_HELP_STRING([--without-libpaper],
                           [disable libpaper support]))
if test x$with_libpaper != xno; then
    AC_CHECK_LIB(paper, systempapername, [with_libpaper=yes],
    [
        AC_MSG_WARN([disabling support for libpaper])
        with_libpaper=no
    ])
fi
if test x$with_libpaper != xno; then
    AC_CHECK_HEADER([paper.h], [with_libpaper=yes],
    [
        AC_MSG_WARN([disabling support for libpaper])
        with_libpaper=no
    ])
fi

if test x$with_libpaper != xno; then
    LIBS="$LIBS -lpaper"
    AC_DEFINE(USE_LIBPAPER, 1, [Using libpaper])
    CFLAGS="$CFLAGS -DUSE_LIBPAPER"
fi

dnl Fontconfig support
HAVE_FONTCONFIG=""
FONTCONFIG_CFLAGS=""
FONTCONFIG_LIBS=""
AC_ARG_ENABLE([fontconfig], AS_HELP_STRING([--disable-fontconfig],
    [Do not use fontconfig to list system fonts]))
if test "$enable_fontconfig" != "no"; then
        # We MUST NOT use PKG_CHECK_MODULES since it is a) not a standard
        # autoconf macro and b) requires pkg-config on the system, which is
        # NOT standard on ANY OS, including Linux!
        if test "x$PKGCONFIG" != x; then
                AC_MSG_CHECKING(for fontconfig with pkg-config)
                if $PKGCONFIG --exists fontconfig; then
                        AC_MSG_RESULT(yes)
                        FONTCONFIG_CFLAGS="$CFLAGS `$PKGCONFIG --cflags fontconfig`"
                        FONTCONFIG_LIBS="`$PKGCONFIG --libs fontconfig`"
                        HAVE_FONTCONFIG=-DHAVE_FONTCONFIG
                else
                        AC_MSG_RESULT(no)
                fi
        fi
        if test -z "$HAVE_FONTCONFIG"; then
                AC_CHECK_LIB([fontconfig], [FcInitLoadConfigAndFonts], [
                  AC_CHECK_HEADER([fontconfig/fontconfig.h], [
                    FONTCONFIG_LIBS="-lfontconfig"
                    HAVE_FONTCONFIG="-DHAVE_FONTCONFIG"
                  ])
                ])
        fi
fi
AC_SUBST(HAVE_FONTCONFIG)
AC_SUBST(FONTCONFIG_CFLAGS)
AC_SUBST(FONTCONFIG_LIBS)

dnl DBus support
HAVE_DBUS=""
DBUS_CFLAGS=""
DBUS_LIBS=""
AC_ARG_ENABLE([dbus], AS_HELP_STRING([--disable-dbus],
    [Do not use dbus to communicate with external services]))
if test "$enable_dbus" != "no"; then
        if test "x$PKGCONFIG" != x; then
                AC_MSG_CHECKING(for dbus with pkg-config)
                if $PKGCONFIG --exists dbus-1; then
                        AC_MSG_RESULT(yes)
                        DBUS_CFLAGS="$CFLAGS `$PKGCONFIG --cflags dbus-1`"
                        DBUS_LIBS="`$PKGCONFIG --libs dbus-1`"
                        HAVE_DBUS=-DHAVE_DBUS
                else
                        AC_MSG_RESULT(no)
                fi
        fi
        if test -z "$HAVE_DBUS"; then
                AC_CHECK_LIB([dbus], [dbus_message_iter_get_basic], [
                  AC_CHECK_HEADER([dbus-1.0/dbus/dbus.h], [
                    DBUS_LIBS="-ldbus-1 -lpthread -lrt"
                    HAVE_DBUS="-DHAVE_DBUS"
                  ])
                ])
        fi
fi
AC_SUBST(HAVE_DBUS)
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)


AC_ARG_ENABLE([freetype], AS_HELP_STRING([--disable-freetype],
        [Disable freetype for font rasterization]))

FT_BRIDGE=0
SHARE_FT=0
FTSRCDIR=src
FT_CFLAGS=
FT_LIBS=

INSERT_UFST_BRIDGE_EQUAL_ONE=
UFST_ROOT=
UFST_CFLAGS=
UFST_LIB_EXT=
FAPIUFST_MAK="\$(GLSRCDIR)\$(D)stub.mak"

if test x"$enable_fapi" != xno; then

  dnl UFST detection
  AC_ARG_WITH([ufst], AS_HELP_STRING([--with-ufst=UFST_ROOT_DIR],
                                   [Use UFST]),
            [], [with_ufst=no])

  if test x"$with_ufst" != xno; then
    if test -d $with_ufst; then
      INSERT_UFST_BRIDGE_EQUAL_ONE="UFST_BRIDGE=1"
      case $with_ufst in
        /*) UFST_ROOT=$with_ufst ;;
         *) UFST_ROOT=`pwd`/$with_ufst ;;
      esac

      # Various versions of UFST fail to build with -Werror=return-type so
      # strip that off our GCFLAGS if it's there.
      tmp_cflags=""
      for tmp_cflag in $GCFLAGS ; do
        echo $tmp_cflag
        tmp_cflags="$tmp_cflags $(echo $tmp_cflag | grep -v "Werror=return-type")"
      done
      GCFLAGS="$tmp_cflags"

      FAPIUFST_MAK="\$(UFST_ROOT)\$(D)fapiufst.mak"

      if ! test -f $UFST_ROOT/fapiufst.mak ; then
        echo $UFST_ROOT/fapiufst.mak
        AC_MSG_ERROR([UFST: fapiufst.mak not found])
      fi

      # Check pointer size
      AC_CHECK_SIZEOF(char*)

      # UFST 5.x uses strcmpi() but on GNU systems the equivalent is
      # strcasecmp()
      if test "x$ac_cv_sizeof_charp" = "x8"; then
        UFST_CFLAGS="-DGCCx86_64 -DO_BINARY=0 -Dstrcmpi=strcasecmp"
      else
        if test "x$ac_cv_sizeof_charp" = "x4"; then
          UFST_CFLAGS="-DGCCx86 -DO_BINARY=0 -Dstrcmpi=strcasecmp"
        fi
      fi

      UFST_LIB_EXT=.a
    else
      AC_MSG_ERROR([UFST source directory not found])
    fi
  fi

  if test x"$enable_freetype" != xno; then
    AC_MSG_CHECKING([for local freetype library source])
    dnl We prefer freetype2 over freetype, so it is easy to override
    dnl the included freetype source with a checkout from upstream.
    for dir in $srccdir/freetype2 $srcdir/freetype; do
      if test -f $dir/src/base/ftbbox.c; then
        AC_MSG_RESULT(yes)
        SHARE_FT=0
        FTSRCDIR="$dir"
        FT_CFLAGS="-I$dir/include"
        FT_BRIDGE=1
        break;
      fi
    done

    if test x"$FTSRCDIR" = x"src"; then
      AC_MSG_RESULT([no])
      if test x"$PKGCONFIG" != x""; then
        AC_MSG_CHECKING(for system freetype2 >= 2.4.2 with pkg-config)
            # pkg-config needs the libtool version, which != the freetype2 version <sigh!>
            # There is a table of corresponding ft2<->libtool numbers in freetype/docs/VERSION.DLL
            if $PKGCONFIG --atleast-version=12.0.6 freetype2; then
                AC_MSG_RESULT(yes)
                if test "x$FT_CFLAGS" = "x"; then
                  FT_CFLAGS="$CFLAGS `$PKGCONFIG --cflags freetype2`"
		fi
		if test "x$FT_LIBS" = "x"; then
                  FT_LIBS="`$PKGCONFIG --libs-only-l freetype2`"
                fi
		if test "x$FT_LIB_PATH" = "x"; then
		  FT_LIB_PATH="`$PKGCONFIG --libs-only-L freetype2`"
		fi
		FT_BRIDGE=1
                SHARE_FT=1
            else
                AC_MSG_RESULT(no)
                AFS=1
            fi
      else
        AC_CHECK_HEADER([ft2build.h], [FT_BRIDGE=1], [AFS=1])

        if test "x$FT_BRIDGE" = x1; then
          AC_CHECK_LIB(freetype, FT_Init_FreeType,
            [FT_BRIDGE=1], [FT_BRIDGE=0; AFS=1])

          if test "x$FT_BRIDGE" = x1; then
            AC_MSG_CHECKING(for system freetype2 library >= 2.4.2)
            AC_COMPILE_IFELSE(
              [AC_LANG_PROGRAM([#include "ft2build.h"
              #include FT_FREETYPE_H], [
                 #if FREETYPE_MAJOR < 2
                   FAIL
                 #endif
                 #if FREETYPE_MINOR < 4
                   FAIL
                 #endif
                 #if FREETYPE_PATCH < 2
                   FAIL
                 #endif
                 return(0);
              ])],
              [FT_BRIDGE=1;AC_MSG_RESULT(yes)], [FT_BRIDGE=0; AFS=1;AC_MSG_RESULT(no)])
          fi
        fi
      fi
    fi
  fi
fi

if test x"$FT_BRIDGE" != x"1"; then
  if test x"$enable_auxtools_only" != x"yes" ; then
    AC_MSG_ERROR([No usable Freetype source or library found])
  fi
fi

AC_SUBST(INSERT_UFST_BRIDGE_EQUAL_ONE)
AC_SUBST(UFST_ROOT)
AC_SUBST(UFST_CFLAGS)
AC_SUBST(UFST_LIB_EXT)
AC_SUBST(FAPIUFST_MAK)

AC_SUBST(FT_BRIDGE)
AC_SUBST(SHARE_FT)
AC_SUBST(FTSRCDIR)
AC_SUBST(FT_CFLAGS)
AC_SUBST(FT_LIBS)
AC_SUBST(FT_LIB_PATH)

TIFF_JPEG_INCLUDE=

if test x"$enable_auxtools_only" = x"yes" ; then
  LIBJPEGDIR=
  SHARE_LIBJPEG=0
  AC_DEFINE([DONT_HAVE_JMEMSYS_H], 1, [])
  AC_SUBST(SHARE_LIBJPEG)
  AC_SUBST(LIBJPEGDIR)
else
  AC_MSG_CHECKING([for local jpeg library source])
  dnl At present, we give the local source priority over the shared
  dnl build, so that the D_MAX_BLOCKS_IN_MCU patch will be applied.
  dnl A more sophisticated approach would be to test the shared lib
  dnl to see whether it has already been patched.
  LIBJPEGDIR=src
  if test -f $srcdir/jpeg/jpeglib.h; then
          AC_MSG_RESULT([jpeg])
          SHARE_LIBJPEG=0
          LIBJPEGDIR=$srcdir/jpeg
  elif test -f $srcdir/jpeg-6b/jpeglib.h; then
          AC_MSG_RESULT([jpeg-6b])
          SHARE_LIBJPEG=0
          LIBJPEGDIR=$srcdir/jpeg-6b
  else
          AC_MSG_RESULT([no])
          AC_CHECK_LIB(jpeg, jpeg_set_defaults, [
            AC_CHECK_HEADERS([jpeglib.h], [SHARE_LIBJPEG=1])
          ])
  fi

  if test x"$LIBJPEGDIR" != x"src" ; then
    TIFF_JPEG_INCLUDE="$LIBJPEGDIR"
  fi

  if test -z "$SHARE_LIBJPEG"; then
    AC_MSG_ERROR([I wasn't able to find a copy
    of the jpeg library. This is required for compiling
    ghostscript. Please download a copy of the source,
    e.g. from http://www.ijg.org/, unpack it at the
    top level of the gs source tree, and rename
    the directory to 'jpeg'.
    ])
  fi
  AC_SUBST(SHARE_LIBJPEG)
  AC_SUBST(LIBJPEGDIR)
  dnl check for the internal jpeg memory header
  AC_MSG_CHECKING([for jmemsys.h])
  if test -r $LIBJPEGDIR/jmemsys.h; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
    AC_DEFINE([DONT_HAVE_JMEMSYS_H], 1,
      [define if the libjpeg memory system prototypes aren't available])
  fi
fi

# this option is useful if you're cross-compiling and want to use
# your toolchain's zlib for $host but the local one for $build
AC_ARG_WITH([local-zlib], AS_HELP_STRING([--without-local-zlib],
  [do not check for local zlib sources (has no effect on AUX toolchain when cross-building)]))

AC_MSG_CHECKING([for local zlib source])
dnl zlib is needed for language level 3, and libpng
# we must define ZLIBDIR regardless because png.mak does a -I$(ZLIBDIR)
# this seems a harmless default
ZLIBDIR=src
AUX_SHARED_ZLIB=
ZLIBCFLAGS=""

if test x"$with_local_zlib" = x"no" ; then
  AC_MSG_RESULT([skip])
  AC_CHECK_LIB(z, deflate, [
    AC_CHECK_HEADERS(zlib.h, [SHARE_ZLIB=1; AUX_SHARED_ZLIB="-l\$(ZLIB_NAME)"])
  ])
else
  if test -d $srcdir/zlib; then
        AC_MSG_RESULT([yes])
        SHARE_ZLIB=0
        ZLIBDIR=$srcdir/zlib

        if test x"$GCC" = x"yes"; then
          ZLIBCFLAGS="-Wno-write-strings"
        fi
  else
        AC_MSG_RESULT([no])
        AC_CHECK_LIB(z, deflate, [
          AC_CHECK_HEADERS(zlib.h, [SHARE_ZLIB=1; AUX_SHARED_ZLIB="-l\$(ZLIB_NAME)"])
        ])
  fi
fi
if test -z "$SHARE_ZLIB"; then
  AC_MSG_ERROR([I did not find a copy of zlib on your system.
  Please either install it, or unpack a copy of the source in a
  local directory named 'zlib'. See http://www.gzip.org/zlib/
  for more information.
  ])
fi

dnl if GS is to use the system zlib, freetype
dnl should do the same
FT_SYS_ZLIB=""
if test x$FT_BRIDGE != x0; then
  if test xx$SHARE_FT != x1; then
    if test x$SHARE_ZLIB != x0; then
      FT_SYS_ZLIB="-DFT_CONFIG_OPTION_SYSTEM_ZLIB"
    fi
  fi
fi

AC_SUBST(SHARE_ZLIB)
AC_SUBST(AUX_SHARED_ZLIB)
AC_SUBST(ZLIBCFLAGS)
AC_SUBST(ZLIBDIR)
AC_SUBST(FT_SYS_ZLIB)

dnl png for the png output device; it also requires zlib
if test x"$enable_auxtools_only" = x"yes" ; then
  LIBPNGDIR=""
  SHARE_LIBPNG=0
else
  LIBPNGDIR=src
  PNGDEVS=''
  PNGDEVS_ALL='png48 png16m pnggray pngmono pngmonod png256 png16 pngalpha'
  AC_MSG_CHECKING([for local png library source])
  if test -f $srcdir/libpng/pngread.c; then
          AC_MSG_RESULT([yes])
          SHARE_LIBPNG=0
          LIBPNGDIR=$srcdir/libpng
          PNGDEVS="$PNGDEVS_ALL"
          case $host in
            *powerpc*)
                # Building on PPC with gcc, disable libpng's PPC specific
                # optimizations: we don't include the source file for it
                # and it only applies to reading, which we don't use.
                if test $GCC = yes; then
                  CFLAGS="$CFLAGS -DPNG_POWERPC_VSX_OPT=0"
                fi
            ;;
            *arm64*|*aarch64*)
                # Building on arm64 with gcc, disable libpng's neon
                # optimizations.
                if test $GCC = yes; then
                  CFLAGS="$CFLAGS -DPNG_ARM_NEON_OPT=0"
                fi
            ;;
          esac
 else
          AC_MSG_RESULT([no])
          AC_CHECK_LIB(png, png_create_write_struct, [
            AC_CHECK_HEADERS(png.h, [
                  SHARE_LIBPNG=1
                  PNGDEVS="$PNGDEVS_ALL"
            ], [SHARE_LIBPNG=0])
          ], [SHARE_LIBPNG=0], [-lz])
  fi
  if test -z "$PNGDEVS"; then
    AC_MSG_NOTICE([disabling png output devices])
  fi
fi
AC_SUBST(SHARE_LIBPNG)
AC_SUBST(LIBPNGDIR)
#AC_SUBST(PNGDEVS)

WHICHLCMS=

AC_MSG_CHECKING([for local lcms2mt library source])
LCMS2DIR=src
LCMS2MTDIR=src

if test x"$enable_auxtools_only" = x"yes" ; then
  SHARELCMS=0
  LCMS2DIR=
  LCMS2MTDIR=
else
# First check for Artifex version
  LCMS2MTSRC=$srcdir/lcms2mt
  if test -f $LCMS2MTSRC/include/lcms2mt.h; then
      AC_MSG_RESULT([yes])
      SHARELCMS=0
      WHICHLCMS=lcms2mt
      LCMS2MTDIR=$LCMS2MTSRC
  else
    AC_MSG_RESULT([no])
    # Now check for local lcms2 (non-Artifex)
    AC_MSG_CHECKING([for local lcms2 library source])
    LCMS2SRC=$srcdir/lcms2
    if test -f $LCMS2SRC/include/lcms2.h; then
      AC_MSG_RESULT([yes])
      SHARELCMS=0
      WHICHLCMS=lcms2
      LCMS2DIR=$LCMS2SRC
    else
      # See if we have system library of lcms2
      AC_MSG_RESULT([no])
      AC_MSG_CHECKING([for system lcms2 library])
      AC_CHECK_LIB(lcms2, _cmsCreateMutex, [
         AC_CHECK_HEADERS([lcms2.h], [SHARELCMS=1;WHICHLCMS=lcms2])
       ])
      if test x$WHICHLCMS = x; then
         AC_MSG_ERROR([lcms2 not found, or too old])
      fi
     fi
  fi
fi

AC_SUBST(SHARELCMS)
AC_SUBST(WHICHLCMS)
AC_SUBST(LCMS2DIR)
AC_SUBST(LCMS2MTDIR)

dnl look for libtiff, it also requires lib
dnl png for the png output device; it also requires zlib
AC_ARG_WITH([libtiff],  AS_HELP_STRING([--without-libtiff],
    [disable use of libtiff]), with_libtiff=no)

AC_ARG_WITH([system-libtiff], AS_HELP_STRING([--with-system-libtiff],
                                             [Force using the systems libtiff]),
            [], [with_system_libtiff=check])

if test x"$with_libtiff" = x"no" ; then
  with_system_libtiff=none
fi

TIFFDEVS=''
FAX_DEVS=''
XPSWRITEDEVICE=''


LIBTIFFDIR='src'
LIBTIFFCONFDIR='src'
TIFFCFLAGS=''

TIFFDEVS_ALL='tiffs tiff12nc tiff24nc tiff48nc tiff32nc tiff64nc tiffcrle tifflzw tiffpack tiffgray tiffsep tiffsep1 tiffscaled tiffscaled4 tiffscaled8 tiffscaled24 tiffscaled32'
FAX_DEVS_ALL='cfax dfaxlow dfaxhigh fax faxg3 faxg32d faxg4 tiffg3 tiffg32d tiffg4 tfax'

case "x$with_system_libtiff" in
    xcheck)
        if test -d $srcdir/tiff; then
            LIBTIFFDIR=$srcdir/tiff
            LIBTIFFCONFDIR=tiff-config
            HAVE_LOCAL_LIBTIFF=1
            SHARE_LIBTIFF=0
            ENABLETIFF="\$(D_)TIFF_INCLUDED\$(_D)"
        else
            # We MUST NOT use PKG_CHECK_MODULES since it is a) not a standard
            # autoconf macro and b) requires pkg-config on the system, which is
            # NOT standard on ANY OS, including Linux!
            if test "x$PKGCONFIG" != x; then
                AC_MSG_CHECKING(for libtiff with pkg-config)
                if $PKGCONFIG --exists libtiff-4; then
                        AC_MSG_RESULT(yes)
                        CFLAGS="$CFLAGS `$PKGCONFIG --cflags libtiff-4`"
                        LIBS="$LIBS `$PKGCONFIG --libs libtiff-4`"
                        HAVE_SYSTEM_LIBTIFF=1
               fi
            fi
            if test "x$HAVE_SYSTEM_LIBTIFF" = "x1"; then
            AC_CHECK_LIB(tiff, TIFFOpen,
                     [AC_CHECK_HEADERS(tiff.h, [HAVE_SYSTEM_LIBTIFF=1;SHARE_LIBTIFF=1])],
                     [], [-ljpeg])
            fi
        fi
        if test "x$HAVE_LOCAL_LIBTIFF" = x && test "x$HAVE_SYSTEM_LIBTIFF" = x; then
            AC_MSG_NOTICE([Could not find a copy of libtiff on your system. Disabling tiff and xps output devices.])
        else
            TIFFDEVS="$TIFFDEVS_ALL"
            FAX_DEVS="$FAX_DEVS_ALL"
            XPSWRITEDEVICE='xpswrite'
            ENABLETIFF="\$(D_)TIFF_INCLUDED\$(_D)"
        fi
        ;;
    xyes)
        # We MUST NOT use PKG_CHECK_MODULES since it is a) not a standard
        # autoconf macro and b) requires pkg-config on the system, which is
        # NOT standard on ANY OS, including Linux!
        if test "x$PKGCONFIG" != x; then
                AC_MSG_CHECKING(for libtiff with pkg-config)
                if $PKGCONFIG --exists libtiff-4; then
                        AC_MSG_RESULT(yes)
                        CFLAGS="$CFLAGS `$PKGCONFIG --cflags libtiff-4`"
                        LIBS="$LIBS `$PKGCONFIG --libs libtiff-4`"
                        HAVE_SYSTEM_LIBTIFF=1
                fi
        fi
        if test -z "$HAVE_SYSTEM_LIBTIFF"; then
        AC_CHECK_LIB(tiff, TIFFOpen,
                     [AC_CHECK_HEADERS(tiff.h, [HAVE_SYSTEM_LIBTIFF=1;SHARE_LIBTIFF=1])],
                     [], [-ljpeg])
        fi
        if test "x$HAVE_SYSTEM_LIBTIFF" != x; then
            SHARE_LIBTIFF=1
            TIFFDEVS="$TIFFDEVS_ALL"
            FAX_DEVS="$FAX_DEVS_ALL"
            XPSWRITEDEVICE='xpswrite'
            ENABLETIFF="\$(D_)TIFF_INCLUDED\$(_D)"
       else
            AC_MSG_NOTICE([Could not find a copy of libtiff on your system. Disabling tiff and xps output devices.])
        fi
        ;;
    xno)
        AC_MSG_CHECKING([for local libtiff source])
        if test -d $srcdir/tiff; then
            AC_MSG_RESULT([yes])
            LIBTIFFDIR=$srcdir/tiff
            LIBTIFFCONFDIR=tiff-config
            SHARE_LIBTIFF=0
            TIFFDEVS="$TIFFDEVS_ALL"
            FAX_DEVS="$FAX_DEVS_ALL"
            XPSWRITEDEVICE='xpswrite'
            ENABLETIFF="\$(D_)TIFF_INCLUDED\$(_D)"
        else
            AC_MSG_RESULT([no])
            AC_MSG_NOTICE([Could not find local copy of libtiff. Disabling tiff and xps output devices.])
        fi
        ;;
    xnone)
        ;;
esac

# Skip this check for the recursive configure call.
# Not relevant for AUX tools.
if test x"$enable_auxtools_only" != x"yes" ; then
  if test x"$ENABLETIFF" != x"" && test x"$SHARE_LIBTIFF" != x"$SHARE_LIBJPEG" ; then
    AC_MSG_ERROR([Mixing local libtiff with shared libjpeg not supported])
  fi
fi

CGLAGS_STORE=$CFLAGS
CFLAGS=-Wno-misleading-indentation
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[CFLAGS_NMI="-Wno-misleading-indentation"],[CFLAGS_NMI=""])

CFLAGS=-Wno-undef
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[CFLAGS_NUD="-Wno-undef"],[CFLAGS_NUD=""])

CFLAGS=$CGLAGS_STORE

if test x"$SHARE_LIBTIFF" = x"0" ; then
      echo "Running libtiff configure script..."
      olddir=`pwd`
      if ! test -d "$LIBTIFFCONFDIR" ; then
        mkdir "$LIBTIFFCONFDIR"
      fi
      cd "$LIBTIFFCONFDIR" && "$absolute_source_path/$LIBTIFFDIR/configure" $GS_TIFF_CONFIGURE_OPTS --disable-jbig --disable-lzma --disable-zstd --disable-webp --disable-libdeflate $SUBCONFIG_OPTS
      status=$?
      if test $status -ne 0 ; then
        AC_MSG_ERROR([libtiff configure script failed], $status)
      fi
      cd "$olddir"

      if test x"$GCC" = x"yes"; then
        TIFFCFLAGS="-Wno-write-strings $CFLAGS_NMI $CFLAGS_NUD -DJPEG_LIB_MK1_OR_12BIT=0"
      fi
      # if we're building with our own libjpeg, or we have another libjpeg available, allow jpeg compression in TIFF
      if test x"$SHARE_LIBJPEG" = x"1" ; then
        TIFFCFLAGS="-DJPEG_SUPPORT -DOJPEG_SUPPORT -DJPEG_LIB_MK1_OR_12BIT=0 $TIFFCFLAGS"
      fi

      echo
      echo "Continuing with Ghostscript configuration..."
fi

AC_SUBST(SHARE_LIBTIFF)
AC_SUBST(LIBTIFFDIR)
AC_SUBST(LIBTIFFCONFDIR)
AC_SUBST(TIFFCFLAGS)
AC_SUBST(ENABLETIFF)


SHARE_EXPAT=0
EXPATDIR=src
EXPAT_CFLAGS=
EXPAT_LIBS=

if test x"$with_xps" != x"no" -a x"$enable_auxtools_only" != x"yes" ; then
  if test -f $srcdir/xps/xps.mak; then
    AC_MSG_CHECKING([for local expat library source])
    if test -f $srcdir/expat/lib/expat.h ; then
      AC_MSG_RESULT([yes])
      SHARE_EXPAT=0
      EXPATDIR=$srcdir/expat
      EXPAT_CFLAGS="-DHAVE_MEMMOVE"
      if test x"$BIGENDIAN" != x"0"; then
        EXPAT_CFLAGS="$EXPAT_CFLAGS -DBYTEORDER=4321"
      else
        EXPAT_CFLAGS="$EXPAT_CFLAGS -DBYTEORDER=1234"
      fi
    else
      AC_CHECK_LIB(expat, XML_ParserCreateNS, [
           AC_CHECK_HEADER([expat.h], [SHARE_EXPAT=1])
          ], [
        AC_MSG_ERROR([expat lib not found])
      ])
      if test $SHARE_EXPAT = 1; then
        if test "x$PKGCONFIG" != x; then
          if $PKGCONFIG --exists expat; then
            EXPAT_LIBS="`$PKGCONFIG --libs expat`"
            CFLAGS="$CFLAGS `$PKGCONFIG --cflags expat`"
          fi
        fi
        if test x"$XPS_LDFLAGS" = x ; then
            EXPAT_LIBS="-lexpat"
        fi
      fi
    fi
  fi
fi

AC_SUBST(SHARE_EXPAT)
AC_SUBST(EXPATDIR)
AC_SUBST(EXPAT_CFLAGS)
AC_SUBST(EXPAT_LIBS)

dnl look for CUPS...
AC_ARG_ENABLE([cups], AS_HELP_STRING([--disable-cups],
    [Do not include CUPS support]))

AC_ARG_WITH([pdftoraster], AS_HELP_STRING([--without-pdftoraster],
    [Do not include CUPS' pdftoraster filter]))

AC_ARG_WITH([local-cups], AS_HELP_STRING([--with-local-cups],
                                             [Force using the GS supplied cups code - only useful for debugging]),
            [with_local_cups=yes], [with_local_cups=no])

AC_ARG_WITH([cups-serverbin],  AS_HELP_STRING([--with-cups-serverbin],
    [override the "cups-config --serverbin" path]), CUPS_SERVERBIN="$withval", CUPS_SERVERBIN="")

AC_ARG_WITH([cups-serverroot],  AS_HELP_STRING([--with-cups-serverroot],
    [override the "cups-config --serverroot" path]), CUPS_SERVERROOT="$withval", CUPS_SERVERROOT="")


AC_ARG_WITH([cups-datadir],  AS_HELP_STRING([--with-cups-datadir],
    [override the "cups-config --datadir" path]), CUPS_DATADIR="$withval", CUPS_DATADIR="")

CUPSDEV=""
CUPSINCLUDE=""
CUPSCFLAGS=""
CUPSLIBS=""
CUPSLIBDIRS=""
CUPSCONFIG="${CUPSCONFIG:=}"
CUPSSERVERBIN=""
CUPSSERVERROOT=""
CUPSDATA=""
CUPSVERSION="0"
CUPSPDFTORASTER="0"
CUPS_DIR=""

cups_hard_fail=0

SHARELCUPS=1
SHARELCUPSI=1

case $host in
    *-mingw*|*-msys*|*-cygwin*)
        AC_MSG_WARN([disabling cups])
        enable_cups=no
    ;;
    *)
    ;;
esac

if ( test -f $srcdir/cups/gdevcups.c ); then
    CUPS_DIR="$srcdir/cups"
#   If the user explicitly enabled cups, hard fail if not found
    if test x"$enable_cups" = x"yes"; then
      cups_hard_fail=1
    fi
    if test x$enable_cups != xno; then
        if test x$with_local_cups != xyes; then
            if test x"$CUPSCONFIG" = x""; then
              AC_PATH_TOOL(CUPSCONFIG,cups-config)
              if test x"$build" != x"$host"; then
                AC_PATH_PROG(BUILD_CUPSCONFIG, cups-config)
                if test x"$BUILD_CUPSCONFIG" = x"$CUPSCONFIG" ; then
                  CUPSCONFIG=
                fi
              fi
            else
#             We were given a cups-config on the command line, assume the user intends to
#             include cups, and make not finding a hard fail.
              cups_hard_fail=1
            fi
            # check for a libcups header
            AC_CHECK_HEADER([cups/cups.h],[],[CUPSCONFIG=""])
            # And check for a libcupsimage header
            if test "x$CUPSCONFIG" != x; then
                AC_CHECK_HEADER([cups/raster.h],[],[CUPSCONFIG=""])
            fi

            if test "x$CUPSCONFIG" != x; then
                dnl Use values from CUPS config...
                CUPSCFLAGS="`$CUPSCONFIG --cflags` $CFLAGS"
#                CUPSLINK="`$CUPSCONFIG --ldflags` `$CUPSCONFIG --static --image --libs | sed -e '1,$s/-lssl//'` $LIBS"
                CUPSLINK="`$CUPSCONFIG --ldflags` `$CUPSCONFIG --image --libs`"
                CUPSAPIVERSION="`$CUPSCONFIG --api-version`"

                GS_SPLIT_LIBS([CUPSLIBS], [$CUPSLINK])
                GS_SPLIT_LIBPATHS([CUPSLIBDIRS],[$CUPSLINK])

                if test "x$CUPS_SERVERROOT" != "x"; then
                  CUPSSERVERROOT="$CUPS_SERVERROOT"
                else
                  CUPSSERVERROOT="`$CUPSCONFIG --serverroot`"
                fi

                if test "x$CUPS_SERVERBIN" != "x"; then
                  CUPSSERVERBIN="$CUPS_SERVERBIN"
                else
                  CUPSSERVERBIN="`$CUPSCONFIG --serverbin`"
                fi

                if test "x$CUPS_DATADIR" != "x"; then
                  CUPSDATA="$CUPS_DATADIR"
                else
                  CUPSDATA="`$CUPSCONFIG --datadir`"
                fi

                CUPSINCLUDE="include $srcdir/cups/cups.mak"
                CUPSDEV="cups"

                # pwgraster support arrived in cups 1.5.x
                if test "$CUPSAPIVERSION" ">" "1.4" ; then
                  CUPSDEV="$CUPSDEV pwgraster"
                fi

                CUPSVERSION="`$CUPSCONFIG --version`"

                LCUPSINCLUDE="include \$(GLSRCDIR)/lcups.mak"
                LCUPSIINCLUDE="include \$(GLSRCDIR)/lcupsi.mak"
                if ( test x$with_pdftoraster != xno ); then
                    if test "$CUPSVERSION" ">" "1.2"; then
                        CUPSPDFTORASTER="1"
                    fi
                fi
            else
                if test x"$cups_hard_fail" = x"1" ; then
                  AC_MSG_ERROR([libcups/libcupsimage not found])
                fi
            fi
        else
            AC_MSG_WARN([USING LOCAL CUPS SOURCE])
            SHARELCUPS=0
            SHARELCUPSI=0
            AC_CHECK_FUNCS([strlcat], [CUPSCFLAGS="$CUPSCFLAGS -DHAVE_STRLCAT=1"], [])
            AC_CHECK_FUNCS([strlcpy], [CUPSCFLAGS="$CUPSCFLAGS -DHAVE_STRLCPY=1"], [])
            AC_CHECK_FUNCS([snprintf], [CUPSCFLAGS="$CUPSCFLAGS -DHAVE_SNPRINTF=1"], [])
            AC_CHECK_FUNCS([vsnprintf], [CUPSCFLAGS="$CUPSCFLAGS -DHAVE_VSNPRINTF=1"], [])
            LCUPSBUILDTYPE=linux
            LCUPSINCLUDE="include \$(GLSRCDIR)/lcups.mak"
            LCUPSIINCLUDE="include \$(GLSRCDIR)/lcupsi.mak"
            CUPSDEV="cups pwgraster"
        fi
    fi
fi

#AC_SUBST(CUPSDEV)
AC_SUBST(CUPSCFLAGS)
AC_SUBST(CUPSLIBS)
AC_SUBST(CUPSLIBDIRS)
AC_SUBST(CUPSINCLUDE)
AC_SUBST(CUPSSERVERBIN)
AC_SUBST(CUPSSERVERROOT)
AC_SUBST(CUPSDATA)
AC_SUBST(CUPSINSTALL)
AC_SUBST(CUPS_DIR)

AC_SUBST(SHARELCUPS)
AC_SUBST(SHARELCUPSI)
AC_SUBST(LCUPSBUILDTYPE)
AC_SUBST(LCUPSINCLUDE)
AC_SUBST(LCUPSIINCLUDE)


AC_SUBST(CUPSPDFTORASTER)

dnl look for IJS implementation
AC_ARG_WITH([ijs], AS_HELP_STRING([--without-ijs],
    [disable IJS driver support]))

case $host in
    *-mingw*|*-msys*)
        AC_MSG_WARN([disabling the ijs device])
        with_ijs=no
    ;;
    *)
    ;;
esac

dnl set safe defaults
    IJSDIR=src
    IJSDEVS=''
    SHAREIJS=0
if test x$with_ijs != xno; then
    AC_MSG_CHECKING([for local ijs library source])
    if test -d $srcdir/ijs; then
        AC_MSG_RESULT([yes])
        IJSDIR=$srcdir/ijs
        IJSDEVS='ijs'
    else
        AC_MSG_RESULT([no])
        AC_CHECK_LIB(ijs, ijs_server_init, [
          AC_CHECK_HEADERS(ijs/ijs.h, [SHAREIJS=1])
        ])
        if test $SHAREIJS -eq 1 ; then
            IJSLIB=ijs
            # This is for safety - it prevents our header search path going outside the GS source tree
            IJSDIR='$(GLOBJDIR)'
            IJSDEVS='ijs'
        fi
    fi
fi
AC_SUBST(IJSDIR)
AC_SUBST(SHAREIJS)
AC_SUBST(IJSLIB)
#AC_SUBST(IJSDEVS)


JBIG2_DECODER=
JBIG2DIR=src
SHARE_JBIG2=0
JBIG2DEVS=''
JBIG2_AUTOCONF_CFLAGS=

JB2_STDINT_TYPES_IN=
JBIG2DEC_REQ=0.19

if test "x$JBIG2_DECODER" = x; then
  dnl look for jbig2dec
  AC_ARG_WITH([jbig2dec], AS_HELP_STRING([--without-jbig2dec],
      [disable JBIG2 decode support]))
  if test x$with_jbig2dec != xno; then
    AC_MSG_CHECKING([for local jbig2dec library source])
    for d in $srcdir/jbig2dec  $srcdir/jbig2dec-0.2  $srcdir/jbig2dec-0.3; do
      test -d "$d" && JBIG2DIR=$d && break
    done
    if test "x$JBIG2DIR" != xsrc; then
        JBIG2_DECODER=jbig2dec
        if test x$ac_cv_header_stdint_h = xyes ; then
            JBIG2_AUTOCONF_CFLAGS="-DHAVE_STDINT_H=1"
        else

          for include in sys/types.h inttypes.h sys/inttypes.h sys/int_types.h ; do
            AC_MSG_CHECKING([for uint32_t in $include])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <$include>]], [[uint32_t canary;]])],[
              AC_MSG_RESULT([yes])
              stdint_types_in="$include"
              break;
              ],[AC_MSG_RESULT(no)
            ])
          done

          case "$stdint_types_in" in
            "sys/types.h")
              JBIG2_AUTOCONF_CFLAGS="$JBIG2_AUTOCONF_CFLAGS -DSTD_INT_USE_SYS_TYPES_H"
              ;;
            "inttypes.h")
              JBIG2_AUTOCONF_CFLAGS="$JBIG2_AUTOCONF_CFLAGS -DSTD_INT_USE_INTTYPES_H"
              ;;
            "sys/inttypes.h")
              JBIG2_AUTOCONF_CFLAGS="$JBIG2_AUTOCONF_CFLAGS -DSTD_INT_USE_SYS_INTTYPES_H"
              ;;
            "sys/int_types.h")
              JBIG2_AUTOCONF_CFLAGS="$JBIG2_AUTOCONF_CFLAGS -DSTD_INT_USE_SYS_INT_TYPES_H"
              ;;
            *)
              AC_MSG_ERROR([
                Unable to find suitable definitions of the stdint.h types (uint32_t and friends).
                These are required by jbig2dec.
              ])
          esac
        fi

        if test "x$BIGENDIAN" != "x0"; then
            JBIG2_AUTOCONF_CFLAGS="$JBIG2_AUTOCONF_CFLAGS -DWORDS_BIGENDIAN"
        fi

        AC_MSG_RESULT([$JBIG2DIR])
        echo "Continuing with Ghostscript configuration..."
    else
      AC_MSG_RESULT([no])
      # We MUST NOT use PKG_CHECK_MODULES since it is a) not a standard
      # autoconf macro and b) requires pkg-config on the system, which is
      # NOT standard on ANY OS, including Linux!
      if test "x$PKGCONFIG" != x; then
        AC_MSG_CHECKING(for jbig2dec $JBIG2DEC_REQ with pkg-config)
        if $PKGCONFIG --atleast-version=$JBIG2DEC_REQ jbig2dec; then
          AC_MSG_RESULT(yes)
          JBIG2DEC_CFLAGS="$CFLAGS `$PKGCONFIG --cflags jbig2dec`"
          JBIG2DEC_LIBS="`$PKGCONFIG --libs jbig2dec`"
          SHARE_JBIG2=1
        else
          AC_MSG_ERROR([jbig2 decoder not found, or too old])
        fi
      else
        AC_MSG_CHECKING(for jbig2dec library)
        AC_CHECK_LIB([jbig2dec], [jbig2_page_out], [
            SHARE_JBIG2=1
        ], [
            AC_MSG_ERROR([jbig2 decoder not found, or too old])
        ])
      fi
      if test x$with_jbig2dec != xno; then
        JBIG2_DECODER=jbig2dec
      fi
    fi
  fi
  if test x$with_jbig2dec != xno; then
# I *think* jbig2dec handles these being missing gracefully
#    if test x$ac_cv_header_stdint_h != xyes && test x$ac_cv_header_inttypes_h != xyes; then
#      AC_MSG_WARN([JBIG2 support requires stdint types which do not seem to be available.])
#    else
      JBIG2DEVS='$(PSD)jbig2.dev'
#    fi
  fi
fi

AC_SUBST(JBIG2_DECODER)
AC_SUBST(JBIG2DIR)
AC_SUBST(SHARE_JBIG2)
AC_SUBST(JBIG2DEVS)
AC_SUBST(JBIG2_AUTOCONF_CFLAGS)

JPXDIR=src
SHARE_JPX=0
JPXDEVS=''
JPX_DECODER=
JPX_AUTOCONF_CFLAGS=
JPX_LRINTF_SUBST=

if test "x$ac_cv_header_stdint_h" = "xyes"; then
  CFLAGS_OPJ_HAVE_STDINT_H="-DOPJ_HAVE_STDINT_H=1"
else
  CFLAGS_OPJ_HAVE_STDINT_H=
fi


if test "x$ac_cv_header_inttypes_h" = "xyes"; then
  CFLAGS_OPJ_HAVE_INTTYPES_H="-DOPJ_HAVE_INTTYPES_H=1"
else
  CFLAGS_OPJ_HAVE_INTTYPES_H=
fi

if test "x$BIGENDIAN" != "x0"; then
  CFLAGS_OPJ_BIGENDIAN="-DOPJ_BIG_ENDIAN"
else
  CFLAGS_OPJ_BIGENDIAN=
fi

AC_CHECK_FUNCS([fseeko], [CFLAGS_OPJ_HAVE_FSEEKO="-DOPJ_HAVE_FSEEKO=1"], [CFLAGS_OPJ_HAVE_STDINT_H=])

JPX_SSE_CFLAGS=""

if test "x$HAVE_SSE2" = "x" ; then
  if test $GCC = yes; then
    JPX_SSE_CFLAGS="-U__SSE__"
  fi
fi

AC_CHECK_DECL(lrintf, , [OPJ_LRINTF_SUBST="-D\"lrintf(a)\"=\"((long)(a+0.5))\"]",[[#include <math.h>]])

AC_ARG_ENABLE([openjpeg], AS_HELP_STRING([--disable-openjpeg],
        [Do not use OpenJPEG for JPX decoding]))

OPENJPEGDIR=$srcdir/openjpeg

if test "x$JPX_DECODER" = "x"; then
  if test "x$enable_openjpeg" != "xno"; then
    AC_MSG_CHECKING([for local OpenJPEG library source])
    if test -e $OPENJPEGDIR/src/lib/openjp2/openjpeg.h; then
      AC_MSG_RESULT([yes])
      JPXDIR="$OPENJPEGDIR"
      JPX_DECODER=openjpeg
      SHARE_JPX=0
      AC_CHECK_FUNCS([memalign], [have_memalign=1])

      if test "x$have_memalign" = "x1"; then
        JPX_AUTOCONF_CFLAGS=""
      else
        JPX_AUTOCONF_CFLAGS="-D\"memalign(a,b)=malloc(b)\""
      fi

      CFLAGS_old="$CFLAGS"
      CFLAGS="-Wno-attributes"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[JPX_AUTOCONF_CFLAGS="$JPX_AUTOCONF_CFLAGS -Wno-attributes"],[])
      CFLAGS="$CFLAGS_old"

      JPX_AUTOCONF_CFLAGS="$JPX_AUTOCONF_CFLAGS -DOPJ_STATIC -DMUTEX_pthread=0 $OPJ_LRINTF_SUBST -DUSE_JPIP -DUSE_OPENJPEG_JP2 $CFLAGS_OPJ_HAVE_STDINT_H $CFLAGS_OPJ_HAVE_INTTYPES_H $CFLAGS_OPJ_BIGENDIAN $CFLAGS_OPJ_HAVE_FSEEKO"

      JPXDEVS='$(PSD)jpx.dev'
    else
      AC_MSG_RESULT([no])
      check_for_opj_stream_set_user_data=no
      if test x"$PKGCONFIG" != x""; then
        AC_MSG_CHECKING(for OpenJPEG2)
        if $PKGCONFIG --exists libopenjp2; then
          JPX_AUTOCONF_CFLAGS="`$PKGCONFIG libopenjp2 --cflags` -DUSE_OPENJPEG_JP2"
          JPX_AUTOCONF_LIBS="`$PKGCONFIG libopenjp2 --libs`"
          SHARE_JPX=1
          JPX_DECODER=openjpeg
          AC_MSG_RESULT([yes])
        else
          AC_MSG_RESULT([no])
          check_for_opj_stream_set_user_data=yes
        fi
      else
        check_for_opj_stream_set_user_data=yes
      fi
      if test x"$check_for_opj_stream_set_user_data" != x"no"; then
        AC_CHECK_LIB(openjp2, opj_stream_set_user_data,
            [JPX_AUTOCONF_CFLAGS="$JPX_AUTOCONF_CFLAGS $OPJ_LRINTF_SUBST -DUSE_JPIP -DUSE_OPENJPEG_JP2 $CFLAGS_OPJ_HAVE_STDINT_H $CFLAGS_OPJ_HAVE_INTTYPES_H $CFLAGS_OPJ_BIGENDIAN $CFLAGS_OPJ_HAVE_FSEEKO";
             JPX_AUTOCONF_LIBS="-lopenjp2";
             SHARE_JPX=1]
        )
      fi
    fi
  fi
fi

AC_SUBST(JPX_DECODER)
AC_SUBST(JPX_AUTOCONF_CFLAGS)
AC_SUBST(JPX_AUTOCONF_LIBS)
AC_SUBST(JPXDIR)
AC_SUBST(SHARE_JPX)
AC_SUBST(JPXDEVS)

ENABLEURF=
URF_DEVS=''
URF_INCLUDE=
URF_DEV=
SURFX_H=
AC_ARG_WITH([urf], AS_HELP_STRING([--without-urf],
    [do not try to include URF support]))

if test x$with_urf != xno; then
  AC_MSG_CHECKING([for URF support])
  if test -d $srcdir/urf; then
    AC_MSG_RESULT([yes])
    ENABLEURF="\$(D_)URF_INCLUDED\$(_D)"
    URF_DEVS='urfgray urfrgb urfcmyk'
    GPDL_URF_TOP_OBJ="\$(GPDLOBJ)/\$(GPDL_URF_TOP_OBJ_FILE)"
    URF_INCLUDE="\$(I_)\$(URFSRCDIR)\$(_I)"
    URF_DEV="\$(GLD)urfd.dev"
    SURFX_H="\$(URFSRCDIR)\$(D)surfx.h"
fi
fi

AC_SUBST(ENABLEURF)
AC_SUBST(GPDL_URF_TOP_OBJ)
AC_SUBST(URF_INCLUDE)
AC_SUBST(URF_DEV)
AC_SUBST(SURFX_H)

AC_ARG_WITH([cal], AS_HELP_STRING([--without-cal],
    [do not try to use the CAL library for acceleration]))

CALSRCDIR=$srcdir/cal
if test x$with_cal != xno; then
  AC_MSG_CHECKING([for local CAL library source])
  if test -d $srcdir/cal; then
    AC_MSG_RESULT([yes])
    WITH_CAL=1
  else
    WITH_CAL=0
    AC_MSG_RESULT([no])
  fi
else
  WITH_CAL=0
fi

if test x$WITH_CAL != x0; then
  dnl --------------------------------------------------
  dnl check for sse4.2, avx2 or neon
  dnl --------------------------------------------------
  AC_MSG_CHECKING([sse4.2 support])
  save_cflags=$CFLAGS
  CFLAGS="$CFLAGS $OPT_CFLAGS -msse4.2"

  HAVE_SSE4_2=""
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <emmintrin.h>
    #include <smmintrin.h>], [
    __m128i input1;
    unsigned char buf1[[128]];
    input1 = _mm_loadu_si128((const __m128i *)buf1);
    input1 = _mm_hadd_epi32(input1, input1);
    return(0);
    ])],
    [HAVE_SSE4_2="-DHAVE_SSE4_2"], [HAVE_SSE4_2=""])

  #AC_ARG_ENABLE([sse4.2], AS_HELP_STRING([--disable-sse4.2],
  #       [Do not use sse4.2 instrinsics]), [
  #             if test "x$enable_sse4.2" = xno; then
  #                HAVE_SSE4_2=""
  #             fi])

  if test "x$HAVE_SSE4_2" != x; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

  AC_MSG_CHECKING([avx2 support])
  CFLAGS="$save_cflags $OPT_CFLAGS -mavx2"

  HAVE_AVX2=""
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <immintrin.h>], [
    __m256i input1;
    unsigned char buf1[[256]];
    input1 = _mm256_loadu_si256((const __m256i *)buf1);
    input1 = _mm256_adds_epu8(input1, input1);
    return(0);
    ])],
    [HAVE_AVX2="-DHAVE_AVX2"], [HAVE_AVX2=""])

  #AC_ARG_ENABLE([sse4.2], AS_HELP_STRING([--disable-sse4.2],
  #       [Do not use sse4.2 instrinsics]), [
  #             if test "x$enable_sse4.2" = xno; then
  #                HAVE_SSE4_2=""
  #             fi])

  if test "x$HAVE_AVX2" != x; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

  AC_MSG_CHECKING([neon support])
  CFLAGS="$save_cflags $OPT_CFLAGS -mfpu=neon -mcpu=cortex-a53"
  HAVE_NEON=""
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include "arm_neon.h"], [
    int32x4_t round = vdupq_n_s32(10);
    return(0);
    ])],
    [HAVE_NEON="-DHAVE_NEON"], [HAVE_NEON=""])

  #AC_ARG_ENABLE([neon], AS_HELP_STRING([--disable-neon],
  #       [Do not use neon instrinsics]), [
  #             if test "x$enable_neon" = xno; then
  #                HAVE_NEON=""
  #             fi])

  if test "x$HAVE_NEON" != x; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

  #AC_SUBST(HAVE_SSE4_2)
  #AC_SUBST(HAVE_NEON)
  CFLAGS=$save_cflags

  CAL_CFLAGS=""
  CAL_SSE4_2_CFLAGS=""
  CAL_AVX2_CFLAGS=""
  CAL_NEON_CFLAGS=""
  if test "x$HAVE_SSE4_2" != x; then
    CAL_SSE4_2_CFLAGS="-msse4.2"
  fi
  if test "x$HAVE_AVX2" != x; then
    CAL_AVX2_CFLAGS="-mavx2"
  fi
  if test "x$HAVE_NEON" != x; then
    CAL_NEON_CFLAGS="-mfpu=neon -mcpu=cortex-a53"
  fi
fi

AC_SUBST(CALSRCDIR)
AC_SUBST(WITH_CAL)
AC_SUBST(CAL_CFLAGS)
AC_SUBST(CAL_SSE4_2_CFLAGS)
AC_SUBST(CAL_AVX2_CFLAGS)
AC_SUBST(CAL_NEON_CFLAGS)

dnl check if we can/should build the gtk loader
AC_ARG_ENABLE([gtk], AS_HELP_STRING([--disable-gtk],
    [Do not build the gtk loader]))
SOC_CFLAGS=""
SOC_LIBS=""
SOC_LOADER=""
if test "x$enable_gtk" != "xno"; then
    # Try GTK+ 3.x first...
    if test "x$PKGCONFIG" != x; then
        AC_MSG_CHECKING(for GTK+ 3.x)
        if $PKGCONFIG --exists gtk+-3.0; then
            SOC_LOADER="dxmain"
            SOC_CFLAGS="`$PKGCONFIG gtk+-3.0 --cflags`"
            SOC_LIBS="`$PKGCONFIG gtk+-3.0 --libs`"
            AC_MSG_RESULT([yes])
        else
            AC_MSG_RESULT([no])
        fi

        # Or resort to GTK+ 2.x
        if test "x$SOC_LOADER" = "x"; then
            AC_MSG_CHECKING(for GTK+ 2.x)
            if $PKGCONFIG --exists gtk+-2.0; then
                SOC_LOADER="dxmain"
                SOC_CFLAGS="`$PKGCONFIG gtk+-2.0 --cflags`"
                SOC_LIBS="`$PKGCONFIG gtk+-2.0 --libs`"
                AC_MSG_RESULT([yes])
            else
                AC_MSG_RESULT([no])
            fi
        fi
    fi
fi

if test "x$SOC_LOADER" = "x"; then
  SOC_LOADER="dxmainc"
fi

AC_SUBST(SOC_CFLAGS)
AC_SUBST(SOC_LIBS)
AC_SUBST(SOC_LOADER)

dnl optional X11 for display devices
if test x"$enable_auxtools_only" = x"yes" ; then
  no_x=yes
else
  AC_PATH_XTRA
fi

X_LDFLAGS=""
X_CFLAGS=""
X_DEVS=""
X_LIBS=""

if test x$no_x != xyes; then
        if test "$x_libraries" = "/usr/lib"; then
                echo "Ignoring X library directory \"$x_libraries\" requested by configure."
                x_libraries="NONE"
        fi
        if test ! "$x_libraries" = "NONE" -a ! "$x_libraries" = ""; then
                X_LDFLAGS="-L$x_libraries"
                case $host in
                    *-sun*|*-solaris*)
                        X_LDFLAGS="$X_LDFLAGS -R$x_libraries"
                    ;;
                    *)
                    ;;
                esac

        fi

        if test "$x_includes" = "/usr/include"; then
                echo "Ignoring X include directory \"$x_includes\" requested by configure."
                x_includes="NONE"
        fi
        if test ! "$x_includes" = "NONE" -a ! "$x_includes" = ""; then
                X_CFLAGS="-I$x_includes"
        fi

        SAVELIBS="$LIBS"
        SAVELDFLAGS="$LDFLAGS"
        LDFLAGS="$LDFLAGS $X_LDFLAGS"

        AC_CHECK_LIB(X11,XOpenDisplay)
        AC_CHECK_LIB(Xext,XdbeQueryExtension)
        AC_CHECK_LIB(Xt,XtAppCreateShell)

        LDFLAGS="$SAVELDFLAGS"
        LIBS="$SAVELIBS"

        if test "$ac_cv_lib_Xt_XtAppCreateShell" = yes; then
                X11DEVS="x11 x11alpha x11cmyk x11mono x11_ x11alt_ x11cmyk2 x11cmyk4 x11cmyk8 x11rg16x x11rg32x x11gray2 x11gray4"
                X_DEVS=$X11DEVS
                # the makefile wants a list of just the library names in X_LIBS
                GS_SPLIT_LIBS([X_LIBS],
                        [-lXt $X_PRE_LIBS -lXext -lX11 $X_EXTRA_LIBS])
        fi
fi

AC_SUBST(X_LDFLAGS)
AC_SUBST(X_CFLAGS)
AC_SUBST(X_LIBS)
#AC_SUBST(X_DEVS)
#AC_SUBST(X11DEVS)
AC_SUBST(XLIBS)

dnl executible name
AC_ARG_WITH([gs], AS_HELP_STRING([--with-gs=NAME],
        [name of the Ghostscript executible [[gs]]]),
        [GS="$with_gs"],[GS='gs'])
AC_SUBST(GS)

PCL=no_gpcl6
PCL_TARGET=

PLROMFS_MAK="\$(GLSRCDIR)\$(D)stub.mak"
PL_MAK="\$(GLSRCDIR)\$(D)stub.mak"
PCL_MAK="\$(GLSRCDIR)\$(D)stub.mak"
PCL_TOP_MAK="\$(GLSRCDIR)\$(D)stub.mak"
PXL_MAK="\$(GLSRCDIR)\$(D)stub.mak"

if test x"$with_pcl" != x"no" ; then
  if test -f $srcdir/pcl/pl/pl.mak; then
    AC_ARG_WITH([pcl], AS_HELP_STRING([--with-pcl=NAME],
        [name of the GhostPCL executible (if the source is available, ignored otherwise) [[gpcl6]]]),
        [PCL="$with_pcl"],[PCL='gpcl6'])

    PCL_TARGET=gpcl6

    PLROMFS_MAK="\$(PLSRCDIR)\$(D)plromfs.mak"
    PL_MAK="\$(PLSRCDIR)\$(D)pl.mak"
    PCL_MAK="\$(PCL5SRCDIR)\$(D)pcl.mak"
    PCL_TOP_MAK="\$(PCL5SRCDIR)\$(D)pcl_top.mak"
    PXL_MAK="\$(PXLSRCDIR)\$(D)pxl.mak"
  fi
fi
AC_SUBST(PCL)
AC_SUBST(PCL_TARGET)

AC_SUBST(PLROMFS_MAK)
AC_SUBST(PL_MAK)
AC_SUBST(PCL_MAK)
AC_SUBST(PCL_TOP_MAK)
AC_SUBST(PXL_MAK)


XPS=no_gxps
XPS_TARGET=

XPS_MAK="\$(GLSRCDIR)\$(D)stub.mak"
XPSROMFS_MAK="\$(GLSRCDIR)\$(D)stub.mak"
# gxps currently depends on code in pl
if test x"$with_xps" != x"no" ; then
  if test x"$PCL_TARGET" != x ; then
    if test -f $srcdir/xps/xps.mak; then
      AC_ARG_WITH([xps], AS_HELP_STRING([--with-xps=NAME],
            [name of the GhostXPS executible (if the source is available, ignored otherwise) [[gxps]]]),
            [XPS="$with_xps"],[XPS='gxps'])

      XPS_TARGET=gxps
      XPS_MAK="\$(XPSSRCDIR)\$(D)xps.mak"
      XPSROMFS_MAK="\$(XPSSRCDIR)\$(D)xpsromfs.mak"
    fi
  fi
fi

AC_SUBST(XPS)
AC_SUBST(XPS_TARGET)
AC_SUBST(XPS_MAK)
AC_SUBST(XPSROMFS_MAK)

GPDF_DEV=
PDF=
PDF_MAK="\$(GLSRCDIR)\$(D)stub.mak"
PDFROMFS_MAK="\$(GLSRCDIR)\$(D)stub.mak"

if test x"$with_pdf" != x"no" ; then
  if test -f $srcdir/pdf/pdf.mak; then
    AC_ARG_WITH([pdf], AS_HELP_STRING([--with-pdf=NAME],
        [name of the Ghostpdf executible (if the source is available, ignored otherwise) [[gpdf]]]),
        [PDFEXE="$with_pdf"],[PDFEXE='gpdf'])

    if test -f $srcdir/pcl/pl/pl.mak && test x"$PCL_TARGET" != x"" ; then
        PDF_TARGET=gpdf
        PDF=$PDFEXE
    else
        PDF=no_gpdf
    fi
    CFLAGS="-DBUILD_PDF=1 -I$srcdir/pdf $CFLAGS"
    GPDF_DEV="\$(PDFOBJDIR)\$(D)pdfi.dev"
    PDF_MAK="\$(PDFSRCDIR)\$(D)pdf.mak"
    PDFROMFS_MAK="\$(PDFSRCDIR)\$(D)pdfromfs.mak"
  fi
fi
AC_SUBST(PDF)
AC_SUBST(PDF_TARGET)

AC_SUBST(PDF_MAK)
AC_SUBST(GPDF_DEV)
AC_SUBST(PDFROMFS_MAK)


GPDL=no_gpdl
GPDL_TARGET=
GPDL_TARGET_VAR=

GPDL_MAK="\$(GLSRCDIR)\$(D)stub.mak"
# in order to build gpdl we need the pdls available

if test "x$XPS_TARGET" != "x" && test "x$PCL_TARGET" != "x" ; then
  if test -f $srcdir/gpdl/gpdl.mak; then
    AC_ARG_WITH([gpdl], AS_HELP_STRING([--with-gpdl=NAME],
          [name of the GhostPDL executible (if the source is available, ignored otherwise) [[gpdl]]]),
          [GPDL="$with_gpdl"],[GPDL='gpdl'])

    GPDL_TARGET=gpdl
    GPDL_MAK="\$(GPDLSRCDIR)\$(D)gpdl.mak"
  fi
fi
AC_SUBST(GPDL)
AC_SUBST(GPDL_TARGET)
AC_SUBST(GPDL_MAK)


dnl do we compile the postscript initialization files into Ghostscript?
COMPILE_INITS="1"
AC_ARG_ENABLE([compile-inits], AS_HELP_STRING([--disable-compile-inits],
       [Do not compile in initialization files]),[
               if test "x$enable_compile_inits" = xno; then
                       COMPILE_INITS="0"
               fi])
AC_SUBST(COMPILE_INITS)

dnl look for drivers to compile...
APPLE_DEVICES_REQUESTED="no"
AC_ARG_WITH(drivers,
[  --with-drivers=LIST     Drivers to support, separated by commas.
                          Either list the drivers or use aliases:
                          ALL      = all drivers
                          FILES    = all file format drivers
                          PRINTERS = all printer drivers
                          Printers:
                          APPLE    = all Apple printers
                          BROTHER  = all Brother printers
                          CANON    = all Canon printers
                          EPSON    = all Epson printers
                          HP       = all HP printers
                          IBM      = all IBM printers
                          JAPAN    = older japanese printers
                          LEXMARK  = all Lexmark printers
                          OKI      = all OKI printers
                          PCLXL    = all PCL XL/6 printers
                          File formats:
                          BMP      = Output to bmp files
                          FAX      = Output to fax files
                          JPEG     = Output to JPEG files
                          PBM      = Output to PBM/PNM
                          PCX      = Output to PCX
                          PNG      = Output to PNG
                          PS       = Output to PostScript/PDF
                          TIFF     = Output to TIFF
                          You can mix both variants, e.g.
                          --with-drivers=HP,stcolor would build HP drivers and
                          the Epson stcolor driver.
                          Aliases must be uppercase (a 3rd party driver might
                          have the same name).
                          Default: ALL],drivers="$withval",drivers="ALL")

AC_ARG_WITH(driversfile,
[  --with-driversfile=FILE Drivers to support from file, separated by newlines.],
driversfile="$withval",driversfile="")

if test "x$driversfile" != x; then
        # Add drivers from file...
        drivers="`tr '\n' ',' <$driversfile`"
fi

if echo $drivers | grep 'APPLE\|iwhi\|iwlo\|iwlq' >/dev/null; then
   APPLE_DEVICES_REQUESTED="yes"
fi

dnl Check which drivers we'd like to support.
P_DEVS0=""
F_DEVS0=""
CUPS_DEVS0=""
XPS_DEVS0=""
JBIG2_DEVS=""
IJS_DEVS0=""
PNG_DEVS0=""
X11_DEVS0=""


dnl Known printers
HP_DEVS='cdj500 djet500 djet500c dnj650c cljet5pr deskjet laserjet ljetplus ljet2p ljet3 ljet3d ljet4 ljet4d lj4dith lj5mono lj5gray cdeskjet cdjcolor cdjmono cdj550 pj pjxl pjxl300 lp2563 paintjet pjetxl cljet5 cljet5c pxlmono pxlcolor cdj670 cdj850 cdj880 cdj890 cdj970 cdj1600 cdnj500 chp2200 pcl3 hpdjplus hpdjportable hpdj310 hpdj320 hpdj340 hpdj400 hpdj500 hpdj500c hpdj510 hpdj520 hpdj540 hpdj550c hpdj560c hpdj600 hpdj660c hpdj670c hpdj680c hpdj690c hpdj850c hpdj855c hpdj870c hpdj890c hpdj1120c lj3100sw'
PCLXL_DEVS='pxlmono pxlcolor'
EPSON_DEVS='eps9high eps9mid epson epsonc escp lp8000 lq850 photoex st800 stcolor alc1900 alc2000 alc4000 alc4100 alc8500 alc8600 alc9100 lp3000c lp8000c lp8200c lp8300c lp8500c lp8800c lp9000c lp9200c lp9500c lp9800c lps6500 epl2050 epl2050p epl2120 epl2500 epl2750 epl5800 epl5900 epl6100 epl6200 lp1800 lp1900 lp2200 lp2400 lp2500 lp7500 lp7700 lp7900 lp8100 lp8300f lp8400f lp8600 lp8600f lp8700 lp8900 lp9000b lp9100 lp9200b lp9300 lp9400 lp9600 lp9600s lps4500 eplcolor eplmono'
CANON_DEVS='bj10e bj200 bjc600 bjc800 lbp8 lips3 bjcmono bjcgray bjccmyk bjccolor'
LEXMARK_DEVS='lxm5700m lxm3200 lex2050 lex3200 lex5700 lex7000'
BROTHER_DEVS='hl7x0 hl1240 hl1250'
APPLE_DEVS='appledmp iwhi iwlo iwlq'
IBM_DEVS='ibmpro jetp3852'
OKI_DEVS='oki182 okiibm oki4w'
JAPAN_DEVS='lips4 lips4v ljet4pjl lj4dithp dj505j picty180 lips2p bjc880j pr201 pr150 pr1000 pr1000_4 jj100 bj10v bj10vh mj700v2c mj500c mj6000c mj8000c fmpr fmlbp ml600 lbp310 lbp320 md50Mono md50Eco md1xMono escpage lp2000 npdl rpdl'
MISC_PDEVS='uniprint ap3250 atx23 atx24 atx38 itk24i itk38 coslw2p coslwxl declj250 fs600 imagen lj250 m8510 necp6 oce9050 r4081 sj48 tek4696 t4693d2 t4693d4 t4693d8 dl2100 la50 la70 la75 la75plus ln03 xes md2k md5k gdi samsunggdi'
OPVP_DEVS='opvp oprp'

ETS_HALFTONING_DEVS='rinkj'

dnl Known file formats
BMP_DEVS='bmpmono bmpgray bmpsep1 bmpsep8 bmp16 bmp256 bmp16m bmp32b'

JPEG_DEVS='jpeg jpeggray jpegcmyk'
# PNG_DEVS='png16 png16m png256 pngalpha pnggray pngmono'

PCX_DEVS='pcxmono pcxgray pcx16 pcx256 pcx24b pcxcmyk'
PBM_DEVS='pbm pbmraw pgm pgmraw pgnm pgnmraw pnm pnmraw ppm ppmraw pkm pkmraw pksm pksmraw pam pamcmyk4 pamcmyk32 plan plang planm planc plank'
PS_DEVS='psdf psdcmyk psdrgb psdcmyk16 psdrgb16 pdfwrite ps2write eps2write bbox txtwrite inkcov ink_cov psdcmykog fpng pdfimage8 pdfimage24 pdfimage32 PCLm'

# Handle --with-extract-dir=EXTRACT_DIR - build extract library and docxwrite
# device.
#
AC_ARG_WITH([extract-dir],
    AS_HELP_STRING([--with-extract-dir=EXTRACT_DIR],
            [Build with the specified Extract library. By default we build with Extract automatically if <ghostpdl-directory>/extract exists.]
            ),
    [
        EXTRACT_DIR="$withval"
        AS_IF([test x"$EXTRACT_DIR" != x"no"],
        [AS_IF([test -e $EXTRACT_DIR],[ PS_DEVS="$PS_DEVS docxwrite"] ,AC_MSG_ERROR([EXTRACT_DIR does not exist: $EXTRACT_DIR]))])
    ],
    [
        AC_MSG_CHECKING([Checking for "extract" in default location])
        EXTRACT_DIR="extract"
        AS_IF([test -e $EXTRACT_DIR], [AC_MSG_RESULT(yes); PS_DEVS="$PS_DEVS docxwrite"], [EXTRACT_DIR=""; AC_MSG_RESULT(no)])
    ]
)

AC_SUBST(EXTRACT_DIR)

# the "display" device isn't an ideal fit in the list below, but it saves adding a "list" for just that one entry
MISC_FDEVS='ccr cif inferno mgr4 mgr8 mgrgray2 mgrgray4 mgrgray8 mgrmono miff24 plan9bm bit bitrgb bitrgbtags bitcmyk devicen spotcmyk xcf plib plibg plibm plibc plibk display'

XPSDEV=$XPSWRITEDEVICE

while test -n "$drivers"; do
        if echo $drivers |grep "," >/dev/null; then
                THIS="`echo $drivers |sed -e 's/,.*//'`"
                SEDCMD="s/$THIS,//"
                drivers="`echo $drivers |sed -e $SEDCMD`"
        else
                THIS=$drivers
                drivers=""
        fi
        case "$THIS" in
        ALL)
                # ALL = PRINTERS + FILES...
                if test -z "$drivers"; then
                        drivers="PRINTERS,FILES,X11"
                else
                        drivers="$drivers,PRINTERS,FILES,X11"
                fi
                ;;
        PRINTERS)
                P_DEVS0="$P_DEVS0 $CANON_DEVS $EPSON_DEVS $HP_DEVS $LEXMARK_DEVS $BROTHER_DEVS $APPLE_DEVS $IBM_DEVS $OKI_DEVS $JAPAN_DEVS $MISC_PDEVS $ETS_HALFTONING_DEVS $URF_DEVS"
                IJS_DEVS0="$IJSDEVS"
                if test x$ac_cv_lib_dl_dlopen != xno -a x$found_iconv != xno; then
                        P_DEVS0="$P_DEVS0 $OPVP_DEVS"
                else
                        AC_MSG_WARN(Unable to include opvp/oprp driver due to missing or disabled prerequisites...)
                fi
                ;;
        FILES)
                F_DEVS0="$F_DEVS0 $BMP_DEVS $FAX_DEVS $JPEG_DEVS $TIFFDEVS $PCX_DEVS $PBM_DEVS $PS_DEVS $MISC_FDEVS $XPSDEV"
                CUPS_DEVS0="$CUPSDEV"
                JBIG2_DEVS="$JBIG2FILEDEVS"
                PNG_DEVS0="$PNGDEVS"
                ;;
        APPLE)
                # All Apple printers
                P_DEVS0="$P_DEVS0 $APPLE_DEVS"
                ;;
        BMP)
                # BMP file format
                F_DEVS0="$F_DEVS0 $BMP_DEVS"
                ;;
        CANON)
                # All Canon printers
                P_DEVS0="$P_DEVS0 $CANON_DEVS"
                ;;
        EPSON)
                # All Epson printers
                P_DEVS0="$P_DEVS0 $EPSON_DEVS"
                ;;
        FAX)
                # Fax file formats
                F_DEVS0="$F_DEVS0 $FAX_DEVS"
                ;;
        JPEG)
                # Jpeg file formats
                F_DEVS0="$F_DEVS0 $JPEG_DEVS"
                ;;
        JBIG2)
                # JBIG file formats
                JBIG2_DEVS="$JBIG2DEVS"
                ;;
        PNG)
                # PNG file formats
                PNG_DEVS0="$PNGDEVS"
                ;;
        TIFF)
                # TIFF file formats
                F_DEVS0="$F_DEVS0 $TIFFDEVS"
                ;;
        PCLXL)
                # PCL XL/PCL 6 drivers
                F_DEVS0="$F_DEVS0 $PCLXL_DEVS"
                ;;
        PCX)
                # PCX file formats
                F_DEVS0="$F_DEVS0 $PCX_DEVS"
                ;;
        PBM)
                # PBM file formats
                F_DEVS0="$F_DEVS0 $PBM_DEVS"
                ;;
        CUPS)
                # CUPS file format
                CUPS_DEVS0="$CUPSDEV"
                ;;
        HP)
                # All HP printers
                P_DEVS0="$P_DEVS0 $HP_DEVS"
                ;;
        LEXMARK)
                # All Lexmark printers
                P_DEVS0="$P_DEVS0 $LEXMARK_DEVS"
                ;;
        BROTHER)
                # All Brother printers
                P_DEVS0="$P_DEVS0 $BROTHER_DEVS"
                ;;
        OKI)
                # All OKI printers
                P_DEVS0="$P_DEVS0 $OKI_DEVS"
                ;;
        IBM)
                # All IBM printers
                P_DEVS0="$P_DEVS0 $IBM_DEVS"
                ;;
        IJS)
                # IJS printers
                IJS_DEVS0="$IJSDEVS"
                ;;
        JAPAN)
                # All old japanese printers
                P_DEVS0="$P_DEVS0 $JAPAN_DEVS"
                ;;
        PS)
                # PostScript/PDF writing
                F_DEVS0="$F_DEVS0 $PS_DEVS"
                ;;
        XPS)
                # XPS writing
                XPS_DEVS0="$XPSDEV"
                ;;

        ETS)
                # ETS Halftoning devices
                F_DEVS0="$F_DEVS0 $ETS_HALFTONING_DEVS"
                ;;
        X11)
                # X11 display devices
                X11_DEVS0="$X_DEVS"
                ;;
        opvp)
                # Open Vector Printing driver...
                if test x$ac_cv_lib_dl_dlopen != xno -a x$found_iconv != xno; then
                        P_DEVS0="$P_DEVS0 $OPVP_DEVS"
                else
                        AC_MSG_WARN(Unable to include opvp/oprp driver due to missing or disabled prerequisites...)
                fi
                ;;
        *)
                # It's a driver name (or a user messup)
                P_DEVS0="$P_DEVS0 `echo $THIS |sed -e 's,\.dev$,,'`"
                ;;
        esac
done

if test x$enable_threadsafe = xyes; then

  for ntsdev in $NTS_DEVS ; do
    NTS_EXCLUDES="$(echo \"$P_DEVS0{[@]:0}\" | grep -o $ntsdev) $NTS_EXCLUDES"
    P_DEVS0=`echo $P_DEVS0 | sed "s/\b$ntsdev\b//g"`
  done

  for ntsdev in $NTS_DEVS ; do
    NTS_EXCLUDES="$(echo \"$F_DEVS0{[@]:0}\" | grep -o $ntsdev) $NTS_EXCLUDES"
    F_DEVS0=`echo $F_DEVS0 | sed "s/\b$ntsdev\b//g"`
  done

  for ntsdev in $NTS_DEVS ; do
    NTS_EXCLUDES="$(echo \"$CUPS_DEVS0{[@]:0}\" | grep -o $ntsdev) $NTS_EXCLUDES"
    CUPS_DEVS0=`echo $CUPS_DEVS0 | sed "s/\b$ntsdev\b//g"`
  done

  for ntsdev in $NTS_DEVS ; do
    NTS_EXCLUDES="$(echo \"$XPS_DEVS0{[@]:0}\" | grep -o $ntsdev) $NTS_EXCLUDES"
    XPS_DEVS0=`echo $XPS_DEVS0 | sed "s/\b$ntsdev\b//g"`
  done

  for ntsdev in $NTS_DEVS ; do
    NTS_EXCLUDES="$(echo \"$IJS_DEVS0{[@]:0}\" | grep -o $ntsdev) $NTS_EXCLUDES"
    IJS_DEVS0=`echo $IJS_DEVS0 | sed "s/\b$ntsdev\b//g"`
  done

  for ntsdev in $NTS_DEVS ; do
    NTS_EXCLUDES="$(echo \"$PNG_DEVS0{[@]:0}\" | grep -o $ntsdev) $NTS_EXCLUDES"
    PNG_DEVS0=`echo $PNG_DEVS0 | sed "s/\b$ntsdev\b//g"`
  done

  for ntsdev in $NTS_DEVS ; do
    NTS_EXCLUDES="$(echo \"$X11_DEVS0{[@]:0}\" | grep -o $ntsdev) $NTS_EXCLUDES"
    X11_DEVS0=`echo $X11_DEVS0 | sed "s/\b$ntsdev\b//g"`
  done

  for ntsdev in $NTS_DEVS ; do
    NTS_EXCLUDES="$(echo \"$JBIG2_DEVS{[@]:0}\" | grep -o $ntsdev) $NTS_EXCLUDES"
    JBIG2_DEVS=`echo $JBIG2_DEVS | sed "s/\b$ntsdev\b//g"`
  done

  NTS_EXCLUDES=`echo "$NTS_EXCLUDES" | tr " " "\n" | sort | uniq | tr "\n" " "`
fi # x$enable_threadsafe = xyes

noncontribmakefiles=`find $srcdir -name '*.mak' -print | grep -v '/contrib/'`

# No need to include opvp/oprp driver without iconv/libiconv.
if test -n "$P_DEVS0"; then
    if test x$found_iconv = xno ; then
        P_DEVS0=`echo $P_DEVS0 | sed -e 's|opvp||' -e 's|oprp||'`
        AC_MSG_WARN(Unable to include opvp/oprp driver due to missing iconv/libiconv...)
    fi

#    Remove contributed drivers if requested and make sure we don't have any
#   duplicates in there, add $(DD)foo.dev constructs
    P_DEVS=`(for i in $P_DEVS0; do d='$(DD)'${i}.dev; if ( grep '^'$d $noncontribmakefiles 2>&1 >/dev/null ) || ( ! test -z "$CONTRIBINCLUDE" ); then echo $d; fi; done) | sort | uniq | tr '\012\015' '  '`
fi

if test -n "$F_DEVS0"; then
    F_DEVS=`(for i in $F_DEVS0; do d='$(DD)'${i}.dev; if ( grep '^'$d $noncontribmakefiles  2>&1 >/dev/null) || ( ! test -z "$CONTRIBINCLUDE" ); then echo $d; fi; done) | sort | uniq | tr '\012\015' '  '`
fi

if test -n "$CUPS_DEVS0"; then
    CUPS_DEVS=`(for i in $CUPS_DEVS0; do d='$(DD)'${i}.dev; if ( grep '^'$d $noncontribmakefiles  2>&1 >/dev/null) || ( ! test -z "$CONTRIBINCLUDE" ); then echo $d; fi; done) | sort | uniq | tr '\012\015' '  '`
fi

if test -n "$XPS_DEVS0"; then
    XPS_DEVS=`(for i in $XPS_DEVS0; do d='$(DD)'${i}.dev; if ( grep '^'$d $noncontribmakefiles  2>&1 >/dev/null) || ( ! test -z "$CONTRIBINCLUDE" ); then echo $d; fi; done) | sort | uniq | tr '\012\015' '  '`
fi

if test -n "$IJS_DEVS0"; then
    IJS_DEVS=`(for i in $IJS_DEVS0; do d='$(DD)'${i}.dev; if ( grep '^'$d $noncontribmakefiles  2>&1 >/dev/null) || ( ! test -z "$CONTRIBINCLUDE" ); then echo $d; fi; done) | sort | uniq | tr '\012\015' '  '`
fi

if test -n "$PNG_DEVS0"; then
    PNG_DEVS=`(for i in $PNG_DEVS0; do d='$(DD)'${i}.dev; if ( grep '^'$d $noncontribmakefiles  2>&1 >/dev/null) || ( ! test -z "$CONTRIBINCLUDE" ); then echo $d; fi; done) | sort | uniq | tr '\012\015' '  '`
fi

if test -n "$X11_DEVS0"; then
    X11_DEVS=`(for i in $X11_DEVS0; do d='$(DD)'${i}.dev; if ( grep '^'$d $noncontribmakefiles  2>&1 >/dev/null) || ( ! test -z "$CONTRIBINCLUDE" ); then echo $d; fi; done) | sort | uniq | tr '\012\015' '  '`
fi

# if the user explicitly requested the Apple devices, then hard fail if they aren't available
if test x"$APPLE_DEVICES_REQUESTED" = x"yes" ; then
  APDEVS=`echo $P_DEVS | grep 'APPLE\|iwhi\|iwlo\|iwlq'`
  if test -z "$APDEVS" ; then
    AC_MSG_ERROR([Requested Apple device(s) not available (iwhi, iwlo, and/or iwlq)])
  fi
fi

AC_SUBST(P_DEVS)
AC_SUBST(F_DEVS)
AC_SUBST(CUPS_DEVS)
AC_SUBST(XPS_DEVS)
AC_SUBST(JBIG2_DEVS)
AC_SUBST(IJS_DEVS)
AC_SUBST(PNG_DEVS)

# This now gets done after handling --enable-dynamic
# AC_SUBST(X11_DEVS)

dnl Dynamic device support.
DYNAMIC_CFLAGS=""
DYNAMIC_DEVS=""
DYNAMIC_FLAGS=""
GS_DYNAMIC_LDFLAGS=""
PCL_DYNAMIC_LDFLAGS=""
XPS_DYNAMIC_LDFLAGS=""
PDL_DYNAMIC_LDFLAGS=""
DYNAMIC_LIBS=""
INSTALL_SHARED=""
SO_LIB_EXT=".so"
DLL_EXT=""
SO_LIB_VERSION_SEPARATOR="."

libname1="_SO_BASE)\$(GS_SOEXT)\$(DLL_EXT)"
libname2="_SO_BASE)\$(GS_SOEXT)\$(SO_LIB_VERSION_SEPARATOR)\$(GS_VERSION_MAJOR)\$(DLL_EXT)"
libname3="_SO_BASE)\$(GS_SOEXT)\$(SO_LIB_VERSION_SEPARATOR)\$(GS_VERSION_MAJOR)\$(SO_LIB_VERSION_SEPARATOR)\$(GS_VERSION_MINOR)\$(DLL_EXT)"
GS_SONAME="lib\$(GS${libname1}"
GS_SONAME_MAJOR="lib\$(GS${libname2}"
GS_SONAME_MAJOR_MINOR="lib\$(GS${libname3}"
PCL_SONAME="lib\$(PCL${libname1}"
PCL_SONAME_MAJOR="lib\$(PCL${libname2}"
PCL_SONAME_MAJOR_MINOR="lib\$(PCL${libname3}"
XPS_SONAME="lib\$(XPS${libname1}"
XPS_SONAME_MAJOR="lib\$(XPS${libname2}"
XPS_SONAME_MAJOR_MINOR="lib\$(XPS${libname3}"

PDF_SONAME="lib\$(PDF${libname1}"
PDF_SONAME_MAJOR="lib\$(PDF${libname2}"
PDF_SONAME_MAJOR_MINOR="lib\$(PDF${libname3}"

GPDL_SONAME="lib\$(GPDL${libname1}"
GPDL_SONAME_MAJOR="lib\$(GPDL${libname2}"
GPDL_SONAME_MAJOR_MINOR="lib\$(GPDL${libname3}"

hide_symbols=no
attr_default=
attr_hidden=
_ldflags=

AC_ARG_ENABLE([hidden-visibility],
    AS_HELP_STRING([--enable-hidden-visibility],
        [hide all shared library symbols which are not part of its public API]),
    [hide_symbols=yes])

if test x$hide_symbols = xyes -a $GCC = yes; then
    attr_default="__attribute__((visibility(\\\"default\\\")))"
    attr_hidden="__attribute__((visibility(\\\"hidden\\\")))"
fi

case $host in
    *-linux*|*-gnu)
      DYNAMIC_CFLAGS="-fPIC $DYNAMIC_CFLAGS"
      GS_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(GS_SONAME_MAJOR)"
      PCL_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(PCL_SONAME_MAJOR)"
      XPS_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(XPS_SONAME_MAJOR)"
      PDL_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(GPDL_SONAME_MAJOR)"
      PDF_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(PDF_SONAME_MAJOR)"
      if test $GCC = yes; then
        # GCC high level flag
        DYNAMIC_LIBS="-rdynamic"
      else
        DYNAMIC_LIBS=""
      fi
      SO_LIB_EXT=".so"
    ;;
    *-mingw*|*-msys*)
      if test x$hide_symbols = xyes; then
        attr_default="__declspec(dllexport)"
        attr_hidden=""
      else
        _ldflags="-Wl,--export-all-symbols"
      fi
      _ldflags="$_ldflags -Wl,--enable-auto-import"
      GS_DYNAMIC_LDFLAGS="-shared -Wl,--out-implib=\$(BINDIR)/lib\$(GS_SO_BASE).dll.a $_ldflags"
      PCL_DYNAMIC_LDFLAGS="-shared -Wl,--out-implib=\$(BINDIR)/lib\$(PCL_SO_BASE).dll.a $_ldflags"
      XPS_DYNAMIC_LDFLAGS="-shared -Wl,--out-implib=\$(BINDIR)/lib\$(XPS_SO_BASE).dll.a $_ldflags"
      PDL_DYNAMIC_LDFLAGS="-shared -Wl,--out-implib=\$(BINDIR)/lib\$(PDL_SO_BASE).dll.a $_ldflags"
      SO_LIB_EXT=""
      DLL_EXT=".dll"
      SO_LIB_VERSION_SEPARATOR="-"
      # make sure the import library doesn't link a program against the DLL with the minor version in it
      GS_SONAME="lib\$(GS${libname1}"
      GS_SONAME_MAJOR="lib\$(GS${libname3}"
      GS_SONAME_MAJOR_MINOR="lib\$(GS${libname2}"
      PCL_SONAME="lib\$(PCL${libname1}"
      PCL_SONAME_MAJOR="lib\$(PCL${libname3}"
      PCL_SONAME_MAJOR_MINOR="lib\$(PCL${libname2}"
      XPS_SONAME="lib\$(XPS${libname1}"
      XPS_SONAME_MAJOR="lib\$(XPS${libname3}"
      XPS_SONAME_MAJOR_MINOR="lib\$(XPS${libname2}"
      PDF_SONAME="lib\$(PDF${libname1}"
      PDF_SONAME_MAJOR="lib\$(PDF${libname3}"
      PDF_SONAME_MAJOR_MINOR="lib\$(PDF${libname2}"
      GPDL_SONAME="lib\$(GPDL${libname1}"
      GPDL_SONAME_MAJOR="lib\$(GPDL${libname3}"
      GPDL_SONAME_MAJOR_MINOR="lib\$(GPDL${libname2}"
    ;;
    *-cygwin*)
      if test x$hide_symbols = xyes; then
        attr_default="__declspec(dllexport)"
        attr_hidden=""
      fi
      GS_DYNAMIC_LDFLAGS="-shared -Wl,--out-implib=\$(BINDIR)/lib\$(GS_SO_BASE).dll.a $_ldflags"
      PCL_DYNAMIC_LDFLAGS="-shared -Wl,--out-implib=\$(BINDIR)/lib\$(PCL_SO_BASE).dll.a $_ldflags"
      XPS_DYNAMIC_LDFLAGS="-shared -Wl,--out-implib=\$(BINDIR)/lib\$(XPS_SO_BASE).dll.a $_ldflags"
      PDL_DYNAMIC_LDFLAGS="-shared -Wl,--out-implib=\$(BINDIR)/lib\$(PDL_SO_BASE).dll.a $_ldflags"
      SO_LIB_EXT=""
      DLL_EXT=".dll"
      SO_LIB_VERSION_SEPARATOR="-"
      # make sure the import library doesn't link a program against the DLL with the minor version in it
      GS_SONAME="lib\$(GS${libname1}"
      GS_SONAME_MAJOR="lib\$(GS${libname3}"
      GS_SONAME_MAJOR_MINOR="cyg\$(GS${libname2}"
      PCL_SONAME="lib\$(PCL${libname1}"
      PCL_SONAME_MAJOR="lib\$(PCL${libname3}"
      PCL_SONAME_MAJOR_MINOR="cyg\$(PCL${libname2}"
      XPS_SONAME="lib\$(XPS${libname1}"
      XPS_SONAME_MAJOR="lib\$(XPS${libname3}"
      XPS_SONAME_MAJOR_MINOR="cyg\$(XPS${libname2}"
      GPDL_SONAME="lib\$(GPDL${libname1}"
      GPDL_SONAME_MAJOR="lib\$(GPDL${libname3}"
      GPDL_SONAME_MAJOR_MINOR="cyg\$(GPDL${libname2}"
    ;;
    *bsd*)
      DYNAMIC_CFLAGS="-fPIC $DYNAMIC_CFLAGS"
      GS_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(GS_SONAME_MAJOR)"
      PCL_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(PCL_SONAME_MAJOR)"
      XPS_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(XPS_SONAME_MAJOR)"
      PDL_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(GPDL_SONAME_MAJOR)"
      PDF_DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(PDF_SONAME_MAJOR)"
      DYNAMIC_LIBS=""
      SO_LIB_EXT=".so"
    ;;
    *-darwin*)
      DYNAMIC_CFLAGS="-fPIC $DYNAMIC_CFLAGS"
      GS_DYNAMIC_LDFLAGS="-dynamiclib -install_name $DARWIN_LDFLAGS_SO_PREFIX\$(GS_SONAME_MAJOR_MINOR)"
      PCL_DYNAMIC_LDFLAGS="-dynamiclib -install_name $DARWIN_LDFLAGS_SO_PREFIX\$(PCL_SONAME_MAJOR_MINOR)"
      XPS_DYNAMIC_LDFLAGS="-dynamiclib -install_name $DARWIN_LDFLAGS_SO_PREFIX\$(XPS_SONAME_MAJOR_MINOR)"
      PDL_DYNAMIC_LDFLAGS="-dynamiclib -install_name $DARWIN_LDFLAGS_SO_PREFIX\$(GPDL_SONAME_MAJOR_MINOR)"
      PDF_DYNAMIC_LDFLAGS="-dynamiclib -install_name $DARWIN_LDFLAGS_SO_PREFIX\$(PDF_SONAME_MAJOR_MINOR)"
      DYNAMIC_LIBS=""
      SO_LIB_EXT=".dylib"
    ;;
    *-sun*|*-solaris*)
      if test $GCC = yes; then
        DYNAMIC_CFLAGS="-fPIC $DYNAMIC_CFLAGS"
      else
        DYNAMIC_CFLAGS="-KPIC $DYNAMIC_CFLAGS"
        #if test x$hide_symbols = xyes; then
        #  attr_default="__global"
        #  attr_hidden="__hidden"
        #  _ldflags="-xldscope=hidden"
        #fi
        # ^^^ untested
      fi
      GS_DYNAMIC_LDFLAGS="-G -shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(GS_SONAME_MAJOR) $_ldflags"
      PCL_DYNAMIC_LDFLAGS="-G -shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(PCL_SONAME_MAJOR) $_ldflags"
      XPS_DYNAMIC_LDFLAGS="-G -shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(XPS_SONAME_MAJOR) $_ldflags"
      PDL_DYNAMIC_LDFLAGS="-G -shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(GPDL_SONAME_MAJOR) $_ldflags"
      PDF_DYNAMIC_LDFLAGS="-G -shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(PDF_SONAME_MAJOR)"
      DYNAMIC_LIBS=""
      SO_LIB_EXT=".so"
    ;;
    *-aix*)
      if test $GCC = yes; then
        DYNAMIC_CFLAGS="-fPIC $DYNAMIC_CFLAGS"
        GCFLAGS="-Wl,-brtl -D_LARGE_FILES $GCFLAGS"
        GS_DYNAMIC_LDFLAGS="-shared -Wl,-brtl,-G -fPIC"
        PCL_DYNAMIC_LDFLAGS="-shared -Wl,-brtl,-G -fPIC"
        XPS_DYNAMIC_LDFLAGS="-shared -Wl,-brtl,-G -fPIC"
        PDL_DYNAMIC_LDFLAGS="-shared -Wl,-brtl,-G -fPIC"
        PDF_DYNAMIC_LDFLAGS="-shared -Wl,-brtl,-G -fPIC"
      else
        DYNAMIC_CFLAGS="$DYNAMIC_CFLAGS"
        GCFLAGS="-Wl,-brtl -D_LARGE_FILES $GCFLAGS"
        GS_DYNAMIC_LDFLAGS="-G -qmkshrobj -Wl,-brtl,-G"
        PCL_DYNAMIC_LDFLAGS="-G -qmkshrobj -Wl,-brtl,-G"
        XPS_DYNAMIC_LDFLAGS="-G -qmkshrobj -Wl,-brtl,-G"
        PDL_DYNAMIC_LDFLAGS="-G -qmkshrobj -Wl,-brtl,-G"
        PDF_DYNAMIC_LDFLAGS="-G -qmkshrobj -Wl,-brtl,-G"
      fi
      SO_LIB_EXT=".so"
      ;;
esac

if test x$hide_symbols = xyes ; then
  if test $GCC = yes; then
    DYNAMIC_CFLAGS="$DYNAMIC_CFLAGS -fvisibility=hidden"
  fi
  DYNAMIC_CFLAGS="$DYNAMIC_CFLAGS -DGSDLLEXPORT=\"$attr_default\""
  if test "x$FT_BRIDGE" = x1 -a "x$SHARE_FT" = x0 ; then
    DYNAMIC_CFLAGS="$DYNAMIC_CFLAGS -D\"FT_EXPORT(x)\"=\"$attr_hidden x\""
  fi
  if test "x$SHARE_EXPAT" = x0 ; then
    DYNAMIC_CFLAGS="$DYNAMIC_CFLAGS -DXML_STATIC -DXMLIMPORT=\"$attr_hidden\""
  fi
fi

AC_SUBST(GS_SONAME)
AC_SUBST(GS_SONAME_MAJOR)
AC_SUBST(GS_SONAME_MAJOR_MINOR)
AC_SUBST(PCL_SONAME)
AC_SUBST(PCL_SONAME_MAJOR)
AC_SUBST(PCL_SONAME_MAJOR_MINOR)
AC_SUBST(XPS_SONAME)
AC_SUBST(XPS_SONAME_MAJOR)
AC_SUBST(XPS_SONAME_MAJOR_MINOR)

AC_SUBST(PDF_SONAME)
AC_SUBST(PDF_SONAME_MAJOR)
AC_SUBST(PDF_SONAME_MAJOR_MINOR)

AC_SUBST(GPDL_SONAME)
AC_SUBST(GPDL_SONAME_MAJOR)
AC_SUBST(GPDL_SONAME_MAJOR_MINOR)

AC_ARG_ENABLE([dynamic], AS_HELP_STRING([--enable-dynamic],
    [Enable dynamically loaded drivers]),
[
  if test "x$enable_dynamic" != xno; then
    case $host in
      *-linux*|*-gnu)
        INSTALL_SHARED="install-shared"
        if test "x$X_DEVS" != x; then
                DYNAMIC_DEVS="\$(GLOBJDIR)/X11.so"
        else
                DYNAMIC_DEVS=""
        fi
        DYNAMIC_FLAGS="-DGS_DEVS_SHARED -DGS_DEVS_SHARED_DIR=\\\"\$(gssharedir)\\\""
        X11_DEVS=""
        OPT_CFLAGS="$DYNAMIC_CFLAGS $OPT_CFLAGS"
        DBG_CFLAGS="$DYNAMIC_CFLAGS $DBG_CFLAGS"
      ;;
      *bsd*)
        DYNAMIC_DEVS="\$(GLOBJDIR)/X11.so"
        DYNAMIC_FLAGS="-DGS_DEVS_SHARED -DGS_DEVS_SHARED_DIR=\\\"\$(gssharedir)\\\""
        X11_DEVS=""
        OPT_CFLAGS="$DYNAMIC_CFLAGS $OPT_CFLAGS"
        DBG_CFLAGS="$DYNAMIC_CFLAGS $DBG_CFLAGS"
      ;;
      *-darwin*)
        INSTALL_SHARED="install-shared"
        DYNAMIC_FLAGS="-DGS_DEVS_SHARED -DGS_DEVS_SHARED_DIR=\\\"\$(gssharedir)\\\""
        X11_DEVS=""
        OPT_CFLAGS="$DYNAMIC_CFLAGS $OPT_CFLAGS"
        DBG_CFLAGS="$DYNAMIC_CFLAGS $DBG_CFLAGS"
      ;;
      *-sun*|*-solaris*)
        DYNAMIC_DEVS="\$(GLOBJDIR)/X11.so"
        DYNAMIC_FLAGS="-DGS_DEVS_SHARED -DGS_DEVS_SHARED_DIR=\\\"\$(gssharedir)\\\""
        OPT_CFLAGS="$DYNAMIC_CFLAGS $OPT_CFLAGS"
        DBG_CFLAGS="$DYNAMIC_CFLAGS $DBG_CFLAGS"
      ;;
      *)
        AC_MSG_ERROR([Sorry, dynamic driver support not available on this platform!])
      ;;
    esac
  fi
])

AC_SUBST(DYNAMIC_CFLAGS)
AC_SUBST(DYNAMIC_DEVS)
AC_SUBST(DYNAMIC_FLAGS)
AC_SUBST(GS_DYNAMIC_LDFLAGS)
AC_SUBST(PCL_DYNAMIC_LDFLAGS)
AC_SUBST(XPS_DYNAMIC_LDFLAGS)
AC_SUBST(PDL_DYNAMIC_LDFLAGS)
AC_SUBST(PDF_DYNAMIC_LDFLAGS)
AC_SUBST(DYNAMIC_LIBS)
AC_SUBST(INSTALL_SHARED)
AC_SUBST(X11_DEVS)
AC_SUBST(SO_LIB_EXT)
AC_SUBST(DLL_EXT)
AC_SUBST(SO_LIB_VERSION_SEPARATOR)

dnl look for default font path...
AC_ARG_WITH([fontpath],  AS_HELP_STRING([--with-fontpath],
    [set font search path for gs]), fontpath="$withval", fontpath="")

dnl Fix "prefix" variable...
if test "x$prefix" = xNONE; then
        prefix=/usr/local
fi

dnl Fix "datadir" variable...
if test "x$datadir" = 'x${prefix}/share'; then
        datadir="$prefix/share"
fi

dnl Fix "fontpath" variable...
if test "x$fontpath" = "x"; then
        # These font directories are used by various Linux distributions...
        fontpath="$datadir/fonts/default/ghostscript"
        fontpath="${fontpath}:$datadir/fonts/default/Type1"
        fontpath="${fontpath}:$datadir/fonts/default/TrueType"

        # These font directories are used by IRIX...
        fontpath="${fontpath}:/usr/lib/DPS/outline/base"

        # These font directories are used by Solaris...
        fontpath="${fontpath}:/usr/openwin/lib/X11/fonts/Type1"
        fontpath="${fontpath}:/usr/openwin/lib/X11/fonts/TrueType"

        # This font directory is used by CUPS...
        if test "x$CUPSCONFIG" != x; then
          fontpath="${fontpath}:`$CUPSCONFIG --datadir`/fonts"
        fi
fi

AC_SUBST(fontpath)

dnl look for default tessdata...
AC_ARG_WITH([tessdata],  AS_HELP_STRING([--with-tessdata],
    [set tesseract data search path]), tessdata="$withval", tessdata="")

if test "x$tessdata" = "x"; then
        tessdata="${datadir}/tessdata"
fi

AC_SUBST(tessdata)

dnl --------------------------------------------------
dnl Check for library functions
dnl --------------------------------------------------

AC_CHECK_FUNCS([mkstemp], [HAVE_MKSTEMP=-DHAVE_MKSTEMP])
AC_SUBST(HAVE_MKSTEMP)

AC_CHECK_FUNCS([fopen64], [HAVE_FILE64=-DHAVE_FILE64])
AC_SUBST(HAVE_FILE64)

AC_CHECK_FUNCS([fseeko], [HAVE_FSEEKO=-DHAVE_FSEEKO])
AC_SUBST(HAVE_FSEEKO)

AC_CHECK_FUNCS([mkstemp64], [HAVE_MKSTEMP64=-DHAVE_MKSTEMP64])
AC_SUBST(HAVE_MKSTEMP64)

AC_CHECK_FUNCS([setlocale], [HAVE_SETLOCALE=-DHAVE_SETLOCALE])
AC_SUBST(HAVE_SETLOCALE)

AC_CHECK_FUNCS([strerror], [HAVE_STRERROR=-DHAVE_STRERROR])
AC_SUBST(HAVE_STRERROR)

AC_CHECK_FUNCS([isnan], [HAVE_ISNAN=-DHAVE_ISNAN])
AC_SUBST(HAVE_ISNAN)

AC_CHECK_FUNCS([isinf], [HAVE_ISINF=-DHAVE_ISINF])
AC_SUBST(HAVE_ISINF)

AC_CHECK_FUNCS([fpclassify], [HAVE_FPCLASSIFY=-DHAVE_FPCLASSIFY])
AC_SUBST(HAVE_FPCLASSIFY)

AC_PROG_GCC_TRADITIONAL

dnl NB: We don't actually provide autoconf-switched fallbacks for any
dnl     of these functions, so the checks are purely informational.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero dup2 floor gettimeofday memchr memmove memset mkdir mkfifo modf pow putenv rint setenv sqrt strchr strrchr strspn strstr])

dnl --------------------------------------------------
dnl disable the memory header ID code on SPARC
dnl --------------------------------------------------

case $host in
        *sparc*)
	  GCFLAGS="$GCFLAGS -DGS_USE_MEMORY_HEADER_ID=0"
        ;;
esac

dnl --------------------------------------------------
dnl set big/little endian for LCMS
dnl --------------------------------------------------
LCMS_ENDIAN=
LCMS2_ENDIAN=

if test x"$BIGENDIAN" != x"0"; then
  LCMS_ENDIAN="-DUSE_BIG_ENDIAN=$BIGENDIAN"
  LCMS2_ENDIAN="-DCMS_USE_BIG_ENDIAN=$BIGENDIAN"
fi

AC_SUBST(LCMS_ENDIAN)
AC_SUBST(LCMS2_ENDIAN)

dnl --------------------------------------------------
dnl set memory manager's ptr alignment
dnl --------------------------------------------------

ALIGN_TEXT_PROG_INCS="\
#include <stdio.h>
#include <stdlib.h>
"


ALIGN_TEXT_PROG="\
    struct altest
    {
      char a;
      int b;
      long c;
      long long d;
      float e;
      double f;
      void *g;
    };
    struct altest *a;
    struct altest d;
    char *b, *c, *lim;
    int ret = 0;
    c = b = malloc(64 * sizeof(struct altest));
    lim = b + (64 * sizeof(struct altest));
    do
    {
        b++;
        if ((b >= lim) ||
            ((((unsigned int)b) & 4) == 0 && (((unsigned int)b) & 8) != 0))
        {
          break;
        }
    } while(1);
    if (b >= lim)
    {
        ret = -1;
    }
    else
    {
        a = (struct altest *)b;
        b = a->g;
        ret = 0;
    }
    free(c);
    exit(ret);
"


GS_MEMPTR_ALIGNMENT=

AC_ARG_WITH(memory-alignment,
            [AS_HELP_STRING([--with-memory-alignment],
                               [Allows setting minimum alignment for the memory manager (4 or 8 bytes]))],
            [GS_MEMPTR_ALIGNMENT=$with_memory_alignment],[GS_MEMPTR_ALIGNMENT=check])

if test x"$GS_MEMPTR_ALIGNMENT" = xcheck ; then
  case $host in
    *hpux*|*-sun*|*-aix*|*-solaris*)
     GS_MEMPTR_ALIGNMENT=8
    ;;
    *)
    ;;
  esac
fi

if test x"$GS_MEMPTR_ALIGNMENT" = xcheck ; then
   AC_MSG_CHECKING([minimum memory pointer alignment])
   AC_RUN_IFELSE(
    [AC_LANG_PROGRAM([$ALIGN_TEXT_PROG_INCS], [
      $ALIGN_TEXT_PROG
    ])],
    [GS_MEMPTR_ALIGNMENT=4;AC_MSG_RESULT(done)],
    [GS_MEMPTR_ALIGNMENT=8;AC_MSG_RESULT(done)],
    [GS_MEMPTR_ALIGNMENT=4;AC_MSG_RESULT(cross compiling: assuming 4 byte - otherwise use "--with-memory-alignment=")])

fi

if test x"$GS_MEMPTR_ALIGNMENT" != x4 && test x"$GS_MEMPTR_ALIGNMENT" != x8 ; then
    AC_MSG_ERROR([4 and 8 are the only valid values for "--with-memory-alignment"], -1)
fi

if test x"$GS_MEMPTR_ALIGNMENT" != x4  || test x"$with_memory_alignment" != x ; then
  LCMS2_PTR_ALIGNMENT="-DCMS_PTR_ALIGNMENT=$GS_MEMPTR_ALIGNMENT"

  GCFLAGS="$GCFLAGS -DGS_MEMPTR_ALIGNMENT=$GS_MEMPTR_ALIGNMENT"
fi

AC_SUBST(LCMS2_PTR_ALIGNMENT)

dnl --------------------------------------------------
dnl decide if we have to "hack" sqrtf() for lcms2
dnl --------------------------------------------------

AC_CHECK_FUNC([sqrtf], [SQRTF_SUBST=], [SQRTF_SUBST="-Dsqrtf=\"(float)sqrt\""])

AC_SUBST(SQRTF_SUBST)

dnl --------------------------------------------------
dnl check for and possibly replace strnlen
dnl --------------------------------------------------
AC_CHECK_FUNCS([strnlen], [HAVE_STRNLEN=],[HAVE_STRNLEN="-D\"strnlen(a,b)=strlen(a)\""])

GCFLAGS="$GCFLAGS $HAVE_STRNLEN"


dnl --------------------------------------------------
dnl check for byte swap intrinsics
dnl --------------------------------------------------
AC_MSG_CHECKING([byteswap support])

HAVE_BSWAP32=""
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([], [
  int a = 0;
  int b = __builtin_bswap32(a);
  return(0);
  ])],
  [HAVE_BSWAP32="-DHAVE_BSWAP32"], [HAVE_BSWAP32=""])

AC_ARG_ENABLE([bswap32], AS_HELP_STRING([--disable-bswap32],
       [Do not use bswap32 instrinsic]), [
             if test "x$enable_bswap32" = xno; then
                HAVE_BSWAP32=""
             fi])

if test "x$HAVE_BSWAP32" != x; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

AC_SUBST(HAVE_BSWAP32)

dnl --------------------------------------------------
dnl check for byte swap header
dnl --------------------------------------------------
AC_MSG_CHECKING([for byteswap.h])

HAVE_BYTESWAP_H=""
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([#include "byteswap.h"], [
  int a = 0;
  int b = __builtin_bswap32(a);
  return(0);
  ])],
  [HAVE_BYTESWAP_H="-DHAVE_BYTESWAP_H"], [HAVE_BYTESWAP_H=""])

AC_ARG_ENABLE([byteswap-h], AS_HELP_STRING([--disable-byteswap-h],
       [Do not use byteswap.h functions]), [
             if test "x$enable_byteswap-h" = xno; then
                HAVE_BYTESWAP_H=""
             fi])

if test "x$HAVE_BYTESWAP_H" != x; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

AC_SUBST(HAVE_BYTESWAP_H)

# --------------------------------------------------
# Sun, BSD possibly other makes don't have quite the
# feature set of GNU make. We still prefer GNU make,
# but......
# --------------------------------------------------
AC_ARG_WITH([gnu-make],  AS_HELP_STRING([--without-gnu-make],
    [disable GNU make features]), gnu_make=no)

if test "x$gnu_make" != "xno"; then
  SUB_MAKE_OPTION="-f \$(MAKEFILE)"
  ORDER_ONLY="|"
else
  SUB_MAKE_OPTION=
  ORDER_ONLY=""
fi

AC_SUBST(ORDER_ONLY)
AC_SUBST(SUB_MAKE_OPTION)

# --------------------------------------------------
# annoying: Windows doesn't allow "aux" as a
# directory nor file name, so if we're building in
# mingw, add the same prefix as the VS build uses
# --------------------------------------------------
AUXDIRPOSTFIX=""
case $build in
    *-mingw*|*-msys*|*-cygwin*)
    AUXDIRPOSTFIX="_"
    ;;
esac
AC_SUBST(AUXDIRPOSTFIX)

# --------------------------------------------------
# Set executable name extention
# usually empty on Unix-like systems
# --------------------------------------------------
EXEEXT=""

case $host in
    *-mingw*|*-msys*)
      EXEEXT=".exe"
    ;;
    *)
    ;;
esac

AC_ARG_WITH([exe-ext],  AS_HELP_STRING([--with-exe-ext=EXT],
    [set the file name executable extension (must include any separator e.g. the period in ".exe")]),
    [EXEEXT="$with_exe_ext"])

AC_SUBST(EXEEXT)

# --------------------------------------------------
# Set executable name extention for aux binaries
# usually empty on Unix-like systems
# --------------------------------------------------
AUXEXEEXT=""

case $build in
    *-mingw*|*-msys*)
      AUXEXEEXT=".exe"
    ;;
    *)
    ;;
esac

AC_ARG_WITH([aux-exe-ext],  AS_HELP_STRING([--with-aux-exe-ext=EXT],
    [set the file name executable extension for auxiliary binaries (must include any separator e.g. the period in ".exe")]),
    [AUXEXEEXT="$with_aux_exe_ext"])

AC_SUBST(AUXEXEEXT)

# --------------------------------------------------
# Check for disabling of versioned path option.
# By default the versioned path must be enabled!
# Using this option is user's own risk & responsibility.
# --------------------------------------------------
AC_ARG_WITH([versioned-path],
[  --without-versioned-path
                          Do not use file paths containing the version of GS.

                 WARNING: This option is dangerous & unsupported, and
                          you take full responsibility for using it!],
versioned_path=no)

if test "x$versioned_path" != "xno"; then
  VERSIONED_PATH='/$(GS_DOT_VERSION)'
else
  VERSIONED_PATH=''
fi

AC_SUBST(VERSIONED_PATH)

# --------------------------------------------------
# Check if we are using GCC, and disable strict
# aliasing optimization if GCC supports it...
#
# NOTE: Strict aliasing can cause some parts
#       of Ghostscript to malfunction.
# --------------------------------------------------
if test $GCC = yes; then
  AC_MSG_CHECKING([whether to explicitly disable strict aliasing])
  CFLAGS_backup="$CFLAGS"
  CFLAGSAUX_backup="$CFLAGSAUX"
  CFLAGS="$CFLAGS -fno-strict-aliasing"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[
    AC_MSG_RESULT([yes])
    CFLAGS_backup="$CFLAGS"
    if test x"$host" = x"$build" ; then
      CFLAGSAUX_backup="$CFLAGSAUX -fno-strict-aliasing"
    fi
  ],[])
  CFLAGS="$CFLAGS_backup"
  CFLAGSAUX="$CFLAGSAUX_backup"
fi

# --------------------------------------------------
# Modify build for internal cluster testing.
# DO NOT USE THIS OPTION OUTSIDE OF ARTIFEX!
# --------------------------------------------------
AC_ARG_ENABLE([cluster])

if test "x"$enable_cluster != "x"; then
  CLUSTER_CFLAGS="-DCLUSTER"
else
  CLUSTER_CFLAGS=""
fi

AC_SUBST(CLUSTER_CFLAGS)

if test x"$build" != x"$host"; then

  ilog2()
  {
    ILOG2_VAL=$1
    ILOG2_RES=0
    while test $ILOG2_VAL -gt 1 ; do
      ILOG2_VAL=$((ILOG2_VAL + 1))
      ILOG2_VAL=$((ILOG2_VAL >> 1))
      ILOG2_RES=$((ILOG2_RES + 1))
    done
    return $ILOG2_RES
  };

# --------------------------------------------------
# Generate arch.h headers
# --------------------------------------------------
  AC_CHECK_ALIGNOF([short])
  AC_CHECK_ALIGNOF([int])
  AC_CHECK_ALIGNOF([long])
  AC_CHECK_ALIGNOF([size_t])
  AC_CHECK_ALIGNOF([void *])
  AC_CHECK_ALIGNOF([float])
  AC_CHECK_ALIGNOF([double])

  AC_SUBST(ARCH_ALIGN_SHORT_MOD, $ac_cv_alignof_short)
  AC_SUBST(ARCH_ALIGN_INT_MOD, $ac_cv_alignof_int)
  AC_SUBST(ARCH_ALIGN_LONG_MOD, $ac_cv_alignof_long)
  AC_SUBST(ARCH_ALIGN_SIZE_T_MOD, $ac_cv_alignof_size_t)
  AC_SUBST(ARCH_ALIGN_PTR_MOD, $ac_cv_alignof_void_p)
  AC_SUBST(ARCH_ALIGN_FLOAT_MOD, $ac_cv_alignof_float)
  AC_SUBST(ARCH_ALIGN_DOUBLE_MOD, $ac_cv_alignof_double)

  AC_CHECK_SIZEOF([char])
  AC_CHECK_SIZEOF([short])
  AC_CHECK_SIZEOF([int])
  AC_CHECK_SIZEOF([long])
  AC_CHECK_SIZEOF([long long])
  AC_CHECK_SIZEOF([size_t])
  AC_CHECK_SIZEOF([void *])
  AC_CHECK_SIZEOF([float])
  AC_CHECK_SIZEOF([double])

  ilog2 $ac_cv_sizeof_char
  AC_SUBST(ARCH_LOG2_SIZEOF_CHAR, $?)

  ilog2 $ac_cv_sizeof_short
  AC_SUBST(ARCH_LOG2_SIZEOF_SHORT, $?)

  ilog2 $ac_cv_sizeof_int
  AC_SUBST(ARCH_LOG2_SIZEOF_INT, $?)

  ilog2 $ac_cv_sizeof_long
  AC_SUBST(ARCH_LOG2_SIZEOF_LONG, $?)

  ilog2 $ac_cv_sizeof_long_long
  AC_SUBST(ARCH_LOG2_SIZEOF_LONG_LONG, $?)

  ilog2 $ac_cv_sizeof_size_t
  AC_SUBST(ARCH_LOG2_SIZEOF_SIZE_T, $?)
  AC_SUBST(ARCH_SIZEOF_SIZE_T, $ac_cv_sizeof_size_t)

  AC_SUBST(ARCH_SIZEOF_PTR, $ac_cv_sizeof_void_p)
  ilog2 $ac_cv_sizeof_void_p
  AC_SUBST(ARCH_LOG2_SIZEOF_PTR, $?)

  AC_SUBST(ARCH_SIZEOF_FLOAT, $ac_cv_sizeof_float)
  ilog2 $ac_cv_sizeof_float
  AC_SUBST(ARCH_LOG2_SIZEOF_FLOAT, $?)

  AC_SUBST(ARCH_SIZEOF_DOUBLE, $ac_cv_sizeof_double)
  ilog2 $ac_cv_sizeof_double
  AC_SUBST(ARCH_LOG2_SIZEOF_DOUBLE, $?)

  AC_SUBST(ARCH_IS_BIG_ENDIAN, $BIGENDIAN)

  AC_MSG_CHECKING([if pointers are signed])
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([#include <stdint.h>], [
      char b[[(char *)(uintptr_t)-1 < (char *)(uintptr_t)0 ? -1 : 0]];
      return sizeof(b);
    ])],
    [ARCH_PTRS_ARE_SIGNED=0; AC_MSG_RESULT(no)], [ARCH_PTRS_ARE_SIGNED=1; AC_MSG_RESULT(yes)])
  AC_SUBST(ARCH_PTRS_ARE_SIGNED)

  AC_MSG_CHECKING([if dividing negative by positive truncates])
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([], [
      char b[[-1 / 2 == 0 ? -1 : 0]];
      return sizeof(b);
    ])],
    [ARCH_DIV_NEG_POS_TRUNCATES=0; AC_MSG_RESULT(no)], [ARCH_DIV_NEG_POS_TRUNCATES=1; AC_MSG_RESULT(yes)])
  AC_SUBST(ARCH_DIV_NEG_POS_TRUNCATES)

  AC_MSG_CHECKING([if arithmetic shift works properly])
  ARCH_ARITH_RSHIFT=""
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([], [
      char b[[(-1L >> 2) != -1 || (-1 >> 1) != -1 || (-1 >> 2) != -1 ? 0 : -1]];
      return sizeof(b);
    ])],
    [ARCH_ARITH_RSHIFT=0; AC_MSG_RESULT(no)])

  if test "x${ARCH_ARITH_RSHIFT}" = "x"; then
    # Turbo C problem
    AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([], [
        char b[[(-1L >> 2) != -1 ? 0 : -1]];
        return sizeof(b);
      ])],
      [ARCH_ARITH_RSHIFT=1; AC_MSG_RESULT(no)])
  fi

  if test "x${ARCH_ARITH_RSHIFT}" = "x"; then
    AC_MSG_RESULT(yes)
    ARCH_ARITH_RSHIFT=2
  fi
  AC_SUBST(ARCH_ARITH_RSHIFT)

  AC_SUBST(ARCH_SIZEOF_GX_COLOR_INDEX)
  CONFIG_FILES_LIST="$CONFIG_FILES_LIST $ARCH_AUTOCONF_HEADER:$ARCH_AUTOCONF_HEADER_PROTO"
fi

dnl --------------------------------------------------
dnl AUX tool options
dnl --------------------------------------------------
AC_ARG_ENABLE([mkromfs-quiet], AS_HELP_STRING([--enable-mkromfs-quiet],
       [Do not emit mkromfs verbose output]), [MKROMFS_FLAGS="-q $MKROMFS_FLAGS"])

AC_SUBST(MKROMFS_FLAGS)


if test x"$GCFLAGSAUX" = x"\$(GCFLAGS)" ; then
    GCFLAGSAUX="$GCFLAGS"
fi

CFLAGSAUX=$(echo $CFLAGSAUX | sed -e 's/-DGS_NO_FILESYSTEM//g')

dnl --------------------------------------------------
dnl Do substitutions
dnl --------------------------------------------------
SRCDIR="$srcdir"
AC_SUBST(SRCDIR)
AC_SUBST(OPT_CFLAGS)
AC_SUBST(DBG_CFLAGS)
AC_SUBST(GCFLAGS)
AC_SUBST(CFLAGS_SANITIZE)
AC_SUBST(STRIP_XE)
AC_SUBST(JPX_SSE_CFLAGS)

AC_SUBST(CCAUX)
AC_SUBST(CFLAGSAUX)
AC_SUBST(GCFLAGSAUX)
AC_SUBST(CAPOPTAUX)
AC_SUBST(CFLAGSAUX_STANDARD)
AC_SUBST(CFLAGSAUX_DEBUG)
AC_SUBST(CFLAGSAUX_PROFILE)
AC_SUBST(LDFLAGSAUX)
AC_SUBST(AUXEXTRALIBS)
AC_SUBST(ARCH_AUTOCONF_HEADER)
AC_SUBST(CFLAGS_VISIBILITY)

CONFIG_FILES_LIST="$CONFIG_FILES_LIST $THEMAKEFILE"

if test x"$THEMAKEFILE" != x"$MAKEFILE" && test x"$build" = x"$host"; then
  CONFIG_FILES_LIST="$CONFIG_FILES_LIST"
fi

if test "x$AFS" = "x1"; then
   AC_MSG_WARN([Using "native" font scaler which is now deprecated (rather than freetype),])
   AC_MSG_WARN([Support for this will be removed in a future release])
fi

if test "x$NTS_EXCLUDES" != "x" ; then
   AC_MSG_WARN([Due to the --enable-threadsafe option, the following devices have been excluded because they are not threadsafe:])
   echo "$NTS_EXCLUDES"
fi

if test x"$OCR_DEVS_WARNING_LINE1" != x"" ; then
    echo ""
    AC_MSG_WARN([$OCR_DEVS_WARNING_LINE1])
    if test x"$OCR_DEVS_WARNING_LINE2" != x"" ; then
      AC_MSG_WARN([$OCR_DEVS_WARNING_LINE2])
    fi
    echo ""
fi

AC_CONFIG_FILES($CONFIG_FILES_LIST)

AC_OUTPUT
