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
 *
 * Note that memory handling for render tables remains the responsibility of
 * the client.
 */

/* pccrd.c - PCL interface to the graphic library color rendering dictionary */
#include "gx.h"
#include "gsmatrix.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gscie.h"
#include "gscrd.h"
#include "pcommand.h"
#include "pccrd.h"

/* GC routines */
private_st_crd_t();

/*
 * Unlike other elements of the PCL "palette", color rendering dictionaries
 * are for the most part not a feature that can be controlled from the language.
 * Except for the white point, the parameters of a color rendering dictionary
 * are determined by the output device rather than the language.
 *
 * To accommodate this situation, the device-specific or default color
 * rendering dictionary is maintained as a global. When used as part of a
 * palette, this global should be "copied" using the copy_from macro provided
 * below.
 */
pcl_crd_t * pcl_default_crd;

/*
 * The default color rendering information. This works for a monitor that
 * uses linear SMPTE-C phosphors. This is almost certainly not the case for
 * any actual printing device, but the approximation will do for now.
 *
 * The following parameters are left at their graphic-library default values:
 *
 *    BlackPoint
 *    MatrixPQR
 *    RangePQR
 *    EncodeLMN
 *    RangeLMN
 *    MatrixABC
 *    EncodeABC
 *    RangeABC
 *    RenderTable
 *
 */
private const gs_vector3    dflt_WhitePoint = { 0.95, 1.0, 1.09 };
private const gs_matrix3    dflt_MatrixLMN = { {  3.51, -1.07,  0.06 },
                                               { -1.74,  1.98, -0.20 },
                                               { -0.54,  0.04,  1.05 },
                                               false };

/*
 * The TransformPQR structure.
 *
 * Unlike the earlier impelementations, there is now just a single procedure,
 * which accepts the the component index as an operand. Rather than returning
 * the mapped value, the location pointed to by pnew_val is used to hold the
 * result and a success (0) or error (< 0) indication is returned.
 *
 * The final field of the TransformPQR data structure must be writable, so
 * that the driver can insert its own name in this location. To support
 * environements in which all initialized globals are in ROM, two copies
 * of this structure are maintained: a prototype (qualified as const) and
 * the actual value. The former is copied into the latter each time a CRD
 * using this structure is created.
 */
  private int
dflt_TransformPQR_proc(
    int                 cmp_indx,
    floatp              val,
    const gs_cie_wbsd * cs_wbsd,
    gs_cie_render *     pcrd,
    float *             pnew_val
)
{
    const float *       pcrd_wht = (float *)&(cs_wbsd->wd.pqr);
    const float *       pcs_wht = (float *)&(cs_wbsd->ws.pqr);

    *pnew_val = val * pcrd_wht[cmp_indx] / pcs_wht[cmp_indx];
    return 0;
}

private const gs_cie_transform_proc3    dflt_TransformPQR_proto = {
    dflt_TransformPQR_proc,
    NULL,
    { NULL, 0 },
    NULL
};

private gs_cie_transform_proc3  dflt_TransformPQR;


/*
 * Free a PCL color rendering dictionary structure.
 */
  private void
free_crd(
    gs_memory_t *   pmem,
    void *          pvcrd,
    client_name_t   cname
)
{
    pcl_crd_t *     pcrd = (pcl_crd_t *)pvcrd;

    if (pcrd->pgscrd != 0)
        rc_decrement(pcrd->pgscrd, cname);
    gs_free_object(pmem, pvcrd, cname);
}

/*
 * Allocate a PCL color rendering dictionary structure.
 */
  private int
alloc_crd(
    pcl_crd_t **    ppcrd,
    gs_memory_t *   pmem
)
{
    pcl_crd_t *     pcrd = 0;
    int             code = 0;

    rc_alloc_struct_1( pcrd,
                       pcl_crd_t,
                       &st_crd_t,
                       pmem,
                       return e_Memory,
                       "pcl allocate CRD"
                       );
    pcrd->rc.free = free_crd;
    pcrd->id = pcl_next_id();
    pcrd->is_dflt_illum = true;
    pcrd->pgscrd = 0;

    code == gs_cie_render1_build(&(pcrd->pgscrd), pmem, "pcl allocate CRD");
    if (code >= 0)
        *ppcrd = pcrd;
    else
        free_crd(pmem, pcrd, "pcl allocate CRD");
    return code;
}

/*
 * Build the default color rendering dictionary.
 *
 * This routine should be called only once, and then only when there is no
 * current CRD.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_crd_build_default_crd(
    gs_memory_t *   pmem
)
{
    pcl_crd_t *     pcrd = pcl_default_crd;
    int             code = 0;

    /* must not be a current CRD */
    if (pcrd != 0)
        return e_Range;

    /* allocate the CRD structure */
    if ((code = alloc_crd(&pcrd, pmem)) < 0)
        return code;

    pcl_default_crd = pcrd;
    dflt_TransformPQR = dflt_TransformPQR_proto;
    return gs_cie_render1_initialize( pcrd->pgscrd,
                                      NULL,
                                      &dflt_WhitePoint,
                                      NULL,
                                      NULL,
                                      NULL,
                                      &dflt_TransformPQR,
                                      &dflt_MatrixLMN,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL
                                      );
}

/*
 * Build a CRD with device-provided parameters, but with the default PCL
 * white point.
 *
 * Note that this is one of the few routines which accepts a memory pointer
 * operand but does not have it as the last operand.
 *
 * Any operands set to NULL will be given their graphic library defaults
 * (which may not be the same as their values in the default PCL CRD).
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_crd_build_dev_crd(
    gs_memory_t *                   pmem,
    const gs_vector3 *              BlackPoint,
    const gs_matrix3 *              MatrixPQR,
    const gs_range3 *               RangePQR,
    const gs_cie_transform_proc3 *  TransformPQR,
    const gs_matrix3 *              MatrixLMN,
    const gs_cie_render_proc3 *     EncodeLMN,
    const gs_range3 *               RangeLMN,
    const gs_matrix3 *              MatrixABC,
    const gs_cie_render_proc3 *     EncodeABC,
    const gs_range3 *               RangeABC,
    const gs_cie_render_table_t *   RenderTable
)
{
    pcl_crd_t *                     pcrd = pcl_default_crd;
    int                             code = 0;

    /* release any existing CRD */
    if (pcrd != 0)
        rc_decrement(pcrd, "pcl build device specific CRD");

    /* allocate the CRD structure */
    if ((code = alloc_crd(&pcrd, pmem)) < 0)
        return code;

    pcl_default_crd = pcrd;
    return gs_cie_render1_initialize( pcrd->pgscrd,
                                      NULL,             /* for now */
                                      &dflt_WhitePoint,
                                      BlackPoint,
                                      MatrixPQR,
                                      RangePQR,
                                      TransformPQR,
                                      MatrixLMN,
                                      EncodeLMN,
                                      RangeLMN,
                                      MatrixABC,
                                      EncodeABC,
                                      RangeABC,
                                      RenderTable
                                      );
}

/*
 * Set the viewing illuminant.
 *
 * Though this code goes through the motions of an "unshare" operation, it
 * will almost always allocate a new structure, as the CRD will be referred
 * to both by the palette and the graphic state.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_crd_set_view_illuminant(
    pcl_crd_t **        ppcrd,
    const gs_vector3 *  pwht_pt
)
{
    pcl_crd_t *         pcrd = *ppcrd;
    pcl_crd_t *         pold = pcrd;
    int                 code = 0;

    if (pcrd->rc.ref_count > 1) {
        if ((code = alloc_crd(ppcrd, pcrd->rc.memory)) < 0)
            return code;
        pcrd = *ppcrd;
    } else
        pcrd->id = pcl_next_id();
    pcrd->is_dflt_illum = false;

    /* if no previous CRD, use the default settings */
    if (pold == 0) {
        dflt_TransformPQR = dflt_TransformPQR_proto;
        return  gs_cie_render1_initialize( pcrd->pgscrd,
                                           NULL,
                                           pwht_pt,
                                           NULL,
                                           NULL,
                                           NULL,
                                           &dflt_TransformPQR,
                                           &dflt_MatrixLMN,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL
                                           );
    }
    code = gs_cie_render1_initialize( pcrd->pgscrd,
                                      NULL,     /* for now */
                                      pwht_pt,
                                      &(pold->pgscrd->points.BlackPoint),
                                      &(pold->pgscrd->MatrixPQR),
                                      &(pold->pgscrd->RangePQR),
                                      &(pold->pgscrd->TransformPQR),
                                      &(pold->pgscrd->MatrixLMN),
                                      &(pold->pgscrd->EncodeLMN),
                                      &(pold->pgscrd->RangeLMN),
                                      &(pold->pgscrd->MatrixABC),
                                      &(pold->pgscrd->EncodeABC),
                                      &(pold->pgscrd->RangeABC),
                                      &(pold->pgscrd->RenderTable)
                                      );

    if (pcrd != pold)
        rc_decrement(pold, "pcl set viewing illuminant");
    return code;
}

/*
 * Set a color rendering dictionary into the graphic state. If the rendering
 * dictionary does not yet exist, create a default color rendering dictionary.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_crd_set_crd(
    pcl_crd_t **    ppcrd,
    pcl_state_t *   pcs
)
{
    pcl_crd_t *     pcrd = *ppcrd;
    int             code = 0;

    if (pcrd == 0) {
        if ( (pcl_default_crd == 0)                               &&
             ((code = pcl_crd_build_default_crd(pcs->memory)) < 0)  )
            return code;
        pcrd = pcl_default_crd;
        pcl_crd_init_from(*ppcrd, pcrd);
    }

    /* see if there is anything to do */
    if (pcs->ids.crd_id == pcrd->id)
        return 0;

    gs_cie_render_init(pcrd->pgscrd);
    gs_cie_render_sample(pcrd->pgscrd);

    if ((code = gs_setcolorrendering(pcs->pgs, pcrd->pgscrd)) >= 0)
        pcs->ids.crd_id = pcrd->id;
    return code;
}
