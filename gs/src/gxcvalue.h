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
/* Definition of device color values */

#ifndef gxcvalue_INCLUDED
#  define gxcvalue_INCLUDED

/* Define the type for gray or RGB values at the driver interface. */
typedef unsigned short gx_color_value;

#define arch_sizeof_gx_color_value arch_sizeof_short
/* We might use less than the full range someday. */
/* ...bits must lie between 8 and 16. */
#define gx_color_value_bits (sizeof(gx_color_value) * 8)
#define gx_max_color_value ((gx_color_value)((1L << gx_color_value_bits) - 1))
#define gx_color_value_to_byte(cv)\
  ((cv) >> (gx_color_value_bits - 8))
#define gx_color_value_from_byte(cb)\
  (((cb) << (gx_color_value_bits - 8)) + ((cb) >> (16 - gx_color_value_bits)))

/* Convert between gx_color_values and fracs. */
#define frac2cv(fr) frac2ushort(fr)
#define cv2frac(cv) ushort2frac(cv)

#endif /* gxcvalue_INCLUDED */
