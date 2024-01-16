/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* "Plain bits" devices to measure rendering time. */

#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"
#include "gxdcconv.h"
#include "gdevdcrd.h"
#include "gsutil.h" /* for bittags hack */
#include "gxdevsop.h"

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_open_device(bittag_open);
static dev_proc_get_color_mapping_procs(bittag_get_color_mapping_procs);
static dev_proc_map_rgb_color(bittag_rgb_map_rgb_color);
static dev_proc_map_color_rgb(bittag_map_color_rgb);
static dev_proc_put_params(bittag_put_params);
static dev_proc_create_buf_device(bittag_create_buf_device);
static dev_proc_fillpage(bittag_fillpage);
static dev_proc_map_rgb_color(bit_mono_map_color);
static dev_proc_decode_color(bit_mono_decode_color);
#if 0 /* unused */
static dev_proc_map_rgb_color(bit_forcemono_map_rgb_color);
#endif
static dev_proc_map_rgb_color(bitrgb_rgb_map_rgb_color);
static dev_proc_map_color_rgb(bit_map_color_rgb);
static dev_proc_map_cmyk_color(bit_map_cmyk_color);
static dev_proc_decode_color(bit_map_color_cmyk);
static dev_proc_get_params(bit_get_params);
static dev_proc_put_params(bit_put_params);
static dev_proc_print_page(bit_print_page);
static dev_proc_print_page(bittags_print_page);
static dev_proc_put_image(bit_put_image);
static dev_proc_dev_spec_op(bit_dev_spec_op);
dev_proc_get_color_comp_index(gx_default_DevRGB_get_color_comp_index);

static void
bit_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, map_color_rgb, bit_map_color_rgb);
    set_dev_proc(dev, get_params, bit_get_params);
    set_dev_proc(dev, put_params, bit_put_params);
    set_dev_proc(dev, dev_spec_op, bit_dev_spec_op);
}

/*
 * The following macro is used in get_params and put_params to determine the
 * num_components for the current device. It works using the device name
 * character after "bit" which is either '\0', 'r', or 'c'. Any new devices
 * that are added to this module must modify this macro to return the
 * correct num_components. This is needed to support the ForceMono
 * parameter, which alters dev->num_components.
 */
#define REAL_NUM_COMPONENTS(dev) (dev->dname[3] == 'c' ? 4 : \
                                  dev->dname[3] == 'r' ? 3 : 1)
struct gx_device_bit_s {
    gx_device_common;
    gx_prn_device_common;
    int  FirstLine, LastLine;	/* to allow multi-threaded rendering testing */
};
typedef struct gx_device_bit_s gx_device_bit;

static void
bitmono_initialize_device_procs(gx_device *dev)
{
    bit_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, bit_mono_map_color);
    set_dev_proc(dev, map_cmyk_color, bit_mono_map_color);
    set_dev_proc(dev, encode_color, bit_mono_map_color);
    set_dev_proc(dev, decode_color, bit_mono_decode_color);
}

const gx_device_bit gs_bit_device =
{prn_device_body(gx_device_bit, bitmono_initialize_device_procs, "bit",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,    /* margins */
                 1, 1, 1, 0, 2, 1, bit_print_page)
};

static void
bitrgb_initialize_device_procs(gx_device *dev)
{
    bit_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, bitrgb_rgb_map_rgb_color);
    set_dev_proc(dev, map_cmyk_color, bitrgb_rgb_map_rgb_color);
    set_dev_proc(dev, encode_color, bitrgb_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, bit_map_color_rgb);
}

const gx_device_bit gs_bitrgb_device =
{prn_device_body(gx_device_bit, bitrgb_initialize_device_procs, "bitrgb",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 3, 4, 1, 1, 2, 2, bit_print_page)
};

static void
bitcmyk_initialize_device_procs(gx_device *dev)
{
    bit_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, bit_map_cmyk_color);
    set_dev_proc(dev, map_cmyk_color, bit_map_cmyk_color);
    set_dev_proc(dev, encode_color, bit_map_cmyk_color);
    set_dev_proc(dev, decode_color, bit_map_color_cmyk);
}

const gx_device_bit gs_bitcmyk_device =
{prn_device_body(gx_device_bit, bitcmyk_initialize_device_procs, "bitcmyk",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 4, 4, 1, 1, 2, 2, bit_print_page)
};

static void
bitrgbtags_initialize_device_procs(gx_device *dev)
{
    bit_initialize_device_procs(dev);

    set_dev_proc(dev, open_device, bittag_open);
    set_dev_proc(dev, map_rgb_color, bittag_rgb_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, bittag_map_color_rgb);
    set_dev_proc(dev, get_params, gdev_prn_get_params);
    set_dev_proc(dev, put_params, bittag_put_params);
    set_dev_proc(dev, map_cmyk_color, bittag_rgb_map_rgb_color);
    set_dev_proc(dev, get_color_mapping_procs, bittag_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, gx_default_DevRGB_get_color_comp_index);
    set_dev_proc(dev, encode_color, bittag_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, bittag_map_color_rgb);
    set_dev_proc(dev, fillpage, bittag_fillpage);
    set_dev_proc(dev, put_image, bit_put_image);
}

const gx_device_bit gs_bitrgbtags_device =
    {
        sizeof(gx_device_bit),
        bitrgbtags_initialize_device_procs,
        "bitrgbtags",
        0,                              /* memory */
        &st_device_printer,
        0,                              /* stype_is_dynamic */
        0,                              /* finalize */
        { 0 },                          /* rc header */
        0,                              /* retained */
        0,                              /* parent */
        0,                              /* child */
        0,                              /* subclass data */
        0,                              /* PageList */
        0,                              /* is open */
        0,                              /* max_fill_band */
        {                               /* color infor */
            4,                          /* max_components */
            4,                          /* num_components */
            GX_CINFO_POLARITY_ADDITIVE, /* polarity */
            32,                         /* depth */
            GX_CINFO_COMP_NO_INDEX,     /* gray index */
            255,                        /* max_gray */
            255,                        /* max_colors */
            256,                        /* dither grays */
            256,                        /* dither colors */
            { 1, 1 },                   /* antialiasing */
            GX_CINFO_SEP_LIN_STANDARD,  /* sep and linear */
            { 16, 8, 0, 24 },           /* comp shift */	/* tag is upper byte */
            { 8, 8, 8, 8 },             /* comp bits */
            { 0xFF0000, 0x00FF00, 0x0000FF, 0xFF000000 },  /* comp mask */
            ( "DeviceRGBT" ),            /* color model name */
            GX_CINFO_OPMSUPPORTED_UNKNOWN,     /* overprint mode */
            0,                           /* process comps */
            0                            /* icc_locations */
        },
        {
            ((gx_color_index)(~0)),
            ((gx_color_index)(~0))
        },
        (int)((float)(85) * (X_DPI) / 10 + 0.5), /* width */
        (int)((float)(110) * (Y_DPI) / 10 + 0.5),/* height */
        0, /* Pad */
        0, /* log2_align_mod */
        0, /* num_planar_planes */
        0, /* LeadingEdge */
        {
            (float)(((((int)((float)(85) * (X_DPI) / 10 + 0.5)) * 72.0 + 0.5) - 0.5) / (X_DPI)),
            (float)(((((int)((float)(110) * (Y_DPI) / 10 + 0.5)) * 72.0 + 0.5) - 0.5) / (Y_DPI))
        }, /* MediaSize */
        {
            0,
            0,
            0,
            0
        }, /* ImagingBBox */
        0, /* ImagingBBox_set */
        { X_DPI, Y_DPI }, /* HWResolution*/
        {
          (float)(-(0) * (X_DPI)),
          (float)(-(0) * (Y_DPI))
        }, /* Margins */
        {
          (float)((0) * 72.0),
          (float)((0) * 72.0),
          (float)((0) * 72.0),
          (float)((0) * 72.0)
        }, /* HWMargins */
        0,  /*FirstPage*/
        0,  /*LastPage*/
        0,  /*PageHandlerPushed*/
        0,  /*DisablePageHandler*/
        0,  /*ObjectFilter*/
        0,  /*ObjectHandlerPushed*/
        0,  /*NupControl*/
        0,  /*NupHandlerPushed*/
        0,  /*PageCount*/
        0,  /*ShowPageCount*/
        1,  /*NumCopies*/
        0,  /*NumCopiesSet*/
        0,  /*IgnoreNumCopies*/
        0,  /*UseCIEColor*/
        0,  /*LockSafetyParams*/
        0,  /*band_offset_x*/
        0,  /*band_offset_y*/
        false, /*BLS_force_memory*/
        {false}, /*sgr*/
        0, /*MaxPatternBitmap*/
        0, /*page_uses_transparency*/
        0, /*page_uses_overprint*/
        {
          MAX_BITMAP, BUFFER_SPACE,
          { BAND_PARAMS_INITIAL_VALUES },
          0/*false*/, /* params_are_read_only */
          BandingAuto /* banding_type */
        }, /*space_params*/
        0, /*icc_struct*/
        GS_UNKNOWN_TAG,         /* this device supports tags */
        1,			/* default interpolate_control */
        0,                      /* default non_srict_bounds */
        {
            gx_default_install,
            gx_default_begin_page,
            gx_default_end_page
        }, /* page_procs */
        { 0 }, /* procs */
        GX_CLIST_MUTATABLE_DEVICE_DEFAULTS,
        {
          bittags_print_page,
          gx_default_print_page_copies,
          { bittag_create_buf_device,
            gx_default_size_buf_device,
            gx_default_setup_buf_device,
            gx_default_destroy_buf_device },
          gx_default_get_space_params
        }, /* printer_procs */
        { 0 }, /* fname */
        false, /* OpenOutputFile */
        false, /* ReopenPerPage */
        false, /* Duplex */
        -1,    /* Duplex_set */
        false, /* file_is_new */
        NULL,  /* file */
        false, /* bg_print_requested */
        0,     /* bg_print *  */
        0,     /* num_render_threads_requested */
        NULL,  /* saved_pages_list */
        {0}    /* save_procs_while_delaying_erasepage */
    };

static void
cmyk_cs_to_rgb_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    color_cmyk_to_rgb(c, m, y, k, NULL, out, dev->memory);
}

static void
private_rgb_cs_to_rgb_cm(const gx_device * dev, const gs_gstate *pgs,
                                  frac r, frac g, frac b, frac out[])
{
    out[0] = r;
    out[1] = g;
    out[2] = b;
}

static void
gray_cs_to_rgb_cm(const gx_device * dev, frac gray, frac out[])
{
    out[0] = out[1] = out[2] = gray;
}

static const gx_cm_color_map_procs bittag_DeviceRGB_procs = {
    gray_cs_to_rgb_cm, private_rgb_cs_to_rgb_cm, cmyk_cs_to_rgb_cm
};

static const gx_cm_color_map_procs *
bittag_get_color_mapping_procs(const gx_device *dev, const gx_device **map_dev)
{
    *map_dev = dev;
    return &bittag_DeviceRGB_procs;
}

static gx_color_index
bittag_rgb_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    return
        gx_color_value_to_byte(cv[2]) +
        (((uint) gx_color_value_to_byte(cv[1])) << 8) +
        (((ulong) gx_color_value_to_byte(cv[0])) << 16) +
        ((ulong)(dev->graphics_type_tag & ~GS_DEVICE_ENCODES_TAGS) << 24);
}

static int
bittag_map_color_rgb(gx_device * dev, gx_color_index color, gx_color_value cv[3])
{
    int depth = 24;
    int ncomp = 3;
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    gx_color_index cshift = color;
    cv[2] = cvalue(cshift & mask);
    cshift >>= bpc;
    cv[1] = cvalue(cshift & mask);
    cshift >>= bpc;
    cv[0] = cvalue(cshift & mask);
    return 0;
#undef cvalue
}

/* Map gray to color. */
/* Note that 1-bit monochrome is a special case. */
static gx_color_index
bit_mono_map_color(gx_device * dev, const gx_color_value cv[])
{
    int bpc = dev->color_info.depth;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_value gray = cv[0];

    return (bpc == 1 ? gx_max_color_value - gray : gray) >> drop;
}

static int
bit_mono_decode_color(gx_device * dev, gx_color_index color, gx_color_value *cv)
{
    int bpc = dev->color_info.depth;
    int max = (1<<bpc)-1;

    if (bpc == 1)
        cv[0] = -((gx_color_value)color ^ 1);
    else
        cv[0] = color * gx_max_color_value / max;
    return 0;
}

#if 0 /* unused */
/* Map RGB to gray shade. */
/* Only used in CMYK mode when put_params has set ForceMono=1 */
static gx_color_index
bit_forcemono_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_value color;
    int bpc = dev->color_info.depth / 4;	/* This function is used in CMYK mode */
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_value gray, red, green, blue;
    red = cv[0]; green = cv[1]; blue = cv[2];
    gray = red;
    if ((red != green) || (green != blue))
        gray = (red * (unsigned long)lum_red_weight +
             green * (unsigned long)lum_green_weight +
             blue * (unsigned long)lum_blue_weight +
             (lum_all_weights / 2))
                / lum_all_weights;

    color = (gx_max_color_value - gray) >> drop;	/* color is in K channel */
    return color;
}
#endif

gx_color_index
bitrgb_rgb_map_rgb_color(gx_device * dev, const gx_color_value cv[])
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

/* Map color to RGB.  This has 3 separate cases, but since it is rarely */
/* used, we do a case test rather than providing 3 separate routines. */
static int
bit_map_color_rgb(gx_device * dev, gx_color_index color, gx_color_value cv[3])
{
    int depth = dev->color_info.depth;
    int ncomp = REAL_NUM_COMPONENTS(dev);
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    switch (ncomp) {
        case 1:		/* gray */
            cv[0] = cv[1] = cv[2] =
                (depth == 1 ? (color ? 0 : gx_max_color_value) :
                 cvalue(color));
            break;
        case 3:		/* RGB */
            {
                gx_color_index cshift = color;

                cv[2] = cvalue(cshift & mask);
                cshift >>= bpc;
                cv[1] = cvalue(cshift & mask);
                cv[0] = cvalue(cshift >> bpc);
            }
            break;
        case 4:		/* CMYK */
            /* Map CMYK back to RGB. */
            {
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
            }
            break;
    }
    return 0;
#undef cvalue
}

/* Map CMYK to color. */
static gx_color_index
bit_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
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

static int
bit_map_color_cmyk(gx_device * dev, gx_color_index color, gx_color_value *cv)
{
    int bpc = dev->color_info.depth / 4;
    int max = (1<<bpc)-1;

    cv[0] = (color>>(3*bpc)) * gx_max_color_value / max;
    cv[1] = (color>>(2*bpc)) * gx_max_color_value / max;
    cv[2] = (color>>(1*bpc)) * gx_max_color_value / max;
    cv[3] = (color>>(0*bpc)) * gx_max_color_value / max;
    return 0;
}

static int
bittag_open(gx_device * pdev)
{
    gx_device_printer *ppdev = (gx_device_printer *)pdev;
    int code;
    /* We replace create_buf_device so we can replace copy_alpha
     * for memory device, but not clist. We also have our own
     * fillpage proc to fill with UNTOUCHED tag
     */
    ppdev->printer_procs.buf_procs.create_buf_device = bittag_create_buf_device;
    code = gdev_prn_open(pdev);
    return code;
}

/*
 * Fill the page fills with unmarked white, As with the pdf14 device, we treat
 * GS_UNTOUCHED_TAG == 0 as an invariant
*/
static int
bittag_fillpage(gx_device *dev, gs_gstate * pgs, gx_device_color *pdevc)
{
    return (*dev_proc(dev, fill_rectangle))(dev, 0, 0, dev->width, dev->height,
                                            0x00ffffff);	/* GS_UNTOUCHED_TAG == 0 */
}

static int
bittag_create_buf_device(gx_device **pbdev, gx_device *target, int y,
   const gx_render_plane_t *render_plane, gs_memory_t *mem,
   gx_color_usage_t *color_usage)
{
    int code = gx_default_create_buf_device(pbdev, target, y,
        render_plane, mem, color_usage);
    set_dev_proc(*pbdev, fillpage, bittag_fillpage);
    return code;
}

static int
bittag_put_params(gx_device * pdev, gs_param_list * plist)
{
    pdev->graphics_type_tag |= GS_DEVICE_ENCODES_TAGS;		/* the bittags devices use tags in the color */
    return gdev_prn_put_params(pdev, plist);
}
/* Get parameters.  We provide a default CRD. */
static int
bit_get_params(gx_device * pdev, gs_param_list * plist)
{
    int code, ecode;
    /*
     * The following is a hack to get the original num_components.
     * See comment above.
     */
    int real_ncomps = REAL_NUM_COMPONENTS(pdev);
    int ncomps = pdev->color_info.num_components;
    int forcemono = (ncomps == real_ncomps ? 0 : 1);

    /*
     * Temporarily set num_components back to the "real" value to avoid
     * confusing those that rely on it.
     */
    pdev->color_info.num_components = real_ncomps;

    ecode = gdev_prn_get_params(pdev, plist);
    code = sample_device_crd_get_params(pdev, plist, "CRDDefault");
    if (code < 0)
            ecode = code;
    if ((code = param_write_int(plist, "ForceMono", &forcemono)) < 0) {
        ecode = code;
    }
    if ((code = param_write_int(plist, "FirstLine", &((gx_device_bit *)pdev)->FirstLine)) < 0) {
        ecode = code;
    }
    if ((code = param_write_int(plist, "LastLine", &((gx_device_bit *)pdev)->LastLine)) < 0) {
        ecode = code;
    }

    /* Restore the working num_components */
    pdev->color_info.num_components = ncomps;

    return ecode;
}

/* Set parameters.  We allow setting the number of bits per component. */
/* Also, ForceMono=1 forces monochrome output from RGB/CMYK devices. */
static int
bit_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_color_info save_info;
    int ncomps = pdev->color_info.num_components;
    int real_ncomps = REAL_NUM_COMPONENTS(pdev);
    int v;
    int ecode = 0;
    int code;
    /* map to depths that we actually have memory devices to support */
    static const byte depths[4 /* ncomps - 1 */][16 /* bpc - 1 */] = {
        {1, 2, 0, 4, 8, 0, 0, 8, 0, 0, 0, 16, 0, 0, 0, 16}, /* ncomps = 1 */
        {0}, /* ncomps = 2, not supported */
        {4, 8, 0, 16, 16, 0, 0, 24, 0, 0, 0, 40, 0, 0, 0, 48}, /* ncomps = 3 (rgb) */
        {4, 8, 0, 16, 32, 0, 0, 32, 0, 0, 0, 48, 0, 0, 0, 64}  /* ncomps = 4 (cmyk) */
    };
    /* map back from depth to the actual bits per component */
    static int real_bpc[17] = { 0, 1, 2, 2, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12, 16 };
    int bpc = real_bpc[pdev->color_info.depth / real_ncomps];
    const char *vname;
    int FirstLine = ((gx_device_bit *)pdev)->FirstLine;
    int LastLine = ((gx_device_bit *)pdev)->LastLine;

    /*
     * Temporarily set num_components back to the "real" value to avoid
     * confusing those that rely on it.
     */
    pdev->color_info.num_components = real_ncomps;

    if ((code = param_read_int(plist, (vname = "GrayValues"), &v)) != 1 ||
        (code = param_read_int(plist, (vname = "RedValues"), &v)) != 1 ||
        (code = param_read_int(plist, (vname = "GreenValues"), &v)) != 1 ||
        (code = param_read_int(plist, (vname = "BlueValues"), &v)) != 1
        ) {
        if (code < 0)
            ecode = code;
        else
            switch (v) {
                case   2: bpc = 1; break;
                case   4: bpc = 2; break;
                case  16: bpc = 4; break;
                case 256: bpc = 8; break;
                case 4096: bpc = 12; break;
                case 65536: bpc = 16; break;
                default:
                    param_signal_error(plist, vname,
                                       ecode = gs_error_rangecheck);
            }
    }

    switch (code = param_read_int(plist, (vname = "ForceMono"), &v)) {
    case 0:
        if (v == 1) {
            ncomps = 1;
            break;
        }
        else if (v == 0) {
            ncomps = real_ncomps;
            break;
        }
        code = gs_error_rangecheck;
    default:
        ecode = code;
        param_signal_error(plist, vname, ecode);
    case 1:
        break;
    }
    if (ecode < 0)
        return ecode;
    switch (code = param_read_int(plist, (vname = "FirstLine"), &v)) {
    case 0:
        FirstLine = v;
        break;
    default:
        ecode = code;
        param_signal_error(plist, vname, ecode);
    case 1:
        break;
    }
    if (ecode < 0)
        return ecode;

    switch (code = param_read_int(plist, (vname = "LastLine"), &v)) {
    case 0:
        LastLine = v;
        break;
    default:
        ecode = code;
        param_signal_error(plist, vname, ecode);
    case 1:
        break;
    }
    if (ecode < 0)
        return ecode;

    /*
     * Save the color_info in case gdev_prn_put_params fails, and for
     * comparison.  Note that depth is computed from real_ncomps.
     */
    save_info = pdev->color_info;
    pdev->color_info.depth = depths[real_ncomps - 1][bpc - 1];
    pdev->color_info.max_gray = pdev->color_info.max_color =
        (pdev->color_info.dither_grays =
         pdev->color_info.dither_colors =
         (1 << bpc)) - 1;
    ecode = gdev_prn_put_params(pdev, plist);
    if (ecode < 0) {
        pdev->color_info = save_info;
        return ecode;
    }
    /* Now restore/change num_components. This is done after other	*/
    /* processing since it is used in gx_default_put_params		*/
    pdev->color_info.num_components = ncomps;
    if (pdev->color_info.depth != save_info.depth ||
        pdev->color_info.num_components != save_info.num_components
        ) {
        gs_closedevice(pdev);
    }
    /* Reset the map_cmyk_color procedure if appropriate. */
    if (dev_proc(pdev, map_cmyk_color) == cmyk_1bit_map_cmyk_color ||
        dev_proc(pdev, map_cmyk_color) == cmyk_8bit_map_cmyk_color ||
        dev_proc(pdev, map_cmyk_color) == bit_map_cmyk_color) {
        set_dev_proc(pdev, map_cmyk_color,
                     pdev->color_info.depth == 4 ? cmyk_1bit_map_cmyk_color :
                     pdev->color_info.depth == 32 ? cmyk_8bit_map_cmyk_color :
                     bit_map_cmyk_color);
    }
    /* Reset the separable and linear shift, masks, bits. */
    set_linear_color_bits_mask_shift(pdev);
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    ((gx_device_bit *)pdev)->FirstLine = FirstLine;
    ((gx_device_bit *)pdev)->LastLine = LastLine;

    return 0;
}

/* Send the page to the printer. */
static int
bit_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{				/* Just dump the bits on the file. */
    /* If the file is 'nul', don't even do the writes. */
    int line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    byte *in = gs_alloc_bytes(pdev->memory, line_size, "bit_print_page(in)");
    byte *data;
    int nul = !strcmp(pdev->fname, "nul") || !strcmp(pdev->fname, "/dev/null");
    int lnum = ((gx_device_bit *)pdev)->FirstLine >= pdev->height ?  pdev->height - 1 :
                 ((gx_device_bit *)pdev)->FirstLine;
    int bottom = ((gx_device_bit *)pdev)->LastLine >= pdev->height ?  pdev->height - 1 :
                 ((gx_device_bit *)pdev)->LastLine;
    int line_count = any_abs(bottom - lnum);
    int code = 0, i, step = lnum > bottom ? -1 : 1;

    if (in == 0)
        return_error(gs_error_VMerror);
    if ((lnum == 0) && (bottom == 0))
        line_count = pdev->height - 1;		/* default when LastLine == 0, FirstLine == 0 */
    for (i = 0; i <= line_count; i++, lnum += step) {
        code = gdev_prn_get_bits(pdev, lnum, in, &data);
        if (code < 0)
            goto done;
        if (!nul)
            gp_fwrite(data, 1, line_size, prn_stream);
    }
done:
    gs_free_object(pdev->memory, in, "bit_print_page(in)");
    return code;
}

/* For tags device go ahead and add in the size so that we can strip and create
   proper ppm outputs for various dimensions and not be restricted to 72dpi when
   using the tag viewer */
static int
bittags_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{				/* Just dump the bits on the file. */
    /* If the file is 'nul', don't even do the writes. */
    int line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    byte *in = gs_alloc_bytes(pdev->memory, line_size, "bit_print_page(in)");
    byte *data;
    int nul = !strcmp(pdev->fname, "nul") || !strcmp(pdev->fname, "/dev/null");
    int lnum = ((gx_device_bit *)pdev)->FirstLine >= pdev->height ?  pdev->height - 1 :
                 ((gx_device_bit *)pdev)->FirstLine;
    int bottom = ((gx_device_bit *)pdev)->LastLine >= pdev->height ?  pdev->height - 1 :
                 ((gx_device_bit *)pdev)->LastLine;
    int line_count = any_abs(bottom - lnum);
    int i, step = lnum > bottom ? -1 : 1;
    int code = 0;

    if (in == 0)
        return_error(gs_error_VMerror);

    if (!nul)
        gp_fprintf(prn_stream, "P7\nWIDTH %d\nHEIGHT %d\nMAXVAL 255\nDEPTH 4\nTUPLTYPE RGB_TAG\nENDHDR\n", pdev->width, pdev->height);
    if ((lnum == 0) && (bottom == 0))
        line_count = pdev->height - 1;		/* default when LastLine == 0, FirstLine == 0 */
    for (i = 0; i <= line_count; i++, lnum += step) {
        if ((code = gdev_prn_get_bits(pdev, lnum, in, &data)) < 0)
            goto done;
        if (!nul)
            gp_fwrite(data, 1, line_size, prn_stream);
    }
done:
    gs_free_object(pdev->memory, in, "bit_print_page(in)");
    return 0;
}

static int
bit_put_image(gx_device *pdev, gx_device *mdev, const byte **buffers, int num_chan, int xstart,
              int ystart, int width, int height, int row_stride,
              int alpha_plane_index, int tag_plane_index)
{
    gx_device_memory *pmemdev = (gx_device_memory *)mdev;
    byte *buffer_prn;
    int yend = ystart + height;
    int xend = xstart + width;
    int x, y, k;
    int src_position, des_position;

    if (alpha_plane_index != 0)
        return 0;                /* we don't want alpha, return 0 to ask for the    */
                                 /* pdf14 device to do the alpha composition        */
    if (num_chan != 3 || tag_plane_index <= 0)
            return_error(gs_error_unknownerror);        /* can't handle these cases */
    /* Drill down to get the appropriate memory buffer pointer */
    buffer_prn = pmemdev->base;
    /* Now go ahead and fill */
    for ( y = ystart; y < yend; y++ ) {
        src_position = (y - ystart) * row_stride;
        des_position = y * pmemdev->raster + xstart * 4;
        for ( x = xstart; x < xend; x++ ) {
            /* Tag data first, then RGB */
            buffer_prn[des_position] =
                buffers[tag_plane_index][src_position];
                des_position += 1;
            for ( k = 0; k < 3; k++) {
                buffer_prn[des_position] =
                    buffers[k][src_position];
                    des_position += 1;
            }
            src_position += 1;
        }
    }
    return height;        /* we used all of the data */
}

static int
bit_dev_spec_op(gx_device *pdev, int dso, void *ptr, int size)
{
    switch (dso)
    {
    case gxdso_is_encoding_direct:
        if (pdev->color_info.depth != 8 * pdev->color_info.num_components)
            return 0;
        return (dev_proc(pdev, encode_color) == bitrgb_rgb_map_rgb_color ||
                dev_proc(pdev, encode_color) == bit_map_cmyk_color);
    }

    return gdev_prn_dev_spec_op(pdev, dso, ptr, size);
}
