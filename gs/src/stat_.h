/* Copyright (C) 1991, 2000 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* Generic substitute for Unix sys/stat.h */

#ifndef stat__INCLUDED
#  define stat__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"
#include <sys/stat.h>

/*
 * Many environments, including the MS-DOS compilers, don't define
 * the st_blocks member of a stat structure.
 */
#if defined(__SVR3) || defined(__EMX__) || defined(__DVX__) || defined(OSK) || defined(__MSDOS__) || defined(__QNX__) || defined(VMS) || defined(__WIN32__) || defined(__IBMC__) || defined(__BEOS__) || defined(Plan9) || defined(__WATCOMC__)
#  define stat_blocks(psbuf) (((psbuf)->st_size + 1023) >> 10)
#else
#  define stat_blocks(psbuf) ((psbuf)->st_blocks)
#endif

/*
 * Microsoft C uses _stat instead of stat,
 * for both the function name and the structure name.
 */
#ifdef _MSC_VER
#  define stat _stat
#endif

/*
 * Some (System V?) systems test for directories in a slightly different way.
 */
#if defined(OSK) || !defined(S_ISDIR)
#  define stat_is_dir(stbuf) ((stbuf).st_mode & S_IFDIR)
#else
#  define stat_is_dir(stbuf) S_ISDIR((stbuf).st_mode)
#endif

/*
 * Some systems have S_IFMT and S_IFCHR but not S_ISCHR.
 */
#ifndef S_ISCHR
#  ifndef S_IFMT
#    ifdef _S_IFMT
#      define S_IFMT _S_IFMT
#      define S_IFCHR _S_IFCHR
#    else
#    ifdef __S_IFMT
#      define S_IFMT __S_IFMT
#      define S_IFCHR __S_IFCHR
#    endif
#    endif
#  endif
#  define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#endif
/*
 * Microsoft C defines the O_ and S_ symbols with, and only with, a
 * leading underscore.
 */
#ifndef O_APPEND
#  define O_APPEND _O_APPEND
#endif
#ifndef O_CREAT
#  define O_CREAT _O_CREAT
#endif
#ifndef O_EXCL
#  define O_EXCL _O_EXCL
#endif
#ifndef O_RDONLY
#  define O_RDONLY _O_RDONLY
#endif
#ifndef O_RDWR
#  define O_RDWR _O_RDWR
#endif
#ifndef O_TRUNC
#  define O_TRUNC _O_TRUNC
#endif
#ifndef O_WRONLY
#  define O_WRONLY _O_WRONLY
#endif
/*
 * Microsoft C also doesn't define S_IRUSR or S_IWUSR.
 */
#ifndef S_IRUSR
#  ifndef S_IREAD
#    define S_IRUSR _S_IREAD
#  else
#    define S_IRUSR S_IREAD
#  endif
#endif
#ifndef S_IWUSR
#  ifndef S_IWRITE
#    define S_IWUSR _S_IWRITE
#  else
#    define S_IWUSR S_IWRITE
#  endif
#endif

#endif /* stat__INCLUDED */
