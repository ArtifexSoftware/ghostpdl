# Process this file with autoconf to produce a configure script.

AC_INIT([jbig2dec], [0.2], [jbig2-dev@ghostscript.com])
AC_PREREQ(2.53)
AC_CONFIG_SRCDIR([jbig2dec.c])

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_REVISION([$Revision: 1.17 $])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.
dnl by default we want png support if possible
AC_ARG_WITH([libpng], 
  AC_HELP_STRING([--with-libpng[=prefix]],
    [include support for png output (if libpng is available)]),
  [ac_cv_want_libpng="$withval"], [ac_cv_want_libpng="yes"])
save_cflags="$CFLAGS"
save_ldflags="$LDFLAGS"
have_libpng="no"
if test "x$ac_cv_want_libpng" != "xno"; then
  if test "x$ac_cv_want_libpng" != "xyes"; then
    dnl if it's not yes or no, treat as a prefix
    CFLAGS="$CFLAGS -I$ac_cv_want_libpng/include"
    LDFLAGS="$LDFLAGS -L$ac_cv_want_libpng/lib"
  fi
  dnl libpng requires pow() which may be in libm
  AC_SEARCH_LIBS([pow], [m])
  AC_CHECK_LIB([png], [png_check_sig], [
    AC_CHECK_LIB([z], [deflate], [
      AC_DEFINE(HAVE_LIBPNG, 1, [Define if libpng is available (-lpng)])
      LIBS="-lpng -lz $LIBS"
      AC_LIBOBJ([jbig2_image_png])
      have_libpng="yes"
    ])
  ])
fi
dnl restore (possibly changed) flags if we didn't find working libpng
if test "x$have_libpng" != "xyes"; then
  CFLAGS="$save_cflags"
  LDFLAGS="$save_ldflags"
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([libintl.h stddef.h unistd.h strings.h])

dnl We assume the fixed-size types from stdint.h. If that header is are not available,
dnl look for the same types in a few other headers. We also attempt to define them
dnl ourselves, but only use those is the native versions aren't available.
dnl The substitutions happen in a file config_types.h, whose template is
dnl created by autogen.sh
stdint_types_in="no_replacement_found"
stdint_types_discovered="yes"
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
if test $ac_cv_sizeof_char = 1; then
  int8_type="char"
else
  stdint_types_discovered="no"
fi
if test $ac_cv_sizeof_short = 2; then
  int16_type="short"
elif test $ac_cv_sizeof_char = 2; then
  int16_type="char"
elif test $ac_cv_sizeof_int = 2; then
  int16_type="char"
else
  stdint_types_discovered="no"
fi
if test $ac_cv_sizeof_int = 4; then
  int32_type="int"
elif test $ac_cv_sizeof_long = 4; then
  int32_type="long"
elif test $ac_cv_sizeof_short = 4; then
  int32_type="short"
else
  stdint_types_discovered="no"
fi
AC_CHECK_HEADER([stdint.h])
if test "x$ac_cv_header_stdint_h" != "xyes"; then
  for include in sys/types.h inttypes.h sys/inttypes.h sys/int_types.h ; do
    AC_MSG_CHECKING([for uint32_t in $include])
    AC_TRY_COMPILE([#include <$include>], [uint32_t canary;], [
      AC_MSG_RESULT([yes])
      stdint_types_in="$include"
      break;
    ], AC_MSG_RESULT([no])
    )
  done
  if test "x$stdint_types_in" != "xno_replacement_found"; then
    AC_MSG_RESULT([Adding $stdint_types_in to config header for stdint types])
    AC_DEFINE([JBIG2_REPLACE_STDINT_H],,
      [set by configure if an alternate header with the stdint.h types is found])
  elif test "x$stdint_types_discovered" = "xno"; then
    AC_MSG_ERROR([
Unable to find suitable definitions of the stdint.h types (uint32_t and friends)
You will have to define these yourself in a separate header
    ])
  fi
fi
AC_SUBST(JBIG2_INT32_T, [$int32_type])
AC_SUBST(JBIG2_INT16_T, [$int16_type])
AC_SUBST(JBIG2_INT8_T, [$int8_type])
AC_SUBST(JBIG2_STDINT_H, [$stdint_types_in])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_C_BIGENDIAN

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([memset strdup])

dnl use our included getopt if the system doesn't have getopt_long()
AC_CHECK_FUNC(getopt_long, 
	[AC_DEFINE(HAVE_GETOPT_LONG,,[Define if the local libc includes getopt_long()])],
	AC_LIBOBJ([getopt])
	AC_LIBOBJ([getopt1])
)

# generate output
AC_CONFIG_FILES([Makefile config_types.h])
AC_OUTPUT
