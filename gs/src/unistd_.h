/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
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

#if defined(_MSC_VER) || defined(__BORLANDC__) && defined(__WIN32__) 
#  define fsync(handle) _commit(handle)
#  define read(fd, buf, len) _read(fd, buf, len)
#  define fstat(fd, buf) _fstat(fd, buf)
#else
#  include <unistd.h>
#endif

#endif   /* unistd__INCLUDED */
