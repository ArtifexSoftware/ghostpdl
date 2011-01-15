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

/* pcdraw.c - PCL5 drawing utilities */

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
    return ( code < 0 ? code : gs_initclip(pcs->pgs) );
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
    return gs_setmatrix( pcs->pgs,
                         ( use_pd ? &(pcs->xfm_state.pd2dev_mtx)
                           : &(pcs->xfm_state.lp2dev_mtx) )
                         );
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

    /* use PCL's pattern transparency */
    pcs->pattern_transparent = pcs->pcl_pattern_transparent;

    if (type == pcl_pattern_raster_cspace)
        code = (pcl_pattern_get_proc_PCL(type))(pcs, 0, true);
    else
        code = (pcl_pattern_get_proc_PCL(type))(pcs, id, (int)for_image);
    if (code >= 0) {
        gs_setrasterop(pcs->pgs, (gs_rop3_t)pcs->logical_op);
        gs_setfilladjust(pcs->pgs, 0.0, 0.0);
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

    pids->pht = 0; 
    pids->pcrd = 0;
    pids->pccolor = 0;

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
    if (pcs == 0 || pcs->pids == 0 || pids == 0)
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

void
pcl_free_gstate_stk(pcl_state_t *pcs)
{
    gs_free_object(pcs->memory, pcs->pids, "PCL grestore");
}
		    
