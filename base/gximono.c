/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* General mono-component image rendering */
#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gsutil.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxdcolor.h"
#include "gxgstate.h"
#include "gxdevmem.h"
#include "gdevmem.h"            /* for mem_mono_device */
#include "gxcpath.h"
#include "gximage.h"
#include "gzht.h"
#include "gsicc.h"
#include "gsicc_cache.h"
#include "gsicc_cms.h"
#include "gxcie.h"
#include "gscie.h"
#include "gxht_thresh.h"
#include "gxdda.h"
#include "gxdevsop.h"
#ifdef WITH_CAL
#include "cal.h"
#endif

#define fastfloor(x) (((int)(x)) - (((x)<0) && ((x) != (float)(int)(x))))

/* Enable the following define to perform a little extra work to stop
 * spurious valgrind errors. The code should perform perfectly even without
 * this enabled, but enabling it makes debugging much easier.
 */
/* #define PACIFY_VALGRIND */

/* ------ Strategy procedure ------ */

/* Check the prototype. */
iclass_proc(gs_image_class_3_mono);

static irender_proc(image_render_mono);
#ifndef WITH_CAL
static irender_proc(image_render_mono_ht);
#else
static irender_proc(image_render_mono_ht_cal);
static int image_render_mono_ht_cal_skip_line(gx_image_enum *penum,
					      gx_device *dev);

static void
halftone_callback(cal_halftone_data_t *ht, void *arg)
{
    gx_device *dev = arg;
    gx_color_index dev_white = gx_device_white(dev);
    gx_color_index dev_black = gx_device_black(dev);

    if (dev->num_planar_planes) {
        (*dev_proc(dev, copy_planes)) (dev, ht->data, ht->x + (ht->offset_x<<3), ht->raster,
            gx_no_bitmap_id, ht->x, ht->y, ht->w, ht->h,
            ht->plane_raster);
    } else {
        (*dev_proc(dev, copy_mono)) (dev, ht->data, ht->x + (ht->offset_x<<3), ht->raster,
            gx_no_bitmap_id, ht->x, ht->y, ht->w, ht->h, dev_white,
            dev_black);
    }
}

static cal_halftone*
halftone_init(gx_image_enum *penum)
{
    cal_halftone *cal_ht = NULL;
    gx_dda_fixed dda_ht;
    int k;
    gx_ht_order *d_order;
    int code;
    byte *cache = (penum->color_cache != NULL ? penum->color_cache->device_contone : NULL);
    cal_matrix matrix;
    int clip_x, clip_y;
    gx_device_halftone *pdht = gx_select_dev_ht(penum->pgs);

    if (!gx_device_must_halftone(penum->dev))
        return NULL;

    if (penum->pgs == NULL || pdht == NULL)
        return NULL;

    dda_ht = penum->dda.pixel0.x;
    if (penum->dxx > 0)
        dda_translate(dda_ht, -fixed_epsilon);
    matrix.xx = penum->matrix.xx;
    matrix.xy = penum->matrix.xy;
    matrix.yx = penum->matrix.yx;
    matrix.yy = penum->matrix.yy;
    matrix.tx = penum->matrix.tx + matrix.xx * penum->rect.x + matrix.yx * penum->rect.y;
    matrix.ty = penum->matrix.ty + matrix.xy * penum->rect.x + matrix.yy * penum->rect.y;

    clip_x = fixed2int(penum->clip_outer.p.x);
    clip_y = fixed2int(penum->clip_outer.p.y);
    cal_ht = cal_halftone_init(penum->memory->gs_lib_ctx->core->cal_ctx,
                               penum->memory->non_gc_memory,
                               penum->rect.w,
                               penum->rect.h,
                               &matrix,
                               penum->dev->color_info.num_components,
                               cache,
                               clip_x,
                               clip_y,
                               fixed2int_ceiling(penum->clip_outer.q.x) - clip_x,
                               fixed2int_ceiling(penum->clip_outer.q.y) - clip_y,
                               penum->adjust);
    if (cal_ht == NULL)
        goto fail;

    for (k = 0; k < pdht->num_comp; k++) {
        d_order = &(pdht->components[k].corder);
        code = gx_ht_construct_threshold(d_order, penum->dev, penum->pgs, k);
        if (code < 0)
            goto fail;
        if (cal_halftone_add_screen(penum->memory->gs_lib_ctx->core->cal_ctx,
                                    penum->memory->non_gc_memory,
                                    cal_ht,
                                    pdht->components[k].corder.threshold_inverted,
                                    pdht->components[k].corder.width,
                                    pdht->components[k].corder.full_height,
                                    penum->pgs->screen_phase[k].x,
                                    -penum->pgs->screen_phase[k].y,
                                    pdht->components[k].corder.threshold) < 0)
            goto fail;
    }

    return cal_ht;

fail:
    cal_halftone_fin(cal_ht, penum->memory->non_gc_memory);
    return NULL;
}
#endif

int
gs_image_class_3_mono(gx_image_enum * penum, irender_proc_t *render_fn)
{
#if USE_FAST_HT_CODE
    bool use_fast_code = true;
#else
    bool use_fast_code = false;
#endif
    int code = 0;
    /* Set up the link now */
    const gs_color_space *pcs;
    gsicc_rendering_param_t rendering_params;
    cmm_dev_profile_t *dev_profile;
    bool dev_color_ok = false;
    bool is_planar_dev = penum->dev->num_planar_planes;

    if (penum->spp == 1) {
        /* At this point in time, only use the ht approach if our device
           uses halftoning, and our source image is a reasonable size.  We
           probably don't want to do this if we have a bunch of tiny little
           images.  Then the rect fill approach is probably not all that bad.
           Also for now avoid images that include a type3 image mask.  Due
           to the limited precision and mismatch of the stepping space in which
           the interpolations occur this can cause a minor mismatch at large
           scalings */

        /* Allow this for CMYK planar and mono binary halftoned devices */
        dev_color_ok = ((penum->dev->color_info.num_components == 1 &&
                         penum->dev->color_info.depth == 1) ||
#if 0
                         /* Don't allow CMYK Planar devices just yet */
                         0);
#else
                        (penum->dev->color_info.num_components == 4 &&
                         penum->dev->color_info.depth == 4 && is_planar_dev));
#endif

        if (use_fast_code && penum->pcs != NULL && dev_color_ok &&
            penum->bps == 8 && (penum->posture == image_portrait
            || penum->posture == image_landscape) &&
            penum->image_parent_type == gs_image_type1 &&
            gx_transfer_is_monotonic(penum->pgs, 0)) {
            penum->icc_setup.need_decode = false;
            /* Check if we need to do any decoding.  */
            if ( penum->map[0].decoding != sd_none ) {
                if (!(penum->map[0].decoding == sd_compute
                     && penum->map[0].decode_factor == 1.0 &&
                        penum->map[0].decode_lookup[0] == 0.0)) {
                    penum->icc_setup.need_decode = true;
                }
            }
            code = dev_proc(penum->dev, get_profile)(penum->dev, &dev_profile);
            if (code < 0)
                return code;

            /* Define the rendering intents */
            rendering_params.black_point_comp = penum->pgs->blackptcomp;
            rendering_params.graphics_type_tag = GS_IMAGE_TAG;
            rendering_params.override_icc = false;
            rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
            rendering_params.rendering_intent = penum->pgs->renderingintent;
            rendering_params.cmm = gsCMM_DEFAULT;
            if (gs_color_space_get_index(penum->pcs) ==
                gs_color_space_index_Indexed) {
                pcs = penum->pcs->base_space;
            } else {
                pcs = penum->pcs;
            }
            if (gs_color_space_is_PSCIE(pcs) && pcs->icc_equivalent != NULL) {
                pcs = pcs->icc_equivalent;
            }

            /* The code below falls over if cmm->icc_profile_data is NULL.
             * For now, just drop out. Michael can review this when he
             * returns. */
            if (pcs->cmm_icc_profile_data == NULL)
                goto not_fast_halftoning;

            penum->icc_setup.is_lab = pcs->cmm_icc_profile_data->islab;
            penum->icc_setup.must_halftone = gx_device_must_halftone(penum->dev);
            /* The effective transfer is built into the threshold array */
            penum->icc_setup.has_transfer = false;
            if (penum->icc_setup.is_lab)
                penum->icc_setup.need_decode = false;
            if (penum->icc_link == NULL) {
                penum->icc_link = gsicc_get_link(penum->pgs, penum->dev, pcs, NULL,
                    &rendering_params, penum->memory);
            }
            /* PS CIE color spaces may have addition decoding that needs to
               be performed to ensure that the range of 0 to 1 is provided
               to the CMM since ICC profiles are restricted to that range
               but the PS color spaces are not. */
            penum->use_cie_range = false;
            if (gs_color_space_is_PSCIE(penum->pcs) &&
                penum->pcs->icc_equivalent != NULL) {
                /* We have a PS CIE space.  Check the range */
                if ( !check_cie_range(penum->pcs) ) {
                    /* It is not 0 to 1.  We will be doing decode
                       plus an additional linear adjustment */
                    penum->use_cie_range = (get_cie_range(penum->pcs) != NULL);
                }
            }
            /* If the image has more than 256 pixels then go ahead and
               precompute the contone device colors for all of our 256 source
               values.  We should not be taking this path for cases where
               we have lots of tiny little images.  Mark those that are
               transparent or masked also at this time.  Since halftoning will
               be done via thresholding we will keep clues in continuous tone */
            code = image_init_color_cache(penum, penum->bps, penum->spp);
            if (code >= 0) {
#ifdef WITH_CAL
                penum->cal_ht = halftone_init(penum);
                if (penum->cal_ht != NULL)
                {
                    penum->skip_next_line = image_render_mono_ht_cal_skip_line;
                    *render_fn = &image_render_mono_ht_cal;
                    return code;
                }
#else
                code = gxht_thresh_image_init(penum);
                if (code >= 0) {
                    *render_fn = &image_render_mono_ht;
                    return code;
                }
#endif
            }
        }
not_fast_halftoning:
        /*
         * Use the slow loop for imagemask with a halftone or a non-default
         * logical operation.
         */
        penum->slow_loop =
            (penum->masked && !color_is_pure(penum->icolor0)) ||
            penum->use_rop;
        /* We can bypass X clipping for portrait mono-component images. */
        if (!(penum->slow_loop || penum->posture != image_portrait))
            penum->clip_image &= ~(image_clip_xmin | image_clip_xmax);
        if_debug0m('b', penum->memory, "[b]render=mono\n");
        /* Precompute values needed for rasterizing. */
        penum->dxx =
            float2fixed(penum->matrix.xx + fixed2float(fixed_epsilon) / 2);
        /*
         * Scale the mask colors to match the scaling of each sample to a
         * full byte.  Also, if black or white is transparent, reset icolor0
         * or icolor1, which are used directly in the fast case loop.
         */
        if (penum->use_mask_color) {
            gx_image_scale_mask_colors(penum, 0);
            if (penum->mask_color.values[0] <= 0)
                color_set_null(penum->icolor0);
            if (penum->mask_color.values[1] >= 255)
                color_set_null(penum->icolor1);
        }
        /* Reset the clues here, rather than in image_render_mono as
         * previously. Even doing so this often may be overzealous. */
        image_init_clues(penum, penum->bps, penum->spp);
        *render_fn = &image_render_mono;
    }
    return 0;
}

#define USE_SET_GRAY_FUNCTION 0
#if USE_SET_GRAY_FUNCTION
/* Temporary function to make it easier to debug the uber-macro below */
static inline int
image_set_gray(byte sample_value, const bool masked, uint mask_base,
                uint mask_limit, gx_device_color **ppdevc, gs_client_color *cc,
                const gs_color_space *pcs, const gs_gstate *pgs,
                gx_device * dev, gs_color_select_t gs_color_select_source,
                gx_image_enum * penum)
{
   cs_proc_remap_color((*remap_color));
   int code;
   gx_device_color *pdevc;
   pdevc = *ppdevc = &penum->clues[sample_value].dev_color;

   if (!color_is_set(pdevc)) {
       if ((uint)(sample_value - mask_base) < mask_limit) {
           color_set_null(pdevc);
       } else {
            switch ( penum->map[0].decoding )
            {
            case sd_none:
            cc->paint.values[0] = (sample_value) * (1.0f / 255.0f);  /* faster than / */
            break;
            case sd_lookup:     /* <= 4 significant bits */
            cc->paint.values[0] =
              penum->map[0].decode_lookup[(sample_value) >> 4];
            break;
            case sd_compute:
            cc->paint.values[0] =
              penum->map[0].decode_base + (sample_value) * penum->map[0].decode_factor;
            }
            remap_color = pcs->type->remap_color;
            code = (*remap_color)(cc, pcs, pdevc, pgs, dev, gs_color_select_source);
            return(code);
        }
    } else if (!color_is_pure(pdevc)) {
        code = gx_color_load_select(pdevc, pgs, dev, gs_color_select_source);
        if (code < 0)
            return(code);
    }
    return(0);
}
#endif

/*
 * Rendering procedure for general mono-component images, dealing with
 * multiple bit-per-sample images, general transformations, arbitrary
 * single-component color spaces (DeviceGray, DevicePixel, CIEBasedA,
 * Separation, Indexed), and color masking. This procedure handles a
 * single scan line.
 */
static int
image_render_mono(gx_image_enum * penum, const byte * buffer, int data_x,
                  uint w, int h, gx_device * dev)
{
    const gs_gstate *pgs = penum->pgs;
    gs_logical_operation_t lop = penum->log_op;
    const bool masked = penum->masked;
    const gs_color_space *pcs = NULL;   /* only set for non-masks */
    cs_proc_remap_color((*remap_color)) = NULL; /* ditto */
    gs_client_color cc;
    gx_device_color *pdevc = penum->icolor1;    /* color for masking */
    uint mask_base =            /* : 0 to pacify Valgrind */
        (penum->use_mask_color ? penum->mask_color.values[0] : 0);
    uint mask_limit =
        (penum->use_mask_color ?
         penum->mask_color.values[1] - mask_base + 1 : 0);
/*
 * Free variables of IMAGE_SET_GRAY:
 *   Read: penum, pgs, dev, mask_base, mask_limit
 *   Set: pdevc, code, cc
 */
#define IMAGE_SET_GRAY(sample_value)\
  BEGIN\
    pdevc = &penum->clues[sample_value].dev_color;\
    if (!color_is_set(pdevc)) {\
        if ((uint)(sample_value - mask_base) < mask_limit)\
            color_set_null(pdevc);\
        else {\
            decode_sample(sample_value, cc, 0);\
            code = (*remap_color)(&cc, pcs, pdevc, pgs, dev, gs_color_select_source);\
            if (code < 0)\
                goto err;\
            pdevc->tag = device_current_tag(dev);\
        }\
    } else if (!color_is_pure(pdevc)) {\
        code = gx_color_load_select(pdevc, pgs, dev, gs_color_select_source);\
        if (code < 0)\
            goto err;\
    }\
  END
    gx_dda_fixed_point next;    /* (y not used in fast loop) */
    gx_dda_step_fixed dxx2, dxx3, dxx4;         /* (not used in all loops) */
    const byte *psrc_initial = buffer + data_x;
    const byte *psrc = psrc_initial;
    const byte *rsrc = psrc;    /* psrc at start of run */
    const byte *endp = psrc + w;
    const byte *stop = endp;
    fixed xrun;                 /* x at start of run */
    byte run;           /* run value */
    int htrun = (masked ? 255 : -2);            /* halftone run value */
    int i, code = 0;

    if (h == 0)
        return 0;
    /*
     * Make sure the cache setup matches the graphics state.  Also determine
     * whether all tiles fit in the cache.  We may bypass the latter check
     * for masked images with a pure color.
     */

    next = penum->dda.pixel0;
    xrun = dda_current(next.x);
    if (!masked) {
        pcs = penum->pcs;       /* (may not be set for masks) */
        remap_color = pcs->type->remap_color;
    }
    run = *psrc;
    /* Find the last transition in the input. */
    if (masked &&
        (penum->posture != image_portrait) &&
        (penum->posture != image_landscape)) {
        /* No need to calculate stop */
    } else {
        byte last = stop[-1];

        while (stop > psrc && stop[-1] == last)
            --stop;
    }
    if (penum->slow_loop || penum->posture != image_portrait) {

        /**************************************************************
         * Slow case (skewed, rotated, or imagemask with a halftone). *
         **************************************************************/

        fixed yrun;
        const fixed pdyx = dda_current(penum->dda.row.x) - penum->cur.x;
        const fixed pdyy = dda_current(penum->dda.row.y) - penum->cur.y;
        dev_proc_fill_parallelogram((*fill_pgram)) =
            dev_proc(dev, fill_parallelogram);

#define xl dda_current(next.x)
#define ytf dda_current(next.y)
        yrun = ytf;
        if (masked) {

            /**********************
             * Slow case, masked. *
             **********************/

            pdevc = penum->icolor1;
            code = gx_color_load(pdevc, pgs, dev);
            if (code < 0)
                return code;
            if ((stop <= psrc) && (penum->adjust == 0) &&
                ((penum->posture == image_portrait) ||
                 (penum->posture == image_landscape)))
                goto last;
            if (penum->posture == image_portrait) {

                /********************************
                 * Slow case, masked, portrait. *
                 ********************************/

                /*
                 * We don't have to worry about the Y DDA, and the fill
                 * regions are rectangles.  Calculate multiples of the DDA
                 * step.
                 */
                fixed ax =
                    (penum->matrix.xx < 0 ? -penum->adjust : penum->adjust);
                fixed ay =
                    (pdyy < 0 ? -penum->adjust : penum->adjust);
                fixed dyy = pdyy + (ay << 1);

                yrun -= ay;
                dda_translate(next.x, -ax);
                ax <<= 1;
                dxx2 = next.x.step;
                dda_step_add(dxx2, next.x.step);
                dxx3 = dxx2;
                dda_step_add(dxx3, next.x.step);
                dxx4 = dxx3;
                dda_step_add(dxx4, next.x.step);
                for (;;) {      /* Skip a run of zeros. */
                    while (!psrc[0])
                        if (psrc + 4 <= endp) {
                            /* We can use fast skipping */
                            if (!psrc[1]) {
                                if (!psrc[2]) {
                                    if (!psrc[3]) {
                                        psrc += 4;
                                        dda_state_next(next.x.state, dxx4);
                                        if (psrc >= endp)
                                            break;
                                        continue;
                                    }
                                    psrc += 3;
                                    dda_state_next(next.x.state, dxx3);
                                    break;
                                }
                                psrc += 2;
                                dda_state_next(next.x.state, dxx2);
                                break;
                            } else {
                                ++psrc;
                                dda_next(next.x);
                                break;
                            }
                        } else {
                            /* We're too close to the end - skip 1 at a time */
                            while (!psrc[0]) {
                                ++psrc;
                                dda_next(next.x);
                                if (psrc >= endp)
                                    break;
                            }
                            if (psrc >= endp)
                                break;
                        }
                    xrun = xl;
                    if (psrc >= stop)
                        break;
                    for (; psrc < endp && *psrc; ++psrc)
                        dda_next(next.x);
                    code = (*fill_pgram)(dev, xrun, yrun,
                                         xl - xrun + ax, fixed_0, fixed_0, dyy,
                                         pdevc, lop);
                    if (code < 0)
                        goto err;
                    rsrc = psrc;
                    if (psrc >= stop)
                        break;
                }

            } else if (penum->posture == image_landscape) {

                /*********************************
                 * Slow case, masked, landscape. *
                 *********************************/

                /*
                 * We don't have to worry about the X DDA.  However, we do
                 * have to take adjustment into account.  We don't bother to
                 * optimize this as heavily as the portrait case.
                 */
                fixed ax =
                    (pdyx < 0 ? -penum->adjust : penum->adjust);
                fixed dyx = pdyx + (ax << 1);
                fixed ay =
                    (penum->matrix.xy < 0 ? -penum->adjust : penum->adjust);

                xrun -= ax;
                dda_translate(next.y, -ay);
                ay <<= 1;
                for (;;) {
                    for (; psrc < endp && !*psrc; ++psrc)
                        dda_next(next.y);
                    yrun = ytf;
                    if (psrc >= stop)
                        break;
                    for (; psrc < endp && *psrc; ++psrc)
                        dda_next(next.y);
                    code = (*fill_pgram)(dev, xrun, yrun, fixed_0,
                                         ytf - yrun + ay, dyx, fixed_0,
                                         pdevc, lop);
                    if (code < 0)
                        goto err;
                    rsrc = psrc;
                    if (psrc >= stop)
                        break;
                }

            } else {

                /**************************************
                 * Slow case, masked, not orthogonal. *
                 **************************************/
                /* FIXME: RJW: This code doesn't do adjustment. Should it?
                 * In the grand scheme of things it almost certainly doesn't
                 * matter (as adjust should only be used when plotting chars,
                 * and we use freetype now), but we record the fact that this
                 * may be a defect here. */
                /* Previous code here used to skip blocks of matching pixels
                 * and plot them all in one go. We can't do that, as it can
                 * cause rounding errors and mismatches with the image pixels
                 * that will be plotted after us. */
                stop = endp;
                for (;;) {
                    /* skip forward until we find a 1 bit */
                    for (; psrc < stop && !*psrc; ++psrc) {
                        dda_next(next.x);
                        dda_next(next.y);
                    }
                    if (psrc >= endp) /* Note, endp NOT stop! */
                        break;
                    /* Then draw the pgram and step forward until we find a
                     * 0 bit. */
                    do {
                        yrun = ytf;
                        xrun = xl;
                        dda_next(next.x);
                        dda_next(next.y);
                        code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
                                             ytf - yrun, pdyx, pdyy, pdevc, lop);
                        if (code < 0)
                            goto err;
                        psrc++;
                        rsrc = psrc;
                        if (psrc >= stop)
                            break;
                    } while (*psrc);
                }

            }

        } else if (penum->posture == image_portrait) {
            /**************************************
             * Slow case, not masked, portrait. *
             **************************************/
            dev_proc_fill_rectangle((*fill_proc)) =
                dev_proc(dev, fill_rectangle);
            int iy = fixed2int_pixround(yrun);
            int ih = fixed2int_pixround(yrun + pdyy) - iy;
            if (ih < 0)
                iy += ih, ih = -ih;

            /* In this case, we can fill runs quickly. */
            /****** DOESN'T DO ADJUSTMENT ******/
            if (stop <= psrc)
                goto last;
            for (;;) {
                byte c = *psrc++;
                if (c != run) {
                    int ix = fixed2int_pixround(xrun);
                    int iw = fixed2int_pixround(xl) - ix;
                    if (iw < 0)
                        ix += iw, iw = -iw;
                    switch (run)
                    {
                    case 0:
                        if (!color_is_pure(penum->icolor0))
                            goto ht_port;
                        code = (*fill_proc) (dev, ix, iy, iw, ih,
                                             penum->icolor0->colors.pure);
                        break;
                    case 0xff:
                        if (!color_is_pure(penum->icolor1))
                            goto ht_port;
                        code = (*fill_proc) (dev, ix, iy, iw, ih,
                                             penum->icolor1->colors.pure);
                        break;
                    default:
                        ht_port:
                        if (run != htrun) {
                            htrun = run;
#if USE_SET_GRAY_FUNCTION
                            code = image_set_gray(run,masked,mask_base,mask_limit,&pdevc,
                                    &cc,pcs,pgs,dev,gs_color_select_source,penum);
                            if (code < 0)
                                goto err;
#else
                            IMAGE_SET_GRAY(run);
#endif
                        }
                        code = gx_fill_rectangle_device_rop(ix, iy, iw, ih,
                                                            pdevc, dev, lop);
                    }
                    if (code < 0)
                        goto err;
                    xrun = xl;
                    rsrc = psrc;
                    if (psrc > stop)
                        break;
                    run = c;
                }
                dda_next(next.x);
                if (psrc >= endp)
                    break;
            }
        } else if (penum->posture == image_landscape) {

            /**************************************
             * Slow case, not masked, landscape. *
             **************************************/
            dev_proc_fill_rectangle((*fill_proc)) =
                dev_proc(dev, fill_rectangle);
            int ix = fixed2int_pixround(xrun);
            int iw = fixed2int_pixround(xrun + pdyx) - ix;
            if (iw < 0)
                ix += iw, iw = -iw;

            /* In this case, we can fill runs quickly. */
            /****** DOESN'T DO ADJUSTMENT ******/
            if (stop <= psrc)
                goto last;
            for (;;) {
                byte c = *psrc++;
                if (c != run) {
                    int iy = fixed2int_pixround(yrun);
                    int ih = fixed2int_pixround(ytf) - iy;
                    if (ih < 0)
                        iy += ih, ih = -ih;
                    switch (run)
                    {
                    case 0:
                        if (!color_is_pure(penum->icolor0))
                            goto ht_land;
                        code = (*fill_proc) (dev, ix, iy, iw, ih,
                                             penum->icolor0->colors.pure);
                        break;
                    case 0xff:
                        if (!color_is_pure(penum->icolor1))
                            goto ht_land;
                        code = (*fill_proc) (dev, ix, iy, iw, ih,
                                             penum->icolor1->colors.pure);
                        break;
                    default:
                        ht_land:
                        if (run != htrun) {
                            htrun = run;
#if USE_SET_GRAY_FUNCTION
                            code = image_set_gray(run,masked,mask_base,mask_limit,&pdevc,
                                    &cc,pcs,pgs,dev,gs_color_select_source,penum);
                            if (code < 0)
                                goto err;
#else
                            IMAGE_SET_GRAY(run);
#endif
                        }
                        code = gx_fill_rectangle_device_rop(ix, iy, iw, ih,
                                                            pdevc, dev, lop);
                    }
                    if (code < 0)
                        goto err;
                    yrun = ytf;
                    rsrc = psrc;
                    if (psrc > stop)
                        break;
                    run = c;
                }
                dda_next(next.y);
                if (psrc >= endp)
                    break;
            }
        } else {

            /******************************************
             * Slow case, not masked, not orthogonal. *
             ******************************************/

            /*
             * Since we have to check for the end after every pixel
             * anyway, we may as well avoid the last-run code.
             */
            stop = endp;
            for (;;) {
                /* We can't skip large constant regions quickly, */
                /* because this leads to rounding errors. */
                /* Just fill the region between xrun and xl. */
                if (run != htrun) {
                    htrun = run;
#if USE_SET_GRAY_FUNCTION
                    code = image_set_gray(run,masked,mask_base,mask_limit,&pdevc,
                        &cc,pcs,pgs,dev,gs_color_select_source,penum);
                    if (code < 0)
                        goto err;
#else
                    IMAGE_SET_GRAY(run);
#endif
                }
                code = (*fill_pgram) (dev, xrun, yrun, xl - xrun,
                                      ytf - yrun, pdyx, pdyy, pdevc, lop);
                if (code < 0)
                    goto err;
                yrun = ytf;
                xrun = xl;
                rsrc = psrc;
                if (psrc >= stop)
                    break;
                run = *psrc++;
                dda_next(next.x);
                dda_next(next.y);       /* harmless if no skew */
            }

        }
        /* Fill the last run. */
      last:if (stop < endp && (*stop || !masked)) {
            if (!masked) {
#if USE_SET_GRAY_FUNCTION
                    code = image_set_gray(*stop, masked,mask_base,mask_limit,&pdevc,
                        &cc,pcs,pgs,dev,gs_color_select_source,penum);
                    if (code < 0)
                        goto err;
#else
                    IMAGE_SET_GRAY(*stop);
#endif
            }
            dda_advance(next.x, endp - stop);
            dda_advance(next.y, endp - stop);
            code = (*fill_pgram) (dev, xrun, yrun, xl - xrun,
                                  ytf - yrun, pdyx, pdyy, pdevc, lop);
        }
#undef xl
#undef ytf

    } else {

        /**********************************************************
         * Fast case: no skew, and not imagemask with a halftone. *
         **********************************************************/

        const fixed adjust = penum->adjust;
        const fixed dxx = penum->dxx;
        fixed xa = (dxx >= 0 ? adjust : -adjust);
        const int yt = penum->yci, iht = penum->hci;

        dev_proc_fill_rectangle((*fill_proc)) =
            dev_proc(dev, fill_rectangle);
        int xmin = fixed2int_pixround(penum->clip_outer.p.x);
        int xmax = fixed2int_pixround(penum->clip_outer.q.x);
#define xl dda_current(next.x)

        if_debug2m('b', penum->memory, "[b]image y=%d  dda.y.Q=%lg\n",
                   penum->y + penum->rect.y, penum->dda.row.y.state.Q / 256.);
        /* Fold the adjustment into xrun and xl, */
        /* including the +0.5-epsilon for rounding. */
        xrun = xrun - xa + (fixed_half - fixed_epsilon);
        dda_translate(next.x, xa + (fixed_half - fixed_epsilon));
        xa <<= 1;
        /* Calculate multiples of the DDA step. */
        dxx2 = next.x.step;
        dda_step_add(dxx2, next.x.step);
        dxx3 = dxx2;
        dda_step_add(dxx3, next.x.step);
        dxx4 = dxx3;
        dda_step_add(dxx4, next.x.step);
        if (stop > psrc)
            for (;;) {          /* Skip large constant regions quickly, */
                /* but don't slow down transitions too much. */
              skf:if (psrc[0] == run) {
                    if (psrc[1] == run) {
                        if (psrc[2] == run) {
                            if (psrc[3] == run) {
                                psrc += 4;
                                dda_state_next(next.x.state, dxx4);
                                if (psrc >= endp)
                                    break;
                                goto skf;
                            } else {
                                psrc += 4;
                                dda_state_next(next.x.state, dxx3);
                            }
                        } else {
                            psrc += 3;
                            dda_state_next(next.x.state, dxx2);
                        }
                    } else {
                        psrc += 2;
                        dda_next(next.x);
                    }
                } else
                    psrc++;
                {               /* Now fill the region between xrun and xl. */
                    int xi = fixed2int_var(xrun);
                    int wi = fixed2int_var(xl) - xi;
                    int xei;

                    if (wi <= 0) {
                        if (wi == 0)
                            goto mt;
                        xi += wi, wi = -wi;
                    }
                    if ((xei = xi + wi) > xmax || xi < xmin) {  /* Do X clipping */
                        if (xi < xmin)
                            wi -= xmin - xi, xi = xmin;
                        if (xei > xmax)
                            wi -= xei - xmax;
                        if (wi <= 0)
                            goto mt;
                    }
                    switch (run) {
                        case 0:
                            if (masked)
                                goto mt;
                            if (!color_is_pure(penum->icolor0))
                                goto ht;
                            code = (*fill_proc) (dev, xi, yt, wi, iht,
                                                 penum->icolor0->colors.pure);
                            break;
                        case 255:       /* just for speed */
                            if (!color_is_pure(penum->icolor1))
                                goto ht;
                            code = (*fill_proc) (dev, xi, yt, wi, iht,
                                                 penum->icolor1->colors.pure);
                            break;
                        default:
                          ht:   /* Use halftone if needed */
                            if (run != htrun) {
#if USE_SET_GRAY_FUNCTION
                                code = image_set_gray(run, masked,mask_base,mask_limit,&pdevc,
                                    &cc,pcs,pgs,dev,gs_color_select_source,penum);
                                if (code < 0)
                                    goto err;
#else
                                IMAGE_SET_GRAY(run);
#endif
                                htrun = run;
                            }
                            code = gx_fill_rectangle_device_rop(xi, yt, wi, iht,
                                                                 pdevc, dev, lop);
                    }
                    if (code < 0)
                        goto err;
                  mt:xrun = xl - xa;    /* original xa << 1 */
                    rsrc = psrc - 1;
                    if (psrc > stop) {
                        --psrc;
                        break;
                    }
                    run = psrc[-1];
                }
                dda_next(next.x);
            }
        /* Fill the last run. */
        if (*stop != 0 || !masked) {
            int xi = fixed2int_var(xrun);
            int wi, xei;

            dda_advance(next.x, endp - stop);
            wi = fixed2int_var(xl) - xi;
            if (wi <= 0) {
                if (wi == 0)
                    goto lmt;
                xi += wi, wi = -wi;
            }
            if ((xei = xi + wi) > xmax || xi < xmin) {  /* Do X clipping */
                if (xi < xmin)
                    wi -= xmin - xi, xi = xmin;
                if (xei > xmax)
                    wi -= xei - xmax;
                if (wi <= 0)
                    goto lmt;
            }
#if USE_SET_GRAY_FUNCTION
            code = image_set_gray(*stop, masked,mask_base,mask_limit,&pdevc,
                &cc,pcs,pgs,dev,gs_color_select_source,penum);
            if (code < 0)
                goto err;
#else
            IMAGE_SET_GRAY(*stop);
#endif
            code = gx_fill_rectangle_device_rop(xi, yt, wi, iht,
                                                pdevc, dev, lop);
          lmt:;
        }

    }
#undef xl
    if (code >= 0) {
        code = 1;
        goto done;
    }
    /* Save position if error, in case we resume. */
err:
    penum->used.x = rsrc - psrc_initial;
    penum->used.y = 0;
done:
    /* Since dev_color.binary.b_tile is just a pointer to an entry in the halftone tile "cache"
       we cannot leave it set in case the interpeter changes the halftone
       (whilst it shouldn't do it, it is possible for Postscript image data source
       procedure to execute setcreen or similar). This also doesn't really adversely
       affect performance, as we'll call gx_color_load_select() for all the samples
       from the next buffer, which will NULL the b_tile pointer anyway (it only gets
       set when we actually try to draw with it.
    */
    for (i = 0; i < 256; i++) {
        penum->clues[i].dev_color.colors.binary.b_tile = NULL;
    }
    return code;
}

#ifdef WITH_CAL
static int
image_render_mono_ht_cal_skip_line(gx_image_enum *penum,
                                   gx_device     *dev)
{
    return !cal_halftone_next_line_required(penum->cal_ht);
}

static int
image_render_mono_ht_cal(gx_image_enum * penum, const byte * buffer, int data_x,
    uint w, int h, gx_device * dev)
{
    const byte *input = buffer + data_x;

    if (buffer == NULL)
        return 0;

    return cal_halftone_process_planar(penum->cal_ht, penum->memory->non_gc_memory,
                                       (const byte * const *)&input, halftone_callback, dev);
}
#else
/*
 An image render case where the source color is monochrome or indexed and
 the output is to be halftoned.  If the source color requires decoding,
 an index look-up or ICC color managment, these operations have already been
 performed on the index values and we are ready now to halftone */
static int
image_render_mono_ht(gx_image_enum * penum_orig, const byte * buffer, int data_x,
                  uint w, int h, gx_device * dev)
{
    gx_image_enum *penum = penum_orig; /* const within proc */
    image_posture posture = penum->posture;
    int vdi;  /* amounts to replicate */
    fixed xrun;
    byte *thresh_align;
    int spp_out = penum->dev->color_info.num_components;
    byte *devc_contone[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte *devc_contone_gray;
    const byte *psrc = buffer + data_x;
    int dest_width, dest_height, data_length;
    byte *color_cache;
    int position, k, j;
    int offset_bits = penum->ht_offset_bits;
    int contone_stride = 0;  /* Not used in landscape case */
    fixed offset;
    int src_size;
    bool flush_buff = false;
    int offset_contone[GX_DEVICE_COLOR_MAX_COMPONENTS];    /* to ensure 128 bit boundary */
    int offset_threshold;  /* to ensure 128 bit boundary */
    gx_dda_fixed dda_ht;
    int xn, xr;		/* destination position (pixel, not contone buffer offset) */
    int code = 0;
    byte *dev_value;

    if (h == 0 || penum->line_size == 0) {      /* line_size == 0, nothing to do */
        if (penum->ht_landscape.count == 0 || posture == image_portrait) {
            return 0;
        } else {
            /* Need to flush the buffer */
            offset_bits = penum->ht_landscape.count;
            penum->ht_offset_bits = offset_bits;
            penum->ht_landscape.offset_set = true;
            flush_buff = true;
        }
    }
    src_size = penum->rect.w;

    /* Set up the dda.  We could move this out but the cost is pretty small */
    dda_ht = (posture == image_portrait) ? penum->dda.pixel0.x : penum->dda.pixel0.y;
    if (penum->dxx > 0)
        dda_translate(dda_ht, -fixed_epsilon);      /* to match rounding in non-fast code */

    switch (posture) {
        case image_portrait:
            /* Figure out our offset in the contone and threshold data
               buffers so that we ensure that we are on the 128bit
               memory boundaries when we get offset_bits into the data. */
            /* Can't do this earlier, as GC might move the buffers. */
            xrun = dda_current(dda_ht);
            dest_width = gxht_dda_length(&dda_ht, src_size);
            if (penum->x_extent.x < 0)
                xrun += penum->x_extent.x;
            vdi = penum->hci;
            contone_stride = penum->line_size;
            offset_threshold = (- (((int)(intptr_t)(penum->thresh_buffer)) +
                                   penum->ht_offset_bits)) & 15;
            for (k = 0; k < spp_out; k ++) {
                offset_contone[k]   = (- (((int)(intptr_t)(penum->line)) +
                                           contone_stride * k +
                                           penum->ht_offset_bits)) & 15;
            }
            data_length = dest_width;
            dest_height = fixed2int_var_rounded(any_abs(penum->y_extent.y));
#ifdef DEBUG
            /* Help in spotting problems */
            memset(penum->ht_buffer, 0x00, penum->ht_stride * vdi * spp_out);
#endif
            break;
        case image_landscape:
        default:
            /* Figure out our offset in the contone and threshold data buffers
               so that we ensure that we are on the 128bit memory boundaries.
               Can't do this earlier as GC may move the buffers.
             */
            vdi = penum->wci;
            contone_stride = penum->line_size;
            dest_width = fixed2int_var_rounded(any_abs(penum->y_extent.x));
            /* match height in gxht_thresh.c dev_width calculation */
            xrun = dda_current(dda_ht);            /* really yrun, but just used here for landscape */
            dest_height = gxht_dda_length(&dda_ht, src_size);
            data_length = dest_height;
            offset_threshold = (-(int)(intptr_t)(penum->thresh_buffer)) & 15;
            for (k = 0; k < spp_out; k ++) {
                offset_contone[k] = (- ((int)(intptr_t)(penum->line) +
                                        contone_stride * k)) & 15;
            }
            /* In the landscaped case, we want to accumulate multiple columns
               of data before sending to the device.  We want to have a full
               byte of HT data in one write.  This may not be possible at the
               left or right and for those and for those we have so send partial
               chunks */
            /* Initialize our xstart and compute our partial bit chunk so
               that we get in sync with the 1 bit mem device 16 bit positions
               for the rest of the chunks */
            if (penum->ht_landscape.count == 0) {
                /* In the landscape case, the size depends upon
                   if we are moving left to right or right to left with
                   the image data.  This offset is to ensure that we  get
                   aligned in our chunks along 16 bit boundaries */
                penum->ht_landscape.offset_set = true;
                if (penum->ht_landscape.index < 0) {
                    penum->ht_landscape.xstart = penum->xci + vdi - 1;
                    offset_bits = (penum->ht_landscape.xstart % 16) + 1;
                    /* xci can be negative, so allow for that */
                    if (offset_bits <= 0) offset_bits += 16;
                } else {
                    penum->ht_landscape.xstart = penum->xci;
                    /* xci can be negative, see Bug 692569. */
                    offset_bits = 16 - penum->xci % 16;
                    if (offset_bits >= 16) offset_bits -= 16;
                }
                if (offset_bits == 0 || offset_bits == 16) {
                    penum->ht_landscape.offset_set = false;
                    penum->ht_offset_bits = 0;
                } else {
                    penum->ht_offset_bits = offset_bits;
                }
            }
            break;
    }
    if (flush_buff)
        goto flush;  /* All done */

    /* Get the pointers to our buffers */
    for (k = 0; k < spp_out; k++) {
        if (posture == image_portrait) {
            devc_contone[k] = penum->line + contone_stride * k +
                              offset_contone[k];
        } else {
            devc_contone[k] = penum->line + offset_contone[k] +
                              LAND_BITS * k * contone_stride;
        }
    }
    xr = fixed2int_var_rounded(dda_current(dda_ht));	/* indexes in the destination (contone) */

    devc_contone_gray = devc_contone[0];
    if (penum->color_cache == NULL) {
        /* No look-up in the cache to fill the source buffer. Still need to
           have the data at device resolution.  Do these in quick small
           loops.  This likely could be a vectorized function.  Note that
           since the color_cache is NULL we must be in a case where we
           are going to a monochrome device. */
        switch (posture) {
            case image_portrait:
                if (penum->dst_width > 0) {
                    if (src_size == dest_width) {
                        memcpy(devc_contone_gray, psrc, data_length);
                    } else if (src_size * 2 == dest_width) {
                        const byte *psrc_temp = psrc;
                        for (k = 0; k < data_length; k+=2, devc_contone_gray+=2,
                             psrc_temp++) {
                            *devc_contone_gray = *(devc_contone_gray+1) = *psrc_temp;
                        }
                    } else {
                        /* Mono case, forward */
                        for (k=0; k<src_size; k++) {
                            byte c = *psrc++;
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr < xn) {
                                *devc_contone_gray++ = c;
                                xr++;
                            }           /* at loop exit xn will be >= xr */
                        }
                    }
                } else {
                    /* Mono case, backwards */
                    devc_contone_gray += (data_length - 1);
                    for (k=0; k<src_size; k++) {
                        byte c = *psrc++;
                        dda_next(dda_ht);
                        xn = fixed2int_var_rounded(dda_current(dda_ht));
                        while (xr > xn) {
                            *devc_contone_gray-- = c;
                            xr--;
                        }           /* at loop exit xn will be >= xr */
                    }
                }
                break;
            case image_landscape:
                /* We store the data at this point into a column. Depending
                   upon our landscape direction we may be going left to right
                   or right to left. */
                if (penum->ht_landscape.flipy) {
                    position = penum->ht_landscape.curr_pos +
                                LAND_BITS * (data_length - 1);
                    for (k=0; k<src_size; k++) {
                        byte c = *psrc++;
                        dda_next(dda_ht);
                        xn = fixed2int_var_rounded(dda_current(dda_ht));
                        while (xr > xn) {
                            devc_contone_gray[position] = c;
                            position -= LAND_BITS;
                            xr--;
                        }           /* at loop exit xn will be <= xr */
                    }
                } else {
                    position = penum->ht_landscape.curr_pos;
                    /* Code up special cases for when we have no scaling
                       and 2x scaling which we will run into in 300 and
                       600dpi devices and content */
                    if (src_size == dest_height) {
                        for (k = 0; k < data_length; k++) {
                            devc_contone_gray[position] = psrc[k];
                            position += LAND_BITS;
                        }
                    } else if (src_size*2 == dest_height) {
                        for (k = 0; k < data_length; k+=2) {
                            offset = fixed2int_rounded(fixed_half * k);
                            devc_contone_gray[position] =
                                devc_contone_gray[position + LAND_BITS] = psrc[offset];
                            position += LAND_BITS*2;
                        }
                    } else {
                        /* use dda */
                        for (k=0; k<src_size; k++) {
                            byte c = *psrc++;
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr < xn) {
                                devc_contone_gray[position] = c;
                                position += LAND_BITS;
                                xr++;
                            }           /* at loop exit xn will be >= xr */
                        }
                    }
                }
                /* Store the width information and update our counts */
                penum->ht_landscape.count += vdi;
                penum->ht_landscape.widths[penum->ht_landscape.curr_pos] = vdi;
                penum->ht_landscape.curr_pos += penum->ht_landscape.index;
                penum->ht_landscape.num_contones++;
                break;
            default:
                /* error not allowed */
                break;
            }
    } else {
        /* look-up in the cache to fill the source buffer.  If our spp_out
           is 4 then we need to write out the pixels in the color_cache into
           the planes */
        color_cache = penum->color_cache->device_contone;
        switch (posture) {
            case image_portrait:
                if (penum->dst_width > 0) {
                    /* loop filling contone values selected from the source */
                    if (spp_out == 1) {
                        /* Mono case, forward */
                        for (k=0; k<src_size; k++) {
                            byte c = color_cache[*psrc++];
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr < xn) {
                                *devc_contone_gray++ = c;
                                xr++;
                            }           /* at loop exit xn will be >= xr */
                        }
                    } else {
                        /* CMYK case, forward */
                        for (k=0; k<src_size; k++) {
                            dev_value = &(color_cache[*psrc++ * spp_out]);
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr < xn) {
                                for (j = 0; j < spp_out; j++) {
                                    *(devc_contone[j])++ = dev_value[j];
                                }
                                xr++;
                            }           /* at loop exit xn will be >= xr */
                        }
                    }
                } else {
                    if (spp_out == 1) {
                        /* Mono case, backwards */
                        devc_contone_gray += data_length - 1;		/* move to end */
                        for (k=0; k<src_size; k++) {
                            byte c = color_cache[*psrc++];
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                *devc_contone_gray-- = c;
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                        }
                    } else {
                        /* CMYK case, backwards */
                        /* Move to the other end and we will decrement */
                        for (j = 0; j < spp_out; j++) {
                            devc_contone[j] = devc_contone[j] + data_length - 1;
                        }
                        for (k=0; k<src_size; k++) {
                            dev_value = &(color_cache[*psrc++ * spp_out]);
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                for (j = 0; j < spp_out; j++) {
                                    *(devc_contone[j])-- = dev_value[j];
                                }
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                        }
                    }
                }
                break;
            case image_landscape:
                /* We store the data at this point into a column. Depending
                   upon our landscape direction we may be going left to right
                   or right to left. */
                if (penum->ht_landscape.flipy) {
                    position = penum->ht_landscape.curr_pos +
                                LAND_BITS * (data_length - 1);
                    /* use dda */
                    if (spp_out == 1) {
                        /* Mono case */
                        /* loop filling contone values selected from the source */
                        for (k=0; k<src_size; k++) {
                            byte c = color_cache[*psrc++];
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                devc_contone_gray[position] = c;
                                position -= LAND_BITS;
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                        }
                    } else {
                        /* CMYK case */
                        for (k=0; k<src_size; k++) {
                            dev_value = &(color_cache[*psrc++ * spp_out]);
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                for (j = 0; j < spp_out; j++) {
                                    *(devc_contone[j] + position) = dev_value[j];
                                }
                                position -= LAND_BITS;
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                        }
                    }
                } else {  /* Not flipped in Y */
                    position = penum->ht_landscape.curr_pos;
                    /* use dda */
                    if (spp_out == 1) {
                        /* Mono case */
                        /* loop filling contone values selected from the source */
                        for (k=0; k<src_size; k++) {
                            byte c = color_cache[*psrc++];
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr < xn) {
                                devc_contone_gray[position] = c;
                                position += LAND_BITS;
                                xr++;
                            }           /* at loop exit xn will be >= xr */
                        }
                    } else {
                        /* CMYK case */
                        /* Apply initial offset */
                        for (k = 0; k < spp_out; k++) {
                            devc_contone[k] = devc_contone[k] + position;
                        }
                        /* CMYK case */
                        for (k=0; k<src_size; k++) {
                            dev_value = &(color_cache[*psrc++ * spp_out]);
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr < xn) {
                                for (j = 0; j < spp_out; j++) {
                                    *(devc_contone[j] + position) = dev_value[j];
                                }
                                position += LAND_BITS;
                                xr++;
                            }           /* at loop exit xn will be >= xr */
                        }
                    }
                }
                /* Store the width information and update our counts */
                penum->ht_landscape.count += vdi;
                penum->ht_landscape.widths[penum->ht_landscape.curr_pos] = vdi;
                penum->ht_landscape.curr_pos += penum->ht_landscape.index;
                penum->ht_landscape.num_contones++;
                break;
            default:
                /* error not allowed */
                break;
        }
    }
    /* Apply threshold array to image data */
flush:
    thresh_align = penum->thresh_buffer + offset_threshold;
    code = gxht_thresh_planes(penum, xrun, dest_width, dest_height,
                              thresh_align, dev, offset_contone,
                              contone_stride);
    return code;
}
#endif
