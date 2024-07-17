/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* URF devices. */

#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"
#include "gxdcconv.h"
#include "gdevdcrd.h"
#include "gxdevsop.h"
#include "gxdownscale.h"

typedef enum
{
    urf_cs_W          = 0, /* Luminance w/gamma 2.2 */
    urf_cs_sRGB       = 1,
    urf_cs_AdobeRGB   = 3,
    urf_cs_DeviceW    = 4,
    urf_cs_DeviceRGB  = 5,
    urf_cs_DeviceCMYK = 6
} urf_colorspace;

typedef enum
{
    urf_duplex_auto  = 0, /* default */
    urf_duplex_one   = 1,
    urf_duplex_short = 2, /* landscape */
    urf_duplex_long  = 3  /* portrait */
} urf_duplex;

typedef enum
{
    urf_quality_auto   = 0, /* default */
    urf_quality_draft  = 3,
    urf_quality_normal = 4,
    urf_quality_best   = 5
} urf_quality;

typedef enum
{
    urf_media_auto         = 0, /* default */
    urf_media_plain        = 1,
    urf_media_transparency = 2,
    urf_media_envelope     = 3,
    urf_media_cardstock    = 4,
    urf_media_labels       = 5,
    urf_media_letterhead   = 6,
    urf_media_disc         = 7,
    urf_media_matte        = 8,
    urf_media_satin        = 9,
    urf_media_semigloss    = 10,
    urf_media_gloss        = 11,
    urf_media_highgloss    = 12,
    urf_media_other        = 13
} urf_media;

typedef enum
{
    urf_input_auto          = 0, /* default */
    urf_input_main          = 1,
    urf_input_alternate     = 2,
    urf_input_large         = 3,
    urf_input_manual        = 4,
    urf_input_envelope      = 5,
    urf_input_disc          = 6,
    urf_input_photo         = 7,
    urf_input_hagaki        = 8,
    urf_input_mainroll      = 9,
    urf_input_alternateroll = 10,
    urf_input_top           = 11,
    urf_input_middle        = 12,
    urf_input_bottom        = 13,
    urf_input_side          = 14,
    urf_input_left          = 15,
    urf_input_right         = 16,
    urf_input_center        = 17,
    urf_input_rear          = 18,
    urf_input_bypass        = 19,
    urf_input_tray1         = 20,
    urf_input_tray2         = 21,
    urf_input_tray3         = 22,
    urf_input_tray4         = 23,
    urf_input_tray5         = 24,
    urf_input_tray6         = 25,
    urf_input_tray7         = 26,
    urf_input_tray8         = 27,
    urf_input_tray9         = 28,
    urf_input_tray10        = 29,
    urf_input_tray11        = 30,
    urf_input_tray12        = 31,
    urf_input_tray13        = 32,
    urf_input_tray14        = 33,
    urf_input_tray15        = 34,
    urf_input_tray16        = 35,
    urf_input_tray17        = 36,
    urf_input_tray18        = 37,
    urf_input_tray19        = 38,
    urf_input_tray20        = 39,
    urf_input_roll1         = 40,
    urf_input_roll2         = 41,
    urf_input_roll3         = 42,
    urf_input_roll4         = 43,
    urf_input_roll5         = 44,
    urf_input_roll6         = 45,
    urf_input_roll7         = 46,
    urf_input_roll8         = 47,
    urf_input_roll9         = 48,
    urf_input_roll10        = 49
} urf_input;

typedef enum
{
    urf_output_auto      = 0, /* default */
    urf_output_top       = 1,
    urf_output_middle    = 2,
    urf_output_bottom    = 3,
    urf_output_side      = 4,
    urf_output_left      = 5,
    urf_output_right     = 6,
    urf_output_center    = 7,
    urf_output_rear      = 8,
    urf_output_faceup    = 9,
    urf_output_facedown  = 10,
    urf_output_large     = 11,
    urf_output_stacker   = 12,
    urf_output_mailbox   = 13,
    urf_output_roll1     = 14,
    urf_output_mailbox1  = 20,
    urf_output_mailbox2  = 21,
    urf_output_mailbox3  = 22,
    urf_output_mailbox4  = 23,
    urf_output_mailbox5  = 24,
    urf_output_mailbox6  = 25,
    urf_output_mailbox7  = 26,
    urf_output_mailbox8  = 27,
    urf_output_mailbox9  = 28,
    urf_output_mailbox10 = 29,
    urf_output_stacker1  = 30,
    urf_output_stacker2  = 31,
    urf_output_stacker3  = 32,
    urf_output_stacker4  = 33,
    urf_output_stacker5  = 34,
    urf_output_stacker6  = 35,
    urf_output_stacker7  = 36,
    urf_output_stacker8  = 37,
    urf_output_stacker9  = 38,
    urf_output_stacker10 = 39,
    urf_output_tray1     = 40,
    urf_output_tray2     = 41,
    urf_output_tray3     = 42,
    urf_output_tray4     = 43,
    urf_output_tray5     = 44,
    urf_output_tray6     = 45,
    urf_output_tray7     = 46,
    urf_output_tray8     = 47,
    urf_output_tray9     = 48,
    urf_output_tray10    = 49
} urf_output;

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_map_rgb_color(urf_gray_map_rgb_color);
static dev_proc_map_rgb_color(urf_rgb_map_rgb_color);
static dev_proc_map_color_rgb(urf_map_color_rgb);
static dev_proc_map_cmyk_color(urf_cmyk_map_cmyk_color);
static dev_proc_get_params(urf_get_params);
static dev_proc_put_params(urf_put_params);
static dev_proc_print_page(urf_print_page);
static dev_proc_dev_spec_op(urf_dev_spec_op);
dev_proc_get_color_comp_index(gx_default_DevRGB_get_color_comp_index);

static void
urfgray_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, urf_gray_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, urf_map_color_rgb);
    set_dev_proc(dev, get_params, urf_get_params);
    set_dev_proc(dev, put_params, urf_put_params);
    set_dev_proc(dev, map_cmyk_color, urf_gray_map_rgb_color);

    /* The static init used in previous versions of the code leave
     * encode_color and decode_color set to NULL (which are then rewritten
     * by the system to the default. For compatibility we do the same. */
    set_dev_proc(dev, encode_color, urf_gray_map_rgb_color);
    set_dev_proc(dev, decode_color, urf_map_color_rgb);
    set_dev_proc(dev, dev_spec_op, urf_dev_spec_op);
}

static void
urfrgb_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, urf_rgb_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, urf_map_color_rgb);
    set_dev_proc(dev, get_params, urf_get_params);
    set_dev_proc(dev, put_params, urf_put_params);
    set_dev_proc(dev, map_cmyk_color, urf_rgb_map_rgb_color);

    /* The static init used in previous versions of the code leave
     * encode_color and decode_color set to NULL (which are then rewritten
     * by the system to the default. For compatibility we do the same. */
    set_dev_proc(dev, encode_color, urf_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, urf_map_color_rgb);
    set_dev_proc(dev, dev_spec_op, urf_dev_spec_op);
}

static void
urfcmyk_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_cmyk_color, urf_cmyk_map_cmyk_color);
    set_dev_proc(dev, map_color_rgb, urf_map_color_rgb);
    set_dev_proc(dev, get_params, urf_get_params);
    set_dev_proc(dev, put_params, urf_put_params);
    set_dev_proc(dev, map_cmyk_color, urf_cmyk_map_cmyk_color);

    /* The static init used in previous versions of the code leave
     * encode_color and decode_color set to NULL (which are then rewritten
     * by the system to the default. For compatibility we do the same. */
    set_dev_proc(dev, encode_color, urf_cmyk_map_cmyk_color);
    set_dev_proc(dev, decode_color, urf_map_color_rgb);
    set_dev_proc(dev, dev_spec_op, urf_dev_spec_op);
}

/*
 * The following macro is used in get_params and put_params to determine the
 * num_components for the current device. It works using the device name
 * character after "urf" which is either '\0', 'r', or 'c'. Any new devices
 * that are added to this module must modify this macro to return the
 * correct num_components. This is needed to support the ForceMono
 * parameter, which alters dev->num_components.
 */
#define REAL_NUM_COMPONENTS(dev) (dev->dname[3] == 'c' ? 4 : \
                                  dev->dname[3] == 'r' ? 3 : 1)
struct gx_device_urf_s {
    gx_device_common;
    gx_prn_device_common;
    gx_downscaler_params downscale;
};
typedef struct gx_device_urf_s gx_device_urf;

const gx_device_urf gs_urfgray_device =
{prn_device_body(gx_device_urf, urfgray_initialize_device_procs, "urfgray",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,    /* margins */
                 1, 8, 255, 255, 256, 256, urf_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

const gx_device_urf gs_urfrgb_device =
{prn_device_body(gx_device_urf, urfrgb_initialize_device_procs, "urfrgb",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,    /* margins */
                 3, 24, 255, 255, 256, 256, urf_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

const gx_device_urf gs_urfcmyk_device =
{prn_device_body(gx_device_urf, urfcmyk_initialize_device_procs, "urfcmyk",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,    /* margins */
                 4, 32, 255, 255, 256, 256, urf_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* Map gray to color. */
static gx_color_index
urf_gray_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    int bpc = dev->color_info.depth;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_value gray = cv[0];

    return gray >> drop;
}

gx_color_index
urf_rgb_map_rgb_color(gx_device * dev, const gx_color_value cv[])
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
urf_map_color_rgb(gx_device * dev, gx_color_index color, gx_color_value cv[3])
{
    int depth = dev->color_info.depth;
    int ncomp = REAL_NUM_COMPONENTS(dev);
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    switch (ncomp) {
        case 1:         /* gray */
            cv[0] = cv[1] = cv[2] =
                (depth == 1 ? (color ? 0 : gx_max_color_value) :
                 cvalue(color));
            break;
        case 3:         /* RGB */
            {
                gx_color_index cshift = color;

                cv[2] = cvalue(cshift & mask);
                cshift >>= bpc;
                cv[1] = cvalue(cshift & mask);
                cv[0] = cvalue(cshift >> bpc);
            }
            break;
        case 4:         /* CMYK */
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
urf_cmyk_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
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

/* Get parameters.  We provide a default CRD. */
static int
urf_get_params(gx_device * pdev, gs_param_list * plist)
{
    int code, ecode;
    /*
     * The following is a hack to get the original num_components.
     * See comment above.
     */
    int real_ncomps = REAL_NUM_COMPONENTS(pdev);
    int ncomps = pdev->color_info.num_components;

    /*
     * Temporarily set num_components back to the "real" value to avoid
     * confusing those that rely on it.
     */
    pdev->color_info.num_components = real_ncomps;

    ecode = gdev_prn_get_params(pdev, plist);
    code = sample_device_crd_get_params(pdev, plist, "CRDDefault");
    if (code < 0)
            ecode = code;

    /* Restore the working num_components */
    pdev->color_info.num_components = ncomps;

    return ecode;
}

/* Set parameters.  We allow setting the number of urfs per component. */
/* Also, ForceMono=1 forces monochrome output from RGB/CMYK devices. */
static int
urf_put_params(gx_device * pdev, gs_param_list * plist)
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
    /* map back from depth to the actual urfs per component */
    static int real_bpc[17] = { 0, 1, 2, 2, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12, 16 };
    int bpc = real_bpc[pdev->color_info.depth / real_ncomps];
    const char *vname;

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
    /* Now restore/change num_components. This is done after other      */
    /* processing since it is used in gx_default_put_params             */
    pdev->color_info.num_components = ncomps;
    if (pdev->color_info.depth != save_info.depth ||
        pdev->color_info.num_components != save_info.num_components
        ) {
        gs_closedevice(pdev);
    }
    /* Reset the map_cmyk_color procedure if appropriate. */
    if (dev_proc(pdev, map_cmyk_color) == cmyk_8bit_map_cmyk_color ||
        dev_proc(pdev, map_cmyk_color) == urf_cmyk_map_cmyk_color) {
        set_dev_proc(pdev, map_cmyk_color,
                     pdev->color_info.depth == 32 ? cmyk_8bit_map_cmyk_color :
                     urf_cmyk_map_cmyk_color);
    }
    /* Reset the separable and linear shift, masks, urfs. */
    set_linear_color_bits_mask_shift(pdev);
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;

    return 0;
}

static int
put32be(int i, gp_file *prn_stream)
{
    char data[4];

    if (prn_stream == NULL)
        return 0;

    data[0] = i>>24;
    data[1] = i>>16;
    data[2] = i>>8;
    data[3] = i;

    return gp_fwrite(data, 1, 4, prn_stream);
}

static int
put8(int i, gp_file *prn_stream)
{
    if (prn_stream == NULL)
        return 0;

    return gp_fputc(i, prn_stream);
}

static int
encode_line_g8(int linecount, const byte *line, int w, gp_file *stream)
{
    int x;
    int orig_w = w;

    /* Repeat this line "linecount" times */
    put8(linecount-1, stream);

    /* First, skip back past the number of trailing empty pixels */
    while (w > 0 && line[w-1] == 0)
        w--;

    for (x = 0; x < w;)
    {
        byte match;
        int len;
        /* Count a run of non-matching pixels */
        for (match = line[x], len = 1;
             len < 128 && x+len < w && line[x+len] != match;
             len++)
            match = line[x+len];
        /* So, line[x+len-1] and line[x+len] match (or we've hit the
         * end of the row, or the 128 limit in what we can code. We
         * therefore want to code x to x+len-1 - i.e. a run of len. */
        /* RJW: Potential optimisation here; if we are terminating
         * our run at the last pixel of the row because it matches,
         * we could extend the run to include that. That might break
         * Apple's rules though. */
        if (len >= 2)
        {
            int orig_len = len;
            /* Literal pixels */
            put8(257-len, stream);
            while (len--)
                put8(line[x++], stream);
            if (x == w)
                break;
            if (orig_len == 128)
                continue;
        }
        /* Now count a run of matching pixels */
        for (match = line[x], len = 1;
             len < 128 && x+len < w && line[x+len] == match;
             len++);
        /* So, x+len is the first one that doesn't match. We therefore
         * want to code x to x+len-1 - i.e. a run of len. */
        if (len >= 1)
        {
            /* Repeated pixel */
            put8(len-1, stream);
            put8(line[x], stream);
            x += len;
        }
    }

    /* EOL */
    if (x != orig_w)
        put8(128, stream);

    return 0;
}

static int
encode_line_rgb24(int linecount, const byte *line, int w, gp_file *stream)
{
    int x;
    int orig_w = w;

    /* Repeat this line "linecount" times */
    put8(linecount-1, stream);

#define RGB(line,x) (line[3*(x)] | (line[3*(x)+1]<<8) | (line[3*(x)+2]<<16))
    /* First, skip back past the number of trailing empty pixels */
    while (w > 0 && RGB(line,w-1) == 0xFFFFFF)
        w--;

    for (x = 0; x < w;) {
        int match;
        int len;
        /* Count a run of non-matching pixels */
        for (match = RGB(line,x), len = 1;
             len < 128 && x+len < w && RGB(line,x+len) != match;
             len++)
            match = RGB(line,x+len);
        /* So, line[x+len-1] and line[x+len] match (or we've hit the
         * end of the row, or the 128 limit in what we can code. We
         * therefore want to code x to x+len-1 - i.e. a run of len. */
        /* RJW: Potential optimisation here; if we are terminating
         * our run at the last pixel of the row because it matches,
         * we could extend the run to include that. That might break
         * Apple's rules though. */
        if (len >= 2) {
            int orig_len = len;
            /* Literal pixels */
            put8(257-len, stream);
            while (len--) {
                put8(line[3*x  ], stream);
                put8(line[3*x+1], stream);
                put8(line[3*x+2], stream);
                x++;
            }
            if (x == w)
                break;
            if (orig_len == 128)
                continue;
        }
        /* Now count a run of matching pixels */
        for (match = RGB(line,x), len = 1;
             len < 128 && x+len < w && RGB(line,x+len) == match;
             len++);
#undef RGB
        /* So, x+len is the first one that doesn't match. We therefore
         * want to code x to x+len-1 - i.e. a run of len. */
        if (len >= 1)
        {
            /* Repeated pixel */
            put8(len-1, stream);
            put8(line[3*x  ], stream);
            put8(line[3*x+1], stream);
            put8(line[3*x+2], stream);
            x += len;
        }
    }

    /* EOL */
    if (x != orig_w)
        put8(128, stream);

    return 0;
}

static int
encode_line_cmyk32(int linecount, const byte *line, int w, gp_file *stream)
{
    int x;
    int orig_w = w;

    /* Repeat this line "linecount" times */
    put8(linecount-1, stream);

#define CMYK(line,x) (line[4*(x)] | (line[4*(x)+1]<<8) | (line[4*(x)+2]<<16) | (line[4*(x)+3]<<24))
    /* First, skip back past the number of trailing empty pixels */
    while (w > 0 && CMYK(line,w-1) == 0xFFFFFF)
        w--;

    for (x = 0; x < w;) {
        int match;
        int len;
        /* Count a run of non-matching pixels */
        for (match = CMYK(line,x), len = 1;
             len < 128 && x+len < w && CMYK(line,x+len) != match;
             len++)
            match = CMYK(line,x+len);
        /* So, line[x+len-1] and line[x+len] match (or we've hit the
         * end of the row, or the 128 limit in what we can code. We
         * therefore want to code x to x+len-1 - i.e. a run of len. */
        /* RJW: Potential optimisation here; if we are terminating
         * our run at the last pixel of the row because it matches,
         * we could extend the run to include that. That might break
         * Apple's rules though. */
        if (len >= 2) {
            int orig_len = len;
            /* Literal pixels */
            put8(257-len, stream);
            while (len--) {
                put8(line[4*x  ], stream);
                put8(line[4*x+1], stream);
                put8(line[4*x+2], stream);
                put8(line[4*x+3], stream);
                x++;
            }
            if (x == w)
                break;
            if (orig_len == 128)
                continue;
        }
        /* Now count a run of matching pixels */
        for (match = CMYK(line,x), len = 1;
             len < 128 && x+len < w && CMYK(line,x+len) == match;
             len++);
#undef CMYK
        /* So, x+len is the first one that doesn't match. We therefore
         * want to code x to x+len-1 - i.e. a run of len. */
        if (len >= 1)
        {
            /* Repeated pixel */
            put8(len-1, stream);
            put8(line[4*x  ], stream);
            put8(line[4*x+1], stream);
            put8(line[4*x+2], stream);
            put8(line[4*x+3], stream);
            x += len;
        }
    }

    /* EOL */
    if (x != orig_w)
        put8(128, stream);

    return 0;
}

/* Send the page to the printer. */
static int
urf_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{                               /* Just dump the urfs on the file. */
    /* If the file is 'nul', don't even do the writes. */
    gx_device_urf *dev = (gx_device_urf *)pdev;
    int nul = !strcmp(pdev->fname, "nul") || !strcmp(pdev->fname, "/dev/null");
    int src_width = pdev->width;
    int src_height = pdev->height;
    int i;
    int num_comps = pdev->color_info.num_components;
    int src_bpc = pdev->color_info.depth / num_comps;
    int dst_bpc = src_bpc;
    int factor = dev->downscale.downscale_factor;
    int dst_width  = gx_downscaler_scale_rounded(src_width,  factor);
    int dst_height = gx_downscaler_scale_rounded(src_height, factor);
    int dst_res    = gx_downscaler_scale_rounded((int)pdev->HWResolution[0], factor);
    gx_downscaler_t ds;
    int line_size = (dst_width * num_comps * dst_bpc + 31) & ~31;
    byte *linebuf, *line, *prev;
    int matching_lines = 0;
    int (*encode_line)(int, const byte *, int, gp_file *);
    int code;

    if (nul)
        prn_stream = NULL;

    if (dst_bpc == 8 && num_comps == 1)
        encode_line = encode_line_g8;
    else if (dst_bpc == 8 && num_comps == 3)
        encode_line = encode_line_rgb24;
    else if (dst_bpc == 8 && num_comps == 4)
        encode_line = encode_line_cmyk32;
    else
        return_error(gs_error_rangecheck);

    linebuf = gs_alloc_bytes(pdev->memory, line_size*2, "urf_print_page(in)");
    if (linebuf == 0)
        return_error(gs_error_VMerror);
    memset(linebuf, 0, 2*line_size);
    line = linebuf;
    prev = linebuf + line_size;

    code = gx_downscaler_init(&ds, (gx_device *)pdev,
                              src_bpc, dst_bpc, num_comps,
                              &dev->downscale,
                              NULL, /*&fax_adjusted_width*/
                              0 /*aw*/);

    /* Only output the file header at the start of the stream. */
    if (code >= 0 && prn_stream)
    {
        if (gp_ftell(prn_stream) == 0)
        {
            /* File header */
            gp_fwrite("UNIRAST", 1, 8, prn_stream);
            put32be(0, prn_stream); /* Don't know how many pages */
        }
        /* Page header */
        /* bitsPerPixel */
        put8(pdev->color_info.depth, prn_stream);
        /* colorSpace */
        put8(num_comps == 1 ? urf_cs_DeviceW :
             num_comps == 3 ? urf_cs_DeviceRGB :
             urf_cs_DeviceCMYK, prn_stream);
        /* duplexMode */
        put8(urf_duplex_auto, prn_stream);
        /* printQuality */
        put8(urf_quality_auto, prn_stream);
        /* mediaType */
        put8(urf_media_auto, prn_stream);
        /* inputSlot */
        put8(urf_input_auto, prn_stream);
        /* outputTray */
        put8(urf_output_auto, prn_stream);
        /* copies */
        put8(0 /* default */, prn_stream);
        /* finishings */
        put32be(0, prn_stream);
        /* width */
        put32be(dst_width, prn_stream);
        /* height */
        put32be(dst_height, prn_stream);
        /* resolution */
        put32be(dst_res, prn_stream);
        /* reserved */
        put32be(0, prn_stream);
        put32be(0, prn_stream);
    }

    /* Page bitmap */
    for (i = 0; code >= 0 && i < dst_height; i++) {
        code = gx_downscaler_getbits(&ds, line, i);
        if (code < 0)
            break;
        if (matching_lines > 0)
        {
            if (memcmp(line, prev, line_size) == 0)
            {
                matching_lines++;
                if (matching_lines == 256) {
                    code = encode_line(matching_lines, prev, dst_width, prn_stream);
                    matching_lines = 0;
                }
            }
            else
            {
                code = encode_line(matching_lines, prev, dst_width, prn_stream);
                matching_lines = 1;
            }
        } else
            matching_lines = 1;
        /* Swap the line buffers */
        {
            byte *swap = line; line = prev; prev = swap;
        }
    }
    /* Encode any trailing line */
    if (code >= 0 && matching_lines)
        code = encode_line(matching_lines, prev, dst_width, prn_stream);
    gs_free_object(pdev->memory, linebuf, "urf_print_page(in)");
    return code;
}

static int
urf_dev_spec_op(gx_device *pdev, int dso, void *ptr, int size)
{
    switch (dso)
    {
    case gxdso_is_encoding_direct:
        if (pdev->color_info.depth != 8 * pdev->color_info.num_components)
            return 0;
        return (dev_proc(pdev, encode_color) == urf_rgb_map_rgb_color ||
                dev_proc(pdev, encode_color) == urf_cmyk_map_cmyk_color);
    }

    return gdev_prn_dev_spec_op(pdev, dso, ptr, size);
}
