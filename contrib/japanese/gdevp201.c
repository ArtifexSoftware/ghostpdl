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

/* gdevp201.c */
/* NEC PC-PR201 printer driver for Ghostscript */
/* modified by m.mori for printers other than pr-201 */
/* modified by OkI for hires-98 */
/* modified by ASAYAMA Kazunori for Ghostscript version 2.6.1 */
#include "gdevprn.h"

/* There are NEC PC-PR printer paramaters.
        MODEL		DEVICE_NAME		X_DPI,Y_DPI		HEAD_PINS	LF_PITCH
        PC-PR201	"nec160"			160				24			18
        PC-PR1000	"nec240"			240				40			20
        PC-PR150	"nec320"			320				48			18
        PC-PR1000/4	"nec400"			400				60			18
*/

#define WIDTH 80				/* width_10ths, 8" */
#define HEIGHT 110				/* height_10ths, 11" */

enum{PR201, PR1000, PR150, PR1K4};

static dev_proc_print_page(pr201_print_page);

/* The device descriptor */
gx_device_printer gs_pr201_device =
  prn_device(prn_std_procs, "pr201",
        WIDTH,
        HEIGHT,
        160,
        160,
        0,0,0,0,		/* margins */
        1, pr201_print_page);

gx_device_printer gs_pr1000_device =
  prn_device(prn_std_procs, "pr1000",
        WIDTH,
        HEIGHT,
        240,
        240,
        0,0,0,0,		/* margins */
        1, pr201_print_page);

gx_device_printer gs_pr150_device =
  prn_device(prn_std_procs, "pr150",
        WIDTH,
        HEIGHT,
        320,
        320,
        0,0,0,0,		/* margins */
        1, pr201_print_page);

gx_device_printer gs_pr1000_4_device =
  prn_device(prn_std_procs, "pr1000_4",
        WIDTH,
        HEIGHT,
        400,
        400,
        0,0,0,0,		/* margins */
        1, pr201_print_page);

/* Transpose a block of 8x8 bits */
static int
pr201_transpose_8x8(byte *src, int src_step, byte *dst, int dst_step)
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

static int
check_mode(const char* modename)
{
        if (!strcmp(modename, "pr201"))
                return PR201;
        else if (!strcmp(modename, "pr1000"))
                return PR1000;
        else if (!strcmp(modename, "pr150"))
                return PR150;
        else
                return PR1K4;
}

/* Send the page to the printer. */
static int
pr201_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{	int line_size;
        int height;
        int bits_per_column;
        int bytes_per_column;
        int chunk_size;
        byte *in, *out;
        int lnum, skip;
        int head_pins, lr_pitch, x_dpi;
        int code = 0;
        byte mask;
        int endidx = pdev->width>>3;

        switch (check_mode(pdev->dname)){
                case PR201:
                        head_pins=24; lr_pitch=18; x_dpi=160; break;
                case PR1000:
                        head_pins=40; lr_pitch=20; x_dpi=240; break;
                case PR150:
                        head_pins=48; lr_pitch=18; x_dpi=320; break;
                case PR1K4:
                default:
                        head_pins=60; lr_pitch=18; x_dpi=400; break;
        }

        line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        height = pdev->height;
        bits_per_column	 = head_pins;
        bytes_per_column = bits_per_column / 8;
        chunk_size = bits_per_column * line_size;

        in = (byte *)
                gs_malloc(pdev->memory->non_gc_memory, bits_per_column, line_size, "pr201_print_page(in)");
        out = (byte *)
                gs_malloc(pdev->memory->non_gc_memory, bits_per_column, line_size, "pr201_print_page(out)");
        if(in == 0 || out == 0)
                return -1;

        if (pdev->width & 7)
            mask = ~(255>>(pdev->width & 7));
        else
            mask = 255, endidx--;

        /* Initialize printer */
        gp_fputs("\033cl", pdev->file);	/* Software Reset */
        gp_fputs("\033P", pdev->file);	/* Proportional Mode */
        if (check_mode(pdev->dname)==PR150){
                gp_fprintf(pdev->file, "\034d%d.", x_dpi); /* 320 dpi mode. */
        }
        gp_fprintf(pdev->file, "\033T%d" , lr_pitch);
                                /* 18/120 inch per line */

        /* Send Data to printer */
        lnum = 0;
        skip = 0;
        while(lnum < height) {
                byte *inp, *outp, *out_beg, *out_end, *p;
                int x, y, num_lines, size, mod, i;

                /* The number of lines to process */
                if((num_lines = height - lnum) > bits_per_column)
                        num_lines = bits_per_column;

                /* Copy scan lines */
                for (i = 0, p = in; i < num_lines; i++, p += line_size) {
                    code = gdev_prn_get_bits(pdev, lnum + i, p, NULL);
                    if (code < 0)
                        goto error;
                    p[endidx] &= mask;
                }

                /* Ensure we have a full stripe of line data */
                for (; i < bits_per_column; i++, p += line_size) {
                    memset(p, 0, line_size);
                }

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
                while(skip > 72) {
                        gp_fprintf(pdev->file, "\037%c", 16 + 72);
                        skip -= 72;
                }
                if(skip > 0) {
                        gp_fprintf(pdev->file, "\037%c", 16 + skip);
                }

                /* Transpose in blocks of 8 scan lines. */
                for(y = 0; y < bytes_per_column; y ++) {
                        inp = in + line_size * 8 * y;
                        outp = out + y;
                        for(x = 0; x < line_size; x ++) {
                                pr201_transpose_8x8(inp, line_size,
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
                out_beg -= (out_beg - out) % bytes_per_column;

                /* Dot addressing */
                gp_fprintf(pdev->file, "\033F%04" PRIdSIZE,
                           (out_beg - out) / bytes_per_column);

                /* Dot graphics */
                size = out_end - out_beg + 1;
                if (check_mode(pdev->dname)==PR201){
                        gp_fprintf(pdev->file,"\033J%04d", size / bytes_per_column);
                }else{
                        gp_fprintf(pdev->file,"\034bP,48,%04d.",
                                   size / bytes_per_column);
                }
                gp_fwrite(out_beg, size, 1, pdev->file);

                /* Carriage Return */
                gp_fputc('\r', pdev->file);
                skip = 1;
        }

        /* Form Feed */
        gp_fputc('\f',pdev->file);
        gp_fflush(pdev->file);

error:
        gs_free(pdev->memory->non_gc_memory, (char *)out,
                bits_per_column, line_size, "pr201_print_page(out)");
        gs_free(pdev->memory->non_gc_memory, (char *)in,
                bits_per_column, line_size, "pr201_print_page(in)");

        return code;
}
