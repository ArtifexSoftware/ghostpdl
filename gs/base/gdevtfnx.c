/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* 12-bit & 24-bit RGB uncompressed TIFF driver */

#include "stdint_.h"   /* for tiff.h */
#include "gdevtifs.h"
#include "gdevprn.h"

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

static const gx_device_procs tiff12_procs =
prn_color_params_procs(tiff_open, tiff_output_page, tiff_close,
		gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb,
		tiff_get_params, tiff_put_params);
static const gx_device_procs tiff24_procs =
prn_color_params_procs(tiff_open, tiff_output_page, tiff_close,
		gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb,
		tiff_get_params, tiff_put_params);

const gx_device_tiff gs_tiff12nc_device = {
    prn_device_std_body(gx_device_tiff, tiff12_procs, "tiff12nc",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,
			24, tiff12_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE
};

const gx_device_tiff gs_tiff24nc_device = {
    prn_device_std_body(gx_device_tiff, tiff24_procs, "tiff24nc",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,
			24, tiff_rgb_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE
};

const gx_device_tiff gs_tiff48nc_device = {
    prn_device_std_body(gx_device_tiff, tiff24_procs, "tiff48nc",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,
			48, tiff_rgb_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE
};

/* ------ Private functions ------ */

static void
tiff_set_rgb_fields(gx_device_tiff *tfdev)
{
    TIFFSetField(tfdev->tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tfdev->tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tfdev->tif, TIFFTAG_SAMPLESPERPIXEL, 3);

    tiff_set_compression((gx_device_printer *)tfdev, tfdev->tif,
			 tfdev->Compression, tfdev->MaxStripSize);
}

static int
tiff12_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    /* open the TIFF device */
    if (gdev_prn_file_is_new(pdev)) {
	tfdev->tif = tiff_from_filep(pdev->dname, file, tfdev->BigEndian);
	if (!tfdev->tif)
	    return_error(gs_error_invalidfileaccess);
    }

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
	byte *data = gs_alloc_bytes(pdev->memory, size, "tiff12_print_page");

	if (data == 0)
	    return_error(gs_error_VMerror);

	memset(data, 0, size);

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
tiff_rgb_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    /* open the TIFF device */
    if (gdev_prn_file_is_new(pdev)) {
	tfdev->tif = tiff_from_filep(pdev->dname, file, tfdev->BigEndian);
	if (!tfdev->tif)
	    return_error(gs_error_invalidfileaccess);
    }

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
	return code;

    TIFFSetField(tfdev->tif, TIFFTAG_BITSPERSAMPLE,
		 pdev->color_info.depth / pdev->color_info.num_components);
    tiff_set_rgb_fields(tfdev);

    /* Write the page data. */
    return tiff_print_page(pdev, tfdev->tif);
}
