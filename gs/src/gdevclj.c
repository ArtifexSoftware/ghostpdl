/* Copyright (C) 1991, 1992 Aladdin Enterprises.  All rights reserved.

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

/* gdevclj.c */
/* HP Color LaserJet driver.  Adapted from gdevpjet.c. */
#include "gdevprn.h"
#include "gdevpcl.h"

/* X_DPI and Y_DPI must be the same, and must divide 300 evenly. */
#define X_DPI 300
#define Y_DPI 300

/* The device descriptors */
private dev_proc_print_page(clj_print_page);
private gx_device_procs clj_procs =
prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		gdev_pcl_3bit_map_rgb_color, gdev_pcl_3bit_map_color_rgb);
gx_device_printer gs_cljet_device =
prn_device(clj_procs, "cljet",
	   85,			/* width_10ths, 8.5" */
	   110,			/* height_10ths, 11" */
	   X_DPI, Y_DPI,
	   0.25, 0, 0.25, 0,	/* margins */
	   3, clj_print_page);

/* Forward references */
private int compress1_row(P3(const byte *, const byte *, byte *));

/* ------ Internal routines ------ */

/* Send the page to the printer.  Compress each scan line. */
private int
clj_print_page(gx_device_printer * pdev, FILE * prn_stream)
{
    int line_size =
	(((gdev_mem_bytes_per_scan_line((gx_device *)pdev)) + 7) / 8) * 8;
    int data_size = line_size * 8;
    byte *data =
    (byte *) gs_malloc(data_size, 1,
		       "cljet_print_page(data)");
    byte *plane_data =
    (byte *) gs_malloc(line_size * 3, 1,
		       "cljet_print_page(plane_data)");

    if (data == 0 || plane_data == 0) {
	if (data)
	    gs_free((char *)data, data_size, 1,
		    "cljet_print_page(data)");
	if (plane_data)
	    gs_free((char *)plane_data, line_size * 3, 1,
		    "cljet_print_page(plane_data)");
	return_error(gs_error_VMerror);
    }

    /* we really only need to reset the alignment padding bytes at the
       each scanline, for simplicity I set everything here */
    memset(data, 0, data_size);
    /* init, set raster graphics resolution */
    fprintf(prn_stream, "\033E\033*t%dR", (int)pdev->x_pixels_per_inch);
    /* set the units as well positioning should only affect vertical
       movement */
    fprintf(prn_stream, "\033&u%dD", (int)pdev->y_pixels_per_inch);

    /* set the number of color planes */
    fprintf(prn_stream, "\033*r%dU", 3);	/* always 3 */

    /* move to top left of page */
    fprintf(prn_stream, "\033&a0H\033&a-360V");

    /* select data compression */
    fputs("\033*b1M", prn_stream);

    /* start raster graphics */
    fputs("\033*r1A", prn_stream);

    /* Send each scan line in turn */
    {
	int lnum;
	int num_blank_lines = 0;

	for (lnum = 0; lnum < pdev->height; lnum++) {
	    byte *end_data = data + line_size;

	    gdev_prn_copy_scan_lines(pdev, lnum,
				     (byte *) data, line_size);
	    /* Remove trailing 0s. */

	    while (end_data > data && end_data[-1] == 0)
		end_data--;
	    if (end_data == data) {	/* Blank line */
		num_blank_lines++;
	    } else {
		int i;
		byte *odp;
		byte *row;

		/* Transpose the data to get pixel planes. */
		for (i = 0, odp = plane_data; i < data_size;
		     i += 8, odp++
		    ) {		/* The following is for 16-bit machines */
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

		    odp[0] = (byte) (pword >> 16);
		    odp[line_size] = (byte) (pword >> 8);
		    odp[line_size * 2] = (byte) (pword);
		}

		if ( num_blank_lines > 0 ) {
		    /* move down from current position */
		    fprintf(prn_stream, "\033*p+%dY", num_blank_lines);
		    num_blank_lines = 0;
		}

		/* Transfer raster graphics */
		/* in the order R, G, B. */

		{
		    byte *temp = (byte *) gs_malloc(line_size * 2, 1,
						    "cljet_print_page(plane_data)");
		    for (row = plane_data + line_size * 2, i = 0;
			 i < 3; row -= line_size, i++
			 ) {
			int count = compress1_row(row, row + line_size, temp);
			fprintf(prn_stream, "\033*b%d%c", count, "VVW"[i]);
			fwrite(temp, sizeof(byte), count, prn_stream);
		    }
		    gs_free(temp, line_size * 2, 1, "cljet_print_page(data)");
		}
	    }
	}
    }

    /* end the page */
    fputs("\033*rC\014", prn_stream);

    gs_free((char *)data, data_size, 1, "cljet_print_page(data)");
    gs_free((char *)plane_data, line_size * 3, 1, "cljet_print_page(plane_data)");

    return 0;
}

/*
 * Row compression for Color LaserJet
 * Compresses data from row up to end_row, storing the result
 * starting at compressed.  Returns the number of bytes stored.
 * The compressed format consists of a byte N followed by a
 * data byte that is to be repeated N+1 times.
 * In the worst case, the `compressed' representation is
 * twice as large as the input.
 * We complement the bytes at the same time, because
 * we accumulated the image in complemented form.
 */
private int
compress1_row(const byte * row, const byte * end_row,
	      byte * compressed)
{
    register const byte *in = row;
    register byte *out = compressed;

    while (in < end_row) {
	byte test = *in++;
	const byte *run = in;

	while (in < end_row && *in == test)
	    in++;
	/* Note that in - run + 1 is the repetition count. */
	while (in - run > 255) {
	    *out++ = 255;
	    *out++ = ~test;
	    run += 256;
	}
	*out++ = in - run;
	*out++ = ~test;
    }
    return out - compressed;
}
