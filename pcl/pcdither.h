/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* pcdither.h - PCL user-defined dither object */

#ifndef pcdither_INCLUDED
#define pcdither_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "pcstate.h"
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
extern  const byte *    pcl_udither_get_threshold(
    const pcl_udither_t *   pdither,
    int                     indx
);

/*
 * External access to the user-defined dither matrix machinery.
 */
extern  const pcl_init_t    pcl_udither_init;

#endif  	/* pcdither_INCLUDED */
