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

/* MIFF (Magick Image File Format - native for ImageMagick) file format driver */
#include "gdevprn.h"

/* ------ The device descriptor ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

static dev_proc_print_page(miff24_print_page);

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
miff24_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb_bg(dev);
}

const gx_device_printer gs_miff24_device =
prn_device(miff24_initialize_device_procs, "miff24",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           0, 0, 0, 0,		/* margins */
           24, miff24_print_page);

/* Print one page in 24-bit RLE direct color format. */
static int
miff24_print_page(gx_device_printer * pdev, gp_file * file)
{
    int raster = gx_device_raster((gx_device *) pdev, true);
    byte *line = gs_alloc_bytes(pdev->memory, raster, "miff line buffer");
    int y;
    int code = 0;		/* return code */

    if (line == 0)		/* can't allocate line buffer */
        return_error(gs_error_VMerror);
    gp_fputs("id=ImageMagick\n", file);
    gp_fputs("class=DirectClass\n", file);
    gp_fprintf(file, "columns=%d\n", pdev->width);
    gp_fputs("compression=RunlengthEncoded\n", file);
    gp_fprintf(file, "rows=%d\n", pdev->height);
    gp_fputs(":\n", file);
    for (y = 0; y < pdev->height; ++y) {
        byte *row;
        byte *end;

        code = gdev_prn_get_bits(pdev, y, line, &row);
        if (code < 0)
            break;
        end = row + pdev->width * 3;
        while (row < end) {
            int count = 0;

            while (count < 255 && row < end - 3 &&
                   row[0] == row[3] && row[1] == row[4] &&
                   row[2] == row[5]
                )
                ++count, row += 3;
            gp_fputc(row[0], file);
            gp_fputc(row[1], file);
            gp_fputc(row[2], file);
            gp_fputc(count, file);
            row += 3;
        }
    }
    gs_free_object(pdev->memory, line, "miff line buffer");

    return code;
}
