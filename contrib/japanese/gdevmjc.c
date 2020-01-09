/* Copyright (C) 1991, 1992, 1993 Aladdin Enterprises.  All rights reserved.

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

/* gdevmjc.c */
/* Many parts of this file are copied from gdevcdj.c and gdevescp.c */

/* EPSON MJ-700V2C colour printer drivers for Ghostscript */

/*
   These drivers may also work with EPSON Stylus color printer,
   though the author hasn't tried.
*/

/*
   Any comments, suggestions, and bug reports are welcomed.
   The e-mail address of the author Kubo, Hiroshi is

   h-kubo@kuee.kyoto-u.ac.jp

           or

   JBD02172@niftyserve.or.jp

*/

/*

  Modified by Norihito Ohmori.
   Support -r / -dBitsPerPixel  option.
   Change -dMicroweave / -dUnidirectional option.

 */

#include "std.h"                /* to stop stdlib.h redefining types */
#include <stdlib.h>		/* for rand() */
#include <limits.h>
#include "gdevprn.h"
#include "gdevpcl.h"
#include "gdevmjc.h"

/***
 *** Note: Original driver gdevcdj.c for HP color printer was written
 ***       by a user,  George Cameron.
 ***
 ***       An idea of Kuniyoshi Yoshio to borrow the codes of gdevcdj.c
 ***       for another color printer BJC-600J inspired the author.
 ***
 ***       Basic control sequences and compression algorithm for ESC/P
 ***       V2 printer are taken from gdevescp.c, written by Richard Brown.
 ***
 ***       The author Kubo, Hiroshi gathered necessary codes for EPSON
 ***       MJ-700V2C and Sylus color printer in gdevmjc.c.
 ***/

/*
  available drivers

  1. mj700v2c         EPSON Mach Jet Color, for ALL CMYK printer

  2. mj500c           EPSON Mach Jet Color,
                       CMY or K printer: MJ-500C, MJ-510C,
                       CMY + K printer:  MJ-800C (for plane paper),
                                         MJ-3000C (for plane paper)
  3. mj6000c          EPSON MJ-830C, MJ-930C, MJ-6000C
                       Use "High Performance Monochrome Mode"
  4. mj8000c          EPSON MJ-8000C
                       Use "High Performance Monochrome Mode"
                       A2 Paper Size Margin is different from mj6000c
*/

/*
 **  Options  **
 *
 name		type	description

 Density	int	Controls densty of dots. 1024 is normal setting
                        for 360dpi.
 Cyan		int	Controls depth of Cyan. 1024 is normal setting.
 Magenta	int	Controls depth of Magenta. 1024 is normal setting.
 Yellow		int	Controls depth of Yellow. 1024 is normal setting.
 Black		int	Controls depth of Black. 1024 is normal setting.
 Unidirectional	bool	if true, Force unidirectional printing.
 Microweave	bool	if true, set micro weave mode.
 DotSize	int	Controls the size of the dots. 0 means default,
                        1 means small, 2 means normal.

 **   Examples   **

 % gs -sDEVICE=mj700v2c -sOutputFile=tiger.mj gdevmjc.ps tiger.ps

 % gs -sDEVICE=mj700v2c -dDensity=1152 -dCyan=1000 -dMagenta=896 \
   -dYellow=1024 -dBlack=512 -dUnidirectional -sOutputFile=tiger.mj \
   gdevmjc.ps tiger.ps

 */

#define MJ700V2C_PRINT_LIMIT 0.34       /* taken from gdevescp.c */

/* Margins are left, bottom, right, top. */
/* left bottom right top */
#define MJ700V2C_MARGINS_A4     0.118f, 0.52f, 0.118f, 0.33465f
#define MJ6000C_MARGINS_A2      0.948f, 0.52f, 1.969f, 0.33465f
#define MJ8000C_MARGINS_A2      0.194f, 0.52f, 0.194f, 0.33465f

/* Define bits-per-pixel for generic drivers - default is 24-bit mode */
#ifndef BITSPERPIXEL
#define BITSPERPIXEL 32
#endif

#define W sizeof(word)
#define I sizeof(int)

/* Printer types */
#define MJ700V2C 1
#define MJ500C 2
#define MJ6000C 3
#define MJ8000C 4

/* No. of ink jets (used to minimise head movements) */
#define HEAD_ROWS_MONO 50
#define HEAD_ROWS_COLOUR 16
/* only for mj700v2c */
#define MJ_HEAD_ROWS_MONO 64
#define MJ_HEAD_ROWS_COLOUR 16

/* Colour mapping procedures */
static dev_proc_map_rgb_color (gdev_mjc_map_rgb_color);
static dev_proc_map_color_rgb (gdev_mjc_map_color_rgb);
static dev_proc_encode_color(gdev_mjc_encode_color);
static dev_proc_decode_color(gdev_mjc_decode_color);

/* Print-page, properties and miscellaneous procedures */
static dev_proc_open_device(mj700v2c_open);
static dev_proc_open_device(mj500c_open);
static dev_proc_open_device(mj6000c_open);
static dev_proc_open_device(mj8000c_open);
static dev_proc_print_page(mj700v2c_print_page);
static dev_proc_print_page(mj500c_print_page);
static dev_proc_print_page(mj6000c_print_page);
static dev_proc_print_page(mj8000c_print_page);

static dev_proc_get_params(mj_get_params);
static dev_proc_put_params(mj700v2c_put_params);
static dev_proc_put_params(mj500c_put_params);

static int mj_open(gx_device * pdev, int ptype);
static int mj_put_params(gx_device * pdev, gs_param_list * plist, int ptype);
static int mj_print_page(gx_device_printer * pdev, gp_file * prn_stream, int ptype);
static void expand_line(word *, int, int, int);
static int put_param_int(gs_param_list *, gs_param_name, int *, int, int, int);
static void set_bpp(gx_device *, int);
static void mj500c_set_bpp(gx_device *, int);
static gx_color_index mjc_correct_color(gx_device_printer *, gx_color_index);

/* The device descriptors */
struct gx_device_mj_s {
        gx_device_common;
        gx_prn_device_common;
        int colorcomp;    /* 1: grayscale 3: CMY 4: CMYK */
        int density;	/* (color depth) * density/1024  = otuput */
        int cyan;       /* weight for cyan */
        int magenta;     /* weight for magenta */
        int yellow;      /* weight for yellow */
        int black;      /* weight for black */
        bool direction;   /* direction of the head. 1: one way 2: both way */
        bool microweave;  /* Micro weave switch. 0: off 1: on */
        int dotsize;      /* dot size switch.
                            0: default, 1: small, 2: normal */
};

typedef struct gx_device_mj_s gx_device_mj;

#define mj    ((gx_device_mj *) pdev)

#define prn_hp_colour_device(procs, dev_name, x_dpi, y_dpi, bpp, print_page)\
    prn_device_body(gx_device_mj, procs, dev_name,\
    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, x_dpi, y_dpi,\
    0.118, 0.52, 0.118, 0.33465,\
    (bpp == 32 ? 4 : (bpp == 1 || bpp == 8) ? 1 : 3), bpp,\
    (bpp >= 8 ? 255 : 1), (bpp >= 8 ? 255 : bpp > 1 ? 1 : 0),\
    (bpp >= 8 ? 5 : 2), (bpp >= 8 ? 5 : bpp > 1 ? 2 : 0),\
    print_page)

#define mjcmyk_device(procs, dev_name, x_dpi, y_dpi, bpp, print_page,\
                  dns, r, g, b, k, drct, mcrwv, dtsz)\
{ prn_hp_colour_device(procs, dev_name, x_dpi, y_dpi, bpp, print_page),\
         (bpp == 32 ? 4 : 1), dns, r, g, b, k, drct, mcrwv, dtsz \
}

#define mjcmy_device(procs, dev_name, x_dpi, y_dpi, bpp, print_page,\
                  dns, r, g, b, k, drct, mcrwv, dtsz)\
{ prn_hp_colour_device(procs, dev_name, x_dpi, y_dpi, bpp, print_page),\
         (bpp == 32 ? 3 : 1), dns, r, g, b, k, drct, mcrwv, dtsz \
}

#define mj_colour_procs(proc_colour_open, proc_get_params, proc_put_params)\
{	proc_colour_open,\
        gx_default_get_initial_matrix,\
        gx_default_sync_output,\
        gdev_prn_output_page,\
        gdev_prn_close,\
        gdev_mjc_map_rgb_color,\
        gdev_mjc_map_color_rgb,\
        NULL,	/* fill_rectangle */\
        NULL,	/* tile_rectangle */\
        NULL,	/* copy_mono */\
        NULL,	/* copy_color */\
        NULL,	/* draw_line */\
        gx_default_get_bits,\
        proc_get_params,\
        proc_put_params,\
        NULL,	/* map_cmyk_color */\
        NULL,	/* get_xfont_procs */\
        NULL,	/* get_xfont_device */\
        NULL,	/* map_rgb_alpha_color */\
        NULL,   /* get_page_device */\
        NULL,	/* get_alpha_bits */\
        NULL,   /* copy_alpha */\
        NULL,	/* get_band */\
        NULL,	/* copy_rop */\
        NULL,	/* fill_path */\
        NULL,	/* stroke_path */\
        NULL,	/* fill_mask */\
        NULL,	/* fill_trapezoid */\
        NULL,	/* fill_parallelogram */\
        NULL,	/* fill_triangle */\
        NULL,	/* draw_thin_line */\
        NULL,	/* begin_image */\
        NULL,	/* image_data */\
        NULL,	/* end_image */\
        NULL,	/* strip_tile_rectangle */\
        NULL,	/* strip_copy_rop, */\
        NULL,	/* get_clipping_box */\
        NULL,	/* begin_typed_image */\
        NULL,	/* get_bits_rectangle */\
        NULL,	/* map_color_rgb_alpha */\
        NULL,	/* create_compositor */\
        NULL,	/* get_hardware_params */\
        NULL,	/* text_begin */\
        NULL,	/* finish_copydevice */\
        NULL, 	/* begin_transparency_group */\
        NULL, 	/* end_transparency_group */\
        NULL, 	/* begin_transparency_mask */\
        NULL, 	/* end_transparency_mask */\
        NULL, 	/* discard_transparency_layer */\
        NULL,   /* get_color_mapping_procs */\
        NULL,   /* get_color_comp_index */\
        gdev_mjc_encode_color,\
        gdev_mjc_decode_color\
}

static gx_device_procs mj700v2c_procs =
mj_colour_procs(mj700v2c_open, mj_get_params, mj700v2c_put_params);

static gx_device_procs mj500c_procs =
mj_colour_procs(mj500c_open, mj_get_params, mj500c_put_params);

static gx_device_procs mj6000c_procs =
mj_colour_procs(mj6000c_open, mj_get_params, mj700v2c_put_params);

static gx_device_procs mj8000c_procs =
mj_colour_procs(mj8000c_open, mj_get_params, mj700v2c_put_params);

gx_device_mj far_data gs_mj700v2c_device =
mjcmyk_device(mj700v2c_procs, "mj700v2c", 360, 360, BITSPERPIXEL,
          mj700v2c_print_page, 1024, 1024, 1024, 1024, 1024, 0, 1, 1);

gx_device_mj far_data gs_mj500c_device =
mjcmy_device(mj500c_procs, "mj500c", 360, 360, BITSPERPIXEL,
          mj500c_print_page, 1024, 1024, 1024, 1024, 1024, 0, 1, 1);

gx_device_mj far_data gs_mj6000c_device =
mjcmyk_device(mj6000c_procs, "mj6000c", 360, 360, BITSPERPIXEL,
          mj6000c_print_page, 1024, 1024, 1024, 1024, 1024, 0, 1, 1);

gx_device_mj far_data gs_mj8000c_device =
mjcmyk_device(mj8000c_procs, "mj8000c", 360, 360, BITSPERPIXEL,
          mj8000c_print_page, 1024, 1024, 1024, 1024, 1024, 0, 1, 1);

/* Get the paper size code, based on width and height. */
static int
gdev_mjc_paper_size(gx_device *dev)
{
  int width = (int)dev->MediaSize[0];
  int height = (int)dev->MediaSize[1];

  if (width == 1190 && height == 1684)
    return PAPER_SIZE_A2;
  else
    return PAPER_SIZE_A4;
}

static int
mj700v2c_open(gx_device * pdev)
{
  return mj_open(pdev, MJ700V2C);
}

static int
mj500c_open(gx_device * pdev)
{
  return mj_open(pdev, MJ700V2C);
}

static int
mj6000c_open(gx_device * pdev)
{
  return mj_open(pdev, MJ700V2C);
}

static int
mj8000c_open(gx_device * pdev)
{
  return mj_open(pdev, MJ700V2C);
}

/* Open the printer and set up the margins. */
static int
mj_open(gx_device *pdev, int ptype)
{       /* Change the margins if necessary. */
  int xdpi = (int)pdev->x_pixels_per_inch;
  int ydpi = (int)pdev->y_pixels_per_inch;

  static const float mj_margin[4] = { MJ700V2C_MARGINS_A4 };
  static const float mj6000c_a2[4] = { MJ6000C_MARGINS_A2 };
  static const float mj8000c_a2[4] = { MJ8000C_MARGINS_A2 };

  const float *m;

  int paper_size;

#if 0
  /* Set up colour params if put_props has not already done so */
  if (pdev->color_info.num_components == 0)
    set_bpp(pdev, pdev->color_info.depth);
#endif

  paper_size = gdev_mjc_paper_size(pdev);
  if (paper_size == PAPER_SIZE_A2 ) {
    if (ptype == MJ6000C)
      m = mj6000c_a2;
    else if (ptype == MJ8000C)
      m = mj8000c_a2;
    else
      m = mj_margin;
  } else {
    m = mj_margin;
  }

  gx_device_set_margins(pdev, m, true);

  if (mj->colorcomp == 3)
    mj->density = (int)(mj->density * 720 / ydpi) * 1.5;
  else
    mj->density = mj->density * 720 / ydpi;

  /* Print Resolution Check */
  if (!((xdpi == 180 && ydpi == 180) ||
      (xdpi == 360 && ydpi == 360) ||
      (xdpi == 720 && ydpi == 720) ||
      (xdpi == 360 && ydpi == 720) ||
      (xdpi == 720 && ydpi == 360)))
    return_error(gs_error_rangecheck);

  return gdev_prn_open(pdev);
}

/* Get properties.  In addition to the standard and printer
 * properties, we supply shingling and depletion parameters,
 * and control over the bits-per-pixel used in output rendering */
/* Added properties for DeskJet 5xxC */

static int
mj_get_params(gx_device *pdev, gs_param_list *plist)
{
        int code = gdev_prn_get_params(pdev, plist);

        if ( code < 0 ||
            (code = param_write_int(plist, "Density", &mj->density)) < 0 ||
            (code = param_write_int(plist, "Cyan", &mj->cyan)) < 0 ||
            (code = param_write_int(plist, "Magenta", &mj->magenta)) < 0 ||
            (code = param_write_int(plist, "Yellow", &mj->yellow)) < 0 ||
            (code = param_write_int(plist, "Black", &mj->black)) < 0 ||
            (code = param_write_bool(plist, "Unidirectional", &mj->direction)) < 0 ||
            (code = param_write_bool(plist, "Microweave", &mj->microweave)) < 0 ||
            (code = param_write_int(plist, "DotSize", &mj->dotsize)) < 0
           )
          return code;

        return code;
}

/* Put properties. */
static int
mj700v2c_put_params(gx_device *pdev, gs_param_list *plist)
{
  return mj_put_params(pdev, plist, MJ700V2C);
}

static int
mj500c_put_params(gx_device *pdev, gs_param_list *plist)
{
  return mj_put_params(pdev, plist, MJ500C);
}

static int
mj_put_params(gx_device *pdev,  gs_param_list *plist, int ptype)
{
        int old_bpp = mj->color_info.depth;
        int bpp = 0;
        int code = 0;
        int density = mj->density;
        int cyan = mj->cyan;
        int magenta = mj->magenta;
        int yellow = mj->yellow;
        int black = mj->black;
        bool direction = mj->direction;
        bool microweave = mj->microweave;
        int dotsize = mj->dotsize;
        gs_param_name param_name;
        int ecode = 0;

        code = put_param_int(plist, "Density", &density, 0, INT_MAX, code);
        code = put_param_int(plist, "Cyan", &cyan, 0, INT_MAX, code);
        code = put_param_int(plist, "Magenta", &magenta, 0, INT_MAX, code);
        code = put_param_int(plist, "Yellow", &yellow, 0, INT_MAX, code);
        code = put_param_int(plist, "Black", &black, 0, INT_MAX, code);
        (void) code;

        if ((code = param_read_bool(plist,
                                     (param_name = "Unidirectional"),
                                     &direction))< 0) {
          param_signal_error(plist, param_name, ecode = code);
        }

        if ((code = param_read_bool(plist,
                                     (param_name = "Microweave"),
                                     &microweave))< 0) {
          param_signal_error(plist, param_name, ecode = code);
        }
        if (ecode < 0)
          return code;

        if (microweave && pdev->x_pixels_per_inch == 180)
            return_error(gs_error_rangecheck);

        code = put_param_int(plist, "DotSize", &dotsize, 0, 1, code);
        code = put_param_int(plist, "BitsPerPixel", &bpp, 1, 32, code);

        if ( code < 0 )
          return code;

        mj->density = density;
        mj->cyan = cyan;
        mj->magenta = magenta;
        mj->yellow = yellow;
        mj->black = black;
        mj->direction = direction;
        mj->microweave = microweave;
        mj->dotsize = dotsize;

        if ( bpp != 0 ) {
          if (bpp != 8 && bpp != 32)
            return_error(gs_error_rangecheck);

          if (ptype == MJ500C)
            mj500c_set_bpp(pdev, bpp);
          else
            set_bpp(pdev, bpp);
          gdev_prn_put_params(pdev, plist);
          if ( bpp != old_bpp && pdev->is_open )
            return gs_closedevice(pdev);
          return 0;
        }
        else
          return gdev_prn_put_params(pdev, plist);
}

/* ------ Internal routines ------ */
/* MACROS FOR DITHERING (we use macros for compact source and faster code) */
/* Floyd-Steinberg dithering. Often results in a dramatic improvement in
 * subjective image quality, but can also produce dramatic increases in
 * amount of printer data generated and actual printing time!! Mode 9 2D
 * compression is still useful for fairly flat colour or blank areas but its
 * compression is much less effective in areas where the dithering has
 * effectively randomised the dot distribution. */

#define SHIFT (I * I)
#define MINVALUE  0
#define MAXVALUE  ((256 << SHIFT) - 1)
#define THRESHOLD (128 << SHIFT)

#define FSditherI(inP, out, errP, Err, Bit, Offset)\
        oldErr = Err;\
        Err = (*errP + ((Err * 7) >> 4) + (*inP++ << SHIFT));\
        if (Err > MAXVALUE) Err = MAXVALUE;\
        else if (Err < MINVALUE) Err = MINVALUE;\
        if (Err > THRESHOLD) {\
          out |= Bit;\
          Err -= MAXVALUE;\
        }\
        errP[Offset] += ((Err * 3) >> 4);\
        *errP++ = ((Err * 5 + oldErr) >> 4);

#define FSditherD(inP, out, errP, Err, Bit, Offset)\
        oldErr = Err;\
        Err = (*--errP + ((Err * 7) >> 4) + (*--inP << SHIFT));\
        if (Err > MAXVALUE) Err = MAXVALUE;\
        else if (Err < MINVALUE) Err = MINVALUE;\
        if (Err > THRESHOLD) {\
          out |= Bit;\
          Err -= MAXVALUE;\
        }\
        errP[Offset] += ((Err * 3) >> 4);\
        *errP = ((Err * 5 + oldErr) >> 4);

#define MATRIX_I(inP, out, Bit, Offset)\
        if ((*inP++ << 6) > Offset) {\
          out |= Bit;\
        }

#define MATRIX_D(inP, out, Bit, Offset)\
        if ((*--inP << 6) > Offset) {\
          out |= Bit;\
        }

/* Here we rely on compiler optimisation to remove lines of the form
 * (if (1 >= 4) {...}, ie. the constant boolean expressions */

#define FSDline(scan, i, j, plane_size, cErr, mErr, yErr, kErr, cP, mP, yP, kP, n)\
{\
    unsigned short *mat = matrix2 + (lnum & 127)*128;\
    int x;\
    (void)cErr; /* Stop compiler warning */\
    if (scan == 0) {       /* going_up */\
      x = 0;\
      for (i = 0; i < plane_size; i++) {\
        byte c, y, m, k, bitmask;\
        int val;\
        bitmask = 0x80;\
        for (c = m = y = k = j = 0; j < 8; j++) {\
          val = *(mat + (x++ & 127));\
          if (n >= 4)\
            {\
                         MATRIX_I(dp, k, bitmask, val);\
            }\
          if (n >= 3)\
            { MATRIX_I(dp, c, bitmask, val);\
              MATRIX_I(dp, m, bitmask, val);\
            }\
          MATRIX_I(dp, y, bitmask, val);\
          bitmask >>= 1;\
        }\
        if (n >= 4)\
          *kP++ = k;\
        if (n >= 3)\
          { *cP++ = c;\
            *mP++ = m;\
          }\
            *yP++ = y;\
      }\
    } else {		/* going_down */\
      x = plane_size*8;\
      for (i = 0; i < plane_size; i++) {\
        byte c, y, m, k, bitmask;\
        int val;\
        bitmask = 0x01;\
        for (c = m = y = k = j = 0; j < 8; j++) {\
          val = *(mat + (--x & 127));\
          MATRIX_D(dp, y, bitmask, val);\
          if (n >= 3)\
            { MATRIX_D(dp, m, bitmask, val);\
              MATRIX_D(dp, c, bitmask, val);\
            }\
          if (n >= 4)\
            { MATRIX_D(dp, k, bitmask, val);\
            }\
          bitmask <<= 1;\
        }\
        *--yP = y;\
        if (n >= 3)\
          { *--mP = m;\
            *--cP = c;\
          }\
        if (n >= 4)\
          *--kP = k;\
      }\
    }\
}
/* END MACROS FOR DITHERING */

/* Some convenient shorthand .. */
#define x_dpi        (pdev->x_pixels_per_inch)
#define y_dpi        (pdev->y_pixels_per_inch)
#define CONFIG_16BIT "\033*v6W\000\003\000\005\006\005"
#define CONFIG_24BIT "\033*v6W\000\003\000\010\010\010"

/* To calculate buffer size as next greater multiple of both parameter and W */
#define calc_buffsize(a, b) (((((a) + ((b) * W) - 1) / ((b) * W))) * W)

/*
 * Miscellaneous functions for Canon BJC-600J printers in raster command mode.
 */
#define fputshort(n, f) gp_fputc((n)%256,f);gp_fputc((n)/256,f)

#define row_bytes (img_rows / 8)
#define row_words (row_bytes / sizeof(word))
#define min_rows (32)		/* for optimization of text image printing */

static int
mj_raster_cmd(int c_id, int in_size, byte* in, byte* buf2,
              gx_device_printer* pdev, gp_file* prn_stream)
{
  int band_size = 1;	/* 1, 8, or 24 */

  byte *out = buf2;

  int width = in_size;
  int count;

  byte* in_end = in + in_size;

  static char colour_number[] = "\004\001\002\000"; /* color ID for MJ700V2C */

  byte *inp = in;
  byte *outp = out;
  register byte *p, *q;

  /* specifying a colour */

  gp_fputs("\033r",prn_stream); /* secape sequence to specify a color */
  gp_fputc(colour_number[c_id], prn_stream);

  /* end of specifying a colour */

        /* Following codes for compression are borrowed from gdevescp.c */

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

      for( q++ ; q < in_end && *q == *p ; q++ ) {
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
  /*
   ** Output data:
   */

  gp_fwrite("\033.\001", 1, 3, prn_stream);

  if(pdev->y_pixels_per_inch == 720)
       gp_fputc('\005', prn_stream);
  else if(pdev->y_pixels_per_inch == 180)
       gp_fputc('\024', prn_stream);
  else /* pdev->y_pixels_per_inch == 360 */
       gp_fputc('\012', prn_stream);

  if(pdev->x_pixels_per_inch == 720)
       gp_fputc('\005', prn_stream);
  else if(pdev->x_pixels_per_inch == 180)
       gp_fputc('\024', prn_stream);
  else /* pdev->x_pixels_per_inch == 360 */
       gp_fputc('\012', prn_stream);

  gp_fputc(band_size, prn_stream);

  gp_fputc((width << 3) & 0xff, prn_stream);
  gp_fputc( width >> 5,	   prn_stream);

  gp_fwrite(out, 1, (outp - out), prn_stream);

  gp_fputc('\r', prn_stream);

  return 0;
}

static int
mj_v_skip(int n, gx_device_printer *pdev, gp_file *stream)
{
        /* This is a kind of magic number. */
  static const int max_y_step = (256 * 15 + 255);

  int l = n - max_y_step;
  for (; l > 0; l -= max_y_step) {    /* move 256 * 15 + 255 dots at once*/
    gp_fwrite("\033(v\2\0\xff\x0f", sizeof(byte), 7, stream);
  }
  l += max_y_step;
  /* move to the end. */
  {
    int n2 = l / 256;
    int n1 = l - n2 * 256;
    gp_fwrite("\033(v\2\0", sizeof(byte) ,5 ,stream);
    gp_fputc(n1, stream);
    gp_fputc(n2, stream);
    gp_fputc('\r', stream);
  }
  return 0;
}

/* NOZ */

static void
bld_barrier( short **bar , int x )
{
        int i , j;

        short t;
        short *p;
        short *b;
        short *dat = barrier_dat + 1;

        p = *bar++ + x + 1;

        for ( i = 0 ; i < 11 ; i++ ) {
                t = *dat++;
                if (*p < t )
                        *p = t;
                p++;
        }

        for ( j = 0 ; j < 11 ; j++ ) {
                p = *bar++ + x;
                b = p;

                t = *dat++;
                if (*p < t )
                        *p = t;
                p++;
                for ( i = 0 ; i < 11 ; i++ ) {
                        t = *dat++;
                        if (*p < t )
                                *p = t;
                        p++;

                        if (*(--b) < t )
                                *b = t;
                }
        }
}

static void
DifSubK( int d0 , short *a4 , short *a5 )
{
/*
                                +---+---+---+
                                |   | X |1/2|
                                +---+---+---+
                                |1/4|1/8|1/8|
                                +---+---+---+
*/
        *a4++   =  0;
        d0 >>= 1;
        *a4     += d0;
        d0 >>= 1;
        *(a5-1) += d0;
        d0 >>= 1;
        *a5++   += d0;
        *a5     += d0;
}

/* a4.w , a5.w , */
static  byte
Xtal( byte bitmask , short d0 , int x , short **bar , short *b1 , short *b2 )
{
        short *a2;

        if (d0 != 0)
                d0 += *b1;

        a2 = *bar + x;

/*dprintf1("[%02X]",*a2);*/
        if (*a2 < d0) {
                d0 -= 16400;
                if (-4096 >= d0) {
                        DifSubK( d0 , b1 , b2 );
                        bld_barrier( bar , x );
                } else {
                        DifSubK( d0 , b1 , b2 );
                }
                return( bitmask );
        } else {
                if (d0 > 56)
                        d0 -= 56;
                DifSubK( d0 , b1 , b2 );
                return( 0 );
        }
}

static void
xtal_plane( byte *dp , short *buf[] , byte *oP , short **bar , int plane_size , int xtalbuff_size )
{
        int i;
        int j;
        int x = 0;
        byte bitmask;
        byte out;
        short *p;
        short *b1 , *b2;

        b1 = buf[0];
        b2 = buf[1];
/*
        for ( i = 0 ; i < 100 ; i++ ) {
                dprintf1("[%04X]",bar[0][i]);
        }
        dprintf("\n");
*/
        for ( i = 0 ; i < plane_size ; i++ ) {
                bitmask = 0x80;
                out = 0;
                for ( j = 0 ; j < 8 ; j++ ) {
                        out |= Xtal( bitmask , (short)(*dp) << 6 , x++ , bar , b1++ , b2++ );
                        dp += 4;
                        bitmask >>= 1;
                }
                *oP++ = out;
        }
/*dprintf("\n");*/
        p = buf[0];
/*	dprintf("\n"); */
        buf[0] = buf[1];
        buf[1] = p;

        p = bar[0];
        for ( i = 0 ; i < plane_size*8 ; i++ )
                *p++ = 0;

        /*	memset( p, 0, (xtalbuff_size-16) * W);*/
        p = bar[0];
        for ( i = 0 ; i <= 14 ; i++ )
                bar[i] = bar[i+1];
        bar[15] = p;
}

static int
mj700v2c_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
  return mj_print_page(pdev, prn_stream, MJ700V2C);
}

static int
mj500c_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
  return mj_print_page(pdev, prn_stream, MJ500C);
}

static int
mj6000c_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
  return mj_print_page(pdev, prn_stream, MJ6000C);
}

static int
mj8000c_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
  return mj_print_page(pdev, prn_stream, MJ8000C);
}

/* Send the page to the printer.  Compress each scan line. */
static int
mj_print_page(gx_device_printer * pdev, gp_file * prn_stream, int ptype)
{
/*  int line_size = gdev_prn_rasterwidth(pdev, 0); */
  int line_size = gdev_prn_raster(pdev);
  int line_size_words = (line_size + W - 1) / W;
  int num_comps = pdev->color_info.num_components;
  int bits_per_pixel = pdev->color_info.depth;
  int storage_bpp = bits_per_pixel;
  int expanded_bpp = bits_per_pixel;
  int plane_size, databuff_size;
  int errbuff_size = 0;
  int outbuff_size = 0;
  int scan = 0;
  int *errors[2];
  byte *data[4], *plane_data[4][4], *out_data;
  byte *out_row;
  word *storage;
  uint storage_size_words;
  uint mj_tmp_buf_size;
  byte* mj_tmp_buf;
  int xtalbuff_size;
  short *xtalbuff;
  short *Cbar[16];
  short *Mbar[16];
  short *Ybar[16];
  short *Kbar[16];
  short *Cbuf[2];
  short *Mbuf[2];
  short *Ybuf[2];
  short *Kbuf[2];

  /* Tricks and cheats ... */
  if (num_comps == 3) num_comps = 4;            /* print CMYK */

  if (storage_bpp == 8 && num_comps >= 3)
    bits_per_pixel = expanded_bpp = 3;  /* Only 3 bits of each byte used */

  plane_size = calc_buffsize(line_size, storage_bpp);

  if (bits_per_pixel == 1) {            /* Data printed direct from i/p */
    outbuff_size = plane_size * 4;      /* but need separate output buffers */
  }

  if (bits_per_pixel > 4) {             /* Error buffer for FS dithering */
    expanded_bpp = storage_bpp =        /* 8, 24 or 32 bits */
      num_comps * 8;
    errbuff_size =                      /* 4n extra values for line ends */
      calc_buffsize((plane_size * expanded_bpp + num_comps * 4) * I, 1);
  }

  databuff_size = plane_size * storage_bpp;

  storage_size_words = ((plane_size + plane_size) * num_comps +
                        databuff_size + errbuff_size + outbuff_size) / W;

/* NOZ */
  xtalbuff_size = plane_size*8 + 64;
  xtalbuff = (short *) gs_malloc(pdev->memory->non_gc_memory,  xtalbuff_size*(16*4+2*4) , W, "mj_colour_print_barrier");
  memset(xtalbuff, 0, xtalbuff_size*(16*4+2*4) * W);
  {
        int i;
        short *p = xtalbuff + 16;
        for ( i = 0 ; i < 16 ; i++ ) {
                Cbar[i] = p;
                p += xtalbuff_size;
        }
        for ( i = 0 ; i < 16 ; i++ ) {
                Mbar[i] = p;
                p += xtalbuff_size;
        }
        for ( i = 0 ; i < 16 ; i++ ) {
                Ybar[i] = p;
                p += xtalbuff_size;
        }
        for ( i = 0 ; i < 16 ; i++ ) {
                Kbar[i] = p;
                p += xtalbuff_size;
        }
        Cbuf[0] = p;
        p += xtalbuff_size;
        Cbuf[1] = p;
        p += xtalbuff_size;
        Mbuf[0] = p;
        p += xtalbuff_size;
        Mbuf[1] = p;
        p += xtalbuff_size;
        Ybuf[0] = p;
        p += xtalbuff_size;
        Ybuf[1] = p;
        p += xtalbuff_size;
        Kbuf[0] = p;
        p += xtalbuff_size;
        Kbuf[1] = p;
        p += xtalbuff_size;
        (void) p;
  }

  storage = (word *) gs_malloc(pdev->memory->non_gc_memory, storage_size_words, W, "mj_colour_print_page");

/* prepare a temporary buffer for mj_raster_cmd */

  mj_tmp_buf_size = plane_size;
  mj_tmp_buf = (byte *) gs_malloc(pdev->memory->non_gc_memory, mj_tmp_buf_size, W ,"mj_raster_buffer");

#if 0
  dprintf1("storage_size_words :%d\n", storage_size_words);
  dprintf1("mj_tmp_buf_size :%d\n", mj_tmp_buf_size);
#endif
  /*
   * The principal data pointers are stored as pairs of values, with
   * the selection being made by the 'scan' variable. The function of the
   * scan variable is overloaded, as it controls both the alternating
   * raster scan direction used in the Floyd-Steinberg dithering and also
   * the buffer alternation required for line-difference compression.
   *
   * Thus, the number of pointers required is as follows:
   *
   *   errors:      2  (scan direction only)
   *   data:        4  (scan direction and alternating buffers)
   *   plane_data:  4  (scan direction and alternating buffers)
   */

  if (storage == NULL || mj_tmp_buf == NULL) /* can't allocate working area */
    return_error(gs_error_VMerror);
  else {
    int i;
    byte *p = out_data = out_row = (byte *)storage;
    data[0] = data[1] = data[2] = p;
    data[3] = p + databuff_size;
    if (bits_per_pixel > 1) {
      p += databuff_size;
    }
    if (bits_per_pixel > 4) {
      errors[0] = (int *)p + num_comps * 2;
      errors[1] = errors[0] + databuff_size;
      p += errbuff_size;
    }
    for (i = 0; i < num_comps; i++) {
      plane_data[0][i] = plane_data[2][i] = p;
      p += plane_size;
    }
    for (i = 0; i < num_comps; i++) {
      plane_data[1][i] = p;
      plane_data[3][i] = p + plane_size;
      p += plane_size;
    }
    if (bits_per_pixel == 1) {
      data[1] += databuff_size;   /* coincides with plane_data pointers */
      data[3] += databuff_size;
    }
  }

  /* Clear temp storage */
  memset(storage, 0, storage_size_words * W);

  /* Initialize printer. */
  {
    /** Reset printer, enter graphics mode: */

    gp_fwrite("\033@\033(G\001\000\001", sizeof(byte), 8, prn_stream);

    /** Micro-weave-Mode */
    if (mj->microweave) {
      gp_fwrite("\033(i\001\000\001", sizeof(byte), 6, prn_stream);
    }
    /** Dot-Size define */
    if (mj->dotsize) {
      gp_fwrite("\033(e\002\000\000\001", sizeof(byte), 7, prn_stream);
    }

    if (ptype == MJ6000C || ptype == MJ8000C) {
      /* Select Monochrome/Color Printing Mode Command */
      if (pdev->color_info.depth == 8)
        gp_fwrite("\033(K\002\000\000\001", sizeof(byte), 7, prn_stream);
    }

    if (mj->direction) /* set the direction of the head */
      gp_fwrite("\033U\1", 1, 3, prn_stream); /* Unidirectional Printing */
    else
      gp_fwrite("\033U\0", 1, 3, prn_stream);

#if 0
#ifdef A4
        /*
        ** After reset, the Stylus is set up for US letter paper.
        ** We need to set the page size appropriately for A4 paper.
        ** For some bizarre reason the ESC/P2 language wants the bottom
        ** margin measured from the *top* of the page:
        */

        gp_fwrite("\033(U\001\0\n\033(C\002\0t\020\033(c\004\0\0\0t\020",
                                                        1, 22, prn_stream);
#endif
#endif

        /*
        ** Set the line spacing to match the band height:
        */

        if( pdev->y_pixels_per_inch >= 720 )
          gp_fwrite("\033(U\001\0\005\033+\001", sizeof(byte), 9, prn_stream);
        else if( pdev->y_pixels_per_inch >= 360 )
           gp_fwrite("\033(U\001\0\012\033+\001", sizeof(byte), 9, prn_stream);
        else /* 180 dpi */
           gp_fwrite("\033(U\001\0\024\033+\002", sizeof(byte), 9, prn_stream);

    /* set the length of the page */
        gp_fwrite("\033(C\2\0", sizeof(byte), 5, prn_stream);
        gp_fputc(((pdev->height) % 256), prn_stream);
        gp_fputc(((pdev->height) / 256), prn_stream);
  }

#define MOFFSET (pdev->t_margin - MJ700V2C_PRINT_LIMIT) /* Print position */

  {
    gp_fwrite("\033(V\2\0\0\0",sizeof(byte), 7, prn_stream);
    gp_fwrite("\033(v\2\0\0\xff",sizeof(byte), 7, prn_stream);
  }

  /* Send each scan line in turn */
  {
    long int lend = (int)(pdev->height -
      (dev_t_margin_points(pdev) + dev_b_margin_points(pdev)));
    int cErr, mErr, yErr, kErr;
    int this_pass, i;
    long int lnum;
    int num_blank_lines = 0;
    int start_rows = (num_comps == 1) ?
      HEAD_ROWS_MONO - 1 : HEAD_ROWS_COLOUR - 1;
    word rmask = ~(word) 0 << ((-pdev->width * storage_bpp) & (W * 8 - 1));

    cErr = mErr = yErr = kErr = 0;

    if (bits_per_pixel > 4) { /* Randomly seed initial error buffer */
      int *ep = errors[0];
      for (i = 0; i < databuff_size; i++) {
        *ep++ = (rand() % (MAXVALUE / 2))  - MAXVALUE / 4;
      }
    }

    this_pass = start_rows;

    lnum = 0;

    /* for Debug */

    for (; lnum < lend; lnum++) {
      word *data_words = (word *)data[scan];
      register word *end_data = data_words + line_size_words;
      gx_color_index *p_data;

      gdev_prn_copy_scan_lines(pdev, lnum, data[scan], line_size);

      /* Mask off 1-bits beyond the line width. */
      end_data[-1] &= rmask;

      /* Remove trailing 0s. */
      while (end_data > data_words && end_data[-1] == 0)
        end_data--;
      if (end_data == data_words) {	/* Blank line */
        num_blank_lines++;
        continue; /* skip to  for (lnum) loop */
      }
      /* Skip blank lines if any */
      if (num_blank_lines > 0 ) {
        mj_v_skip(num_blank_lines, pdev, prn_stream);
        memset(plane_data[1 - scan][0], 0, plane_size * num_comps);
        num_blank_lines = 0;
        this_pass = start_rows;
      }

      /* Correct color depth. */
          if (mj->density != 1024 || mj->yellow != 1024 || mj->cyan != 1024
                  || mj->magenta != 1024 || mj->black != 1024 ) {
              for (p_data = (gx_color_index*) data_words; p_data < (gx_color_index *)end_data; p_data++) {
                        *p_data = mjc_correct_color(pdev, *p_data);
              }
          }

      {			/* Printing non-blank lines */
        register byte *kP = plane_data[scan + 2][3];
        register byte *cP = plane_data[scan + 2][2];
        register byte *mP = plane_data[scan + 2][1];
        register byte *yP = plane_data[scan + 2][0];
        register byte *dp = data[scan + 2];
        int i, j;
        byte *odp;

        if (this_pass)
          this_pass--;
        else
          this_pass = start_rows;

        if (expanded_bpp > bits_per_pixel)   /* Expand line if required */
          expand_line(data_words, line_size, bits_per_pixel, expanded_bpp);

        /* In colour modes, we have some bit-shuffling to do before
         * we can print the data; in FS mode we also have the
         * dithering to take care of. */
        switch (expanded_bpp) {    /* Can be 1, 3, 8, 24 or 32 */
        case 3:
          /* Transpose the data to get pixel planes. */
          for (i = 0, odp = plane_data[scan][0]; i < databuff_size;
               i += 8, odp++) {	/* The following is for 16-bit
                                 * machines */
#define spread3(c)\
    { 0, c, c*0x100, c*0x101, c*0x10000L, c*0x10001L, c*0x10100L, c*0x10101L }
            static word spr40[8] = spread3(0x40);
            static word spr08[8] = spread3(8);
            static word spr02[8] = spread3(2);
            register byte *dp = data[scan] + i;
            register word pword =
            (spr40[dp[0]] << 1) +
            (spr40[dp[1]]) +
            (spr40[dp[2]] >> 1) +
            (spr08[dp[3]] << 1) +
            (spr08[dp[4]]) +
            (spr08[dp[5]] >> 1) +
            (spr02[dp[6]]) +
            (spr02[dp[7]] >> 1);
            odp[0] = (byte) (pword >> 16);
            odp[plane_size] = (byte) (pword >> 8);
            odp[plane_size * 2] = (byte) (pword);
          }
          break;

        case 8:
          FSDline(scan, i, j, plane_size, cErr, mErr, yErr, kErr,
                  cP, mP, yP, kP, 1);
          break;
        case 24:
          FSDline(scan, i, j, plane_size, cErr, mErr, yErr, kErr,
                  cP, mP, yP, kP, 3);
          break;
        case 32:
                if (scan == 1) {
                        dp -= plane_size*8*4;
                        cP -= plane_size;
                        mP -= plane_size;
                        yP -= plane_size;
                        kP -= plane_size;
                }
/*
{
        byte *p = dp;
        int i;
        for ( i = 0 ; i < plane_size ; i++ ) {
                dprintf4 ( "[%02X%02X%02X%02X]" , p[0] , p[1] , p[2] , p[3] );
                p += 4;
        }
        dprintf("\n");

}
*/
/*
          FSDline(scan, i, j, plane_size, cErr, mErr, yErr, kErr,
                  cP, mP, yP, kP, 4);
*/
/* NOZ */
          xtal_plane( dp++ , Kbuf , kP , Kbar , plane_size , xtalbuff_size );
          xtal_plane( dp++ , Cbuf , cP , Cbar , plane_size , xtalbuff_size );
          xtal_plane( dp++ , Mbuf , mP , Mbar , plane_size , xtalbuff_size );
          xtal_plane( dp++ , Ybuf , yP , Ybar , plane_size , xtalbuff_size );

          break;

        } /* switch(expanded_bpp) */

        /* Make sure all black is in the k plane */
        if (num_comps == 4 ) {
          if (mj->colorcomp > 3 ) {
            register word *kp = (word *)plane_data[scan][3];
            register word *cp = (word *)plane_data[scan][2];
            register word *mp = (word *)plane_data[scan][1];
            register word *yp = (word *)plane_data[scan][0];
            if (bits_per_pixel > 4) {  /* This has been done as 4 planes */
#if 0
              for (i = 0; i < plane_size / W; i++) {
                word bits = ~*kp++;
                *cp++ &= bits;
                *mp++ &= bits;
                *yp++ &= bits;
              }
#endif
            } else {  /* This has really been done as 3 planes */
              for (i = 0; i < plane_size / W; i++) {
                word bits = *cp & *mp & *yp;
                *kp++ = bits;
                bits = ~bits;
                *cp++ &= bits;
                *mp++ &= bits;
                *yp++ &= bits;
              }
            }
          } else if (mj->colorcomp == 3 ) {
            register word *kp = (word *)plane_data[scan][3];
            register word *cp = (word *)plane_data[scan][2];
            register word *mp = (word *)plane_data[scan][1];
            register word *yp = (word *)plane_data[scan][0];
            if (bits_per_pixel > 4) {  /* This has been done as 4 planes */
              for (i = 0; i < plane_size / W; i++) {
                word bits = *kp++; /* kp will not be used when printing */
                *cp++ |= bits;
                *mp++ |= bits;
                *yp++ |= bits;
              }
            } else {  /* This has really been done as 3 planes */
            }
          }
        }

        /* Transfer raster graphics
         * in the order (K), C, M, Y. */
        switch (mj->colorcomp) {
        case 1:
          out_data = (byte*) plane_data[scan][0];
          /* 3 for balck */
          mj_raster_cmd(3, plane_size, out_data, mj_tmp_buf, pdev, prn_stream);
          break;
        case 3:
          for (i = 3 - 1; i >= 0; i--) {
            out_data = (byte*) plane_data[scan][i];
            mj_raster_cmd(i, plane_size, out_data, mj_tmp_buf, pdev, prn_stream);
          }
          break;
        default:
          for (i = num_comps - 1; i >= 0; i--) {
            out_data = (byte*) plane_data[scan][i];
            mj_raster_cmd(i, plane_size, out_data, mj_tmp_buf, pdev, prn_stream);
          }
          break;
        } /* Transfer Raster Graphics ... */

        {
          if ( pdev->y_pixels_per_inch > 360 ) {
             gp_fwrite("\033(v\2\0\1\0",sizeof(byte),7, prn_stream);
           } else {
             gp_fputc('\n', prn_stream);
           }
        }
        scan = 1 - scan;          /* toggle scan direction */
      }	  /* Printing non-blank lines */
    }     /* for lnum ... */
  }       /* send each scan line in turn */

  /* end raster graphics & reset printer */

  /* eject page */
  {
    gp_fputs("\f\033@", prn_stream);
    gp_fflush(prn_stream);
  }
  /* free temporary storage */
  gs_free(pdev->memory->non_gc_memory, (char *) storage, storage_size_words, W, "mj_colour_print_page");
  gs_free(pdev->memory->non_gc_memory, (char *) mj_tmp_buf, mj_tmp_buf_size, W, "mj_raster_buffer");
  gs_free(pdev->memory->non_gc_memory, (char *) xtalbuff , xtalbuff_size*(16*4+2*4) , W, "mj_colour_print_barrier");

  return 0;
}

static void
mj_color_correct(gx_color_value *Rptr ,gx_color_value *Gptr , gx_color_value *Bptr )
                                                                /* R,G,B : 0 to 255 */
{
        short	R,G,B;				/* R,G,B : 0 to 255  */
        short	C,M,Y;				/* C,M,Y : 0 to 1023 */
        short	H,D,Wa;				/* ese-HSV         */
        long	S;					/*     HSV         */

        R = *Rptr;
        G = *Gptr;
        B = *Bptr;
        if (R==G) {
                if (G==B) {						/* R=G=B */
                        C=M=Y=1023-v_tbl[R];
                        *Rptr = C;
                        *Gptr = M;
                        *Bptr = Y;
                        return;
                }
        }

        if (R>G) {
                if (G>=B) {			/* R>G>B */
                        Wa=R;
                        D=R-B;
                        H=(G-B)*256/D;
                } else if (R>B) {		/* R>B>G */
                        Wa=R;
                        D=R-G;
                        H=1536-(B-G)*256/D;
                } else {			/* B>R>G */
                        Wa=B;
                        D=B-G;
                        H=1024+(R-G)*256/D;
                }
        } else {
                if (R>B) {			/* G>R>B */
                        Wa=G;
                        D=G-B;
                        H=512-(R-B)*256/D;
                } else if (G>B) {		/* G>B>R */
                        Wa=G;
                        D=G-R;
                        H=512+(B-R)*256/D;
                } else {			/* B>G>R */
                        Wa=B;
                        D=B-R;
                        H=1024-(G-R)*256/D;
                }
        }

        if(Wa!=0){
                if(Wa==D){
                        Wa=v_tbl[Wa];
                        D=Wa/4;
                } else {
                        S=((long)D<<16)/Wa;
                        Wa=v_tbl[Wa];
                        D= ( ((long)S*Wa)>>18 );
                }
        }
        Wa=1023-Wa;

        C=(HtoCMY[H*3  ])*D/256+Wa;
        M=(HtoCMY[H*3+1])*D/256+Wa;
        Y=(HtoCMY[H*3+2])*D/256+Wa;
        if (C<0)
                C=0;
        if (M<0)
                M=0;
        if (Y<0)
                Y=0;

        /* 2019-10-29 this used to be 'if(H>256 && H<1024)', which can then go
        beyond bounds of the 512-element grnsep2[]. So have patched up to avoid
        this, but without any proper idea about what's going on. */
        if(H>256 && H<768){		/* green correct */
                short	work;
                work=(((long)grnsep[M]*(long)grnsep2[H-256])>>16);
                C+=work;
                Y+=work+work;
                M-=work+work;
                if(C>1023) C=1023;
                if(Y>1023) Y=1023;
        }

        *Rptr = C;
        *Gptr = M;
        *Bptr = Y;
}

/*
 * Map a r-g-b color to a color index.
 * We complement the colours, since we're using cmy anyway, and
 * because the buffering routines expect white to be zero.
 * Includes colour balancing, following HP recommendations, to try
 * and correct the greenish cast resulting from an equal mix of the
 * c, m, y, inks by reducing the cyan component to give a truer black.
 */
static gx_color_index
gdev_mjc_map_rgb_color(gx_device *pdev, const gx_color_value cv[])
{
  gx_color_value r, g, b;

  r = cv[0]; g = cv[1]; b = cv[2];
  if (gx_color_value_to_byte(r & g & b) == 0xff)
    return (gx_color_index)0;         /* white */
  else {
    gx_color_value c = gx_max_color_value - r;
    gx_color_value m = gx_max_color_value - g;
    gx_color_value y = gx_max_color_value - b;

    switch (pdev->color_info.depth) {
    case 1:
      return ((c | m | y) > gx_max_color_value / 2 ?
              (gx_color_index)1 : (gx_color_index)0);
    case 8:
      if (pdev->color_info.num_components >= 3)
#define gx_color_value_to_1bit(cv) ((cv) >> (gx_color_value_bits - 1))
        return (gx_color_value_to_1bit(c) +
                (gx_color_value_to_1bit(m) << 1) +
                (gx_color_value_to_1bit(y) << 2));
      else
#define red_weight 306
#define green_weight 601
#define blue_weight 117
        return ((((word)c * red_weight +
                  (word)m * green_weight +
                  (word)y * blue_weight)
                 >> (gx_color_value_bits + 2)));
    case 16:
#define gx_color_value_to_5bits(cv) ((cv) >> (gx_color_value_bits - 5))
#define gx_color_value_to_6bits(cv) ((cv) >> (gx_color_value_bits - 6))
      return (gx_color_value_to_5bits(y) +
              (gx_color_value_to_6bits(m) << 5) +
              (gx_color_value_to_5bits(c) << 11));
    case 24:
      return (gx_color_value_to_byte(y) +
              (gx_color_value_to_byte(m) << 8) +
              ((word)gx_color_value_to_byte(c) << 16));
    case 32:
        {
                gx_color_value k;
                c = gx_color_value_to_byte(r);
                m = gx_color_value_to_byte(g);
                y = gx_color_value_to_byte(b);

                mj_color_correct( &c , &m , &y );

                c = esp_dat_c[c];
                m = esp_dat_m[m];
                y = esp_dat_y[y];

                k = c <= m ? (c <= y ? c : y) : (m <= y ? m : y);
                k = black_sep[ k >> 4 ] >> 6;
                c >>= 6;
                m >>= 6;
                y >>= 6;

        return ( (y - k) + ((m - k) << 8) +
                ((word)(c - k) << 16) + ((word)(k) << 24) );
      }
    }
  }
  return (gx_color_index)0;   /* This never happens */
}

/* Map a color index to a r-g-b color. */
static int
gdev_mjc_map_color_rgb(gx_device *pdev, gx_color_index color,
                            gx_color_value prgb[3])
{
  /* For the moment, we simply ignore any black correction */
  switch (pdev->color_info.depth) {
  case 1:
    prgb[0] = prgb[1] = prgb[2] = -((gx_color_value)color ^ 1);
    break;
  case 8:
      if (pdev->color_info.num_components >= 3)
        { gx_color_value c = (gx_color_value)color ^ 7;
          prgb[0] = -(c & 1);
          prgb[1] = -((c >> 1) & 1);
          prgb[2] = -(c >> 2);
        }
      else
        { gx_color_index value = (gx_color_index)color ^ 0xff;
          prgb[0] = prgb[1] = prgb[2] = (value << 8) + value;
        }
    break;
  case 16:
    { gx_color_index c = (gx_color_index)color ^ 0xffff;
      ushort value = c >> 11;
      prgb[0] = ((value << 11) + (value << 6) + (value << 1) +
                 (value >> 4)) >> (16 - gx_color_value_bits);
      value = (c >> 6) & 0x3f;
      prgb[1] = ((value << 10) + (value << 4) + (value >> 2))
        >> (16 - gx_color_value_bits);
      value = c & 0x1f;
      prgb[2] = ((value << 11) + (value << 6) + (value << 1) +
                 (value >> 4)) >> (16 - gx_color_value_bits);
    }
    break;
  case 24:
    { gx_color_index c = (gx_color_index)color ^ 0xffffff;
      prgb[0] = gx_color_value_from_byte(c >> 16);
      prgb[1] = gx_color_value_from_byte((c >> 8) & 0xff);
      prgb[2] = gx_color_value_from_byte(c & 0xff);
    }
    break;
  case 32:
#define  gx_maxcol gx_color_value_from_byte(gx_color_value_to_byte(gx_max_color_value))
    { gx_color_value w = gx_maxcol - gx_color_value_from_byte(color >> 24);
      prgb[0] = w - gx_color_value_from_byte((color >> 16) & 0xff);
      prgb[1] = w - gx_color_value_from_byte((color >> 8) & 0xff);
      prgb[2] = w - gx_color_value_from_byte(color & 0xff);
    }
    break;
  }
  return 0;
}

/*
* Encode a list of colorant values into a gx_color_index_value.
*/
static gx_color_index
gdev_mjc_encode_color(gx_device *dev, const gx_color_value colors[])
{
    return gdev_mjc_map_rgb_color(dev, colors);
}

/*
* Decode a gx_color_index value back to a list of colorant values.
*/
static int
gdev_mjc_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    return gdev_mjc_map_color_rgb(dev, color, out);
}

/*
 * Convert and expand scanlines:
 *
 *       (a)    16 -> 24 bit   (1-stage)
 *       (b)    16 -> 32 bit   (2-stage)
 *   or  (c)    24 -> 32 bit   (1-stage)
 */
static void
expand_line(word *line, int linesize, int bpp, int ebpp)
{
  int endline = linesize;
  byte *start = (byte *)line;
  register byte *in, *out;

  if (bpp == 16)              /* 16 to 24 (cmy) if required */
    { register byte b0, b1;
      endline = ((endline + 1) / 2);
      in = start + endline * 2;
      out = start + (endline *= 3);

      while (in > start)
        { b0 = *--in;
          b1 = *--in;
          *--out = (b0 << 3) + ((b0 >> 2) & 0x7);
          *--out = (b1 << 5) + ((b0 >> 3)  & 0x1c) + ((b1 >> 1) & 0x3);
          *--out = (b1 & 0xf8) + (b1 >> 5);
        }
    }

  if (ebpp == 32)             /* 24 (cmy) to 32 (cmyk) if required */
    { register byte c, m, y, k;
      endline = ((endline + 2) / 3);
      in = start + endline * 3;
      out = start + endline * 4;

      while (in > start)
        { y = *--in;
          m = *--in;
          c = *--in;
          k = c < m ? (c < y ? c : y) : (m < y ? m : y);
          *--out = y - k;
          *--out = m - k;
          *--out = c - k;
          *--out = k;
        }
    }
}

static int
put_param_int(gs_param_list *plist, gs_param_name pname, int *pvalue, int minval, int maxval, int ecode)
{       int code, value;
        switch ( code = param_read_int(plist, pname, &value) )
        {
        default:
                return code;
        case 1:
                return ecode;
        case 0:
                if ( value < minval || value > maxval )
                   param_signal_error(plist, pname, gs_error_rangecheck);
                *pvalue = value;
                return (ecode < 0 ? ecode : 1);
        }
}

static void
set_bpp(gx_device *pdev, int bits_per_pixel)
{ gx_device_color_info *ci = &pdev->color_info;
  /* Only valid bits-per-pixel are 1, 3, 8, 16, 24, 32 */
  int bpp = bits_per_pixel < 3 ? 1 : bits_per_pixel < 8 ? 3 :
    (bits_per_pixel >> 3) << 3;
  ci->num_components = ((bpp == 1) || (bpp == 8) ? 1 : 3);
  ci->depth = ((bpp > 1) && (bpp < 8) ? 8 : bpp);
  ci->max_gray = (bpp >= 8 ? 255 : 1);
  ci->max_color = (bpp >= 8 ? 255 : bpp > 1 ? 1 : 0);
  ci->dither_grays = (bpp >= 8 ? 5 : 2);
  ci->dither_colors = (bpp >= 8 ? 5 : bpp > 1 ? 2 : 0);
  mj->colorcomp = (bpp == 8 ? 1 : 4);
}

static void
mj500c_set_bpp(gx_device *pdev, int bits_per_pixel)
{ gx_device_color_info *ci = &pdev->color_info;
  /* Only valid bits-per-pixel are 1, 3, 8, 16, 24, 32 */
  int bpp = bits_per_pixel < 3 ? 1 : bits_per_pixel < 8 ? 3 :
    (bits_per_pixel >> 3) << 3;
  ci->num_components = ((bpp == 1) || (bpp == 8) ? 1 : 3);
  ci->depth = ((bpp > 1) && (bpp < 8) ? 8 : bpp);
  ci->max_gray = (bpp >= 8 ? 255 : 1);
  ci->max_color = (bpp >= 8 ? 255 : bpp > 1 ? 1 : 0);
  ci->dither_grays = (bpp >= 8 ? 5 : 2);
  ci->dither_colors = (bpp >= 8 ? 5 : bpp > 1 ? 2 : 0);
  mj->colorcomp = (bpp == 8 ? 1 : 3);
}

static gx_color_index
mjc_correct_color(gx_device_printer *pdev, gx_color_index ci)
{
  gx_color_index c, m, y, k, co;
  gx_color_index k2, k3, k4;
#if __WORDSIZE == 64
  gx_color_index c2, m2, y2, k5, k6, k7, k8;
#endif
  const uint cmask = 0xff;
  uint dn = mj->density;
  uint mjy = mj->yellow;
  uint mjc = mj->cyan;
  uint mjm = mj->magenta;
  uint mjb = mj->black;
  switch (pdev->color_info.depth) {
 case 8:
    k = ((ci & cmask) * (mjb)) >> 10;
    k = (k < cmask) ? k : cmask;
    k2 = (((ci >> 8) & cmask) * (mjb)) >> 10;
    k2 = (k2 < cmask) ? k2 : cmask;
    k3 = (((ci >> 16) & cmask) * (mjb)) >> 10;
    k3 = (k3 < cmask) ? k3 : cmask;
    k4 = (((ci >> 24) & cmask) * (mjb)) >> 10;
    k4 = (k4 < cmask) ? k4 : cmask;
#if __WORDSIZE == 64
    /* This code is ugly... (for 64 bit OS) */
    if (sizeof(word) == 8)
      {
        k5 = (((ci >> 32) & cmask) * (mjb)) >> 10;
        k5 = (k5 < cmask) ? k5 : cmask;
        k6 = (((ci >> 40) & cmask) * (mjb)) >> 10;
        k6 = (k6 < cmask) ? k6 : cmask;
        k7 = (((ci >> 48) & cmask) * (mjb)) >> 10;
        k7 = (k7 < cmask) ? k7 : cmask;
        k8 = (((ci >> 56) & cmask) * (mjb)) >> 10;
        k8 = (k8 < cmask) ? k8 : cmask;
        co = k + (k2 << 8) + (k3 << 16) + (k4 << 24)
          + (k5 << 32) + (k6 << 40) + (k7 << 48) + (k8 << 56);
        if (ci != co)
          dprintf1("%s\n", "error");
        return co;
      }
#endif
    return k + (k2 << 8) + (k3 << 16) + (k << 24);
    break;
 case 32:
    y = ((ci & cmask) * mjy * dn) >> 20;
    y = (y < cmask) ? y : cmask;
    m = (((ci >> 8) & cmask) * mjm * dn) >> 20;
    m = (m < cmask) ? m : cmask;
    c = (((ci >> 16) & cmask) * mjc * dn) >> 20;
    c = (c < cmask) ? c : cmask;
    k = (((ci >> 24) & cmask) * mjb * dn) >> 20;
    k = (k < cmask) ? k : cmask;
#if __WORDSIZE == 64
    /* This code is ugly... (for 64 bit OS) */
    if (sizeof(word) == 8)
      {
        y2 = (((ci >> 32) & cmask) * mjy * dn) >> 20;
        y2 = (y2 < cmask) ? y2 : cmask;
        m2 = (((ci >> 40) & cmask) * mjm * dn) >> 20;
        m2 = (m2 < cmask) ? m2 : cmask;
        c2 = (((ci >> 48) & cmask) * mjc * dn) >> 20;
        c2 = (c2 < cmask) ? c2 : cmask;
        k2 = (((ci >> 56) & cmask) * mjb * dn) >> 20;
        k2 = (k2 < cmask) ? k2 : cmask;

        co = y + (m << 8) + (c << 16) + (k << 24)
          + (y2 << 32) + (m2 << 40) + (c2 << 48) + (k2 << 56);

        return co;
      }
#endif
    co =    (y + (m << 8) + (c << 16) + (k << 24));
/*     dprintf6("%d,%d:%d,%d,%d,%d\n", ci,co, y, m, c, k); */
    return co;
/*    return (gx_color_value_to_byte(y) +
            (gx_color_value_to_byte(m) << 8) +
            ((word)gx_color_value_to_byte(c) << 16) +
            ((word)gx_color_value_to_byte(k) << 24)); */
    break;
  }
  return ci;
}
