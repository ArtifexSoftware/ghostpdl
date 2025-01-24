/* Copyright (C) 2001-2025 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Image compression for PostScript and PDF writers */
#include "stdio_.h"		/* for jpeglib.h */
#include "jpeglib_.h"		/* for sdct.h */
#include "math_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"
#include "gdevpsdf.h"
#include "gdevpsds.h"
#include "gxdevmem.h"
#include "gxcspace.h"
#include "gsparamx.h"
#include "strimpl.h"
#include "scfx.h"
#include "slzwx.h"
#include "spngpx.h"
#include "szlibx.h"
#include "gsicc_manage.h"
#include "sisparam.h"

/* Define parameter-setting procedures. */
extern stream_state_proc_put_params(s_CF_put_params, stream_CF_state);
extern stream_template s_IScale_template;

/* ---------------- Image compression ---------------- */

/*
 * Add a filter to expand or reduce the pixel width if needed.
 * At least one of bpc_in and bpc_out is 8; the other is 1, 2, 4, or 8,
 * except if bpc_out is 8, bpc_in may be 12 (or 16).
 */
static int
pixel_resize(psdf_binary_writer * pbw, int width, int num_components,
             int bpc_in, int bpc_out)
{
    gs_memory_t *mem = pbw->dev->v_memory;
    const stream_template *templat;
    stream_1248_state *st;
    int code;

    if (bpc_out == bpc_in)
        return 0;
    if (bpc_in != 8) {
        static const stream_template *const exts[17] = {
            0, &s_1_8_template, &s_2_8_template, 0, &s_4_8_template,
            0, 0, 0, 0, 0, 0, 0, &s_12_8_template, 0, 0, 0, &s_16_8_template
        };

        templat = exts[bpc_in];
    } else {
        static const stream_template *const rets[5] = {
            0, &s_8_1_template, &s_8_2_template, 0, &s_8_4_template
        };

        templat = rets[bpc_out];
    }
    st = (stream_1248_state *)
        s_alloc_state(mem, templat->stype, "pixel_resize state");
    if (st == 0)
        return_error(gs_error_VMerror);
    code = psdf_encode_binary(pbw, templat, (stream_state *) st);
    if (code < 0) {
        gs_free_object(mem, st, "pixel_resize state");
        return code;
    }
    s_1248_init(st, width, num_components);
    return 0;
}

static int
convert_color(gx_device *pdev, const gs_color_space *pcs, const gs_gstate * pgs,
              gs_client_color *cc, float c[3])
{
    int code;
    gx_device_color dc;

    cs_restrict_color(cc, pcs);
    code = pcs->type->remap_color(cc, pcs, &dc, pgs, pdev, gs_color_select_texture);
    if (code < 0)
        return code;
    c[0] = (float)((int)(dc.colors.pure >> pdev->color_info.comp_shift[0]) & ((1 << pdev->color_info.comp_bits[0]) - 1));
    c[1] = (float)((int)(dc.colors.pure >> pdev->color_info.comp_shift[1]) & ((1 << pdev->color_info.comp_bits[1]) - 1));
    c[2] = (float)((int)(dc.colors.pure >> pdev->color_info.comp_shift[2]) & ((1 << pdev->color_info.comp_bits[2]) - 1));
    return 0;
}

/* A heuristic choice of DCT compression parameters - see bug 687174. */
static int
choose_DCT_params(gx_device *pdev, const gs_color_space *pcs,
                  const gs_gstate * pgs,
                  gs_c_param_list *list, gs_c_param_list **param,
                  stream_state *st)
{
    gx_device_memory mdev;
    gs_client_color cc;
    int code;
    float c[4][3];
    const float MIN_FLOAT = - MAX_FLOAT;
    const float domination = (float)0.25;
    const int one = 1, zero = 0;

    if (pcs->type->num_components(pcs) != 3)
        return 0;
    if (*param != NULL) {
        /* Make a copy of the parameter list since we will modify it. */
        code = param_list_copy((gs_param_list *)list, (gs_param_list *)*param);
        if (code < 0)
            return code;
    }
    *param = list;

    /* Create a local memory device for transforming colors to DeviceRGB. */
    gs_make_mem_device(&mdev, gdev_mem_device_for_bits(24), pdev->memory, 0, NULL);
    gx_device_retain((gx_device *)&mdev, true);	/* prevent freeing */
    set_linear_color_bits_mask_shift((gx_device *)&mdev);
    mdev.color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    /* Set mem device icc profile */
    code = gsicc_init_device_profile_struct((gx_device *) &mdev, NULL, 0);
    if (code < 0)
        return code;

    if (pgs) {
        /* Check for an RGB-like color space.
           To recognize that we make a matrix as it were a linear operator,
           suppress an ununiformity by subtracting the image of {0,0,0},
           and then check for giagonal domination.  */
        cc.paint.values[0] = cc.paint.values[1] = cc.paint.values[2] = MIN_FLOAT;
        code = convert_color((gx_device *)&mdev, pcs, pgs, &cc, c[3]);
        if (code < 0)
            return code;
        cc.paint.values[0] = MAX_FLOAT; cc.paint.values[1] = MIN_FLOAT; cc.paint.values[2] = MIN_FLOAT;
        code = convert_color((gx_device *)&mdev, pcs, pgs, &cc, c[0]);
        if (code < 0)
            return code;
        cc.paint.values[0] = MIN_FLOAT; cc.paint.values[1] = MAX_FLOAT; cc.paint.values[2] = MIN_FLOAT;
        code = convert_color((gx_device *)&mdev, pcs, pgs, &cc, c[1]);
        if (code < 0)
            return code;
        cc.paint.values[0] = MIN_FLOAT; cc.paint.values[1] = MIN_FLOAT; cc.paint.values[2] = MAX_FLOAT;
        code = convert_color((gx_device *)&mdev, pcs, pgs, &cc, c[2]);
        if (code < 0)
            return code;
        c[0][0] -= c[3][0]; c[0][1] -= c[3][1]; c[0][2] -= c[3][2];
        c[1][0] -= c[3][0]; c[1][1] -= c[3][1]; c[1][2] -= c[3][2];
        c[2][0] -= c[3][0]; c[2][1] -= c[3][1]; c[2][2] -= c[3][2];
        c[0][0] = any_abs(c[0][0]); c[0][1] = any_abs(c[0][1]); c[0][2] = any_abs(c[0][2]);
        c[1][0] = any_abs(c[1][0]); c[1][1] = any_abs(c[1][1]); c[1][2] = any_abs(c[1][2]);
        c[2][0] = any_abs(c[2][0]); c[2][1] = any_abs(c[2][1]); c[2][2] = any_abs(c[2][2]);
        if (c[0][0] * domination > c[0][1] && c[0][0] * domination > c[0][2] &&
            c[1][1] * domination > c[1][0] && c[1][1] * domination > c[1][2] &&
            c[2][2] * domination > c[2][0] && c[2][2] * domination > c[2][1]) {
            /* Yes, it looks like an RGB color space.
               Replace ColorTransform with 1. */
            code = param_write_int((gs_param_list *)list, "ColorTransform", &one);
            if (code < 0)
                goto error;
            goto done;
        }

        /* Check for a Lab-like color space.
           Colors {v,0,0} should map to grays. */
        cc.paint.values[0] = MAX_FLOAT; cc.paint.values[1] = cc.paint.values[2] = 0;
        convert_color((gx_device *)&mdev, pcs, pgs, &cc, c[0]);
        cc.paint.values[0] /= 2;
        convert_color((gx_device *)&mdev, pcs, pgs, &cc, c[1]);
        cc.paint.values[0] /= 2;
        convert_color((gx_device *)&mdev, pcs, pgs, &cc, c[2]);
        c[0][1] -= c[0][0]; c[0][2] -= c[0][0];
        c[1][1] -= c[1][0]; c[1][2] -= c[1][0];
        c[2][1] -= c[2][0]; c[2][2] -= c[2][0];
        c[0][1] = any_abs(c[0][1]); c[0][2] = any_abs(c[0][2]);
        c[1][1] = any_abs(c[1][1]); c[1][2] = any_abs(c[1][2]);
        c[2][1] = any_abs(c[2][1]); c[2][2] = any_abs(c[2][2]);
    }
    if (pgs && c[0][0] * domination > c[0][1] && c[0][0] * domination > c[0][2] &&
        c[1][0] * domination > c[1][1] && c[1][0] * domination > c[1][2] &&
        c[2][0] * domination > c[2][1] && c[2][0] * domination > c[2][2]) {
        /* Yes, it looks like an Lab color space.
           Replace ColorTransform with 0. */
        code = param_write_int((gs_param_list *)list, "ColorTransform", &zero);
        if (code < 0)
            goto error;
    } else {
        /* Unknown color space type.
           Replace /HSamples [1 1 1 1] /VSamples [1 1 1 1] to avoid quality degradation. */
        gs_param_string a;
        static const byte v[4] = {1, 1, 1, 1};

        a.data = v;
        a.size = 4;
        a.persistent = true;
        code = param_write_string((gs_param_list *)list, "HSamples", &a);
        if (code < 0)
            goto error;
        code = param_write_string((gs_param_list *)list, "VSamples", &a);
        if (code < 0)
            goto error;
    }
done:
    gs_c_param_list_read(list);
    gx_device_finalize(pdev->memory, &mdev);
    return 0;
error:
    gx_device_finalize(pdev->memory, &mdev);
    return code;
}

/* Add the appropriate image compression filter, if any. */
static int
setup_image_compression(psdf_binary_writer *pbw, const psdf_image_params *pdip,
                        const gs_pixel_image_t * pim, const gs_gstate * pgs,
                        bool lossless)
{
    gx_device_psdf *pdev = pbw->dev;
    gs_memory_t *mem = pdev->v_memory;
    const stream_template *templat = pdip->filter_template;
    const stream_template *lossless_template =
        (pdev->params.UseFlateCompression &&
         pdev->version >= psdf_version_ll3 ?
         &s_zlibE_template : &s_LZWE_template);
    const gs_color_space *pcs = pim->ColorSpace; /* null if mask */
    int Colors = (pcs ? gs_color_space_num_components(pcs) : 1);
    bool Indexed =
        (pcs != 0 &&
         gs_color_space_get_index(pcs) == gs_color_space_index_Indexed);
    gs_c_param_list *dict = pdip->Dict;
    stream_state *st;
    int code;

    if (!pdip->Encode)		/* no compression */
        return 0;
    if (pdip->AutoFilter) {
        /*
         * Disregard the requested filter.  What we should do at this point
         * is analyze the image to decide whether to use JPEG encoding
         * (DCTEncode with ACSDict) or the lossless filter.  However, since
         * we don't buffer the entire image, we'll make the choice on-fly,
         * forking the image data into 3 streams : (1) JPEG, (2) lossless,
         * (3) the compression chooser. In this case this function is
         * called 2 times with different values of the 'lossless' argument.
         */
        if (lossless) {
            templat = lossless_template;
        } else if (templat == NULL || templat == &s_zlibE_template ||
                   templat == &s_LZWE_template) {
            templat = &s_DCTE_template;
        }
        dict = pdip->ACSDict;
    } else if (!lossless)
      return_error(gs_error_rangecheck); /* Reject the alternative stream. */
    if (pdev->version < psdf_version_ll3 && templat == &s_zlibE_template)
        templat = lossless_template;
    if (dict != NULL) /* Some interpreters don't supply filter parameters. */
        gs_c_param_list_read(dict);	/* ensure param list is in read mode */
    if (templat == 0 || pdev->JPEG_PassThrough || pdev->JPX_PassThrough)	/* no compression */
        return 0;
    if (pim->Width < 200 && pim->Height < 200) /* Prevent a fixed overflow. */
        if (pim->Width * pim->Height * Colors * pim->BitsPerComponent <= 160)
            return 0;  /* not worth compressing */
    /* Only use DCTE for 8-bit, non-Indexed data. */
    if (templat == &s_DCTE_template) {
        if (Indexed ||
            !(pdip->Downsample ?
              pdip->Depth == 8 ||
              (pdip->Depth == -1 && pim->BitsPerComponent == 8) :
              pim->BitsPerComponent == 8)
            ) {
            /* Use LZW/Flate instead. */
            templat = lossless_template;
        }
    }
    st = s_alloc_state(mem, templat->stype, "setup_image_compression");
    if (st == 0)
        return_error(gs_error_VMerror);

    st->templat = templat;

    if (templat->set_defaults)
        (*templat->set_defaults) (st);
    if (templat == &s_CFE_template) {
        stream_CFE_state *const ss = (stream_CFE_state *) st;

        if (pdip->Dict != 0 && pdip->filter_template == templat) {
            s_CF_put_params((gs_param_list *)pdip->Dict,
                            (stream_CF_state *)ss); /* ignore errors */
        } else {
            ss->K = -1;
            ss->BlackIs1 = true;
        }
        ss->Columns = pim->Width;
        ss->Rows = (ss->EndOfBlock ? 0 : pim->Height);
    } else if ((templat == &s_LZWE_template ||
                templat == &s_zlibE_template) &&
               pdev->version >= psdf_version_ll3) {
        int Effort = -1, Predictor = 15;

        if (templat == &s_zlibE_template) {
            gs_c_param_list *param = pdip->Dict;

            if (pdip->AutoFilter)
                param = pdip->ACSDict;

            if (param != NULL) {
                code = param_read_int((gs_param_list *)param, "Effort", &Effort);
                if (code == 0) {
                    stream_zlib_state *const ss = (stream_zlib_state *)st;
                    ss->level = Effort;
                }
                (void)param_read_int((gs_param_list *)param, "Predictor", &Predictor);
            }
        }
        /* If not Indexed, add a PNGPredictor filter. */
        if (!Indexed && Predictor >= 10) {
            code = psdf_encode_binary(pbw, templat, st);
            if (code < 0)
                goto fail;
            templat = &s_PNGPE_template;
            st = s_alloc_state(mem, templat->stype, "setup_image_compression");
            if (st == 0) {
                code = gs_note_error(gs_error_VMerror);
                goto fail;
            }
            st->templat = templat;
            if (templat->set_defaults)
                (*templat->set_defaults) (st);
            {
                stream_PNGP_state *const ss = (stream_PNGP_state *) st;

                ss->Colors = Colors;
                ss->Columns = pim->Width;
                ss->Predictor = Predictor;
            }
        }
    } else if (templat == &s_DCTE_template) {
        gs_c_param_list list, *param = dict;

        gs_c_param_list_write(&list, mem);
        code = choose_DCT_params((gx_device *)pbw->dev, pcs, pgs, &list, &param, st);
        if (code < 0) {
            gs_c_param_list_release(&list);
            return code;
        }
        code = psdf_DCT_filter((gs_param_list *)param,
                               st, pim->Width, pim->Height, Colors, pbw);
        gs_c_param_list_release(&list);
        if (code < 0)
            goto fail;
        /* psdf_DCT_filter already did the psdf_encode_binary. */
        return 0;
    }
    code = psdf_encode_binary(pbw, templat, st);
    if (code >= 0)
        return 0;
 fail:
    gs_free_object(mem, st, "setup_image_compression");
    return code;
}

/* Determine whether an image should be downsampled. */
static bool
do_downsample(const psdf_image_params *pdip, const gs_pixel_image_t *pim,
              double resolution)
{
    double factor = resolution / pdip->Resolution;

    return (pdip->Downsample && factor >= pdip->DownsampleThreshold &&
            factor <= pim->Width && factor <= pim->Height);
}

/* Add downsampling, antialiasing, and compression filters. */
/* Uses AntiAlias, Depth, DownsampleThreshold, DownsampleType, Resolution. */
/* Assumes do_downsampling() is true. */
static int
setup_downsampling(psdf_binary_writer * pbw, const psdf_image_params * pdip,
                   gs_pixel_image_t * pim, const gs_gstate * pgs,
                   double resolution, bool lossless)
{
    gx_device_psdf *pdev = pbw->dev;
    const stream_template *templat = &s_Subsample_template;
    float factor = resolution / pdip->Resolution;
    int orig_bpc = pim->BitsPerComponent;
    int orig_width = pim->Width;
    int orig_height = pim->Height;
    stream_state *st;
    int code;

    /* We can't apply anything other than a simple downsample to monochrome
     * image without turning them into greyscale images. We set the default
     * to subsample above, so just ignore it if the current image is monochtome.
     */
    if (pim->BitsPerComponent > 1) {
        switch (pdip->DownsampleType) {
            case ds_Subsample:
                templat = &s_Subsample_template;
                break;
            case ds_Average:
                templat = &s_Average_template;
                break;
            case ds_Bicubic:
                templat = &s_IScale_template;
                /* We now use the Mitchell filter instead of the 'bicubic' filter
                 * because it gives better results.
                templat = &s_Bicubic_template;
                 */
                break;
            default:
                dmprintf1(pdev->v_memory, "Unsupported downsample type %d\n", pdip->DownsampleType);
                return gs_note_error(gs_error_rangecheck);
        }

        if (pdip->DownsampleType != ds_Bicubic) {
            /* If downsample type is not bicubic, ensure downsample factor is
             * an integer if we're close to one (< 0.1) or silently switch to
             * bicubic transform otherwise. See bug #693917. */
            float rfactor = floor(factor + 0.5);
            if (fabs(rfactor-factor) < 0.1 || pim->ColorSpace->type->index == gs_color_space_index_Indexed)
                factor = rfactor;  /* round factor to nearest integer */
            else
                templat = &s_Bicubic_template;  /* switch to bicubic */
        }
    } else {
        if (pdip->DownsampleType != ds_Subsample) {
            dmprintf(pdev->memory, "The only Downsample filter for monochrome images is Subsample, ignoring request.\n");
        }
    }

    st = s_alloc_state(pdev->v_memory, templat->stype,
                       "setup_downsampling");
    if (st == 0)
        return_error(gs_error_VMerror);
    if (templat->set_defaults)
        templat->set_defaults(st);

    if (templat != &s_IScale_template)
    {
        stream_Downsample_state *const ss = (stream_Downsample_state *) st;

        ss->Colors =
            (pim->ColorSpace == 0 ? 1 /*mask*/ :
             gs_color_space_num_components(pim->ColorSpace));
        ss->WidthIn = pim->Width;
        ss->HeightIn = pim->Height;
        ss->XFactor = ss->YFactor = factor;
        ss->AntiAlias = pdip->AntiAlias;
        ss->padX = ss->padY = false; /* should be true */

        if (pim->BitsPerComponent == 1) {
            if (floor(ss->XFactor) != ss->XFactor)
                factor = ss->YFactor = ss->XFactor = floor(ss->XFactor + 0.5);
        }
        if (templat->init) {
            code = templat->init(st);
            if (code < 0) {
                dmprintf(st->memory, "Failed to initialise downsample filter, downsampling aborted\n");
                gs_free_object(pdev->v_memory, st, "setup_image_compression");
                return 0;
            }
        }
        pim->BitsPerComponent = pdip->Depth;
        pim->Width = s_Downsample_size_out(pim->Width, factor, false);
        pim->Height = s_Downsample_size_out(pim->Height, factor, false);
        gs_matrix_scale(&pim->ImageMatrix, (double)pim->Width / orig_width,
                        (double)pim->Height / orig_height,
                        &pim->ImageMatrix);
        /****** NO ANTI-ALIASING YET ******/
        if ((code = setup_image_compression(pbw, pdip, pim, pgs, lossless)) < 0 ||
            (code = pixel_resize(pbw, pim->Width, ss->Colors,
                                 8, pdip->Depth)) < 0 ||
            (code = psdf_encode_binary(pbw, templat, st)) < 0 ||
            (code = pixel_resize(pbw, orig_width, ss->Colors,
                                 orig_bpc, 8)) < 0
            ) {
            gs_free_object(pdev->v_memory, st, "setup_image_compression");
            return code;
        }
    } else {
        /* The setup for the Mitchell filter is quite different to the other filters
         * because it isn't one of ours.
         */
        int Colors = (pim->ColorSpace == 0 ? 1 /*mask*/ :
             gs_color_space_num_components(pim->ColorSpace));

        stream_image_scale_state *ss = (stream_image_scale_state *)st;

        ss->params.EntireWidthIn = ss->params.WidthIn = ss->params.PatchWidthIn = pim->Width;
        ss->params.EntireHeightIn = ss->params.HeightIn = ss->params.PatchHeightIn = pim->Height;
        ss->params.EntireWidthOut = ss->params.WidthOut = ss->params.PatchWidthOut = s_Downsample_size_out(pim->Width, factor, false);
        ss->params.EntireHeightOut = ss->params.HeightOut = ss->params.PatchHeightOut = ss->params.PatchHeightOut2 = s_Downsample_size_out(pim->Height, factor, false);

        /* Bug #697944 The code below to apply the downsampling filter always
         * resizes the input data to the filter with 8BPC and then resizes the output back to whatever
         * the original BPC was. So we need to make sure that the stream state
         * for the downsampling filter uses 8 BPC, no more and no less.
         */
        ss->params.BitsPerComponentIn = ss->params.BitsPerComponentOut = 8;

        ss->params.spp_interp = ss->params.spp_decode = Colors;
        ss->params.TopMarginIn = ss->params.TopMarginOut = ss->params.TopMarginOut2 = ss->params.LeftMarginIn = ss->params.LeftMarginOut = 0;
        ss->params.src_y_offset = ss->params.pad_y = 0;
        ss->params.early_cm = true;
        ss->params.MaxValueIn = ss->params.MaxValueOut = (int)pow(2, pdip->Depth);

        /* No idea what's a sensible value here, but we need to have something or we get a crash
         * It looks like this is for scaling up, and we don't do that, so fix it to 1. Parameter
         * Added by Ray in commit a936cf for Bug #693684, allows limiting interpolation to less#
         * than device resolution.
         */
        ss->params.abs_interp_limit = 1;
        /* Apparently ColorPolairtyAdditive is only used by the 'SpecialDownScale filter', don't
         * know what that is and we don't use it, so just set it to 0 to avoid uninitialised
         * variables
         */
        ss->params.ColorPolarityAdditive = 0;
        /* Active = 1 to match gxiscale.c, around line 374 in gs_image_class_0_interpolate() */
        ss->params.Active = 1;

        if (templat->init) {
            code = templat->init(st);
            if (code < 0) {
                dmprintf(st->memory, "Failed to initialise downsample filter, downsampling aborted\n");
                gs_free_object(pdev->v_memory, st, "setup_image_compression");
                return 0;
            }
        }
        pim->Width = s_Downsample_size_out(pim->Width, factor, false);
        pim->Height = s_Downsample_size_out(pim->Height, factor, false);
        pim->BitsPerComponent = pdip->Depth;
        gs_matrix_scale(&pim->ImageMatrix, (double)pim->Width / orig_width,
                        (double)pim->Height / orig_height,
                        &pim->ImageMatrix);
        /****** NO ANTI-ALIASING YET ******/
        if ((code = setup_image_compression(pbw, pdip, pim, pgs, lossless)) < 0 ||
            (code = pixel_resize(pbw, pim->Width, Colors,
                                 8, pdip->Depth)) < 0 ||
            (code = psdf_encode_binary(pbw, templat, st)) < 0 ||
            (code = pixel_resize(pbw, orig_width, Colors,
                                 orig_bpc, 8)) < 0
            ) {
            gs_free_object(pdev->v_memory, st, "setup_image_compression");
            return code;
        }
    }
    return 0;
}

/* Decive whether to convert an image to RGB. */
bool
psdf_is_converting_image_to_RGB(const gx_device_psdf * pdev,
                const gs_gstate * pgs, const gs_pixel_image_t * pim)
{
    return pdev->params.ConvertCMYKImagesToRGB &&
            pgs != 0 && pim->ColorSpace &&
            (gs_color_space_get_index(pim->ColorSpace) == gs_color_space_index_DeviceCMYK ||
            (gs_color_space_get_index(pim->ColorSpace) == gs_color_space_index_ICC
            && gsicc_get_default_type(pim->ColorSpace->cmm_icc_profile_data) ==
            gs_color_space_index_DeviceCMYK));
}

/* Set up compression and downsampling filters for an image. */
/* Note that this may modify the image parameters. */
int
psdf_setup_image_filters(gx_device_psdf * pdev, psdf_binary_writer * pbw,
                         gs_pixel_image_t * pim, const gs_matrix * pctm,
                         const gs_gstate * pgs, bool lossless, bool in_line)
{
    /*
     * The following algorithms are per Adobe Tech Note # 5151,
     * "Acrobat Distiller Parameters", revised 16 September 1996
     * for Acrobat(TM) Distiller(TM) 3.0.
     *
     * The control structure is a little tricky, because filter
     * pipelines must be constructed back-to-front.
     */
    int code = 0;
    psdf_image_params params;
    int bpc = pim->BitsPerComponent;
    int bpc_out = pim->BitsPerComponent = min(bpc, 8);
    int ncomp;
    double resolution;

    /*
     * The Adobe documentation doesn't say this, but mask images are
     * compressed on the same basis as 1-bit-deep monochrome images,
     * except that anti-aliasing (resolution/depth tradeoff) is not
     * allowed.
     */
    if (pim->ColorSpace == NULL) { /* mask image */
        params = pdev->params.MonoImage;
        params.Depth = 1;
        ncomp = 1;
    } else {
        ncomp = gs_color_space_num_components(pim->ColorSpace);
        if (pim->ColorSpace->type->index == gs_color_space_index_Indexed) {
            params = pdev->params.ColorImage;
            /* Ensure we don't use JPEG on a /Indexed colour space */
            params.AutoFilter = false;
            params.Filter = "FlateEncode";
        } else {
            if (ncomp == 1) {
                if (bpc == 1)
                    params = pdev->params.MonoImage;
                else
                    params = pdev->params.GrayImage;
                if (params.Depth == -1)
                    params.Depth = bpc;
            } else {
                params = pdev->params.ColorImage;
            /* params.Depth is reset below */
            }
        }
    }

    /*
     * We can compute the image resolution by:
     *    W / (W * ImageMatrix^-1 * CTM / HWResolution).
     * We can replace W by 1 to simplify the computation.
     */
    if (pctm == 0)
        resolution = -1;
    else {
        gs_point pt;

        /* We could do both X and Y, but why bother? */
        code = gs_distance_transform_inverse(1.0, 0.0, &pim->ImageMatrix, &pt);
        if (code < 0)
            return code;
        gs_distance_transform(pt.x, pt.y, pctm, &pt);
        resolution = 1.0 / hypot(pt.x / pdev->HWResolution[0],
                                 pt.y / pdev->HWResolution[1]);
    }
    if (ncomp == 1 && pim->ColorSpace && pim->ColorSpace->type->index != gs_color_space_index_Indexed) {
        /* Monochrome, gray, or mask */
        /* Check for downsampling. */
        if (do_downsample(&params, pim, resolution)) {
            /* Use the downsampled depth, not the original data depth. */
            if (params.Depth == 1) {
                params.Filter = pdev->params.MonoImage.Filter;
                params.filter_template = pdev->params.MonoImage.filter_template;
                params.Dict = pdev->params.MonoImage.Dict;
            } else {
                params.Filter = pdev->params.GrayImage.Filter;
                params.filter_template = pdev->params.GrayImage.filter_template;
                params.Dict = pdev->params.GrayImage.Dict;
            }
            code = setup_downsampling(pbw, &params, pim, pgs, resolution, lossless);
        } else {
            code = setup_image_compression(pbw, &params, pim, pgs, lossless);
        }
        if (code < 0)
            return code;
        code = pixel_resize(pbw, pim->Width, ncomp, bpc, bpc_out);
    } else {
        /* Color */
        bool cmyk_to_rgb = psdf_is_converting_image_to_RGB(pdev, pgs, pim);

        if (cmyk_to_rgb) {
            gs_memory_t *mem = pdev->v_memory;

            /* {csrc} decref old colorspace? */
            rc_decrement_only_cs(pim->ColorSpace, "psdf_setup_image_filters");
            pim->ColorSpace = gs_cspace_new_DeviceRGB(mem);
            if (pim->ColorSpace == NULL)
                return_error(gs_error_VMerror);
        }
        if (params.Depth == -1)
            params.Depth = (cmyk_to_rgb ? 8 : bpc_out);
        if (do_downsample(&params, pim, resolution)) {
            code = setup_downsampling(pbw, &params, pim, pgs, resolution, lossless);
        } else {
            code = setup_image_compression(pbw, &params, pim, pgs, lossless);
        }
        if (code < 0)
            return code;
        if (cmyk_to_rgb) {
            gs_memory_t *mem = pdev->v_memory;
            stream_C2R_state *ss = (stream_C2R_state *)
                s_alloc_state(mem, s_C2R_template.stype, "C2R state");
            int code = pixel_resize(pbw, pim->Width, 3, 8, bpc_out);

            if (code < 0 ||
                (code = psdf_encode_binary(pbw, &s_C2R_template,
                                           (stream_state *) ss)) < 0 ||
                (code = pixel_resize(pbw, pim->Width, 4, bpc, 8)) < 0
                )
                return code;
            s_C2R_init(ss, pgs);
        } else {
            code = pixel_resize(pbw, pim->Width, ncomp, bpc, bpc_out);
            if (code < 0)
                return code;
        }
    }
    return code;
}

/* Set up compression filters for a lossless image, downsampling is permitted, */
/* no color space conversion, and only lossless filters. */
/* Note that this may modify the image parameters. */
int
psdf_setup_lossless_filters(gx_device_psdf *pdev, psdf_binary_writer *pbw,
                            gs_pixel_image_t *pim, bool in_line)
{
    /*
     * Set up a device with modified parameters for computing the image
     * compression filters.  Don't allow downsampling or lossy compression.
     */
    gx_device_psdf ipdev;

    ipdev = *pdev;
    ipdev.params.ColorImage.AutoFilter = false;
    ipdev.params.ColorImage.Filter = "FlateEncode";
    ipdev.params.ColorImage.filter_template = &s_zlibE_template;
    ipdev.params.ConvertCMYKImagesToRGB = false;
    ipdev.params.GrayImage.AutoFilter = false;
    ipdev.params.GrayImage.Filter = "FlateEncode";
    ipdev.params.GrayImage.filter_template = &s_zlibE_template;
    return psdf_setup_image_filters(&ipdev, pbw, pim, NULL, NULL, true, in_line);
}

/* Set up image compression chooser. */
int
psdf_setup_compression_chooser(psdf_binary_writer *pbw, gx_device_psdf *pdev,
                    int width, int height, int depth, int bits_per_sample)
{
    int code;
    stream_state *ss = s_alloc_state(pdev->memory, s_compr_chooser_template.stype,
                                     "psdf_setup_compression_chooser");

    if (ss == 0)
        return_error(gs_error_VMerror);
    ss->templat = &s_compr_chooser_template;

    pbw->memory = pdev->memory;
    pbw->strm = pdev->strm; /* just a stub - will not write to it. */
    pbw->dev = pdev;
    pbw->target = pbw->strm; /* Since s_add_filter may insert NullEncode to comply buffering,
                                 will need to close a chain of filetrs. */
    code = psdf_encode_binary(pbw, &s_compr_chooser_template, ss);
    if (code < 0)
        return code;
    code = s_compr_chooser_set_dimensions((stream_compr_chooser_state *)ss,
                    width, height, depth, bits_per_sample);
    return code;
}

/* Set up an "image to mask" filter. */
int
psdf_setup_image_to_mask_filter(psdf_binary_writer *pbw, gx_device_psdf *pdev,
                                int width, int height, int input_width,
                                int depth, int bits_per_sample, uint *MaskColor)
{
    int code;
    stream_state *ss = s_alloc_state(pdev->memory, s__image_colors_template.stype,
        "psdf_setup_image_colors_filter");

    if (ss == 0)
        return_error(gs_error_VMerror);
    pbw->memory = pdev->memory;
    pbw->dev = pdev;
    code = psdf_encode_binary(pbw, &s__image_colors_template, ss);
    if (code < 0)
        return code;
    s_image_colors_set_dimensions((stream_image_colors_state *)ss,
                                  width, height, input_width, depth, bits_per_sample);
    s_image_colors_set_mask_colors((stream_image_colors_state *)ss, MaskColor);
    return 0;
}

/* Set up an image colors filter. */
int
psdf_setup_image_colors_filter(psdf_binary_writer *pbw,
                               gx_device_psdf *pdev,
                               const gs_pixel_image_t *input_pim,
                               gs_pixel_image_t * pim,
                               const gs_gstate *pgs)
{   /* fixme: currently it's a stub convertion to mask. */
    int code;
    stream_state *ss = s_alloc_state(pdev->memory, s__image_colors_template.stype,
        "psdf_setup_image_colors_filter");
    unsigned char i;

    if (ss == 0)
        return_error(gs_error_VMerror);
    pbw->memory = pdev->memory;
    pbw->dev = pdev;
    code = psdf_encode_binary(pbw, &s__image_colors_template, ss);
    if (code < 0)
        return code;
    s_image_colors_set_dimensions((stream_image_colors_state *)ss,
                                  pim->Width, pim->Height, input_pim->Width,
                                  gs_color_space_num_components(pim->ColorSpace),
                                  pim->BitsPerComponent);
    s_image_colors_set_color_space((stream_image_colors_state *)ss,
                    (gx_device *)pdev, pim->ColorSpace, pgs, pim->Decode);
    pim->BitsPerComponent = pdev->color_info.comp_bits[0]; /* Same precision for all components. */
    for (i = 0; i < pdev->color_info.num_components; i++) {
        pim->Decode[i * 2 + 0] = 0;
        pim->Decode[i * 2 + 1] = 1;
    }
    return 0;
}

/* Set up compression and downsampling filters for an image. */
/* Note that this may modify the image parameters. */
int
new_setup_image_filters(gx_device_psdf * pdev, psdf_binary_writer * pbw,
                         gs_pixel_image_t * pim, const gs_matrix * pctm,
                         const gs_gstate * pgs, bool lossless, bool in_line,
                         bool colour_conversion)
{
    /*
     * The following algorithms are per Adobe Tech Note # 5151,
     * "Acrobat Distiller Parameters", revised 16 September 1996
     * for Acrobat(TM) Distiller(TM) 3.0.
     *
     * The control structure is a little tricky, because filter
     * pipelines must be constructed back-to-front.
     */
    int code = 0;
    psdf_image_params params;
    int bpc = pim->BitsPerComponent;
    int bpc_out = pim->BitsPerComponent = min(bpc, 8);
    int ncomp;
    double resolution, resolutiony;

    /*
     * The Adobe documentation doesn't say this, but mask images are
     * compressed on the same basis as 1-bit-deep monochrome images,
     * except that anti-aliasing (resolution/depth tradeoff) is not
     * allowed.
     */
    if (pim->ColorSpace == NULL) { /* mask image */
        params = pdev->params.MonoImage;
        params.Depth = 1;
        ncomp = 1;
    } else {
        ncomp = gs_color_space_num_components(pim->ColorSpace);
        if (pim->ColorSpace->type->index == gs_color_space_index_Indexed) {
            params = pdev->params.ColorImage;
            /* Ensure we don't use JPEG on a /Indexed colour space */
            params.AutoFilter = false;
            params.Filter = "FlateEncode";
        } else {
            if (ncomp == 1) {
                if (bpc == 1)
                    params = pdev->params.MonoImage;
                else
                    params = pdev->params.GrayImage;
                if (params.Depth == -1)
                    params.Depth = bpc;
            } else {
                params = pdev->params.ColorImage;
            /* params.Depth is reset below */
            }
        }
    }

    /*
     * We can compute the image resolution by:
     *    W / (W * ImageMatrix^-1 * CTM / HWResolution).
     * We can replace W by 1 to simplify the computation.
     */
    if (pctm == 0)
        resolution = -1;
    else {
        gs_point pt;

        /* We could do both X and Y, but why bother? */
        code = gs_distance_transform_inverse(1.0, 0.0, &pim->ImageMatrix, &pt);
        if (code < 0)
            return code;
        gs_distance_transform(pt.x, pt.y, pctm, &pt);
        resolution = 1.0 / hypot(pt.x / pdev->HWResolution[0],
                                 pt.y / pdev->HWResolution[1]);

        /* Actually we must do both X and Y, in case the image is ananmorphically scaled
         * and one axis is not high enough resolution to be downsampled.
         * Bug #696152
         */
        code = gs_distance_transform_inverse(0.0, 1.0, &pim->ImageMatrix, &pt);
        if (code < 0)
            return code;
        gs_distance_transform(pt.x, pt.y, pctm, &pt);
        resolutiony = 1.0 / hypot(pt.x / pdev->HWResolution[0],
                                 pt.y / pdev->HWResolution[1]);
        if (resolutiony < resolution)
            resolution = resolutiony;
    }

    if (bpc != bpc_out) {
        pdev->JPEG_PassThrough = 0;
        pdev->JPX_PassThrough = 0;
    }

    if (ncomp == 1 && pim->ColorSpace && pim->ColorSpace->type->index != gs_color_space_index_Indexed) {
        /* Monochrome, gray, or mask */
        /* Check for downsampling. */
        if (do_downsample(&params, pim, resolution)) {
            /* Use the downsampled depth, not the original data depth. */
            if (params.Depth == 1) {
                params.Filter = pdev->params.MonoImage.Filter;
                params.filter_template = pdev->params.MonoImage.filter_template;
                params.Dict = pdev->params.MonoImage.Dict;
            } else {
                if (params.Depth > 8)
                    params.Depth = bpc_out;
                params.Filter = pdev->params.GrayImage.Filter;
                params.filter_template = pdev->params.GrayImage.filter_template;
                params.Dict = pdev->params.GrayImage.Dict;
            }
            pdev->JPEG_PassThrough = 0;
            pdev->JPX_PassThrough = 0;
            code = setup_downsampling(pbw, &params, pim, pgs, resolution, lossless);
        } else {
            code = setup_image_compression(pbw, &params, pim, pgs, lossless);
        }
        if (code < 0)
            return code;
        code = pixel_resize(pbw, pim->Width, ncomp, bpc, bpc_out);
    } else {
        /* Color */
        if (params.Depth == -1)
            params.Depth = (colour_conversion ? 8 : bpc_out);
        if (do_downsample(&params, pim, resolution)) {
            pdev->JPEG_PassThrough = 0;
            pdev->JPX_PassThrough = 0;
            code = setup_downsampling(pbw, &params, pim, pgs, resolution, lossless);
        } else {
            code = setup_image_compression(pbw, &params, pim, pgs, lossless);
        }
        if (code < 0)
            return code;
        code = pixel_resize(pbw, pim->Width, ncomp, bpc, bpc_out);
        if (code < 0)
            return code;
    }
    return code;
}

int
new_setup_lossless_filters(gx_device_psdf *pdev, psdf_binary_writer *pbw,
                            gs_pixel_image_t *pim, bool in_line,
                            bool colour_conversion, const gs_matrix *pctm, gs_gstate * pgs)
{
    /*
     * Set up a device with modified parameters for computing the image
     * compression filters.  Don't allow lossy compression, but do allow downsampling.
     */
    gx_device_psdf ipdev;

    ipdev = *pdev;
    ipdev.params.ColorImage.AutoFilter = false;
    ipdev.params.ColorImage.Filter = "FlateEncode";
    ipdev.params.ColorImage.filter_template = &s_zlibE_template;
    ipdev.params.ConvertCMYKImagesToRGB = false;
    ipdev.params.GrayImage.AutoFilter = false;
    ipdev.params.GrayImage.Downsample = false;
    ipdev.params.GrayImage.Filter = "FlateEncode";
    ipdev.params.GrayImage.filter_template = &s_zlibE_template;
    return new_setup_image_filters(&ipdev, pbw, pim, pctm, pgs, true, in_line, colour_conversion);
}

int new_resize_input(psdf_binary_writer *pbw, int width, int num_comps, int bpc_in, int bpc_out)
{
    return pixel_resize(pbw, width, num_comps, bpc_in, bpc_out);
}
