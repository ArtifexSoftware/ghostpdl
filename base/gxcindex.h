/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Define the device color index type and macros */

#ifndef gxcindex_INCLUDED
#  define gxcindex_INCLUDED

#include "stdint_.h"		/* for uint64_t and uint32_t */

/*
 * Define the maximum number of components in a device color.
 * The minimum value is 4, to handle CMYK; the maximum value is
 * ARCH_SIZEOF_COLOR_INDEX * 8, since for larger values, there aren't enough
 * bits in a gx_color_index to have even 1 bit per component.
 */
#define GX_DEVICE_COLOR_MAX_COMPONENTS (ARCH_SIZEOF_GX_COLOR_INDEX * 8)

/*
 * We might change gx_color_index to a pointer or a structure in the
 * future.  These disabled options help us assess how much disruption
 * such a change might cause.
 */
/*#define TEST_CINDEX_POINTER*/
/*#define TEST_CINDEX_STRUCT*/

/*
 * Internally, a (pure) device color is represented by opaque values of
 * type gx_color_index, which are tied to the specific device.  The driver
 * maps between these values and RGB[alpha] or CMYK values.  In this way,
 * the driver can convert RGB values to its most natural color representation,
 * and have the graphics library cache the result.
 */

#ifdef TEST_CINDEX_STRUCT

/* Define the type for device color index (pixel value) data. */
typedef struct { ulong value[2]; } gx_color_index_data;

#else  /* !TEST_CINDEX_STRUCT */

/* Define the type for device color index (pixel value) data. */
#ifdef GX_COLOR_INDEX_TYPE
enum { ARCH_SIZEOF_GX_COLOR_INDEX__must_equal__sizeof_GX_COLOR_INDEX_TYPE = 1/!!(ARCH_SIZEOF_GX_COLOR_INDEX == sizeof(GX_COLOR_INDEX_TYPE)) };
typedef GX_COLOR_INDEX_TYPE gx_color_index_data;
#else
/* Usually this is set by the makefile, but if not, set
   it using the ARCH_SIZEOF_GX_COLOR_INDEX */
#  if ARCH_SIZEOF_GX_COLOR_INDEX == 8
      typedef uint64_t gx_color_index_data;
#  else
      typedef uint32_t gx_color_index_data;
#  endif
#endif

#endif /* (!)TEST_CINDEX_STRUCT */

#ifdef TEST_CINDEX_POINTER

/* Define the type for device color indices (pixel values). */
typedef gx_color_index_data * gx_color_index;
#define ARCH_SIZEOF_COLOR_INDEX ARCH_SIZEOF_PTR

extern const gx_color_index_data gx_no_color_index_data;
#define gx_no_color_index_values (&gx_no_color_index_data)
#define gx_no_color_index (&gx_no_color_index_data)

#else  /* !TEST_CINDEX_POINTER */

#define ARCH_SIZEOF_COLOR_INDEX sizeof(gx_color_index_data)

/* Define the type for device color indices (pixel values). */
typedef gx_color_index_data gx_color_index;

/*
 * Define the 'transparent' or 'undefined' color index.
 */
#define gx_no_color_index_value (~0)	/* no cast -> can be used in #if */
/*
 * There was a comment here about the SGI C compiler provided with Irix 5.2
 * giving error messages.  I hope that was fixed when the value of gx_no_color_index
 * was changed from (-1) to (~0).  If not then let us know.
 */
#define gx_no_color_index ((gx_color_index)gx_no_color_index_value)

#endif /* (!)TEST_CINDEX_POINTER */

#endif /* gxcindex_INCLUDED */
