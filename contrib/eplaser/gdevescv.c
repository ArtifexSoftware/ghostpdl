/* Copyright (C) EPSON SOFTWARE DEVELOPMENT LABORATORY, INC. 1999,2000.
   Copyright (C) SEIKO EPSON CORPORATION 2000-2006,2009.

   Ghostscript printer driver for EPSON ESC/Page and ESC/Page-Color.

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

 SPECIAL THANKS:

    本ドライバの作成にあたり、大森紀人さんの gdevlips, gdevl4v.c を参考に
    させて頂きました。

 NOTES:

  - About Ghostscript 5.10/5.50 BUGS
    Ghostscript 5.10/5.50 の Vector driver の setlinewidth 関数には
    バグがあります。本来スケールが変更されるにしたがって線の太さも変更され
    なければなりませんが、Ghostscript 5.10/5.50 ではスケールを考慮するのを
    忘れています。
    このドライバはそのバグを回避するためにスケールを自分で処理しています。

 */

#include <stdlib.h>		/* for abs() and free */

#include "math_.h"
#ifndef _WIN32
#include <sys/utsname.h>	/* for uname(2) */
#endif
#include <ctype.h>		/* for toupper(3) */

#include "time_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gzpath.h"
#include "gxdevice.h"
#include "gdevvec.h"

#include "gscspace.h"

#include "gdevescv.h"

#define ESCV_FORCEDRAWPATH 0	/* 0: correct LP-9200C path trouble. */

/* ---------------- Device definition ---------------- */

/* Device procedures */
static dev_proc_open_device(escv_open);
static dev_proc_output_page(escv_output_page);
static dev_proc_close_device(escv_close);
static dev_proc_copy_mono(escv_copy_mono);
static dev_proc_copy_color(escv_copy_color);
static dev_proc_put_params(escv_put_params);
static dev_proc_get_params(escv_get_params);
static dev_proc_fill_mask(escv_fill_mask);
static dev_proc_begin_typed_image(escv_begin_typed_image);

gs_public_st_suffix_add0_final(st_device_escv, gx_device_escv,
                               "gx_device_escv", device_escv_enum_ptrs, device_escv_reloc_ptrs,
                               gx_device_finalize, st_device_vector);

/* for ESC/Page-Color
** 原点の値を 0 とした場合,計算誤差？の問題から描画エリアが狂うため
** 原点を 0.001 としておく。
*/
#define escv_device_full_body(dtype, init, dname, stype, w, h, xdpi, ydpi, \
        ncomp, depth, mg, mc, dg, dc, lm, bm, rm, tm)\
        std_device_part1_(dtype, init, dname, stype, open_init_closed),\
        dci_values(ncomp, depth, mg, mc, dg, dc),\
        std_device_part2_(w, h, xdpi, ydpi),\
        offset_margin_values(0.001, 0.001, lm, 0, 0, tm),\
        std_device_part3_()

/* for ESC/Page (Monochrome) */
#define esmv_device_full_body(dtype, init, dname, stype, w, h, xdpi, ydpi, \
        ncomp, depth, mg, mc, dg, dc, lm, bm, rm, tm)\
        std_device_part1_(dtype, init, dname, stype, open_init_closed),\
        dci_values(ncomp, depth, mg, mc, dg, dc),\
        std_device_part2_(w, h, xdpi, ydpi),\
        offset_margin_values(-lm * xdpi / 72.0, -tm * ydpi / 72.0, 5.0 / (MMETER_PER_INCH / POINT),\
        0, 0, 5.0 / (MMETER_PER_INCH / POINT)), /* LPD.2.  041203 saito */\
        std_device_part3_()

/* for ESC/Page-Color */
#define escv_device_body(name) \
{\
  escv_device_full_body(gx_device_escv, escv_initialize_device_procs,\
                        name, &st_device_escv,\
/* width & height */    ESCPAGE_DEFAULT_WIDTH, ESCPAGE_DEFAULT_HEIGHT,\
/* default resolution */X_DPI, Y_DPI,\
/* color info */        3, 24, 255, 255, 256, 256,\
                        ESCPAGE_LEFT_MARGIN_DEFAULT,\
                        ESCPAGE_BOTTOM_MARGIN_DEFAULT,\
                        ESCPAGE_RIGHT_MARGIN_DEFAULT,\
                        ESCPAGE_TOP_MARGIN_DEFAULT),\
  { 0 },\
  escv_init_code\
}

/* for ESC/Page (Monochrome) */
#define esmv_device_body(name) \
{\
  esmv_device_full_body(gx_device_escv, esmv_initialize_device_procs,\
                        name, &st_device_escv,\
/* width & height */    ESCPAGE_DEFAULT_WIDTH, ESCPAGE_DEFAULT_HEIGHT,\
/* default resolution */X_DPI, Y_DPI,\
/* color info */        1, 8, 255, 255, 256, 256,\
                        ESCPAGE_LEFT_MARGIN_DEFAULT,\
                        ESCPAGE_BOTTOM_MARGIN_DEFAULT,\
                        ESCPAGE_RIGHT_MARGIN_DEFAULT,\
                        ESCPAGE_TOP_MARGIN_DEFAULT),\
  { 0 },\
  esmv_init_code\
}

static void
esc_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, open_device, escv_open);
    set_dev_proc(dev, output_page, escv_output_page);
    set_dev_proc(dev, close_device, escv_close);
    set_dev_proc(dev, fill_rectangle, gdev_vector_fill_rectangle);
    set_dev_proc(dev, copy_mono, escv_copy_mono);
    set_dev_proc(dev, copy_color, escv_copy_color);
    set_dev_proc(dev, get_params, escv_get_params);
    set_dev_proc(dev, put_params, escv_put_params);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
    set_dev_proc(dev, fill_path, gdev_vector_fill_path);
    set_dev_proc(dev, stroke_path, gdev_vector_stroke_path);
    set_dev_proc(dev, fill_mask, escv_fill_mask);
    set_dev_proc(dev, fill_trapezoid, gdev_vector_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, gdev_vector_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, gdev_vector_fill_triangle);
    set_dev_proc(dev, begin_typed_image, escv_begin_typed_image);
}

/* for ESC/Page-Color */
static void
escv_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, map_rgb_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_default_rgb_map_color_rgb);
    set_dev_proc(dev, encode_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, gx_default_rgb_map_color_rgb);

    esc_initialize_device_procs(dev);
}

/* for ESC/Page (Monochrome) */
static void
esmv_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, map_rgb_color, gx_default_gray_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_default_gray_map_color_rgb);
    set_dev_proc(dev, encode_color, gx_default_gray_encode_color);
    set_dev_proc(dev, decode_color, gx_default_gray_decode_color);

    esc_initialize_device_procs(dev);
}

#define	escv_init_code_common \
        ESCPAGE_MANUALFEED_DEFAULT, /*   bool          manualFeed;     * Use manual feed *  */\
        ESCPAGE_CASSETFEED_DEFAULT, /*   int           cassetFeed;     * Input Casset *  */\
        ESCPAGE_RIT_DEFAULT,	/*   bool          RITOff;         * RIT Control * */\
        FALSE,			/*   bool          Collate; */\
        0,			/*   int           toner_density; */\
        FALSE,			/*   bool          toner_saving; */\
        0,			/*   int           prev_paper_size; */\
        0,			/*   int           prev_paper_width; */\
        0,			/*   int           prev_paper_height; */\
        0,			/*   int           prev_num_copies; */\
        -1,			/*   int           prev_feed_mode; */\
        0,			/*   int           orientation; */\
        ESCPAGE_FACEUP_DEFAULT,	/*   bool          faceup; */\
        ESCPAGE_MEDIATYPE_DEFAULT, /*   int           MediaType; */\
        0,			/*   bool          first_page; */\
        0,			/*   bool          Duplex; */\
        ESCPAGE_TUMBLE_DEFAULT,	/*   bool          Tumble; */\
        0,			/*   int           ncomp; */\
        0,			/*   int           MaskReverse; */\
        0,			/*   int           MaskState; */\
        TRUE,			/*   bool          c4map;          * 4bit ColorMap * */\
        TRUE,			/*   bool          c8map;          * 8bit ColorMap * */\
        0,			/*   int           prev_x; */\
        0,			/*   int           prev_y; */\
        0,			/*   gx_color_index        prev_color; */\
        0,			/*   gx_color_index        current_color; */\
        3,			/*   double        lwidth; */\
        0,			/*   long          cap; */\
        3,			/*   long          join; */\
        0,			/*   long          reverse_x; */\
        0,			/*   long          reverse_y; */\
       {0,},			/*   gs_matrix     xmat;		* matrix * */\
        0,			/*   int           bx; */\
        0,			/*   int           by; */\
        0,			/*   int           w;		* width * */\
        0,			/*   int           h;		* height * */\
        0,			/*   int           roll; */\
        0,			/*   float         sx;		* scale x * */\
        0,			/*   float         sy;		* scale y * */\
        0,			/*   long          dd; */\
        0,                      /*   int           ispath */\
       {0,},			/*   gx_bitmap_id  id_cache[VCACHE + 1];    * for Font Downloading * */\
       {0,},                    /*   char          JobID[ESCPAGE_JOBID_MAX + 1]; */\
       {0,},                    /*   char          UserName[ESCPAGE_USERNAME_MAX + 1]; */\
       {0,},                    /*   char          HostName[ESCPAGE_HOSTNAME_MAX + 1]; */\
       {0,},                    /*   char          Document[ESCPAGE_DOCUMENT_MAX + 1]; */\
       {0,},			/*   char          Comment[ESCPAGE_COMMENT_MAX + 1]; */\
       {0,0,0},                 /*  gs_param_string gpsJobID; */\
       {0,0,0},                 /*  gs_param_string gpsUserName; */\
       {0,0,0},                 /*  gs_param_string gpsHostName; */\
       {0,0,0},                 /*  gs_param_string gpsDocument; */\
       {0,0,0},                 /*  gs_param_string gpsComment; */\
       false,                   /*  bool            modelJP; */\
       false,                   /*  bool            capFaceUp; */\
       false,                   /*  bool            capDuplexUnit; */\
       RES600                   /*  int             capMaxResolution; */

/* for ESC/Page-Color */
#define	escv_init_code \
        vector_initial_values,\
        1,                      /*   int           colormode;      1=ESC/Page-Color */\
        escv_init_code_common

/* for ESC/Page (Monochrome) */
#define	esmv_init_code \
        vector_initial_values,\
        0,                      /*   int           colormode;      0=ESC/Page(Monochrome) */\
        escv_init_code_common

/* for ESC/Page (Monochrome) */
gx_device_escv far_data gs_epl2050_device = esmv_device_body("epl2050");
gx_device_escv far_data gs_epl2050p_device= esmv_device_body("epl2050p");
gx_device_escv far_data gs_epl2120_device = esmv_device_body("epl2120");
gx_device_escv far_data gs_epl2500_device = esmv_device_body("epl2500");
gx_device_escv far_data gs_epl2750_device = esmv_device_body("epl2750");
gx_device_escv far_data gs_epl5800_device = esmv_device_body("epl5800");
gx_device_escv far_data gs_epl5900_device = esmv_device_body("epl5900");
gx_device_escv far_data gs_epl6100_device = esmv_device_body("epl6100");
gx_device_escv far_data gs_epl6200_device = esmv_device_body("epl6200");
gx_device_escv far_data gs_lp1800_device  = esmv_device_body("lp1800");
gx_device_escv far_data gs_lp1900_device  = esmv_device_body("lp1900");
gx_device_escv far_data gs_lp2200_device  = esmv_device_body("lp2200");
gx_device_escv far_data gs_lp2400_device  = esmv_device_body("lp2400");
gx_device_escv far_data gs_lp2500_device  = esmv_device_body("lp2500");
gx_device_escv far_data gs_lp7500_device  = esmv_device_body("lp7500");
gx_device_escv far_data gs_lp7700_device  = esmv_device_body("lp7700");
gx_device_escv far_data gs_lp7900_device  = esmv_device_body("lp7900");
gx_device_escv far_data gs_lp8100_device  = esmv_device_body("lp8100");
gx_device_escv far_data gs_lp8300f_device = esmv_device_body("lp8300f");
gx_device_escv far_data gs_lp8400f_device = esmv_device_body("lp8400f");
gx_device_escv far_data gs_lp8600_device  = esmv_device_body("lp8600");
gx_device_escv far_data gs_lp8600f_device = esmv_device_body("lp8600f");
gx_device_escv far_data gs_lp8700_device  = esmv_device_body("lp8700");
gx_device_escv far_data gs_lp8900_device  = esmv_device_body("lp8900");
gx_device_escv far_data gs_lp9000b_device = esmv_device_body("lp9000b");
gx_device_escv far_data gs_lp9100_device  = esmv_device_body("lp9100");
gx_device_escv far_data gs_lp9200b_device = esmv_device_body("lp9200b");
gx_device_escv far_data gs_lp9300_device  = esmv_device_body("lp9300");
gx_device_escv far_data gs_lp9400_device  = esmv_device_body("lp9400");
gx_device_escv far_data gs_lp9600_device  = esmv_device_body("lp9600");
gx_device_escv far_data gs_lp9600s_device = esmv_device_body("lp9600s");
gx_device_escv far_data gs_lps4500_device = esmv_device_body("lps4500");
gx_device_escv far_data gs_eplmono_device = esmv_device_body(ESCPAGE_DEVICENAME_MONO);

/* for ESC/Page-Color */
gx_device_escv far_data gs_alc1900_device = escv_device_body("alc1900");
gx_device_escv far_data gs_alc2000_device = escv_device_body("alc2000");
gx_device_escv far_data gs_alc4000_device = escv_device_body("alc4000");
gx_device_escv far_data gs_alc4100_device = escv_device_body("alc4100");
gx_device_escv far_data gs_alc8500_device = escv_device_body("alc8500");
gx_device_escv far_data gs_alc8600_device = escv_device_body("alc8600");
gx_device_escv far_data gs_alc9100_device = escv_device_body("alc9100");
gx_device_escv far_data gs_lp3000c_device = escv_device_body("lp3000c");
gx_device_escv far_data gs_lp8000c_device = escv_device_body("lp8000c");
gx_device_escv far_data gs_lp8200c_device = escv_device_body("lp8200c");
gx_device_escv far_data gs_lp8300c_device = escv_device_body("lp8300c");
gx_device_escv far_data gs_lp8500c_device = escv_device_body("lp8500c");
gx_device_escv far_data gs_lp8800c_device = escv_device_body("lp8800c");
gx_device_escv far_data gs_lp9000c_device = escv_device_body("lp9000c");
gx_device_escv far_data gs_lp9200c_device = escv_device_body("lp9200c");
gx_device_escv far_data gs_lp9500c_device = escv_device_body("lp9500c");
gx_device_escv far_data gs_lp9800c_device = escv_device_body("lp9800c");
gx_device_escv far_data gs_lps6500_device = escv_device_body("lps6500");
gx_device_escv far_data gs_eplcolor_device= escv_device_body(ESCPAGE_DEVICENAME_COLOR);

/* Vector device implementation */
/* Page management */
static int escv_beginpage (gx_device_vector * vdev);
/* Imager state */
static int escv_setlinewidth (gx_device_vector * vdev, double width);
static int escv_setlinecap (gx_device_vector * vdev, gs_line_cap cap);
static int escv_setlinejoin (gx_device_vector * vdev, gs_line_join join);
static int escv_setmiterlimit (gx_device_vector * vdev, double limit);
static int escv_setdash (gx_device_vector * vdev, const float *pattern,
                          uint count, double offset);
static int escv_setflat (gx_device_vector * vdev, double flatness);
static int escv_setlogop (gx_device_vector * vdev, gs_logical_operation_t lop,
                           gs_logical_operation_t diff);
/* Other state */
static bool escv_can_handle_hl_color (gx_device_vector * vdev, const gs_gstate * pgs,
                                       const gx_drawing_color * pdc);
static int escv_setfillcolor (gx_device_vector * vdev, const gs_gstate * pgs,
                               const gx_drawing_color * pdc);
static int escv_setstrokecolor (gx_device_vector * vdev, const gs_gstate * pgs,
                                 const gx_drawing_color * pdc);
/* Paths */
/* dopath and dorect are normally defaulted */
static int escv_vector_dopath (gx_device_vector * vdev, const gx_path * ppath,
                                gx_path_type_t type, const gs_matrix *pmat);
static int escv_vector_dorect (gx_device_vector * vdev, fixed x0, fixed y0, fixed x1,
                                fixed y1, gx_path_type_t type);
static int escv_beginpath (gx_device_vector * vdev, gx_path_type_t type);
static int escv_moveto (gx_device_vector * vdev, double x0, double y0,
                         double x, double y, gx_path_type_t type);
static int escv_lineto (gx_device_vector * vdev, double x0, double y0,
                         double x, double y, gx_path_type_t type);
static int escv_curveto (gx_device_vector * vdev, double x0, double y0,
                          double x1, double y1, double x2, double y2,
                          double x3, double y3, gx_path_type_t type);
static int escv_closepath (gx_device_vector * vdev, double x0, double y0,
                            double x_start, double y_start, gx_path_type_t type);
static int escv_endpath (gx_device_vector * vdev, gx_path_type_t type);


static const gx_device_vector_procs escv_vector_procs =
  {
    /* Page management */
    escv_beginpage,
    /* Imager state */
    escv_setlinewidth,
    escv_setlinecap,
    escv_setlinejoin,
    escv_setmiterlimit,
    escv_setdash,
    escv_setflat,
    escv_setlogop,
    /* Other state */
    escv_can_handle_hl_color,	/* add gs815 */
    escv_setfillcolor,	/* fill & stroke colors are the same */
    escv_setstrokecolor,
    /* Paths */
    escv_vector_dopath,
    escv_vector_dorect,
    escv_beginpath,
    escv_moveto,
    escv_lineto,
    escv_curveto,
    escv_closepath,
    escv_endpath
  };

static void escv_write_begin(gx_device *dev, int bits, int x, int y, int sw, int sh, int dw, int dh, int roll);
static void escv_write_data(gx_device *dev, int bits, byte *buf, int bsize, int w, int ras);
static void escv_write_end(gx_device *dev, int bits);

/* ---------------- Utilities ---------------- */

/* Put a string on a stream.
   This function is copy of `pputs' in gdevpstr.c */
static int
lputs(stream * s, const char *str)
{
  uint	len = strlen(str);
  uint	used;
  int		status;

  status = sputs(s, (const byte *)str, len, &used);

  return (status >= 0 && used == len ? 0 : EOF);
}

/* Write a string on a stream. */
static void
put_bytes(stream * s, const byte * data, uint count)
{
  uint used;

  sputs(s, data, count, &used);
}

static int
escv_range_check(gx_device * dev)
{
  int width = dev->MediaSize[0];
  int height = dev->MediaSize[1];
  int xdpi = dev->x_pixels_per_inch;
  int ydpi = dev->y_pixels_per_inch;

  /* Paper Size Check */
  if (width <= height) {	/* portrait */
    if ((width < ESCPAGE_WIDTH_MIN ||
         width > ESCPAGE_WIDTH_MAX ||
         height < ESCPAGE_HEIGHT_MIN ||
         height > ESCPAGE_HEIGHT_MAX)) {
      return_error(gs_error_rangecheck);
    }
  } else {			/* landscape */
    if ((width < ESCPAGE_HEIGHT_MIN ||
         width > ESCPAGE_HEIGHT_MAX ||
         height < ESCPAGE_WIDTH_MIN ||
         height > ESCPAGE_WIDTH_MAX )) {
      return_error(gs_error_rangecheck);
    }
  }

  /* Resolution Check */
  if (xdpi != ydpi) {
    return_error(gs_error_rangecheck);
  }

  if ((xdpi < ESCPAGE_DPI_MIN ||
       xdpi > ESCPAGE_DPI_MAX)) {
    return_error(gs_error_rangecheck);
  }

  return 0;			/* pass */
}

/* ---------------- Vector device implementation ---------------- */

static int
escv_vector_dopath(gx_device_vector * vdev, const gx_path * ppath,
                   gx_path_type_t type, const gs_matrix *pmat
                   )
{
  gx_device_escv *pdev = (gx_device_escv *) vdev;
  bool do_close = (type & gx_path_type_stroke) != 0;
  gs_fixed_rect rect;
  gs_point scale;
  double x_start = 0, y_start = 0;
  bool first = true;
  gs_path_enum cenum;
  int code;

  stream	*s = gdev_vector_stream(vdev);
  char	obuf[128];

  if (gx_path_is_rectangle(ppath, &rect))
    return (*vdev_proc(vdev, dorect)) (vdev, rect.p.x, rect.p.y, rect.q.x, rect.q.y, type);
  scale = vdev->scale;
  code = (*vdev_proc(vdev, beginpath)) (vdev, type);
  gx_path_enum_init(&cenum, ppath);

  for (;;) {
    double	x, y;
    fixed	vs[6];
    int	pe_op, cnt;
    const segment *pseg;

    pe_op = gx_path_enum_next(&cenum, (gs_fixed_point *) vs);

   sw:
    switch (pe_op) {
    case 0:		/* done */
      return (*vdev_proc(vdev, endpath)) (vdev, type);

    case gs_pe_moveto:
      x = fixed2float(vs[0]) / scale.x;
      y = fixed2float(vs[1]) / scale.y;

      /* サブパス開始命令 p1 */
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "0;%d;%dmvpG", (int)x, (int)y);
      lputs(s, obuf);

      if (first)
        x_start = x, y_start = y, first = false;
      break;

    case gs_pe_lineto:
      cnt = 1;
      for (pseg = cenum.pseg; pseg != 0 && pseg->type == s_line; cnt++, pseg = pseg->next);

      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "0;%d", cnt);
      lputs(s, obuf);

      do {
        (void)gs_snprintf(obuf, sizeof(obuf), ";%d;%d",
                          (int)(fixed2float(vs[0]) / scale.x),
                          (int)(fixed2float(vs[1]) / scale.y));
        lputs(s, obuf);

        pe_op = gx_path_enum_next(&cenum, (gs_fixed_point *) vs);
      } while (pe_op == gs_pe_lineto);

      /* パス・ポリライン命令 */
      lputs(s, "lnpG");
      pdev->ispath = 1;

      goto sw;

    case gs_pe_curveto:
      cnt = 1;
      for (pseg = cenum.pseg; pseg != 0 && pseg->type == s_curve; cnt++, pseg = pseg->next);
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "0;%d", cnt * 3);
      lputs(s, obuf);

      do {
        (void)gs_snprintf(obuf, sizeof(obuf), ";%d;%d;%d;%d;%d;%d",
                    (int)(fixed2float(vs[0]) / scale.x), (int)(fixed2float(vs[1]) / scale.y),
                    (int)(fixed2float(vs[2]) / scale.x), (int)(fixed2float(vs[3]) / scale.y),
                    (int)(fixed2float(vs[4]) / scale.x), (int)(fixed2float(vs[5]) / scale.y));
        lputs(s, obuf);

        pe_op = gx_path_enum_next(&cenum, (gs_fixed_point *) vs);
      } while (pe_op == gs_pe_curveto);

      /* ベジェ曲線 */
      lputs(s, "bzpG");
      pdev->ispath = 1;

      goto sw;

    case gs_pe_closepath:
      x = x_start, y = y_start;
      if (do_close) {
        lputs(s, ESC_GS "clpG");
        break;
      }

      pe_op = gx_path_enum_next(&cenum, (gs_fixed_point *) vs);
      if (pe_op != 0) {
        lputs(s, ESC_GS "clpG");

        if (code < 0)
          return code;
        goto sw;
      }
      return (*vdev_proc(vdev, endpath)) (vdev, type);
    default:		/* can't happen */
      return_error(gs_error_unknownerror);
    }
    if (code < 0)
      return code;
  }
}

static int
escv_vector_dorect(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1,
                   fixed y1, gx_path_type_t type)
{
  gx_device_escv *pdev = (gx_device_escv *) vdev;
  int		code;
  char	obuf[128];
  gs_point	scale;
  stream	*s = gdev_vector_stream(vdev);

  code = (*vdev_proc(vdev, beginpath))(vdev, type);
  if (code < 0)
    return code;

  scale = vdev->scale;

  (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "0;%d;%d;%d;%d;0;0rrpG",
                (int)(fixed2float(x0) / scale.x),
                (int)(fixed2float(y0) / scale.y),
                (int)(fixed2float(x1) / scale.x),
                (int)(fixed2float(y1) / scale.y));
  lputs(s, obuf);
  pdev->ispath = 1;

#if 0
  /* Ghostscript 側のバグで closepath を呼んでいないので処理を会わせる。 */

  /* 本来は (*vdev_proc(vdev, closepath))() を呼ぶべき */
  lputs(s, ESC_GS "clpG");
#endif

  return (*vdev_proc(vdev, endpath))(vdev, type);
}

/* ---------- */

static const EPaperTable ePaperTable[NUM_OF_PAPER_TABLES] =
  {
    {933, 1369, 72, "A3PLUS"},	/* A3 NOBI */
    {842, 1191, 13, "A3"},	/* A3 */
    {792, 1224, 36, "B"},	/* Ledger */
    {729, 1032, 24, "B4"},	/* B4 JIS */
    {709, 1001, 24, "B4"},	/* B4 */
    {612, 1008, 32, "LGL"},	/* Legal */
    {612,  936, 34, "GLG"},	/* Government Letter */ /* LPD.1. */
    {612,  792, 30, "LT"},	/* Letter */
    {595,  935, 37, "F4"},	/* F4 */
    {595,  842, 14, "A4"},	/* A4 */
    {576,  756, 35, "GLT"},	/* Government Legal */ /* LPD.1. */
    {522,  756, 33, "EXE"},	/* Executive */
    {516,  729, 25, "B5"},	/* B5 JIS */
    {499,  709, 99, "IB5"},	/* Envelope ISO B5 */
    {459,  649, 91, "C5"},	/* Envelope C5 */
    {420,  595, 15, "A5"},	/* A5 */
    {396,  612, 31, "HLT"},	/* Half Letter */
    {312,  624, 90, "DL"},	/* DL */
    {298,  666, 64, "YOU4"},	/* Japanese Envelope You4 */
    {297,  684, 81, "C10"},	/* Commercial 10 */
    {283,  420, 38, "POSTCARD"},/* PostCard */
    {279,  540, 80, "MON"},	/* Monarch */
    {  0,    0, -1, ""}		/* Undefined */
  };

static const EPaperTable *
escv_checkpapersize(gx_device_vector * vdev)
{
  gx_device_escv	*const pdev = (gx_device_escv *)vdev;
  int			devw, devh;
  paper_candidate	candidate[NUM_OF_PAPER_TABLES];
  int			num_candidate;

  if (pdev->MediaSize[0] < pdev->MediaSize[1]) {
    /* portrait */
    devw = pdev->MediaSize[0];
    devh = pdev->MediaSize[1];
  } else {
    /* landscape */
    devw = pdev->MediaSize[1];
    devh = pdev->MediaSize[0];
  }

  /* pick up papersize candidate */
  {
    const EPaperTable *pt;
    int delta;

    num_candidate = 0;

    for (delta = 0; delta <= MAX_PAPER_SIZE_DELTA; delta++) {
      for (pt = ePaperTable; 0 <= pt->escpage; pt++) {
        if ( (pt->width  + delta) >= devw &&
             (pt->width  - delta) <= devw &&
             (pt->height + delta) >= devh &&
             (pt->height - delta) <= devh) {

          candidate[num_candidate].paper = pt;
          candidate[num_candidate].absw = abs(pt->width  - devw);
          candidate[num_candidate].absh = abs(pt->height - devh);
          candidate[num_candidate].score = 0;
          candidate[num_candidate].isfillw = false;
          candidate[num_candidate].isfillh = false;
          candidate[num_candidate].isminw  = false;
          candidate[num_candidate].isminh  = false;

          if( 0 <= (pt->width - devw) ){
            candidate[num_candidate].isfillw = true;
          }
          if( 0 <= (pt->height - devh) ){
            candidate[num_candidate].isfillh = true;
          }
          num_candidate++;
        }
      }
      if ( 0 < num_candidate ) {
        break;
      }
    }
  }

  /* no papersize match, so use default paper size */
  if ( 0 == num_candidate  ) {
    return (const EPaperTable *)0;		/* not found */
  }

  if ( 1 == num_candidate ) {
    return candidate[0].paper; /* find */
  }

  /* search abstruct minw & minh */
  {
    int absminw;
    int absminh;
    int i;

    absminw = candidate[0].absw;
    absminh = candidate[0].absh;
    for (i = 1; i < num_candidate; i++) {
      if (absminw > candidate[i].absw) {
        absminw = candidate[i].absw;
      }
      if (absminh > candidate[i].absh) {
        absminh = candidate[i].absh;
      }
    }

    /* check isminw & isminh flag */
    for (i = 0; i < num_candidate; i++) {
      if (absminw == candidate[i].absw) {
        candidate[i].isminw = true;
      }
      if (absminh == candidate[i].absh) {
        candidate[i].isminh = true;
      }
    }

    /* add score */
    for (i = 0; i < num_candidate; i++) {
      if (candidate[i].isminw == true) {
        candidate[i].score += 100;
      }
      if (candidate[i].isminh == true) {
        candidate[i].score += 100;
      }
      if (candidate[i].isfillw == true) {
        candidate[i].score += 10;
      }
      if (candidate[i].isfillh == true) {
        candidate[i].score += 10;
      }
      if (absminw < absminh) {
        if (candidate[i].isminw == true) {
          candidate[i].score += 1;
        }
      } else {
        if (candidate[i].isminh == true) {
          candidate[i].score += 1;
        }
      }
    }
  }

  /* select highest score papersize */
  {
    int best_candidate;
    int i;

    best_candidate = 0;
    for (i = 1; i < num_candidate; i++) {
      if ( candidate[best_candidate].score <= candidate[i].score ) {
        best_candidate = i;
      }
    }
    return candidate[best_candidate].paper;
  }
}

static char *
get_sysname ( void )
{
#ifdef _WIN32
  return strdup("BOGUS");
#else
  char *result = NULL;
  struct utsname utsn;

  if (0 == uname (&utsn))
    {
      result = strdup (utsn.sysname);
    }
  return result;
#endif
}

/* EPSON printer model name translation.
  return: 0 unknown model, not translated.
          1 completed.
          -1 error.  ... This value not return now.
 */
static int
trans_modelname ( char *dest, const char * src, size_t dest_len )
{
  const char *cp = src;

  dest[0] = '\0';

  if ( 0 == strncmp( cp, "epl", 3 ) ) {
    strcat( dest, "EPSON EPL-" );
    cp = &cp[3];
  } else if ( 0 == strncmp( cp, "al", 2 ) ) {
    strcat( dest, "EPSON AL-" );
    cp = &cp[2];
  } else if ( 0 == strncmp( cp, "lp", 2 ) ) {
    strcat( dest, "EPSON LP-" );
    cp = &cp[2];
  } else {
    strncpy( dest, src, dest_len );
    dest[dest_len] = '\0';
    return 0;
  }

  {
    char * pdest = strchr( dest, '\0' );
    size_t len = strlen( dest );

    while ( ( len < (dest_len -1) ) && *cp && ( '_' != *cp) ) {
      *pdest = toupper( *cp );
      pdest++;
      cp++;
    }
    *pdest = '\0';
  }

  return 1;
}

static int
escv_beginpage(gx_device_vector * vdev)
{
  gx_device_escv	*const pdev = (gx_device_escv *)vdev;

  if (pdev -> first_page) {

    /* not use gdev_vector_stream */
    stream		*s = vdev->strm;
    char		ebuf[1024];
    int                 MaxRes;
    int                 Local;
    int                 Duplex;
    int                 FaceUp;

    const struct {
      const char *name;
      const int resolution;
      const int locale;
      const int duplex;
      const int faceup;
    } model_resource[] = {
      /* model, resolution, loca,deplex,faceup */
      { "alc1900", RES600,  ENG, TRUE,  FALSE },
      { "alc2000", RES600,  ENG, TRUE,  FALSE },
      { "alc4000", RES1200, ENG, TRUE,  FALSE },
      { "alc4100", RES600,  ENG, TRUE,  FALSE },
      { "alc8500", RES600,  ENG, TRUE,  TRUE },
      { "alc8600", RES600,  ENG, TRUE,  TRUE },
      { "alc9100", RES600,  ENG, TRUE,  TRUE },
      { "epl2050", RES1200, ENG, TRUE,  FALSE },
      { "epl2050p",RES1200, ENG, TRUE,  FALSE },
      { "epl2120", RES1200, ENG, TRUE,  FALSE },
      { "epl2500", RES600,  ENG, TRUE,  FALSE },
      { "epl2750", RES600,  ENG, TRUE,  FALSE },
      { "epl5800", RES1200, ENG, FALSE, FALSE },
      { "epl5900", RES1200, ENG, FALSE, FALSE },
      { "epl6100", RES1200, ENG, FALSE, FALSE },
      { "epl6200", RES1200, ENG, FALSE, FALSE },
      { "lp1800",  RES600,  JPN, FALSE, FALSE },
      { "lp1900",  RES1200, JPN, FALSE, FALSE },
      { "lp2200",  RES1200, JPN, FALSE, FALSE },
      { "lp2400",  RES1200, JPN, FALSE, FALSE },
      { "lp2500",  RES1200, JPN, FALSE, FALSE },
      { "lp3000c", RES600,  JPN, TRUE,  FALSE },
      { "lp7500",  RES600,  JPN, TRUE,  FALSE },
      { "lp7700",  RES600,  JPN, TRUE,  FALSE },
      { "lp7900",  RES600,  JPN, TRUE,  FALSE },
      { "lp8000c", RES600,  JPN, FALSE, TRUE },
      { "lp8100",  RES600,  JPN, TRUE,  FALSE },
      { "lp8200c", RES600,  JPN, FALSE, TRUE },
      { "lp8300c", RES600,  JPN, TRUE,  TRUE },
      { "lp8300f", RES600,  JPN, TRUE,  FALSE },
      { "lp8400f", RES600,  JPN, TRUE,  FALSE },
      { "lp8500c", RES600,  JPN, TRUE,  TRUE },
      { "lp8600",  RES600,  JPN, TRUE,  FALSE },
      { "lp8600f", RES600,  JPN, TRUE,  FALSE },
      { "lp8700",  RES1200, JPN, TRUE,  FALSE },
      { "lp8800c", RES600,  JPN, TRUE,  TRUE },
      { "lp8900",  RES600,  JPN, TRUE,  FALSE },
      { "lp9000b", RES600,  JPN, TRUE,  FALSE },
      { "lp9000c", RES600,  JPN, TRUE,  FALSE },
      { "lp9100",  RES600,  JPN, TRUE,  FALSE },
      { "lp9200b", RES600,  JPN, TRUE,  FALSE },
      { "lp9200c", RES600,  JPN, TRUE,  FALSE },
      { "lp9300",  RES600,  JPN, TRUE,  FALSE },
      { "lp9400",  RES600,  JPN, TRUE,  FALSE },
      { "lp9500c", RES600,  JPN, TRUE,  TRUE },
      { "lp9600",  RES600,  JPN, TRUE,  FALSE },
      { "lp9600s", RES600,  JPN, TRUE,  FALSE },
      { "lp9800c", RES600,  JPN, TRUE,  TRUE },
      { "lps4500", RES600,  JPN, TRUE,  FALSE },
      { "lps6500", RES600,  JPN, TRUE,  FALSE },
      { "",        -1,      -1,  FALSE, FALSE }
    };

    /* set default */
    MaxRes = RES600;
    Local  = JPN;
    Duplex = FALSE;
    FaceUp = FALSE;

    if ( !*pdev->JobID )
      strcpy(pdev->JobID, "0");

    lputs(s, "\033\001@EJL \012");

    lputs(s, "@EJL SJ ID=\"");
    lputs(s, pdev->JobID);
    lputs(s, "\"\012");

    lputs(s, "@EJL JI ID=\"");
    lputs(s, pdev->JobID);
    lputs(s, "\"");

    {
      time_t t;

#ifdef CLUSTER
      memset(&t, 0, sizeof(t));
#else
      time(&t);
#endif

      lputs(s, " DATE=\"");
      {
        struct tm *tm;
        char   str[32];
        size_t i;

#ifdef CLUSTER
        memset(&tm, 0, sizeof(tm));
        strcpy(str, "1970/01/01 00:00:00");
        i = strlen(str);
#else
        tm =  localtime( &t );
        i = strftime(str, 30, "%Y/%m/%d %H:%M:%S", tm);
#endif
        if ( 30 >= i )
          str[i] = '\0';

        lputs(s, str);
      }
      lputs(s, "\"");

      lputs(s, "\012");
    }

    lputs(s, "@EJL JI");
    {
      lputs(s, " USER=\"");
      if ( *pdev->UserName )
        lputs(s, pdev->UserName);
      lputs(s, "\"");

      lputs(s, " MACHINE=\"");
      if ( *pdev->HostName )
        lputs(s, pdev->HostName);
      lputs(s, "\"");

      lputs(s, " DOCUMENT=\"");
      if ( *pdev->Document )
        lputs(s, pdev->Document);
      lputs(s, "\"");
    }
    lputs(s, "\012");

    lputs(s, "@EJL JI OS=\"");
    {
      char *sysname = get_sysname ();
      if (sysname)
        {
          lputs(s, sysname );
          free(sysname);
          sysname = NULL;
        }
    }
    lputs(s, "\"\012");

    if ( ( 0 == strcmp( pdev->dname, ESCPAGE_DEVICENAME_COLOR ) ) ||
         ( 0 == strcmp( pdev->dname, ESCPAGE_DEVICENAME_MONO  ) ) )
      {
        Local = pdev->modelJP;
        FaceUp = pdev->capFaceUp;
        Duplex = pdev->capDuplexUnit;
        MaxRes = pdev->capMaxResolution;

        lputs(s, "@EJL JI DRIVER=\"");
        lputs(s, pdev->dname);
        lputs(s, "\"\012");

        lputs(s, "@EJL JI PRINTER=\"");
        lputs(s, pdev->dname);
        lputs(s, "\"\012");

      } else {

      char _modelname[ ESCPAGE_MODELNAME_MAX + 1 ] = {0,};
      const char *modelname;
      int i;

      modelname = pdev->dname;

      for ( i = 0; (-1) != model_resource[i].resolution; i++ ) {
        if ( 0 == strcmp( pdev->dname, model_resource[i].name ) )
          break;
      }

      lputs(s, "@EJL JI DRIVER=\"");
      if ( (-1) != model_resource[i].resolution ) {
        MaxRes = model_resource[i].resolution;
        Local  = model_resource[i].locale;
        Duplex = model_resource[i].duplex;
        FaceUp = model_resource[i].faceup;

        if ( 0 <= trans_modelname( _modelname, model_resource[i].name, ESCPAGE_MODELNAME_MAX ) )
            modelname = _modelname;

        lputs(s, modelname);
      } else {
        lputs(s, "Ghostscript");
      }
      lputs(s, "\"\012");

      lputs(s, "@EJL JI PRINTER=\"");
      lputs(s, modelname);
      lputs(s, "\"\012");
    }

    if ( *pdev->Comment ) {
      lputs(s, "@EJL CO ");
      lputs(s, pdev->Comment );
      lputs(s, "\012");
    }

    lputs(s, "@EJL SE LA=ESC/PAGE\012");

    lputs(s, "@EJL SET");

    /* Resolusion */
    if (vdev->x_pixels_per_inch == 1200){
      if (MaxRes == 1200){
        lputs(s, " RS=1200");
      } else {
        lputs(s, " RS=FN");
      }
    } else if (vdev->x_pixels_per_inch == 600) {
      lputs(s, " RS=FN");
    } else {
      lputs(s, " RS=QK");
    }

    /* Output Unit */
    if ((pdev->faceup && FaceUp) || (pdev->MediaType && FaceUp)) {
      lputs(s, " OU=FU");
    } else {
      lputs(s, " OU=FD");
    }

    /* Paper unit */
    if (pdev->MediaType){
      if (Local == ENG){
        lputs(s, " PU=1");
      } else {
        lputs(s, " PU=15");
      }
    }else{
      if (pdev->manualFeed) {
        if (Local == ENG){
          lputs(s, " PU=1");
        } else {
          lputs(s, " PU=15");
        }
      } else if (pdev->cassetFeed) {
        (void)gs_snprintf(ebuf, sizeof(ebuf), " PU=%d", pdev->cassetFeed);
        lputs(s, ebuf);
      } else {
        lputs(s, " PU=AU");
      }
    }

    if (Duplex && pdev->Duplex) {
      /* Duplex ON */
      lputs(s, " DX=ON");

      /* binding type */
      if (pdev->Tumble) {
        lputs(s, " BD=SE");
      } else {
        lputs(s, " BD=LE");
      }
    } else {
      /* Duplex off */
      lputs(s, " DX=OFF");
    }

    /* Number of copies */
    if (pdev->NumCopies) {
      if (pdev->NumCopies >= 1000) {
        pdev->NumCopies = 999;
      }

      /* lp8000c not have QT */
      if (strcmp(pdev->dname, "lp8000c") == 0) {
        (void)gs_snprintf(ebuf, sizeof(ebuf), " QT=1 CO=%d", pdev->NumCopies);
      } else {
        if (pdev->Collate) {
          /* CO is 1, when set QT */
          (void)gs_snprintf(ebuf, sizeof(ebuf), " QT=%d CO=1", pdev->NumCopies);
        } else {
          /* QT is 1, when not specified QT */
          (void)gs_snprintf(ebuf, sizeof(ebuf), " QT=1 CO=%d", pdev->NumCopies);
        }
      }
      lputs(s, ebuf);
    } else {
      lputs(s, " QT=1 CO=1");
    }

    if (pdev->toner_density) {
      (void)gs_snprintf(ebuf, sizeof(ebuf), " DL=%d", pdev->toner_density);
      lputs(s, ebuf);
    }

    if (pdev->orientation) {
      lputs(s, " OR=LA");
    }

    if (pdev->toner_saving) {
      lputs(s, " SN=ON");
    }

    if (pdev->RITOff) {
      lputs(s, " RI=OFF");
    } else {
      lputs(s, " RI=ON");
    }

    if        (pdev->MediaType == 0) { lputs(s, " PK=NM");
    } else if (pdev->MediaType == 1) { lputs(s, " PK=TH");
    } else if (pdev->MediaType == 2) { lputs(s, " PK=TR");
    } else if (pdev->MediaType == 3) { lputs(s, " PK=TN");
    } else if (pdev->MediaType == 4) { lputs(s, " PK=LH");
    } else if (pdev->MediaType == 5) { lputs(s, " PK=CT");
    } else if (pdev->MediaType == 6) { lputs(s, " PK=ET");
    } else if (pdev->MediaType == 7) { lputs(s, " PK=HQ");
    } else if (pdev->MediaType == 8) { lputs(s, " PK=UT");
    } else if (pdev->MediaType == 9) { lputs(s, " PK=UM");
    } else {
      lputs(s, " PK=NM");
    }

    lputs(s, " PS=");
    {
      const EPaperTable *pt;

      pt = escv_checkpapersize(vdev);
      if ( 0 == pt ) {
        lputs(s, "A4");
      } else {
        lputs(s, pt->name);
      }
    }

    if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

#define	START_CODE1	ESC_GS "1tsE" ESC_GS "1owE" ESC_GS "0alfP" ESC_GS "0affP" ESC_GS "0;0;0clfP" ESC_GS "0pmP" ESC_GS "1024ibI" ESC_GS "2cmE" ESC_GS "0bcI" ESC_GS "1;10mlG"

#define	STRAT_CODE	ESC_GS "1mmE" ESC_GS "1csE"

      lputs(s, " ZO=OFF EC=ON SZ=OFF SL=YES TO=0.0MM LO=0.0MM\012");
      lputs(s, "@EJL EN LA=ESC/PAGE\012");

      lputs(s, ESC_GS "rhE");

      lputs(s, STRAT_CODE);

      if (vdev->x_pixels_per_inch == 1200) {
        /* 1200 dpi */
        lputs(s, ESC_GS "0;0.06muE");
        lputs(s, ESC_GS "1;45;156htmE");
        lputs(s, ESC_GS "9;1200;1200drE" ESC_GS "2;1200;1200drE" ESC_GS "1;1200;1200drE" ESC_GS "0;1200;1200drE");
        lputs(s, ESC_GS "1;1;raE");
      } else if (vdev->x_pixels_per_inch == 600) {
        /* 600 dpi */
        lputs(s, ESC_GS "0;0.12muE");
        lputs(s, ESC_GS "1;45;106htmE");
        lputs(s, ESC_GS "9;600;600drE" ESC_GS "2;600;600drE" ESC_GS "1;600;600drE" ESC_GS "0;600;600drE");
      } else {
        /* 300 dpi */
        lputs(s, ESC_GS "0;0.24muE");
        lputs(s, ESC_GS "1;45;71htmE");
        lputs(s, ESC_GS "9;300;300drE" ESC_GS "2;300;300drE" ESC_GS "1;300;300drE" ESC_GS "0;300;300drE");
      }

      lputs(s, START_CODE1);

      lputs(s, ESC_GS "0sarG");		/* 絶対座標指定 */
      /*	lputs(s, ESC_GS "1owE");*/

    } else {			/* ESC/Page-Color */

#define	COLOR_START_CODE1	ESC_GS "1tsE" ESC_GS "0alfP" ESC_GS "0affP" ESC_GS "0;0;0clfP" ESC_GS "0pmP" ESC_GS "1024ibI" ESC_GS "2cmE" ESC_GS "0bcI" ESC_GS "1;10mlG"

#define	LP8000_CODE	ESC_GS "0pddO" ESC_GS "0;0mmE" ESC_GS "2csE" ESC_GS "0;1;3cmmE" ESC_GS "0;1raE" ESC_GS "0;2;4ccmE"

#define	LP8200_CODE	ESC_GS "0pddO" ESC_GS "0;0cmmE" ESC_GS "1;2;3ccmE" ESC_GS "2;2;3ccmE" ESC_GS "3;2;4ccmE" ESC_GS "1;1raE" ESC_GS "2;1raE" ESC_GS "3;1raE"

      lputs(s, " ZO=OFF EC=ON SZ=OFF SL=YES TO=0 LO=0\012");
      lputs(s, "@EJL EN LA=ESC/PAGE-COLOR\012");

      lputs(s, ESC_GS "rhE");

      if (strcmp(vdev -> dname, "lp8000c") == 0) {
        lputs(s, LP8000_CODE);
      } else {
        lputs(s, LP8200_CODE);
      }

      put_bytes(s, (const byte *)ESC_GS "7;0;2;0cam{E\012\000\000\000\000\000\000", 20);
      lputs(s, ESC_GS "0;0cmmE");

      if (vdev->x_pixels_per_inch == 1200) {
        /* 1200 dpi */
        lputs(s, ESC_GS "0;0.06muE");
        lputs(s, ESC_GS "3;1200;1200drE" ESC_GS "2;1200;1200drE" ESC_GS "1;1200;1200drE" ESC_GS "0;1200;1200drE");
      } else if (vdev->x_pixels_per_inch == 600) {
        /* 600 dpi */
        lputs(s, ESC_GS "0;0.12muE");
        lputs(s, ESC_GS "3;600;600drE" ESC_GS "2;600;600drE" ESC_GS "1;600;600drE" ESC_GS "0;600;600drE");
      } else {
        /* 300 dpi */
        lputs(s, ESC_GS "0;0.24muE");
        lputs(s, ESC_GS "3;300;300drE" ESC_GS "2;300;300drE" ESC_GS "1;300;300drE" ESC_GS "0;300;300drE");
      }
      lputs(s, ESC_GS "0;0loE");

/*    lputs(s, ESC_GS "0poE"); *//* for TEST */

      lputs(s, COLOR_START_CODE1);
      lputs(s, ESC_GS "8;1;2;2;2plr{E");
      put_bytes(s, (const byte *)"\377\377\377\377\000\000\000\000", 8);

      lputs(s, ESC_GS "0sarG");		/* 絶対座標指定 */
      lputs(s, ESC_GS "2;204wfE");		/* rop 指定 */

    }	/* ESC/Page-Color */

  }

  return 0;
}

static int
escv_setlinewidth(gx_device_vector * vdev, double width)
{
  stream			*s = gdev_vector_stream(vdev);
  gx_device_escv *const	pdev = (gx_device_escv *) vdev;
  char			obuf[64];

  if (width < 1) width = 1;

  /* ESC/Page では線幅／終端／接合部の設定は１つのコマンドになっているため保持しておく。 */
  pdev -> lwidth = width;

  (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%d;%d;%dlwG",
                (int)(pdev -> lwidth),
                (int)(pdev -> cap),
                (int)(pdev -> join));
  lputs(s, obuf);

  return 0;
}

static int
escv_setlinecap(gx_device_vector * vdev, gs_line_cap cap)
{
  stream			*s = gdev_vector_stream(vdev);
  gx_device_escv *const	pdev = (gx_device_escv *) vdev;
  char			obuf[64];

  /* ESC/Page では線幅／終端／接合部の設定は１つのコマンドになっているため保持しておく。 */
  pdev -> cap = cap;

  if (pdev -> cap >= 3) return -1;

  (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%d;%d;%dlwG",
                (int)(pdev -> lwidth),
                (int)(pdev -> cap),
                (int)(pdev -> join));
  lputs(s, obuf);

  return 0;
}

static int
escv_setlinejoin(gx_device_vector * vdev, gs_line_join join)
{
  stream			*s = gdev_vector_stream(vdev);
  gx_device_escv *const	pdev = (gx_device_escv *) vdev;
  char			obuf[64];

  /* ESC/Page では線幅／終端／接合部の設定は１つのコマンドになっているため保持しておく。 */
  switch (join) {
  case 0:
    pdev -> join = 3;		/* miter */
    break;
  case 1:
    pdev -> join = 1;		/* round */
    break;
  case 2:
    pdev -> join = 2;		/* bevel */
    break;
  default:
    return -1;
  }

  (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%d;%d;%dlwG",
                (int)(pdev -> lwidth),
                (int)(pdev -> cap),
                (int)(pdev -> join));
  lputs(s, obuf);

  return 0;
}

static int
escv_setmiterlimit(gx_device_vector * vdev, double limit)
{
  stream			*s = gdev_vector_stream(vdev);
  gx_device_escv *const	pdev = (gx_device_escv *) vdev;
  char			obuf[128];

  /* マイターリミット値を設定するには lwG にて 接合部指定(n3) が 3 になっている
  ** 必要がある。
  */
  if (pdev -> join != 3) {
    /* 強制的に接合部指定を行う */
    pdev -> join = 3;
    (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%d;%d;%dlwG",
                  (int)(pdev -> lwidth),
                  (int)(pdev -> cap),
                  (int)(pdev -> join));
    lputs(s, obuf);
  }

  (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;%dmlG", (int)limit);
  lputs(s, obuf);

  return 0;
}

static bool
escv_can_handle_hl_color(gx_device_vector * vdev, const gs_gstate * pgs,
                         const gx_drawing_color * pdc)
{
  return false;
}

static int
escv_setfillcolor(gx_device_vector * vdev,
                  const gs_gstate * pgs,
                  const gx_drawing_color * pdc)
{
  stream			*s = gdev_vector_stream(vdev);
  gx_device_escv *const	pdev = (gx_device_escv *) vdev;
  gx_color_index		color = gx_dc_pure_color(pdc);
  char			obuf[64];

  if (!gx_dc_is_pure(pdc)) return_error(gs_error_rangecheck);
  pdev->current_color = color;

  if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

    (void)gs_snprintf(obuf, sizeof(obuf), /*ESC_GS "1owE"*/ ESC_GS "0;0;100spE" ESC_GS "1;0;%ldccE" ,color);
    lputs(s, obuf);

    if (vdev->x_pixels_per_inch == 1200) {
      lputs(s, ESC_GS "1;45;156htmE");
    } else if (vdev->x_pixels_per_inch == 600) {
      lputs(s, ESC_GS "1;45;106htmE");
    } else {
      lputs(s, ESC_GS "1;45;71htmE");
    }

  } else {			/* ESC/Page-Color */

    /* パターンＯＮ指定／ソリッドパターン指定 */
    (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;2;3;%d;%d;%dfpE",
                  (unsigned char)(color >> 16 & 0xff),
                  (unsigned char)(color >> 8 & 0xff),
                  (unsigned char)(color & 0xff));
    lputs(s, obuf);
    lputs(s, ESC_GS "3;2;1;0;0cpE" ESC_GS "1;2;1;0;0cpE" ESC_GS "5;2;1;0;0cpE");

  }	/* ESC/Page-Color */

  return 0;
}

static int
escv_setstrokecolor(gx_device_vector * vdev,
                    const gs_gstate * pgs,
                    const gx_drawing_color * pdc)
{
  stream			*s = gdev_vector_stream(vdev);
  gx_device_escv *const	pdev = (gx_device_escv *) vdev;
  gx_color_index		color = gx_dc_pure_color(pdc);
  char			obuf[64];

  if (!gx_dc_is_pure(pdc)) return_error(gs_error_rangecheck);

  if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

    pdev->current_color = color;

    (void)gs_snprintf(obuf, sizeof(obuf), /*ESC_GS "1owE"*/ ESC_GS "0;0;100spE" ESC_GS "1;1;%ldccE" , color);
    lputs(s, obuf);

    if (vdev->x_pixels_per_inch == 1200) {
      lputs(s, ESC_GS "1;45;156htmE");
    } else if (vdev->x_pixels_per_inch == 600) {
      lputs(s, ESC_GS "1;45;106htmE");
    } else {
      lputs(s, ESC_GS "1;45;71htmE");
    }

  } else {			/* ESC/Page-Color */

    if (vdev->color_info.depth == 24) {

      pdev->current_color = color;
      /* パターンＯＮ色指定／ソリッドパターン指定 */
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;2;3;%d;%d;%dfpE" ESC_GS "2;2;1;0;0cpE",
                    (unsigned char)(color >> 16 & 0xff),
                    (unsigned char)(color >> 8 & 0xff),
                    (unsigned char)(color & 0xff));
      lputs(s, obuf);

    }
  }	/* ESC/Page-Color */

  return 0;
}

/* 線種指定命令 */
static int
escv_setdash(gx_device_vector * vdev, const float *pattern, uint count, double offset)
{
  stream			*s = gdev_vector_stream(vdev);
  int				i;
  char			obuf[64];

  if (count == 0){
    /* 実線 */
    lputs(s, ESC_GS "0;0lpG");
    return 0;
  }

  /* offset が０以外の場合は描画不可として返却 */
  if (offset != 0) return -1;

  if (count) {
    if (count == 1) {
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;%d;%ddlG", (int) pattern[0], (int) pattern[0]);
      lputs(s, obuf);
    } else {
      /* pattern に０があった場合は描画不可として返却 */
      for (i = 0; i < count; ++i) {
        if (pattern[i] == 0) return -1;
      }

      lputs(s, ESC_GS "1");
      for (i = 0; i < count; ++i) {
        (void)gs_snprintf(obuf, sizeof(obuf), ";%d", (int) pattern[i]);
        lputs(s, obuf);
      }
      lputs(s, "dlG");
    }
    lputs(s, ESC_GS "1;1lpG");
  }
  return 0;
}

/* パス平滑度指定 */
static int
escv_setflat(gx_device_vector * vdev, double flatness)
{
  return 0;
}

static int
escv_setlogop(gx_device_vector * vdev, gs_logical_operation_t lop,
              gs_logical_operation_t diff)
{
  /****** SHOULD AT LEAST DETECT SET-0 & SET-1 ******/
  return 0;
}

static int
escv_beginpath(gx_device_vector * vdev, gx_path_type_t type)
{
  stream		*s = gdev_vector_stream(vdev);
  gx_device_escv *pdev = (gx_device_escv *) vdev;

  /* パス構築開始命令 */
  if (type & gx_path_type_clip) {
    lputs(s, ESC_GS "1bgpG");		/* クリップ登録 */
  } else {
    lputs(s, ESC_GS "0bgpG");		/* 描画登録 */
  }
  pdev->ispath = 0;

  return 0;
}

static int
escv_moveto(gx_device_vector * vdev,
            double x0, double y0, double x1, double y1, gx_path_type_t type)
{
  stream	*s = gdev_vector_stream(vdev);
  char	obuf[64];

  /* サブパス開始命令 */
  (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "0;%d;%dmvpG", (int)x1, (int)y1);
  lputs(s, obuf);

  return 0;
}

static int
escv_lineto(gx_device_vector * vdev,
            double x0, double y0, double x1, double y1, gx_path_type_t type)
{
  stream	*s = gdev_vector_stream(vdev);
  gx_device_escv *pdev = (gx_device_escv *) vdev;
  char	obuf[64];

  (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "0;1;%d;%dlnpG", (int)x1, (int)y1);
  lputs(s, obuf);
  pdev->ispath = 1;

  return 0;
}

static int
escv_curveto(gx_device_vector * vdev, double x0, double y0,
             double x1, double y1, double x2, double y2, double x3, double y3,
             gx_path_type_t type)
{
  stream	*s = gdev_vector_stream(vdev);
  gx_device_escv *pdev = (gx_device_escv *) vdev;
  char	obuf[128];

  /* ベジェ曲線 */
  (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "0;3;%d;%d;%d;%d;%d;%dbzpG",
                (int)x1, (int)y1, (int)x2, (int)y2, (int)x3, (int)y3);
  lputs(s, obuf);
  pdev->ispath = 1;

  return 0;
}

static int
escv_closepath(gx_device_vector * vdev, double x, double y,
               double x_start, double y_start, gx_path_type_t type)
{
  stream	*s = gdev_vector_stream(vdev);

  lputs(s, ESC_GS "clpG");
  return 0;
}

static int
escv_endpath(gx_device_vector * vdev, gx_path_type_t type)
{
  stream	*s = gdev_vector_stream(vdev);
  gx_device_escv *pdev = (gx_device_escv *) vdev;

  if (type & gx_path_type_fill || type & gx_path_type_clip) {
    /* default で処理されるが出力しておく */
    lputs(s, ESC_GS "clpG");
  }

  /* パスクローズ */
  lputs(s, ESC_GS "enpG");

  /* パス描画 */
  if (type & gx_path_type_clip) {

    if ( ( 0 != ESCV_FORCEDRAWPATH ) || ( 0 != pdev->ispath ) ) {
      /* クリップ指定
      ** クリップにも gx_path_type_winding_number, gx_path_type_even_odd の判断が
      ** 必要だと思うが gs 側が付加してこない。
      ** とりあえず gx_path_type_even_odd をデフォルトにする。
      */
      lputs(s, ESC_GS "1;2capG");
    }
  } else if (type & gx_path_type_fill) {

    /* 塗りつぶし規則設定 */
    if (type & gx_path_type_even_odd) {
      lputs(s, ESC_GS "0;2drpG");		/* 塗りつぶし描画 */
    } else {
      lputs(s, ESC_GS "0;1drpG");		/* 塗りつぶし描画 */
    }
  } else {
    lputs(s, ESC_GS "0;0drpG");		/* 輪郭線描画 */
  }

  return 0;
}

/* ---------------- Driver procedures ---------------- */

/* ------ Open/close/page ------ */

/* Open the device. */
static int
escv_open(gx_device * dev)
{
  gx_device_vector	*const vdev = (gx_device_vector *) dev;
  gx_device_escv	*const pdev = (gx_device_escv *) dev;
  int			code;
  /*    char		*error, *path;*/
  float               width, height;

  code = escv_range_check(dev);
  if (code < 0) return code;

  vdev->v_memory = dev->memory;
  /****** VERY WRONG ******/
  vdev->vec_procs = &escv_vector_procs;

  code = gdev_vector_open_file_options(vdev, 512, VECTOR_OPEN_FILE_BBOX
                                       | VECTOR_OPEN_FILE_SEQUENTIAL_OK);
  if (code < 0) return code;

  gdev_vector_init(vdev);
  pdev->first_page = true;

  if(pdev->orientation){

    if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

      pdev->Margins[1] = (pdev->width - pdev->height - \
                          ESCPAGE_LEFT_MARGIN_DEFAULT * vdev->x_pixels_per_inch / 72.0) * \
        X_DPI / vdev->x_pixels_per_inch;

    } else {			/* ESC/Page-Color */

      /*    pdev->Margins[1] = pdev->width - pdev->height + dev->HWMargins[0];
       */
      pdev->Margins[1] = (pdev->width - pdev->height) * \
        X_DPI / vdev->x_pixels_per_inch;

    }	/* ESC/Page-Color */

    width = dev->MediaSize[0];
    height  = dev->MediaSize[1];
    dev->MediaSize[0] = height;
    dev->MediaSize[1] = width;
  }

  return 0;
}

/* Wrap up ("output") a page. */
static int
escv_output_page(gx_device * dev, int num_copies, int flush)
{
  gx_device_vector *const vdev = (gx_device_vector *) dev;
  gx_device_escv *const pdev = (gx_device_escv *) dev;
  stream *s = gdev_vector_stream(vdev);

  /* 線幅,終端処理,接合部処理を初期化しておく */
  lputs(s, ESC_GS "3;0;0lwG" ESC_GS "1;10mlG" ESC_FF);

  sflush(s);
  vdev->in_page = false;
  pdev->first_page = false;

  gdev_vector_reset(vdev);

  return 0;
}

static int
escv_close(gx_device *dev)
{
  gx_device_vector *const vdev = (gx_device_vector *) dev;
  gp_file          *f = vdev->file;

  /* 終了処理コードは決め打ち */
  (void)gp_fprintf(f, ESC_GS "rhE" "\033\001@EJL \012@EJL EJ \012\033\001@EJL \012");

  gdev_vector_close_file(vdev);

  return 0;
}

/* Close the device. */
/* Note that if this is being called as a result of finalization, */
/* the stream may no longer exist; but the file will still be open. */

/* ---------------- Get/put parameters ---------------- */

static int
escv_get_str_param( gs_param_list * plist, gs_param_name key, gs_param_string *pgsstr, int code )
{
    int             ncode;

    ncode = param_write_string(plist, key, pgsstr);

    if ( ncode < 0)
      code = ncode;

    return code;
}

/* Get parameters. */
static int
escv_get_params(gx_device * dev, gs_param_list * plist)
{
  gx_device_escv	*const pdev = (gx_device_escv *) dev;
  int			code;
  int			ncode;

  code = gdev_vector_get_params(dev, plist);
  if (code < 0) return code;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_EPLModelJP, &pdev->modelJP)) < 0)
    code = ncode;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_EPLCapFaceUp, &pdev->capFaceUp)) < 0)
    code = ncode;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_EPLCapDuplexUnit, &pdev->capDuplexUnit)) < 0)
    code = ncode;

  if ((ncode = param_write_int(plist, ESCPAGE_OPTION_EPLCapMaxResolution, &pdev->capMaxResolution)) < 0)
    code = ncode;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_MANUALFEED, &pdev->manualFeed)) < 0)
    code = ncode;

  if ((ncode = param_write_int(plist, ESCPAGE_OPTION_CASSETFEED, &pdev->cassetFeed)) < 0)
    code = ncode;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_RIT, &pdev->RITOff)) < 0)
    code = ncode;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_COLLATE, &pdev->Collate)) < 0)
    code = ncode;

  if ((ncode = param_write_int(plist, ESCPAGE_OPTION_TONERDENSITY, &pdev->toner_density)) < 0)
    code = ncode;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_LANDSCAPE, &pdev->orientation)) < 0)
    code = ncode;

  if ( param_write_bool(plist, ESCPAGE_OPTION_TONERSAVING, &pdev->toner_saving)< 0)
    code = ncode;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_DUPLEX, &pdev->Duplex)) < 0)
    code = ncode;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_DUPLEX_TUMBLE, &pdev->Tumble)) < 0)
    code = ncode;

  if ((ncode = param_write_bool(plist, ESCPAGE_OPTION_FACEUP, &pdev->faceup)) < 0)
    code = ncode;

  if ((ncode = param_write_int(plist, ESCPAGE_OPTION_MEDIATYPE, &pdev->MediaType)) < 0)
    code = ncode;

  code = escv_get_str_param( plist, ESCPAGE_OPTION_JOBID,    &pdev->gpsJobID, code );
  code = escv_get_str_param( plist, ESCPAGE_OPTION_USERNAME, &pdev->gpsUserName, code );
  code = escv_get_str_param( plist, ESCPAGE_OPTION_HOSTNAME, &pdev->gpsHostName, code );
  code = escv_get_str_param( plist, ESCPAGE_OPTION_DOCUMENT, &pdev->gpsDocument, code );
  code = escv_get_str_param( plist, ESCPAGE_OPTION_COMMENT,  &pdev->gpsComment, code );

  return code;
}

static int
escv_set_str_param( gs_param_list * plist, const char * key, char *strvalue, int bufmax, int ecode )
{
    gs_param_name	param_name;
    gs_param_string	gsstr;
    int			code;
    int                 writesize = bufmax;

    switch (code = param_read_string(plist, (param_name = key), &gsstr)) {
    case 0:
      writesize = ( bufmax < gsstr.size )? bufmax : gsstr.size ;
      strncpy( strvalue, (const char *)gsstr.data, writesize );
      strvalue[ writesize ] = '\0';
      break;
    default:
      ecode = code;
      param_signal_error(plist, param_name, ecode);
    case 1:
      break;
    }
    return ecode;
}

/* Put parameters. */
static int
escv_put_params(gx_device * dev, gs_param_list * plist)
{
  gx_device_escv	*const pdev = (gx_device_escv *) dev;
  int			ecode = 0;
  int			code;
  gs_param_name	param_name;
  gs_param_string	pmedia;
  bool		mf = pdev->manualFeed;
  int			cass = pdev->cassetFeed;
  bool		tum = pdev->Tumble;
  bool		collate = pdev->Collate;
  int			toner_density = pdev->toner_density;
  bool		toner_saving = pdev->toner_saving;
  bool		landscape = pdev->orientation;
  bool		faceup = pdev->faceup;
  bool		duplex = pdev->Duplex;
  bool		RITOff = pdev->RITOff;
  int			old_bpp = dev->color_info.depth;
  int			bpp = 0;
  bool          modelJP = false;
  bool          capFaceUp = false;
  bool          capDuplexUnit = false;
  int           capMaxResolution = 0;

  ecode = escv_set_str_param( plist, ESCPAGE_OPTION_JOBID,    pdev->JobID,    ESCPAGE_JOBID_MAX,    ecode );
  ecode = escv_set_str_param( plist, ESCPAGE_OPTION_USERNAME, pdev->UserName, ESCPAGE_USERNAME_MAX, ecode );
  ecode = escv_set_str_param( plist, ESCPAGE_OPTION_HOSTNAME, pdev->HostName, ESCPAGE_HOSTNAME_MAX, ecode );
  ecode = escv_set_str_param( plist, ESCPAGE_OPTION_DOCUMENT, pdev->Document, ESCPAGE_DOCUMENT_MAX, ecode );
  ecode = escv_set_str_param( plist, ESCPAGE_OPTION_COMMENT,  pdev->Comment,  ESCPAGE_COMMENT_MAX,  ecode );

  modelJP = pdev->modelJP;
  param_name = ESCPAGE_OPTION_EPLModelJP;
  code = param_read_bool(plist, param_name, &modelJP);
  if (code < 0) {
    ecode = code;
    param_signal_error(plist, param_name, ecode);
  }

  capFaceUp = pdev->capFaceUp;
  param_name = ESCPAGE_OPTION_EPLCapFaceUp;
  code = param_read_bool(plist, param_name, &capFaceUp);
  if (code < 0) {
    ecode = code;
    param_signal_error(plist, param_name, ecode);
  }

  capDuplexUnit = pdev->capDuplexUnit;
  param_name = ESCPAGE_OPTION_EPLCapDuplexUnit;
  code = param_read_bool(plist, param_name, &capDuplexUnit);
  if (code < 0) {
    ecode = code;
    param_signal_error(plist, param_name, ecode);
  }

  capMaxResolution = pdev->capMaxResolution;
  param_name = ESCPAGE_OPTION_EPLCapMaxResolution;
  code = param_read_int(plist, param_name, &capMaxResolution);
  switch ( code )
    {
    case 1:
      break;

    case 0:
      if ( ( 600 != capMaxResolution ) &&
           ( 1200 != capMaxResolution ) ) {
        ecode = gs_error_limitcheck;
        goto maxrese;
      }
      break;

    default:
      ecode = code;
      /* through */
    maxrese:
      param_signal_error(plist, param_name, ecode);
      break;
    }

  if ((code = param_read_bool(plist, (param_name = ESCPAGE_OPTION_MANUALFEED), &mf)) < 0) {
    param_signal_error(plist, param_name, ecode = code);
  }
  switch (code = param_read_int(plist, (param_name = ESCPAGE_OPTION_CASSETFEED), &cass)) {
  case 0:
    if (cass < -1 || cass > 15)
      ecode = gs_error_limitcheck;
    else
      break;
    goto casse;
  default:
    ecode = code;
  casse:param_signal_error(plist, param_name, ecode);
  case 1:
    break;
  }

  if((code = param_read_bool(plist, (param_name = ESCPAGE_OPTION_COLLATE), &collate)) < 0) {
    param_signal_error(plist, param_name, ecode = code);
  }

  if ((code = param_read_bool(plist, (param_name = ESCPAGE_OPTION_RIT), &RITOff)) < 0) {
    param_signal_error(plist, param_name, ecode = code);
  }

  switch (code = param_read_string(plist, (param_name = ESCPAGE_OPTION_MEDIATYPE), &pmedia)) {
  case 0:
    if (pmedia.size > ESCPAGE_MEDIACHAR_MAX) {
        ecode = gs_error_limitcheck;
        goto pmediae;
    } else {   /* Check the validity of ``MediaType'' characters */

      if (strcmp((const char *)pmedia.data, "NM") == 0) {
        pdev->MediaType = 0;
      } else if ((strcmp((const char *)pmedia.data, "THICK") == 0) ||
                 (strcmp((const char *)pmedia.data, "TH") == 0)) {
        pdev->MediaType = 1;
      } else if ((strcmp((const char *)pmedia.data, "TRANS") == 0) ||
                 (strcmp((const char *)pmedia.data, "TR") == 0)) {
        pdev->MediaType = 2;
      } else if (strcmp((const char *)pmedia.data, "TN") == 0) {
        pdev->MediaType = 3;
      } else if (strcmp((const char *)pmedia.data, "LH") == 0) {
        pdev->MediaType = 4;
      } else if (strcmp((const char *)pmedia.data, "CT") == 0) {
        pdev->MediaType = 5;
      } else if (strcmp((const char *)pmedia.data, "ET") == 0) {
        pdev->MediaType = 6;
      } else if (strcmp((const char *)pmedia.data, "HQ") == 0) {
        pdev->MediaType = 7;
      } else if (strcmp((const char *)pmedia.data, "UT") == 0) {
        pdev->MediaType = 8;
      } else if (strcmp((const char *)pmedia.data, "UM") == 0) {
        pdev->MediaType = 9;
      } else {
        ecode = gs_error_rangecheck;
        goto pmediae;
      }
    }
    break;
  default:
    ecode = code;
  pmediae:
    param_signal_error(plist, param_name, ecode);
    /* Fall through. */
  case 1:
    if(!pdev->MediaType){
      pdev->MediaType = 0;
      pmedia.data = 0;
    }
    break;
  }

  switch (code = param_read_int(plist,
                                (param_name = ESCPAGE_OPTION_TONERDENSITY), &toner_density)) {
  case 0:
    if (toner_density < 0 || toner_density > 5)
      ecode = gs_error_rangecheck;
    else
      break;
    goto tden;
  default:
    ecode = code;
  tden:
    param_signal_error(plist, param_name, ecode);
  case 1:
    break;
  }

  switch (code = param_read_bool(plist, (param_name = ESCPAGE_OPTION_TONERSAVING), &toner_saving)) {
  case 0:
    break;
  default:
    if ((code = param_read_null(plist, param_name)) == 0) {
      break;
    }
    ecode = code;
    param_signal_error(plist, param_name, ecode);
  case 1:
    break;
  }

  if ((code = param_read_bool(plist, (param_name = ESCPAGE_OPTION_DUPLEX), &duplex)) < 0)
    param_signal_error(plist, param_name, ecode = code);

  if ((code = param_read_bool(plist, (param_name = ESCPAGE_OPTION_DUPLEX_TUMBLE), &tum)) < 0)
    param_signal_error(plist, param_name, ecode = code);

  if ((code = param_read_bool(plist, (param_name = ESCPAGE_OPTION_LANDSCAPE), &landscape)) < 0)
    param_signal_error(plist, param_name, ecode = code);

  if ((code = param_read_bool(plist, (param_name = ESCPAGE_OPTION_FACEUP), &faceup)) < 0) {
    param_signal_error(plist, param_name, ecode = code);
  }

  switch (code = param_read_int(plist, (param_name = "BitsPerPixel"), &bpp)) {
  case 0:
    if (bpp != 8 && bpp != 24)
      ecode = gs_error_rangecheck;
    else
      break;
    goto bppe;
  default:
    ecode = code;
  bppe:param_signal_error(plist, param_name, ecode);
  case 1:
    break;
  }

  if (bpp != 0) {
    dev->color_info.depth = bpp;
    dev->color_info.num_components = ((bpp == 8) ? 1 : 3);
    dev->color_info.max_gray = (bpp > 8 ? 255 : 1000);
    dev->color_info.max_color = (bpp > 8 ? 255 : 1000);
    dev->color_info.dither_grays = (bpp > 8 ? 256 : 5);
    dev->color_info.dither_colors = (bpp > 8 ? 256 : 2);
    dev_proc(pdev, map_rgb_color) = ((bpp == 8) ? gx_default_gray_map_rgb_color : gx_default_rgb_map_rgb_color);
    dev_proc(pdev, map_color_rgb) = ((bpp == 8) ? gx_default_gray_map_color_rgb : gx_default_rgb_map_color_rgb);
  }

  if (ecode < 0) return ecode;
  code = gdev_vector_put_params(dev, plist);
  if (code < 0) return code;

  pdev->modelJP = modelJP;
  pdev->capFaceUp = capFaceUp;
  pdev->capDuplexUnit = capDuplexUnit;
  pdev->capMaxResolution = capMaxResolution;

  pdev->manualFeed = mf;
  pdev->cassetFeed = cass;
  pdev->faceup = faceup;
  pdev->RITOff = RITOff;
  pdev->orientation = landscape;
  pdev->toner_density = toner_density;
  pdev->toner_saving = toner_saving;
  pdev->Collate = collate;
  pdev->Duplex = duplex;
  pdev->Tumble = tum;

  if (bpp != 0 && bpp != old_bpp && pdev->is_open)
    return gs_closedevice(dev);

  return 0;
}

/* ---------------- Images ---------------- */

static int
escv_copy_mono(gx_device * dev, const byte * data,
               int data_x, int raster, gx_bitmap_id id, int x, int y, int w, int h,
               gx_color_index zero, gx_color_index one)
{
  gx_device_escv *const	pdev = (gx_device_escv *) dev;
  gx_device_vector *const	vdev = (gx_device_vector *) dev;
  stream			*s = gdev_vector_stream(vdev);
  gx_drawing_color		color;
  int				code = 0;
  gx_color_index		c_color = 0;
  char			obuf[128];
  int				depth = 1;
  const gs_gstate * pgs = (const gs_gstate *)0;

  if (id != gs_no_id && zero == gx_no_color_index && one != gx_no_color_index && data_x == 0) {
    gx_drawing_color dcolor;

    color_set_pure(&dcolor, one);
    escv_setfillcolor(vdev,
                      pgs,
                      &dcolor); /* FIXME! gs815 */
  }

  if (zero == gx_no_color_index) {

    if (one == gx_no_color_index) return 0;
    if (pdev->MaskState != 1) {

      if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

        /*	    lputs(s, ESC_GS "1owE");*/
        (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;1;%ldccE", c_color);
        lputs(s, obuf);

        if (vdev->x_pixels_per_inch == 1200) {
          lputs(s, ESC_GS "1;45;156htmE");
        } else if (vdev->x_pixels_per_inch == 600) {
          lputs(s, ESC_GS "1;45;106htmE");
        } else {
          lputs(s, ESC_GS "1;45;71htmE");
        }

      } else {			/* ESC/Page-Color */

        lputs(s, ESC_GS "2;184wfE" ESC_GS "3;184wfE" ESC_GS "5;184wfE");

      }	/* ESC/Page-Color */

      pdev->MaskState = 1;
    }
    c_color = one;

  } else if (one == gx_no_color_index)
    /* 1bit は透明 ビット反転・zero 色に染める */
    {
      if (pdev->MaskState != 1) {

        if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

          /*	    lputs(s, ESC_GS "1owE");*/

        } else {			/* ESC/Page-Color */

          lputs(s, ESC_GS "3;184wfE" ESC_GS "5;184wfE");

        }	/* ESC/Page-Color */

        pdev->MaskState = 1;
      }
      c_color = zero;
    } else if (one == vdev->white) {

      if (pdev->MaskState != 0) {

        if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

          /*	    lputs(s, ESC_GS "1owE");*/

        } else {			/* ESC/Page-Color */

          lputs(s, ESC_GS "3;204wfE" ESC_GS "5;204wfE");

        }	/* ESC/Page-Color */

        pdev->MaskState = 0;
      }
      c_color = zero;
    } else {

      if (pdev->MaskState != 1) {

        if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

          /*	    lputs(s, ESC_GS "1owE");*/

        } else {			/* ESC/Page-Color */

          lputs(s, ESC_GS "3;184wfE" ESC_GS "5;184wfE");

        }	/* ESC/Page-Color */

        pdev->MaskState = 1;
      }
      color_set_pure(&color, one);
      code = gdev_vector_update_fill_color((gx_device_vector *) pdev,
                                           pgs,
                                           &color);

      /* ここを通過したら以下の色設定は無意味？ */
    }
  if (code < 0) return 0;

  if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */
  } else {			/* ESC/Page-Color */

    /* パターンＯＮ指定／ソリッドパターン指定 */
    (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;2;3;%d;%d;%dfpE",
                  (unsigned char)(c_color >> 16 & 0xff),
                  (unsigned char)(c_color >> 8 & 0xff),
                  (unsigned char)(c_color & 0xff));
    lputs(s, obuf);

    lputs(s, ESC_GS "5;2;1;0;0cpE");

  }	/* ESC/Page-Color */

  escv_write_begin(dev, depth, (int)x, (int)y, w, h, w, h, 0);
  {
    int		i, j;
    uint		width_bytes = (w + 7) >> 3;
    uint		num_bytes = width_bytes * h;

    byte *buf = gs_alloc_bytes(vdev->memory, num_bytes, "escv_copy_mono(buf)");

    if (data_x % 8 == 0) {
      for (i = 0; i < h; ++i) {
        memcpy(buf + i * width_bytes, data + (data_x >> 3) + i * raster, width_bytes);
      }
    } else {
      for (i = 0; i < h; ++i) {
        for (j = 0; j < width_bytes; j++) {
          *(buf + i * width_bytes + j) =
            *(data + (data_x >> 3) + i * raster + j) << (data_x % 8) |
            *(data + (data_x >> 3) + i * raster + j + 1) >> (8 - data_x % 8);
        }
      }
    }

    escv_write_data(dev, depth, buf, num_bytes, w, h);
    gs_free_object(vdev->memory, buf, "escv_copy_mono(buf)");
  }
  escv_write_end(dev, depth);
  return 0;
}

/* Copy a color bitmap. */
static int
escv_copy_color(gx_device * dev,
                const byte * data, int data_x, int raster, gx_bitmap_id id,
                int x, int y, int w, int h)
{
  gx_device_escv	*const pdev = (gx_device_escv *) dev;
  gx_device_vector	*const vdev = (gx_device_vector *) dev;

  int			depth = dev->color_info.depth;
  int			num_components = (depth < 24 ? 1 : 3);
  uint		width_bytes = w * num_components;

  if (pdev->MaskState != 0) {

    if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

      /*	lputs(s, ESC_GS "1owE");*/

    } else {			/* ESC/Page-Color */
      stream		*s = gdev_vector_stream(vdev);

      lputs(s, ESC_GS "3;204wfE" ESC_GS "5;204wfE");

    }	/* ESC/Page-Color */
    pdev->MaskState = 0;
  }

  escv_write_begin(dev, depth, (int)x, (int)y, w, h, w, h, 0);

  {
    int		i;
    uint		num_bytes = width_bytes * h;
    byte		*buf = gs_alloc_bytes(vdev->memory, num_bytes, "escv_copy_color(buf)");

    for (i = 0; i < h; ++i) {
      memcpy(buf + i * width_bytes, data + ((data_x * depth) >> 3) + i * raster, width_bytes);
    }

    escv_write_data(dev, depth, buf, num_bytes, w, h);
    gs_free_object(vdev->memory, buf, "escv_copy_color(buf)");
  }

  escv_write_end(dev, depth);
  return 0;
}

/* Fill a mask. */
static int
escv_fill_mask(gx_device * dev,
               const byte * data, int data_x, int raster, gx_bitmap_id id,
               int x, int y, int w, int h,
               const gx_drawing_color * pdcolor, int depth,
               gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
  gx_device_vector *const vdev = (gx_device_vector *) dev;
  gx_device_escv   *const pdev = (gx_device_escv *)   dev;
  stream           *s    = gdev_vector_stream(vdev);

  gx_color_index		color = gx_dc_pure_color(pdcolor);
  char			obuf[64];

  const gs_gstate * pgs = (const gs_gstate *)0;

  if (w <= 0 || h <= 0) return 0;

  if (depth > 1 ||
      gdev_vector_update_fill_color(vdev,
                                    pgs,
                                    pdcolor) < 0 ||
      gdev_vector_update_clip_path(vdev, pcpath) < 0 ||
      gdev_vector_update_log_op(vdev, lop) < 0
      )
    return gx_default_fill_mask(dev, data, data_x, raster, id,
                                x, y, w, h, pdcolor, depth, lop, pcpath);

  if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

    if (!gx_dc_is_pure(pdcolor)) return_error(gs_error_rangecheck);
    pdev->current_color = color;

    (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "0;0;100spE" ESC_GS "1;1;%ldccE" ,color);
    lputs(s, obuf);

    if (vdev->x_pixels_per_inch == 1200) {
      lputs(s, ESC_GS "1;45;156htmE");
    } else if (vdev->x_pixels_per_inch == 600) {
      lputs(s, ESC_GS "1;45;106htmE");
    } else {
      lputs(s, ESC_GS "1;45;71htmE");
    }

    if(pdev->MaskState != 1) {
      /*      lputs(s, ESC_GS "1owE");*/
      pdev->MaskState = 1;
    }

  } else {			/* ESC/Page-Color */

    if (pdev->MaskState != 1) {

      lputs(s, ESC_GS "3;184wfE" ESC_GS "5;184wfE");
      pdev->MaskState = 1;
    }

    if (id != gs_no_id && data_x == 0 && depth == 1) {
      char		obuf[128];
      int		i;
      uint		width_bytes = (w + 7) >> 3;
      uint		num_bytes = width_bytes * h;
      byte		*buf;

      if (pdev -> id_cache[id & VCACHE] != id) {

        buf = gs_alloc_bytes(vdev->memory, num_bytes, "escv_fill_mask(buf)");

        /* cache entry */
        for (i = 0; i < h; ++i) {
          memcpy(buf + i * width_bytes, data + (data_x >> 3) + i * raster, width_bytes);
        }

        (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%d;%d;%d;%d;0db{F", num_bytes, (int)(id & VCACHE), w, h);
        lputs(s, obuf);
        put_bytes(s, buf, num_bytes);

        gs_free_object(vdev->memory, buf, "escv_fill_mask(buf)");
        pdev -> id_cache[id & VCACHE] = id;
      }

      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%dX" ESC_GS "%dY", x, y);
      lputs(s, obuf);
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%lddbF", id & VCACHE);
      lputs(s, obuf);

      return 0;
    }
  }	/* ESC/Page-Color */

  escv_write_begin(dev, depth, (int)x, (int)y, w, h, w, h, 0);
  {
    int		i;
    uint		width_bytes = (w + 7) >> 3;
    uint		num_bytes = width_bytes * h;
    byte		*buf = gs_alloc_bytes(vdev->memory, num_bytes, "escv_fill_mask(buf)");

    for (i = 0; i < h; ++i) {
      memcpy(buf + i * width_bytes, data + (data_x >> 3) + i * raster, width_bytes);
    }

    escv_write_data(dev, depth, buf, num_bytes, w, h);
    escv_write_end(dev, depth);
    gs_free_object(vdev->memory, buf, "escv_fill_mask(buf)");
  }

  return 0;
}

/* ---------------- High-level images ---------------- */

static image_enum_proc_plane_data(escv_image_plane_data);
static image_enum_proc_end_image(escv_image_end_image);
static const gx_image_enum_procs_t escv_image_enum_procs =
  {
    escv_image_plane_data, escv_image_end_image
  };

/* Start processing an image. */
static int
escv_begin_typed_image(gx_device               *dev,
                 const gs_gstate               *pgs,
                 const gs_matrix               *pmat,
                 const gs_image_common_t       *pic,
                 const gs_int_rect             *prect,
                 const gx_drawing_color        *pdcolor,
                 const gx_clip_path            *pcpath,
                       gs_memory_t             *mem,
                       gx_image_enum_common_t **pinfo)
{
  const gs_image_t *pim = (const gs_image_t *)pic;
  gx_device_vector *const	vdev = (gx_device_vector *) dev;
  gx_device_escv *const	pdev = (gx_device_escv *) dev;
  stream			*s;
  gdev_vector_image_enum_t	*pie;
  const gs_color_space	*pcs;
  gs_color_space_index	index;
  int				num_components = 1;
  bool can_do;

  gs_matrix			imat;
  int				code;
  int				ty, bx, by, cy, sx, sy;

  char		        obuf[128];

  s = gdev_vector_stream((gx_device_vector *) pdev);
  pie = gs_alloc_struct(mem, gdev_vector_image_enum_t,
                        &st_vector_image_enum, "escv_begin_typed_image");
  if (pie == NULL) return_error(gs_error_VMerror);
  pie->memory = mem;

  /* This code can only cope with type1 images. Anything else, we need
   * to send to the default. */
  if (pic->type->index != 1)
    goto fallback;

  can_do = prect == NULL &&
           (pim->format == gs_image_format_chunky ||
            pim->format == gs_image_format_component_planar);
  pcs = pim->ColorSpace;
  code = gdev_vector_begin_image(vdev, pgs, pim, pim->format, prect,
                                 pdcolor, pcpath, mem,
                                 &escv_image_enum_procs, pie);
  if (code < 0) return code;

  *pinfo = (gx_image_enum_common_t *) pie;

  if (!pim->ImageMask) {
    index = gs_color_space_get_index(pcs);
    num_components = gs_color_space_num_components(pcs);

    if (pim->CombineWithColor) {
      can_do = false;
    } else {
      switch (index) {
      case gs_color_space_index_DeviceGray:
        if ((pim->Decode[0] != 0 || pim->Decode[1] != 1)
            && (pim->Decode[0] != 1 || pim->Decode[1] != 0))
          can_do = false;
        break;
      case gs_color_space_index_DeviceRGB:
        if (pim->Decode[0] != 0 || pim->Decode[1] != 1 ||
            pim->Decode[2] != 0 || pim->Decode[3] != 1 ||
            pim->Decode[4] != 0)
          can_do = false;
        break;
      default:
        can_do = false;
      }
    }
  }
  if (!can_do) {
fallback:
    return gx_default_begin_typed_image(dev, pgs, pmat, pic, prect,
                                        pdcolor, pcpath, mem,
                                        &pie->default_info);
  }

  if (pim->ImageMask || (pim->BitsPerComponent == 1 && num_components == 1)) {
    if (pim->Decode[0] > pim->Decode[1]) {
      pdev->MaskReverse = 1;
    } else {
      if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */
      } else {			/* ESC/Page-Color */
        lputs(s, ESC_GS "8;1;2;2;2plr{E");
        put_bytes(s, (const byte *)"\000\000\000\000\377\377\377\377", 8);
      }	/* ESC/Page-Color */
      pdev->MaskReverse = 0;
    }
  }

  /* Write the image/colorimage/imagemask preamble. */
  code = gs_matrix_invert(&pim->ImageMatrix, &imat);
  if (code < 0)
      return code;

  if (pmat == NULL)
    pmat = &ctm_only(pgs);
  gs_matrix_multiply(&imat, pmat, &imat);

  ty = imat.ty;
  bx = imat.xx * pim->Width + imat.yx * pim->Height + imat.tx;
  by = imat.xy * pim->Width + imat.yy * pim->Height + imat.ty;
  cy = imat.yy * pim->Height + imat.ty;

  sx = bx - (int)imat.tx;
  sy = by - (int)imat.ty;

  /* とりあえず絵の位置に収まるように強制的に座標を変更する。 */
  pdev -> roll = 0;
  pdev -> reverse_x = pdev -> reverse_y = 0;
  if (imat.tx > bx) {
    pdev -> reverse_x = 1;
    sx = -sx;
    imat.tx = bx;
  }

  if (imat.ty > by) {
    pdev -> reverse_y = 1;
    sy = -sy;
    imat.ty = by;
  }

  (void)memcpy(&pdev -> xmat, &imat, sizeof(gs_matrix));
  pdev -> sx = sx;
  pdev -> sy = sy;
  pdev -> h = pim->Height;
  pdev -> w = pim->Width;
  pdev -> dd = 0;
  pdev -> bx = 0;
  pdev -> by = 0;

  if (ty == cy) {
    /* 回転時の描画については現在未実装。GS 側の機能を使用する */
    return -1;
  }

  if (pim->ImageMask) {
    pdev->ncomp = 1;

    /* 描画論理設定命令 - 透過 */
    if (pdev->MaskState != 1) {

      if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */
        gx_color_index color = gx_dc_pure_color(pdcolor);

        /*	    lputs(s, ESC_GS "1owE");*/
        (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;1;%ldccE", color);
        lputs(s, obuf);

        if (vdev->x_pixels_per_inch == 1200) {
          lputs(s, ESC_GS "1;45;156htmE");
        } else if (vdev->x_pixels_per_inch == 600) {
          lputs(s, ESC_GS "1;45;106htmE");
        } else {
          lputs(s, ESC_GS "1;45;71htmE");
        }

      } else {			/* ESC/Page-Color */

        lputs(s, ESC_GS "3;184wfE" ESC_GS "5;184wfE");
        pdev->MaskState = 1;

      }	/* ESC/Page-Color */

    }

  } else {

    /* 描画論理設定命令 - 白塗り */
    if (pdev->MaskState != 0) {

      if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */
        /*	  lputs(s, ESC_GS "1owE");*/ /* 184->204 */
      } else {			/* ESC/Page-Color */
        lputs(s, ESC_GS "3;204wfE" ESC_GS "5;204wfE");
      }	/* ESC/Page-Color */

      pdev->MaskState = 0;
    }
    pdev->ncomp = num_components;
  }

  if (pdev -> reverse_y) return 0;

  escv_write_begin(dev, pie->bits_per_pixel, (int)imat.tx, (int)imat.ty, pie->width, pie->height, sx, sy, pdev -> roll);

  return 0;
}

/* Process the next piece of an image. */
static int
escv_image_plane_data(gx_image_enum_common_t *info, const gx_image_plane_t *planes, int height, int *rows_used)
{
  gx_device *dev = info->dev;
  gx_device_vector *const	vdev = (gx_device_vector *) dev;
  gx_device_escv *const	pdev = (gx_device_escv *) dev;
  gdev_vector_image_enum_t	*pie = (gdev_vector_image_enum_t *) info;

  int				y;
  int				plane;
  int				width_bytes, tbyte;
  byte			*buf;

  if (pie->default_info) return gx_image_plane_data(pie->default_info, planes, height);

  gx_image_plane_data(pie->bbox_info, planes, height);

  {

    if (height == 260)
      height = 1;
    width_bytes = (pie->width * pie->bits_per_pixel / pdev->ncomp + 7) / 8 * pdev->ncomp;
    tbyte = width_bytes * height;
    buf = gs_alloc_bytes(vdev->memory, tbyte, "escv_image_data(buf)");

    if (pdev -> reverse_y) {

      if (pdev -> h == height) {
        if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

          if(tbyte == 1){
            if(strcmp(pdev->dname, "lp1800") != 0 &&
               strcmp(pdev->dname, "lp9600") != 0) {
              pdev->w += pdev->sx / 2048;
              height  += pdev->sy / 2048;
            }
          }

        } else {			/* ESC/Page-Color */

          if(tbyte == 1){
            pdev->w += pdev->sx / 2048;
            height  += pdev->sy / 2048;
          }

        }	/* ESC/Page-Color */

        escv_write_begin(dev, pie->bits_per_pixel, (int)pdev -> xmat.tx, (int)pdev -> xmat.ty, pdev -> w, height, (int)pdev -> sx, (int)pdev -> sy, pdev -> roll);

      } else {
        float	yy, sy;

        yy = (pdev -> h * pdev->xmat.yy) - (pdev -> dd * pdev -> xmat.yy) - (height * pdev -> xmat.yy);
        if (yy == 0) {
          yy = (pdev -> h * pdev->xmat.yx) - (pdev -> dd * pdev -> xmat.yx) - (height * pdev -> xmat.yx);
        }

        if (pdev -> by) {
          sy = (int)pdev -> xmat.ty - (int)yy;
          sy = pdev -> by - (int)sy;
        } else {
          sy = height * pdev -> xmat.yy + 0.5;
        }
        if (sy < 0) {
          sy = -sy;
        }

        escv_write_begin(dev, pie->bits_per_pixel, (int)pdev -> xmat.tx, (int)pdev -> xmat.ty - (int)yy, pdev -> w, height, (int)pdev -> sx, (int)sy, pdev -> roll);

        pdev -> by = (int)pdev -> xmat.ty - (int)yy;
      }
    }
    pdev -> dd += height;

    for (plane = 0; plane < pie->num_planes; ++plane) {

      for (y = 0; y < height; ++y) {

        int     bit, w;
        const byte *p;
        byte *d;
        byte c;

        p = planes[plane].data + ((planes[plane].data_x * pie->bits_per_pixel) >> 3) + y * planes[plane].raster;
        if (pdev -> reverse_y) {

          d = buf + (height - y) * width_bytes;

          if (!pdev -> reverse_x) {
            (void)memcpy(buf + (height - y - 1) * width_bytes,
                         planes[plane].data + ((planes[plane].data_x * pie->bits_per_pixel) >> 3)
                         + y * planes[plane].raster, width_bytes);

          }

        } else {

          d = buf + (y + 1) * width_bytes;

          if (!pdev -> reverse_x) {

            (void)memcpy(buf + y * width_bytes,
                         planes[plane].data + ((planes[plane].data_x * pie->bits_per_pixel) >> 3)
                         + y * planes[plane].raster, width_bytes);

          }
        }
        if (pdev -> reverse_x) {
          if (pie->bits_per_pixel == 1) {
            for (w = 0; w < width_bytes; w++) {
              c = 0;
              for (bit = 0; bit < 8; bit++) {
                if (*p & 1 << (7 - bit)) {
                  c |= 1 << bit;
                }
              }
              p++;
              *--d = c;
            }
          } else if (pie->bits_per_pixel == 8){
            for (w = 0; w < width_bytes; w++) {
              *--d = *p++;
            }
          } else {
            for (w = 0; w < width_bytes / 3; w++) {
              *--d = *(p + 2);
              *--d = *(p + 1);
              *--d = *p;
              p += 3;
            }
          }
        }
      }
    }

    if(tbyte == 1){
      int t;

      if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

        gs_free_object(vdev->memory, buf, "escv_image_data(buf)");
        if(strcmp(pdev->dname, "lp1800") == 0 ||
           strcmp(pdev->dname, "lp9600") == 0) {
          if(pdev->sx > pdev->sy){
            height  = pdev->sy;
            pdev->w = pdev->sx;
            tbyte = ((pdev->sx + 7) / 8) * pdev->sy;
          } else {
            if(pdev->sx < pdev->sy){
              height  = pdev->sy;
              pdev->w = pdev->sx;
              tbyte = ((pdev->sx + 7) / 8) * pdev->sy;
            } else {
              tbyte = 1;
            }
          }

        } else {
          if(pdev->sx > pdev->sy){
            tbyte = 1;
          } else {
            if(pdev->sx < pdev->sy){
              tbyte = tbyte * height;
            } else {
              tbyte = 1;
            }
          }
        }
        buf = gs_alloc_bytes(vdev->memory, tbyte, "escv_image_data(buf)");
        for(t = 0; t < tbyte; t++){
          buf[t] = 0xff;
        }

      } else {			/* ESC/Page-Color */

        gs_free_object(vdev->memory, buf, "escv_image_data(buf)");
        if(pdev->sx > pdev->sy){
          tbyte = 1;
        } else {
          if(pdev->sx < pdev->sy){
            tbyte = tbyte * height;
          } else {
            tbyte = 1;
          }
        }

        buf = gs_alloc_bytes(vdev->memory, tbyte, "escv_image_data(buf)");
        for(t = 0; t < tbyte; t++){
          buf[t] = 0x00;
        }

      }	/* ESC/Page-Color */

    }

    escv_write_data(dev, pie->bits_per_pixel, buf, tbyte, pdev -> w, height);

    if (pdev -> reverse_y) {
      escv_write_end(dev, pie->bits_per_pixel);
    }

    gs_free_object(vdev->memory, buf, "escv_image_data(buf)");
  }
  return (pie->y += height) >= pie->height;
}

static int
escv_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
  gx_device *dev = info->dev;
  gx_device_vector		*const vdev = (gx_device_vector *) dev;
  gx_device_escv		*const pdev = (gx_device_escv *) dev;
  gdev_vector_image_enum_t	*pie = (gdev_vector_image_enum_t *) info;
  int				code;

  if (!(pdev -> reverse_y)) {
    escv_write_end(dev, pie->bits_per_pixel);
  }

  pdev->reverse_x = pdev->reverse_y = 0;
  if (pdev->MaskReverse == 0) {
    if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */
      ;
    } else {			/* ESC/Page-Color */

      stream			*s = gdev_vector_stream((gx_device_vector *)pdev);

      lputs(s, ESC_GS "8;1;2;2;2plr{E");
      put_bytes(s, (const byte *)"\377\377\377\377\000\000\000\000", 8);
    }	/* ESC/Page-Color */
  }
  pdev->MaskReverse = -1;

  code = gdev_vector_end_image(vdev, (gdev_vector_image_enum_t *) pie, draw_last, pdev->white);
  return code;
}

static void escv_write_begin(gx_device *dev, int bits, int x, int y, int sw, int sh, int dw, int dh, int roll)
{
  gx_device_vector *const     vdev = (gx_device_vector *) dev;
  gx_device_escv   *const     pdev = (gx_device_escv *)dev;
  stream			*s = gdev_vector_stream((gx_device_vector *)dev);
  char                          obuf[128];
  byte                         *tmp, *p;
  int				i, comp;

  if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

    (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%dX" ESC_GS "%dY", x, y);
    lputs(s, obuf);

    comp = 10;

    if (bits == 1) {
      if (strcmp(pdev->dname, "lp1800") == 0 ||
          strcmp(pdev->dname, "lp9600") == 0) {
        (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "0bcI");
      }else{
        (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "5;%d;%d;%d;%d;%dsrI",  sw, sh, dw, dh, roll);
      }
    } else if (bits == 4) {
      if (pdev -> c4map) {
        pdev -> c4map = FALSE;
      }
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;1;1;0;%d;%d;%d;%d;%d;%dscrI", comp, sw, sh, dw, dh, roll);
    } else if (bits == 8) {
      if (pdev -> c8map) {
        pdev -> c8map = FALSE;
      }
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;1;1;0;%d;%d;%d;%d;%d;%dscrI", comp, sw, sh, dw, dh, roll);
    } else {
      /* 24 bit */
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "1;1;1;0;%d;%d;%d;%d;%d;%dscrI", comp, sw, sh, dw, dh, roll);
    }

  } else {			/* ESC/Page-Color */

    (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%dX" ESC_GS "%dY", x, y);
    lputs(s, obuf);

    comp = 0;

    if (bits == 1) {
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "2;201;1;%d;%d;%d;%d;%d;%dscrI", comp, sw, sh, dw, dh, roll);
    } else if (bits == 4) {
      if (pdev -> c4map) {
        /* カラーマップ登録 */
        lputs(s, ESC_GS "64;2;2;16;16plr{E");
        p = tmp = gs_alloc_bytes(vdev->memory, 64, "escv_write_begin(tmp4)");
        for (i = 0; i < 16; i++) {
          *p++ = i << 4;
          *p++ = i << 4;
          *p++ = i << 4;
          *p++ = i << 4;
        }
        put_bytes(s, tmp, 64);
        gs_free_object(vdev->memory, tmp, "escv_write_begin(tmp4)");
        pdev -> c4map = FALSE;
      }
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "2;203;2;%d;%d;%d;%d;%d;%dscrI", comp, sw, sh, dw, dh, roll);
    } else if (bits == 8) {
      if (pdev -> c8map) {
        /* カラーマップ登録 */
        lputs(s, ESC_GS "1024;4;2;256;256plr{E");
        p = tmp = gs_alloc_bytes(vdev->memory, 1024, "escv_write_begin(tmp)");
        for (i = 0; i < 256; i++) {
          *p++ = i;
          *p++ = i;
          *p++ = i;
          *p++ = i;
        }
        put_bytes(s, tmp, 1024);
        gs_free_object(vdev->memory, tmp, "escv_write_begin(tmp)");
        pdev -> c8map = FALSE;
      }
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "2;204;4;%d;%d;%d;%d;%d;%dscrI", comp, sw, sh, dw, dh, roll);
    } else {
      /* 24 bit */
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "2;102;0;%d;%d;%d;%d;%d;%dscrI", comp, sw, sh, dw, dh, roll);
    }

  }	/* ESC/Page-Color */

  lputs(s, obuf);

  return;
}

static void escv_write_data(gx_device *dev, int bits, byte *buf, int bsize, int w, int ras)
{
  gx_device_vector *const  vdev = (gx_device_vector *) dev;
  gx_device_escv   *const  pdev = (gx_device_escv *) dev;
  stream		  *s = gdev_vector_stream((gx_device_vector *)pdev);
  char                     obuf[128];
  int                      size;
  byte                    *tmps, *p;
  unsigned char           *rgbbuf;
  unsigned char           *ucp;
  double                   gray8;

  if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

    tmps = 0;

    if (bits == 12) {
      p = tmps = gs_alloc_bytes(vdev->memory, bsize * 2, "escv_write_data(tmp)");
      for (size = 0; size < bsize; size++) {
        *p++ = buf[size] & 0xF0;
        *p++ = buf[size] << 4;
      }
      bsize = bsize * 2;
      buf = tmps;
    }

    if(bits == 4) {
      p = tmps = gs_alloc_bytes(vdev->memory, bsize * 2, "escv_write_data(tmp)");
      for (size = 0; size < bsize; size++) {
        *p++ = ((buf[size] & 0xF0) * 0xFF / 0xF0);
        *p++ = ((buf[size] << 4 & 0xF0) * 0xFF / 0xF0);
      }
      bsize = bsize * 2;
      buf = tmps;
    }

    if(bits == 24) {		/* 8bit RGB */
      tmps = gs_alloc_bytes(vdev->memory, bsize / 3, "escv_write_data(tmp)");

      /* convert 24bit RGB to 8bit Grayscale */
      rgbbuf = buf;
      ucp = tmps;
      for (size = 0; size < bsize; size = size + 3) {
        gray8 = (0.299L * rgbbuf[size]) + (0.587L * rgbbuf[size + 1]) + (0.114L * rgbbuf[size + 2]);
        if ( gray8 > 255L )
          *ucp = 255;
        else
          *ucp = gray8;
        ucp++;
      }
      bsize = bsize / 3;
      buf = tmps;
    }

    if(bits == 1){
      if (strcmp(pdev->dname, "lp1800") == 0 || \
          strcmp(pdev->dname, "lp9600") == 0) {
        (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%d;1;%d;%d;0db{I", bsize, w, ras);
      }else{
        (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%d;%du{I", bsize, ras);
      }
    }else{
      (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%d;%dcu{I", bsize, ras);
    }
    lputs(s, obuf);

    put_bytes(s, buf, bsize);

    if (bits == 12 || bits == 4 || bits == 24) {
      gs_free_object(vdev->memory, tmps, "escv_write_data(tmp)");
    }

  } else {			/* ESC/Page-Color */

    char                        tmp;

    tmps = 0;
    if (bits == 12) {
      p = tmps = gs_alloc_bytes(vdev->memory, bsize * 2, "escv_write_data(tmp)");
      for (size = 0; size < bsize; size++) {
        tmp = buf[size] & 0xF0;
        *p++ = (tmp + (tmp >> 4 & 0x0F));
        tmp = buf[size] << 4;
        *p++ = (tmp + (tmp >> 4 & 0x0F));
      }
      bsize = bsize * 2;
      buf = tmps;
    }

    (void)gs_snprintf(obuf, sizeof(obuf), ESC_GS "%d;%dcu{I", bsize, ras);
    lputs(s, obuf);
    put_bytes(s, buf, bsize);

    if (bits == 12) {
      gs_free_object(vdev->memory, tmps, "escv_write_data(tmp)");
    }
  }	/* ESC/Page-Color */

  return;
}

static void escv_write_end(gx_device *dev, int bits)
{
  gx_device_escv *const       pdev = (gx_device_escv *) dev;
  stream			*s = gdev_vector_stream((gx_device_vector *)pdev);

  if( 0 == pdev->colormode ) { /* ESC/Page (Monochrome) */

    if(bits == 1){
      if (strcmp(pdev->dname, "lp1800") == 0 || \
          strcmp(pdev->dname, "lp9600") == 0) {
        lputs(s, ESC_GS "1dbI");
      } else {
        lputs(s, ESC_GS "erI");
      }
    }else{
      lputs(s, ESC_GS "ecrI");
    }

  } else {			/* ESC/Page-Color */

    lputs(s, ESC_GS "ecrI");

  }	/* ESC/Page-Color */

  return;
}

/* end of file */
