/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
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
#include "gxistate.h"
#include "gxdevmem.h"
#include "gdevmem.h"            /* for mem_mono_device */
#include "gxcpath.h"
#include "gximage.h"
#include "gzht.h"
#include "vdtrace.h"
#include "gsicc.h"
#include "gsicc_cache.h"
#include "gsicc_cms.h"
#include "gxcie.h"
#include "gscie.h"
#include "gxht_thresh.h"
#include "gxdda.h"

#define USE_FAST_CODE 1
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
static irender_proc(image_render_mono_ht);

irender_proc_t
gs_image_class_3_mono(gx_image_enum * penum)
{
#if USE_FAST_CODE
    bool use_fast_code = true;
#else
    bool use_fast_code = false;
#endif
    int code = 0;
    /* Set up the link now */
    const gs_color_space *pcs;
    gsicc_rendering_param_t rendering_params;

    if (penum->spp == 1) {
        /* At this point in time, only use the ht approach if our device
           uses halftoning, and our source image is a reasonable size.  We
           probably don't want to do this if we have a bunch of tiny little
           images.  Then the rect fill approach is probably not all that bad.
           Also for now avoid images that include a type3 image mask.  Due
           to the limited precision and mismatch of the stepping space in which
           the interpolations occur this can cause a minor mismatch at large
           scalings */
        if (use_fast_code && penum->pcs != NULL &&
            penum->dev->color_info.num_components == 1 &&
            penum->dev->color_info.depth == 1 &&
            penum->bps == 8 && (penum->posture == image_portrait
            || penum->posture == image_landscape) &&
            penum->image_parent_type == gs_image_type1) {
            penum->icc_setup.need_decode = false;
            /* Check if we need to do any decoding.  */
            if ( penum->map[0].decoding != sd_none ) {
                if (!(penum->map[0].decoding == sd_compute
                     && penum->map[0].decode_factor == 1.0 &&
                        penum->map[0].decode_lookup[0] == 0.0)) {
                    penum->icc_setup.need_decode = true;
                }
            }
            /* Define the rendering intents */
            rendering_params.black_point_comp = BP_ON;
            rendering_params.object_type = GS_IMAGE_TAG;
            rendering_params.rendering_intent = penum->pis->renderingintent;
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
            /* The effective transfer is built into the threshold array and
               need require a special lookup to decode it */
            penum->icc_setup.has_transfer = gx_has_transfer(penum->pis,
                                    penum->dev->device_icc_profile->num_comps);
            if (penum->icc_setup.is_lab) penum->icc_setup.need_decode = false;
            if (penum->icc_link == NULL) {
                penum->icc_link = gsicc_get_link(penum->pis, penum->dev, pcs, NULL,
                    &rendering_params, penum->memory, false);
            }
            /* PS CIE color spaces may have addition decoding that needs to
               be performed to ensure that the range of 0 to 1 is provided
               to the CMM since ICC profiles are restricted to that range
               but the PS color spaces are not. */
            if (gs_color_space_is_PSCIE(penum->pcs) &&
                penum->pcs->icc_equivalent != NULL) {
                /* We have a PS CIE space.  Check the range */
                if ( !check_cie_range(penum->pcs) ) {
                    /* It is not 0 to 1.  We will be doing decode
                       plus an additional linear adjustment */
                    penum->cie_range = get_cie_range(penum->pcs);
                }
            }
            /* If the image has more than 256 pixels then go ahead and
               precompute the con-tone device colors for all of our 256 source
               values.  We should not be taking this path for cases where
               we have lots of tiny little images.  Mark those that are
               transparent or masked also at this time.  Since halftoning will
               be done via thresholding we will keep clues in continuous tone */
            code = image_init_color_cache(penum, penum->bps, penum->spp);
            if (code >= 0) {
                code = gxht_thresh_image_init(penum);
                if (code >= 0)
                    return &image_render_mono_ht;
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
        if_debug0('b', "[b]render=mono\n");
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
        return &image_render_mono;
    }
    return 0;
}

#define USE_SET_GRAY_FUNCTION 0
/* Temporary function to make it easier to debug the uber-macro below */
static inline int
image_set_gray(byte sample_value, const bool masked, uint mask_base,
                uint mask_limit, gx_device_color **ppdevc, gs_client_color *cc,
                const gs_color_space *pcs, const gs_imager_state *pis,
                gx_device * dev, gs_color_select_t gs_color_select_source,
                gx_image_enum * penum, bool tiles_fit)
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
            code = (*remap_color)(cc, pcs, pdevc, pis, dev, gs_color_select_source);
            return(code);
        }
    } else if (!color_is_pure(pdevc)) {
        if (!tiles_fit) {
            code = gx_color_load_select(pdevc, pis, dev, gs_color_select_source);
            if (code < 0)
                return(code);
        }
    }
    return(0);
}


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
    const gs_imager_state *pis = penum->pis;
    gs_logical_operation_t lop = penum->log_op;
    const bool masked = penum->masked;
    const gs_color_space *pcs = NULL;   /* only set for non-masks */
    cs_proc_remap_color((*remap_color)) = NULL; /* ditto */
    gs_client_color cc;
    gx_device_color *pdevc = penum->icolor1;    /* color for masking */
    bool tiles_fit;
    uint mask_base =            /* : 0 to pacify Valgrind */
        (penum->use_mask_color ? penum->mask_color.values[0] : 0);
    uint mask_limit =
        (penum->use_mask_color ?
         penum->mask_color.values[1] - mask_base + 1 : 0);
/*
 * Free variables of IMAGE_SET_GRAY:
 *   Read: penum, pis, dev, tiles_fit, mask_base, mask_limit
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
            code = (*remap_color)(&cc, pcs, pdevc, pis, dev, gs_color_select_source);\
            if (code < 0)\
                goto err;\
        }\
    } else if (!color_is_pure(pdevc)) {\
        if (!tiles_fit) {\
            code = gx_color_load_select(pdevc, pis, dev, gs_color_select_source);\
            if (code < 0)\
                goto err;\
        }\
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
    int code = 0;

    if (h == 0)
        return 0;
    /*
     * Make sure the cache setup matches the graphics state.  Also determine
     * whether all tiles fit in the cache.  We may bypass the latter check
     * for masked images with a pure color.
     */

    /* TO_DO_DEVICEN - The gx_check_tile_cache_current() routine is bogus */

    if (pis == 0 || !gx_check_tile_cache_current(pis)) {
        image_init_clues(penum, penum->bps, penum->spp);
    }
    tiles_fit = (pis && penum->device_color ? gx_check_tile_cache(pis) : false);
    next = penum->dda.pixel0;
    xrun = dda_current(next.x);
    if (!masked) {
        pcs = penum->pcs;       /* (may not be set for masks) */
        remap_color = pcs->type->remap_color;
    }
    run = *psrc;
    /* Find the last transition in the input. */
    {
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
            code = gx_color_load(pdevc, pis, dev);
            if (code < 0)
                return code;
            if (stop <= psrc)
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
                        if (!psrc[1]) {
                            if (!psrc[2]) {
                                if (!psrc[3]) {
                                    psrc += 4;
                                    dda_state_next(next.x.state, dxx4);
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
                    xrun = xl;
                    if (psrc >= stop)
                        break;
                    for (; *psrc; ++psrc)
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
                    for (; !*psrc; ++psrc)
                        dda_next(next.y);
                    yrun = ytf;
                    if (psrc >= stop)
                        break;
                    for (; *psrc; ++psrc)
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

                for (;;) {
                    for (; !*psrc; ++psrc) {
                        dda_next(next.x);
                        dda_next(next.y);
                    }
                    yrun = ytf;
                    xrun = xl;
                    if (psrc >= stop)
                        break;
                    for (; *psrc; ++psrc) {
                        dda_next(next.x);
                        dda_next(next.y);
                    }
                    code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
                                         ytf - yrun, pdyx, pdyy, pdevc, lop);
                    if (code < 0)
                        goto err;
                    rsrc = psrc;
                    if (psrc >= stop)
                        break;
                }

            }

        } else if (penum->posture == image_portrait ||
                   penum->posture == image_landscape
            ) {

            /**************************************
             * Slow case, not masked, orthogonal. *
             **************************************/

            /* In this case, we can fill runs quickly. */
            /****** DOESN'T DO ADJUSTMENT ******/
            if (stop <= psrc)
                goto last;
            for (;;) {
                if (*psrc != run) {
                    if (run != htrun) {
                        htrun = run;
#if USE_SET_GRAY_FUNCTION
                        code = image_set_gray(run,masked,mask_base,mask_limit,&pdevc,
                            &cc,pcs,pis,dev,gs_color_select_source,penum,tiles_fit);
                        if (code < 0)
                            goto err;
#else
                        IMAGE_SET_GRAY(run);
#endif
                    }
                    code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
                                         ytf - yrun, pdyx, pdyy,
                                         pdevc, lop);
                    if (code < 0)
                        goto err;
                    yrun = ytf;
                    xrun = xl;
                    rsrc = psrc;
                    if (psrc >= stop)
                        break;
                    run = *psrc;
                }
                psrc++;
                dda_next(next.x);
                dda_next(next.y);
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
                        &cc,pcs,pis,dev,gs_color_select_source,penum,tiles_fit);
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
                        &cc,pcs,pis,dev,gs_color_select_source,penum,tiles_fit);
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

        if_debug2('b', "[b]image y=%d  dda.y.Q=%lg\n", penum->y + penum->rect.y,
                    penum->dda.row.y.state.Q / 256.);
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
                                    &cc,pcs,pis,dev,gs_color_select_source,penum,tiles_fit);
                                if (code < 0)
                                    goto err;
#else
                                IMAGE_SET_GRAY(run);
#endif
                                htrun = run;
                            }
                            code = gx_fill_rectangle_device_rop(xi, yt, wi, iht,
                                                                 pdevc, dev, lop);
                            vd_rect(int2fixed(xi), int2fixed(yt), int2fixed(xi + wi), int2fixed(yt + iht),
                                0, pdevc->colors.pure /* wrong with halftones */);
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
                &cc,pcs,pis,dev,gs_color_select_source,penum,tiles_fit);
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
    if (code >= 0)
        return 1;
    /* Save position if error, in case we resume. */
err:
    penum->used.x = rsrc - psrc_initial;
    penum->used.y = 0;
    return code;
}

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
    byte *contone_align, *thresh_align;
    int spp_out = penum->dev->color_info.num_components;
    byte *devc_contone;
    const byte *psrc = buffer + data_x;
    int dest_width, dest_height, data_length;
    byte *dev_value, *color_cache;
    gx_ht_order *d_order = &(penum->pis->dev_ht->components[0].corder);
    int position, k;
    int offset_bits = penum->ht_offset_bits;
    int contone_stride = 0;  /* Not used in landscape case */
    fixed scale_factor, offset;
    int src_size;
    bool flush_buff = false;
    byte *psrc_temp;
    int offset_contone;    /* to ensure 128 bit boundary */
    int offset_threshold;  /* to ensure 128 bit boundary */
    gx_dda_int_t dda_ht;
    int code;

    if (h == 0) {
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
    src_size = (penum->rect.w - 1.0);

    switch (posture) {
        case image_portrait:
            /* Figure out our offset in the contone and threshold data
               buffers so that we ensure that we are on the 128bit
               memory boundaries when we get offset_bits into the data. */
            /* Can't do this earlier, as GC might move the buffers. */
            offset_contone   = (- (((long)(penum->line)) +
                                   penum->ht_offset_bits)) & 15;
            offset_threshold = (- (((long)(penum->thresh_buffer)) +
                                   penum->ht_offset_bits)) & 15;
            xrun = dda_current(penum->dda.pixel0.x);
            xrun = xrun - penum->adjust + (fixed_half - fixed_epsilon);
            dest_width = fixed2int_var_rounded(any_abs(penum->x_extent.x));
            if (penum->x_extent.x < 0)
                xrun += penum->x_extent.x;
            vdi = penum->hci;
            data_length = dest_width;
            dest_height = fixed2int_var_rounded(any_abs(penum->y_extent.y));
            contone_stride = penum->line_size;
            scale_factor = float2fixed_rounded((float) src_size / (float) (dest_width - 1));
#ifdef DEBUG
            /* Help in spotting problems */
            memset(penum->ht_buffer,0x00, penum->ht_stride * vdi);
#endif
            break;
        case image_landscape:
        default:
            /* Figure out our offset in the contone and threshold data buffers
               so that we ensure that we are on the 128bit memory boundaries.
               Can't do this earlier as GC may move the buffers.
             */
            offset_contone   = (-(long)(penum->line)) & 15;
            offset_threshold = (-(long)(penum->thresh_buffer)) & 15;
            vdi = penum->wci;
            dest_width = fixed2int_var_rounded(any_abs(penum->y_extent.x));
            dest_height = fixed2int_var_rounded(any_abs(penum->x_extent.y));
            data_length = dest_height;
            scale_factor = float2fixed_rounded((float) src_size / (float) (dest_height - 1));
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
                } else {
                    penum->ht_landscape.xstart = penum->xci;
                    offset_bits = 16 - penum->xci % 16;
                    if (offset_bits == 16) offset_bits = 0;
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
    /* Get the pointers to our buffers */
    contone_align = penum->line + offset_contone;
    thresh_align = penum->thresh_buffer + offset_threshold;

    if (flush_buff) goto flush;  /* All done */
    /* Set up the dda.  We could move this out but the cost is pretty small */
    dda_init(dda_ht, 0, src_size, data_length-1);
    devc_contone = contone_align;
    if (penum->color_cache == NULL) {
        /* No look-up in the cache to fill the source buffer. Still need to
           have the data at device resolution.  Do these in quick small
           loops.  This likely could be a vectorized function */
        switch (posture) {
            case image_portrait:
                if (penum->dst_width > 0) {
                    if (scale_factor == fixed_1) {
                        memcpy(devc_contone, psrc, data_length);
                    } else if (scale_factor == fixed_half) {
                        psrc_temp = psrc;
                        for (k = 0; k < data_length; k+=2, devc_contone+=2,
                             psrc_temp++) {
                            *devc_contone = *(devc_contone+1) = *psrc_temp;
                        }
                    } else {
                        for (k = 0; k < data_length; k++, devc_contone++) {
                            *devc_contone = psrc[dda_ht.state.Q];
                            dda_next(dda_ht);
                        }
                    }
                } else {
                    devc_contone += (data_length - 1);
                    for (k = 0; k < data_length; k++, devc_contone--) {
                        *devc_contone = psrc[dda_ht.state.Q];
                        dda_next(dda_ht);
                    }
                }
                break;
            case image_landscape:
                /* We store the data at this point into a column. Depending
                   upon our landscape direction we may be going left to right
                   or right to left. */
                if (penum->ht_landscape.flipy) {
                    position = penum->ht_landscape.curr_pos +
                                16 * (data_length - 1);
                    for (k = 0; k < data_length; k++) {
                        devc_contone[position] = psrc[dda_ht.state.Q];
                        position -= 16;
                        dda_next(dda_ht);
                    }
                } else {
                    position = penum->ht_landscape.curr_pos;
                    /* Code up special cases for when we have no scaling
                       and 2x scaling which we will run into in 300 and
                       600dpi devices and content */
                    if (scale_factor == fixed_1) {
                        for (k = 0; k < data_length; k++) {
                            devc_contone[position] = psrc[k];
                            position += 16;
                        }
                    } else if (scale_factor == fixed_half) {
                        for (k = 0; k < data_length; k+=2) {
                            offset = fixed2int_rounded(scale_factor * k);
                            devc_contone[position] =
                                devc_contone[position + 16] = psrc[offset];
                            position += 32;
                        }
                    } else {
                        /* use dda */
                        for (k = 0; k < data_length; k++) {
                            devc_contone[position] = psrc[dda_ht.state.Q];
                            position += 16;
                            dda_next(dda_ht);
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
        /* look-up in the cache to fill the source buffer */
        color_cache = penum->color_cache->device_contone;
        switch (posture) {
            case image_portrait:
                if (penum->dst_width > 0) {
                    for (k = 0; k < data_length; k++) {
                        dev_value = color_cache + psrc[dda_ht.state.Q] * spp_out;
                        memcpy(devc_contone, dev_value, spp_out);
                        devc_contone += spp_out;
                        dda_next(dda_ht);
                    }
                } else {
                    for (k = 0; k < data_length; k++) {
                        dev_value = color_cache + psrc[dda_ht.state.Q] * spp_out;
                        memcpy(&(devc_contone[(data_length - k - 1) * spp_out]),
                               dev_value, spp_out);
                        dda_next(dda_ht);
                    }
                }
                break;
            case image_landscape:
                /* We store the data at this point into a column. Depending
                   upon our landscape direction we may be going left to right
                   or right to left. */
                if (penum->ht_landscape.flipy) {
                    position = penum->ht_landscape.curr_pos +
                                16 * (data_length - 1);
                    /* use dda */
                    for (k = 0; k < data_length; k++) {
                        dev_value = color_cache + psrc[dda_ht.state.Q] * spp_out;
                        devc_contone[position] = dev_value[0];  /* Only works for monochrome device now */
                        position -= 16;
                        dda_next(dda_ht);
                    }
                } else {
                    position = penum->ht_landscape.curr_pos;
                    /* use dda */
                    for (k = 0; k < data_length; k++) {
                        dev_value = color_cache + psrc[dda_ht.state.Q] * spp_out;
                        devc_contone[position] = dev_value[0];  /* Only works for monochrome device now */
                        position += 16;
                        dda_next(dda_ht);
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
    code = gxht_thresh_plane(penum, d_order, xrun, dest_width, dest_height,
                             thresh_align, contone_align, contone_stride, dev); 
    return code;
}
