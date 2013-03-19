/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* plsrgb.h - interface for controlling srgb colorspace as used by pcl. */

#ifndef plsrgb_INCLUDED
#  define plsrgb_INCLUDED

/* note each of the following will set up a color rendering dictionary
   if one is not present, possibly reading the dictionary from the
   device. */

/* return an srgb color space to the client */
int pl_cspace_init_SRGB(gs_color_space ** ppcs, const gs_state * pgs);

/* set an srgb color */
int pl_setSRGBcolor(gs_state * pgs, float r, float g, float b);

/* true if device does color conversion as a post process */
bool pl_device_does_color_conversion(void);

#endif /* plsrgb_INCLUDED */
