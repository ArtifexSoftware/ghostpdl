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

/* High-res 24Dot-matrix printer driver */

/* Supported printers
 *  NEC P6 and similar, implemented by Andreas Schwab (schwab@ls5.informatik.uni-dortmund.de)
 *  Epson LQ850, implemented by Christian Felsch (felsch@tu-harburg.d400.de)
 */

#include "gdevprn.h"

/* Driver for NEC P6 */
static dev_proc_print_page (necp6_print_page);
const gx_device_printer far_data gs_necp6_device =
  prn_device (prn_bg_procs, "necp6",	/* The print_page proc is compatible with allowing bg printing */
              DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
              360, 360,
              0, 0, 0.5, 0,	/* margins */
              1, necp6_print_page);

/* Driver for Epson LQ850 */
/* I've tested this driver on a BJ300 with LQ850 emulation and there it produce correct 360x360dpi output. */
static dev_proc_print_page (lq850_print_page);
const gx_device_printer gs_lq850_device =
  prn_device (prn_bg_procs, "lq850",	/* The print_page proc is compatible with allowing bg printing */
              DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
              360, 360,
              0, 0, 0.5, 0,	/* margins */
              1, lq850_print_page);

/* ------ Internal routines ------ */

/* Forward references */
static void dot24_output_run (byte *, int, int, gp_file *);
static void dot24_improve_bitmap (byte *, int);

/* Send the page to the printer. */
static int
dot24_print_page (gx_device_printer *pdev, gp_file *prn_stream, char *init_string, int init_len)
{
  int xres;
  int yres;
  int x_high;
  int y_high;
  int bits_per_column;
  uint line_size;
  uint in_size;
  byte *in;
  uint out_size;
  byte *out;
  int y_passes;
  int dots_per_space;
  int bytes_per_space;
  int skip = 0, lnum = 0, code = 0, ypass;

  xres = (int)pdev->x_pixels_per_inch;
  yres = (int)pdev->y_pixels_per_inch;
  x_high = (xres == 360);
  y_high = (yres == 360);
  dots_per_space = xres / 10;       /* pica space = 1/10" */
  bytes_per_space = dots_per_space * 3;
  if (bytes_per_space == 0) {
    /* We divide by bytes_per_space later on. */
    return_error(gs_error_rangecheck);
  }

  bits_per_column = (y_high ? 48 : 24);
  line_size = gdev_prn_raster (pdev);
  in_size = line_size * bits_per_column;
  in = (byte *) gs_malloc (pdev->memory, in_size, 1, "dot24_print_page (in)");
  out_size = ((pdev->width + 7) & -8) * 3;
  out = (byte *) gs_malloc (pdev->memory, out_size, 1, "dot24_print_page (out)");
  y_passes = (y_high ? 2 : 1);

  /* Check allocations */
  if (in == 0 || out == 0)
    {
      if (out)
        gs_free (pdev->memory, (char *) out, out_size, 1, "dot24_print_page (out)");
      if (in)
        gs_free (pdev->memory, (char *) in, in_size, 1, "dot24_print_page (in)");
      return_error (gs_error_VMerror);
    }

  /* Initialize the printer and reset the margins. */
  gp_fwrite (init_string, init_len - 1, sizeof (char), prn_stream);
  gp_fputc ((int) (pdev->width / pdev->x_pixels_per_inch * 10) + 2,
         prn_stream);

  /* Print lines of graphics */
  while (lnum < pdev->height)
    {
      byte *inp;
      byte *in_end;
      byte *out_end;
      byte *out_blk;
      register byte *outp;
      int lcnt;

      /* Copy 1 scan line and test for all zero. */
      code = gdev_prn_copy_scan_lines (pdev, lnum, in, line_size);
      if (code < 0)
          goto xit;
      if (in[0] == 0
          && !memcmp ((char *) in, (char *) in + 1, line_size - 1))
        {
          lnum++;
          skip += 2 - y_high;
          continue;
        }

      /* Vertical tab to the appropriate position. */
      while ((skip >> 1) > 255)
        {
          gp_fputs ("\033J\377", prn_stream);
          skip -= 255 * 2;
        }

      if (skip)
        {
          if (skip >> 1)
            gp_fprintf (prn_stream, "\033J%c", skip >> 1);
          if (skip & 1)
            gp_fputc ('\n', prn_stream);
        }

      /* Copy the rest of the scan lines. */
      if (y_high)
        {
          /* NOTE: even though fixed to not ignore return < 0 from gdev_prn_copy_scan_lines, */
          /*       this code seems wrong -- it doesn't seem to match the 'else' method.      */
          inp = in + line_size;
          for (lcnt = 1; lcnt < 24; lcnt++, inp += line_size) {
            code = gdev_prn_copy_scan_lines (pdev, lnum + lcnt * 2, inp, line_size);
            if (code < 0)
                goto xit;
            if (code == 0)
              {
                /* Pad with lines of zeros. */
                memset (inp, 0, (24 - lcnt) * line_size);
                break;
              }
          }
          inp = in + line_size * 24;
          for (lcnt = 0; lcnt < 24; lcnt++, inp += line_size) {
            code = gdev_prn_copy_scan_lines (pdev, lnum + lcnt * 2 + 1, inp, line_size);
            if (code < 0)
                goto xit;
            if (code == 0)
              {
                /* Pad with lines of zeros. */
                memset (inp, 0, (24 - lcnt) * line_size);
                break;
              }
          }
        }
      else
        {
          code = gdev_prn_copy_scan_lines (pdev, lnum + 1, in + line_size,
                                               in_size - line_size);
          if (code < 0)
              goto xit;
          lcnt = code + 1;	/* FIXME: why +1 */
          if (lcnt < 24)
            /* Pad with lines of zeros. */
            memset (in + lcnt * line_size, 0, in_size - lcnt * line_size);
        }

      for (ypass = 0; ypass < y_passes; ypass++)
        {
          out_end = out;
          inp = in;
          if (ypass)
            inp += line_size * 24;
          in_end = inp + line_size;

          for (; inp < in_end; inp++, out_end += 24)
            {
              memflip8x8 (inp, line_size, out_end, 3);
              memflip8x8 (inp + line_size * 8, line_size, out_end + 1, 3);
              memflip8x8 (inp + line_size * 16, line_size, out_end + 2, 3);
            }
          /* Remove trailing 0s. */
          while (out_end - 3 >= out && out_end[-1] == 0
                 && out_end[-2] == 0 && out_end[-3] == 0)
            out_end -= 3;

          for (out_blk = outp = out; outp < out_end;)
            {
              /* Skip a run of leading 0s. */
              /* At least 10 are needed to make tabbing worth it. */

              if (outp[0] == 0 && outp + 12 <= out_end
                  && outp[1] == 0 && outp[2] == 0
                  && outp[3] == 0 && outp[4] == 0 && outp[5] == 0
                  && outp[6] == 0 && outp[7] == 0 && outp[8] == 0
                  && outp[9] == 0 && outp[10] == 0 && outp[11] == 0)
                {
                  byte *zp = outp;
                  int tpos;
                  byte *newp;
                  outp += 12;
                  while (outp + 3 <= out_end
                         && outp[0] == 0 && outp[1] == 0 && outp[2] == 0)
                    outp += 3;
                  tpos = (outp - out) / bytes_per_space;
                  newp = out + tpos * bytes_per_space;
                  if (newp > zp + 10)
                    {
                      /* Output preceding bit data. */
                      /* only false at beginning of line */
                      if (zp > out_blk)
                        {
                          if (x_high)
                            dot24_improve_bitmap (out_blk, (int) (zp - out_blk));
                          dot24_output_run (out_blk, (int) (zp - out_blk),
                                          x_high, prn_stream);
                        }
                      /* Tab over to the appropriate position. */
                      gp_fprintf (prn_stream, "\033D%c%c\t", tpos, 0);
                      out_blk = outp = newp;
                    }
                }
              else
                outp += 3;
            }
          if (outp > out_blk)
            {
              if (x_high)
                dot24_improve_bitmap (out_blk, (int) (outp - out_blk));
              dot24_output_run (out_blk, (int) (outp - out_blk), x_high,
                              prn_stream);
            }

          gp_fputc ('\r', prn_stream);
          if (ypass < y_passes - 1)
            gp_fputc ('\n', prn_stream);
        }
      skip = 48 - y_high;
      lnum += bits_per_column;
    }

  /* Eject the page and reinitialize the printer */
  gp_fputs ("\f\033@", prn_stream);
  gp_fflush (prn_stream);

xit:
  gs_free (pdev->memory, (char *) out, out_size, 1, "dot24_print_page (out)");
  gs_free (pdev->memory, (char *) in, in_size, 1, "dot24_print_page (in)");

  return code;
}

/* Output a single graphics command. */
static void
dot24_output_run (byte *data, int count, int x_high, gp_file *prn_stream)
{
  int xcount = count / 3;
  gp_fputc (033, prn_stream);
  gp_fputc ('*', prn_stream);
  gp_fputc ((x_high ? 40 : 39), prn_stream);
  gp_fputc (xcount & 0xff, prn_stream);
  gp_fputc (xcount >> 8, prn_stream);
  gp_fwrite (data, 1, count, prn_stream);
}

/* If xdpi == 360, the P6 / LQ850 cannot print adjacent pixels.  Clear the
   second last pixel of every run of set pixels, so that the last pixel
   is always printed.  */
static void
dot24_improve_bitmap (byte *data, int count)
{
  int i;
  register byte *p = data + 6;

      for (i = 6; i < count; i += 3, p += 3)
        {
          p[-6] &= ~(~p[0] & p[-3]);
          p[-5] &= ~(~p[1] & p[-2]);
          p[-4] &= ~(~p[2] & p[-1]);
        }
      p[-6] &= ~p[-3];
      p[-5] &= ~p[-2];
      p[-4] &= ~p[-1];

}

static int
necp6_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
  char necp6_init_string [] = "\033@\033P\033l\000\r\034\063\001\033Q";

  return dot24_print_page(pdev, prn_stream, necp6_init_string, sizeof(necp6_init_string));
}

static int
lq850_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
  char lq850_init_string [] = "\033@\033P\033l\000\r\033\053\001\033Q";

  return dot24_print_page(pdev, prn_stream, lq850_init_string, sizeof(lq850_init_string));
}
