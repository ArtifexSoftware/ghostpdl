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


/* Generic substitute for Unix unistd.h */

#ifndef unistd__INCLUDED
#  define unistd__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/*
 * It's likely that you will have to edit the next lines on some Unix
 * and most non-Unix platforms, since there is no standard (ANSI or
 * otherwise) for where to find these definitions.
 */

#ifdef __OS2__
#  include <io.h>
#endif
#ifdef __WIN32__
#  include <io.h>
#endif

#if defined(_MSC_VER)
#  define fsync(handle) _commit(handle)
#  define read(fd, buf, len) _read(fd, buf, len)
#  define isatty(fd) _isatty(fd)
#  define setmode(fd, mode) _setmode(fd, mode)
#  define dup(fd) _dup(fd)
#  define open(fname, flags, mode) _open(fname, flags, mode)
#  define close(fd) _close(fd)
#elif defined(__BORLANDC__) && defined(__WIN32__)
#  define fsync(handle) _commit(handle)
#  define read(fd, buf, len) _read(fd, buf, len)
#  define isatty(fd) _isatty(fd)
#  define setmode(fd, mode) _setmode(fd, mode)
#else
   /* _XOPEN_SOURCE 500 define is needed to get
    * access to pread and pwrite */
#  define _XOPEN_SOURCE 500
#  define __USE_UNIX98
#  include <unistd.h>
#endif

#endif   /* unistd__INCLUDED */
