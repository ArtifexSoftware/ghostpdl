/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Chameleon devices to allow for changing bit depths/colors at runtime. */

#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"
#include "gxdcconv.h"
#include "gdevdcrd.h"
#include "gxdownscale.h"
#include "gxdevsop.h"
#include "gsicc_manage.h"

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_encode_color(chameleon_mono_encode_color);
static dev_proc_encode_color(chameleon_rgb_encode_color);
static dev_proc_encode_color(chameleon_cmyk_encode_color);
static dev_proc_decode_color(chameleon_mono_decode_color);
static dev_proc_decode_color(chameleon_rgb_decode_color);
static dev_proc_decode_color(chameleon_cmyk_decode_color);
static dev_proc_get_params(chameleon_get_params);
static dev_proc_put_params(chameleon_put_params);
static dev_proc_print_page(chameleon_print_page);
static dev_proc_dev_spec_op(chameleon_spec_op);
static dev_proc_close_device(chameleon_close);

typedef enum {
    linktype_none = 0,
    linktype_postrender,
    linktype_device
} linktype_t;

struct gx_device_chameleon_s {
    gx_device_common;
    gx_prn_device_common;
    int num_components;
    int bpc;
    int dst_num_components;
    int dst_bpc;
    int output_as_pxm;
    bool language_uses_rops;
    gx_downscaler_params downscale;
    gsicc_link_t *icclink;
    linktype_t linktype;
    cmm_profile_t *default_device_profile;
};
typedef struct gx_device_chameleon_s gx_device_chameleon;

static void
chameleon_finalize(gx_device *dev)
{
    gx_device_chameleon *pcdev = (gx_device_chameleon *)dev;

    gsicc_adjust_profile_rc(pcdev->default_device_profile, -1, "chameleon_finalize");
}

static void
chameleon_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, map_rgb_color, chameleon_rgb_encode_color);
    set_dev_proc(dev, map_color_rgb, chameleon_rgb_decode_color);
    set_dev_proc(dev, get_params, chameleon_get_params);
    set_dev_proc(dev, put_params, chameleon_put_params);
    set_dev_proc(dev, map_cmyk_color, chameleon_rgb_encode_color);
    set_dev_proc(dev, encode_color, chameleon_rgb_encode_color);
    set_dev_proc(dev, decode_color, chameleon_rgb_decode_color);
    set_dev_proc(dev, dev_spec_op, chameleon_spec_op);
    set_dev_proc(dev, close_device, chameleon_close);
    dev->finalize = chameleon_finalize;
}

const gx_device_chameleon gs_chameleon_device =
{prn_device_body(gx_device_chameleon, chameleon_initialize_device_procs,
                 "chameleon",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 4, /* 3 colors (by default) */
                 4, /* depth */
                 15, /* max grey */
                 15, /* max color */
                 16, /* dither greys */
                 16, /* dither colors */
                 chameleon_print_page),
        4, /* Default to CMYK output */
        1, /* Default to 1bpc output */
        4, /* Default to CMYK output */
        1, /* Default to 1bpc output */
        0, /* Default to outputting headerless data */
        0, /* Default to not ropping */
        GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* Map gray to color. */
/* Note that 1-bit monochrome is a special case. */
static gx_color_index
chameleon_mono_encode_color(gx_device * dev, const gx_color_value cv[])
{
    int bpc = dev->color_info.depth;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_value gray = cv[0];

    return (bpc == 1 ? gx_max_color_value - gray : gray) >> drop;
}

static gx_color_index
chameleon_rgb_encode_color(gx_device * dev, const gx_color_value cv[])
{
    if (dev->color_info.depth == 24)
        return gx_color_value_to_byte(cv[2]) +
            ((uint) gx_color_value_to_byte(cv[1]) << 8) +
            ((ulong) gx_color_value_to_byte(cv[0]) << 16);
    else {
        COLROUND_VARS;
        /* The following needs special handling to avoid bpc=5 when depth=16 */
        int bpc = dev->color_info.depth == 16 ? 4 : dev->color_info.depth / 3;
        COLROUND_SETUP(bpc);

        return (((COLROUND_ROUND(cv[0]) << bpc) +
                 COLROUND_ROUND(cv[1])) << bpc) +
               COLROUND_ROUND(cv[2]);
    }
}

/* Map CMYK to color. */
static gx_color_index
chameleon_cmyk_encode_color(gx_device * dev, const gx_color_value cv[])
{
    int bpc = dev->color_info.depth / 4;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_index color =
    (((((((gx_color_index) cv[0] >> drop) << bpc) +
        (cv[1] >> drop)) << bpc) +
      (cv[2] >> drop)) << bpc) +
    (cv[3] >> drop);

    return (color == gx_no_color_index ? color ^ 1 : color);
}

/* Map color to RGB.  This has 3 separate cases, but since it is rarely */
/* used, we do a case test rather than providing 3 separate routines. */
static int
chameleon_mono_decode_color(gx_device * dev, gx_color_index color, gx_color_value cv[4])
{
    int depth = dev->color_info.depth;
    int ncomp = dev->color_info.num_components;
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    cv[0] = cv[1] = cv[2] =
        (depth == 1 ? (color ? 0 : gx_max_color_value) :
        cvalue(color));
    return 0;
#undef cvalue
}

static int
chameleon_rgb_decode_color(gx_device * dev, gx_color_index color, gx_color_value cv[4])
{
    int depth = dev->color_info.depth;
    int ncomp = dev->color_info.num_components;
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    gx_color_index cshift = color;

    cv[2] = cvalue(cshift & mask);
    cshift >>= bpc;
    cv[1] = cvalue(cshift & mask);
    cv[0] = cvalue(cshift >> bpc);
    return 0;
#undef cvalue
}

static int
chameleon_cmyk_decode_color(gx_device * dev, gx_color_index color, gx_color_value cv[4])
{
    int depth = dev->color_info.depth;
    int ncomp = dev->color_info.num_components;
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    gx_color_index cshift = color;
    uint c, m, y, k;

    k = cshift & mask;
    cshift >>= bpc;
    y = cshift & mask;
    cshift >>= bpc;
    m = cshift & mask;
    c = cshift >> bpc;
    /* We use our improved conversion rule.... */
    cv[0] = cvalue((mask - c) * (mask - k) / mask);
    cv[1] = cvalue((mask - m) * (mask - k) / mask);
    cv[2] = cvalue((mask - y) * (mask - k) / mask);
    return 0;
#undef cvalue
}

static int
fixup_icc_struct(gx_device_chameleon *pcdev, gsicc_colorbuffer_t cs, char *profile)
{
    if (pcdev->icc_struct &&
        pcdev->icc_struct->device_profile[gsDEFAULTPROFILE] &&
        pcdev->icc_struct->device_profile[gsDEFAULTPROFILE]->data_cs == cs)
        return 0; /* Already the right profile type */

    rc_decrement(pcdev->icc_struct, "fixup_icc_struct(chameleon)");

    pcdev->icc_struct = gsicc_new_device_profile_array((gx_device *)pcdev);
    if (pcdev->icc_struct == NULL)
        return gs_error_VMerror;

    return gsicc_set_device_profile((gx_device *)pcdev, pcdev->memory,
                                    profile, gsDEFAULTPROFILE);
}

static void reset_icclink(gx_device_chameleon *pcdev)
{
    gsicc_free_link_dev(pcdev->icclink);
    pcdev->icclink = NULL;
    pcdev->linktype = linktype_none;
}

static void
stash_default_device_profile(gx_device_chameleon *pcdev)
{
    if (pcdev->default_device_profile != NULL)
        return;

    pcdev->default_device_profile = pcdev->icc_struct->device_profile[gsDEFAULTPROFILE];
    gsicc_adjust_profile_rc(pcdev->default_device_profile, 1, "stash_default_device_profile");
}

/* # So long, oh, I hate to see you go */
static int
reconfigure_baby(gx_device_chameleon *pcdev)
{
    int bpc = pcdev->dst_bpc;
    int num_comps = pcdev->dst_num_components;
    int code;

    stash_default_device_profile(pcdev);

    if (pcdev->language_uses_rops) {
        bpc = 8;
        num_comps = 3;
    }

    if (pcdev->bpc == bpc && pcdev->num_components == num_comps)
        return 0; /* Nothing needs to change! */

    gs_closedevice((gx_device *)pcdev);

    pcdev->bpc = bpc;
    pcdev->num_components = num_comps;

    switch (num_comps * bpc) {
        case 1*1: case 1*2: case 1*4: case 1*8:
        case 3*8:
        case 4*4: case 4*8:
            pcdev->color_info.depth = bpc * num_comps;
            break;
        case 3*1:
            pcdev->color_info.depth = 4;
            break;
        case 3*2:
            pcdev->color_info.depth = 8;
            break;
        case 3*4:
            pcdev->color_info.depth = 16;
            break;
    }
    pcdev->color_info.num_components = num_comps;
    pcdev->color_info.max_components = num_comps;
    pcdev->color_info.max_gray = pcdev->color_info.max_color =
        (pcdev->color_info.dither_grays = pcdev->color_info.dither_colors = 1<<bpc) - 1;

    switch (num_comps) {
        case 1:
            set_dev_proc(pcdev, encode_color, chameleon_mono_encode_color);
            set_dev_proc(pcdev, decode_color, chameleon_mono_decode_color);
            set_dev_proc(pcdev, get_color_mapping_procs,
                         gx_default_DevGray_get_color_mapping_procs);
            set_dev_proc(pcdev, get_color_comp_index,
                         gx_default_DevGray_get_color_comp_index);
            pcdev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
            pcdev->color_info.cm_name = "DeviceGray";
            pcdev->color_info.gray_index = 0;
            code = fixup_icc_struct(pcdev, gsGRAY, DEFAULT_GRAY_ICC);
            if (code < 0)
                return code;
            break;
        case 3:
            set_dev_proc(pcdev, encode_color, chameleon_rgb_encode_color);
            set_dev_proc(pcdev, decode_color, chameleon_rgb_decode_color);
            set_dev_proc(pcdev, get_color_mapping_procs,
                         gx_default_DevRGB_get_color_mapping_procs);
            set_dev_proc(pcdev, get_color_comp_index,
                         gx_default_DevRGB_get_color_comp_index);
            pcdev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
            pcdev->color_info.cm_name = "DeviceRGB";
            pcdev->color_info.gray_index = GX_CINFO_COMP_NO_INDEX;
            code = fixup_icc_struct(pcdev, gsRGB, DEFAULT_RGB_ICC);
            if (code < 0)
                return code;
            break;
        case 4:
            set_dev_proc(pcdev, encode_color, chameleon_cmyk_encode_color);
            set_dev_proc(pcdev, decode_color, chameleon_cmyk_decode_color);
            set_dev_proc(pcdev, get_color_mapping_procs,
                         gx_default_DevCMYK_get_color_mapping_procs);
            set_dev_proc(pcdev, get_color_comp_index,
                         gx_default_DevCMYK_get_color_comp_index);
            pcdev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
            pcdev->color_info.cm_name = "DevicCMYK";
            pcdev->color_info.gray_index = 3;
            code = fixup_icc_struct(pcdev, gsCMYK, DEFAULT_CMYK_ICC);
            if (code < 0)
                return code;
            break;
    }
    set_dev_proc(pcdev, map_color_rgb, dev_proc(pcdev, decode_color));
    set_dev_proc(pcdev, map_cmyk_color, dev_proc(pcdev, encode_color));
    set_dev_proc(pcdev, map_rgb_color, dev_proc(pcdev, encode_color));

    /* Reset the separable and linear shift, masks, bits. */
    set_linear_color_bits_mask_shift((gx_device *)pcdev);
    pcdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    pcdev->bpc = bpc;
    pcdev->num_components = num_comps;

    /* Invalidate the link. */
    reset_icclink(pcdev);

    return 0;
}

/* Get parameters.  We provide a default CRD. */
static int
chameleon_get_params(gx_device * pdev, gs_param_list * plist)
{
    int code, ecode;
    gx_device_chameleon *pcdev = (gx_device_chameleon *)pdev;

    ecode = gdev_prn_get_params(pdev, plist);
    code = sample_device_crd_get_params(pdev, plist, "CRDDefault");
    if (code < 0)
            ecode = code;

    if ((code = param_write_int(plist, "DstBitDepth", &pcdev->dst_bpc)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "DstComponents", &pcdev->dst_num_components)) < 0)
        ecode = code;
    if ((code = gx_downscaler_write_params(plist, &pcdev->downscale, 0)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "OutputAsPXM", &pcdev->output_as_pxm)) < 0)
        ecode = code;
    if ((code = param_write_bool(plist, "LanguageUsesROPs", &pcdev->language_uses_rops)) < 0)
        ecode = code;

    return ecode;
}

/* Set parameters.  We allow setting the number of bits per component, and number of components. */
static int
chameleon_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_chameleon *pcdev = (gx_device_chameleon *)pdev;
    gx_device_color_info save_info;
    int pxm = pcdev->output_as_pxm;
    int dst_num_comps = pcdev->dst_num_components;
    int dst_bpc = pcdev->dst_bpc;
    bool language_uses_rops = pcdev->language_uses_rops;
    int ecode;
    int code;
    const char *vname;

    ecode = gx_downscaler_read_params(plist, &pcdev->downscale, 0);

    if ((code = param_read_int(plist, (vname = "DstBitDepth"), &dst_bpc)) != 1) {
        if (code < 0)
            ecode = code;
        else
            switch (dst_bpc) {
                case 1: case 2: case 4: case 8: break;
                default:
                    param_signal_error(plist, vname,
                                       ecode = gs_error_rangecheck);
            }
    }

    if ((code = param_read_int(plist, (vname = "DstComponents"), &dst_num_comps)) != 1) {
        if (code < 0)
            ecode = code;
        else
            switch (dst_num_comps) {
                case 1: case 3: case 4: break;
                default:
                    param_signal_error(plist, vname,
                                       ecode = gs_error_rangecheck);
            }
    }

    if ((code = param_read_int(plist, (vname = "OutputAsPXM"), &pxm)) != 1) {
        if (code == gs_error_typecheck)
            pxm = 1;
        else if (code < 0)
            ecode = code;
        else
            pxm = !!pxm;
    }

    if ((code = param_read_bool(plist, (vname = "LanguageUsesROPs"), &language_uses_rops)) != 1) {
        if (code == gs_error_typecheck)
            language_uses_rops = false;
        else if (code < 0)
            ecode = code;
    }

    ecode = gdev_prn_put_params(pdev, plist);
    if (ecode < 0)
        return ecode;

    pcdev->dst_bpc = dst_bpc;
    pcdev->dst_num_components = dst_num_comps;
    pcdev->output_as_pxm = pxm;
    pcdev->language_uses_rops = language_uses_rops;

    return reconfigure_baby(pcdev);
}

static int
chameleon_spec_op(gx_device *dev_, int op, void *data, int datasize)
{
    gx_device_chameleon *dev = (gx_device_chameleon *)dev_;

    if (op == gxdso_supports_iccpostrender) {
        return true;
    }
    return gdev_prn_dev_spec_op(dev_, op, data, datasize);
}

static int
header_4x1(gp_file *file, gx_device_chameleon *pcdev)
{
    gp_fprintf(file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE CMYK\nENDHDR\n",
        pcdev->width, pcdev->height);
    return 0;
}

static int
write_4x1(const byte *data, int n, gp_file *file)
{
    byte b[8];
    n -= 4;
    while (n > 0) {
        byte d = *data++;
        b[0] = (d & 128) ? 255 : 0;
        b[1] = (d &  64) ? 255 : 0;
        b[2] = (d &  32) ? 255 : 0;
        b[3] = (d &  16) ? 255 : 0;
        b[4] = (d &   8) ? 255 : 0;
        b[5] = (d &   4) ? 255 : 0;
        b[6] = (d &   2) ? 255 : 0;
        b[7] = (d &   1) ? 255 : 0;
        gp_fwrite(b, 1, 8, file);
        n -= 8;
    }
    if (n == 0) {
        byte d = *data;
        b[0] = (d & 128) ? 255 : 0;
        b[1] = (d &  64) ? 255 : 0;
        b[2] = (d &  32) ? 255 : 0;
        b[3] = (d &  16) ? 255 : 0;
        gp_fwrite(b, 1, 4, file);
    }
    return 0;
}

static int
header_3x8(gp_file *file, gx_device_chameleon *pcdev)
{
    gp_fprintf(file, "P6\n%d %d 255\n",
        pcdev->width, pcdev->height);
    return 0;
}

static int
header_4x8(gp_file *file, gx_device_chameleon *pcdev)
{
    gp_fprintf(file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE CMYK\nENDHDR\n",
        pcdev->width, pcdev->height);
    return 0;
}

static int
do_fwrite(const byte *data, int n, gp_file *file)
{
    return gp_fwrite(data, 1, (n+7)>>3, file);
}

static int chunky_post_cm(void  *arg, byte **dst, byte **src, int w, int h,
    int raster)
{
    gsicc_bufferdesc_t input_buffer_desc, output_buffer_desc;
    gsicc_link_t *icclink = (gsicc_link_t*)arg;

    gsicc_init_buffer(&input_buffer_desc, icclink->num_input, 1, false,
        false, false, 0, raster, h, w);
    gsicc_init_buffer(&output_buffer_desc, icclink->num_output, 1, false,
        false, false, 0, raster, h, w);
    icclink->procs.map_buffer(NULL, icclink, &input_buffer_desc, &output_buffer_desc,
        src[0], dst[0]);
    return 0;
}

static byte ht_data[256] =
{
        0x0E, 0x8E, 0x2E, 0xAE, 0x06, 0x86, 0x26, 0xA6, 0x0C, 0x8C, 0x2C, 0xAC, 0x04, 0x84, 0x24, 0xA4,
        0xCE, 0x4E, 0xEE, 0x6E, 0xC6, 0x46, 0xE6, 0x66, 0xCC, 0x4C, 0xEC, 0x6C, 0xC4, 0x44, 0xE4, 0x64,
        0x3E, 0xBE, 0x1E, 0x9E, 0x36, 0xB6, 0x16, 0x96, 0x3C, 0xBC, 0x1C, 0x9C, 0x34, 0xB4, 0x14, 0x94,
        0xFE, 0x7E, 0xDE, 0x5E, 0xF6, 0x76, 0xD6, 0x56, 0xFC, 0x7C, 0xDC, 0x5C, 0xF4, 0x74, 0xD4, 0x54,
        0x01, 0x81, 0x21, 0xA1, 0x09, 0x89, 0x29, 0xA9, 0x03, 0x83, 0x23, 0xA3, 0x0B, 0x8B, 0x2B, 0xAB,
        0xC1, 0x41, 0xE1, 0x61, 0xC9, 0x49, 0xE9, 0x69, 0xC3, 0x43, 0xE3, 0x63, 0xCB, 0x4B, 0xEB, 0x6B,
        0x31, 0xB1, 0x11, 0x91, 0x39, 0xB9, 0x19, 0x99, 0x33, 0xB3, 0x13, 0x93, 0x3B, 0xBB, 0x1B, 0x9B,
        0xF1, 0x71, 0xD1, 0x51, 0xF9, 0x79, 0xD9, 0x59, 0xF3, 0x73, 0xD3, 0x53, 0xFB, 0x7B, 0xDB, 0x5B,
        0x0D, 0x8D, 0x2D, 0xAD, 0x05, 0x85, 0x25, 0xA5, 0x0F, 0x8F, 0x2F, 0xAF, 0x07, 0x87, 0x27, 0xA7,
        0xCD, 0x4D, 0xED, 0x6D, 0xC5, 0x45, 0xE5, 0x65, 0xCF, 0x4F, 0xEF, 0x6F, 0xC7, 0x47, 0xE7, 0x67,
        0x3D, 0xBD, 0x1D, 0x9D, 0x35, 0xB5, 0x15, 0x95, 0x3F, 0xBF, 0x1F, 0x9F, 0x37, 0xB7, 0x17, 0x97,
        0xFD, 0x7D, 0xDD, 0x5D, 0xF5, 0x75, 0xD5, 0x55, 0xFF, 0x7F, 0xDF, 0x5F, 0xF7, 0x77, 0xD7, 0x57,
        0x02, 0x82, 0x22, 0xA2, 0x0A, 0x8A, 0x2A, 0xAA, 0x01 /*0x00*/, 0x80, 0x20, 0xA0, 0x08, 0x88, 0x28, 0xA8,
        0xC2, 0x42, 0xE2, 0x62, 0xCA, 0x4A, 0xEA, 0x6A, 0xC0, 0x40, 0xE0, 0x60, 0xC8, 0x48, 0xE8, 0x68,
        0x32, 0xB2, 0x12, 0x92, 0x3A, 0xBA, 0x1A, 0x9A, 0x30, 0xB0, 0x10, 0x90, 0x38, 0xB8, 0x18, 0x98,
        0xF2, 0x72, 0xD2, 0x52, 0xFA, 0x7A, 0xDA, 0x5A, 0xF0, 0x70, 0xD0, 0x50, 0xF8, 0x78, 0xD8, 0x58
};

static gx_downscaler_ht_t default_ht[4] =
{
    { 16, 16, 16, 0, 0, ht_data },
    { 16, 16, 16, 0, 0, ht_data },
    { 16, 16, 16, 0, 0, ht_data },
    { 16, 16, 16, 0, 0, ht_data }
};

/* Send the page to the printer. */
static int
chameleon_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{				/* Just dump the bits on the file. */
    gx_device_chameleon *pcdev = (gx_device_chameleon *)pdev;
    /* If the file is 'nul', don't even do the writes. */
    int line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    byte *in, *in_alloc;
    byte *data;
    int nul;
    int line_count = pdev->height;
    int i, code;
    gx_downscaler_t ds = { NULL };
    int depth = pdev->color_info.depth;
    int factor = pcdev->downscale.downscale_factor;
    int mfs = pcdev->downscale.min_feature_size;
    int pxm = pcdev->output_as_pxm;
    int dst_bpp;
    int (*write)(const byte *, int, gp_file *) = do_fwrite;
    int (*header)(gp_file *, gx_device_chameleon *) = NULL;
    int bitwidth;

    switch (pcdev->dst_num_components * pcdev->dst_bpc) {
        case 1*1: case 1*2: case 1*4: case 1*8:
        case 4*4: case 4*8: case 3*8:
            dst_bpp = pcdev->dst_num_components * pcdev->dst_bpc;
            break;
        case 3*1:
            dst_bpp = 4;
            break;
        case 3*2:
            dst_bpp = 8;
            break;
        case 3*4:
            dst_bpp = 12;
            break;
        default:
            return gs_error_rangecheck;
    }

    bitwidth = pdev->width * dst_bpp;
    line_size = (bitwidth+7)>>3;
    in_alloc = gs_alloc_bytes(pdev->memory, line_size+32, "chameleon_print_page(in)");
    if (in_alloc == 0)
        return_error(gs_error_VMerror);
    in = in_alloc + ((32-(intptr_t)in_alloc) & 31);

    if (!strcmp(pdev->fname, "nul") || !strcmp(pdev->fname, "/dev/null"))
        write = NULL;
    else if (pxm) {
        switch (pcdev->dst_num_components) {
        case 4:
            if (pcdev->dst_bpc == 1)
                header = header_4x1, write = write_4x1;
            else if (pcdev->dst_bpc == 8)
                header = header_4x8;
            break;
        case 3:
            if (pcdev->dst_bpc == 8)
                header = header_3x8;
            break;
        }
    }

    /* If we have a post render profile, and the number of components
     * of that is compatible with the desired destination number of
     * components, then use that. */
    if (pcdev->icc_struct->postren_profile &&
        pcdev->icc_struct->postren_profile->num_comps == pcdev->dst_num_components) {
        if (pcdev->linktype != linktype_postrender) {
            reset_icclink(pcdev);
            code = gx_downscaler_create_post_render_link((gx_device *)pdev,
                                                         &pcdev->icclink);
            if (code < 0)
                return code;
            pcdev->linktype = linktype_postrender;
        }
    } else if (pcdev->num_components == pcdev->dst_num_components &&
               pcdev->bpc == pcdev->dst_bpc) {
        /* Nothing to do */
        reset_icclink(pcdev);
    } else if (pcdev->bpc == 8 && pcdev->dst_num_components == 4) {
        if (pcdev->linktype != linktype_device) {
            reset_icclink(pcdev);
            code = gx_downscaler_create_icc_link((gx_device *)pdev,
                                                 &pcdev->icclink,
                                                 pcdev->default_device_profile);
            if (code < 0)
                return code;
            pcdev->linktype = linktype_device;
        }
    } else if (pcdev->bpc == 8 && pcdev->dst_num_components == 3) {
        if (pcdev->linktype != linktype_device) {
            reset_icclink(pcdev);
            code = gx_downscaler_create_icc_link((gx_device *)pdev,
                                                 &pcdev->icclink,
                                                 pcdev->default_device_profile);
            if (code < 0)
                return code;
            pcdev->linktype = linktype_device;
        }
    } else if (pcdev->bpc == 8 && pcdev->dst_num_components == 1) {
        if (pcdev->linktype != linktype_device) {
            reset_icclink(pcdev);
            code = gx_downscaler_create_icc_link((gx_device *)pdev,
                                                 &pcdev->icclink,
                                                 pcdev->default_device_profile);
            if (code < 0)
                return code;
            pcdev->linktype = linktype_device;
        }
    } else {
        emprintf(pdev->memory, "Chameleon device doesn't support this color conversion.\n");
        return gs_error_rangecheck;
    }

    code = gx_downscaler_init_cm_halftone(&ds,
                                          (gx_device *)pdev,
                                          pcdev->bpc,
                                          pcdev->dst_bpc,
                                          pcdev->num_components,
                                          &pcdev->downscale,
                                          NULL, 0, /* Adjust width */
                                          pcdev->icclink ? chunky_post_cm : NULL,
                                          pcdev->icclink, /* Color Management */
                                          pcdev->dst_num_components,
                                          default_ht);
    if (code < 0)
        goto cleanup;

    if (header) {
        code = header(prn_stream, pcdev);
        if (code < 0)
            goto cleanup;
    }

    for (i = 0; i < line_count; i++) {
        code = gx_downscaler_getbits(&ds, in, i);
        if (code < 0)
            break;
        if (write != NULL)
            write(in, bitwidth, prn_stream);
    }
    gx_downscaler_fin(&ds);
cleanup:
    gs_free_object(pdev->memory, in_alloc, "chameleon_print_page(in)");
    return code;
}

static int
chameleon_close(gx_device *pdev)
{
    gx_device_chameleon *pcdev = (gx_device_chameleon *)pdev;

    gsicc_free_link_dev(pcdev->icclink);
    pcdev->icclink = NULL;
    return gdev_prn_close(pdev);
}
