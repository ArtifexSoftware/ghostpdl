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


/* pcdither.h - PCL user-defined dither object */

#ifndef pcdither_INCLUDED
#define pcdither_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "pcommand.h"

/*
 * PCL user-defined dither matrix structure.
 *
 * This structure is defined by HP.
 *
 * NB: don't take sizeof of this structure.
 *
 */
typedef struct pcl__udither_s
{
    byte format;
    byte nplanes;
    byte height[2];
    byte width[2];
    byte data[1 /* actually height * width * nplanes */ ];
} pcl__udither_t;

/*
 * Header for tha PCL user-defined dither object. This exists to provide a
 * reference-counting mechanism for the user-defined dithers. It also is a
 * convenient location in which to store endian-adjusted size information
 * about the dither.
 */
typedef struct pcl_udither_s
{
    rc_header rc;
    uint height;
    uint width;
    const pcl__udither_t *ptbl;
} pcl_udither_t;

/*
 * Copy, init, and release macros.
 */
#define pcl_udither_init_from(pto, pfrom)   \
    BEGIN                                   \
    rc_increment(pfrom);                    \
    (pto) = (pfrom);                        \
    END

#define pcl_udither_copy_from(pto, pfrom)           \
    BEGIN                                           \
    if ((pto) != (pfrom)) {                         \
        rc_increment(pfrom);                        \
        rc_decrement(pto, "pcl_udither_copy_from"); \
        (pto) = (pfrom);                            \
    }                                               \
    END

#define pcl_udither_release(pdither)            \
    rc_decrement(pdither, "pcl_udither_release")

/*
 * Macros for retrieving information from a pcl_udither_t structure.
 */
#define pcl_udither_get_nplanes(pdither)    ((pdither)->ptbl->nplanes)
#define pcl_udither_get_height(pdither)     ((pdither)->height)
#define pcl_udither_get_width(pdither)      ((pdither)->width)

#define pcl_udither_get_size(pdither)   ((pdither)->height * (pdither)->width)

/*
 * Get a pointer to the appropriate portion of the dither table in a user
 * defined dither structure.
 */
const byte *pcl_udither_get_threshold(const pcl_udither_t * pdither,
                                      int indx);

/*
 * External access to the user-defined dither matrix machinery.
 */
extern const pcl_init_t pcl_udither_init;

#endif /* pcdither_INCLUDED */
