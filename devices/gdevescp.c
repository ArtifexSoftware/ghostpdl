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
 * Epson 'ESC/P 2' language printer driver.
 *
 * This driver uses the ESC/P2 language raster graphics commands with
 * compression. The driver skips vertical white space, provided that
 * the white space is >= 24/band_size (<~ 1.7mm @ 360dpi!) high. There
 * is no attempt to skip horizontal white space, but the compression
 * greatly reduces the significance of this (a nearly blank line would
 * take about 45 bytes). The driver compresses the data one scan line at
 * a time, even though this is not enforced by the hardware. The reason
 * I have done this is that, since the driver skips data outside the
 * margins, we would have to set up a extra pointers to keep track of
 * the data from the previous scan line. Doing this would add extra
 * complexity at a small saving of disk space.
 *
 * These are the only possible optimisations that remain, and would
 * greatly increase the complexity of the driver. At this point, I don't
 * consider them necessary, but I might consider implementing them if
 * enough people encourage me to do so.
 *
 * Richard Brown (rab@tauon.ph.unimelb.edu.au)
 *
 */

#include "gdevprn.h"

/*
 * Valid values for X_DPI and Y_DPI: 180, 360
 *
 * The value specified at compile time is the default value used if the
 * user does not specify a resolution at runtime.
 */
#ifndef X_DPI
#  define X_DPI 360
#endif

#ifndef Y_DPI
#  define Y_DPI 360
#endif

/*
 * Margin definitions: Stylus 800 printer driver:
 *
 * The commented margins are from the User's Manual.
 *
 * The values actually used here are more accurate for my printer.
 * The Stylus paper handling is quite sensitive to these settings.
 * If you find that the printer uses an extra page after every real
 * page, you'll need to increase the top and/or bottom margin.
 */

#define STYLUS_L_MARGIN 0.13	/*0.12*/
#define STYLUS_B_MARGIN 0.56	/*0.51*/
#define STYLUS_T_MARGIN 0.34	/*0.12*/
#ifdef A4
#   define STYLUS_R_MARGIN 0.18 /*0.15*/
#else
#   define STYLUS_R_MARGIN 0.38
#endif

/*
 * Epson AP3250 Margins:
 */

#define AP3250_L_MARGIN 0.18
#define AP3250_B_MARGIN 0.51
#define AP3250_T_MARGIN 0.34
#define AP3250_R_MARGIN 0.28  /* US paper */

/* The device descriptor */
static dev_proc_print_page(escp2_print_page);

/* Stylus 800 device */
const gx_device_printer far_data gs_st800_device =
  prn_device(prn_bg_procs, "st800",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS,
        DEFAULT_HEIGHT_10THS,
        X_DPI, Y_DPI,
        STYLUS_L_MARGIN, STYLUS_B_MARGIN, STYLUS_R_MARGIN, STYLUS_T_MARGIN,
        1, escp2_print_page);

/* AP3250 device */
const gx_device_printer far_data gs_ap3250_device =
  prn_device(prn_bg_procs, "ap3250",	/* The print_page proc is compatible with allowing bg printing */
        DEFAULT_WIDTH_10THS,
        DEFAULT_HEIGHT_10THS,
        X_DPI, Y_DPI,
        AP3250_L_MARGIN, AP3250_B_MARGIN, AP3250_R_MARGIN, AP3250_T_MARGIN,
        1, escp2_print_page);

/* ------ Internal routines ------ */

/* Send the page to the printer. */
static int
escp2_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
        int line_size = gdev_prn_raster((gx_device_printer *)pdev);
        int band_size = 24;	/* 1, 8, or 24 */
        int in_size = line_size * band_size;

        byte *buf1 = (byte *)gs_malloc(pdev->memory, in_size, 1, "escp2_print_page(buf1)");
        byte *buf2 = (byte *)gs_malloc(pdev->memory, in_size, 1, "escp2_print_page(buf2)");
        byte *in = buf1;
        byte *out = buf2;

        int skip, lnum, top, bottom, left, width;
        int code = 0, count, i;

        /*
        ** Check for valid resolution:
        **
        **	XDPI	YDPI
        **	360	360
        **	360	180
        **	180	180
        */

        if( !( (pdev->x_pixels_per_inch == 180 &&
                pdev->y_pixels_per_inch == 180) ||
               (pdev->x_pixels_per_inch == 360 &&
               (pdev->y_pixels_per_inch == 360 ||
                pdev->y_pixels_per_inch == 180) )) )
                   return_error(gs_error_rangecheck);

        /*
        ** Check buffer allocations:
        */

        if ( buf1 == 0 || buf2 == 0 ) {
           code = gs_error_VMerror;
           goto xit;
        }

        /*
        ** Reset printer, enter graphics mode:
        */

        gp_fwrite("\033@\033(G\001\000\001", 1, 8, prn_stream);

#ifdef A4
        /*
        ** After reset, the Stylus is set up for US letter paper.
        ** We need to set the page size appropriately for A4 paper.
        ** For some bizarre reason the ESC/P2 language wants the bottom
        ** margin measured from the *top* of the page:
        */

        fwrite("\033(U\001\0\n\033(C\002\0t\020\033(c\004\0\0\0t\020",
                                                        1, 22, prn_stream);
#endif

        /*
        ** Set the line spacing to match the band height:
        */

        if( pdev->y_pixels_per_inch == 360 )
           gp_fwrite("\033(U\001\0\012\033+\030", 1, 9, prn_stream);
        else
           gp_fwrite("\033(U\001\0\024\033+\060", 1, 9, prn_stream);

        /*
        ** If the printer has automatic page feeding, then the paper
        ** will already be positioned at the top margin value, so we
        ** start printing the image from there. Similarly, we must not
        ** try to print or even line feed past the bottom margin, since
        ** the printer will automatically load a new page.
        ** Printers without this feature may actually need to be told
        ** to skip past the top margin.
        */

        /* auto_feed was set to 1 and never altered, meaning the test here
         * was irrelvant and led to dead code. Removed the code and 'auto_feed'.
         */
       top = (int)(dev_t_margin(pdev) * pdev->y_pixels_per_inch);
       bottom = (int)(pdev->height -
                    dev_b_margin(pdev) * pdev->y_pixels_per_inch);

        /*
        ** Make left margin and width sit on byte boundaries:
        */

        left  = ( (int) (dev_l_margin(pdev) * pdev->x_pixels_per_inch) ) >> 3;

        width = ((pdev->width - (int)(dev_r_margin(pdev) * pdev->x_pixels_per_inch)) >> 3) - left;

        /*
        ** Print the page:
        */

        for ( lnum = top, skip = 0 ; lnum < bottom ; )
        {
                byte *in_data;
                byte *inp;
                byte *in_end;
                byte *outp;
                register byte *p, *q;
                int lcnt;

                /*
                ** Check buffer for 0 data. We can't do this mid-band
                */

                code = gdev_prn_get_bits(pdev, lnum, in, &in_data);
                if (code < 0)
                   goto xit;
                while ( in_data[0] == 0 &&
                        !memcmp((char *)in_data, (char *)in_data + 1, line_size - 1) &&
                        lnum < bottom )
                {
                        lnum++;
                        skip++;
                        code = gdev_prn_get_bits(pdev, lnum, in, &in_data);
                        if (code < 0)
                            goto xit;
                }

                if(lnum == bottom ) break;	/* finished with this page */

                /*
                ** Skip blank lines if we need to:
                */

                if( skip ) {
                   gp_fwrite("\033(v\002\000", 1, 5, prn_stream);
                   gp_fputc(skip & 0xff, prn_stream);
                   gp_fputc(skip >> 8,   prn_stream);
                   skip = 0;
                }

                code = lcnt = gdev_prn_copy_scan_lines(pdev, lnum, in, in_size);
                if (lcnt < 0)
                    goto xit;

                /*
                ** Check to see if we don't have enough data to fill an entire
                ** band. Padding here seems to work (the printer doesn't jump
                ** to the next (blank) page), although the ideal behaviour
                ** would probably be to reduce the band height.
                **
                ** Pad with nulls:
                */

                if( lcnt < band_size )
                   memset(in + lcnt * line_size, 0, in_size - lcnt * line_size);

                /*
                ** Now we have a band of data: try to compress it:
                */

                for( outp = out, i = 0 ; i < band_size ; i++ ) {

                   /*
                   ** Take margins into account:
                   */

                   inp = in + i * line_size + left;
                   in_end = inp + width;

                   /*
                   ** walk through input buffer, looking for repeated data:
                   ** Since we need more than 2 repeats to make the compression
                   ** worth it, we can compare pairs, since it doesn't matter if we
                   **
                   */

                   for( p = inp, q = inp + 1 ; q < in_end ; ) {

                        if( *p != *q ) {

                           p += 2;
                           q += 2;

                        } else {

                           /*
                           ** Check behind us, just in case:
                           */

                           if( p > inp && *p == *(p-1) )
                              p--;

                           /*
                           ** walk forward, looking for matches:
                           */

                           for( q++ ; *q == *p && q < in_end ; q++ ) {
                              if( (q-p) >= 128 ) {
                                 if( p > inp ) {
                                    count = p - inp;
                                    while( count > 128 ) {
                                       *outp++ = '\177';
                                       memcpy(outp, inp, 128);	/* data */
                                       inp += 128;
                                       outp += 128;
                                       count -= 128;
                                    }
                                    *outp++ = (char) (count - 1); /* count */
                                    memcpy(outp, inp, count);	/* data */
                                    outp += count;
                                 }
                                 *outp++ = '\201';	/* Repeat 128 times */
                                 *outp++ = *p;
                                 p += 128;
                                 inp = p;
                              }
                           }

                           if( (q - p) > 2 ) {	/* output this sequence */
                              if( p > inp ) {
                                 count = p - inp;
                                 while( count > 128 ) {
                                    *outp++ = '\177';
                                    memcpy(outp, inp, 128);	/* data */
                                    inp += 128;
                                    outp += 128;
                                    count -= 128;
                                 }
                                 *outp++ = (char) (count - 1);	/* byte count */
                                 memcpy(outp, inp, count);	/* data */
                                 outp += count;
                              }
                              count = q - p;
                              *outp++ = (char) (256 - count + 1);
                              *outp++ = *p;
                              p += count;
                              inp = p;
                           } else	/* add to non-repeating data list */
                              p = q;
                           if( q < in_end )
                              q++;
                        }
                   }

                   /*
                   ** copy remaining part of line:
                   */

                   if( inp < in_end ) {

                      count = in_end - inp;

                      /*
                      ** If we've had a long run of varying data followed by a
                      ** sequence of repeated data and then hit the end of line,
                      ** it's possible to get data counts > 128.
                      */

                      while( count > 128 ) {
                        *outp++ = '\177';
                        memcpy(outp, inp, 128);	/* data */
                        inp += 128;
                        outp += 128;
                        count -= 128;
                      }

                      *outp++ = (char) (count - 1);	/* byte count */
                      memcpy(outp, inp, count);	/* data */
                      outp += count;
                   }
                }

                /*
                ** Output data:
                */

                gp_fwrite("\033.\001", 1, 3, prn_stream);

                if(pdev->y_pixels_per_inch == 360)
                   gp_fputc('\012', prn_stream);
                else
                   gp_fputc('\024', prn_stream);

                if(pdev->x_pixels_per_inch == 360)
                   gp_fputc('\012', prn_stream);
                else
                   gp_fputc('\024', prn_stream);

                gp_fputc(band_size, prn_stream);

                gp_fputc((width << 3) & 0xff, prn_stream);
                gp_fputc( width >> 5,         prn_stream);

                gp_fwrite(out, 1, (outp - out), prn_stream);

                gp_fwrite("\r\n", 1, 2, prn_stream);
                lnum += band_size;
        }

        /* Eject the page and reinitialize the printer */

        gp_fputs("\f\033@", prn_stream);
        gp_fflush(prn_stream);

xit:
	if ( buf1 )
            gs_free(pdev->memory, (char *)buf1, in_size, 1, "escp2_print_page(buf1)");
        if ( buf2 )
            gs_free(pdev->memory, (char *)buf2, in_size, 1, "escp2_print_page(buf2)");
        if (code < 0)
           return_error(code);

        return code;
}
