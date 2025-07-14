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

/* Epson color dot-matrix printer driver by dave@exlog.com */
#include "gdevprn.h"

/*
 * For 9-pin printers, you may select
 *   X_DPI = 60, 120, or 240
 *   Y_DPI = 60 or 72
 * For 24-pin printers, you may select
 *   X_DPI = 60, 120, 180, 240, or 360
 *   Y_DPI = 60, 72, 180, or 216
 * Note that a given printer implements *either* Y_DPI = 60 | 180 *or*
 * Y_DPI = 72 | 216; no attempt is made to check this here.
 * Note that X_DPI = 180 or 360 requires Y_DPI > 100;
 * this isn't checked either.  Finally, note that X_DPI=240 and
 * X_DPI=360 are double-density modes requiring two passes to print.
 *
 * The values of X_DPI and Y_DPI may be set at compile time:
 * see gdevs.mak.
 *
 * At some time in the future, we could simulate 24-bit output on
 * 9-pin printers by using fractional vertical positioning;
 * we could even implement an X_DPI=360 mode by using the
 * ESC++ command that spaces vertically in units of 1/360"
 * (not supported on many printers.)
 */

#ifndef X_DPI
#  define X_DPI 180             /* pixels per inch */
#endif
#ifndef Y_DPI
#  define Y_DPI 180             /* pixels per inch */
#endif

/*
**    Colors for EPSON LQ-2550.
**
**    We map VIOLET to BLUE since this is the best we can do.
*/
#define BLACK   0
#define MAGENTA 1
#define CYAN    2
#define VIOLET  3
#define YELLOW  4
#define RED     5
#define GREEN   6
#define WHITE   7

/*
**    The offset in this array correspond to
**    the ESC-r n value
*/
static char rgb_color[2][2][2] = {
    {{BLACK, VIOLET}, {GREEN, CYAN}},
    {{RED, MAGENTA}, {YELLOW, WHITE}}
};

/* Map an RGB color to a printer color. */
#define cv_shift (sizeof(gx_color_value) * 8 - 1)
static gx_color_index
epson_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{

    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];

    if (gx_device_has_color(dev))
/* use ^7 so WHITE is 0 for internal calculations */
        return (gx_color_index) rgb_color[r >> cv_shift][g >> cv_shift][b >> cv_shift] ^ 7;
    else
        return gx_default_map_rgb_color(dev, cv);
}

/* Map the printer color back to RGB. */
static int
epson_map_color_rgb(gx_device * dev, gx_color_index color,
                    gx_color_value prgb[3])
{
#define c1 gx_max_color_value
    if (gx_device_has_color(dev))
        switch ((ushort) color ^ 7) {
            case BLACK:
                prgb[0] = 0;
                prgb[1] = 0;
                prgb[2] = 0;
                break;
            case VIOLET:
                prgb[0] = 0;
                prgb[1] = 0;
                prgb[2] = c1;
                break;
            case GREEN:
                prgb[0] = 0;
                prgb[1] = c1;
                prgb[2] = 0;
                break;
            case CYAN:
                prgb[0] = 0;
                prgb[1] = c1;
                prgb[2] = c1;
                break;
            case RED:
                prgb[0] = c1;
                prgb[1] = 0;
                prgb[2] = 0;
                break;
            case MAGENTA:
                prgb[0] = c1;
                prgb[1] = 0;
                prgb[2] = c1;
                break;
            case YELLOW:
                prgb[0] = c1;
                prgb[1] = c1;
                prgb[2] = 0;
                break;
            case WHITE:
                prgb[0] = c1;
                prgb[1] = c1;
                prgb[2] = c1;
                break;
    } else
        return gx_default_map_color_rgb(dev, color, prgb);
    return 0;
}

/* The device descriptor */
static dev_proc_print_page(epsc_print_page);

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
epson_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, map_rgb_color, epson_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, epson_map_color_rgb);
    set_dev_proc(dev, encode_color, epson_map_rgb_color);
    set_dev_proc(dev, decode_color, epson_map_color_rgb);
}

const gx_device_printer far_data gs_epsonc_device =
prn_device(epson_initialize_device_procs,
           "epsonc",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           0, 0, 0.25, 0,       /* margins */
           3, epsc_print_page);

/* ------ Internal routines ------ */

/* Forward references */
static void epsc_output_run(byte *, int, int, char, gp_file *, int);

/* Send the page to the printer. */
#define DD 0x80                 /* double density flag */
static int
epsc_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    static int graphics_modes_9[5] = { -1, 0 /*60 */ , 1 /*120 */ , -1, DD + 3  /*240 */
    };
    static int graphics_modes_24[7] =
        { -1, 32 /*60 */ , 33 /*120 */ , 39 /*180 */ ,
        -1, -1, DD + 40         /*360 */
    };
    int y_24pin = pdev->y_pixels_per_inch > 72;
    int y_mult = (y_24pin ? 3 : 1);
    int line_size = (pdev->width + 7) >> 3;     /* always mono */
    size_t in_size = line_size * (8 * y_mult);
    size_t out_size = ((pdev->width + 7) & -8) * y_mult;
    byte *in;
    byte *out;
    int x_dpi = (int)pdev->x_pixels_per_inch;

    char start_graphics;
    int first_pass;
    int last_pass;
    int dots_per_space;
    int bytes_per_space;
    int skip = 0, lnum = 0, code = 0, pass;

    byte *color_in;
    size_t color_line_size, color_in_size;
    int spare_bits;
    int whole_bits;

    int max_dpi = 60 * (
            (y_24pin) ?
            sizeof(graphics_modes_24) / sizeof(graphics_modes_24[0])
            :
            sizeof(graphics_modes_9) / sizeof(graphics_modes_9[0])
            )
            - 1;
    if (x_dpi > max_dpi) {
        return_error(gs_error_rangecheck);
    }

    start_graphics = (char)
        ((y_24pin ? graphics_modes_24 : graphics_modes_9)[x_dpi / 60]);
    first_pass = (start_graphics & DD ? 1 : 0);
    last_pass = first_pass * 2;
    dots_per_space = x_dpi / 10;    /* pica space = 1/10" */
    bytes_per_space = dots_per_space * y_mult;
    if (bytes_per_space == 0) {
        /* This avoids divide by zero later on, bug 701843. */
        return_error(gs_error_rangecheck);
    }

    in =
        (byte *) gs_malloc(pdev->memory, in_size + 1, 1,
                           "epsc_print_page(in)");
    out =
        (byte *) gs_malloc(pdev->memory, out_size + 1, 1,
                           "epsc_print_page(out)");

    /* declare color buffer and related vars */
    spare_bits = (pdev->width % 8); /* left over bits to go to margin */
    whole_bits = pdev->width - spare_bits;

    /* Check allocations */
    if (in == 0 || out == 0) {
        if (in)
            gs_free(pdev->memory, (char *)in, in_size + 1, 1,
                    "epsc_print_page(in)");
        if (out)
            gs_free(pdev->memory, (char *)out, out_size + 1, 1,
                    "epsc_print_page(out)");
        return_error(gs_error_VMerror);
    }

    /* Initialize the printer and reset the margins. */
    gp_fwrite("\033@\033P\033l\000\033Q\377\033U\001\r", 1, 14, prn_stream);

    /* Create color buffer */
    if (gx_device_has_color(pdev)) {
        color_line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
        color_in_size = color_line_size * (8 * y_mult);
        if ((color_in = (byte *) gs_malloc(pdev->memory, color_in_size + 1, 1,
                                           "epsc_print_page(color)")) == 0) {
            gs_free(pdev->memory, (char *)in, in_size + 1, 1,
                    "epsc_print_page(in)");
            gs_free(pdev->memory, (char *)out, out_size + 1, 1,
                    "epsc_print_page(out)");
            return_error(gs_error_VMerror);
        }
    } else {
        color_in = in;
        color_in_size = in_size;
        color_line_size = line_size;
    }

    /* Print lines of graphics */
    while (lnum < pdev->height) {
        int lcnt;
        byte *nextcolor = NULL; /* position where next color appears */
        byte *nextmono = NULL;  /* position to map next color */

        /* Copy 1 scan line and test for all zero. */
        code = gdev_prn_copy_scan_lines(pdev, lnum, color_in, color_line_size);
        if (code < 0)
            goto xit;

        if (color_in[0] == 0 &&
            !memcmp((char *)color_in, (char *)color_in + 1,
                    color_line_size - 1)
            ) {
            lnum++;
            skip += 3 / y_mult;
            continue;
        }

        /* Vertical tab to the appropriate position. */
        while (skip > 255) {
            gp_fputs("\033J\377", prn_stream);
            skip -= 255;
        }
        if (skip)
            gp_fprintf(prn_stream, "\033J%c", skip);

        /* Copy the rest of the scan lines. */
        code = gdev_prn_copy_scan_lines(pdev, lnum + 1,
                                            color_in + color_line_size,
                                            color_in_size - color_line_size);
         if (code < 0)
             goto xit;
         lcnt = code + 1;

        if (lcnt < 8 * y_mult) {
            memset((char *)(color_in + lcnt * color_line_size), 0,
                   color_in_size - lcnt * color_line_size);
            if (gx_device_has_color(pdev))      /* clear the work buffer */
                memset((char *)(in + lcnt * line_size), 0,
                       in_size - lcnt * line_size);
        }

        /*
        ** We need to create a normal epson scan line from our color scan line
        ** We do this by setting a bit in the "in" buffer if the pixel byte is set
        ** to any color.  We then search for any more pixels of that color, setting
        ** "in" accordingly.  If any other color is found, we save it for the next
        ** pass.  There may be up to 7 passes.
        ** In the future, we should make the passes so as to maximize the
        ** life of the color ribbon (i.e. go lightest to darkest).
        */
        do {
            byte *inp = in;
            byte *in_end = in + line_size;
            byte *out_end = out;
            byte *out_blk;
            register byte *outp;

            if (gx_device_has_color(pdev)) {
                register int i, j;
                register byte *outbuf, *realbuf;
                byte current_color;
                int end_next_bits = whole_bits;
                int lastbits;

                /* Move to the point in the scanline that has a new color */
                if (nextcolor) {
                    realbuf = nextcolor;
                    outbuf = nextmono;
                    memset((char *)in, 0, (nextmono - in));
                    i = nextcolor - color_in;
                    nextcolor = NULL;
                    end_next_bits = (i / color_line_size) * color_line_size
                        + whole_bits;
                } else {
                    i = 0;
                    realbuf = color_in;
                    outbuf = in;
                    nextcolor = NULL;
                }
                /* move thru the color buffer, turning on the appropriate
                ** bit in the "mono" buffer", setting pointers to the next
                ** color and changing the color output of the epson
                */
                for (current_color = 0; i <= color_in_size && outbuf < in + in_size; outbuf++) {
                    /* Remember, line_size is rounded up to next whole byte
                    ** whereas color_line_size is the proper length
                    ** We only want to set the proper bits in the last line_size byte.
                    */
                    if (spare_bits && i == end_next_bits) {
                        end_next_bits = whole_bits + i + spare_bits;
                        lastbits = 8 - spare_bits;
                    } else
                        lastbits = 0;

                    for (*outbuf = 0, j = 8;
                         --j >= lastbits && i <= color_in_size;
                         realbuf++, i++) {
                        if (*realbuf) {
                            if (current_color > 0) {
                                if (*realbuf == current_color) {
                                    *outbuf |= 1 << j;
                                    *realbuf = 0;       /* throw this byte away */
                                }
                                /* save this location for next pass */
                                else if (nextcolor == NULL) {
                                    nextcolor = realbuf - (7 - j);
                                    nextmono = outbuf;
                                }
                            } else {
                                *outbuf |= 1 << j;
                                current_color = *realbuf;       /* set color */
                                *realbuf = 0;
                            }
                        }
                    }
                }
                *outbuf = 0;    /* zero the end, for safe keeping */
               /* Change color on the EPSON, current_color must be set
               ** but lets check anyway
               */
                if (current_color)
                    gp_fprintf(prn_stream, "\033r%c", current_color ^ 7);
            }

            /* We have to 'transpose' blocks of 8 pixels x 8 lines, */
            /* because that's how the printer wants the data. */
            /* If we are in a 24-pin mode, we have to transpose */
            /* groups of 3 lines at a time. */

            if (y_24pin) {
                for (; inp < in_end; inp++, out_end += 24) {
                    gdev_prn_transpose_8x8(inp, line_size, out_end, 3);
                    gdev_prn_transpose_8x8(inp + line_size * 8, line_size,
                                           out_end + 1, 3);
                    gdev_prn_transpose_8x8(inp + line_size * 16, line_size,
                                           out_end + 2, 3);
                }
                /* Remove trailing 0s. */
                while (out_end > out && out_end[-1] == 0 &&
                       out_end[-2] == 0 && out_end[-3] == 0)
                    out_end -= 3;
            } else {
                for (; inp < in_end; inp++, out_end += 8) {
                    gdev_prn_transpose_8x8(inp, line_size, out_end, 1);
                }
                /* Remove trailing 0s. */
                while (out_end > out && out_end[-1] == 0)
                    out_end--;
            }

            for (pass = first_pass; pass <= last_pass; pass++) {
                for (out_blk = outp = out; outp < out_end;) {   /* Skip a run of leading 0s. */
                    /* At least 10 are needed to make tabbing worth it. */
                    /* We do everything by 3's to avoid having to make */
                    /* different cases for 9- and 24-pin. */

                    if (*outp == 0 && outp + 12 <= out_end &&
                        outp[1] == 0 && outp[2] == 0 &&
                        (outp[3] | outp[4] | outp[5]) == 0 &&
                        (outp[6] | outp[7] | outp[8]) == 0 &&
                        (outp[9] | outp[10] | outp[11]) == 0) {
                        byte *zp = outp;
                        int tpos;
                        byte *newp;

                        outp += 12;
                        while (outp + 3 <= out_end && *outp == 0 &&
                               outp[1] == 0 && outp[2] == 0)
                            outp += 3;
                        tpos = (outp - out) / bytes_per_space;
                        newp = out + tpos * bytes_per_space;
                        if (newp > zp + 10) {   /* Output preceding bit data. */
                            if (zp > out_blk)
                                /* only false at */
                                /* beginning of line */
                                epsc_output_run(out_blk, (int)(zp - out_blk),
                                                y_mult, start_graphics,
                                                prn_stream, pass);
                            /* Tab over to the appropriate position. */
                            gp_fprintf(prn_stream, "\033D%c%c\t", tpos, 0);
                            out_blk = outp = newp;
                        }
                    } else
                        outp += y_mult;
                }
                if (outp > out_blk)
                    epsc_output_run(out_blk, (int)(outp - out_blk),
                                    y_mult, start_graphics, prn_stream, pass);

                gp_fputc('\r', prn_stream);
            }
        } while (nextcolor);
        skip = 24;
        lnum += 8 * y_mult;
    }

    /* Eject the page and reinitialize the printer */
    gp_fputs("\f\033@", prn_stream);

xit:
    gs_free(pdev->memory, (char *)out, out_size + 1, 1,
            "epsc_print_page(out)");
    gs_free(pdev->memory, (char *)in, in_size + 1, 1, "epsc_print_page(in)");
    if (gx_device_has_color(pdev))
        gs_free(pdev->memory, (char *)color_in, color_in_size + 1, 1,
                "epsc_print_page(rin)");
    return code;
}

/* Output a single graphics command. */
/* pass=0 for all columns, 1 for even columns, 2 for odd columns. */
static void
epsc_output_run(byte * data, int count, int y_mult,
                char start_graphics, gp_file * prn_stream, int pass)
{
    int xcount = count / y_mult;

    gp_fputc(033, prn_stream);
    if (!(start_graphics & ~3)) {
        gp_fputc("KLYZ"[(int)start_graphics], prn_stream);
    } else {
        gp_fputc('*', prn_stream);
        gp_fputc(start_graphics & ~DD, prn_stream);
    }
    gp_fputc(xcount & 0xff, prn_stream);
    gp_fputc(xcount >> 8, prn_stream);
    if (!pass)
        gp_fwrite((char *)data, 1, count, prn_stream);
    else {                      /* Only write every other column of y_mult bytes. */
        int which = pass;
        byte *dp = data;
        register int i, j;

        for (i = 0; i < xcount; i++, which++)
            for (j = 0; j < y_mult; j++, dp++) {
                gp_fputc(((which & 1) ? *dp : 0), prn_stream);
            }
    }
}
