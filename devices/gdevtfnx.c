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


/* 12-bit & 24-bit RGB uncompressed TIFF driver */

#include "stdint_.h"   /* for tiff.h */
#include "gdevtifs.h"
#include "gdevprn.h"
#include "gscms.h"

#include "gstiffio.h"

/*
 * Thanks to Alan Barclay <alan@escribe.co.uk> for donating the original
 * version of this code to Ghostscript.
 */

/* ------ The device descriptors ------ */

/* Default X and Y resolution */
#define X_DPI 72
#define Y_DPI 72

static dev_proc_print_page(tiff12_print_page);
static dev_proc_print_page(tiff_rgb_print_page);

/* FIXME: From initial analysis this is NOT safe for bg_printing, but might be fixable */

static void
tiff12_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb(dev);

    set_dev_proc(dev, open_device, tiff_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params);
    set_dev_proc(dev, put_params, tiff_put_params);
}

static void
tiff24_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb(dev);

    set_dev_proc(dev, open_device, tiff_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, tiff_close);
    set_dev_proc(dev, get_params, tiff_get_params);
    set_dev_proc(dev, put_params, tiff_put_params);
}

const gx_device_tiff gs_tiff12nc_device = {
    prn_device_std_body(gx_device_tiff, tiff12_initialize_device_procs, "tiff12nc",
                        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                        X_DPI, Y_DPI,
                        0, 0, 0, 0,
                        24, tiff12_print_page),
    ARCH_IS_BIG_ENDIAN          /* default to native endian (i.e. use big endian iff the platform is so*/,
    false,                      /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0 /* icclink */
};

const gx_device_tiff gs_tiff24nc_device = {
    prn_device_std_body(gx_device_tiff, tiff24_initialize_device_procs, "tiff24nc",
                        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                        X_DPI, Y_DPI,
                        0, 0, 0, 0,
                        24, tiff_rgb_print_page),
    ARCH_IS_BIG_ENDIAN          /* default to native endian (i.e. use big endian iff the platform is so*/,
    false,                      /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0 /* icclink */
};

const gx_device_tiff gs_tiff48nc_device = {
    prn_device_std_body(gx_device_tiff, tiff24_initialize_device_procs, "tiff48nc",
                        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                        X_DPI, Y_DPI,
                        0, 0, 0, 0,
                        48, tiff_rgb_print_page),
    ARCH_IS_BIG_ENDIAN          /* default to native endian (i.e. use big endian iff the platform is so*/,
    false,                      /* default to not bigtiff */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    0, /* Adjust size */
    true, /* write_datetime */
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0 /* icclink */
};

/* ------ Private functions ------ */

static void
tiff_set_rgb_fields(gx_device_tiff *tfdev)
{
    /* Put in a switch statement in case we want to have others */
    switch (tfdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE]->data_cs) {
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
tiff12_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    TIFFSetField(tfdev->tif, TIFFTAG_BITSPERSAMPLE, 4);
    tiff_set_rgb_fields(tfdev);

    TIFFCheckpointDirectory(tfdev->tif);

    /* Write the page data. */
    {
        int y;
        int size = gdev_prn_raster(pdev);

        /* We allocate an extra 5 bytes to avoid buffer overflow when accessing
        src[5] below, if size if not multiple of 6. This fixes bug-701807. */
        int size_alloc = size + 5;
        byte *data = gs_alloc_bytes(pdev->memory, size_alloc, "tiff12_print_page");

        if (data == 0)
            return_error(gs_error_VMerror);

        memset(data, 0, size_alloc);

        for (y = 0; y < pdev->height; ++y) {
            const byte *src;
            byte *dest;
            int x;

            code = gdev_prn_copy_scan_lines(pdev, y, data, size);
            if (code < 0)
                break;

            for (src = data, dest = data, x = 0; x < size;
                 src += 6, dest += 3, x += 6
                ) {
                dest[0] = (src[0] & 0xf0) | (src[1] >> 4);
                dest[1] = (src[2] & 0xf0) | (src[3] >> 4);
                dest[2] = (src[4] & 0xf0) | (src[5] >> 4);
            }
            TIFFWriteScanline(tfdev->tif, data, y, 0);
        }
        gs_free_object(pdev->memory, data, "tiff12_print_page");

        TIFFWriteDirectory(tfdev->tif);
    }

    return code;
}

static int
tiff_rgb_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    TIFFSetField(tfdev->tif, TIFFTAG_BITSPERSAMPLE,
                 pdev->color_info.depth / pdev->color_info.num_components);
    tiff_set_rgb_fields(tfdev);

    /* Write the page data. */
    return tiff_print_page(pdev, tfdev->tif, 0);
}
