/* Copyright (C) 1992, 1993, 1997, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* .BMP file format output drivers */
#include "gdevprn.h"
#include "gdevpccm.h"
#include "gdevbmp.h"

/* ------ The device descriptors ------ */

private dev_proc_print_page(bmp_print_page);

/* Monochrome. */

gx_device_printer far_data gs_bmpmono_device =
prn_device(prn_std_procs, "bmpmono",
	   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	   X_DPI, Y_DPI,
	   0, 0, 0, 0,		/* margins */
	   1, bmp_print_page);

/* 4-bit planar (EGA/VGA-style) color. */

private const gx_device_procs bmp16_procs =
prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		pc_4bit_map_rgb_color, pc_4bit_map_color_rgb);
gx_device_printer far_data gs_bmp16_device =
prn_device(bmp16_procs, "bmp16",
	   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	   X_DPI, Y_DPI,
	   0, 0, 0, 0,		/* margins */
	   4, bmp_print_page);

/* 8-bit (SuperVGA-style) color. */
/* (Uses a fixed palette of 3,3,2 bits.) */

private const gx_device_procs bmp256_procs =
prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		pc_8bit_map_rgb_color, pc_8bit_map_color_rgb);
gx_device_printer far_data gs_bmp256_device =
prn_device(bmp256_procs, "bmp256",
	   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	   X_DPI, Y_DPI,
	   0, 0, 0, 0,		/* margins */
	   8, bmp_print_page);

/* 24-bit color. */

private const gx_device_procs bmp16m_procs =
prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		bmp_map_16m_rgb_color, bmp_map_16m_color_rgb);
gx_device_printer far_data gs_bmp16m_device =
prn_device(bmp16m_procs, "bmp16m",
	   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	   X_DPI, Y_DPI,
	   0, 0, 0, 0,		/* margins */
	   24, bmp_print_page);

/* ------ Private definitions ------ */

/* Write out a page in BMP format. */
/* This routine is used for all formats. */
private int
bmp_print_page(gx_device_printer * pdev, FILE * file)
{
    uint raster = gdev_prn_raster(pdev);
    /* BMP scan lines are padded to 32 bits. */
    uint bmp_raster = raster + (-raster & 3);
    byte *row = (byte *)gs_malloc(bmp_raster, 1, "bmp file buffer");
    int y;
    int code;		/* return code */

    if (row == 0)		/* can't allocate row buffer */
	return_error(gs_error_VMerror);

    /* Write the file header. */

    code = write_bmp_header(pdev, file);
    if (code < 0)
	goto done;

    /* Write the contents of the image. */
    /* BMP files want the image in bottom-to-top order! */

    for (y = pdev->height - 1; y >= 0; y--) {
	gdev_prn_copy_scan_lines(pdev, y, row, raster);
	fwrite((const char *)row, bmp_raster, 1, file);
    }

done:
    gs_free((char *)row, bmp_raster, 1, "bmp file buffer");

    return code;
}
