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
cov_write_page(gx_device_printer *pdev, gp_file *file)
{
    int code = 0, ecode = 0;
    int raster = gdev_prn_raster(pdev);
    int height = pdev->height;
    byte *line = gs_alloc_bytes(pdev->memory, raster, "ink coverage plugin buffer");
    int y;
    uint64_t c_pix = 0, m_pix = 0, y_pix = 0, k_pix = 0, total_pix = 0;

    if (line == NULL)
        return gs_error_VMerror;
    for (y = 0; y < height; y++) {
        byte *row, *end;

        ecode = gdev_prn_get_bits(pdev, y, line, &row);
        if (ecode < 0)
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

    if ((uint64_t)pdev->width * height != total_pix || total_pix == 0)
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


        if (IS_LIBCTX_STDOUT(pdev->memory, gp_get_file(file))) {
            outprintf(pdev->memory, "%8.5f %8.5f %8.5f %8.5f CMYK %s\n",
                c, m, y, k, code ? "ERROR" : "OK");
        }
        else if (IS_LIBCTX_STDERR(pdev->memory, gp_get_file(file))) {
            errprintf(pdev->memory, "%8.5f %8.5f %8.5f %8.5f CMYK %s\n",
                c, m, y, k, code ? "ERROR" : "OK");
        }
        else {
            gp_fprintf (file, "%8.5f %8.5f %8.5f %8.5f CMYK %s\n",
                c, m, y, k, code ? "ERROR" : "OK");
        }
    }

    return (code > 0) ? ecode : 0;
}

/*  cov_write_page2 gave ink coverage values not ratecoverage */

static int cov_write_page_ink(gx_device_printer *pdev, gp_file *file)
{
    int code = 0, ecode = 0;
    int raster = gdev_prn_raster(pdev);
    int height = pdev->height;
	double dc_pix=0;
	double dm_pix=0;
	double dy_pix=0;
	double dk_pix=0;

    byte *line = gs_alloc_bytes(pdev->memory, raster, "ink coverage plugin buffer");
    int y;
    uint64_t  total_pix = 0;

    if (line == NULL)
        return gs_error_VMerror;
    for (y = 0; y < height; y++) {
        byte *row, *end;

        ecode = gdev_prn_get_bits(pdev, y, line, &row);
        if (ecode < 0)
            break;
        end = row + raster;

        for (; row < end; row += 4) {

			dc_pix += row[0];

            dm_pix += row[1];

			dy_pix += row[2];

			dk_pix += row[3];

			++total_pix;
        }
    }

    if ((uint64_t)pdev->width * height != total_pix || total_pix == 0)
        code = 1;

    gs_free_object(pdev->memory, line, "ink coverage plugin buffer");

    {
        double c = -1., m = -1., y = -1., k = -1.;
        if (code == 0) {
            c = (dc_pix*100) / (total_pix*255);
            m = (dm_pix*100) / (total_pix*255);
            y = (dy_pix*100) / (total_pix*255);
            k = (dk_pix*100) / (total_pix*255);
        }

        if (IS_LIBCTX_STDOUT(pdev->memory, gp_get_file(file))) {
            outprintf(pdev->memory, "%8.5f %8.5f %8.5f %8.5f CMYK %s\n",
                c, m, y, k, code ? "ERROR" : "OK");
        }
        else if (IS_LIBCTX_STDERR(pdev->memory, gp_get_file(file))) {
            errprintf(pdev->memory, "%8.5f %8.5f %8.5f %8.5f CMYK %s\n",
                c, m, y, k, code ? "ERROR" : "OK");
        }
        else {
            gp_fprintf (file, "%8.5f %8.5f %8.5f %8.5f CMYK %s\n",
                c, m, y, k, code ? "ERROR" : "OK");
        }
    }

    return (code > 0) ? ecode : 0;
}

const gx_device_printer gs_inkcov_device = prn_device(
    gdev_prn_initialize_device_procs_cmyk8_bg, "inkcov",
    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
    75, 75,	/* dpi */
    0, 0, 0, 0,	/* margins */
    32, cov_write_page);

const gx_device_printer gs_ink_cov_device = prn_device(
    gdev_prn_initialize_device_procs_cmyk8_bg, "ink_cov",
    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
    75, 75,	/* dpi */
    0, 0, 0, 0,	/* margins */
    32, cov_write_page_ink);
