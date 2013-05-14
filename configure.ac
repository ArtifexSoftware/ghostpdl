# Copyright (C) 2001-2012 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
#
# Refer to licensing information at http://www.artifex.com or contact
# Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
# CA  94903, U.S.A., +1(415)492-9861, for further information.
#


# Process this file with autoconf to produce a configure script

# ------------------------------------------------
# Initialization and Versioning
# ------------------------------------------------

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.  This
# file is a modified template produced by autoscan with additions
# derived from ghostscript's configure.ac.
#

AC_PREREQ([2.61])
AC_INIT
AC_LANG(C)
AC_CONFIG_SRCDIR([pcl/pcjob.c])

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_RESOLV
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h inttypes.h langinfo.h libintl.h limits.h locale.h malloc.h memory.h netdb.h netinet/in.h nl_types.h stddef.h stdint.h stdlib.h string.h strings.h sys/ioctl.h sys/socket.h sys/time.h unistd.h inttypes.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STRTOD

AC_CHECK_FUNCS([alarm dup2 floor ftruncate gethostbyaddr gethostbyname gethostname getpass gettimeofday localeconv memchr memmove memset mkdir modf nl_langinfo pow putenv regcomp select setlocale socket sqrt strcasecmp strchr strdup strerror strncasecmp strpbrk strrchr strspn strstr strtol strtoul uname])

AC_CHECK_FUNCS([mkstemp], [HAVE_MKSTEMP=-DHAVE_MKSTEMP=1])
AC_SUBST(HAVE_MKSTEMP)

AC_CHECK_FUNCS([fopen64], [HAVE_FILE64=-DHAVE_FILE64])
AC_SUBST(HAVE_FILE64)

AC_CHECK_FUNCS([fseeko], [HAVE_FSEEKO=-DHAVE_FSEEKO])
AC_SUBST(HAVE_FSEEKO)

AC_CHECK_FUNCS([mkstemp64], [HAVE_MKSTEMP64=-DHAVE_MKSTEMP64])
AC_SUBST(HAVE_MKSTEMP64)

AC_CHECK_FUNCS([setlocale], [HAVE_SETLOCALE=-DHAVE_SETLOCALE])
AC_SUBST(HAVE_SETLOCALE)

if test "x$ac_cv_header_inttypes_h" = xyes; then
  HAVE_INTTYPES="-DHAVE_INTTYPES_H=1"
fi

AC_SUBST(HAVE_INTTYPES)

dnl --------------------------------------------------
dnl Set options that we want to pass into all other
dnl configure scripts we might call
dnl --------------------------------------------------

SUBCONFIG_OPTS=""

if test x$build_alias != x; then
SUBCONFIG_OPTS="$SUBCONFIG_OPTS --build=$build_alias"
fi


dnl --------------------------------------------------
dnl Run subsidiary configure scripts
dnl --------------------------------------------------
LIBTIFFDIR=./gs/tiff
LIBTIFFCONFDIR=./tiff-config


echo
echo "Running libtiff configure script..."
olddir=`pwd`
if ! test -d $LIBTIFFCONFDIR ; then
  mkdir $LIBTIFFCONFDIR
fi

cd $LIBTIFFCONFDIR && $olddir/$LIBTIFFDIR/configure --disable-jbig --disable-lzma $SUBCONFIG_OPTS
status=$?
if test "$status" -ne 0 ; then
  AC_MSG_ERROR([libtiff configure script failed], $status)
fi
cd $olddir
echo
echo "Continuing with GhostPDL configuration..."


dnl --------------------------------------------------
dnl Should we use FAPI, and which server(s)
dnl --------------------------------------------------

FT_BRIDGE=0
UFST_BRIDGE=
UFST_ROOT=
UFST_CFLAGS=
UFST_LIB_EXT=
FAPIUFST_MAK="\$(GLSRCDIR)/stub.mak"

AC_ARG_ENABLE([fapi], AC_HELP_STRING([--disable-fapi],
       [Force use of the (deprecated) Artifex font scaler/renderer]))

if test x"$enable_fapi" != xno; then
  FT_BRIDGE=1
  AC_ARG_ENABLE([freetype], AC_HELP_STRING([--disable-freetype],
        [Disable freetype for font rasterization]))

  if test x"$enable_freetype" != xno; then
    if ! test -f gs/freetype/src/base/ftbbox.c; then
      FT_BRIDGE=0
    fi
  else
    FT_BRIDGE=0
  fi
  
  dnl UFST detection
  AC_ARG_WITH([ufst], AC_HELP_STRING([--with-ufst=UFST_ROOT_DIR],
             [Use UFST]), [], [with_ufst=no])

  if test x"$with_ufst" != xno; then
    if test -d $with_ufst; then
      UFST_BRIDGE=1
      case $with_ufst in
        /*) UFST_ROOT=$with_ufst ;;
         *) UFST_ROOT=`pwd`/$with_ufst ;;
      esac

      FAPIUFST_MAK="\$(UFST_ROOT)/fapiufst.mak"
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
fi


AC_SUBST(UFST_BRIDGE)
AC_SUBST(UFST_ROOT)
AC_SUBST(UFST_CFLAGS)
AC_SUBST(UFST_LIB_EXT)
AC_SUBST(FAPIUFST_MAK)

AC_SUBST(FT_BRIDGE)



dnl --------------------------------------------------
dnl check for big/little endian for LCMS
dnl --------------------------------------------------

BIGENDIAN=

AC_MSG_CHECKING([for big endian])

AC_ARG_ENABLE([big-endian], AC_HELP_STRING([--enable-big-endian],
    [Force big endian]), [BIGENDIAN=1])

if test x$BIGENDIAN = x; then
  AC_ARG_ENABLE([little-endian], AC_HELP_STRING([--enable-little-endian],
      [Force little endian]), [BIGENDIAN=0])
fi

if test x$BIGENDIAN = x ; then
  AC_RUN_IFELSE(
    [AC_LANG_PROGRAM([#include <stdio.h>], [
     static const int one = 1;
     return (*(char*)&one == 0 ? 0 : 1);
    ])],
    [BIGENDIAN=1;AC_MSG_RESULT(big)],
    [BIGENDIAN=0;AC_MSG_RESULT(little)])
else
  if test x$BIGENDIAN = x1 ; then
    AC_MSG_RESULT(big)
  else
    AC_MSG_RESULT(little)
  fi
fi

if test x$BIGENDIAN = x1 ; then
  LCMS_ENDIAN="-DUSE_BIG_ENDIAN=$BIGENDIAN"
  LCMS2_ENDIAN="-DCMS_USE_BIG_ENDIAN=$BIGENDIAN"
else
  LCMS_ENDIAN=
  LCMS2_ENDIAN=
fi

AC_SUBST(LCMS_ENDIAN)
AC_SUBST(LCMS2_ENDIAN)

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

AC_ARG_ENABLE([sse2], AC_HELP_STRING([--disable-sse2],
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

AC_ARG_ENABLE([bswap32], AC_HELP_STRING([--disable-bswap32],
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

AC_ARG_ENABLE([byteswap-h], AC_HELP_STRING([--disable-byteswap-h],
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

dnl --------------------------------------------------
dnl Check for libraries and x11 includes/libraries
dnl --------------------------------------------------

SYNC="nosync"
PTHREAD_LIBS=""
AC_CHECK_LIB(pthread, pthread_create, [
  SYNC=posync;
  PTHREAD_LIBS="-lpthread"
])
AC_SUBST(SYNC)
AC_SUBST(PTHREAD_LIBS)

AC_PATH_X

XINCLUDE=""
XLIBDIRS=""
XLIBDIR=""
XLIBS=""
X11DEVS=""

if test -z "$no_x"; then
  # workaround autoconf bug which used to depend on xmkmf to set the
  # $x_includes directory.  Danger quadrigraph ahead!
  if test ! -z "$x_libraries && test -z "$x_includes; then
     includedir="include"
     x_includes=$(echo $x_libraries | sed 's|@<:@^/@:>@*$||')
     x_includes="$x_includes$includedir"
  fi
  if test ! -z "$x_includes"; then
     XINCLUDE=-I$x_includes
  fi
  if test ! -z "$x_libraries"; then
    XLIBDIRS=-L$x_libraries
    XLIBDIR=-L$x_libraries
  fi
  XLIBS="Xt SM ICE Xext X11"
  X11DEVS='$(DD)x11.dev $(DD)x11alpha.dev $(DD)x11mono.dev $(DD)x11cmyk.dev'
fi

if test "x$ac_cv_header_sys_time_h" = xyes; then
    HAVE_SYS_TIME_H="-DHAVE_SYS_TIME_H=1"
fi

AC_SUBST(HAVE_SYS_TIME_H)

AC_SUBST(XINCLUDE)
AC_SUBST(XLIBDIRS)
AC_SUBST(XLIBDIR)
AC_SUBST(XLIBS)
AC_SUBST(X11DEVS)
AC_OUTPUT(config.mak)
