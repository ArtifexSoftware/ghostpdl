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

/* pcdraw.h - PCL5 drawing utilities */

#include "gx.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gsstate.h"
#include "gsrop.h"
#include "gxfixed.h"
#include "pcstate.h"
#include "pcpatrn.h"
#include "pcdraw.h"

/*
 * Set all necessary graphics state parameters for PCL drawing
 * (currently only CTM and clipping region).
 */
  int
pcl_set_graphics_state(
    pcl_state_t *   pcs
)
{
    int             code = pcl_set_ctm(pcs, true);

    return ( code < 0 ? code 
                      : gx_clip_to_rectangle( pcs->pgs,
                                              &(pcs->xfm_state.dev_print_rect)
                                              ) );
}

/*
 * Backwards compatibility function. Now, however, it provides accurate
 * results.
 */
  int
pcl_set_ctm(
    pcl_state_t *   pcs,
    bool            use_pd
)
{
    gs_setmatrix( pcs->pgs,
                  ( use_pd ? &(pcs->xfm_state.pd2dev_mtx)
                           : &(pcs->xfm_state.lp2dev_mtx) )
                  );
    return 0;
}

/*
 * set pcl's drawing color.  Uses pcl state values to determine
 * rotation and translation of patterns.
 *
 * The final operand indicates if a PCL raster (image) is being rendered;
 * special considerations apply in this case. See the comments in pcbipatrn.c
 * pcpatrn.c for further information.
 */
 int
pcl_set_drawing_color(
    pcl_state_t *           pcs,
    pcl_pattern_source_t    type,
    int                     id,
    bool                    for_image
)
{   int                     code;

    code = (pcl_pattern_get_proc_PCL(type))(pcs, id, (int)for_image);
    if (code >= 0) {

        /******* TEMPORARY HACK - make all images opaque ***********/
        if (for_image) {
            gs_setsourcetransparent(pcs->pgs, false);
            gs_settexturetransparent(pcs->pgs, false);
        } else {
            gs_setsourcetransparent(pcs->pgs, pcs->source_transparent );
            gs_settexturetransparent(pcs->pgs, pcs->pattern_transparent);
        }
        gs_setrasterop(pcs->pgs, (gs_rop3_t)pcs->logical_op);
        gs_setfilladjust(pcs->pgs, pcs->grid_adjust, pcs->grid_adjust);
    }
    return 0;
}
