dnl Copyright (C) 2001-2012 Artifex Software, Inc.
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
dnl Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
dnl CA  94903, U.S.A., +1(415)492-9861, for further information.


dnl Process this file with autoconf to produce a configure script

dnl ------------------------------------------------
dnl Initialization and Versioning
dnl ------------------------------------------------

AC_INIT
AC_PREREQ(2.52)
AC_LANG(C)
AC_CONFIG_SRCDIR(psi/gs.c)

dnl Inherit compiler flags from the environment...
CFLAGS="${CFLAGS:=}"
CPPFLAGS="${CPPFLAGS:=}"
CXXFLAGS="${CXXFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"

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
save_cflags=$CFLAGS
AC_PROG_CC
AC_PROG_CPP
CFLAGS=$save_cflags

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

dnl pkg-config is used for several tests now...
AC_PATH_PROG(PKGCONFIG, pkg-config)

dnl --------------------------------------------------  
dnl Allow excluding the contributed drivers
dnl --------------------------------------------------  

AC_ARG_ENABLE([contrib], AC_HELP_STRING([--disable-contrib],
    [Do not include contributed drivers]))
CONTRIBINCLUDE="include $srcdir/contrib/contrib.mak"
INSTALL_CONTRIB="install-contrib-extras"

case `uname` in
    MINGW*)
      AC_MSG_WARN([disabling contrib devices])
      enable_contrib=no
    ;;
    *)
#     This is just an arbitrary file in contrib to check
      if ! test -f $srcdir/contrib/gdevbjc_.c; then
        enable_contrib=no
      fi
    ;;
esac

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

#AC_CANONICAL_HOST

CC_OPT_FLAGS_TO_TRY="-O"
SET_DT_SONAME="-soname="

case `uname` in
        Linux*|GNU*)
        if test $ac_cv_prog_gcc = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
        fi
        ;;
        *BSD)
        if test $ac_cv_prog_gcc = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
        fi
        ;;
        Darwin*)
        if test $ac_cv_prog_gcc = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
        fi
        SET_DT_SONAME=""
        ;;
        SunOS)
        CC_OPT_FLAGS_TO_TRY="-O2"
        # the trailing space is required!
        if test $ac_cv_prog_gcc = no; then
            SET_DT_SONAME="-h "
        fi
        if test $ac_cv_prog_gcc = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
        else
            CC_DBG_FLAGS_TO_TRY="-g -O0"
        fi
        ;;
        AIX)
        if test $ac_cv_prog_gcc = yes; then
            CC_OPT_FLAGS_TO_TRY="-O2"
            CC_DBG_FLAGS_TO_TRY="-gdwarf-2 -g3 -O0"
            SET_DT_SONAME="so"
        fi
        ;;
esac

AC_SUBST(SET_DT_SONAME)


if test $ac_cv_prog_gcc = yes; then
    cflags_to_try="-Wall -Wstrict-prototypes -Wundef \
-Wmissing-declarations -Wmissing-prototypes -Wwrite-strings \
-Wno-strict-aliasing -Wdeclaration-after-statement \
-fno-builtin -fno-common"
    optflags_to_try="$CC_OPT_FLAGS_TO_TRY"
    dbgflags_to_try="$CC_DBG_FLAGS_TO_TRY"
else
    cflags_to_try=
    optflags_to_try="$CC_OPT_FLAGS_TO_TRY"
    dbgflags_to_try="$CC_DBG_FLAGS_TO_TRY"
fi

ARCH_CONF_HEADER=

case `uname` in
        Darwin*)
          ARCH_CONF_HEADER="\$(GLSRCDIR)/../arch/osx-x86-x86_64-ppc-gcc.h"
        ;;
        *)
          ARCH_CONF_HEADER=
        ;;
esac

AC_SUBST(ARCH_CONF_HEADER)

# debug configurarion is available by default with "make debug"
#AC_ARG_ENABLE([debug], AC_HELP_STRING([--enable-debug],
#    [turn on debugging]))
#if test x$enable_debug = xyes; then
#    optflags_to_try="-g"
#    CFLAGS="-DDEBUG $CFLAGS"
#fi

AC_MSG_CHECKING([supported compiler flags])
old_cflags=$CFLAGS
echo
for flag in $optflags_to_try; do
    CFLAGS="$CFLAGS $flag"
    AC_TRY_COMPILE(, [return 0;], [
        echo "   $flag"
        OPT_CFLAGS="$OPT_CFLAGS $flag"
    ])
    CFLAGS=$old_cflags
done
for flag in $cflags_to_try; do
        CFLAGS="$CFLAGS $flag"
        AC_TRY_COMPILE(, [return 0;], [
                echo "   $flag"
                GCFLAGS="$GCFLAGS $flag"
        ])
        CFLAGS=$old_cflags
done
old_cflags=$CFLAGS
echo
for flag in $dbgflags_to_try; do
    CFLAGS="$CFLAGS $flag"
    AC_TRY_COMPILE(, [return 0;], [
        echo "   $flag"
        DBG_CFLAGS="$DBG_CFLAGS $flag"
    ])
    CFLAGS=$old_cflags
done

AC_MSG_RESULT([ ...done.])

dnl ----------------------------
dnl check for big/little endian
dnl ----------------------------

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
dnl Enable thread safe build
dnl --------------------------------------------------
AC_ARG_ENABLE([threadsafe], AC_HELP_STRING([--enable-threadsafe],
    [enable a thread safe build - NOTE: this will exclude non-threadsafe devices]))
if test x$enable_threadsafe = xyes ; then
    CFLAGS="-DGS_THREADSAFE"
fi

dnl --------------------------------------------------
dnl Check for headers
dnl --------------------------------------------------

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([errno.h fcntl.h limits.h malloc.h memory.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/time.h sys/times.h syslog.h unistd.h dirent.h ndir.h sys/dir.h sys/ndir.h inttypes.h])

dnl --------------------------------------------------
dnl Check for *BSD and apply BSD Make workaround
dnl - BSD Make treats obj special and cd into it first.
dnl --------------------------------------------------

OBJDIR_BSDMAKE_WORKAROUND=obj

case `uname` in
        *BSD)
        OBJDIR_BSDMAKEWORKAOROUND="notobj"
        ;;
esac
AC_SUBST(OBJDIR_BSDMAKE_WORKAROUND)

# for gdev3b1.c (AT&T terminal interface)
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
AC_HEADER_TIME
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
if test "x$ac_cv_header_stdint_h" = xyes; then
        GCFLAGS="$GCFLAGS -DHAVE_STDINT_H=1"
fi

if test "x$ac_cv_header_dirent_h" = xyes; then
        GCFLAGS="$GCFLAGS -DHAVE_DIRENT_H=1"
fi

if test "x$ac_cv_header_ndir_h" = xyes; then
        GCFLAGS="$GCFLAGS -DHAVE_NDIR_H=1"
fi

if test "x$ac_cv_header_sys_dir_h" = xyes; then
        GCFLAGS="$GCFLAGS -DHAVE_SYS_DIR_H=1"
fi

if test "x$ac_cv_header_sys_ndir_h" = xyes; then
        GCFLAGS="$GCFLAGS -DHAVE_SYS_NDIR_H=1"
fi

if test "x$ac_cv_header_sys_time_h" = xyes; then
        GCFLAGS="$GCFLAGS -DHAVE_SYS_TIME_H=1"
fi

if test "x$ac_cv_header_sys_times_h" = xyes; then
        GCFLAGS="$GCFLAGS -DHAVE_SYS_TIMES_H=1"
fi

if test "x$ac_cv_header_inttypes_h" = xyes; then
        GCFLAGS="$GCFLAGS -DHAVE_INTTYPES_H=1"
fi

large_color_index=1

AC_ARG_WITH([large_color_index],, large_color_index="$with_large_color_index")

if test x"$large_color_index" != "x0"; then
  dnl try to find a 64 bit type for devicen color index
  color_ind_type="none"
   AC_CHECK_SIZEOF(unsigned long int)
   if test $ac_cv_sizeof_unsigned_long_int = 8; then
          color_ind_type="unsigned long int"
   else
    AC_CHECK_SIZEOF(unsigned long long)
    if test $ac_cv_sizeof_unsigned_long_long = 8; then
          color_ind_type="unsigned long long"
    else
     AC_CHECK_SIZEOF(unsigned __int64)
     if test $ac_cv_sizeof_unsigned___int64 = 8; then
          color_ind_type="unsigned __int64"
     else
      AC_CHECK_SIZEOF(u_int64_t)
      if test $ac_cv_sizeof_u_int64_t = 8; then
          color_ind_type="u_int64_t"
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
   else
    AC_CHECK_SIZEOF(unsigned int)
    if test $ac_cv_sizeof_unsigned_int = 4; then
          color_ind_type="unsigned int"
    else
     AC_CHECK_SIZEOF(unsigned __int32)
     if test $ac_cv_sizeof_unsigned___int32 = 4; then
          color_ind_type="unsigned __int32"
     else
      AC_CHECK_SIZEOF(u_int32_t)
      if test $ac_cv_sizeof_u_int32_t = 4; then
          color_ind_type="u_int32_t"
      fi
     fi
    fi
   fi
fi

dnl if a suitable type wasn't found above, we fall back to
dnl a default in genarch.
if test "$color_ind_type" != "none"; then
        GCFLAGS="$GCFLAGS -DGX_COLOR_INDEX_TYPE=\"$color_ind_type\""
fi

dnl --------------------------------------------------
dnl Set options that we want to pass into all other
dnl configure scripts we might call
dnl --------------------------------------------------

SUBCONFIG_OPTS=""

if test x$build_alias != x; then
SUBCONFIG_OPTS="$SUBCONFIG_OPTS --build=$build_alias"
fi


dnl --------------------------------------------------
dnl Check for libraries
dnl --------------------------------------------------

AC_CHECK_LIB(m, cos)

SYNC="nosync"
PTHREAD_LIBS=""

case `uname` in
    MINGW*)
      AC_MSG_WARN([disabling support for pthreads......])
    ;;
    *)
      AC_CHECK_LIB(pthread, pthread_create, [
        SYNC=posync;
        PTHREAD_LIBS="-lpthread"
      ])
    ;;
esac

AC_SUBST(SYNC)
AC_SUBST(PTHREAD_LIBS)

AC_MSG_CHECKING([for local trio library source])
if test -f $srcdir/trio/trio.h; then
        AC_MSG_RESULT([trio])
        SHARE_TRIO=0
        TRIODIR=$srcdir/trio
else
# This is how it should be done, but link fails.
# But link libtrio to gs, and it works - answers on
# a postcard, please......
#        AC_MSG_RESULT([no])
#        AC_CHECK_LIB(trio, trio_vsprintf, [
#          AC_CHECK_HEADERS([trio.h], [SHARE_TRIO=1])
#        ],[])

# So, for now, use a "standard" function instead - link
# will still fail if the libtrio is not found.
        AC_MSG_RESULT([no])
        AC_CHECK_LIB(trio, malloc, [
          AC_CHECK_HEADERS([trio.h], [SHARE_TRIO=1])
        ],[])
fi
if test -z "$SHARE_TRIO"; then
  AC_MSG_ERROR([I wasn't able to find a copy
  of the trio library. This is required for compiling
  ghostscript.
  ])
fi
AC_SUBST(SHARE_TRIO)
AC_SUBST(TRIODIR)

dnl Tests for iconv (Needed for OpenPrinting Vector, "opvp" output device)
AC_ARG_WITH(libiconv,
            [AC_HELP_STRING([--with-libiconv=@<:@no/gnu/native@:>@],
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
            [AC_HELP_STRING([--without-libidn],
                               [Do not use libidn to support Unicode passwords])],,
            [with_libidn=maybe])
if test x$with_libidn != xno; then
  AC_CHECK_LIB(idn, stringprep, [
    with_libidn=no
    AC_CHECK_HEADER([stringprep.h], [with_libidn=yes])
    ], [
    if test x$with_libidn != xmaybe; then
      AC_MSG_ERROR([libidn not found])
    fi
    with_libidn=no
  ])
fi
HAVE_LIBIDN=''
UTF8DEVS=''
if test x$with_libidn != xno; then
  HAVE_LIBIDN=-DHAVE_LIBIDN
  LIBS="$LIBS -lidn"

  if test x$found_iconv != xno; then
    UTF8DEVS='$(PSD)utf8.dev'
  fi
fi
AC_SUBST(HAVE_LIBIDN)
AC_SUBST(UTF8DEVS)

dnl Tests for libpaper (to determine system default paper size)
AC_ARG_WITH([libpaper],
            AC_HELP_STRING([--without-libpaper],
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
AC_ARG_ENABLE([fontconfig], AC_HELP_STRING([--disable-fontconfig],
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
AC_ARG_ENABLE([dbus], AC_HELP_STRING([--disable-dbus],
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

AC_CHECK_LIB(dl, dlopen)

AC_ARG_ENABLE([freetype], AC_HELP_STRING([--disable-freetype],
        [Disable freetype for font rasterization]))

FT_BRIDGE=0
SHARE_FT=0
FTSRCDIR=
FT_CFLAGS=
FT_LIBS=

INSERT_UFST_BRIDGE_EQUAL_ONE=
UFST_ROOT=
UFST_CFLAGS=
UFST_LIB_EXT=
FAPIUFST_MAK="\$(GLSRCDIR)/stub.mak"

AC_ARG_ENABLE([fapi], AC_HELP_STRING([--disable-fapi],
       [Force use of the (deprecated) Artifex font scaler/renderer]))

if test x"$enable_fapi" != xno; then

  dnl UFST detection
  AC_ARG_WITH([ufst], AC_HELP_STRING([--with-ufst=UFST_ROOT_DIR],
                                   [Use UFST]),
            [], [with_ufst=no])

  if test x"$with_ufst" != xno; then
    if test -d $with_ufst; then
      INSERT_UFST_BRIDGE_EQUAL_ONE="UFST_BRIDGE=1"
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

    if test -z $FTSRCDIR; then
      AC_MSG_RESULT([no])
      if test "x$PKGCONFIG" != x; then
        AC_MSG_CHECKING(for system freetype2 >= 2.4.2 with pkg-config)
            # pkg-config needs the libtool version, which != the freetype2 version <sigh!>
            # There is a table of corresponding ft2<->libtool numbers in freetype/docs/VERSION.DLL
            if $PKGCONFIG --atleast-version=12.0.6 freetype2; then
                AC_MSG_RESULT(yes)
                FT_CFLAGS="$CFLAGS `$PKGCONFIG --cflags freetype2`"
                FT_LIBS="`$PKGCONFIG --libs freetype2`"
                FT_BRIDGE=1
                SHARE_FT=1
            else
                AC_MSG_RESULT(no)
                AC_MSG_WARN([freetype library source not found...using native rasterizer])
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

AC_MSG_CHECKING([for local zlib source])
dnl zlib is needed for language level 3, and libpng
# we must define ZLIBDIR regardless because png.mak does a -I$(ZLIBDIR)
# this seems a harmless default
ZLIBDIR=src
AUX_SHARED_ZLIB=

if test -d $srcdir/zlib; then
        AC_MSG_RESULT([yes])
        SHARE_ZLIB=0
        ZLIBDIR=$srcdir/zlib
else
        AC_MSG_RESULT([no])
        AC_CHECK_LIB(z, deflate, [
          AC_CHECK_HEADERS(zlib.h, [SHARE_ZLIB=1; AUX_SHARED_ZLIB="-l\$(ZLIB_NAME)"])
        ])
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
AC_SUBST(ZLIBDIR)
AC_SUBST(FT_SYS_ZLIB)

dnl png for the png output device; it also requires zlib
LIBPNGDIR=src
PNGDEVS=''
PNGDEVS_ALL='png48 png16m pnggray pngmono png256 png16 pngalpha'
AC_MSG_CHECKING([for local png library source])
if test -f $srcdir/libpng/pngread.c; then
        AC_MSG_RESULT([yes])
        SHARE_LIBPNG=0
        LIBPNGDIR=$srcdir/libpng
        PNGDEVS="$PNGDEVS_ALL"
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
AC_SUBST(SHARE_LIBPNG)
AC_SUBST(LIBPNGDIR)
#AC_SUBST(PNGDEVS)

WHICHLCMS=

AC_ARG_WITH([lcms], AC_HELP_STRING([--with-lcms],
    [try to use LittleCMS 1.x instead of the default of LittleCMS 2.x]))

if test x$with_lcms != xyes; then
  AC_MSG_CHECKING([for local lcms2 library source])
  LCMS2DIR=$srcdir/lcms2
  if test -f $LCMS2DIR/include/lcms2.h; then
        AC_MSG_RESULT([yes])
        SHARELCMS=0
        WHICHLCMS=lcms2
  else
    AC_MSG_RESULT([no])
    AC_MSG_CHECKING([for system lcms2 library])
    AC_CHECK_LIB(lcms2, cmsGetTransformOutputFormat, [
           AC_CHECK_HEADERS([lcms2.h], [LCMS2DIR="";SHARELCMS=1;WHICHLCMS=lcms2])
         ])
    if test x$WHICHLCMS = x; then
      AC_MSG_WARN([lcms2 not found, or too old])
    fi
  fi
fi

if test x$WHICHLCMS = x; then
  AC_MSG_CHECKING([for local lcms library source])
  LCMSDIR=$srcdir/lcms
  SHARELCMS=0
  if test -f $LCMSDIR/include/lcms.h; then
        AC_MSG_RESULT([yes])
        SHARELCMS=0
        WHICHLCMS=lcms
  else

    AC_MSG_ERROR([LittleCMS source not found!])
    #AC_CHECK_LIB(lcms, cmsCreateXYZProfile, [
    #          AC_CHECK_HEADERS([lcms.h], [SHARELCMS=1;LCMSDIR=""])
    #        ])
  fi
fi

AC_SUBST(SHARELCMS)
AC_SUBST(WHICHLCMS)
AC_SUBST(LCMSDIR)
AC_SUBST(LCMS2DIR)

dnl look for libtiff, it also requires lib
dnl png for the png output device; it also requires zlib
AC_ARG_WITH([system-libtiff], AC_HELP_STRING([--with-system-libtiff],
                                             [Force using the systems libtiff]),
            [], [with_system_libtiff=check])
TIFFDEVS=''
FAX_DEVS=''

LIBTIFFDIR=''
LIBTIFFCONFDIR=''

TIFFDEVS_ALL='tiffs tiff12nc tiff24nc tiff48nc tiff32nc tiff64nc tiffcrle tifflzw tiffpack tiffgray tiffsep tiffsep1 tiffscaled'
FAX_DEVS_ALL='cfax dfaxlow dfaxhigh fax faxg3 faxg32d faxg4 tiffg3 tiffg32d tiffg4 tfax'
case "x$with_system_libtiff" in
    xcheck)
        if test -d $srcdir/tiff; then
            LIBTIFFDIR=$srcdir/tiff
            LIBTIFFCONFDIR=tiff-config
            HAVE_LOCAL_LIBTIFF=1
            SHARE_LIBTIFF=0
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
            AC_MSG_NOTICE([Could not find a copy of libtiff on your system.
Disabling tiff output devices.])
        else
            TIFFDEVS="$TIFFDEVS_ALL"
            FAX_DEVS="$FAX_DEVS_ALL"
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
       else
            AC_MSG_NOTICE([Could not find a copy of libtiff on your system.
Disabling tiff output devices.])
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
        else
            AC_MSG_RESULT([no])
            AC_MSG_NOTICE([Could not find local copy of libtiff.
Disabling tiff output devices.])
        fi
        ;;
esac

if test $SHARE_LIBTIFF -eq 0; then
      echo
      echo "Running libtiff configure script..."
      olddir=`pwd`
      if ! test -d $LIBTIFFCONFDIR ; then
        mkdir $LIBTIFFCONFDIR
      fi
      cd "$LIBTIFFCONFDIR" && $olddir/$LIBTIFFDIR/configure --disable-jbig --disable-lzma $SUBCONFIG_OPTS
      status=$?
      if test "$status" -ne 0 ; then
        AC_MSG_ERROR([libtiff configure script failed], $status)
      fi
      cd "$olddir"
      echo
      echo "Continuing with Ghostscript configuration..."
fi

AC_SUBST(SHARE_LIBTIFF)
AC_SUBST(LIBTIFFDIR)
AC_SUBST(LIBTIFFCONFDIR)


dnl look for CUPS...
AC_ARG_ENABLE([cups], AC_HELP_STRING([--disable-cups],
    [Do not include CUPS support]))

AC_ARG_WITH([pdftoraster], AC_HELP_STRING([--without-pdftoraster],
    [Do not include CUPS' pdftoraster filter]))

AC_ARG_WITH([local-cups], AC_HELP_STRING([--with-local-cups],
                                             [Force using the GS supplied cups code - only useful for debugging]),
            [with_local_cups=yes], [with_local_cups=no])

AC_ARG_WITH([cups-serverbin],  AC_HELP_STRING([--with-cups-serverbin],
    [override the "cups-config --serverbin" path]), CUPS_SERVERBIN="$withval", CUPS_SERVERBIN="")

AC_ARG_WITH([cups-serverroot],  AC_HELP_STRING([--with-cups-serverroot],
    [override the "cups-config --serverroot" path]), CUPS_SERVERROOT="$withval", CUPS_SERVERROOT="")


AC_ARG_WITH([cups-datadir],  AC_HELP_STRING([--with-cups-datadir],
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

SHARELCUPS=1
SHARELCUPSI=1

if ( test -d $srcdir/cups ); then
    CUPS_DIR="$srcdir/cups"
    if test x$enable_cups != xno; then
        if test x$with_local_cups != xyes; then
            AC_PATH_PROG(CUPSCONFIG,cups-config)
            AC_CHECK_HEADER([cups/raster.h],[],[CUPSCONFIG=""])
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

                echo "\n\n\n $CUPSAPIVERSION \n\n\n"
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
            fi
        else
            if test "$(uname)" = "Linux"; then
                AC_MSG_WARN([* USING LOCAL CUPS SOURCE *])
                SHARELCUPS=0
                SHARELCUPSI=0
                LCUPSBUILDTYPE=linux
                LCUPSINCLUDE="include \$(GLSRCDIR)/lcups.mak"
                LCUPSIINCLUDE="include \$(GLSRCDIR)/lcupsi.mak"
                CUPSDEV="cups pwgraster"
            fi
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
AC_ARG_WITH([ijs], AC_HELP_STRING([--without-ijs],
    [disable IJS driver support]))

case `uname` in
    MINGW*)
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

dnl Luratech detection
LURATECHDIR=luratech

AC_ARG_WITH([luratech], AC_HELP_STRING([--without-luratech],
    [do not try to use the Luratech library for JBIG2 nor JPX decoding]))


if test x$with_luratech != xno; then
  AC_MSG_CHECKING([for local Luratech JBIG2 library source])
  if test -d $srcdir/luratech/ldf_jb2; then
    AC_MSG_RESULT([yes])
    JBIG2_DECODER=luratech
    SHARE_JBIG2=0
    JBIG2DIR=$srcdir/luratech/ldf_jb2

    case `uname` in
      Darwin*)
        JBIG2_AUTOCONF_CFLAGS="-DUSE_LDF_JB2 -DMAC -DMAC_OS_X_BUILD"
      ;;
      AIX)
        if test $ac_cv_prog_gcc = yes; then
          JBIG2_AUTOCONF_CFLAGS="-DUSE_LDF_JB2 -fsigned-char -DLINUX"
        else
          JBIG2_AUTOCONF_CFLAGS="-DUSE_LDF_JB2 -qchars=signed -DLINUX"
        fi
     ;;
     *)
        JBIG2_AUTOCONF_CFLAGS="-DUSE_LDF_JB2 -DLINUX"
     ;;
     esac

    JBIG2FILEDEVS='$(DD)gdevjbig2.dev'
    JBIG2DEVS='$(PSD)jbig2.dev'
  else
    AC_MSG_RESULT([no])
  fi
fi

JB2_STDINT_TYPES_IN=

if test "x$JBIG2_DECODER" = x; then
  dnl look for jbig2dec
  AC_ARG_WITH([jbig2dec], AC_HELP_STRING([--without-jbig2dec],
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
            AC_TRY_COMPILE([#include <$include>], [uint32_t canary;], [
              AC_MSG_RESULT([yes])
              stdint_types_in="$include"
              break;
              ], AC_MSG_RESULT([no])
            )
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

        echo
        AC_MSG_RESULT([$JBIG2DIR])
        echo "Continuing with Ghostscript configuration..."
    else
      AC_MSG_RESULT([no])
      AC_CHECK_LIB([jbig2dec], [jbig2_page_out], [
          SHARE_JBIG2=1
      ], [
          AC_MSG_WARN([disabling support for JBIG2 files])
          with_jbig2dec=no
    ])
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

if test x$with_luratech != xno; then
  AC_MSG_CHECKING([for local Luratech JPEG2K library source])
  if test -d $srcdir/luratech/lwf_jp2; then
    AC_MSG_RESULT([yes])
    JPX_DECODER=luratech
    SHARE_JPX=0
    JPXDIR=$srcdir/luratech/lwf_jp2

    case `uname` in
      Darwin*)
        JPX_AUTOCONF_CFLAGS="-DUSE_LWF_JP2 -DMAC -DMAC_OS_X_BUILD"
      ;;
      AIX)
        if test $ac_cv_prog_gcc = yes; then
          JPX_AUTOCONF_CFLAGS="-DUSE_LWF_JP2 -fsigned-char -DLINUX"
        else
          JPX_AUTOCONF_CFLAGS="-DUSE_LWF_JP2 -qchars=signed -DLINUX"
        fi
     ;;
     *)
        JPX_AUTOCONF_CFLAGS="-DUSE_LWF_JP2 -DLINUX"
     ;;
     esac

    JPXDEVS='$(PSD)jpx.dev'
  else
    AC_MSG_RESULT([no])
  fi
fi

OPENJPEGDIR=$srcdir/openjpeg

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
  if test $ac_cv_prog_gcc = yes; then
    JPX_SSE_CFLAGS="-U__SSE__"
  fi
fi

AC_ARG_ENABLE([openjpeg], AC_HELP_STRING([--disable-openjpeg],
        [Do not use OpenJPEG for JPX decoding]))

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
      
      JPX_AUTOCONF_CFLAGS="$JPX_AUTOCONF_CFLAGS -DUSE_JPIP -DUSE_OPENJPEG_JP2 $CFLAGS_OPJ_HAVE_STDINT_H $CFLAGS_OPJ_HAVE_INTTYPES_H $CFLAGS_OPJ_BIGENDIAN $CFLAGS_OPJ_HAVE_FSEEKO"

      JPXDEVS='$(PSD)jpx.dev'
    else
      AC_MSG_RESULT([no])
    fi
  fi
fi

AC_SUBST(JPX_DECODER)
AC_SUBST(JPX_AUTOCONF_CFLAGS)
AC_SUBST(JPXDIR)
AC_SUBST(SHARE_JPX)
AC_SUBST(JPXDEVS)

dnl check if we can/should build the gtk loader
AC_ARG_ENABLE([gtk], AC_HELP_STRING([--disable-gtk],
    [Do not build the gtk loader]))
SOC_CFLAGS=""
SOC_LIBS=""
SOC_LOADER=""
if test "x$enable_gtk" != "xno"; then
    # Try GTK+ 3.x first...
    if test "x$PKGCONFIG" != x; then
        AC_MSG_CHECKING(for GTK+ 3.x)
        if $PKGCONFIG --exists gtk+-3.0; then
            SOC_LOADER="dxmain.c"
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
                SOC_LOADER="dxmain.c"
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
  SOC_LOADER="dxmainc.c"
fi

AC_SUBST(SOC_CFLAGS)
AC_SUBST(SOC_LIBS)
AC_SUBST(SOC_LOADER)

dnl look for omni implementation
AC_ARG_WITH([omni], AC_HELP_STRING([--with-omni],
        [enable the omni driver]))
dnl set safe defaults
OMNIDEVS=''
INCLUDEOMNI=no
if ( ! test -z "$CONTRIBINCLUDE" ) && ( test x$with_omni = xyes ); then
        INCLUDEOMNI=yes
        OMNIDEVS='$(DD)omni.dev'

        if test -n "$GCC"; then
                LIBS="$LIBS -lstdc++"
        fi
fi
AC_SUBST(OMNIDEVS)

dnl optional X11 for display devices
AC_PATH_XTRA

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
                if test "$uname" = "SunOS"; then
                        X_LDFLAGS="$X_LDFLAGS -R$x_libraries"
                fi
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
AC_ARG_WITH([gs], AC_HELP_STRING([--with-gs=NAME],
        [name of the Ghostscript executible [[gs]]]),
        [GS="$with_gs"],[GS='gs'])
AC_SUBST(GS)

dnl do we compile the postscript initialization files into Ghostscript?
COMPILE_INITS="1"
AC_ARG_ENABLE([compile-inits], AC_HELP_STRING([--disable-compile-inits],
       [Do not compile in initialization files]),[
               if test "x$enable_compile_inits" = xno; then
                       COMPILE_INITS="0"
               fi])
AC_SUBST(COMPILE_INITS)

dnl look for drivers to compile...
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
MISC_PDEVS='uniprint ap3250 atx23 atx24 atx38 coslw2p coslwxl cp50 declj250 fs600 imagen lj250 m8510 necp6 oce9050 r4081 sj48 tek4696 t4693d2 t4693d4 t4693d8 dl2100 la50 la70 la75 la75plus ln03 xes md2k md5k gdi samsunggdi'
OPVP_DEVS='opvp oprp'

ETS_HALFTONING_DEVS='rinkj'

dnl Known file formats
BMP_DEVS='bmpmono bmpgray bmpsep1 bmpsep8 bmp16 bmp256 bmp16m bmp32b'

JPEG_DEVS='jpeg jpeggray jpegcmyk'
# PNG_DEVS='png16 png16m png256 pngalpha pnggray pngmono'

PCX_DEVS='pcxmono pcxgray pcx16 pcx256 pcx24b pcxcmyk pcx2up'
PBM_DEVS='pbm pbmraw pgm pgmraw pgnm pgnmraw pnm pnmraw ppm ppmraw pkm pkmraw pksm pksmraw pam pamcmyk4 pamcmyk32 plan plang planm planc plank'
PS_DEVS='psdf psdcmyk psdrgb pdfwrite ps2write eps2write epswrite bbox txtwrite inkcov ink_cov psdcmykog fpng'
MISC_FDEVS='ccr cif inferno mag16 mag256 mgr4 mgr8 mgrgray2 mgrgray4 mgrgray8 mgrmono miff24 plan9bm sgirgb sunhmono bit bitrgb bitrgbtags bitcmyk devicen spotcmyk xcf'
XPSDEV='xpswrite'

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
                P_DEVS0="$P_DEVS0 $CANON_DEVS $EPSON_DEVS $HP_DEVS $LEXMARK_DEVS $BROTHER_DEVS $APPLE_DEVS $IBM_DEVS $OKI_DEVS $JAPAN_DEVS $MISC_PDEVS $ETS_HALFTONING_DEVS"
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

noncontribmakefiles=`find $srcdir -name '*.mak' -print | grep -v '^\./contrib/'`

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
DYNAMIC_LDFLAGS=""
DYNAMIC_LIBS=""
INSTALL_SHARED=""
SO_LIB_EXT=".so"
DLL_EXT=""
SO_LIB_VERSION_SEPARATOR="."

case `uname` in
        Linux*|GNU*)
          DYNAMIC_CFLAGS="-fPIC"
          DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(GS_SONAME_MAJOR)"
          if test $ac_cv_prog_gcc = yes; then
            # GCC high level flag
            DYNAMIC_LIBS="-rdynamic -ldl"
          else
            DYNAMIC_LIBS=""
          fi
          SO_LIB_EXT=".so"
        ;;
        MINGW*)
          DYNAMIC_CFLAGS=""
          DYNAMIC_LDFLAGS="-shared  -Wl,--out-implib=\$(BINDIR)/lib\$(GS_SO_BASE).dll.a -Wl,--export-all-symbols -Wl,--enable-auto-import"
          SO_LIB_EXT=""
          DLL_EXT=".dll"
          SO_LIB_VERSION_SEPARATOR="-"
        ;;
        *BSD)
          DYNAMIC_CFLAGS="-fPIC"
          DYNAMIC_LDFLAGS="-shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(GS_SONAME_MAJOR)"
          DYNAMIC_LIBS=""
          SO_LIB_EXT=".so"
        ;;
        Darwin*)
          DYNAMIC_LDFLAGS="-dynamiclib -install_name \$(GS_SONAME_MAJOR_MINOR)"
          DYNAMIC_LIBS=""
          SO_LIB_EXT=".dylib"
        ;;
        SunOS)
          if test $ac_cv_prog_gcc = yes; then
            DYNAMIC_CFLAGS="-fPIC"
          else
            DYNAMIC_CFLAGS="-KPIC"
          fi

          DYNAMIC_LDFLAGS="-G -shared -Wl,\$(LD_SET_DT_SONAME)\$(LDFLAGS_SO_PREFIX)\$(GS_SONAME_MAJOR)"
          DYNAMIC_LIBS=""
          SO_LIB_EXT=".so"
        ;;
        AIX)
          DYNAMIC_CFLAGS="-fPIC"
          GCFLAGS="-Wl,-brtl $GCFLAGS"
          DYNAMIC_LDFLAGS="-shared -Wl,-brtl,-G -fPIC"
          if test $ac_cv_prog_gcc = yes; then
            # GCC high level flag
            DYNAMIC_LIBS="-ldl"
          else
            DYNAMIC_LIBS=""
          fi
          SO_LIB_EXT=".so"
        ;;
esac

AC_ARG_ENABLE([dynamic], AC_HELP_STRING([--enable-dynamic],
    [Enable dynamically loaded drivers]),
[
        if test "x$enable_dynamic" != xno; then
                case `uname` in
                        Linux*|GNU*)
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
                        *BSD)
                        DYNAMIC_DEVS="\$(GLOBJDIR)/X11.so"
                        DYNAMIC_FLAGS="-DGS_DEVS_SHARED -DGS_DEVS_SHARED_DIR=\\\"\$(gssharedir)\\\""
                        X11_DEVS=""
                        OPT_CFLAGS="$DYNAMIC_CFLAGS $OPT_CFLAGS"
                        DBG_CFLAGS="$DYNAMIC_CFLAGS $DBG_CFLAGS"
                        ;;
                        Darwin*)
                        INSTALL_SHARED="install-shared"
                        DYNAMIC_FLAGS="-DGS_DEVS_SHARED -DGS_DEVS_SHARED_DIR=\\\"\$(gssharedir)\\\""
                        X11_DEVS=""
                        OPT_CFLAGS="$DYNAMIC_CFLAGS $OPT_CFLAGS"
                        DBG_CFLAGS="$DYNAMIC_CFLAGS $DBG_CFLAGS"
                        ;;
                        SunOS)
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
AC_SUBST(DYNAMIC_LDFLAGS)
AC_SUBST(DYNAMIC_LIBS)
AC_SUBST(INSTALL_SHARED)
AC_SUBST(X11_DEVS)
AC_SUBST(SO_LIB_EXT)
AC_SUBST(DLL_EXT)
AC_SUBST(SO_LIB_VERSION_SEPARATOR)

dnl look for default font path...
AC_ARG_WITH([fontpath],  AC_HELP_STRING([--with-fontpath],
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

AC_PROG_GCC_TRADITIONAL

dnl NB: We don't actually provide autoconf-switched fallbacks for any
dnl     of these functions, so the checks are purely informational.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero dup2 floor gettimeofday memchr memmove memset mkdir mkfifo modf pow putenv rint setenv sqrt strchr strrchr strspn strstr])

dnl --------------------------------------------------
dnl set big/little endian for LCMS
dnl --------------------------------------------------

if test "x$BIGENDIAN" != "x0"; then
  LCMS_ENDIAN="-DUSE_BIG_ENDIAN=$BIGENDIAN"
  LCMS2_ENDIAN="-DCMS_USE_BIG_ENDIAN=$BIGENDIAN"
else
  LCMS_ENDIAN=
  LCMS2_ENDIAN=
fi

AC_SUBST(LCMS_ENDIAN)
AC_SUBST(LCMS2_ENDIAN)

dnl --------------------------------------------------
dnl decide if we have to "hack" sqrtf() for lcms2
dnl --------------------------------------------------

AC_CHECK_FUNC([sqrtf], [SQRTF_SUBST=], [SQRTF_SUBST="-Dsqrtf=\"(float)sqrt\""])

AC_SUBST(SQRTF_SUBST)


dnl --------------------------------------------------
dnl currently just for trio
dnl --------------------------------------------------

AC_CHECK_FUNC([floorl], [TRIO_NO_FLOORL=""], [TRIO_NO_FLOORL="-DTRIO_NO_FLOORL"])

AC_CHECK_FUNC([ceill], [TRIO_NO_CEILL=""], [TRIO_NO_CEILL="-DTRIO_NO_CEILL"])

AC_CHECK_FUNC([powl], [TRIO_NO_POWL=""], [TRIO_NO_POWL="-DTRIO_NO_POWL"])

AC_CHECK_FUNC([fmodl], [TRIO_NO_FMODL=""], [TRIO_NO_FMODL="-DTRIO_NO_FMODL"])

AC_CHECK_FUNC([log10l], [TRIO_NO_LOG10L=""], [TRIO_NO_LOG10L="-DTRIO_NO_LOG10L"])

AC_TRIO_CFLAGS="$TRIO_NO_FLOORL $TRIO_NO_CEILL $TRIO_NO_POWL $TRIO_NO_FMODL $TRIO_NO_LOG10L"


AC_SUBST(AC_TRIO_CFLAGS)

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

# --------------------------------------------------
# Sun, BSD possibly other makes don't have quite the
# feature set of GNU make. We still prefer GNU make,
# but......
# --------------------------------------------------
AC_ARG_WITH([gnu-make],  AC_HELP_STRING([--without-gnu-make],
    [disable GNU make features]), gnu_make=no)

if test "x$gnu_make" != "xno"; then
  ORDER_ONLY="|"
else
  ORDER_ONLY=""
fi

AC_SUBST(ORDER_ONLY)

# --------------------------------------------------
# annoying: Windows doesn't allow "aux" as a
# directory nor file name, so if we're building in
# mingw, add the same prefix as the VS build uses
# --------------------------------------------------
AUXDIRPOSTFIX=""
case `uname` in
    MINGW*)
    AUXDIRPOSTFIX="_"
    CFLAGS="-DGS_NO_UTF8=1 $CFLAGS"
    ;;
esac
AC_SUBST(AUXDIRPOSTFIX)


dnl --------------------------------------------------
dnl Do substitutions
dnl --------------------------------------------------

AC_SUBST(OPT_CFLAGS)
AC_SUBST(DBG_CFLAGS)
AC_SUBST(GCFLAGS)

AC_OUTPUT(Makefile)

if test "x$AFS" = "x1"; then
   AC_MSG_WARN([Using "native" font scaler which is now deprecated (rather than freetype),])
   AC_MSG_WARN([Support for this will be removed in a future release])
fi

if test "x$NTS_EXCLUDES" != "x" ; then
   AC_MSG_WARN([Due to the --enable-threadsafe option, the following devices have been excluded because they are not threadsafe:])
   echo "$NTS_EXCLUDES"
fi
