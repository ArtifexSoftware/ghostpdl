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


/* H-P PaintJet, PaintJet XL, and DEC LJ250 drivers. */
/* Thanks to Rob Reiss (rob@moray.berkeley.edu) for the PaintJet XL */
/* modifications. */
#include "gdevprn.h"
#include "gdevpcl.h"

/* X_DPI and Y_DPI must be the same, and may be either 90 or 180. */
#define X_DPI 180
#define Y_DPI 180

/* The device descriptors */
static dev_proc_print_page(lj250_print_page);
static dev_proc_print_page(paintjet_print_page);
static dev_proc_print_page(pjetxl_print_page);
static int pj_common_print_page(gx_device_printer *, gp_file *, int, const char *);
/* Since the print_page doesn't alter the device, this device can print in the background */
static gx_device_procs paintjet_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_bg_output_page, gdev_prn_close,
    gdev_pcl_3bit_map_rgb_color, gdev_pcl_3bit_map_color_rgb);
const gx_device_printer far_data gs_lj250_device =
  prn_device(paintjet_procs, "lj250",
        85,				/* width_10ths, 8.5" */
        110,				/* height_10ths, 11" */
        X_DPI, Y_DPI,
        0.25, 0, 0.25, 0,		/* margins */
        3, lj250_print_page);
const gx_device_printer far_data gs_paintjet_device =
  prn_device(paintjet_procs, "paintjet",
        85,				/* width_10ths, 8.5" */
        110,				/* height_10ths, 11" */
        X_DPI, Y_DPI,
        0.25, 0, 0.25, 0,		/* margins */
        3, paintjet_print_page);
/* Since the print_page doesn't alter the device, this device can print in the background */
static gx_device_procs pjetxl_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_bg_output_page, gdev_prn_close,
    gdev_pcl_3bit_map_rgb_color, gdev_pcl_3bit_map_color_rgb);
const gx_device_printer far_data gs_pjetxl_device =
  prn_device(pjetxl_procs, "pjetxl",
        85,				/* width_10ths, 8.5" */
        110,				/* height_10ths, 11" */
        X_DPI, Y_DPI,
        0.25, 0, 0, 0,			/* margins */
        3, pjetxl_print_page);

/* Forward references */
static int compress1_row(const byte *, const byte *, byte *);

/* ------ Internal routines ------ */

/* Send a page to the LJ250.  We need to enter and exit */
/* the PaintJet emulation mode. */
static int
lj250_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{	gp_fputs("\033%8", prn_stream);	/* Enter PCL emulation mode */
        /* ends raster graphics to set raster graphics resolution */
        gp_fputs("\033*rB", prn_stream);
        /* Exit PCL emulation mode after printing */
        return pj_common_print_page(pdev, prn_stream, 0, "\033*r0B\014\033%@");
}

/* Send a page to the PaintJet. */
static int
paintjet_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{	/* ends raster graphics to set raster graphics resolution */
        gp_fputs("\033*rB", prn_stream);
        return pj_common_print_page(pdev, prn_stream, 0, "\033*r0B\014");
}

/* Send a page to the PaintJet XL. */
static int
pjetxl_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{	/* Initialize PaintJet XL for printing */
        gp_fputs("\033E", prn_stream);
        /* The XL has a different vertical origin, who knows why?? */
        return pj_common_print_page(pdev, prn_stream, -360, "\033*rC");
}

/* Send the page to the printer.  Compress each scan line. */
static int
pj_common_print_page(gx_device_printer *pdev, gp_file *prn_stream, int y_origin,
  const char *end_page)
{
        int line_size;
        int data_size;
        byte *data = NULL;
        byte *plane_data = NULL;
        byte *temp = NULL;
        int code = 0;

        /* We round up line_size to a multiple of 8 bytes */
        /* because that's the unit of transposition from pixels to planes. */
        line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        line_size = (line_size + 7) / 8 * 8;
        data_size = line_size * 8;

        data =
                (byte *)gs_malloc(pdev->memory, data_size, 1,
                                  "paintjet_print_page(data)");
        plane_data =
                (byte *)gs_malloc(pdev->memory, line_size * 3, 1,
                                  "paintjet_print_page(plane_data)");
        temp = gs_malloc(pdev->memory, line_size * 2, 1, "paintjet_print_page(temp)");

        if ( data == 0 || plane_data == 0 || temp == 0)
        {	if ( data )
                        gs_free(pdev->memory, (char *)data, data_size, 1,
                                "paintjet_print_page(data)");
                if ( plane_data )
                        gs_free(pdev->memory, (char *)plane_data, line_size * 3, 1,
                                "paintjet_print_page(plane_data)");
                if (temp)
                        gs_free(pdev->memory, temp, line_size * 2, 1,
                                "paintjet_print_page(temp)");
                return_error(gs_error_VMerror);
        }
        memset(data, 0x00, data_size);

        /* set raster graphics resolution -- 90 or 180 dpi */
        gp_fprintf(prn_stream, "\033*t%dR", X_DPI);

        /* set the line width */
        gp_fprintf(prn_stream, "\033*r%dS", data_size);

        /* set the number of color planes */
        gp_fprintf(prn_stream, "\033*r%dU", 3);		/* always 3 */

        /* move to top left of page */
        gp_fprintf(prn_stream, "\033&a0H\033&a%dV", y_origin);

        /* select data compression */
        gp_fputs("\033*b1M", prn_stream);

        /* start raster graphics */
        gp_fputs("\033*r1A", prn_stream);

        /* Send each scan line in turn */
           {	int lnum;
                int num_blank_lines = 0;
                for ( lnum = 0; lnum < pdev->height; lnum++ )
                   {	byte *end_data = data + line_size;
                        code = gdev_prn_copy_scan_lines(pdev, lnum,
                                                 (byte *)data, line_size);
                        if (code < 0)
                            goto xit;
                        /* Remove trailing 0s. */
                        while ( end_data > data && end_data[-1] == 0 )
                                end_data--;
                        if ( end_data == data )
                           {	/* Blank line */
                                num_blank_lines++;
                           }
                        else
                           {	int i;
                                byte *odp;
                                byte *row;

                                /* Pad with 0s to fill out the last */
                                /* block of 8 bytes. */
                                memset(end_data, 0, 7);

                                /* Transpose the data to get pixel planes. */
                                for ( i = 0, odp = plane_data; i < data_size;
                                      i += 8, odp++
                                    )
                                 { /* The following is for 16-bit machines */
#define spread3(c)\
 { 0, c, c*0x100, c*0x101, c*0x10000L, c*0x10001L, c*0x10100L, c*0x10101L }
                                   static ulong spr40[8] = spread3(0x40);
                                   static ulong spr8[8] = spread3(8);
                                   static ulong spr2[8] = spread3(2);
                                   register byte *dp = data + i;
                                   register ulong pword =
                                     (spr40[dp[0]] << 1) +
                                     (spr40[dp[1]]) +
                                     (spr40[dp[2]] >> 1) +
                                     (spr8[dp[3]] << 1) +
                                     (spr8[dp[4]]) +
                                     (spr8[dp[5]] >> 1) +
                                     (spr2[dp[6]]) +
                                     (spr2[dp[7]] >> 1);
                                   odp[0] = (byte)(pword >> 16);
                                   odp[line_size] = (byte)(pword >> 8);
                                   odp[line_size*2] = (byte)(pword);
                                 }
                                /* Skip blank lines if any */
                                if ( num_blank_lines > 0 )
                                   {	/* move down from current position */
                                        gp_fprintf(prn_stream, "\033&a+%dV",
                                                   num_blank_lines * (720 / Y_DPI));
                                        num_blank_lines = 0;
                                   }

                                /* Transfer raster graphics */
                                /* in the order R, G, B. */
                                for ( row = plane_data + line_size * 2, i = 0;
                                      i < 3; row -= line_size, i++
                                    )
                                   {
                                        int count = compress1_row(row, row + line_size, temp);
                                        gp_fprintf(prn_stream, "\033*b%d%c",
                                                   count, "VVW"[i]);
                                        gp_fwrite(temp, sizeof(byte),
                                                  count, prn_stream);
                                   }
                           }
                   }
           }

        /* end the page */
        gp_fputs(end_page, prn_stream);

xit:
        gs_free(pdev->memory, (char *)data, data_size, 1, "paintjet_print_page(data)");
        gs_free(pdev->memory, (char *)plane_data, line_size * 3, 1, "paintjet_print_page(plane_data)");
        gs_free(pdev->memory, temp, line_size * 2, 1, "paintjet_print_page(temp)");

        return code;
}

/*
 * Row compression for the H-P PaintJet.
 * Compresses data from row up to end_row, storing the result
 * starting at compressed.  Returns the number of bytes stored.
 * The compressed format consists of a byte N followed by a
 * data byte that is to be repeated N+1 times.
 * In the worst case, the `compressed' representation is
 * twice as large as the input.
 * We complement the bytes at the same time, because
 * we accumulated the image in complemented form.
 */
static int
compress1_row(const byte *row, const byte *end_row,
  byte *compressed)
{	register const byte *in = row;
        register byte *out = compressed;
        while ( in < end_row )
           {	byte test = *in++;
                const byte *run = in;
                while ( in < end_row && *in == test ) in++;
                /* Note that in - run + 1 is the repetition count. */
                while ( in - run > 255 )
                   {	*out++ = 255;
                        *out++ = ~test;
                        run += 256;
                   }
                *out++ = in - run;
                *out++ = ~test;
           }
        return out - compressed;
}
