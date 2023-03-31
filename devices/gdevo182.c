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

/* Okidata Microline 182 printer driver */

/* Contributed by Maarten Koning (smeg@bnr.ca) April 4, 1993 */

/****************************************************************

I use this driver from Unix with the following aliases:

alias psp "gs -q -sDEVICE=oki182 -sOutputFile=\|lpr - <\!*"
alias psphigh "gs -q -sDEVICE=oki182 -r144 -sOutputFile=\|lpr - <\!*"

ps. I have my printer DIP switches set to the following (as viewed
        while standing in front of your printer looking down into the
        config access hatch located at the top of your printer
        in the centre back).

Upper  Upper   Bottom
Left   Right   (at right)

 x	x        x
 x       x	x
 x       x       x
x        x	x
 x	x        x
 x	x        x
x        x	x
 x	x        x

The upper DIP switches are on a SuperSpeed Serial
card that will do 19200 baud.  I have it set at 9600
baud since that seems sufficient to keep the printer
busy.

The important thing is to be in 8-bit mode so that
the graphics data can't match any Okidata commands
(This driver sets the high bit of graphics data to 1).

****************************************************************/

#include "gdevprn.h"

/*
 *  Available resolutions are 72x72 or 144x144;
 *  (144x72) would be possible to do also, but I didn't bother)
 */

/* The device descriptor */

static dev_proc_print_page(oki_print_page);

/* The print_page proc is compatible with allowing bg printing */
const gx_device_printer far_data gs_oki182_device =
  prn_device(gdev_prn_initialize_device_procs_mono_bg, "oki182",
        80,				/* width_10ths, 8.0" */
        110,				/* height_10ths, 11" */
        72,				/* x_dpi */
        72,				/* y_dpi */
        0, 0, 0, 0,			/* margins */
        1, oki_print_page);

/* ------ internal routines ------ */

/* out is a pointer to an array of 7 scan lines,
   lineSize is the number of bytes between a pixel and
   the pixel directly beneath it.
   scanBits is the number of bits in each scan line
   out is a pointer to an array of column data, which
   is how the Okidata wants the graphics image.

   each column of graphics data is 7 bits high and
   is encoded in a byte - highest pixel in the column
   is the lowest bit in the byte.  The upper bit of the
   byte is set so that the okidata doesn't mistake
   graphic image data for graphic commands.
*/

static void
oki_transpose(byte *in, byte *out, int scanBits, register int lineSize)
{
        register int bitMask = 0x80;
        register byte *inPtr;
        register byte outByte;

        while (scanBits-- > 0) {

                inPtr = in;

                if (*inPtr & bitMask)
                        outByte = 0x81;
                else
                        outByte = 0x80;
                if (*(inPtr += lineSize) & bitMask)
                        outByte += 0x02;
                if (*(inPtr += lineSize) & bitMask)
                        outByte += 0x04;
                if (*(inPtr += lineSize) & bitMask)
                        outByte += 0x08;
                if (*(inPtr += lineSize) & bitMask)
                        outByte += 0x10;
                if (*(inPtr += lineSize) & bitMask)
                        outByte += 0x20;
                if (*(inPtr += lineSize) & bitMask)
                        outByte += 0x40;

                *out++ = outByte;

                if ((bitMask >>= 1) == 0) {
                        bitMask = 0x80;
                        in ++;
                }
        }
}

/* This routine tries to compress a sequence of okidata
   graphic bytes by trimming off leading and trailing
   zeros.  Trailing zeros can be thrown away and leading
   zeros can be replaced with a much smaller number of spaces.

   'in' is a pointer to the graphic bytes to be compressed.
   origWidth is the number of bytes pointed to by 'in'.
   highRes is non-zero when 144x144 mode is being used.

   numSpaces is set to the number of spaces that should
   be printed before the compressed image. newWidth is
   the new number of bytes that the return value of this
   function points to.

   xxx - A future enhancement would be to replace long sequences
   of embedded zeros with exit.graphics-<n> spaces-enter.graphics
*/
static byte *
oki_compress(byte *in, int origWidth, int highRes,
                        int *numSpaces, int *newWidth)
{
        int spaces = 0;
        int columns_per_space = 6;

        byte *in_end = in + origWidth;

        /* remove trailing zeros (which are realy 0x80's) */
        while (in_end > in && in_end[-1] == 0x80)
                in_end --;

        if (highRes)
                columns_per_space = 12;

        /* remove leading zeros that can be replaced by spaces */
        while(in < in_end && in[0] == 0x80 && memcmp((char *)in,
                                (char *)in + 1, columns_per_space - 1) == 0) {
                spaces++;
                in += columns_per_space;
        }

        *numSpaces = spaces;

        /* just in case we compressed this line out of existance */
        if (in_end > in)
                *newWidth = in_end - in;
        else
                *newWidth = 0;

        return(in);
}

/* Send the page to the printer. */

static int
oki_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
        int highRes = pdev->y_pixels_per_inch > 100;
        int bits_per_column = 7;
        int i, spaces, width;
        int lcnt;

        int line_size = gdev_prn_raster((gx_device_printer *)pdev);

        byte *in = (byte *)gs_malloc(pdev->memory, 16, line_size, "oki_print_page(in)");

        byte *out1 = (byte *)gs_malloc(pdev->memory, 8, line_size, "oki_print_page(out1)");
        byte *out2 = (byte *)gs_malloc(pdev->memory, 8, line_size, "oki_print_page(out2)");

        byte *out3;

        int lnum = 0;
        int skip = 0;
        int code = 0;

        if ( in == 0 || out1 == 0 || out2 == 0)
        {	code = gs_note_error(gs_error_VMerror);
                goto bail;
        }

        /* Initialize the printer. */
        /* CAN; 72x72; left margin = 001; disable skip over perforation */
        gp_fwrite("\030\034\033%C001\033%S0", 1, 12, prn_stream);

        if (highRes) {
                gp_fwrite("\033R", 1, 2, prn_stream);
                bits_per_column = 14;
        }

        /* Transfer pixels to printer */
        while ( lnum < pdev->height ) {

                /* Copy 1 scan line and test for all zero. */
                code = gdev_prn_copy_scan_lines(pdev, lnum, in, line_size);
                if ( code < 0 )
                        goto xit;

                /* if line is all zero, skip */
                if ( in[0] == 0 && !memcmp((char *)in, (char *)in + 1,
                                                        line_size - 1)) {
                        lnum++;
                        if (highRes)
                                skip++;
                        else
                                skip += 2;
                        continue;
                }

                /* use fine line feed to get to the appropriate position. */
                while ( skip > 127 ) {
                        gp_fputs("\033%5\177", prn_stream);
                        skip -= 127;
                }
                if ( skip )
                        gp_fprintf(prn_stream, "\033%%5%c",
                                        (char) (skip & 0xff));
                skip = 0;

                /* get the rest of the scan lines */
                code = gdev_prn_copy_scan_lines(pdev, lnum + 1,
                        in + line_size, (bits_per_column - 1) * line_size);

                if ( code < 0 )
                        goto xit;

                lcnt = code + 1; /* since we already grabbed one line */

                if ( lcnt < bits_per_column )
                        memset(in + lcnt * line_size, 0,
                                        (bits_per_column - lcnt) * line_size);

                if (highRes) {
                        oki_transpose(in, out1, pdev->width, 2 * line_size);
                        oki_transpose(in + line_size, out2,
                                                pdev->width, 2 * line_size);
                } else
                        oki_transpose(in, out1, pdev->width, line_size);

                out3 = oki_compress(out1, pdev->width, highRes,
                                                &spaces, &width);

                for (i=0; i < spaces; i++)
                        gp_fputc(' ', prn_stream);

                gp_fwrite("\003", 1, 1, prn_stream);
                gp_fwrite(out3, 1, width, prn_stream);

                if (highRes) {
                        /* exit graphics; carriage return; 1 bit line feed */
                        gp_fprintf(prn_stream, "\003\002\015\033%%5%c", (char) 1);
                        out3 = oki_compress(out2, pdev->width, highRes,
                                                        &spaces, &width);
                        for (i=0; i < spaces; i++)
                                gp_fputc(' ', prn_stream);
                        gp_fwrite("\003", 1, 1, prn_stream);
                        gp_fwrite(out3, 1, width, prn_stream);
                        gp_fprintf(prn_stream, "\003\002\015\033%%5%c", (char) 13);
                } else
                        gp_fwrite("\003\016\003\002", 1, 4, prn_stream);

                lnum += bits_per_column;
           }

        /* Eject the page */
xit:
        gp_fputc(014, prn_stream);	/* form feed */
        gp_fflush(prn_stream);

bail:
        if ( out1 != 0 )
                gs_free(pdev->memory, (char *)out1, 8, line_size, "oki_print_page(out1)");

        if ( out2 != 0 )
                gs_free(pdev->memory, (char *)out2, 8, line_size, "oki_print_page(out2)");

        if ( in != 0 )
                gs_free(pdev->memory, (char *)in, 16, line_size, "oki_print_page(in)");

        return code;
}
