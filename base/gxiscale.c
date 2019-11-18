/* Copyright (C) 2001-2019 Artifex Software, Inc.
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
#include "gsbitops.h"
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
    } else if (dev_proc(penum->dev, copy_alpha_hl_color) == gx_default_no_copy_alpha_hl_color) {
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

int
gs_image_class_0_interpolate(gx_image_enum * penum, irender_proc_t *render_fn)
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
    gx_color_polarity_t pol = GX_CINFO_POLARITY_UNKNOWN;
    int mask_col_high_level = 0;
    int interpolate_control = penum->dev->interpolate_control;
    int abs_interp_limit = max(1, any_abs(interpolate_control));
    int limited_WidthOut, limited_HeightOut;

    if (interpolate_control < 0)
        penum->interpolate = interp_on;		/* not the same as "interp_force" -- threshold still used */
    if (interpolate_control == interp_off || penum->interpolate == interp_off) {
        penum->interpolate = interp_off;
        return 0;
    }
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
        /* A calculation has overflowed. Bail */
        return 0;
    }

    if (penum->masked) {
        abs_interp_limit = 1;		/* ignore this for masked images for now */
        use_icc = false;
        num_des_comps = 1;
        if (pcs)
            pol = cs_polarity(pcs);
        else
            pol = GX_CINFO_POLARITY_ADDITIVE;
    } else {
        if (pcs == NULL)
            return 0;		/* can't handle this */
        if (pcs->cmm_icc_profile_data != NULL) {
            use_icc = true;
        }
        if (pcs->type->index == gs_color_space_index_Indexed) {
            if (pcs->base_space->cmm_icc_profile_data != NULL) {
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
    iss.abs_interp_limit = abs_interp_limit;
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
    iss.PatchWidthIn = penum->drect.w;
    iss.PatchHeightIn = penum->drect.h;
    iss.LeftMarginIn = penum->drect.x - penum->rect.x;
    iss.TopMarginIn = penum->drect.y - penum->rect.y;
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
        iss.TopMarginOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.y - penum->rect.y) *
                                                              dh / penum->Height));
        iss.PatchHeightOut = fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.h *
                                                                dh / penum->Height))
                           - iss.TopMarginOut;
        iss.PatchWidthOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.x + penum->rrect.w) *
                                                               dw / penum->Width))
                          - fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.x *
                                                               dw / penum->Width));
        iss.LeftMarginOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.x - penum->rect.x) *
                                                               dw / penum->Width));
        iss.TopMarginOut2 = fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.y *
                                                              dh / penum->Height));
        iss.PatchHeightOut2 = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.y + penum->rrect.h) *
                                                                dh / penum->Height))
                           - iss.TopMarginOut2;
        iss.pad_y = iss.TopMarginOut2
                  - fixed2int_pixround_perfect(
                                 (fixed)((int64_t)penum->rect.y *
                                         dh / penum->Height));
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
        iss.TopMarginOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.y - penum->rect.y) *
                                                              dw / penum->Height));
        iss.PatchHeightOut = fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.h *
                                                                dw / penum->Height))
                           - iss.TopMarginOut;
        iss.PatchWidthOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.x + penum->rrect.w) *
                                                               dh / penum->Width))
                          - fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.x *
                                                               dh / penum->Width));
        iss.LeftMarginOut = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.x - penum->rect.x) *
                                                               dh / penum->Width));
        iss.TopMarginOut2 = fixed2int_pixround_perfect((fixed)((int64_t)penum->rrect.y *
                                                              dw / penum->Height));
        iss.PatchHeightOut2 = fixed2int_pixround_perfect((fixed)((int64_t)(penum->rrect.y + penum->rrect.h) *
                                                                dw / penum->Height))
                           - iss.TopMarginOut2;
        iss.pad_y = 0;
    }
    iss.PatchWidthOut = any_abs(iss.PatchWidthOut);
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
    limited_WidthOut = (iss.WidthOut + abs_interp_limit - 1)/abs_interp_limit;
    limited_HeightOut = (iss.HeightOut + abs_interp_limit - 1)/abs_interp_limit;
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
    /* Set up the filter template. May be changed for "Special" downscaling */
#ifdef USE_MITCHELL_FILTER
    templat = &s_IScale_template;
#else
    templat = &s_IIEncode_template;
#endif

    if ((iss.WidthOut < iss.WidthIn) &&
        (iss.HeightOut < iss.HeightIn) &&       /* downsampling */
        (pol != GX_CINFO_POLARITY_UNKNOWN) &&
        (dev_proc(penum->dev, dev_spec_op)(penum->dev, gxdso_interpolate_antidropout, NULL, 0) > 0)) {
        /* Special case handling for when we are downsampling to a dithered
        * device.  The point of this non-linear downsampling is to preserve
        * dark pixels from the source image to avoid dropout. The color
        * polarity is used for this. */
        templat = &s_ISpecialDownScale_template;
    } else {
        /* No interpolation unless we exceed the device selected minimum */
        int threshold = dev_proc(penum->dev, dev_spec_op)(penum->dev, gxdso_interpolate_threshold, NULL, 0);

        if ((iss.WidthOut == iss.WidthIn && iss.HeightOut == iss.HeightIn) ||
            ((penum->interpolate != interp_force) &&
                (threshold > 0) &&
                (iss.WidthOut < iss.WidthIn * threshold) &&
                (iss.HeightOut < iss.HeightIn * threshold))) {
            penum->interpolate = interp_off;
            return 0;       /* don't interpolate if not scaled up enough */
        }
    }
    /* The SpecialDownScale filter needs polarity, either ADDITIVE or SUBTRACTIVE */
    /* UNKNOWN case (such as for palette colors) has been handled above */
    iss.ColorPolarityAdditive = (pol == GX_CINFO_POLARITY_ADDITIVE);

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
    /* Allocate a buffer for one source/destination line.			*/
    /* NB: The out_size is for full device res, regardless of abs_interp_limit	*/
    /*     since we will expand into that area in the x-loop			*/
    {
        uint out_size = iss.WidthOut * max(iss.spp_interp * ((iss.BitsPerComponentOut) / 8),
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
    /* If the InterpolationControl specifies interpolation to less than full device res	*/
    /* Set up a scaling DDA to control scaling back to desired image size.		*/
    if (abs_interp_limit > 1) {
        dda_init(pss->params.scale_dda.x, 0, iss.WidthOut, limited_WidthOut);
        dda_init(pss->params.scale_dda.y, 0, iss.HeightOut, limited_HeightOut);
    }
    if_debug0m('b', penum->memory, "[b]render=interpolate\n");
    if (penum->masked) {
        if (!mask_col_high_level) {
            *render_fn = (penum->posture == image_portrait ?
                    &image_render_interpolate_masked :
                    &image_render_interpolate_landscape_masked);
            return 0;
        } else {
            *render_fn = (penum->posture == image_portrait ?
                    &image_render_interpolate_masked_hl :
                    &image_render_interpolate_landscape_masked_hl);
            return 0;
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
        *render_fn = (penum->posture == image_portrait ?
                &image_render_interpolate_icc :
                &image_render_interpolate_landscape_icc);
        return 0;
    } else {
        *render_fn = (penum->posture == image_portrait ?
                &image_render_interpolate :
                &image_render_interpolate_landscape);
        return 0;
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
            }
        } else {
            /* More than 8-bits/color values */
            /* Even in this case we need to worry about an indexed color space.
               We need to get to the base color space for the interpolation and
               then if necessary do the remap to the device space */
            if (pcs == NULL || pcs->type->index != gs_color_space_index_Indexed) {
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
                    decode_value = penum->map[0].decode_base +
                        (((const frac *)pdata)[0]) * penum->map[0].decode_factor;
                    /* Now we need to do the lookup of this value, and stick it
                       in psrc as a frac, which is what the interpolator is
                       expecting, since we had more than 8 bits of original
                       image data */
                    gs_cspace_indexed_lookup_frac(pcs, decode_value,psrc);
                    pdata += dpd;
                }
            } /* end of else on indexed */
        }  /* end of else on more than 8 bps */
        stream_r->limit = stream_r->ptr + row_size;
    } else {                    /* h == 0 */
        stream_r->ptr = 0, stream_r->limit = 0;
    }
}

static int
handle_device_color(gx_image_enum *penum, const frac *psrc,
                    gx_device_color *devc, gx_device *dev,
                    const cmm_dev_profile_t *dev_profile,
                    const gs_color_space *pcs)
{
    const gs_gstate *pgs = penum->pgs;

    return (*pcs->type->remap_concrete_color)
            (pcs, psrc, devc, pgs, dev, gs_color_select_source, dev_profile);
}

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
static int
handle_labicc_color8(gx_image_enum *penum, const frac *psrc,
                     gx_device_color *devc, gx_device *dev,
                     const cmm_dev_profile_t *dev_profile,
                     const gs_color_space *pcs)
{
    const gs_gstate *pgs = penum->pgs;
    gs_client_color cc;

    decode_sample_frac_to_float(penum, psrc[0], &cc, 0);
    decode_sample_frac_to_float(penum, psrc[1], &cc, 1);
    decode_sample_frac_to_float(penum, psrc[2], &cc, 2);
    cc.paint.values[0] /= 100.0;
    cc.paint.values[1] = (cc.paint.values[1] + 128) / 255.0;
    cc.paint.values[2] = (cc.paint.values[2] + 128) / 255.0;
    return gx_remap_ICC_imagelab(&cc, pcs, devc, pgs, dev, gs_color_select_source);
}

static int
handle_labicc_color16(gx_image_enum *penum, const frac *psrc,
                      gx_device_color *devc, gx_device *dev,
                      const cmm_dev_profile_t *dev_profile,
                      const gs_color_space *pcs)
{
    const gs_gstate *pgs = penum->pgs;
    gs_client_color cc;

    decode_sample_frac_to_float(penum, psrc[0], &cc, 0);
    decode_sample_frac_to_float(penum, psrc[1], &cc, 1);
    decode_sample_frac_to_float(penum, psrc[2], &cc, 2);
    cc.paint.values[0] *= 0x7ff8 / 25500.0f;
    cc.paint.values[1] = (cc.paint.values[1] + 128) * 0x7ff8 / 65025.0;
    cc.paint.values[2] = (cc.paint.values[2] + 128) * 0x7ff8 / 65025.0;
    return gx_remap_ICC_imagelab(&cc, pcs, devc, pgs, dev, gs_color_select_source);
}

static int
handle_lab_color8(gx_image_enum *penum, const frac *psrc,
                  gx_device_color *devc, gx_device *dev,
                  const cmm_dev_profile_t *dev_profile,
                  const gs_color_space *pcs)
{
    const gs_gstate *pgs = penum->pgs;
    gs_client_color cc;

    decode_sample_frac_to_float(penum, psrc[0], &cc, 0);
    decode_sample_frac_to_float(penum, psrc[1], &cc, 1);
    decode_sample_frac_to_float(penum, psrc[2], &cc, 2);
    cc.paint.values[0] /= 100.0;
    cc.paint.values[1] = (cc.paint.values[1] + 128) / 255.0;
    cc.paint.values[2] = (cc.paint.values[2] + 128) / 255.0;
    return (pcs->type->remap_color)
                (&cc, pcs, devc, pgs, dev, gs_color_select_source);
}

static int
handle_lab_color16(gx_image_enum *penum, const frac *psrc,
                   gx_device_color *devc, gx_device *dev,
                   const cmm_dev_profile_t *dev_profile,
                   const gs_color_space *pcs)
{
    const gs_gstate *pgs = penum->pgs;
    gs_client_color cc;

    decode_sample_frac_to_float(penum, psrc[0], &cc, 0);
    decode_sample_frac_to_float(penum, psrc[1], &cc, 1);
    decode_sample_frac_to_float(penum, psrc[2], &cc, 2);
    cc.paint.values[0] *= 0x7ff8 / 25500.0f;
    cc.paint.values[1] = (cc.paint.values[1] + 128) * 0x7ff8 / 65025.0;
    cc.paint.values[2] = (cc.paint.values[2] + 128) * 0x7ff8 / 65025.0;
    return (pcs->type->remap_color)
                (&cc, pcs, devc, pgs, dev, gs_color_select_source);
}

static int
handle_labicc_color2_idx(gx_image_enum *penum, const frac *psrc,
                         gx_device_color *devc, gx_device *dev,
                         const cmm_dev_profile_t *dev_profile,
                         const gs_color_space *pcs)
{
    const gs_gstate *pgs = penum->pgs;
    gs_client_color cc;
    int j;
    int num_components = gs_color_space_num_components(pcs);

    /* If we were indexed, dont use the decode procedure for the index
       values just get to float directly */
    for (j = 0; j < num_components; ++j)
        cc.paint.values[j] = frac2float(psrc[j]);
    /* If the source colors are LAB then use the mapping that does not
       rescale the source colors */
    return gx_remap_ICC_imagelab(&cc, pcs, devc, pgs, dev,
                                 gs_color_select_source);
}

static int
handle_remap_color_idx(gx_image_enum *penum, const frac *psrc,
                       gx_device_color *devc, gx_device *dev,
                       const cmm_dev_profile_t *dev_profile,
                       const gs_color_space *pcs)
{
    const gs_gstate *pgs = penum->pgs;
    gs_client_color cc;
    int j;
    int num_components = gs_color_space_num_components(pcs);

    for (j = 0; j < num_components; ++j)
        cc.paint.values[j] = frac2float(psrc[j]);

    return (pcs->type->remap_color)
                (&cc, pcs, devc, pgs, dev, gs_color_select_source);
}

static int
handle_labicc_color2(gx_image_enum *penum, const frac *psrc,
                     gx_device_color *devc, gx_device *dev,
                     const cmm_dev_profile_t *dev_profile,
                     const gs_color_space *pcs)
{
    const gs_gstate *pgs = penum->pgs;
    gs_client_color cc;
    int j;
    int num_components = gs_color_space_num_components(pcs);

    for (j = 0; j < num_components; ++j)
        decode_sample_frac_to_float(penum, psrc[j], &cc, j);
    /* If the source colors are LAB then use the mapping that does not
       rescale the source colors */
    return gx_remap_ICC_imagelab(&cc, pcs, devc, pgs, dev,
                                 gs_color_select_source);
}

static int
handle_remap_color(gx_image_enum *penum, const frac *psrc,
                   gx_device_color *devc, gx_device *dev,
                   const cmm_dev_profile_t *dev_profile,
                   const gs_color_space *pcs)
{
    const gs_gstate *pgs = penum->pgs;
    gs_client_color cc;
    int j;
    int num_components = gs_color_space_num_components(pcs);

    for (j = 0; j < num_components; ++j)
        decode_sample_frac_to_float(penum, psrc[j], &cc, j);

    return (pcs->type->remap_color)
                (&cc, pcs, devc, pgs, dev, gs_color_select_source);
}

typedef int (color_handler_fn)(gx_image_enum *penum, const frac *psrc,
                               gx_device_color *devc, gx_device *dev,
                               const cmm_dev_profile_t *dev_profile,
                               const gs_color_space *pcs);

static color_handler_fn *
get_color_handler(gx_image_enum *penum, int spp_decode,
                  bool islab, const cmm_dev_profile_t *dev_profile,
                  const gs_color_space **pconc)
{
    const gs_gstate *pgs = penum->pgs;
    const gs_color_space *pcs = penum->pcs;
    bool is_index_space;

    if (pcs == NULL)
        return NULL; /* Must be masked */

    is_index_space = (pcs->type->index == gs_color_space_index_Indexed);
    /* If we are in a non device space then work from the pcs not from the
    concrete space also handle index case, where base case was device type */
    /* We'll have done the interpolation in the base space, not the indexed
     * space, so allow for that here. */
    if (is_index_space)
        pcs = pcs->base_space;
    if (dev_profile->usefastcolor &&
        gsicc_is_default_profile(pcs->cmm_icc_profile_data) &&
        dev_profile->device_profile[0]->num_comps == spp_decode) {
        const gs_color_space * pconcs = cs_concrete_space(pcs, pgs);
        if (pconcs && pconcs == pcs) {
            *pconc = pconcs;
            return handle_device_color;
        }
    }

    *pconc = pcs;
    /* If we are device dependent we need to get back to float prior to remap.*/
    if (islab) {
        if (gs_color_space_is_ICC(pcs) &&
            pcs->cmm_icc_profile_data != NULL &&
            pcs->cmm_icc_profile_data->islab)
            return penum->bps <= 8 ? handle_labicc_color8 : handle_labicc_color16;
        else
            return penum->bps <= 8 ? handle_lab_color8 : handle_lab_color16;
    } else if (is_index_space) {
        if (gs_color_space_is_ICC(pcs) &&
            pcs->cmm_icc_profile_data != NULL &&
            pcs->cmm_icc_profile_data->islab)
            return handle_labicc_color2_idx;
        else
            return handle_remap_color_idx;
    } else {
        if (gs_color_space_is_ICC(pcs) &&
            pcs->cmm_icc_profile_data != NULL &&
            pcs->cmm_icc_profile_data->islab)
            return handle_labicc_color2;
        else
            return handle_remap_color;
    }
}

/* returns the expanded width using the dda.x */
static int
interpolate_scaled_expanded_width(int delta_x, stream_image_scale_state *pss)
{
    gx_dda_fixed_point tmp_dda = pss->params.scale_dda;
    int start_x = dda_current(tmp_dda.x);

    do {
        dda_next(tmp_dda.x);
    } while (--delta_x > 0);

    return dda_current(tmp_dda.x) - start_x;
}

/* returns the expanded height using the dda.y */
static int
interpolate_scaled_expanded_height(int delta_y, stream_image_scale_state *pss)
{
    gx_dda_fixed_point tmp_dda = pss->params.scale_dda;
    int start_y = dda_current(tmp_dda.y);

    do {
        dda_next(tmp_dda.y);
    } while (--delta_y > 0);

    return dda_current(tmp_dda.y) - start_y;
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
    int abs_interp_limit = pss->params.abs_interp_limit;
    int limited_PatchWidthOut = (pss->params.PatchWidthOut + abs_interp_limit - 1) / abs_interp_limit;
    const gs_color_space *pconc;
    cmm_dev_profile_t *dev_profile;
    int code = dev_proc(penum->dev, get_profile)(penum->dev, &dev_profile);

    if (code < 0)
        return code;

    if (!penum->masked && pcs->cmm_icc_profile_data != NULL) {
        islab = pcs->cmm_icc_profile_data->islab;
    }
    /* Perform any decode procedure if needed */
    initial_decode(penum, buffer, data_x, h, &stream_r, false);
    /*
     * Process input and/or collect output.  By construction, the pixels are
     * 1-for-1 with the device if the abs(interpolate_control) is 1  but the
     * Y coordinate might be inverted.
     */
    {
        int xo = penum->xyi.x;
        int yo = penum->xyi.y;
        int width = (pss->params.WidthOut + abs_interp_limit - 1) / abs_interp_limit;
        int sizeofPixelOut = pss->params.BitsPerComponentOut / 8;
        int dy;
        int bpp = dev->color_info.depth;
        uint raster = bitmap_raster(width * bpp);
        color_handler_fn *color_handler = NULL;

        if (penum->matrix.yy > 0)
            dy = 1;
        else
            dy = -1, yo--;
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            int x;
            const frac *psrc;
            gx_device_color devc;
            int status;
            byte *l_dptr = out;
            int l_dbit = 0;
            byte l_dbyte = 0;
            int l_xprev = (xo);
            int scaled_x_prev = 0;
            gx_dda_fixed save_x_dda = pss->params.scale_dda.x;

            devc.type = gx_dc_type_none; /* Needed for coverity, in call to color_is_pure() if color_handler is NULL. */
            stream_w.limit = out + pss->params.WidthOut *
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
                int xe = xo + limited_PatchWidthOut;
                int scaled_w = 0;		/* accumulate scaled up width */
                int scaled_h = 0;
                int scaled_y = 0;

                if (abs_interp_limit > 1) {
                    scaled_h = interpolate_scaled_expanded_height(1, pss);
                    scaled_y = yo + (dy * dda_current(pss->params.scale_dda.y));
                }

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated row %d:\n[B]",
                           penum->line_xy);
                psrc += ((pss->params.LeftMarginOut + abs_interp_limit - 1) / abs_interp_limit) * spp_decode;

                if (color_handler == NULL)
                    color_handler = get_color_handler(penum, spp_decode, islab, dev_profile, &pconc);
                for (x = xo; x < xe;) {
                    if (color_handler != NULL) {
#ifdef DEBUG
                        if (gs_debug_c('B')) {
                            int ci;

                            for (ci = 0; ci < spp_decode; ++ci)
                                dmprintf2(dev->memory, "%c%04x", (ci == 0 ? ' ' : ','),
                                          psrc[ci]);
                        }
#endif
                        code = color_handler(penum, psrc, &devc, dev, dev_profile, pconc);
                        if (code < 0)
                            return code;
                    }
                    if (color_is_pure(&devc)) {
                        gx_color_index color = devc.colors.pure;
                        int expand = 1;

                        if (abs_interp_limit > 1) {
                            expand = interpolate_scaled_expanded_width(1, pss);
                        }
                        /* Just pack colors into a scan line. */
                        /* Skip runs quickly for the common cases. */
                        switch (spp_decode) {
                            case 1:
                                do {
                                    scaled_w += expand;
                                    while (expand-- > 0) {
                                        if (sizeof(color) > 4) {
                                            if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                                return_error(gs_error_rangecheck);
                                        }
                                        else {
                                            if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                                return_error(gs_error_rangecheck);
                                        }
                                    };
                                    x++, psrc += 1;
                                    if (abs_interp_limit > 1) {
                                        dda_next(pss->params.scale_dda.x);
                                        expand = interpolate_scaled_expanded_width(1, pss);
                                    } else
                                        expand = 1;
                                } while (x < xe && psrc[-1] == psrc[0]);
                                break;
                            case 3:
                                do {
                                    scaled_w += expand;
                                    while (expand-- > 0) {
                                        if (sizeof(color) > 4) {
                                            if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                                return_error(gs_error_rangecheck);
                                        }
                                        else {
                                            if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                                return_error(gs_error_rangecheck);
                                        }
                                    };
                                    x++, psrc += 3;
                                    if (abs_interp_limit > 1) {
                                        dda_next(pss->params.scale_dda.x);
                                        expand = interpolate_scaled_expanded_width(1, pss);
                                    } else
                                        expand = 1;
                                } while (x < xe &&
                                         psrc[-3] == psrc[0] &&
                                         psrc[-2] == psrc[1] &&
                                         psrc[-1] == psrc[2]);
                                break;
                            case 4:
                                do {
                                    scaled_w += expand;
                                    while (expand-- > 0) {
                                        if (sizeof(color) > 4) {
                                            if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                                return_error(gs_error_rangecheck);
                                        }
                                        else {
                                            if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                                return_error(gs_error_rangecheck);
                                        }
                                    };
                                    x++, psrc += 4;
                                    if (abs_interp_limit > 1) {
                                        dda_next(pss->params.scale_dda.x);
                                        expand = interpolate_scaled_expanded_width(1, pss);
                                    } else
                                        expand = 1;
                                } while (x < xe &&
                                         psrc[-4] == psrc[0] &&
                                         psrc[-3] == psrc[1] &&
                                         psrc[-2] == psrc[2] &&
                                         psrc[-1] == psrc[3]);
                                break;
                            default:	/* no run length check for these spp cases */
                                scaled_w += expand;
                                while (expand-- > 0) {
                                    if (sizeof(color) > 4) {
                                        if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                    else {
                                        if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                            return_error(gs_error_rangecheck);
                                    }
                                };
                                x++, psrc += spp_decode;
                                if (abs_interp_limit > 1)
                                    dda_next(pss->params.scale_dda.x);
                        }
                    } else {
                        int rcode, rep = 0;

                        /* do _COPY in case any pure colors were accumulated above */
                        if ( x > l_xprev ) {
                            sample_store_flush(l_dptr, l_dbit, l_dbyte);
                            if (abs_interp_limit <= 1) {
                                code = (*dev_proc(dev, copy_color))
                                  (dev, out, l_xprev - xo, raster,
                                   gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                                if (code < 0)
                                    return code;
                            } else {
                                /* scale up in X and Y */
                                int scaled_x = xo + scaled_x_prev;
                                int i = scaled_h;
                                int iy = scaled_y;

                                for (; i > 0; --i) {
                                    code = (*dev_proc(dev, copy_color))
                                      (dev, out, scaled_x_prev, raster,
                                       gx_no_bitmap_id, scaled_x, iy, scaled_w, 1);
                                    if (code < 0)
                                        return code;
                                    iy += dy;
                                }
                                scaled_x_prev = dda_current(pss->params.scale_dda.x);
                            }
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
                        if (abs_interp_limit <= 1) {
                            scaled_w = rep;
                            rcode = gx_fill_rectangle_device_rop(x, ry, rep, 1, &devc, dev, lop);
                            if (rcode < 0)
                                return rcode;
                        } else {
                            int scaled_x = xo + scaled_x_prev;

                            scaled_w = interpolate_scaled_expanded_width(rep, pss);
                            rcode = gx_fill_rectangle_device_rop(scaled_x, scaled_y, scaled_w, scaled_h,
                                                                 &devc, dev, lop);
                            if (rcode < 0)
                                return rcode;
                            dda_advance(pss->params.scale_dda.x, rep);
                            scaled_x_prev = dda_current(pss->params.scale_dda.x);
                        }
                        while (scaled_w-- > 0)
                            sample_store_skip_next(&l_dptr, &l_dbit, bpp, &l_dbyte);
                        scaled_w = 0;
                        l_xprev = x + rep;
                        x += rep;
                    }
                }  /* End on x loop */
                if ( x > l_xprev ) {
                    sample_store_flush(l_dptr, l_dbit, l_dbyte);
                    if (abs_interp_limit <= 1) {
                        code = (*dev_proc(dev, copy_color))
                          (dev, out, l_xprev - xo, raster,
                           gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                        if (code < 0)
                            return code;
                    } else {
                        /* scale up in X and Y */
                        int scaled_x = xo + scaled_x_prev;

                        for (; scaled_h > 0; --scaled_h) {
                            code = (*dev_proc(dev, copy_color))
                              (dev, out, scaled_x_prev, raster,
                               gx_no_bitmap_id, scaled_x, scaled_y, scaled_w, 1);
                            if (code < 0)
                                return code;
                            scaled_y += dy;
                        }
                    }
                }
                /*if_debug1m('w', dev->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
inactive:
                penum->line_xy++;
                if (abs_interp_limit > 1) {
                    dda_next(pss->params.scale_dda.y);
                    pss->params.scale_dda.x = save_x_dda;	/* reset X to start of line */
                }
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

typedef int (*irii_core_fn)(gx_image_enum * penum, int xo, int xe, int spp_cm, unsigned short *p_cm_interp, gx_device *dev, int abs_interp_limit, int bpp, int raster, int yo, int dy, gs_logical_operation_t lop);

static inline int
irii_inner_template(gx_image_enum * penum, int xo, int xe, int spp_cm, unsigned short *p_cm_interp, gx_device *dev, int abs_interp_limit, int bpp, int raster, int yo, int dy, gs_logical_operation_t lop)
{
    int x;
    gx_device_color devc;
    gx_color_index color;
    stream_image_scale_state *pss = penum->scaler;
    int scaled_w = 0;		/* accumulate scaled up width */
    byte *out = penum->line;
    byte *l_dptr = out;
    int l_dbit = 0;
    byte l_dbyte = 0;
    int l_xprev = (xo);
    int scaled_x_prev = 0;
    int code;
    int ry = yo + penum->line_xy * dy;
    gx_dda_fixed save_x_dda = pss->params.scale_dda.x;
    int scaled_h = 0;
    int scaled_y = 0;

    if (abs_interp_limit > 1) {
        scaled_h = interpolate_scaled_expanded_height(1, pss);
        scaled_y = yo + (dy * dda_current(pss->params.scale_dda.y));
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
            gx_color_index color = devc.colors.pure;
            int expand = 1;

            if (abs_interp_limit > 1) {
                expand = interpolate_scaled_expanded_width(1, pss);
            }
            /* Just pack colors into a scan line. */
            /* Skip runs quickly for the common cases. */
            switch (spp_cm) {
                case 1:
                    do {
                        scaled_w += expand;
                        while (expand-- > 0) {
                            if (sizeof(color) > 4) {
                                if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                    return_error(gs_error_rangecheck);
                            } else {
                                if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                    return_error(gs_error_rangecheck);
                            }
                        }
                        x++, p_cm_interp += 1;
                        if (abs_interp_limit > 1) {
                            dda_next(pss->params.scale_dda.x);
                            expand = interpolate_scaled_expanded_width(1, pss);
                        } else
                            expand = 1;
                    } while (x < xe && p_cm_interp[-1] == p_cm_interp[0]);
                    break;
                case 3:
                    do {
                        scaled_w += expand;
                        while (expand-- > 0) {
                            if (sizeof(color) > 4) {
                                if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                    return_error(gs_error_rangecheck);
                            } else {
                                if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                    return_error(gs_error_rangecheck);
                            }
                        }
                        x++, p_cm_interp += 3;
                        if (abs_interp_limit > 1) {
                            dda_next(pss->params.scale_dda.x);
                            expand = interpolate_scaled_expanded_width(1, pss);
                        } else
                            expand = 1;
                    } while (x < xe && p_cm_interp[-3] == p_cm_interp[0] &&
                                       p_cm_interp[-2] == p_cm_interp[1] &&
                                       p_cm_interp[-1] == p_cm_interp[2]);
                    break;
                case 4:
                    do {
                        scaled_w += expand;
                        while (expand-- > 0) {
                            if (sizeof(color) > 4) {
                                if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                    return_error(gs_error_rangecheck);
                            } else {
                                if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                    return_error(gs_error_rangecheck);
                            }
                        }
                        x++, p_cm_interp += 4;
                        if (abs_interp_limit > 1) {
                            dda_next(pss->params.scale_dda.x);
                            expand = interpolate_scaled_expanded_width(1, pss);
                        } else
                            expand = 1;
                    } while (x < xe && p_cm_interp[-4] == p_cm_interp[0] &&
                                       p_cm_interp[-3] == p_cm_interp[1] &&
                                       p_cm_interp[-2] == p_cm_interp[2] &&
                                       p_cm_interp[-1] == p_cm_interp[3]);
                    break;
                default:
                    scaled_w += expand;
                    while (expand-- > 0) {
                        if (sizeof(color) > 4) {
                            if (sample_store_next64(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                return_error(gs_error_rangecheck);
                        } else {
                            if (sample_store_next32(color, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                                return_error(gs_error_rangecheck);
                        }
                    };
                    x++, p_cm_interp += spp_cm;
                    if (abs_interp_limit > 1)
                        dda_next(pss->params.scale_dda.x);
            }
        } else {
            int rcode, rep = 0;

            /* do _COPY in case any pure colors were accumulated above*/
            if ( x > l_xprev ) {
                sample_store_flush(l_dptr, l_dbit, l_dbyte);
                if (abs_interp_limit <= 1) {
                    code = (*dev_proc(dev, copy_color))
                                   (dev, out, l_xprev - xo, raster,
                                    gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                    if (code < 0)
                        return code;
                } else {
                    /* scale up in X and Y */
                    int scaled_x = xo + scaled_x_prev;
                    int i = scaled_h;
                    int iy = scaled_y;

                    for (; i > 0; --i) {
                         code = (*dev_proc(dev, copy_color))
                                      (dev, out, scaled_x_prev, raster,
                                       gx_no_bitmap_id, scaled_x, iy, scaled_w, 1);
                         if (code < 0)
                             return code;
                         iy += dy;
                    }
                    scaled_x_prev = dda_current(pss->params.scale_dda.x);
                }
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
            if (abs_interp_limit <= 1) {
                scaled_w = rep;
                rcode = gx_fill_rectangle_device_rop(x, ry, rep, 1, &devc, dev, lop);
                if (rcode < 0)
                    return rcode;
            } else {
                int scaled_x = xo + scaled_x_prev;

                scaled_w = interpolate_scaled_expanded_width(rep, pss);
                rcode = gx_fill_rectangle_device_rop(scaled_x, scaled_y, scaled_w, scaled_h,
                                                     &devc, dev, lop);
                if (rcode < 0)
                    return rcode;
                dda_advance(pss->params.scale_dda.x, rep);
                scaled_x_prev = dda_current(pss->params.scale_dda.x);
            }
            while (scaled_w-- > 0)
                sample_store_skip_next(&l_dptr, &l_dbit, bpp, &l_dbyte);
            scaled_w = 0;
            x += rep;
            l_xprev = x;
        }
    }  /* End on x loop */
    if ( x > l_xprev ) {
        sample_store_flush(l_dptr, l_dbit, l_dbyte);
        if (abs_interp_limit <= 1) {
            code = (*dev_proc(dev, copy_color))
                          (dev, out, l_xprev - xo, raster,
                           gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
            if (code < 0)
                return code;
        } else {
            /* scale up in X and Y */
            int scaled_x = xo + scaled_x_prev;

            for (; scaled_h > 0; --scaled_h) {
                code = (*dev_proc(dev, copy_color))
                              (dev, out, scaled_x_prev, raster,
                               gx_no_bitmap_id, scaled_x, scaled_y, scaled_w, 1);
                if (code < 0)
                    return code;
                scaled_y += dy;
            }
        }
    }
    if (abs_interp_limit > 1) {
        pss->params.scale_dda.x = save_x_dda;	/* reset X to start of line */
    }
    /*if_debug1m('w', penum->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
    return 0;
}

static int irii_inner_32bpp_4spp_1abs(gx_image_enum * penum, int xo, int xe, int spp_cm, unsigned short *p_cm_interp, gx_device *dev, int abs_interp_limit, int bpp, int raster, int yo, int dy, gs_logical_operation_t lop)
{
    int x;
    gx_device_color devc;
    gx_color_index color;
    byte *out = penum->line;
    byte *l_dptr = out;
    int l_xprev = (xo);
    int code;
    int ry = yo + penum->line_xy * dy;

    for (x = xo; x < xe;) {

#ifdef DEBUG
        if (gs_debug_c('B')) {
            int ci;

            for (ci = 0; ci < 3; ++ci)
                dmprintf2(dev->memory, "%c%04x", (ci == 0 ? ' ' : ','),
                    p_cm_interp[ci]);
        }
#endif
        /* Get the device color */
        get_device_color(penum, p_cm_interp, &devc, &color, dev);
        if (color_is_pure(&devc)) {
            gx_color_index color = devc.colors.pure;

            /* Just pack colors into a scan line. */
            /* Skip runs quickly for the common cases. */
            do {
                *l_dptr++ = (byte)(color >> 24);
                *l_dptr++ = (byte)(color >> 16);
                *l_dptr++ = (byte)(color >> 8);
                *l_dptr++ = (byte)(color);
                x++, p_cm_interp += 4;
            } while (x < xe && p_cm_interp[-4] == p_cm_interp[0] &&
                               p_cm_interp[-3] == p_cm_interp[1] &&
                               p_cm_interp[-2] == p_cm_interp[2] &&
                               p_cm_interp[-1] == p_cm_interp[3]);
        }
        else {
            int rep = 0;

            /* do _COPY in case any pure colors were accumulated above*/
            if (x > l_xprev) {
                code = (*dev_proc(dev, copy_color))
                    (dev, out, l_xprev - xo, raster,
                        gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                if (code < 0)
                    return code;
            }
            /* as above, see if we can accumulate any runs */
            do {
                rep++, p_cm_interp += 4;
            } while ((rep + x) < xe && p_cm_interp[-4] == p_cm_interp[0] &&
                                       p_cm_interp[-3] == p_cm_interp[1] &&
                                       p_cm_interp[-2] == p_cm_interp[2] &&
                                       p_cm_interp[-1] == p_cm_interp[3]);
            code = gx_fill_rectangle_device_rop(x, ry, rep, 1, &devc, dev, lop);
            if (code < 0)
                return code;
            x += rep;
            l_xprev = x;
            l_dptr += 4 * rep;
        }
    }  /* End on x loop */
    if (x > l_xprev) {
        code = (*dev_proc(dev, copy_color))
            (dev, out, l_xprev - xo, raster,
                gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
        if (code < 0)
            return code;
    }
    /*if_debug1m('w', penum->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
    return 0;
}

static int irii_inner_24bpp_3spp_1abs(gx_image_enum * penum, int xo, int xe, int spp_cm, unsigned short *p_cm_interp, gx_device *dev, int abs_interp_limit, int bpp, int raster, int yo, int dy, gs_logical_operation_t lop)
{
    int x;
    gx_device_color devc;
    gx_color_index color;
    byte *out = penum->line;
    byte *l_dptr = out;
    int l_xprev = (xo);
    int code;
    int ry = yo + penum->line_xy * dy;

    for (x = xo; x < xe;) {
#ifdef DEBUG
        if (gs_debug_c('B')) {
            int ci;

            for (ci = 0; ci < 3; ++ci)
                dmprintf2(dev->memory, "%c%04x", (ci == 0 ? ' ' : ','),
                    p_cm_interp[ci]);
        }
#endif
        /* Get the device color */
        get_device_color(penum, p_cm_interp, &devc, &color, dev);
        if (color_is_pure(&devc)) {
            gx_color_index color = devc.colors.pure;

            /* Just pack colors into a scan line. */
            /* Skip runs quickly for the common cases. */
            do {
                *l_dptr++ = (byte)(color >> 16);
                *l_dptr++ = (byte)(color >> 8);
                *l_dptr++ = (byte)(color);
                x++, p_cm_interp += 3;
            } while (x < xe && p_cm_interp[-3] == p_cm_interp[0] &&
                               p_cm_interp[-2] == p_cm_interp[1] &&
                               p_cm_interp[-1] == p_cm_interp[2]);
        }
        else {
            int rep = 0;

            /* do _COPY in case any pure colors were accumulated above*/
            if (x > l_xprev) {
                code = (*dev_proc(dev, copy_color))
                    (dev, out, l_xprev - xo, raster,
                        gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                if (code < 0)
                    return code;
            }
            /* as above, see if we can accumulate any runs */
            do {
                rep++, p_cm_interp += 3;
            } while ((rep + x) < xe && p_cm_interp[-3] == p_cm_interp[0] &&
                p_cm_interp[-2] == p_cm_interp[1] &&
                p_cm_interp[-1] == p_cm_interp[2]);
            code = gx_fill_rectangle_device_rop(x, ry, rep, 1, &devc, dev, lop);
            if (code < 0)
                return code;
            x += rep;
            l_xprev = x;
            l_dptr += 3 * rep;
        }
    }  /* End on x loop */
    if (x > l_xprev) {
        code = (*dev_proc(dev, copy_color))
            (dev, out, l_xprev - xo, raster,
                gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
        if (code < 0)
            return code;
    }
    /*if_debug1m('w', penum->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
    return 0;
}

static int irii_inner_8bpp_1spp_1abs(gx_image_enum * penum, int xo, int xe, int spp_cm, unsigned short *p_cm_interp, gx_device *dev, int abs_interp_limit, int bpp, int raster, int yo, int dy, gs_logical_operation_t lop)
{
    int x;
    gx_device_color devc;
    gx_color_index color;
    byte *out = penum->line;
    byte *l_dptr = out;
    int l_xprev = (xo);
    int code;
    int ry = yo + penum->line_xy * dy;

    for (x = xo; x < xe;) {
#ifdef DEBUG
        if (gs_debug_c('B')) {
            int ci;

            for (ci = 0; ci < 3; ++ci)
                dmprintf2(dev->memory, "%c%04x", (ci == 0 ? ' ' : ','),
                    p_cm_interp[ci]);
        }
#endif
        /* Get the device color */
        get_device_color(penum, p_cm_interp, &devc, &color, dev);
        if (color_is_pure(&devc)) {
            gx_color_index color = devc.colors.pure;

            /* Just pack colors into a scan line. */
            /* Skip runs quickly for the common cases. */
            do {
                *l_dptr++ = (byte)(color);
                x++, p_cm_interp++;
            } while (x < xe && p_cm_interp[-1] == p_cm_interp[0]);
        }
        else {
            int rep = 0;

            /* do _COPY in case any pure colors were accumulated above*/
            if (x > l_xprev) {
                code = (*dev_proc(dev, copy_color))
                    (dev, out, l_xprev - xo, raster,
                        gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
                if (code < 0)
                    return code;
            }
            /* as above, see if we can accumulate any runs */
            do {
                rep++, p_cm_interp++;
            } while ((rep + x) < xe && p_cm_interp[-1] == p_cm_interp[0]);
            code = gx_fill_rectangle_device_rop(x, ry, rep, 1, &devc, dev, lop);
            if (code < 0)
                return code;
            x += rep;
            l_xprev = x;
            l_dptr += rep;
        }
    }  /* End on x loop */
    if (x > l_xprev) {
        code = (*dev_proc(dev, copy_color))
            (dev, out, l_xprev - xo, raster,
                gx_no_bitmap_id, l_xprev, ry, x - l_xprev, 1);
        if (code < 0)
            return code;
    }
    /*if_debug1m('w', penum->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
    return 0;
}

static int irii_inner_generic(gx_image_enum * penum, int xo, int xe, int spp_cm, unsigned short *p_cm_interp, gx_device *dev, int abs_interp_limit, int bpp, int raster, int yo, int dy, gs_logical_operation_t lop)
{
    return irii_inner_template(penum, xo, xe, spp_cm, p_cm_interp, dev, abs_interp_limit, bpp, raster, yo, dy, lop);
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
    int abs_interp_limit = pss->params.abs_interp_limit;
    int limited_PatchWidthOut = (pss->params.PatchWidthOut + abs_interp_limit - 1) / abs_interp_limit;

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
        int width = (pss->params.WidthOut + abs_interp_limit - 1) / abs_interp_limit;
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
        int code;
        cmm_dev_profile_t *dev_profile;
        int num_bytes_decode = pss->params.BitsPerComponentIn / 8;
        irii_core_fn irii_core;

        code = dev_proc(dev, get_profile)(dev, &dev_profile);
        if (code)
            return code;
        spp_cm = gsicc_get_device_profile_comps(dev_profile);
        if (penum->matrix.yy > 0)
            dy = 1;
        else
            dy = -1, yo--;
        if (spp_cm == 4 && abs_interp_limit == 1 && bpp == 32)
            irii_core = &irii_inner_32bpp_4spp_1abs;
        else if (spp_cm == 3 && abs_interp_limit == 1 && bpp == 24)
            irii_core = &irii_inner_24bpp_3spp_1abs;
        else if (spp_cm == 1 && abs_interp_limit == 1 && bpp == 8)
            irii_core = &irii_inner_8bpp_1spp_1abs;
        else
            irii_core = &irii_inner_generic;

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
                              1, limited_PatchWidthOut);
                gsicc_init_buffer(&output_buff_desc, spp_cm, 2,
                              false, false, false, 0, width * spp_cm,
                              1, limited_PatchWidthOut);
            }
        }
        for (;;) {
            const unsigned short *pinterp;
            int status;

            stream_w.limit = out + pss->params.WidthOut *
                max(spp_decode * sizeofPixelOut, ARCH_SIZEOF_COLOR_INDEX) - 1;
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
                int xe = xo + limited_PatchWidthOut;

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
                    p_cm_interp += (pss->params.LeftMarginOut / abs_interp_limit) * spp_cm;
                } else {
                    /* Transform */
                    pinterp += (pss->params.LeftMarginOut / abs_interp_limit) * spp_decode;
                    p_cm_interp = (unsigned short *) p_cm_buff;
                    p_cm_interp += (pss->params.LeftMarginOut / abs_interp_limit) * spp_cm;
                    (penum->icc_link->procs.map_buffer)(dev, penum->icc_link,
                                                        &input_buff_desc,
                                                        &output_buff_desc,
                                                        (void*) pinterp,
                                                        (void*) p_cm_interp);
                }
                code = irii_core(penum, xo, xe, spp_cm, p_cm_interp, dev, abs_interp_limit, bpp, raster, yo, dy, lop);
                if (code < 0)
                    return code;
inactive:
                penum->line_xy++;
                if (abs_interp_limit > 1) {
                    dda_next(pss->params.scale_dda.y);
                }
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
    int abs_interp_limit = pss->params.abs_interp_limit;
    const gs_color_space *pconc;
    cmm_dev_profile_t *dev_profile;
    int code = dev_proc(penum->dev, get_profile)(penum->dev, &dev_profile);

    if (code < 0)
        return code;

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
        int width = (pss->params.WidthOut + abs_interp_limit - 1) / abs_interp_limit;
        int sizeofPixelOut = pss->params.BitsPerComponentOut / 8;
        int dy;
        color_handler_fn *color_handler = NULL;

        if (penum->matrix.yx > 0)
            dy = 1;
        else
            dy = -1, yo--;
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            int x;
            const frac *psrc;
            gx_device_color devc;
            int status;
            int scaled_w = 0;
            gx_dda_fixed save_x_dda;

            if (abs_interp_limit > 1) {
                save_x_dda = pss->params.scale_dda.x;
            }
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
                int xe = xo + (pss->params.PatchWidthOut + abs_interp_limit - 1) / abs_interp_limit;
                int scaled_h = 0;
                int scaled_y = 0;

                if (abs_interp_limit > 1) {
                    scaled_h = interpolate_scaled_expanded_height(1, pss);
                    scaled_y = yo + (dy * dda_current(pss->params.scale_dda.y)) -
                               ((dy < 0) ? (scaled_h - 1) : 0);
                }

                /* Are we active? (i.e. in the render rectangle) */
                if (!pss->params.Active)
                    goto inactive;
                if_debug1m('B', penum->memory, "[B]Interpolated (rotated) row %d:\n[B]",
                           penum->line_xy);
                psrc += (pss->params.LeftMarginOut / abs_interp_limit) * spp_decode;
                if (color_handler == NULL)
                    color_handler = get_color_handler(penum, spp_decode, islab, dev_profile, &pconc);
                for (x = xo; x < xe;) {
                    if (color_handler != NULL) {
#ifdef DEBUG
                        if (gs_debug_c('B')) {
                            int ci;

                            for (ci = 0; ci < spp_decode; ++ci)
                                dmprintf2(dev->memory, "%c%04x", (ci == 0 ? ' ' : ','),
                                          psrc[ci]);
                        }
#endif
                        code = color_handler(penum, psrc, &devc, dev, dev_profile, pconc);
                        if (code < 0)
                            return code;
                    }
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
                        if (abs_interp_limit <= 1) {
                            rcode = gx_fill_rectangle_device_rop(ry, x, 1, rep, &devc, dev, lop);
                            if (rcode < 0)
                                return rcode;
                        } else {
                            int scaled_x = xo + dda_current(pss->params.scale_dda.x);

                            scaled_w = interpolate_scaled_expanded_width(rep, pss);
                            rcode = gx_fill_rectangle_device_rop(scaled_y, scaled_x, scaled_h, scaled_w,
                                                                 &devc, dev, lop);
                            if (rcode < 0)
                                return rcode;
                            dda_advance(pss->params.scale_dda.x, rep);
                        }
                        x += rep;
                    }
                }
                /*if_debug1m('w', dev->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
inactive:
                penum->line_xy++;
                if (abs_interp_limit > 1) {
                    dda_next(pss->params.scale_dda.y);
                    pss->params.scale_dda.x = save_x_dda;	/* reset X to start of line */
                }
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
    int abs_interp_limit = pss->params.abs_interp_limit;
    int limited_PatchWidthOut = (pss->params.PatchWidthOut + abs_interp_limit - 1) / abs_interp_limit;

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
        int width = (pss->params.WidthOut + abs_interp_limit - 1) / abs_interp_limit;
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
                              1, limited_PatchWidthOut);
                gsicc_init_buffer(&output_buff_desc, spp_cm, 2,
                              false, false, false, 0, width * spp_cm,
                              1, limited_PatchWidthOut);
            }
        }
        for (;;) {
            int ry = yo + penum->line_xy * dy;
            int x;
            const unsigned short *pinterp;
            gx_device_color devc;
            int status;
            int scaled_w = 0;
            gx_dda_fixed save_x_dda;

            if (abs_interp_limit > 1) {
                save_x_dda = pss->params.scale_dda.x;
            }
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
                int xe = xo + (pss->params.PatchWidthOut + abs_interp_limit - 1) / abs_interp_limit;
                int scaled_h = 0;
                int scaled_y = 0;

                if (abs_interp_limit > 1) {
                    scaled_h = interpolate_scaled_expanded_height(1, pss);
                    scaled_y = yo + (dy * dda_current(pss->params.scale_dda.y)) -
                               ((dy < 0) ? (scaled_h - 1) : 0);
                }

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
                    pinterp += (pss->params.LeftMarginOut / abs_interp_limit) * spp_decode;
                    p_cm_interp = (unsigned short *) p_cm_buff;
                    p_cm_interp += (pss->params.LeftMarginOut / abs_interp_limit) * spp_cm;
                    (penum->icc_link->procs.map_buffer)(dev, penum->icc_link,
                                                        &input_buff_desc,
                                                        &output_buff_desc,
                                                        (void*) pinterp,
                                                        (void*) p_cm_interp);
                }
                p_cm_interp += (pss->params.LeftMarginOut / abs_interp_limit) * spp_cm;
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

                        if (abs_interp_limit <= 1) {
                            rcode = gx_fill_rectangle_device_rop(ry, x, 1, rep, &devc, dev, lop);
                            if (rcode < 0)
                                return rcode;
                        } else {
                            int scaled_x = xo + dda_current(pss->params.scale_dda.x);

                            scaled_w = interpolate_scaled_expanded_width(rep, pss);
                            rcode = gx_fill_rectangle_device_rop(scaled_y, scaled_x, scaled_h, scaled_w,
                                                                 &devc, dev, lop);
                            if (rcode < 0)
                                return rcode;
                            dda_advance(pss->params.scale_dda.x, rep);
                        }
                        x += rep;
                    }
                }  /* End on x loop */
                /*if_debug1m('w', penum->memory, "[w]Y=%d:\n", ry);*/ /* See siscale.c about 'w'. */
inactive:
                penum->line_xy++;
                if (abs_interp_limit > 1) {
                    dda_next(pss->params.scale_dda.y);
                    pss->params.scale_dda.x = save_x_dda;	/* reset X to start of line */
                }
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
