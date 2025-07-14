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


/* Gimp (XCF) export device, supporting DeviceN color models. */

#include "math_.h"
#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"
#include "gdevdcrd.h"
#include "gstypes.h"
#include "gxdcconv.h"
#include "gsicc_cache.h"
#include "gsicc_manage.h"
#include "gsicc_cms.h"
#include "gdevdevn.h"

#ifndef MAX_CHAN
#   define MAX_CHAN 8
#endif

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_get_params(xcf_get_params);
static dev_proc_close_device(xcf_prn_close);
static dev_proc_put_params(xcf_put_params);
static dev_proc_print_page(xcf_print_page);
static dev_proc_map_color_rgb(xcf_map_color_rgb);
static dev_proc_get_color_mapping_procs(get_spotrgb_color_mapping_procs);
#if 0
static dev_proc_get_color_mapping_procs(get_spotcmyk_color_mapping_procs);
#endif
static dev_proc_get_color_mapping_procs(get_xcf_color_mapping_procs);
static dev_proc_get_color_comp_index(xcf_get_color_comp_index);
static dev_proc_encode_color(xcf_encode_color);
static dev_proc_decode_color(xcf_decode_color);

/*
 * Structure for holding SeparationNames and SeparationOrder elements.
 */
typedef struct gs_separation_names_s {
    int num_names;
    const gs_param_string * names[GX_DEVICE_COLOR_MAX_COMPONENTS];
} gs_separation_names;

/* This is redundant with color_info.cm_name. We may eliminate this
   typedef and use the latter string for everything. */
typedef enum {
    XCF_DEVICE_GRAY,
    XCF_DEVICE_RGB,
    XCF_DEVICE_CMYK,
    XCF_DEVICE_N
} xcf_color_model;

/*
 * A structure definition for a DeviceN type device
 */
typedef struct xcf_device_s {
    gx_device_common;
    gx_prn_device_common;

    /*        ... device-specific parameters ... */

    xcf_color_model color_model;

    /*
     * Bits per component (device colorant).  Currently only 1 and 8 are
     * supported.
     */
    int bitspercomponent;

    /*
     * Pointer to the colorant names for the color model.  This will be
     * null if we have DeviceN type device.  The actual possible colorant
     * names are those in this list plus those in the separation_names
     * list (below).
     */
    fixed_colorant_names_list std_colorant_names;
    int num_std_colorant_names;	/* Number of names in list */

    /*
    * Separation names (if any).
    */
    gs_separation_names separation_names;

    /*
     * Separation Order (if specified).
     */
    gs_separation_names separation_order;

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

} xcf_device;

/*
 * Macro definition for DeviceN procedures
 */
static void
xcf_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, open_device, gdev_prn_open);
    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
    set_dev_proc(dev, close_device, xcf_prn_close);
    set_dev_proc(dev, map_color_rgb, xcf_map_color_rgb);
    set_dev_proc(dev, get_params, xcf_get_params);
    set_dev_proc(dev, put_params, xcf_put_params);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
    set_dev_proc(dev, get_color_comp_index, xcf_get_color_comp_index);
    set_dev_proc(dev, encode_color, xcf_encode_color);
    set_dev_proc(dev, decode_color, xcf_decode_color);
}

static void
spot_rgb_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, get_color_mapping_procs, get_spotrgb_color_mapping_procs);

    xcf_initialize_device_procs(dev);
}

/*
 * Example device with RGB and spot color support
 */
const xcf_device gs_xcf_device =
{
    prn_device_body_extended(xcf_device,
         spot_rgb_initialize_device_procs, "xcf",
         DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
         X_DPI, Y_DPI,		/* X and Y hardware resolution */
         0, 0, 0, 0,		/* margins */
         GX_DEVICE_COLOR_MAX_COMPONENTS, 3,	/* MaxComponents, NumComp */
         GX_CINFO_POLARITY_ADDITIVE,		/* Polarity */
         24, 0,			/* Depth, Gray_index, */
         255, 255, 256, 256,	/* MaxGray, MaxColor, DitherGray, DitherColor */
         GX_CINFO_UNKNOWN_SEP_LIN, /* Let check_device_separable set up values */
         "DeviceN",		/* Process color model name */
         xcf_print_page),	/* Printer page print routine */
    /* DeviceN device specific parameters */
    XCF_DEVICE_RGB,		/* Color model */
    8,				/* Bits per color - must match ncomp, depth, etc. above */
    DeviceRGBComponents,	/* Names of color model colorants */
    3,				/* Number colorants for RGB */
    {0},			/* SeparationNames */
    {0}				/* SeparationOrder names */
};

static void
spot_cmyk_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, get_color_mapping_procs, get_xcf_color_mapping_procs);

    xcf_initialize_device_procs(dev);
}

const xcf_device gs_xcfcmyk_device =
{
    prn_device_body_extended(xcf_device,
         spot_cmyk_initialize_device_procs, "xcfcmyk",
         DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
         X_DPI, Y_DPI,		/* X and Y hardware resolution */
         0, 0, 0, 0,		/* margins */
         GX_DEVICE_COLOR_MAX_COMPONENTS, 4,	/* MaxComponents, NumComp */
         GX_CINFO_POLARITY_SUBTRACTIVE,		/* Polarity */
         32, 0,			/* Depth, Gray_index, */
         255, 255, 256, 256,	/* MaxGray, MaxColor, DitherGray, DitherColor */
         GX_CINFO_UNKNOWN_SEP_LIN, /* Let check_device_separable set up values */
         "DeviceN",		/* Process color model name */
         xcf_print_page),	/* Printer page print routine */
    /* DeviceN device specific parameters */
    XCF_DEVICE_CMYK,		/* Color model */
    8,				/* Bits per color - must match ncomp, depth, etc. above */
    DeviceCMYKComponents,	/* Names of color model colorants */
    4,				/* Number colorants for RGB */
    {0},			/* SeparationNames */
    {0}				/* SeparationOrder names */
};

/*
 * The following procedures are used to map the standard color spaces into
 * the color components for the spotrgb device.
 */
static void
gray_cs_to_spotrgb_cm(const gx_device * dev, frac gray, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((xcf_device *)dev)->separation_names.num_names;

    out[0] = out[1] = out[2] = gray;
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
}

static void
rgb_cs_to_spotrgb_cm(const gx_device * dev, const gs_gstate *pgs,
                                  frac r, frac g, frac b, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((xcf_device *)dev)->separation_names.num_names;

    out[0] = r;
    out[1] = g;
    out[2] = b;
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
}

static void
cmyk_cs_to_spotrgb_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((xcf_device *)dev)->separation_names.num_names;

    color_cmyk_to_rgb(c, m, y, k, NULL, out, dev->memory);
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
}

static void
gray_cs_to_spotcmyk_cm(const gx_device * dev, frac gray, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((xcf_device *)dev)->separation_names.num_names;

    out[0] = out[1] = out[2] = 0;
    out[3] = frac_1 - gray;
    for(; i>0; i--)			/* Clear spot colors */
        out[3 + i] = 0;
}

static void
rgb_cs_to_spotcmyk_cm(const gx_device * dev, const gs_gstate *pgs,
                                   frac r, frac g, frac b, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    xcf_device *xdev = (xcf_device *)dev;
    int n = xdev->separation_names.num_names;
    int i;

    color_rgb_to_cmyk(r, g, b, pgs, out, dev->memory);
    for(i = 0; i < n; i++)			/* Clear spot colors */
        out[4 + i] = 0;
}

static void
cmyk_cs_to_spotcmyk_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    xcf_device *xdev = (xcf_device *)dev;
    int n = xdev->separation_names.num_names;
    int i;

    out[0] = c;
    out[1] = m;
    out[2] = y;
    out[3] = k;
    for(i = 0; i < n; i++)			/* Clear spot colors */
        out[4 + i] = 0;
}

static void
cmyk_cs_to_spotn_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    xcf_device *xdev = (xcf_device *)dev;
    int n = xdev->separation_names.num_names;

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

        gscms_transform_color_const(dev, link, &(in[0]), &(tmp[0]), 2);
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
gray_cs_to_spotn_cm(const gx_device * dev, frac gray, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */

    cmyk_cs_to_spotn_cm(dev, 0, 0, 0, frac_1 - gray, out);
}

static void
rgb_cs_to_spotn_cm(const gx_device * dev, const gs_gstate *pgs,
                                   frac r, frac g, frac b, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    xcf_device *xdev = (xcf_device *)dev;
    int n = xdev->separation_names.num_names;
    gcmmhlink_t link = xdev->rgb_icc_link;
    int i;

    if (link != NULL) {
        unsigned short in[3];
        unsigned short tmp[MAX_CHAN];
        int outn = xdev->rgb_profile->num_comps_out;

        in[0] = frac2ushort(r);
        in[1] = frac2ushort(g);
        in[2] = frac2ushort(b);

        gscms_transform_color_const(dev, link, &(in[0]), &(tmp[0]), 2);

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

static const gx_cm_color_map_procs spotRGB_procs = {
    gray_cs_to_spotrgb_cm, rgb_cs_to_spotrgb_cm, cmyk_cs_to_spotrgb_cm
};

static const gx_cm_color_map_procs spotCMYK_procs = {
    gray_cs_to_spotcmyk_cm, rgb_cs_to_spotcmyk_cm, cmyk_cs_to_spotcmyk_cm
};

static const gx_cm_color_map_procs spotN_procs = {
    gray_cs_to_spotn_cm, rgb_cs_to_spotn_cm, cmyk_cs_to_spotn_cm
};

/*
 * These are the handlers for returning the list of color space
 * to color model conversion routines.
 */
static const gx_cm_color_map_procs *
get_spotrgb_color_mapping_procs(const gx_device * dev, const gx_device **tdev)
{
    *tdev = dev;
    return &spotRGB_procs;
}

#if 0
static const gx_cm_color_map_procs *
get_spotcmyk_color_mapping_procs(const gx_device * dev, const gx_device **tdev)
{
    *tdev = dev;
    return &spotCMYK_procs;
}
#endif

static const gx_cm_color_map_procs *
get_xcf_color_mapping_procs(const gx_device * dev, const gx_device **tdev)
{
    const xcf_device *xdev = (const xcf_device *)dev;

    *tdev = dev;
    if (xdev->color_model == XCF_DEVICE_RGB)
        return &spotRGB_procs;
    else if (xdev->color_model == XCF_DEVICE_CMYK)
        return &spotCMYK_procs;
    else if (xdev->color_model == XCF_DEVICE_N)
        return &spotN_procs;
    else
        return NULL;
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
 */
static gx_color_index
xcf_encode_color(gx_device *dev, const gx_color_value colors[])
{
    int bpc = ((xcf_device *)dev)->bitspercomponent;
    gx_color_index color = 0;
    int i = 0;
    int ncomp = dev->color_info.num_components;
    COLROUND_VARS;

    COLROUND_SETUP(bpc);
    for (; i<ncomp; i++) {
        color <<= bpc;
        color |= COLROUND_ROUND(colors[i]);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 */
static int
xcf_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    int bpc = ((xcf_device *)dev)->bitspercomponent;
    int mask = (1 << bpc) - 1;
    int i = 0;
    int ncomp = dev->color_info.num_components;
    COLDUP_VARS;

    COLDUP_SETUP(bpc);
    for (; i<ncomp; i++) {
        out[ncomp - i - 1] = COLDUP_DUP(color & mask);
        color >>= bpc;
    }
    return 0;
}

/*
 * Convert a gx_color_index to RGB.
 */
static int
xcf_map_color_rgb(gx_device *dev, gx_color_index color, gx_color_value rgb[3])
{
    xcf_device *xdev = (xcf_device *)dev;

    if (xdev->color_model == XCF_DEVICE_RGB)
        return xcf_decode_color(dev, color, rgb);
    /* TODO: return reasonable values. */
    rgb[0] = 0;
    rgb[1] = 0;
    rgb[2] = 0;
    return 0;
}

/*
 * This routine will extract a specified set of bits from a buffer and pack
 * them into a given buffer.
 *
 * Parameters:
 *   source - The source of the data
 *   dest - The destination for the data
 *   depth - The size of the bits per pixel - must be a multiple of 8
 *   first_bit - The location of the first data bit (LSB).
 *   bit_width - The number of bits to be extracted.
 *   npixel - The number of pixels.
 *
 * Returns:
 *   Length of the output line (in bytes)
 *   Data in dest.
 */
#if 0
static int
repack_data(byte * source, byte * dest, int depth, int first_bit,
                int bit_width, int npixel)
{
    int in_nbyte = depth >> 3;		/* Number of bytes per input pixel */
    int out_nbyte = bit_width >> 3;	/* Number of bytes per output pixel */
    gx_color_index mask = 1;
    gx_color_index data;
    int i, j, length = 0;
    int in_byte_loc = 0, out_byte_loc = 0;
    byte temp;
    byte * out = dest;
    int max_bit_byte = 8 - bit_width;

    mask = (mask << bit_width) - 1;
    for (i=0; i<npixel; i++) {
        /* Get the pixel data */
        if (!in_nbyte) {		/* Multiple pixels per byte */
            data = *source;
            data >>= in_byte_loc;
            in_byte_loc += depth;
            if (in_byte_loc >= 8) {	/* If finished with byte */
                in_byte_loc = 0;
                source++;
            }
        }
        else {				/* One or more bytes per pixel */
            data = *source++;
            for (j=1; j<in_nbyte; j++)
                data = (data << 8) + *source++;
        }
        data >>= first_bit;
        data &= mask;

        /* Put the output data */
        if (!out_nbyte) {		/* Multiple pixels per byte */
            temp = *out & ~(mask << out_byte_loc);
            *out = temp | (data << out_byte_loc);
            out_byte_loc += bit_width;
            if (out_byte_loc > max_bit_byte) {	/* If finished with byte */
                out_byte_loc = 0;
                out++;
            }
        }
        else {				/* One or more bytes per pixel */
            *out++ = data >> ((out_nbyte - 1) * 8);
            for (j=1; j<out_nbyte; j++) {
                *out++ = data >> ((out_nbyte - 1 - j) * 8);
            }
        }
    }
    /* Return the number of bytes in the destination buffer. */
    length = out - dest;
    if (out_byte_loc)		 	/* If partially filled last byte */
        length++;
    return length;
}
#endif /* 0 */

static int
xcf_open_profile(const char *profile_out_fn, cmm_profile_t *icc_profile, gcmmhlink_t icc_link, gs_memory_t *memory)
{

    gsicc_rendering_param_t rendering_params;

    icc_profile = gsicc_get_profile_handle_file(profile_out_fn,
                    strlen(profile_out_fn), memory);

    if (icc_profile == NULL)
        return gs_throw(-1, "Could not create profile for xcf device");

    /* Set up the rendering parameters */

    rendering_params.black_point_comp = gsBPNOTSPECIFIED;
    rendering_params.graphics_type_tag = GS_UNKNOWN_TAG;  /* Already rendered */
    rendering_params.rendering_intent = gsPERCEPTUAL;

    /* Call with a NULL destination profile since we are using a device link profile here */
    icc_link = gscms_get_link(icc_profile,
                              NULL, &rendering_params, 0, memory);

    if (icc_link == NULL)
        return gs_throw(-1, "Could not create link handle for xdev device");

    return(0);

}

static int
xcf_open_profiles(xcf_device *xdev)
{
    int code = 0;

    if (xdev->output_icc_link == NULL && xdev->profile_out_fn[0]) {

        code = xcf_open_profile(xdev->profile_out_fn, xdev->output_profile,
            xdev->output_icc_link, xdev->memory);

    }

    if (code >= 0 && xdev->rgb_icc_link == NULL && xdev->profile_rgb_fn[0]) {

        code = xcf_open_profile(xdev->profile_rgb_fn, xdev->rgb_profile,
            xdev->rgb_icc_link, xdev->memory);

    }

    if (code >= 0 && xdev->cmyk_icc_link == NULL && xdev->profile_cmyk_fn[0]) {

        code = xcf_open_profile(xdev->profile_cmyk_fn, xdev->cmyk_profile,
            xdev->cmyk_icc_link, xdev->memory);

    }

    return code;
}

#define set_param_array(a, d, s)\
  (a.data = d, a.size = s, a.persistent = false);

/* Get parameters.  We provide a default CRD. */
static int
xcf_get_params(gx_device * pdev, gs_param_list * plist)
{
    xcf_device *xdev = (xcf_device *)pdev;
    int code;
    bool seprs = false;
    gs_param_string_array scna;
    gs_param_string pos;
    gs_param_string prgbs;
    gs_param_string pcmyks;

    set_param_array(scna, NULL, 0);

    if ( (code = gdev_prn_get_params(pdev, plist)) < 0 ||
         (code = sample_device_crd_get_params(pdev, plist, "CRDDefault")) < 0 ||
         (code = param_write_name_array(plist, "SeparationColorNames", &scna)) < 0 ||
         (code = param_write_bool(plist, "Separations", &seprs)) < 0)
        return code;

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
    code = param_write_string(plist, "ProfileCmyk", &pcmyks);

    return code;
}
#undef set_param_array

#define compare_color_names(name, name_size, str, str_size) \
    (name_size == str_size && \
        (strncmp((const char *)name, (const char *)str, name_size) == 0))

/*
 * This routine will check if a name matches any item in a list of process model
 * color component names.
 */
static bool
check_process_color_names(fixed_colorant_names_list plist,
                          const gs_param_string * pstring)
{
    if (plist) {
        uint size = pstring->size;

        while( *plist) {
            if (compare_color_names(*plist, strlen(*plist), pstring->data, size)) {
                return true;
            }
            plist++;
        }
    }
    return false;
}

#define BEGIN_ARRAY_PARAM(pread, pname, pa, psize, e)\
    BEGIN\
    switch (code = pread(plist, (param_name = pname), &(pa))) {\
      case 0:\
        if ((pa).size != psize) {\
          ecode = gs_note_error(gs_error_rangecheck);\
          (pa).data = 0;	/* mark as not filled */\
        } else
#define END_ARRAY_PARAM(pa, e)\
        goto e;\
      default:\
        ecode = code;\
e:	param_signal_error(plist, param_name, ecode);\
      case 1:\
        (pa).data = 0;		/* mark as not filled */\
    }\
    END

static int
xcf_param_read_fn(gs_param_list *plist, const char *name,
                  gs_param_string *pstr, int max_len)
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

/* Compare a C string and a gs_param_string. */
static bool
param_string_eq(const gs_param_string *pcs, const char *str)
{
    return (strlen(str) == pcs->size &&
            !strncmp(str, (const char *)pcs->data, pcs->size));
}

static int
xcf_set_color_model(xcf_device *xdev, xcf_color_model color_model)
{
    xdev->color_model = color_model;
    if (color_model == XCF_DEVICE_GRAY) {
        xdev->std_colorant_names = DeviceGrayComponents;
        xdev->num_std_colorant_names = 1;
        xdev->color_info.cm_name = "DeviceGray";
        xdev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
    } else if (color_model == XCF_DEVICE_RGB) {
        xdev->std_colorant_names = DeviceRGBComponents;
        xdev->num_std_colorant_names = 3;
        xdev->color_info.cm_name = "DeviceRGB";
        xdev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
    } else if (color_model == XCF_DEVICE_CMYK) {
        xdev->std_colorant_names = DeviceCMYKComponents;
        xdev->num_std_colorant_names = 4;
        xdev->color_info.cm_name = "DeviceCMYK";
        xdev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else if (color_model == XCF_DEVICE_N) {
        xdev->std_colorant_names = DeviceCMYKComponents;
        xdev->num_std_colorant_names = 4;
        xdev->color_info.cm_name = "DeviceN";
        xdev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else {
        return -1;
    }

    return 0;
}

/*
 * Close device and clean up ICC structures.
 */

static int
xcf_prn_close(gx_device *dev)
{
    xcf_device * const xdev = (xcf_device *) dev;
    int i;

    if (xdev->cmyk_icc_link != NULL) {
        gscms_release_link(xdev->cmyk_icc_link);
        rc_decrement(xdev->cmyk_profile, "xcf_prn_close");
    }

    if (xdev->rgb_icc_link != NULL) {
        gscms_release_link(xdev->rgb_icc_link);
        rc_decrement(xdev->rgb_profile, "xcf_prn_close");
    }

    if (xdev->output_icc_link != NULL) {
        gscms_release_link(xdev->output_icc_link);
        rc_decrement(xdev->output_profile, "xcf_prn_close");
    }

    /* Free all the colour separation names */
    for (i = 0; i < xdev->separation_names.num_names; i++) {
        if (xdev->separation_names.names[i] != NULL) {
            gs_free_object(xdev->memory->non_gc_memory, (void *)xdev->separation_names.names[i]->data, "devicen_put_params_no_sep_order");
            gs_free_object(xdev->memory->non_gc_memory, (void *)xdev->separation_names.names[i], "devicen_put_params_no_sep_order");
        }
        xdev->separation_names.names[i] = NULL;
    }
    xdev->separation_names.num_names = 0;

    return gdev_prn_close(dev);
}

/* Set parameters.  We allow setting the number of bits per component. */
static int
xcf_put_params(gx_device * pdev, gs_param_list * plist)
{
    xcf_device * const pdevn = (xcf_device *) pdev;
    gx_device_color_info save_info;
    gs_param_name param_name;
    int npcmcolors;
    int num_spot = pdevn->separation_names.num_names;
    int ecode = 0;
    int code;
    gs_param_string_array scna;
    gs_param_string po;
    gs_param_string prgb;
    gs_param_string pcmyk;
    gs_param_string pcm;
    xcf_color_model color_model = pdevn->color_model;

    BEGIN_ARRAY_PARAM(param_read_name_array, "SeparationColorNames", scna, scna.size, scne) {
        break;
    } END_ARRAY_PARAM(scna, scne);

    if (code >= 0)
        code = xcf_param_read_fn(plist, "ProfileOut", &po,
                                 sizeof(pdevn->profile_out_fn));
    if (code >= 0)
        code = xcf_param_read_fn(plist, "ProfileRgb", &prgb,
                                 sizeof(pdevn->profile_rgb_fn));
    if (code >= 0)
        code = xcf_param_read_fn(plist, "ProfileCmyk", &pcmyk,
                                 sizeof(pdevn->profile_cmyk_fn));

    if (code >= 0)
        code = param_read_name(plist, "ProcessColorModel", &pcm);
    if (code == 0) {
        if (param_string_eq (&pcm, "DeviceGray"))
            color_model = XCF_DEVICE_GRAY;
        else if (param_string_eq (&pcm, "DeviceRGB"))
            color_model = XCF_DEVICE_RGB;
        else if (param_string_eq (&pcm, "DeviceCMYK"))
            color_model = XCF_DEVICE_CMYK;
        else if (param_string_eq (&pcm, "DeviceN"))
            color_model = XCF_DEVICE_N;
        else {
            param_signal_error(plist, "ProcessColorModel",
                               code = gs_error_rangecheck);
        }
    }
    if (code < 0)
        return code;

    /*
     * Save the color_info in case gdev_prn_put_params fails, and for
     * comparison.
     */
    save_info = pdevn->color_info;
    ecode = xcf_set_color_model(pdevn, color_model);
    if (ecode == 0)
        ecode = gdev_prn_put_params(pdev, plist);
    if (ecode < 0) {
        pdevn->color_info = save_info;
        return ecode;
    }

    /* Separations are only valid with a subrtractive color model */
    if (pdev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE) {
        /*
         * Process the separation color names.  Remove any names that already
         * match the process color model colorant names for the device.
         */
        if (scna.data != 0) {
            int num_names = scna.size, i = 0;
            fixed_colorant_names_list pcomp_names = pdevn->std_colorant_names;

            if (num_spot + num_names > pdev->color_info.max_components) {
                param_signal_error(plist, "SeparationColorNames", gs_error_rangecheck);
                return_error(gs_error_rangecheck);
            }
            for (i = 0; i < num_names; i++) {
                /* Verify that the name is not one of our process colorants */
                if (!check_process_color_names(pcomp_names, &scna.data[i])) {
                    byte * sep_name;
                    int name_size = scna.data[i].size;
                    gs_param_string *new_string;

                    new_string = (gs_param_string *)gs_alloc_bytes(pdev->memory->non_gc_memory, sizeof(gs_param_string), "devicen_put_params_no_sep_order");
                    if (new_string == NULL) {
                        param_signal_error(plist, "SeparationColorNames", gs_error_VMerror);
                        return_error(gs_error_VMerror);
                    }
                    /* We have a new separation */
                    sep_name = (byte *)gs_alloc_bytes(pdev->memory->non_gc_memory,
                        name_size, "devicen_put_params_no_sep_order");
                    if (sep_name == NULL) {
                        gs_free_object(pdev->memory, new_string, "devicen_put_params_no_sep_order");
                        param_signal_error(plist, "SeparationColorNames", gs_error_VMerror);
                        return_error(gs_error_VMerror);
                    }
                    memcpy(sep_name, scna.data[i].data, name_size);
                    new_string->size = name_size;
                    new_string->data = sep_name;
                    new_string->persistent = true;
                    if (pdevn->separation_names.names[num_spot] != NULL) {
                        gs_free_object(pdev->memory->non_gc_memory, (void *)pdevn->separation_names.names[num_spot]->data, "devicen_put_params_no_sep_order");
                        gs_free_object(pdev->memory->non_gc_memory, (void *)pdevn->separation_names.names[num_spot], "devicen_put_params_no_sep_order");
                    }
                    pdevn->separation_names.names[num_spot] = new_string;

                    num_spot++;
                }
            }
            pdevn->separation_names.num_names = num_spot;
        }

        npcmcolors = pdevn->num_std_colorant_names;
        pdevn->color_info.num_components = npcmcolors + num_spot;

        if (pdevn->color_info.num_components > pdevn->color_info.max_components)
            pdevn->color_info.num_components = pdevn->color_info.max_components;
        /*
         * The DeviceN device can have zero components if nothing has been
         * specified.  This causes some problems so force at least one
         * component until something is specified.
         */
        if (!pdevn->color_info.num_components)
            pdevn->color_info.num_components = 1;
        pdevn->color_info.depth = bpc_to_depth(pdevn->color_info.num_components,
                                                pdevn->bitspercomponent);
        if (pdevn->color_info.depth != save_info.depth) {
            gs_closedevice(pdev);
        }
    }

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
    code = xcf_open_profiles(pdevn);

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
xcf_get_color_comp_index(gx_device * dev, const char * pname, int name_size,
                                        int component_type)
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    fixed_colorant_name * pcolor = ((const xcf_device *)dev)->std_colorant_names;
    int color_component_number = 0;
    int i;

    /* Check if the component is in the implied list. */
    if (pcolor) {
        while( *pcolor) {
            if (compare_color_names(pname, name_size, *pcolor, strlen(*pcolor)))
                return color_component_number;
            pcolor++;
            color_component_number++;
        }
    }

    /* Check if the component is in the separation names list. */
    {
        const gs_separation_names * separations = &((const xcf_device *)dev)->separation_names;
        int num_spot = separations->num_names;

        for (i=0; i<num_spot; i++) {
            if (compare_color_names((const char *)separations->names[i]->data,
                  separations->names[i]->size, pname, name_size)) {
                return color_component_number;
            }
            color_component_number++;
        }
    }

    return -1;
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

typedef struct {
    gp_file *f;
    int offset;

    int width;
    int height;
    int base_bytes_pp; /* almost always 3 (rgb) */
    int n_extra_channels;

    int n_tiles_x;
    int n_tiles_y;
    int n_tiles;
    int n_levels;

    /* byte offset of image data */
    int image_data_off;
} xcf_write_ctx;

#define TILE_WIDTH 64
#define TILE_HEIGHT 64

static int
xcf_calc_levels(int size, int tile_size)
{
    int levels = 1;
    while (size > tile_size) {
        size >>= 1;
        levels++;
    }
    return levels;
}

static int
xcf_setup_tiles(xcf_write_ctx *xc, xcf_device *dev)
{
    xc->base_bytes_pp = 3;
    xc->n_extra_channels = dev->separation_names.num_names;
    xc->width = dev->width;
    xc->height = dev->height;
    xc->n_tiles_x = (dev->width + TILE_WIDTH - 1) / TILE_WIDTH;
    xc->n_tiles_y = (dev->height + TILE_HEIGHT - 1) / TILE_HEIGHT;
    xc->n_tiles = xc->n_tiles_x * xc->n_tiles_y;
    xc->n_levels = max(xcf_calc_levels(dev->width, TILE_WIDTH),
                       xcf_calc_levels(dev->height, TILE_HEIGHT));

    return 0;
}

/* Return value: Size of tile in pixels. */
static int
xcf_tile_sizeof(xcf_write_ctx *xc, int tile_idx)
{
    int tile_i = tile_idx % xc->n_tiles_x;
    int tile_j = tile_idx / xc->n_tiles_x;
    int tile_size_x = min(TILE_WIDTH, xc->width - tile_i * TILE_WIDTH);
    int tile_size_y = min(TILE_HEIGHT, xc->height - tile_j * TILE_HEIGHT);
    return tile_size_x * tile_size_y;
}

static int
xcf_write(xcf_write_ctx *xc, const byte *buf, int size) {
    int code;

    code = gp_fwrite(buf, 1, size, xc->f);
    if (code < 0)
        return code;
    xc->offset += code;
    return 0;
}

static int
xcf_write_32(xcf_write_ctx *xc, bits32 v)
{
    bits32 buf;

    assign_u32(buf, v);
    return xcf_write(xc, (byte *)&buf, 4);
}

static int
xcf_write_image_props(xcf_write_ctx *xc)
{
    int code = 0;

    xcf_write_32(xc, 0);
    xcf_write_32(xc, 0);

    return code;
}

/**
 * Return value: Number of bytes needed to write layer.
 **/
static int
xcf_base_size(xcf_write_ctx *xc, const char *layer_name)
{
    int bytes_pp = xc->base_bytes_pp + xc->n_extra_channels;

    return 17 + strlen (layer_name) +		/* header and name */
        8 +					/* layer props */
        12 + xc->n_levels * 16 +		/* layer tile hierarchy */
        12 + xc->n_tiles * 4 +			/* tile offsets */
        xc->width * xc->height * bytes_pp;	/* image data */
}

static int
xcf_channel_size(xcf_write_ctx *xc, int name_size)
{
    return 17 + name_size +			/* header and name */
        8 +					/* channel props */
        4 + xc->n_levels * 16 +			/* channel tile hiearchy */
        12 + xc->n_tiles * 4;			/* tile offsets */
}

static int
xcf_write_header(xcf_write_ctx *xc, xcf_device *pdev)
{
    int code = 0;
    const char *layer_name = "Background";
    int level;
    int tile_offset;
    int tile_idx;
    int n_extra_channels = xc->n_extra_channels;
    int bytes_pp = xc->base_bytes_pp + n_extra_channels;
    int channel_idx;

    xcf_write(xc, (const byte *)"gimp xcf file", 14);
    xcf_write_32(xc, xc->width);
    xcf_write_32(xc, xc->height);
    xcf_write_32(xc, 0);

    xcf_write_image_props(xc);

    /* layer offsets */
    xcf_write_32(xc, xc->offset + 12 + 4 * n_extra_channels);
    xcf_write_32(xc, 0);

    /* channel offsets */
    tile_offset = xc->offset + 4 + 4 * n_extra_channels +
        xcf_base_size(xc, layer_name);
    for (channel_idx = 0; channel_idx < n_extra_channels; channel_idx++) {
        const gs_param_string *separation_name =
            pdev->separation_names.names[channel_idx];
        dmlprintf1(pdev->memory, "tile offset: %d\n", tile_offset);
        xcf_write_32(xc, tile_offset);
        tile_offset += xcf_channel_size(xc, separation_name->size);
    }
    xcf_write_32(xc, 0);

    /* layer */
    xcf_write_32(xc, xc->width);
    xcf_write_32(xc, xc->height);
    xcf_write_32(xc, 0);
    xcf_write_32(xc, strlen(layer_name) + 1);
    xcf_write(xc, (const byte *)layer_name, strlen(layer_name) + 1);

    /* layer props */
    xcf_write_32(xc, 0);
    xcf_write_32(xc, 0);

    /* layer tile hierarchy */
    xcf_write_32(xc, xc->offset + 8);
    xcf_write_32(xc, 0);

    xcf_write_32(xc, xc->width);
    xcf_write_32(xc, xc->height);
    xcf_write_32(xc, xc->base_bytes_pp);
    xcf_write_32(xc, xc->offset + (1 + xc->n_levels) * 4);
    tile_offset = xc->offset + xc->width * xc->height * bytes_pp +
        xc->n_tiles * 4 + 12;
    for (level = 1; level < xc->n_levels; level++) {
        xcf_write_32(xc, tile_offset);
        tile_offset += 12;
    }
    xcf_write_32(xc, 0);

    /* layer tile offsets */
    xcf_write_32(xc, xc->width);
    xcf_write_32(xc, xc->height);
    tile_offset = xc->offset + (xc->n_tiles + 1) * 4;
    for (tile_idx = 0; tile_idx < xc->n_tiles; tile_idx++) {
        xcf_write_32(xc, tile_offset);
        tile_offset += xcf_tile_sizeof(xc, tile_idx) * bytes_pp;
    }
    xcf_write_32(xc, 0);

    xc->image_data_off = xc->offset;

    return code;
}

static void
xcf_shuffle_to_tile(xcf_write_ctx *xc, byte **tile_data, const byte *row,
                    int y)
{
    int tile_j = y / TILE_HEIGHT;
    int yrem = y % TILE_HEIGHT;
    int tile_i;
    int base_bytes_pp = xc->base_bytes_pp;
    int n_extra_channels = xc->n_extra_channels;
    int row_idx = 0;

    for (tile_i = 0; tile_i < xc->n_tiles_x; tile_i++) {
        int x;
        int tile_width = min(TILE_WIDTH, xc->width - tile_i * TILE_WIDTH);
        int tile_height = min(TILE_HEIGHT, xc->height - tile_j * TILE_HEIGHT);
        byte *base_ptr = tile_data[tile_i] +
            yrem * tile_width * base_bytes_pp;
        int extra_stride = tile_width * tile_height;
        byte *extra_ptr = tile_data[tile_i] + extra_stride * base_bytes_pp +
            yrem * tile_width;

        int base_idx = 0;

        for (x = 0; x < tile_width; x++) {
            int plane_idx;
            for (plane_idx = 0; plane_idx < base_bytes_pp; plane_idx++)
                base_ptr[base_idx++] = row[row_idx++];
            for (plane_idx = 0; plane_idx < n_extra_channels; plane_idx++)
                extra_ptr[plane_idx * extra_stride + x] = 255 ^ row[row_idx++];
        }
    }
}

static void
xcf_icc_to_tile(gx_device_printer *pdev, xcf_write_ctx *xc, byte **tile_data, const byte *row,
                    int y, gcmmhlink_t link)
{
    int tile_j = y / TILE_HEIGHT;
    int yrem = y % TILE_HEIGHT;
    int tile_i;
    int base_bytes_pp = xc->base_bytes_pp;
    int n_extra_channels = xc->n_extra_channels;
    int row_idx = 0;

    for (tile_i = 0; tile_i < xc->n_tiles_x; tile_i++) {
        int x;
        int tile_width = min(TILE_WIDTH, xc->width - tile_i * TILE_WIDTH);
        int tile_height = min(TILE_HEIGHT, xc->height - tile_j * TILE_HEIGHT);
        byte *base_ptr = tile_data[tile_i] +
            yrem * tile_width * base_bytes_pp;
        int extra_stride = tile_width * tile_height;
        byte *extra_ptr = tile_data[tile_i] + extra_stride * base_bytes_pp +
            yrem * tile_width;

        int base_idx = 0;

        for (x = 0; x < tile_width; x++) {

            int plane_idx;

                /* This loop could be optimized.  I don't quite
                   understand what is going on in the loop
                   with the 255^row[row_idx++] operation */

            gscms_transform_color((gx_device*) pdev, link,
                                  (void *) (&(row[row_idx])),
                                   &(base_ptr[base_idx]), 1);

            for (plane_idx = 0; plane_idx < n_extra_channels; plane_idx++)
                extra_ptr[plane_idx * extra_stride + x] = 255 ^ row[row_idx++];
        }
    }
}

static int
xcf_write_image_data(xcf_write_ctx *xc, gx_device_printer *pdev)
{
    int code = 0;
    size_t raster = gdev_prn_raster(pdev);
    int tile_i, tile_j;
    byte **tile_data;
    byte *line;
    int base_bytes_pp = xc->base_bytes_pp;
    int n_extra_channels = xc->n_extra_channels;
    int bytes_pp = base_bytes_pp + n_extra_channels;
    int chan_idx;
    xcf_device *xdev = (xcf_device *)pdev;
    gcmmhlink_t link = xdev->output_icc_link;

    line = gs_alloc_bytes(pdev->memory, raster, "xcf_write_image_data");
    tile_data = (byte **)gs_alloc_bytes(pdev->memory,
                                        (size_t)xc->n_tiles_x * sizeof(byte *),
                                        "xcf_write_image_data");
    if (line == NULL || tile_data ==  NULL) {
        code = gs_error_VMerror;
        goto xit;
    }
    memset(tile_data, 0, xc->n_tiles_x * sizeof(byte *));
    for (tile_i = 0; tile_i < xc->n_tiles_x; tile_i++) {
        size_t tile_bytes = xcf_tile_sizeof(xc, tile_i) * bytes_pp;

        tile_data[tile_i] = gs_alloc_bytes(pdev->memory, tile_bytes,
                                           "xcf_write_image_data");
        if (tile_data[tile_i] == NULL)
            goto xit;
    }
    for (tile_j = 0; tile_j < xc->n_tiles_y; tile_j++) {
        int y0, y1;
        int y;
        byte *row;

        y0 = tile_j * TILE_HEIGHT;
        y1 = min(xc->height, y0 + TILE_HEIGHT);
        for (y = y0; y < y1; y++) {
            code = gdev_prn_get_bits(pdev, y, line, &row);
            if (code < 0)
                goto xit;
            if (link == NULL)
                xcf_shuffle_to_tile(xc, tile_data, row, y);
            else
                xcf_icc_to_tile(pdev, xc, tile_data, row, y, link);
        }
        for (tile_i = 0; tile_i < xc->n_tiles_x; tile_i++) {
            int tile_idx = tile_j * xc->n_tiles_x + tile_i;
            int tile_size = xcf_tile_sizeof(xc, tile_idx);
            int base_size = tile_size * base_bytes_pp;

            xcf_write(xc, tile_data[tile_i], base_size);
            for (chan_idx = 0; chan_idx < n_extra_channels; chan_idx++) {
                xcf_write(xc, tile_data[tile_i] + base_size +
                          tile_size * chan_idx, tile_size);
            }
        }
    }

xit:
    if (tile_data != NULL) {
        for (tile_i = 0; tile_i < xc->n_tiles_x; tile_i++) {
            gs_free_object(pdev->memory, tile_data[tile_i],
                    "xcf_write_image_data");
        }
    }
    gs_free_object(pdev->memory, tile_data, "xcf_write_image_data");
    gs_free_object(pdev->memory, line, "xcf_write_image_data");
    return code;
}

static int
xcf_write_fake_hierarchy(xcf_write_ctx *xc)
{
    int widthf = xc->width, heightf = xc->height;
    int i;

    for (i = 1; i < xc->n_levels; i++) {
        widthf >>= 1;
        heightf >>= 1;
        xcf_write_32(xc, widthf);
        xcf_write_32(xc, heightf);
        xcf_write_32(xc, 0);
    }
    return 0;
}

static int
xcf_write_footer(xcf_write_ctx *xc, xcf_device *pdev)
{
    int code = 0;
    int base_bytes_pp = xc->base_bytes_pp;
    int n_extra_channels = xc->n_extra_channels;
    int bytes_pp = base_bytes_pp + n_extra_channels;
    int chan_idx;

    xcf_write_fake_hierarchy(xc);

    for (chan_idx = 0; chan_idx < xc->n_extra_channels; chan_idx++) {
        const gs_param_string *separation_name =
            pdev->separation_names.names[chan_idx];
        byte nullbyte[] = { 0 };
        int level;
        int offset;
        int tile_idx;

        dmlprintf2(pdev->memory, "actual tile offset: %d %d\n", xc->offset, (int)ARCH_SIZEOF_COLOR_INDEX);
        xcf_write_32(xc, xc->width);
        xcf_write_32(xc, xc->height);
        xcf_write_32(xc, separation_name->size + 1);
        xcf_write(xc, separation_name->data, separation_name->size);
        xcf_write(xc, nullbyte, 1);

        /* channel props */
        xcf_write_32(xc, 0);
        xcf_write_32(xc, 0);

        /* channel tile hierarchy */
        xcf_write_32(xc, xc->offset + 4);

        xcf_write_32(xc, xc->width);
        xcf_write_32(xc, xc->height);
        xcf_write_32(xc, 1);
        xcf_write_32(xc, xc->offset + xc->n_levels * 16 - 8);
        offset = xc->offset + xc->n_levels * 4;
        for (level = 1; level < xc->n_levels; level++) {
            xcf_write_32(xc, offset);
            offset += 12;
        }
        xcf_write_32(xc, 0);
        xcf_write_fake_hierarchy(xc);

        /* channel tile offsets */
        xcf_write_32(xc, xc->width);
        xcf_write_32(xc, xc->height);
        offset = xc->image_data_off;
        for (tile_idx = 0; tile_idx < xc->n_tiles; tile_idx++) {
            int tile_size = xcf_tile_sizeof(xc, tile_idx);

            xcf_write_32(xc, offset + (base_bytes_pp + chan_idx) * tile_size);
            offset += bytes_pp * tile_size;
        }
        xcf_write_32(xc, 0);

    }
    return code;
}

static int
xcf_print_page(gx_device_printer *pdev, gp_file *file)
{
    xcf_write_ctx xc;

    xc.f = file;
    xc.offset = 0;

    xcf_setup_tiles(&xc, (xcf_device *)pdev);
    xcf_write_header(&xc, (xcf_device *)pdev);
    xcf_write_image_data(&xc, pdev);
    xcf_write_footer(&xc, (xcf_device *)pdev);

    return 0;
}
