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


/* tiffgray device:  8-bit Gray uncompressed TIFF device
 * tiff32nc device:  32-bit CMYK uncompressed TIFF device
 * tiffsep device:   Generate individual TIFF gray files for each separation
 *                   as well as a 'composite' 32-bit CMYK for the page.
 * tiffsep1 device:  Generate individual TIFF 1-bit files for each separation
 * tiffscaled device:Mono TIFF device (error-diffused downscaled output from
 *                   8-bit Gray internal rendering)
 * tiffscaled8 device:Greyscale TIFF device (downscaled output from
 *                   8-bit Gray internal rendering)
 * tiffscaled24 device:24-bit RGB TIFF device (dithered downscaled output
 *                   from 24-bit RGB internal rendering)
 * tiffscaled32 device:32-bit CMYK TIFF device (downscaled output
 *                   from 32-bit CMYK internal rendering)
 * tiffscaled4 device:4-bit CMYK TIFF device (dithered downscaled output
 *                   from 32-bit CMYK internal rendering)
 */

#include "stdint_.h"   /* for tiff.h */
#include "gdevtifs.h"
#include "gdevprn.h"
#include "gdevdevn.h"
#include "gsequivc.h"
#include "gxdht.h"
#include "gxiodev.h"
#include "gzht.h"
#include "stdio_.h"
#include "ctype_.h"
#include "gxgetbit.h"
#include "gdevppla.h"
#include "gxdownscale.h"
#include "gp.h"
#include "gstiffio.h"
#include "gscms.h"
#include "gsicc_cache.h"
#include "gxdevsop.h"
#include "gsicc.h"
#ifdef WITH_CAL
#include "cal.h"
#endif

/*
 * Some of the code in this module is based upon the gdevtfnx.c module.
 * gdevtfnx.c has the following message:
 * Thanks to Alan Barclay <alan@escribe.co.uk> for donating the original
 * version of this code to Ghostscript.
 */

/* ------ The device descriptors ------ */

/* Default X and Y resolution */
#define X_DPI 72
#define Y_DPI 72

/* ------ The tiffgray device ------ */

static dev_proc_print_page(tiffgray_print_page);

/* FIXME: From initial analysis this is NOT safe for bg_printing, but might be fixable */

static const gx_device_procs tiffgray_procs =
prn_color_params_procs(tiff_open, gdev_prn_output_page_seekable, tiff_close,
                gx_default_gray_map_rgb_color, gx_default_gray_map_color_rgb,
                tiff_get_params, tiff_put_params);

const gx_device_tiff gs_tiffgray_device = {
    prn_device_body(gx_device_tiff, tiffgray_procs, "tiffgray",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    X_DPI, Y_DPI,
                    0, 0, 0, 0, /* Margins */
                    1, 8, 255, 0, 256, 0, tiffgray_print_page),
    ARCH_IS_BIG_ENDIAN          /* default to native endian (i.e. use big endian iff the platform is so*/,
    false,                      /* default to *not* bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0
};

static int
tiffscaled_spec_op(gx_device *dev_, int op, void *data, int datasize)
{
    if (op == gxdso_supports_iccpostrender) {
        return true;
    }
    return gdev_prn_dev_spec_op(dev_, op, data, datasize);
}

/* ------ The tiffscaled device ------ */

dev_proc_open_device(tiff_open_s);
static dev_proc_print_page(tiffscaled_print_page);
static int tiff_set_icc_color_fields(gx_device_printer *pdev);

static const gx_device_procs tiffscaled_procs =
prn_color_params_procs(tiff_open,
                       gdev_prn_output_page_seekable,
                       tiff_close,
                       gx_default_gray_map_rgb_color,
                       gx_default_gray_map_color_rgb,
                       tiff_get_params_downscale,
                       tiff_put_params_downscale);

const gx_device_tiff gs_tiffscaled_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled_procs,
                    "tiffscaled",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    1,          /* num components */
                    8,          /* bits per sample */
                    255, 0, 256, 0,
                    tiffscaled_print_page),
    ARCH_IS_BIG_ENDIAN,/* default to native endian (i.e. use big endian iff the platform is so */
    false,             /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0
};

/* ------ The tiffscaled8 device ------ */

static dev_proc_print_page(tiffscaled8_print_page);

static const gx_device_procs tiffscaled8_procs = {
    tiff_open_s, NULL, NULL, gdev_prn_output_page_seekable, tiff_close,
    gx_default_gray_map_rgb_color, gx_default_gray_map_color_rgb, NULL, NULL,
    NULL, NULL, NULL, NULL, tiff_get_params_downscale, tiff_put_params_downscale,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, tiffscaled_spec_op
};

const gx_device_tiff gs_tiffscaled8_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled8_procs,
                    "tiffscaled8",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    1,          /* num components */
                    8,          /* bits per sample */
                    255, 0, 256, 0,
                    tiffscaled8_print_page),
    ARCH_IS_BIG_ENDIAN,/* default to native endian (i.e. use big endian iff the platform is so */
    false,             /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0
};

/* ------ The tiffscaled24 device ------ */

static dev_proc_print_page(tiffscaled24_print_page);

static const gx_device_procs tiffscaled24_procs = {
    tiff_open_s, NULL, NULL, gdev_prn_output_page_seekable, tiff_close,
    gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb, NULL, NULL,
    NULL, NULL, NULL, NULL, tiff_get_params_downscale, tiff_put_params_downscale,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, tiffscaled_spec_op
};

const gx_device_tiff gs_tiffscaled24_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled24_procs,
                    "tiffscaled24",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    3,          /* num components */
                    24,         /* bits per sample */
                    255, 255, 256, 256,
                    tiffscaled24_print_page),
    ARCH_IS_BIG_ENDIAN,/* default to native endian (i.e. use big endian iff the platform is so */
    false,             /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0
};

/* ------ The tiffscaled32 device ------ */

static dev_proc_print_page(tiffscaled32_print_page);

static const gx_device_procs tiffscaled32_procs = {
    tiff_open_s, NULL, NULL, gdev_prn_output_page_seekable, tiff_close,
    NULL, cmyk_8bit_map_color_cmyk, NULL, NULL, NULL, NULL, NULL, NULL,
    tiff_get_params_downscale_cmyk, tiff_put_params_downscale_cmyk,
    cmyk_8bit_map_cmyk_color, NULL, NULL, NULL, gx_page_device_get_page_device,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, tiffscaled_spec_op
};

const gx_device_tiff gs_tiffscaled32_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled32_procs,
                    "tiffscaled32",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    4,          /* num components */
                    32,         /* bits per sample */
                    255, 255, 256, 256,
                    tiffscaled32_print_page),
    ARCH_IS_BIG_ENDIAN,/* default to native endian (i.e. use big endian iff the platform is so */
    false,             /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0
};

/* ------ The tiffscaled4 device ------ */

static dev_proc_print_page(tiffscaled4_print_page);

static const gx_device_procs tiffscaled4_procs = {
    tiff_open, NULL, NULL, gdev_prn_output_page_seekable, tiff_close,
    NULL, cmyk_8bit_map_color_cmyk, NULL, NULL, NULL, NULL, NULL, NULL,
    tiff_get_params_downscale_cmyk_ets, tiff_put_params_downscale_cmyk_ets,
    cmyk_8bit_map_cmyk_color, NULL, NULL, NULL, gx_page_device_get_page_device
};

const gx_device_tiff gs_tiffscaled4_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled4_procs,
                    "tiffscaled4",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    4,          /* num components */
                    32,         /* bits per sample */
                    255, 255, 256, 256,
                    tiffscaled4_print_page),
    ARCH_IS_BIG_ENDIAN,/* default to native endian (i.e. use big endian iff the platform is so */
    false,             /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0
};

/* ------ Private functions ------ */

static void
tiff_set_gray_fields(gx_device_printer *pdev, TIFF *tif,
                     unsigned short bits_per_sample,
                     int compression,
                     long max_strip_size)
{
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);

    tiff_set_compression(pdev, tif, compression, max_strip_size);
}

static int
tiffgray_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    if (!tfdev->UseBigTIFF && tfdev->Compression==COMPRESSION_NONE &&
        pdev->height > ((unsigned long) 0xFFFFFFFF - gp_ftell(file))/(pdev->width)) /* note width is never 0 in print_page */
        return_error(gs_error_rangecheck);  /* this will overflow 32 bits */

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    tiff_set_gray_fields(pdev, tfdev->tif, 8, tfdev->Compression, tfdev->MaxStripSize);

    return tiff_print_page(pdev, tfdev->tif, 0);
}

static int
tiffscaled_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    tiff_set_gray_fields(pdev, tfdev->tif, 1, tfdev->Compression,
        tfdev->MaxStripSize);


    return tiff_downscale_and_print_page(pdev, tfdev->tif,
                                         tfdev->downscale.downscale_factor,
                                         tfdev->downscale.min_feature_size,
                                         tfdev->AdjustWidth,
                                         1, 1,
                                         0, 0, NULL,
                                         tfdev->downscale.ets);
}

static int
tiffscaled8_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    if (tfdev->icclink != NULL && tfdev->icclink->num_output != 1)
    {
        code = tiff_set_icc_color_fields(pdev);
        if (code < 0)
            return code;
    } else {
        tiff_set_gray_fields(pdev, tfdev->tif, 8, tfdev->Compression,
            tfdev->MaxStripSize);
    }
    return tiff_downscale_and_print_page(pdev, tfdev->tif,
                                         tfdev->downscale.downscale_factor,
                                         tfdev->downscale.min_feature_size,
                                         tfdev->AdjustWidth,
                                         8, 1,
                                         0, 0, NULL,
                                         0);
}

static void
tiff_set_rgb_fields(gx_device_tiff *tfdev)
{
    cmm_profile_t *icc_profile;

    if (tfdev->icc_struct->postren_profile != NULL)
        icc_profile = tfdev->icc_struct->postren_profile;
    else if (tfdev->icc_struct->oi_profile != NULL)
        icc_profile = tfdev->icc_struct->oi_profile;
    else
        icc_profile = tfdev->icc_struct->device_profile[0];

    switch (icc_profile->data_cs) {
        case gsRGB:
            TIFFSetField(tfdev->tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            break;
        case gsCIELAB:
            TIFFSetField(tfdev->tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ICCLAB);
            break;
        default:
            TIFFSetField(tfdev->tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            break;
    }
    TIFFSetField(tfdev->tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tfdev->tif, TIFFTAG_SAMPLESPERPIXEL, 3);

    tiff_set_compression((gx_device_printer *)tfdev, tfdev->tif,
                         tfdev->Compression, tfdev->MaxStripSize);
}


static int
tiffscaled24_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    if (tfdev->icclink != NULL && tfdev->icclink->num_output != 3) {
        code = tiff_set_icc_color_fields(pdev);
        if (code < 0)
            return code;
    } else {
        TIFFSetField(tfdev->tif, TIFFTAG_BITSPERSAMPLE, 8);
        tiff_set_rgb_fields(tfdev);
    }

    return tiff_downscale_and_print_page(pdev, tfdev->tif,
                                         tfdev->downscale.downscale_factor,
                                         tfdev->downscale.min_feature_size,
                                         tfdev->AdjustWidth,
                                         8, 3,
                                         0, 0, NULL,
                                         0);
}

static void
tiff_set_cmyk_fields(gx_device_printer *pdev, TIFF *tif,
                     short bits_per_sample,
                     uint16 compression,
                     long max_strip_size)
{
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
    TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);

    tiff_set_compression(pdev, tif, compression, max_strip_size);
}

static int
tiffscaled32_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    if (tfdev->icclink != NULL && tfdev->icclink->num_output != 4)
    {
        code = tiff_set_icc_color_fields(pdev);
        if (code < 0)
            return code;
    } else {
        tiff_set_cmyk_fields(pdev, tfdev->tif, 8, tfdev->Compression,
            tfdev->MaxStripSize);
    }

    return tiff_downscale_and_print_page(pdev, tfdev->tif,
                                         tfdev->downscale.downscale_factor,
                                         tfdev->downscale.min_feature_size,
                                         tfdev->AdjustWidth,
                                         8, 4,
                                         tfdev->downscale.trap_w, tfdev->downscale.trap_h,
                                         tfdev->downscale.trap_order,
                                         0);
}

static int
tiffscaled4_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    tiff_set_cmyk_fields(pdev,
                         tfdev->tif,
                         1,
                         tfdev->Compression,
                         tfdev->MaxStripSize);

    return tiff_downscale_and_print_page(pdev, tfdev->tif,
                                         tfdev->downscale.downscale_factor,
                                         tfdev->downscale.min_feature_size,
                                         tfdev->AdjustWidth,
                                         1, 4,
                                         tfdev->downscale.trap_w, tfdev->downscale.trap_h,
                                         tfdev->downscale.trap_order,
                                         tfdev->downscale.ets);
}

/* Called when the post render ICC profile is in a different color space
* type compared to the output ICC profile (e.g. cmyk vs rgb) */
static int
tiff_set_icc_color_fields(gx_device_printer *pdev)
{
    gx_device_tiff *tfdev = (gx_device_tiff *)pdev;

    TIFFSetField(tfdev->tif, TIFFTAG_BITSPERSAMPLE, 8);
    switch (tfdev->icclink->num_output)
    {
    case 1:
        tiff_set_gray_fields(pdev, tfdev->tif, 8, tfdev->Compression,
            tfdev->MaxStripSize);
        break;
    case 3:
        tiff_set_rgb_fields(tfdev);
        break;
    case 4:
        tiff_set_cmyk_fields(pdev, tfdev->tif,
            pdev->color_info.depth / pdev->color_info.num_components,
            tfdev->Compression, tfdev->MaxStripSize);
        break;
    default:
        return gs_error_undefined;
    }
    return 0;
}

static int
tiffsep_spec_op(gx_device *dev_, int op, void *data, int datasize)
{
    if (op == gxdso_supports_iccpostrender || op == gxdso_supports_devn) {
        return true;
    }
    return gdev_prn_dev_spec_op(dev_, op, data, datasize);
}

/* ------ The cmyk devices ------ */

static dev_proc_print_page(tiffcmyk_print_page);

#define cmyk_procs(p_map_color_rgb, p_map_cmyk_color)\
    tiff_open, NULL, NULL, gdev_prn_output_page_seekable, tiff_close,\
    NULL, p_map_color_rgb, NULL, NULL, NULL, NULL, NULL, NULL,\
    tiff_get_params, tiff_put_params,\
    p_map_cmyk_color, NULL, NULL, NULL, gx_page_device_get_page_device

/* 8-bit-per-plane separated CMYK color. */

static const gx_device_procs tiffcmyk_procs = {
    cmyk_procs(cmyk_8bit_map_color_cmyk, cmyk_8bit_map_cmyk_color)
};

const gx_device_tiff gs_tiff32nc_device = {
    prn_device_body(gx_device_tiff, tiffcmyk_procs, "tiff32nc",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    X_DPI, Y_DPI,
                    0, 0, 0, 0, /* Margins */
                    4, 32, 255, 255, 256, 256, tiffcmyk_print_page),
    ARCH_IS_BIG_ENDIAN          /* default to native endian (i.e. use big endian iff the platform is so*/,
    false,                      /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0
};

/* 16-bit-per-plane separated CMYK color. */

static const gx_device_procs tiff64nc_procs = {
    cmyk_procs(cmyk_16bit_map_color_cmyk, cmyk_16bit_map_cmyk_color)
};

const gx_device_tiff gs_tiff64nc_device = {
    prn_device_body(gx_device_tiff, tiff64nc_procs, "tiff64nc",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    X_DPI, Y_DPI,
                    0, 0, 0, 0, /* Margins */
                    4, 64, 255, 255, 256, 256, tiffcmyk_print_page),
    ARCH_IS_BIG_ENDIAN          /* default to native endian (i.e. use big endian iff the platform is so*/,
    false,                      /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    false, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0
};

/* ------ Private functions ------ */

static int
tiffcmyk_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    if (!tfdev->UseBigTIFF && tfdev->Compression==COMPRESSION_NONE &&
        pdev->height > ((unsigned long) 0xFFFFFFFF - gp_ftell(file))/(pdev->width)) /* note width is never 0 in print_page */
        return_error(gs_error_rangecheck);  /* this will overflow 32 bits */

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    tiff_set_cmyk_fields(pdev,
                         tfdev->tif,
                         pdev->color_info.depth / pdev->color_info.num_components,
                         tfdev->Compression,
                         tfdev->MaxStripSize);

    return tiff_print_page(pdev, tfdev->tif, 0);
}

/* ----------  The tiffsep device ------------ */

#define NUM_CMYK_COMPONENTS 4
#define MAX_COLOR_VALUE 255             /* We are using 8 bits per colorant */

/* The device descriptor */
static dev_proc_open_device(tiffsep_prn_open);
static dev_proc_close_device(tiffsep_prn_close);
static dev_proc_get_params(tiffsep_get_params);
static dev_proc_put_params(tiffsep_put_params);
static dev_proc_print_page(tiffsep_print_page);
static dev_proc_get_color_mapping_procs(tiffsep_get_color_mapping_procs);
static dev_proc_get_color_comp_index(tiffsep_get_color_comp_index);
static dev_proc_encode_color(tiffsep_encode_color);
static dev_proc_decode_color(tiffsep_decode_color);
static dev_proc_update_spot_equivalent_colors(tiffsep_update_spot_equivalent_colors);
static dev_proc_ret_devn_params(tiffsep_ret_devn_params);
static dev_proc_open_device(tiffsep1_prn_open);
static dev_proc_close_device(tiffsep1_prn_close);
static dev_proc_put_params(tiffsep1_put_params);
static dev_proc_print_page(tiffsep1_print_page);
static dev_proc_fill_path(sep1_fill_path);

/* common to tiffsep and tiffsepo1 */
#define tiffsep_devices_common\
    gx_device_common;\
    gx_prn_device_common;\
        /* tiff state for separation files */\
    gp_file *sep_file[GX_DEVICE_COLOR_MAX_COMPONENTS];\
    TIFF *tiff[GX_DEVICE_COLOR_MAX_COMPONENTS]; \
    bool  NoSeparationFiles;    /* true = no separation files created only a composite file */\
    bool  BigEndian;            /* true = big endian; false = little endian */\
    bool  UseBigTIFF;           /* true = output bigtiff, false don't */ \
    bool  write_datetime;       /* true = write DATETIME tag, false = don't */ \
    bool  PrintSpotCMYK;        /* true = print CMYK equivalents for spot inks; false = do nothing */\
    uint16 Compression;         /* for the separation files, same values as TIFFTAG_COMPRESSION */\
    long MaxStripSize;\
    long BitsPerComponent;\
    int max_spots;\
    bool lock_colorants;\
    gx_downscaler_params downscale;\
    gs_devn_params devn_params;         /* DeviceN generated parameters */\
    equivalent_cmyk_color_params equiv_cmyk_colors;\
    bool warning_given		/* avoid issuing lots of warnings */

/*
 * A structure definition for a DeviceN type device
 */
typedef struct tiffsep_device_s {
    tiffsep_devices_common;
    gp_file *comp_file;            /* Underlying file for tiff_comp */
    TIFF *tiff_comp;            /* tiff file for comp file */
    gsicc_link_t *icclink;      /* link profile if we are doing post rendering */
} tiffsep_device;

/* threshold array structure */
typedef struct threshold_array_s {
      int dheight, dwidth;
      byte *dstart;
} threshold_array_t;

typedef struct tiffsep1_device_s {
    tiffsep_devices_common;
    threshold_array_t thresholds[GX_DEVICE_COLOR_MAX_COMPONENTS + 1]; /* one extra for Default */
    dev_t_proc_fill_path((*fill_path), gx_device); /* we forward to here */
} tiffsep1_device;

/* GC procedures */
static
ENUM_PTRS_WITH(tiffsep_device_enum_ptrs, tiffsep_device *pdev)
{
    if (index < pdev->devn_params.separations.num_separations)
        ENUM_RETURN(pdev->devn_params.separations.names[index].data);
    ENUM_PREFIX(st_device_printer,
                    pdev->devn_params.separations.num_separations);
    return 0;
}
ENUM_PTRS_END

static RELOC_PTRS_WITH(tiffsep_device_reloc_ptrs, tiffsep_device *pdev)
{
    RELOC_PREFIX(st_device_printer);
    {
        int i;

        for (i = 0; i < pdev->devn_params.separations.num_separations; ++i) {
            RELOC_PTR(tiffsep_device, devn_params.separations.names[i].data);
        }
    }
}
RELOC_PTRS_END

/* Even though tiffsep_device_finalize is the same as gx_device_finalize, */
/* we need to implement it separately because st_composite_final */
/* declares all 3 procedures as private. */
static void
tiffsep_device_finalize(const gs_memory_t *cmem, void *vpdev)
{
    /* We need to deallocate the names. */
    devn_free_params((gx_device*) vpdev);
    gx_device_finalize(cmem, vpdev);
}

gs_private_st_composite_final(st_tiffsep_device, tiffsep_device,
    "tiffsep_device", tiffsep_device_enum_ptrs, tiffsep_device_reloc_ptrs,
    tiffsep_device_finalize);

/*
 * Macro definition for tiffsep device procedures
 */
#define sep_device_procs(open, close, encode_color, decode_color, update_spot_colors,put_params, fill_path) \
{       open,\
        gx_default_get_initial_matrix,\
        NULL,                           /* sync_output */\
        gdev_prn_output_page_seekable,  /* output_page */\
        close,                          /* close */\
        NULL,                           /* map_rgb_color - not used */\
        tiffsep_decode_color,           /* map_color_rgb */\
        NULL,                           /* fill_rectangle */\
        NULL,                           /* tile_rectangle */\
        NULL,                           /* copy_mono */\
        NULL,                           /* copy_color */\
        NULL,                           /* draw_line */\
        NULL,                           /* get_bits */\
        tiffsep_get_params,             /* get_params */\
        put_params,                     /* put_params */\
        NULL,                           /* map_cmyk_color - not used */\
        NULL,                           /* get_xfont_procs */\
        NULL,                           /* get_xfont_device */\
        NULL,                           /* map_rgb_alpha_color */\
        gx_page_device_get_page_device, /* get_page_device */\
        NULL,                           /* get_alpha_bits */\
        NULL,                           /* copy_alpha */\
        NULL,                           /* get_band */\
        NULL,                           /* copy_rop */\
        fill_path,                      /* fill_path */\
        NULL,                           /* stroke_path */\
        NULL,                           /* fill_mask */\
        NULL,                           /* fill_trapezoid */\
        NULL,                           /* fill_parallelogram */\
        NULL,                           /* fill_triangle */\
        NULL,                           /* draw_thin_line */\
        NULL,                           /* begin_image */\
        NULL,                           /* image_data */\
        NULL,                           /* end_image */\
        NULL,                           /* strip_tile_rectangle */\
        NULL,                           /* strip_copy_rop */\
        NULL,                           /* get_clipping_box */\
        NULL,                           /* begin_typed_image */\
        NULL,                           /* get_bits_rectangle */\
        NULL,                           /* map_color_rgb_alpha */\
        NULL,                           /* create_compositor */\
        NULL,                           /* get_hardware_params */\
        NULL,                           /* text_begin */\
        NULL,                           /* finish_copydevice */\
        NULL,                           /* begin_transparency_group */\
        NULL,                           /* end_transparency_group */\
        NULL,                           /* begin_transparency_mask */\
        NULL,                           /* end_transparency_mask */\
        NULL,                           /* discard_transparency_layer */\
        tiffsep_get_color_mapping_procs,/* get_color_mapping_procs */\
        tiffsep_get_color_comp_index,   /* get_color_comp_index */\
        encode_color,                   /* encode_color */\
        decode_color,                   /* decode_color */\
        NULL,                           /* pattern_manage */\
        NULL,                           /* fill_rectangle_hl_color */\
        NULL,                           /* include_color_space */\
        NULL,                           /* fill_linear_color_scanline */\
        NULL,                           /* fill_linear_color_trapezoid */\
        NULL,                           /* fill_linear_color_triangle */\
        update_spot_colors,             /* update_spot_equivalent_colors */\
        tiffsep_ret_devn_params,         /* ret_devn_params */\
        NULL,                           /* fillpage */\
        NULL,                           /* push_transparency_state */\
        NULL,                           /* pop_transparency_state */\
        NULL,                           /* put_image */\
        tiffsep_spec_op                 /* dev_spec_op */\
}


#define tiffsep_devices_body(dtype, procs, dname, ncomp, pol, depth, mg, mc, sl, cn, print_page, compr)\
    std_device_full_body_type_extended(dtype, &procs, dname,\
          &st_tiffsep_device,\
          (int)((long)(DEFAULT_WIDTH_10THS) * (X_DPI) / 10),\
          (int)((long)(DEFAULT_HEIGHT_10THS) * (Y_DPI) / 10),\
          X_DPI, Y_DPI,\
          ncomp,                /* MaxComponents */\
          ncomp,                /* NumComp */\
          pol,                  /* Polarity */\
          depth, 0,             /* Depth, GrayIndex */\
          mg, mc,               /* MaxGray, MaxColor */\
          mg + 1, mc + 1,       /* DitherGray, DitherColor */\
          sl,                   /* Linear & Separable? */\
          cn,                   /* Process color model name */\
          0, 0,                 /* offsets */\
          0, 0, 0, 0            /* margins */\
        ),\
        prn_device_body_rest_(print_page),\
        { 0 },                  /* tiff state for separation files */\
        { 0 },                  /* separation files */\
        false,                  /* NoSeparationFiles */\
        ARCH_IS_BIG_ENDIAN      /* true = big endian; false = little endian */,\
        false,                  /* UseBigTIFF */\
        true,                   /* write_datetime */ \
        false,                  /* PrintSpotCMYK */\
        compr                   /* COMPRESSION_* */,\
        TIFF_DEFAULT_STRIP_SIZE,/* MaxStripSize */\
        8,                      /* BitsPerComponent */\
        GS_SOFT_MAX_SPOTS,      /* max_spots */\
        false,                  /* Colorants not locked */\
        GX_DOWNSCALER_PARAMS_DEFAULTS

#define GCIB (ARCH_SIZEOF_GX_COLOR_INDEX * 8)

/*
 * TIFF devices with CMYK process color model and spot color support.
 */
static const gx_device_procs spot_cmyk_procs =
                sep_device_procs(tiffsep_prn_open, tiffsep_prn_close, tiffsep_encode_color, tiffsep_decode_color,
                                tiffsep_update_spot_equivalent_colors, tiffsep_put_params, NULL);

static const gx_device_procs spot1_cmyk_procs =
                sep_device_procs(tiffsep1_prn_open, tiffsep1_prn_close, tiffsep_encode_color, tiffsep_decode_color,
                                tiffsep_update_spot_equivalent_colors, tiffsep1_put_params, sep1_fill_path);

const tiffsep_device gs_tiffsep_device =
{
    tiffsep_devices_body(tiffsep_device, spot_cmyk_procs, "tiffsep", ARCH_SIZEOF_GX_COLOR_INDEX, GX_CINFO_POLARITY_SUBTRACTIVE, GCIB, MAX_COLOR_VALUE, MAX_COLOR_VALUE, GX_CINFO_SEP_LIN, "DeviceCMYK", tiffsep_print_page, COMPRESSION_LZW),
    /* devn_params specific parameters */
    { 8,                        /* Ignored - Bits per color */
      DeviceCMYKComponents,     /* Names of color model colorants */
      4,                        /* Number colorants for CMYK */
      0,                        /* MaxSeparations has not been specified */
      -1,                       /* PageSpotColors has not been specified */
      {0},                      /* SeparationNames */
      0,                        /* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 } /* Initial component SeparationOrder */
    },
    { true },                   /* equivalent CMYK colors for spot colors */
    false,			/* warning_given */
};

const tiffsep1_device gs_tiffsep1_device =
{
    tiffsep_devices_body(tiffsep1_device, spot1_cmyk_procs, "tiffsep1", ARCH_SIZEOF_GX_COLOR_INDEX, GX_CINFO_POLARITY_SUBTRACTIVE, GCIB, MAX_COLOR_VALUE, MAX_COLOR_VALUE, GX_CINFO_SEP_LIN, "DeviceCMYK", tiffsep1_print_page, COMPRESSION_CCITTFAX4),
    /* devn_params specific parameters */
    { 8,                        /* Ignored - Bits per color */
      DeviceCMYKComponents,     /* Names of color model colorants */
      4,                        /* Number colorants for CMYK */
      0,                        /* MaxSeparations has not been specified */
      -1,                       /* PageSpotColors has not been specified */
      {0},                      /* SeparationNames */
      0,                        /* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 } /* Initial component SeparationOrder */
    },
    { true },                   /* equivalent CMYK colors for spot colors */
    false,                      /* warning_given */
    { {0} },                    /* threshold arrays */
    0,                          /* fill_path */
};

#undef NC
#undef SL
#undef ENCODE_COLOR
#undef DECODE_COLOR

static const uint32_t bit_order[32]={
#if ARCH_IS_BIG_ENDIAN
        0x80000000, 0x40000000, 0x20000000, 0x10000000, 0x08000000, 0x04000000, 0x02000000, 0x01000000,
        0x00800000, 0x00400000, 0x00200000, 0x00100000, 0x00080000, 0x00040000, 0x00020000, 0x00010000,
        0x00008000, 0x00004000, 0x00002000, 0x00001000, 0x00000800, 0x00000400, 0x00000200, 0x00000100,
        0x00000080, 0x00000040, 0x00000020, 0x00000010, 0x00000008, 0x00000004, 0x00000002, 0x00000001
#else
        0x00000080, 0x00000040, 0x00000020, 0x00000010, 0x00000008, 0x00000004, 0x00000002, 0x00000001,
        0x00008000, 0x00004000, 0x00002000, 0x00001000, 0x00000800, 0x00000400, 0x00000200, 0x00000100,
        0x00800000, 0x00400000, 0x00200000, 0x00100000, 0x00080000, 0x00040000, 0x00020000, 0x00010000,
        0x80000000, 0x40000000, 0x20000000, 0x10000000, 0x08000000, 0x04000000, 0x02000000, 0x01000000
#endif
    };

/*
 * The following procedures are used to map the standard color spaces into
 * the color components for the tiffsep device.
 */
static void
tiffsep_gray_cs_to_cm(gx_device * dev, frac gray, frac out[])
{
    int * map = ((tiffsep_device *) dev)->devn_params.separation_order_map;

    gray_cs_to_devn_cm(dev, map, gray, out);
}

static void
tiffsep_rgb_cs_to_cm(gx_device * dev, const gs_gstate *pgs,
                                   frac r, frac g, frac b, frac out[])
{
    int * map = ((tiffsep_device *) dev)->devn_params.separation_order_map;

    rgb_cs_to_devn_cm(dev, map, pgs, r, g, b, out);
}

static void
tiffsep_cmyk_cs_to_cm(gx_device * dev,
                frac c, frac m, frac y, frac k, frac out[])
{
    const gs_devn_params *devn = tiffsep_ret_devn_params(dev);
    const int *map = devn->separation_order_map;
    int j;

    if (devn->num_separation_order_names > 0) {

        /* We need to make sure to clear everything */
        for (j = 0; j < dev->color_info.num_components; j++)
            out[j] = frac_0;

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

static const gx_cm_color_map_procs tiffsep_cm_procs = {
    tiffsep_gray_cs_to_cm,
    tiffsep_rgb_cs_to_cm,
    tiffsep_cmyk_cs_to_cm
};

/*
 * These are the handlers for returning the list of color space
 * to color model conversion routines.
 */
static const gx_cm_color_map_procs *
tiffsep_get_color_mapping_procs(const gx_device * dev)
{
    return &tiffsep_cm_procs;
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
 * With 32 bit gx_color_index values, we simply pack values.
 */
static gx_color_index
tiffsep_encode_color(gx_device *dev, const gx_color_value colors[])
{
    int bpc = ((tiffsep_device *)dev)->devn_params.bitspercomponent;
    gx_color_index color = 0;
    int i = 0;
    int ncomp = dev->color_info.num_components;
    COLROUND_VARS;

    COLROUND_SETUP(bpc);
    for (; i < ncomp; i++) {
        color <<= bpc;
        color |= COLROUND_ROUND(colors[i]);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 * With 32 bit gx_color_index values, we simply pack values.
 */
static int
tiffsep_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    int bpc = ((tiffsep_device *)dev)->devn_params.bitspercomponent;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    int mask = (1 << bpc) - 1;
    int i = 0;
    int ncomp = dev->color_info.num_components;

    for (; i < ncomp; i++) {
        out[ncomp - i - 1] = (gx_color_value) ((color & mask) << drop);
        color >>= bpc;
    }
    return 0;
}

/*
 *  Device proc for updating the equivalent CMYK color for spot colors.
 */
static int
tiffsep_update_spot_equivalent_colors(gx_device * dev, const gs_gstate * pgs)
{
    tiffsep_device * pdev = (tiffsep_device *)dev;

    update_spot_equivalent_cmyk_colors(dev, pgs,
                    &pdev->devn_params, &pdev->equiv_cmyk_colors);
    return 0;
}

/*
 *  Device proc for returning a pointer to DeviceN parameter structure
 */
static gs_devn_params *
tiffsep_ret_devn_params(gx_device * dev)
{
    tiffsep_device * pdev = (tiffsep_device *)dev;

    return &pdev->devn_params;
}

/* Get parameters.  We provide a default CRD. */
static int
tiffsep_get_params(gx_device * pdev, gs_param_list * plist)
{
    tiffsep_device * const pdevn = (tiffsep_device *) pdev;
    int code = gdev_prn_get_params(pdev, plist);
    int ecode = code;
    gs_param_string comprstr;

    if (code < 0)
        return code;

    code = devn_get_params(pdev, plist,
                &(((tiffsep_device *)pdev)->devn_params),
                &(((tiffsep_device *)pdev)->equiv_cmyk_colors));
    if (code < 0)
        return code;

    if ((code = param_write_bool(plist, "NoSeparationFiles", &pdevn->NoSeparationFiles)) < 0)
        ecode = code;
    if ((code = param_write_bool(plist, "BigEndian", &pdevn->BigEndian)) < 0)
        ecode = code;
    if ((code = param_write_bool(plist, "TIFFDateTime", &pdevn->write_datetime)) < 0)
        ecode = code;
    if ((code = tiff_compression_param_string(&comprstr, pdevn->Compression)) < 0 ||
        (code = param_write_string(plist, "Compression", &comprstr)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "MaxStripSize", &pdevn->MaxStripSize)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "BitsPerComponent", &pdevn->BitsPerComponent)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "MaxSpots", &pdevn->max_spots)) < 0)
        ecode = code;
    if ((code = param_write_bool(plist, "LockColorants", &pdevn->lock_colorants)) < 0)
        ecode = code;
    if ((code = param_write_bool(plist, "PrintSpotCMYK", &pdevn->PrintSpotCMYK)) < 0)
        ecode = code;
    if ((code = gx_downscaler_write_params(plist, &pdevn->downscale,
                                           GX_DOWNSCALER_PARAMS_MFS |
                                           GX_DOWNSCALER_PARAMS_TRAP)) < 0)
        ecode = code;

    return ecode;
}

/* Set parameters.  We allow setting the number of bits per component. */
static int
tiffsep_put_params(gx_device * pdev, gs_param_list * plist)
{
    tiffsep_device * const pdevn = (tiffsep_device *) pdev;
    int code;
    const char *param_name;
    gs_param_string comprstr;
    long bpc = pdevn->BitsPerComponent;
    int max_spots = pdevn->max_spots;

    switch (code = param_read_bool(plist, (param_name = "NoSeparationFiles"),
        &pdevn->NoSeparationFiles)) {
    default:
        param_signal_error(plist, param_name, code);
        return code;
    case 0:
    case 1:
        break;
    }
    /* Read BigEndian option as bool */
    switch (code = param_read_bool(plist, (param_name = "BigEndian"), &pdevn->BigEndian)) {
        default:
            param_signal_error(plist, param_name, code);
            return code;
        case 0:
        case 1:
            break;
    }
    switch (code = param_read_bool(plist, (param_name = "TIFFDateTime"), &pdevn->write_datetime)) {
        default:
            param_signal_error(plist, param_name, code);
        case 0:
        case 1:
            break;
    }
    switch (code = param_read_bool(plist, (param_name = "PrintSpotCMYK"), &pdevn->PrintSpotCMYK)) {
        default:
            param_signal_error(plist, param_name, code);
            return code;
        case 0:
        case 1:
            break;
    }

    switch (code = param_read_long(plist, (param_name = "BitsPerComponent"), &bpc)) {
        case 0:
            if ((bpc == 1) || (bpc == 8)) {
                pdevn->BitsPerComponent = bpc;
                break;
            }
            code = gs_error_rangecheck;
        default:
            param_signal_error(plist, param_name, code);
            return code;
        case 1:
            break;
    }

    /* Read Compression */
    switch (code = param_read_string(plist, (param_name = "Compression"), &comprstr)) {
        case 0:
            if ((code = tiff_compression_id(&pdevn->Compression, &comprstr)) < 0) {

                errprintf(pdevn->memory, "Unknown compression setting\n");

                param_signal_error(plist, param_name, code);
                return code;
            }
            /* Because pdevn->BitsPerComponent is ignored for tiffsep(1) we have to get
             * the value based on whether we're called from tiffsep or tiffsep1
             */
            bpc = (dev_proc(pdev, put_params) == tiffsep1_put_params) ? 1 : 8;
            if (!tiff_compression_allowed(pdevn->Compression, bpc)) {
                errprintf(pdevn->memory, "Invalid compression setting for this bitdepth\n");

                param_signal_error(plist, param_name, gs_error_rangecheck);
                return_error(gs_error_rangecheck);
            }
            break;
        case 1:
            break;
        default:
            param_signal_error(plist, param_name, code);
            return code;
    }
    switch (code = param_read_long(plist, (param_name = "MaxStripSize"), &pdevn->MaxStripSize)) {
        case 0:
            /*
             * Strip must be large enough to accommodate a raster line.
             * If the max strip size is too small, we still write a single
             * line per strip rather than giving an error.
             */
            if (pdevn->MaxStripSize >= 0)
                break;
            code = gs_error_rangecheck;
        default:
            param_signal_error(plist, param_name, code);
            return code;
        case 1:
            break;
    }
    switch (code = param_read_bool(plist, (param_name = "LockColorants"),
            &(pdevn->lock_colorants))) {
        case 0:
            break;
        case 1:
            break;
        default:
            param_signal_error(plist, param_name, code);
            return code;
    }
    switch (code = param_read_int(plist, (param_name = "MaxSpots"), &max_spots)) {
        case 0:
            if ((max_spots >= 0) && (max_spots <= GS_CLIENT_COLOR_MAX_COMPONENTS-4)) {
                pdevn->max_spots = max_spots;
                break;
            }
            emprintf1(pdev->memory, "MaxSpots must be between 0 and %d\n",
                      GS_CLIENT_COLOR_MAX_COMPONENTS-4);
            return_error(gs_error_rangecheck);
        case 1:
            break;
        default:
            param_signal_error(plist, param_name, code);
            return code;
    }

    code = gx_downscaler_read_params(plist, &pdevn->downscale,
                                     GX_DOWNSCALER_PARAMS_MFS | GX_DOWNSCALER_PARAMS_TRAP);
    if (code < 0)
        return code;

    code = devn_printer_put_params(pdev, plist,
                &(pdevn->devn_params), &(pdevn->equiv_cmyk_colors));

    return(code);
}

static int
tiffsep1_put_params(gx_device * pdev, gs_param_list * plist)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *) pdev;
    int code;

    if ((code = tiffsep_put_params(pdev, plist)) < 0)
        return code;

    /* put_params may have changed the fill_path proc -- we need it set to ours */
    if (dev_proc(pdev, fill_path) != sep1_fill_path) {
        tfdev->fill_path = dev_proc(pdev, fill_path);
        set_dev_proc(pdev, fill_path, sep1_fill_path);
    }
    return code;

}

static void build_comp_to_sep_map(tiffsep_device *, short *);
static int number_output_separations(int, int, int, int);
static int create_separation_file_name(tiffsep_device *, char *, uint, int, bool);
static int sep1_ht_order_to_thresholds(gx_device *pdev, const gs_gstate *pgs);
dev_proc_fill_path(clist_fill_path);

/* Open the tiffsep1 device.  This will now be using planar buffers so that
   we are not limited to 64 bit chunky */
int
tiffsep1_prn_open(gx_device * pdev)
{
    gx_device_printer *ppdev = (gx_device_printer *)pdev;
    tiffsep1_device *pdev_sep = (tiffsep1_device *) pdev;
    int code, k;

    /* Use our own warning and error message handlers in libtiff */
    tiff_set_handlers();

    /* With planar the depth can be more than 64.  Update the color
       info to reflect the proper depth and number of planes */
    pdev_sep->warning_given = false;
    if (pdev_sep->devn_params.page_spot_colors >= 0) {
        pdev->color_info.num_components =
            (pdev_sep->devn_params.page_spot_colors
                                 + pdev_sep->devn_params.num_std_colorant_names);
        if (pdev->color_info.num_components > pdev->color_info.max_components)
            pdev->color_info.num_components = pdev->color_info.max_components;
    } else {
        /* We do not know how many spots may occur on the page.
           For this reason we go ahead and allocate the maximum that we
           have available.  Note, lack of knowledge only occurs in the case
           of PS files.  With PDF we know a priori the number of spot
           colorants.  */
        int num_comp = pdev_sep->max_spots + 4; /* Spots + CMYK */
        if (num_comp > GS_CLIENT_COLOR_MAX_COMPONENTS)
            num_comp = GS_CLIENT_COLOR_MAX_COMPONENTS;
        pdev->color_info.num_components = num_comp;
        pdev->color_info.max_components = num_comp;
    }
    /* Push this to the max amount as a default if someone has not set it */
    if (pdev_sep->devn_params.num_separation_order_names == 0)
        for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
            pdev_sep->devn_params.separation_order_map[k] = k;
        }
    pdev->color_info.depth = pdev->color_info.num_components *
                             pdev_sep->devn_params.bitspercomponent;
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    code = gdev_prn_open_planar(pdev, true);
    while (pdev->child)
        pdev = pdev->child;
    ppdev = (gx_device_printer *)pdev;
    pdev_sep = (tiffsep1_device *)pdev;

    ppdev->file = NULL;
    pdev->icc_struct->supports_devn = true;

    /* gdev_prn_open_planae may have changed the fill_path proc -- we need it set to ours */
    if (dev_proc(pdev, fill_path) != sep1_fill_path) {
        pdev_sep->fill_path = pdev->procs.fill_path;
        set_dev_proc(pdev, fill_path, sep1_fill_path);
    }
    return code;
}

/* Close the tiffsep device */
int
tiffsep1_prn_close(gx_device * pdev)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *) pdev;
    int num_dev_comp = tfdev->color_info.num_components;
    int num_std_colorants = tfdev->devn_params.num_std_colorant_names;
    int num_order = tfdev->devn_params.num_separation_order_names;
    int num_spot = tfdev->devn_params.separations.num_separations;
    char *name= NULL;
    int code = gdev_prn_close(pdev);
    short map_comp_to_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int comp_num;
    int num_comp = number_output_separations(num_dev_comp, num_std_colorants,
                                        num_order, num_spot);
    const char *fmt;
    gs_parsed_file_name_t parsed;

    if (code < 0)
        return code;

    name = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "tiffsep1_prn_close(name)");
    if (!name)
        return_error(gs_error_VMerror);

    code = gx_parse_output_file_name(&parsed, &fmt, tfdev->fname,
                                     strlen(tfdev->fname), pdev->memory);
    if (code < 0) {
        goto done;
    }

    /* If we are doing separate pages, delete the old default file */
    if (parsed.iodev == iodev_default(pdev->memory)) {          /* filename includes "%nnd" */
        char *compname = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "tiffsep1_prn_close(compname)");
        if (!compname) {
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }

        if (fmt) {
            long count1 = pdev->PageCount;

            while (*fmt != 'l' && *fmt != '%')
                --fmt;
            if (*fmt == 'l')
                gs_sprintf(compname, parsed.fname, count1);
            else
                gs_sprintf(compname, parsed.fname, (int)count1);
            parsed.iodev->procs.delete_file(parsed.iodev, compname);
        } else {
            parsed.iodev->procs.delete_file(parsed.iodev, tfdev->fname);
        }
        gs_free_object(pdev->memory, compname, "tiffsep1_prn_close(compname)");
    }

    build_comp_to_sep_map((tiffsep_device *)tfdev, map_comp_to_sep);
    /* Close the separation files */
    for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
        if (tfdev->sep_file[comp_num] != NULL) {
            int sep_num = map_comp_to_sep[comp_num];

            code = create_separation_file_name((tiffsep_device *)tfdev, name,
                                                gp_file_name_sizeof, sep_num, true);
            if (code < 0) {
                goto done;
            }
            code = gx_device_close_output_file(pdev, name, tfdev->sep_file[comp_num]);
            if (code >= 0)
                code = gs_remove_outputfile_control_path(pdev->memory, name);
            if (code < 0) {
                goto done;
            }
            tfdev->sep_file[comp_num] = NULL;
        }
        if (tfdev->tiff[comp_num]) {
            TIFFCleanup(tfdev->tiff[comp_num]);
            tfdev->tiff[comp_num] = NULL;
        }
    }

done:

    if (name)
        gs_free_object(pdev->memory, name, "tiffsep1_prn_close(name)");
    return code;
}


static int
sep1_fill_path(gx_device * pdev, const gs_gstate * pgs,
                 gx_path * ppath, const gx_fill_params * params,
                 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *)pdev;

    /* If we haven't already converted the ht into thresholds, do it now */
    if( tfdev->thresholds[0].dstart == NULL) {
        int code = sep1_ht_order_to_thresholds(pdev, pgs);

        if (code < 0)
            return code;
    }
    return (tfdev->fill_path)( pdev, pgs, ppath, params, pdevc, pcpath);
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
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the colorant is not being used due to a SeparationOrder device parameter.
 * It returns a negative value if not found.
 */
static int
tiffsep_get_color_comp_index(gx_device * dev, const char * pname,
                                int name_size, int component_type)
{
    tiffsep_device * pdev = (tiffsep_device *) dev;
    int index;

    if (strncmp(pname, "None", name_size) == 0) return -1;
    index = devn_get_color_comp_index(dev,
                &(pdev->devn_params), &(pdev->equiv_cmyk_colors),
                pname, name_size, component_type, ENABLE_AUTO_SPOT_COLORS);
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

/*
 * There can be a conflict if a separation name is used as part of the file
 * name for a separation output file.  PostScript and PDF do not restrict
 * the characters inside separation names.  However most operating systems
 * have some sort of restrictions.  For instance: /, \, and : have special
 * meaning under Windows.  This implies that there should be some sort of
 * escape sequence for special characters.  This routine exists as a place
 * to put the handling of that escaping.  However it is not actually
 * implemented. Instead we just map them to '_'.
 */
static void
copy_separation_name(tiffsep_device * pdev,
                char * buffer, int max_size, int sep_num, int escape)
{
    int sep_size = pdev->devn_params.separations.names[sep_num].size;
    const byte *p = pdev->devn_params.separations.names[sep_num].data;
    int r, w;

    /* Previously the code here would simply replace any char that wasn't
     * passed by gp_file_name_good_char (and %) with '_'. The grounds for
     * gp_file_name_good_char are obvious enough. The reason for '%' is
     * that the string gets fed to a printf style consumer later. It had
     * problems in that any top bit set char was let through, which upset
     * the file handling routines as they assume the filenames are in
     * utf-8 format. */

    /* New code: Copy the name, escaping non gp_file_name_good_char chars,
     * % and top bit set chars using %02x format. In addition, if 'escape'
     * is set, output % as %% to allow for printf later.
     */
    r = 0;
    w = 0;
    while (r < sep_size && w < max_size-1)
    {
        int c = p[r++];
        if (c >= 127 ||
            !gp_file_name_good_char(c) ||
            c == '%')
        {
            /* Top bit set, backspace, or char we can't represent on the
             * filesystem. */
            if (w + 2 + escape >= max_size-1)
                break;
            buffer[w++] = '%';
            if (escape)
                buffer[w++] = '%';
            buffer[w++] = "0123456789ABCDEF"[c>>4];
            buffer[w++] = "0123456789ABCDEF"[c&15];
        }
        else
        {
            buffer[w++] = c;
        }
    }
    buffer[w] = 0;       /* Terminate string */
}

/*
 * Determine the length of the base file name.  If the file name includes
 * the extension '.tif', then we remove it from the length of the file
 * name.
 */
static int
length_base_file_name(tiffsep_device * pdev, bool *double_f)
{
    int base_filename_length = strlen(pdev->fname);

#define REMOVE_TIF_FROM_BASENAME 1
#if REMOVE_TIF_FROM_BASENAME
    if (base_filename_length > 4 &&
        pdev->fname[base_filename_length - 4] == '.'  &&
        toupper(pdev->fname[base_filename_length - 3]) == 'T'  &&
        toupper(pdev->fname[base_filename_length - 2]) == 'I'  &&
        toupper(pdev->fname[base_filename_length - 1]) == 'F') {
        base_filename_length -= 4;
        *double_f = false;
    }
    else if (base_filename_length > 5 &&
        pdev->fname[base_filename_length - 5] == '.'  &&
        toupper(pdev->fname[base_filename_length - 4]) == 'T'  &&
        toupper(pdev->fname[base_filename_length - 3]) == 'I'  &&
        toupper(pdev->fname[base_filename_length - 2]) == 'F'  &&
        toupper(pdev->fname[base_filename_length - 1]) == 'F') {
        base_filename_length -= 5;
        *double_f = true;
    }
#endif
#undef REMOVE_TIF_FROM_BASENAME

    return base_filename_length;
}

/*
 * Create a name for a separation file.
 */
static int
create_separation_file_name(tiffsep_device * pdev, char * buffer,
                                uint max_size, int sep_num, bool use_sep_name)
{
    bool double_f = false;
    uint base_filename_length = length_base_file_name(pdev, &double_f);

    /*
     * In most cases it is more convenient if we append '.tif' to the end
     * of the file name.
     */
#define APPEND_TIF_TO_NAME 1
#define SUFFIX_SIZE (4 * APPEND_TIF_TO_NAME)

    memcpy(buffer, pdev->fname, base_filename_length);
    buffer[base_filename_length++] = use_sep_name ? '(' : '.';
    buffer[base_filename_length] = 0;  /* terminate the string */

    if (sep_num < pdev->devn_params.num_std_colorant_names) {
        if (max_size < strlen(pdev->devn_params.std_colorant_names[sep_num]))
            return_error(gs_error_rangecheck);
        strcat(buffer, pdev->devn_params.std_colorant_names[sep_num]);
    }
    else {
        sep_num -= pdev->devn_params.num_std_colorant_names;
        if (use_sep_name) {
            copy_separation_name(pdev, buffer + base_filename_length,
                                max_size - SUFFIX_SIZE - 2, sep_num, 1);
        } else {
                /* Max of 10 chars in %d format */
            if (max_size < base_filename_length + 11)
                return_error(gs_error_rangecheck);
            gs_sprintf(buffer + base_filename_length, "s%d", sep_num);
        }
    }
    if (use_sep_name)
        strcat(buffer, ")");

#if APPEND_TIF_TO_NAME
    if (double_f) {
        if (max_size < strlen(buffer) + SUFFIX_SIZE + 1)
            return_error(gs_error_rangecheck);
        strcat(buffer, ".tiff");
    }
    else {
        if (max_size < strlen(buffer) + SUFFIX_SIZE)
            return_error(gs_error_rangecheck);
        strcat(buffer, ".tif");
    }
#endif
    return 0;
}

/*
 * Determine the number of output separations for the tiffsep device.
 *
 * There are several factors which affect the number of output separations
 * for the tiffsep device.
 *
 * Due to limitations on the size of a gx_color_index, we are limited to a
 * maximum of 8 colors per pass.  Thus the tiffsep device is set to 8
 * components.  However this is not usually the number of actual separation
 * files to be created.
 *
 * If the SeparationOrder parameter has been specified, then we use it to
 * select the number and which separation files are created.
 *
 * If the SeparationOrder parameter has not been specified, then we use the
 * nuber of process colors (CMYK) and the number of spot colors unless we
 * exceed the 8 component maximum for the device.
 *
 * Note:  Unlike most other devices, the tiffsep device will accept more than
 * four spot colors.  However the extra spot colors will not be imaged
 * unless they are selected by the SeparationOrder parameter.  (This does
 * allow the user to create more than 8 separations by a making multiple
 * passes and using the SeparationOrder parameter.)
*/
static int
number_output_separations(int num_dev_comp, int num_std_colorants,
                                        int num_order, int num_spot)
{
    int num_comp =  num_std_colorants + num_spot;

    if (num_comp > num_dev_comp)
        num_comp = num_dev_comp;
    if (num_order)
        num_comp = num_order;
    return num_comp;
}

/*
 * This routine creates a list to map the component number to a separation number.
 * Values less than 4 refer to the CMYK colorants.  Higher values refer to a
 * separation number.
 *
 * This is the inverse of the separation_order_map.
 */
static void
build_comp_to_sep_map(tiffsep_device * pdev, short * map_comp_to_sep)
{
    int num_sep = pdev->devn_params.separations.num_separations;
    int num_std_colorants = pdev->devn_params.num_std_colorant_names;
    int sep_num;
    int num_channels;

    /* since both proc colors and spot colors are packed in same encoded value we
       need to have this limit */

    num_channels =
        ( (num_std_colorants + num_sep) < (GX_DEVICE_COLOR_MAX_COMPONENTS) ? (num_std_colorants + num_sep) : (GX_DEVICE_COLOR_MAX_COMPONENTS) );

    for (sep_num = 0; sep_num < num_channels; sep_num++) {
        int comp_num = pdev->devn_params.separation_order_map[sep_num];

        if (comp_num >= 0 && comp_num < GX_DEVICE_COLOR_MAX_COMPONENTS)
            map_comp_to_sep[comp_num] = sep_num;
    }
}

/* Open the tiffsep device.  This will now be using planar buffers so that
   we are not limited to 64 bit chunky */
int
tiffsep_prn_open(gx_device * pdev)
{
    gx_device_printer *ppdev = (gx_device_printer *)pdev;
    tiffsep_device *pdev_sep = (tiffsep_device *) pdev;
    int code, k;
    bool force_pdf, force_ps;
    cmm_dev_profile_t *profile_struct;
    gsicc_rendering_param_t rendering_params;

    /* Use our own warning and error message handlers in libtiff */
    tiff_set_handlers();

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

    /* For the planar device we need to set up the bit depth of each plane.
       For other devices this is handled in check_device_separable where
       we compute the bit shift for the components etc. */
    for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
        pdev->color_info.comp_bits[k] = 8;
    }

    /* With planar the depth can be more than 64.  Update the color
       info to reflect the proper depth and number of planes */
    pdev_sep->warning_given = false;
    if ((pdev_sep->devn_params.page_spot_colors >= 0 || force_pdf) && !force_ps) {
        if (force_pdf) {
            /* Use the information that is in the ICC profle.  We will be here
               anytime that we have limited ourselves to a fixed number
               of colorants specified by the DeviceN ICC profile */
            pdev->color_info.num_components =
                (pdev_sep->devn_params.separations.num_separations
                 + pdev_sep->devn_params.num_std_colorant_names);
            if (pdev->color_info.num_components > pdev->color_info.max_components)
                pdev->color_info.num_components = pdev->color_info.max_components;
            /* Limit us only to the ICC colorants */
            pdev->color_info.max_components = pdev->color_info.num_components;
        } else {
            /* Do not allow the spot count to update if we have specified the
               colorants already */
            if (!(pdev_sep->lock_colorants)) {
                pdev->color_info.num_components =
                    (pdev_sep->devn_params.page_spot_colors
                    + pdev_sep->devn_params.num_std_colorant_names);
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
        if (!(pdev_sep->lock_colorants)) {
            int num_comp = pdev_sep->max_spots + 4; /* Spots + CMYK */
            if (num_comp > GS_CLIENT_COLOR_MAX_COMPONENTS)
                num_comp = GS_CLIENT_COLOR_MAX_COMPONENTS;
            pdev->color_info.num_components = num_comp;
            pdev->color_info.max_components = num_comp;
        }
    }
    /* Push this to the max amount as a default if someone has not set it */
    if (pdev_sep->devn_params.num_separation_order_names == 0)
        for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
            pdev_sep->devn_params.separation_order_map[k] = k;
        }
    pdev->color_info.depth = pdev->color_info.num_components *
                             pdev_sep->devn_params.bitspercomponent;
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    code = gdev_prn_open_planar(pdev, true);
    while (pdev->child)
        pdev = pdev->child;
    ppdev = (gx_device_printer *)pdev;

    ppdev->file = NULL;
    pdev->icc_struct->supports_devn = true;

    /* Set up the icc link settings at this time.  Only CMYK post render profiles
       are allowed */
    code = dev_proc(pdev, get_profile)((gx_device *)pdev, &profile_struct);
    if (code < 0)
        return_error(gs_error_undefined);

    if (profile_struct->postren_profile != NULL &&
        profile_struct->postren_profile->data_cs == gsCMYK) {
        rendering_params.black_point_comp = gsBLACKPTCOMP_ON;
        rendering_params.graphics_type_tag = GS_UNKNOWN_TAG;
        rendering_params.override_icc = false;
        rendering_params.preserve_black = gsBLACKPRESERVE_OFF;
        rendering_params.rendering_intent = gsRELATIVECOLORIMETRIC;
        rendering_params.cmm = gsCMM_DEFAULT;
        if (profile_struct->oi_profile != NULL) {
            pdev_sep->icclink = gsicc_alloc_link_dev(pdev->memory,
                profile_struct->oi_profile, profile_struct->postren_profile,
                &rendering_params);
        } else if (profile_struct->link_profile != NULL) {
            pdev_sep->icclink = gsicc_alloc_link_dev(pdev->memory,
                profile_struct->link_profile, profile_struct->postren_profile,
                &rendering_params);
        } else {
            pdev_sep->icclink = gsicc_alloc_link_dev(pdev->memory,
                profile_struct->device_profile[0], profile_struct->postren_profile,
                &rendering_params);
        }
        if (pdev_sep->icclink == NULL) {
            return_error(gs_error_VMerror);
        }
        /* If it is identity, release it now and set link to NULL */
        if (pdev_sep->icclink->is_identity) {
            pdev_sep->icclink->procs.free_link(pdev_sep->icclink);
            gsicc_free_link_dev(pdev->memory, pdev_sep->icclink);
            pdev_sep->icclink = NULL;
        }
    }
    return code;
}

static int
tiffsep_close_sep_file(tiffsep_device *tfdev, const char *fn, int comp_num)
{
    int code;

    if (tfdev->tiff[comp_num]) {
        TIFFCleanup(tfdev->tiff[comp_num]);
        tfdev->tiff[comp_num] = NULL;
    }

    code = gx_device_close_output_file((gx_device *)tfdev,
                                       fn,
                                       tfdev->sep_file[comp_num]);
    tfdev->sep_file[comp_num] = NULL;
    tfdev->tiff[comp_num] = NULL;

    return code;
}

static int
tiffsep_close_comp_file(tiffsep_device *tfdev, const char *fn)
{
    int code;

    if (tfdev->tiff_comp) {
        TIFFCleanup(tfdev->tiff_comp);
        tfdev->tiff_comp = NULL;
    }

    code = gx_device_close_output_file((gx_device *)tfdev,
                                       fn,
                                       tfdev->comp_file);
    tfdev->comp_file = NULL;

    return code;
}

/* Close the tiffsep device */
int
tiffsep_prn_close(gx_device * pdev)
{
    tiffsep_device * const pdevn = (tiffsep_device *) pdev;
    int num_dev_comp = pdevn->color_info.num_components;
    int num_std_colorants = pdevn->devn_params.num_std_colorant_names;
    int num_order = pdevn->devn_params.num_separation_order_names;
    int num_spot = pdevn->devn_params.separations.num_separations;
    short map_comp_to_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    char *name = NULL;
    int code;
    int comp_num;
    int num_comp = number_output_separations(num_dev_comp, num_std_colorants,
                                        num_order, num_spot);
    if (pdevn->icclink != NULL) {
        pdevn->icclink->procs.free_link(pdevn->icclink);
        gsicc_free_link_dev(pdevn->memory, pdevn->icclink);
        pdevn->icclink = NULL;
    }

    name = (char *)gs_alloc_bytes(pdevn->memory, gp_file_name_sizeof, "tiffsep_prn_close(name)");
    if (!name)
        return_error(gs_error_VMerror);

    if (pdevn->tiff_comp) {
        TIFFCleanup(pdevn->tiff_comp);
        pdevn->tiff_comp = NULL;
    }
    code = gdev_prn_close(pdev);
    if (code < 0) {
        goto done;
    }

    build_comp_to_sep_map(pdevn, map_comp_to_sep);
    /* Close the separation files */
    for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
        if (pdevn->sep_file[comp_num] != NULL) {
            int sep_num = pdevn->devn_params.separation_order_map[comp_num];

            code = create_separation_file_name(pdevn, name,
                    gp_file_name_sizeof, sep_num, true);
            if (code < 0) {
                goto done;
            }
            code = tiffsep_close_sep_file(pdevn, name, comp_num);
            if (code >= 0)
                code = gs_remove_outputfile_control_path(pdevn->memory, name);
            if (code < 0) {
                goto done;
            }
        }
    }

done:
    if (name)
        gs_free_object(pdev->memory, name, "tiffsep_prn_close(name)");
    return code;
}

/*
 * Build a CMYK equivalent to a raster line from planar buffer
 */
static void
build_cmyk_raster_line_fromplanar(gs_get_bits_params_t *params, byte * dest,
                                  int width, int num_comp,
                                  cmyk_composite_map * cmyk_map, int num_order,
                                  tiffsep_device * const tfdev)
{
    int pixel, comp_num;
    uint temp, cyan, magenta, yellow, black;
    cmyk_composite_map * cmyk_map_entry;
    byte *start = dest;

    for (pixel = 0; pixel < width; pixel++) {
        cmyk_map_entry = cmyk_map;
        temp = *(params->data[tfdev->devn_params.separation_order_map[0]] + pixel);
        cyan = cmyk_map_entry->c * temp;
        magenta = cmyk_map_entry->m * temp;
        yellow = cmyk_map_entry->y * temp;
        black = cmyk_map_entry->k * temp;
        cmyk_map_entry++;
        for (comp_num = 1; comp_num < num_comp; comp_num++) {
            temp =
                *(params->data[tfdev->devn_params.separation_order_map[comp_num]] + pixel);
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
        *dest++ = cyan;
        *dest++ = magenta;
        *dest++ = yellow;
        *dest++ = black;
    }
    /* And now apply the post rendering profile to the scan line if it exists.
       In place conversion */
    if (tfdev->icclink != NULL) {
        gsicc_bufferdesc_t buffer_desc;

        gsicc_init_buffer(&buffer_desc, tfdev->icclink->num_input, 1, false,
            false, false, 0, width * 4, 1, width);
        tfdev->icclink->procs.map_buffer(NULL, tfdev->icclink, &buffer_desc,
            &buffer_desc, start, start);
    }
}

static void
build_cmyk_raster_line_fromplanar_1bpc(gs_get_bits_params_t *params, byte * dest,
                                       int width, int num_comp,
                                       cmyk_composite_map * cmyk_map, int num_order,
                                       tiffsep_device * const tfdev)
{
    int pixel, comp_num;
    uint temp, cyan, magenta, yellow, black;
    cmyk_composite_map * cmyk_map_entry;

    for (pixel = 0; pixel < width; pixel++) {
        cmyk_map_entry = cmyk_map;
        temp = *(params->data[tfdev->devn_params.separation_order_map[0]] + (pixel>>3));
        temp = ((temp<<(pixel & 7))>>7) & 1;
        cyan = cmyk_map_entry->c * temp;
        magenta = cmyk_map_entry->m * temp;
        yellow = cmyk_map_entry->y * temp;
        black = cmyk_map_entry->k * temp;
        cmyk_map_entry++;
        for (comp_num = 1; comp_num < num_comp; comp_num++) {
            temp =
                *(params->data[tfdev->devn_params.separation_order_map[comp_num]] + (pixel>>3));
            temp = ((temp<<(pixel & 7))>>7) & 1;
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
        if (cyan > 1)
            cyan = 1;
        if (magenta > 1)
            magenta = 1;
        if (yellow > 1)
            yellow = 1;
        if (black > 1)
            black = 1;
        if ((pixel & 1) == 0)
            *dest = (cyan<<7) | (magenta<<6) | (yellow<<5) | (black<<4);
        else
            *dest++ |= (cyan<<3) | (magenta<<2) | (yellow<<1) | black;
    }
}
static void
build_cmyk_raster_line_fromplanar_2bpc(gs_get_bits_params_t *params, byte * dest,
                                       int width, int num_comp,
                                       cmyk_composite_map * cmyk_map, int num_order,
                                       tiffsep_device * const tfdev)
{
    int pixel, comp_num;
    uint temp, cyan, magenta, yellow, black;
    cmyk_composite_map * cmyk_map_entry;

    for (pixel = 0; pixel < width; pixel++) {
        cmyk_map_entry = cmyk_map;
        temp = *(params->data[tfdev->devn_params.separation_order_map[0]] + (pixel>>2));
        temp = (((temp<<((pixel & 3)<<1))>>6) & 3) * 85;
        cyan = cmyk_map_entry->c * temp;
        magenta = cmyk_map_entry->m * temp;
        yellow = cmyk_map_entry->y * temp;
        black = cmyk_map_entry->k * temp;
        cmyk_map_entry++;
        for (comp_num = 1; comp_num < num_comp; comp_num++) {
            temp =
                *(params->data[tfdev->devn_params.separation_order_map[comp_num]] + (pixel>>2));
            temp = (((temp<<((pixel & 3)<<1))>>6) & 3) * 85;
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
        if (cyan > 3)
            cyan = 3;
        if (magenta > 3)
            magenta = 3;
        if (yellow > 3)
            yellow = 3;
        if (black > 3)
            black = 3;
        *dest++ = (cyan<<6) | (magenta<<4) | (yellow<<2) | black;
    }
}

static void
build_cmyk_raster_line_fromplanar_4bpc(gs_get_bits_params_t *params, byte * dest,
                                       int width, int num_comp,
                                       cmyk_composite_map * cmyk_map, int num_order,
                                       tiffsep_device * const tfdev)
{
    int pixel, comp_num;
    uint temp, cyan, magenta, yellow, black;
    cmyk_composite_map * cmyk_map_entry;

    for (pixel = 0; pixel < width; pixel++) {
        cmyk_map_entry = cmyk_map;
        temp = *(params->data[tfdev->devn_params.separation_order_map[0]] + (pixel>>1));
        if (pixel & 1)
            temp >>= 4;
        temp &= 15;
        cyan = cmyk_map_entry->c * temp;
        magenta = cmyk_map_entry->m * temp;
        yellow = cmyk_map_entry->y * temp;
        black = cmyk_map_entry->k * temp;
        cmyk_map_entry++;
        for (comp_num = 1; comp_num < num_comp; comp_num++) {
            temp =
                *(params->data[tfdev->devn_params.separation_order_map[comp_num]] + (pixel>>1));
            if (pixel & 1)
                temp >>= 4;
            temp &= 15;
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
        if (cyan > 15)
            cyan = 15;
        if (magenta > 15)
            magenta = 15;
        if (yellow > 15)
            yellow = 15;
        if (black > 15)
            black = 15;
        *dest++ = (cyan<<4) | magenta;
        *dest++ = (yellow<<4) | black;
    }
}

static int
sep1_ht_order_to_thresholds(gx_device *pdev, const gs_gstate *pgs)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *)pdev;
    gs_memory_t *mem = pdev->memory;
    int code;

    /* If we have thresholds, clear the pointers */
    if( tfdev->thresholds[0].dstart != NULL) {
        tfdev->thresholds[0].dstart = NULL;
    } else {
        int nc, j;
        gx_ht_order *d_order;
        threshold_array_t *dptr;

        if (pgs->dev_ht == NULL) {
            emprintf(mem, "sep1_order_to_thresholds: no dev_ht available\n");
            return_error(gs_error_rangecheck);  /* error condition */
        }
        nc = pgs->dev_ht->num_comp;
        for( j=0; j<nc; j++ ) {
            int x, y;

            d_order = &(pgs->dev_ht->components[j].corder);
            dptr = &(tfdev->thresholds[j]);
            /* In order to use the function from gsht.c we need to set the color_info */
            /* values it uses to reflect the eventual 1-bit output, not contone       */
            pdev->color_info.dither_grays = pdev->color_info.dither_colors = 2;
            pdev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
            code = gx_ht_construct_threshold(d_order, pdev, pgs, j);
            if( code < 0 ) {
                emprintf(mem,
                         "sep1_order_to_thresholds: conversion to thresholds failed.\n");
                return_error(code);      /* error condition */
            }
            pdev->color_info.dither_grays = pdev->color_info.dither_colors = 256;
            pdev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
            /* Invert the thresholds so we (almost) match pbmraw dithered output */
            for (y=0; y<d_order->full_height; y++) {
                byte *s = d_order->threshold;
                int s_offset = y * d_order->width;

                for (x=0; x<d_order->width; x++) {
                    s[s_offset + x] = 256 - s[s_offset + x];
                }
            }
            dptr->dstart = d_order->threshold;
            dptr->dwidth = d_order->width;
            dptr->dheight = d_order->full_height;
        }
    }
    return 0;
}

 /*
 * This function prints out CMYK value with separation name for every
 * separation. Where the original alternate colour space was DeviceCMYK, and the output
 * ICC profile is CMYK, no transformation takes place. Where the original alternate space
 * was not DeviceCMYK, the colour management system will be used to generate CMYK values
 * from the original tint transform.
 * NB if the output profile is DeviceN then we will use the DeviceCMYK profile to map the
 * equivalents, *not* the DeviceN profile. This is a peculiar case.....
 */
static int
print_cmyk_equivalent_colors(tiffsep_device *tfdev, int num_comp, cmyk_composite_map *cmyk_map)
{
    int comp_num;
    char *name = (char *)gs_alloc_bytes(tfdev->memory, gp_file_name_sizeof,
                                "tiffsep_print_cmyk_equivalent_colors(name)");

    if (!name) {
        return_error(gs_error_VMerror);
    }

    for (comp_num = 0; comp_num < num_comp; comp_num++) {
        int sep_num = tfdev->devn_params.separation_order_map[comp_num];

        if (sep_num < tfdev->devn_params.num_std_colorant_names) {
            if (gp_file_name_sizeof < strlen(tfdev->devn_params.std_colorant_names[sep_num])) {
                if (name)
                    gs_free_object(tfdev->memory, name, "tiffsep_print_cmyk_equivalent_colors(name)");
                return_error(gs_error_rangecheck);
            }
            strcpy(name, tfdev->devn_params.std_colorant_names[sep_num]);
        } else {
            sep_num -= tfdev->devn_params.num_std_colorant_names;
            if (gp_file_name_sizeof < tfdev->devn_params.separations.names[sep_num].size) {
                if (name)
                    gs_free_object(tfdev->memory, name, "tiffsep_print_cmyk_equivalent_colors(name)");
                return_error(gs_error_rangecheck);
            }

            memcpy(name, (char *)tfdev->devn_params.separations.names[sep_num].data, tfdev->devn_params.separations.names[sep_num].size);
            name[tfdev->devn_params.separations.names[sep_num].size] = '\0';
        }

        dmlprintf5(tfdev->memory, "%%%%SeparationColor: \"%s\" 100%% ink = %hd %hd %hd %hd CMYK\n",
                     name,
                     cmyk_map[comp_num].c,
                     cmyk_map[comp_num].m,
                     cmyk_map[comp_num].y,
                     cmyk_map[comp_num].k);
    }

    if (name) {
        gs_free_object(tfdev->memory, name, "tiffsep_print_cmyk_equivalent_colors(name)");
    }

    return 0;
}

/*
 * Output the image data for the tiff separation (tiffsep) device.  The data
 * for the tiffsep device is written in separate planes to separate files.
 *
 * The DeviceN parameters (SeparationOrder, SeparationColorNames, and
 * MaxSeparations) are applied to the tiffsep device.
 */
static int
tiffsep_print_page(gx_device_printer * pdev, gp_file * file)
{
    tiffsep_device * const tfdev = (tiffsep_device *)pdev;
    int num_std_colorants = tfdev->devn_params.num_std_colorant_names;
    int num_order = tfdev->devn_params.num_separation_order_names;
    int num_spot = tfdev->devn_params.separations.num_separations;
    int num_comp, comp_num, sep_num, code = 0, code1 = 0;
    cmyk_composite_map cmyk_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
    char *name = NULL;
    bool double_f = false;
    int base_filename_length = length_base_file_name(tfdev, &double_f);
    int save_depth = pdev->color_info.depth;
    int save_numcomps = pdev->color_info.num_components;
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int plane_count = 0;  /* quiet compiler */
    int factor = tfdev->downscale.downscale_factor;
    int mfs = tfdev->downscale.min_feature_size;
    int dst_bpc = tfdev->BitsPerComponent;
    gx_downscaler_t ds;
    int width = gx_downscaler_scale(tfdev->width, factor);
    int height = gx_downscaler_scale(tfdev->height, factor);

    name = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "tiffsep_print_page(name)");
    if (!name)
        return_error(gs_error_VMerror);

    /* Print the names of the spot colors */
    if (num_order == 0) {
        for (sep_num = 0; sep_num < num_spot; sep_num++) {
            copy_separation_name(tfdev, name,
                gp_file_name_sizeof - base_filename_length - SUFFIX_SIZE, sep_num, 0);
            dmlprintf1(pdev->memory, "%%%%SeparationName: %s\n", name);
        }
    }

    /*
     * Since different pages may have different spot colors, if this is for a
     * page after Page 1, we require that each output file is unique with a "fmt"
     * (i.e. %d) as part of the filename. We create individual separation files
     * for each page of the input.
     * Since the TIFF lib requires seeakable files, /dev/null or nul: are
     * not allowed (as they are with the psdcmyk devices).
    */
    code = gx_parse_output_file_name(&parsed, &fmt, tfdev->fname,
                                     strlen(tfdev->fname), pdev->memory);
    if (code < 0 || (fmt == NULL && tfdev->PageCount > 0)) {
       emprintf(tfdev->memory,
                "\nUse of the %%d format is required to output more than one page to tiffsep.\n"
                "See doc/Devices.htm#TIFF for details.\n\n");
       code = gs_note_error(gs_error_ioerror);
       goto done;
    }
    /* Write the page directory for the CMYK equivalent file. */
    if (!tfdev->comp_file) {
        pdev->color_info.depth = dst_bpc*4;        /* Create directory for 32 bit cmyk */
        if (!tfdev->UseBigTIFF && tfdev->Compression==COMPRESSION_NONE &&
            height > ((unsigned long) 0xFFFFFFFF - (file ? gp_ftell(file) : 0))/(width*4)) { /* note width is never 0 in print_page */
            dmprintf(pdev->memory, "CMYK composite file would be too large! Reduce resolution or enable compression.\n");
            return_error(gs_error_rangecheck);  /* this will overflow 32 bits */
        }

        code = gx_device_open_output_file((gx_device *)pdev, pdev->fname, true, true, &(tfdev->comp_file));
        if (code < 0) {
            goto done;
        }

        tfdev->tiff_comp = tiff_from_filep(pdev, pdev->dname, tfdev->comp_file, tfdev->BigEndian, tfdev->UseBigTIFF);
        if (!tfdev->tiff_comp) {
            code = gs_note_error(gs_error_invalidfileaccess);
            goto done;
        }

    }
    code = tiff_set_fields_for_printer(pdev, tfdev->tiff_comp, factor, 0, tfdev->write_datetime);

    if (dst_bpc == 1 || dst_bpc == 8) {
        tiff_set_cmyk_fields(pdev, tfdev->tiff_comp, dst_bpc, tfdev->Compression, tfdev->MaxStripSize);
    }
    else {
        /* Catch-all just for safety's sake */
        tiff_set_cmyk_fields(pdev, tfdev->tiff_comp, dst_bpc, COMPRESSION_NONE, tfdev->MaxStripSize);
    }

    pdev->color_info.depth = save_depth;
    if (code < 0) {
        goto done;
    }

    /* Set up the separation output files */
    num_comp = number_output_separations( tfdev->color_info.num_components,
                                        num_std_colorants, num_order, num_spot);

    if (!tfdev->NoSeparationFiles && !num_order && num_comp < num_std_colorants + num_spot) {
        dmlprintf(pdev->memory, "Warning: skipping one or more colour separations, see: Devices.htm#TIFF\n");
    }

    if (!tfdev->NoSeparationFiles) {
        for (comp_num = 0; comp_num < num_comp; comp_num++) {
            int sep_num = tfdev->devn_params.separation_order_map[comp_num];

            code = create_separation_file_name(tfdev, name, gp_file_name_sizeof,
                sep_num, true);
            if (code < 0) {
                goto done;
            }

            /*
             * Close the old separation file if we are creating individual files
             * for each page.
             */
            if (tfdev->sep_file[comp_num] != NULL && fmt != NULL) {
                code = tiffsep_close_sep_file(tfdev, name, comp_num);
                if (code >= 0)
                    code = gs_remove_outputfile_control_path(tfdev->memory, name);
                if (code < 0)
                    return code;
            }
            /* Open the separation file, if not already open */
            if (tfdev->sep_file[comp_num] == NULL) {
                code = gs_add_outputfile_control_path(tfdev->memory, name);
                if (code < 0) {
                    goto done;
                }
                code = gx_device_open_output_file((gx_device *)pdev, name,
                    true, true, &(tfdev->sep_file[comp_num]));
                if (code < 0) {
                    goto done;
                }
                tfdev->tiff[comp_num] = tiff_from_filep(pdev, name,
                    tfdev->sep_file[comp_num],
                    tfdev->BigEndian, tfdev->UseBigTIFF);
                if (!tfdev->tiff[comp_num]) {
                    code = gs_note_error(gs_error_ioerror);
                    goto done;
                }
            }

            pdev->color_info.depth = dst_bpc;     /* Create files for 8 bit gray */
            pdev->color_info.num_components = 1;
            if (!tfdev->UseBigTIFF && tfdev->Compression == COMPRESSION_NONE &&
                height * 8 / dst_bpc > ((unsigned long)0xFFFFFFFF - (file ? gp_ftell(file) : 0)) / width) /* note width is never 0 in print_page */
            {
                code = gs_note_error(gs_error_rangecheck);  /* this will overflow 32 bits */
                goto done;
            }


            code = tiff_set_fields_for_printer(pdev, tfdev->tiff[comp_num], factor, 0, tfdev->write_datetime);
            tiff_set_gray_fields(pdev, tfdev->tiff[comp_num], dst_bpc, tfdev->Compression, tfdev->MaxStripSize);
            pdev->color_info.depth = save_depth;
            pdev->color_info.num_components = save_numcomps;
            if (code < 0) {
                goto done;
            }
        }
    }

    build_cmyk_map((gx_device*) tfdev, num_comp, &tfdev->equiv_cmyk_colors, cmyk_map);
    if (tfdev->PrintSpotCMYK) {
        code = print_cmyk_equivalent_colors(tfdev, num_comp, cmyk_map);
        if (code < 0) {
            goto done;
        }
    }

    {
        int raster_plane = bitmap_raster(width * 8);
        byte *planes[GS_CLIENT_COLOR_MAX_COMPONENTS] = { 0 };
        int cmyk_raster = width * NUM_CMYK_COMPONENTS;
        int pixel, y;
        byte * sep_line;
        int plane_index;
        int offset_plane = 0;

        sep_line =
            gs_alloc_bytes(pdev->memory, cmyk_raster, "tiffsep_print_page");
        if (!sep_line) {
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }

        if (!tfdev->NoSeparationFiles)
            for (comp_num = 0; comp_num < num_comp; comp_num++ )
                TIFFCheckpointDirectory(tfdev->tiff[comp_num]);
        TIFFCheckpointDirectory(tfdev->tiff_comp);

        /* Write the page data. */
        {
            gs_get_bits_params_t params;
            int byte_width;

            /* Return planar data */
            params.options = (GB_RETURN_POINTER | GB_RETURN_COPY |
                 GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_RASTER_STANDARD |
                 GB_PACKING_PLANAR | GB_COLORS_NATIVE | GB_ALPHA_NONE);
            params.x_offset = 0;
            params.raster = bitmap_raster(width * pdev->color_info.depth);

            if (num_order > 0) {
                /* In this case, there was a specification for a separation
                   color order, which indicates what colorants we will
                   actually creat individual separation files for.  We need
                   to allocate for the standard colorants.  This is due to the
                   fact that even when we specify a single spot colorant, we
                   still create the composite CMYK output file. */
                for (comp_num = 0; comp_num < num_std_colorants; comp_num++) {
                    planes[comp_num] = gs_alloc_bytes(pdev->memory, raster_plane,
                                                      "tiffsep_print_page");
                    params.data[comp_num] = planes[comp_num];
                    if (params.data[comp_num] == NULL) {
                        code = gs_note_error(gs_error_VMerror);
                        goto cleanup;
                    }
                }
                offset_plane = num_std_colorants;
                /* Now we need to make sure that we do not allocate extra
                   planes if any of the colorants in the order list are
                   one of the standard colorant names */
                plane_index = plane_count = num_std_colorants;
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    int temp_pos;

                    temp_pos = tfdev->devn_params.separation_order_map[comp_num];
                    if (temp_pos >= num_std_colorants) {
                        /* We have one that is not a standard colorant name
                           so allocate a new plane */
                        planes[plane_count] = gs_alloc_bytes(pdev->memory, raster_plane,
                                                        "tiffsep_print_page");
                        /* Assign the new plane to the appropriate position */
                        params.data[plane_index] = planes[plane_count];
                        if (params.data[plane_index] == NULL) {
                            code = gs_note_error(gs_error_VMerror);
                            goto cleanup;
                        }
                        plane_count += 1;
                    } else {
                        /* Assign params.data with the appropriate std.
                           colorant plane position */
                        params.data[plane_index] = planes[temp_pos];
                    }
                    plane_index += 1;
                }
            } else {
                /* Sep color order number was not specified so just render all
                   the  planes that we can */
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    planes[comp_num] = gs_alloc_bytes(pdev->memory, raster_plane,
                                                    "tiffsep_print_page");
                    params.data[comp_num] = planes[comp_num];
                    if (params.data[comp_num] == NULL) {
                        code = gs_note_error(gs_error_VMerror);
                        goto cleanup;
                    }
                }
            }
            code = gx_downscaler_init_planar_trapped(&ds, (gx_device *)pdev, &params,
                                                     num_comp, factor, mfs, 8, dst_bpc,
                                                     tfdev->downscale.trap_w, tfdev->downscale.trap_h,
                                                     tfdev->downscale.trap_order);
            if (code < 0)
                goto cleanup;
            byte_width = (width * dst_bpc + 7)>>3;
            for (y = 0; y < height; ++y) {
                code = gx_downscaler_get_bits_rectangle(&ds, &params, y);
                if (code < 0)
                    goto cleanup;
                /* Write separation data (tiffgray format) */
                if (!tfdev->NoSeparationFiles) {
                    for (comp_num = 0; comp_num < num_comp; comp_num++) {
                        byte *src;
                        byte *dest = sep_line;

                        if (num_order > 0) {
                            src = params.data[tfdev->devn_params.separation_order_map[comp_num]];
                        }
                        else
                            src = params.data[comp_num];
                        for (pixel = 0; pixel < byte_width; pixel++, dest++, src++)
                            *dest = MAX_COLOR_VALUE - *src;    /* Gray is additive */
                        TIFFWriteScanline(tfdev->tiff[comp_num], (tdata_t)sep_line, y, 0);
                    }
                }
                /* Write CMYK equivalent data */
                switch(dst_bpc)
                {
                default:
                case 8:
                    build_cmyk_raster_line_fromplanar(&params, sep_line, width,
                                                      num_comp, cmyk_map, num_order,
                                                      tfdev);
                    break;
                case 4:
                    build_cmyk_raster_line_fromplanar_4bpc(&params, sep_line, width,
                                                           num_comp, cmyk_map, num_order,
                                                           tfdev);
                    break;
                case 2:
                    build_cmyk_raster_line_fromplanar_2bpc(&params, sep_line, width,
                                                           num_comp, cmyk_map, num_order,
                                                           tfdev);
                    break;
                case 1:
                    build_cmyk_raster_line_fromplanar_1bpc(&params, sep_line, width,
                                                           num_comp, cmyk_map, num_order,
                                                           tfdev);
                    break;
                }
                TIFFWriteScanline(tfdev->tiff_comp, (tdata_t)sep_line, y, 0);
            }
cleanup:
            if (num_order > 0) {
                /* Free up the standard colorants if num_order was set.
                   In this process, we need to make sure that none of them
                   were the standard colorants.  plane_count should have
                   the sum of the std. colorants plus any non-standard
                   ones listed in separation color order */
                for (comp_num = 0; comp_num < plane_count; comp_num++) {
                    gs_free_object(pdev->memory, planes[comp_num],
                                                    "tiffsep_print_page");
                }
            } else {
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    gs_free_object(pdev->memory, planes[comp_num + offset_plane],
                                                    "tiffsep_print_page");
                }
            }
            gx_downscaler_fin(&ds);
            gs_free_object(pdev->memory, sep_line, "tiffsep_print_page");
        }
        code1 = code;
        if (!tfdev->NoSeparationFiles) {
            for (comp_num = 0; comp_num < num_comp; comp_num++) {
                TIFFWriteDirectory(tfdev->tiff[comp_num]);
                if (fmt) {
                    int sep_num = tfdev->devn_params.separation_order_map[comp_num];

                    code = create_separation_file_name(tfdev, name, gp_file_name_sizeof, sep_num, false);
                    if (code < 0) {
                        code1 = code;
                        continue;
                    }
                    code = tiffsep_close_sep_file(tfdev, name, comp_num);
                    if (code >= 0)
                        code = gs_remove_outputfile_control_path(tfdev->memory, name);
                    if (code < 0) {
                        code1 = code;
                    }
                }
            }
        }
        TIFFWriteDirectory(tfdev->tiff_comp);
        if (fmt) {
            code = tiffsep_close_comp_file(tfdev, pdev->fname);
        }
        if (code1 < 0) {
            code = code1;
        }
    }

done:
    if (name)
        gs_free_object(pdev->memory, name, "tiffsep_print_page(name)");
    return code;
}

#ifdef WITH_CAL
static void
ht_callback(cal_halftone_data_t *ht, void *arg)
{
    tiffsep1_device *tfdev = (tiffsep1_device *)arg;
    int num_std_colorants = tfdev->devn_params.num_std_colorant_names;
    int num_order = tfdev->devn_params.num_separation_order_names;
    int num_spot = tfdev->devn_params.separations.num_separations;
    int comp_num;
    int num_comp = number_output_separations(tfdev->color_info.num_components,
                                             num_std_colorants, num_order, num_spot);
    /* Deliberately cast away const, cos tifflib has a bad interface. */
    unsigned char *data = (unsigned char *)ht->data;

    for (comp_num = 0; comp_num < num_comp; comp_num++) {
        TIFFWriteScanline(tfdev->tiff[comp_num], &data[ht->raster*comp_num], ht->y, 0);
    }
}
#endif

/*
 * Output the image data for the tiff separation (tiffsep1) device.  The data
 * for the tiffsep1 device is written in separate planes to separate files.
 *
 * The DeviceN parameters (SeparationOrder, SeparationColorNames, and
 * MaxSeparations) are applied to the tiffsep device.
 */
static int
tiffsep1_print_page(gx_device_printer * pdev, gp_file * file)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *)pdev;
    int num_std_colorants = tfdev->devn_params.num_std_colorant_names;
    int num_order = tfdev->devn_params.num_separation_order_names;
    int num_spot = tfdev->devn_params.separations.num_separations;
    int num_comp, comp_num, code = 0, code1 = 0;
    short map_comp_to_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    char *name = NULL;
    int save_depth = pdev->color_info.depth;
    int save_numcomps = pdev->color_info.num_components;
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int non_encodable_count = 0;

    if (tfdev->thresholds[0].dstart == NULL)
        return_error(gs_error_rangecheck);

    name = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "tiffsep1_print_page(name)");
    if (!name)
        return_error(gs_error_VMerror);

    build_comp_to_sep_map((tiffsep_device *)tfdev, map_comp_to_sep);

    /*
     * Since different pages may have different spot colors, if this is for a
     * page after Page 1, we require that each output file is unique with a "fmt"
     * (i.e. %d) as part of the filename. We create individual separation files
     * for each page of the input.
     * Since the TIFF lib requires seeakable files, /dev/null or nul: are
     * not allowed (as they are with the psdcmyk devices).
    */
    code = gx_parse_output_file_name(&parsed, &fmt, tfdev->fname,
                                     strlen(tfdev->fname), pdev->memory);
    if (code < 0 || (fmt == NULL && tfdev->PageCount > 0)) {
       emprintf(tfdev->memory,
                "\nUse of the %%d format is required to output more than one page to tiffsep1.\n"
                "See doc/Devices.htm#TIFF for details.\n\n");
       code = gs_note_error(gs_error_ioerror);
       goto done;
    }
    /* If the output file is on disk and the name contains a page #, */
    /* then delete the previous file. */
    if (pdev->file != NULL && parsed.iodev == iodev_default(pdev->memory) && fmt) {
        long count1 = pdev->PageCount;
        char *compname = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "tiffsep1_print_page(compname)");
        if (!compname) {
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }

        gx_device_close_output_file((gx_device *)pdev, pdev->fname, pdev->file);
        pdev->file = NULL;

        while (*fmt != 'l' && *fmt != '%')
            --fmt;
        if (*fmt == 'l')
            gs_sprintf(compname, parsed.fname, count1);
        else
            gs_sprintf(compname, parsed.fname, (int)count1);
        parsed.iodev->procs.delete_file(parsed.iodev, compname);
        /* we always need an open printer (it will get deleted in tiffsep1_prn_close */
        code = gdev_prn_open_printer((gx_device *)pdev, 1);

        gs_free_object(pdev->memory, compname, "tiffsep_print_page(compname)");
        if (code < 0) {
            goto done;
        }
    }

    /* Set up the separation output files */
    num_comp = number_output_separations( tfdev->color_info.num_components,
                                        num_std_colorants, num_order, num_spot);
    for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
        int sep_num = map_comp_to_sep[comp_num];

        code = create_separation_file_name((tiffsep_device *)tfdev, name,
                                        gp_file_name_sizeof, sep_num, true);
        if (code < 0) {
            goto done;
        }

        /* Open the separation file, if not already open */
        if (tfdev->sep_file[comp_num] == NULL) {
            code = gs_add_outputfile_control_path(tfdev->memory, name);
            if (code < 0) {
                goto done;
            }
            code = gx_device_open_output_file((gx_device *)pdev, name,
                    true, true, &(tfdev->sep_file[comp_num]));
            if (code < 0) {
                goto done;
            }
            tfdev->tiff[comp_num] = tiff_from_filep(pdev, name,
                                                    tfdev->sep_file[comp_num],
                                                    tfdev->BigEndian, tfdev->UseBigTIFF);
            if (!tfdev->tiff[comp_num]) {
                code = gs_note_error(gs_error_ioerror);
                goto done;
            }
        }

        pdev->color_info.depth = 8;     /* Create files for 8 bit gray */
        pdev->color_info.num_components = 1;
        code = tiff_set_fields_for_printer(pdev, tfdev->tiff[comp_num], 1, 0, tfdev->write_datetime);
        tiff_set_gray_fields(pdev, tfdev->tiff[comp_num], 1, tfdev->Compression, tfdev->MaxStripSize);
        pdev->color_info.depth = save_depth;
        pdev->color_info.num_components = save_numcomps;
        if (code < 0) {
            goto done;
        }

    }   /* end initialization of separation files */


    {   /* Get the expanded contone line, halftone and write out the dithered separations */
        byte *planes[GS_CLIENT_COLOR_MAX_COMPONENTS];
        int width = tfdev->width;
        int raster_plane = bitmap_raster(width * 8);
        int dithered_raster = ((7 + width) / 8) + ARCH_SIZEOF_LONG;
        int pixel, y;
        gs_get_bits_params_t params;
        gs_int_rect rect;
        uint32_t *dithered_line = NULL;

#ifdef WITH_CAL
        cal_context *cal = pdev->memory->gs_lib_ctx->core->cal_ctx;
        cal_halftone *cal_ht = NULL;
        cal_matrix matrix = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };

        cal_ht = cal_halftone_init(cal,
                                   pdev->memory->non_gc_memory,
                                   pdev->width,
                                   pdev->height,
                                   &matrix,
                                   comp_num,
                                   NULL,
                                   0,
                                   0,
                                   pdev->width,
                                   pdev->height,
                                   0);
        if (cal_ht != NULL) {
            for (comp_num = 0; comp_num < num_comp; comp_num++)
                if (cal_halftone_add_screen(cal,
                                            pdev->memory->non_gc_memory,
                                            cal_ht,
                                            0,
                                            tfdev->thresholds[comp_num].dwidth,
                                            tfdev->thresholds[comp_num].dheight,
                                            0,
                                            0,
                                            tfdev->thresholds[comp_num].dstart) < 0)
                    goto cal_fail;
        } else
#endif
        /* the dithered_line is assumed to be 32-bit aligned by the alloc */
        dithered_line = (uint32_t *)gs_alloc_bytes(pdev->memory, dithered_raster,
                                "tiffsep1_print_page");

        memset(planes, 0, sizeof(*planes) * GS_CLIENT_COLOR_MAX_COMPONENTS);

        /* Return planar data */
        params.options = (GB_RETURN_POINTER | GB_RETURN_COPY |
             GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_RASTER_STANDARD |
             GB_PACKING_PLANAR | GB_COLORS_NATIVE | GB_ALPHA_NONE);
        params.x_offset = 0;
        params.raster = bitmap_raster(width * pdev->color_info.depth);

        code = 0;
        for (comp_num = 0; comp_num < num_comp; comp_num++) {
            planes[comp_num] = gs_alloc_bytes(pdev->memory, raster_plane,
                                            "tiffsep1_print_page");
            if (planes[comp_num] == NULL) {
                code = gs_error_VMerror;
                break;
            }
        }

#ifdef WITH_CAL
        if (code < 0 || (cal_ht == NULL && dithered_line == NULL)) {
#else
        if (code < 0 || dithered_line == NULL) {
#endif
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }

        for (comp_num = 0; comp_num < num_comp; comp_num++ )
            TIFFCheckpointDirectory(tfdev->tiff[comp_num]);

        rect.p.x = 0;
        rect.q.x = pdev->width;
        /* Loop for the lines */
        for (y = 0; y < pdev->height; ++y) {
            rect.p.y = y;
            rect.q.y = y + 1;
            /* We have to reset the pointers since get_bits_rect will have moved them */
            for (comp_num = 0; comp_num < num_comp; comp_num++)
                params.data[comp_num] = planes[comp_num];
            code = (*dev_proc(pdev, get_bits_rectangle))((gx_device *)pdev, &rect, &params, NULL);
            if (code < 0)
                break;

#ifdef WITH_CAL
            if(cal_ht != NULL) {
                if (cal_halftone_process_planar(cal_ht,
                                                pdev->memory->non_gc_memory,
                                                (const byte * const *)&params.data[0],
                                                ht_callback,
                                                tfdev) < 0)
                    goto cal_fail;
            } else
#endif
            /* Dither the separation and write it out */
            for (comp_num = 0; comp_num < num_comp; comp_num++ ) {

/***** #define SKIP_HALFTONING_FOR_TIMING *****/ /* uncomment for timing test */
#ifndef SKIP_HALFTONING_FOR_TIMING

                /*
                 * Define 32-bit writes by default. Testing shows that while this is more
                 * complex code, it runs measurably and consistently faster than the more
                 * obvious 8-bit code. The 8-bit code is kept to help future optimization
                 * efforts determine what affects tight loop optimization. Subtracting the
                 * time when halftoning is skipped shows that the 32-bit halftoning is
                 * 27% faster.
                 */
#define USE_32_BIT_WRITES
                byte *thresh_line_base = tfdev->thresholds[comp_num].dstart +
			             ((y % tfdev->thresholds[comp_num].dheight) *
                                     tfdev->thresholds[comp_num].dwidth) ;
                byte *thresh_ptr = thresh_line_base;
                byte *thresh_limit = thresh_ptr + tfdev->thresholds[comp_num].dwidth;
                byte *src = params.data[comp_num];
#ifdef USE_32_BIT_WRITES
                uint32_t *dest = dithered_line;
                uint32_t val = 0;
                const uint32_t *mask = &bit_order[0];
#else   /* example 8-bit code */
                byte *dest = dithered_line;
                byte val = 0;
                byte mask = 0x80;
#endif /* USE_32_BIT_WRITES */

                for (pixel = 0; pixel < width; pixel++, src++) {
#ifdef USE_32_BIT_WRITES
                    if (*src < *thresh_ptr++)
                        val |= *mask;
                    if (++mask == &(bit_order[32])) {
                        *dest++ = val;
                        val = 0;
                        mask = &bit_order[0];
                    }
#else   /* example 8-bit code */
                    if (*src < *thresh_ptr++)
                        val |= mask;
                    mask >>= 1;
                    if (mask == 0) {
                        *dest++ = val;
                        val = 0;
                        mask = 0x80;
                    }
#endif /* USE_32_BIT_WRITES */
                    if (thresh_ptr >= thresh_limit)
                        thresh_ptr = thresh_line_base;
                } /* end src pixel loop - collect last bits if any */
                /* the following relies on their being enough 'pad' in dithered_line */
#ifdef USE_32_BIT_WRITES
                if (mask != &bit_order[0]) {
                    *dest = val;
                }
#else   /* example 8-bit code */
                if (mask != 0x80) {
                    *dest = val;
                }
#endif /* USE_32_BIT_WRITES */
#endif /* SKIP_HALFTONING_FOR_TIMING */
                TIFFWriteScanline(tfdev->tiff[comp_num], (tdata_t)dithered_line, y, 0);
            } /* end component loop */
        }
        /* Update the strip data */
        for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
            TIFFWriteDirectory(tfdev->tiff[comp_num]);
            if (fmt) {
                int sep_num = map_comp_to_sep[comp_num];

                code = create_separation_file_name((tiffsep_device *)tfdev, name, gp_file_name_sizeof, sep_num, false);
                if (code < 0) {
                    code1 = code;
                    continue;
                }
                code = tiffsep_close_sep_file((tiffsep_device *)tfdev, name, comp_num);
                if (code >= 0)
                    code = gs_remove_outputfile_control_path(tfdev->memory, name);
                if (code < 0) {
                    code1 = code;
                }
            }
        }
        code = code1;

#ifdef WITH_CAL
        if(0) {
cal_fail:
            code = gs_error_unknownerror;
        }
        cal_halftone_fin(cal_ht, pdev->memory->non_gc_memory);
#endif

        /* free any allocations and exit with code */
cleanup:
        gs_free_object(pdev->memory, dithered_line, "tiffsep1_print_page");
        for (comp_num = 0; comp_num < num_comp; comp_num++) {
            gs_free_object(pdev->memory, planes[comp_num], "tiffsep1_print_page");
        }
    }
    /*
     * If we have any non encodable pixels then signal an error.
     */
    if (non_encodable_count) {
        dmlprintf1(pdev->memory, "WARNING:  Non encodable pixels = %d\n", non_encodable_count);
        code = gs_note_error(gs_error_rangecheck);
    }

done:
    if (name)
        gs_free_object(pdev->memory, name, "tiffsep1_print_page(name)");
    return code;
}

/* The tiffscaled contone devices have to be able to change their color model
to allow a more flexible use of the post render ICC profile with the output
intent. For example, if we are wanting to render to a CMYK intermediate
output intent but we want the output to be in sRGB then we need to use
-sDEVICE=tiffscaled24 -dUsePDFX3Profile -sOutputICCProfile=default_cmyk.icc
-sPostRenderProfile=srgb.icc .  This should then render to a temporary
buffer the is in the OutputIntent color space and then be converted to
sRGB.  This should look like the result we get when we go out to the
tiffscaled32 device. This is in contrast to the command line
sDEVICE=tiffscaled24 -dUsePDFX3Profile -sPostRenderProfile=srgb.icc which would
end up using the output intent as a proofing profile.  The results may be similar
but not exact as overprint and spot colors would not appear correctly due to the
additive color model during rendering. */
int
tiff_open_s(gx_device *pdev)
{
    int code;

    /* Take care of any color model changes now */
    if (pdev->icc_struct->postren_profile != NULL &&
        pdev->icc_struct->device_profile[0]->num_comps != pdev->color_info.num_components &&
        pdev->color_info.depth == 8 * pdev->color_info.num_components) {

        code = gx_change_color_model((gx_device*)pdev,
            pdev->icc_struct->device_profile[0]->num_comps, 8);
        if (code < 0)
            return code;

        /* Reset the device procs */
        memset(&(pdev->procs), 0, sizeof(pdev->procs));
        switch (pdev->icc_struct->device_profile[0]->num_comps) {
        case 1:
            pdev->procs = tiffscaled8_procs;
            pdev->color_info.dither_colors = 0;
            pdev->color_info.max_color = 0;
            break;
        case 3:
            pdev->procs = tiffscaled24_procs;
            pdev->color_info.dither_colors = 0;
            pdev->color_info.max_color = 0;
            break;
        case 4:
            pdev->procs = tiffscaled32_procs;
            pdev->color_info.dither_colors = 256;
            pdev->color_info.max_color = 255;
            break;
        }
        check_device_separable(pdev);
        gx_device_fill_in_procs(pdev);
    }
    return tiff_open(pdev);
}

