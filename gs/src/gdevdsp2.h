/* Copyright (C) 2001, Ghostgum Software Pty Ltd.  All rights reserved.

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
/* gdevdsp2.c */

#ifndef gdevdsp2_INCLUDED
#  define gdevdsp2_INCLUDED

typedef struct gx_device_display_s gx_device_display;

#define gx_device_display_common\
	gx_device_memory *mdev;\
	display_callback *callback;\
	void *pHandle;\
	int nFormat;\
	void *pBitmap;\
	unsigned long ulBitmapSize

/* The device descriptor */
struct gx_device_display_s {
    gx_device_common;
    gx_device_display_common;
};

extern_st(st_device_display);
#define public_st_device_display()	/* in gdevdsp.c */\
  gs_public_st_composite_use_final(st_device_display, gx_device_display,\
    "gx_device_display", display_enum_ptrs, display_reloc_ptrs,\
    gx_device_finalize)

#endif /* gdevdsp2_INCLUDED */
