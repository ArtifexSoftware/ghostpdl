/* Copyright (C) 1989, 1992, 1993, 1994, 1998, 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
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
#endif

#endif /* string__INCLUDED */
