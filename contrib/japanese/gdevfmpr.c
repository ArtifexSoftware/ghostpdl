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

/* gdevfmpr.c */
/* Fujitsu FMPR printer driver for Ghostscript */

/* This derives from "gdevp201.c" programed by Norio KATAYAMA. */

#include "gdevprn.h"

static dev_proc_print_page(fmpr_print_page);

#define A4

/* The device descriptor */
gx_device_printer gs_fmpr_device =
  prn_device(gdev_prn_initialize_device_procs_mono, "fmpr",
             DEFAULT_WIDTH_10THS,
             DEFAULT_HEIGHT_10THS,
             180,		/* x_dpi */
             180,		/* y_dpi */
             0, /* 0.35, */	/* left margins */
             0,			/* bottom margins */
             0, /* 0.84, */	/* top margins */
             0,			/* right margins */
             1, fmpr_print_page);

/* ---- Printer output routines ---- */

static int
prn_putc(gx_device_printer *pdev, int c)
{
  return gp_fputc(c, pdev->file);
}

static int
prn_puts(gx_device_printer *pdev, const char *ptr)
{
  return gp_fputs(ptr, pdev->file);
}

static int
prn_write(gx_device_printer *pdev, const char *ptr, int size)
{
  return gp_fwrite(ptr, 1, size, pdev->file);
}

static void
prn_flush(gx_device_printer *pdev)
{
  gp_fflush(pdev->file);
}

/* ------ internal routines ------ */

/* Transpose a block of 8x8 bits */
static int
fmpr_transpose_8x8(byte *src, int src_step, byte *dst, int dst_step)
{
  byte mask, s, d0, d1, d2, d3, d4, d5, d6, d7;
  int i;

  d0 = d1 = d2 = d3 = d4 = d5 = d6 = d7 = 0;

  for(i=0, mask=0x80; i<8; i++, mask >>= 1) {
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
fmpr_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
  int line_size = gdev_prn_raster(pdev);
  int height = pdev->height;
  int bits_per_column = 24;
  int bytes_per_column = bits_per_column / 8;
  int chunk_size = bits_per_column * line_size;
  byte *in, *out;
  int lnum;
  char prn_buf[16];

  in = (byte *)
    gs_malloc(pdev->memory->non_gc_memory, bits_per_column, line_size, "fmpr_print_page(in)");
  out = (byte *)
    gs_malloc(pdev->memory->non_gc_memory, bits_per_column, line_size, "fmpr_print_page(out)");
  if(in == 0 || out == 0)
    return -1;

  /* Initialize printer */
  prn_puts(pdev, "\033c");
  prn_puts(pdev, "\033Q1 `\033[24;18 G");

  /* Send Data to printer */
  lnum = 0;
  while(lnum < height) {
    byte *inp, *outp, *out_beg, *out_end;
    int x, y, num_lines, size, mod;

    if(gdev_prn_copy_scan_lines(pdev, lnum, in, chunk_size) < 0)
      break;

    if((num_lines = height - lnum) > bits_per_column)
      num_lines = bits_per_column;

    size = line_size * num_lines;
    if(in[0] == 0 &&
       !memcmp((char *)in, (char *)in + 1, size - 1)) {
      lnum += bits_per_column;
      prn_putc(pdev, '\n');
      continue;
    }

    if(num_lines < bits_per_column) {
      size = line_size * (bits_per_column - num_lines);
      memset(in + line_size * num_lines, 0, size);
    }
    lnum += bits_per_column;

    for(y = 0; y < bytes_per_column; y ++) {
      inp = in + line_size * 8 * y;
      outp = out + y;
      for(x = 0; x < line_size; x ++) {
        fmpr_transpose_8x8(inp, line_size,
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

    gs_snprintf(prn_buf, sizeof(prn_buf), "\033[%da",
            (out_beg - out) / bytes_per_column);
    prn_puts(pdev, prn_buf);

    /* Dot graphics */
    size = out_end - out_beg + 1;
    gs_snprintf(prn_buf, sizeof(prn_buf), "\033Q%d W", size / bytes_per_column);
    prn_puts(pdev, prn_buf);
    prn_write(pdev, (const char *)out_beg, size);

    prn_putc(pdev, '\n');
  }

  /* Form Feed */
  prn_putc(pdev, '\f');
  prn_flush(pdev);

  gs_free(pdev->memory->non_gc_memory, (char *)out,
          bits_per_column, line_size, "fmpr_print_page(out)");
  gs_free(pdev->memory->non_gc_memory, (char *)in,
          bits_per_column, line_size, "fmpr_print_page(in)");

  return 0;
}
