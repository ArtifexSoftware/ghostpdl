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


/* gprf device, based upon the gdevpsd.c code. */

#include "math_.h"
#include "zlib.h"
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
#include "gxdevsop.h"

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

#define MAX_COLOR_VALUE 255             /* We are using 8 bits per colorant */


/* The device descriptor */
static dev_proc_open_device(gprf_prn_open);
static dev_proc_close_device(gprf_prn_close);
static dev_proc_get_params(gprf_get_params);
static dev_proc_put_params(gprf_put_params);
static dev_proc_print_page(gprf_print_page);
static dev_proc_get_color_mapping_procs(get_gprf_color_mapping_procs);
static dev_proc_get_color_comp_index(gprf_get_color_comp_index);

/*
 * A structure definition for a DeviceN type device
 */
typedef struct gprf_device_s {
    gx_devn_prn_device_common;
    gx_downscaler_params downscale;
    int max_spots;
    bool lock_colorants;
    gsicc_link_t *icclink;
    bool warning_given;  /* Used to notify the user that max colorants reached */
} gprf_device;

/* GC procedures */
static
ENUM_PTRS_WITH(gprf_device_enum_ptrs, gprf_device *pdev)
{
    ENUM_PREFIX(st_gx_devn_prn_device, 0);
    (void)pdev; /* Stop unused var warning */
    return 0;
}
ENUM_PTRS_END

static RELOC_PTRS_WITH(gprf_device_reloc_ptrs, gprf_device *pdev)
{
    RELOC_PREFIX(st_gx_devn_prn_device);
    (void)pdev; /* Stop unused var warning */
}
RELOC_PTRS_END

static int
gprf_spec_op(gx_device *dev_, int op, void *data, int datasize)
{
    if (op == gxdso_supports_devn) {
        return true;
    }
    if (op == gxdso_supports_iccpostrender) {
        return true;
    }
    return gx_default_dev_spec_op(dev_, op, data, datasize);
}

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
        NULL,		        /* map_color_rgb */\
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
        gx_devn_prn_ret_devn_params, /* ret_devn_params */\
        NULL,                        /* fillpage */\
        NULL,                        /* push_transparency_state */\
        NULL,                        /* pop_transparency_state */\
        NULL,                        /* put_image */\
        gprf_spec_op                 /* dev_spec_op */\
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
 * CMYK process color model and spot color support.
 */
static const gx_device_procs spot_cmyk_procs
        = device_procs(get_gprf_color_mapping_procs);

const gprf_device gs_gprf_device =
{
    gprf_device_body(spot_cmyk_procs, "gprf",
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
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    GS_SOFT_MAX_SPOTS,          /* max_spots */
    false,                      /* colorants not locked */
    0,                          /* ICC link */
};

/* Open the gprf device */
int
gprf_prn_open(gx_device * pdev)
{
    gprf_device *pdev_gprf = (gprf_device *) pdev;
    int code;
    int k;
    bool force_pdf, force_ps;
    cmm_dev_profile_t *profile_struct;
    gsicc_rendering_param_t rendering_params;

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
    if (code < 0)
        return code;

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
    profile_struct->supports_devn = true;
    code = gdev_prn_open_planar(pdev, true);

    /* Take care of the ICC transform now.  There are several possible ways
     * that this could go.  One is that the CMYK colors are color managed
     * but the non-standard colorants are not.  That is, we have specified
     * a CMYK profile for our device but we have other spot colorants in
     * the source file.  In this case, we will do managed proofing for the
     * CMYK colorants and use the equivalent CMYK values for the spot colorants.
     * DeviceN colorants will be problematic in this case and we will do the
     * the best we can with the tools given to us.   The other possibility is
     * that someone has given an NColor ICC profile for the device and we can
     * actually deal with the mapping directly. This is a much cleaner case
     * but as we know NColor profiles are not too common.  Also we need to
     * deal with transparent colorants (e.g. varnish) in an intelligent manner.
     * Note that we require the post rendering profile to be RGB based for
     * this particular device. */

    if (pdev_gprf->icclink == NULL && profile_struct->device_profile[0] != NULL
        && profile_struct->postren_profile != NULL &&
        profile_struct->postren_profile->data_cs == gsRGB) {
        rendering_params.black_point_comp = gsBLACKPTCOMP_ON;
        rendering_params.graphics_type_tag = GS_UNKNOWN_TAG;
        rendering_params.override_icc = false;
        rendering_params.preserve_black = gsBLACKPRESERVE_OFF;
        rendering_params.rendering_intent = gsRELATIVECOLORIMETRIC;
        rendering_params.cmm = gsCMM_DEFAULT;
        pdev_gprf->icclink = gsicc_alloc_link_dev(pdev_gprf->memory,
            profile_struct->device_profile[0], profile_struct->postren_profile,
            &rendering_params);
        if (pdev_gprf->icclink == NULL)
            code = gs_error_VMerror;
    }
    return code;
}

/* Color mapping routines */

/*
* The following procedures are used to map the standard color spaces into
* the color components. This is not where the color management occurs.  These
* may do a reordering of the colors following some ICC base color management
* or they may be used in the case of -dUseFastColor as that relies upon the
* device color mapping procedures
*/
static void
gray_cs_to_gprf_cm(gx_device * dev, frac gray, frac out[])
{
    int * map = ((gprf_device *)dev)->devn_params.separation_order_map;
    gray_cs_to_devn_cm(dev, map, gray, out);
}

static void
rgb_cs_to_gprf_cm(gx_device * dev, const gs_gstate *pgs, frac r, frac g,
                    frac b, frac out[])
{
    int * map = ((gprf_device *)dev)->devn_params.separation_order_map;
    rgb_cs_to_devn_cm(dev, map, pgs, r, g, b, out);
}

static void
cmyk_cs_to_gprf_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    const gs_devn_params *devn = &(((gprf_device *) dev)->devn_params);
    const int *map = devn->separation_order_map;
    int j;

    if (devn->num_separation_order_names > 0) {
        /* This is to set only those that we are using */
        for (j = 0; j < devn->num_separation_order_names; j++) {
            switch (map[j]) {
            case 0:
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
    }
    else {
        cmyk_cs_to_devn_cm(dev, map, c, m, y, k, out);
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

/* Get parameters.  We provide a default CRD. */
static int
gprf_get_params(gx_device * pdev, gs_param_list * plist)
{
    gprf_device *xdev = (gprf_device *)pdev;
    int code;

    code = gx_devn_prn_get_params(pdev, plist);
    if (code < 0)
        return code;

    code = gx_downscaler_write_params(plist, &xdev->downscale, 0);
    if (code < 0)
        return code;
    code = param_write_int(plist, "MaxSpots", &xdev->max_spots);
    if (code < 0)
        return code;
    code = param_write_bool(plist, "LockColorants", &xdev->lock_colorants);
    return code;
}

/* Set parameters.  We allow setting the number of bits per component. */
static int
gprf_put_params(gx_device * pdev, gs_param_list * plist)
{
    gprf_device * const pdevn = (gprf_device *) pdev;
    int code = 0;

    gx_device_color_info save_info = pdevn->color_info;

    code = gx_downscaler_read_params(plist, &pdevn->downscale, 0);
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
            code = gs_error_rangecheck;
            /* fall through */
        default:
            param_signal_error(plist, "MaxSpots", code);
            return code;
        case 1:
            break;
    }
    code = 0;

    /* handle the standard DeviceN related parameters */
    if (code == 0)
        code = gx_devn_prn_put_params(pdev, plist);

    if (code < 0) {
        pdev->color_info = save_info;
        return code;
    }
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
    gp_file *f;

    int width;
    int height;
    int n_extra_channels;
    int num_channels;	/* base_bytes_pp + any spot colors that are imaged */
    /* Map output channel number to original separation number. */
    int chnl_to_orig_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    /* Map output channel number to gx_color_index position. */
    int chnl_to_position[GX_DEVICE_COLOR_MAX_COMPONENTS];

    gsicc_link_t *icclink;

    unsigned long table_offset;
    gx_device_printer *dev;
    int deflate_bound;
    byte *deflate_block;
} gprf_write_ctx;

static int
gprf_setup(gprf_write_ctx *xc, gx_device_printer *pdev, gp_file *file, int w, int h,
           gsicc_link_t *icclink)
{
    int i;
    int spot_count;
    z_stream zstm;
    gx_devn_prn_device *dev = (gx_devn_prn_device *)pdev;

    xc->f = file;
    xc->dev = pdev;
    xc->icclink = icclink;

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
    deflateInit(&zstm, Z_BEST_SPEED);
    xc->deflate_bound = deflateBound(&zstm, 256*256);
    deflateEnd(&zstm);
    xc->deflate_block = gs_alloc_bytes(dev->memory, xc->deflate_bound, "gprf_setup");

    return (xc->deflate_block != NULL);
}

/* All multi-byte quantities are stored LSB-first! */
#if ARCH_IS_BIG_ENDIAN
#  define assign_u16(a,v) a = ((v) >> 8) + ((v) << 8)
#  define assign_u32(a,v) a = (((v) >> 24) & 0xff) + (((v) >> 8) & 0xff00) + (((v) & 0xff00) << 8) + (((v) & 0xff) << 24)
#else
#  define assign_u16(a,v) a = (v)
#  define assign_u32(a,v) a = (v)
#endif

static int
gprf_write(gprf_write_ctx *xc, const byte *buf, int size) {
    int code;

    code = gp_fwrite(buf, 1, size, xc->f);
    if (code < size)
        return_error(gs_error_ioerror);
    return 0;
}

static int
gprf_write_8(gprf_write_ctx *xc, byte v)
{
    return gprf_write(xc, (byte *)&v, 1);
}

static int
gprf_write_16(gprf_write_ctx *xc, bits16 v)
{
    bits16 buf;

    assign_u16(buf, v);
    return gprf_write(xc, (byte *)&buf, 2);
}

static int
gprf_write_32(gprf_write_ctx *xc, bits32 v)
{
    bits32 buf;

    assign_u32(buf, v);
    return gprf_write(xc, (byte *)&buf, 4);
}

static int
gprf_write_header(gprf_write_ctx *xc)
{
    int i, code;
    int index;
    unsigned int offset;
    gx_devn_prn_device *dev = (gx_devn_prn_device *)xc->dev;

    code = (unsigned int)gp_ftell(xc->f);
    if (code < 0)
        return_error(gs_error_ioerror);

    offset = (unsigned int)code;

    code = gprf_write(xc, (const byte *)"GSPF", 4); /* Signature */
    if (code < 0)
        return code;
    code = gprf_write_16(xc, 1); /* Version - Always equal to 1*/
    if (code < 0)
        return code;
    code = gprf_write_16(xc, 0); /* Compression method - 0 (deflated deltas) */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, xc->width);
    if (code < 0)
        return code;
    code = gprf_write_32(xc, xc->height);
    if (code < 0)
        return code;
    code = gprf_write_16(xc, 8); /* Bits per channel */
    if (code < 0)
        return code;
    code = gprf_write_16(xc, xc->num_channels); /* Number of separations */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* ICC offset */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* ICC offset */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* Table offset */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* Table offset */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* Reserved */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* Reserved */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* Reserved */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* Reserved */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* Reserved */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* Reserved */
    if (code < 0)
        return code;
    code = gprf_write_32(xc, 0); /* Reserved */
    if (code < 0)
        return code;

    /* Color Mode Data */
    for (i = 0; i < xc->num_channels; i++)
    {
        int j = xc->chnl_to_orig_sep[i];
        const char *name;
        int namelen;
        int c, m, y, k;
        byte cmyk[4], rgba[4];

        if (j < NUM_CMYK_COMPONENTS) {
            name = dev->devn_params.std_colorant_names[j];
            cmyk[0] = 0;
            cmyk[1] = 0;
            cmyk[2] = 0;
            cmyk[3] = 0;
            cmyk[j] = 255;
            if (name == NULL)
                namelen = 0;
            else
                namelen = strlen(name);
        } else {
            index = j - NUM_CMYK_COMPONENTS;
            name = (const char *)dev->devn_params.separations.names[index].data;
            namelen = dev->devn_params.separations.names[index].size;
            c = (2 * 255 * dev->equiv_cmyk_colors.color[index].c + frac_1) / (2 * frac_1);
            cmyk[0] = (c < 0 ? 0 : (c > 255 ? 255 : c));
            m = (2 * 255 * dev->equiv_cmyk_colors.color[index].m + frac_1) / (2 * frac_1);
            cmyk[1] = (m < 0 ? 0 : (m > 255 ? 255 : m));
            y = (2 * 255 * dev->equiv_cmyk_colors.color[index].y + frac_1) / (2 * frac_1);
            cmyk[2] = (y < 0 ? 0 : (y > 255 ? 255 : y));
            k = (2 * 255 * dev->equiv_cmyk_colors.color[index].k + frac_1) / (2 * frac_1);
            cmyk[3] = (k < 0 ? 0 : (k > 255 ? 255 : k));
        }

        /* Convert color to RGBA. To get the A value, we are going to need to
           deal with the mixing hints information in the PDF content.  A ToDo
           project. At this point, everything has an alpha of 1.0 */
        rgba[3] = 255;
        if (xc->icclink != NULL) {
            xc->icclink->procs.map_color((gx_device *)dev, xc->icclink,
                &(cmyk[0]), &(rgba[0]), 1);
        } else {
            /* Something was wrong with the icclink. Use the canned routines. */
            frac rgb_frac[3], cmyk_frac[4];
            int index;
            int temp;

            if (j >= NUM_CMYK_COMPONENTS) {
                /* Non std. colorant */
                color_cmyk_to_rgb(dev->equiv_cmyk_colors.color[j].c,
                    dev->equiv_cmyk_colors.color[j].m,
                    dev->equiv_cmyk_colors.color[j].y,
                    dev->equiv_cmyk_colors.color[j].k, NULL, rgb_frac, dev->memory);
            } else {
                /* Std. colorant */
                cmyk_frac[0] = frac_0;
                cmyk_frac[1] = frac_0;
                cmyk_frac[2] = frac_0;
                cmyk_frac[3] = frac_0;
                cmyk_frac[j] = frac_1;
                color_cmyk_to_rgb(cmyk_frac[0], cmyk_frac[1], cmyk_frac[2],
                    cmyk_frac[3], NULL, rgb_frac, dev->memory);
            }
            /* Out of frac and to byte */
            for (index = 0; index < 3; index++) {
                temp = (2 * 255 * rgb_frac[index] + frac_1) / (2 * frac_1);
                rgba[index] = (temp < 0 ? 0 : (temp > 255 ? 255 : temp));
            }
        }

        code = gprf_write_8(xc, rgba[0]);
        if (code < 0)
            return code;
        code = gprf_write_8(xc, rgba[1]);
        if (code < 0)
            return code;
        code = gprf_write_8(xc, rgba[2]);
        if (code < 0)
            return code;
        code = gprf_write_8(xc, rgba[3]);
        if (code < 0)
            return code;
        code = gprf_write_8(xc, cmyk[0]);
        if (code < 0)
            return code;
        code = gprf_write_8(xc, cmyk[1]);
        if (code < 0)
            return code;
        code = gprf_write_8(xc, cmyk[2]);
        if (code < 0)
            return code;
        code = gprf_write_8(xc, cmyk[3]);
        if (code < 0)
            return code;

        if (namelen > 0) {
            code = gprf_write(xc, (const byte *)name, namelen);
            if (code < 0)
                return code;
        }
        code = gprf_write_8(xc, 0);
        if (code < 0)
            return code;
    }

    /* FIXME: ICC Profile would go here */
    /* Since MuPDF can't really use the profile and it's optional, at this point
     * we will not spend time writing out the profile. */
    /* Update header pointer to table */
    code = (unsigned long)gp_ftell(xc->f);
    if (code < 0)
        return_error(gs_error_ioerror);
    xc->table_offset = (unsigned int) code;
    if (gp_fseek(xc->f, offset+28, SEEK_SET) != 0)
        return_error (gs_error_ioerror);
    code = gprf_write_32(xc, xc->table_offset);
    if (code < 0)
        return code;
    if (gp_fseek(xc->f, xc->table_offset, SEEK_SET) != 0)
        return_error(gs_error_ioerror);
    return 0;
}

/*
 * Close device and clean up ICC structures.
 */
static int
gprf_prn_close(gx_device *dev)
{
    gprf_device * const xdev = (gprf_device *) dev;

    if (xdev->icclink != NULL) {
        xdev->icclink->procs.free_link(xdev->icclink);
        gsicc_free_link_dev(xdev->memory, xdev->icclink);
        xdev->icclink = NULL;
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

static int
updateTable(gprf_write_ctx *xc)
{
    int offset, code;

    /* Read the current position of the file */
    offset = gp_ftell(xc->f);
    if (offset < 0)
        return_error(gs_error_ioerror);

    /* Put that value into the table */
    if (gp_fseek(xc->f, xc->table_offset, SEEK_SET) != 0)
        return_error(gs_error_ioerror);

    code = gprf_write_32(xc, offset);
    if (code < 0)
        return code;

    code = gprf_write_32(xc, 0);
    if (code < 0)
        return code;

    xc->table_offset += 8;

    /* Seek back to where we were before */
    if (gp_fseek(xc->f, offset, SEEK_SET) != 0)
        return_error(gs_error_ioerror);
    return 0;
}

static int
compressAndWrite(gprf_write_ctx *xc, byte *data, int tile_w, int tile_h, int raster)
{
    int x, y, code;
    int delta = 0;
    byte *row_d;
    int orr = 0;
    z_stream zstm;

    code = updateTable(xc);
    if (code < 0)
        return code;

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
    deflateInit(&zstm, Z_BEST_SPEED);
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

    code = gprf_write(xc, xc->deflate_block, xc->deflate_bound - zstm.avail_out);
    if (code < 0)
        return_error(gs_error_ioerror);
    return 0;
}

/* If the profile is NULL or if the profile does not support the spot
   colors then we have to calculate the equivalent CMYK values and then color
   manage. */
static void
build_cmyk_planar_raster(gprf_write_ctx *xc, byte *planes[], byte *cmyk_in,
        int raster, int ypos, cmyk_composite_map * cmyk_map)
{
    int pixel, comp_num;
    uint temp, cyan, magenta, yellow, black;
    cmyk_composite_map * cmyk_map_entry;
    int num_comp = xc->num_channels;
    byte *cmyk = cmyk_in;

    for (pixel = 0; pixel < raster; pixel++) {
        cmyk_map_entry = cmyk_map;
        /* Get the first one */
        temp = *(planes[xc->chnl_to_position[0]] + ypos * raster + pixel);
        cyan = cmyk_map_entry->c * temp;
        magenta = cmyk_map_entry->m * temp;
        yellow = cmyk_map_entry->y * temp;
        black = cmyk_map_entry->k * temp;
        cmyk_map_entry++;
        /* Add in the contributions from the rest */
        for (comp_num = 1; comp_num < num_comp; comp_num++) {
            temp = *(planes[xc->chnl_to_position[comp_num]] + ypos * raster + pixel);
            cyan += cmyk_map_entry->c * temp;
            magenta += cmyk_map_entry->m * temp;
            yellow += cmyk_map_entry->y * temp;
            black += cmyk_map_entry->k * temp;
            cmyk_map_entry++;
        }
        cyan /= frac_1;
        magenta /= frac_1;
        yellow /= frac_1;
        black /= frac_1;
        if (cyan > MAX_COLOR_VALUE)
            cyan = MAX_COLOR_VALUE;
        if (magenta > MAX_COLOR_VALUE)
            magenta = MAX_COLOR_VALUE;
        if (yellow > MAX_COLOR_VALUE)
            yellow = MAX_COLOR_VALUE;
        if (black > MAX_COLOR_VALUE)
            black = MAX_COLOR_VALUE;
        /* Now store the CMYK value in the planar buffer */
        cmyk[0] = cyan;
        cmyk[raster] = magenta;
        cmyk[2 * raster] = yellow;
        cmyk[3 * raster] = black;
        cmyk++;
    }
}

/* Create RGB from CMYK the horribly slow way */
static void
get_rgb_planar_line(gprf_write_ctx *xc, byte *c, byte *m, byte *y, byte *k,
    byte *red_in, byte *green_in, byte *blue_in, int width)
{
    int x_pos;
    int c_val, m_val, y_val, k_val;
    gprf_device *gprf_dev = (gprf_device *)xc->dev;
    frac rgb_frac[3];
    int temp;
    byte *rp = red_in;
    byte *gp = green_in;
    byte *bp = blue_in;

    for (x_pos = 0; x_pos < width; x_pos++) {

        c_val = (int)((long)(*c++)* frac_1 / 255.0);
        c_val = (c_val < 0 ? 0 : (c_val > frac_1 ? frac_1 : c_val));

        m_val = (int)((long)(*m++)* frac_1 / 255.0);
        m_val = (m_val < 0 ? 0 : (m_val > frac_1 ? frac_1 : m_val));

        y_val = (int)((long)(*y++)* frac_1 / 255.0);
        y_val = (y_val < 0 ? 0 : (y_val > frac_1 ? frac_1 : y_val));

        k_val = (int)((long)(*k++)* frac_1 / 255.0);
        k_val = (k_val < 0 ? 0 : (k_val > frac_1 ? frac_1 : k_val));

        color_cmyk_to_rgb(c_val, m_val, y_val, k_val, NULL, rgb_frac,
            gprf_dev->memory);

        temp = (2 * 255 * rgb_frac[0] + frac_1) / (2 * frac_1);
        temp = (temp < 0 ? 0 : (temp > 255 ? 255 : temp));
        *rp++ = temp;

        temp = (2 * 255 * rgb_frac[1] + frac_1) / (2 * frac_1);
        temp = (temp < 0 ? 0 : (temp > 255 ? 255 : temp));
        *gp++ = temp;

        temp = (2 * 255 * rgb_frac[2] + frac_1) / (2 * frac_1);
        temp = (temp < 0 ? 0 : (temp > 255 ? 255 : temp));
        *bp++ = temp;
    }
}

/*
 * Output the image data for the GPRF device.
 */
static int
gprf_write_image_data(gprf_write_ctx *xc)
{
    gx_device_printer *pdev = xc->dev;
    int raster_row = bitmap_raster(pdev->width * 8);
    int raster_plane;
    byte *planes[GS_CLIENT_COLOR_MAX_COMPONENTS];
    byte *rgb[3];
    byte *cmyk = NULL;
    int code = 0;
    int i, y;
    int chan_idx;
    int tiled_w, tiled_h, tile_x, tile_y;
    int num_comp = xc->num_channels;
    gs_get_bits_params_t params;
    gx_downscaler_t ds = { NULL };
    gprf_device *gprf_dev = (gprf_device *)pdev;
    bool slowcolor = false;
    bool equiv_needed = false;
    cmyk_composite_map cmyk_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gsicc_bufferdesc_t input_buffer_desc;
    gsicc_bufferdesc_t output_buffer_desc;

    /* Get set up for color management */
    /* We are going to deal with four cases for creating the RGB content.
    * Case 1: We have a CMYK ICC profile and only CMYK colors.
    * Case 2: We have a DeviceN ICC profile, which defines all our colorants
    * Case 3: We have a CMYK ICC profile and non-standard spot colorants
    * Case 4: There was an issue creating the ICC link
    * For Case 1 and Case 2, we will do a direct ICC mapping from the
    * planar components to the RGB proofing color.  This is the best work
    * flow for accurate color proofing.
    * For Case 3, we will need to compute the equivalent CMYK color similar
    * to what the tiffsep device does and then apply the ICC mapping
    * For Case 4, we need to compute the equivalent CMYK color mapping AND
    * do the slow conversion to RGB */

    if (gprf_dev->icclink == NULL) {
        /* Case 4 */
        slowcolor = true;
        equiv_needed = true;
    } else {
        if (num_comp > 4 &&
            gprf_dev->icc_struct->device_profile[0]->data_cs == gsCMYK) {
            /* Case 3 */
            equiv_needed = true;
        }
    }

    /* Return planar data */
    params.options = (GB_RETURN_POINTER | GB_RETURN_COPY |
         GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_RASTER_STANDARD |
         GB_PACKING_PLANAR | GB_COLORS_NATIVE | GB_ALPHA_NONE);
    params.x_offset = 0;
    params.raster = raster_row;

    /* For every plane, we need a buffer large enough to let us pull out
     * 256 raster lines from that plane.  Make contiguous so that we can apply
     * color management */
    raster_plane = raster_row * 256;
    planes[0] = gs_alloc_bytes(pdev->memory, raster_plane * num_comp, "gprf_write_image_data");
    if (planes[0] == NULL)
        return_error(gs_error_VMerror);

    for (chan_idx = 1; chan_idx < num_comp; chan_idx++)
    {
        planes[chan_idx] = planes[0] + raster_plane * chan_idx;
        params.data[chan_idx] = planes[chan_idx];
    }

    /* We also need space for our RGB planes. */
    rgb[0] = gs_alloc_bytes(pdev->memory, raster_plane * 3,
        "gprf_write_image_data");
    if (rgb[0] == NULL)
        return_error(gs_error_VMerror);
    rgb[1] = rgb[0] + raster_plane;
    rgb[2] = rgb[0] + raster_plane * 2;

    /* Finally, we may need a temporary CMYK composite if we have no profile
     * to handle the spot colorants. Case 3 and Case 4. Also build the mapping
     * at this point. Note that this does not need to be the whole tile, just
     * a row across as it can be reused */
    if (equiv_needed) {
        build_cmyk_map((gx_device*)gprf_dev, num_comp, &gprf_dev->equiv_cmyk_colors,
            cmyk_map);
        cmyk = gs_alloc_bytes(pdev->memory, raster_row * 4,
            "gprf_write_image_data");
        if (cmyk == NULL)
            return_error(gs_error_VMerror);
    }

    code = gx_downscaler_init_planar(&ds, (gx_device *)pdev, &params, num_comp,
                                     gprf_dev->downscale.downscale_factor, 0, 8, 8);
    if (code < 0)
        goto cleanup;

    tiled_w = (xc->width + 255) / 256;
    tiled_h = (xc->height + 255) / 256;

    /* Reserve space in the table for all the offsets */
    for (i = 8 * tiled_w * tiled_h * (3 + xc->num_channels); i >= 0; i -= 8) {
        code = gprf_write_32(xc, 0);
        if (code < 0)
            return code;
        code = gprf_write_32(xc, 0);
        if (code < 0)
            return code;
    }

    /* Print the output planes */
    /* For each row of tiles... */
    for (tile_y = 0; tile_y < tiled_h; tile_y++) {
        /* Pull out the data for the tiles in that row. */
        int tile_h = (xc->height - tile_y * 256);

        if (tile_h > 256)
            tile_h = 256;
        for (y = 0; y < tile_h; y++) {
            for (chan_idx = 0; chan_idx < num_comp; chan_idx++) {
                params.data[chan_idx] = planes[chan_idx] + y * raster_row;
            }
            code = gx_downscaler_get_bits_rectangle(&ds, &params, y + tile_y * 256);
            if (code < 0)
                goto cleanup;
            for (chan_idx = 0; chan_idx < num_comp; chan_idx++) {
                if (params.data[chan_idx] != planes[chan_idx] + y * raster_row)
                    memcpy(planes[chan_idx] + y * raster_row, params.data[chan_idx], raster_row);
            }
            /* Take care of any color management */
            if (equiv_needed) {
                build_cmyk_planar_raster(xc, planes, cmyk, raster_row, y, cmyk_map);
                /* At this point we have equiv. CMYK data */
                if (slowcolor) {
                    /* Slowest case, no profile and spots present */
                    get_rgb_planar_line(xc, cmyk, cmyk + raster_row,
                        cmyk + 2 * raster_row, cmyk + 3 * raster_row,
                        rgb[0] + y * raster_row, rgb[1] + y * raster_row,
                        rgb[2] + y * raster_row, pdev->width);
                } else {
                    /* ICC approach.  Likely a case with spots but CMYK profile */
                    /* set up for planar buffer transform */
                    gsicc_init_buffer(&input_buffer_desc, 4, 1, false, false, true,
                        raster_row, raster_row, 1, pdev->width);
                    gsicc_init_buffer(&output_buffer_desc, 3, 1, false, false, true,
                        raster_plane, raster_row, 1, pdev->width);
                    xc->icclink->procs.map_buffer((gx_device *)gprf_dev,
                        xc->icclink, &input_buffer_desc, &output_buffer_desc,
                        cmyk, rgb[0] + y * raster_row);
                }
            } else {
                if (slowcolor) {
                    /* CMYK input but profile likely missing here */
                    get_rgb_planar_line(xc, planes[xc->chnl_to_position[0]] + y * raster_row,
                        planes[xc->chnl_to_position[xc->chnl_to_position[1]]] + y * raster_row,
                        planes[xc->chnl_to_position[xc->chnl_to_position[2]]] + y * raster_row,
                        planes[xc->chnl_to_position[xc->chnl_to_position[3]]] + y * raster_row,
                        rgb[0] + y * raster_row, rgb[1] + y * raster_row,
                        rgb[2] + y * raster_row, pdev->width);
                } else {
                    /* Fastest case all ICC.  Note this could have an issue
                       if someone reorders the cmyk values. */
                    gsicc_init_buffer(&input_buffer_desc, num_comp, 1, false,
                        false, true, raster_plane, raster_row, 1, pdev->width);
                    gsicc_init_buffer(&output_buffer_desc, 3, 1, false, false, true,
                        raster_plane, raster_row, 1, pdev->width);
                    xc->icclink->procs.map_buffer((gx_device *)gprf_dev,
                        xc->icclink, &input_buffer_desc, &output_buffer_desc,
                        planes[0] + y * raster_row, rgb[0] + y * raster_row);
                }
            }
        }

        /* Now, for each tile... */
        for (tile_x = 0; tile_x < tiled_w; tile_x++) {
            int tile_w = (xc->width - tile_x * 256);

            if (tile_w > 256)
                tile_w = 256;

            /* Now we have to compress and write each separation in turn */
            code = compressAndWrite(xc, rgb[0] + tile_x * 256, tile_w, tile_h, raster_row);
            if (code >= 0)
                code = compressAndWrite(xc, rgb[1] + tile_x * 256, tile_w, tile_h, raster_row);
            if (code >= 0)
                code = compressAndWrite(xc, rgb[2] + tile_x * 256, tile_w, tile_h, raster_row);
            for (chan_idx = 0; chan_idx < num_comp; chan_idx++) {
                int j = xc->chnl_to_position[chan_idx];
                if (code >= 0)
                    code = compressAndWrite(xc, planes[j] + tile_x * 256, tile_w, tile_h, raster_row);
            }
        }
    }

    /* And put the last entry in the table */
    updateTable(xc);

cleanup:
    gx_downscaler_fin(&ds);
    gs_free_object(pdev->memory, planes[0], "gprf_write_image_data");
    gs_free_object(pdev->memory, rgb[0], "gprf_write_image_data");
    gs_free_object(pdev->memory, xc->deflate_block, "gprf_write_image_data");
    if (equiv_needed) {
        gs_free_object(pdev->memory, cmyk, "gprf_write_image_data");
    }
    return code;
}

static int
gprf_print_page(gx_device_printer *pdev, gp_file *file)
{
    int code;

    gprf_write_ctx xc;
    gprf_device *gprf_dev = (gprf_device *)pdev;

    gprf_setup(&xc, pdev, file,
              gx_downscaler_scale(pdev->width, gprf_dev->downscale.downscale_factor),
              gx_downscaler_scale(pdev->height, gprf_dev->downscale.downscale_factor),
              gprf_dev->icclink);
    code = gprf_write_header(&xc);
    if (code < 0)
        return code;
    code = gprf_write_image_data(&xc);
    if (code < 0)
        return code;
    return 0;
}
