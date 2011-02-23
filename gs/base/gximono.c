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
#include "gsicc_littlecms.h"
#include "gxcie.h"
#include "gscie.h"

#define RAW_HT_DUMP 0
#define USE_FAST_CODE 0
#define fastfloor(x) (((int)(x)) - (((x)<0) && ((x) != (float)(int)(x))))

/* This should be moved someplace else later */
#ifndef __WIN32__
#define __align16  __attribute__((align(16)))
#else
#define __align16 __declspec(align(16))
#endif

#ifdef HAVE_SSE2

#include <emmintrin.h>

static const byte bitreverse[] =
{ 0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0,
  0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
  0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4,
  0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
  0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
  0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA,
  0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6,
  0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
  0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1,
  0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9,
  0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
  0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
  0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3,
  0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
  0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7,
  0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF,
  0x3F, 0xBF, 0x7F, 0xFF};
#endif

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
    int spp_out;
    fixed ox, oy;
    int dev_width, max_height;
    int temp;

    if (penum->spp == 1) {
        /* At this point in time, only use the ht approach if our device
           uses halftoning, and our source image is a reasonable size.  We
           probably don't want to do this if we have a bunch of tiny little
           images.  Then the rect fill approach is probably not all that bad */

        if (use_fast_code && penum->pcs != NULL &&
            penum->dev->color_info.num_components == 1 &&
            penum->dev->color_info.depth == 1 &&
            penum->bps == 8) {
            spp_out = penum->dev->color_info.num_components;
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
            /* TODO: Also check that the threshold arrays are initialized */
            if (gx_device_must_halftone(penum->dev) && code >= 0) {
                if (penum->pis != NULL && penum->pis->dev_ht != NULL) {
                    gx_ht_order *d_order =
                                    &(penum->pis->dev_ht->components[0].corder);
                    code = gx_ht_construct_threshold(d_order, penum->dev, 0);
                } else {
                    code = -1;
                }
            }
            if (code >= 0 ) {
                /* If the image is landscaped then we want to maintain a buffer
                   that is sufficiently large so that we can hold a byte
                   of halftoned data along the column.  This way we avoid doing
                   multiple writes into the same position over and over.
                   The size of the buffer we need depends upon the bitdepth of
                   the output device, the number of device coloranants and the
                   number of  colorants in the source space.  Note we will
                   need to eventually  consider  multi-level halftone case
                   here too.  For now, to make use of the SSE2 stuff, we would
                   like to have 16 bytes of data to process at a time.  So we
                   will collect the columns of data in a buffer that is 16 wide.
                   We will also keep track of the widths of each column.  When
                   the total width count reaches 16, we will create our
                   threshold array and apply it.  We may have one column that is
                   buffered between calls in this case.  Also if a call is made
                   with h=0 we will flush the buffer as we are at the end of the
                   data.  */
                if (penum->posture == image_landscape) {
                    int col_length =
                        fixed2int_var_rounded(any_abs(penum->x_extent.y)) * spp_out;
                    ox = dda_current(penum->dda.pixel0.x);
                    oy = dda_current(penum->dda.pixel0.y);
                    temp = (int) ceil((float) col_length/16.0);
                    penum->line_size = temp * 16;  /* The stride */
                    /* Now we need at most 16 of these */
                    penum->line = gs_alloc_bytes(penum->memory,
                                                 16 * penum->line_size + 16,
                                                 "gs_image_class_3_mono");
                    /* Same with this */
                    penum->thresh_buffer =
                                gs_alloc_bytes(penum->memory,
                                               penum->line_size * 16  + 16,
                                               "gs_image_class_3_mono");
                    /* That maps into 2 bytes of Halftone data */
                    penum->ht_buffer =
                                    gs_alloc_bytes(penum->memory,
                                                   penum->line_size * 2,
                                                   "gs_image_class_3_mono");
                    penum->ht_stride = penum->line_size;
                    if (penum->line == NULL || penum->thresh_buffer == NULL
                                || penum->ht_buffer == NULL)
                        code = -1;
                    penum->ht_landscape.count = 0;
                    penum->ht_landscape.num_contones = 0;
                    if (penum->y_extent.x < 0) {
                        /* Going right to left */
                        penum->ht_landscape.curr_pos = 15;
                        penum->ht_landscape.index = -1;
                    } else {
                        /* Going left to right */
                        penum->ht_landscape.curr_pos = 0;
                        penum->ht_landscape.index = 1;
                    }
                    if (penum->x_extent.y < 0) {
                        penum->ht_landscape.flipy = true;
                        penum->ht_landscape.y_pos =
                            fixed2int_pixround_perfect(dda_current(penum->dda.pixel0.y) + penum->x_extent.y);
                    } else {
                        penum->ht_landscape.flipy = false;
                        penum->ht_landscape.y_pos =
                            fixed2int_pixround_perfect(dda_current(penum->dda.pixel0.y));
                    }
                    memset(&(penum->ht_landscape.widths[0]), 0, sizeof(int)*16);
                    penum->ht_landscape.offset_set = false;
                    penum->ht_offset_bits = 0; /* Will get set in call to render */
#ifdef DEBUG
                    memset(penum->line, 0, 16 * penum->line_size);
                    memset(penum->thresh_buffer, 0, 16 * penum->line_size);
                    memset(penum->ht_buffer, 0, penum->line_size * 2);
#endif
                    code = 1;  /* Landscape support off for check in */
                } else {
                    /* In the portrait case we allocate a single line buffer
                       in device width, a threshold buffer of the same size
                       and possibly wider and the buffer for the halftoned
                       bits. We have to do a bit of work to enable 16 byte
                       boundary after an offset to ensure that we can make use
                       of  the SSE2 operations for thresholding.  We do the
                       allocations now to avoid doing them with every line */
                    ox = dda_current(penum->dda.pixel0.x);
                    oy = dda_current(penum->dda.pixel0.y);
                    dev_width =
                       (int) fabs((long) fixed2long_pixround(ox + penum->x_extent.x) -
                                fixed2long_pixround(ox));
                    /* Get the bit position so that we can do a copy_mono for
                       the left remainder and then 16 bit aligned copies for the
                       rest.  The right remainder will be OK as it will land in
                       the MSBit positions. Note the #define chunk bits16 in
                       gdevm1.c.  Allow also for a 15 sample over run.
                    */
                    penum->ht_offset_bits = (-fixed2int_var_pixround(ox)) & 15;
                    if (penum->ht_offset_bits > 0) {
                        penum->ht_stride = ((7 + (dev_width + 4) * spp_out) / 8) +
                                            ARCH_SIZEOF_LONG;
                    } else {
                        penum->ht_stride = ((7 + (dev_width + 2) * spp_out) / 8) +
                                        ARCH_SIZEOF_LONG;
                    }
                    /* We want to figure out the maximum height that we may
                       have in taking a single source row and going to device
                       space */
                    max_height =
                        (int) ceil(fixed2float(any_abs(penum->dst_height)) /
                                                        (float) penum->Height);
                    penum->ht_buffer =
                                    gs_alloc_bytes(penum->memory,
                                                   penum->ht_stride * max_height,
                                                   "gs_image_class_3_mono");
                    /* We want to have 128 bit alignement for our contone and
                       threshold strips so that we can use SSE operations
                       in the threshold operation.  Add in a minor buffer and offset
                       to ensure this.  If gs_alloc_bytes provides at least 16
                       bit alignment so we may need to move 14 bytes.  However, the
                       HT process is split in two operations.  One that involves
                       the HT of a left remainder and the rest which ensures that
                       we pack in the HT data in the bits with no skew for a fast
                       copy into the gdevm1 device (16 bit copies).  So, we
                       need to account for those pixels which occur first and which
                       are NOT aligned for the contone buffer.  After we offset
                       by this remainder portion we should be 128 bit aligned.
                       Also allow a 15 sample over run during the execution.  */
                    temp = (int) ceil((float) ((dev_width + 15.0) * spp_out + 15.0)/16.0);
                    penum->line_size = temp * 16;  /* The stride */
                    penum->line = gs_alloc_bytes(penum->memory, penum->line_size,
                                                 "gs_image_class_3_mono");
                    penum->thresh_buffer =
                                gs_alloc_bytes(penum->memory,
                                               penum->line_size * max_height,
                                               "gs_image_class_3_mono");
                    if (penum->line == NULL || penum->thresh_buffer == NULL
                                || penum->ht_buffer == NULL)
                        code = -1;
                }
                /* Precompute values needed for rasterizing. */
                penum->dxx =
                    float2fixed(penum->matrix.xx + fixed2float(fixed_epsilon) / 2);
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

static void
fill_threshhold_buffer(byte *dest_strip, byte *src_strip, int src_width,
                       int left_offset, int left_width, int num_tiles,
                       int right_width)
{
    byte *ptr_out_temp = dest_strip;
    int ii;

    /* Left part */
    memcpy(dest_strip, src_strip + left_offset, left_width);
    ptr_out_temp += left_width;
    /* Now the full parts */
    for (ii = 0; ii < num_tiles; ii++){
        memcpy(ptr_out_temp, src_strip, src_width);
        ptr_out_temp += src_width;
    }
    /* Now the remainder */
    memcpy(ptr_out_temp, src_strip, right_width);
}

#if RAW_HT_DUMP
/* This is slow thresholding, byte output for debug only */
static void
threshold_row_byte(byte *contone, byte *threshold_strip, int contone_stride,
                              byte *halftone, int dithered_stride, int width,
                              int num_rows)
{
    int k, j;
    byte *contone_ptr;
    byte *thresh_ptr;
    byte *halftone_ptr;

    /* For the moment just do a very slow compare until we get
       get this working */
    for (j = 0; j < num_rows; j++) {
        contone_ptr = contone;
        thresh_ptr = threshold_strip + contone_stride * j;
        halftone_ptr = halftone + dithered_stride * j;
        for (k = 0; k < width; k++) {
            if (contone_ptr[k] < thresh_ptr[k]) {
                halftone_ptr[k] = 0;
            } else {
                halftone_ptr[k] = 255;
            }
        }
    }
}
#endif

#ifndef HAVE_SSE2
/* This is slow thresholding bit output */
static void
threshold_row_bit(byte *contone,  byte *threshold_strip,  int contone_stride,
                  byte *halftone, int dithered_stride, int width,
                  int num_rows, int offset_bits)
{
    int k, j;
    byte *contone_ptr;
    byte *thresh_ptr;
    byte *halftone_ptr;
    byte bit_init;
    int ht_index;

    /* For the moment just do a very slow compare until we get
       get this working.  This could use some serious optimization */
    for (j = 0; j < num_rows; j++) {
        contone_ptr = contone;
        thresh_ptr = threshold_strip + contone_stride * j;
        halftone_ptr = halftone + dithered_stride * j;
        /* First get the left remainder portion.  Put into MSBs of first byte */
        bit_init = 0x80;
        ht_index = -1;
        for (k = 0; k < offset_bits; k++) {
            if ( (k % 8) == 0) {
                ht_index++;
            }
            if (contone_ptr[k] < thresh_ptr[k]) {
                halftone_ptr[ht_index] |=  bit_init;
            } else {
                halftone_ptr[ht_index] &=  ~bit_init;
            }
            if (bit_init == 1) {
                bit_init = 0x80;
            } else {
                bit_init >>= 1;
            }
        }
        bit_init = 0x80;
        ht_index = -1;
        if (offset_bits > 0) {
            halftone_ptr += 2; /* Point to the next 16 bits of data */
        }
        /* Now get the rest, which will be 16 bit aligned. */
        for (k = offset_bits; k < width; k++) {
            if (((k - offset_bits) % 8) == 0) {
                ht_index++;
            }
            if (contone_ptr[k] < thresh_ptr[k]) {
                halftone_ptr[ht_index] |=  bit_init;
            } else {
                halftone_ptr[ht_index] &=  ~bit_init;
            }
            if (bit_init == 1) {
                bit_init = 0x80;
            } else {
                bit_init >>= 1;
            }
        }
    }
}

/* A simple case for use in the landscape mode. Could probably be coded up
   faster */
static void
threshold_16_bit(byte *contone_ptr_in, byte *thresh_ptr_in, byte *ht_data)
{
    int k, j;
    byte *contone_ptr = contone_ptr_in;
    byte *thresh_ptr = thresh_ptr_in;
    byte bit_init;

    for (j = 0; j < 2; j++) {
        bit_init = 0x80;
        for (k = 0; k < 8; k++) {
            if (contone_ptr[k] < thresh_ptr[k]) {
                ht_data[j] |=  bit_init;
            } else {
                ht_data[j] &=  ~bit_init;
            }
            bit_init >>= 1;
        }
        contone_ptr += 8;
        thresh_ptr += 8;
    }
}
#else
/* Note this function has strict data alignment needs */
static void
threshold_16_SSE(byte *contone_ptr, byte *thresh_ptr, byte *ht_data)
{
    __m128i input1;
    __m128i input2;
    register int result_int;
    const unsigned int mask1 = 0x80808080;
    __m128i sign_fix = _mm_set_epi32(mask1, mask1, mask1, mask1);

    /* Load */
    input1 = _mm_load_si128((const __m128i *)contone_ptr);
    input2 = _mm_load_si128((const __m128i *) thresh_ptr);
    /* Unsigned subtraction does Unsigned saturation so we
       have to use the signed operation */
    input1 = _mm_xor_si128(input1, sign_fix);
    input2 = _mm_xor_si128(input2, sign_fix);
    /* Subtract the two */
    input2 = _mm_subs_epi8(input1, input2);
    /* Grab the sign mask */
    result_int = _mm_movemask_epi8(input2);
    /* bit wise reversal on 16 bit word */
    ht_data[0] = bitreverse[(result_int & 0xff)];
    ht_data[1] = bitreverse[((result_int >> 8) & 0xff)];
}

/* Not so fussy on its alignment */
static void
threshold_16_SSE_unaligned(byte *contone_ptr, byte *thresh_ptr, byte *ht_data)
{
    __m128i input1;
    __m128i input2;
    int result_int;
    byte *sse_data;
    const unsigned int mask1 = 0x80808080;
    __m128i sign_fix = _mm_set_epi32(mask1, mask1, mask1, mask1);

    sse_data = (byte*) &(result_int);
    /* Load */
    input1 = _mm_loadu_si128((const __m128i *)contone_ptr);
    input2 = _mm_loadu_si128((const __m128i *) thresh_ptr);
    /* Unsigned subtraction does Unsigned saturation so we
       have to use the signed operation */
    input1 = _mm_xor_si128(input1, sign_fix);
    input2 = _mm_xor_si128(input2, sign_fix);
    /* Subtract the two */
    input2 = _mm_subs_epi8(input1, input2);
    /* Grab the sign mask */
    result_int = _mm_movemask_epi8(input2);
    /* bit wise reversal on 16 bit word */
    ht_data[0] = bitreverse[sse_data[0]];
    ht_data[1] = bitreverse[sse_data[1]];
}

/* This uses SSE2 simd operations to perform the thresholding operation.
   Intrinsics are used since in-line assm is not supported in Visual
   Studio on 64 bit machines, plus instrinsics are easily ported between
   Visual Studio and gcc. requires <emmintrin.h> */
static void
threshold_row_SSE(byte *contone,  byte *threshold_strip,  int contone_stride,
                  byte *halftone, int dithered_stride, int width,
                  int num_rows, int offset_bits)
{
    byte *contone_ptr;
    byte *thresh_ptr;
    byte *halftone_ptr;
    int num_tiles = (int) ceil((float) (width - offset_bits)/16.0);
    int k, j;

    for (j = 0; j < num_rows; j++) {
        /* contone and thresh_ptr are 128 bit aligned.  We do need to do this in
           two steps to ensure that we pack the bits in an aligned fashion
           into halftone_ptr.  */
        contone_ptr = contone;
        thresh_ptr = threshold_strip + contone_stride * j;
        halftone_ptr = halftone + dithered_stride * j;
        if (offset_bits > 0) {
            /* Since we allowed for 16 bits in our left remainder
               we can go directly in to the destination.  threshold_16_SSE
               requires 128 bit alignment.  contone_ptr and thresh_ptr
               are set up so that after we move in by offset_bits elements
               then we are 128 bit aligned.  */
            threshold_16_SSE_unaligned(contone_ptr, thresh_ptr, halftone_ptr);
            halftone_ptr += 2;
            thresh_ptr += offset_bits;
            contone_ptr += offset_bits;
        }
        /* Now we should have 128 bit aligned with our input data. Iterate
           over sets of 16 going directly into our HT buffer.  Sources and
           halftone_ptr buffers should be padded to allow 15 bit overrun */
        for (k = 0; k < num_tiles; k++) {
            threshold_16_SSE(contone_ptr, thresh_ptr, halftone_ptr);
            thresh_ptr += 16;
            contone_ptr += 16;
            halftone_ptr += 2;
        }
    }
}
#endif

/* This thresholds a buffer that is 16 wide by data_length tall */
static void
threshold_landscape(byte *contone_align, byte *thresh_align,
                    ht_landscape_info_t ht_landscape, byte *halftone,
                    int data_length)
{
    __align16 byte contone[16];
    int position_start, position, curr_position;
    int *widths = &(ht_landscape.widths[0]);
    int local_widths[16];
    int num_contone = ht_landscape.num_contones;
    int k, j, w, contone_out_posit;
    byte *contone_ptr, *thresh_ptr, *halftone_ptr;

    /* Work through chunks of 16.  */
    /* Data may have come in left to right or right to left. */
    if (ht_landscape.index > 0) {
        position = position_start = 0;
    } else {
        position = position_start = ht_landscape.curr_pos + 1;
    }
    thresh_ptr = thresh_align;
    halftone_ptr = halftone;

    /* Copy the widths to a local array, and truncate the last one (which may
     * be the first one!) if required. */
    k = 0;
    for (j = 0; j < num_contone; j++)
        k += (local_widths[j] = widths[position_start+j]);
    if (k > 16) {
        if (ht_landscape.index > 0) {
            local_widths[num_contone-1] -= k-16;
        } else {
            local_widths[0] -= k-16;
        }
    }

    for (k = data_length; k > 0; k--) { /* Loop on rows */

        contone_ptr = &(contone_align[position]); /* Point us to our row start */
        curr_position = 0; /* We use this in keeping track of widths */
        contone_out_posit = 0; /* Our index out */

        for (j = num_contone; j > 0; j--) {
            byte c = *contone_ptr;
            for (w = local_widths[curr_position]; w > 0; w--) {
                contone[contone_out_posit] = c;
                contone_out_posit++;
            }

            curr_position++; /* Move us to the next position in our width array */
            contone_ptr++;   /* Move us to a new location in our contone buffer */
        }
        /* Now we have our left justified and expanded contone data for a single
           set of 16.  Go ahead and threshold these */
#ifdef HAVE_SSE2
        threshold_16_SSE(&(contone[0]), thresh_ptr, halftone_ptr);
#else
        threshold_16_bit(&(contone[0]), thresh_ptr, halftone_ptr);
#endif
        thresh_ptr += 16;
        position += 16;
        halftone_ptr += 2;
    }
}

/* If we are in here, we had data left over.  Move it to the proper position
   and get ht_landscape_info_t set properly */
static void
reset_landscape_buffer(ht_landscape_info_t *ht_landscape, byte *contone_align,
                       int data_length, int num_used)
{
    int k;
    int position_curr, position_new, delta;
    int curr_x_pos = ht_landscape->xstart;

    if (ht_landscape->index < 0) {
        /* Moving right to left, move column to far right */
        position_curr = ht_landscape->curr_pos + 1;
        position_new = 15;
        delta = ht_landscape->count - num_used;
        memset(&(ht_landscape->widths[0]), 0, sizeof(int)*16);
        ht_landscape->widths[15] = delta;
        ht_landscape->curr_pos = 14;
        ht_landscape->xstart = curr_x_pos - num_used;
    } else {
        /* Moving left to right, move column to far left */
        position_curr = ht_landscape->curr_pos - 1;
        position_new = 0;
        delta = ht_landscape->count - num_used;
        memset(&(ht_landscape->widths[0]), 0, sizeof(int)*16);
        ht_landscape->widths[0] = delta;
        ht_landscape->curr_pos = 1;
        ht_landscape->xstart = curr_x_pos + num_used;
    }
    ht_landscape->count = delta;
    ht_landscape->num_contones = 1;
    for (k = 0; k < data_length; k++) {
            contone_align[position_new] = contone_align[position_curr];
            position_curr += 16;
            position_new += 16;
    }
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
    gx_dda_fixed_point pnext;
    image_posture posture = penum->posture;
    int vdi;  /* amounts to replicate */
    fixed xrun;
    byte *contone_align, *thresh_align, *halftone;
    int spp_out = penum->dev->color_info.num_components;
    byte *devc_contone;
    const byte *psrc = buffer + data_x;
    int dest_width, dest_height, data_length;
    byte *dev_value, *color_cache;
    gx_ht_order *d_order = &(penum->pis->dev_ht->components[0].corder);
    byte *threshold = d_order->threshold;
    byte *thresh_tile;
    int thresh_width, thresh_height;
    int dx, dy;
    int left_rem_end, left_width, right_tile_width;
    int num_full_tiles, i;
    int dithered_stride;
    int position, k;
    int offset_bits = penum->ht_offset_bits;
    int contone_stride;
    const int y_pos = penum->yci;
    fixed scale_factor, offset;
    int in_row_offset, jj, ii;
    byte *row_ptr, *ptr_out_temp, *ptr_out;
    int init_tile, num_tiles, tile_remainder;
    bool replicate_tile;
    int width;
    bool flush_buff = false;
    fixed scale_step;
    byte *psrc_temp;
    int offset_contone;    /* to ensure 128 bit boundary */
    int offset_threshold;  /* to ensure 128 bit boundary */

#if RAW_HT_DUMP
    FILE *fid;
    char file_name[50];
#endif

    if (h == 0) {
        if (penum->ht_landscape.count == 0 || posture == image_portrait) {
            return 0;
        } else {
            /* Need to flush the buffer */
            offset_bits = penum->ht_landscape.count;
            penum->ht_landscape.offset_set = true;
            flush_buff = true;
        }
    }
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
            pnext = penum->dda.pixel0;
            xrun = dda_current(pnext.x);
            xrun = xrun - penum->adjust + (fixed_half - fixed_epsilon);
            dest_width = fixed2int_var_rounded(any_abs(penum->x_extent.x));
            if (penum->x_extent.x < 0)
                xrun += penum->x_extent.x;
            vdi = penum->hci;
            data_length = dest_width;
            dest_height = fixed2int_var_rounded(any_abs(penum->y_extent.y));
            contone_stride = penum->line_size;
            scale_factor =
              float2fixed_rounded((float) penum->Width / fixed2float(any_abs(penum->x_extent.x)));
#if RAW_HT_DUMP
            dithered_stride = data_length * spp_out;
            offset_bits = 0;
            thresh_align = gs_alloc_bytes(penum->memory, contone_stride * vdi,
                                             "image_render_mono_ht");
            halftone = gs_alloc_bytes(penum->memory, dithered_stride * vdi,
                                      "image_render_mono_ht");
            contone_align = gs_alloc_bytes(penum->memory, contone_stride * spp_out,
                                     "image_render_mono_ht");
            if (contone_align == NULL || thresh_align == NULL || halftone == NULL)
                return gs_rethrow(gs_error_VMerror, "Memory allocation failure");
#else
            /* Get the pointers to our buffers */
            dithered_stride = penum->ht_stride;
            halftone = penum->ht_buffer;
            contone_align = penum->line + offset_contone;
            thresh_align = penum->thresh_buffer + offset_threshold;
#endif
#ifdef DEBUG
            /* Help in spotting problems */
            memset(halftone,0x00, dithered_stride * vdi);
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
            scale_factor = float2fixed_rounded((float) (penum->rect.w - 1.0) / (float) dest_height);
            data_length = dest_height;
            /* In the landscaped case, we want to accumulate multiple columns
               of data before sending to the device.  We want to have a full
               byte of HT data in one write.  This may not be possible at the
               left or right and for those and for those we have so send partial
               chunks */
#if RAW_HT_DUMP
            dithered_stride = data_length * spp_out;
            offset_bits = 0;
            thresh_align = gs_alloc_bytes(penum->memory, contone_stride * vdi,
                                             "image_render_mono_ht");
            halftone = gs_alloc_bytes(penum->memory, dithered_stride * vdi,
                                      "image_render_mono_ht");
            contone_align = gs_alloc_bytes(penum->memory, contone_stride * spp_out,
                                     "image_render_mono_ht");
            if (contone_align == NULL || thresh_align == NULL || halftone == NULL)
                return gs_rethrow(gs_error_VMerror, "Memory allocation failure");
#else
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
            /* Get the pointers to our buffers */
            dithered_stride = penum->ht_stride;
            halftone = penum->ht_buffer;
            contone_align = penum->line + offset_contone;
            thresh_align = penum->thresh_buffer + offset_threshold;
#endif
            break;
    }
    if (flush_buff) goto flush;  /* All done */
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
                        scale_step = 0;
                        for (k = 0; k < data_length; k++, devc_contone++,
                                                    scale_step += scale_factor) {
                            offset = fixed2int_rounded(scale_step);
                            *devc_contone = psrc[offset];
                        }
                    }
                } else {
                    devc_contone += (data_length - 1);
                    scale_step = 0;
                    for (k = 0; k < data_length; k++, devc_contone--,
                                                scale_step += scale_factor) {
                        offset = fixed2int_rounded(scale_step);
                        *devc_contone = psrc[offset];
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
                            offset = fixed2int_rounded(scale_factor * k);
                            devc_contone[position] = psrc[offset];
                            position -= 16;
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
                        for (k = 0; k < data_length; k++) {
                                offset = fixed2int_rounded(scale_factor * k);
                                devc_contone[position] = psrc[offset];
                                position += 16;
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
                        offset = fixed2int_rounded(scale_factor * k);
                        dev_value = color_cache + psrc[offset] * spp_out;
                        memcpy(devc_contone, dev_value, spp_out);
                        devc_contone += spp_out;
                    }
                } else {
                    for (k = 0; k < data_length; k++) {
                        offset = fixed2int_rounded(scale_factor * k);
                        dev_value = color_cache + psrc[offset] * spp_out;
                        /* This could probably be done a bit better but
                           reversed indexed images probably don't occur
                           that often */
                        memcpy(&(devc_contone[data_length - k]),
                               dev_value, spp_out);
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
                            offset = fixed2int_rounded(scale_factor * k);
                            dev_value = color_cache + psrc[offset] * spp_out;
                            devc_contone[position] = dev_value[0];  /* Only works for monochrome device now */
                            position -= 16;
                    }
                } else {
                    position = penum->ht_landscape.curr_pos;
                    for (k = 0; k < data_length; k++) {
                            offset = fixed2int_rounded(scale_factor * k);
                            dev_value = color_cache + psrc[offset] * spp_out;
                            devc_contone[position] = dev_value[0];  /* Only works for monochrome device now */
                            position += 16;
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
    /* Go ahead and fill the threshold line buffer with tiled threshold values.
       First just grab the row or column that we are going to tile with and
       then do memcpy into the buffer */
flush:
    thresh_width = d_order->width;
    thresh_height = d_order->height;
    /* Figure out the tile steps.  Left offset, Number of tiles, Right offset. */
    switch (posture) {
        case image_portrait:
            /* Compute the tiling positions with dest_width */
            dx = fixed2int_var(xrun) % thresh_width;
            /* Left remainder part */
            left_rem_end = min(dx + dest_width, thresh_width);
            left_width = left_rem_end - dx;  /* The left width of our tile part */
            /* Now the middle part */
            num_full_tiles =
                (int)fastfloor((dest_width - left_width)/ (float) thresh_width);
            /* Now the right part */
            right_tile_width = dest_width -  num_full_tiles * thresh_width -
                               left_width;
            /* Those dimensions stay the same across the set of lines that
               we fill in our buffer.  Iterate over the vdi and fill up our
               threshold buffer */
            for (k = 0; k < vdi; k++) {
                /* Get a pointer to our tile row */
                dy = (penum->yci + k + penum->dev->band_offset_y) % thresh_height;
                thresh_tile = threshold + d_order->width * dy;
                /* Fill the buffer, can be multiple rows.  Make sure
                   to update with stride */
                position = contone_stride * k;
                /* Tile into the 128 bit aligned threshold strip */
                fill_threshhold_buffer(&(thresh_align[position]),
                                       thresh_tile, thresh_width, dx, left_width,
                                       num_full_tiles, right_tile_width);
            }
            /* Apply the threshold operation */
#if RAW_HT_DUMP
            threshold_row_byte(contone_align, thresh_align, contone_stride,
                              halftone, dithered_stride, dest_width, vdi);
            sprintf(file_name,"HT_Portrait_%d_%dx%dx%d.raw", penum->id, dest_width,
                    dest_height, spp_out);
            fid = fopen(file_name,"a+b");
            fwrite(halftone,1,dest_width * vdi,fid);
            fclose(fid);
#else
#ifdef HAVE_SSE2
            threshold_row_SSE(contone_align, thresh_align, contone_stride,
                              halftone, dithered_stride, dest_width, vdi,
                              offset_bits);
#else
            threshold_row_bit(contone_align, thresh_align, contone_stride,
                              halftone, dithered_stride, dest_width, vdi,
                              offset_bits);
#endif
            /* Now do the copy mono operation */
            /* First the left remainder bits */
            if (offset_bits > 0) {
                int x_pos = fixed2int_var(xrun);
                (*dev_proc(dev, copy_mono)) (dev, halftone, 0, dithered_stride,
                                             gx_no_bitmap_id, x_pos, y_pos,
                                             offset_bits, vdi,
                                             (gx_color_index) 0,
                                             (gx_color_index) 1);
            }
            if ((dest_width - offset_bits) > 0 ) {
                /* Now the primary aligned bytes */
                byte *curr_ptr = halftone;
                int curr_width = dest_width - offset_bits;
                int x_pos = fixed2int_var(xrun) + offset_bits;
                if (offset_bits > 0) {
                    curr_ptr += 2; /* If the first 2 bytes had the left part then increment */
                }
                (*dev_proc(dev, copy_mono)) (dev, curr_ptr, 0, dithered_stride,
                                             gx_no_bitmap_id, x_pos, y_pos,
                                             curr_width, vdi,
                                             (gx_color_index) 0, (gx_color_index) 1);
            }
#endif
            break;
        case image_landscape:
            /* Go ahead and paint the chunk if we have 16 values or a partial
               to get us in sync with the 1 bit devices 16 bit positions */
            while (penum->ht_landscape.count > 15 ||
                   ((penum->ht_landscape.count >= offset_bits) &&
                    penum->ht_landscape.offset_set)) {
                /* Go ahead and 2D tile in the threshold buffer at this time */
                /* Always work the tiling from the upper left corner of our
                   16 columns */
                if (penum->ht_landscape.offset_set) {
                    width = offset_bits;
                } else {
                    width = 16;
                }
                if (penum->y_extent.x < 0) {
                    dx = (penum->ht_landscape.xstart - width + 1) % thresh_width;
                } else {
                    dx = penum->ht_landscape.xstart % thresh_width;
                }
                dy = (penum->dev->band_offset_y + penum->ht_landscape.y_pos) % thresh_height;
                /* Left remainder part */
                left_rem_end = min(dx + 16, thresh_width);
                left_width = left_rem_end - dx;
                /* Now the middle part */
                num_full_tiles =
                    (int)fastfloor((float) (16 - left_width)/ (float) thresh_width);
                /* Now the right part */
                right_tile_width =
                    16 - num_full_tiles * thresh_width - left_width;
                /* Now loop over the y stuff */
                ptr_out = thresh_align;
                /* Do this in three parts.  We do a top part, followed by
                   larger mem copies followed by a bottom partial. After
                   a slower initial fill we are able to do larger faster
                   expansions */
                if (dest_height <= 2 * thresh_height) {
                    init_tile = dest_height;
                    replicate_tile = false;
                } else {
                    init_tile = thresh_height;
                    replicate_tile = true;
                }
                for (jj = 0; jj < init_tile; jj++) {
                    in_row_offset = (jj + dy) % thresh_height;
                    row_ptr = threshold + in_row_offset * thresh_width;
                    ptr_out_temp = ptr_out;
                    /* Left part */
                    memcpy(ptr_out_temp, row_ptr + dx, left_width);
                    ptr_out_temp += left_width;
                    /* Now the full tiles */
                    for (ii = 0; ii < num_full_tiles; ii++) {
                        memcpy(ptr_out_temp, row_ptr, thresh_width);
                        ptr_out_temp += thresh_width;
                    }
                    /* Now the remainder */
                    memcpy(ptr_out_temp, row_ptr, right_tile_width);
                    ptr_out += 16;
                }
                if (replicate_tile) {
                    /* Find out how many we need to copy */
                    num_tiles =
                        (int)fastfloor((float) (dest_height - thresh_height)/ (float) thresh_height);
                    tile_remainder = dest_height - (num_tiles + 1) * thresh_height;
                    for (jj = 0; jj < num_tiles; jj ++) {
                        memcpy(ptr_out, thresh_align, 16 * thresh_height);
                        ptr_out += 16 * thresh_height;
                    }
                    /* Now fill in the remainder */
                    memcpy(ptr_out, thresh_align, 16 * tile_remainder);
                }
                /* Apply the threshold operation */
                threshold_landscape(contone_align, thresh_align,
                                    penum->ht_landscape, halftone, data_length);
                /* Perform the copy mono */
                penum->ht_landscape.offset_set = false;
                if (penum->ht_landscape.index < 0) {
                    (*dev_proc(dev, copy_mono)) (dev, halftone, 0, 2,
                                                 gx_no_bitmap_id,
                                                 penum->ht_landscape.xstart - width + 1,
                                                 penum->ht_landscape.y_pos,
                                                 width, data_length,
                                                 (gx_color_index) 0,
                                                 (gx_color_index) 1);
                } else {
                    (*dev_proc(dev, copy_mono)) (dev, halftone, 0, 2,
                                                 gx_no_bitmap_id,
                                                 penum->ht_landscape.xstart,
                                                 penum->ht_landscape.y_pos,
                                                 width, data_length,
                                                 (gx_color_index) 0,
                                                 (gx_color_index) 1);
                }
                /* Clean up and reset our buffer.  We may have a line left
                   over that has to be maintained due to line replication in the
                   resolution conversion */
                if (width != penum->ht_landscape.count) {
                    reset_landscape_buffer(&(penum->ht_landscape), contone_align,
                                           data_length, width);
                } else {
                    /* Reset the whole buffer */
                    penum->ht_landscape.count = 0;
                    if (penum->ht_landscape.index < 0) {
                        /* Going right to left */
                        penum->ht_landscape.curr_pos = 15;
                    } else {
                        /* Going left to right */
                        penum->ht_landscape.curr_pos = 0;
                    }
                    penum->ht_landscape.num_contones = 0;
                    memset(&(penum->ht_landscape.widths[0]), 0, sizeof(int)*16);
                }
            }
            break;
        default:
            return gs_rethrow(-1, "Invalid orientation for thresholding");
    }

    /* Clean up.  Only for debug case */
#if RAW_HT_DUMP
    gs_free_object(penum->memory, contone_align, "image_render_mono_ht");
    gs_free_object(penum->memory, thresh_align, "image_render_mono_ht");
    gs_free_object(penum->memory, halftone, "image_render_mono_ht");
#endif
    return 0;
}


