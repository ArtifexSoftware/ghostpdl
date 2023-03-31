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


/* Generic substitute for Unix malloc.h */

#ifndef malloc__INCLUDED
#  define malloc__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/* This is a good place to #define MEMENTO, if you're using it. */

#include "bobbin.h"
#include "memento.h"

#if defined(BSD4_2) || defined(apollo) || defined(vax) || defined(sequent) || defined(UTEK)
#  if defined(_POSIX_SOURCE) || (defined(__STDC__) && (!defined(sun) || defined(__svr4__)))   /* >>> */
#    include <stdlib.h>
#  else                       /* Ancient breakage */
extern char *malloc();
extern void free();

#  endif
#else
#  if defined(_HPUX_SOURCE) || defined(__CONVEX__) || defined(__convex__) || defined(__OSF__) || defined(__386BSD__) || defined(_POSIX_SOURCE) || defined(__STDC__) || defined(VMS)
#    include <stdlib.h>
#  else
#    include <malloc.h>
#  endif                      /* !_HPUX_SOURCE, ... */
#endif                        /* !BSD4_2, ... */

/* (At least some versions of) Linux don't have a working realloc.... */
#ifdef linux
#  define malloc__need_realloc
void *gs_realloc(void *, size_t, size_t);

#else
#  define gs_realloc(ptr, old_size, new_size) realloc(ptr, new_size)
#endif

#endif /* malloc__INCLUDED */
