/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Interface to DevicePixel color space */
/* Requires gscspace.h */

#ifndef gscpixel_INCLUDED
#  define gscpixel_INCLUDED

/* Initialize a DevicePixel color space. */
int gs_cspace_init_DevicePixel(P2(gs_color_space * pcs, int depth));

#endif /* gscpixel_INCLUDED */
