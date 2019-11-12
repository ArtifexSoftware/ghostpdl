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

/*
 * Okidata IBM compatible dot-matrix printer driver for Ghostscript.
 *
 * This device is for the Okidata Microline IBM compatible 9 pin dot
 * matrix printers.  It is derived from the Epson 9 pin printer driver
 * using the standard 1/72" vertical pin spacing and the 60/120/240
 * dpi horizontal resolutions.  The vertical feed resolution however
 * is 1/144" and the Okidata implements the standard 1/216" requests
 * through "scaling":
 *
 *   (power on)
 *   "\033J\001" (vertical feed 1/216")  => Nothing happens
 *   "\033J\001" (vertical feed 1/216")  => Advance 1/144"
 *   "\033J\001" (vertical feed 1/216")  => Advance 1/144"
 *   "\033J\001" (vertical feed 1/216")  => Nothing happens
 *   (and so on)
 *
 * The simple minded accounting used here keep track of when the
 * page actually advances assumes the printer starts in a "power on"
 * state.
 *
 * Supported resolutions are:
 *
 *    60x72      60x144
 *   120x72     120x144
 *   240x72     240x144
 *
 */
#include "gdevprn.h"

/*
 * Valid values for X_DPI:
 *
 *     60, 120, 240
 *
 * The value specified at compile time is the default value used if the
 * user does not specify a resolution at runtime.
 */

#ifndef X_DPI
#  define X_DPI 120
#endif

/*
 * Valid values for Y_DPI:
 *
 *     72, 144
 *
 * The value specified at compile time is the default value used if the
 * user does not specify a resolution at runtime.
 */

#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_print_page(okiibm_print_page);

/* Okidata IBM device */
const gx_device_printer far_data gs_okiibm_device =
  prn_device(prn_bg_procs, "okiibm",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
        X_DPI, Y_DPI,
        0.25, 0.0, 0.25, 0.0,			/* margins */
        1, okiibm_print_page);

/* ------ Internal routines ------ */

/* Forward references */
static void okiibm_output_run(byte *, int, int, char, gp_file *, int);

/* Send the page to the printer. */
static int
okiibm_print_page1(gx_device_printer *pdev, gp_file *prn_stream, int y_9pin_high,
  const char *init_string, int init_length,
  const char *end_string, int end_length)
{
        static const char graphics_modes_9[5] =
        {
        -1, 0 /*60*/, 1 /*120*/, -1, 3 /*240*/
        };

        int in_y_mult;
        int line_size;
        int in_size;
        byte *buf1;
        byte *buf2;
        byte *in;
        byte *out;
        int out_y_mult;
        int x_dpi;
        char start_graphics;
        int first_pass;
        int last_pass;
        int y_passes;
        int skip = 0, lnum = 0, pass, ypass;
        int y_step = 0;
        int code = 0;

        x_dpi = pdev->x_pixels_per_inch;
        if (x_dpi / 60 >= sizeof(graphics_modes_9)/sizeof(graphics_modes_9[0])) {
            return_error(gs_error_rangecheck);
        }
        in_y_mult = (y_9pin_high ? 2 : 1);
        line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        /* Note that in_size is a multiple of 8. */
        in_size = line_size * (8 * in_y_mult);
        buf1 = (byte *)gs_malloc(pdev->memory, in_size, 1, "okiibm_print_page(buf1)");
        buf2 = (byte *)gs_malloc(pdev->memory, in_size, 1, "okiibm_print_page(buf2)");
        in = buf1;
        out = buf2;
        out_y_mult = 1;
        start_graphics = graphics_modes_9[x_dpi / 60];
        first_pass = (start_graphics == 3 ? 1 : 0);
        last_pass = first_pass * 2;
        y_passes = (y_9pin_high ? 2 : 1);
        y_step = 0;

        /* Check allocations */
        if ( buf1 == 0 || buf2 == 0 ) {
            code = gs_error_VMerror;
            goto xit;
        }

        /* Initialize the printer. */
        gp_fwrite(init_string, 1, init_length, prn_stream);

        /* Print lines of graphics */
        while ( lnum < pdev->height )
        {
                byte *in_data;
                byte *inp;
                byte *in_end;
                byte *out_end = NULL;
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
                        skip += 2 / in_y_mult;
                        continue;
                }

                /*
                 * Vertical tab to the appropriate position.
                 * The skip count is in 1/144" steps.  If total
                 * vertical request is not a multiple od 1/72"
                 * we need to make sure the page is actually
                 * going to advance.
                 */
                if ( skip & 1 )
                {
                        int n = 1 + (y_step == 0 ? 1 : 0);
                        gp_fprintf(prn_stream, "\033J%c", n);
                        y_step = (y_step + n) % 3;
                        skip -= 1;
                }
                skip = skip / 2 * 3;
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
                lcnt = gdev_prn_copy_scan_lines(pdev, lnum, in, in_size);
                if ( lcnt < 8 * in_y_mult )
                {	/* Pad with lines of zeros. */
                        memset(in + lcnt * line_size, 0,
                               in_size - lcnt * line_size);
                }

                if ( y_9pin_high )
                {	/* Shuffle the scan lines */
                        byte *p;
                        int i;
                        static const char index[] =
                        {  0, 2, 4, 6, 8, 10, 12, 14,
                           1, 3, 5, 7, 9, 11, 13, 15
                        };
                        for ( i = 0; i < 16; i++ )
                        {
                                memcpy( out + (i * line_size),
                                        in + (index[i] * line_size),
                                        line_size);
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

                if ( pass == first_pass )
                {
                    out_end = out;
                    inp = in;
                    in_end = inp + line_size;

                    for ( ; inp < in_end; inp++, out_end += 8 )
                    {
                        gdev_prn_transpose_8x8(inp + (ypass * 8 * line_size),
                                               line_size, out_end, 1);
                    }
                    /* Remove trailing 0s. */
                    while ( out_end > out && out_end[-1] == 0 )
                    {
                        out_end--;
                    }
                }

                /* Transfer whatever is left and print. */
                if ( out_end > out )
                {
                    okiibm_output_run(out, (int)(out_end - out),
                                   out_y_mult, start_graphics,
                                   prn_stream, pass);
                }
                gp_fputc('\r', prn_stream);
            }
            if ( ypass < y_passes - 1 )
            {
                int n = 1 + (y_step == 0 ? 1 : 0);
                gp_fprintf(prn_stream, "\033J%c", n);
                y_step = (y_step + n) % 3;
            }
        }
        skip = 16 - y_passes + 1;		/* no skip on last Y pass */
        lnum += 8 * in_y_mult;
        }

        /* Reinitialize the printer. */
        gp_fwrite(end_string, 1, end_length, prn_stream);
        gp_fflush(prn_stream);

xit:
        if ( buf1 )
            gs_free(pdev->memory, (char *)buf1, in_size, 1, "okiibm_print_page(buf1)");
        if ( buf2 )
            gs_free(pdev->memory, (char *)buf2, in_size, 1, "okiibm_print_page(buf2)");
        if (code < 0)
            return_error(code);
        return 0;
}

/* Output a single graphics command. */
/* pass=0 for all columns, 1 for even columns, 2 for odd columns. */
static void
okiibm_output_run(byte *data, int count, int y_mult,
  char start_graphics, gp_file *prn_stream, int pass)
{
        int xcount = count / y_mult;

        gp_fputc(033, prn_stream);
        gp_fputc((int)("KLYZ"[(int)start_graphics]), prn_stream);
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

static const char okiibm_init_string[]	= { 0x18 };
static const char okiibm_end_string[]	= { 0x0c };
static const char okiibm_one_direct[]	= { 0x1b, 0x55, 0x01 };
static const char okiibm_two_direct[]	= { 0x1b, 0x55, 0x00 };

static int
okiibm_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
        char init_string[16], end_string[16];
        int init_length, end_length;

        init_length = sizeof(okiibm_init_string);
        memcpy(init_string, okiibm_init_string, init_length);

        end_length = sizeof(okiibm_end_string);
        memcpy(end_string, okiibm_end_string, end_length);

        if ( pdev->y_pixels_per_inch > 72 &&
             pdev->x_pixels_per_inch > 60 )
        {
                /* Unidirectional printing for the higher resolutions. */
                memcpy( init_string + init_length, okiibm_one_direct,
                        sizeof(okiibm_one_direct) );
                init_length += sizeof(okiibm_one_direct);

                memcpy( end_string + end_length, okiibm_two_direct,
                        sizeof(okiibm_two_direct) );
                end_length += sizeof(okiibm_two_direct);
        }

        return okiibm_print_page1( pdev, prn_stream,
                                   pdev->y_pixels_per_inch > 72 ? 1 : 0,
                                   init_string, init_length,
                                   end_string, end_length );
}
