dnl Process this file with autoconf to produce a configure script

dnl ------------------------------------------------
dnl Initialization and Versioning
dnl ------------------------------------------------

AC_INIT
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(src/gs.c)

dnl --------------------------------------------------  
dnl Check for programs
dnl --------------------------------------------------  

dnl AC_PROG_CC likes to add '-g -O2' to CFLAGS. however,
dnl we ignore those flags and construct our own.
AC_PROG_CC
AC_PROG_CPP

AC_PROG_RANLIB
#AC_PROG_INSTALL

dnl --------------------------------------------------
dnl Set build flags based on environment
dnl --------------------------------------------------

#AC_CANONICAL_HOST

if test $ac_cv_prog_gcc = yes; then
    cflags_to_try="-Wall -Wstrict-prototypes \
-Wmissing-declarations -Wmissing-prototypes \
-Wcast-qual -Wwrite-strings \
-fno-builtin -fno-common"
else
    cflags_to_try=
fi
optflags_to_try="-O2"
	
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
AC_MSG_RESULT([ ...done.])

dnl --------------------------------------------------
dnl Check for headers
dnl --------------------------------------------------

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([errno.h fcntl.h limits.h malloc.h memory.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/time.h syslog.h unistd.h])

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

dnl we aren't interested in all of DEFS, so manually insert
dnl the flags we care about
if test "$ac_cv_c_const" != yes; then
	GCFLAGS="$GCFLAGS -Dconst="
fi
if test "$ac_cv_header_stdint_h" = yes; then
	GCFLAGS="$GCFLAGS -DHAVE_STDINT_H"
fi

dnl try to find a 64 bit type for devicen color index
uint64_type="none"
 AC_CHECK_SIZEOF(unsigned long int)
 if test $ac_cv_sizeof_unsigned_long_int = 8; then
	uint64_type="unsigned long int"
 else
  AC_CHECK_SIZEOF(unsigned long long)
  if test $ac_cv_sizeof_unsigned_long_long = 8; then
	uint64_type="unsigned long long"
  else
   AC_CHECK_SIZEOF(unsigned __int64)
   if test $ac_cv_sizeof_unsigned___int64 = 8; then
	uint64_type="unsigned __int64"
   else
    AC_CHECK_SIZEOF(u_int64_t)
    if test $ac_cv_sizeof_u_int64_t = 8; then
	unit64_type="u_int64_t"
    fi
   fi
  fi
 fi
dnl we don't need to do anything if a 64-bit type wasn't found
dnl the code falls back to a (probably 32-bit) default
if test "$uint64_type" != "none"; then
	GCFLAGS="$GCFLAGS -DGX_COLOR_INDEX_TYPE=\"$uint64_type\""
fi


dnl --------------------------------------------------
dnl Check for libraries
dnl --------------------------------------------------

AC_CHECK_LIB(m, cos)
dnl AC_CHECK_LIB(pthread, pthread_create)

AC_MSG_CHECKING([for local jpeg library source])
dnl At present, we give the local source priority over the shared
dnl build, so that the D_MAX_BLOCKS_IN_MCU patch will be applied.
dnl A more sophisticated approach would be to test the shared lib
dnl to see whether it has already been patched.
LIBJPEGDIR=src
if test -d jpeg; then
	AC_MSG_RESULT([yes])
	SHARE_LIBJPEG=0
	LIBJPEGDIR=jpeg
else
	AC_MSG_RESULT([no])
	AC_CHECK_LIB(jpeg, jpeg_set_defaults, [SHARE_LIBJPEG=1], [
		AC_MSG_ERROR([I wasn't able to find a copy
  of the jpeg library. This is required for compiling
  ghostscript. Please download a copy of the source,
  e.g. from http://www.ijg.org/, unpack it at the
  top level of the gs source tree, and rename
  the directory to 'jpeg'.
		])
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

dnl these are technically optional

AC_MSG_CHECKING([for local zlib source])
dnl zlib is needed for language level 3, and libpng
# we must define ZLIBDIR regardless because libpng.mak does a -I$(ZLIBDIR)
# this seems a harmless default
ZLIBDIR=src
if test -d zlib; then
	AC_MSG_RESULT([yes])
	SHARE_ZLIB=0
	ZLIBDIR=zlib
else
	AC_MSG_RESULT([no])
	AC_CHECK_LIB(z, deflate, [SHARE_ZLIB=1], [
		AC_MSG_ERROR([I did not find a copy of zlib on your system.
Please either install it, or unpack a copy of the source in a
local directory named 'zlib'. See http://www.gzip.org/zlib/
for more information.
		])
	])
fi
AC_SUBST(SHARE_ZLIB)
AC_SUBST(ZLIBDIR)

dnl png for the png output device; it also requires zlib
LIBPNGDIR=src
AC_MSG_CHECKING([for local png library source])
if test -d libpng; then
	AC_MSG_RESULT([yes])
	SHARE_LIBPNG=0
	LIBPNGDIR=libpng
	PNGDEVS='$(DD)png16m.dev $(DD)pnggray.dev $(DD)pngmono.dev $(DD)png256.dev $(DD)png16.dev $(DD)pngalpha.dev'
else
	AC_MSG_RESULT([no])
	AC_CHECK_LIB(png, png_check_sig, [
		SHARE_LIBPNG=1
		PNGDEVS='$(DD)png16m.dev $(DD)pnggray.dev $(DD)pngmono.dev $(DD)png256.dev $(DD)png16.dev $(DD)pngalpha.dev'
	], [
		SHARE_LIBPNG=0
		AC_MSG_RESULT([no])
		AC_MSG_NOTICE([disabling png output devices])
		PNGDEVS=''
	], [-lz])
fi
AC_SUBST(SHARE_LIBPNG)
AC_SUBST(LIBPNGDIR)
AC_SUBST(PNGDEVS)

dnl look for IJS implementation
AC_ARG_WITH(ijs, AC_HELP_STRING([--with-ijs],[include IJS driver support]))
dnl set safe defaults
    IJSDIR=src
    IJSDEVS=''
if test x$with_ijs != xno; then
    AC_MSG_CHECKING([for local ijs library source])
    if test -d ijs; then
        AC_MSG_RESULT([yes])
        IJSDIR=ijs
        IJSDEVS='$(DD)ijs.dev'
    else
        AC_MSG_RESULT([no])
    fi
fi
AC_SUBST(IJSDIR)
AC_SUBST(IJSDEVS)

dnl look for jbig2dec
AC_ARG_WITH(jbig2dec, AC_HELP_STRING([--with-jbig2dec],[include JBIG2 decode support]))
JBIG2DIR=src
SHARE_JBIG2=0
JBIG2DEVS=''
if test x$with_jbig2dec != xno; then
  AC_MSG_CHECKING([for local jbig2dec library source])
  for d in jbig2dec jbig2dec-0.2 jbig2dec-0.3; do
    test -d "$d" && JBIG2DIR=$d && break
  done
  if test "x$JBIG2DIR" != xsrc; then
    AC_MSG_RESULT([$JBIG2DIR])
  else
    AC_MSG_RESULT([no])
    AC_CHECK_LIB([jbig2dec], [jbig2_page_out], [
	SHARE_JBIG2=1
    ], [
	AC_MSG_WARN([disabling support for JBIG2 files])
        with_jbig2dec=no
    ])
  fi
fi
if test x$with_jbig2dec != xno; then
  JBIG2DEVS='$(PSD)jbig2.dev'
  if test x$ac_cv_header_stdint_h != xyes; then
    AC_MSG_WARN([JBIG2 support requires stdint types which do not seem to be available.])
  fi
fi

AC_SUBST(JBIG2DIR)
AC_SUBST(SHARE_JBIG2)
AC_SUBST(JBIG2DEVS)

dnl optional X11 for display devices
AC_PATH_XTRA
XLIBS=''
if test x"$no_x" = x"yes"; then
	AC_MSG_NOTICE([disabling X11 output devices])
	X11DEVS=''
else
	# enable X11 output devices
	X11DEVS='$(DD)x11.dev $(DD)x11alpha.dev $(DD)x11cmyk.dev $(DD)x11gray2.dev $(DD)x11gray4.dev $(DD)x11mono.dev'
	# the makefile wants a list of just the library names in XLIBS
	for item in -lXt $X_PRE_LIBS -lXext -lX11 $X_EXTRA_LIBS; do
		stripped=`echo "$item" | sed -e 's/-l//'`
		if test "x$stripped" != "x$item"; then
			XLIBS="$XLIBS $stripped"
		fi
	done
fi
AC_SUBST(X11DEVS)
AC_SUBST(XLIBS)

dnl executible name
AC_ARG_WITH(gs, AC_HELP_STRING([--with-gs=NAME],
	[name of the ghostscript executible [[gs]]]),
	[GS="$with_gs"],[GS='gs'])
AC_SUBST(GS)

dnl do we compile the postscript initialization files into Ghostscript?
COMPILE_INITS="0"
AC_ARG_ENABLE(compile-inits, AC_HELP_STRING([--enable-compile-inits],
       [compile initialization files into Ghostscript]),[
               if test x$enable_compile_inits = xyes; then
                       COMPILE_INITS="1"
               fi])
AC_SUBST(COMPILE_INITS)

dnl --------------------------------------------------
dnl Check for library functions
dnl --------------------------------------------------

AC_CHECK_FUNCS([mkstemp], [HAVE_MKSTEMP=-DHAVE_MKSTEMP])
AC_SUBST(HAVE_MKSTEMP)

AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero dup2 floor gettimeofday memchr memmove memset mkdir mkfifo modf pow putenv rint setenv sqrt strchr strerror strrchr strspn strstr])

dnl --------------------------------------------------
dnl Do substitutions
dnl --------------------------------------------------

AC_SUBST(OPT_CFLAGS)
AC_SUBST(GCFLAGS)
AC_OUTPUT(Makefile)
