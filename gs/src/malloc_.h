/* Copyright (C) 1989, 1992, 1996, 1998 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Generic substitute for Unix malloc.h */

#ifndef malloc__INCLUDED
#  define malloc__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

#ifdef __TURBOC__
#  include <alloc.h>
#else
#  if defined(BSD4_2) || defined(apollo) || defined(vax) || defined(sequent) || defined(UTEK)
#    if defined(_POSIX_SOURCE) || (defined(__STDC__) && (!defined(sun) || defined(__svr4__)))	/* >>> */
#      include <stdlib.h>
#    else			/* Ancient breakage */
extern char *malloc();
extern void free();

#    endif
#  else
#    if defined(_HPUX_SOURCE) || defined(__CONVEX__) || defined(__convex__) || defined(__OSF__) || defined(__386BSD__) || defined(_POSIX_SOURCE) || defined(__STDC__) || defined(VMS)
#      include <stdlib.h>
#    else
#      include <malloc.h>
#    endif			/* !_HPUX_SOURCE, ... */
#  endif			/* !BSD4_2, ... */
#endif /* !__TURBOC__ */

/* (At least some versions of) Linux don't have a working realloc.... */
#ifdef linux
#  define malloc__need_realloc
void *gs_realloc(P3(void *, size_t, size_t));

#else
#  define gs_realloc(ptr, old_size, new_size) realloc(ptr, new_size)
#endif

#endif /* malloc__INCLUDED */
