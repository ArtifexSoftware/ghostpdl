/* Copyright (C) 1999 Norihito Ohmori.

   Ghostscript laser printer driver supprting Library.

   This software is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
   to anyone for the consequences of using it or for whether it serves any
   particular purpose or works at all, unless he says so in writing.  Refer
   to the GNU General Public License for full details.

   Everyone is granted permission to copy, modify and redistribute
   this software, but only under the conditions described in the GNU
   General Public License.  A copy of this license is supposed to have been
   given to you along with this software so you can know your rights and
   responsibilities.  It should be in a file named COPYING.  Among other
   things, the copyright notice and this notice must be preserved on all
   copies.
 */

/*$Id: gdevlprn.h,v 1.4 2002/10/12 23:24:34 tillkamppeter Exp $ */
/* Library for Laser Printer Driver */

/*
   Main part of this library was taked from Hiroshi Narimatsu's
   Ghostscript driver, epag-3.08.
   <ftp://ftp.humblesoft.com/pub/epag-3.08.tar.gz>
 */

#include "gdevprn.h"

#if GS_VERSION_MAJOR >= 8
#define lprn_dev_proc_image_out(proc)\
    void proc(gx_device_printer *, gp_file *, int, int, int, int)
#else
#define lprn_dev_proc_image_out(proc)\
  void proc(P6(gx_device_printer *, gp_file *, int, int, int, int))
#endif

#define dev_proc_image_out(proc) lprn_dev_proc_image_out(proc)

#define gx_lprn_device_common\
        lprn_dev_proc_image_out((*image_out));\
        bool initialized;\
        bool ManualFeed;\
        bool NegativePrint;\
        bool Tumble;           /* for Duplex */\
        bool RITOff;           /* currently only escpage.dev use this */\
        int prev_x;\
        int prev_y;\
        int BlockLine;\
        byte *ImageBuf;		/* Image Buffer */\
        byte *TmpBuf;           /* Tmporary buffer */\
        byte *CompBuf;		/* Compress buffer */\
        byte *CompBuf2;		/* Compress buffer */\
        int nBw; /* block width (byte,8dot)    */\
        int nBh; /* block height(line)         */\
        struct _Bubble **bubbleTbl;\
        struct _Bubble *freeBubbleList;\
        bool ShowBubble

#define lp_device_body_rest_(print_page_copies, image_out)\
       prn_device_body_rest2_(NULL, print_page_copies, -1),\
       image_out,\
       0/*false*/,    /* initialized */\
       0/*false*/,    /* ManualFeed */\
       0/*false*/,    /* NegativePrint */\
       0/*false*/,    /* Tumble */\
       0/*false*/,    /* RITOff */\
       0, 0,\
       256,\
       NULL, NULL, NULL, NULL,\
       4,\
       32,\
       NULL, NULL,\
       0			/*false */

#define lp_duplex_device_body_rest_(print_page_copies, image_out)\
       prn_device_body_rest2_(NULL, print_page_copies, 0),\
       image_out,\
       0/*false*/,    /* initialized */\
       0/*false*/,    /* ManualFeed */\
       0/*false*/,    /* NegativePrint */\
       0/*false*/,    /* Tumble */\
       0/*false*/,    /* RITOff */\
       0, 0,\
       256,\
       NULL, NULL, NULL, NULL,\
       4,\
       32,\
       NULL, NULL,\
       0			/*false */

#define lprn_device(dtype, procs, dname, xdpi, ydpi, lm, bm, rm, tm, color_bits,\
                    print_page_copies, image_out)\
{        std_device_std_color_full_body(dtype, &procs, dname,\
          (int)((long)(DEFAULT_WIDTH_10THS) * (xdpi) / 10),\
          (int)((long)(DEFAULT_HEIGHT_10THS) * (ydpi) / 10),\
          xdpi, ydpi, color_bits,\
          -(lm) * (xdpi), -(tm) * (ydpi),\
          (lm) * 72.0, (bm) * 72.0,\
          (rm) * 72.0, (tm) * 72.0\
        ),\
       lp_device_body_rest_(print_page_copies, image_out)\
}

#define lprn_duplex_device(dtype, procs, dname, xdpi, ydpi, lm, bm, rm, tm, color_bits,\
                    print_page_copies, image_out)\
{        std_device_std_color_full_body(dtype, &procs, dname,\
          (int)((long)(DEFAULT_WIDTH_10THS) * (xdpi) / 10),\
          (int)((long)(DEFAULT_HEIGHT_10THS) * (ydpi) / 10),\
          xdpi, ydpi, color_bits,\
          -(lm) * (xdpi), -(tm) * (ydpi),\
          (lm) * 72.0, (bm) * 72.0,\
          (rm) * 72.0, (tm) * 72.0\
        ),\
       lp_duplex_device_body_rest_(print_page_copies, image_out)\
}

#define lprn_procs(p_open, p_output_page, p_close)\
  prn_params_procs(p_open, p_output_page, p_close, lprn_get_params, lprn_put_params)

typedef struct _Bubble
{
    struct _Bubble *next;
    gs_int_rect brect;
}
Bubble;

typedef struct gx_device_lprn_s gx_device_lprn;
struct gx_device_lprn_s
{
    gx_device_common;
    gx_prn_device_common;
    gx_lprn_device_common;
};

#define ESC 0x1b		/* \033 */
#define GS  0x1d		/* \035 */

#include "assert_.h"

dev_proc_get_params(lprn_get_params);
dev_proc_put_params(lprn_put_params);

int lprn_print_image(gx_device_printer * pdev, gp_file * fp);
