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

/* plsrgb.h - interface for controlling srgb colorspace as used by pcl. */

#ifndef plsrgb_INCLUDED
#  define plsrgb_INCLUDED

/* note each of the following will set up a color rendering dictionary
   if one is not present, possibly reading the dictionary from the
   device. */

/* return an srgb color space to the client */
int pl_cspace_init_SRGB(gs_color_space **ppcs, const gs_state *pgs);

/* set an srgb color */
int pl_setSRGBcolor(gs_state *pgs, float r, float g, float b);

/* true if device does color conversion as a post process */
bool pl_device_does_color_conversion(void);

#endif /* plsrgb_INCLUDED */
