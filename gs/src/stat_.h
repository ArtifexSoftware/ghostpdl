/* Copyright (C) 1991, 1995, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
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

#endif /* stat__INCLUDED */
