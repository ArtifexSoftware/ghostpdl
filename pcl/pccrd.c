/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pccrd.c - PCL interface to the graphic library color rendering dictionary */
#include "string_.h"
#include "gx.h"
#include "gxdevcli.h"
#include "gsmatrix.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "gsparam.h"
#include "gsdevice.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gscie.h"
#include "gscrd.h"
#include "gscrdp.h"
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
    pcrd->is_dflt_illum = true;
    pcrd->pgscrd = 0;

    code = gs_cie_render1_build(&(pcrd->pgscrd), pmem, "pcl allocate CRD");
    if (code >= 0)
        *ppcrd = pcrd;
    else
        free_crd(pmem, pcrd, "pcl allocate CRD");
    return code;
}

/*
 * See if the default CRD is specified by the device.
 *
 * To simplify selection of more than one default CRD, this code allows more
 * than one CRD to be included in the parameters associated with a device,
 * and uses the device parameter "CRDName" to select the one that is to be
 * used as a default.
 *
 * Returns 
 */
  private bool
read_device_CRD(
    pcl_crd_t *     pcrd,
    pcl_state_t *   pcs
)
{
    gx_device *     pdev = pcl_get_target_device(pcs);
    gs_c_param_list list;
    gs_param_string dstring;
    char            nbuff[64];  /* ample size */
    int             code = 0;

    /*get the CRDName parameter from the device */
    gs_c_param_list_write(&list, pcs->memory);
    if (param_request((gs_param_list *)&list, "CRDName") < 0)
        return false;

    if ((code = gs_getdeviceparams(pdev, (gs_param_list *)&list)) >= 0) {
        gs_c_param_list_read(&list);
        if ( (code = param_read_string( (gs_param_list *)&list,
                                        "CRDName",
                                        &dstring
                                        )) == 0 ) {
            if (dstring.size > sizeof(nbuff) - 1)
                code = 1;
            else {
                strncpy(nbuff, (char *)dstring.data, dstring.size);
                nbuff[dstring.size] = '\0';
            }
        }
    }
    gs_c_param_list_release(&list);
    if (code != 0)
        return false;

    gs_c_param_list_write(&list, pcs->memory);
    if (param_request((gs_param_list *)&list, nbuff) < 0)
        return false;
    if ((code = gs_getdeviceparams(pdev, (gs_param_list *)&list)) >= 0) {
        gs_param_dict   dict;

        gs_c_param_list_read(&list);
        if ( (code = param_begin_read_dict( (gs_param_list *)&list,
                                            nbuff,
                                            &dict,
                                            false
                                            )) == 0 ) {
            code = param_get_cie_render1(pcrd->pgscrd, dict.list, pdev);
            param_end_read_dict((gs_param_list *)&list, nbuff, &dict);
            if (code > 0)
                code = 0;
        }
    }
    gs_c_param_list_release(&list);
    return (code == 0);
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
    pcl_state_t *   pcs
)
{
    pcl_crd_t *     pcrd = pcl_default_crd;
    int             code = 0;

    /* must not be a current CRD */
    if (pcrd != 0)
        return e_Range;

    /* allocate the CRD structure */
    if ((code = alloc_crd(&pcrd, pcs->memory)) < 0)
        return code;
    pcl_default_crd = pcrd;

    if (read_device_CRD(pcrd, pcs))
        return 0;
    else {
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
    }
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
        if ( (pcl_default_crd == 0)                       &&
             ((code = pcl_crd_build_default_crd(pcs)) < 0)  )
            return code;
        pcrd = pcl_default_crd;
        pcl_crd_init_from(*ppcrd, pcrd);
    }

    /* see if there is anything to do */
    if (pcs->pids->pcrd == pcrd)
        return 0;

    gs_cie_render_init(pcrd->pgscrd);
    gs_cie_render_sample(pcrd->pgscrd);

    if ((code = gs_setcolorrendering(pcs->pgs, pcrd->pgscrd)) >= 0)
        pcl_crd_copy_from(pcs->pids->pcrd, pcrd);
    return code;
}
