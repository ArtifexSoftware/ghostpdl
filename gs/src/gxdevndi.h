/* Copyright (C) 2002 artofcode LLC.  All rights reserved.
  
   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$ */
/* Interface to gxdevndi.c */

/* This is the halfoning handlers for DeviceN colors */

#ifndef gxdevndi_INCLUDED
#  define gxdevndi_INCLUDED

#include "gxfrac.h"

#ifndef gx_device_halftone_DEFINED
#  define gx_device_halftone_DEFINED
typedef struct gx_device_halftone_s gx_device_halftone;
#endif

/* Render a color, possibly by halftoning. */
/* Return as for gx_render_[device_]gray. */
int gx_render_device_color_devn(P10(frac red, frac green, frac blue, frac white,
			       bool cmyk, gx_color_value alpha,
			       gx_device_color * pdevc, gx_device * dev,
			       gx_device_halftone * pdht,
			       const gs_int_point * ht_phase));


#endif /* gxdevndi_INCLUDED */
