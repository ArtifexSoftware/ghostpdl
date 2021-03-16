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


/* pccprint.c - PCL5c print model commands */

#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcfont.h"
#include "gsmatrix.h"           /* for gsstate.h */
#include "gsstate.h"
#include "gsrop.h"

/*
 * ESC * l <rop> O
 *
 * Set logical operation.
 */
static int
pcl_logical_operation(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint rop = uint_arg(pargs);

    if (pcs->raster_state.graphics_mode)
        return 0;
    if (rop > 255)
        return e_Range;

    pcs->logical_op = rop;

    /* use the 5c convention; in 5e, the
     * underline is not broken by a change in
     * the logical operation */
    return pcl_break_underline(pcs);
}

/*
 * ESC * l <bool> R
 *
 * Set prixel placement. Note that this feature is not yet properly
 * implemented.
 */
static int
pcl_pixel_placement(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint i = uint_arg(pargs);

    if (pcs->raster_state.graphics_mode)
        return 0;
    if (i > 1)
        return 0;
    pcs->pp_mode = i;
    return 0;
}

/*
 * Initialization
 */
static int
pccprint_do_registration(pcl_parser_state_t * pcl_parser_state,
                         gs_memory_t * pmem)
{
    /* Register commands */
    DEFINE_CLASS('*') {
        'l', 'O',
            PCL_COMMAND("Logical Operation",
                        pcl_logical_operation,
                        pca_neg_ok | pca_big_error | pca_in_rtl |
                        pca_raster_graphics)
    }, {
        'l', 'R',
            PCL_COMMAND("Pixel Placement",
                        pcl_pixel_placement,
                        pca_neg_ok | pca_big_ignore | pca_in_rtl |
                        pca_raster_graphics)
    }, END_CLASS return 0;
}

static int
pccprint_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    static const uint mask = (pcl_reset_initial
                              | pcl_reset_printer | pcl_reset_overlay);

    if ((type & mask) != 0) {
        pcs->logical_op = 252;
        pcs->pp_mode = 0;
    }

    return 0;
}

const pcl_init_t pccprint_init =
    { pccprint_do_registration, pccprint_do_reset, 0 };
