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


/* Color image rendering */

#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxfrac.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxdcconv.h"
#include "gxdcolor.h"
#include "gxgstate.h"
#include "gxdevmem.h"
#include "gxcpath.h"
#include "gximage.h"
#include "gsicc.h"
#include "gsicc_cache.h"
#include "gsicc_cms.h"
#include "gxcie.h"
#include "gscie.h"
#include "gzht.h"
#include "gxht_thresh.h"
#include "gxdevsop.h"

typedef union {
    byte v[GS_IMAGE_MAX_COLOR_COMPONENTS];
#define BYTES_PER_BITS32 4
#define BITS32_PER_COLOR_SAMPLES\
  ((GS_IMAGE_MAX_COLOR_COMPONENTS + BYTES_PER_BITS32 - 1) / BYTES_PER_BITS32)
    bits32 all[BITS32_PER_COLOR_SAMPLES];	/* for fast comparison */
} color_samples;

/* ------ Strategy procedure ------ */

/* Check the prototype. */
iclass_proc(gs_image_class_4_color);

static irender_proc(image_render_color_DeviceN);
static irender_proc(image_render_color_icc_tpr);
#ifndef WITH_CAL
static irender_proc(image_render_color_thresh);
#else

#include "cal.h"

static irender_proc(image_render_color_ht_cal);

static int
image_render_color_ht_cal_skip_line(gx_image_enum *penum,
                                    gx_device     *dev)
{
    return !cal_halftone_next_line_required(penum->cal_ht);
}

static void
color_halftone_callback(cal_halftone_data_t *ht, void *arg)
{
    gx_device *dev = arg;
    gx_color_index dev_white = gx_device_white(dev);
    gx_color_index dev_black = gx_device_black(dev);

    if (dev->is_planar) {
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
color_halftone_init(gx_image_enum *penum)
{
    cal_halftone *cal_ht = NULL;
    gx_dda_fixed dda_ht;
    cal_context *ctx = penum->memory->gs_lib_ctx->core->cal_ctx;
    int k;
    gx_ht_order *d_order;
    int code;
    byte *cache = (penum->color_cache != NULL ? penum->color_cache->device_contone : NULL);
    cal_matrix matrix;
    int clip_x, clip_y;

    if (!gx_device_must_halftone(penum->dev))
        return NULL;

    if (penum->pgs == NULL || penum->pgs->dev_ht == NULL)
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
    cal_ht = cal_halftone_init(ctx,
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

    for (k = 0; k < penum->pgs->dev_ht->num_comp; k++) {
        d_order = &(penum->pgs->dev_ht->components[k].corder);
        code = gx_ht_construct_threshold(d_order, penum->dev, penum->pgs, k);
        if (code < 0)
            goto fail;
        if (cal_halftone_add_screen(ctx,
                                    penum->memory->non_gc_memory,
                                    cal_ht,
                                    penum->pgs->dev_ht->components[k].corder.threshold_inverted,
                                    penum->pgs->dev_ht->components[k].corder.width,
                                    penum->pgs->dev_ht->components[k].corder.full_height,
                                    -penum->pgs->screen_phase[k].x,
                                    -penum->pgs->screen_phase[k].y,
                                    penum->pgs->dev_ht->components[k].corder.threshold) < 0)
            goto fail;
    }

    return cal_ht;

fail:
    cal_halftone_fin(cal_ht, penum->memory->non_gc_memory);
    return NULL;
}
#endif

static int image_skip_color_icc_tpr(gx_image_enum *penum, gx_device *dev);

int
gs_image_class_4_color(gx_image_enum * penum, irender_proc_t *render_fn)
{
    int code = 0;
#if USE_FAST_HT_CODE
    bool use_fast_thresh = true;
#else
    bool use_fast_thresh = false;
#endif
    const gs_color_space *pcs;
    gsicc_rendering_param_t rendering_params;
    int k;
    int src_num_comp = cs_num_components(penum->pcs);
    int des_num_comp, bpc;
    cmm_dev_profile_t *dev_profile;

    if (penum->use_mask_color) {
        /*
         * Scale the mask colors to match the scaling of each sample to
         * a full byte, and set up the quick-filter parameters.
         */
        int i;
        color_samples mask, test;
        bool exact = penum->spp <= BYTES_PER_BITS32;

        memset(&mask, 0, sizeof(mask));
        memset(&test, 0, sizeof(test));
        for (i = 0; i < penum->spp; ++i) {
            byte v0, v1;
            byte match = 0xff;

            gx_image_scale_mask_colors(penum, i);
            v0 = (byte)penum->mask_color.values[2 * i];
            v1 = (byte)penum->mask_color.values[2 * i + 1];
            while ((v0 & match) != (v1 & match))
                match <<= 1;
            mask.v[i] = match;
            test.v[i] = v0 & match;
            exact &= (v0 == match && (v1 | match) == 0xff);
        }
        penum->mask_color.mask = mask.all[0];
        penum->mask_color.test = test.all[0];
        penum->mask_color.exact = exact;
    } else {
        penum->mask_color.mask = 0;
        penum->mask_color.test = ~0;
    }
    /* If the device has some unique color mapping procs due to its color space,
       then we will need to use those and go through pixel by pixel instead
       of blasting through buffers.  This is true for example with many of
       the color spaces for CUPs */
    if ( (gs_color_space_get_index(penum->pcs) == gs_color_space_index_DeviceN &&
        penum->pcs->cmm_icc_profile_data == NULL) || penum->use_mask_color) {
         *render_fn = &image_render_color_DeviceN;
         return 0;
    }

    /* Set up the link now */
    code = dev_proc(penum->dev, get_profile)(penum->dev, &dev_profile);
    if (code < 0)
        return code;

    des_num_comp = gsicc_get_device_profile_comps(dev_profile);
    bpc = penum->dev->color_info.depth / des_num_comp;	/* bits per component */
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
        pcs = penum->pcs;
    }
    penum->icc_setup.is_lab = pcs->cmm_icc_profile_data->islab;
    penum->icc_setup.must_halftone = gx_device_must_halftone(penum->dev);
    penum->icc_setup.has_transfer = gx_has_transfer(penum->pgs, des_num_comp);
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
    if (!gx_device_uses_std_cmap_procs(penum->dev, penum->pgs)) {
        *render_fn = &image_render_color_DeviceN;
        return code;
    }
    if (gx_device_must_halftone(penum->dev) && use_fast_thresh &&
        (penum->posture == image_portrait || penum->posture == image_landscape)
        && penum->image_parent_type == gs_image_type1) {
        bool transfer_is_monotonic = true;

        for (k=0; k<des_num_comp; k++) {
            if (!gx_transfer_is_monotonic(penum->pgs, k)) {
                transfer_is_monotonic = false;
                break;
            }
        }
        /* If num components is 1 or if we are going to CMYK planar device
           then we will may use the thresholding if it is a halftone
           device IFF we have one bit per component */
        if ((bpc == 1) && transfer_is_monotonic &&
            (penum->dev->color_info.num_components == 1 || penum->dev->is_planar) &&
            penum->bps == 8) {
#ifdef WITH_CAL
            penum->cal_ht = color_halftone_init(penum);
            if (penum->cal_ht != NULL)
            {
                penum->skip_next_line = image_render_color_ht_cal_skip_line;
                *render_fn = &image_render_color_ht_cal;
                return code;
            }
#else
            code = gxht_thresh_image_init(penum);
            if (code == 0) {
                 /* NB: transfer function is pickled into the threshold arrray */
                 penum->icc_setup.has_transfer = false;
                 *render_fn = &image_render_color_thresh;
                 return code;
            }
#endif
        }
    }
    {
        gs_int_rect rect;
        transform_pixel_region_data data;
        data.u.init.clip = &rect;
        data.u.init.w = penum->rect.w;
        data.u.init.h = penum->rect.h;
        data.u.init.pixels = &penum->dda.pixel0;
        data.u.init.rows = &penum->dda.row;
        data.u.init.lop = penum->log_op;
        rect.p.x = fixed2int(penum->clip_outer.p.x);
        rect.p.y = fixed2int(penum->clip_outer.p.y);
        rect.q.x = fixed2int_ceiling(penum->clip_outer.q.x);
        rect.q.y = fixed2int_ceiling(penum->clip_outer.q.y);

        if (penum->icc_link == NULL || (penum->icc_link->is_identity && !penum->icc_setup.need_decode))
            data.u.init.spp = penum->spp;
        else
            data.u.init.spp = des_num_comp;

        code = dev_proc(penum->dev, transform_pixel_region)(penum->dev, transform_pixel_region_begin, &data);
        if (code >= 0) {
            penum->tpr_state = data.state;
            penum->skip_next_line = image_skip_color_icc_tpr;
            *render_fn = &image_render_color_icc_tpr;
            return code;
        }
    }
    return code;
}

/* ------ Rendering procedures ------ */

/* Test whether a color is transparent. */
static bool
mask_color_matches(const byte *v, const gx_image_enum *penum,
                   int num_components)
{
    int i;

    for (i = num_components * 2, v += num_components - 1; (i -= 2) >= 0; --v)
        if (*v < penum->mask_color.values[i] ||
            *v > penum->mask_color.values[i + 1]
            )
            return false;
    return true;
}

static inline float
rescale_input_color(gs_range range, float input)
{
    return((input-range.rmin)/(range.rmax-range.rmin));
}

/* This one includes an extra adjustment for the CIE PS color space
   non standard range */
static void
decode_row_cie(const gx_image_enum *penum, const byte *psrc, int spp, byte *pdes,
                byte *bufend, gs_range range_array[])
{
    byte *curr_pos = pdes;
    int k;
    float temp;

    while ( curr_pos < bufend ) {
        for ( k = 0; k < spp; k ++ ) {
            switch ( penum->map[k].decoding ) {
                case sd_none:
                    *curr_pos = *psrc;
                    break;
                case sd_lookup:
                    temp = penum->map[k].decode_lookup[(*psrc) >> 4]*255.0;
                    temp = rescale_input_color(range_array[k], temp);
                    temp = temp*255;
                    if (temp > 255) temp = 255;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned char) temp;
                    break;
                case sd_compute:
                    temp = penum->map[k].decode_base +
                        (*psrc) * penum->map[k].decode_factor;
                    temp = rescale_input_color(range_array[k], temp);
                    temp = temp*255;
                    if (temp > 255) temp = 255;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned char) temp;
                default:
                    break;
            }
            curr_pos++;
            psrc++;
        }
    }
}

static void
decode_row(const gx_image_enum *penum, const byte *psrc, int spp, byte *pdes,
                byte *bufend)
{
    byte *curr_pos = pdes;
    int k;
    float temp;

    while ( curr_pos < bufend ) {
        for ( k = 0; k < spp; k ++ ) {
            switch ( penum->map[k].decoding ) {
                case sd_none:
                    *curr_pos = *psrc;
                    break;
                case sd_lookup:
                    temp = penum->map[k].decode_lookup[(*psrc) >> 4]*255.0;
                    if (temp > 255) temp = 255;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned char) temp;
                    break;
                case sd_compute:
                    temp = penum->map[k].decode_base +
                        (*psrc) * penum->map[k].decode_factor;
                    temp *= 255;
                    if (temp > 255) temp = 255;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned char) temp;
                default:
                    break;
            }
            curr_pos++;
            psrc++;
        }
    }
}

/* Common code shared amongst the thresholding and non thresholding color image
   renderers */
static int
image_color_icc_prep(gx_image_enum *penum_orig, const byte *psrc, uint w,
                     gx_device *dev, int *spp_cm_out, byte **psrc_cm,
                     byte **psrc_cm_start, byte **bufend, int *pspan, bool planar_out)
{
    const gx_image_enum *const penum = penum_orig; /* const within proc */
    const gs_gstate *pgs = penum->pgs;
    bool need_decode = penum->icc_setup.need_decode;
    gsicc_bufferdesc_t input_buff_desc;
    gsicc_bufferdesc_t output_buff_desc;
    int spp_cm;
    int spp = penum->spp;
    bool force_planar = false;
    int num_des_comps;
    int code;
    cmm_dev_profile_t *dev_profile;
    byte *psrc_decode;
    const byte *planar_src;
    byte *planar_des;
    int j, k;

    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0) return code;
    num_des_comps = gsicc_get_device_profile_comps(dev_profile);
    if (penum->icc_link == NULL) {
        return gs_rethrow(-1, "ICC Link not created during image render color");
    }
    if (pspan)
        *pspan = w;
    /* If the link is the identity, then we don't need to do any color
       conversions except for potentially a decode.  Planar out is a special
       case. For now we let the CMM do the reorg into planar.  We will want
       to optimize this to do something special when we have the identity
       transform for CM and going out to a planar CMYK device */
    if (num_des_comps != 1 && planar_out == true) {
        force_planar = true;
    }
    if (penum->icc_link->is_identity && !need_decode && !force_planar) {
        /* Fastest case.  No decode or CM needed */
        *psrc_cm = (unsigned char *) psrc;
        spp_cm = spp;
        *bufend = *psrc_cm + w;
        *psrc_cm_start = NULL;
    } else {
        int width = w/spp;
        int span = (width+31)&~31;
        spp_cm = num_des_comps;

        if (pspan)
            *pspan = span;
        /* Put the buffer on a 32 byte memory alignment for SSE/AVX for every
         * line. Also extra space for 32 byte overrun. */
        *psrc_cm_start = gs_alloc_bytes(pgs->memory,  span * spp_cm + 64,
                                        "image_color_icc_prep");
        *psrc_cm = *psrc_cm_start + ((32 - (intptr_t)(*psrc_cm_start)) & 31);
        *bufend = *psrc_cm +  span * spp_cm;
        if (penum->icc_link->is_identity) {
            if (!force_planar) {
                /* decode only. no CM.  This is slow but does not happen that often */
                decode_row(penum, psrc, spp, *psrc_cm, *bufend);
            } else {
                /* CM is identity but we may need to do decode and then off
                   to planar. The planar out case is only used when coming from
                   imager_render_color_thresh, which is limited to 8 bit case */
                if (need_decode) {
                    /* Need decode and then to planar */
                    psrc_decode = gs_alloc_bytes(pgs->memory,  w,
                                                  "image_color_icc_prep");
                    if (!penum->use_cie_range) {
                        decode_row(penum, psrc, spp, psrc_decode, psrc_decode+w);
                    } else {
                        /* Decode needs to include adjustment for CIE range */
                        decode_row_cie(penum, psrc, spp, psrc_decode,
                                        psrc_decode + w, get_cie_range(penum->pcs));
                    }
                    planar_src = psrc_decode;
                } else {
                    psrc_decode = NULL;
                    planar_src = psrc;
                }
                /* Now to planar */
                planar_des = *psrc_cm;
                for (k = 0; k < width; k++) {
                    for (j = 0; j < spp; j++) {
                        *(planar_des + j * span) = *planar_src++;
                    }
                    planar_des++;
                }
                /* Free up decode if we used it */
                if (psrc_decode != NULL) {
                    gs_free_object(pgs->memory, (byte *) psrc_decode,
                                   "image_render_color_icc");
                }
            }
        } else {
            /* Set up the buffer descriptors. planar out always ends up here */
            gsicc_init_buffer(&input_buff_desc, spp, 1,
                          false, false, false, 0, w,
                          1, width);
            if (!force_planar) {
                gsicc_init_buffer(&output_buff_desc, spp_cm, 1,
                              false, false, false, 0, width * spp_cm,
                              1, width);
            } else {
                gsicc_init_buffer(&output_buff_desc, spp_cm, 1,
                              false, false, true, span, span,
                              1, width);
            }
            /* For now, just blast it all through the link. If we had a significant reduction
               we will want to repack the data first and then do this.  That will be
               an optimization shortly.  For now just allocate a new output
               buffer.  We can reuse the old one if the number of channels in the output is
               less than or equal to the new one.  */
            if (need_decode) {
                /* Need decode and CM.  This is slow but does not happen that often */
                psrc_decode = gs_alloc_bytes(pgs->memory, w,
                                              "image_color_icc_prep");
                if (!penum->use_cie_range) {
                    decode_row(penum, psrc, spp, psrc_decode, psrc_decode+w);
                } else {
                    /* Decode needs to include adjustment for CIE range */
                    decode_row_cie(penum, psrc, spp, psrc_decode,
                                    psrc_decode+w, get_cie_range(penum->pcs));
                }
                (penum->icc_link->procs.map_buffer)(dev, penum->icc_link,
                                                    &input_buff_desc,
                                                    &output_buff_desc,
                                                    (void*) psrc_decode,
                                                    (void*) *psrc_cm);
                gs_free_object(pgs->memory, psrc_decode, "image_color_icc_prep");
            } else {
                /* CM only. No decode */
                (penum->icc_link->procs.map_buffer)(dev, penum->icc_link,
                                                    &input_buff_desc,
                                                    &output_buff_desc,
                                                    (void*) psrc,
                                                    (void*) *psrc_cm);
            }
        }
    }
    *spp_cm_out = spp_cm;
    return 0;
}

#ifdef WITH_CAL
static int
image_render_color_ht_cal(gx_image_enum *penum, const byte *buffer, int data_x,
                          uint w, int h, gx_device * dev)
{
    const byte *psrc = buffer + data_x;
    int code = 0;
    int spp_cm = 0;
    byte *psrc_cm = NULL, *psrc_cm_start = NULL;
    byte *bufend = NULL;
    byte *input[GX_DEVICE_COLOR_MAX_COMPONENTS];    /* to ensure 128 bit boundary */
    int i;
    int planestride;

    if (h == 0)
        return 0;

    /* Get the buffer into the device color space */
    code = image_color_icc_prep(penum, psrc, w, dev, &spp_cm, &psrc_cm,
                                &psrc_cm_start,  &bufend, &planestride, true);
    if (code < 0)
        return code;

    for (i = 0; i < spp_cm; i++)
        input[i] = psrc_cm + w*i;

    code = cal_halftone_process_planar(penum->cal_ht, penum->memory->non_gc_memory,
                                       (const byte * const *)input, color_halftone_callback, dev);

    /* Free cm buffer, if it was used */
    if (psrc_cm_start != NULL) {
        gs_free_object(penum->pgs->memory, (byte *)psrc_cm_start,
                       "image_render_color_thresh");
    }
    return code;
}
#else
static int
image_render_color_thresh(gx_image_enum *penum_orig, const byte *buffer, int data_x,
                          uint w, int h, gx_device * dev)
{
    gx_image_enum *penum = penum_orig; /* const within proc */
    image_posture posture = penum->posture;
    int vdi;  /* amounts to replicate */
    fixed xrun = 0;
    byte *thresh_align;
    byte *devc_contone[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte *psrc_plane[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte *devc_contone_gray;
    const byte *psrc = buffer + data_x;
    int dest_width, dest_height, data_length;
    int spp_out = dev->color_info.num_components;
    int position, i, j, k;
    int offset_bits = penum->ht_offset_bits;
    int contone_stride = 0;  /* Not used in landscape case */
    fixed offset;
    int src_size;
    bool flush_buff = false;
    byte *psrc_temp;
    int offset_contone[GX_DEVICE_COLOR_MAX_COMPONENTS];    /* to ensure 128 bit boundary */
    int offset_threshold;  /* to ensure 128 bit boundary */
    gx_dda_fixed dda_ht;
    int xn, xr;		/* destination position (pixel, not contone buffer offset) */
    int code = 0;
    int spp_cm = 0;
    byte *psrc_cm = NULL, *psrc_cm_start = NULL;
    byte *bufend = NULL;
    int psrc_planestride = w/penum->spp;

    if (h != 0 && penum->line_size != 0) {      /* line_size == 0, nothing to do */
        /* Get the buffer into the device color space */
        code = image_color_icc_prep(penum, psrc, w, dev, &spp_cm, &psrc_cm,
                                    &psrc_cm_start,  &bufend, &psrc_planestride, true);
        if (code < 0)
            return code;
    } else {
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
    /* Data is now in the proper destination color space.  Now we want
       to go ahead and get the data into the proper spatial setting and then
       threshold.  First get the data spatially sampled correctly */
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
            offset_threshold = (- (((long)(penum->thresh_buffer)) +
                                      penum->ht_offset_bits)) & 15;
            for (k = 0; k < spp_out; k ++) {
                offset_contone[k]   = (- (((long)(penum->line)) +
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
            offset_threshold = (-(long)(penum->thresh_buffer)) & 15;
            for (k = 0; k < spp_out; k ++) {
                offset_contone[k]   = (- ((long)(penum->line) +
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
        psrc_plane[k] = psrc_cm + psrc_planestride * k;
    }
    xr = fixed2int_var_rounded(dda_current(dda_ht));	/* indexes in the destination (contone) */

    /* Do conversion to device resolution in quick small loops. */
    /* For now we have 3 cases.  A CMYK (4 channel), gray, or other case
       the latter of which is not yet implemented */
    switch (spp_out)
    {
        /* Monochrome output case */
        case 1:
            devc_contone_gray = devc_contone[0];
            switch (posture) {
                /* Monochrome portrait */
                case image_portrait:
                    if (penum->dst_width > 0) {
                        if (src_size == dest_width) {
                            memcpy(devc_contone_gray, psrc_cm, data_length);
                        } else if (src_size * 2 == dest_width) {
                            psrc_temp = psrc_cm;
                            for (k = 0; k < data_length; k+=2,
                                 devc_contone_gray+=2, psrc_temp++) {
                                *devc_contone_gray =
                                    *(devc_contone_gray+1) = *psrc_temp;
                            }
                        } else {
                            /* Mono case, forward */
                            psrc_temp = psrc_cm;
                            for (k=0; k<src_size; k++) {
                                dda_next(dda_ht);
                                xn = fixed2int_var_rounded(dda_current(dda_ht));
                                while (xr < xn) {
                                    *devc_contone_gray++ = *psrc_temp;
                                    xr++;
                                }           /* at loop exit xn will be >= xr */
                                psrc_temp++;
                            }
                        }
                    } else {
                        /* Mono case, backwards */
                        devc_contone_gray += (data_length - 1);
                        psrc_temp = psrc_cm;
                        for (k=0; k<src_size; k++) {
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                *devc_contone_gray-- = *psrc_temp;
                                xr--;
                            }           /* at loop exit xn will be >= xr */
                            psrc_temp++;
                        }
                    }
                    break;
                /* Monochrome landscape */
                case image_landscape:
                    /* We store the data at this point into a column. Depending
                       upon our landscape direction we may be going left to right
                       or right to left. */
                    if (penum->ht_landscape.flipy) {
                        position = penum->ht_landscape.curr_pos +
                                    LAND_BITS * (data_length - 1);
                        psrc_temp = psrc_cm;
                        for (k=0; k<src_size; k++) {
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                devc_contone_gray[position] = *psrc_temp;
                                position -= LAND_BITS;
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                            psrc_temp++;
                        }
                    } else {
                        position = penum->ht_landscape.curr_pos;
                        /* Code up special cases for when we have no scaling
                           and 2x scaling which we will run into in 300 and
                           600dpi devices and content */
                        if (src_size == dest_height) {
                            for (k = 0; k < data_length; k++) {
                                devc_contone_gray[position] = psrc_cm[k];
                                position += LAND_BITS;
                            }
                        } else if (src_size*2 == dest_height) {
                            for (k = 0; k < data_length; k+=2) {
                                offset = fixed2int_var_rounded(fixed_half * k);
                                devc_contone_gray[position] =
                                    devc_contone_gray[position + LAND_BITS] =
                                    psrc_cm[offset];
                                position += 2*LAND_BITS;
                            }
                        } else {
                            /* use dda */
                            psrc_temp = psrc_cm;
                            for (k=0; k<src_size; k++) {
                                dda_next(dda_ht);
                                xn = fixed2int_var_rounded(dda_current(dda_ht));
                                while (xr < xn) {
                                    devc_contone_gray[position] = *psrc_temp;
                                    position += LAND_BITS;
                                    xr++;
                                }           /* at loop exit xn will be >= xr */
                                psrc_temp++;
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
        break;

        /* CMYK case */
        case 4:
            switch (posture) {
                /* CMYK portrait */
                case image_portrait:
                    if (penum->dst_width > 0) {
                        if (src_size == dest_width) {
                            memcpy(devc_contone[0], psrc_plane[0], data_length);
                            memcpy(devc_contone[1], psrc_plane[1], data_length);
                            memcpy(devc_contone[2], psrc_plane[2], data_length);
                            memcpy(devc_contone[3], psrc_plane[3], data_length);
                        } else if (src_size * 2 == dest_width) {
                            for (k = 0; k < data_length; k+=2) {
                                *(devc_contone[0]) = *(devc_contone[0]+1) =
                                    *psrc_plane[0]++;
                                *(devc_contone[1]) = *(devc_contone[1]+1) =
                                    *psrc_plane[1]++;
                                *(devc_contone[2]) = *(devc_contone[2]+1) =
                                    *psrc_plane[2]++;
                                *(devc_contone[3]) = *(devc_contone[3]+1) =
                                    *psrc_plane[3]++;
                                devc_contone[0] += 2;
                                devc_contone[1] += 2;
                                devc_contone[2] += 2;
                                devc_contone[3] += 2;
                            }
                        } else {
                        /* CMYK case, forward */
                            for (k=0, j=0; k<src_size; k++) {
                                dda_next(dda_ht);
                                xn = fixed2int_var_rounded(dda_current(dda_ht));
                                while (xr < xn) {
                                    *(devc_contone[0])++ = (psrc_plane[0])[j];
                                    *(devc_contone[1])++ = (psrc_plane[1])[j];
                                    *(devc_contone[2])++ = (psrc_plane[2])[j];
                                    *(devc_contone[3])++ = (psrc_plane[3])[j];
                                    xr++;
                                }           /* at loop exit xn will be >= xr */
                                j++;
                            }
                        }
                    } else {
                        /* CMYK case, backwards */
                        /* Move to the other end and we will decrement */
                        devc_contone[0] += (data_length - 1);
                        devc_contone[1] += (data_length - 1);
                        devc_contone[2] += (data_length - 1);
                        devc_contone[3] += (data_length - 1);
                        for (k=0, j=0; k<src_size; k++) {
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                *(devc_contone[0])-- = (psrc_plane[0])[j];
                                *(devc_contone[1])-- = (psrc_plane[1])[j];
                                *(devc_contone[2])-- = (psrc_plane[2])[j];
                                *(devc_contone[3])-- = (psrc_plane[3])[j];
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                            j++;
                        }
                    }
                    break;
                /* CMYK landscape */
                case image_landscape:
                    /* Data is already color managed. */
                    /* We store the data at this point into columns in
                       seperate planes. Depending upon our landscape direction
                       we may be going left to right or right to left. */
                    if (penum->ht_landscape.flipy) {
                        position = penum->ht_landscape.curr_pos +
                                    LAND_BITS * (data_length - 1);
                        /* use dda */
                        for (k=0, i=0; k<src_size; k++) {
                            dda_next(dda_ht);
                            xn = fixed2int_var_rounded(dda_current(dda_ht));
                            while (xr > xn) {
                                for (j = 0; j < spp_out; j++)
                                    *(devc_contone[j] + position) = (psrc_plane[j])[i];
                                position -= LAND_BITS;
                                xr--;
                            }           /* at loop exit xn will be <= xr */
                            i++;
                        }
                    } else {
                        position = penum->ht_landscape.curr_pos;
                        /* Code up special cases for when we have no scaling
                           and 2x scaling which we will run into in 300 and
                           600dpi devices and content */
                        /* Apply initial offset */
                        for (k = 0; k < spp_out; k++)
                            devc_contone[k] = devc_contone[k] + position;
                        if (src_size == dest_height) {
                            for (k = 0; k < data_length; k++) {
                                /* Is it better to unwind this?  We know it is 4 */
                                for (j = 0; j < spp_out; j++) {
                                    *(devc_contone[j]) = (psrc_plane[j])[k];
                                    devc_contone[j] += LAND_BITS;
                                }
                            }
                        } else if (src_size*2 == dest_height) {
                            for (k = 0; k < data_length; k+=2) {
                                offset = fixed2int_var_rounded(fixed_half * k);
                                /* Is it better to unwind this?  We know it is 4 */
                                for (j = 0; j < spp_out; j++) {
                                    *(devc_contone[j]) =
                                      *(devc_contone[j] + LAND_BITS) =
                                      (psrc_plane[j])[offset];
                                    devc_contone[j] += 2 * LAND_BITS;
                                }
                            }
                        } else {
                            /* use dda */
                            for (k=0, i=0; k<src_size; k++) {
                                dda_next(dda_ht);
                                xn = fixed2int_var_rounded(dda_current(dda_ht));
                                while (xr > xn) {
                                    for (j = 0; j < spp_out; j++)
                                        *(devc_contone[j] + position) = (psrc_plane[j])[i];
                                    position -= LAND_BITS;
                                    xr--;
                                }           /* at loop exit xn will be <= xr */
                                i++;
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
        break;
        default:
            /* Not yet handled (e.g. CMY case) */
        break;
    }
    /* Apply threshold array to image data. It may be neccessary to invert
       depnding upon the polarity of the device */
flush:
    thresh_align = penum->thresh_buffer + offset_threshold;
    code = gxht_thresh_planes(penum, xrun, dest_width, dest_height,
                              thresh_align, dev, offset_contone,
                              contone_stride);
    /* Free cm buffer, if it was used */
    if (psrc_cm_start != NULL) {
        gs_free_object(penum->pgs->memory, (byte *)psrc_cm_start,
                       "image_render_color_thresh");
    }
    return code;
}
#endif

/* Render a color image with 8 or fewer bits per sample using ICC profile. */
static int
image_skip_color_icc_tpr(gx_image_enum *penum, gx_device *dev)
{
    transform_pixel_region_data data;
    data.state = penum->tpr_state;
    return dev_proc(dev, transform_pixel_region)(dev, transform_pixel_region_data_needed, &data) == 0;
}

static int
image_render_color_icc_tpr(gx_image_enum *penum_orig, const byte *buffer, int data_x,
                          uint w, int h, gx_device * dev)
{
    const gx_image_enum *const penum = penum_orig; /* const within proc */
    const gs_gstate *pgs = penum->pgs;
    int spp = penum->spp;
    const byte *psrc = buffer + data_x * spp;
    int code;
    byte *psrc_cm = NULL, *psrc_cm_start = NULL;
    byte *psrc_cm_initial;
    byte *bufend = NULL;
    int spp_cm = 0;
    bool must_halftone = penum->icc_setup.must_halftone;
    bool has_transfer = penum->icc_setup.has_transfer;
    gx_cmapper_t cmapper;
    transform_pixel_region_data data;

    if (h == 0)
        return 0;
    code = image_color_icc_prep(penum_orig, psrc, w, dev, &spp_cm, &psrc_cm,
                                &psrc_cm_start, &bufend, NULL, false);
    if (code < 0) return code;
    psrc_cm_initial = psrc_cm;
    gx_get_cmapper(&cmapper, pgs, dev, has_transfer, must_halftone, gs_color_select_source);

    data.state = penum->tpr_state;
    data.u.process_data.buffer[0] = psrc_cm;
    data.u.process_data.data_x = 0;
    data.u.process_data.cmapper = &cmapper;
    code = dev_proc(dev, transform_pixel_region)(dev, transform_pixel_region_process_data, &data);
    gs_free_object(pgs->memory, (byte *)psrc_cm_start, "image_render_color_icc");

    if (code < 0) {
        /* Save position if error, in case we resume. */
        penum_orig->used.x = (data.u.process_data.buffer[0] - psrc_cm_initial) / spp_cm;
        penum_orig->used.y = 0;
    }
    return code;
}

/* Render a color image for deviceN source color with no ICC profile.  This
   is also used if the image has any masking (type4 image) since we will not
   be blasting through quickly */
static int
image_render_color_DeviceN(gx_image_enum *penum_orig, const byte *buffer, int data_x,
                   uint w, int h, gx_device * dev)
{
    const gx_image_enum *const penum = penum_orig; /* const within proc */
    const gs_gstate *pgs = penum->pgs;
    gs_logical_operation_t lop = penum->log_op;
    gx_dda_fixed_point pnext;
    image_posture posture = penum->posture;
    fixed xprev, yprev;
    fixed pdyx, pdyy;		/* edge of parallelogram */
    int vci, vdi;
    const gs_color_space *pcs = penum->pcs;
    cs_proc_remap_color((*remap_color)) = pcs->type->remap_color;
    gs_client_color cc;
    gx_device_color devc1;
    gx_device_color devc2;
    gx_device_color *pdevc;
    gx_device_color *pdevc_next;
    gx_device_color *ptemp;
    int spp = penum->spp;
    const byte *psrc_initial = buffer + data_x * spp;
    const byte *psrc = psrc_initial;
    const byte *rsrc = psrc + spp; /* psrc + spp at start of run */
    fixed xrun;			/* x ditto */
    fixed yrun;			/* y ditto */
    int irun;			/* int x/rrun */
    color_samples run;		/* run value */
    color_samples next;		/* next sample value */
    const byte *bufend = psrc + w;
    int code = 0, mcode = 0;
    int i;
    bits32 mask = penum->mask_color.mask;
    bits32 test = penum->mask_color.test;
    bool lab_case = false;

    if (h == 0)
        return 0;

    /* Decide on which remap proc to use.  If the source colors are LAB
       then use the mapping that does not rescale the source colors */
    if (gs_color_space_is_ICC(pcs) && pcs->cmm_icc_profile_data != NULL &&
        pcs->cmm_icc_profile_data->islab) {
        remap_color = gx_remap_ICC_imagelab;
        lab_case = true;
    } else {
        remap_color = pcs->type->remap_color;
    }
    pdevc = &devc1;
    pdevc_next = &devc2;
    /* In case these are devn colors */
    if (dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < GS_CLIENT_COLOR_MAX_COMPONENTS; i++) {
            pdevc->colors.devn.values[i] = 0;
            pdevc_next->colors.devn.values[i] = 0;
        }
    }
    /* These used to be set by init clues */
    pdevc->type = gx_dc_type_none;
    pdevc_next->type = gx_dc_type_none;
    pnext = penum->dda.pixel0;
    xrun = xprev = dda_current(pnext.x);
    yrun = yprev = dda_current(pnext.y);
    pdyx = dda_current(penum->dda.row.x) - penum->cur.x;
    pdyy = dda_current(penum->dda.row.y) - penum->cur.y;
    switch (posture) {
        case image_portrait:
            vci = penum->yci, vdi = penum->hci;
            irun = fixed2int_var_rounded(xrun);
            break;
        case image_landscape:
        default:    /* we don't handle skew -- treat as landscape */
            vci = penum->xci, vdi = penum->wci;
            irun = fixed2int_var_rounded(yrun);
            break;
    }
    if_debug5m('b', penum->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               penum->y, data_x, w, fixed2float(xprev), fixed2float(yprev));
    memset(&run, 0, sizeof(run));
    memset(&next, 0, sizeof(next));
    cs_full_init_color(&cc, pcs);
    run.v[0] = ~psrc[0];	/* force remap */
    while (psrc < bufend) {
        dda_next(pnext.x);
        dda_next(pnext.y);
        if (posture != image_skewed && !memcmp(psrc, run.v, spp)) {
            psrc += spp;
            goto inc;
        }
        memcpy(next.v, psrc, spp);
        psrc += spp;
    /* Check for transparent color. */
        if ((next.all[0] & mask) == test &&
            (penum->mask_color.exact ||
             mask_color_matches(next.v, penum, spp))) {
            color_set_null(pdevc_next);
            goto mapped;
        }
        /* Data is already properly set up for ICC use of LAB */
        if (lab_case)
            for (i = 0; i < spp; ++i)
                cc.paint.values[i] = (next.v[i]) * (1.0f / 255.0f);
        else
            for (i = 0; i < spp; ++i)
                decode_sample(next.v[i], cc, i);
#ifdef DEBUG
        if (gs_debug_c('B')) {
            dmprintf2(dev->memory, "[B]cc[0..%d]=%g", spp - 1,
                     cc.paint.values[0]);
            for (i = 1; i < spp; ++i)
                dmprintf1(dev->memory, ",%g", cc.paint.values[i]);
            dmputs(dev->memory, "\n");
        }
#endif
        if (lab_case || penum->icc_link == NULL || pcs->cmm_icc_profile_data == NULL)
            mcode = remap_color(&cc, pcs, pdevc_next, pgs, dev, gs_color_select_source);
        else
            mcode = gx_remap_ICC_with_link(&cc, pcs, pdevc_next, pgs, dev,
                                           gs_color_select_source, penum->icc_link);

mapped:	if (mcode < 0)
            goto fill;
        if (sizeof(pdevc_next->colors.binary.color[0]) <= sizeof(ulong))
            if_debug7m('B', penum->memory,
                       "[B]0x%x,0x%x,0x%x,0x%x -> 0x%lx,0x%lx,0x%lx\n",
                       next.v[0], next.v[1], next.v[2], next.v[3],
                       (ulong)pdevc_next->colors.binary.color[0],
                       (ulong)pdevc_next->colors.binary.color[1],
                       (ulong) pdevc_next->type);
        else
            if_debug9m('B', penum->memory,
                       "[B]0x%x,0x%x,0x%x,0x%x -> 0x%08lx%08lx,0x%08lx%08lx,0x%lx\n",
                       next.v[0], next.v[1], next.v[2], next.v[3],
                       (ulong)(pdevc_next->colors.binary.color[0] >>
                               8 * (sizeof(pdevc_next->colors.binary.color[0]) - sizeof(ulong))),
                       (ulong)pdevc_next->colors.binary.color[0],
                       (ulong)(pdevc_next->colors.binary.color[1] >>
                               8 * (sizeof(pdevc_next->colors.binary.color[1]) - sizeof(ulong))),
                       (ulong)pdevc_next->colors.binary.color[1],
                       (ulong) pdevc_next->type);
        /* NB: printf above fails to account for sizeof gx_color_index 4 or 8 bytes */
        if (posture != image_skewed && dev_color_eq(*pdevc, *pdevc_next))
            goto set;
fill:	/* Fill the region between */
        /* xrun/irun and xprev */
        /*
         * Note;  This section is nearly a copy of a simlar section below
         * for processing the last image pixel in the loop.  This would have been
         * made into a subroutine except for complications about the number of
         * variables that would have been needed to be passed to the routine.
         */
        switch (posture) {
        case image_portrait:
            {		/* Rectangle */
                int xi = irun;
                int wi = (irun = fixed2int_var_rounded(xprev)) - xi;

                if (wi < 0)
                    xi += wi, wi = -wi;
                if (wi > 0)
                    code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
                                                        pdevc, dev, lop);
            }
            break;
        case image_landscape:
            {		/* 90 degree rotated rectangle */
                int yi = irun;
                int hi = (irun = fixed2int_var_rounded(yprev)) - yi;

                if (hi < 0)
                    yi += hi, hi = -hi;
                if (hi > 0)
                    code = gx_fill_rectangle_device_rop(vci, yi, vdi, hi,
                                                        pdevc, dev, lop);
            }
            break;
        default:
            {		/* Parallelogram */
                code = (*dev_proc(dev, fill_parallelogram))
                    (dev, xrun, yrun, xprev - xrun, yprev - yrun, pdyx, pdyy,
                     pdevc, lop);
                xrun = xprev;
                yrun = yprev;
            }
        }
        if (code < 0)
            goto err;
        rsrc = psrc;

        if ((code = mcode) < 0) goto err;
        /* Swap around the colors due to a change */
        ptemp = pdevc;
        pdevc = pdevc_next;
        pdevc_next = ptemp;
set:	run = next;
inc:	xprev = dda_current(pnext.x);
        yprev = dda_current(pnext.y);	/* harmless if no skew */
    }
    /* Fill the last run. */
    /*
     * Note;  This section is nearly a copy of a simlar section above
     * for processing an image pixel in the loop.  This would have been
     * made into a subroutine except for complications about the number
     * variables that would have been needed to be passed to the routine.
     */
    switch (posture) {
        case image_portrait:
            {		/* Rectangle */
                int xi = irun;
                int wi = (irun = fixed2int_var_rounded(xprev)) - xi;

                if (wi < 0)
                    xi += wi, wi = -wi;
                if (wi > 0)
                    code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
                                                        pdevc, dev, lop);
            }
            break;
        case image_landscape:
            {		/* 90 degree rotated rectangle */
                int yi = irun;
                int hi = (irun = fixed2int_var_rounded(yprev)) - yi;

                if (hi < 0)
                    yi += hi, hi = -hi;
                if (hi > 0)
                    code = gx_fill_rectangle_device_rop(vci, yi, vdi, hi,
                                                        pdevc, dev, lop);
            }
            break;
        default:
            {		/* Parallelogram */
                code = (*dev_proc(dev, fill_parallelogram))
                    (dev, xrun, yrun, xprev - xrun, yprev - yrun, pdyx, pdyy,
                     pdevc, lop);
            }
    }
    return (code < 0 ? code : 1);
    /* Save position if error, in case we resume. */
err:
    penum_orig->used.x = (rsrc - spp - psrc_initial) / spp;
    penum_orig->used.y = 0;
    return code;
}
