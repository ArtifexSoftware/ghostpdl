/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Generic substitute for locale.h */

#ifndef locale__INCLUDED
#  define locale__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/* If the system has setlocale, include the real <locale.h>; otherwise,
 * just fake it. */
#ifdef HAVE_SETLOCALE
#  include <locale.h>
#else
#  define setlocale(c,l) ((char *)0)
#endif

#endif /* locale__INCLUDED */
