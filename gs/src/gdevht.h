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
