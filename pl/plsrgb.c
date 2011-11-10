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

/* uncomment the following definition to specify the device does the
   color conversion.  If the definition is commented out we set up an
   srgb color space and associated color rendering dictionary using
   the regular color conversion machinery in the graphics library
   pipeline.  The wtsimdi device is an example device that does color
   conversion as a "postprocess" and requires the definition.  If
   defined all additive colors are passed through as Device RGB but
   the device assumes the triples are in fact sRGB.  NB eventually
   this should be decided at run time.  */

#define DEVICE_DOES_COLOR_CONVERSION

bool
pl_device_does_color_conversion()
{
#ifdef DEVICE_DOES_COLOR_CONVERSION
    return true;
#endif
    return false;
}

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
inline static float
pl_decodeLMN(floatp val, const gs_cie_common *pcie)
{
    if ( val <= 0.03928 )
        return (float)(val / 12.92321);
    else
        return (float)pow((val + 0.055) / 1.055, (double)2.4);
}

static float
pl_DecodeLMN_0(floatp val, const gs_cie_common *pcie)
{
    return pl_decodeLMN(val, pcie);
}

static float
pl_DecodeLMN_1(floatp val, const gs_cie_common *pcie)
{
    return pl_decodeLMN(val, pcie);
}

static float
pl_DecodeLMN_2(floatp val, const gs_cie_common *pcie)
{
    return pl_decodeLMN(val, pcie);
}

static const gs_cie_common_proc3 pl_DecodeLMN = {
    { pl_DecodeLMN_0, pl_DecodeLMN_1, pl_DecodeLMN_2 }
};

/* LMN matrix for srgb. sRGB to XYZ (D65) matrix (ITU-R BT.709-2 Primaries) */
static const gs_matrix3 pl_MatrixLMN = {
    {0.412457f, 0.212673f, 0.019334f},
    {0.357576f, 0.715152f, 0.119192f},
    {0.180437f, 0.072175f, 0.950301f},
    false
};

/* LMN matrix for the crd, just the inverse of the color spaces LMN. */
static const gs_matrix3 pl_MatrixCRDLMN = {
    {3.240449f, -0.969265f, 0.055643f},
    {-1.537136f, 1.876011f, -0.204026f},
    {-0.498531f, 0.041556f, 1.057229f},
    false
};

/* D65 white point */
static const gs_vector3 pl_WhitePoint = {0.9505f, 1.0f, 1.0890f};
static const gs_vector3 pl_BlackPoint = {0.0f, 0.0f, 0.0f};

/* Bradford Cone Space - www.srgb.com */
static const gs_matrix3 pl_MatrixPQR = {
    {0.8951f, -0.7502f, 0.0389f},
    {0.2664f, 1.7135f, -0.0685f},
    {-0.1614f, 0.0367f, 1.0296f},
    false
};

static const gs_range3 pl_RangePQR = {
    {{-0.5, 2.0},
     {-0.5, 2.0},
     {-0.5, 2.0}}
};

/* tranform pqr */
static int
pl_TransformPQR_proc(int indx, floatp val, const gs_cie_wbsd *cs_wbsd,
                  gs_cie_render *pcrd, float *pnew_val)
{
    const float *pcrd_wht = (float *)&(cs_wbsd->wd.pqr);
    const float *pcs_wht = (float *)&(cs_wbsd->ws.pqr);
    *pnew_val = val * pcrd_wht[indx] / pcs_wht[indx];
    return 0;
}

static const gs_cie_transform_proc3 pl_TransformPQR = {
    pl_TransformPQR_proc,
    NULL,
    { NULL, 0 },
    NULL
};

/* ABC - inverse srgb gamma transform */
inline static float
pl_encodeABC(floatp in, const gs_cie_render * pcrd)
{
    if ( in <= 0.00304 )
        return (float)(in * 12.92321);
    return (float)(pow(in, (1.0 / 2.4)) * 1.055 - 0.055);
}

static float
pl_EncodeABC_0(floatp in, const gs_cie_render * pcrd)
{
    return pl_encodeABC(in, pcrd);
}

static float
pl_EncodeABC_1(floatp in, const gs_cie_render * pcrd)
{
    return pl_encodeABC(in, pcrd);
}

static float
pl_EncodeABC_2(floatp in, const gs_cie_render * pcrd)
{
    return pl_encodeABC(in, pcrd);
}

static const gs_cie_render_proc3 pl_EncodeABC_procs = {
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

static bool
pl_read_device_CRD(gs_cie_render *pcrd, gs_state *pgs)
{
    gx_device *     pdev = gs_currentdevice(pgs);
    gs_c_param_list list;
    gs_param_string dstring;
    char            nbuff[64];  /* ample size */
    int             code = 0;

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

static int
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
        dprintf("CRD initialized from device\n");
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

/* return SRGB color space to the client */
int
pl_cspace_init_SRGB(gs_color_space **ppcs, const gs_state *pgs)
{

    int code;
    /* make sure we have a crd set up */
#ifdef DEVICE_DOES_COLOR_CONVERSION
    *ppcs = gs_cspace_new_DeviceRGB(pgs->memory);
    return 0;
#endif

    code = pl_build_crd((gs_state *)pgs);
    if ( code < 0 )
        return code;

    code = gs_cspace_build_CIEABC(ppcs, NULL, gs_state_memory(pgs));
    if ( code < 0 )
        return code;
    *(gs_cie_DecodeLMN(*ppcs)) = pl_DecodeLMN;
    *(gs_cie_MatrixLMN(*ppcs)) = pl_MatrixLMN;
    (gs_cie_WhitePoint(*ppcs)) = pl_WhitePoint;
    (gs_cie_BlackPoint(*ppcs)) = pl_BlackPoint;
    return 0;
}

/* set the srgb color space */
static int
pl_setSRGB(gs_state *pgs)
{
    gs_color_space *pcs;
    int code;

    code = pl_cspace_init_SRGB(&pcs, pgs);
    if ( code < 0 )
        return code;
    code = gs_setcolorspace(pgs, pcs);
    rc_decrement(pcs, "ps_setSRGB");
    return code;
}

/* set an srgb color */
int
pl_setSRGBcolor(gs_state *pgs, float r, float g, float b)
{
    int code;
    gs_client_color color;

#ifdef DEVICE_DOES_COLOR_CONVERSION
    return gs_setrgbcolor(pgs, r, g, b);
#endif
    /* make sure we have a crd set up */
    code = pl_build_crd(pgs);
    if ( code < 0 )
        return code;

    code = pl_setSRGB(pgs);
    if ( code < 0 )
        return code;

    /* set the color */
    color.paint.values[0] = r;
    color.paint.values[1] = g;
    color.paint.values[2] = b;
    code = gs_setcolor(pgs, &color);
    return code;
}
