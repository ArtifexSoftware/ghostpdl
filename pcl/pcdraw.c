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
#include "pcht.h"
#include "pccrd.h"
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
 *
 * PCL no longer uses the graphic library transparency mechanism.
 */
 int
pcl_set_drawing_color(
    pcl_state_t *           pcs,
    pcl_pattern_source_t    type,
    int                     id,
    bool                    for_image
)
{   int                     code;

    if (type == pcl_pattern_raster_cspace)
        code = (pcl_pattern_get_proc_PCL(type))(pcs, 0, 0);
    else
        code = (pcl_pattern_get_proc_PCL(type))(pcs, id, (int)for_image);
    if (code >= 0) {
        gs_setrasterop(pcs->pgs, (gs_rop3_t)pcs->logical_op);
        gs_setfilladjust(pcs->pgs, pcs->grid_adjust, pcs->grid_adjust);
    }
    return 0;
}

/*
 * The pcl state structure retains information concerning the current contents
 * of the grpahic state. To keep this information synchronized with the
 * graphic state itself, this information is kept in a stack that is pushed
 * or poped, repsectively, for each invocation of gsave and grestore.
 * This opertion is prefromed by the following pair of routines, which should
 * be used in place of gsave and grestore.
 */

private_st_gstate_ids_t();

  int
pcl_gsave(
    pcl_state_t *       pcs
)
{
    int                 code = 0;
    pcl_gstate_ids_t *  pids = gs_alloc_struct( pcs->memory,
                                                pcl_gstate_ids_t,
                                                &st_gstate_ids_t,
                                                "PCL gsave"
                                                );

    if (pids == 0)
        return e_Memory;

    if ((code = gs_gsave(pcs->pgs)) >= 0) {
        pids->prev = pcs->pids->prev;
        pcs->pids->prev = pids;
        pcl_ccolor_init_from(pids->pccolor, pcs->pids->pccolor);
        pcl_ht_init_from(pids->pht, pcs->pids->pht);
        pcl_crd_init_from(pids->pcrd, pcs->pids->pcrd);
    } else
        gs_free_object(pcs->memory, pids, "PCL gsave");

    return code;
}

  int
pcl_grestore(
    pcl_state_t *       pcs
)
{
    pcl_gstate_ids_t *  pids = pcs->pids->prev;
    int                 code = 0;

    /* check for bottom of graphic state stack */
    if (pids == 0)
        return e_Range;
    if ((code = gs_grestore(pcs->pgs)) >= 0) {
        pcs->pids->prev = pids->prev;
        pcl_ccolor_copy_from(pcs->pids->pccolor, pids->pccolor);
        pcl_ccolor_release(pids->pccolor);
        pcl_ht_copy_from(pcs->pids->pht, pids->pht);
        pcl_ht_release(pids->pht);
        pcl_crd_copy_from(pcs->pids->pcrd, pids->pcrd);
        pcl_crd_release(pids->pcrd);
        gs_free_object(pcs->memory, pids, "PCL grestore");
    }

    return code;
}

  void
pcl_init_gstate_stk(
    pcl_state_t *       pcs
)
{
    pcl_gstate_ids_t *  pids = gs_alloc_struct( pcs->memory,
                                                pcl_gstate_ids_t,
                                                &st_gstate_ids_t,
                                                "PCL gsave"
                                                );
    if (pids != 0) {    /* otherwise will crash soon enough */
        pids->prev = 0;
        pids->pccolor = 0;
        pids->pht = 0;
        pids->pcrd = 0;
    }
    pcs->pids = pids;
}
