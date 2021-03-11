/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* pcbiptrn.c - code for PCL built-in patterns */

#include "math_.h"
#include "string_.h"
#include "gstypes.h"            /* for gsstate.h */
#include "gsmatrix.h"           /* for gsstate.h */
#include "gsmemory.h"           /* for gsstate.h */
#include "gsstate.h"
#include "gscoord.h"
#include "pcpatrn.h"
#include "pcuptrn.h"
#include "pcbiptrn.h"
#include "pcstate.h"

/*
 * 16x16 bitmap arrays for the built-in patterns.
 */
static const byte
    bi_data_array[(PCL_NUM_SHADE_PATTERNS +
                   PCL_NUM_CROSSHATCH_PATTERNS) * 2 * 16] = {

    /* shade 1% to 2% */
    0x80, 0x80,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x08, 0x08,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,

    /* shade 3% to 10% */
    0x80, 0x80,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x08, 0x08,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x80, 0x80,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x08, 0x08,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,

    /* shade 11% to 20% */
    0xc0, 0xc0,
    0xc0, 0xc0,
    0x00, 0x00,
    0x00, 0x00,

    0x0c, 0x0c,
    0x0c, 0x0c,
    0x00, 0x00,
    0x00, 0x00,

    0xc0, 0xc0,
    0xc0, 0xc0,
    0x00, 0x00,
    0x00, 0x00,

    0x0c, 0x0c,
    0x0c, 0x0c,
    0x00, 0x00,
    0x00, 0x00,


    /* shade 21% to 35% */
    0xc1, 0xc1,
    0xc1, 0xc1,
    0x80, 0x80,
    0x08, 0x08,

    0x1c, 0x1c,
    0x1c, 0x1c,
    0x08, 0x08,
    0x80, 0x80,

    0xc1, 0xc1,
    0xc1, 0xc1,
    0x80, 0x80,
    0x08, 0x08,

    0x1c, 0x1c,
    0x1c, 0x1c,
    0x08, 0x08,
    0x80, 0x80,


    /* shade 36% to 55% */
    0xc1, 0xc1,
    0xeb, 0xeb,
    0xc1, 0xc1,
    0x88, 0x88,

    0x1c, 0x1c,
    0xbe, 0xbe,
    0x1c, 0x1c,
    0x88, 0x88,

    0xc1, 0xc1,
    0xeb, 0xeb,
    0xc1, 0xc1,
    0x88, 0x88,

    0x1c, 0x1c,
    0xbe, 0xbe,
    0x1c, 0x1c,
    0x88, 0x88,


    /* shade 56% to 80% */
    0xe3, 0xe3,
    0xe3, 0xe3,
    0xe3, 0xe3,
    0xdd, 0xdd,

    0x3e, 0x3e,
    0x3e, 0x3e,
    0x3e, 0x3e,
    0xdd, 0xdd,

    0xe3, 0xe3,
    0xe3, 0xe3,
    0xe3, 0xe3,
    0xdd, 0xdd,

    0x3e, 0x3e,
    0x3e, 0x3e,
    0x3e, 0x3e,
    0xdd, 0xdd,


    /* shade 81% to 99% */
    0xf7, 0xf7,
    0xe3, 0xe3,
    0xf7, 0xf7,
    0xff, 0xff,

    0x7f, 0x7f,
    0x3e, 0x3e,
    0x7f, 0x7f,
    0xff, 0xff,

    0xf7, 0xf7,
    0xe3, 0xe3,
    0xf7, 0xf7,
    0xff, 0xff,

    0x7f, 0x7f,
    0x3e, 0x3e,
    0x7f, 0x7f,
    0xff, 0xff,


    /* cross-hatch 1 (horizontal stripes) */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,

    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0xff, 0xff,

    0xff, 0xff,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,

    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,


    /* cross-hatch 2 (vertical stripes) */
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,

    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,

    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,

    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,


    /* cross-hatch 3 (upper right/lower left diagonal stripes) */
    0x80, 0x03,
    0x00, 0x07,
    0x00, 0x0e,
    0x00, 0x1c,

    0x00, 0x38,
    0x00, 0x70,
    0x00, 0xe0,
    0x01, 0xc0,

    0x03, 0x80,
    0x07, 0x00,
    0x0e, 0x00,
    0x1c, 0x00,

    0x38, 0x00,
    0x70, 0x00,
    0xe0, 0x00,
    0xc0, 0x01,


    /* cross-hatch 4 (upper left/lower right diagonal stripes) */
    0xc0, 0x01,
    0xe0, 0x00,
    0x70, 0x00,
    0x38, 0x00,

    0x1c, 0x00,
    0x0e, 0x00,
    0x07, 0x00,
    0x03, 0x80,

    0x01, 0xc0,
    0x00, 0xe0,
    0x00, 0x70,
    0x00, 0x38,

    0x00, 0x1c,
    0x00, 0x0e,
    0x00, 0x07,
    0x80, 0x03,


    /* cross-hatch 5 (aligned cross-hatch) */
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,

    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0xff, 0xff,

    0xff, 0xff,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,

    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,


    /* cross-hatch 6 (diagnoal cross-hatch) */
    0xc0, 0x03,
    0xe0, 0x07,
    0x70, 0x0e,
    0x38, 0x1c,

    0x1c, 0x38,
    0x0e, 0x70,
    0x07, 0xe0,
    0x03, 0xc0,

    0x03, 0xc0,
    0x07, 0xe0,
    0x0e, 0x70,
    0x1c, 0x38,

    0x38, 0x1c,
    0x70, 0x0e,
    0xe0, 0x07,
    0xc0, 0x03,
};

#define make_pixmap(indx)                                           \
    { (byte *)(bi_data_array + indx * 2 * 16), 2, {16, 16}, 0, 1, 1 }

static const gs_depth_bitmap bi_pixmap_array[PCL_NUM_CROSSHATCH_PATTERNS +
                                             PCL_NUM_SHADE_PATTERNS] = {
    make_pixmap(0),
    make_pixmap(1),
    make_pixmap(2),
    make_pixmap(3),
    make_pixmap(4),
    make_pixmap(5),
    make_pixmap(6),
    make_pixmap(7),
    make_pixmap(8),
    make_pixmap(9),
    make_pixmap(10),
    make_pixmap(11),
    make_pixmap(12)
};

#define bi_cross_offset 7

/*
 * A special pattern, used for rendering images that interact with solid
 * foregrounds.
 *
 * Handling the interaction of rasters and foregrounds in PCL is tricky. PCL
 * foregounds carry a full set of rendering information, including color
 * correction and halftoning information. These may differe from the color
 * correction information and halftone mechanism used to process the raster
 * itself (which is always taken from the current palette at the time the
 * raster is output). The graphic library can accommodate only a single
 * color rendering dictionary and halftone mechanism at one time, hence the
 * problem.
 *
 * The solution is to invoke a second graphic state. Patterns in the graphic
 * library are provided with their own graphic state, so method for handling
 * solid foreground colors is to create a solid, uncolored pattern that is
 * is "on" (assumes the foreground color) everywhere.
 *
 * The following 1x1 pixel texture is used for this prupose. As with the
 * pattern above, two copies of this structure are required: a prototype
 * (qualified as const) and the pattern actually used.
 */
/* Although only a byte is used, the 1bpp rendering code expects at least
 * a short.
 */
static const byte solid_pattern_data[2] = { 0xff, 0xff };

static const gs_depth_bitmap solid_pattern_pixmap = {
    (byte *) & solid_pattern_data[0], 1, {1, 1}, 0, 1, 1
};

/*
 * An "un-solid" pattern, similar to the solid pattern described above, but
 * with only background. This is used primarily to handle the case of an
 * uncolored patter with a white foreground color in GL/2, which such patterns
 * are completely transparent (if pattern transparency is on; note that what
 * the GL/2 documentation describes as source transparency is actually pattern
 * transparency).
 */
/* This only actually needs to be a single byte, but we
 * allocate it to be 2 as this stops address sanitizer
 * reporting (intentional, safe) overreads by
 * mem_copy_color_mono. See bug 696603. */
static const byte unsolid_pattern_data[2] = { 0, 0 };

static const gs_depth_bitmap unsolid_pattern_pixmap = {
    (byte *) & unsolid_pattern_data[0], 1, {1, 1}, 0, 1, 1
};

/*
 * Initialize the built-in patterns
 */
void
pcl_pattern_init_bi_patterns(pcl_state_t * pcs)
{
    memset(pcs->bi_pattern_array, 0, sizeof(pcs->bi_pattern_array));
    pcs->psolid_pattern = 0;
    pcs->punsolid_pattern = 0;
}

/*
 * Clear all built-in patterns. This is normally called during a reset, to
 * conserve memory.
 */
void
pcl_pattern_clear_bi_patterns(pcl_state_t * pcs)
{
    int i;

    for (i = 0; i < countof(pcs->bi_pattern_array); i++) {
        if (pcs->bi_pattern_array[i] != 0) {
            pcl_pattern_free_pattern(pcs->memory,
                                     pcs->bi_pattern_array[i],
                                     "clear PCL built-in patterns");
            pcs->bi_pattern_array[i] = 0;
        }
    }

    if (pcs->psolid_pattern != 0) {
        pcl_pattern_free_pattern(pcs->memory,
                                 pcs->psolid_pattern,
                                 "clear PCL built-in patterns");

        pcs->psolid_pattern = 0;
    }
    if (pcs->punsolid_pattern != 0) {
        pcl_pattern_free_pattern(pcs->memory,
                                 pcs->punsolid_pattern,
                                 "clear PCL built-in patterns");

        pcs->punsolid_pattern = 0;
    }
}

/*
 * pcl patterns are always always 300 dpi but we use the device
 * resolution on devices lower than 300 dpi so we can at least see the
 * patterns on screen resolution devices.  We also provide a #define
 * here for customers that wish to have better patterns at higher
 * resolutions.
 */

/* #define DEVICE_RES_PATTERNS */

static int
pcl_get_pattern_resolution(pcl_state_t * pcs, gs_point * pattern_res)
{
    /* default is 300 */
    pattern_res->x = 300;
    pattern_res->y = 300;
    /* get the current resolutions based on the device. */
    {
        gs_point device_res;
        gx_device *pdev = gs_currentdevice(pcs->pgs);

        device_res.x = pdev->HWResolution[0];
        device_res.y = pdev->HWResolution[1];
#ifdef DEVICE_RES_PATTERNS
        pattern_res->x = device_res.x;
        pattern_res->y = device_res.y;
#else
        /* if both are less than 300 dpi override the 300 dpi default. */
        if ((device_res.x < 300) && (device_res.y < 300)) {
            pattern_res->x = device_res.x;
            pattern_res->y = device_res.y;
        }
#endif
    }
    return 0;
}
#undef DEVICE_RES_PATTERNS
/*
 * Return the pointer to a built-in pattern, building it if necessary.
 */
static pcl_pattern_t *
get_bi_pattern(pcl_state_t * pcs, int indx)
{
    if (pcs->bi_pattern_array[indx] == 0) {
        gs_point pattern_res;
        int code;

        pcl_get_pattern_resolution(pcs, &pattern_res);
        code = pcl_pattern_build_pattern(&(pcs->bi_pattern_array[indx]),
                                        &(bi_pixmap_array[indx]),
                                        pcl_pattern_uncolored,
                                        (int)pattern_res.x,
                                        (int)pattern_res.y, pcs->memory);
        if (code < 0) return NULL;
        pcs->bi_pattern_array[indx]->ppat_data->storage = pcds_internal;
    }
    return pcs->bi_pattern_array[indx];
}

/*
 * For a given intensity value, return the corresponding shade pattern. A
 * null return indicates that a solid pattern should be used - the caller
 * must look at the intensity to determine if it is black or white.
 */
pcl_pattern_t *
pcl_pattern_get_shade(pcl_state_t * pcs, int inten)
{
    pcl_pattern_t *shade = 0;

    if (inten <= 0)
        shade = 0;
    else if (inten <= 2)
        shade = get_bi_pattern(pcs, 0);
    else if (inten <= 10)
        shade = get_bi_pattern(pcs, 1);
    else if (inten <= 20)
        shade = get_bi_pattern(pcs, 2);
    else if (inten <= 35)
        shade = get_bi_pattern(pcs, 3);
    else if (inten <= 55)
        shade = get_bi_pattern(pcs, 4);
    else if (inten <= 80)
        shade = get_bi_pattern(pcs, 5);
    else if (inten <= 99)
        shade = get_bi_pattern(pcs, 6);
    return shade;
}

/*
 * For a given index value, return the corresponding cross-hatch pattern. A
 * null return indicates that the pattern is out of range. The caller must
 * determine what to do in this case.
 */
pcl_pattern_t *
pcl_pattern_get_cross(pcl_state_t * pcs, int indx)
{
    if ((indx < 1) || (indx > 6))
        return 0;
    else
        return get_bi_pattern(pcs, indx + bi_cross_offset - 1);
}

/*
 * Return the solid uncolored pattern, to be used with rasters (see above).
 */
pcl_pattern_t *
pcl_pattern_get_solid_pattern(pcl_state_t * pcs)
{
    if (pcs->psolid_pattern == 0) {
        gs_point pattern_res;
        int code;

        pcl_get_pattern_resolution(pcs, &pattern_res);
        code = pcl_pattern_build_pattern(&(pcs->psolid_pattern),
                                        &solid_pattern_pixmap,
                                        pcl_pattern_uncolored,
                                        (int)pattern_res.x,
                                        (int)pattern_res.y, pcs->memory);
        if (code < 0) return NULL;
        pcs->psolid_pattern->ppat_data->storage = pcds_internal;
    }
    return pcs->psolid_pattern;
}

/*
 * Return the "unsolid" uncolored patterns, to be used with GL/2.
 */
pcl_pattern_t *
pcl_pattern_get_unsolid_pattern(pcl_state_t * pcs)
{
    if (pcs->punsolid_pattern == 0) {
        gs_point pattern_res;
        int code;

        pcl_get_pattern_resolution(pcs, &pattern_res);
        code = pcl_pattern_build_pattern(&(pcs->punsolid_pattern),
                                        &unsolid_pattern_pixmap,
                                        pcl_pattern_uncolored,
                                        (int)pattern_res.x,
                                        (int)pattern_res.y, pcs->memory);
        if (code < 0) return NULL;
        pcs->punsolid_pattern->ppat_data->storage = pcds_internal;
    }
    return pcs->punsolid_pattern;
}
