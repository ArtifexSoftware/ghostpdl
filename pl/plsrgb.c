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

/* plsrgb.c - setting of srgb device independent color space. */
#include "math_.h"
#include "string_.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gscspace.h"
#include "gsstate.h"
#include "gscie.h"
#include "gscolor2.h"
#include "gscrdp.h"
#include "gscrd.h"
#include "gsparam.h"
#include "gxstate.h"
#include "gzstate.h"
#include "plsrgb.h"

/* shared language (pcl and pclxl) for setting up sRGB to XYZ and an
   associated default CRD to be used.  The code will request a crd
   from the driver which will override the default crd.  We use the
   postscript definitions (converted to C) from www.srgb.com, these
   definitions are repeated here

    [ /CIEBasedABC <<
    % sRGB gamma transform 
            /DecodeLMN [
                    {dup 0.03928 le {12.92321 div}{0.055 add 1.055 div 2.4 exp}ifelse}
                    bind dup dup ]
    
    % sRGB to XYZ (D65) matrix (ITU-R BT.709-2 Primaries)
            /MatrixLMN [
                    0.412457 0.212673 0.019334
                    0.357576 0.715152 0.119192
                    0.180437 0.072175 0.950301 ]
            /WhitePoint [ 0.9505 1 1.0890 ] % D65
            /BlackPoint[0 0 0]
    >> ] setcolorspace 
    
    <<
    % sRGB output CRD, D65 white point
    /ColorRenderingType 1
    /RangePQR [ -0.5 2 -0.5 2 -0.5 2 ] 
    
    % Bradford Cone Space
    /MatrixPQR [0.8951 -0.7502 0.0389
            0.2664 1.7135 -0.0685
            -0.1614 0.0367 1.0296] 
    
    % VonKries-like transform in Bradford Cone Space
    /TransformPQR
            [{exch pop exch 3 get mul exch pop exch 3 get div} bind
            {exch pop exch 4 get mul exch pop exch 4 get div} bind
            {exch pop exch 5 get mul exch pop exch 5 get div} bind] 
    
    /MatrixLMN [3.240449 -0.969265 0.055643
            -1.537136 1.876011 -0.204026
            -0.498531 0.041556 1.057229] 
    
    % Inverse sRGB gamma transform
            /EncodeABC [{dup 0.00304 le {12.92321 mul}
            {1 2.4 div exp 1.055 mul 0.055 sub}ifelse}
            bind dup dup] 
    
    /WhitePoint[0.9505 1 1.0890] % D65
    /BlackPoint [0 0 0]
    >> setcolorrendering
*/


/* CIEBasedABC definitions */
/* Decode LMN procedures for srgb color spaces or sRGB gamma transform. */
inline private float
pl_decodeLMN(floatp val, const gs_cie_common *pcie)
{
    if ( val <= 0.03928 )
	return (float)(val / 12.92321);
    else
	return (float)pow((val + 0.055) / 1.055, (double)2.4);
}


private float
pl_DecodeLMN_0(floatp val, const gs_cie_common *pcie)
{
    return pl_decodeLMN(val, pcie);
}

private float
pl_DecodeLMN_1(floatp val, const gs_cie_common *pcie)
{
    return pl_decodeLMN(val, pcie);
}

private float
pl_DecodeLMN_2(floatp val, const gs_cie_common *pcie)
{
    return pl_decodeLMN(val, pcie);
}

private const gs_cie_common_proc3 pl_DecodeLMN = {
    { pl_DecodeLMN_0, pl_DecodeLMN_1, pl_DecodeLMN_2 }
};

/* LMN matrix for srgb. sRGB to XYZ (D65) matrix (ITU-R BT.709-2 Primaries) */
private const gs_matrix3 pl_MatrixLMN = {
    {0.412457, 0.212673, 0.019334},
    {0.357576, 0.715152, 0.119192},
    {0.180437, 0.072175, 0.950301},
    false
};

/* LMN matrix for the crd, just the inverse of the color spaces LMN. */
private const gs_matrix3 pl_MatrixCRDLMN = {
    {3.240449, -0.969265, 0.055643},
    {-1.537136, 1.876011, -0.204026},
    {-0.498531, 0.041556, 1.057229},
    false
};

/* D65 white point */
private const gs_vector3 pl_WhitePoint = {0.9505, 1.0, 1.0890};
private const gs_vector3 pl_BlackPoint = {0.0, 0.0, 0.0};

/* Bradford Cone Space - www.srgb.com */
private const gs_matrix3 pl_MatrixPQR = {
    {0.8951, -0.7502, 0.0389},
    {0.2664, 1.7135, -0.0685},
    {-0.1614, 0.0367, 1.0296},
    false
};

private const gs_range3 pl_RangePQR = {
    {{-0.5, 2.0},
     {-0.5, 2.0},
     {-0.5, 2.0}}
};


/* tranform pqr */
private int
pl_TransformPQR_proc(const gs_memory_t *mem, int indx, floatp val, const gs_cie_wbsd *cs_wbsd,
                  gs_cie_render *pcrd, float *pnew_val)
{
    const float *pcrd_wht = (float *)&(cs_wbsd->wd.pqr);
    const float *pcs_wht = (float *)&(cs_wbsd->ws.pqr);
    *pnew_val = val * pcrd_wht[indx] / pcs_wht[indx];
    return 0;
}

private const gs_cie_transform_proc3 pl_TransformPQR = {
    pl_TransformPQR_proc,
    NULL,
    { NULL, 0 },
    NULL
};


/* ABC - inverse srgb gamma transform */
inline private float
pl_encodeABC(floatp in, const gs_cie_render * pcrd)
{
    if ( in <= 0.00304 )
        return (float)(in * 12.92321);
    return (float)(pow(in, (1.0 / 2.4)) * 1.055 - 0.055);
}

private float
pl_EncodeABC_0(floatp in, const gs_cie_render * pcrd)
{
    return pl_encodeABC(in, pcrd);
}

private float
pl_EncodeABC_1(floatp in, const gs_cie_render * pcrd)
{
    return pl_encodeABC(in, pcrd);
}

private float
pl_EncodeABC_2(floatp in, const gs_cie_render * pcrd)
{
    return pl_encodeABC(in, pcrd);
}

private const gs_cie_render_proc3 pl_EncodeABC_procs = {
    {pl_EncodeABC_0, pl_EncodeABC_1, pl_EncodeABC_2}
};

/*
 * See if the default CRD is specified by the device.
 *
 * To simplify selection of more than one default CRD, this code allows more
 * than one CRD to be included in the parameters associated with a device,
 * and uses the device parameter "CRDName" to select the one that is to be
 * used as a default.
 *
 */

/* comment this define out to enable reading crd's from devices - the current implementation suffers from memory leaks */

#define DISABLE_DEVICE_CRD

private bool
pl_read_device_CRD(gs_cie_render *pcrd, gs_state *pgs)
{
    gx_device *     pdev = gs_currentdevice(pgs);
    gs_c_param_list list;
    gs_param_string dstring;
    char            nbuff[64];  /* ample size */
    int             code = 0;

#ifdef DISABLE_DEVICE_CRD
    return false;
#endif
    /*get the CRDName parameter from the device */
    gs_c_param_list_write(&list, gs_state_memory(pgs));
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

    gs_c_param_list_write(&list, gs_state_memory(pgs));
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
            code = param_get_cie_render1(pcrd, dict.list, pdev);
            param_end_read_dict((gs_param_list *)&list, nbuff, &dict);
            if (code > 0)
                code = 0;
        }
    }
    gs_c_param_list_release(&list);
    return (code == 0);
}


/* statics to see if the crd has been built, in practice the crd is a
   singleton. */
gs_cie_render *pl_pcrd;
bool pl_pcrd_built = false; /* the crd has been built */


int
pl_build_crd(gs_state *pgs)
{
    int code;
    /* nothing to do */
    if ( pl_pcrd_built == true )
        return gs_setcolorrendering(pgs, pl_pcrd);

    code = gs_cie_render1_build(&pl_pcrd, gs_state_memory(pgs), "build_crd");
    if ( code < 0 )
	return code;
    pl_pcrd_built = true;

    if ( pl_read_device_CRD(pl_pcrd, pgs) ) {
        dprintf(pgs->memory, "CRD initialized from device\n");
        return 0;
    }

    code = gs_cie_render1_initialize(pgs->memory, 
				     pl_pcrd,
                                     NULL,
                                     &pl_WhitePoint,
                                     &pl_BlackPoint,
                                     &pl_MatrixPQR,
                                     &pl_RangePQR,
                                     &pl_TransformPQR,
                                     &pl_MatrixCRDLMN,
                                     NULL, /* EncodeLMN */
                                     NULL, /* RangeLMN */
                                     NULL, /* MatrixABC */
                                     &pl_EncodeABC_procs,
                                     NULL,
                                     NULL);
    if ( code < 0 )
	return code; /* should not fail */
    code = gs_setcolorrendering(pgs, pl_pcrd);
    return code;
}

/* statics for singleton CIEABC color space and a procedure to set the
   sRGB via CIEABC. */
gs_color_space *pl_pcs;
bool pl_pcs_built = false;
extern const gs_color_space_type gs_color_space_type_CIEABC;

int
pl_build_and_set_sRGB_space(gs_state *pgs)
{
    if ( pl_pcs_built == false ) {
        int code = gs_cspace_build_CIEABC(&pl_pcs, NULL, gs_state_memory(pgs));
        if ( code < 0 )
            return code;
        *(gs_cie_DecodeLMN(pl_pcs)) = pl_DecodeLMN;
        *(gs_cie_MatrixLMN(pl_pcs)) = pl_MatrixLMN;
        (gs_cie_WhitePoint(pl_pcs)) = pl_WhitePoint;
        (gs_cie_BlackPoint(pl_pcs)) = pl_BlackPoint;
        pl_pcs_built = true;
    }
    pl_pcs->type = &gs_color_space_type_CIEABC;
    return gs_setcolorspace(pgs, pl_pcs);
}

/* more statics for a singleton CIEABC color space.  This duplicates code but
   this color space is used for images.  Note we just return the color
   space to the client and don't set the color space in the gstate. */
gs_color_space *pl_pcs2;
bool pl_pcs2_built = false;
int
pl_cspace_init_SRGB(gs_color_space **ppcs, const gs_state *pgs)
{

    /* make sure we have a crd set up */
    int code = pl_build_crd(pgs);
    if ( code < 0 )
        return code;

    if ( pl_pcs2_built == false ) {
        int code = gs_cspace_build_CIEABC(&pl_pcs2, NULL, gs_state_memory(pgs));
        if ( code < 0 )
            return code;
        *(gs_cie_DecodeLMN(pl_pcs2)) = pl_DecodeLMN;
        *(gs_cie_MatrixLMN(pl_pcs2)) = pl_MatrixLMN;
        (gs_cie_WhitePoint(pl_pcs2)) = pl_WhitePoint;
        (gs_cie_BlackPoint(pl_pcs2)) = pl_BlackPoint;
        pl_pcs2_built = true;
    }
    pl_pcs2->type = &gs_color_space_type_CIEABC;
    *ppcs = pl_pcs2;
    return 0;
}

/* set an srgb color and check crd and color space prerequisites are ok */
int
pl_setSRGB(gs_state *pgs, float r, float g, float b)
{
    int code;
    /* make sure we have a crd set up */
    code = pl_build_crd(pgs);
    if ( code < 0 )
        return code;
    /* build (if necessary) and set the color space */
    code = pl_build_and_set_sRGB_space(pgs);
    if ( code < 0 )
        return code;
    /* set the color */
    {
        gs_client_color color;
        color.paint.values[0] = r;
        color.paint.values[1] = g;
        color.paint.values[2] = b;
        code = gs_setcolor(pgs, &color);
    }
    return code;
}
    
        
void
pl_free_srgb(gs_state *pgs)
{
    if (pl_pcrd_built) {
        gs_free_object(gs_state_memory(pgs), pl_pcrd, "pl_free_srgb");
        pl_pcrd_built = false;
    }
    if (pl_pcs_built) {
        gs_free_object(gs_state_memory(pgs), pl_pcs->params.abc, "pl_free_srgb");
        gs_free_object(gs_state_memory(pgs), pl_pcs, "pl_free_srgb");
        pl_pcs_built = false;
    }
    if (pl_pcs2_built) {
        gs_free_object(gs_state_memory(pgs), pl_pcs2->params.abc, "pl_free_srgb");
        gs_free_object(gs_state_memory(pgs), pl_pcs2, "pl_free_srgb");
        pl_pcs2_built = false;
    }
    return;
}
