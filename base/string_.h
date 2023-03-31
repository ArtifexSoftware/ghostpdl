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


/* Generic substitute for Unix string.h */

#ifndef string__INCLUDED
#  define string__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

#ifdef BSD4_2
#  include <strings.h>
#  define strchr index
#else
#  ifdef MEMORY__NEED_MEMMOVE
#    undef memmove		/* This is disgusting, but so is GCC */
#  endif
#  include <string.h>
#  if defined(THINK_C)
        /* Patch strlen to return a uint rather than a size_t. */
#    define strlen (uint)strlen
#  endif
#  ifdef MEMORY__NEED_MEMMOVE
#    define memmove(dest,src,len) gs_memmove(dest,src,len)
#  endif
#  ifdef _MSC_VER
#    define strcasecmp stricmp		/* MSC doesn't have the POSIX functions */
#    define strncasecmp strnicmp
#  endif
#endif

#include "gsstrtok.h"
#include "gsstrl.h"

#define strtok DO_NOT_USE_STRTOK

#endif /* string__INCLUDED */
