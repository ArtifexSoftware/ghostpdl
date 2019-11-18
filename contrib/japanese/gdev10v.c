/* Copyright (C) 1990, 1992, 1993 Aladdin Enterprises.  All rights reserved.
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

/* gdev10v.c */
/* Canon Bubble Jet BJ-10v printer driver for Ghostscript.

   Based on BJ-10e driver (gdevbj10.c) and Epson driver (gdevepsn.c).
   Modified for BJ-10v by MAEDA Atusi (maeda@is.uec.ac.jp) and
   IWAI Teruo (oteru@nak.ics.keio.ac.jp).  PC9801 support by MATUI
   Takao (mat@nuis.ac.jp).
 */

/* Fixed for GNU Ghostscript 5.10
        by Norihito Ohmori (ohmori@p.chiba-u.ac.jp)
 */

#include "gdevprn.h"

/*
   The only available resolutions are 360x360, 360x180, and 180x180.
   No checking on resolutions are being made.
 */

#if 0
#define prn_matrix_procs(p_open, p_get_initial_matrix, p_output_page, p_close) {\
        p_open,\
        p_get_initial_matrix,\
        NULL,	/* sync_output */\
        p_output_page,\
        p_close,\
        gdev_prn_map_rgb_color,\
        gdev_prn_map_color_rgb,\
        NULL,	/* fill_rectangle */\
        NULL,	/* tile_rectangle */\
        NULL,	/* copy_mono */\
        NULL,	/* copy_color */\
        NULL,	/* draw_line */\
        NULL,	/* get_bits */\
        gdev_prn_get_params,\
        gdev_prn_put_params,\
        NULL,	/* map_cmyk_color */\
        NULL,	/* get_xfont_procs */\
        NULL,	/* get_xfont_device */\
        NULL,	/* map_rgb_alpha_color */\
        gx_page_device_get_page_device\
}
#endif

/* The device descriptor */
static dev_proc_print_page(bj10v_print_page);
#if 0
static dev_proc_get_initial_matrix(bj10v_get_initial_matrix);
#endif

static int
bj10v_open(gx_device * pdev)
{
    if (pdev->HWResolution[0] < 180 ||
        pdev->HWResolution[1] < 180)
    {
        emprintf(pdev->memory, "device requires a resolution of at least 180dpi\n");
        return_error(gs_error_rangecheck);
    }
    return gdev_prn_open(pdev);
}


#if 0
gx_device_procs prn_bj10v_procs =
  prn_matrix_procs(gdev_prn_open, bj10v_get_initial_matrix,
    gdev_prn_output_page, gdev_prn_close);
#endif
gx_device_procs prn_bj10v_procs =
  prn_procs(bj10v_open, gdev_prn_output_page, gdev_prn_close);

gx_device_printer gs_bj10v_device =
  prn_device(prn_bj10v_procs, "bj10v",
        DEFAULT_WIDTH_10THS,		/* width_10ths */
        DEFAULT_HEIGHT_10THS,	/* height_10ths */
        360,				/* x_dpi */
        360,				/* y_dpi */
        0.134, 0.507, 0.166, 0.867,	/* l, b, r, t margins */
        1, bj10v_print_page);

gx_device_printer gs_bj10vh_device =
  prn_device(prn_bj10v_procs, "bj10vh",
        DEFAULT_WIDTH_10THS,		/* width_10ths */
        DEFAULT_HEIGHT_10THS,  	/* height_10ths */
        360,				/* x_dpi */
        360,				/* y_dpi */
        0.134, 0.507, 0.166, 0.335,	/* l, b, r, t margins */
        1, bj10v_print_page);

/* ------ Internal routines ------ */

#if 0
/* Shift the origin from the top left corner of the pysical page to the
   first printable pixel, as defined by the top and left margins. */
static void
bj10v_get_initial_matrix(gx_device *dev, gs_matrix *pmat)
{	gx_default_get_initial_matrix(dev, pmat);
        pmat->tx -= dev_l_margin(dev) * dev->x_pixels_per_inch;
        pmat->ty -= dev_t_margin(dev) * dev->y_pixels_per_inch;
}
#endif

/* ---- Printer output routines ---- */

/* Note: Following code is stolen from gdevp201.c.  On NEC PC9801 series,
         which is very popular PC in Japan, DOS printer driver strips off
         some control characters.  So we must bypass the driver and put
         data directly to get correct results.  */

#ifdef PC9801
static int
is_printer(gx_device_printer *pdev)
{
        return (strlen(pdev->fname) == 0 || strcmp(pdev->fname, "PRN") == 0);
}

static void
pc98_prn_out(int c)
{
        while(!(inportb(0x42) & 0x04));
        outportb(0x40, c);
        outportb(0x46, 0x0e);
        outportb(0x46, 0x0f);
}

static int
prn_putc(gx_device_printer *pdev, int c)
{
        if(is_printer(pdev)) {
                pc98_prn_out(c);
                return 0;
        }
        return gp_fputc(c, pdev->file);
}

static int
prn_puts(gx_device_printer *pdev, char *ptr)
{
        if(is_printer(pdev)) {
                while(*ptr)
                        pc98_prn_out(*ptr ++);
                return 0;
        }
        return gp_fputs(ptr, pdev->file);
}

static int
prn_write(gx_device_printer *pdev, char *ptr, int size)
{
        if(is_printer(pdev)) {
                while(size --)
                        pc98_prn_out(*ptr ++);
                return size;
        }
        return fwrite(ptr, 1, size, pdev->file);
}

static int
prn_flush(gx_device_printer *pdev)
{
        if(is_printer(pdev))
                return 0;
        return fflush(pdev->file);
}

#else /* PC9801 */

#define prn_putc(pdev, c) gp_fputc(c, pdev->file)
#define prn_puts(pdev, ptr) gp_fputs(ptr, pdev->file)
#define prn_write(pdev, ptr, size) gp_fwrite(ptr, 1, size, pdev->file)
#define prn_flush(pdev) gp_fflush(pdev->file)

#endif

/* ------ internal routines ------ */

static void
bj10v_output_run(byte *data, int dnum, int bytes,
                 const char *mode, gx_device_printer *pdev)
{
        prn_putc(pdev, '\033');
        prn_puts(pdev, mode);
        prn_putc(pdev, dnum & 0xff);
        prn_putc(pdev, dnum >> 8);
        prn_write(pdev, data, bytes);
}

/* Send the page to the printer. */
static int
bj10v_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{	int line_size = gdev_prn_raster((gx_device *)pdev);
        int xres = (int)pdev->x_pixels_per_inch;
        int yres = (int)pdev->y_pixels_per_inch;
        const char *mode = (yres == 180 ?
                      (xres == 180 ? "\052\047" : "\052\050") :
                      "|*");
        int bits_per_column = 24 * (yres / 180);
        int bytes_per_column = bits_per_column / 8;
        int x_skip_unit = bytes_per_column * (xres / 180);
        int y_skip_unit = (yres / 180);
        byte *in = (byte *)gs_malloc(pdev->memory->non_gc_memory, 8, line_size, "bj10v_print_page(in)");
        /* We need one extra byte in <out> for our sentinel. */
        byte *out = (byte *)gs_malloc(pdev->memory->non_gc_memory, bits_per_column * line_size + 1, 1, "bj10v_print_page(out)");
        int lnum = 0;
        int y_skip = 0;
        int code = 0;
        int blank_lines = 0;
        int bytes_per_data = ((xres == 360) && (yres == 360)) ? 1 : 3;

        if ( in == 0 || out == 0 )
                return -1;

        /* Initialize the printer. */
        prn_puts(pdev, "\033@");
        /* Transfer pixels to printer */
        while ( lnum < pdev->height )
           {	byte *out_beg;
                byte *out_end;
                byte *outl, *outp;
                int count, bnum;

                /* Copy 1 scan line and test for all zero. */
                code = gdev_prn_get_bits(pdev, lnum + blank_lines, in, NULL);
                if ( code < 0 ) goto xit;
                /* The mem... or str... functions should be faster than */
                /* the following code, but all systems seem to implement */
                /* them so badly that this code is faster. */
                {	register long *zip = (long *)in;
                        register int zcnt = line_size;
                        static const long zeroes[4] = { 0, 0, 0, 0 };
                        for ( ; zcnt >= 4 * sizeof(long); zip += 4, zcnt -= 4 * sizeof(long) )
                            {	if ( zip[0] | zip[1] | zip[2] | zip[3] )
                                    goto notz;
                            }
                        if ( !memcmp(in, (const char *)zeroes, zcnt) )
                            {	/* Line is all zero, skip */
                                if (++blank_lines >= y_skip_unit) {
                                    lnum += y_skip_unit;
                                    y_skip++;
                                    blank_lines = 0;
                                }
                                continue;
                            }
                }
notz:
                blank_lines = 0;
                out_end = out + bytes_per_column * pdev->width;
                /* Vertical tab to the appropriate position. */
                while ( y_skip > 255 )
                   {	prn_puts(pdev, "\033J\377");
                        y_skip -= 255;
                   }
                if ( y_skip ) {
                        prn_puts(pdev, "\033J");
                        prn_putc(pdev, y_skip);
                }

                /* Transpose in blocks of 8 scan lines. */
                for ( bnum = 0, outl = out; bnum < bits_per_column; bnum += 8, lnum += 8 )
                   {	int lcnt = gdev_prn_copy_scan_lines(pdev,
                                lnum, in, 8 * line_size);
                        byte *inp = in;
                        if ( lcnt < 0 )
                           {	code = lcnt;
                                goto xit;
                           }
                        if ( lcnt < 8 )
                                memset(in + lcnt * line_size, 0,
                                       (8 - lcnt) * line_size);
                        for (outp = outl ; inp < in + line_size; inp++, outp += bits_per_column )
                           {	memflip8x8(inp, line_size,
                                        outp, bytes_per_column);
                           }
                        outl++;
                   }

                /* Remove trailing 0s. */
                /* Note that non zero byte always exists. */
                outl = out_end;
                while ( *--outl == 0 )
                    ;
                count = ((out_end - (outl + 1)) / bytes_per_column) * bytes_per_column;
                out_end -= count;
                *out_end = 1;		/* sentinel */

                for ( out_beg = outp = out; outp < out_end; )
                    { /* Skip a run of leading 0s. */
                        /* At least 10 bytes are needed to make tabbing worth it. */
                        byte *zp = outp;
                        int x_skip;
                        while ( *outp == 0 )
                                outp++;
                        x_skip = ((outp - zp) / x_skip_unit) * x_skip_unit;
                        outp = zp + x_skip;
                        if (x_skip >= 10) {
                            /* Output preceding bit data. */
                            int bytes = zp - out_beg;
                            if (bytes > 0)
                                        /* Only false at beginning of line. */
                                        bj10v_output_run(out_beg, bytes / bytes_per_data, bytes,
                                                                         mode, pdev);
                            /* Tab over to the appropriate position. */
                            {	int skip = x_skip / x_skip_unit;
                                        prn_puts(pdev, "\033\\");
                                        prn_putc(pdev, skip & 0xff);
                                        prn_putc(pdev, skip >> 8);
                            }
                            out_beg = outp;
                        } else
                            outp += x_skip_unit;
                    }
                if (out_end > out_beg) {
                    int bytes = out_end - out_beg;
                    bj10v_output_run(out_beg, bytes / bytes_per_data, bytes,
                                     mode, pdev);
                }
                prn_putc(pdev, '\r');
                y_skip = 24;
           }

        /* Eject the page */
xit:	prn_putc(pdev, 014);	/* form feed */
        prn_flush(pdev);
        gs_free(pdev->memory->non_gc_memory, (char *)out, bits_per_column, line_size, "bj10v_print_page(out)");
        gs_free(pdev->memory->non_gc_memory, (char *)in, 8, line_size, "bj10v_print_page(in)");
        return code;
}
