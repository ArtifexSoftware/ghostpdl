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


/* PhotoShop (PSD) export device, supporting DeviceN color models. */

#include "math_.h"
#include "gxiodev.h"
#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"
#include "gdevdcrd.h"
#include "gstypes.h"
#include "gxdcconv.h"
#include "gdevdevn.h"
#include "gsequivc.h"
#include "gsicc_cache.h"
#include "gsicc_manage.h"
#include "gxgetbit.h"
#include "gdevppla.h"
#include "gxdownscale.h"
#include "gdevdevnprn.h"
#include "gdevpsd.h"
#include "gxdevsop.h"
#include "gsicc_cms.h"

#ifndef MAX_CHAN
#   define MAX_CHAN 15
#endif

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_open_device(psd_prn_open);
static dev_proc_close_device(psd_prn_close);
static dev_proc_get_params(psd_get_params);
static dev_proc_put_params(psd_put_params);
static dev_proc_get_params(psd_get_params_cmyk);
static dev_proc_put_params(psd_put_params_cmyk);
static dev_proc_print_page(psd_print_page);
static dev_proc_map_color_rgb(psd_map_color_rgb);
static dev_proc_get_color_mapping_procs(get_psdrgb_color_mapping_procs);
static dev_proc_get_color_mapping_procs(get_psd_color_mapping_procs);
static dev_proc_get_color_comp_index(psd_get_color_comp_index);

/* This is redundant with color_info.cm_name. We may eliminate this
   typedef and use the latter string for everything. */
typedef enum {
    psd_DEVICE_GRAY,
    psd_DEVICE_RGB,
    psd_DEVICE_CMYK,
    psd_DEVICE_N
} psd_color_model;

/*
 * A structure definition for a DeviceN type device
 */
typedef struct psd_device_s {
    gx_devn_prn_device_common;

    psd_color_model color_model;

    int max_spots;
    bool lock_colorants;

    gx_downscaler_params downscale;

    /* ICC color profile objects, for color conversion.
       These are all device link profiles.  At least that
       is how it appears looking at how this code
       was written to work with the old icclib.  Just
       doing minimal updates here so that it works
       with the new CMM API.  I would be interested
       to hear how people are using this. */

    char profile_rgb_fn[256];
    cmm_profile_t *rgb_profile;
    gcmmhlink_t rgb_icc_link;

    char profile_cmyk_fn[256];
    cmm_profile_t *cmyk_profile;
    gcmmhlink_t cmyk_icc_link;

    char profile_out_fn[256];
    cmm_profile_t *output_profile;
    gcmmhlink_t output_icc_link;

    bool warning_given;  /* Used to notify the user that max colorants reached */

} psd_device;

/* GC procedures */
static
ENUM_PTRS_WITH(psd_device_enum_ptrs, psd_device *pdev)
{
    ENUM_PREFIX(st_gx_devn_prn_device, 0);
    (void)pdev; /* Stop unused var warning */
    return 0;
}
ENUM_PTRS_END

static RELOC_PTRS_WITH(psd_device_reloc_ptrs, psd_device *pdev)
{
    RELOC_PREFIX(st_gx_devn_prn_device);
    (void)pdev; /* Stop unused var warning */
}
RELOC_PTRS_END

static int
psd_spec_op(gx_device *dev_, int op, void *data, int datasize)
{
    if (op == gxdso_supports_devn) {
        return true;
    }
    return gdev_prn_dev_spec_op(dev_, op, data, datasize);
}

/* Even though psd_device_finalize is the same as gx_devn_prn_device_finalize,
 * we need to implement it separately because st_composite_final
 * declares all 3 procedures as private. */
static void
psd_device_finalize(const gs_memory_t *cmem, void *vpdev)
{
    gx_devn_prn_device_finalize(cmem, vpdev);
}

gs_private_st_composite_final(st_psd_device, psd_device,
    "psd_device", psd_device_enum_ptrs, psd_device_reloc_ptrs,
    psd_device_finalize);

/*
 * Macro definition for psd device procedures
 */
#define device_procs(get_color_mapping_procs, params)\
{	psd_prn_open,\
        gx_default_get_initial_matrix,\
        NULL,				/* sync_output */\
        /* Since the print_page doesn't alter the device, this device can print in the background */\
        gdev_prn_bg_output_page,		/* output_page */\
        psd_prn_close,			/* close */\
        NULL,				/* map_rgb_color - not used */\
        psd_map_color_rgb,		/* map_color_rgb */\
        NULL,				/* fill_rectangle */\
        NULL,				/* tile_rectangle */\
        NULL,				/* copy_mono */\
        NULL,				/* copy_color */\
        NULL,				/* draw_line */\
        NULL,				/* get_bits */\
        psd_get_##params,		/* get_params */\
        psd_put_##params,		/* put_params */\
        NULL,				/* map_cmyk_color - not used */\
        NULL,				/* get_xfont_procs */\
        NULL,				/* get_xfont_device */\
        NULL,				/* map_rgb_alpha_color */\
        gx_page_device_get_page_device,	/* get_page_device */\
        NULL,				/* get_alpha_bits */\
        NULL,				/* copy_alpha */\
        NULL,				/* get_band */\
        NULL,				/* copy_rop */\
        NULL,				/* fill_path */\
        NULL,				/* stroke_path */\
        NULL,				/* fill_mask */\
        NULL,				/* fill_trapezoid */\
        NULL,				/* fill_parallelogram */\
        NULL,				/* fill_triangle */\
        NULL,				/* draw_thin_line */\
        NULL,				/* begin_image */\
        NULL,				/* image_data */\
        NULL,				/* end_image */\
        NULL,				/* strip_tile_rectangle */\
        NULL,				/* strip_copy_rop */\
        NULL,				/* get_clipping_box */\
        NULL,				/* begin_typed_image */\
        NULL,				/* get_bits_rectangle */\
        NULL,				/* map_color_rgb_alpha */\
        NULL,				/* create_compositor */\
        NULL,				/* get_hardware_params */\
        NULL,				/* text_begin */\
        NULL,				/* finish_copydevice */\
        NULL,				/* begin_transparency_group */\
        NULL,				/* end_transparency_group */\
        NULL,				/* begin_transparency_mask */\
        NULL,				/* end_transparency_mask */\
        NULL,				/* discard_transparency_layer */\
        get_color_mapping_procs,	/* get_color_mapping_procs */\
        psd_get_color_comp_index,	/* get_color_comp_index */\
        gx_devn_prn_encode_color,	/* encode_color */\
        gx_devn_prn_decode_color,	/* decode_color */\
        NULL,				/* pattern_manage */\
        NULL,				/* fill_rectangle_hl_color */\
        NULL,				/* include_color_space */\
        NULL,				/* fill_linear_color_scanline */\
        NULL,				/* fill_linear_color_trapezoid */\
        NULL,				/* fill_linear_color_triangle */\
        gx_devn_prn_update_spot_equivalent_colors, /* update_spot_equivalent_colors */\
        gx_devn_prn_ret_devn_params, /* ret_devn_params */\
        NULL,                       /* fillpage */\
        NULL,                       /* push_transparency_state */\
        NULL,                       /* pop_transparency_state */\
        NULL,                       /* put_image */\
        psd_spec_op                 /* dev_spec_op */\
}

#define psd_device_body(procs, dname, ncomp, pol, depth, mg, mc, sl, cn)\
    std_device_full_body_type_extended(psd_device, &procs, dname,\
          &st_psd_device,\
          (int)((long)(DEFAULT_WIDTH_10THS) * (X_DPI) / 10),\
          (int)((long)(DEFAULT_HEIGHT_10THS) * (Y_DPI) / 10),\
          X_DPI, Y_DPI,\
          GX_DEVICE_COLOR_MAX_COMPONENTS,	/* MaxComponents */\
          ncomp,		/* NumComp */\
          pol,			/* Polarity */\
          depth, 0,		/* Depth, GrayIndex */\
          mg, mc,		/* MaxGray, MaxColor */\
          mg + 1, mc + 1,	/* DitherGray, DitherColor */\
          sl,			/* Linear & Separable? */\
          cn,			/* Process color model name */\
          0, 0,			/* offsets */\
          0, 0, 0, 0		/* margins */\
        ),\
        prn_device_body_rest_(psd_print_page)

/*
 * PSD device with RGB process color model.
 */
static const gx_device_procs spot_rgb_procs =
    device_procs(get_psdrgb_color_mapping_procs, params);

const psd_device gs_psdrgb_device =
{
    psd_device_body(spot_rgb_procs, "psdrgb", 3, GX_CINFO_POLARITY_ADDITIVE, 24, 255, 255, GX_CINFO_SEP_LIN, "DeviceRGB"),
    /* devn_params specific parameters */
    { 8,	/* Bits per color - must match ncomp, depth, etc. above */
      DeviceRGBComponents,	/* Names of color model colorants */
      3,			/* Number colorants for RGB */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    { true },			/* equivalent CMYK colors for spot colors */
    /* PSD device specific parameters */
    psd_DEVICE_RGB,		/* Color model */
    GS_SOFT_MAX_SPOTS,           /* max_spots */
    false,                      /* colorants not locked */
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

const psd_device gs_psdrgb16_device =
{
    psd_device_body(spot_rgb_procs, "psdrgb16", 3, GX_CINFO_POLARITY_ADDITIVE, 48, 65535, 65535, GX_CINFO_SEP_LIN, "DeviceRGB"),
    /* devn_params specific parameters */
    { 16,	/* Bits per color - must match ncomp, depth, etc. above */
    DeviceRGBComponents,	/* Names of color model colorants */
    3,			/* Number colorants for RGB */
    0,			/* MaxSeparations has not been specified */
    -1,			/* PageSpotColors has not been specified */
    { 0 },			/* SeparationNames */
    0,			/* SeparationOrder names */
    { 0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    { true },			/* equivalent CMYK colors for spot colors */
    /* PSD device specific parameters */
    psd_DEVICE_RGB,		/* Color model */
    GS_SOFT_MAX_SPOTS,           /* max_spots */
    false,                      /* colorants not locked */
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/*
 * PSD device with CMYK process color model and spot color support.
 */
static const gx_device_procs spot_cmyk_procs
        = device_procs(get_psd_color_mapping_procs, params_cmyk);

const psd_device gs_psdcmyk_device =
{
    psd_device_body(spot_cmyk_procs, "psdcmyk",
                    ARCH_SIZEOF_GX_COLOR_INDEX, /* Number of components - need a nominal 1 bit for each */
                    GX_CINFO_POLARITY_SUBTRACTIVE,
                    ARCH_SIZEOF_GX_COLOR_INDEX * 8, /* 8 bits per component (albeit in planes) */
                    255, 255, GX_CINFO_SEP_LIN, "DeviceCMYK"),
    /* devn_params specific parameters */
    { 8,	/* Bits per color - must match ncomp, depth, etc. above */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    { true },			/* equivalent CMYK colors for spot colors */
    /* PSD device specific parameters */
    psd_DEVICE_CMYK,		/* Color model */
    GS_SOFT_MAX_SPOTS,          /* max_spots */
    false,                      /* colorants not locked */
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

const psd_device gs_psdcmyk16_device =
{
    psd_device_body(spot_cmyk_procs, "psdcmyk16",
    ARCH_SIZEOF_GX_COLOR_INDEX, /* Number of components - need a nominal 1 bit for each */
    GX_CINFO_POLARITY_SUBTRACTIVE,
    ARCH_SIZEOF_GX_COLOR_INDEX * 16, /* 8 bits per component (albeit in planes) */
    65535, 65535, GX_CINFO_SEP_LIN, "DeviceCMYK"),
    /* devn_params specific parameters */
    { 16,	/* Bits per color - must match ncomp, depth, etc. above */
    DeviceCMYKComponents,	/* Names of color model colorants */
    4,			/* Number colorants for CMYK */
    0,			/* MaxSeparations has not been specified */
    -1,			/* PageSpotColors has not been specified */
    { 0 },			/* SeparationNames */
    0,			/* SeparationOrder names */
    { 0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    { true },			/* equivalent CMYK colors for spot colors */
    /* PSD device specific parameters */
    psd_DEVICE_CMYK,		/* Color model */
    GS_SOFT_MAX_SPOTS,          /* max_spots */
    false,                      /* colorants not locked */
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* Open the psd devices */
int
psd_prn_open(gx_device * pdev)
{
    psd_device *pdev_psd = (psd_device *) pdev;
    int code;
    int k;
    bool force_pdf, force_ps;
    cmm_dev_profile_t *profile_struct;

#ifdef TEST_PAD_AND_ALIGN
    pdev->pad = 5;
    pdev->log2_align_mod = 6;
#endif

    /* There are 2 approaches to the use of a DeviceN ICC output profile.
       One is to simply limit our device to only output the colorants
       defined in the output ICC profile.   The other is to use the
       DeviceN ICC profile to color manage those N colorants and
       to let any other separations pass through unmolested.   The define
       LIMIT_TO_ICC sets the option to limit our device to only the ICC
       colorants defined by -sICCOutputColors (or to the ones that are used
       as default names if ICCOutputColors is not used).  The pass through option
       (LIMIT_TO_ICC set to 0) makes life a bit more difficult since we don't
       know if the page_spot_colors overlap with any spot colorants that exist
       in the DeviceN ICC output profile. Hence we don't know how many planes
       to use for our device.  This is similar to the issue when processing
       a PostScript file.  So that I remember, the cases are
       DeviceN Profile?     limit_icc       Result
       0                    0               force_pdf 0 force_ps 0  (no effect)
       0                    0               force_pdf 0 force_ps 0  (no effect)
       1                    0               force_pdf 0 force_ps 1  (colorants not known)
       1                    1               force_pdf 1 force_ps 0  (colorants known)
       */
    code = dev_proc(pdev, get_profile)((gx_device *)pdev, &profile_struct);
    if (profile_struct->spotnames == NULL) {
        force_pdf = false;
        force_ps = false;
    } else {
#if LIMIT_TO_ICC
        force_pdf = true;
        force_ps = false;
#else
        force_pdf = false;
        force_ps = true;
#endif
    }
    pdev_psd->warning_given = false;

    /* For the planar device we need to set up the bit depth of each plane.
       For other devices this is handled in check_device_separable where
       we compute the bit shift for the components etc. */
    for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
        pdev->color_info.comp_bits[k] = 8;
    }

    /* With planar the depth can be more than 64.  Update the color
       info to reflect the proper depth and number of planes.  Also note
       that the number of spot colors can change from page to page.
       Update things so that we only output separations for the
       inks on that page. */
    if (pdev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE) {
        if ((pdev_psd->devn_params.page_spot_colors >= 0 || force_pdf) && !force_ps) {
            if (force_pdf) {
                /* Use the information that is in the ICC profle.  We will be here
                   anytime that we have limited ourselves to a fixed number
                   of colorants specified by the DeviceN ICC profile */
                pdev->color_info.num_components =
                    (pdev_psd->devn_params.separations.num_separations
                    + pdev_psd->devn_params.num_std_colorant_names);
                if (pdev->color_info.num_components > pdev->color_info.max_components)
                    pdev->color_info.num_components = pdev->color_info.max_components;
                /* Limit us only to the ICC colorants */
                pdev->color_info.max_components = pdev->color_info.num_components;
            } else {
                /* Use the information that is in the page spot color.  We should
                   be here if we are processing a PDF and we do not have a DeviceN
                   ICC profile specified for output */
                if (!(pdev_psd->lock_colorants)) {
                    pdev->color_info.num_components =
                        (pdev_psd->devn_params.page_spot_colors
                        + pdev_psd->devn_params.num_std_colorant_names);
                    if (pdev->color_info.num_components > pdev->color_info.max_components)
                        pdev->color_info.num_components = pdev->color_info.max_components;
                }
            }
        } else {
            /* We do not know how many spots may occur on the page.
               For this reason we go ahead and allocate the maximum that we
               have available.  Note, lack of knowledge only occurs in the case
               of PS files.  With PDF we know a priori the number of spot
               colorants.  */
            if (!(pdev_psd->lock_colorants)) {
                int num_comp = pdev_psd->max_spots + 4; /* Spots + CMYK */
                if (num_comp > GS_CLIENT_COLOR_MAX_COMPONENTS)
                    num_comp = GS_CLIENT_COLOR_MAX_COMPONENTS;
                pdev->color_info.num_components = num_comp;
                pdev->color_info.max_components = num_comp;
            }
        }
    }
    /* Push this to the max amount as a default if someone has not set it */
    if (pdev_psd->devn_params.num_separation_order_names == 0)
        for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
            pdev_psd->devn_params.separation_order_map[k] = k;
        }
    pdev->color_info.depth = pdev->color_info.num_components *
                             pdev_psd->devn_params.bitspercomponent;
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    set_linear_color_bits_mask_shift(pdev);
    if (pdev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        pdev->icc_struct->supports_devn = false;
    else
        pdev->icc_struct->supports_devn = true;
    code = gdev_prn_open_planar(pdev, true);
    return code;
}

/* 2007/05/04
psdgray device
*/
static void
gray_cs_to_psdgray_cm(gx_device * dev, frac gray, frac out[])
{
    out[0] = gray;
}

static void
rgb_cs_to_psdgray_cm(gx_device * dev, const gs_gstate *pgs,
                                   frac r, frac g, frac b, frac out[])
{
    out[0] = color_rgb_to_gray(r, g, b, NULL);
}

static void
cmyk_cs_to_psdgray_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    out[0] = color_cmyk_to_gray(c, m, y, k, NULL);
}

/*
 * The following procedures are used to map the standard color spaces into
 * the color components for the psdrgb device.
 */
static void
gray_cs_to_psdrgb_cm(gx_device * dev, frac gray, frac out[])
{
    int i = ((psd_device *)dev)->devn_params.separations.num_separations;

    out[0] = out[1] = out[2] = gray;
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
}

static void
rgb_cs_to_psdrgb_cm(gx_device * dev, const gs_gstate *pgs,
                                  frac r, frac g, frac b, frac out[])
{
    int i = ((psd_device *)dev)->devn_params.separations.num_separations;

    out[0] = r;
    out[1] = g;
    out[2] = b;
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
}

static void
cmyk_cs_to_psdrgb_cm(gx_device * dev,
                        frac c, frac m, frac y, frac k, frac out[])
{
    int i = ((psd_device *)dev)->devn_params.separations.num_separations;

    color_cmyk_to_rgb(c, m, y, k, NULL, out, dev->memory);
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
}

/* Color mapping routines for the psdcmyk device */

static void
gray_cs_to_psdcmyk_cm(gx_device * dev, frac gray, frac out[])
{
    int * map = ((psd_device *) dev)->devn_params.separation_order_map;

    gray_cs_to_devn_cm(dev, map, gray, out);
}

static void
rgb_cs_to_psdcmyk_cm(gx_device * dev, const gs_gstate *pgs,
                           frac r, frac g, frac b, frac out[])
{
    int * map = ((psd_device *) dev)->devn_params.separation_order_map;

    rgb_cs_to_devn_cm(dev, map, pgs, r, g, b, out);
}

static void
cmyk_cs_to_psdcmyk_cm(gx_device * dev,
                        frac c, frac m, frac y, frac k, frac out[])
{
    const gs_devn_params *devn = gx_devn_prn_ret_devn_params(dev);
    const int *map = devn->separation_order_map;
    int j;

    if (devn->num_separation_order_names > 0) {
        /* This is to set only those that we are using */
        int ncomps = dev->color_info.num_components;
        for (j = 0; j < ncomps; j++)
            out[j] = 0;
        for (j = 0; j < devn->num_separation_order_names; j++) {
            switch (map[j]) {
                case 0 :
                    out[0] = c;
                    break;
                case 1:
                    out[1] = m;
                    break;
                case 2:
                    out[2] = y;
                    break;
                case 3:
                    out[3] = k;
                    break;
                default:
                    break;
            }
        }
    } else {
        cmyk_cs_to_devn_cm(dev, map, c, m, y, k, out);
    }
}

static void
cmyk_cs_to_spotn_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    psd_device *xdev = (psd_device *)dev;
    int n = xdev->devn_params.separations.num_separations;

    gcmmhlink_t link = xdev->cmyk_icc_link;
    int i;

    if (link != NULL) {

        unsigned short in[4];
        unsigned short tmp[MAX_CHAN];
        int outn = xdev->cmyk_profile->num_comps_out;

        in[0] = frac2ushort(c);
        in[1] = frac2ushort(m);
        in[2] = frac2ushort(y);
        in[3] = frac2ushort(k);

        gscms_transform_color(dev, link, &(in[0]),
                        &(tmp[0]), 2);

        for (i = 0; i < outn; i++)
            out[i] = ushort2frac(tmp[i]);
        for (; i < n + 4; i++)
            out[i] = 0;

    } else {
        /* If no profile given, assume CMYK */
        out[0] = c;
        out[1] = m;
        out[2] = y;
        out[3] = k;
        for(i = 0; i < n; i++)			/* Clear spot colors */
            out[4 + i] = 0;
    }
}

static void
gray_cs_to_spotn_cm(gx_device * dev, frac gray, frac out[])
{
    cmyk_cs_to_spotn_cm(dev, 0, 0, 0, (frac)(frac_1 - gray), out);
}

static void
rgb_cs_to_spotn_cm(gx_device * dev, const gs_gstate *pgs,
                                   frac r, frac g, frac b, frac out[])
{
    psd_device *xdev = (psd_device *)dev;
    int n = xdev->devn_params.separations.num_separations;
    gcmmhlink_t link = xdev->rgb_icc_link;
    int i;

    if (link != NULL) {

        unsigned short in[3];
        unsigned short tmp[MAX_CHAN];
        int outn = xdev->rgb_profile->num_comps_out;

        in[0] = frac2ushort(r);
        in[1] = frac2ushort(g);
        in[2] = frac2ushort(b);

        gscms_transform_color(dev, link, &(in[0]),
                        &(tmp[0]), 2);

        for (i = 0; i < outn; i++)
            out[i] = ushort2frac(tmp[i]);
        for (; i < n + 4; i++)
            out[i] = 0;

    } else {
        frac cmyk[4];

        color_rgb_to_cmyk(r, g, b, pgs, cmyk, dev->memory);
        cmyk_cs_to_spotn_cm(dev, cmyk[0], cmyk[1], cmyk[2], cmyk[3],
                            out);
    }
}

static const gx_cm_color_map_procs psdGray_procs = {/* 2007/05/04 Test */
    gray_cs_to_psdgray_cm, rgb_cs_to_psdgray_cm, cmyk_cs_to_psdgray_cm
};

static const gx_cm_color_map_procs psdRGB_procs = {
    gray_cs_to_psdrgb_cm, rgb_cs_to_psdrgb_cm, cmyk_cs_to_psdrgb_cm
};

static const gx_cm_color_map_procs psdCMYK_procs = {
    gray_cs_to_psdcmyk_cm, rgb_cs_to_psdcmyk_cm, cmyk_cs_to_psdcmyk_cm
};

static const gx_cm_color_map_procs psdN_procs = {
    gray_cs_to_spotn_cm, rgb_cs_to_spotn_cm, cmyk_cs_to_spotn_cm
};

/*
 * These are the handlers for returning the list of color space
 * to color model conversion routines.
 */
static const gx_cm_color_map_procs *
get_psdrgb_color_mapping_procs(const gx_device * dev)
{
    return &psdRGB_procs;
}

static const gx_cm_color_map_procs *
get_psd_color_mapping_procs(const gx_device * dev)
{
    const psd_device *xdev = (const psd_device *)dev;

    if (xdev->color_model == psd_DEVICE_RGB)
        return &psdRGB_procs;
    else if (xdev->color_model == psd_DEVICE_CMYK)
        return &psdCMYK_procs;
    else if (xdev->color_model == psd_DEVICE_N)
        return &psdN_procs;
    else if (xdev->color_model == psd_DEVICE_GRAY)
        return &psdGray_procs;
    else
        return NULL;
}

/*
 * Convert a gx_color_index to RGB.
 */
static int
psd_map_color_rgb(gx_device *dev, gx_color_index color, gx_color_value rgb[3])
{
    psd_device *xdev = (psd_device *)dev;

    if (xdev->color_model == psd_DEVICE_RGB)
        return gx_devn_prn_decode_color(dev, color, rgb);
    /* TODO: return reasonable values. */
    rgb[0] = 0;
    rgb[1] = 0;
    rgb[2] = 0;
    return 0;
}

/* Get parameters.  We provide a default CRD. */
static int
psd_get_params_generic(gx_device * pdev, gs_param_list * plist, int cmyk)
{
    psd_device *xdev = (psd_device *)pdev;
    int code;

    code = gx_devn_prn_get_params(pdev, plist);
    if (code < 0)
        return code;

    code = gx_downscaler_write_params(plist, &xdev->downscale,
                                      cmyk ? GX_DOWNSCALER_PARAMS_TRAP : 0);
    if (code < 0)
        return code;
    code = param_write_int(plist, "MaxSpots", &xdev->max_spots);
    if (code < 0)
        return code;
    code = param_write_bool(plist, "LockColorants", &xdev->lock_colorants);
    return code;
}

static int
psd_get_params(gx_device * pdev, gs_param_list * plist)
{
    return psd_get_params_generic(pdev, plist, 0);
}

static int
psd_get_params_cmyk(gx_device * pdev, gs_param_list * plist)
{
    return psd_get_params_generic(pdev, plist, 1);
}



/* Compare a C string and a gs_param_string. */
static bool
param_string_eq(const gs_param_string *pcs, const char *str)
{
    return (strlen(str) == pcs->size &&
            !strncmp(str, (const char *)pcs->data, pcs->size));
}

static int
psd_set_color_model(psd_device *xdev, psd_color_model color_model)
{
    xdev->color_model = color_model;
    if (color_model == psd_DEVICE_GRAY) {
        xdev->devn_params.std_colorant_names = DeviceGrayComponents;
        xdev->devn_params.num_std_colorant_names = 1;
        xdev->color_info.cm_name = "DeviceGray";
        xdev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
    } else if (color_model == psd_DEVICE_RGB) {
        xdev->devn_params.std_colorant_names = DeviceRGBComponents;
        xdev->devn_params.num_std_colorant_names = 3;
        xdev->color_info.cm_name = "DeviceRGB";
        xdev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
    } else if (color_model == psd_DEVICE_CMYK) {
        xdev->devn_params.std_colorant_names = DeviceCMYKComponents;
        xdev->devn_params.num_std_colorant_names = 4;
        xdev->color_info.cm_name = "DeviceCMYK";
        xdev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else if (color_model == psd_DEVICE_N) {
        xdev->devn_params.std_colorant_names = DeviceCMYKComponents;
        xdev->devn_params.num_std_colorant_names = 4;
        xdev->color_info.cm_name = "DeviceN";
        xdev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else {
        return -1;
    }

    return 0;
}

/* Set parameters.  We allow setting the number of bits per component. */
static int
psd_put_params_generic(gx_device * pdev, gs_param_list * plist, int cmyk)
{
    psd_device * const pdevn = (psd_device *) pdev;
    int code = 0;
    gs_param_string pcm;
    psd_color_model color_model = pdevn->color_model;
    gx_device_color_info save_info = pdevn->color_info;

    code = gx_downscaler_read_params(plist, &pdevn->downscale,
                                     cmyk ? GX_DOWNSCALER_PARAMS_TRAP : 0);
    if (code < 0)
        return code;

    switch (code = param_read_bool(plist, "LockColorants", &(pdevn->lock_colorants))) {
        case 0:
            break;
        case 1:
            break;
        default:
            param_signal_error(plist, "LockColorants", code);
            return code;
    }

    switch (code = param_read_int(plist,
                                  "MaxSpots",
                                  &pdevn->max_spots)) {
        case 0:
            if (pdevn->max_spots >= 0 && pdevn->max_spots <= GS_CLIENT_COLOR_MAX_COMPONENTS-4)
                break;
            emprintf1(pdevn->memory, "MaxSpots must be between 0 and %d\n",
                      GS_CLIENT_COLOR_MAX_COMPONENTS-4);
            code = gs_note_error(gs_error_rangecheck);
            /* fall through */
        default:
            param_signal_error(plist, "MaxSpots", code);
            return code;
        case 1:
            break;
    }

    if (code >= 0)
        code = param_read_name(plist, "ProcessColorModel", &pcm);

    /* Bug 696318.  Don't allow process color model change for RGB device */
    if (code == 0 && color_model != psd_DEVICE_RGB) {
        if (param_string_eq (&pcm, "DeviceGray"))
            color_model = psd_DEVICE_GRAY;
        else if (param_string_eq (&pcm, "DeviceRGB"))
            color_model = psd_DEVICE_RGB;
        else if (param_string_eq (&pcm, "DeviceCMYK"))
            color_model = psd_DEVICE_CMYK;
        else if (param_string_eq (&pcm, "DeviceN"))
            color_model = psd_DEVICE_N;
        else {
            param_signal_error(plist, "ProcessColorModel",
                               code = gs_error_rangecheck);
        }
    }

    if (code >= 0)
        code = psd_set_color_model(pdevn, color_model);

    /* handle the standard DeviceN related parameters */
    if (code == 0)
        code = gx_devn_prn_put_params(pdev, plist);

    if (code < 0) {
        pdev->color_info = save_info;
        return code;
    }

    return code;
}

static int
psd_put_params(gx_device * pdev, gs_param_list * plist)
{
    return psd_put_params_generic(pdev, plist, 0);
}

static int
psd_put_params_cmyk(gx_device * pdev, gs_param_list * plist)
{
    return psd_put_params_generic(pdev, plist, 1);
}

/*
 * This routine will check to see if the color component name  match those
 * that are available amoung the current device's color components.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns a negative value if not found.
 */
static int
psd_get_color_comp_index(gx_device * dev, const char * pname,
                                        int name_size, int component_type)
{
    int index;
    psd_device *pdev = (psd_device *)dev;

    if (strncmp(pname, "None", name_size) == 0) return -1;
    index = gx_devn_prn_get_color_comp_index(dev, pname, name_size,
                                             component_type);
    /* This is a one shot deal.  That is it will simply post a notice once that
       some colorants will be converted due to a limit being reached.  It will
       not list names of colorants since then I would need to keep track of
       which ones I have already mentioned.  Also, if someone is fooling with
       num_order, then this warning is not given since they should know what
       is going on already */
    if (index < 0 && component_type == SEPARATION_NAME &&
        pdev->warning_given == false &&
        pdev->devn_params.num_separation_order_names == 0) {
        dmlprintf(dev->memory, "**** Max spot colorants reached.\n");
        dmlprintf(dev->memory, "**** Some colorants will be converted to equivalent CMYK values.\n");
        dmlprintf(dev->memory, "**** If this is a Postscript file, try using the -dMaxSpots= option.\n");
        pdev->warning_given = true;
    }
    return index;
}

/* ------ Private definitions ------ */

/* All two-byte quantities are stored MSB-first! */
#if ARCH_IS_BIG_ENDIAN
#  define assign_u16(a,v) a = (v)
#  define assign_u32(a,v) a = (v)
#else
#  define assign_u16(a,v) a = ((v) >> 8) + ((v) << 8)
#  define assign_u32(a,v) a = (((v) >> 24) & 0xff) + (((v) >> 8) & 0xff00) + (((v) & 0xff00) << 8) + (((v) & 0xff) << 24)
#endif

int
psd_setup(psd_write_ctx *xc, gx_devn_prn_device *dev, gp_file *file, int w, int h)
{
    int i;
    int spot_count;

    xc->f = file;

#define NUM_CMYK_COMPONENTS 4
    for (i = 0; i < GX_DEVICE_COLOR_MAX_COMPONENTS; i++) {
        if (dev->devn_params.std_colorant_names[i] == NULL)
            break;
    }
    xc->base_num_channels = dev->devn_params.num_std_colorant_names;
    xc->num_channels = i;
    if (dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE
        && strcmp(dev->dname, "psdcmykog") != 0) {
        if (dev->devn_params.num_separation_order_names == 0) {
            xc->n_extra_channels = dev->devn_params.separations.num_separations;
        } else {
            /* Have to figure out how many in the order list were not std
               colorants */
            spot_count = 0;
            for (i = 0; i < dev->devn_params.num_separation_order_names; i++) {
                if (dev->devn_params.separation_order_map[i] >= NUM_CMYK_COMPONENTS) {
                    spot_count++;
                }
            }
            xc->n_extra_channels = spot_count;
        }
    } else {
        xc->n_extra_channels = 0;
    }
    xc->width = w;
    xc->height = h;
    /*
     * Determine the order of the output components.  This is based upon
     * the SeparationOrder parameter.  This parameter can be used to select
     * which planes are actually imaged.  For the process color model channels
     * we image the channels which are requested.  Non requested process color
     * model channels are simply filled with white.  For spot colors we only
     * image the requested channels.
     */
    for (i = 0; i < xc->num_channels + xc->n_extra_channels; i++) {
        xc->chnl_to_position[i] = i;
        xc->chnl_to_orig_sep[i] = i;
    }
    /* If we had a specify order name, then we may need to adjust things */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE &&
        strcmp(dev->dname, "psdcmykog") != 0) {
        if (dev->devn_params.num_separation_order_names > 0) {
            for (i = 0; i < dev->devn_params.num_separation_order_names; i++) {
                int sep_order_num = dev->devn_params.separation_order_map[i];
                if (sep_order_num >= NUM_CMYK_COMPONENTS) {
                    xc->chnl_to_position[xc->num_channels] = sep_order_num;
                    xc->chnl_to_orig_sep[xc->num_channels++] = sep_order_num;
                }
            }
        } else {
            xc->num_channels += dev->devn_params.separations.num_separations;
        }
    }
    return 0;
}

int
psd_write(psd_write_ctx *xc, const byte *buf, int size) {
    int code;

    code = gp_fwrite(buf, 1, size, xc->f);
    if (code < 0)
        return code;
    return 0;
}

int
psd_write_8(psd_write_ctx *xc, byte v)
{
    return psd_write(xc, (byte *)&v, 1);
}

int
psd_write_16(psd_write_ctx *xc, bits16 v)
{
    bits16 buf;

    assign_u16(buf, v);
    return psd_write(xc, (byte *)&buf, 2);
}

int
psd_write_32(psd_write_ctx *xc, bits32 v)
{
    bits32 buf;

    assign_u32(buf, v);
    return psd_write(xc, (byte *)&buf, 4);
}

static fixed_colorant_name
get_sep_name(gx_devn_prn_device *pdev, int n)
{
    fixed_colorant_name p = NULL;
    int i;

    for (i = 0; i <= n; i++) {
        p = pdev->devn_params.std_colorant_names[i];
        if (p == NULL)
            break;
    }
    return p;
}

int
psd_write_header(psd_write_ctx *xc, gx_devn_prn_device *pdev)
{
    int code = 0;
    int num_channels = xc->num_channels;
    int bpc = pdev->devn_params.bitspercomponent;
    int chan_idx;
    int chan_names_len = 0;
    int sep_num;
    const devn_separation_name *separation_name;

    psd_write(xc, (const byte *)"8BPS", 4); /* Signature */
    psd_write_16(xc, 1); /* Version - Always equal to 1*/
    /* Reserved 6 Bytes - Must be zero */
    psd_write_32(xc, 0);
    psd_write_16(xc, 0);
    psd_write_16(xc, (bits16)num_channels); /* Channels (2 Bytes) - Supported range is 1 to 56 */
    psd_write_32(xc, xc->height); /* Rows */
    psd_write_32(xc, xc->width); /* Columns */
    psd_write_16(xc, bpc); /* Depth - 1, 8 and 16 */
    /* Modes: Bitmap=0, Grayscale=1, RGB=3, CMYK=4 MultiChannel=7 Lab=9 */
    psd_write_16(xc, (bits16) xc->base_num_channels);  /* We use 1, 3 or 4. */

    /* Color Mode Data */
    psd_write_32(xc, 0); 	/* No color mode data */

    /* Image Resources */

    /* Channel Names */
    for (chan_idx = NUM_CMYK_COMPONENTS; chan_idx < xc->num_channels; chan_idx++) {
        fixed_colorant_name n = pdev->devn_params.std_colorant_names[chan_idx];
        if (n == NULL)
            break;
        chan_names_len += strlen(n) + 1;
    }
    for (; chan_idx < xc->num_channels; chan_idx++) {
        sep_num = xc->chnl_to_orig_sep[chan_idx] - NUM_CMYK_COMPONENTS;
        separation_name = &(pdev->devn_params.separations.names[sep_num]);
        chan_names_len += (separation_name->size + 1);
    }
    psd_write_32(xc, 12 + (chan_names_len + (chan_names_len % 2))
                        + (12 + (14 * (xc->num_channels - xc->base_num_channels)))
                        + 28);
    psd_write(xc, (const byte *)"8BIM", 4);
    psd_write_16(xc, 1006); /* 0x03EE */
    psd_write_16(xc, 0); /* PString */
    psd_write_32(xc, chan_names_len + (chan_names_len % 2));
    for (chan_idx = NUM_CMYK_COMPONENTS; chan_idx < xc->num_channels; chan_idx++) {
        int len;
        fixed_colorant_name n = pdev->devn_params.std_colorant_names[chan_idx];
        if (n == NULL)
            break;
        len = strlen(n);
        psd_write_8(xc, (byte)len);
        psd_write(xc, (const byte *)n, len);
    }
    for (; chan_idx < xc->num_channels; chan_idx++) {
        sep_num = xc->chnl_to_orig_sep[chan_idx] - NUM_CMYK_COMPONENTS;
        separation_name = &(pdev->devn_params.separations.names[sep_num]);
        psd_write_8(xc, (byte) separation_name->size);
        psd_write(xc, separation_name->data, separation_name->size);
    }
    if (chan_names_len % 2)
        psd_write_8(xc, 0); /* pad */

    /* DisplayInfo - Colors for each spot channels */
    psd_write(xc, (const byte *)"8BIM", 4);
    psd_write_16(xc, 1007); /* 0x03EF */
    psd_write_16(xc, 0); /* PString */
    psd_write_32(xc, 14 * (xc->num_channels - xc->base_num_channels)); /* Length */
    for (chan_idx = NUM_CMYK_COMPONENTS; chan_idx < xc->num_channels; chan_idx++) {
        sep_num = xc->chnl_to_orig_sep[chan_idx] - NUM_CMYK_COMPONENTS;
        psd_write_16(xc, 02); /* CMYK */
        /* PhotoShop stores all component values as if they were additive. */
        if (pdev->equiv_cmyk_colors.color[sep_num].color_info_valid) {
#define convert_color(component) ((bits16)((65535 * ((double)\
    (frac_1 - pdev->equiv_cmyk_colors.color[sep_num].component)) / frac_1)))
            psd_write_16(xc, convert_color(c)); /* Cyan */
            psd_write_16(xc, convert_color(m)); /* Magenta */
            psd_write_16(xc, convert_color(y)); /* Yellow */
            psd_write_16(xc, convert_color(k)); /* Black */
#undef convert_color
        } else {
            /* This is a bit of a hack, introduced for the psdcmykog device
             * so that we get a reasonable approximation for the colors out
             * even when used without the appropriate profile. */
            fixed_colorant_name sepname = get_sep_name(pdev, chan_idx);
            if (sepname && !strcmp(sepname, "Artifex Orange")) {
                psd_write_16(xc, 0xfbde); /* Cyan */
                psd_write_16(xc, 0x7376); /* Magenta */
                psd_write_16(xc, 0x0000); /* Yellow */
                psd_write_16(xc, 0xffff); /* Black */
            } else if (sepname && !strcmp(sepname, "Artifex Green")) {
                psd_write_16(xc, 0x0000); /* Cyan */
                psd_write_16(xc, 0xe33d); /* Magenta */
                psd_write_16(xc, 0x0000); /* Yellow */
                psd_write_16(xc, 0xf8c8); /* Black */
            } else {
                /* Else set C = M = Y = 0, K = 1 */
                psd_write_16(xc, 65535); /* Cyan */
                psd_write_16(xc, 65535); /* Magenta */
                psd_write_16(xc, 65535); /* Yellow */
                psd_write_16(xc, 0); /* Black */
            }
        }
        psd_write_16(xc, 0); /* Opacity 0 to 100 */
        psd_write_8(xc, 2); /* Don't know */
        psd_write_8(xc, 0); /* Padding - Always Zero */
    }

    /* Image resolution */
    psd_write(xc, (const byte *)"8BIM", 4);
    psd_write_16(xc, 1005); /* 0x03ED */
    psd_write_16(xc, 0); /* PString */
    psd_write_32(xc, 16); /* Length */
                /* Resolution is specified as a fixed 16.16 bits */
    psd_write_32(xc, (int) (pdev->HWResolution[0] * 0x10000 * xc->width / pdev->width + 0.5));
    psd_write_16(xc, 1);	/* width:  1 --> resolution is pixels per inch */
    psd_write_16(xc, 1);	/* width:  1 --> resolution is pixels per inch */
    psd_write_32(xc, (int) (pdev->HWResolution[1] * 0x10000 * xc->height / pdev->height + 0.5));
    psd_write_16(xc, 1);	/* height:  1 --> resolution is pixels per inch */
    psd_write_16(xc, 1);	/* height:  1 --> resolution is pixels per inch */

    /* Layer and Mask information */
    psd_write_32(xc, 0); 	/* No layer or mask information */

    /* Compression: 0=None, 1=RLE/PackBits, 2=Deflate 3=Defalte+Prediction */
    psd_write_16(xc, 0);

    return code;
}

/*
 * Close device and clean up ICC structures.
 */
static int
psd_prn_close(gx_device *dev)
{
    psd_device * const xdev = (psd_device *) dev;

    if (xdev->cmyk_icc_link != NULL) {
        gscms_release_link(xdev->cmyk_icc_link);
        rc_decrement(xdev->cmyk_profile, "psd_prn_close");
    }

    if (xdev->rgb_icc_link != NULL) {
        gscms_release_link(xdev->rgb_icc_link);
        rc_decrement(xdev->rgb_profile, "psd_prn_close");
    }

    if (xdev->output_icc_link != NULL) {
        gscms_release_link(xdev->output_icc_link);
        rc_decrement(xdev->output_profile, "psd_prn_close");
    }

    return gdev_prn_close(dev);
}

/*
 * Output the image data for the PSD device.  The data for the PSD is
 * written in separate planes.  If the device is psdrgb then we simply
 * write three planes of RGB data.  The DeviceN parameters (SeparationOrder,
 * SeparationCOlorNames, and MaxSeparations) are not applied to the psdrgb
 * device.
 *
 * The DeviceN parameters are applied to the psdcmyk device.  If the
 * SeparationOrder parameter is not specified then first we write out the data
 * for the CMYK planes and then any separation planes.  If the SeparationOrder
 * parameter is specified, then things are more complicated.  Logically we
 * would simply write the planes specified by the SeparationOrder data.
 * However Photoshop expects there to be CMYK data.  First we will write out
 * four planes of data for CMYK.  If any of these colors are present in the
 * SeparationOrder data then the plane data will contain the color information.
 * If a color is not present then the plane data will be zero.  After the CMYK
 * data, we will write out any separation data which is specified in the
 * SeparationOrder data.
 */

static int
psd_write_image_data(psd_write_ctx *xc, gx_device_printer *pdev)
{

    psd_device *psd_dev = (psd_device *)pdev;
    int bpc = psd_dev->devn_params.bitspercomponent;
    int raster_plane = bitmap_raster(pdev->width * bpc);
    byte *planes[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int code = 0;
    int i, j;
    byte *sep_line;
    int base_num_channels = xc->base_num_channels;
    int chan_idx;
    byte * unpacked;
    int num_comp = xc->num_channels;
    gs_get_bits_params_t params;
    gx_downscaler_t ds = { NULL };
    int octets_per_component = bpc >> 3;
    int octets_per_line = xc->width * octets_per_component;

    /* Return planar data */
    params.options = (GB_RETURN_POINTER | GB_RETURN_COPY |
         GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_RASTER_STANDARD |
         GB_PACKING_PLANAR | GB_COLORS_NATIVE | GB_ALPHA_NONE);
    params.x_offset = 0;
    params.raster = bitmap_raster(pdev->width * pdev->color_info.depth);

    sep_line = gs_alloc_bytes(pdev->memory, octets_per_line, "psd_write_sep_line");

    for (chan_idx = 0; chan_idx < num_comp; chan_idx++) {
        planes[chan_idx] = gs_alloc_bytes(pdev->memory, raster_plane,
                                        "psd_write_sep_line");
        params.data[chan_idx] = planes[chan_idx];
        if (params.data[chan_idx] == NULL)
            return_error(gs_error_VMerror);
    }

    if (sep_line == NULL)
        return_error(gs_error_VMerror);

    code = gx_downscaler_init_planar_trapped(&ds, (gx_device *)pdev, &params, num_comp,
                                             psd_dev->downscale.downscale_factor, 0, bpc, bpc,
                                             psd_dev->downscale.trap_w,
                                             psd_dev->downscale.trap_h,
                                             psd_dev->downscale.trap_order);
    if (code < 0)
        goto cleanup;

    /* Print the output planes */
    for (j = 0; j < xc->height; ++j) {
        code = gx_downscaler_get_bits_rectangle(&ds, &params, j);
        if (code < 0)
            goto cleanup;
        for (chan_idx = 0; chan_idx < num_comp; chan_idx++) {
            int data_pos = xc->chnl_to_position[chan_idx];
            if (data_pos >= 0) {

                unpacked = params.data[data_pos];

                if (base_num_channels == 3) {
                    memcpy(sep_line, unpacked, octets_per_line);
                } else if (octets_per_component == 1) {
                    for (i = 0; i < xc->width; ++i) {
                        sep_line[i] = 255 - unpacked[i];
                    }
                } else { /* octets_per_component == 2 */
                    for (i = 0; i < xc->width; ++i) {
                        ((unsigned short *)sep_line)[i] = 65535 - ((unsigned short *)unpacked)[i];
                    }
                }
                psd_write(xc, sep_line, octets_per_line);
            } else if (chan_idx < NUM_CMYK_COMPONENTS) {
                /* Write empty process color in the area */
                memset(sep_line,255,octets_per_line);
                psd_write(xc, sep_line, octets_per_line);
            }
            code = gp_fseek(xc->f, ((gs_offset_t)xc->height-1) * octets_per_line, SEEK_CUR);
            if (code < 0) {
                code = gs_note_error(gs_error_ioerror);
                goto cleanup;
            }
        }
        if (j < xc->height-1) {
            code = gp_fseek(xc->f, -((gs_offset_t)num_comp * xc->height - 1) * octets_per_line, SEEK_CUR);
            if (code < 0) {
                code = gs_note_error(gs_error_ioerror);
                goto cleanup;
            }
	}
    }

cleanup:
    gx_downscaler_fin(&ds);
    gs_free_object(pdev->memory, sep_line, "psd_write_sep_line");
    for (chan_idx = 0; chan_idx < num_comp; chan_idx++) {
        gs_free_object(pdev->memory, planes[chan_idx],
                                        "psd_write_image_data");
    }
    return code;
}

bool
psd_allow_multiple_pages (gx_device_printer *pdev)
{
    psd_device *psd_dev = (psd_device *)pdev;
    int code;
    const char *fmt;
    bool retval = true;
    gs_parsed_file_name_t parsed;

    if (strcmp(gp_null_file_name, psd_dev->fname) == 0) {
        /* If the output is discarded, it doesn't matter whether
         * the file is format valid or not
         */
        retval = true;
    }
    else {
        code = gx_parse_output_file_name(&parsed, &fmt, psd_dev->fname,
                                     strlen(psd_dev->fname), psd_dev->memory);
        if (code < 0 || (fmt == NULL && psd_dev->PageCount > 0)) {
            retval = false;
        }
    }
    return retval;
}

static int
psd_print_page(gx_device_printer *pdev, gp_file *file)
{
    psd_write_ctx xc;
    gx_devn_prn_device *devn_dev = (gx_devn_prn_device *)pdev;
    psd_device *psd_dev = (psd_device *)pdev;
    int code = 0;

    if (!psd_allow_multiple_pages(pdev)) {
       emprintf(pdev->memory, "Use of the %%d format is required to output more than one page to PSD\nSee doc/Devices.htm#PSD for details\n\n");
       code = gs_note_error(gs_error_ioerror);
    }
    else {
        code = psd_setup(&xc, devn_dev, file,
                  gx_downscaler_scale(pdev->width, psd_dev->downscale.downscale_factor),
                  gx_downscaler_scale(pdev->height, psd_dev->downscale.downscale_factor));
        if (code >= 0)
            code = psd_write_header(&xc, devn_dev);
        if (code >= 0)
            code = psd_write_image_data(&xc, pdev);
    }
    return code;
}
