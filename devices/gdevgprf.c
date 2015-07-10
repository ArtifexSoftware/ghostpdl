/* Copyright (C) 2001-2015 Artifex Software, Inc.
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


/* GProof device, based upon the gdevpsd.c code. */

#include "math_.h"
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
#include "gscms.h"
#include "gsicc_cache.h"
#include "gsicc_manage.h"
#include "gxgetbit.h"
#include "gdevppla.h"
#include "gxdownscale.h"
#include "gdevdevnprn.h"

#include "zlib.h"

#ifndef cmm_gcmmhlink_DEFINED
    #define cmm_gcmmhlink_DEFINED
    typedef void* gcmmhlink_t;
#endif

#ifndef cmm_gcmmhprofile_DEFINED
    #define cmm_gcmmhprofile_DEFINED
    typedef void* gcmmhprofile_t;
#endif

#ifndef MAX_CHAN
#   define MAX_CHAN 15
#endif

/* Enable logic for a local ICC output profile. */
#define ENABLE_ICC_PROFILE 0

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_open_device(gprf_prn_open);
static dev_proc_close_device(gprf_prn_close);
static dev_proc_get_params(gprf_get_params);
static dev_proc_put_params(gprf_put_params);
static dev_proc_print_page(gprf_print_page);
static dev_proc_map_color_rgb(gprf_map_color_rgb);
static dev_proc_get_color_mapping_procs(get_gprfrgb_color_mapping_procs);
static dev_proc_get_color_mapping_procs(get_gprf_color_mapping_procs);
static dev_proc_get_color_comp_index(gprf_get_color_comp_index);

/*
 * A structure definition for a DeviceN type device
 */
typedef struct gprf_device_s {
    gx_devn_prn_device_common;

    long downscale_factor;
    int max_spots;
    bool lock_colorants;

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

} gprf_device;

/* GC procedures */
static
ENUM_PTRS_WITH(gprf_device_enum_ptrs, gprf_device *pdev)
{
    ENUM_PREFIX(st_gx_devn_prn_device, 0);
    return 0;
}
ENUM_PTRS_END

static RELOC_PTRS_WITH(gprf_device_reloc_ptrs, gprf_device *pdev)
{
    RELOC_PREFIX(st_gx_devn_prn_device);
}
RELOC_PTRS_END

/* Even though gprf_device_finalize is the same as gx_devn_prn_device_finalize,
 * we need to implement it separately because st_composite_final
 * declares all 3 procedures as private. */
static void
gprf_device_finalize(const gs_memory_t *cmem, void *vpdev)
{
    gx_devn_prn_device_finalize(cmem, vpdev);
}

gs_private_st_composite_final(st_gprf_device, gprf_device,
    "gprf_device", gprf_device_enum_ptrs, gprf_device_reloc_ptrs,
    gprf_device_finalize);

/*
 * Macro definition for gprf device procedures
 */
#define device_procs(get_color_mapping_procs)\
{	gprf_prn_open,\
        gx_default_get_initial_matrix,\
        NULL,				/* sync_output */\
        /* Since the print_page doesn't alter the device, this device can print in the background */\
        gdev_prn_bg_output_page,	/* output_page */\
        gprf_prn_close,			/* close */\
        NULL,				/* map_rgb_color - not used */\
        gprf_map_color_rgb,		/* map_color_rgb */\
        NULL,				/* fill_rectangle */\
        NULL,				/* tile_rectangle */\
        NULL,				/* copy_mono */\
        NULL,				/* copy_color */\
        NULL,				/* draw_line */\
        NULL,				/* get_bits */\
        gprf_get_params,		/* get_params */\
        gprf_put_params,		/* put_params */\
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
        gprf_get_color_comp_index,	/* get_color_comp_index */\
        gx_devn_prn_encode_color,	/* encode_color */\
        gx_devn_prn_decode_color,	/* decode_color */\
        NULL,				/* pattern_manage */\
        NULL,				/* fill_rectangle_hl_color */\
        NULL,				/* include_color_space */\
        NULL,				/* fill_linear_color_scanline */\
        NULL,				/* fill_linear_color_trapezoid */\
        NULL,				/* fill_linear_color_triangle */\
        gx_devn_prn_update_spot_equivalent_colors, /* update_spot_equivalent_colors */\
        gx_devn_prn_ret_devn_params	/* ret_devn_params */\
}

#define gprf_device_body(procs, dname, ncomp, pol, depth, mg, mc, sl, cn)\
    std_device_full_body_type_extended(gprf_device, &procs, dname,\
          &st_gprf_device,\
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
        prn_device_body_rest_(gprf_print_page)

/*
 * PSD device with CMYK process color model and spot color support.
 */
static const gx_device_procs spot_cmyk_procs
        = device_procs(get_gprf_color_mapping_procs);

const gprf_device gs_gprf_device =
{
    gprf_device_body(spot_cmyk_procs, "gproof",
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
    /* gprf device specific parameters */
    1,                          /* downscale_factor */
    GS_SOFT_MAX_SPOTS,          /* max_spots */
    false,                      /* colorants not locked */
};

/* Open the gprf device */
int
gprf_prn_open(gx_device * pdev)
{
    gprf_device *pdev_gprf = (gprf_device *) pdev;
    int code;
    int k;
    bool force_pdf, limit_icc, force_ps;
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
#if LIMIT_TO_ICC
    limit_icc = true;
#else
    limit_icc = false;
#endif
    code = dev_proc(pdev, get_profile)((gx_device *)pdev, &profile_struct);
    if (profile_struct->spotnames == NULL) {
        force_pdf = false;
        force_ps = false;
    } else {
        if (limit_icc) {
            force_pdf = true;
            force_ps = false;
        } else {
            force_pdf = false;
            force_ps = true;
        }
    }
    pdev_gprf->warning_given = false;
    /* With planar the depth can be more than 64.  Update the color
       info to reflect the proper depth and number of planes.  Also note
       that the number of spot colors can change from page to page.  
       Update things so that we only output separations for the
       inks on that page. */

    if ((pdev_gprf->devn_params.page_spot_colors >= 0 || force_pdf) && !force_ps) {
        if (force_pdf) {
            /* Use the information that is in the ICC profle.  We will be here
               anytime that we have limited ourselves to a fixed number
               of colorants specified by the DeviceN ICC profile */
            pdev->color_info.num_components =
                (pdev_gprf->devn_params.separations.num_separations
                + pdev_gprf->devn_params.num_std_colorant_names);
            if (pdev->color_info.num_components > pdev->color_info.max_components)
                pdev->color_info.num_components = pdev->color_info.max_components;
            /* Limit us only to the ICC colorants */
            pdev->color_info.max_components = pdev->color_info.num_components;
        } else {
            /* Use the information that is in the page spot color.  We should
               be here if we are processing a PDF and we do not have a DeviceN
               ICC profile specified for output */
            if (!(pdev_gprf->lock_colorants)) {
                pdev->color_info.num_components =
                    (pdev_gprf->devn_params.page_spot_colors
                    + pdev_gprf->devn_params.num_std_colorant_names);
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
        if (!(pdev_gprf->lock_colorants)) {
            int num_comp = pdev_gprf->max_spots + 4; /* Spots + CMYK */
            if (num_comp > GS_CLIENT_COLOR_MAX_COMPONENTS)
                num_comp = GS_CLIENT_COLOR_MAX_COMPONENTS;
            pdev->color_info.num_components = num_comp;
            pdev->color_info.max_components = num_comp;
        }
    }
    /* Push this to the max amount as a default if someone has not set it */
    if (pdev_gprf->devn_params.num_separation_order_names == 0)
        for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
            pdev_gprf->devn_params.separation_order_map[k] = k;
        }
    pdev->color_info.depth = pdev->color_info.num_components * 
                             pdev_gprf->devn_params.bitspercomponent;
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    pdev->icc_struct->supports_devn = true;
    code = gdev_prn_open_planar(pdev, true);
    return code;
}

/* Color mapping routines */

static void
cmyk_cs_to_gprf_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    gprf_device *xdev = (gprf_device *)dev;
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
gray_cs_to_gprf_cm(gx_device * dev, frac gray, frac out[])
{
    cmyk_cs_to_gprf_cm(dev, 0, 0, 0, (frac)(frac_1 - gray), out);
}

static void
rgb_cs_to_gprf_cm(gx_device * dev, const gs_imager_state *pis,
                  frac r, frac g, frac b, frac out[])
{
    gprf_device *xdev = (gprf_device *)dev;
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

        color_rgb_to_cmyk(r, g, b, pis, cmyk, dev->memory);
        cmyk_cs_to_gprf_cm(dev, cmyk[0], cmyk[1], cmyk[2], cmyk[3],
                            out);
    }
}

static const gx_cm_color_map_procs gprf_cm_procs = {
    gray_cs_to_gprf_cm, rgb_cs_to_gprf_cm, cmyk_cs_to_gprf_cm
};

/*
 * These are the handlers for returning the list of color space
 * to color model conversion routines.
 */
static const gx_cm_color_map_procs *
get_gprf_color_mapping_procs(const gx_device * dev)
{
    return &gprf_cm_procs;
}

/*
 * Convert a gx_color_index to RGB.
 */
static int
gprf_map_color_rgb(gx_device *dev, gx_color_index color, gx_color_value rgb[3])
{
    /* TODO: return reasonable values. */
    rgb[0] = 0;
    rgb[1] = 0;
    rgb[2] = 0;
    return 0;
}

#if ENABLE_ICC_PROFILE
static int
gprf_open_profile(const char *profile_out_fn, cmm_profile_t *icc_profile, gcmmhlink_t icc_link, gs_memory_t *memory)
{

    gsicc_rendering_param_t rendering_params;

    icc_profile = gsicc_get_profile_handle_file(profile_out_fn,
                    strlen(profile_out_fn), memory);

    if (icc_profile == NULL)
        return gs_throw(-1, "Could not create profile for psd device");

    /* Set up the rendering parameters */

    rendering_params.black_point_comp = gsBPNOTSPECIFIED;
    rendering_params.graphics_type_tag = GS_UNKNOWN_TAG;  /* Already rendered */
    rendering_params.rendering_intent = gsPERCEPTUAL;

    /* Call with a NULL destination profile since we are using a device link profile here */
    icc_link = gscms_get_link(icc_profile,
                    NULL, &rendering_params);

    if (icc_link == NULL)
        return gs_throw(-1, "Could not create link handle for psd device");

    return(0);

}

static int
gprf_open_profiles(gprf_device *xdev)
{
    int code = 0;

    if (xdev->output_icc_link == NULL && xdev->profile_out_fn[0]) {

        code = gprf_open_profile(xdev->profile_out_fn, xdev->output_profile,
            xdev->output_icc_link, xdev->memory);

    }

    if (code >= 0 && xdev->rgb_icc_link == NULL && xdev->profile_rgb_fn[0]) {

        code = gprf_open_profile(xdev->profile_rgb_fn, xdev->rgb_profile,
            xdev->rgb_icc_link, xdev->memory);

    }

    if (code >= 0 && xdev->cmyk_icc_link == NULL && xdev->profile_cmyk_fn[0]) {

        code = gprf_open_profile(xdev->profile_cmyk_fn, xdev->cmyk_profile,
            xdev->cmyk_icc_link, xdev->memory);

    }

    return code;

}
#endif

/* Get parameters.  We provide a default CRD. */
static int
gprf_get_params(gx_device * pdev, gs_param_list * plist)
{
    gprf_device *xdev = (gprf_device *)pdev;
    int code;
#if ENABLE_ICC_PROFILE
    gs_param_string pos;
    gs_param_string prgbs;
    gs_param_string pcmyks;
#endif

    code = gx_devn_prn_get_params(pdev, plist);
    if (code < 0)
        return code;

#if ENABLE_ICC_PROFILE
    pos.data = (const byte *)xdev->profile_out_fn,
        pos.size = strlen(xdev->profile_out_fn),
        pos.persistent = false;
    code = param_write_string(plist, "ProfileOut", &pos);
    if (code < 0)
        return code;

    prgbs.data = (const byte *)xdev->profile_rgb_fn,
        prgbs.size = strlen(xdev->profile_rgb_fn),
        prgbs.persistent = false;
    code = param_write_string(plist, "ProfileRgb", &prgbs);
    if (code < 0)
        return code;

    pcmyks.data = (const byte *)xdev->profile_cmyk_fn,
        pcmyks.size = strlen(xdev->profile_cmyk_fn),
        pcmyks.persistent = false;
    code = param_write_string(plist, "ProfileCmyk", &prgbs);
    if (code < 0)
        return code;
#endif
    code = param_write_long(plist, "DownScaleFactor", &xdev->downscale_factor);
    if (code < 0)
        return code;
    code = param_write_int(plist, "MaxSpots", &xdev->max_spots);
    if (code < 0)
        return code;
    code = param_write_bool(plist, "LockColorants", &xdev->lock_colorants);
    return code;
}

#if ENABLE_ICC_PROFILE
static int
gprf_param_read_fn(gs_param_list *plist, const char *name,
                  gs_param_string *pstr, uint max_len)
{
    int code = param_read_string(plist, name, pstr);

    if (code == 0) {
        if (pstr->size >= max_len)
            param_signal_error(plist, name, code = gs_error_rangecheck);
    } else {
        pstr->data = 0;
    }
    return code;
}
#endif

/* Compare a C string and a gs_param_string. */
static bool
param_string_eq(const gs_param_string *pcs, const char *str)
{
    return (strlen(str) == pcs->size &&
            !strncmp(str, (const char *)pcs->data, pcs->size));
}

/* Set parameters.  We allow setting the number of bits per component. */
static int
gprf_put_params(gx_device * pdev, gs_param_list * plist)
{
    gprf_device * const pdevn = (gprf_device *) pdev;
    int code = 0;
#if ENABLE_ICC_PROFILE
    gs_param_string po;
    gs_param_string prgb;
    gs_param_string pcmyk;
#endif
    gs_param_string pcm;
    gx_device_color_info save_info = pdevn->color_info;

    switch (code = param_read_long(plist,
                                   "DownScaleFactor",
                                   &pdevn->downscale_factor)) {
        case 0:
            if (pdevn->downscale_factor <= 0)
                pdevn->downscale_factor = 1;
            break;
        case 1:
            break;
        default:
            param_signal_error(plist, "DownScaleFactor", code);
            return code;
    }

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
            code = gs_error_rangecheck;
            /* fall through */
        default:
            param_signal_error(plist, "MaxSpots", code);
            return code;
        case 1:
            break;
    }
    code = 0;

#if ENABLE_ICC_PROFILE
    code = gprf_param_read_fn(plist, "ProfileOut", &po,
                                 sizeof(pdevn->profile_out_fn));
    if (code >= 0)
        code = gprf_param_read_fn(plist, "ProfileRgb", &prgb,
                                 sizeof(pdevn->profile_rgb_fn));
    if (code >= 0)
        code = gprf_param_read_fn(plist, "ProfileCmyk", &pcmyk,
                                 sizeof(pdevn->profile_cmyk_fn));
#endif

    /* handle the standard DeviceN related parameters */
    if (code == 0)
        code = gx_devn_prn_put_params(pdev, plist);

    if (code < 0) {
        pdev->color_info = save_info;
        return code;
    }

#if ENABLE_ICC_PROFILE
    /* Open any ICC profiles that have been specified. */
    if (po.data != 0) {
        memcpy(pdevn->profile_out_fn, po.data, po.size);
        pdevn->profile_out_fn[po.size] = 0;
    }
    if (prgb.data != 0) {
        memcpy(pdevn->profile_rgb_fn, prgb.data, prgb.size);
        pdevn->profile_rgb_fn[prgb.size] = 0;
    }
    if (pcmyk.data != 0) {
        memcpy(pdevn->profile_cmyk_fn, pcmyk.data, pcmyk.size);
        pdevn->profile_cmyk_fn[pcmyk.size] = 0;
    }
    if (memcmp(&pdevn->color_info, &save_info,
                            size_of(gx_device_color_info)) != 0)
        code = gprf_open_profiles(pdevn);
#endif
    return code;
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
gprf_get_color_comp_index(gx_device * dev, const char * pname,
                                        int name_size, int component_type)
{
    int index;
    gprf_device *pdev = (gprf_device *)dev;

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

typedef struct {
    FILE *f;

    int width;
    int height;
    int n_extra_channels;
    int num_channels;	/* base_bytes_pp + any spot colors that are imaged */
    /* Map output channel number to original separation number. */
    int chnl_to_orig_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    /* Map output channel number to gx_color_index position. */
    int chnl_to_position[GX_DEVICE_COLOR_MAX_COMPONENTS];

    int table_offset;
    gx_device_printer *dev;
    int deflate_bound;
    byte *deflate_block;
} gprf_write_ctx;

int
gprf_setup(gprf_write_ctx *xc, gx_device_printer *pdev, FILE *file, int w, int h)
{
    int i;
    int spot_count;
    z_stream zstm;
    gx_devn_prn_device *dev = (gx_devn_prn_device *)pdev;

    xc->f = file;
    xc->dev = pdev;

#define NUM_CMYK_COMPONENTS 4
    for (i = 0; i < GX_DEVICE_COLOR_MAX_COMPONENTS; i++) {
        if (dev->devn_params.std_colorant_names[i] == NULL)
            break;
    }
    xc->num_channels = i;
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

    zstm.zalloc = NULL;
    zstm.zfree = NULL;
    zstm.opaque = NULL;
    deflateInit(&zstm, Z_BEST_COMPRESSION);
    xc->deflate_bound = deflateBound(&zstm, 256*256);
    deflateEnd(&zstm);
    xc->deflate_block = gs_alloc_bytes(dev->memory, xc->deflate_bound, "gprf_setup");

    return (xc->deflate_block != NULL);
}

/* All multi-byte quantities are stored LSB-first! */
#if arch_is_big_endian
#  define assign_u16(a,v) a = ((v) >> 8) + ((v) << 8)
#  define assign_u32(a,v) a = (((v) >> 24) & 0xff) + (((v) >> 8) & 0xff00) + (((v) & 0xff00) << 8) + (((v) & 0xff) << 24)
#else
#  define assign_u16(a,v) a = (v)
#  define assign_u32(a,v) a = (v)
#endif

int
gprf_write(gprf_write_ctx *xc, const byte *buf, int size) {
    int code;

    code = fwrite(buf, 1, size, xc->f);
    if (code < 0)
        return code;
    return 0;
}

int
gprf_write_8(gprf_write_ctx *xc, byte v)
{
    return gprf_write(xc, (byte *)&v, 1);
}

int
gprf_write_16(gprf_write_ctx *xc, bits16 v)
{
    bits16 buf;

    assign_u16(buf, v);
    return gprf_write(xc, (byte *)&buf, 2);
}

int
gprf_write_32(gprf_write_ctx *xc, bits32 v)
{
    bits32 buf;

    assign_u32(buf, v);
    return gprf_write(xc, (byte *)&buf, 4);
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

void
gprf_write_header(gprf_write_ctx *xc)
{
    int i;
    int offset = ftell(xc->f);
    gx_devn_prn_device *dev = (gx_devn_prn_device *)xc->dev;

    gprf_write(xc, (const byte *)"GSPF", 4); /* Signature */
    gprf_write_16(xc, 1); /* Version - Always equal to 1*/
    gprf_write_16(xc, 0); /* Compression method - 0 (deflated deltas) */
    gprf_write_32(xc, xc->width);
    gprf_write_32(xc, xc->height);
    gprf_write_16(xc, 8); /* Bits per channel */
    gprf_write_16(xc, xc->num_channels); /* Number of separations */
    gprf_write_32(xc, 0); /* ICC offset */
    gprf_write_32(xc, 0); /* ICC offset */
    gprf_write_32(xc, 0); /* Table offset */
    gprf_write_32(xc, 0); /* Table offset */
    gprf_write_32(xc, 0); /* Reserved */
    gprf_write_32(xc, 0); /* Reserved */
    gprf_write_32(xc, 0); /* Reserved */
    gprf_write_32(xc, 0); /* Reserved */
    gprf_write_32(xc, 0); /* Reserved */
    gprf_write_32(xc, 0); /* Reserved */
    gprf_write_32(xc, 0); /* Reserved */

    /* Color Mode Data */
    for (i = 0; i < xc->num_channels; i++)
    {
        int j = xc->chnl_to_orig_sep[i];
        const char *name = dev->devn_params.std_colorant_names[j];
        int namelen;
        int c, m, y, k;
        if (name != NULL)
            namelen = strlen(name);
        else
        {
            name = (const char *)dev->devn_params.separations.names[j-4].data;
            namelen = dev->devn_params.separations.names[j-4].size;
        }
        if (name == NULL)
            namelen = 0;
        gprf_write_32(xc, 0); /* RGBA */
        c = (2 * 255 * dev->equiv_cmyk_colors.color[j].c + frac_1) / (2*frac_1);
        c = (c < 0 ? 0 : (c > 255 ? 255 : c));
        m = (2 * 255 * dev->equiv_cmyk_colors.color[j].m + frac_1) / (2*frac_1);
        m = (m < 0 ? 0 : (m > 255 ? 255 : m));
        y = (2 * 255 * dev->equiv_cmyk_colors.color[j].y + frac_1) / (2*frac_1);
        y = (y < 0 ? 0 : (y > 255 ? 255 : y));
        k = (2 * 255 * dev->equiv_cmyk_colors.color[j].k + frac_1) / (2*frac_1);
        k = (k < 0 ? 0 : (k > 255 ? 255 : k));
        gprf_write_8(xc, c);
        gprf_write_8(xc, m);
        gprf_write_8(xc, y);
        gprf_write_8(xc, k);
        if (namelen > 0)
            gprf_write(xc, (const byte *)name, namelen);
        gprf_write_8(xc, 0);
    }

    /* FIXME: ICC Profile would go here */

    /* Update header pointer to table */
    xc->table_offset = ftell(xc->f);
    fseek(xc->f, offset+28, SEEK_SET);
    gprf_write_32(xc, xc->table_offset);
    fseek(xc->f, xc->table_offset, SEEK_SET);
}

/*
 * Close device and clean up ICC structures.
 */
static int
gprf_prn_close(gx_device *dev)
{
    gprf_device * const xdev = (gprf_device *) dev;

    if (xdev->cmyk_icc_link != NULL) {
        gscms_release_link(xdev->cmyk_icc_link);
        rc_decrement(xdev->cmyk_profile, "gprf_prn_close");
    }

    if (xdev->rgb_icc_link != NULL) {
        gscms_release_link(xdev->rgb_icc_link);
        rc_decrement(xdev->rgb_profile, "gprf_prn_close");
    }

    if (xdev->output_icc_link != NULL) {
        gscms_release_link(xdev->output_icc_link);
        rc_decrement(xdev->output_profile, "gprf_prn_close");
    }

    return gdev_prn_close(dev);
}

static void *my_zalloc(void *opaque, unsigned int items, unsigned int size)
{
    gs_memory_t *mem = (gs_memory_t *)opaque;
    return gs_alloc_bytes(mem, items * size, "gprf_zalloc");
}

static void my_zfree(void *opaque, void *addr)
{
    gs_memory_t *mem = (gs_memory_t *)opaque;
    gs_free_object(mem, addr, "gprf_zalloc");
}

static void
updateTable(gprf_write_ctx *xc)
{
    int offset;

    /* Read the current position of the file */
    offset = ftell(xc->f);

    /* Put that value into the table */
    fseek(xc->f, xc->table_offset, SEEK_SET);
    gprf_write_32(xc, offset);
    gprf_write_32(xc, 0);
    xc->table_offset += 8;

    /* Seek back to where we were before */
    fseek(xc->f, offset, SEEK_SET);
}

static int
compressAndWrite(gprf_write_ctx *xc, byte *data, int tile_w, int tile_h, int raster)
{
    int x, y;
    int delta = 0;
    byte *row_d;
    int orr = 0;
    z_stream zstm;

    updateTable(xc);

    /* Delta the data (and check for non-zero) */
    row_d = data;
    for (y = 0; y < tile_h; y++)
    {
        byte *d = row_d;
        for (x = 0; x < tile_w; x++)
        {
            int del = *d;
            orr |= *d;
            *d++ -= delta;
            delta = del;
        }
        row_d += raster;
    }

    /* If the separation is blank, no need to write anything more */
    if (orr == 0)
        return 0;

    /* Now we need to compress the data and write it. */
    zstm.zalloc = my_zalloc;
    zstm.zfree = my_zfree;
    zstm.opaque = xc->dev->memory;
    deflateInit(&zstm, Z_BEST_COMPRESSION);
    zstm.avail_out = xc->deflate_bound;
    zstm.next_out = xc->deflate_block;

    row_d = data;
    for (y = 0; y < tile_h; y++)
    {
        zstm.avail_in = tile_w;
        zstm.next_in = row_d;
        deflate(&zstm, Z_NO_FLUSH);
        row_d += raster;
    }
    deflate(&zstm, Z_FINISH);
    deflateEnd(&zstm);
   
    gprf_write(xc, xc->deflate_block, xc->deflate_bound - zstm.avail_out);
    return 0;
}

/*
 * Output the image data for the GPRF device.
 */

static int
gprf_write_image_data(gprf_write_ctx *xc)
{
    gx_device_printer *pdev = xc->dev;
    int raster_plane = bitmap_raster(pdev->width * 8);
    byte *planes[GS_CLIENT_COLOR_MAX_COMPONENTS];
    byte *rgb[3];
    int code = 0;
    int i, y;
    int chan_idx;
    int tiled_w, tiled_h, tile_x, tile_y;
/*  gprf_device *xdev = (gprf_device *)pdev;
    gcmmhlink_t link = xdev->output_icc_link; */
    int num_comp = xc->num_channels;
    gs_get_bits_params_t params;
    gx_downscaler_t ds = { NULL };
    gprf_device *gprf_dev = (gprf_device *)pdev;

    /* Return planar data */
    params.options = (GB_RETURN_POINTER | GB_RETURN_COPY |
         GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_RASTER_STANDARD |
         GB_PACKING_PLANAR | GB_COLORS_NATIVE | GB_ALPHA_NONE);
    params.x_offset = 0;
    params.raster = raster_plane;

    /* For every plane, we need a buffer large enough to let us pull out
     * 256 raster lines from that plane */
    for (chan_idx = 0; chan_idx < num_comp; chan_idx++)
    {
        planes[chan_idx] = gs_alloc_bytes(pdev->memory,
                                          raster_plane * 256, 
                                          "gprf_write_image_data");
        params.data[chan_idx] = planes[chan_idx];
        if (params.data[chan_idx] == NULL)
            return_error(gs_error_VMerror); /* LEAK! */
    }

    /* We also need space for our RGB planes. */
    for (chan_idx = 0; chan_idx < 3; chan_idx++)
    {
        rgb[chan_idx] = gs_alloc_bytes(pdev->memory,
                                       raster_plane * 256, 
                                       "gprf_write_image_data");
        if (rgb[chan_idx] == NULL)
            return_error(gs_error_VMerror); /* LEAK! */
    }

    code = gx_downscaler_init_planar(&ds, (gx_device *)pdev, &params, num_comp,
                                     gprf_dev->downscale_factor, 0, 8, 8);
    if (code < 0)
        goto cleanup;

    tiled_w = (xc->width + 255)/256;
    tiled_h = (xc->height + 255)/256;

    /* Reserve space in the table for all the offsets */
    for (i = 8 * tiled_w * tiled_h * (3 + xc->num_channels); i >= 0; i -= 8)
    {
        gprf_write_32(xc, 0);
        gprf_write_32(xc, 0);
    }

    /* Print the output planes */
    /* For each row of tiles... */
    for (tile_y = 0; tile_y < tiled_h; tile_y++)
    {
        /* Pull out the data for the tiles in that row. */
        int tile_h = (xc->height - tile_y*256);

        if (tile_h > 256)
            tile_h = 256;
        for (y = 0; y < tile_h; y++)
        {
            for (chan_idx = 0; chan_idx < num_comp; chan_idx++)
            {
                params.data[chan_idx] = planes[chan_idx] + y * raster_plane;
            }
            code = gx_downscaler_get_bits_rectangle(&ds, &params, y + tile_y * 256);
            if (code < 0)
                goto cleanup;
            for (chan_idx = 0; chan_idx < num_comp; chan_idx++)
            {
                if (params.data[chan_idx] != planes[chan_idx] + y * raster_plane)
                    memcpy(planes[chan_idx] + y * raster_plane, params.data[chan_idx], raster_plane);
            }
        }

        /* Make the RGB version from the separations */
        /* FIXME: Crap version for now. */
        for (y = 0; y < tile_h; y++)
        {
            byte *cp = planes[xc->chnl_to_position[0]] + y * raster_plane;
            byte *mp = planes[xc->chnl_to_position[1]] + y * raster_plane;
            byte *yp = planes[xc->chnl_to_position[2]] + y * raster_plane;
            byte *kp = planes[xc->chnl_to_position[3]] + y * raster_plane;
            byte *rp = rgb[0] + y * raster_plane;
            byte *gp = rgb[1] + y * raster_plane;
            byte *bp = rgb[2] + y * raster_plane;
            int x;
            for (x = 0; x < xc->width; x++)
            {
                int C = *cp++;
                int M = *mp++;
                int Y = *yp++;
                int K = *kp++;
                K = 255 - K;
                C = K - C; if (C < 0) C = 0;
                M = K - M; if (M < 0) M = 0;
                Y = K - Y; if (Y < 0) Y = 0;
                *rp++ = C;
                *gp++ = M;
                *bp++ = Y;
            }
        }

        /* Now, for each tile... */
        for (tile_x = 0; tile_x < tiled_w; tile_x++)
        {
            int tile_w = (xc->width - tile_x*256);

            if (tile_w > 256)
                tile_w = 256;

            /* Now we have to compress and write each separation in turn */
            code = compressAndWrite(xc, rgb[0] + tile_x*256, tile_w, tile_h, raster_plane);
            if (code >= 0)
                code = compressAndWrite(xc, rgb[1] + tile_x*256, tile_w, tile_h, raster_plane);
            if (code >= 0)
                code = compressAndWrite(xc, rgb[2] + tile_x*256, tile_w, tile_h, raster_plane);
            for (chan_idx = 0; chan_idx < num_comp; chan_idx++)
            {
                int j = xc->chnl_to_position[chan_idx];
                if (code >= 0)
                    code = compressAndWrite(xc, planes[j] + tile_x*256, tile_w, tile_h, raster_plane);
            }
        }
    }

    /* And put the last entry in the table */
    updateTable(xc);

cleanup:
    gx_downscaler_fin(&ds);
    for (chan_idx = 0; chan_idx < num_comp; chan_idx++) {
        gs_free_object(pdev->memory, planes[chan_idx], 
                       "gprf_write_image_data");
    }
    gs_free_object(pdev->memory, rgb[0],
                   "gprf_write_image_data");
    gs_free_object(pdev->memory, rgb[1],
                   "gprf_write_image_data");
    gs_free_object(pdev->memory, rgb[2],
                   "gprf_write_image_data");
    return code;
}

static int
gprf_print_page(gx_device_printer *pdev, FILE *file)
{
    gprf_write_ctx xc;
    gprf_device *gprf_dev = (gprf_device *)pdev;

    gprf_setup(&xc, pdev, file,
              gx_downscaler_scale(pdev->width, gprf_dev->downscale_factor),
              gx_downscaler_scale(pdev->height, gprf_dev->downscale_factor));
    gprf_write_header(&xc);
    gprf_write_image_data(&xc);
    return 0;
}
