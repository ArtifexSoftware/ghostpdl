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


/* pccsbase.h - base color space code for PCL 5c */

#include "gx.h"
#include "math_.h"
#include "gstypes.h"
#include "gsmatrix.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gscie.h"
#include "pcmtx3.h"
#include "pccsbase.h"
#include "pcstate.h"

/* for RC structures */
gs_private_st_simple(st_cs_base_t, pcl_cs_base_t, "pcl base color space");


/*
 * Handle min/max values for device-independent color spaces.
 *
 * Examples in HP's "PCL 5 Color Technical Reference Manual"  are unclear
 * about the interpretation of minimum/maximum value for components for the
 * device independent color spaces. It is clear that the "raw" input range
 * for these parameters is always [ 0, 255 ], but how this range is mapped
 * is not fully obvious.
 *
 * Empirical observations with both the CIE L*a*b* and the luminance-
 * chrominance color space do little to clear up this confusion. In the range
 * [ 0, 255 ] (as suggested in the "PCL 5 Color Technical Reference Manual"),
 * integer arithmetic overflow seems to occur at some points, leading to
 * rather curious color progressions (a moderate brown changes abruptly to
 * a dark green at the half-intensity point of a "gray" scale).
 *
 * Side Note:
 *      Device dependent color spaces in PCL 5c are not provided with
 *      ranges, but are assigned white and black points. The interpretation of
 *      these points is clear: specify the white point to get the maximum
 *      intensity value for all components on the device, and the black point
 *      to achieve the minimum value (for printers these are reasonably white
 *      and black; most screens are adjusted to achieve the same result,
 *      though there is no strict requirement for this to be the case).
 *      Values within this range [black_point, white_point] are mapped
 *      by the obvious linear transformation; values outside of the range
 *      are clamped to the nearest boundary point.
 *
 *      Two items of note for device dependent color spaces:
 *
 *          a CMY color space is just an RGB color space with the white and
 *          black points inverted.
 *
 *          For a given value of bits-per-primary, it is quite possible to
 *          set the white and black points so that one or both may not be
 *          achievable
 *
 *      In this implementation, the white and black points of the device-
 *      specific color spaces are handled in the initial "normalization"
 *      step, before colors are entered into the palette.
 *
 * To do something sensible for device independent color space ranges, it is
 * ncessary to ignore HP's implementation and ask what applications might
 * reasonably want to do with the range parameters. The answer depends on the
 * particular color space:
 *
 * a.  For colorimetric RGB spaces, the reasonable assumption is that the
 *     parameters correspond to the range of the input primaries, from
 *     minimal 0 to full intensity. Furthermore, the white point corresponds
 *     to the full intensity for each primary, the black point to minimum
 *     intensity.
 *
 *     The difficulty with this interpretation is that it renders the range
 *     parameters meaningless. PCL requires input data for device-independent
 *     color spaces to be in the range [0, 255], with 0 ==> minimum value and
 *     255 ==> maximum value. Combined with the assumption above, this will
 *     always map the "raw" input values {0, 0, 0} and {255, 255, 255} to
 *     the black and white points, respectively.
 *
 *     To avoid making the range parameters completely meaningless in this
 *     case, we will actually use a different interpretation. The modification
 *     continues to map the raw input values such that 0 ==> minimum value
 *     and 255 ==> maximum value, but the black and white points continue
 *     to be {0, 0, 0} and {1, 1, 1}. Values outside of this range are
 *     clipped.
 *
 *     To the extent that we can determine, this interpretation bears some
 *     relationship to that used by HP.
 *
 * b.  For the CIE L*a*b* space, the interpretation of the value of each
 *     component is fixed by standards, so the ranges specified in the
 *     Configure Image Data command can only be interpreted as indicating
 *     which set of values the "raw" input range [0, 255] should be mapped
 *     to (0 ==> min_value, 255 ==> max value)
 *
 * c.  For consistency it is necessary to handle the range parameters for
 *     luminance-chrominance in the same manner as they are handled for the
 *     colorimetric RGB case. This approach makes even less sense in this
 *     case than for the colorimetric RGB case, as the region of input space
 *     that corresponds to real colors for luminance-chrominance spaces is
 *     not a cube (i.e.: it is possible for each of the components to be
 *     in a reasonable range, but for the combination to yield primary
 *     component values < 0 or > 1). There is not much choice about this
 *     arrangement, however, because a luminance-chrominance space can be
 *     set up to directly mimic a colorimetric RGB space by setting the
 *     transformation between the two to the identity transformation.
 *
 * For all of these range mappings, the initial step is to map the "raw" input
 * range to [min_val, max_val], and incorporate the effect of the lookup
 * table for the particular color space (if any). This is accomplished by the
 * following macro. Note that the initial normalization step has already
 * converted the "raw" input range to [0, 1] (the same range as is used by
 * device-dependent color spaces).
 */

#define convert_val(val, min_val, range, plktbl)            \
    BEGIN                                                   \
    if ((plktbl) != 0)                                      \
        val = (double)(plktbl)[(int)(255 * val)] / 255.0;   \
    val = min_val + val * range;                            \
    END

/*
 * Default Configure Image Data information for the various color spaces.
 *
 * The black and white points for device dependent color spaces are not included
 * here as those are handled via palette value normalization, not via the color
 * space. Since the black and white points are the only parameters for the
 * device-specific color spaces, there is no default information here for them
 * at all.
 *
 * Other color spaces have up to three sets of default information:
 *
 *     default parameter ranges (all device-independent color spaces)
 *
 *     the default chromaticity of the primaries (colorimetric RGB and
 *         luminance-chrominance spaces)
 *
 *     the default conversion between the color components and the primaries
 *         for which chromaticities have been provided (only luminance-
 *         chrominance space)
 */
static const pcl_cid_minmax_t cielab_range_default = {
    {{0.0f, 100.0f}, {-100.0f, 100.0f}, {-100.0f, 100.0f}}
};

static const pcl_cid_minmax_t colmet_range_default = {
    {{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}}
};

static const pcl_cid_minmax_t lumchrom_range_default = {
    {{0.0f, 1.0f}, {-0.89f, 0.89f}, {-0.70f, 0.70f}}
};

static const pcl_cid_col_common_t chroma_default = {
    {
     {0.640f, 0.340f},          /* "red" primary chromaticity */
     {0.310f, 0.595f},          /* "green" primary chromaticity */
     {0.155f, 0.070f},          /* "blue" chromaticity */
     {0.313f, 0.329f}           /* white chromaticity */
     },
    {{1, 1.0f}, {1, 1.0f}, {1, 1.0f}}
};

static const float lumchrom_xform_default[9] = {
    0.30f, 0.59f, 0.11f, -0.30f, -0.59f, 0.89f, 0.70f, -0.59f, -0.11f
};

/* structure of default values for all color spaces */
static const struct
{
    const pcl_cid_minmax_t *pminmax;
    const pcl_cid_col_common_t *pchroma;
    const float *pxform;
} cid_data_default[(int)pcl_cspace_num] = {
    {0, 0, 0},                   /* pcl_cspace_RGB */
    {0, 0, 0},                   /* pcl_cspace_CMY */
    {&colmet_range_default, &chroma_default, 0}, /* pcl_cspace_Colorimetric */
    {&cielab_range_default, 0, 0},       /* pcl_cspace_CIELab */
    {&lumchrom_range_default, &chroma_default, lumchrom_xform_default} /* pcl_cspace_LumChrom */
};

/*
 * Code for constructing/modifying the client data structure of PCL base
 * color spaces.
 */

/*
 * Set the range parameters for a color space.
 */
static void
set_client_info_range(pcl_cs_client_data_t * pdata,
                      const pcl_cid_minmax_t * pminmax)
{
    int i;

    for (i = 0; i < 3; i++) {
        pdata->min_val[i] = pminmax->val_range[i].min_val;
        pdata->range[i] = pminmax->val_range[i].max_val
            - pminmax->val_range[i].min_val;
    }
}

/*
 * Set the gamma/gain information for a color space.
 */
static void
set_client_info_chroma(pcl_cs_client_data_t * pdata,
                       const pcl_cid_col_common_t * pchroma)
{
    int i;

    for (i = 0; i < 3; i++) {
        double gamma = pchroma->nonlin[i].gamma;

        double gain = pchroma->nonlin[i].gain;

        pdata->inv_gamma[i] = (gamma == 0.0 ? 1.0 : 1.0 / gamma);
        pdata->inv_gain[i] = (gain == 0.0 ? 1.0 : 1.0 / gain);
    }
}

/*
 * Build the client information structure for a color space.
 */
static void
build_client_data(pcl_cs_client_data_t * pdata,
                  const pcl_cid_data_t * pcid, gs_memory_t * pmem)
{
    pcl_cspace_type_t type = pcl_cid_get_cspace(pcid);
    const pcl_cid_minmax_t *pminmax = cid_data_default[type].pminmax;
    const pcl_cid_col_common_t *pchroma = cid_data_default[type].pchroma;

    /* see if we have long-form information for device-independent spaces */
    if (pcid->len > 6) {
        if (type == pcl_cspace_Colorimetric) {
            pminmax = &(pcid->u.col.minmax);
            pchroma = &(pcid->u.col.colmet);
        } else if (type == pcl_cspace_CIELab)
            pminmax = &(pcid->u.lab.minmax);
        else if (type == pcl_cspace_LumChrom) {
            pminmax = &(pcid->u.lum.minmax);
            pchroma = &(pcid->u.col.colmet);
        }
    }

    /* set the range and gamma/gain parameters, as required */
    if (pminmax != 0)
        set_client_info_range(pdata, pminmax);
    if (pchroma != 0)
        set_client_info_chroma(pdata, pchroma);
}

/*
 * Init a client data structure from an existing client data structure.
 */
static void
init_client_data_from(pcl_cs_client_data_t * pnew,
                      const pcl_cs_client_data_t * pfrom)
{
    *pnew = *pfrom;
    pcl_lookup_tbl_init_from(pnew->plktbl1, pfrom->plktbl1);
    pcl_lookup_tbl_init_from(pnew->plktbl2, pfrom->plktbl2);
}

/*
 * Update the lookup table information in a PCL base color space.
 */
static void
update_lookup_tbls(pcl_cs_client_data_t * pdata,
                   pcl_lookup_tbl_t * plktbl1, pcl_lookup_tbl_t * plktbl2)
{
    pcl_lookup_tbl_copy_from(pdata->plktbl1, plktbl1);
    pcl_lookup_tbl_copy_from(pdata->plktbl2, plktbl2);
}

/*
 * Free a client data structure. This releases the lookup tables, if they
 * are present.
 */
#define free_lookup_tbls(pdata)               \
    update_lookup_tbls((pdata), NULL, NULL)

/*
 * The colorimetric case.
 *
 * The information provided with this color space consists of the CIE (x, y)
 * chromaticities of the three primaries, and the white point. In order to
 * derive a color space from this information, three additional assumptions
 * are required:
 *
 *     the intensity (Y value) or the white point is 1.0 (identical to the
 *         convention used by PostScript)
 *
 *     the white point is achieved by setting each of the primaries to its
 *         maximum value (1.0)
 *
 *     the black point (in this case, { 0, 0, 0 }, since it is not specified)
 *         is achieved by setting each of the primaries to its minimum value
 *         (0.0)
 *
 * Relaxing the former assumption would only modify the mapping provided by the
 * color space by a multiplicative constant. The assumption is also reasonable
 * for a printing device: even under the strongest reasonable assumptions, the
 * actual intensity achieved by printed output is determined by the intensity
 * of the illuminant and the reflectivity of the paper, neither one of which is
 * known to the color space. Hence, the value of Y selected is arbitrary (so
 * long as it is > 0), and using 1.0 simplifies the calculations a bit.
 *
 * The second and third assumptions are standard and, in fact, define the
 * concept of "white point" and "black point" for the purposes of this color
 * space. These assumptions are, however, often either poorly documented or
 * not documented at all. At least the former is also not particularly intuitive:
 * in an additive color arrangement (such as a display), the color achieved by
 * full intensity on each of the primaries may be colored, and its color need
 * not correspond to any of the standard "color temperatures" usually used
 * as white points.
 *
 * The assumption is, unfortunately, critical to allow derivation of the
 * transformation from the primaries provided to the XYZ color space. If we
 * let {Xr, Yr, Zr}, {Xg, Yg, Z,}, and {Xb, Yb, Zb} denote the XYZ coordinates
 * of the red, green, and blue primaries, respectively, then the desired
 * conversion is:
 *
 *                              -          -
 *     {X, Y, Z} = {R, G, B} * | Xr  Yr  Zr |
 *                             | Xg  Yg  Zg |
 *                             | Xb  Yb  Zb |
 *                              -          -
 *
 * The chromaticities of the primaries and the white point are derived by
 * adjusting the X, Y, and Z coordinates such that x + y + z = 1. Hence:
 *
 *     x = X / (X + Y + Z)
 *
 *     y = Y / (X + Y + Z)
 *
 *     z = Z / (X + Y + Z)
 *
 * Note that these relationships preserve the ratios between components:
 *
 *     x / y = X / Y
 *
 * Hence:
 *
 *    X = (x / y) * Y
 *
 *    Z = ((1 - (x + y)) / y) * Y
 *
 * Using this relationship, the conversion equation above can be restated as:
 *
 *                            -                                -
 *   {X, Y, Z} = {R, G, B} * | Yr * xr / yr   Yr   Yr * zr / yr |
 *                           | Yg * xg / yg   Yg   Yg * zg / yg |
 *                           | Yb * xb / yb   Yb   Yb * zb / yb |
 *                            -                                -
 *
 * Where zr = 1.0 - (xr + yr), zg = 1.0 - (xg + yg), and zb = 1.0 - (xb + yb).
 *
 * As discussed above, in order to make the range parameters of the long form
 * Configure Image Data command meaningful, we must use the convention that
 * full intensity for all components is {1, 1, 1}, and no intensity is
 * {0, 0, 0}. Because the transformation is linear, the latter point provides
 * no information, but the former establishes the following relationship.
 *
 *                               -                                -
 *   {Xw, Yw, Zw} = {1, 1, 1} * | Yr * xr / yr   Yr   Yr * zr / yr |
 *                              | Yg * xg / yg   Yg   Yg * zg / yg |
 *                              | Yb * xb / yb   Yb   Yb * zb / yb |
 *                               -                                -
 *
 * This is equivalent to:
 *
 *                                  -                     -
 *   {Xw, Yw, Zw} = {Yr, Yg, Yb} * | xr / yr  1.0  zr / yr |
 *                                 | xg / yg  1.0  zg / yg |
 *                                 | xb / yb  1.0  zb / yb |
 *                                  -                     -
 *
 *                = {Yr, Yg, Yb} * mtx
 *
 * Using the assumption that Yw = 1.0, we have Xw = xw / yw and Zw = zw / yw
 * (zw = 1 - (xw + yw)), so:
 *
 *    {Yr, Yg, Yb} = {xw / yw, 1.0, zw / yw} * mtx^-1
 *
 * Since Yr, Yg, and Yb are now known, it is possible to generate the
 * RGB ==> XYZ transformation.
 *
 * HP also provides for a gamma and gain parameter to be applied to each
 * primary, though it does not specify exactly what these mean. The
 * interpretation provided below (in the EncodeABC procedures) seems to
 * correspond with the observed phenomena, though it is not clear that this
 * interpretation is correct. Note also that the interpretation of gamma
 * requires that component intensities be positive.
 */

/*
 * The EncodeABC procedures for colorimetric RGB spaces. All three procedures
 * are the same, except for the array index used.
 */
#define colmet_DecodeABC_proc(procname, indx)                               \
      static float                                                         \
    procname(                                                               \
        double                          val,                                \
        const gs_cie_abc *              pabc                                \
    )                                                                       \
    {                                                                       \
        const pcl_cs_client_data_t *    pdata =                             \
                                             (const pcl_cs_client_data_t *) \
                                             pabc->common.client_data;      \
        double                          inv_gamma = pdata->inv_gamma[indx]; \
        double                          inv_gain = pdata->inv_gain[indx];   \
                                                                            \
        convert_val( val,                                                   \
                     pdata->min_val[indx],                                  \
                     pdata->range[indx],                                    \
                     pcl_lookup_tbl_get_tbl(pdata->plktbl1, indx)           \
                     );                                                     \
        if (val < 0.0)                                                      \
            val = 0.0;                                                      \
        if (inv_gamma != 1.0)                                               \
            val = pow(val, inv_gamma);                                      \
        if (inv_gain != 1.0)                                                \
            val = 1.0 - (1.0 - val) * inv_gain;                            \
        return val;                                                         \
    }

colmet_DecodeABC_proc(colmet_DecodeABC_0, 0)
    colmet_DecodeABC_proc(colmet_DecodeABC_1, 1)
    colmet_DecodeABC_proc(colmet_DecodeABC_2, 2)

     static const gs_cie_abc_proc3 colmet_DecodeABC = {
         {colmet_DecodeABC_0, colmet_DecodeABC_1, colmet_DecodeABC_2}
     };

/*
 * Build the matrix to convert a calibrated RGB color space to XYZ space; see
 * the discussion above for the reasoning behind this derivation.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
static int
build_colmet_conv_mtx(const pcl_cid_col_common_t * pdata,
                      pcl_vec3_t * pwhite_pt, pcl_mtx3_t * pmtx)
{
    pcl_vec3_t tmp_vec;
    pcl_mtx3_t inv_mtx;
    const float *pf = (float *)pdata->chroma;
    int i, code;

    for (i = 0; i < 3; i++) {
        double x = pf[2 * i];
        double y = pf[2 * i + 1];

        pmtx->a[3 * i] = x / y;
        pmtx->a[3 * i + 1] = 1.0;
        pmtx->a[3 * i + 2] = (1.0 - x - y) / y;
    }
    if ((code = pcl_mtx3_invert(pmtx, &inv_mtx)) < 0)
        return code;

    pwhite_pt->vc.v1 = pdata->chroma[3].x / pdata->chroma[3].y;
    pwhite_pt->vc.v2 = 1.0;
    pwhite_pt->vc.v3 = (1.0 - pdata->chroma[3].x - pdata->chroma[3].y)
        / pdata->chroma[3].y;
    pcl_vec3_xform(pwhite_pt, &tmp_vec, &inv_mtx);

    for (i = 0; i < 9; i++)
        pmtx->a[i] *= tmp_vec.va[i / 3];
    return 0;
}

/*
 * Finish the creation of a colorimetric RGB color space.
 */
static int
finish_colmet_cspace(gs_color_space * pcspace, const pcl_cid_data_t * pcid)
{
    pcl_mtx3_t mtxABC;
    pcl_vec3_t white_pt;
    const pcl_cid_col_common_t *pcoldata;
    int code = 0;

    if (pcid->len == 6)
        pcoldata = cid_data_default[pcl_cspace_Colorimetric].pchroma;
    else
        pcoldata = &(pcid->u.col.colmet);
    if ((code = build_colmet_conv_mtx(pcoldata, &white_pt, &mtxABC)) < 0)
        return code;

    /* RangeABC has the default value */
    *(gs_cie_abc_DecodeABC(pcspace)) = colmet_DecodeABC;
    pcl_mtx3_convert_to_gs(&mtxABC, gs_cie_abc_MatrixABC(pcspace));

    gs_cie_RangeLMN(pcspace)->ranges[0].rmin = 0;
    gs_cie_RangeLMN(pcspace)->ranges[0].rmax = white_pt.va[0];
    gs_cie_RangeLMN(pcspace)->ranges[1].rmin = 0;
    gs_cie_RangeLMN(pcspace)->ranges[1].rmax = white_pt.va[1];
    gs_cie_RangeLMN(pcspace)->ranges[2].rmin = 0;
    gs_cie_RangeLMN(pcspace)->ranges[2].rmax = white_pt.va[2];
    /* DecodeLMN and MatrixLMN have the default values */

    pcl_vec3_to_gs_vector3(gs_cie_WhitePoint(pcspace), white_pt);
    /* BlackPoint has the default value */

    return 0;
}

/*
 * The CIE L*a*b* case.
 *
 * The mapping from L*a*b* space to XYZ space is fairly simple over most of
 * its range, but becomes complicated in the range of dark grays because the
 * dominant cubic/cube root relationship changes to linear in this region.
 *
 * Let:
 *
 *    f(h) = (h > (6/29)^3 ? h^(1/3) : (29^2 / (3 * 6^2))  * h + 4/29)
 *
 *    g(h) = (h > (6/29)^3 ? 116 * h^(1/3) - 16 : (29/3)^3 * h)
 *
 * Note that, for h = (6/29)^3, the two different expressions for g(h) yield
 * the same result:
 *
 *    116 * h^(1.3) - 16 = (116 * 6 / 29) - 16 = 24 - 16 = 8, and
 *
 *    (29/3)^3 * h = (29 * 6 / 3 * 29)^3 = 2^3 = 8
 *
 * Since each part of g is monotonically increasing, g(h) is itself
 * monotonically increasing, and therefore invertible. Similarly, for
 * this value of h both expressions for f yield the same result:
 *
 *    h^(1/3) = 6/29, and
 *
 *    (29^2 / (3 * 6^2)) * h + 4/29 = (29^2 * 6^3) / (29^3 * 3 * 6^2) + 4/29
 *
 *                                  = 2/29 + 4/29 = 6/29
 *
 * Again, the individual parts of f are monotonically increasing, hence f is
 * monotonically increasing and therefore invertible.
 *
 * Let { Xw, 1.0, Yw } be the desired white-point. Then, the conversion from
 * XYZ ==> L*a*b* is given by:
 *
 *    L* = g(Y)
 *
 *    a* = 500 * (f(X/Xw) - f(Y))
 *
 *    b* = 200 * (f(Y) - f(Z/Zw))
 *
 * Inverting this relationship, we find that:
 *
 *    Y = g^-1(L*)
 *
 *    X = Xw * f^-1(a* / 500 + f(Y)) = Xw * f^-1(a* / 500 + f(g^-1(L*)))
 *
 *    Z = Zw * f^-1(b* / 200 + f(Y)) = Zw * f^-1(f(g^-1(L*)) - b* / 200)
 *
 * Before providing expressions for f^-1 and g^-1 (we know from the argument
 * above that these functions exist), we should note that the structure of the
 * PostScript CIE color spaces cannot deal directly with a relationship such as
 * described above, because all cross-component steps (operations that depend
 * more than one component) must be linear. It is possible, however, to convert
 * the relationship to the required form by extracting the value of Y in two
 * steps. This is accomplished by the following algorithm:
 *
 *    T1 = f(g^-1(L*))
 *    a1 = a* / 500
 *    b1 = b* / 200
 *                                       -         -
 *    { a2, T1, b2 } = { T1, a1, b1 } * | 1   1   1 |
 *                                      | 1   0   0 |
 *                                      | 0   0  -1 |
 *                                       -         -
 *    X = Xw * f^-1(a2)
 *    Y = f^-1(f(g^-1(L*)))
 *    Z = Zw * f^-1(b2)
 *
 * While the handling of the L* ==> Y conversion in this algorithm may seem a
 * bit overly complex, it is perfectly legitimate and, as shown below, results
 * in a very simple expression.
 *
 * To complete the algorithm, expressions for f^-1 and g^-1 must be provided.
 * These are derived directly from the forward expressions:
 *
 *    f^-1(h) = (h > 6/29 ? h^3 : (3 * 6^2) * (h - 4/29) / 29^2)
 *
 *    g^-1(h) = (h > 8 ? ((h + 16) / 116)^3 : (3/29)^3 * h)
 *
 * Note that because both f and g change representations at the same point,
 * there is only a single representation of f(g^-1(h)) required. Specifically,
 * if h > 8, then
 *
 *     g-1(h) = ((h + 16) / 116)^3 > (24 / 116)^3 = (6/29)^3
 *
 * so
 *
 *     f(g^-1(h)) = (g^-1(h))^(1/3) = (h + 16) / 116
 *
 * while if h <= 8
 *
 *     g-1(h) = (3/29)^3 * h <= (6/29)^3
 *
 * so
 *
 *     f(g-1(h)) = (29^2 / (3 * 6^2)) * g^-1(h) + 4/29
 *
 *               = ((29^2 * 3^3) / (29^3 * 3 * 6^2)) * h + 4/29
 *
 *               = h/116 + 16/116 = (h + 16) / 116
 *
 * This is the algorithm used below, with the Encode procedures also responsible
 * for implementing the color lookup tables (if present).
 */

/*
 * Unlike the other color spaces, the DecodeABC procedures for the CIE L*a*b*
 * color space have slightly different code for the different components. The
 * conv_code operand allows for this difference.
 */
#define lab_DecodeABC_proc(procname, indx, conv_code)                       \
      static float                                                         \
    procname(                                                               \
        double                          val,                                \
        const gs_cie_abc *              pabc                                \
    )                                                                       \
    {                                                                       \
        const pcl_cs_client_data_t *    pdata =                             \
                                              (const pcl_cs_client_data_t *)\
                                              pabc->common.client_data;     \
                                                                            \
        convert_val( val,                                                   \
                     pdata->min_val[indx],                                  \
                     pdata->range[indx],                                    \
                     pcl_lookup_tbl_get_tbl(pdata->plktbl1, indx)           \
                     );                                                     \
        conv_code;                                                          \
        return val;                                                         \
    }

lab_DecodeABC_proc(lab_DecodeABC_0, 0, (val = (val + 16.0) / 116.0))
    lab_DecodeABC_proc(lab_DecodeABC_1, 1, (val /= 500))
    lab_DecodeABC_proc(lab_DecodeABC_2, 2, (val /= 200))

     static const gs_cie_abc_proc3 lab_DecodeABC = {
         {lab_DecodeABC_0, lab_DecodeABC_1, lab_DecodeABC_2}
     };

static const gs_matrix3 lab_MatrixABC = {
    {1, 1, 1}, {1, 0, 0}, {0, 0, -1},
    false
};

/*
 * The DecodeLMN procedures for CIE L*a*b* color spaces are all identical
 * except for the index. The explicit use of the white point is overkill,
 * since we know this will always be the D65 white point with Y normalized
 * to 1.0, but it guards against future variations.
 */
#define lab_DecodeLMN_proc(procname, indx)                          \
      static float                                                 \
    procname(                                                       \
        double                  val,                                \
        const gs_cie_common *   pcie                                \
    )                                                               \
    {                                                               \
        if (val > 6.0 / 29.0)                                       \
            val = val * val * val;                                  \
        else                                                        \
            val = 108 * (29.0 * val + 4) / (29.0 * 29.0 * 29.0);    \
        val *= (&(pcie->points.WhitePoint.u))[indx];                \
        return val;                                                 \
    }

lab_DecodeLMN_proc(lab_DecodeLMN_0, 0)
lab_DecodeLMN_proc(lab_DecodeLMN_1, 1)
lab_DecodeLMN_proc(lab_DecodeLMN_2, 2)

static const gs_cie_common_proc3 lab_DecodeLMN = {
    {lab_DecodeLMN_0, lab_DecodeLMN_1, lab_DecodeLMN_2}
};

static const gs_vector3 lab_WhitePoint = { .9504f, 1.0f, 1.0889f };

/*
 * Finish the creation of a CIE L*a*b* color space.
 */
static int
finish_lab_cspace(gs_color_space * pcspace, const pcl_cid_data_t * pcid)
{
    /* RangeABC has the default value */
    *(gs_cie_abc_DecodeABC(pcspace)) = lab_DecodeABC;
    *(gs_cie_abc_MatrixABC(pcspace)) = lab_MatrixABC;

    /* RangeLMN and MatrixLMN have the default values */
    *(gs_cie_DecodeLMN(pcspace)) = lab_DecodeLMN;

    gs_cie_WhitePoint(pcspace) = lab_WhitePoint;
    /* BlackPoint has the default value */
    return 0;
}

/*
 * The luminance-chrominance color space
 *
 * As HP would have it, the matrix provided in the long-form luminance-
 * chrominance color space specification maps the calibrated RGB coordinates
 * to the coordinates of the source space. This is, of course, the inverse
 * of the transform that is useful: from the desired color space to calibrated
 * RGB.
 *
 * The rest of the handling of luminance-chrominance spaces is similar to
 * that for colorimetric RGB spaces. Note, however, that in this case the
 * RangeLMN is the default value (primary components must be clipped to
 * [0, 1]), and the DecodeLMN function must verify that its output is still
 * in this range.
 *
 * As commented upon elsewhere, HP allows multiple lookup tables to be attached
 * to the same color space, but does not clarify what this should do. The
 * lookup table for the device dependent color spaces is not a problem; this
 * is implemented as a transfer function. The situation with device independent
 * color spaces is not as clear. The choice made here is to allow two device
 * independent lookup tables to be applied to the luminance-chrominance color
 * space: the luminance-chrominance lookup table and the colorimetric RGB
 * lookup table. This does not match HP's behavior, but the latter does not
 * make any sense, so this should not be an issue.
 */

/*
 * The DecodeABC procedures for luminance-chrominance color space are simple.
 */
#define lumchrom_DecodeABC_proc(procname, indx)                             \
      static float                                                         \
    procname(                                                               \
        double                          val,                                \
        const gs_cie_abc *              pabc                                \
    )                                                                       \
    {                                                                       \
        const pcl_cs_client_data_t *    pdata =                             \
                                             (const pcl_cs_client_data_t *) \
                                             pabc->common.client_data;      \
                                                                            \
        convert_val( val,                                                   \
                     pdata->min_val[indx],                                  \
                     pdata->range[indx],                                    \
                     pcl_lookup_tbl_get_tbl(pdata->plktbl1, indx)           \
                   );                                                       \
        return val;                                                         \
    }

lumchrom_DecodeABC_proc(lumchrom_DecodeABC_0, 0)
lumchrom_DecodeABC_proc(lumchrom_DecodeABC_1, 1)
lumchrom_DecodeABC_proc(lumchrom_DecodeABC_2, 2)

static const gs_cie_abc_proc3 lumchrom_DecodeABC = {
    {lumchrom_DecodeABC_0, lumchrom_DecodeABC_1, lumchrom_DecodeABC_2}
};

/*
 * The DecodeLMN procedures for luminance-chrominance spaces are similar
 * to the colorimetric DecodeABC procedures. Since there is no Range* parameter
 * for the XYZ components, this procedure checks the range of its output.
 */
#define lumchrom_DecodeLMN_proc(procname, indx)                             \
      static float                                                         \
    procname(                                                               \
        double                          val,                                \
        const gs_cie_common *           pcie                                \
    )                                                                       \
    {                                                                       \
        const pcl_cs_client_data_t *    pdata =                             \
                                              (const pcl_cs_client_data_t *)\
                                              pcie->client_data;            \
        double                          inv_gamma = pdata->inv_gamma[indx]; \
        double                          inv_gain = pdata->inv_gain[indx];   \
                                                                            \
        convert_val( val,                                                   \
                     0.0,                                                   \
                     1.0,                                                   \
                     pcl_lookup_tbl_get_tbl(pdata->plktbl2, indx));         \
        if (inv_gamma != 1.0)                                               \
            val = pow(val, inv_gamma);                                      \
        if (inv_gain != 1.0)                                                \
            val = 1.0 - (1.0 - val) * inv_gain;                             \
        if (val < 0.0)                                                      \
            val = 0.0;                                                      \
        else if (val > 1.0)                                                 \
            val = 1.0;                                                      \
        return val;                                                         \
    }

lumchrom_DecodeLMN_proc(lumchrom_DecodeLMN_0, 0)
lumchrom_DecodeLMN_proc(lumchrom_DecodeLMN_1, 1)
lumchrom_DecodeLMN_proc(lumchrom_DecodeLMN_2, 2)

static const gs_cie_common_proc3 lumchrom_DecodeLMN = {
    {lumchrom_DecodeLMN_0, lumchrom_DecodeLMN_1, lumchrom_DecodeLMN_2}
};

/*
 * Build the MatrixABC value for a luminance/chrominance color space. Note that
 * this is the inverse of the matrix provided in the Configure Image Data
 * command.
 *
 * Return 0 on success, < 0 in the event of an error.
 */
static int
build_lum_chrom_mtxABC(const float pin_mtx[9], pcl_mtx3_t * pmtxABC)
{
    int i;
    pcl_mtx3_t tmp_mtx;

    /* transpose the input to create a row-order matrix */
    for (i = 0; i < 3; i++) {
        int j;

        for (j = 0; j < 3; j++)
            tmp_mtx.a[i * 3 + j] = pin_mtx[i + 3 * j];
    }

    return pcl_mtx3_invert(&tmp_mtx, pmtxABC);
}

/*
 * Finish the creation of a luminance-chrominance color space.
 */
static int
finish_lumchrom_cspace(gs_color_space * pcspace, const pcl_cid_data_t * pcid)
{
    const float *pin_mtx;
    pcl_mtx3_t mtxABC, mtxLMN;
    pcl_vec3_t white_pt;
    const pcl_cid_col_common_t *pcoldata;
    int code = 0;

    if (pcid->len == 6) {
        pcoldata = cid_data_default[pcl_cspace_LumChrom].pchroma;
        pin_mtx = cid_data_default[pcl_cspace_LumChrom].pxform;
    } else {
        pcoldata = &(pcid->u.lum.colmet);
        pin_mtx = pcid->u.lum.matrix;
    }

    if (((code = build_lum_chrom_mtxABC(pin_mtx, &mtxABC)) < 0) ||
        ((code = build_colmet_conv_mtx(pcoldata, &white_pt, &mtxLMN)) < 0))
        return code;

    /* RangeABC has the default value */
    *(gs_cie_abc_DecodeABC(pcspace)) = lumchrom_DecodeABC;
    pcl_mtx3_convert_to_gs(&mtxABC, gs_cie_abc_MatrixABC(pcspace));

    /* RangeLMN has the default value */
    *(gs_cie_DecodeLMN(pcspace)) = lumchrom_DecodeLMN;
    pcl_mtx3_convert_to_gs(&mtxLMN, gs_cie_MatrixLMN(pcspace));

    pcl_vec3_to_gs_vector3(gs_cie_WhitePoint(pcspace), white_pt);
    /* BlackPoint has the default value */

    return 0;
}

static int (*const finish_cspace[(int)pcl_cspace_num]) (gs_color_space *,
                                                        const pcl_cid_data_t
                                                        *) = {
    0,                      /* pcl_cspace_RGB */
    0,                      /* pcl_cspace_CMY */
    finish_colmet_cspace,   /* pcl_cspace_Colorimetric */
    finish_lab_cspace,      /* pcl_cspace_CIELab */
    finish_lumchrom_cspace  /* pcl_cspace_LumChrom */
};

/*
 * Free a PCL base color space. This decrements the reference count for the
 * GS color space, and frees any lookup tables that might have been
 * used (device independent color spaces only).
 */
static void
free_base_cspace(gs_memory_t * pmem, void *pvbase, client_name_t cname)
{
    pcl_cs_base_t *pbase = (pcl_cs_base_t *) pvbase;

    rc_decrement(pbase->pcspace, "free_base_cspace");
    gs_free_object(pmem, pvbase, cname);
}

/*
 * Allocate a PCL base color space.
 *
 * Because a PCL base color space and the associated graphic-library color
 * space must be kept in a one-to-one relationship, the latter color space is
 * allocated here as well. For this reason the PCL color space type is
 * an operand.
 *
 * Returns 0 on success, e_Memory in the event of a memory error.
 */
static int
alloc_base_cspace(pcl_cs_base_t ** ppbase,
                  pcl_cspace_type_t type, gs_memory_t * pmem)
{
    pcl_cs_base_t *pbase = 0;
    int code = 0;

    *ppbase = 0;
    rc_alloc_struct_1(pbase,
                      pcl_cs_base_t,
                      &st_cs_base_t,
                      pmem, return e_Memory, "allocate pcl base color space");
    pbase->rc.free = free_base_cspace;
    pbase->type = type;
    pbase->client_data.plktbl1 = 0;
    pbase->client_data.plktbl2 = 0;
    pbase->pcspace = 0;

    if (type == pcl_cspace_White)
        pbase->pcspace = gs_cspace_new_DeviceGray(pmem);
    else if (type <= pcl_cspace_CMY)
        pbase->pcspace = gs_cspace_new_DeviceRGB(pmem);
    else
        code = gs_cspace_build_CIEABC(&(pbase->pcspace),
                                      &(pbase->client_data), pmem);
    if (code < 0 || pbase->pcspace == NULL)
        free_base_cspace(pmem, pbase, "allocate pcl base color space");
    else
        *ppbase = pbase;
    return code;
}

/*
 * Create a unique instance of a pcl_cs_base_t object (if one does not already
 * exist).
 *
 * This code is not fully legitimate. To assure that a PCL base color space is
 * unique, it is also necessary to assure that the associated graphics library
 * color space is unique. Unfortunately, that is not a simple matter, because
 * graphic library color spaces are not themselves reference counted, though
 * they have reference-counted elements.
 *
 * We can get away with this arrangement for now by relying on a one-to-one
 * association between PCL base color spaces and the associated graphic library
 * color spaces. For all current implementations of the graphic library this
 * will work. The code may fail, however, for implementations that use a
 * "lazy evaluation" technique, as these may require access to the graphics
 * library color space after the PCL base color space has been released (the
 * graphic library color space will still be present in this case, but its
 * client data may have been changed).
 */
static int
unshare_base_cspace(const gs_memory_t * mem, pcl_cs_base_t ** ppbase)
{
    pcl_cs_base_t *pbase = *ppbase;
    pcl_cs_base_t *pnew = 0;
    int code;

    /* check if there is anything to do */
    if (pbase->rc.ref_count == 1)
        return 0;
    rc_decrement(pbase, "unshare PCL base color space");

    /* allocate a new gs_color_space */
    if ((code = alloc_base_cspace(ppbase, pbase->type, pbase->rc.memory)) < 0)
        return code;
    pnew = *ppbase;

    /* copy the client data */
    init_client_data_from(&(pnew->client_data), &(pbase->client_data));

    /* copy the color space (primarily for CIE color spaces; UGLY!!!) */
    if (pbase->type > pcl_cspace_CMY) {
        gs_cie_abc *pcs1 = pbase->pcspace->params.abc;
        gs_cie_abc *pcs2 = pnew->pcspace->params.abc;
        pcs2->common.install_cspace = pcs1->common.install_cspace;
        pcs2->common.RangeLMN = pcs1->common.RangeLMN;
        pcs2->common.DecodeLMN = pcs1->common.DecodeLMN;
        pcs2->common.MatrixLMN = pcs1->common.MatrixLMN;
        pcs2->common.points = pcs1->common.points;
        pcs2->RangeABC = pcs1->RangeABC;
        pcs2->DecodeABC = pcs1->DecodeABC;
        pcs2->MatrixABC = pcs1->MatrixABC;
    } else
        pnew->pcspace->params.pixel = pbase->pcspace->params.pixel;
    return 0;
}

/*
 * Build a PCL base color space. This should be invoked whenever a color space
 * is required, typically after a Configure Image Data (CID) command.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int
pcl_cs_base_build_cspace(pcl_cs_base_t ** ppbase,
                         const pcl_cid_data_t * pcid, gs_memory_t * pmem)
{
    pcl_cs_base_t *pbase = *ppbase;
    pcl_cspace_type_t type = pcl_cid_get_cspace(pcid);
    int code = 0;

    /* release the existing color space, if present */
    if (pbase != 0) {
        if_debug1m('c', pmem, "[c]releasing color space:%s\n",
                   pcl_cid_cspace_get_debug_name(pmem, pbase->type));
        rc_decrement(pbase, "build base pcl color space");
    }
    /* build basic structure and client info. structure */
    if ((code = alloc_base_cspace(ppbase, type, pmem)) < 0)
        return code;
    pbase = *ppbase;
    build_client_data(&(pbase->client_data), pcid, pmem);

    /* fill in color space parameters */
    if ((finish_cspace[type] != 0) &&
        ((code = finish_cspace[type] (pbase->pcspace, pcid)) < 0))
        free_base_cspace(pmem, pbase, "build base pcl color space");
    return code;
}

/*
 * Build a special base color space, used for setting the color white.
 * This base space is unique in that it uses the DeviceGray graphic library
 * color space.
 *
 * This routine is usually called once at initialization.
 */
int
pcl_cs_base_build_white_cspace(pcl_state_t * pcs,
                               pcl_cs_base_t ** ppbase, gs_memory_t * pmem)
{
    int code = 0;

    if (pcs->pwhite_cs == 0)
        code = alloc_base_cspace(&pcs->pwhite_cs, pcl_cspace_White, pmem);
    if (code >= 0)
        pcl_cs_base_copy_from(*ppbase, pcs->pwhite_cs);
    return code;
}

/*
 * Update the lookup table information for a base color space. This applies
 * only to device-independent color spaces (updating device dependent color
 * spaces updates the transfer function in the current halftone). Passing a
 * null pointer for the lookup table operand resets the tables for ALL color
 * spaces to be the identity table.
 *
 * See the comments in pclookup.h for a description of how device independent
 * lookup tables are interpreted in this implementation.
 *
 * Returns > 0 if the update changed the color space, 0 if the update did not
 * change the color space, and < 0 in the event of an error. If the base color
 * Space was updated, the current PCL indexed color space (which includes this
 * color space as a base color space) must also be updated.
 */
int
pcl_cs_base_update_lookup_tbl(pcl_cs_base_t ** ppbase,
                              pcl_lookup_tbl_t * plktbl)
{
    pcl_cs_base_t *pbase = *ppbase;
    pcl_lookup_tbl_t *plktbl1 = pbase->client_data.plktbl1;
    pcl_lookup_tbl_t *plktbl2 = pbase->client_data.plktbl2;
    int code = 0;

    if (plktbl == 0) {
        if ((pbase->client_data.plktbl1 == 0) &&
            (pbase->client_data.plktbl2 == 0))
            return 0;
        plktbl1 = 0;
        plktbl2 = 0;

    } else {
        pcl_cspace_type_t cstype = pbase->type;
        pcl_cspace_type_t lktype = pcl_lookup_tbl_get_cspace(plktbl);

        /* lookup tables for "higher" color spaces are always ignored */
        if ((cstype < lktype) ||
            (lktype == pcl_cspace_RGB) || (lktype == pcl_cspace_CMY))
            return 0;

        /* CIE L*a*b* space and the L*a*b* lookup table must match */
        if ((cstype == pcl_cspace_CIELab) || (lktype == pcl_cspace_CIELab)) {
            plktbl1 = plktbl;
        } else if (cstype == lktype)
            plktbl1 = plktbl;
        else
            plktbl2 = plktbl;
    }

    /* make a unique copy of the base color space */
    if ((code = unshare_base_cspace(pbase->rc.memory, ppbase)) < 0)
        return code;
    pbase = *ppbase;

    /* update the lookup table information */
    update_lookup_tbls(&(pbase->client_data), plktbl1, plktbl2);

    return 1;
}

/*
 * Install a base color space into the graphic state.
 *
 * The pointer-pointer form of the first operand is for consistency with the
 * other "install" procedures.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int
pcl_cs_base_install(pcl_cs_base_t ** ppbase, pcl_state_t * pcs)
{
    return gs_setcolorspace(pcs->pgs, (*ppbase)->pcspace);
}

/*
 * One-time initialization routine. This exists only to handle possible non-
 * initialization of BSS.
 */
void
pcl_cs_base_init(pcl_state_t * pcs)
{
    pcs->pwhite_cs = 0;
}
