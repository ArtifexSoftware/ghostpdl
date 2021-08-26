/* Copyright (C) 2001-2021 Artifex Software, Inc.
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

/* #define PPM_COMBINED_OUTPUT */ /* Uncomment to get PPM output similar to pknraw */

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
static void
tiffgray_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray(dev);

    set_dev_proc(dev, open_device, tiff_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params);
    set_dev_proc(dev, put_params, tiff_put_params);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_tiff gs_tiffgray_device = {
    prn_device_body(gx_device_tiff, tiffgray_initialize_device_procs, "tiffgray",
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

static void
tiffscaled_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray(dev);

    set_dev_proc(dev, open_device, tiff_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params_downscale);
    set_dev_proc(dev, put_params, tiff_put_params_downscale);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_tiff gs_tiffscaled_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled_initialize_device_procs,
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

static void
tiffscaled8_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray(dev);

    set_dev_proc(dev, open_device, tiff_open_s);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params_downscale);
    set_dev_proc(dev, put_params, tiff_put_params_downscale);
    set_dev_proc(dev, dev_spec_op, tiffscaled_spec_op);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_tiff gs_tiffscaled8_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled8_initialize_device_procs,
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

static void
tiffscaled24_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb(dev);

    set_dev_proc(dev, open_device, tiff_open_s);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params_downscale);
    set_dev_proc(dev, put_params, tiff_put_params_downscale);
    set_dev_proc(dev, dev_spec_op, tiffscaled_spec_op);
    set_dev_proc(dev, encode_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, gx_default_rgb_map_color_rgb);
}

const gx_device_tiff gs_tiffscaled24_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled24_initialize_device_procs,
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

static void
tiffscaled32_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk8(dev);

    set_dev_proc(dev, open_device, tiff_open_s);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params_downscale_cmyk);
    set_dev_proc(dev, put_params, tiff_put_params_downscale_cmyk);
    set_dev_proc(dev, dev_spec_op, tiffscaled_spec_op);
    set_dev_proc(dev, encode_color, cmyk_8bit_map_cmyk_color);
    set_dev_proc(dev, decode_color, cmyk_8bit_map_color_cmyk);
}

const gx_device_tiff gs_tiffscaled32_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled32_initialize_device_procs,
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

static void
tiffscaled4_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk8(dev);

    set_dev_proc(dev, open_device, tiff_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params_downscale_cmyk_ets);
    set_dev_proc(dev, put_params, tiff_put_params_downscale_cmyk_ets);
}

const gx_device_tiff gs_tiffscaled4_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled4_initialize_device_procs,
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
                                         &tfdev->downscale,
                                         tfdev->AdjustWidth,
                                         1, 1);
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
                                         &tfdev->downscale,
                                         tfdev->AdjustWidth,
                                         8, 1);
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
        icc_profile = tfdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];

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
                                         &tfdev->downscale,
                                         tfdev->AdjustWidth,
                                         8, 3);
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
                                         &tfdev->downscale,
                                         tfdev->AdjustWidth,
                                         8, 4);
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
                                         &tfdev->downscale,
                                         tfdev->AdjustWidth,
                                         1, 4);
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
    if (op == gxdso_supports_iccpostrender || op == gxdso_supports_devn
     || op == gxdso_skip_icc_component_validation) {
        return true;
    }
    return gdev_prn_dev_spec_op(dev_, op, data, datasize);
}

/* ------ The cmyk devices ------ */

static dev_proc_print_page(tiffcmyk_print_page);

/* 8-bit-per-plane separated CMYK color. */

static void
tiffcmyk_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk8(dev);

    set_dev_proc(dev, open_device, tiff_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params);
    set_dev_proc(dev, put_params, tiff_put_params);
}

const gx_device_tiff gs_tiff32nc_device = {
    prn_device_body(gx_device_tiff, tiffcmyk_initialize_device_procs, "tiff32nc",
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

static void
tiff64_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk16(dev);

    set_dev_proc(dev, open_device, tiff_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params);
    set_dev_proc(dev, put_params, tiff_put_params);
}

const gx_device_tiff gs_tiff64nc_device = {
    prn_device_body(gx_device_tiff, tiff64_initialize_device_procs, "tiff64nc",
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
static dev_proc_ret_devn_params_const(tiffsep_ret_devn_params_const);
static dev_proc_open_device(tiffsep1_prn_open);
static dev_proc_close_device(tiffsep1_prn_close);
static dev_proc_print_page(tiffsep1_print_page);
static dev_proc_encode_color(tiffsep1_encode_color);
static dev_proc_decode_color(tiffsep1_decode_color);

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
    bool warning_given;		/* avoid issuing lots of warnings */\
    gp_file *comp_file;            /* Underlying file for tiff_comp */\
    TIFF *tiff_comp;            /* tiff file for comp file */\
    gsicc_link_t *icclink      /* link profile if we are doing post rendering */

/*
 * A structure definition for a DeviceN type device
 */
typedef struct tiffsep_device_s {
    tiffsep_devices_common;
} tiffsep_device;

typedef struct tiffsep1_device_s {
    tiffsep_devices_common;
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

static void
tiffsep_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, open_device, tiffsep_prn_open);
    set_dev_proc(dev, close_device, tiffsep_prn_close);
    set_dev_proc(dev, map_color_rgb, tiffsep_decode_color);
    set_dev_proc(dev, get_params, tiffsep_get_params);
    set_dev_proc(dev, put_params, tiffsep_put_params);
    set_dev_proc(dev, get_color_mapping_procs, tiffsep_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, tiffsep_get_color_comp_index);
    set_dev_proc(dev, encode_color, tiffsep_encode_color);
    set_dev_proc(dev, decode_color, tiffsep_decode_color);
    set_dev_proc(dev, update_spot_equivalent_colors, tiffsep_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, tiffsep_ret_devn_params);
    set_dev_proc(dev, dev_spec_op, tiffsep_spec_op);
}

static void
tiffsep1_initialize_device_procs(gx_device *dev)
{
    tiffsep_initialize_device_procs(dev);
    set_dev_proc(dev, open_device, tiffsep1_prn_open);
    set_dev_proc(dev, close_device, tiffsep1_prn_close);
    set_dev_proc(dev, encode_color, tiffsep1_encode_color);
    set_dev_proc(dev, decode_color, tiffsep1_decode_color);
    set_dev_proc(dev, map_color_rgb, cmyk_1bit_map_color_rgb);
}

#define tiffsep_devices_body(dtype, procs, dname, ncomp, pol, depth, mg, mc, sl, cn, print_page, compr, bpc)\
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
        bpc,                      /* BitsPerComponent */\
        GS_SOFT_MAX_SPOTS,      /* max_spots */\
        false,                  /* Colorants not locked */\
        GX_DOWNSCALER_PARAMS_DEFAULTS

#define GCIB (ARCH_SIZEOF_GX_COLOR_INDEX * 8)

/*
 * TIFF devices with CMYK process color model and spot color support.
 */
const tiffsep_device gs_tiffsep_device =
{
    tiffsep_devices_body(tiffsep_device, tiffsep_initialize_device_procs, "tiffsep", ARCH_SIZEOF_GX_COLOR_INDEX, GX_CINFO_POLARITY_SUBTRACTIVE, GCIB, MAX_COLOR_VALUE, MAX_COLOR_VALUE, GX_CINFO_SEP_LIN, "DeviceCMYK", tiffsep_print_page, COMPRESSION_LZW, 8),
    /* devn_params specific parameters */
    { 8,                        /* Bits per color */
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
    tiffsep_devices_body(tiffsep1_device, tiffsep1_initialize_device_procs, "tiffsep1", ARCH_SIZEOF_GX_COLOR_INDEX, GX_CINFO_POLARITY_SUBTRACTIVE, GCIB, 1, 1, GX_CINFO_SEP_LIN, "DeviceCMYK", tiffsep1_print_page, COMPRESSION_CCITTFAX4, 1),
    /* devn_params specific parameters */
    { 1,                        /* Bits per color */
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
};

#undef GCIB

/*
 * The following procedures are used to map the standard color spaces into
 * the color components for the tiffsep device.
 */
static void
tiffsep_gray_cs_to_cm(const gx_device * dev, frac gray, frac out[])
{
    int * map = ((tiffsep_device *) dev)->devn_params.separation_order_map;

    gray_cs_to_devn_cm(dev, map, gray, out);
}

static void
tiffsep_rgb_cs_to_cm(const gx_device * dev, const gs_gstate *pgs,
                                   frac r, frac g, frac b, frac out[])
{
    int * map = ((tiffsep_device *) dev)->devn_params.separation_order_map;

    rgb_cs_to_devn_cm(dev, map, pgs, r, g, b, out);
}

static void
tiffsep_cmyk_cs_to_cm(const gx_device * dev,
                frac c, frac m, frac y, frac k, frac out[])
{
    const gs_devn_params *devn = tiffsep_ret_devn_params_const(dev);
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
tiffsep_get_color_mapping_procs(const gx_device * dev, const gx_device **tdev)
{
    *tdev = dev;
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

static const gs_devn_params *
tiffsep_ret_devn_params_const (const gx_device * dev)
{
    const tiffsep_device * pdev = (const tiffsep_device *)dev;

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

static void build_comp_to_sep_map(tiffsep_device *, short *);
static int number_output_separations(int, int, int, int);
static int create_separation_file_name(tiffsep_device *, char *, uint, int, bool);

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
    pdev->color_info.depth = bpc_to_depth(pdev->color_info.num_components,
                                          pdev_sep->devn_params.bitspercomponent);
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    code = gdev_prn_open_planar(pdev, true);
    while (pdev->child)
        pdev = pdev->child;
    ppdev = (gx_device_printer *)pdev;
    pdev_sep = (tiffsep1_device *)pdev;

    ppdev->file = NULL;
    pdev->icc_struct->supports_devn = true;

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

#ifndef PPM_COMBINED_OUTPUT	/* Only delete the default file if it isn't our pppraw output */
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
#endif

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
    cmm_dev_profile_t *profile_struct;
    gsicc_rendering_param_t rendering_params;

    /* Use our own warning and error message handlers in libtiff */
    tiff_set_handlers();

    code = dev_proc(pdev, get_profile)((gx_device *)pdev, &profile_struct);

    /* For the planar device we need to set up the bit depth of each plane.
       For other devices this is handled in check_device_separable where
       we compute the bit shift for the components etc. */
    for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
        pdev->color_info.comp_bits[k] = 8;
    }

    /* With planar the depth can be more than 64.  Update the color
       info to reflect the proper depth and number of planes */
    pdev_sep->warning_given = false;
    if (pdev_sep->devn_params.page_spot_colors >= 0) {

        /* PDF case, as the page spot colors are known. */
        if (profile_struct->spotnames != NULL) {

            /* PDF case, NCLR ICC profile with spot names. The ICC spots
               will use up some of the max_spots values. If max_spots is
               too small to accomodate even the ICC spots, throw an error */
            if (profile_struct->spotnames->count - 4 > pdev_sep->max_spots ||
                profile_struct->spotnames->count < 4 ||
                profile_struct->spotnames->count <
                profile_struct->device_profile[0]->num_comps) {
                gs_warn("ICC profile colorant names count error");
                return_error(gs_error_rangecheck);
            }
            pdev->color_info.num_components =
                (profile_struct->spotnames->count
                    + pdev_sep->devn_params.page_spot_colors);
            if (pdev->color_info.num_components > pdev->color_info.max_components)
                pdev->color_info.num_components = pdev->color_info.max_components;
        } else {

            /* Use the information that is in the page spot color. We should
               be here if we are processing a PDF and we do not have a DeviceN
               ICC profile specified for output */
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
           of PS files. With PDF we know a priori the number of spot
           colorants. However, the first time the device is opened,
           pdev_sep->devn_params.page_spot_colors is -1 even if we are
           dealing with a PDF file, so we will first find ourselves here,
           which will set num_comp based upon max_spots + 4. If -dMaxSpots
           was set (Default is GS_SOFT_MAX_SPOTS which is 10) ,
           it is made use of here. */
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
                profile_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                profile_struct->postren_profile, &rendering_params);
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
        TIFFClose(tfdev->tiff[comp_num]);
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
        TIFFClose(tfdev->tiff_comp);
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

        if (sep_num >= tfdev->devn_params.num_std_colorant_names) {
            sep_num -= tfdev->devn_params.num_std_colorant_names;
            if (gp_file_name_sizeof < tfdev->devn_params.separations.names[sep_num].size) {
                if (name)
                    gs_free_object(tfdev->memory, name, "tiffsep_print_cmyk_equivalent_colors(name)");
                return_error(gs_error_rangecheck);
            }
            memcpy(name,
                   (char *)tfdev->devn_params.separations.names[sep_num].data,
                   tfdev->devn_params.separations.names[sep_num].size);
            name[tfdev->devn_params.separations.names[sep_num].size] = '\0';
            dmlprintf5(tfdev->memory, "%%%%SeparationColor: \"%s\" 100%% ink = %hd %hd %hd %hd CMYK\n",
                       name,
                       cmyk_map[comp_num].c,
                       cmyk_map[comp_num].m,
                       cmyk_map[comp_num].y,
                       cmyk_map[comp_num].k);
        }
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
    int num_comp, comp_num, code = 0, code1 = 0;
    cmyk_composite_map cmyk_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
    char *name = NULL;
    bool double_f = false;
    int save_depth = pdev->color_info.depth;
    int save_numcomps = pdev->color_info.num_components;
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int plane_count = 0;  /* quiet compiler */
    int factor = tfdev->downscale.downscale_factor;
    int dst_bpc = tfdev->BitsPerComponent;
    gx_downscaler_t ds;
    int width = gx_downscaler_scale(tfdev->width, factor);
    int height = gx_downscaler_scale(tfdev->height, factor);

    name = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "tiffsep_print_page(name)");
    if (!name)
        return_error(gs_error_VMerror);

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

    build_cmyk_map((gx_device *)tfdev, num_comp, &tfdev->equiv_cmyk_colors, cmyk_map);
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
            code = gx_downscaler_init_planar(&ds, (gx_device *)pdev,
                                             8, dst_bpc, num_comp,
                                             &tfdev->downscale,
                                             &params);
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
        code = tiffsep_close_comp_file(tfdev, pdev->fname);
        if (code1 < 0) {
            code = code1;
        }
    }

done:
    if (name)
        gs_free_object(pdev->memory, name, "tiffsep_print_page(name)");
    return code;
}

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
    bool double_f = false;
    int base_filename_length = length_base_file_name((tiffsep_device *)tfdev, &double_f);
    short map_comp_to_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    char *name = NULL;
    int save_depth = pdev->color_info.depth;
    int save_numcomps = pdev->color_info.num_components;
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int non_encodable_count = 0;
    cmyk_composite_map cmyk_map[GX_DEVICE_COLOR_MAX_COMPONENTS];

    name = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "tiffsep1_print_page(name)");
    if (!name)
        return_error(gs_error_VMerror);

    build_comp_to_sep_map((tiffsep_device *)tfdev, map_comp_to_sep);

    /* Print the names of the spot colors */
    if (num_order == 0) {
        for (comp_num = 0; comp_num < num_spot; comp_num++) {
            copy_separation_name((tiffsep_device *)tfdev, name,
                gp_file_name_sizeof - base_filename_length - SUFFIX_SIZE, comp_num, 0);
            dmlprintf1(pdev->memory, "%%%%SeparationName: %s", name);
            dmlprintf4(pdev->memory, " CMYK = [ %d %d %d %d ]\n",
                tfdev->equiv_cmyk_colors.color[comp_num].c,
                tfdev->equiv_cmyk_colors.color[comp_num].m,
                tfdev->equiv_cmyk_colors.color[comp_num].y,
                tfdev->equiv_cmyk_colors.color[comp_num].k
            );
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
                "\nUse of the %%d format is required to output more than one page to tiffsep1.\n"
                "See doc/Devices.htm#TIFF for details.\n\n");
       code = gs_note_error(gs_error_ioerror);
       goto done;
    }
    /* If the output file is on disk and the name contains a page #, */
    /* then delete the previous file. */
    if (pdev->file != NULL && parsed.iodev == iodev_default(pdev->memory) && fmt) {
        char *compname = (char *)gs_alloc_bytes(pdev->memory, gp_file_name_sizeof, "tiffsep1_print_page(compname)");
        if (!compname) {
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }
#ifndef PPM_COMBINED_OUTPUT
        {
            long count1 = pdev->PageCount;

            gx_device_close_output_file((gx_device *)pdev, pdev->fname, pdev->file);
            pdev->file = NULL;
            while (*fmt != 'l' && *fmt != '%')
                --fmt;
            if (*fmt == 'l')
                gs_sprintf(compname, parsed.fname, count1);
            else
                gs_sprintf(compname, parsed.fname, (int)count1);
            parsed.iodev->procs.delete_file(parsed.iodev, compname);
        }
#endif	/* PPM_COMBINED_OUTPUT */

        /* we always need an open printer (it will get deleted in tiffsep1_prn_close */
        code = gdev_prn_open_printer((gx_device *)pdev, 1);

        gs_free_object(pdev->memory, compname, "tiffsep_print_page(compname)");
        if (code < 0) {
            goto done;
        }
    }
    /* Set up the separation output files */
    num_comp = number_output_separations(tfdev->color_info.num_components,
                                         num_std_colorants, num_order, num_spot);
    build_cmyk_map((gx_device *)tfdev, num_comp, &tfdev->equiv_cmyk_colors, cmyk_map);
    if (tfdev->PrintSpotCMYK) {
        code = print_cmyk_equivalent_colors((tiffsep_device *)tfdev, num_comp, cmyk_map);
        if (code < 0) {
            goto done;
        }
    }
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

        pdev->color_info.depth = 1;
        pdev->color_info.num_components = 1;
        code = tiff_set_fields_for_printer(pdev, tfdev->tiff[comp_num], 1, 0, tfdev->write_datetime);
        tiff_set_gray_fields(pdev, tfdev->tiff[comp_num], 1, tfdev->Compression, tfdev->MaxStripSize);
        pdev->color_info.depth = save_depth;
        pdev->color_info.num_components = save_numcomps;
        if (code < 0) {
            goto done;
        }

    }   /* end initialization of separation files */


    {   /* Get the halftoned line and write out the separations */
        byte *planes[GS_CLIENT_COLOR_MAX_COMPONENTS];
        int width = tfdev->width;
        int raster_plane = bitmap_raster(width);
        int y;
        gs_get_bits_params_t params;
        gs_int_rect rect;

        /* the line is assumed to be 32-bit aligned by the alloc */
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
            code = (*dev_proc(pdev, get_bits_rectangle))((gx_device *)pdev, &rect, &params);
            if (code < 0)
                break;

            /* write it out */
            for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
                int i;
                byte *src = params.data[comp_num];

                /* TIFF 1-bit is additive, invert the data */
                for (i=0; i<raster_plane; i++)
                    *src++ ^= 0xff;		/* invert the data */
                src = params.data[comp_num];
                TIFFWriteScanline(tfdev->tiff[comp_num], src, y, 0);
            } /* end component loop */
#ifdef PPM_COMBINED_OUTPUT
            {
                int i;

                if (y == 0) {
                    gp_fprintf(pdev->file, "P6\n");
                    gp_fprintf(pdev->file, "# Image generated by %s (device=pkmraw)\n", gs_product);
                    gp_fprintf(pdev->file, "%d %d\n255\n", pdev->width, pdev->height);
                }
                for (i=0; i<pdev->width; i += 8) {
                    int b, ib = i>>3;
                    byte C = *((byte *)(params.data[0]) + ib);
                    byte M = *((byte *)(params.data[1]) + ib);
                    byte Y = *((byte *)(params.data[2]) + ib);
                    byte K = *((byte *)(params.data[3]) + ib);
                    byte mask = 128;

                    for (b=7; b >= 0; b--) {
                        byte RGB[3];

                        if (i + (8-b) > pdev->width)
                            break;
                        if ((K & mask) != 0) {
                            RGB[0] = (C & mask) == 0 ? 0 : 255;
                            RGB[1] = (M & mask) == 0 ? 0 : 255;
                            RGB[2] = (Y & mask) == 0 ? 0 : 255;
                        } else {
                            RGB[0] = RGB[1] = RGB[2] = 0;
                        }
                        /* If there are any spot colors, add them in proportionally to this dot */
                        if (num_comp > 4) {
                            uint64_t SPOT[4] = { 0, 0, 0, 0 };  /* accumulate frac colorants */
                            int s;
                            uint64_t denom_scale = frac_1 * (num_comp - 3) / 255;

                            for (s=4; s<num_comp; s++) {
                                if ((*((byte *)(params.data[s]) + ib) & mask) == 0) {
                                    SPOT[0] += cmyk_map[s].c;
                                    SPOT[1] += cmyk_map[s].m;
                                    SPOT[2] += cmyk_map[s].y;
                                    SPOT[3] += cmyk_map[s].k;
                                }
                            }
                            for (s=0; s<4; s++)
                                SPOT[s] /= denom_scale;         /* map to 0..255 range */
                            RGB[0] = RGB[0] > SPOT[0] + SPOT[3] ? RGB[0] -= SPOT[0] + SPOT[3] : 0;
                            RGB[1] = RGB[1] > SPOT[1] + SPOT[3] ? RGB[1] -= SPOT[1] + SPOT[3] : 0;
                            RGB[2] = RGB[2] > SPOT[2] + SPOT[3] ? RGB[2] -= SPOT[2] + SPOT[3] : 0;
                        }
                        gp_fwrite(RGB, 3, 1, pdev->file);
                        mask >>= 1;
                    }
                }
                gp_fflush(pdev->file);
            }
#endif	/* PPM_COMBINED_OUTPUT */
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

        /* free any allocations and exit with code */
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

/*
 * Encode a list of colorant values into a gx_color_index_value.
 */
static gx_color_index
tiffsep1_encode_color(gx_device *dev, const gx_color_value colors[])
{
    gx_color_index color = 0;
    int i = 0;
    int ncomp = dev->color_info.num_components;

    for (; i < ncomp; i++) {
        color <<= 1;
        color |= colors[i] == gx_max_color_value;
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 */
static int
tiffsep1_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    int i = 0;
    int ncomp = dev->color_info.num_components;

    for (; i < ncomp; i++) {
        out[ncomp - i - 1] = (color & 1) ? gx_max_color_value : 0;
        color >>= 1;
    }
    return 0;
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
        pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE]->num_comps != pdev->color_info.num_components &&
        pdev->color_info.depth == 8 * pdev->color_info.num_components) {

        code = gx_change_color_model((gx_device*)pdev,
            pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE]->num_comps, 8);
        if (code < 0)
            return code;

        /* Reset the device procs */
        memset(&(pdev->procs), 0, sizeof(pdev->procs));
        switch (pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE]->num_comps) {
        case 1:
            pdev->initialize_device_procs = tiffscaled8_initialize_device_procs;
            pdev->color_info.dither_colors = 0;
            pdev->color_info.max_color = 0;
            break;
        case 3:
            pdev->initialize_device_procs = tiffscaled24_initialize_device_procs;
            pdev->color_info.dither_colors = 0;
            pdev->color_info.max_color = 0;
            break;
        case 4:
            pdev->initialize_device_procs = tiffscaled32_initialize_device_procs;
            pdev->color_info.dither_colors = 256;
            pdev->color_info.max_color = 255;
            break;
        }
        pdev->initialize_device_procs(pdev);
        /* We know pdev->procs.initialize_device is NULL */
        check_device_separable(pdev);
        gx_device_fill_in_procs(pdev);
    }
    return tiff_open(pdev);
}
