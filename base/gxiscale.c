/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Interpolated image procedures */
#include "gx.h"
#include "math_.h"
#include "memory_.h"
#include "stdint_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxfrac.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxdcolor.h"
#include "gxgstate.h"
#include "gxdevmem.h"
#include "gxcpath.h"
#include "gximage.h"
#include "stream.h"             /* for s_alloc_state */
#include "siinterp.h"           /* for spatial interpolation */
#include "siscale.h"            /* for Mitchell filtering */
#include "sidscale.h"           /* for special case downscale filter */
#include "gscindex.h"           /* included for proper handling of index color spaces
                                and keeping data in source color space */
#include "gxcolor2.h"           /* define of float_color_to_byte_color */
#include "gscspace.h"           /* Needed for checking is space is CIE */
#include "gsicc_cache.h"
#include "gsicc_manage.h"
#include "gsicc.h"
#include "gxdevsop.h"
#include <limits.h>             /* For INT_MAX */

static void
decode_sample_frac_to_float(gx_image_enum *penum, frac sample_value, gs_client_color *cc, int i);

/*
 * Define whether we are using Mitchell filtering or spatial
 * interpolation to implement Interpolate.  (The latter doesn't work yet.)
 */
#define USE_MITCHELL_FILTER

/* ------ Strategy procedure ------ */

/* Check the prototype. */
iclass_proc(gs_image_class_0_interpolate);

/* If we're interpolating, use special logic.
   This function just gets interpolation stucture
   initialized and allocates buffer space if needed */
static irender_proc(image_render_interpolate);
static irender_proc(image_render_interpolate_icc);
static irender_proc(image_render_interpolate_masked);
static irender_proc(image_render_interpolate_masked_hl);
static irender_proc(image_render_interpolate_landscape);
static irender_proc(image_render_interpolate_landscape_icc);
static irender_proc(image_render_interpolate_landscape_masked);
static irender_proc(image_render_interpolate_landscape_masked_hl);

#if 0 // Unused now, but potentially useful in the future
static bool
is_high_level_device(gx_device *dev)
{
    char data[] = "HighLevelDevice";
    dev_param_req_t request;
    gs_c_param_list list;
    int highlevel = 0;
    int code;

    gs_c_param_list_write(&list, dev->memory);
    /* Stuff the data into a structure for passing to the spec_op */
    request.Param = data;
    request.list = &list;
    code = dev_proc(dev, dev_spec_op)(dev, gxdso_get_dev_param, &request, sizeof(dev_param_req_t));
    if (code < 0 && code != gs_error_undefined) {
        gs_c_param_list_release(&list);
        return 0;
    }
    gs_c_param_list_read(&list);
    code = param_read_bool((gs_param_list *)&list,
            "HighLevelDevice",
            &highlevel);
    gs_c_param_list_release(&list);
    if (code < 0)
        return 0;

    return highlevel;
}
#endif

static bool
device_allows_imagemask_interpolation(gx_device *dev)
{
    char data[] = "NoInterpolateImagemasks";
    dev_param_req_t request;
    gs_c_param_list list;
    int nointerpolate = 0;
    int code;

    gs_c_param_list_write(&list, dev->memory);
    /* Stuff the data into a structure for passing to the spec_op */
    request.Param = data;
    request.list = &list;
    code = dev_proc(dev, dev_spec_op)(dev, gxdso_get_dev_param, &request, sizeof(dev_param_req_t));
    if (code < 0 && code != gs_error_undefined) {
        gs_c_param_list_release(&list);
        return 0;
    }
    gs_c_param_list_read(&list);
    code = param_read_bool((gs_param_list *)&list,
            "NoInterpolateImagemasks",
            &nointerpolate);
    gs_c_param_list_release(&list);
    if (code < 0)
        return 0;

    return !nointerpolate;
}

#define DC_IS_NULL(pdc)\
  (gx_dc_is_pure(pdc) && (pdc)->colors.pure == gx_no_color_index)

/* Returns < 0 for error, 0 for pure color, 1 for high level */
static int mask_suitable_for_interpolation(gx_image_enum *penum)
{
    gx_device_color * const pdc1 = penum->icolor1;
    int code;
    int high_level_color = 1;

    if (gx_device_must_halftone(penum->dev)) {
        /* We don't interpolate when going to 1bpp outputs */
        return -1;
    } else if (gx_dc_is_pure(pdc1) && (pdc1)->colors.pure != gx_no_color_index &&
        dev_proc(penum->dev, copy_alpha) != NULL &&
        dev_proc(penum->dev, copy_alpha) != gx_no_copy_alpha) {
        /* We have a 'pure' color, and a valid copy_alpha. We can work with that. */
        high_level_color = 0;
    } else if (dev_proc(penum->dev, copy_alpha_hl_color) == NULL) {
        /* No copy_alpha_hl_color. We're out of luck. */
        return -1;
    } else if ((code = gx_color_load(pdc1, penum->pgs, penum->dev)) < 0) {
        /* Otherwise we'll need to load the color value. If this gives an
         * error, we can't cope. */
        return -1;
    } else if (!gx_dc_is_devn(pdc1)) {
        /* If it's not a devn color, then we're really out of luck. */
        return -1;
    }

    /* Never turn this on for devices that disallow it (primarily
     * high level devices) */
    if (!device_allows_imagemask_interpolation(penum->dev))
        return -1;

    return high_level_color;
}

irender_proc_t
gs_image_class_0_interpolate(gx_image_enum * penum)
{
    gs_memory_t *mem = penum->memory;
    stream_image_scale_params_t iss;
    stream_image_scale_state *pss;
    const stream_template *templat;
    byte *line;
    const gs_color_space *pcs = penum->pcs;
    uint in_size;
    bool use_icc = false;
    int num_des_comps;
    cmm_dev_profile_t *dev_profile;
    int code;
    gx_color_polarity_t pol;
    int mask_col_high_level = 0;

    if (penum->interpolate == interp_off)
        return 0;
    if (penum->masked && (mask_col_high_level = mask_suitable_for_interpolation(penum)) < 0) {
        penum->interpolate = interp_off;
        return 0;
    }
    if (penum->use_mask_color ||
        (penum->posture != image_portrait &&
         penum->posture != image_landscape) ||
        penum->alpha) {
        /* We can't handle these cases yet.  Punt. */
        penum->interpolate = interp_off;
        return 0;
    }
    if (penum->Width == 0 || penum->Height == 0) {
        penum->interpolate = interp_off; /* No need to interpolate and      */
        return 0;                  /* causes division by 0 if we try. */
    }
    if (penum->Width == 1 && penum->Height == 1) {
        penum->interpolate = interp_off; /* No need to interpolate */
        return 0;
    }
    if (any_abs(penum->dst_width) < 0 || any_abs(penum->dst_height) < 0)
    {
        /* A calculation has overflowed. Bale */
        return 0;
    }
    if (penum->x_extent.x == INT_MIN || penum->x_extent.x == INT_MAX ||
        penum->x_extent.y == INT_MIN || penum->x_extent.y == INT_MAX ||
        penum->y_extent.x == INT_MIN || penum->y_extent.x == INT_MAX ||
        penum->y_extent.y == INT_MIN || penum->y_extent.y == INT_MAX)
    {
        /* A calculation has overflowed. Bale */
        return 0;
    }
    if (penum->masked) {
        use_icc = false;
        num_des_comps = 1;
    } else {
        if ( pcs->cmm_icc_profile_data != NULL ) {
            use_icc = true;
        }
        if ( pcs->type->index == gs_color_space_index_Indexed) {
            if ( pcs->base_space->cmm_icc_profile_data != NULL) {
                use_icc = true;
            }
        }
        if (!(penum->bps <= 8 || penum->bps == 16)) {
            use_icc = false;
        }
        /* Do not allow mismatch in devices component output with the
           profile output size.  For example sep device with CMYK profile should
           not go through the fast method */
        code = dev_proc(penum->dev, get_profile)(penum->dev, &dev_profile);
        if (code) {
            penum->interpolate = interp_off;
            return 0;
        }
        num_des_comps = gsicc_get_device_profile_comps(dev_profile);
        if (num_des_comps != penum->dev->color_info.num_components ||
            dev_profile->usefastcolor == true) {

            use_icc = false;
        }
        /* If the device has some unique color mapping procs due to its color space,
           then we will need to use those and go through pixel by pixel instead
           of blasting through buffers.  This is true for example with many of
           the color spaces for CUPs */
        if(!gx_device_uses_std_cmap_procs(penum->dev, penum->pgs)) {
            use_icc = false;
        }
    }
/*
 * USE_CONSERVATIVE_INTERPOLATION_RULES is normally NOT defined since
 * the MITCHELL digital filter seems OK as long as we are going out to
 * a device that can produce > 15 shades.
 */
#if defined(USE_MITCHELL_FILTER) && defined(USE_CONSERVATIVE_INTERPOLATION_RULES)
    /*
     * We interpolate using a digital filter, rather than Adobe's
     * spatial interpolation algorithm: this produces very bad-looking
     * results if the input resolution is close to the output resolution,
     * especially if the input has low color resolution, so we resort to
     * some hack tests on the input color resolution and scale to suppress
     * interpolation if we think the result would look especially bad.
     * If we used Adobe's spatial interpolation approach, we wouldn't need
     * to do this, but the spatial interpolation filter doesn't work yet.
     */
    if (penum->bps < 4 || penum->bps * penum->spp < 8 ||
        (fabs(penum->matrix.xx) <= 5 && fabs(penum->matrix.yy <= 5))
        ) {
        penum->interpolate = interp_off;
        return 0;
    }
#endif
    if (penum->masked) {
        iss.BitsPerComponentOut = 8;
        iss.MaxValueOut = 0xff;
    } else if (use_icc) {
        iss.BitsPerComponentOut = 16;
        iss.MaxValueOut = 0xffff;
    } else {
        iss.BitsPerComponentOut = sizeof(frac) * 8;
        iss.MaxValueOut = frac_1;
    }
    iss.PatchWidthIn = penum->rrect.w;
    iss.PatchHeightIn = penum->rrect.h;
    iss.LeftMarginIn = penum->rrect.x - penum->rect.x;
    iss.TopMargin = penum->rrect.y - penum->rect.y;
    if (penum->posture == image_portrait) {
        fixed dw = any_abs(penum->dst_width);
        fixed dh = any_abs(penum->dst_height);
        iss.WidthOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rect.x + penum->rect.w) *
                                                          dw / penum->Width))
                     - fixed2int_pixround_perfect((fixed)((int64_t)penum->rect.x *
                                                          dw / penum->Width));
        iss.HeightOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rect.y + penum->rect.h) *
                                                           dh / penum->Height))
                      - fixed2int_pixround_perfect((fixed)((int64_t)penum->rect.y *
                                                           dh / penum->Height));
        iss.EntireWidthOut = fixed2int_pixround(dw);
        iss.EntireHeightOut = fixed2int_pixround(dh);
        iss.PatchWidthOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.x + penum->rrect.w) *
                                                               dw / penum->Width))
                          - fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.x *
                                                               dw / penum->Width));
        iss.PatchHeightOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.y + penum->rrect.h) *
                                                                dh / penum->Height))
                           - fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.y *
                                                                dh / penum->Height));
        iss.LeftMarginOut = fixed2int_pixround_perfect((fixed)((int64_t)iss.LeftMarginIn *
                                                               dw / penum->Width));
    } else {
        fixed dw = any_abs(penum->dst_width);
        fixed dh = any_abs(penum->dst_height);
        iss.WidthOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rect.x + penum->rect.w) *
                                                          dh / penum->Width))
                     - fixed2int_pixround_perfect((fixed)((int64_t)penum->rect.x *
                                                          dh / penum->Width));
        iss.HeightOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rect.y + penum->rect.h) *
                                                           dw / penum->Height))
                      - fixed2int_pixround_perfect((fixed)((int64_t)penum->rect.y *
                                                           dw / penum->Height));
        iss.EntireWidthOut = fixed2int_pixround(dh);
        iss.EntireHeightOut = fixed2int_pixround(dw);
        iss.PatchWidthOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.x + penum->rrect.w) *
                                                               dh / penum->Width))
                          - fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.x *
                                                               dh / penum->Width));
        iss.PatchHeightOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.y + penum->rrect.h) *
                                                                dw / penum->Height))
                           - fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.y *
                                                                dw / penum->Height));
        iss.LeftMarginOut = fixed2int_pixround_perfect((fixed)((int64_t)iss.LeftMarginIn *
                                                               dh / penum->Width));
    }
    iss.PatchWidthOut = any_abs(iss.PatchWidthOut);
    iss.PatchHeightOut = any_abs(iss.PatchHeightOut);
    if (iss.LeftMarginOut + iss.PatchWidthOut >= iss.WidthOut) {
        iss.LeftMarginOut = iss.WidthOut - iss.PatchWidthOut;
        if (iss.LeftMarginOut < 0) {
            iss.WidthOut += iss.LeftMarginOut;
            iss.LeftMarginOut = 0;
        }
    }
    iss.src_y_offset = penum->rect.y;
    iss.EntireWidthIn = penum->Width;
    iss.EntireHeightIn = penum->Height;
    iss.WidthIn = penum->rect.w;
    iss.HeightIn = penum->rect.h;
    iss.WidthOut = any_abs(iss.WidthOut);
    iss.HeightOut = any_abs(iss.HeightOut);
    if ((penum->posture == image_portrait ? penum->dst_width : penum->dst_height) < 0)
        iss.LeftMarginOut = iss.WidthOut - iss.LeftMarginOut - iss.PatchWidthOut;
    /* For interpolator cores that don't set Active, have us always active */
    iss.Active = 1;
    if (iss.EntireWidthOut == 0 || iss.EntireHeightOut == 0)
    {
        penum->interpolate = interp_off;
        return 0;
    }
    if (penum->masked) {
        iss.spp_decode = 1;
    } else {
        /* If we are in an indexed space then we need to use the number of components
           in the base space.  Otherwise we use the number of components in the source space */
        if (pcs->type->index == gs_color_space_index_Indexed) {
           /* Use the number of colors in the base space */
            iss.spp_decode = cs_num_components(pcs->base_space);
        } else {
            /* Use the number of colors that exist in the source space
            as this is where we are doing our interpolation */
            iss.spp_decode = cs_num_components(pcs);
        }
    }
    if (iss.HeightOut > iss.EntireHeightIn && use_icc) {
        iss.early_cm = true;
        iss.spp_interp = num_des_comps;
    } else {
        iss.early_cm = false;
        iss.spp_interp = iss.spp_decode;
    }
    if (penum->bps <= 8 ) {
       /* If the input is ICC or other device independent format, go ahead
          and do the interpolation in that space.
          If we have more than 8 bits per channel then we will need to
          handle that in a slightly different manner so
          that the interpolation algorithm handles it properly.
          The interpolation will still be in the source
          color space.  Note that if image data was less the 8 bps
          It is handed here to us in 8 bit form already decoded. */
        iss.BitsPerComponentIn = 8;
        iss.MaxValueIn = 0xff;
        if (penum->masked) {
            in_size = iss.WidthIn * iss.spp_decode;
        } else {
            /* We either use the data as is, or allocate space if it is
               reversed in X or if the colorspace requires it */
            /* Do we need a buffer for reversing each scan line? */
            bool reverse = (penum->posture == image_portrait ?
                            penum->matrix.xx : penum->matrix.xy) < 0;
            in_size = (reverse ? iss.WidthIn * iss.spp_decode : 0);
            /* If it is not reversed, and we have 8 bit/color channel data then
               we may not need to allocate extra as we will use the source directly.
               However, if we have a nonstandard encoding and are in a device
               color space we will need to allocate in that case also. We will
               maintain 8 bits but do the decode and then interpolate.
               This is OK for the linear decode
            */
            if (!penum->device_color ||  pcs->type->index > gs_color_space_index_DeviceCMYK) {
                in_size = iss.WidthIn * iss.spp_decode;
            }
        }
    } else {
        /* If it has more than 8 bits per color channel then we will go to frac
           for the interpolation to mantain precision or 16 bit for icc  */
        if (use_icc) {
            iss.BitsPerComponentIn = 16;
            iss.MaxValueIn = 0xffff;
        } else {
            iss.BitsPerComponentIn = sizeof(frac) * 8;
            iss.MaxValueIn = frac_1;
        }
        in_size = round_up(iss.WidthIn * iss.spp_decode * sizeof(frac),
                           align_bitmap_mod);
        /* Size to allocate space to store the input as frac type */
    }
#ifdef USE_MITCHELL_FILTER
    templat = &s_IScale_template;
#else
    templat = &s_IIEncode_template;
#endif
    if (!pcs)
        pol = GX_CINFO_POLARITY_ADDITIVE;
    else
        pol = cs_polarity(pcs);
    if ((iss.WidthOut < iss.WidthIn) &&
        (iss.HeightOut < iss.HeightIn) &&       /* downsampling */
        (pol != GX_CINFO_POLARITY_UNKNOWN) &&
        (dev_proc(penum->dev, dev_spec_op)(penum->dev, gxdso_interpolate_antidropout, NULL, 0) > 0)) {
        /* Special case handling for when we are downsampling (to a dithered
         * device.  The point of this non-linear downsampling is to preserve
         * dark pixels from the source image to avoid dropout. The color
         * polarity is used for this. */
        templat = &s_ISpecialDownScale_template;
    } else {
        int threshold = dev_proc(penum->dev, dev_spec_op)(penum->dev, gxdso_interpolate_threshold, NULL, 0);
        if ((iss.WidthOut == iss.WidthIn && iss.HeightOut == iss.HeightIn) ||
            ((penum->interpolate != interp_force) &&
             (threshold > 0) &&
             (iss.WidthOut < iss.WidthIn * threshold) &&
             (iss.HeightOut < iss.HeightIn * threshold))) {
            penum->interpolate = interp_off;
            return 0;       /* no interpolation / downsampling */
        }
    }
    /* The SpecialDownScale filter needs polarity, either ADDITIVE or SUBTRACTIVE */
    /* UNKNOWN case (such as for palette colors) has been handled above */
    iss.ColorPolarityAdditive = (pol == GX_CINFO_POLARITY_ADDITIVE);
    /* Allocate a buffer for one source/destination line. */
    {
        uint out_size =
            iss.WidthOut * max(iss.spp_interp * (iss.BitsPerComponentOut / 8),
                               ARCH_SIZEOF_COLOR_INDEX);
        /* Allocate based upon frac size (as BitsPerComponentOut=16) output scan
           line input plus output. The outsize may have an adjustment for
           word boundary on it. Need to account for that now */
        out_size += align_bitmap_mod;
        line = gs_alloc_bytes(mem, in_size + out_size,
                              "image scale src+dst line");
    }
    pss = (stream_image_scale_state *)
        s_alloc_state(mem, templat->stype, "image scale state");
    if (line == 0 || pss == 0 ||
        (pss->params = iss, pss->templat = templat,
         (*pss->templat->init) ((stream_state *) pss) < 0)
        ) {
        gs_free_object(mem, pss, "image scale state");
        gs_free_object(mem, line, "image scale src+dst line");
        /* Try again without interpolation. */
        penum->interpolate = interp_off;
        return 0;
    }
    penum->line = line;  /* Set to the input and output buffer */
    penum->scaler = pss;
    penum->line_xy = 0;
    if (penum->posture == image_portrait) {
        gx_dda_fixed x0;
        x0 = penum->dda.pixel0.x;
        /* We always plot from left to right. If the matrix would have us
         * plotting from right to left, then adjust to allow for the fact
         * we'll flip the data later. */
        if (penum->matrix.xx < 0)
            dda_advance(x0, penum->rect.w);
        penum->xyi.x = fixed2int_pixround(dda_current(x0)) + pss->params.LeftMarginOut;
        penum->xyi.y = penum->yi0 + fixed2int_pixround_perfect(
                                 (fixed)((int64_t)penum->rect.y *
                                         penum->dst_height / penum->Height));
    } else /* penum->posture == image_landscape */ {
        /* We always plot from top to bottom. If the matrix would have us
         * plotting from bottom to top, then adjust to allow for the fact
         * we'll flip the data later. */
        int x0 = penum->rrect.x;
        if (penum->matrix.xy < 0)
            x0 += penum->rrect.w;
        penum->xyi.x = fixed2int_pixround(dda_current(penum->dda.pixel0.x));
        penum->xyi.y = penum->yi0 + fixed2int_pixround_perfect(
                                 (fixed)((int64_t)x0 *
                                         penum->dst_height / penum->Width));
    }
    if_debug0m('b', penum->memory, "[b]render=interpolate\n");
    if (penum->masked) {
        if (!mask_col_high_level) {
            return (penum->posture == image_portrait ?
                    &image_render_interpolate_masked :
                    &image_render_interpolate_landscape_masked);
        } else {
            return (penum->posture == image_portrait ?
                    &image_render_interpolate_masked_hl :
                    &image_render_interpolate_landscape_masked_hl);
        }
    } else if (use_icc) {
        /* Set up the link now */
        const gs_color_space *pcs;
        gsicc_rendering_param_t rendering_params;
        int k;
        int src_num_comp = cs_num_components(penum->pcs);

        penum->icc_setup.need_decode = false;
        /* Check if we need to do any decoding.  If yes, then that will slow us down */
        for (k = 0; k < src_num_comp; k++) {
            if ( penum->map[k].decoding != sd_none ) {
                penum->icc_setup.need_decode = true;
                break;
            }
        }
        /* Define the rendering intents */
        rendering_params.black_point_comp = penum->pgs->blackptcomp;
        rendering_params.graphics_type_tag = GS_IMAGE_TAG;
        rendering_params.override_icc = false;
        rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
        rendering_params.rendering_intent = penum->pgs->renderingintent;
        rendering_params.cmm = gsCMM_DEFAULT;
        if (gs_color_space_is_PSCIE(penum->pcs) && penum->pcs->icc_equivalent != NULL) {
            pcs = penum->pcs->icc_equivalent;
        } else {
            /* Look for indexed space */
            if ( penum->pcs->type->index == gs_color_space_index_Indexed) {
                pcs = penum->pcs->base_space;
            } else {
                pcs = penum->pcs;
            }
        }
        penum->icc_setup.is_lab = pcs->cmm_icc_profile_data->islab;
        if (penum->icc_setup.is_lab) penum->icc_setup.need_decode = false;
        penum->icc_setup.must_halftone = gx_device_must_halftone(penum->dev);
        penum->icc_setup.has_transfer =
            gx_has_transfer(penum->pgs, num_des_comps);
        if (penum->icc_link == NULL) {
            penum->icc_link = gsicc_get_link(penum->pgs, penum->dev, pcs, NULL,
                &rendering_params, penum->memory);
        }
        /* We need to make sure that we do the proper unpacking proc if we
           are doing 16 bit */
        if (penum->bps == 16) {
            penum->unpack = sample_unpackicc_16;
        }
        return (penum->posture == image_portrait ?
                &image_render_interpolate_icc :
                &image_render_interpolate_landscape_icc);
    } else {
        return (penum->posture == image_portrait ?
                &image_render_interpolate :
                &image_render_interpolate_landscape);
    }
}

/* ------ Rendering for interpolated images ------ */

/* This does some initial required decoding of index spaces and general
   decoding of odd scaled image data needed prior to interpolation or
   application of color management. */
static void
initial_decode(gx_image_enum * penum, const byte * buffer, int data_x, int h,
               stream_cursor_read *stream_r, bool is_icc)
{
    stream_image_scale_state *pss = penum->scaler;
    const gs_color_space *pcs = penum->pcs;
    int spp_decode = pss->params.spp_decode;
    byte *out = penum->line;
    bool need_decode;
    int reversed =
        (penum->posture == image_portrait ? penum->matrix.xx : penum->matrix.xy) < 0;
    /* If cs is neither a device color nor a CIE color then for an image case
       it is going to be deviceN, sep, or index. */
    bool is_devn_sep_index =
        (!(penum->device_color) && !gs_color_space_is_CIE(pcs));

    /* Determine if we need to perform any decode procedures */
    if (is_icc) {
        /* In icc case, decode upfront occurs if specified by icc setup or if
           cs is neither a device color nor a CIE color (i.e. if it's DeviceN,
           Index or Separation) The color space cannot be a pattern for an image */
        need_decode = (penum->icc_setup.need_decode || is_devn_sep_index);
    } else
        need_decode = is_devn_sep_index;

    if (h != 0) {
        /* Convert the unpacked data to concrete values in the source buffer. */
        int sizeofPixelIn = pss->params.BitsPerComponentIn / 8;
        uint row_size = pss->params.WidthIn * spp_decode * sizeofPixelIn;
        /* raw input data */
        const int raw_size = (pcs != NULL && pcs->type->index == gs_color_space_index_Indexed ?
                              1 : spp_decode);
        const unsigned char *bdata = buffer + data_x * raw_size * sizeofPixelIn;
        /* We have the following cases to worry about
          1) Device 8 bit color but not indexed (e.g. ICC).
             Apply CMM after interpolation if needed.
             Also if ICC CIELAB do not do a decode operation
          2) Indexed 8 bit color.  Get to the base space. We will then be in
             the same state as 1.
          3) 16 bit not indexed.  Remap after interpolation.
          4) Indexed 16bit color.   Get to base space in 16bit form. We
             will then be in same state as 3.
         */
        if (sizeofPixelIn == 1) {
            if (pcs == NULL || pcs->type->index != gs_color_space_index_Indexed) {
                /* An issue here is that we may not be "device color" due to
                   how the data is encoded.  Need to check for that case here */
                /* Decide here if we need to decode or not. Essentially, as
                 * far as I can gather, we use the top case if we DON'T need
                 * to decode. This is fairly obviously conditional on
                 * need_decode being set to 0. The major exception to this is
                 * that if the colorspace is CIE, we interpolate, THEN decode,
                 * so the decode is done later in the pipeline, so we needn't
                 * decode here (see Bugs 692225 and 692331). */
                if (!need_decode) {
                    /* 8-bit color values, possibly device  indep. or device
                       depend., not indexed. Decode range was [0 1] */
                    if (!reversed) {
                        /* Use the input data directly. sets up data in the
                           stream buffer structure */
                        stream_r->ptr = bdata - 1;
                    } else {
                        /* Mirror the data in X. */
                        const byte *p = bdata + row_size - spp_decode;
                        byte *q = out;
                        int i;

                        for (i = 0; i < pss->params.WidthIn;
                            p -= spp_decode, q += spp_decode, ++i)
                            memcpy(q, p, spp_decode);
                        stream_r->ptr = out - 1;
                        out += round_up(pss->params.WidthIn *
                                        spp_decode, align_bitmap_mod);
                    }
                } else {
                    /* We need to do some decoding. Data will remain in 8 bits
                       This does not occur if color space was CIE encoded.
                       Then we do the decode during concretization which occurs
                       after interpolation */
                    int dc = penum->spp;
                    const byte *pdata = bdata;
                    byte *psrc = (byte *) penum->line;
                    int i, j;
                    int dpd = dc;
                    gs_client_color cc;

                    /* Go backwards through the data */
                    if (reversed) {
                        pdata += (pss->params.WidthIn - 1) * dpd;
                        dpd = - dpd;
                    }
                    stream_r->ptr = (byte *) psrc - 1;
                    for (i = 0; i < pss->params.WidthIn; i++, psrc += spp_decode) {
                        /* Do the decode but remain in 8 bits */
                        for (j = 0; j < dc;  ++j) {
                            decode_sample(pdata[j], cc, j);
                            psrc[j] = float_color_to_byte_color(cc.paint.values[j]);
                        }
                        pdata += dpd;
                    }
                    out += round_up(pss->params.WidthIn * spp_decode,
                                    align_bitmap_mod);
                }
            } else {
                /* indexed 8 bit color values, possibly a device indep. or
                   device depend. base space. We need to get out of the indexed
                   space and into the base color space. Note that we need to
                   worry about the decode function for the index values. */
                int bps = penum->bps;
                int dc = penum->spp;
                const byte *pdata = bdata; /* Input buffer */
                unsigned char *psrc = (unsigned char *) penum->line;  /* Output */
                int i;
                int dpd = dc * (bps <= 8 ? 1 : sizeof(frac));
                float max_range;

                /* Get max of decode range */
                max_range = (penum->map[0].decode_factor < 0 ?
                    penum->map[0].decode_base :
                penum->map[0].decode_base + 255.0 * penum->map[0].decode_factor);
                /* flip the horizontal direction if indicated by the matrix value */
                if (reversed) {
                    pdata += (pss->params.WidthIn - 1) * dpd;
                    dpd = - dpd;
                }
                stream_r->ptr = (byte *) psrc - 1;

                for (i = 0; i < pss->params.WidthIn; i++, psrc += spp_decode) {
                    /* Let's get directly to a decoded byte type loaded into
                       psrc, and do the interpolation in the source space. Then
                       we will do the appropriate remap function after
                       interpolation. */
                    /* First we need to get the properly decoded value. */
                    float decode_value;
                    switch ( penum->map[0].decoding )
                    {
                        case sd_none:
                         /* while our indexin is going to be 0 to 255.0 due to
                            what is getting handed to us, the range of our
                            original data may not have been as such and we may
                            need to rescale, to properly lookup at the correct
                            location (or do the proc correctly) during the index
                            look-up.  This occurs even if decoding was set to
                            sd_none.  */
                            decode_value = (float) pdata[0] * (float)max_range / 255.0;
                            break;
                        case sd_lookup:
                            decode_value =
                              (float) penum->map[0].decode_lookup[pdata[0] >> 4];
                            break;
                        case sd_compute:
                            decode_value =
                              penum->map[0].decode_base +
                              ((float) pdata[0]) * penum->map[0].decode_factor;
                            break;
                        default:
                            decode_value = 0; /* Quiet gcc warning. */
                    }
                    gs_cspace_indexed_lookup_bytes(pcs, decode_value,psrc);
                    pdata += dpd;    /* Can't have just ++
                                        since we could be going backwards */
                }
                /* We need to set the output to the end of the input buffer
                   moving it to the next desired word boundary.  This must
                   be accounted for in the memory allocation of
                   gs_image_class_0_interpolate */
                out += round_up(pss->params.WidthIn * spp_decode,
                                align_bitmap_mod);
            }
        } else {
            /* More than 8-bits/color values */
            /* Even in this case we need to worry about an indexed color space.
               We need to get to the base color space for the interpolation and
               then if necessary do the remap to the device space */
            if (pcs->type->index != gs_color_space_index_Indexed) {
                int bps = penum->bps;
                int dc = penum->spp;
                const byte *pdata = bdata;
                frac *psrc = (frac *) penum->line;
                int i, j;
                int dpd = dc * (bps <= 8 ? 1 : sizeof(frac));

                if (reversed) {
                    pdata += (pss->params.WidthIn - 1) * dpd;
                    dpd = - dpd;
                }
                stream_r->ptr = (byte *) psrc - 1;
                if_debug0m('B', penum->memory, "[B]Remap row:\n[B]");
                if (is_icc) {
                    if (reversed) {
                        byte *to = penum->line;
                        for (i = 0; i < pss->params.WidthIn; i++) {
                            memcpy(to, pdata, -dpd);
                            to -= dpd;
                            pdata += dpd;
                        }
                    } else {
                        stream_r->ptr = (byte *) pdata - 1;
                    }
                } else {
                    if (sizeof(frac) * dc == dpd) {
                        stream_r->ptr = (byte *) pdata - 1;
                    } else {
                        for (i = 0; i < pss->params.WidthIn; i++,
                             psrc += spp_decode) {
                            for (j = 0; j < dc; ++j) {
                                psrc[j] = ((const frac *)pdata)[j];
                            }
                            pdata += dpd;
    #ifdef DEBUG
                            if (gs_debug_c('B')) {
                                int ci;

                                for (ci = 0; ci < spp_decode; ++ci)
                                    dmprintf2(penum->memory, "%c%04x", (ci == 0 ? ' ' : ','), psrc[ci]);
                            }
    #endif
                        }
                    }
                }
                out += round_up(pss->params.WidthIn * spp_decode * sizeof(frac),
                                align_bitmap_mod);
                if_debug0m('B', penum->memory, "\n");
            } else {
                /* indexed and more than 8bps.  Need to get to the base space */
                int bps = penum->bps;
                int dc = penum->spp;
                const byte *pdata = bdata; /* Input buffer */
                frac *psrc = (frac *) penum->line;    /* Output buffer */
                int i;
                int dpd = dc * (bps <= 8 ? 1 : sizeof(frac));
                float decode_value;

                /* flip the horizontal direction if indicated by the matrix value */
                if (reversed) {
                    pdata += (pss->params.WidthIn - 1) * dpd;
                    dpd = - dpd;
                }
                stream_r->ptr = (byte *) psrc - 1;
                for (i = 0; i < pss->params.WidthIn; i++, psrc += spp_decode) {
                    /* Lets get the decoded value. Then we need to do the lookup
                       of this */
                    decode_value = penum->map[i].decode_base +
                        (((const frac *)pdata)[0]) * penum->map[i].decode_factor;
                    /* Now we need to do the lookup of this value, and stick it
                       in psrc as a frac, which is what the interpolator is
                       expecting, since we had more than 8 bits of original
                       image data */
                    gs_cspace_indexed_lookup_frac(pcs, decode_value,psrc);
                    pdata += dpd;
                }
                /* We need to set the output to the end of the input buffer
                   moving it to the next desired word boundary.  This must
                   be accounted for in the memory allocation of
                   gs_image_class_0_interpolate */
                out += round_up(pss->params.WidthIn * spp_decode,
                                align_bitmap_mod);
            } /* end of else on indexed */
        }  /* end of else on more than 8 bps */
        stream_r->limit = stream_r->ptr + row_size;
    } else {                    /* h == 0 */
        stream_r->ptr = 0, stream_r->limit = 0;
    }
}

static int handle_colors(gx_image_enum *penum, const frac *psrc, int spp_decode,
    gx_device_color *devc, bool islab, gx_device *dev)
{
    const gs_color_space *pactual_cs;
    const gs_color_space *pconcs;
    const gs_gstate *pgs = penum->pgs;
    const gs_color_space *pcs = penum->pcs;
    bool device_color;
    bool is_index_space;
    int code = 0;
    cmm_dev_profile_t *dev_profile;

    if (pcs == NULL)
        return 0; /* Must be masked */

    is_index_space = (pcs->type->index == gs_color_space_index_Indexed);
#ifdef DEBUG
    if (gs_debug_c('B')) {
        int ci;

        for (ci = 0; ci < spp_decode; ++ci)
            dmprintf2(dev->memory, "%c%04x", (ci == 0 ? ' ' : ','),
            psrc[ci]);
    }
#endif
    /* If we are in a non device space then work from the pcs not from the
    concrete space also handle index case, where base case was device type */
    if (pcs->type->index == gs_color_space_index_Indexed) {
        pactual_cs = pcs->base_space;
    } else {
        pactual_cs = pcs;
    }
    code = dev_proc(penum->dev, get_profile)(penum->dev, &dev_profile);
    /* ignore code since error from get_profile would have prevented interpolation */
    pconcs = cs_concrete_space(pactual_cs, pgs);
    if (pconcs && pconcs->cmm_icc_profile_data != NULL) {
        if (pconcs->cmm_icc_profile_data != NULL && dev_profile->usefastcolor == false) {
            device_color = false;
        } else {
            device_color = (pconcs == pactual_cs);
        }
    }
    if (device_color) {
        /* Use the underlying concrete space remap */
        code = (*pconcs->type->remap_concrete_color)
            (psrc, pconcs, devc, pgs, dev, gs_color_select_source);
    } else {
        /* If we are device dependent we need to get back to float prior to remap.*/
        gs_client_color cc;
        int j;
        int num_components = gs_color_space_num_components(pactual_cs);

        if (islab) {
            /* LAB colors are normally decoded with a decode array
             * of [0 100  -128 127   -128 127 ]. The color management
             * however, expects this decode array NOT to have been
             * applied.
             *
             * It would be possible for an LAB image to be given a
             * non-standard decode array, in which case, we should
             * take account of that. The easiest way is to apply the
             * decode array as given, and then 'undo' the standard
             * one.
             */
            decode_sample_frac_to_float(penum, psrc[0], &cc, 0);
            decode_sample_frac_to_float(penum, psrc[1], &cc, 1);
            decode_sample_frac_to_float(penum, psrc[2], &cc, 2);
            if (penum->bps <= 8) {
                cc.paint.values[0] /= 100.0;
                cc.paint.values[1] = (cc.paint.values[1] + 128) / 255.0;
                cc.paint.values[2] = (cc.paint.values[2] + 128) / 255.0;
            } else {
                cc.paint.values[0] *= 0x7ff8 / 25500.0f;
                cc.paint.values[1] = (cc.paint.values[1] + 128) * 0x7ff8 / 65025.0;
                cc.paint.values[2] = (cc.paint.values[2] + 128) * 0x7ff8 / 65025.0;
            }
        } else {
            for (j = 0; j < num_components; ++j) {
                /* If we were indexed, dont use the decode procedure for the index
                   values just get to float directly */
                if (is_index_space) {
                    cc.paint.values[j] = frac2float(psrc[j]);
                } else {
                    decode_sample_frac_to_float(penum, psrc[j], &cc, j);
                }
            }
        }
        /* If the source colors are LAB then use the mapping that does not
        rescale the source colors */
        if (gs_color_space_is_ICC(pactual_cs) &&
            pactual_cs->cmm_icc_profile_data != NULL &&
            pactual_cs->cmm_icc_profile_data->islab) {
            code = gx_remap_ICC_imagelab(&cc, pactual_cs, devc, pgs, dev,
                gs_color_select_source);
        } else {
            code = (pactual_cs->type->remap_color)
                (&cc, pactual_cs, devc, pgs, dev, gs_color_select_source);
        }
    }
    return code;
}

static int
image_render_interpolate(gx_image_enum * penum, const byte * buffer,
                         int data_x, uint iw, int h, gx_device * dev)
{
    stream_image_scale_state *pss = penum->scaler;
    const gs_color_space *pcs = penum->pcs;
    gs_logical_operation_t lop = penum->log_op;
    int spp_decode = pss->params.spp_decode;
    stream_cursor_read stream_r;
    stream_cursor_write stream_w;
    byte *out = penum->line;
    bool islab = false;

    if (!penum->masked && pcs->cmm_icc_profile_data != NULL) {
        islab = pcs->cmm_icc_profile_data->islab;
    }
    /* Perform any decode procedure if needed */
    initial_decode(penum, buffer, data_x, h, &stream_r, false);
    /*
     * Process input and/or collect output.  By construction, the pixels are
     * 1-for-1 with the device, but the Y coordinate might be inverted.
     */
    {
        int xo = penum->xyi.x;
        int yo = penum->xyi.y;
        int width = pss->params.WidthOut;
        int sizeofPixelOut = pss->params.BitsPerComponentOut / 8;
        int dy;
        int bpp = dev->color_info.depth;
        uint raster = bitmap_raster(width * bpp);

        if (penum->matrix.yy > 0)
            dy = 1;
        else
            dy = -1, yo--;
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            int x;
            const frac *psrc;
            gx_device_color devc;
            int status, code;

            byte *l_dptr = out;
            int l_dbit = 0;
            byte l_dbyte = ((l_dbit) ? (byte)(*(l_dptr) & (0xff00 >> (l_dbit))) : 0);
            int l_xprev = (xo);
            stream_w.limit = out + width *
                max(spp_decode * sizeofPixelOut, ARCH_SIZEOF_COLOR_INDEX) - 1;
            stream_w.ptr = stream_w.limit - width * spp_decode * sizeofPixelOut;
            psrc = (const frac *)(stream_w.ptr + 1);
            /* This is where the rescale takes place; this will consume the
             * data from stream_r, and post processed data into stream_w. The
             * data in stream_w may be bogus if we are outside the active
             * region, and this will be indicated by pss->params.Active being
             * set to false. */
            status = (*pss->templat->process)
                ((stream_state *) pss, &stream_r, &stream_w, h == 0);
            if (status < 0 && status != EOFC)
                return_error(gs_error_ioerror);
            if (stream_w.ptr == stream_w.limit) {
                int xe = xo + pss->params.PatchWidthOut;

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated row %d:\n[B]",
                           penum->line_xy);
                psrc += pss->params.LeftMarginOut * spp_decode;
                for (x = xo; x < xe;) {
                    code = handle_colors(penum, psrc, spp_decode, &devc, islab, dev);
                    if (code < 0)
                        return code;
                    if (color_is_pure(&devc)) {
                        /* Just pack colors into a scan line. */
                        gx_color_index color = devc.colors.pure;
                        /* Skip runs quickly for the common cases. */
                        switch (spp_decode) {
                            case 1:
                                do {
                                    if (sizeof(color) > 4) {
                                        if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    else {
                                        if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    x++, psrc += 1;
                                } while (x < xe && psrc[-1] == psrc[0]);
                                break;
                            case 3:
                                do {
                                    if (sizeof(color) > 4) {
                                        if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    else {
                                        if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    x++, psrc += 3;
                                } while (x < xe &&
                                         psrc[-3] == psrc[0] &&
                                         psrc[-2] == psrc[1] &&
                                         psrc[-1] == psrc[2]);
                                break;
                            case 4:
                                do {
                                    if (sizeof(color) > 4) {
                                        if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    else {
                                        if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    x++, psrc += 4;
                                } while (x < xe &&
                                         psrc[-4] == psrc[0] &&
                                         psrc[-3] == psrc[1] &&
                                         psrc[-2] == psrc[2] &&
                                         psrc[-1] == psrc[3]);
                                break;
                            default:
                                if (sizeof(color) > 4) {
                                    if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                        return_error(gs_error_rangecheck);
                                }
                                else {
                                    if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                        return_error(gs_error_rangecheck);
                                }
                                x++, psrc += spp_decode;
                        }
                    } else {
                        int rcode, i, rep = 0;

                        /* do _COPY in case any pure colors were accumulated above */
                        if ( x > l_xprev ) {
                            sample_store_flush(l_dptr, l_dbit, l_dbyte);
                            code = (*dev_proc(dev, copy_color))
                              (dev, out, l_xprev - xo, raster,
                               gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                            if (code < 0)
                                return code;
                        }
                        /* as above, see if we can accumulate any runs */
                        switch (spp_decode) {
                            case 1:
                                do {
                                    rep++, psrc += 1;
                                } while ((rep + x) < xe &&
                                         psrc[-1] == psrc[0]);
                                break;
                            case 3:
                                do {
                                    rep++, psrc += 3;
                                } while ((rep + x) < xe &&
                                         psrc[-3] == psrc[0] &&
                                         psrc[-2] == psrc[1] &&
                                         psrc[-1] == psrc[2]);
                                break;
                            case 4:
                                do {
                                    rep++, psrc += 4;
                                } while ((rep + x) < xe &&
                                         psrc[-4] == psrc[0] &&
                                         psrc[-3] == psrc[1] &&
                                         psrc[-2] == psrc[2] &&
                                         psrc[-1] == psrc[3]);
                                break;
                            default:
                                rep = 1;
                                psrc += spp_decode;
                                break;
                        }
                        rcode = gx_fill_rectangle_device_rop(x, ry, rep, 1, &devc, dev, lop);
                        if (rcode < 0)
                            return rcode;
                        for (i = 0; i < rep; i++) {
                            sample_store_skip_next(&l_dptr, &l_dbit, bpp, &l_dbyte);
                        }
                        l_xprev = x + rep;
                        x += rep;
                    }
                }
                if ( x > l_xprev ) {
                    sample_store_flush(l_dptr, l_dbit, l_dbyte);
                    code = (*dev_proc(dev, copy_color))
                      (dev, out, l_xprev - xo, raster,
                       gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                    if (code < 0)
                        return code;
                }
                /*if_debug1m('w', dev->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
inactive:
                penum->line_xy++;
                if_debug0m('B', dev->memory, "\n");
            }
            if ((status == 0 && stream_r.ptr == stream_r.limit) || status == EOFC)
                break;
        }
    }
    return (h == 0 ? 0 : 1);
}

static int
image_render_interpolate_masked(gx_image_enum * penum, const byte * buffer,
                                int data_x, uint iw, int h, gx_device * dev)
{
    stream_image_scale_state *pss = penum->scaler;
    stream_cursor_read stream_r;
    stream_cursor_write stream_w;
    byte *out = penum->line;
    gx_color_index color = penum->icolor1->colors.pure;

    /* Perform any decode procedure if needed */
    initial_decode(penum, buffer, data_x, h, &stream_r, false);
    /*
     * Process input and/or collect output.  By construction, the pixels are
     * 1-for-1 with the device, but the Y coordinate might be inverted.
     */
    {
        int xo = penum->xyi.x;
        int yo = penum->xyi.y;
        int width = pss->params.WidthOut;
        int dy;
        int bpp = dev->color_info.depth;
        uint raster = bitmap_raster(width * bpp);

        if (penum->matrix.yy > 0)
            dy = 1;
        else
            dy = -1, yo--;
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            const byte *psrc;
            int status, code;

            stream_w.limit = out + width - 1;
            stream_w.ptr = stream_w.limit - width;
            psrc = stream_w.ptr + 1;
            /* This is where the rescale takes place; this will consume the
             * data from stream_r, and post processed data into stream_w. The
             * data in stream_w may be bogus if we are outside the active
             * region, and this will be indicated by pss->params.Active being
             * set to false. */
            status = (*pss->templat->process)
                ((stream_state *) pss, &stream_r, &stream_w, h == 0);
            if (status < 0 && status != EOFC)
                return_error(gs_error_ioerror);
            if (stream_w.ptr == stream_w.limit) {
                int xe = xo + pss->params.PatchWidthOut;

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated mask row %d:\n[B]",
                           penum->line_xy);
                psrc += pss->params.LeftMarginOut;
                code = (*dev_proc(dev, copy_alpha))
                            (dev, psrc, 0, raster,
                             gx_no_bitmap_id, xo, ry, xe-xo, 1,
                             color, 8);
                if ( code < 0 )
                    return code;
inactive:
                penum->line_xy++;
                if_debug0m('B', dev->memory, "\n");
            }
            if ((status == 0 && stream_r.ptr == stream_r.limit) || status == EOFC)
                break;
        }
    }
    return (h == 0 ? 0 : 1);
}

static int
image_render_interpolate_masked_hl(gx_image_enum * penum, const byte * buffer,
                                   int data_x, uint iw, int h, gx_device * dev)
{
    stream_image_scale_state *pss = penum->scaler;
    stream_cursor_read stream_r;
    stream_cursor_write stream_w;
    byte *out = penum->line;

    /* Perform any decode procedure if needed */
    initial_decode(penum, buffer, data_x, h, &stream_r, false);
    /*
     * Process input and/or collect output.  By construction, the pixels are
     * 1-for-1 with the device, but the Y coordinate might be inverted.
     */
    {
        int xo = penum->xyi.x;
        int yo = penum->xyi.y;
        int width = pss->params.WidthOut;
        int dy;
        int bpp = dev->color_info.depth;
        uint raster = bitmap_raster(width * bpp);

        if (penum->matrix.yy > 0)
            dy = 1;
        else
            dy = -1, yo--;
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            const byte *psrc;
            int status, code;

            stream_w.limit = out + width - 1;
            stream_w.ptr = stream_w.limit - width;
            psrc = stream_w.ptr + 1;
            /* This is where the rescale takes place; this will consume the
             * data from stream_r, and post processed data into stream_w. The
             * data in stream_w may be bogus if we are outside the active
             * region, and this will be indicated by pss->params.Active being
             * set to false. */
            status = (*pss->templat->process)
                ((stream_state *) pss, &stream_r, &stream_w, h == 0);
            if (status < 0 && status != EOFC)
                return_error(gs_error_ioerror);
            if (stream_w.ptr == stream_w.limit) {
                int xe = xo + pss->params.PatchWidthOut;

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated mask row %d:\n[B]",
                           penum->line_xy);
                psrc += pss->params.LeftMarginOut;
                code = (*dev_proc(dev, copy_alpha_hl_color))
                            (dev, psrc, 0, raster,
                             gx_no_bitmap_id, xo, ry, xe-xo, 1,
                             penum->icolor1, 8);
                if ( code < 0 )
                    return code;
inactive:
                penum->line_xy++;
                if_debug0m('B', dev->memory, "\n");
            }
            if ((status == 0 && stream_r.ptr == stream_r.limit) || status == EOFC)
                break;
        }
    }
    return (h == 0 ? 0 : 1);
}

static void
get_device_color(gx_image_enum * penum, unsigned short *p_cm_interp,
gx_device_color *devc, gx_color_index *color, gx_device * dev)
{
    bool must_halftone = penum->icc_setup.must_halftone;
    bool has_transfer = penum->icc_setup.has_transfer;

    if (must_halftone || has_transfer) {
        /* We need to do the tranfer function and/or the halftoning */
        cmap_transfer_halftone(p_cm_interp, devc, penum->pgs, dev,
            has_transfer, must_halftone, gs_color_select_source);
    } else {
        /* encode as a color index. avoid all the cv to frac to cv conversions */
        *color = dev_proc(dev, encode_color)(dev, p_cm_interp);
        /* check if the encoding was successful; we presume failure is rare */
        if (*color != gx_no_color_index)
            color_set_pure(devc, *color);
    }
}

/* Interpolation with ICC based source spaces. This is done seperately to
   enable optimization and avoid the multiple tranformations that occur in
   the above code */
static int
image_render_interpolate_icc(gx_image_enum * penum, const byte * buffer,
                         int data_x, uint iw, int h, gx_device * dev)
{
    stream_image_scale_state *pss = penum->scaler;
    const gs_gstate *pgs = penum->pgs;
    gs_logical_operation_t lop = penum->log_op;
    byte *out = penum->line;
    stream_cursor_read stream_r;
    stream_cursor_write stream_w;

    if (penum->icc_link == NULL) {
        return gs_rethrow(-1, "ICC Link not created during gs_image_class_0_interpolate");
    }
    initial_decode(penum, buffer, data_x, h, &stream_r, true);
    /*
     * Process input and/or collect output.  By construction, the pixels are
     * 1-for-1 with the device, but the Y coordinate might be inverted.
     * CM is performed on the entire row.
     */
    {
        int xo = penum->xyi.x;
        int yo = penum->xyi.y;
        int width = pss->params.WidthOut;
        int width_in = pss->params.WidthIn;
        int sizeofPixelOut = pss->params.BitsPerComponentOut / 8;
        int dy;
        int bpp = dev->color_info.depth;
        uint raster = bitmap_raster(width * bpp);
        unsigned short *p_cm_interp;
        byte *p_cm_buff = NULL;
        byte *psrc;
        int spp_decode = pss->params.spp_decode;
        int spp_interp = pss->params.spp_interp;
        int spp_cm;
        gsicc_bufferdesc_t input_buff_desc;
        gsicc_bufferdesc_t output_buff_desc;
        gx_color_index color;
        int code;
        cmm_dev_profile_t *dev_profile;
        int num_bytes_decode = pss->params.BitsPerComponentIn / 8;

        code = dev_proc(dev, get_profile)(dev, &dev_profile);
        if (code)
            return code;
        spp_cm = gsicc_get_device_profile_comps(dev_profile);
        if (penum->matrix.yy > 0)
            dy = 1;
        else
            dy = -1, yo--;
        /* If it makes sense (if enlarging), do early CM */
        if (pss->params.early_cm && !penum->icc_link->is_identity
            && stream_r.ptr != stream_r.limit) {
            /* Get the buffers set up. */
            p_cm_buff =
                (byte *) gs_alloc_bytes(pgs->memory,
                                        num_bytes_decode * width_in * spp_cm,
                                        "image_render_interpolate_icc");
            /* Set up the buffer descriptors. We keep the bytes the same */
            gsicc_init_buffer(&input_buff_desc, spp_decode, num_bytes_decode,
                          false, false, false, 0, width_in * spp_decode,
                          1, width_in);
            gsicc_init_buffer(&output_buff_desc, spp_cm, num_bytes_decode,
                          false, false, false, 0, width_in * spp_cm,
                          1, width_in);
            /* Do the transformation */
            psrc = (byte*) (stream_r.ptr + 1);
            (penum->icc_link->procs.map_buffer)(dev, penum->icc_link, &input_buff_desc,
                                                &output_buff_desc, (void*) psrc,
                                                (void*) p_cm_buff);
            /* Re-set the reading stream to use the cm data */
            stream_r.ptr = p_cm_buff - 1;
            stream_r.limit = stream_r.ptr + num_bytes_decode * width_in * spp_cm;
        } else {
            /* CM after interpolation (or none).  Just set up the buffers
               if needed.  16 bit operations if CM takes place.  */
            if (!penum->icc_link->is_identity) {
                p_cm_buff = (byte *) gs_alloc_bytes(pgs->memory,
                    sizeof(unsigned short) * width * spp_cm,
                    "image_render_interpolate_icc");
                /* Set up the buffer descriptors. */
                gsicc_init_buffer(&input_buff_desc, spp_decode, 2,
                              false, false, false, 0, width * spp_decode,
                              1, pss->params.PatchWidthOut);
                gsicc_init_buffer(&output_buff_desc, spp_cm, 2,
                              false, false, false, 0, width * spp_cm,
                              1, pss->params.PatchWidthOut);
            }
        }
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            int x;
            const unsigned short *pinterp;
            gx_device_color devc;
            int status;

            byte *l_dptr = out;
            int l_dbit = 0;
            byte l_dbyte = ((l_dbit) ? (byte)(*(l_dptr) & (0xff00 >> (l_dbit))) : 0);
            int l_xprev = (xo);
            stream_w.limit = out + width *
                max(spp_interp * sizeofPixelOut, ARCH_SIZEOF_COLOR_INDEX) - 1;
            stream_w.ptr = stream_w.limit - width * spp_interp * sizeofPixelOut;
            pinterp = (const unsigned short *)(stream_w.ptr + 1);
            /* This is where the rescale takes place; this will consume the
             * data from stream_r, and post processed data into stream_w. The
             * data in stream_w may be bogus if we are outside the active
             * region, and this will be indiated by pss->params.Active being
             * set to false. */
            status = (*pss->templat->process)
                ((stream_state *) pss, &stream_r, &stream_w, h == 0);
            if (status < 0 && status != EOFC)
                return_error(gs_error_ioerror);
            if (stream_w.ptr == stream_w.limit) {
                int xe = xo + pss->params.PatchWidthOut;

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated row %d:\n[B]",
                           penum->line_xy);
                /* Take care of CM on the entire interpolated row, if we
                   did not already do CM */
                if (penum->icc_link->is_identity || pss->params.early_cm) {
                    /* Fastest case. No CM needed */
                    p_cm_interp = (unsigned short *) pinterp;
                    p_cm_interp += pss->params.LeftMarginOut * spp_cm;
                } else {
                    /* Transform */
                    pinterp += pss->params.LeftMarginOut * spp_decode;
                    p_cm_interp = (unsigned short *) p_cm_buff;
                    p_cm_interp += pss->params.LeftMarginOut * spp_cm;
                    (penum->icc_link->procs.map_buffer)(dev, penum->icc_link,
                                                        &input_buff_desc,
                                                        &output_buff_desc,
                                                        (void*) pinterp,
                                                        (void*) p_cm_interp);
                }
                for (x = xo; x < xe;) {
#ifdef DEBUG
                    if (gs_debug_c('B')) {
                        int ci;

                        for (ci = 0; ci < spp_cm; ++ci)
                            dmprintf2(dev->memory, "%c%04x", (ci == 0 ? ' ' : ','),
                                     p_cm_interp[ci]);
                    }
#endif
                    /* Get the device color */
                    get_device_color(penum, p_cm_interp, &devc, &color, dev);
                    if (color_is_pure(&devc)) {
                        /* Just pack colors into a scan line. */
                        gx_color_index color = devc.colors.pure;
                        /* Skip runs quickly for the common cases. */
                        switch (spp_cm) {
                            case 1:
                                do {
                                    if (sizeof(color) > 4) {
                                        if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    else {
                                        if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    x++, p_cm_interp += 1;
                                } while (x < xe && p_cm_interp[-1] == p_cm_interp[0]);
                                break;
                            case 3:
                                do {
                                    if (sizeof(color) > 4) {
                                        if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    else {
                                        if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    x++, p_cm_interp += 3;
                                } while (x < xe && p_cm_interp[-3] == p_cm_interp[0] &&
                                     p_cm_interp[-2] == p_cm_interp[1] &&
                                     p_cm_interp[-1] == p_cm_interp[2]);
                                break;
                            case 4:
                                do {
                                    if (sizeof(color) > 4) {
                                        if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    else {
                                        if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    x++, p_cm_interp += 4;
                                } while (x < xe && p_cm_interp[-4] == p_cm_interp[0] &&
                                     p_cm_interp[-3] == p_cm_interp[1] &&
                                     p_cm_interp[-2] == p_cm_interp[2] &&
                                     p_cm_interp[-1] == p_cm_interp[3]);
                                break;
                            default:
                                if (sizeof(color) > 4) {
                                    if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                        return_error(gs_error_rangecheck);
                                }
                                else {
                                    if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                        return_error(gs_error_rangecheck);
                                }
                                x++, p_cm_interp += spp_cm;
                        }
                    } else {
                        int rcode, i, rep = 0;

                        /* do _COPY in case any pure colors were accumulated above*/
                        if ( x > l_xprev ) {
                            sample_store_flush(l_dptr, l_dbit, l_dbyte);
                            code = (*dev_proc(dev, copy_color))
                              (dev, out, l_xprev - xo, raster,
                               gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                            if (code < 0)
                                return code;
                        }
                        /* as above, see if we can accumulate any runs */
                        switch (spp_cm) {
                            case 1:
                                do {
                                    rep++, p_cm_interp += 1;
                                } while ((rep + x) < xe && p_cm_interp[-1] == p_cm_interp[0]);
                                break;
                            case 3:
                                do {
                                    rep++, p_cm_interp += 3;
                                } while ((rep + x) < xe && p_cm_interp[-3] == p_cm_interp[0] &&
                                     p_cm_interp[-2] == p_cm_interp[1] &&
                                     p_cm_interp[-1] == p_cm_interp[2]);
                                break;
                            case 4:
                                do {
                                    rep++, p_cm_interp += 4;
                                } while ((rep + x) < xe && p_cm_interp[-4] == p_cm_interp[0] &&
                                     p_cm_interp[-3] == p_cm_interp[1] &&
                                     p_cm_interp[-2] == p_cm_interp[2] &&
                                     p_cm_interp[-1] == p_cm_interp[3]);
                                break;
                            default:
                                rep = 1, p_cm_interp += spp_cm;
                                break;
                        }
                        rcode = gx_fill_rectangle_device_rop(x, ry, rep, 1, &devc, dev, lop);
                        if (rcode < 0)
                            return rcode;
                        for (i = 0; i < rep; i++) {
                            sample_store_skip_next(&l_dptr, &l_dbit, bpp, &l_dbyte);
                        }
                        l_xprev = x + rep;
                        x += rep;
                    }
                }  /* End on x loop */
                if ( x > l_xprev ) {
                    sample_store_flush(l_dptr, l_dbit, l_dbyte);
                    code = (*dev_proc(dev, copy_color))
                      (dev, out, l_xprev - xo, raster,
                       gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                    if (code < 0)
                        return code;
                }
                /*if_debug1m('w', penum->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
inactive:
                penum->line_xy++;
                if_debug0m('B', penum->memory, "\n");
            }
            if ((status == 0 && stream_r.ptr == stream_r.limit) || status == EOFC)
                break;
        }
        /* Free cm buffer, if it was used */
        if (p_cm_buff != NULL) {
            gs_free_object(pgs->memory, (byte *)p_cm_buff,
                           "image_render_interpolate_icc");
        }
    }
    return (h == 0 ? 0 : 1);
}

static int
image_render_interpolate_landscape(gx_image_enum * penum,
                                   const byte * buffer,
                                   int data_x, uint iw, int h,
                                   gx_device * dev)
{
    stream_image_scale_state *pss = penum->scaler;
    const gs_color_space *pcs = penum->pcs;
    gs_logical_operation_t lop = penum->log_op;
    int spp_decode = pss->params.spp_decode;
    stream_cursor_read stream_r;
    stream_cursor_write stream_w;
    byte *out = penum->line;
    bool islab = false;

    if (pcs->cmm_icc_profile_data != NULL) {
        islab = pcs->cmm_icc_profile_data->islab;
    }
    /* Perform any decode procedure if needed */
    initial_decode(penum, buffer, data_x, h, &stream_r, false);
    /*
     * Process input and/or collect output.  By construction, the pixels are
     * 1-for-1 with the device, but the Y coordinate might be inverted.
     */
    {
        int xo = penum->xyi.y;
        int yo = penum->xyi.x;
        int width = pss->params.WidthOut;
        int sizeofPixelOut = pss->params.BitsPerComponentOut / 8;
        int dy;

        if (penum->matrix.yx > 0)
            dy = 1;
        else
            dy = -1, yo--;
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            int x;
            const frac *psrc;
            gx_device_color devc;
            int status, code;

            stream_w.limit = out + width *
                max(spp_decode * sizeofPixelOut, ARCH_SIZEOF_COLOR_INDEX) - 1;
            stream_w.ptr = stream_w.limit - width * spp_decode * sizeofPixelOut;
            psrc = (const frac *)(stream_w.ptr + 1);
            /* This is where the rescale takes place; this will consume the
             * data from stream_r, and post processed data into stream_w. The
             * data in stream_w may be bogus if we are outside the active
             * region, and this will be indicated by pss->params.Active being
             * set to false. */
            status = (*pss->templat->process)
                ((stream_state *) pss, &stream_r, &stream_w, h == 0);
            if (status < 0 && status != EOFC)
                return_error(gs_error_ioerror);
            if (stream_w.ptr == stream_w.limit) {
                int xe = xo + pss->params.PatchWidthOut;

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated (rotated) row %d:\n[B]",
                           penum->line_xy);
                psrc += pss->params.LeftMarginOut * spp_decode;
                for (x = xo; x < xe;) {
                    code = handle_colors(penum, psrc, spp_decode, &devc, islab, dev);
                    if (code < 0)
                        return code;
                    /* We scan for vertical runs of pixels, even if they end up
                     * being split up in most cases within copy_color_unaligned anyway. */
                    {
                        int rcode;
                        int rep = 0;

                        /* as above, see if we can accumulate any runs */
                        switch (spp_decode) {
                            case 1:
                                do {
                                    rep++, psrc += 1;
                                } while ((rep + x) < xe &&
                                         psrc[-1] == psrc[0]);
                                break;
                            case 3:
                                do {
                                    rep++, psrc += 3;
                                } while ((rep + x) < xe &&
                                         psrc[-3] == psrc[0] &&
                                         psrc[-2] == psrc[1] &&
                                         psrc[-1] == psrc[2]);
                                break;
                            case 4:
                                do {
                                    rep++, psrc += 4;
                                } while ((rep + x) < xe &&
                                         psrc[-4] == psrc[0] &&
                                         psrc[-3] == psrc[1] &&
                                         psrc[-2] == psrc[2] &&
                                         psrc[-1] == psrc[3]);
                                break;
                            default:
                                rep = 1;
                                psrc += spp_decode;
                                break;
                        }
                        rcode = gx_fill_rectangle_device_rop(ry, x, 1, rep, &devc, dev, lop);
                        if (rcode < 0)
                            return rcode;
                        x += rep;
                    }
                }
                /*if_debug1m('w', dev->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
inactive:
                penum->line_xy++;
                if_debug0m('B', dev->memory, "\n");
            }
            if ((status == 0 && stream_r.ptr == stream_r.limit) || status == EOFC)
                break;
        }
    }
    return (h == 0 ? 0 : 1);
}

static int
image_render_interpolate_landscape_masked(gx_image_enum * penum,
                                          const byte * buffer,
                                          int data_x, uint iw, int h,
                                          gx_device * dev)
{
    stream_image_scale_state *pss = penum->scaler;
    int spp_decode = pss->params.spp_decode;
    stream_cursor_read stream_r;
    stream_cursor_write stream_w;
    byte *out = penum->line;
    gx_color_index color = penum->icolor1->colors.pure;

    /* Perform any decode procedure if needed. Probably only reversal
     * of the data in this case. */
    initial_decode(penum, buffer, data_x, h, &stream_r, false);
    /*
     * Process input and/or collect output.  By construction, the pixels are
     * 1-for-1 with the device, but the Y coordinate might be inverted.
     */
    {
        int xo = penum->xyi.y;
        int yo = penum->xyi.x;
        int width = pss->params.WidthOut;
        int sizeofPixelOut = pss->params.BitsPerComponentOut / 8;
        int dy;

        if (penum->matrix.yx > 0)
            dy = 1;
        else
            dy = -1, yo--;
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            int x;
            const byte *psrc;
            int status, code;

            stream_w.limit = out + width *
                max(spp_decode * sizeofPixelOut, ARCH_SIZEOF_COLOR_INDEX) - 1;
            stream_w.ptr = stream_w.limit - width * spp_decode * sizeofPixelOut;
            psrc = stream_w.ptr + 1;
            /* This is where the rescale takes place; this will consume the
             * data from stream_r, and post processed data into stream_w. The
             * data in stream_w may be bogus if we are outside the active
             * region, and this will be indicated by pss->params.Active being
             * set to false. */
            status = (*pss->templat->process)
                ((stream_state *) pss, &stream_r, &stream_w, h == 0);
            if (status < 0 && status != EOFC)
                return_error(gs_error_ioerror);
            if (stream_w.ptr == stream_w.limit) {
                int xe = xo + pss->params.PatchWidthOut;

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated masked (rotated) row %d:\n[B]",
                           penum->line_xy);
                psrc += pss->params.LeftMarginOut * spp_decode;
                for (x = xo; x < xe; x++) {
                    code = (*dev_proc(dev, copy_alpha))(dev, psrc, 0, 0,
                        gx_no_bitmap_id, ry, x, 1, 1, color, 8);
                    if (code < 0)
                        return code;
                    psrc += spp_decode;
                }
                /*if_debug1m('w', dev->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
inactive:
                penum->line_xy++;
                if_debug0m('B', dev->memory, "\n");
            }
            if ((status == 0 && stream_r.ptr == stream_r.limit) || status == EOFC)
                break;
        }
    }
    return (h == 0 ? 0 : 1);
}

static int
image_render_interpolate_landscape_masked_hl(gx_image_enum * penum,
                                             const byte * buffer,
                                             int data_x, uint iw, int h,
                                             gx_device * dev)
{
    stream_image_scale_state *pss = penum->scaler;
    int spp_decode = pss->params.spp_decode;
    stream_cursor_read stream_r;
    stream_cursor_write stream_w;
    byte *out = penum->line;

    /* Perform any decode procedure if needed. Probably only reversal
     * of the data in this case. */
    initial_decode(penum, buffer, data_x, h, &stream_r, false);
    /*
     * Process input and/or collect output.  By construction, the pixels are
     * 1-for-1 with the device, but the Y coordinate might be inverted.
     */
    {
        int xo = penum->xyi.y;
        int yo = penum->xyi.x;
        int width = pss->params.WidthOut;
        int sizeofPixelOut = pss->params.BitsPerComponentOut / 8;
        int dy;

        if (penum->matrix.yx > 0)
            dy = 1;
        else
            dy = -1, yo--;
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            int x;
            const byte *psrc;
            int status, code;

            stream_w.limit = out + width *
                max(spp_decode * sizeofPixelOut, ARCH_SIZEOF_COLOR_INDEX) - 1;
            stream_w.ptr = stream_w.limit - width * spp_decode * sizeofPixelOut;
            psrc = stream_w.ptr + 1;
            /* This is where the rescale takes place; this will consume the
             * data from stream_r, and post processed data into stream_w. The
             * data in stream_w may be bogus if we are outside the active
             * region, and this will be indicated by pss->params.Active being
             * set to false. */
            status = (*pss->templat->process)
                ((stream_state *) pss, &stream_r, &stream_w, h == 0);
            if (status < 0 && status != EOFC)
                return_error(gs_error_ioerror);
            if (stream_w.ptr == stream_w.limit) {
                int xe = xo + pss->params.PatchWidthOut;

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated masked (rotated) row %d:\n[B]",
                           penum->line_xy);
                psrc += pss->params.LeftMarginOut * spp_decode;
                for (x = xo; x < xe; x++) {
                    code = (*dev_proc(dev, copy_alpha_hl_color))(dev, psrc, 0, 0,
                        gx_no_bitmap_id, ry, x, 1, 1, penum->icolor1, 8);
                    if (code < 0)
                        return code;
                    psrc += spp_decode;
                }
                /*if_debug1m('w', dev->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
inactive:
                penum->line_xy++;
                if_debug0m('B', dev->memory, "\n");
            }
            if ((status == 0 && stream_r.ptr == stream_r.limit) || status == EOFC)
                break;
        }
    }
    return (h == 0 ? 0 : 1);
}

/* Interpolation with ICC based source spaces. This is done seperately to
   enable optimization and avoid the multiple tranformations that occur in
   the above code */
static int
image_render_interpolate_landscape_icc(gx_image_enum * penum,
                                       const byte * buffer,
                                       int data_x, uint iw, int h,
                                       gx_device * dev)
{
    stream_image_scale_state *pss = penum->scaler;
    const gs_gstate *pgs = penum->pgs;
    gs_logical_operation_t lop = penum->log_op;
    byte *out = penum->line;
    stream_cursor_read stream_r;
    stream_cursor_write stream_w;

    if (penum->icc_link == NULL) {
        return gs_rethrow(-1, "ICC Link not created during gs_image_class_0_interpolate");
    }
    initial_decode(penum, buffer, data_x, h, &stream_r, true);
    /*
     * Process input and/or collect output.  By construction, the pixels are
     * 1-for-1 with the device, but the Y coordinate might be inverted.
     * CM is performed on the entire row.
     */
    {
        int xo = penum->xyi.y;
        int yo = penum->xyi.x;
        int width = pss->params.WidthOut;
        int width_in = pss->params.WidthIn;
        int sizeofPixelOut = pss->params.BitsPerComponentOut / 8;
        int dy;
        unsigned short *p_cm_interp;
        byte *p_cm_buff = NULL;
        byte *psrc;
        int spp_decode = pss->params.spp_decode;
        int spp_interp = pss->params.spp_interp;
        int spp_cm;
        gsicc_bufferdesc_t input_buff_desc;
        gsicc_bufferdesc_t output_buff_desc;
        gx_color_index color;
        int code;
        cmm_dev_profile_t *dev_profile;
        int num_bytes_decode = pss->params.BitsPerComponentIn / 8;

        code = dev_proc(dev, get_profile)(dev, &dev_profile);
        if (code) {
            penum->interpolate = interp_off;
            return 0;
        }
        spp_cm = gsicc_get_device_profile_comps(dev_profile);
        if (penum->matrix.yx > 0)
            dy = 1;
        else
            dy = -1, yo--;
        /* If it makes sense (if enlarging), do early CM */
        if (pss->params.early_cm && !penum->icc_link->is_identity
            && stream_r.ptr != stream_r.limit) {
            /* Get the buffers set up. */
            p_cm_buff =
                (byte *) gs_alloc_bytes(pgs->memory,
                                        num_bytes_decode * width_in * spp_cm,
                                        "image_render_interpolate_icc");
            /* Set up the buffer descriptors. We keep the bytes the same */
            gsicc_init_buffer(&input_buff_desc, spp_decode, num_bytes_decode,
                          false, false, false, 0, width_in * spp_decode,
                          1, width_in);
            gsicc_init_buffer(&output_buff_desc, spp_cm, num_bytes_decode,
                          false, false, false, 0, width_in * spp_cm,
                          1, width_in);
            /* Do the transformation */
            psrc = (byte*) (stream_r.ptr + 1);
            (penum->icc_link->procs.map_buffer)(dev, penum->icc_link, &input_buff_desc,
                                                &output_buff_desc, (void*) psrc,
                                                (void*) p_cm_buff);
            /* Re-set the reading stream to use the cm data */
            stream_r.ptr = p_cm_buff - 1;
            stream_r.limit = stream_r.ptr + num_bytes_decode * width_in * spp_cm;
        } else {
            /* CM after interpolation (or none).  Just set up the buffers
               if needed.  16 bit operations if CM takes place.  */
            if (!penum->icc_link->is_identity) {
                p_cm_buff = (byte *) gs_alloc_bytes(pgs->memory,
                    sizeof(unsigned short) * width * spp_cm,
                    "image_render_interpolate_icc");
                /* Set up the buffer descriptors. */
                gsicc_init_buffer(&input_buff_desc, spp_decode, 2,
                              false, false, false, 0, width * spp_decode,
                              1, width);
                gsicc_init_buffer(&output_buff_desc, spp_cm, 2,
                              false, false, false, 0, width * spp_cm,
                              1, width);
            }
        }
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            int x;
            const unsigned short *pinterp;
            gx_device_color devc;
            int status;

            stream_w.limit = out + width *
                max(spp_interp * sizeofPixelOut, ARCH_SIZEOF_COLOR_INDEX) - 1;
            stream_w.ptr = stream_w.limit - width * spp_interp * sizeofPixelOut;
            pinterp = (const unsigned short *)(stream_w.ptr + 1);
            /* This is where the rescale takes place; this will consume the
             * data from stream_r, and post processed data into stream_w. The
             * data in stream_w may be bogus if we are outside the active
             * region, and this will be indiated by pss->params.Active being
             * set to false. */
            status = (*pss->templat->process)
                ((stream_state *) pss, &stream_r, &stream_w, h == 0);
            if (status < 0 && status != EOFC)
                return_error(gs_error_ioerror);
            if (stream_w.ptr == stream_w.limit) {
                int xe = xo + pss->params.PatchWidthOut;

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated row %d:\n[B]",
                           penum->line_xy);
                /* Take care of CM on the entire interpolated row, if we
                   did not already do CM */
                if (penum->icc_link->is_identity || pss->params.early_cm) {
                    /* Fastest case. No CM needed */
                    p_cm_interp = (unsigned short *) pinterp;
                } else {
                    /* Transform */
                    p_cm_interp = (unsigned short *) p_cm_buff;
                    (penum->icc_link->procs.map_buffer)(dev, penum->icc_link,
                                                        &input_buff_desc,
                                                        &output_buff_desc,
                                                        (void*) pinterp,
                                                        (void*) p_cm_interp);
                }
                p_cm_interp += pss->params.LeftMarginOut * spp_cm;
                for (x = xo; x < xe;) {
#ifdef DEBUG
                    if (gs_debug_c('B')) {
                        int ci;

                        for (ci = 0; ci < spp_cm; ++ci)
                            dmprintf2(dev->memory, "%c%04x", (ci == 0 ? ' ' : ','),
                                     p_cm_interp[ci]);
                    }
#endif
                    /* Get the device color */
                    get_device_color(penum, p_cm_interp, &devc, &color, dev);
                    /* We scan for vertical runs of pixels, even if they end up
                     * being split up in most cases within copy_color_unaligned anyway. */
                    {
                        int rcode;
                        int rep = 0;

                        switch (spp_cm) {
                            case 1:
                                do {
                                    rep++, p_cm_interp += 1;
                                } while ((rep + x) < xe && p_cm_interp[-1] == p_cm_interp[0]);
                                break;
                            case 3:
                                do {
                                    rep++, p_cm_interp += 3;
                                } while ((rep + x) < xe && p_cm_interp[-3] == p_cm_interp[0] &&
                                     p_cm_interp[-2] == p_cm_interp[1] &&
                                     p_cm_interp[-1] == p_cm_interp[2]);
                                break;
                            case 4:
                                do {
                                    rep++, p_cm_interp += 4;
                                } while ((rep + x) < xe && p_cm_interp[-4] == p_cm_interp[0] &&
                                     p_cm_interp[-3] == p_cm_interp[1] &&
                                     p_cm_interp[-2] == p_cm_interp[2] &&
                                     p_cm_interp[-1] == p_cm_interp[3]);
                                break;
                            default:
                                rep = 1, p_cm_interp += spp_cm;
                                break;
                        }

                        rcode = gx_fill_rectangle_device_rop(ry, x, 1, rep, &devc, dev, lop);
                        if (rcode < 0)
                            return rcode;
                        x += rep;
                    }
                }  /* End on x loop */
                /*if_debug1m('w', penum->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
inactive:
                penum->line_xy++;
                if_debug0m('B', penum->memory, "\n");
            }
            if ((status == 0 && stream_r.ptr == stream_r.limit) || status == EOFC)
                break;
        }
        /* Free cm buffer, if it was used */
        if (p_cm_buff != NULL) {
            gs_free_object(pgs->memory, (byte *)p_cm_buff,
                           "image_render_interpolate_icc");
        }
    }
    return (h == 0 ? 0 : 1);
}

/* Decode a 16-bit sample into a floating point color component.
   This is used for cases where the spatial interpolation function output is 16 bit.
   It is only used here, hence the static declaration for now. */

static void
decode_sample_frac_to_float(gx_image_enum *penum, frac sample_value, gs_client_color *cc, int i)
{
    switch ( penum->map[i].decoding )
    {
        case sd_none:
            cc->paint.values[i] = frac2float(sample_value);
            break;
        case sd_lookup:
            cc->paint.values[i] =
                   penum->map[i].decode_lookup[(frac2byte(sample_value)) >> 4];
            break;
        case sd_compute:
            cc->paint.values[i] =
                   penum->map[i].decode_base + frac2float(sample_value)*255.0 * penum->map[i].decode_factor;
    }
}
