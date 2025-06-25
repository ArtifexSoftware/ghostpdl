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


/*
 * Epson (and similar) dot-matrix printer driver for Ghostscript.
 *
 * Four devices are defined here: 'epson', 'eps9mid', 'eps9high', and 'ibmpro'.
 * The 'epson' device is the generic device, for 9-pin and 24-pin printers.
 * 'eps9high' is a special mode for 9-pin printers where scan lines are
 * interleaved in multiple passes to produce high vertical resolution at
 * the expense of several passes of the print head.  'eps9mid' is a special
 * mode for 9 pin printers too, scan lines are interleaved but with next
 * vertical line. 'ibmpro' is for the IBM ProPrinter, which has slightly
 * (but only slightly) different control codes.
 *
 * Thanks to:
 *	David Wexelblat (dwex@mtgzfs3.att.com) for the 'eps9high' code;
 *	Guenther Thomsen (thomsen@cs.tu-berlin.de) for the 'eps9mid' code;
 *	James W. Birdsall (jwbirdsa@picarefy.picarefy.com) for the
 *	  'ibmpro' modifications;
 *	Russell J. Lang (rjl@aladdin.com) for the 180x60 and 240x180 dpi
 *	  enhancements.
 */
#include "gdevprn.h"

/*
 * Define whether the printer is archaic -- so old that it doesn't
 * support settable tabs, pitch, or left margin.  (This should be a
 * run-time property....)  Note: the IBM ProPrinter is archaic.
 */
/*#define ARCHAIC 1*/

/*
 * Define whether the printer is a Panasonic 9-pin printer,
 * which sometimes doesn't recognize a horizontal tab command
 * when a line contains a lot of graphics commands,
 * requiring a "backspace, space" sequence before a tab.
 */
/*#define TAB_HICCUP 1*/

/*
 * Define the minimum distance for which it's worth converting white space
 * into a tab.  This can be specified in pixels (to save transmission time),
 * in tenths of an inch (for printers where tabs provoke actual head motion),
 * or both.  The distance must meet BOTH criteria for the driver to tab,
 * so an irrelevant criterion should be set to 0 rather than infinite.
 */
#define MIN_TAB_PIXELS 10
#define MIN_TAB_10THS 15

/*
 * Valid values for X_DPI:
 *
 *    For 9-pin printers: 60, 120, 240
 *    For 24-pin printers: 60, 120, 180, 240, 360
 *
 * The value specified at compile time is the default value used if the
 * user does not specify a resolution at runtime.
 */
#ifndef X_DPI
#  define X_DPI 240
#endif

/*
 * For Y_DPI, a given printer will support a base resolution of 60 or 72;
 * check the printer manual.  The Y_DPI value must be a multiple of this
 * base resolution.  Valid values for Y_DPI:
 *
 *    For 9-pin printers: 1*base_res
 *    For 24-pin printers: 1*base_res, 3*base_res
 *
 * The value specified at compile time is the default value used if the
 * user does not specify a resolution at runtime.
 */

#ifndef Y_BASERES
#  define Y_BASERES 72
#endif
#ifndef Y_DPI
#  define Y_DPI (1*Y_BASERES)
#endif

/* The device descriptors */
static dev_proc_print_page(epson_print_page);
static dev_proc_print_page(eps9mid_print_page);
static dev_proc_print_page(eps9high_print_page);
static dev_proc_print_page(ibmpro_print_page);

/* Standard Epson device */
const gx_device_printer far_data gs_epson_device =
  /* The print_page proc is compatible with allowing bg printing */
  prn_device(gdev_prn_initialize_device_procs_mono_bg,
             "epson",
             DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
             X_DPI, Y_DPI,
             0.25, 0.02, 0.25, 0.4, /* margins */
             1, epson_print_page);

/* Mid-res (interleaved, 1 pass per line) 9-pin device */
const gx_device_printer far_data gs_eps9mid_device =
  /* The print_page proc is compatible with allowing bg printing */
  prn_device(gdev_prn_initialize_device_procs_mono_bg,
             "eps9mid",
             DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
             X_DPI, 3*Y_BASERES,
             0.2, 0.0, 0, 0.0, /* margins */
             1, eps9mid_print_page);

/* High-res (interleaved) 9-pin device */
const gx_device_printer far_data gs_eps9high_device =
  /* The print_page proc is compatible with allowing bg printing */
  prn_device(gdev_prn_initialize_device_procs_mono_bg,
             "eps9high",
             DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
             X_DPI, 3*Y_BASERES,
             0.2, 0.0, 0.0, 0.0, /* margins */
             1, eps9high_print_page);

/* IBM ProPrinter device */
const gx_device_printer far_data gs_ibmpro_device =
  /* The print_page proc is compatible with allowing bg printing */
  prn_device(gdev_prn_initialize_device_procs_mono_bg,
             "ibmpro",
             DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
             X_DPI, Y_DPI,
             0.2, 0.0, 0.0, 0.0, /* margins */
             1, ibmpro_print_page);

/* ------ Driver procedures ------ */

/* Forward references */
static void eps_output_run(byte *, int, int, char, gp_file *, int);

/* Send the page to the printer. */
#define DD 0x40				/* double density flag */
static int
eps_print_page(gx_device_printer *pdev, gp_file *prn_stream, int y_9pin_high,
  const char *init_string, int init_length, const char *end_string,
  int archaic, int tab_hiccup)
{
        static const char graphics_modes_9[5] =
        {
        -1, 0 /*60*/, 1	/*120*/, 7 /*180*/, DD+3 /*240*/
        };

        static const char graphics_modes_24[7] =
        {
        -1, 32 /*60*/, 33 /*120*/, 39 /*180*/,
        DD+35 /*240*/, -1, DD+40 /*360*/
        };

        int y_24pin = (y_9pin_high ? 0 : pdev->y_pixels_per_inch > 72);
        int in_y_mult = ((y_24pin | y_9pin_high) ? 3 : 1);
        int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        /* Note that in_size is a multiple of 8. */
        int in_size = line_size * (8 * in_y_mult);
        byte *buf1 = NULL;
        byte *buf2 = NULL;
        byte *in;
        byte *out;
        int out_y_mult = (y_24pin ? 3 : 1);
        int x_dpi = (int)pdev->x_pixels_per_inch;
        int y_passes = (y_9pin_high ? 3 : 1);
        int dots_per_space = x_dpi / 10;	/* pica space = 1/10" */
        int bytes_per_space = dots_per_space * out_y_mult;
        int tab_min_pixels = x_dpi * MIN_TAB_10THS / 10;
        int skip = 0, lnum = 0, pass, ypass;
        int code = 0;

        char start_graphics;
        int first_pass;
        int last_pass;

        if (line_size > max_int / (8 * in_y_mult))
            return_error(gs_error_rangecheck);

        if (y_24pin) {
            if (x_dpi / 60 >= sizeof(graphics_modes_24) / sizeof(graphics_modes_24[0])) {
                return_error(gs_error_rangecheck);
            }
            start_graphics = graphics_modes_24[x_dpi / 60];
        }
        else {
            if (x_dpi / 60 >= sizeof(graphics_modes_9) / sizeof(graphics_modes_9[0])) {
                return_error(gs_error_rangecheck);
            }
            start_graphics = graphics_modes_9[x_dpi / 60];
        }
        first_pass = (start_graphics & DD ? 1 : 0);
        last_pass = first_pass * (y_9pin_high == 2 ? 1 : 2);

        if (bytes_per_space == 0) {
            /* This avoids divide by zero later on, bug 701843. */
            return_error(gs_error_rangecheck);
        }

        buf1 = (byte *)gs_malloc(pdev->memory, in_size, 1, "eps_print_page(buf1)");
        buf2 = (byte *)gs_malloc(pdev->memory, in_size, 1, "eps_print_page(buf2)");
        in = buf1;
        out = buf2;


        /* Check allocations */
        if ( buf1 == 0 || buf2 == 0 ) {
            code = gs_error_VMerror;
            goto xit;
        }

        /* Initialize the printer and reset the margins. */
        gp_fwrite(init_string, 1, init_length, prn_stream);
        if ( init_string[init_length - 1] == 'Q' )
                gp_fputc((int)(pdev->width / pdev->x_pixels_per_inch * 10) + 2,
                      prn_stream);

        /* Calculate the minimum tab distance. */
        if ( tab_min_pixels < max(MIN_TAB_PIXELS, 3) )
                tab_min_pixels = max(MIN_TAB_PIXELS, 3);
        tab_min_pixels -= tab_min_pixels % 3;	/* simplify life */

        /* Print lines of graphics */
        while ( lnum < pdev->height )
        {
                byte *in_data;
                byte *inp;
                byte *in_end;
                byte *out_end = NULL;
                byte *out_blk;
                register byte *outp;
                int lcnt;

                /* Copy 1 scan line and test for all zero. */
                code = gdev_prn_get_bits(pdev, lnum, in, &in_data);
                if (code < 0)
                    goto xit;
                if ( in_data[0] == 0 &&
                     !memcmp((char *)in_data, (char *)in_data + 1, line_size - 1)
                   )
                {
                        lnum++;
                        skip += 3 / in_y_mult;
                        continue;
                }

                /* Vertical tab to the appropriate position. */
                while ( skip > 255 )
                {
                        gp_fputs("\033J\377", prn_stream);
                        skip -= 255;
                }
                if ( skip )
                {
                        gp_fprintf(prn_stream, "\033J%c", skip);
                }

                /* Copy the the scan lines. */
                code = lcnt = gdev_prn_copy_scan_lines(pdev, lnum, in, in_size);
                if (code < 0)
                    goto xit;
                if ( lcnt < 8 * in_y_mult )
                {	/* Pad with lines of zeros. */
                        memset(in + lcnt * line_size, 0,
                               in_size - lcnt * line_size);
                }

                if ( y_9pin_high == 2 )
                {	/* Force printing of every dot in one pass */
                        /* by reducing vertical resolution */
                        /* (ORing with the next line of data). */
                        /* This is necessary because some Epson compatibles */
                        /* can't print neighboring dots. */
                        int i;
                        for ( i = 0; i < line_size * in_y_mult; ++i )
                                in[i] |= in[i + line_size];
                }

                if ( y_9pin_high )
                {	/* Shuffle the scan lines */
                        byte *p;
                        int i;
                        static const char index[] =
                        {  0,  8, 16,  1,  9, 17,
                           2, 10, 18,  3, 11, 19,
                           4, 12, 20,  5, 13, 21,
                           6, 14, 22,  7, 15, 23
                        };

                        for ( i = 0; i < 24; i++ )
                        {
                                memcpy(out+(index[i]*line_size),
                                       in+(i*line_size), line_size);
                        }
                        p = in;
                        in = out;
                        out = p;
                }

        for ( ypass = 0; ypass < y_passes; ypass++ )
        {
            for ( pass = first_pass; pass <= last_pass; pass++ )
            {
                /* We have to 'transpose' blocks of 8 pixels x 8 lines, */
                /* because that's how the printer wants the data. */
                /* If we are in a 24-pin mode, we have to transpose */
                /* groups of 3 lines at a time. */

                if ( pass == first_pass )
                {
                    out_end = out;
                    inp = in;
                    in_end = inp + line_size;

                    if ( y_24pin )
                    {
                        for ( ; inp < in_end; inp++, out_end += 24 )
                        {
                            gdev_prn_transpose_8x8(inp, line_size, out_end, 3);
                            gdev_prn_transpose_8x8(inp + line_size * 8,
                                                   line_size, out_end + 1, 3);
                            gdev_prn_transpose_8x8(inp + line_size * 16,
                                                   line_size, out_end + 2, 3);
                        }
                        /* Remove trailing 0s. */
                        while ( out_end > out && out_end[-1] == 0 &&
                                out_end[-2] == 0 && out_end[-3] == 0)
                        {
                             out_end -= 3;
                        }
                    }
                    else
                    {
                        for ( ; inp < in_end; inp++, out_end += 8 )
                        {
                            gdev_prn_transpose_8x8(inp + (ypass * 8*line_size),
                                                   line_size, out_end, 1);
                        }
                        /* Remove trailing 0s. */
                        while ( out_end > out && out_end[-1] == 0 )
                        {
                            out_end--;
                        }
                    }
                }

                for ( out_blk = outp = out; outp < out_end; )
                {
                    /* Skip a run of leading 0s.  At least */
                    /* tab_min_pixels are needed to make tabbing */
                    /* worth it.  We do everything by 3's to */
                    /* avoid having to make different cases */
                    /* for 9- and 24-pin. */
                   if ( !archaic &&
                        *outp == 0 && out_end - outp >= tab_min_pixels &&
                        (outp[1] | outp[2]) == 0 &&
                        !memcmp((char *)outp, (char *)outp + 3,
                                tab_min_pixels - 3)
                      )
                    {
                        byte *zp = outp;
                        int tpos;
                        byte *newp;

                        outp += tab_min_pixels;
                        while ( outp + 3 <= out_end &&
                                *outp == 0 &&
                                outp[1] == 0 && outp[2] == 0 )
                        {
                            outp += 3;
                        }
                        tpos = (outp - out) / bytes_per_space;
                        newp = out + tpos * bytes_per_space;
                        if ( newp > zp + 10 )
                        {
                            /* Output preceding bit data.*/
                            if ( zp > out_blk )
                            {
                                /* only false at beginning of line */
                                eps_output_run(out_blk, (int)(zp - out_blk),
                                               out_y_mult, start_graphics,
                                               prn_stream,
                                               (y_9pin_high == 2 ?
                                                (1 + ypass) & 1 : pass));
                            }
                            /* Tab over to the appropriate position. */
                            if ( tab_hiccup )
                              gp_fputs("\010 ", prn_stream); /* bksp, space */
                            /* The following statement is broken up */
                            /* to work around a bug in emx/gcc. */
                            gp_fprintf(prn_stream, "\033D%c", tpos);
                            gp_fputc(0, prn_stream);
                            gp_fputc('\t', prn_stream);
                            out_blk = outp = newp;
                        }
                    }
                    else
                    {
                        outp += out_y_mult;
                    }
                }
                if ( outp > out_blk )
                {
                    eps_output_run(out_blk, (int)(outp - out_blk),
                                   out_y_mult, start_graphics,
                                   prn_stream,
                                   (y_9pin_high == 2 ? (1 + ypass) & 1 : pass));
                }

                gp_fputc('\r', prn_stream);
            }
            if ( ypass < y_passes - 1 )
                gp_fputs("\033J\001", prn_stream);
        }
        skip = 24 - y_passes + 1;		/* no skip on last Y pass */
        lnum += 8 * in_y_mult;
        }

        /* Eject the page and reinitialize the printer */
        gp_fputs(end_string, prn_stream);
        gp_fflush(prn_stream);

xit:
        if ( buf1 )
            gs_free(pdev->memory, (char *)buf1, in_size, 1, "eps_print_page(buf1)");
        if ( buf2 )
            gs_free(pdev->memory, (char *)buf2, in_size, 1, "eps_print_page(buf2)");
        if (code < 0)
            return_error(code);
        return code;
}

/* Output a single graphics command. */
/* pass=0 for all columns, 1 for even columns, 2 for odd columns. */
static void
eps_output_run(byte *data, int count, int y_mult,
  char start_graphics, gp_file *prn_stream, int pass)
{
        int xcount = count / y_mult;

        gp_fputc(033, prn_stream);
        if ( !(start_graphics & ~3) )
        {
                gp_fputc("KLYZ"[(int)start_graphics], prn_stream);
        }
        else
        {
                gp_fputc('*', prn_stream);
                gp_fputc(start_graphics & ~DD, prn_stream);
        }
        gp_fputc(xcount & 0xff, prn_stream);
        gp_fputc(xcount >> 8, prn_stream);
        if ( !pass )
        {
                gp_fwrite(data, 1, count, prn_stream);
        }
        else
        {
                /* Only write every other column of y_mult bytes. */
                int which = pass;
                register byte *dp = data;
                register int i, j;

                for ( i = 0; i < xcount; i++, which++ )
                {
                        for ( j = 0; j < y_mult; j++, dp++ )
                        {
                                gp_fputc(((which & 1) ? *dp : 0), prn_stream);
                        }
                }
        }
}

/* The print_page procedures are here, to avoid a forward reference. */
#ifndef ARCHAIC
#  define ARCHAIC 0
#endif
#ifndef TAB_HICCUP
#  define TAB_HICCUP 0
#endif

#define ESC 0x1b
static const char eps_init_string[] = {
#if ARCHAIC
        ESC, '@', 022 /*^R*/, ESC, 'Q'
#else
        ESC, '@', ESC, 'P', ESC, 'l', 0, '\r', ESC, 'Q'
#endif
};

static int
epson_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
        return eps_print_page(pdev, prn_stream, 0, eps_init_string,
                              sizeof(eps_init_string), "\f\033@",
                              ARCHAIC, TAB_HICCUP);
}

static int
eps9high_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
        return eps_print_page(pdev, prn_stream, 1, eps_init_string,
                              sizeof(eps_init_string), "\f\033@",
                              ARCHAIC, TAB_HICCUP);
}

static int
eps9mid_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
        return eps_print_page(pdev, prn_stream, 2, eps_init_string,
                              sizeof(eps_init_string), "\f\033@",
                              ARCHAIC, TAB_HICCUP);
}

static int
ibmpro_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
    /*
     * IBM Proprinter Guide to Operations, p. 4-5: "DC1: Select Printer: Sets
     * the printer to accept data from your computer."  Prevents printer from
     * interpreting first characters as literal text.
     */
#define DC1 0x11
        static const char ibmpro_init_string[] = {
                DC1, ESC, '3', 0x30
        };
#undef DC1
        return eps_print_page(pdev, prn_stream, 0, ibmpro_init_string,
                              sizeof(ibmpro_init_string), "\f", 1, 0);
}
