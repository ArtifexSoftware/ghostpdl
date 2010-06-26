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
/*$Id$ */

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
typedef struct pcl__udither_s {
    byte             format;
    byte             nplanes;
    byte             height[2];
    byte             width[2];
    byte             data[1 /* actually height * width * nplanes */ ];
} pcl__udither_t;

/*
 * Header for tha PCL user-defined dither object. This exists to provide a
 * reference-counting mechanism for the user-defined dithers. It also is a
 * convenient location in which to store endian-adjusted size information
 * about the dither.
 */
typedef struct pcl_udither_s {
    rc_header               rc;
    uint                    height;
    uint                    width;
    const pcl__udither_t *  ptbl;
} pcl_udither_t;

#define private_st_udither_t()                              \
    gs_private_st_ptrs1( st_udither_t,                      \
                         pcl_udither_t,                     \
                         "pcl user defined dither matrix",  \
                         udither_enum_ptr,                  \
                         udither_reloc_ptr,                 \
                         ptbl                               \
                         )

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
const byte *pcl_udither_get_threshold(
    const pcl_udither_t *   pdither,
    int                     indx
);

/*
 * External access to the user-defined dither matrix machinery.
 */
extern  const pcl_init_t    pcl_udither_init;

#endif  	/* pcdither_INCLUDED */
