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


/* 12-bit image procedures */
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
#include "gxdevice.h"
#include "gxcmap.h"
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
#include "gxdevsop.h"

/* ---------------- Unpacking procedures ---------------- */

const byte *
sample_unpack_12(byte * bptr, int *pdata_x, const byte * data,
                 int data_x, uint dsize, const sample_map *ignore_smap, int spread,
                 int ignore_num_components_per_plane)
{
    /* Assuming an identity map for all components. */
    register frac *bufp = (frac *) bptr;
    uint dskip = (data_x >> 1) * 3;
    const byte *psrc = data + dskip;
#define inc_bufp(bp, n) bp = (frac *)((byte *)(bp) + (n))
    uint sample;
    int left = dsize - dskip;

    if ((data_x & 1) && left > 0)
        switch (left) {
            default:
                sample = ((uint) (psrc[1] & 0xf) << 8) + psrc[2];
                *bufp = bits2frac(sample, 12);
                inc_bufp(bufp, spread);
                psrc += 3;
                left -= 3;
                break;
            case 2:		/* xxxxxxxx xxxxdddd */
                *bufp = (psrc[1] & 0xf) * (frac_1 / 15);
            case 1:		/* xxxxxxxx */
                left = 0;
        }
    while (left >= 3) {
        sample = ((uint) * psrc << 4) + (psrc[1] >> 4);
        *bufp = bits2frac(sample, 12);
        inc_bufp(bufp, spread);
        sample = ((uint) (psrc[1] & 0xf) << 8) + psrc[2];
        *bufp = bits2frac(sample, 12);
        inc_bufp(bufp, spread);
        psrc += 3;
        left -= 3;
    }
    /* Handle trailing bytes. */
    switch (left) {
        case 2:		/* dddddddd ddddxxxx */
            sample = ((uint) * psrc << 4) + (psrc[1] >> 4);
            *bufp = bits2frac(sample, 12);
            inc_bufp(bufp, spread);
            *bufp = (psrc[1] & 0xf) * (frac_1 / 15);
            break;
        case 1:		/* dddddddd */
            sample = (uint) * psrc << 4;
            *bufp = bits2frac(sample, 12);
            break;
        case 0:		/* Nothing more to do. */
            ;
    }
    *pdata_x = 0;
    return bptr;
}

/* ------ Strategy procedure ------ */

/* Check the prototype. */
iclass_proc(gs_image_class_2_fracs);

/* Use special (slow) logic for 12-bit source values. */
static irender_proc(image_render_frac);
static irender_proc(image_render_icc16); /* icc 16bit case */

int
gs_image_class_2_fracs(gx_image_enum * penum, irender_proc_t *render_fn)
{
    bool std_cmap_procs;

    if (penum->bps > 8) {
        if (penum->use_mask_color) {
            /* Convert color mask values to fracs. */
            int i;

            for (i = 0; i < penum->spp * 2; ++i)
                penum->mask_color.values[i] =
                    bits2frac(penum->mask_color.values[i], 12);
        }
        /* If the device has some unique color mapping procs due to its color space,
           then we will need to use those and go through pixel by pixel instead
           of blasting through buffers.  This is true for example with many of
           the color spaces for CUPs */
        std_cmap_procs = gx_device_uses_std_cmap_procs(penum->dev, penum->pgs);
        if ( (gs_color_space_get_index(penum->pcs) == gs_color_space_index_DeviceN &&
            penum->pcs->cmm_icc_profile_data == NULL) || penum->use_mask_color ||
            penum->bps != 16 || !std_cmap_procs ||
            gs_color_space_get_index(penum->pcs) == gs_color_space_index_DevicePixel ||
            gs_color_space_get_index(penum->pcs) == gs_color_space_index_Indexed) {
            /* DevicePixel color space used in mask from 3x type.  Basically
               a simple color space that just is scaled to the device bit
               depth when remapped. No CM needed */
            if_debug0m('b', penum->memory, "[b]render=frac\n");
            *render_fn =  &image_render_frac;
            return 0;
        } else {
            /* Set up the link now */
            const gs_color_space *pcs;
            gsicc_rendering_param_t rendering_params;
            int k;
            int src_num_comp = cs_num_components(penum->pcs);
            int num_des_comps;
            int code;
            cmm_dev_profile_t *dev_profile;

            code = dev_proc(penum->dev, get_profile)(penum->dev, &dev_profile);
            if (code < 0)
                return 0;
            num_des_comps = gsicc_get_device_profile_comps(dev_profile);
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
            penum->icc_setup.has_transfer =
                gx_has_transfer(penum->pgs, num_des_comps);
            if (penum->icc_setup.is_lab) penum->icc_setup.need_decode = false;
            if (penum->icc_link == NULL) {
                penum->icc_link = gsicc_get_link(penum->pgs, penum->dev, pcs, NULL,
                    &rendering_params, penum->memory);
            }
            /* Use the direct unpacking proc */
            penum->unpack = sample_unpackicc_16;
            if_debug0m('b', penum->memory, "[b]render=icc16\n");
            *render_fn = &image_render_icc16;
            return 0;
        }
    }
    return 0;
}

/* ---------------- Rendering procedures ---------------- */

/* ------ Rendering for 12-bit samples ------ */

#define FRACS_PER_LONG (ARCH_SIZEOF_LONG / arch_sizeof_frac)
typedef union {
    frac v[GS_IMAGE_MAX_COLOR_COMPONENTS];
#define LONGS_PER_COLOR_FRACS\
  ((GS_IMAGE_MAX_COLOR_COMPONENTS + FRACS_PER_LONG - 1) / FRACS_PER_LONG)
    long all[LONGS_PER_COLOR_FRACS];	/* for fast comparison */
} color_fracs;

#define LONGS_PER_4_FRACS ((arch_sizeof_frac * 4 + ARCH_SIZEOF_LONG - 1) / ARCH_SIZEOF_LONG)
#if LONGS_PER_4_FRACS == 1
#  define COLOR_FRACS_4_EQ(f1, f2)\
     ((f1).all[0] == (f2).all[0])
#else
#if LONGS_PER_4_FRACS == 2
#  define COLOR_FRACS_4_EQ(f1, f2)\
     ((f1).all[0] == (f2).all[0] && (f1).all[1] == (f2).all[1])
#endif
#endif

/* Test whether a color is transparent. */
static bool
mask_color12_matches(const frac *v, const gx_image_enum *penum,
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

/* Render an image with more than 8 bits per sample. */
/* The samples have been expanded into fracs. */
static int
image_render_frac(gx_image_enum * penum, const byte * buffer, int data_x,
                  uint w, int h, gx_device * dev)
{
    const gs_gstate *pgs = penum->pgs;
    gs_logical_operation_t lop = penum->log_op;
    gx_dda_fixed_point pnext;
    image_posture posture = penum->posture;
    fixed xl, ytf;
    fixed pdyx, pdyy;		/* edge of parallelogram */
    int yt = penum->yci, iht = penum->hci;
    const gs_color_space *pcs = penum->pcs;
    cs_proc_remap_color((*remap_color)) = pcs->type->remap_color;
    gs_client_color cc;
    bool device_color = penum->device_color;
    const gx_color_map_procs *cmap_procs = gx_get_cmap_procs(pgs, dev);
    cmap_proc_rgb((*map_rgb)) = cmap_procs->map_rgb;
    cmap_proc_cmyk((*map_cmyk)) = cmap_procs->map_cmyk;
    bool use_mask_color = penum->use_mask_color;
    gx_device_color devc1, devc2;
    gx_device_color *pdevc = &devc1;
    gx_device_color *pdevc_next = &devc2;
    int spp = penum->spp;
    const frac *psrc_initial = (const frac *)buffer + data_x * spp;
    const frac *psrc = psrc_initial;
    const frac *rsrc = psrc + spp; /* psrc + spp at start of run */
    fixed xrun;			/* x at start of run */
    int irun;			/* int xrun */
    fixed yrun;			/* y ditto */
    color_fracs run;		/* run value */
    color_fracs next;		/* next sample value */
    const frac *bufend = psrc + w;
    int code = 0, mcode = 0;
    int i;
    bool is_devn = false;
    bool is_sep = (gs_color_space_get_index(penum->pcs) ==
                    gs_color_space_index_Separation);

    if (h == 0)
        return 0;
    pnext = penum->dda.pixel0;
    xrun = xl = dda_current(pnext.x);
    irun = fixed2int_var_rounded(xrun);
    yrun = ytf = dda_current(pnext.y);
    pdyx = dda_current(penum->dda.row.x) - penum->cur.x;
    pdyy = dda_current(penum->dda.row.y) - penum->cur.y;
    if_debug5m('b', penum->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               penum->y, data_x, w, fixed2float(xl), fixed2float(ytf));
    memset(&run, 0, sizeof(run));
    memset(&next, 0, sizeof(next));
    /* Ensure that we don't get any false dev_color_eq hits. */
    set_nonclient_dev_color(&devc1, gx_no_color_index);
    set_nonclient_dev_color(&devc2, gx_no_color_index);
    cs_full_init_color(&cc, pcs);
    run.v[0] = ~psrc[0];	/* force remap */

    if (dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < GS_CLIENT_COLOR_MAX_COMPONENTS; i++) {
            pdevc->colors.devn.values[i] = 0;
            pdevc_next->colors.devn.values[i] = 0;
        }
        is_devn = true;
    }
    while (psrc < bufend) {
        next.v[0] = psrc[0];
        switch (spp) {
            case 4:		/* may be CMYK */
                next.v[1] = psrc[1];
                next.v[2] = psrc[2];
                next.v[3] = psrc[3];
                psrc += 4;
                if (COLOR_FRACS_4_EQ(next, run))
                    goto inc;
                if (use_mask_color && mask_color12_matches(next.v, penum, 4)) {
                    color_set_null(pdevc_next);
                    goto f;
                }
                if (device_color) {
                    (*map_cmyk) (next.v[0], next.v[1],
                                 next.v[2], next.v[3],
                                 pdevc_next, pgs, dev,
                                 gs_color_select_source, NULL);
                    goto f;
                }
                decode_frac(next.v[0], cc, 0);
                decode_frac(next.v[1], cc, 1);
                decode_frac(next.v[2], cc, 2);
                decode_frac(next.v[3], cc, 3);
                if_debug4m('B', penum->memory, "[B]cc[0..3]=%g,%g,%g,%g\n",
                           cc.paint.values[0], cc.paint.values[1],
                           cc.paint.values[2], cc.paint.values[3]);
                break;
            case 3:		/* may be RGB */
                next.v[1] = psrc[1];
                next.v[2] = psrc[2];
                psrc += 3;
                if (COLOR_FRACS_4_EQ(next, run))
                    goto inc;
                if (use_mask_color && mask_color12_matches(next.v, penum, 3)) {
                    color_set_null(pdevc_next);
                    goto f;
                }
                if (device_color) {
                    (*map_rgb) (next.v[0], next.v[1],
                                next.v[2], pdevc_next, pgs, dev,
                                gs_color_select_source);
                    goto f;
                }
                decode_frac(next.v[0], cc, 0);
                decode_frac(next.v[1], cc, 1);
                decode_frac(next.v[2], cc, 2);
                if_debug3m('B', penum->memory, "[B]cc[0..2]=%g,%g,%g\n",
                           cc.paint.values[0], cc.paint.values[1],
                           cc.paint.values[2]);
                break;
            case 1:		/* may be Gray, but could be a separation */
                if (is_devn && is_sep) {
                    psrc++;
                    if (next.v[0] == run.v[0])
                        goto inc;
                    if (use_mask_color && mask_color12_matches(next.v, penum, 1)) {
                        color_set_null(pdevc_next);
                        goto f;
                    }
                    decode_frac(next.v[0], cc, 0);
                    if_debug1m('B', penum->memory, "[B]cc[0]=%g\n",
                               cc.paint.values[0]);
                    break;
                } else {
                    psrc++;
                    if (next.v[0] == run.v[0])
                        goto inc;
                    if (use_mask_color && mask_color12_matches(next.v, penum, 1)) {
                        color_set_null(pdevc_next);
                        goto f;
                    }
                    if (device_color) {
                        (*map_rgb) (next.v[0], next.v[0],
                                    next.v[0], pdevc_next, pgs, dev,
                                    gs_color_select_source);
                        goto f;
                    }
                    decode_frac(next.v[0], cc, 0);
                    if_debug1('B', "[B]cc[0]=%g\n",
                              cc.paint.values[0]);
                    break;
                }
            default:		/* DeviceN */
                {
                    int i;

                    for (i = 1; i < spp; ++i)
                        next.v[i] = psrc[i];
                    psrc += spp;
                    if (!memcmp(next.v, run.v, spp * sizeof(next.v[0])))
                        goto inc;
                    if (use_mask_color &&
                        mask_color12_matches(next.v, penum, spp)
                        ) {
                        color_set_null(pdevc_next);
                        goto f;
                    }
                    for (i = 0; i < spp; ++i)
                        decode_frac(next.v[i], cc, i);
#ifdef DEBUG
                    if (gs_debug_c('B')) {
                        dmprintf2(dev->memory, "[B]cc[0..%d]=%g", spp - 1,
                                 cc.paint.values[0]);
                        for (i = 1; i < spp; ++i)
                            dmprintf1(dev->memory, ",%g", cc.paint.values[i]);
                        dmputs(dev->memory, "\n");
                    }
#endif
                }
                break;
        }
        mcode = remap_color(&cc, pcs, pdevc_next, pgs, dev,
                           gs_color_select_source);
        if (mcode < 0)
            goto fill;
f:
        if (sizeof(pdevc_next->colors.binary.color[0]) <= sizeof(ulong))
            if_debug7m('B', penum->memory,
                       "[B]0x%x,0x%x,0x%x,0x%x -> 0x%lx,0x%lx,0x%lx\n",
                       next.v[0], next.v[1], next.v[2], next.v[3],
                       (ulong)pdevc_next->colors.binary.color[0],
                       (ulong)pdevc_next->colors.binary.color[1],
                       (ulong)pdevc_next->type);
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
                       (ulong)pdevc_next->type);
/* NB: sizeof gx_color_index is 4 or 8 bytes! */

        /* Even though the supplied colors don't match, */
        /* the device colors might. */
        if (!dev_color_eq(devc1, devc2)) {
            /* Fill the region between xrun/irun and xl */
            gx_device_color *ptemp;

fill:
            if (posture != image_portrait) {	/* Parallelogram */
                code = (*dev_proc(dev, fill_parallelogram))
                    (dev, xrun, yrun,
                     xl - xrun, ytf - yrun, pdyx, pdyy,
                     pdevc, lop);
            } else {		/* Rectangle */
                int xi = irun;
                int wi = (irun = fixed2int_var_rounded(xl)) - xi;

                if (wi < 0)
                    xi += wi, wi = -wi;
                code = gx_fill_rectangle_device_rop(xi, yt,
                                                  wi, iht, pdevc, dev, lop);
            }
            if (code < 0)
                goto err;
            rsrc = psrc;
            if ((code = mcode) < 0)
                goto err;
            ptemp = pdevc;
            pdevc = pdevc_next;
            pdevc_next = ptemp;
            xrun = xl;
            yrun = ytf;
        }
        run = next;
inc:
        dda_next_assign(pnext.x, xl);
        dda_next_assign(pnext.y, ytf);
    }
    /* Fill the final run. */
    if (posture != image_portrait) {
        code = (*dev_proc(dev, fill_parallelogram))
            (dev, xrun, yrun, xl - xrun, ytf - yrun, pdyx, pdyy, pdevc, lop);
    } else {
        /* Same code as above near 'fill:' : */
        int xi = irun;
        int wi = (irun = fixed2int_var_rounded(xl)) - xi;

        if (wi < 0)
            xi += wi, wi = -wi;
        code = gx_fill_rectangle_device_rop(xi, yt,
                                          wi, iht, pdevc, dev, lop);
    }
    return (code < 0 ? code : 1);

    /* Save position if error, in case we resume. */
err:
    penum->used.x = (rsrc - spp - psrc_initial) / spp;
    penum->used.y = 0;
    return code;
}

static inline float
rescale_input_color(gs_range range, float input)
{
    return((input-range.rmin)/(range.rmax-range.rmin));
}

/* This one includes an extra adjustment for the CIE PS color space
   non standard range */
static void
decode_row_cie16(const gx_image_enum *penum, const unsigned short *psrc,
                 int spp, unsigned short *pdes,
                  const unsigned short *bufend, gs_range range_array[])
{
    unsigned short *curr_pos = pdes;
    int k;
    float temp;

    while ( curr_pos < bufend ) {
        for ( k = 0; k < spp; k ++ ) {
            switch ( penum->map[k].decoding ) {
                case sd_none:
                    *curr_pos = *psrc;
                    break;
                case sd_lookup:
                    temp = penum->map[k].decode_lookup[(*psrc) >> 4]*65535.0;
                    temp = rescale_input_color(range_array[k], temp);
                    temp = temp*65535;
                    if (temp > 65535.0) temp = 65535.0;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned short) temp;
                    break;
                case sd_compute:
                    temp = penum->map[k].decode_base +
                        (*psrc) * penum->map[k].decode_factor;
                    temp = rescale_input_color(range_array[k], temp);
                    temp = temp*65535;
                    if (temp > 65535) temp = 65535;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned short) temp;
                default:
                    break;
            }
            curr_pos++;
            psrc++;
        }
    }
}

static void
decode_row16(const gx_image_enum *penum, const unsigned short *psrc, int spp,
             unsigned short *pdes,const unsigned short *bufend)
{
    unsigned short *curr_pos = pdes;
    int k;
    float temp;

    while ( curr_pos < bufend ) {
        for ( k = 0; k < spp; k ++ ) {
            switch ( penum->map[k].decoding ) {
                case sd_none:
                    *curr_pos = *psrc;
                    break;
                case sd_lookup:
                    temp = penum->map[k].decode_lookup[(*psrc) >> 4]*65535;
                    if (temp > 65535) temp = 65535;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned short) temp;
                    break;
                case sd_compute:
                    temp = penum->map[k].decode_base +
                        (*psrc) * penum->map[k].decode_factor;
                    temp *= 65535;
                    if (temp > 65535) temp = 65535;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned short) temp;
                default:
                    break;
            }
            curr_pos++;
            psrc++;
        }
    }
}

/* Render an image with more than 8 bits per sample where we keep the data
   in 16 bit form and hand directly to the CMM */
static int
image_render_icc16(gx_image_enum * penum, const byte * buffer, int data_x,
                  uint w, int h, gx_device * dev)
{
    const gs_gstate *pgs = penum->pgs;
    gs_logical_operation_t lop = penum->log_op;
    gx_dda_fixed_point pnext;
    image_posture posture = penum->posture;
    fixed pdyx, pdyy;		/* edge of parallelogram */
    int vci, vdi;
    int spp = penum->spp;
    const unsigned short *psrc = (const unsigned short *)buffer + data_x * spp;
    fixed xrun;			/* x at start of run */
    int irun;			/* int xrun */
    fixed yrun;			/* y ditto */
    const unsigned short *bufend = psrc + w;
    unsigned short *run;
    int code = 0;
    gsicc_bufferdesc_t input_buff_desc;
    gsicc_bufferdesc_t output_buff_desc;
    unsigned short *psrc_cm, *psrc_cm_start, *psrc_decode, *psrc_cm_initial;
    int k;
    int spp_cm, num_pixels;
    bool need_decode = penum->icc_setup.need_decode;
    bool must_halftone = penum->icc_setup.must_halftone;
    bool has_transfer = penum->icc_setup.has_transfer;
    int num_des_comps;
    cmm_dev_profile_t *dev_profile;
    gx_cmapper_t data;
    gx_cmapper_fn *mapper;
    gx_color_value *conc = &data.conc[0];
    int first = 1;

    if (h == 0)
        return 0;

    if (penum->icc_link == NULL) {
        return gs_rethrow(-1, "ICC Link not created during image render icc16");
    }
    gx_get_cmapper(&data, pgs, dev, has_transfer, must_halftone, gs_color_select_source);
    mapper = data.set_color;
    /* Needed for device N */
    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    num_des_comps = gsicc_get_device_profile_comps(dev_profile);
    /* If the link is the identity, then we don't need to do any color
       conversions except for potentially a decode. */
    if (penum->icc_link->is_identity && !need_decode) {
        /* Fastest case.  No decode or CM needed */
        psrc_cm = (unsigned short *) psrc;
        spp_cm = spp;
        bufend = psrc_cm +  w;
        psrc_cm_start = NULL;
    } else {
        spp_cm = num_des_comps;
        psrc_cm = (unsigned short*) gs_alloc_bytes(pgs->memory,
                        sizeof(unsigned short)  * w * spp_cm/spp,
                        "image_render_icc16");
        psrc_cm_start = psrc_cm;
        bufend = psrc_cm +  w * spp_cm/spp;
        if (penum->icc_link->is_identity) {
            /* decode only. no CM.  This is slow but does not happen that often */
            decode_row16(penum, psrc, spp, psrc_cm, bufend);
        } else {
            /* Set up the buffer descriptors. */
            num_pixels = w/spp;
            gsicc_init_buffer(&input_buff_desc, spp, 2,
                          false, false, false, 0, w * 2,
                          1, num_pixels);
            gsicc_init_buffer(&output_buff_desc, spp_cm, 2,
                          false, false, false, 0, num_pixels * spp_cm * 2,
                          1, num_pixels);
            /* For now, just blast it all through the link. If we had a significant reduction
               we will want to repack the data first and then do this.  That will be
               an optimization shortly.  For now just allocate a new output
               buffer.  We can reuse the old one if the number of channels in the output is
               less than or equal to the new one.  */
            if (need_decode) {
                /* Need decode and CM.  This is slow but does not happen that often */
                psrc_decode = (unsigned short*) gs_alloc_bytes(pgs->memory,
                                sizeof(unsigned short) * w * spp_cm/spp,
                                "image_render_icc16");
                if (!penum->use_cie_range) {
                    decode_row16(penum, psrc, spp, psrc_decode,
                                    (const unsigned short*) (psrc_decode+w));
                } else {
                    /* Decode needs to include adjustment for CIE range */
                    decode_row_cie16(penum, psrc, spp, psrc_decode,
                                        (const unsigned short*) (psrc_decode+w),
                                         get_cie_range(penum->pcs));
                }
                (penum->icc_link->procs.map_buffer)(dev, penum->icc_link,
                                                    &input_buff_desc,
                                                    &output_buff_desc,
                                                    (void*) psrc_decode,
                                                    (void*) psrc_cm);
                gs_free_object(pgs->memory, (byte *)psrc_decode, "image_render_color_icc");
            } else {
                /* CM only. No decode */
                (penum->icc_link->procs.map_buffer)(dev, penum->icc_link,
                                                    &input_buff_desc,
                                                    &output_buff_desc,
                                                    (void*) psrc,
                                                    (void*) psrc_cm);
            }
        }
    }
    psrc_cm_initial = psrc_cm;

    pnext = penum->dda.pixel0;
    xrun = dda_current(pnext.x);
    yrun = dda_current(pnext.y);
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
               penum->y, data_x, w, fixed2float(xrun), fixed2float(yrun));
    while (psrc_cm < bufend) {
        /* Find the length of the next run. It will either end when we hit
         * the end of the source data, or when the pixel data differs. */
        run = psrc_cm + spp_cm;
        while (1)
        {
            dda_next(pnext.x);
            dda_next(pnext.y);
            if (run >= bufend)
                break;
            if (memcmp(run, psrc_cm, spp_cm * 2))
                break;
            if (posture == image_skewed) {
                if (first) /* We always need to map the first pixel on a line */
                    break;
                /* The color doesn't need remapping, but we can't handle a run */
                goto skewed_but_same;
            }
            run += spp_cm;
        }
        first = 0;
        /* So we have a run of pixels from psrc_cm to run that are all the same. */
        /* This needs to be sped up */
        for ( k = 0; k < spp_cm; k++ ) {
            conc[k] = psrc_cm[k];
        }
        mapper(&data);
        /* Fill the region between */
        /* xrun/irun and pnext.x/pnext.y */
        switch(posture)
        {
        default: /* skew */
skewed_but_same:
        {
            fixed xprev = dda_current(pnext.x);
            fixed yprev = dda_current(pnext.y);
            code = (*dev_proc(dev, fill_parallelogram))
                (dev, xrun, yrun, xprev - xrun, yprev - yrun, pdyx, pdyy,
                 &data.devc, lop);
            xrun = xprev;
            yrun = yprev;
            break;
        }
        case image_portrait:
        {
            int xi = irun;
            int wi = (irun = fixed2int_var_rounded(dda_current(pnext.x))) - xi;

            if (wi < 0)
                xi += wi, wi = -wi;
            if (wi > 0)
                code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
                                                    &data.devc, dev, lop);
            break;
        }
        case image_landscape:
        {
            int yi = irun;
            int hi = (irun = fixed2int_var_rounded(dda_current(pnext.y))) - yi;

            if (hi < 0)
                yi += hi, hi = -hi;
            if (hi > 0)
                code = gx_fill_rectangle_device_rop(vci, yi, vdi, hi,
                                                    &data.devc, dev, lop);
        }
        }
        if (code < 0)
            goto err;
        psrc_cm = run;
    }
    /* Free cm buffer, if it was used */
    if (psrc_cm_start != NULL) {
        gs_free_object(pgs->memory, (byte *)psrc_cm_start, "image_render_icc16");
    }
    return (code < 0 ? code : 1);

    /* Save position if error, in case we resume. */
err:
    gs_free_object(pgs->memory, (byte *)psrc_cm_start, "image_render_icc16");
    penum->used.x = (psrc_cm - psrc_cm_initial) / spp_cm;
    penum->used.y = 0;
    return code;
}
