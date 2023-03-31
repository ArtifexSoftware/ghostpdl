/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Generic substitute for Unix sys/stat.h */

#ifndef stat__INCLUDED
#  define stat__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/* Metrowerks Standard Library doesn't use subdirs */
#ifdef __MWERKS__
#include <stat.h>
#else
#include <sys/stat.h>
#endif

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
#  define stat __stat64
#  define struct_stat struct __stat64
#  define fstat _fstat64
#else
#define struct_stat struct stat
#endif

/* Find permissions for file */
/* Ideally this would defined in gp.h, but the macroisms mean it has to be
 * defined here. */
extern int gp_stat(const gs_memory_t *mem, const char *path, struct stat *buf);

/*
 * Some (System V?) systems test for directories in a slightly different way.
 */
#if defined(OSK) || !defined(S_ISDIR)
#  ifdef S_IFDIR
#    define stat_is_dir(stbuf) ((stbuf).st_mode & S_IFDIR)
#  else
#    ifdef _S_IFDIR
#      define stat_is_dir(stbuf) ((stbuf).st_mode & _S_IFDIR)
#    endif
#  endif
#else
#  define stat_is_dir(stbuf) S_ISDIR((stbuf).st_mode)
#endif

/*
 * Some systems have S_IFMT and S_IFCHR but not S_ISCHR.
 */
#if !defined(S_ISCHR) || !defined(S_ISREG)
#  ifndef S_IFMT
#    ifdef _S_IFMT
#      define S_IFMT _S_IFMT
#      define S_IFCHR _S_IFCHR
#      define S_IFREG _S_IFREG
#    else
#    ifdef __S_IFMT
#      define S_IFMT __S_IFMT
#      define S_IFCHR __S_IFCHR
#      define S_IFREG __S_IFREG
#    endif
#    endif
#  endif
#  define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#  define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

/*
 * Microsoft C doesn't define S_IRUSR or S_IWUSR.
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
