/* Copyright (C) 1989, 1991, 1994 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Common internal definitions for Ghostscript library */

#ifndef gx_INCLUDED
#  define gx_INCLUDED

#include "stdio_.h"		/* includes std.h */
#include "gserror.h"
#include "gsio.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gdebug.h"

/* Configuration swtches - development needs only. */
/* Users should not modify them. */
#define NEW_TT_INTERPRETER 1
#define TT_GRID_FITTING (NEW_TT_INTERPRETER && 1) /* old code = 0, new code = 1. */
#define CURVED_TRAPEZOID_FILL 1 /* old code = 0, new code = 1. */
/* The following switches are only important with CURVED_TRAPEZOID_FILL 1. */
#   define CURVED_TRAPEZOID_FILL_HEAVY_TEST 0  /* Temporarily used for a debug purpose. */
#   define CURVED_TRAPEZOID_FILL0_COMPATIBLE 1 /* Temporarily used for a backward compatibility. 
                                               The implementation of 0 is not completed yet. */
#   define FLATTENED_ITERATOR_BACKSCAN 1 /* 0 - simpler code, 1 - faster and smaller RAM. */
#   define FLATTENED_ITERATOR_SELFTEST 0 /* Temporarily used for a debug purpose. */
#   define FLATTENED_ITERATOR_HEAVY_SELFTEST 0 /* Temporarily used for a debug purpose. */


/* Define opaque types for the graphics state. */
/* This is used so pervasively that we define it here, */
/* rather than at a higher level as perhaps would be more appropriate. */
#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;

#endif
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;

#endif

#endif /* gx_INCLUDED */
