/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* pcdraw.c - PCL5 drawing utilities */

#include "gx.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gspath.h"
#include "gsstate.h"
#include "gsrop.h"
#include "gxfixed.h"
#include "pcstate.h"
#include "pcht.h"
#include "pcpatrn.h"
#include "pcdraw.h"

#include "gzstate.h"    /* for gstate */
#include "gxdcolor.h"   /* for gx_set_dev_color */

/*
 * Set all necessary graphics state parameters for PCL drawing
 * (currently only CTM and clipping region).
 */
int
pcl_set_graphics_state(pcl_state_t * pcs)
{
    int code = pcl_set_ctm(pcs, true);

    return (code < 0 ? code : gs_initclip(pcs->pgs));
}

/*
 * Backwards compatibility function. Now, however, it provides accurate
 * results.
 */
int
pcl_set_ctm(pcl_state_t * pcs, bool use_pd)
{
    return gs_setmatrix(pcs->pgs, (use_pd ? &(pcs->xfm_state.pd2dev_mtx)
                                   : &(pcs->xfm_state.lp2dev_mtx))
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
pcl_set_drawing_color(pcl_state_t * pcs,
                      pcl_pattern_source_t type, int id, bool for_image)
{
    int code;

    /* use PCL's pattern transparency */
    pcs->pattern_transparent = pcs->pcl_pattern_transparent;

    code = pcl_ht_set_halftone(pcs);
    if (code < 0)
        return code;

    if (type == pcl_pattern_raster_cspace)
        code = (pcl_pattern_get_proc_PCL(type)) (pcs, 0, true);
    else
        code = (pcl_pattern_get_proc_PCL(type)) (pcs, id, (int)for_image);
    if (code >= 0) {
        code = gs_setrasterop(pcs->pgs, (gs_rop3_t) pcs->logical_op);
        if (code < 0)
            return code;
        gs_setfilladjust(pcs->pgs, 0.0, 0.0);
        code = gx_set_dev_color(pcs->pgs);
        if (code == gs_error_Remap_Color)
            code = pixmap_high_level_pattern(pcs->pgs);
        return code;
    }
    return code;
}

/*
 * The pcl state structure retains information concerning the current contents
 * of the grpahic state. To keep this information synchronized with the
 * graphic state itself, this information is kept in a stack that is pushed
 * or poped, repsectively, for each invocation of gsave and grestore.
 * This opertion is prefromed by the following pair of routines, which should
 * be used in place of gsave and grestore.
 */

gs_private_st_simple(st_gstate_ids_t, pcl_gstate_ids_t,
                     "PCL graphics state tracker");

int
pcl_gsave(pcl_state_t * pcs)
{
    int code = 0;
    pcl_gstate_ids_t *pids = gs_alloc_struct(pcs->memory,
                                             pcl_gstate_ids_t,
                                             &st_gstate_ids_t,
                                             "PCL gsave");

    if (pids == 0)
        return e_Memory;

    pids->pht = 0;
    pids->pccolor = 0;

    if ((code = gs_gsave(pcs->pgs)) >= 0) {
        pids->prev = pcs->pids->prev;
        pcs->pids->prev = pids;
        pcl_ccolor_init_from(pids->pccolor, pcs->pids->pccolor);
        pcl_ht_init_from(pids->pht, pcs->pids->pht);
    } else
        gs_free_object(pcs->memory, pids, "PCL gsave");

    return code;
}

int
pcl_grestore(pcl_state_t * pcs)
{
    pcl_gstate_ids_t *pids;
    int code = 0;

    /* check for bottom of graphic state stack */
    if (pcs == 0 || pcs->pids == 0 || pcs->pids->prev == 0)
        return e_Range;

    pids = pcs->pids->prev;

    if ((code = gs_grestore(pcs->pgs)) >= 0) {
        pcs->pids->prev = pids->prev;
        pcl_ccolor_copy_from(pcs->pids->pccolor, pids->pccolor);
        pcl_ccolor_release(pids->pccolor);
        pcl_ht_copy_from(pcs->pids->pht, pids->pht);
        pcl_ht_release(pids->pht);
        gs_free_object(pcs->memory, pids, "PCL grestore");
    }

    return code;
}

void
pcl_init_gstate_stk(pcl_state_t * pcs)
{
    pcl_gstate_ids_t *pids = gs_alloc_struct(pcs->memory,
                                             pcl_gstate_ids_t,
                                             &st_gstate_ids_t,
                                             "PCL gsave");

    if (pids != 0) {            /* otherwise will crash soon enough */
        pids->prev = 0;
        pids->pccolor = 0;
        pids->pht = 0;
    }
    pcs->pids = pids;
}

void
pcl_free_gstate_stk(pcl_state_t * pcs)
{
    gs_free_object(pcs->memory, pcs->pids, "PCL grestore");
}
