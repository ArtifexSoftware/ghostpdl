/* Copyright (C) 2001-2025 Artifex Software, Inc.
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

/* Canon LBP-8II and LIPS III driver */
#include "gdevprn.h"

/*
  Modifications:
    2.2.97  Lauri Paatero
            Changed CSI command into ESC [. DCS commands may still need to be changed
            (to ESC P).
    4.9.96  Lauri Paatero
            Corrected LBP-8II margins again. Real problem was that (0,0) is NOT
                in upper left corner.
            Now using relative addressing for vertical addressing. This avoids
problems
                when printing to paper with wrong size.
    18.6.96 Lauri Paatero, lauri.paatero@paatero.pp.fi
            Corrected LBP-8II margins.
            Added logic to recognize (and optimize away) long strings of 00's in data.
            For LBP-8II removed use of 8-bit CSI (this does not work if 8-bit character
                set has been configured in LBP-8II. (Perhaps this should also be done
                for LBP-8III?)
  Original versions:
    LBP8 driver: Tom Quinn (trq@prg.oxford.ac.uk)
    LIPS III driver: Kenji Okamoto (okamoto@okamoto.cias.osakafu-u.ac.jp)
*/

#define X_DPI 300
#define Y_DPI 300

/* The device descriptors */
static dev_proc_print_page(lbp8_print_page);
#ifdef NOCONTRIB
static dev_proc_print_page(lips3_print_page);
#endif

const gx_device_printer far_data gs_lbp8_device =
  /* The print_page proc is compatible with allowing bg printing */
  prn_device(gdev_prn_initialize_device_procs_mono_bg, "lbp8",
        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
        X_DPI, Y_DPI,
        0.16, 0.2, 0.32, 0.21,		/* margins: left, bottom, right, top */
        1, lbp8_print_page);

#ifdef NOCONTRIB
/* The print_page proc is compatible with allowing bg printing */
const gx_device_printer far_data gs_lips3_device =
  prn_device(gdev_prn_initialize_device_procs_mono_bg, "lips3",
        82,				/* width_10ths, 8.3" */
        117,				/* height_10ths, 11.7" */
        X_DPI, Y_DPI,
        0.16, 0.27, 0.23, 0.27,		/* margins */
        1, lips3_print_page);
#endif

/* ------ Internal routines ------ */

#define ESC (char)0x1b
#define CSI '\233'
#define DCS '\220'
#define ST '\234'

static const char lbp8_init[] = {
  ESC, ';', ESC, 'c', ESC, ';', /* reset, ISO */
  ESC, '[', '2', '&', 'z',	/* fullpaint mode */
  ESC, '[', '1', '4', 'p',	/* select page type (A4) */
  ESC, '[', '1', '1', 'h',	/* set mode */
  ESC, '[', '7', ' ', 'I',	/* select unit size (300dpi)*/
  ESC, '[', '6', '3', 'k', 	/* Move 63 dots up (to top of printable area) */
};

#ifdef NOCONTRIB
static const char lips3_init[] = {
  ESC, '<', /* soft reset */
  DCS, '0', 'J', ST, /* JOB END */
  DCS, '3', '1', ';', '3', '0', '0', ';', '2', 'J', ST, /* 300dpi, LIPS3 JOB START */
  ESC, '<',  /* soft reset */
  DCS, '2', 'y', 'P', 'r', 'i', 'n', 't', 'i', 'n', 'g', '(', 'g', 's', ')', ST,  /* Printing (gs) display */
  ESC, '[', '?', '1', 'l',  /* auto cr-lf disable */
  ESC, '[', '?', '2', 'h', /* auto ff disable */
  ESC, '[', '1', '1', 'h', /* set mode */
  ESC, '[', '7', ' ', 'I', /* select unit size (300dpi)*/
  ESC, '[', 'f' /* move to home position */
};

static const char lips3_end[] = {
  DCS, '0', 'J', ST  /* JOB END */
};
#endif

/* Send the page to the printer.  */
static int
can_print_page(gx_device_printer *pdev, gp_file *prn_stream,
  const char *init, int init_size, const char *end, int end_size)
{
        char *data;
        char *out_data;
        int last_line_nro = 0;
        size_t line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        int code = 0;

        data = (char *)gs_alloc_bytes(pdev->memory,
                                      line_size*2,
                                      "lbp8_line_buffer");
        if (data == NULL)
            return_error(gs_error_VMerror);

        gp_fwrite(init, init_size, 1, prn_stream);		/* initialize */

        /* Send each scan line in turn */
        {
            int lnum;
            byte rmask = (byte)(0xff << (-pdev->width & 7));

            for ( lnum = 0; lnum < pdev->height; lnum++ ) {
                char *end_data = data + line_size;
                code = gdev_prn_copy_scan_lines(pdev, lnum,
                                               (byte *)data, line_size);
                if (code < 0)
                    goto xit;
                /* Mask off 1-bits beyond the line width. */
                end_data[-1] &= rmask;
                /* Remove trailing 0s. */
                while ( end_data > data && end_data[-1] == 0 )
                        end_data--;
                if ( end_data != data ) {
                    int num_cols = 0;
                    int out_count;
                    int zero_count;
                    out_data = data;

                    /* move down */
                    gp_fprintf(prn_stream, "%c[%de",
                               ESC, lnum-last_line_nro );
                    last_line_nro = lnum;

                    while (out_data < end_data) {
                        /* Remove leading 0s*/
                        while(out_data < end_data && *out_data == 0) {
                            num_cols += 8;
                            out_data++;
                        }

                        out_count = end_data - out_data;
                        zero_count = 0;

                        /* if there is a lot data, find if there is sequence of zeros */
                        if (out_count>22) {

                                out_count = 1;

                                while(out_data+out_count+zero_count < end_data) {
                                        if (out_data[zero_count+out_count] != 0) {
                                                out_count += 1+zero_count;
                                                zero_count = 0;
                                        }
                                        else {
                                                zero_count++;
                                                if (zero_count>20)
                                                        break;
                                        }
                                }

                        }

                        if (out_count==0)
                                break;

                        /* move down and across*/
                        gp_fprintf(prn_stream, "%c[%d`",
                                   ESC, num_cols );
                        /* transfer raster graphic command */
                        gp_fprintf(prn_stream, "%c[%d;%d;300;.r",
                                   ESC, out_count, out_count);

                        /* send the row */
                        gp_fwrite(out_data, sizeof(char),
                                  out_count, prn_stream);

                        out_data += out_count+zero_count;
                        num_cols += 8*(out_count+zero_count);
                    }
                }
            }
        }

        /* eject page */
        gp_fprintf(prn_stream, "%c=", ESC);

        /* terminate */
        if (end != NULL)
            (void)gp_fwrite(end, end_size, 1, prn_stream);

xit:
        gs_free_object(pdev->memory, data, "lbp8_line_buffer");

        return code;
}

/* Print an LBP-8 page. */
static int
lbp8_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
    return can_print_page(pdev, prn_stream, lbp8_init, sizeof(lbp8_init),
                              NULL, 0);
}

#ifdef NOCONTRIB
/* Print a LIPS III page. */
static int
lips3_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{	return can_print_page(pdev, prn_stream, lips3_init, sizeof(lips3_init),
                              lips3_end, sizeof(lips3_end));
}
#endif
