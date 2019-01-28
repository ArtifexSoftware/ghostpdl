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


/* Client color structure definition */

#ifndef gsccolor_INCLUDED
#  define gsccolor_INCLUDED

#include "gsstype.h"		/* for extern_st */

/* Pattern instance, usable in color. */
typedef struct gs_pattern_instance_s gs_pattern_instance_t;

/*
 * Define the maximum number of components in a client color.
 * This must be at least 4, and should be at least 6 to accommodate
 * hexachrome DeviceN color spaces. We saw 9-component (bug 691002)
 * and 13-component (bug 691425) colors in the wild.   Also define
 * the maximum number of colorants that we will support within a
 * single DeviceN color space.  AR supports 32.
 */
#ifndef GS_CLIENT_COLOR_MAX_COMPONENTS		/* Allow override with XCFLAGS */
#  define GS_CLIENT_COLOR_MAX_COMPONENTS (64)
#endif

#ifndef MAX_COMPONENTS_IN_DEVN		
#  define MAX_COMPONENTS_IN_DEVN (64)
#endif

/* There is a speed penalty for supporting lots of spot colors, so certain
 * devices (tiffsep, tiffsep1, psdcmyk, etc) offer -dMaxSpots. This allows
 * us to compile in a high 'hard' limit on the number of components
 * (GS_CLIENT_COLOR_MAX_COMPONENTS) and yet to allow runtime selection of the
 * real number to be used.
 *
 * GS_SOFT_MAX_SPOTS is the maximum number of spots we allow by default;
 * currently this is set to 10, to match the (historical) limit of 14
 * components that Ghostscript has shipped with for years.
 */
#ifndef GS_SOFT_MAX_SPOTS
#define GS_SOFT_MAX_SPOTS 10
#endif

/* Paint (non-Pattern) colors */
typedef struct gs_paint_color_s {
    float values[GS_CLIENT_COLOR_MAX_COMPONENTS];
    /* CAUTION: The shading decomposition algorithm may allocate
       a smaller space when a small number of color components is in use.
    */
} gs_paint_color;

/* General colors */
typedef struct gs_client_color_s gs_client_color;

struct gs_client_color_s {
    gs_pattern_instance_t *pattern;
    gs_paint_color paint;	/* also color for uncolored pattern */
    /* CAUTION: gs_paint_color structure must be the last field in
       gs_client_color_s to allow allocating a smaller space when
       a small number of color components is in use.
    */
};

extern_st(st_client_color);
#define public_st_client_color() /* in gscolor.c */\
  gs_public_st_ptrs1(st_client_color, gs_client_color, "gs_client_color",\
    client_color_enum_ptrs, client_color_reloc_ptrs, pattern)
#define st_client_color_max_ptrs 1

/* Define the color space for a transparency */
/* Used to keep track of parent versus child */
/* color space changes with Smask and for */
/* blending */
typedef enum {
    GRAY_SCALE,
    DEVICE_RGB,
    DEVICE_CMYK,
    CIE_XYZ,
    DEVICEN,
    ICC,
    UNKNOWN,
    OTHER
} gs_transparency_color_t;


/*
 * Color model component polarity. An "unknown" value has been added to
 * this enumeration.  This is used for devices and color spaces.
 */
typedef enum {
    GX_CINFO_POLARITY_UNKNOWN = -1,
    GX_CINFO_POLARITY_SUBTRACTIVE = 0,
    GX_CINFO_POLARITY_ADDITIVE
} gx_color_polarity_t;

#endif /* gsccolor_INCLUDED */
