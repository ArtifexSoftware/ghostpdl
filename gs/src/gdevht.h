/* Copyright (C) 1995, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$RCSfile$ $Revision$ */
/* Halftoning "device" definitions */
/* Requires gxdevice.h */

#ifndef gdevht_INCLUDED
#  define gdevht_INCLUDED

#include "gzht.h"

/*
 * A halftoning device converts between a non-halftoned device color space
 * (e.g., 8-bit gray) and a halftoned space (e.g., 1-bit black and white).
 * We represent colors by packing the two colors being halftoned and the
 * halftone level into a gx_color_index.
 */
typedef struct gx_device_ht_s {
    gx_device_forward_common;
    /* Following + target are set before opening. */
    const gx_device_halftone *dev_ht;
    gs_int_point ht_phase;	/* halftone phase from gstate */
    /* Following are computed when device is opened. */
    int color_shift;		/* # of bits of color */
    int level_shift;		/* = color_shift * 2 */
    gx_color_index color_mask;	/* (1 << color_shift) - 1 */
    gs_int_point phase;		/* halftone tile offset */
} gx_device_ht;

#endif /* gdevht_INCLUDED */
