# Process this file with autoconf to produce a configure script.

package=jbig2dec
version=0.1

AC_INIT([$package], [$version], [jbig2-dev@ghostscript.com])
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR([jbig2dec.c])

AM_INIT_AUTOMAKE([$package], [$version])
AM_CONFIG_HEADER(config.h)

AC_REVISION([$Revision: 1.1 $])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_LIB([png], [png_check_sig])
AC_CHECK_LIB([z], [deflate])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([libintl.h stddef.h stdint.h stdlib.h string.h strings.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([memset strdup])

dnl use our included getopt if the system doesn't have getopt_long()
AC_SUBST(LIBOJBS)
AC_CHECK_FUNC(getopt_long, [AC_DEFINE(HAVE_GETOPT_LONG)],
	[LIBOBJS="$LIBOBJS getopt.o getopt1.o"])

# generate output
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
