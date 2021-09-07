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


/* Shading rendering support */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsrect.h"
#include "gxcspace.h"
#include "gscindex.h"
#include "gscie.h"		/* requires gscspace.h */
#include "gxdevcli.h"
#include "gxgstate.h"
#include "gxdht.h"		/* for computing # of different colors */
#include "gxpaint.h"
#include "gxshade.h"
#include "gxshade4.h"
#include "gsicc.h"
#include "gsicc_cache.h"
#include "gxcdevn.h"
#include "gximage.h"

/* Define a maximum smoothness value. */
/* smoothness > 0.2 produces severely blocky output. */
#define MAX_SMOOTHNESS 0.2

/* ================ Packed coordinate streams ================ */

/* Forward references */
static int cs_next_packed_value(shade_coord_stream_t *, int, uint *);
static int cs_next_array_value(shade_coord_stream_t *, int, uint *);
static int cs_next_packed_decoded(shade_coord_stream_t *, int,
                                   const float[2], float *);
static int cs_next_array_decoded(shade_coord_stream_t *, int,
                                  const float[2], float *);
static void cs_packed_align(shade_coord_stream_t *cs, int radix);
static void cs_array_align(shade_coord_stream_t *cs, int radix);
static bool cs_eod(const shade_coord_stream_t * cs);

/* Initialize a packed value stream. */
void
shade_next_init(shade_coord_stream_t * cs,
                const gs_shading_mesh_params_t * params,
                const gs_gstate * pgs)
{
    cs->params = params;
    cs->pctm = &pgs->ctm;
    if (data_source_is_stream(params->DataSource)) {
        /*
         * Rewind the data stream iff it is reusable -- either a reusable
         * file or a reusable string.
         */
        stream *s = cs->s = params->DataSource.data.strm;

        if ((s->file != 0 && s->file_limit != max_long) ||
            (s->file == 0 && s->strm == 0)
            )
            sseek(s, 0);
    } else {
        s_init(&cs->ds, NULL);
        sread_string(&cs->ds, params->DataSource.data.str.data,
                     params->DataSource.data.str.size);
        cs->s = &cs->ds;
    }
    if (data_source_is_array(params->DataSource)) {
        cs->get_value = cs_next_array_value;
        cs->get_decoded = cs_next_array_decoded;
        cs->align = cs_array_align;
    } else {
        cs->get_value = cs_next_packed_value;
        cs->get_decoded = cs_next_packed_decoded;
        cs->align = cs_packed_align;
    }
    cs->is_eod = cs_eod;
    cs->left = 0;
    cs->ds_EOF = false;
    cs->first_patch = 1;
}

/* Check for the End-Of-Data state form a stream. */
static bool
cs_eod(const shade_coord_stream_t * cs)
{
    return cs->ds_EOF;
}

/* Get the next (integer) value from a packed value stream. */
/* 1 <= num_bits <= sizeof(uint) * 8. */
static int
cs_next_packed_value(shade_coord_stream_t * cs, int num_bits, uint * pvalue)
{
    uint bits = cs->bits;
    int left = cs->left;

    if (left >= num_bits) {
        /* We can satisfy this request with the current buffered bits. */
        cs->left = left -= num_bits;
        *pvalue = (bits >> left) & ((1 << num_bits) - 1);
    } else {
        /* We need more bits. */
        int needed = num_bits - left;
        uint value = bits & ((1 << left) - 1);	/* all the remaining bits */

        for (; needed >= 8; needed -= 8) {
            int b = sgetc(cs->s);

            if (b < 0) {
                cs->ds_EOF = true;
                return_error(gs_error_rangecheck);
            }
            value = (value << 8) + b;
        }
        if (needed == 0) {
            cs->left = 0;
            *pvalue = value;
        } else {
            int b = sgetc(cs->s);

            if (b < 0) {
                cs->ds_EOF = true;
                return_error(gs_error_rangecheck);
            }
            cs->bits = b;
            cs->left = left = 8 - needed;
            *pvalue = (value << needed) + (b >> left);
        }
    }
    return 0;
}

/*
 * Get the next (integer) value from an unpacked array.  Note that
 * num_bits may be 0 if we are reading a coordinate or color value.
 */
static int
cs_next_array_value(shade_coord_stream_t * cs, int num_bits, uint * pvalue)
{
    float value;
    uint read;

    if (sgets(cs->s, (byte *)&value, sizeof(float), &read) < 0 ||
        read != sizeof(float)) {
        cs->ds_EOF = true;
        return_error(gs_error_rangecheck);
    }
    if (value < 0 || (num_bits != 0 && num_bits < sizeof(uint) * 8 &&
         value >= (1 << num_bits)) ||
        value != (uint)value
        )
        return_error(gs_error_rangecheck);
    *pvalue = (uint) value;
    return 0;
}

/* Get the next decoded floating point value. */
static int
cs_next_packed_decoded(shade_coord_stream_t * cs, int num_bits,
                       const float decode[2], float *pvalue)
{
    uint value;
    int code = cs->get_value(cs, num_bits, &value);
    double max_value = (double)(uint)
        (num_bits == sizeof(uint) * 8 ? ~0 : ((1 << num_bits) - 1));
    double dvalue = (double)value;

    if (code < 0)
        return code;
    *pvalue =
        (decode == 0 ? dvalue / max_value :
         decode[0] + dvalue * (decode[1] - decode[0]) / max_value);
    return 0;
}

/* Get the next floating point value from an array, without decoding. */
static int
cs_next_array_decoded(shade_coord_stream_t * cs, int num_bits,
                      const float decode[2], float *pvalue)
{
    float value;
    uint read;

    if (sgets(cs->s, (byte *)&value, sizeof(float), &read) < 0 ||
        read != sizeof(float)
    ) {
        cs->ds_EOF = true;
        return_error(gs_error_rangecheck);
    }
    *pvalue = value;
    return 0;
}

static void
cs_packed_align(shade_coord_stream_t *cs, int radix)
{
    cs->left = cs->left / radix * radix;
}

static void
cs_array_align(shade_coord_stream_t *cs, int radix)
{
}

/* Get the next flag value. */
/* Note that this always starts a new data byte. */
int
shade_next_flag(shade_coord_stream_t * cs, int BitsPerFlag)
{
    uint flag;
    int code;

    cs->left = 0;		/* start a new byte if packed */
    code = cs->get_value(cs, BitsPerFlag, &flag);
    return (code < 0 ? code : flag);
}

/* Get one or more coordinate pairs. */
int
shade_next_coords(shade_coord_stream_t * cs, gs_fixed_point * ppt,
                  int num_points)
{
    int num_bits = cs->params->BitsPerCoordinate;
    const float *decode = cs->params->Decode;
    int code = 0;
    int i;

    for (i = 0; i < num_points; ++i) {
        float x, y;

        if ((code = cs->get_decoded(cs, num_bits, decode, &x)) < 0 ||
            (code = cs->get_decoded(cs, num_bits, decode + 2, &y)) < 0 ||
            (code = gs_point_transform2fixed(cs->pctm, x, y, &ppt[i])) < 0
            )
            break;
    }
    return code;
}

/* Get a color.  Currently all this does is look up Indexed colors. */
int
shade_next_color(shade_coord_stream_t * cs, float *pc)
{
    const float *decode = cs->params->Decode + 4;	/* skip coord decode */
    const gs_color_space *pcs = cs->params->ColorSpace;
    gs_color_space_index index = gs_color_space_get_index(pcs);
    int num_bits = cs->params->BitsPerComponent;

    if (index == gs_color_space_index_Indexed) {
        int ncomp = gs_color_space_num_components(gs_cspace_base_space(pcs));
        int ci;
        float cf;
        int code = cs->get_decoded(cs, num_bits, decode, &cf);
        gs_client_color cc;
        int i;

        if (code < 0)
            return code;
        if (cf < 0)
            return_error(gs_error_rangecheck);
        ci = (int)cf;
        if (ci >= gs_cspace_indexed_num_entries(pcs))
            return_error(gs_error_rangecheck);
        code = gs_cspace_indexed_lookup(pcs, ci, &cc);
        if (code < 0)
            return code;
        for (i = 0; i < ncomp; ++i)
            pc[i] = cc.paint.values[i];
    } else {
        int i, code;
        int ncomp = (cs->params->Function != 0 ? 1 :
                     gs_color_space_num_components(pcs));

        for (i = 0; i < ncomp; ++i) {
            if ((code = cs->get_decoded(cs, num_bits, decode + i * 2, &pc[i])) < 0)
                return code;
            if (cs->params->Function) {
                gs_function_params_t *params = &cs->params->Function->params;

                if (pc[i] < params->Domain[i + i])
                    pc[i] = params->Domain[i + i];
                else if (pc[i] > params->Domain[i + i + 1])
                    pc[i] = params->Domain[i + i + 1];
            }
        }
    }
    return 0;
}

/* Get the next vertex for a mesh element. */
int
shade_next_vertex(shade_coord_stream_t * cs, shading_vertex_t * vertex, patch_color_t *c)
{   /* Assuming p->c == c, provides a non-const access. */
    int code = shade_next_coords(cs, &vertex->p, 1);

    if (code >= 0)
        code = shade_next_color(cs, c->cc.paint.values);
    if (code >= 0)
        cs->align(cs, 8); /* CET 09-47J.PS SpecialTestI04Test01. */
    return code;
}

/* ================ Shading rendering ================ */

/* Initialize the common parts of the recursion state. */
int
shade_init_fill_state(shading_fill_state_t * pfs, const gs_shading_t * psh,
                      gx_device * dev, gs_gstate * pgs)
{
    const gs_color_space *pcs = psh->params.ColorSpace;
    float max_error = min(pgs->smoothness, MAX_SMOOTHNESS);
    bool is_lab;
    bool cs_lin_test;
    int code;

    /*
     * There's no point in trying to achieve smoothness beyond what
     * the device can implement, i.e., the number of representable
     * colors times the number of halftone levels.
     */
    long num_colors =
        max(dev->color_info.max_gray, dev->color_info.max_color) + 1;
    const gs_range *ranges = 0;
    int ci;
    gsicc_rendering_param_t rendering_params;

    pfs->cs_always_linear = false;
    pfs->dev = dev;
    pfs->pgs = pgs;
top:
    pfs->direct_space = pcs;
    pfs->num_components = gs_color_space_num_components(pcs);
    switch ( gs_color_space_get_index(pcs) )
        {
        case gs_color_space_index_Indexed:
            pcs = gs_cspace_base_space(pcs);
            goto top;
        case gs_color_space_index_CIEDEFG:
            ranges = pcs->params.defg->RangeDEFG.ranges;
            break;
        case gs_color_space_index_CIEDEF:
            ranges = pcs->params.def->RangeDEF.ranges;
            break;
        case gs_color_space_index_CIEABC:
            ranges = pcs->params.abc->RangeABC.ranges;
            break;
        case gs_color_space_index_CIEA:
            ranges = &pcs->params.a->RangeA;
            break;
        case gs_color_space_index_ICC:
            ranges = pcs->cmm_icc_profile_data->Range.ranges;
            break;
        default:
            break;
        }
    if (num_colors <= 32) {
        /****** WRONG FOR MULTI-PLANE HALFTONES ******/
        num_colors *= pgs->dev_ht[HT_OBJTYPE_DEFAULT]->components[0].corder.num_levels;
    }
    if (psh->head.type == 2 || psh->head.type == 3) {
        max_error *= 0.25;
        num_colors *= 2;
    }
    if (max_error < 1.0 / num_colors)
        max_error = 1.0 / num_colors;
    for (ci = 0; ci < pfs->num_components; ++ci)
        pfs->cc_max_error[ci] =
            (ranges == 0 ? max_error :
             max_error * (ranges[ci].rmax - ranges[ci].rmin));
    if (pgs->has_transparency && pgs->trans_device != NULL) {
        pfs->trans_device = pgs->trans_device;
    } else {
        pfs->trans_device = dev;
    }
    /* If the CS is PS based and we have not yet converted to the ICC form
       then go ahead and do that now */
    if (gs_color_space_is_PSCIE(pcs) && pcs->icc_equivalent == NULL) {
        code = gs_colorspace_set_icc_equivalent((gs_color_space *)pcs, &(is_lab), pgs->memory);
        if (code < 0)
            return code;
    }
    rendering_params.black_point_comp = pgs->blackptcomp;
    rendering_params.graphics_type_tag = GS_VECTOR_TAG;
    rendering_params.override_icc = false;
    rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
    rendering_params.rendering_intent = pgs->renderingintent;
    rendering_params.cmm = gsCMM_DEFAULT;
    /* Grab the icc link transform that we need now */
    if (pcs->cmm_icc_profile_data != NULL) {
        pfs->icclink = gsicc_get_link(pgs, pgs->trans_device, pcs, NULL,
                                      &rendering_params, pgs->memory);
        if (pfs->icclink == NULL)
            return_error(gs_error_VMerror);
    } else {
        if (pcs->icc_equivalent != NULL ) {
            /* We have a PS equivalent ICC profile.  We may need to go
               through special range adjustments in this case */
            pfs->icclink = gsicc_get_link(pgs, pgs->trans_device,
                                          pcs->icc_equivalent, NULL,
                                          &rendering_params, pgs->memory);
            if (pfs->icclink == NULL)
                return_error(gs_error_VMerror);
        } else {
            pfs->icclink = NULL;
        }
    }
    /* Two possible cases of interest here for performance.  One is that the
    * icclink is NULL, which could occur if the source space were DeviceN or
    * a separation color space, while at the same time, the output device
    * supports these colorants (e.g. a separation device).   The other case is
    * that the icclink is the identity.  This could happen for example if the
    * source space were CMYK and we are going out to a CMYK device. For both
    * of these cases we can avoid going through the standard
    * color mappings to determine linearity. This is true, provided that the
    * transfer function is linear.  It is likely that we can improve
    * things even in cases where the transfer function is nonlinear, but for
    * now, we will punt on those and let them go through the longer processing
    * steps */
    if (pfs->icclink == NULL)
            cs_lin_test = !(using_alt_color_space((gs_gstate*)pgs));
    else
        cs_lin_test = pfs->icclink->is_identity;

    if (cs_lin_test && !gx_has_transfer(pgs, dev->color_info.num_components)) {
        pfs->cs_always_linear = true;
    }

#ifdef IGNORE_SPEC_MATCH_ADOBE_SHADINGS
    /* Per the spec. If the source space is DeviceN or Separation and the
       colorants are not supported (i.e. if we are using the alternate tint
       transform) the interpolation should occur in the source space to
       accommodate non-linear tint transform functions.
       e.g. We had a case where the transform function
       was an increasing staircase. Including that function in the
       gradient smoothness calculation gave us severe quantization. AR on
       the other hand is doing the interpolation in device color space
       and has a smooth result for that case. So AR is not following the spec. The
       bit below solves the issues for Type 4 and Type 5 shadings as
       this will avoid interpolations in source space. Type 6 and Type 7 will still
       have interpolations in the source space even if pfs->cs_always_linear == true.
       So the approach below does not solve those issues. To do that
       without changing the shading code, we could make a linear
       approximation to the alternate tint transform, which would
       ensure smoothness like what AR provides.
    */
    if ((gs_color_space_get_index(pcs) == gs_color_space_index_DeviceN ||
        gs_color_space_get_index(pcs) == gs_color_space_index_Separation) &&
        using_alt_color_space((gs_gstate*)pgs) && (psh->head.type == 4 ||
        psh->head.type == 5)) {
        pfs->cs_always_linear = true;
    }
#endif

    return 0;
}

/* Fill one piece of a shading. */
int
shade_fill_path(const shading_fill_state_t * pfs, gx_path * ppath,
                gx_device_color * pdevc, const gs_fixed_point *fill_adjust)
{
    gx_fill_params params;

    params.rule = -1;		/* irrelevant */
    params.adjust = *fill_adjust;
    params.flatness = 0;	/* irrelevant */
    return (*dev_proc(pfs->dev, fill_path)) (pfs->dev, pfs->pgs, ppath,
                                             &params, pdevc, NULL);
}
