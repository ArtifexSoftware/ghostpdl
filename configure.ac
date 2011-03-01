#  Copyright (C) 2001-2011 Artifex Software, Inc.
#  All Rights Reserved.

#  This software is provided AS-IS with no warranty, either express or
#  implied.

#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/
#  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
#  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.

# $Id$

# Process this file with autoconf to produce a configure script

# ------------------------------------------------
# Initialization and Versioning
# ------------------------------------------------

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.  This
# file is a modified template produced by autoscan with additions
# derived from ghostscript's configure.ac.
#

AC_PREREQ([2.67])
AC_INIT
AC_CONFIG_SRCDIR([pcl/pcjob.c])

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_RESOLV
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h inttypes.h langinfo.h libintl.h limits.h locale.h malloc.h memory.h netdb.h netinet/in.h nl_types.h stddef.h stdint.h stdlib.h string.h strings.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

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

AC_CHECK_FUNCS([mkstemp], [HAVE_MKSTEMP=-DHAVE_MKSTEMP])
AC_SUBST(HAVE_MKSTEMP)

AC_CHECK_FUNCS([hypot], [HAVE_HYPOT=-DHAVE_HYPOT])
AC_SUBST(HAVE_HYPOT)

AC_CHECK_FUNCS([fopen64], [HAVE_FILE64=-DHAVE_FILE64])
AC_SUBST(HAVE_FILE64)

AC_CHECK_FUNCS([mkstemp64], [HAVE_MKSTEMP64=-DHAVE_MKSTEMP64])
AC_SUBST(HAVE_MKSTEMP64)

AC_CHECK_FUNCS([setlocale], [HAVE_SETLOCALE=-DHAVE_SETLOCALE])
AC_SUBST(HAVE_SETLOCALE)

dnl --------------------------------------------------
dnl check for sse2 intrinsics
dnl --------------------------------------------------

HAVE_SSE2=""
AC_COMPILE_IFELSE([
#include <emmintrin.h>
],HAVE_SSE2="-DHAVE_SSE2")

AC_ARG_ENABLE([sse2], AC_HELP_STRING([--disable-sse2],
       [Do not use sse2 instrinsics]), [
             if test "x$enable_sse2" = xno; then
                HAVE_SSE2=""
             fi])

AC_SUBST(HAVE_SSE2)

AC_OUTPUT(config.mak)
