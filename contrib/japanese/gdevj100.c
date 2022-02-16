/* Copyright (C) 1990, 1992 Aladdin Enterprises.  All rights reserved.
   Distributed by Free Software Foundation, Inc.

This file is part of Ghostscript.

Ghostscript is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
to anyone for the consequences of using it or for whether it serves any
particular purpose or works at all, unless he says so in writing.  Refer
to the Ghostscript General Public License for full details.

Everyone is granted permission to copy, modify and redistribute
Ghostscript, but only under the conditions described in the Ghostscript
General Public License.  A copy of this license is supposed to have been
given to you along with Ghostscript so you can know your rights and
responsibilities.  It should be in a file named COPYING.  Among other
things, the copyright notice and this notice must be preserved on all
copies.  */

/* gdevjj100.c */
/* Star JJ-100 printer driver for Ghostscript */
#include "gdevprn.h"

static dev_proc_print_page(jj100_print_page);

/*
 * The only available resolutions are 360 x 360.
 */

/* The device descriptor */
gx_device_printer gs_jj100_device =
  prn_device(gdev_prn_initialize_device_procs_mono, "jj100",
        82,                  /* width_10ths, 8.2" = 210mm(A4) */
        115 /*113, 117*/,    /* height_10ths, 11.7" = 297mm(A4) */
        360,			/* x_dpi */
        360,			/* y_dpi */
        0,0,0,0,		/* margins */
        1, jj100_print_page);

/* ------ internal routines ------ */

/* Transpose a block of 8x8 bits */
static int
jj100_transpose_8x8(byte *src, int src_step, byte *dst, int dst_step)
{
        byte mask, s, d0, d1, d2, d3, d4, d5, d6, d7;
        int i;

        d0 = d1 = d2 = d3 = d4 = d5 = d6 = d7 = 0;

        for(i=0, mask=0x01; i<8; i++, mask <<= 1) {
                s = *src;
                if(s & 0x80) d0 |= mask;
                if(s & 0x40) d1 |= mask;
                if(s & 0x20) d2 |= mask;
                if(s & 0x10) d3 |= mask;
                if(s & 0x08) d4 |= mask;
                if(s & 0x04) d5 |= mask;
                if(s & 0x02) d6 |= mask;
                if(s & 0x01) d7 |= mask;
                src += src_step;
        }

        *dst = d0;
        *(dst += dst_step) = d1;
        *(dst += dst_step) = d2;
        *(dst += dst_step) = d3;
        *(dst += dst_step) = d4;
        *(dst += dst_step) = d5;
        *(dst += dst_step) = d6;
        *(dst += dst_step) = d7;

        return 0;
}

/* Send the page to the printer. */
static int
jj100_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{	int line_size = gdev_prn_raster(pdev);
        int height = pdev->height;
        int bits_per_column = 48;
        int bytes_per_column = bits_per_column / 8;
        int chunk_size = bits_per_column * line_size;
        byte *in, *out;
        int lnum, skip;
        char prn_buf[16];

        in = (byte *)gs_malloc(pdev->memory->non_gc_memory, bits_per_column, line_size, "jj100_print_page(in)");
        out = (byte *)gs_malloc(pdev->memory->non_gc_memory, bits_per_column, line_size, "jj100_print_page(out)");
        if(in == 0 || out == 0)
                return -1;

        /* Initialize printer */
        gp_fputs("\033P", pdev->file);	/* Proportional Mode */
        gp_fputs("\033G", pdev->file);        /* 1/180 inch per line */
        gp_fputs("\033T16", pdev->file);	/* 16/180 inch per line */

        /* Send Data to printer */
        lnum = 0;
        skip = 0;
        while(lnum < height) {
                byte *inp, *outp, *out_beg, *out_end;
                int x, y, num_lines, size, mod;

                /* Copy scan lines */
                if(gdev_prn_copy_scan_lines(pdev, lnum, in, chunk_size) < 0)
                        break;

                /* The number of lines to process */
                if((num_lines = height - lnum) > bits_per_column)
                        num_lines = bits_per_column;

                /* Test for all zero */
                size = line_size * num_lines;
                if(in[0] == 0 &&
                   !memcmp((char *)in, (char *)in + 1, size - 1)) {
                        lnum += bits_per_column;
                        skip ++;
                        continue;
                }

                /* Fill zero */
                if(num_lines < bits_per_column) {
                        size = line_size * (bits_per_column - num_lines);
                        memset(in + line_size * num_lines, 0, size);
                }
                lnum += bits_per_column;

                /* Vertical tab to the appropriate position. */
                while(skip > 15) {
                        gs_snprintf(prn_buf, sizeof(prn_buf), "\037%c", 16 + 15);
                        gp_fputs(prn_buf, pdev->file);
                        skip -= 15;
                }
                if(skip > 0) {
                        gs_snprintf(prn_buf, sizeof(prn_buf), "\037%c", 16 + skip);
                        gp_fputs(prn_buf, pdev->file);
                }

                /* Transpose in blocks of 8 scan lines. */
                for(y = 0; y < bytes_per_column; y ++) {
                        inp = in + line_size * 8 * y;
                        outp = out + y;
                        for(x = 0; x < line_size; x ++) {
                                jj100_transpose_8x8(inp, line_size,
                                                    outp, bytes_per_column);
                                inp ++;
                                outp += bits_per_column;
                        }
                }

                /* Remove trailing 0s. */
                out_end = out + chunk_size - 1;
                while(out_end >= out) {
                        if(*out_end)
                                break;
                        out_end --;
                }
                size = (out_end - out) + 1;
                if((mod = size % bytes_per_column) != 0)
                        out_end += bytes_per_column - mod;

                /* Remove leading 0s. */
                out_beg = out;
                while(out_beg <= out_end) {
                        if(*out_beg)
                                break;
                        out_beg ++;
                }
                out_beg -= (out_beg - out) % (bytes_per_column * 2);

                /* Dot addressing */
                gs_snprintf(prn_buf, sizeof(prn_buf), "\033F%04d",
                        (out_beg - out) / bytes_per_column / 2);
                gp_fputs(prn_buf, pdev->file);

                /* Dot graphics */
                size = out_end - out_beg + 1;
                gs_snprintf(prn_buf, sizeof(prn_buf), "\034bP,48,%04d.", size / bytes_per_column);
                gp_fputs(prn_buf, pdev->file);
                gp_fwrite(out_beg, 1, size, pdev->file);

                /* Carriage Return */
                gp_fputc('\r', pdev->file);
                skip = 1;
        }

        /* Form Feed */
        gp_fputc('\f', pdev->file);
        gp_fflush(pdev->file);

        gs_free(pdev->memory->non_gc_memory, (char *)out,
                bits_per_column, line_size, "jj100_print_page(out)");
        gs_free(pdev->memory->non_gc_memory, (char *)in,
                bits_per_column, line_size, "jj100_print_page(in)");

        return 0;
}
