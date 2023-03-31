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


/* Declaration of popen and pclose */

#ifndef pipe__INCLUDED
#  define pipe__INCLUDED

#include "stdio_.h"

#if defined(__WIN32__) && !(defined(__MINGW32__) && __MINGW32__ == 1)
/*
 * MS Windows has popen and pclose in stdio.h, but under different names.
 * Unfortunately MSVC5 and 6 have a broken implementation of _popen,
 * so we use own.  Our implementation only supports mode "wb".
 */
extern FILE *mswin_popen(const char *cmd, const char *mode);
#  define popen(cmd, mode) mswin_popen(cmd, mode)
/* #  define popen(cmd, mode) _popen(cmd, mode) */
#  define pclose(file) _pclose(file)
#else  /* !__WIN32__ */
/*
 * popen wasn't POSIX-standard from the beginning, and that resulted in
 * inconsistent and incorrect prototypes and other issues. Hence we include
 * a declaration here.
 * But as it is now included in POSIX, we make this optional.
 */
#if !defined(HAVE_POPEN_PROTO) || HAVE_POPEN_PROTO!=1
extern FILE *popen( /* const char *, const char * */ );
extern int pclose(FILE *);
#endif /* HAVE_POPEN_PROTO */

#endif /* !__WIN32__ */

#endif /* pipe__INCLUDED */
