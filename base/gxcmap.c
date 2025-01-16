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


/* Color mapping for Ghostscript */
#include "assert_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsccolor.h"
#include "gxalpha.h"
#include "gxcspace.h"
#include "gxfarith.h"
#include "gxfrac.h"
#include "gxdcconv.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxlum.h"
#include "gzstate.h"
#include "gzht.h"
#include "gxdither.h"
#include "gxcdevn.h"
#include "string_.h"
#include "gsicc_manage.h"
#include "gdevdevn.h"
#include "gsicc_cache.h"
#include "gscms.h"
#include "gsicc.h"
#include "gxdevsop.h"

/* If enabled, this makes use of the alternate transform
   ICC profile for mapping separation and
   DeviceN colorants from CMYK to output
   ICC color space iff the profiles make
   sense.  We should probably make this yet
   another color command line option. Disabling
   it for now for the current release. */
#define USE_ALT_MAP 0

/* Structure descriptor */
public_st_device_color();
static
ENUM_PTRS_WITH(device_color_enum_ptrs, gx_device_color *cptr)
{
        return ENUM_USING(*cptr->type->stype, vptr, size, index);
}
ENUM_PTRS_END
static RELOC_PTRS_WITH(device_color_reloc_ptrs, gx_device_color *cptr)
{
    RELOC_USING(*cptr->type->stype, vptr, size);
}
RELOC_PTRS_END

gx_color_index
gx_default_encode_color(gx_device * dev, const gx_color_value cv[])
{
    uchar             ncomps = dev->color_info.num_components;
    uchar             i;
    const byte *    comp_shift = dev->color_info.comp_shift;
    const byte *    comp_bits = dev->color_info.comp_bits;
    gx_color_index  color = 0;

#ifdef DEBUG
    if (!colors_are_separable_and_linear(&dev->color_info)) {
        dmprintf(dev->memory, "gx_default_encode_color() requires separable and linear\n" );
        return gx_no_color_index;
    }
#endif
    for (i = 0; i < ncomps; i++) {
        COLROUND_VARS;
        COLROUND_SETUP(comp_bits[i]);
        color |= COLROUND_ROUND(cv[i]) << comp_shift[i];

    }
    return color;
}

/*
 * This routine is only used if the device is 'separable'.  See
 * separable_and_linear in gxdevcli.h for more information.
 */
int
gx_default_decode_color(gx_device * dev, gx_color_index color, gx_color_value cv[])
{
    uchar                   ncomps = dev->color_info.num_components;
    uchar                   i;
    const byte *            comp_shift = dev->color_info.comp_shift;
    const byte *            comp_bits = dev->color_info.comp_bits;
    const gx_color_index *  comp_mask = dev->color_info.comp_mask;
    uint shift, ivalue, nbits, scale;

#ifdef DEBUG
    if (!colors_are_separable_and_linear(&dev->color_info)) {
        dmprintf(dev->memory, "gx_default_decode_color() requires separable and linear\n" );
        return_error(gs_error_rangecheck);
    }
#endif

    for (i = 0; i < ncomps; i++) {
        /*
         * Convert from the gx_color_index bits to a gx_color_value.
         * Split the conversion into an integer and a fraction calculation
         * so we can do integer arthmetic.  The calculation is equivalent
         * to floor(0xffff.fffff * ivalue / ((1 << nbits) - 1))
         */
        nbits = comp_bits[i];
        scale = gx_max_color_value / ((1 << nbits) - 1);
        ivalue = (color & comp_mask[i]) >> comp_shift[i];
        cv[i] = ivalue * scale;
        /*
         * Since our scaling factor is an integer, we lost the fraction.
         * Determine what part of the ivalue that the faction would have
         * added into the result.
         */
        shift = nbits - (gx_color_value_bits % nbits);
        cv[i] += ivalue >> shift;
    }
    return 0;
}

gx_color_index
gx_error_encode_color(gx_device * dev, const gx_color_value colors[])
{
#ifdef DEBUG
    /* The "null" device is expected to be missing encode_color */
    if (strcmp(dev->dname, "null") != 0)
        dmprintf(dev->memory, "No encode_color proc defined for device.\n");
#endif
    return gx_no_color_index;
}

int
gx_error_decode_color(gx_device * dev, gx_color_index cindex, gx_color_value colors[])
{
     int i=dev->color_info.num_components;

#ifdef DEBUG
     dmprintf(dev->memory, "No decode_color proc defined for device.\n");
#endif
     for(; i>=0; i--)
        colors[i] = 0;
     return_error(gs_error_rangecheck);
}

/*
 * The "back-stop" default encode_color method. This will be used only
 * if no applicable color encoding procedure is provided, and the number
 * of color model components is 1. The encoding is presumed to induce an
 * additive color model (DeviceGray).
 *
 * The particular method employed is a trivial generalization of the
 * default map_rgb_color method used in the pre-DeviceN code (this was
 * known as gx_default_w_b_map_rgb_color). Since the DeviceRGB color
 * model is assumed additive, any of the procedures used as a default
 * map_rgb_color method are assumed to induce an additive color model.
 * gx_default_w_b_map_rgb_color mapped white to 1 and black to 0, so
 * the new procedure is set up with zero-base and positive slope as well.
 * The generalization is the use of depth; the earlier procedure assumed
 * a bi-level device.
 *
 * Two versions of this procedure are provided, the first of which
 * applies if max_gray == 2^depth - 1 and is faster, while the second
 * applies to the general situation. Note that, as with the encoding
 * procedures used in the pre-DeviceN code, both of these methods induce
 * a small rounding error if 1 < depth < gx_color_value_bits.
 */
gx_color_index
gx_default_gray_fast_encode(gx_device * dev, const gx_color_value cv[])
{
    COLROUND_VARS;
    COLROUND_SETUP(dev->color_info.depth);
    return COLROUND_ROUND(cv[0]);
}

gx_color_index
gx_default_gray_encode(gx_device * dev, const gx_color_value cv[])
{
    return (gx_color_index)(cv[0]) * (dev->color_info.max_gray + 1) / (gx_max_color_value + 1);
}

/**
 * This routine is provided for old devices which provide a
 * map_rgb_color routine but not encode_color. New devices are
 * encouraged either to use the defaults or to set encode_color rather
 * than map_rgb_color.
 **/
gx_color_index
gx_backwards_compatible_gray_encode(gx_device *dev,
                                    const gx_color_value cv[])
{
    gx_color_value gray_val = cv[0];
    gx_color_value rgb_cv[3];

    rgb_cv[0] = gray_val;
    rgb_cv[1] = gray_val;
    rgb_cv[2] = gray_val;
    return (*dev_proc(dev, map_rgb_color))(dev, rgb_cv);
}

/* -------- Default color space to color model conversion routines -------- */

void
gray_cs_to_gray_cm(const gx_device * dev, frac gray, frac out[])
{
    out[0] = gray;
}

static void
rgb_cs_to_gray_cm(const gx_device * dev, const gs_gstate *pgs,
                                   frac r, frac g, frac b, frac out[])
{
    out[0] = color_rgb_to_gray(r, g, b, NULL);
}

static void
cmyk_cs_to_gray_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    out[0] = color_cmyk_to_gray(c, m, y, k, NULL);
}

static void
gray_cs_to_rgb_cm(const gx_device * dev, frac gray, frac out[])
{
    out[0] = out[1] = out[2] = gray;
}

void
rgb_cs_to_rgb_cm(const gx_device * dev, const gs_gstate *pgs,
                                  frac r, frac g, frac b, frac out[])
{
    out[0] = r;
    out[1] = g;
    out[2] = b;
}

static void
cmyk_cs_to_rgb_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    color_cmyk_to_rgb(c, m, y, k, NULL, out, dev->memory);
}

static void
gray_cs_to_rgbk_cm(const gx_device * dev, frac gray, frac out[])
{
    out[0] = out[1] = out[2] = frac_0;
    out[3] = gray;
}

static void
rgb_cs_to_rgbk_cm(const gx_device * dev, const gs_gstate *pgs,
                                  frac r, frac g, frac b, frac out[])
{
    if ((r == g) && (g == b)) {
        out[0] = out[1] = out[2] = frac_0;
        out[3] = r;
    }
    else {
        out[0] = r;
        out[1] = g;
        out[2] = b;
        out[3] = frac_0;
    }
}

static void
cmyk_cs_to_rgbk_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    frac rgb[3];
    if ((c == frac_0) && (m == frac_0) && (y == frac_0)) {
        out[0] = out[1] = out[2] = frac_0;
        out[3] = frac_1 - k;
    }
    else {
        color_cmyk_to_rgb(c, m, y, k, NULL, rgb, dev->memory);
        rgb_cs_to_rgbk_cm(dev, NULL, rgb[0], rgb[1], rgb[2], out);
    }
}

static void
gray_cs_to_cmyk_cm(const gx_device * dev, frac gray, frac out[])
{
    out[0] = out[1] = out[2] = frac_0;
    out[3] = frac_1 - gray;
}

/*
 * Default map from DeviceRGB color space to DeviceCMYK color
 * model. Since this mapping is defined by the PostScript language
 * it is unlikely that any device with a DeviceCMYK color model
 * would define this mapping on its own.
 *
 * If the gs_gstate is not available, map as though the black
 * generation and undercolor removal functions are identity
 * transformations. This mode is used primarily to support the
 * raster operation (rop) feature of PCL, which requires that
 * the raster operation be performed in an RGB color space.
 * Note that default black generation and undercolor removal
 * functions in PostScript need NOT be identity transformations:
 * often they are { pop 0 }.
 */
static void
rgb_cs_to_cmyk_cm(const gx_device * dev, const gs_gstate *pgs,
                           frac r, frac g, frac b, frac out[])
{
    if (pgs != 0)
        color_rgb_to_cmyk(r, g, b, pgs, out, dev->memory);
    else {
        frac    c = frac_1 - r, m = frac_1 - g, y = frac_1 - b;
        frac    k = min(c, min(m, y));

        out[0] = c - k;
        out[1] = m - k;
        out[2] = y - k;
        out[3] = k;
    }
}

void
cmyk_cs_to_cmyk_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    out[0] = c;
    out[1] = m;
    out[2] = y;
    out[3] = k;
}

/* The list of default color space to color model conversion routines. */

static const gx_cm_color_map_procs DeviceGray_procs = {
    gray_cs_to_gray_cm, rgb_cs_to_gray_cm, cmyk_cs_to_gray_cm
};

static const gx_cm_color_map_procs DeviceRGB_procs = {
    gray_cs_to_rgb_cm, rgb_cs_to_rgb_cm, cmyk_cs_to_rgb_cm
};

static const gx_cm_color_map_procs DeviceCMYK_procs = {
    gray_cs_to_cmyk_cm, rgb_cs_to_cmyk_cm, cmyk_cs_to_cmyk_cm
};

static const gx_cm_color_map_procs DeviceRGBK_procs = {
    gray_cs_to_rgbk_cm, rgb_cs_to_rgbk_cm, cmyk_cs_to_rgbk_cm
};

/*
 * These are the default handlers for returning the list of color space
 * to color model conversion routines.
 */
const gx_cm_color_map_procs *
gx_default_DevGray_get_color_mapping_procs(const gx_device * dev,
                                           const gx_device ** tdev)
{
    *tdev = dev;
    return &DeviceGray_procs;
}

const gx_cm_color_map_procs *
gx_default_DevRGB_get_color_mapping_procs(const gx_device * dev,
                                          const gx_device ** tdev)
{
    *tdev = dev;
    return &DeviceRGB_procs;
}

const gx_cm_color_map_procs *
gx_default_DevCMYK_get_color_mapping_procs(const gx_device * dev,
                                           const gx_device ** tdev)
{
    *tdev = dev;
    return &DeviceCMYK_procs;
}

const gx_cm_color_map_procs *
gx_default_DevRGBK_get_color_mapping_procs(const gx_device * dev,
                                           const gx_device ** tdev)
{
    *tdev = dev;
    return &DeviceRGBK_procs;
}

const gx_cm_color_map_procs *
gx_error_get_color_mapping_procs(const gx_device * dev,
                                 const gx_device ** tdev)
{
    /*
     * We should never get here.  If we do then we do not have a "get_color_mapping_procs"
     * routine for the device. This will be noisy, but better than returning NULL which
     * would lead to SEGV (Segmentation Fault) errors when this is used.
     */
    emprintf1(dev->memory,
              "No get_color_mapping_procs proc defined for device '%s'\n",
              dev->dname);
    switch (dev->color_info.num_components) {
      case 1:     /* DeviceGray or DeviceInvertGray */
        return gx_default_DevGray_get_color_mapping_procs(dev, tdev);

      case 3:
        return gx_default_DevRGB_get_color_mapping_procs(dev, tdev);

      case 4:
      default:		/* Unknown color model - punt with CMYK */
        return gx_default_DevCMYK_get_color_mapping_procs(dev, tdev);
    }
}

/* ----- Default color component name to colorant index conversion routines ------ */

#define compare_color_names(pname, name_size, name_str) \
    (name_size == (int)strlen(name_str) && strncmp(pname, name_str, name_size) == 0)

/* Default color component to index for a DeviceGray color model */
int
gx_default_DevGray_get_color_comp_index(gx_device * dev, const char * pname,
                                          int name_size, int component_type)
{
    if (compare_color_names(pname, name_size, "Gray") ||
        compare_color_names(pname, name_size, "Grey"))
        return 0;
    else
        return -1;		    /* Indicate that the component name is "unknown" */
}

/* Default color component to index for a DeviceRGB color model */
int
gx_default_DevRGB_get_color_comp_index(gx_device * dev, const char * pname,
                                           int name_size, int component_type)
{
    if (compare_color_names(pname, name_size, "Red"))
        return 0;
    if (compare_color_names(pname, name_size, "Green"))
        return 1;
    if (compare_color_names(pname, name_size, "Blue"))
        return 2;
    else
        return -1;		    /* Indicate that the component name is "unknown" */
}

/* Default color component to index for a DeviceCMYK color model */
int
gx_default_DevCMYK_get_color_comp_index(gx_device * dev, const char * pname,
                                            int name_size, int component_type)
{
    if (compare_color_names(pname, name_size, "Cyan"))
        return 0;
    if (compare_color_names(pname, name_size, "Magenta"))
        return 1;
    if (compare_color_names(pname, name_size, "Yellow"))
        return 2;
    if (compare_color_names(pname, name_size, "Black"))
        return 3;
    else
        return -1;		    /* Indicate that the component name is "unknown" */
}

/* Default color component to index for a DeviceRGBK color model */
int
gx_default_DevRGBK_get_color_comp_index(gx_device * dev, const char * pname,
                                            int name_size, int component_type)
{
    if (compare_color_names(pname, name_size, "Red"))
        return 0;
    if (compare_color_names(pname, name_size, "Green"))
        return 1;
    if (compare_color_names(pname, name_size, "Blue"))
        return 2;
    if (compare_color_names(pname, name_size, "Black"))
        return 3;
    else
        return -1;		    /* Indicate that the component name is "unknown" */
}

/* Default color component to index for an unknown color model */
int
gx_error_get_color_comp_index(gx_device * dev, const char * pname,
                                        int name_size, int component_type)
{
    /*
     * We should never get here.  If we do then we do not have a "get_color_comp_index"
     * routine for the device.
     */
#ifdef DEBUG
    dmprintf(dev->memory, "No get_color_comp_index proc defined for device.\n");
#endif
    return -1;			    /* Always return "unknown" component name */
}

#undef compare_color_names

/* ---------------- Device color rendering ---------------- */

static cmap_proc_gray(cmap_gray_halftoned);
static cmap_proc_gray(cmap_gray_direct);

static cmap_proc_rgb(cmap_rgb_halftoned);
static cmap_proc_rgb(cmap_rgb_direct);

#define cmap_cmyk_halftoned cmap_cmyk_direct
static cmap_proc_cmyk(cmap_cmyk_direct);

/* Procedure names are only guaranteed unique to 23 characters.... */
static cmap_proc_separation(cmap_separation_halftoned);
static cmap_proc_separation(cmap_separation_direct);

static cmap_proc_devicen(cmap_devicen_halftoned);
static cmap_proc_devicen(cmap_devicen_direct);

static cmap_proc_is_halftoned(cmap_halftoned_is_halftoned);
static cmap_proc_is_halftoned(cmap_direct_is_halftoned);

static const gx_color_map_procs cmap_few = {
     cmap_gray_halftoned,
     cmap_rgb_halftoned,
     cmap_cmyk_halftoned,
     cmap_separation_halftoned,
     cmap_devicen_halftoned,
     cmap_halftoned_is_halftoned
    };
static const gx_color_map_procs cmap_many = {
     cmap_gray_direct,
     cmap_rgb_direct,
     cmap_cmyk_direct,
     cmap_separation_direct,
     cmap_devicen_direct,
     cmap_direct_is_halftoned
    };

const gx_color_map_procs *const cmap_procs_default = &cmap_many;

/* Determine the color mapping procedures for a device. */
/* Note that the default procedure doesn't consult the gs_gstate. */
const gx_color_map_procs *
gx_get_cmap_procs(const gs_gstate *pgs, const gx_device * dev)
{
    return (pgs->get_cmap_procs)(pgs, dev);
}

const gx_color_map_procs *
gx_default_get_cmap_procs(const gs_gstate *pgs, const gx_device * dev)
{
    return (gx_device_must_halftone(dev) ? &cmap_few : &cmap_many);
}

/* Set the color mapping procedures in the graphics state. */
void
gx_set_cmap_procs(gs_gstate * pgs, const gx_device * dev)
{
    pgs->cmap_procs = gx_get_cmap_procs(pgs, dev);
}

/* Remap the color in the graphics state. */
int
gx_remap_color(gs_gstate * pgs)
{
    const gs_color_space *pcs = gs_currentcolorspace_inline(pgs);
    int                   code = 0;

    /* The current color in the graphics state is always used for */
    /* the texture, never for the source. */
    /* skip remap if the dev_color is already set and is type "pure" (a common case) */
    if (!gx_dc_is_pure(gs_currentdevicecolor_inline(pgs)))
        code = (*pcs->type->remap_color) (gs_currentcolor_inline(pgs),
                                          pcs, gs_currentdevicecolor_inline(pgs),
                                          (gs_gstate *) pgs, pgs->device,
                                          gs_color_select_texture);
    return code;
}

/* Indicate that a color space has no underlying concrete space. */
const gs_color_space *
gx_no_concrete_space(const gs_color_space * pcs, const gs_gstate * pgs)
{
    return NULL;
}

/* Indicate that a color space is concrete. */
const gs_color_space *
gx_same_concrete_space(const gs_color_space * pcs, const gs_gstate * pgs)
{
    return pcs;
}

/* Indicate that a color cannot be concretized. */
int
gx_no_concretize_color(const gs_client_color * pcc, const gs_color_space * pcs,
                       frac * pconc, const gs_gstate * pgs, gx_device *dev)
{
    return_error(gs_error_rangecheck);
}

/* If someone has specified a table for handling named spot colors then we will
   be attempting to do the special handling to go directly to the device colors
   here */
bool
gx_remap_named_color(const gs_client_color * pcc, const gs_color_space * pcs,
gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
gs_color_select_t select)
{
    gx_color_value device_values[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte *pname;
    uint name_size;
    gsicc_rendering_param_t rendering_params;
    int code;
    gsicc_namedcolor_t named_color_sep;
    gsicc_namedcolor_t *named_color_devn = NULL;
    gsicc_namedcolor_t *named_color_ptr = NULL;
    uchar num_des_comps = dev->color_info.num_components;
    uchar k;
    frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int i = pcs->type->num_components(pcs);
    cmm_dev_profile_t *dev_profile = NULL;
    gs_color_space_index type = gs_color_space_get_index(pcs);
    uchar num_src_comps = 1;

    /* Define the rendering intents. */
    rendering_params.black_point_comp = pgs->blackptcomp;
    rendering_params.graphics_type_tag = dev->graphics_type_tag;
    rendering_params.override_icc = false;
    rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
    rendering_params.rendering_intent = pgs->renderingintent;
    rendering_params.cmm = gsCMM_DEFAULT;

    if (type == gs_color_space_index_Separation) {
        named_color_sep.colorant_name = pcs->params.separation.sep_name;
        named_color_sep.name_size = strlen(pcs->params.separation.sep_name);
        named_color_ptr = &named_color_sep;
    } else if (type == gs_color_space_index_DeviceN) {
        char **names = pcs->params.device_n.names;
        num_src_comps = pcs->params.device_n.num_components;
        /* Allocate and initialize name structure */
        named_color_devn =
            (gsicc_namedcolor_t*)gs_alloc_bytes(dev->memory->non_gc_memory,
            num_src_comps * sizeof(gsicc_namedcolor_t),
            "gx_remap_named_color");
        if (named_color_devn == NULL)
            return false; /* Clearly a bigger issue. But lets not end here */
        for (k = 0; k < num_src_comps; k++) {
            pname = (byte *)names[k];
            name_size = strlen(names[k]);
            named_color_devn[k].colorant_name = (char*)pname;
            named_color_devn[k].name_size = name_size;
        }
        named_color_ptr = named_color_devn;
    } else
        return false; /* Only sep and deviceN for named color replacement */

    code = gsicc_transform_named_color(pcc->paint.values, named_color_ptr,
        num_src_comps, device_values, pgs, dev, NULL, &rendering_params);
    if (named_color_devn != NULL)
        gs_free_object(dev->memory->non_gc_memory, named_color_devn,
            "gx_remap_named_color");

    if (code == 0) {
        /* Named color was found and set.  Note that  gsicc_transform_named_color
           MUST set ALL the colorant values AND they must be in the proper
           order already.  If we have specified the colorants with
           -sICCOutputColors (i.e. if you are using an NCLR output profile) then
           we should be good. If not or if instead one used SeparationColorNames and
           SeparationOrder to set up the device, then we need to make a copy
           of the gs_gstate and make sure that we set color_component_map is
           properly set up for the gx_remap_concrete_devicen proc. */
        for (k = 0; k < num_des_comps; k++)
            conc[k] = float2frac(((float)device_values[k]) / 65535.0);

        /* If we are looking to create the equivalent CMYK value then no need
           to worry about NCLR profiles or about altering the colorant map */
        if (!named_color_equivalent_cmyk_colors(pgs)) {
            /* We need to apply transfer functions, possibily halftone and
               encode the color for the device. To get proper mapping of the
               colors to the device positions, you MUST specify -sICCOutputColors
               which will enumerate the positions of the colorants and enable
               proper color management for the CMYK portions IF you are using
               an NCLR output profile. */
            code = dev_proc(dev, get_profile)(dev, &dev_profile);
            if (code < 0)
                return false;

            /* Check if the profile is DeviceN (NCLR) */
            if (dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE]->data_cs == gsNCHANNEL) {
                if (dev_profile->spotnames == NULL)
                    return false;
                if (!dev_profile->spotnames->equiv_cmyk_set) {
                    /* Note that if the improper NCLR profile is used, then the
                       composite preview will be wrong. */
                    code = gsicc_set_devicen_equiv_colors(dev, pgs,
                                      dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE]);
                    if (code < 0)
                        return false;
                    dev_profile->spotnames->equiv_cmyk_set = true;
                }
                gx_remap_concrete_devicen(conc, pdc, pgs, dev, select, pcs);
            } else {
                gs_gstate temp_state = *((const gs_gstate *)pgs);

                /* No NCLR profile with spot names.  So set up the
                   color_component_map in the gs_gstate.  Again, note that
                   gsicc_transform_named_color must have set ALL the device
                   colors */
                for (k = 0; k < dev->color_info.num_components; k++)
                    temp_state.color_component_map.color_map[k] = k;
                temp_state.color_component_map.num_components = dev->color_info.num_components;
                gx_remap_concrete_devicen(conc, pdc, &temp_state, dev, select, pcs);
            }
        } else {
            gx_remap_concrete_devicen(conc, pdc, pgs, dev, select, pcs);
        }
        /* Save original color space and color info into dev color */
        i = any_abs(i);
        for (i--; i >= 0; i--)
            pdc->ccolor.paint.values[i] = pcc->paint.values[i];
        pdc->ccolor_valid = true;
        return true;
    }
    return false;
}

/* By default, remap a color by concretizing it and then remapping the concrete
   color. */
int
gx_default_remap_color(const gs_client_color * pcc, const gs_color_space * pcs,
        gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
                       gs_color_select_t select)
{
    frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    const gs_color_space *pconcs;
    int i = pcs->type->num_components(pcs);
    int code = (*pcs->type->concretize_color)(pcc, pcs, conc, pgs, dev);
    cmm_dev_profile_t *dev_profile;

    if (code < 0)
        return code;
    pconcs = cs_concrete_space(pcs, pgs);
    if (!pconcs)
        return gs_note_error(gs_error_undefined);
    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;
    code = (*pconcs->type->remap_concrete_color)(pconcs, conc, pdc, pgs, dev, select, dev_profile);

    /* Save original color space and color info into dev color */
    i = any_abs(i);
    for (i--; i >= 0; i--)
        pdc->ccolor.paint.values[i] = pcc->paint.values[i];
    pdc->ccolor_valid = true;
    return code;
}

/* Color remappers for the standard color spaces. */
/* Note that we use D... instead of Device... in some places because */
/* gcc under VMS only retains 23 characters of procedure names. */

/* DeviceGray */
int
gx_concretize_DeviceGray(const gs_client_color * pc, const gs_color_space * pcs,
                         frac * pconc, const gs_gstate * pgs, gx_device *dev)
{
    pconc[0] = gx_unit_frac(pc->paint.values[0]);
    return 0;
}
int
gx_remap_concrete_DGray(const gs_color_space * pcs, const frac * pconc,
                        gx_device_color * pdc, const gs_gstate * pgs,
                        gx_device * dev, gs_color_select_t select,
                        const cmm_dev_profile_t *dev_profile)
{
    (*pgs->cmap_procs->map_gray)(pconc[0], pdc, pgs, dev, select);
    return 0;
}
int
gx_remap_DeviceGray(const gs_client_color * pc, const gs_color_space * pcs,
                    gx_device_color * pdc, const gs_gstate * pgs,
                    gx_device * dev, gs_color_select_t select)
{
    frac fgray = gx_unit_frac(pc->paint.values[0]);
    int code;

    /* We are in here due to the fact that we are using a color space that
       was set in the graphic state before the ICC manager was intitialized
       and the color space was never actually "installed" and hence set
       over to a proper ICC color space. We will "install" this color space
       at this time */
    if (pgs->icc_manager->default_gray != NULL) {
        gs_color_space *pcs_notconst = (gs_color_space*) pcs;
        pcs_notconst->cmm_icc_profile_data = pgs->icc_manager->default_gray;
        gsicc_adjust_profile_rc(pgs->icc_manager->default_gray, 1, "gx_remap_DeviceGray");
        pcs_notconst->type = &gs_color_space_type_ICC;
        code =
            (*pcs_notconst->type->remap_color)(gs_currentcolor_inline(pgs),
                                               pcs_notconst,
                                               gs_currentdevicecolor_inline(pgs),
                                               pgs, pgs->device,
                                               gs_color_select_texture);
        return code;
    }

    /* Save original color space and color info into dev color */
    pdc->ccolor.paint.values[0] = pc->paint.values[0];
    pdc->ccolor_valid = true;

    (*pgs->cmap_procs->map_gray)(fgray, pdc, pgs, dev, select);
    return 0;
}

/* DeviceRGB */
int
gx_concretize_DeviceRGB(const gs_client_color * pc, const gs_color_space * pcs,
                        frac * pconc, const gs_gstate * pgs, gx_device *dev)
{
    pconc[0] = gx_unit_frac(pc->paint.values[0]);
    pconc[1] = gx_unit_frac(pc->paint.values[1]);
    pconc[2] = gx_unit_frac(pc->paint.values[2]);
    return 0;
}
int
gx_remap_concrete_DRGB(const gs_color_space * pcs, const frac * pconc,
                       gx_device_color * pdc, const gs_gstate * pgs,
                       gx_device * dev, gs_color_select_t select,
                       const cmm_dev_profile_t *dev_profile)
{

    gx_remap_concrete_rgb(pconc[0], pconc[1], pconc[2], pdc, pgs, dev, select);
    return 0;
}
int
gx_remap_DeviceRGB(const gs_client_color * pc, const gs_color_space * pcs,
        gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
                   gs_color_select_t select)
{
    frac fred = gx_unit_frac(pc->paint.values[0]), fgreen = gx_unit_frac(pc->paint.values[1]),
         fblue = gx_unit_frac(pc->paint.values[2]);

    /* Save original color space and color info into dev color */
    pdc->ccolor.paint.values[0] = pc->paint.values[0];
    pdc->ccolor.paint.values[1] = pc->paint.values[1];
    pdc->ccolor.paint.values[2] = pc->paint.values[2];
    pdc->ccolor_valid = true;

    gx_remap_concrete_rgb(fred, fgreen, fblue, pdc, pgs, dev, select);
    return 0;
}

/* DeviceCMYK */
int
gx_concretize_DeviceCMYK(const gs_client_color * pc, const gs_color_space * pcs,
                         frac * pconc, const gs_gstate * pgs, gx_device *dev)
{
    pconc[0] = gx_unit_frac(pc->paint.values[0]);
    pconc[1] = gx_unit_frac(pc->paint.values[1]);
    pconc[2] = gx_unit_frac(pc->paint.values[2]);
    pconc[3] = gx_unit_frac(pc->paint.values[3]);
    return 0;
}
int
gx_remap_concrete_DCMYK(const gs_color_space * pcs, const frac * pconc,
                        gx_device_color * pdc, const gs_gstate * pgs,
                        gx_device * dev, gs_color_select_t select,
                        const cmm_dev_profile_t *dev_profile)
{
/****** IGNORE alpha ******/
    gx_remap_concrete_cmyk(pconc[0], pconc[1], pconc[2], pconc[3], pdc,
                           pgs, dev, select, pcs);
    return 0;
}
int
gx_remap_DeviceCMYK(const gs_client_color * pc, const gs_color_space * pcs,
        gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
                    gs_color_select_t select)
{
/****** IGNORE alpha ******/
    /* Save original color space and color info into dev color */
    pdc->ccolor.paint.values[0] = pc->paint.values[0];
    pdc->ccolor.paint.values[1] = pc->paint.values[1];
    pdc->ccolor.paint.values[2] = pc->paint.values[2];
    pdc->ccolor.paint.values[3] = pc->paint.values[3];
    pdc->ccolor_valid = true;
    gx_remap_concrete_cmyk(gx_unit_frac(pc->paint.values[0]),
                           gx_unit_frac(pc->paint.values[1]),
                           gx_unit_frac(pc->paint.values[2]),
                           gx_unit_frac(pc->paint.values[3]),
                           pdc, pgs, dev, select, pcs);
    return 0;
}

/* ------ Utility for selecting the dev_ht from the pgs using the dev->graphics_type_tag ----- */

static gs_HT_objtype_t
tag_to_HT_objtype[8] = { HT_OBJTYPE_DEFAULT,
                         HT_OBJTYPE_TEXT,	/* GS_TEXT_TAG = 0x1  */
                         HT_OBJTYPE_IMAGE,	/* GS_IMAGE_TAG = 0x2 */
                         HT_OBJTYPE_DEFAULT,
                         HT_OBJTYPE_VECTOR,	/* GS_VECTOR_TAG = 0x4  */
                         HT_OBJTYPE_DEFAULT, HT_OBJTYPE_DEFAULT, HT_OBJTYPE_DEFAULT
                       };

/* Return the selected dev_ht[] or the pgs->dev_ht[HT_OBJTYPE_DEFAULT] */
gx_device_halftone *
gx_select_dev_ht(const gs_gstate *pgs)
{
    gs_HT_objtype_t objtype;

    /* This function only works with 3 bits currently. Flag here in case we add object types */
    assert(HT_OBJTYPE_COUNT == 4);

    objtype = tag_to_HT_objtype[pgs->device->graphics_type_tag & 7];
    if (pgs->dev_ht[objtype] == NULL)
        objtype = HT_OBJTYPE_DEFAULT;
    return pgs->dev_ht[objtype];
}

/* ------ Render Gray color. ------ */

static void
cmap_gray_halftoned(frac gray, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select)
{
    uchar i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    const gx_device *cmdev;
    const gx_cm_color_map_procs *cmprocs;

    /* map to the color model */
    cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);
    cmprocs->map_gray(cmdev, gray, cm_comps);

    /* apply the transfer function(s); convert to color values */
    if (pgs->effective_transfer_non_identity_count == 0) {
        /* No transfer function to apply */
    } else if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            cm_comps[i] = gx_map_color_frac(pgs,
                                cm_comps[i], effective_transfer[i]);
    else {
        if (gx_get_opmsupported(dev) == GX_CINFO_OPMSUPPORTED) {  /* CMYK-like color space */
            i = dev->color_info.black_component;
            if (i < ncomps)
                cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                        (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
        } else {
            for (i = 0; i < ncomps; i++)
                    cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                            (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
        }
    }
    if (gx_render_device_DeviceN(cm_comps, pdc, dev, gx_select_dev_ht(pgs),
                                        &pgs->screen_phase[select]) == 1)
        gx_color_load_select(pdc, pgs, dev, select);
}

static void
cmap_gray_direct(frac gray, gx_device_color * pdc, const gs_gstate * pgs,
                 gx_device * dev, gs_color_select_t select)
{
    uchar i, nc, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    const gx_device *cmdev;
    const gx_cm_color_map_procs *cmprocs;

    /* map to the color model */
    cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);
    cmprocs->map_gray(cmdev, gray, cm_comps);

    nc = ncomps;
    if (device_encodes_tags(dev))
        nc--;
    /* apply the transfer function(s); convert to color values */
    if (pgs->effective_transfer_non_identity_count == 0) {
        for (i = 0; i < nc; i++)
            cv[i] = frac2cv(cm_comps[i]);
    } else if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < nc; i++) {
            cm_comps[i] = gx_map_color_frac(pgs,
                                cm_comps[i], effective_transfer[i]);
            cv[i] = frac2cv(cm_comps[i]);
        }
    else {
        if (gx_get_opmsupported(dev) == GX_CINFO_OPMSUPPORTED) {  /* CMYK-like color space */
            i = dev->color_info.black_component;
            if (i < ncomps)
                cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                        (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
            for (i = 0; i < nc; i++)
                cv[i] = frac2cv(cm_comps[i]);
        } else {
            for (i = 0; i < nc; i++) {
                cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                            (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
                cv[i] = frac2cv(cm_comps[i]);
            }
        }
    }
    /* Copy tags untransformed. */
    if (nc < ncomps)
        cv[nc] = cm_comps[nc];

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index) {
        color_set_pure(pdc, color);
        return;
    }
    if (gx_render_device_DeviceN(cm_comps, pdc, dev, gx_select_dev_ht(pgs),
                                        &pgs->screen_phase[select]) == 1)
        gx_color_load_select(pdc, pgs, dev, select);
}

/* ------ Render RGB color. ------ */

static void
cmap_rgb_halftoned(frac r, frac g, frac b, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select)
{
    uchar i, nc, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    const gx_device *cmdev;
    const gx_cm_color_map_procs *cmprocs;

    /* map to the color model */
    cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);
    cmprocs->map_rgb(cmdev, pgs, r, g, b, cm_comps);

    nc = ncomps;
    if (device_encodes_tags(dev))
        nc--;
    /* apply the transfer function(s); convert to color values */
    if (pgs->effective_transfer_non_identity_count != 0) {
        int n = 0;
        if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
            n = nc < 3 ? nc : 3;

        for (i = 0; i < n; i++)
            cm_comps[i] = gx_map_color_frac(pgs,
                                cm_comps[i], effective_transfer[i]);
        for (; i < n; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                        (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
    }

    if (gx_render_device_DeviceN(cm_comps, pdc, dev, gx_select_dev_ht(pgs),
                                        &pgs->screen_phase[select]) == 1)
        gx_color_load_select(pdc, pgs, dev, select);
}

static void
cmap_rgb_direct(frac r, frac g, frac b, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select)
{
    uchar i, nc, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    const gx_device *cmdev;
    const gx_cm_color_map_procs *cmprocs;

    /* map to the color model */
    cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);
    cmprocs->map_rgb(cmdev, pgs, r, g, b, cm_comps);

    nc = ncomps;
    if (device_encodes_tags(dev))
        nc--;
    /* apply the transfer function(s); convert to color values */
    if (pgs->effective_transfer_non_identity_count != 0) {
        int n = 0;
        if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
            n = nc < 3 ? nc : 3;
        for (i = 0; i < n; i++)
            cm_comps[i] = gx_map_color_frac(pgs, cm_comps[i],
                                            effective_transfer[i]);
        for (; i < nc; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                        (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
    }

    if (dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < nc; i++)
            pdc->colors.devn.values[i] = frac2cv(cm_comps[i]);
        if (i < ncomps)
            pdc->colors.devn.values[i] = cm_comps[i];
        pdc->type = gx_dc_type_devn;
    } else {
        for (i = 0; i < nc; i++)
            cv[i] = frac2cv(cm_comps[i]);
        if (i < ncomps)
            cv[i] = cm_comps[i];
        /* encode as a color index */
        color = dev_proc(dev, encode_color)(dev, cv);

        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index) {
            color_set_pure(pdc, color);
            return;
        }
        if (gx_render_device_DeviceN(cm_comps, pdc, dev, gx_select_dev_ht(pgs),
                                        &pgs->screen_phase[select]) == 1)
            gx_color_load_select(pdc, pgs, dev, select);
    }
}

/* ------ Render CMYK color. ------ */

static void
cmap_cmyk_direct(frac c, frac m, frac y, frac k, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select,
     const gs_color_space *source_pcs)
{
    uchar i, nc, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    uint black_index;
    cmm_dev_profile_t *dev_profile;
    gsicc_colorbuffer_t src_space = gsUNDEFINED;
    bool gray_to_k;
    const gx_device *cmdev;
    const gx_cm_color_map_procs *cmprocs;

    /* map to the color model */
    cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);
    cmprocs->map_cmyk(cmdev, c, m, y, k, cm_comps);

    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE) {
        if (pgs->effective_transfer_non_identity_count != 0)
            for (i = 0; i < ncomps; i++)
                cm_comps[i] = gx_map_color_frac(pgs,
                                cm_comps[i], effective_transfer[i]);
    } else {
        /* Check if source space is gray.  In this case we are to use only the
           transfer function on the K channel.  Do this only if gray to K is
           also set */
        dev_proc(dev, get_profile)(dev, &dev_profile);
        gray_to_k = dev_profile->devicegraytok;
        if (source_pcs != NULL && source_pcs->cmm_icc_profile_data != NULL) {
            src_space = source_pcs->cmm_icc_profile_data->data_cs;
        } else if (source_pcs != NULL && source_pcs->icc_equivalent != NULL) {
            src_space = source_pcs->icc_equivalent->cmm_icc_profile_data->data_cs;
        }
        if (src_space == gsGRAY && gray_to_k) {
            /* Find the black channel location */
            black_index = dev_proc(dev, get_color_comp_index)(dev, "Black",
                                    strlen("Black"), SEPARATION_NAME);
            cm_comps[black_index] = frac_1 - gx_map_color_frac(pgs,
                                    (frac)(frac_1 - cm_comps[black_index]),
                                    effective_transfer[black_index]);
        } else if (pgs->effective_transfer_non_identity_count != 0)
            for (i = 0; i < ncomps; i++)
                cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                            (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
    }
    /* We make a test for direct vs. halftoned, rather than */
    /* duplicating most of the code of this procedure. */
    if (gx_device_must_halftone(dev)) {
        if (gx_render_device_DeviceN(cm_comps, pdc, dev,
                    gx_select_dev_ht(pgs), &pgs->screen_phase[select]) == 1)
            gx_color_load_select(pdc, pgs, dev, select);
        return;
    }
    /* if output device supports devn, we need to make sure we send it the
       proper color type */
    nc = ncomps;
    if (device_encodes_tags(dev))
        nc--;
    if (dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < nc; i++)
            pdc->colors.devn.values[i] = frac2cv(cm_comps[i]);
        if (i < ncomps)
            pdc->colors.devn.values[i] = cm_comps[i];
        pdc->type = gx_dc_type_devn;
    } else {
        for (i = 0; i < nc; i++)
            cv[i] = frac2cv(cm_comps[i]);
        if (i < ncomps)
            cv[i] = cm_comps[i];
        color = dev_proc(dev, encode_color)(dev, cv);
        if (color != gx_no_color_index)
            color_set_pure(pdc, color);
        else {
            if (gx_render_device_DeviceN(cm_comps, pdc, dev,
                        gx_select_dev_ht(pgs), &pgs->screen_phase[select]) == 1)
                gx_color_load_select(pdc, pgs, dev, select);
        }
    }
    return;
}

/* ------ Render Separation All color. ------ */

/*
 * This routine maps DeviceN components into the order of the device's
 * colorants.
 *
 * Parameters:
 *    pcc - Pointer to DeviceN components.
 *    pcolor_component_map - Map from DeviceN to the Devices colorants.
 *        A negative value indicates component is not to be mapped.
 *    plist - Pointer to list for mapped components
 *    num_comps - num_comps that we need to zero (may be more than
 *                is set if we are mapping values for an NCLR ICC profile
 *                via an alternate tint transform for a sep value) --
 *                i.e. cmyk+og values and we may have some spots that
 *                are supported but may have reached the limit and
 *                using the alt tint values.  Need to make sure to zero all.
 *
 * Returns:
 *    Mapped components in plist.
 */
static inline void
map_components_to_colorants(const frac * pcc,
        const gs_devicen_color_map * pcolor_component_map, frac * plist,
        int num_colorants)
{
    int i;
    int pos;

    /* Clear all output colorants first */
    for (i = num_colorants - 1; i >= 0; i--) {
        plist[i] = frac_0;
    }

    /* Map color components into output list */
    for (i = pcolor_component_map->num_components - 1; i >= 0; i--) {
        pos = pcolor_component_map->color_map[i];
        if (pos >= 0)
            plist[pos] = pcc[i];
    }
}

static bool
named_color_supported(const gs_gstate * pgs)
{
    gs_color_space *pcs = gs_currentcolorspace_inline(pgs);
    gs_color_space_index type = gs_color_space_get_index(pcs);

    if (pgs->icc_manager->device_named == NULL)
        return false;

    if (type == gs_color_space_index_Separation && pcs->params.separation.named_color_supported)
        return true;

    if (type == gs_color_space_index_DeviceN && pcs->params.device_n.named_color_supported)
        return true;

    return false;
}

/* Routines for handling CM of CMYK components of a DeviceN color space */
static bool
devicen_has_cmyk(gx_device * dev, cmm_profile_t *des_profile)
{
    gs_devn_params *devn_params;

    devn_params = dev_proc(dev, ret_devn_params)(dev);
    if (devn_params == NULL) {
        if (des_profile != NULL && des_profile->data_cs == gsCMYK)
            return true;
        else
            return false;
    }
    return(devn_params->num_std_colorant_names == 4);
}

static void
devicen_sep_icc_cmyk(frac cm_comps[], const gs_gstate * pgs,
    const gs_color_space * pcs, gx_device *dev)
{
    gsicc_link_t *icc_link;
    gsicc_rendering_param_t rendering_params;
    unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int k, code;
    unsigned short *psrc_temp;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile = NULL;
    cmm_profile_t *des_profile = NULL;
    cmm_profile_t *src_profile = pgs->icc_manager->default_cmyk;

    code = dev_proc(dev, get_profile)(dev, &dev_profile);

    /* If we can't transform them, we will just leave them as is. */
    if (code < 0)
        return;

    gsicc_extract_profile(dev->graphics_type_tag,
        dev_profile, &des_profile, &render_cond);
    /* Define the rendering intents. */
    rendering_params.black_point_comp = pgs->blackptcomp;
    rendering_params.graphics_type_tag = dev->graphics_type_tag;
    rendering_params.override_icc = false;
    rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
    rendering_params.rendering_intent = pgs->renderingintent;
    rendering_params.cmm = gsCMM_DEFAULT;
    /* Sigh, frac to full 16 bit.  Need to clean this up */
    for (k = 0; k < 4; k++) {
        psrc[k] = frac2cv(cm_comps[k]);
    }

    /* Determine what src profile to use.  First choice is the attributes
       process color space if it is the correct type.  Second choice is
       the alternate tint transform color space if it is the correct type.
       Third type is default_cmyk.  If we have an issue with bad profiles then
       the color values will just remain as they were from the source */
    if (gs_color_space_get_index(pcs) == gs_color_space_index_DeviceN) {
        if (pcs->params.device_n.devn_process_space != NULL &&
            pcs->params.device_n.devn_process_space->cmm_icc_profile_data != NULL &&
            pcs->params.device_n.devn_process_space->cmm_icc_profile_data->data_cs == gsCMYK) {
            src_profile = pcs->params.device_n.devn_process_space->cmm_icc_profile_data;
        } else if (pcs->base_space != NULL &&
            pcs->base_space->cmm_icc_profile_data != NULL &&
            pcs->base_space->cmm_icc_profile_data->data_cs == gsCMYK &&
            USE_ALT_MAP) {
            src_profile = pcs->base_space->cmm_icc_profile_data;
        }
    } else if (gs_color_space_get_index(pcs) == gs_color_space_index_Separation) {
        if (pcs->base_space != NULL &&
            pcs->base_space->cmm_icc_profile_data != NULL &&
            pcs->base_space->cmm_icc_profile_data->data_cs == gsCMYK &&
            USE_ALT_MAP) {
            src_profile = pcs->base_space->cmm_icc_profile_data;
        }
    }

    icc_link = gsicc_get_link_profile(pgs, dev, src_profile, des_profile,
        &rendering_params, pgs->memory, dev_profile->devicegraytok);

    if (icc_link == NULL && src_profile != pgs->icc_manager->default_cmyk) {
        icc_link = gsicc_get_link_profile(pgs, dev,
            pgs->icc_manager->default_cmyk, des_profile,
            &rendering_params, pgs->memory, dev_profile->devicegraytok);
    }

    /* If we can't transform them, we will just leave them as is. */
    if (icc_link == NULL)
        return;

    /* Transform the color */
    if (icc_link->is_identity) {
        psrc_temp = &(psrc[0]);
    } else {
        /* Transform the color */
        psrc_temp = &(psrc_cm[0]);
        (icc_link->procs.map_color)(dev, icc_link, psrc, psrc_temp, 2);
    }
    /* This needs to be optimized */
    for (k = 0; k < 4; k++) {
        cm_comps[k] = float2frac(((float)psrc_temp[k]) / 65535.0);
    }
    /* Release the link */
    gsicc_release_link(icc_link);
}

static void
cmap_separation_halftoned(frac all, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select,
     const gs_color_space *pcs)
{
    uint i, ncomps = dev->color_info.num_components;
    bool additive = dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE;
    frac comp_value = all;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile = NULL;
    cmm_profile_t *des_profile = NULL;

    dev_proc(dev, get_profile)(dev, &dev_profile);
    gsicc_extract_profile(dev->graphics_type_tag,
        dev_profile, &des_profile, &render_cond);

    if (pgs->color_component_map.sep_type == SEP_ALL) {
        /*
         * Invert the photometric interpretation for additive
         * color spaces because separations are always subtractive.
         */
        if (additive)
            comp_value = frac_1 - comp_value;

        /* Use the "all" value for all components */
        for (i = 0; i < pgs->color_component_map.num_colorants; i++)
            cm_comps[i] = comp_value;
    } else {
        if (pgs->color_component_map.sep_type == SEP_NONE) {
            color_set_null(pdc);
            return;
        }

        /* map to the color model */
        map_components_to_colorants(&all, &(pgs->color_component_map), cm_comps,
            pgs->color_component_map.num_colorants);
    }

    if (devicen_has_cmyk(dev, des_profile) &&
        des_profile->data_cs == gsCMYK &&
        !named_color_supported(pgs)) {
        devicen_sep_icc_cmyk(cm_comps, pgs, pcs, dev);
    }

    /* apply the transfer function(s); convert to color values */
    if (pgs->effective_transfer_non_identity_count != 0) {
        int n = 0;
        if (additive)
            n = ncomps < 3 ? ncomps : 3;
        for (i = 0; i < n; i++)
            cm_comps[i] = gx_map_color_frac(pgs,
                                cm_comps[i], effective_transfer[i]);
        for (; i < ncomps; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                        (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
    }

    if (gx_render_device_DeviceN(cm_comps, pdc, dev, gx_select_dev_ht(pgs),
                                        &pgs->screen_phase[select]) == 1)
        gx_color_load_select(pdc, pgs, dev, select);
}

static void
cmap_separation_direct(frac all, gx_device_color * pdc, const gs_gstate * pgs,
                 gx_device * dev, gs_color_select_t select, const gs_color_space *pcs)
{
    uint i, nc, ncomps = dev->color_info.num_components;
    bool additive = dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE;
    frac comp_value = all;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    bool use_rgb2dev_icc = false;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile = NULL;
    cmm_profile_t *des_profile = NULL;
    int num_additives = additive ? dev_proc(dev, dev_spec_op)(dev, gxdso_is_sep_supporting_additive_device, NULL, 0) : 0;

    dev_proc(dev, get_profile)(dev,  &dev_profile);
    gsicc_extract_profile(dev->graphics_type_tag,
                          dev_profile, &des_profile, &render_cond);
    if (pgs->color_component_map.sep_type == SEP_ALL) {
        /*
         * Invert the photometric interpretation for additive
         * color spaces because separations are always subtractive.
         */
        if (additive && num_additives <= 0) {
            comp_value = frac_1 - comp_value;
        }

        /* Use the "all" value for all components */
        for (i = 0; i < pgs->color_component_map.num_colorants; i++)
            cm_comps[i] = comp_value;
        /* If our device space is CIELAB then we really want to treat this
           as RGB during the fill up here of the separation value and then
           go ahead and convert from RGB to CIELAB.  The PDF spec is not clear
           on how addivite devices should behave with the ALL option but it
           is clear from testing the AR 10 does simply do the RGB = 1 - INK
           type of mapping */
        if (des_profile->data_cs == gsCIELAB || des_profile->islab) {
            use_rgb2dev_icc = true;
        }
    } else {
        if (pgs->color_component_map.sep_type == SEP_NONE) {
            color_set_null(pdc);
            return;
        }

        /* map to the color model */
        map_components_to_colorants(&comp_value, &(pgs->color_component_map), cm_comps,
            pgs->color_component_map.num_colorants);
    }

    /* Check if we have the standard colorants.  If yes, then we will apply
      ICC color management to those colorants. */
    if (devicen_has_cmyk(dev, des_profile) && des_profile->data_cs == gsCMYK &&
        !named_color_supported(pgs) && pgs->color_component_map.sep_type != SEP_ALL) {
        /* We need to do a CMYK to CMYK conversion here.  This will always
           use the default CMYK profile and the device's output profile.
           We probably need to add some checking here
           and possibly permute the colorants, much as is done on the input
           side for the case when we add DeviceN icc source profiles for use
           in PDF and PS data. Also, don't do this if we are doing mapping
           through the named color mapping.  */
        devicen_sep_icc_cmyk(cm_comps, pgs, pcs, dev);
    }

    /* apply the transfer function(s); convert to color values */
    nc = ncomps;
    if (device_encodes_tags(dev))
        nc--;
    if (pgs->effective_transfer_non_identity_count != 0) {
        int n = 0;
        if (additive)
            n = nc < 3 ? nc : 3;
        for (i = 0; i < n; i++)
            cm_comps[i] = gx_map_color_frac(pgs,
                                cm_comps[i], effective_transfer[i]);
        for (; i < nc; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                        (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
    }
    for (i = 0; i < nc; i++)
        cv[i] = frac2cv(cm_comps[i]);
    /* For additive devices, we should invert the process colors
     * here! But how do we know how many process colors we have? For
     * now we'll have to ask the device using a dso. */
    if (additive) {
        int j;
        for (j = 0; j < num_additives; j++)
            cv[j] = 65535 - cv[j];
    }
    /* Copy tags untransformed. */
    if (nc < ncomps)
        cv[nc] = cm_comps[nc];

    if (use_rgb2dev_icc && pgs->icc_manager->default_rgb != NULL) {
        /* After the transfer function go ahead and do the mapping from RGB to
           the device profile. */
        gsicc_link_t *icc_link;
        gsicc_rendering_param_t rendering_params;
        unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS], psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];

        rendering_params.black_point_comp = pgs->blackptcomp;
        rendering_params.graphics_type_tag = dev->graphics_type_tag;
        rendering_params.override_icc = false;
        rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
        rendering_params.rendering_intent = pgs->renderingintent;
        rendering_params.cmm = gsCMM_DEFAULT;

        icc_link = gsicc_get_link_profile(pgs, dev, pgs->icc_manager->default_rgb,
                                          des_profile, &rendering_params,
                                          pgs->memory, dev_profile->devicegraytok);
        /* Transform the color */
        for (i = 0; i < ncomps; i++) {
            psrc[i] = cv[i];
        }
        (icc_link->procs.map_color)(dev, icc_link, &(psrc[0]), &(psrc_cm[0]), 2);
        gsicc_release_link(icc_link);
        for (i = 0; i < ncomps; i++) {
            cv[i] = psrc_cm[i];
        }
    }
    /* if output device supports devn, we need to make sure we send it the
       proper color type */
    if (dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < ncomps; i++)
            pdc->colors.devn.values[i] = cv[i];
        pdc->type = gx_dc_type_devn;

        /* Let device set the tags if present */
        if (device_encodes_tags(dev)) {
            const gx_device *cmdev;
            const gx_cm_color_map_procs *cmprocs;

            cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);
            cmprocs->map_cmyk(cmdev, 0, 0, 0, 0, cm_comps);
            pdc->colors.devn.values[ncomps - 1] = cm_comps[ncomps - 1];
        }
        return;
    }

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index) {
        color_set_pure(pdc, color);
        return;
    }

    if (gx_render_device_DeviceN(cm_comps, pdc, dev, gx_select_dev_ht(pgs),
                                        &pgs->screen_phase[select]) == 1)
        gx_color_load_select(pdc, pgs, dev, select);
}

/* ------ DeviceN color mapping */

/*
 * This routine is called to map a DeviceN colorspace to a DeviceN
 * output device which requires halftoning.  T
 */
static void
cmap_devicen_halftoned(const frac * pcc,
    gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
    gs_color_select_t select, const gs_color_space *pcs)
{
    uchar i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile = NULL;
    cmm_profile_t *des_profile = NULL;

    if (pcs->params.device_n.all_none == true) {
        color_set_null(pdc);
        return;
    }

    dev_proc(dev, get_profile)(dev,  &dev_profile);
    gsicc_extract_profile(dev->graphics_type_tag,
                          dev_profile, &des_profile, &render_cond);
    /* map to the color model */
    map_components_to_colorants(pcc, &(pgs->color_component_map), cm_comps,
        pgs->color_component_map.num_colorants);
    /* See comments in cmap_devicen_direct for details on below operations */
    if (devicen_has_cmyk(dev, des_profile) &&
        des_profile->data_cs == gsCMYK &&
        !named_color_supported(pgs)) {
        devicen_sep_icc_cmyk(cm_comps, pgs, pcs, dev);
    }
    /* apply the transfer function(s); convert to color values */
    if (pgs->effective_transfer_non_identity_count != 0) {
        int n = 0;
        if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
            n = ncomps < 3 ? ncomps : 3;
        for (i = 0; i < n; i++)
            cm_comps[i] = gx_map_color_frac(pgs,
                                cm_comps[i], effective_transfer[i]);
        for (; i < ncomps; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                        (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
    }

    /* We need to finish halftoning */
    if (gx_render_device_DeviceN(cm_comps, pdc, dev, gx_select_dev_ht(pgs),
                                        &pgs->screen_phase[select]) == 1)
        gx_color_load_select(pdc, pgs, dev, select);
}

static void
encode_tags(const gx_device *dev, gx_device_color *pdc)
{
    const gx_device *cmdev;
    const gx_cm_color_map_procs *cmprocs;
    uchar ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];

    cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);
    cmprocs->map_cmyk(cmdev, 0, 0, 0, 0, cm_comps);
    pdc->colors.devn.values[ncomps - 1] = cm_comps[ncomps - 1];
}

/*
 * This routine is called to map a DeviceN colorspace to a DeviceN
 * output device which does not require halftoning.
 */
static void
cmap_devicen_direct(const frac * pcc,
    gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
    gs_color_select_t select, const gs_color_space *pcs)
{
    uchar i, nc, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile = NULL;
    cmm_profile_t *des_profile = NULL;
    int additive = dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE;

    if (pcs->params.device_n.all_none == true) {
        color_set_null(pdc);
        return;
    }

    dev_proc(dev, get_profile)(dev,  &dev_profile);
    gsicc_extract_profile(dev->graphics_type_tag,
                          dev_profile, &des_profile, &render_cond);
    /*   See the comment below */
    /* map to the color model */
    if (dev_profile->spotnames != NULL && dev_profile->spotnames->equiv_cmyk_set) {
        map_components_to_colorants(pcc, dev_profile->spotnames->color_map,
                                    cm_comps, ncomps);
    } else {
        map_components_to_colorants(pcc, &(pgs->color_component_map), cm_comps,
            pgs->color_component_map.num_colorants);
    }
    /*  Check if we have the standard colorants.  If yes, then we will apply
       ICC color management to those colorants. To understand why, consider
       the example where I have a Device with CMYK + O  and I have a
       DeviceN color in the document that is specified for any set of
       these colorants, and suppose that I let them pass through
       witout any color management.  This is probably  not the
       desired effect since I could have a DeviceN color fill that had 10% C,
       20% M 0% Y 0% K and 0% O.  I would like this to look the same
       as a CMYK color that will be color managed and specified with 10% C,
       20% M 0% Y 0% K. Hence the CMYK values should go through the same
       color management as a stand alone CMYK value.  */
    if (devicen_has_cmyk(dev, des_profile) && des_profile->data_cs == gsCMYK &&
        !named_color_supported(pgs)) {
        /* We need to do a CMYK to CMYK conversion here.  This will always
           use the default CMYK profile and the device's output profile.
           We probably need to add some checking here
           and possibly permute the colorants, much as is done on the input
           side for the case when we add DeviceN icc source profiles for use
           in PDF and PS data. Also, don't do this if we are doing mapping
           through the named color mapping.  */
        devicen_sep_icc_cmyk(cm_comps, pgs, pcs, dev);
    }
    nc = ncomps;
    if (device_encodes_tags(dev))
        nc--;
    /* apply the transfer function(s); convert to color values.
       assign directly if output device supports devn */
    if (dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0)) {
        if (pgs->effective_transfer_non_identity_count == 0)
            for (i = 0; i < nc; i++)
                pdc->colors.devn.values[i] = frac2cv(cm_comps[i]);
        else {
            int n = 0;
            if (additive)
                n = nc < 3 ? nc : 3;
            for (i = 0; i < n; i++)
                pdc->colors.devn.values[i] = frac2cv(gx_map_color_frac(pgs,
                                    cm_comps[i], effective_transfer[i]));
            for (; i < nc; i++)
                pdc->colors.devn.values[i] = frac2cv(frac_1 - gx_map_color_frac(pgs,
                            (frac)(frac_1 - cm_comps[i]), effective_transfer[i]));
        }
        if (i < ncomps)
            pdc->colors.devn.values[i] = cm_comps[i];
        pdc->type = gx_dc_type_devn;
        /* For additive devices, we should invert the process colors
         * here! But how do we know how many process colors we have?
         * Ask the device using a dso. */
        if (additive) {
            int j, n = dev_proc(dev, dev_spec_op)(dev, gxdso_is_sep_supporting_additive_device, NULL, 0);
            for (j = 0; j < n; j++)
                pdc->colors.devn.values[j] = 65535 - pdc->colors.devn.values[j];
        }

        /* Let device set the tags if present */
        if (device_encodes_tags(dev))
            encode_tags(dev, pdc);

        return;
    }

    if (pgs->effective_transfer_non_identity_count != 0) {
        int n = 0;
        if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
            n = nc < 3 ? nc : 3;
        for (i = 0; i < n; i++)
            cm_comps[i] = gx_map_color_frac(pgs,
                                    cm_comps[i], effective_transfer[i]);
        for (; i < nc; i++)
            cm_comps[i] = frac_1 - gx_map_color_frac(pgs,
                            (frac)(frac_1 - cm_comps[i]), effective_transfer[i]);
    }
    if (nc < ncomps)
        encode_tags(dev, pdc);
    /* For additive devices, we should invert the process colors
     * here! But how do we know how many process colors we have?
     * Ask the device using a dso. */
    if (additive) {
        int j, n = dev_proc(dev, dev_spec_op)(dev, gxdso_is_sep_supporting_additive_device, NULL, 0);
        for (j = 0; j < n; j++)
            cm_comps[j] = frac_1 - cm_comps[j];
    }
    for (i = 0; i < nc; i++)
        cv[i] = frac2cv(cm_comps[i]);
    if(i < ncomps)
        cv[i] = cm_comps[i];
    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index) {
        color_set_pure(pdc, color);
        return;
    }
    if (gx_render_device_DeviceN(cm_comps, pdc, dev, gx_select_dev_ht(pgs),
                                        &pgs->screen_phase[select]) == 1)
        gx_color_load_select(pdc, pgs, dev, select);
}

/* ------ Halftoning check ----- */

static bool
cmap_halftoned_is_halftoned(const gs_gstate * pgs, gx_device * dev)
{
    return true;
}

static bool
cmap_direct_is_halftoned(const gs_gstate * pgs, gx_device * dev)
{
    return false;
}

/* ------ Transfer function mapping ------ */

/* Define an identity transfer function. */
float
gs_identity_transfer(double value, const gx_transfer_map * pmap)
{
    return (float) value;
}

/* Define the generic transfer function for the library layer. */
/* This just returns what's already in the map. */
float
gs_mapped_transfer(double value, const gx_transfer_map * pmap)
{
    int index = (int)((value) * (transfer_map_size) + 0.5);
    if (index > transfer_map_size - 1)
        index = transfer_map_size - 1;
    return frac2float(pmap->values[index]);
}

/* Set a transfer map to the identity map. */
void
gx_set_identity_transfer(gx_transfer_map *pmap)
{
    int i;

    pmap->proc = gs_identity_transfer;
    /* We still have to fill in the cached values. */
    for (i = 0; i < transfer_map_size; ++i)
        pmap->values[i] = bits2frac(i, log2_transfer_map_size);
}

#if FRAC_MAP_INTERPOLATE	/* NOTA BENE */

/* Map a color fraction through a transfer map. */
/* We only use this if we are interpolating. */
frac
gx_color_frac_map(frac cv, const frac * values)
{
#define cp_frac_bits (frac_bits - log2_transfer_map_size)
    int cmi = frac2bits_floor(cv, log2_transfer_map_size);
    frac mv = values[cmi];
    int rem, mdv;

    /* Interpolate between two adjacent values if needed. */
    rem = cv - bits2frac(cmi, log2_transfer_map_size);
    if (rem == 0)
        return mv;
    mdv = values[cmi + 1] - mv;
#if ARCH_INTS_ARE_SHORT
    /* Only use long multiplication if necessary. */
    if (mdv < -(1 << (16 - cp_frac_bits)) ||
        mdv > 1 << (16 - cp_frac_bits)
        )
        return mv + (uint) (((ulong) rem * mdv) >> cp_frac_bits);
#endif
    return mv + ((rem * mdv) >> cp_frac_bits);
#undef cp_frac_bits
}

#endif /* FRAC_MAP_INTERPOLATE */

/* ------ Default device color mapping ------ */
/* White-on-black */
gx_color_index
gx_default_w_b_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{				/* Map values >= 1/2 to 1, < 1/2 to 0. */
    int i, ncomps = dev->color_info.num_components;
    gx_color_value  cv_all = 0;

    for (i = 0; i < ncomps; i++)
        cv_all |= cv[i];
    return cv_all > gx_max_color_value / 2 ? (gx_color_index)1
        : (gx_color_index)0;

}

int
gx_default_w_b_map_color_rgb(gx_device * dev, gx_color_index color,
                             gx_color_value prgb[3])
{				/* Map 1 to max_value, 0 to 0. */
    prgb[0] = prgb[1] = prgb[2] = -(gx_color_value) color;
    return 0;
}

gx_color_index
gx_default_w_b_mono_encode_color(gx_device *dev, const gx_color_value cv[])
{
    return cv[0] > gx_max_color_value / 2 ? (gx_color_index)1
                                          : (gx_color_index)0;
}

int
gx_default_w_b_mono_decode_color(gx_device * dev, gx_color_index color,
                                 gx_color_value pgray[])
{				/* Map 0 to max_value, 1 to 0. */
    pgray[0] = -(gx_color_value) color;
    return 0;
}

/* Black-on-white */
gx_color_index
gx_default_b_w_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    uchar i, ncomps = dev->color_info.num_components;
    gx_color_value  cv_all = 0;

    for (i = 0; i < ncomps; i++)
        cv_all |= cv[i];
    return cv_all > gx_max_color_value / 2 ? (gx_color_index)0
        : (gx_color_index)1;
}

int
gx_default_b_w_map_color_rgb(gx_device * dev, gx_color_index color,
                             gx_color_value prgb[3])
{				/* Map 0 to max_value, 1 to 0. */
    prgb[0] = prgb[1] = prgb[2] = -((gx_color_value) color ^ 1);
    return 0;
}

gx_color_index
gx_default_b_w_mono_encode_color(gx_device *dev, const gx_color_value cv[])
{
    return cv[0] > gx_max_color_value / 2 ? (gx_color_index)0
                                          : (gx_color_index)1;
}

int
gx_default_b_w_mono_decode_color(gx_device * dev, gx_color_index color,
                                 gx_color_value pgray[])
{				/* Map 0 to max_value, 1 to 0. */
    pgray[0] = -((gx_color_value) color ^ 1);
    return 0;
}

/* RGB mapping for gray-scale devices */

gx_color_index
gx_default_gray_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{				/* We round the value rather than truncating it. */
    gx_color_value gray =
    (((cv[0] * (ulong) lum_red_weight) +
      (cv[1] * (ulong) lum_green_weight) +
      (cv[2] * (ulong) lum_blue_weight) +
      (lum_all_weights / 2)) / lum_all_weights
     * dev->color_info.max_gray +
     (gx_max_color_value / 2)) / gx_max_color_value;

    return gray;
}

int
gx_default_gray_map_color_rgb(gx_device * dev, gx_color_index color,
                              gx_color_value prgb[3])
{
    gx_color_value gray = (gx_color_value)
        (color * gx_max_color_value / dev->color_info.max_gray);

    prgb[0] = gray;
    prgb[1] = gray;
    prgb[2] = gray;
    return 0;
}

gx_color_index
gx_default_gray_encode_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_value gray = (cv[0] * dev->color_info.max_gray +
                           (gx_max_color_value / 2)) / gx_max_color_value;

    return gray;
}

int
gx_default_gray_decode_color(gx_device * dev, gx_color_index color,
                             gx_color_value *cv)
{
    gx_color_value gray = (gx_color_value)
        (color * gx_max_color_value / dev->color_info.max_gray);

    cv[0] = gray;
    return 0;
}

gx_color_index
gx_default_8bit_map_gray_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_index color = gx_color_value_to_byte(cv[0]);

    return color;
}

int
gx_default_8bit_map_color_gray(gx_device * dev, gx_color_index color,
                              gx_color_value pgray[])
{
    pgray[0] = (gx_color_value)(color * gx_max_color_value / 255);
    return 0;
}

/* RGB mapping for 24-bit true (RGB) color devices */

gx_color_index
gx_default_rgb_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    if (dev->color_info.depth == 24)
        return gx_color_value_to_byte(cv[2]) +
            ((uint) gx_color_value_to_byte(cv[1]) << 8) +
            ((ulong) gx_color_value_to_byte(cv[0]) << 16);
    else {
        COLROUND_VARS;
        int bpc = dev->color_info.depth / 3;
        COLROUND_SETUP(bpc);

        return (((COLROUND_ROUND(cv[0]) << bpc) +
                 COLROUND_ROUND(cv[1])) << bpc) +
               COLROUND_ROUND(cv[2]);
    }
}

/* Map a color index to a r-g-b color. */
int
gx_default_rgb_map_color_rgb(gx_device * dev, gx_color_index color,
                             gx_color_value prgb[3])
{
    if (dev->color_info.depth == 24) {
        prgb[0] = gx_color_value_from_byte(color >> 16);
        prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
        prgb[2] = gx_color_value_from_byte(color & 0xff);
    } else {
        uint bits_per_color = dev->color_info.depth / 3;
        uint color_mask = (1 << bits_per_color) - 1;

        prgb[0] = ((color >> (bits_per_color * 2)) & color_mask) *
            (ulong) gx_max_color_value / color_mask;
        prgb[1] = ((color >> (bits_per_color)) & color_mask) *
            (ulong) gx_max_color_value / color_mask;
        prgb[2] = (color & color_mask) *
            (ulong) gx_max_color_value / color_mask;
    }
    return 0;
}

/* CMYK mapping for RGB devices (should never be called!) */

gx_color_index
gx_default_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{				/* Convert to RGB */
    frac rgb[3];
    gx_color_value rgb_cv[3];
    color_cmyk_to_rgb(cv2frac(cv[0]), cv2frac(cv[1]), cv2frac(cv[2]), cv2frac(cv[3]),
                      NULL, rgb, dev->memory);
    rgb_cv[0] = frac2cv(rgb[0]);
    rgb_cv[1] = frac2cv(rgb[1]);
    rgb_cv[2] = frac2cv(rgb[2]);
    return (*dev_proc(dev, map_rgb_color)) (dev, rgb_cv);
}

/* Mapping for CMYK devices */

gx_color_index
cmyk_1bit_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{
#define CV_BIT(v) ((v) >> (gx_color_value_bits - 1))
    return (gx_color_index)
        (CV_BIT(cv[3]) + (CV_BIT(cv[2]) << 1) + (CV_BIT(cv[1]) << 2) + (CV_BIT(cv[0]) << 3));
#undef CV_BIT
}

/* Shouldn't be called: decode_color should be cmyk_1bit_map_color_cmyk */
int
cmyk_1bit_map_color_rgb(gx_device * dev, gx_color_index color,
                        gx_color_value prgb[3])
{
    if (color & 1)
        prgb[0] = prgb[1] = prgb[2] = 0;
    else {
        prgb[0] = (color & 8 ? 0 : gx_max_color_value);
        prgb[1] = (color & 4 ? 0 : gx_max_color_value);
        prgb[2] = (color & 2 ? 0 : gx_max_color_value);
    }
    return 0;
}

int
cmyk_1bit_map_color_cmyk(gx_device * dev, gx_color_index color,
                        gx_color_value pcv[])
{
    pcv[0] = (color & 8 ? 0 : gx_max_color_value);
    pcv[1] = (color & 4 ? 0 : gx_max_color_value);
    pcv[2] = (color & 2 ? 0 : gx_max_color_value);
    pcv[3] = (color & 1 ? 0 : gx_max_color_value);
    return 0;
}

gx_color_index
cmyk_8bit_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_index color =
        gx_color_value_to_byte(cv[3]) +
        ((uint)gx_color_value_to_byte(cv[2]) << 8) +
        ((uint)gx_color_value_to_byte(cv[1]) << 16) +
        ((uint)gx_color_value_to_byte(cv[0]) << 24);

#if ARCH_SIZEOF_GX_COLOR_INDEX > 4
    return color;
#else
    return (color == gx_no_color_index ? color ^ 1 : color);
#endif
}

gx_color_index
cmyk_16bit_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_index color =
        (uint64_t)cv[3] +
        ((uint64_t)cv[2] << 16) +
        ((uint64_t)cv[1] << 32) +
        ((uint64_t)cv[0] << 48);

    return (color == gx_no_color_index ? color ^ 1 : color);
}

/* Shouldn't be called: decode_color should be cmyk_8bit_map_color_cmyk */
int
cmyk_8bit_map_color_rgb(gx_device * dev, gx_color_index color,
                        gx_color_value prgb[3])
{
    int
        not_k = (int) (~color & 0xff),
        r = not_k - (int) (color >> 24),
        g = not_k - (int) ((color >> 16) & 0xff),
        b = not_k - (int) ((color >> 8) & 0xff);

    prgb[0] = (r < 0 ? 0 : gx_color_value_from_byte(r));
    prgb[1] = (g < 0 ? 0 : gx_color_value_from_byte(g));
    prgb[2] = (b < 0 ? 0 : gx_color_value_from_byte(b));
    return 0;
}

int
cmyk_8bit_map_color_cmyk(gx_device * dev, gx_color_index color,
                        gx_color_value pcv[])
{
    pcv[0] = gx_color_value_from_byte((color >> 24) & 0xff);
    pcv[1] = gx_color_value_from_byte((color >> 16) & 0xff);
    pcv[2] = gx_color_value_from_byte((color >> 8) & 0xff);
    pcv[3] = gx_color_value_from_byte(color & 0xff);
    return 0;
}

int
cmyk_16bit_map_color_cmyk(gx_device * dev, gx_color_index color,
                          gx_color_value pcv[])
{
    pcv[0] = ((color >> 24) >> 24) & 0xffff;
    pcv[1] = ((color >> 16) >> 16) & 0xffff;
    pcv[2] = ( color        >> 16) & 0xffff;
    pcv[3] = ( color             ) & 0xffff;
    return 0;
}

int
cmyk_16bit_map_color_rgb(gx_device * dev, gx_color_index color,
                         gx_color_value prgb[3])
{
    gx_color_value c     = ((color >> 24) >> 24) & 0xffff;
    gx_color_value m     = ((color >> 16) >> 16) & 0xffff;
    gx_color_value y     = ( color        >> 16) & 0xffff;
    gx_color_value not_k = (~color             ) & 0xffff;
    int r     = not_k - c;
    int g     = not_k - m;
    int b     = not_k - y;

    prgb[0] = (r < 0 ? 0 : r);
    prgb[1] = (g < 0 ? 0 : g);
    prgb[2] = (b < 0 ? 0 : b);
    return 0;
}

frac
gx_unit_frac(float fvalue)
{
    frac f = frac_0;
    if (is_fneg(fvalue))
        f = frac_0;
    else if (is_fge1(fvalue))
        f = frac_1;
    else
        f = float2frac(fvalue);
    return f;
}

static void
cmapper_transfer_halftone_add(gx_cmapper_t *data)
{
    gx_color_value *pconc = &data->conc[0];
    const gs_gstate * pgs = data->pgs;
    gx_device * dev = data->dev;
    gs_color_select_t select = data->select;
    uchar ncomps = dev->color_info.num_components;
    frac frac_value;
    uchar i;
    frac cv_frac[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* apply the transfer function(s) */
    for (i = 0; i < ncomps; i++) {
        frac_value = cv2frac(pconc[i]);
        cv_frac[i] = gx_map_color_frac(pgs, frac_value, effective_transfer[i]);
    }
    /* Halftoning */
    if (gx_render_device_DeviceN(&(cv_frac[0]), &data->devc, dev,
                    gx_select_dev_ht(pgs), &pgs->screen_phase[select]) == 1)
        gx_color_load_select(&data->devc, pgs, dev, select);
}

static void
cmapper_transfer_halftone_op(gx_cmapper_t *data)
{
    gx_color_value *pconc = &data->conc[0];
    const gs_gstate * pgs = data->pgs;
    gx_device * dev = data->dev;
    gs_color_select_t select = data->select;
    uchar ncomps = dev->color_info.num_components;
    frac frac_value;
    uchar i;
    frac cv_frac[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* apply the transfer function(s) */
    uint k = dev->color_info.black_component;
    for (i = 0; i < ncomps; i++) {
        frac_value = cv2frac(pconc[i]);
        if (i == k) {
            cv_frac[i] = frac_1 - gx_map_color_frac(pgs,
                (frac)(frac_1 - frac_value), effective_transfer[i]);
        } else {
            cv_frac[i] = frac_value;  /* Ignore transfer, see PLRM3 p. 494 */
        }
    }
    /* Halftoning */
    if (gx_render_device_DeviceN(&(cv_frac[0]), &data->devc, dev,
                    gx_select_dev_ht(pgs), &pgs->screen_phase[select]) == 1)
        gx_color_load_select(&data->devc, pgs, dev, select);
}

static void
cmapper_transfer_halftone_sub(gx_cmapper_t *data)
{
    gx_color_value *pconc = &data->conc[0];
    const gs_gstate * pgs = data->pgs;
    gx_device * dev = data->dev;
    gs_color_select_t select = data->select;
    uchar ncomps = dev->color_info.num_components;
    frac frac_value;
    uchar i;
    frac cv_frac[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* apply the transfer function(s) */
    for (i = 0; i < ncomps; i++) {
        frac_value = cv2frac(pconc[i]);
        cv_frac[i] = frac_1 - gx_map_color_frac(pgs,
                    (frac)(frac_1 - frac_value), effective_transfer[i]);
    }
    /* Halftoning */
    if (gx_render_device_DeviceN(&(cv_frac[0]), &data->devc, dev,
                    gx_select_dev_ht(pgs), &pgs->screen_phase[select]) == 1)
        gx_color_load_select(&data->devc, pgs, dev, select);
}

static void
cmapper_transfer_add(gx_cmapper_t *data)
{
    gx_color_value *pconc = &data->conc[0];
    const gs_gstate * pgs = data->pgs;
    gx_device * dev = data->dev;
    uchar ncomps = dev->color_info.num_components;
    frac frac_value;
    uchar i;
    gx_color_index color;

    /* apply the transfer function(s) */
    if (device_encodes_tags(dev))
        ncomps--;
    for (i = 0; i < ncomps; i++) {
        frac_value = cv2frac(pconc[i]);
        frac_value = gx_map_color_frac(pgs,
                                frac_value, effective_transfer[i]);
        pconc[i] = frac2cv(frac_value);
    }
    /* Halftoning */
    color = dev_proc(dev, encode_color)(dev, &(pconc[0]));
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(&data->devc, color);
}

static void
cmapper_transfer_op(gx_cmapper_t *data)
{
    gx_color_value *pconc = &data->conc[0];
    const gs_gstate * pgs = data->pgs;
    gx_device * dev = data->dev;
    frac frac_value;
    gx_color_index color;

    uint k = dev->color_info.black_component;
    /* Ignore transfer for non blacks, see PLRM3 p. 494 */
    frac_value = cv2frac(pconc[k]);
    frac_value = frac_1 - gx_map_color_frac(pgs,
                (frac)(frac_1 - frac_value), effective_transfer[k]);
    pconc[k] = frac2cv(frac_value);
    /* Halftoning */
    color = dev_proc(dev, encode_color)(dev, &(pconc[0]));
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(&data->devc, color);
}

static void
cmapper_transfer_sub(gx_cmapper_t *data)
{
    gx_color_value *pconc = &data->conc[0];
    const gs_gstate * pgs = data->pgs;
    gx_device * dev = data->dev;
    uchar ncomps = dev->color_info.num_components;
    frac frac_value;
    uchar i;
    gx_color_index color;

    /* apply the transfer function(s) */
    if (device_encodes_tags(dev))
        ncomps--;
    for (i = 0; i < ncomps; i++) {
        frac_value = cv2frac(pconc[i]);
        frac_value = frac_1 - gx_map_color_frac(pgs,
                    (frac)(frac_1 - frac_value), effective_transfer[i]);
        pconc[i] = frac2cv(frac_value);
    }
    /* Halftoning */
    color = dev_proc(dev, encode_color)(dev, &(pconc[0]));
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(&data->devc, color);
}

/* This is used by image color render to handle the cases where we need to
   perform either a transfer function or halftoning on the color values
   during an ICC color flow.  In this case, the color is already in the
   device color space but in 16bpp color values. */
static void
cmapper_halftone(gx_cmapper_t *data)
{
    gx_color_value *pconc = &data->conc[0];
    const gs_gstate * pgs = data->pgs;
    gx_device * dev = data->dev;
    gs_color_select_t select = data->select;
    uchar ncomps = dev->color_info.num_components;
    uchar i;
    frac cv_frac[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* We need this to be in frac form */
    for (i = 0; i < ncomps; i++) {
        cv_frac[i] = cv2frac(pconc[i]);
    }
    if (gx_render_device_DeviceN(&(cv_frac[0]), &data->devc, dev,
                gx_select_dev_ht(pgs), &pgs->screen_phase[select]) == 1)
        gx_color_load_select(&data->devc, pgs, dev, select);
}

/* This is used by image color render to handle the cases where we need to
   perform either a transfer function or halftoning on the color values
   during an ICC color flow.  In this case, the color is already in the
   device color space but in 16bpp color values. */
static void
cmapper_vanilla(gx_cmapper_t *data)
{
    gx_color_value *pconc = &data->conc[0];
    gx_device * dev = data->dev;
    gx_color_index color;

    color = dev_proc(dev, encode_color)(dev, &(pconc[0]));
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(&data->devc, color);
}

void
gx_get_cmapper(gx_cmapper_t *data, const gs_gstate *pgs,
               gx_device *dev, bool has_transfer, bool has_halftone,
               gs_color_select_t select)
{
    memset(&(data->conc[0]), 0, sizeof(gx_color_value[GX_DEVICE_COLOR_MAX_COMPONENTS]));
    data->pgs = pgs;
    data->dev = dev;
    data->select = select;
    data->devc.type = gx_dc_type_none;
    data->direct = 0;
    /* Per spec. Images with soft mask, and the mask, do not use transfer function */
    if (pgs->effective_transfer_non_identity_count == 0 ||
        (dev_proc(dev, dev_spec_op)(dev, gxdso_in_smask, NULL, 0)) > 0)
        has_transfer = 0;
    if (has_transfer) {
        if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE) {
            if (has_halftone)
                data->set_color = cmapper_transfer_halftone_add;
            else
                data->set_color = cmapper_transfer_add;
        } else if (gx_get_opmsupported(dev) == GX_CINFO_OPMSUPPORTED) {
            if (has_halftone)
                data->set_color = cmapper_transfer_halftone_op;
            else
                data->set_color = cmapper_transfer_op;
        } else {
            if (has_halftone)
                data->set_color = cmapper_transfer_halftone_sub;
            else
                data->set_color = cmapper_transfer_sub;
        }
    } else {
        if (has_halftone)
            data->set_color = cmapper_halftone;
        else {
            int code = dev_proc(dev, dev_spec_op)(dev, gxdso_is_encoding_direct, NULL, 0);
            data->set_color = cmapper_vanilla;
            data->direct = (code == 1);
        }
    }
}

/* This is used by image color render to handle the cases where we need to
   perform either a transfer function or halftoning on the color values
   during an ICC color flow.  In this case, the color is already in the
   device color space but in 16bpp color values. */
void
cmap_transfer_halftone(gx_color_value *pconc, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, bool has_transfer,
     bool has_halftone, gs_color_select_t select)
{
    uchar nc, ncomps = dev->color_info.num_components;
    frac frac_value;
    uchar i;
    frac cv_frac[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_color_value color_val[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* apply the transfer function(s) */
    nc = ncomps;
    if (device_encodes_tags(dev))
        nc--;
    if (has_transfer) {
        if (pgs->effective_transfer_non_identity_count == 0) {
            for (i = 0; i < nc; i++)
                cv_frac[i] = cv2frac(pconc[i]);
        } else if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE) {
            for (i = 0; i < nc; i++) {
                frac_value = cv2frac(pconc[i]);
                cv_frac[i] = gx_map_color_frac(pgs,
                                    frac_value, effective_transfer[i]);
            }
        } else {
            if (gx_get_opmsupported(dev) == GX_CINFO_OPMSUPPORTED) {  /* CMYK-like color space */
                uint k = dev->color_info.black_component;
                for (i = 0; i < nc; i++) {
                    frac_value = cv2frac(pconc[i]);
                    if (i == k) {
                        cv_frac[i] = frac_1 - gx_map_color_frac(pgs,
                            (frac)(frac_1 - frac_value), effective_transfer[i]);
                    } else {
                        cv_frac[i] = cv2frac(pconc[i]);  /* Ignore transfer, see PLRM3 p. 494 */
                    }
                }
            } else {
                for (i = 0; i < nc; i++) {
                    frac_value = cv2frac(pconc[i]);
                    cv_frac[i] = frac_1 - gx_map_color_frac(pgs,
                                (frac)(frac_1 - frac_value), effective_transfer[i]);
                }
            }
        }
        if (nc < ncomps)
            cv_frac[nc] = pconc[nc];
    } else {
        if (has_halftone) {
            /* We need this to be in frac form */
            for (i = 0; i < nc; i++) {
                cv_frac[i] = cv2frac(pconc[i]);
            }
            if (nc < ncomps)
                cv_frac[nc] = pconc[nc];
        }
    }
    /* Halftoning */
    if (has_halftone) {
        if (gx_render_device_DeviceN(&(cv_frac[0]), pdc, dev,
                    gx_select_dev_ht(pgs), &pgs->screen_phase[select]) == 1)
            gx_color_load_select(pdc, pgs, dev, select);
    } else {
        /* We have a frac value from the transfer function.  Do the encode.
           which does not take a frac value...  */
        for (i = 0; i < nc; i++) {
            color_val[i] = frac2cv(cv_frac[i]);
        }
        if (i < ncomps)
            color_val[i] = cv_frac[i];
        color = dev_proc(dev, encode_color)(dev, &(color_val[0]));
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
            color_set_pure(pdc, color);
    }
}

/* This is used by image color render to apply only the transfer function.
   We follow this up with threshold rendering. */
void
cmap_transfer(gx_color_value *pconc, const gs_gstate * pgs, gx_device * dev)
{
    uchar ncomps = dev->color_info.num_components;
    uchar i;

    /* apply the transfer function(s) */
    if (device_encodes_tags(dev))
        ncomps--;
    if (pgs->effective_transfer_non_identity_count == 0) {
        /* No transfer function to apply */
    } else if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        for (i = 0; i < ncomps; i++)
            pconc[i] = frac2cv(gx_map_color_frac(pgs,
                               cv2frac(pconc[i]), effective_transfer[i]));
    else {
        if (gx_get_opmsupported(dev) == GX_CINFO_OPMSUPPORTED) {  /* CMYK-like color space */
            i = dev->color_info.black_component;
            if (i < ncomps)
                pconc[i] = frac2cv(frac_1 - gx_map_color_frac(pgs,
                                   (frac)(frac_1 - cv2frac(pconc[i])), effective_transfer[i]));
        } else {
            for (i = 0; i < ncomps; i++)
                pconc[i] = frac2cv(frac_1 - gx_map_color_frac(pgs,
                        (frac)(frac_1 - cv2frac(pconc[i])), effective_transfer[i]));
        }
    }
}

/* A planar version which applies only one transfer function */
void
cmap_transfer_plane(gx_color_value *pconc, const gs_gstate *pgs,
                    gx_device *dev, int plane)
{
    frac frac_value;
    frac cv_frac;

    /* apply the transfer function(s) */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE) {
        frac_value = cv2frac(pconc[0]);
        cv_frac = gx_map_color_frac(pgs, frac_value, effective_transfer[plane]);
        pconc[0] = frac2cv(cv_frac);
    } else {
        if (gx_get_opmsupported(dev) == GX_CINFO_OPMSUPPORTED) {  /* CMYK-like color space */
            uint k = dev->color_info.black_component;
            if (plane == k) {
                frac_value = cv2frac(pconc[0]);
                cv_frac = frac_1 - gx_map_color_frac(pgs,
                (frac)(frac_1 - frac_value), effective_transfer[plane]);
                pconc[0] = frac2cv(cv_frac);
            }
        } else {
            frac_value = cv2frac(pconc[0]);
            cv_frac = frac_1 - gx_map_color_frac(pgs,
                    (frac)(frac_1 - frac_value), effective_transfer[plane]);
            pconc[0] = frac2cv(cv_frac);
        }
    }
}


bool
gx_device_uses_std_cmap_procs(gx_device * dev, const gs_gstate * pgs)
{
    const gx_cm_color_map_procs *pprocs;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile = NULL;
    cmm_profile_t *des_profile = NULL;

    dev_proc(dev, get_profile)(dev,  &dev_profile);
    gsicc_extract_profile(dev->graphics_type_tag,
                          dev_profile, &des_profile, &render_cond);

    if (des_profile != NULL) {
        const gx_device *cmdev;

        pprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);
        /* Check if they are forwarding procs */
        switch(des_profile->num_comps) {
            case 1:
                if (pprocs == &DeviceGray_procs) {
                    return true;
                }
                break;
            case 3:
                if (pprocs == &DeviceRGB_procs) {
                    return true;
                }
                break;
            case 4:
                if (pprocs == &DeviceCMYK_procs) {
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return false;
}
