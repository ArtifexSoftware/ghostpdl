/* Copyright (C) 1991 Aladdin Enterprises.  All rights reserved.
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

/* gdevfmlbp.c */
/* Fujitsu FMLBP2xx driver for Ghostscript */
/*
 * Based on LBP-8II driver(gdevlbp8.c) and ESC/Page driver(gdevepag.c).
 * Modified for FMLBP by Kazushige Goto <goto@statabo.rim.or.jp> and
 * Shoichi Nakayama <naka@fadev.fvd.fujitsu.co.jp>.
 *
 *  ver 0.9 1997/03/08 goto@statabo.rim.or.jp
 *                     -- Initial release.
 *  ver 1.0 1997/12/24 naka@fadev.fvd.fujitsu.co.jp
 *                     -- Fixed bug.
 *  ver 1.1 1998/03/12 naka@fadev.fvd.fujitsu.co.jp
 *                     -- Added 240dpi and paper size support.
 *  ver 1.2 1998/03/30 naka@fadev.fvd.fujitsu.co.jp
 *                     -- Added adjust margin for gs261.
 */
#include "gdevprn.h"

#define X_DPI 400
#define Y_DPI 400

/* The device descriptors */
static dev_proc_print_page(fmlbp_print_page);

#ifdef	FMLBP_NOADJUST_MARGIN
#define	PRNFMLBP	prn_std_procs
#else
/* Adjust margin for ghostscript 2.6.1 */
#define	PRNFMLBP	prn_fmlbp_procs
static dev_proc_get_initial_matrix(fmlbp_get_initial_matrix);
gx_device_procs prn_fmlbp_procs =
  prn_matrix_procs(gdev_prn_open, fmlbp_get_initial_matrix,
    gdev_prn_output_page, gdev_prn_close);

/* Shift the origin from the top left corner of the pysical page to the
   first printable pixel, as defined by the top and left margins. */
/* modified from gdevsppr.c. */
static void
fmlbp_get_initial_matrix(gx_device *dev, gs_matrix *pmat)
{ gx_default_get_initial_matrix(dev, pmat);
  pmat->tx -= (dev->l_margin * dev->x_pixels_per_inch);
  pmat->ty -= (dev->t_margin * dev->y_pixels_per_inch);
}
#endif/*FMLBP_NOADJUST_MARGIN*/

gx_device_printer gs_fmlbp_device =
  prn_device(PRNFMLBP, "fmlbp",
        DEFAULT_WIDTH_10THS_A4,		/* width_10ths, 8.3" */
        DEFAULT_HEIGHT_10THS_A4,	/* height_10ths, 11.7" */
        X_DPI, Y_DPI,
        0.20, 0.35, 0.21, 0.20,		/* left,bottom,right,top margins */
        1, fmlbp_print_page);

/* ------ Internal routines ------ */

#define ESC 0x1b
#define CEX 0x1c
#define CSI 0x1b,0x5b
#define PU1 0x1b,0x51

static char can_inits[] ={ ESC, 'c',              /* Software reset */
#ifdef	OLD_FMLBP_400DPI
  PU1, '4', '0', '0', '!', 'A'                    /* 400dpi */
/*PU1, '0', ';','4', ';','0', ';','0','!','@' */  /* landscape */
#endif/*OLD_FMLBP_400DPI*/
};

#ifdef	FMLBP_NOPAPERSIZE
#define LINE_SIZE ((4520+7) / 8)	/* bytes per line */
#else
/* Paper sizes */
#define	PAPER_SIZE_A3		"0;3"
#define	PAPER_SIZE_A4		"0;4"
#define	PAPER_SIZE_A5		"0;5"
#define	PAPER_SIZE_B4		"1;4"
#define	PAPER_SIZE_B5		"1;5"
#define	PAPER_SIZE_LEGAL	"2;0"
#define	PAPER_SIZE_LETTER	"3;0"
#define	PAPER_SIZE_HAGAKI	"4;0"

/* Get the paper size code, based on width and height. */
/* modified from gdevpcl.c, gdevmjc.c and gdevnpdl.c. */
static char *
gdev_fmlbp_paper_size(gx_device_printer *dev, char *paper)
{
  int    landscape = 0;	/* portrait */
  float height_inches = dev->height / dev->y_pixels_per_inch;
  float  width_inches = dev->width  / dev->x_pixels_per_inch;

  if (width_inches > height_inches){	/* landscape */
    float t = width_inches;
    width_inches = height_inches;
    height_inches = t;
    landscape = 1;
  }
  gs_sprintf(paper, "%s;%d",
    (height_inches >= 15.9 ? PAPER_SIZE_A3 :
     height_inches >= 11.8 ?
     (width_inches >=  9.2 ? PAPER_SIZE_B4 : PAPER_SIZE_LEGAL) :
     height_inches >= 11.1 ? PAPER_SIZE_A4 :
     height_inches >= 10.4 ? PAPER_SIZE_LETTER :
     height_inches >=  9.2 ? PAPER_SIZE_B5 :
     height_inches >=  7.6 ? PAPER_SIZE_A5 : PAPER_SIZE_HAGAKI), landscape);
#ifdef	FMLBP_DEBUG
  dprintf5("w=%d(%f) x h=%d(%f) -> %s\n",
           dev->width, width_inches, dev->height, height_inches, paper);
#endif/*FMLBP_DEBUG*/
  return paper;
}
#endif/*FMLBP_NOPAPERSIZE*/

/* move down and move across */
static void goto_xy(gp_file *prn_stream,int x,int y)
  {
    unsigned char buff[20];
    unsigned char *p=buff;

    gp_fputc(CEX,prn_stream);
    gp_fputc('"',prn_stream);
    gs_sprintf((char *)buff,"%d",x);
    while (*p)
      {
        if (!*(p+1)) gp_fputc((*p)+0x30,prn_stream);
        else
          gp_fputc((*p)-0x10,prn_stream);
        p++;
      }

    p=buff;
    gs_sprintf((char *)buff,"%d",y);
    while (*p)
      {
        if (!*(p+1)) gp_fputc((*p)+0x40,prn_stream);
        else
          gp_fputc((*p)-0x10,prn_stream);
        p++;
      }
  }

/* Send the page to the printer.  */
static int
fmlbp_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
  int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
#ifdef	FMLBP_NOPAPERSIZE
  char data[LINE_SIZE*2];
#else
  char paper[16];
  byte *data = (byte *)gs_malloc(pdev->memory->non_gc_memory, 1, line_size, "fmlpr_print_page(data)");
  if(data == NULL) return_error(gs_error_VMerror);
#endif/*FMLBP_NOPAPERSIZE*/

  /* initialize */
  gp_fwrite(can_inits, sizeof(can_inits), 1, prn_stream);
  gp_fprintf(prn_stream, "%c%c%d!I", PU1, 0);	/* 100% */
#ifndef	OLD_FMLBP_400DPI
  gp_fprintf(prn_stream, "%c%c%d!A", PU1,
          (int)(pdev->x_pixels_per_inch));	/* 240dpi or 400dpi */
#endif/*!OLD_FMLBP_400DPI*/
#ifndef	FMLBP_NOPAPERSIZE
  gp_fprintf(prn_stream, "%c%c%s!F", PU1,
          gdev_fmlbp_paper_size(pdev, paper));		/* Paper size */
#endif/*!FMLBP_NOPAPERSIZE*/

  /* Send each scan line in turn */
  {	int lnum;
        byte rmask = (byte)(0xff << (-pdev->width & 7));

        for ( lnum = 0; lnum < pdev->height; lnum++ )
          {	byte *end_data = data + line_size;
                int s = gdev_prn_copy_scan_lines(pdev, lnum,
                                                (byte *)data, line_size);
                if(s < 0) return_error(s);
                /* Mask off 1-bits beyond the line width. */
                end_data[-1] &= rmask;
                /* Remove trailing 0s. */
                while ( end_data > data && end_data[-1] == 0 )
                  end_data--;
                if ( end_data != data ) {
                  int num_cols = 0;
                  int out_count = 0;
                  byte *out_data = data;

                  while(out_data < end_data && *out_data == 0) {
                    num_cols += 8;
                    out_data++;
                  }
                  out_count = end_data - out_data;

                  /* move down */ /* move across */
                  goto_xy(prn_stream, num_cols, lnum);

                  /* transfer raster graphics */
                  gp_fprintf(prn_stream, "%c%c%d;%d;0!a",
                             PU1, out_count, out_count*8 );

                  /* send the row */
                  gp_fwrite(out_data, sizeof(byte), out_count, prn_stream);
                }
              }
      }
  /* eject page */
  gp_fputc(0x0c,prn_stream);
  gp_fflush(prn_stream);
#ifndef	FMLBP_NOPAPERSIZE
  gs_free(pdev->memory->non_gc_memory, (char *)data, line_size, sizeof(byte), "fmlbp_print_page(data)");
#endif/*!FMLBP_NOPAPERSIZE*/

  return 0;
}
