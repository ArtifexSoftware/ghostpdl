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


/* Generic substitute for stdio.h */

#ifndef stdio__INCLUDED
#  define stdio__INCLUDED

/*
 * This is here primarily because we must include std.h before
 * any file that includes sys/types.h.
 */
#include "std.h"
#include <stdio.h>

#ifdef VMS
/* VMS prior to 7.0 doesn't have the unlink system call.  Use delete instead. */
#  ifdef __DECC
#    include <unixio.h>
#  endif
#  if ( __VMS_VER < 70000000 )
#    define unlink(fname) delete(fname)
#  endif
#else
#if !defined(const) && !defined(_WIN32) && !defined(_WIN64)
/*
 * Other systems may or may not declare unlink in stdio.h;
 * if they do, the declaration will be compatible with this one, as long
 * as const has not been disabled by defining it to be the empty string.
 */
int unlink(const char *);
#endif

#endif

/*
 * Plan 9 has a system function called sclose, which interferes with the
 * procedure defined in stream.h.  The following makes the system sclose
 * inaccessible, but avoids the name clash.
 */
#ifdef Plan9
#  undef sclose
#  define sclose(s) Sclose(s)
#endif

/* Patch a couple of things possibly missing from stdio.h. */
#ifndef SEEK_SET
#  define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#  define SEEK_CUR 1
#endif
#ifndef SEEK_END
#  define SEEK_END 2
#endif

#if defined(_MSC_VER)
#  define fdopen(handle,mode) _fdopen(handle,mode)
#  define fileno(file) _fileno(file)
#  if _MSC_VER < 1500	/* VS 2008 has vsnprintf */
#    define vsnprintf _vsnprintf
#  endif
#  if _MSC_VER<1900
/* Microsoft Visual C++ 2005  doesn't properly define snprintf  */
/* But, finally, with VS 2014 and above, Microsoft has snprintf */
        int snprintf(char *buffer, size_t count, const char *format , ...);
#  endif
#endif	/* _MSC_VER */

/* for our non-localizing (v)s(n)printf() functions */
/* only *really* required for floating point conversions */
#include "gssprintf.h"

#ifndef sprintf
#define sprintf DO_NOT_USE_SPRINTF
#endif


#ifndef fopen
#define fopen DO_NOT_USE_FOPEN
#endif

#endif /* stdio__INCLUDED */
