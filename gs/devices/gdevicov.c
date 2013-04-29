/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/



/* inkcov: compute ink coverage of the document being rendered.
 * originally copyright 2011 Sebastian Kapfer <sebastian.kapfer@physik.uni-erlangen.de>
 * but assigned to Artifex Software, Inc. (see http://bugs.ghostscript.com/show_bug.cgi?id=692665)
 *
 * output is plain text; one line per page.
 * columns 1 through 4 give the fraction of pixels containing
 * c, m, y and black ink.
 * column 5 is the string 'CMYK'.
 * column 6 is 'OK' if everything went fine, 'ERROR' if there
 * was a problem.
 *
 * the resolution defaults to 75 dpi (which gives good-enough estimates)
 * but can be changed via the -r flag to Ghostscript.
 */

#include "stdint_.h"
#include "stdio_.h"
#include "gdevprn.h"

static int
cov_write_page(gx_device_printer *pdev, FILE *file)
{
    int code = 0;
    int raster = gdev_prn_raster(pdev);
    int height = pdev->height;
    byte *line = gs_alloc_bytes(pdev->memory, raster, "ink coverage plugin buffer");
    int y;
    uint64_t c_pix = 0, m_pix = 0, y_pix = 0, k_pix = 0, total_pix = 0;

    for (y = 0; y < height; y++) {
        byte *row, *end;

        code = gdev_prn_get_bits(pdev, y, line, &row);
        if (code < 0)
            break;
        end = row + raster;

        for (; row < end; row += 4) {
            c_pix += !!row[0];
            m_pix += !!row[1];
            y_pix += !!row[2];
            k_pix += !!row[3];
            ++total_pix;
        }
    }

    if (pdev->width * height != total_pix)
        code = 1;

    gs_free_object(pdev->memory, line, "ink coverage plugin buffer");

    {
        double c = -1., m = -1., y = -1., k = -1.;
        if (code == 0) {
            c = (double)c_pix / total_pix;
            m = (double)m_pix / total_pix;
            y = (double)y_pix / total_pix;
            k = (double)k_pix / total_pix;
        }

        fprintf (file, "%8.5f %8.5f %8.5f %8.5f CMYK %s\n",
            c, m, y, k, code ? "ERROR" : "OK");
    }

    return 0;
}

static const gx_device_procs cov_procs =
{
    gdev_prn_open,
    NULL,			/* get_initial_matrix */
    NULL,			/* sync_output */
    /* Since the print_page doesn't alter the device, this device can print in the background */
    gdev_prn_bg_output_page,
    gdev_prn_close,
    NULL,			/* map_rgb_color */
    cmyk_8bit_map_color_rgb,
    NULL,			/* fill_rectangle */
    NULL,			/* tile_rectangle */
    NULL,			/* copy_mono */
    NULL,			/* copy_color */
    NULL,			/* draw_line */
    NULL,			/* get_bits */
    gdev_prn_get_params,
    gdev_prn_put_params,
    cmyk_8bit_map_cmyk_color,
    NULL,			/* get_xfont_procs */
    NULL,			/* get_xfont_device */
    NULL,			/* map_rgb_alpha_color */
    gx_page_device_get_page_device
};

const gx_device_printer gs_inkcov_device = prn_device(
    cov_procs, "inkcov",
    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
    75, 75,	/* dpi */
    0, 0, 0, 0,	/* margins */
    32, cov_write_page);

