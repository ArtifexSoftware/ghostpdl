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

/* DigiBoard fax device. */
/***
 *** Note: this driver is maintained by a user: please contact
 ***       Rick Richardson (rick@digibd.com) if you have questions.
 ***/
#include "gdevprn.h"
#include "strimpl.h"
#include "scfx.h"
#include "gdevfax.h"
#include "gdevtfax.h"

/* Define the device parameters. */
#define X_DPI 204
#define Y_DPI 196

/* The device descriptors */

static dev_proc_open_device(dfax_prn_open);
static dev_proc_print_page(dfax_print_page);

struct gx_device_dfax_s {
        gx_device_common;
        gx_prn_device_common;
        long pageno;
        uint iwidth;		/* width of image data in pixels */
};
typedef struct gx_device_dfax_s gx_device_dfax;

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
dfax_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_mono_bg(dev);

    set_dev_proc(dev, open_device, dfax_prn_open);
    set_dev_proc(dev, output_page, gdev_prn_bg_output_page_seekable);
}

gx_device_dfax far_data gs_dfaxlow_device =
{   prn_device_std_body(gx_device_dfax, dfax_initialize_device_procs, "dfaxlow",
        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
        X_DPI, Y_DPI/2,
        0,0,0,0,			/* margins */
        1, dfax_print_page)
};

gx_device_dfax far_data gs_dfaxhigh_device =
{   prn_device_std_body(gx_device_dfax, dfax_initialize_device_procs, "dfaxhigh",
        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
        X_DPI, Y_DPI,
        0,0,0,0,			/* margins */
        1, dfax_print_page)
};

#define dfdev ((gx_device_dfax *)dev)

/* Open the device, adjusting the paper size. */
static int
dfax_prn_open(gx_device *dev)
{	dfdev->pageno = 0;
        return gdev_fax_open(dev);
}

/* Print a DigiFAX page. */
static int
dfax_print_page(gx_device_printer *dev, gp_file *prn_stream)
{	stream_CFE_state state;
        static char hdr[64] = "\000PC Research, Inc\000\000\000\000\000\000";
        int code;

        gdev_fax_init_state(&state, (gx_device_fax *)dev);
        state.EndOfLine = true;
        state.EncodedByteAlign = true;

        /* Start a page: write the header */
        hdr[24] = 0; hdr[28] = 1;
        hdr[26] = ++dfdev->pageno; hdr[27] = dfdev->pageno >> 8;
        if (dev->y_pixels_per_inch == Y_DPI)
                { hdr[45] = 0x40; hdr[29] = 1; }	/* high res */
        else
                { hdr[45] = hdr[29] = 0; }		/* low res */
        code = gp_fseek(prn_stream, 0, SEEK_END);
        if (code < 0)
            return_error(gs_error_ioerror);

        gp_fwrite(hdr, sizeof(hdr), 1, prn_stream);

        /* Write the page */
        code = gdev_fax_print_page(dev, prn_stream, &state);
        if (code < 0)
            return code;

        /* Fixup page count */
        if (gp_fseek(prn_stream, 24L, SEEK_SET) != 0)
            return_error(gs_error_ioerror);

        hdr[24] = dfdev->pageno; hdr[25] = dfdev->pageno >> 8;
        gp_fwrite(hdr+24, 2, 1, prn_stream);

        return 0;
}

#undef dfdev
