/* Portions Copyright (C) 2004 artofcode LLC.
   Portions Copyright (C) 1996, 2004 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* plsrgb.h - interface for controlling srgb colorspace */

/* initialize srgb color space */
#ifndef plsrgb_INCLUDED
#  define plsrgb_INCLUDED

int pl_cspace_init_SRGB(gs_color_space **ppcs, const gs_state *pgs);

/* set an srgb color */
int pl_setSRGB(gs_state *pgs, float r, float g, float b);

/* build a color rendering dictionary to be used with the srgb color
   space */
int pl_build_crd(gs_state *pgs);

/* free color spaces and the crd associated with setting up srgb */
void pl_free_srgb(gs_state *pgs);

#endif /* plsrgb_INCLUDED */
