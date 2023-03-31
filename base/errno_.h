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

/* Generic substitute for Unix errno.h */

#ifndef errno__INCLUDED
#  define errno__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/* All environments provide errno.h, but in some of them, errno.h */
/* only defines the error numbers, and doesn't declare errno. */
#include <errno.h>
#ifndef errno			/* in case it was #defined (very implausible!) */
extern int errno;
#endif

/* Some systems don't have EILSEQ, so (following GNU iconv) we use */
/* ENOENT instead if necessary. */
#ifndef EILSEQ
#  define EILSEQ ENOENT
#endif

#endif /* errno__INCLUDED */
