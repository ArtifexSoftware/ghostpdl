/* Copyright (C) 1998, 1999 Norihito Ohmori.

   Ghostscript printer driver
   for Canon LBP (LIPS IV)

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

/*$Id: gdevl4v.c $ */
/* Vector Version of LIPS driver */

/*

   Vector driver は Ghostscript 5.0 から新たに加わったドライバです。
   よって仕様が安定していません。Ghostscript 5.10 と Ghostscript 5.50 では
   Hi-level bitmap imaging の仕様が変わっています。
   Ghostscript 6.0 では更に text_begin という API が追加されるようです。

   ○Ghostscript 5.10/5.50 のバグについて

   Ghostscript 5.10/5.50 の Vector driver の setlinewidth 関数には
   バグがあります。本来スケールが変更されるにしたがって線の太さも変更され
   なければなりませんが、Ghostscript 5.10/5.50 ではスケールを考慮するのを
   忘れています。(バグ報告を2回ほど送ったのだけれど...)
   このドライバはそのバグを回避するためにスケールを自分で処理しています。

   Ghostscript 5.10 の生成するパスは Ghostscript 5.50 が生成するパスよりも
   非常に効率の悪いものになることがあります。LIPS IV の処理できる限界を超える
   こともありました。効率の悪いパスは速度の低下にもつながるので Ghostscript
   5.50 を使った方がこのドライバでは出力速度が速くなります。

   また Ghostscript 5.10 では
   fill_trapezoid、fill_parallelogram、fill_triangle
   の関数の挙動が変になることがあったのでこれらは使っていません。

   ○ LIPS IV のバグについて

   このドライバは開発中に発見した次の LIPS IV のバグを回避しています。
   1. 1 dot のイメージを変形したときに座標の計算が狂う
   2. 小さなイメージを描画すると正常に描かれない
   3. クリッピング命令とパス・クリッピング命令がそれぞれ独立に働いてしまう。

   1. は 1 dot のイメージを矩形にして回避しました。
   2. はイメージ領域確保命令で分割を指定することにより回避しました。
   3. はクリッピング命令を使わないことにして回避しました。

   ○ LIPS IV の仕様について

   1. 単色イメージ・カラー指定命令はグレースケールイメージに対しても
   働いてしまいます。
   2. LIPS IV ではグレースケールの色表現が PostScript と逆です。
   3. LIPS IV にはイメージのビットを反転させるコマンドはありません。
   4. LIPS IV には CMYK のカラースペースはありません。

 */

#include "string.h"
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gscspace.h"
#include "gsutil.h"
#include "gdevvec.h"
#if 0
#include "gdevpstr.h"
#else
#include "spprint.h"
#endif

#include "gzstate.h"

#include "gdevlips.h"

/* ---------------- Device definition ---------------- */

/* Device procedures */
static dev_proc_open_device(lips4v_open);
static dev_proc_output_page(lips4v_output_page);
static dev_proc_close_device(lips4v_close);
static dev_proc_copy_mono(lips4v_copy_mono);
static dev_proc_copy_color(lips4v_copy_color);
static dev_proc_put_params(lips4v_put_params);
static dev_proc_get_params(lips4v_get_params);
static dev_proc_fill_mask(lips4v_fill_mask);
static dev_proc_begin_typed_image(lips4v_begin_typed_image);

#define X_DPI 600
#define Y_DPI 600

typedef struct gx_device_lips4v_s
{
    gx_device_vector_common;
    lips_params_common;
    lips4_params_common;
    bool first_page;
    bool ManualFeed;
    bool Duplex;
    int Duplex_set;
    bool Tumble;
    bool OneBitMask;		/* for LIPS Bug */
    int ncomp;
    int MaskReverse;
    int MaskState;
    bool TextMode;
    int prev_x;
    int prev_y;
    gx_color_index prev_color;
    gx_color_index current_color;
    int linecap;
    /* for Font Downloading */
#define max_cached_chars 256	/* 128 * n */
    bool FontDL;
    int current_font;
    int count;
    gx_bitmap_id id_table[max_cached_chars + 1];
    gx_bitmap_id id_cache[max_cached_chars + 1];
}
gx_device_lips4v;

gs_public_st_suffix_add0_final(st_device_lips4v, gx_device_lips4v,
                               "gx_device_lips4v", device_lips4v_enum_ptrs,
                               device_lips4v_reloc_ptrs, gx_device_finalize,
                               st_device_vector);

#define lips_device_full_body(dtype, pprocs, dname, stype, w, h, xdpi, ydpi, ncomp, depth, mg, mc, dg, dc, lm, bm, rm, tm)\
        std_device_part1_(dtype, pprocs, dname, stype, open_init_closed),\
        dci_values(ncomp, depth, mg, mc, dg, dc),\
        std_device_part2_(w, h, xdpi, ydpi),\
        offset_margin_values(-lm * xdpi, -tm * ydpi, lm * 72.0, bm * 72.0, rm * 72.0, tm * 72.0),\
        std_device_part3_()

#define lips4v_device_body\
  lips_device_full_body(gx_device_lips4v,\
                        lips4v_initialize_device_procs, "lips4v",\
                        &st_device_lips4v,\
                        DEFAULT_WIDTH_10THS * X_DPI / 10,\
                        DEFAULT_HEIGHT_10THS * Y_DPI / 10,\
                        X_DPI, Y_DPI,\
                        1, 8, 1000, 1000, 5, 2,\
                        LIPS4_LEFT_MARGIN_DEFAULT,\
                        LIPS4_BOTTOM_MARGIN_DEFAULT,\
                        LIPS4_RIGHT_MARGIN_DEFAULT,\
                        LIPS4_TOP_MARGIN_DEFAULT)

static void
lips4v_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, open_device, lips4v_open);
    set_dev_proc(dev, get_initial_matrix, gx_upright_get_initial_matrix);
    set_dev_proc(dev, output_page, lips4v_output_page);
    set_dev_proc(dev, close_device, lips4v_close);
    set_dev_proc(dev, map_rgb_color, gx_default_gray_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_default_gray_map_color_rgb);
    set_dev_proc(dev, fill_rectangle, gdev_vector_fill_rectangle);
    set_dev_proc(dev, copy_mono, lips4v_copy_mono);
    set_dev_proc(dev, copy_color, lips4v_copy_color);
    set_dev_proc(dev, get_params, lips4v_get_params);
    set_dev_proc(dev, put_params, lips4v_put_params);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
    set_dev_proc(dev, fill_path, gdev_vector_fill_path);
    set_dev_proc(dev, stroke_path, gdev_vector_stroke_path);
    set_dev_proc(dev, fill_mask, lips4v_fill_mask);
    set_dev_proc(dev, fill_trapezoid, gdev_vector_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, gdev_vector_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, gdev_vector_fill_triangle);
    set_dev_proc(dev, begin_typed_image, lips4v_begin_typed_image);
    set_dev_proc(dev, get_bits_rectangle, gx_blank_get_bits_rectangle);
}

gx_device_lips4v far_data gs_lips4v_device = {
    lips4v_device_body,
    { 0 },
    vector_initial_values,
    LIPS_CASSETFEED_DEFAULT,
    LIPS_USERNAME_DEFAULT,
    FALSE /* PJL */ ,
    0 /* toner_density */ ,
    FALSE /* toner_saving */ ,
    0 /* toner_saving_set */ ,
    0, 0, 0, 0, -1,
    0 /* prev_duplex_mode */ ,
    LIPS_NUP_DEFAULT,
    LIPS_FACEUP_DEFAULT,
    LIPS_MEDIATYPE_DEFAULT,
    0 /* first_page */ ,
    LIPS_MANUALFEED_DEFAULT,
    0 /* Duplex */ , 0 /* Duplex_set */ , LIPS_TUMBLE_DEFAULT,
    0 /* OneBitMask */ ,
    0 /* ncomp */ , -1 /* MaskReverse */ , 0 /* MaskState */ ,
    0 /* TextMode */ , 0 /* prev_x */ , 0 /* prev_y */ ,
    0 /* prev_color */ , 0 /* current_color */ ,
    0 /* linecap */ ,
    0 /* FontDL */ ,
    -1 /* current_font */ ,
    0 /* count */ ,
    {0},
    {0}
};

/* Vector device implementation */
static int lips4v_beginpage(gx_device_vector * vdev);
static int lips4v_setfillcolor(gx_device_vector * vdev, const gs_gstate * pgs,
                                                                                const gx_drawing_color * pdc);
static int lips4v_setstrokecolor(gx_device_vector * vdev, const gs_gstate * pgs,
                                                                        const gx_drawing_color * pdc);
static int lips4v_setdash(gx_device_vector * vdev, const float *pattern,
                              uint count, double offset);
static int lips4v_setflat(gx_device_vector * vdev, double flatness);
static int
lips4v_setlogop(gx_device_vector * vdev, gs_logical_operation_t lop,
                 gs_logical_operation_t diff);
static int lips4v_can_handle_hl_color(gx_device_vector * vdev, const gs_gstate * pgs,
                  const gx_drawing_color * pdc);
static int

lips4v_beginpath(gx_device_vector * vdev, gx_path_type_t type);
static int
lips4v_moveto(gx_device_vector * vdev, double x0, double y0, double x,
               double y, gx_path_type_t type);
static int
lips4v_lineto(gx_device_vector * vdev, double x0, double y0, double x,
               double y, gx_path_type_t type);
static int
lips4v_curveto(gx_device_vector * vdev, double x0, double y0, double x1,
                double y1, double x2, double y2, double x3, double y3,
                gx_path_type_t type);
static int
lips4v_closepath(gx_device_vector * vdev, double x, double y, double x_start,
                  double y_start, gx_path_type_t type);

static int lips4v_endpath(gx_device_vector * vdev, gx_path_type_t type);
static int lips4v_setlinewidth(gx_device_vector * vdev, double width);
static int lips4v_setlinecap(gx_device_vector * vdev, gs_line_cap cap);
static int lips4v_setlinejoin(gx_device_vector * vdev, gs_line_join join);
static int lips4v_setmiterlimit(gx_device_vector * vdev, double limit);
static const gx_device_vector_procs lips4v_vector_procs = {
    /* Page management */
    lips4v_beginpage,
    /* Imager state */
    lips4v_setlinewidth,
    lips4v_setlinecap,
    lips4v_setlinejoin,
    lips4v_setmiterlimit,
    lips4v_setdash,
    lips4v_setflat,
    lips4v_setlogop,
    /* Other state */
    lips4v_can_handle_hl_color,		/* can_handle_hl_color (dummy) */
    lips4v_setfillcolor,	/* fill & stroke colors are the same */
    lips4v_setstrokecolor,
    /* Paths */
    gdev_vector_dopath,
    gdev_vector_dorect,
    lips4v_beginpath,
    lips4v_moveto,
    lips4v_lineto,
    lips4v_curveto,
    lips4v_closepath,
    lips4v_endpath
};

/* ---------------- File header ---------------- */

static const char *l4v_file_header1 =

    "\033%-12345X@PJL CJLMODE\n@PJL JOB\n\033%-12345X@PJL CJLMODE\n";
static const char *l4v_file_header2 = "@PJL SET LPARM : LIPS SW2 = ON\n";
static const char *l4v_file_header3 = "@PJL ENTER LANGUAGE = LIPS\n";
static const char *l4v_file_header4 = "\033%@\033P41;";

static const char *l4vmono_file_header =

    ";1J" L4VMONO_STRING LIPS_VERSION "\033\\\033[0\"p\033<";

static const char *l4vcolor_file_header =

    ";1J" L4VCOLOR_STRING LIPS_VERSION "\033\\\033[1\"p\033<";

/* ---------------- Utilities ---------------- */

static void
lips_param(int param, char *c)
{
    int i, j;
    bool bSign;

    bSign = TRUE;
    if (param < 0) {
        bSign = FALSE;
        param = -param;
    }
    if (param < 16)
        i = 1;
    else if (param < 1024)
        i = 2;
    else if (param < 65536)
        i = 3;
    else
        i = 4;

    c[i] = '\0';
    c[i - 1] = (param & 0x0f) | 0x20 | (bSign ? 0x10 : 0x00);
    param >>= 4;
    for (j = i - 2; j >= 0; j--) {
        c[j] = (param & 0x3f) | 0x40;
        param >>= 6;
    }
}

static void
sput_lips_int(stream * s, int param)
{
    int i;
    char c[5];

    lips_param(param, c);
    for (i = 0; i < strlen(c); i++)
        sputc(s, c[i]);
}

/* Put a string on a stream.
   This function is copy of `pputs' in gdevpstr.c */
static int
lputs(stream * s, const char *str)
{
    uint len = strlen(str);
    uint used;
    int status = sputs(s, (const byte *)str, len, &used);

    return (status >= 0 && used == len ? 0 : EOF);
}

/* Write a string on a stream. */
static void
put_bytes(stream * s, const byte * data, uint count)
{
    uint used;

    sputs(s, data, count, &used);
}

/* for Font Downloading */
static void
put_int(stream * s, uint number)
{
    sputc(s, number >> 8);
    sputc(s, number & 0xff);
}

static int
lips4v_range_check(gx_device * dev)
{
    int width = dev->MediaSize[0];
    int height = dev->MediaSize[1];
    int xdpi = dev->x_pixels_per_inch;
    int ydpi = dev->y_pixels_per_inch;

    /* Paper Size Check */
    if (width <= height) {	/* portrait */
        if ((width < LIPS_WIDTH_MIN || width > LIPS_WIDTH_MAX ||
             height < LIPS_HEIGHT_MIN || height > LIPS_HEIGHT_MAX) &&
            !(width == LIPS_LEDGER_WIDTH && height == LIPS_LEDGER_HEIGHT))
            return_error(gs_error_rangecheck);
    } else {			/* landscape */
        if ((width < LIPS_HEIGHT_MIN || width > LIPS_HEIGHT_MAX ||
             height < LIPS_WIDTH_MIN || height > LIPS_WIDTH_MAX) &&
            !(width == LIPS_LEDGER_HEIGHT && height == LIPS_LEDGER_WIDTH))
            return_error(gs_error_rangecheck);
    }

    /* Resolution Check */
    if (xdpi != ydpi)
        return_error(gs_error_rangecheck);
    else {
        if ((xdpi < LIPS_DPI_MIN || xdpi > LIPS4_DPI_MAX)
            && xdpi != LIPS4_DPI_SUPERFINE) return_error(gs_error_rangecheck);
    }

    return 0;
}

static void
lips4v_set_cap(gx_device * dev, int x, int y)
{

    char cap[15];
    stream *s = gdev_vector_stream((gx_device_vector *) dev);
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    int dx = x - pdev->prev_x;
    int dy = y - pdev->prev_y;

    if (dx > 0) {
        gs_snprintf(cap, sizeof(cap), "%c%da", LIPS_CSI, dx);
        lputs(s, cap);
    } else if (dx < 0) {
        gs_snprintf(cap, sizeof(cap), "%c%dj", LIPS_CSI, -dx);
        lputs(s, cap);
    }
    if (dy > 0) {
        gs_snprintf(cap, sizeof(cap), "%c%dk", LIPS_CSI, dy);
        lputs(s, cap);
    } else if (dy < 0) {
        gs_snprintf(cap, sizeof(cap), "%c%de", LIPS_CSI, -dy);
        lputs(s, cap);
    }
    pdev->prev_x = x;
    pdev->prev_y = y;
}

#define POINT 18

/* Font Downloading Routine */
static int
lips4v_copy_text_char(gx_device * dev, const byte * data,
                      int raster, gx_bitmap_id id, int x, int y, int w, int h)
{
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    stream *s = gdev_vector_stream((gx_device_vector *) dev);
    uint width_bytes = (w + 7) >> 3;
    uint size = width_bytes * h;
    int i, j;
    uint ccode = 0;
    char cset_sub[9], cset[64], cset_number[8], text_color[15];
    int cell_length = (POINT * (int)dev->x_pixels_per_inch) / 72;
    bool download = TRUE;

    if (w > cell_length || h > cell_length || !pdev->FontDL)
        return -1;

    for (j = pdev->count - 1; j >= 0; j--) {
        if (pdev->id_table[j] == id)
            /* font is found */
        {
            download = FALSE;
            ccode = j;
            for (i = j; i < pdev->count - 1; i++) {
                pdev->id_cache[i] = pdev->id_cache[i + 1];
            }
            pdev->id_cache[pdev->count - 1] = id;
            break;
        }
    }

    if (download) {
        if (pdev->count > max_cached_chars - 1) {
            gx_bitmap_id tmpid = pdev->id_cache[0];

            for (j = pdev->count - 1; j >= 0; j--) {
                if (pdev->id_table[j] == tmpid) {
                    ccode = j;
                    break;
                }
            }
            for (i = j; i < pdev->count - 1; i++) {
                pdev->id_cache[i] = pdev->id_cache[i + 1];
            }
            pdev->id_cache[pdev->count - 1] = tmpid;
        } else {
            ccode = pdev->count;
            pdev->id_cache[pdev->count] = id;
        }
    }
    if (pdev->TextMode == FALSE) {
        /* Text mode */
        lputs(s, "}p");
        sput_lips_int(s, x);
        sput_lips_int(s, y);
        sputc(s, LIPS_IS2);
        pdev->TextMode = TRUE;
        pdev->prev_x = x;
        pdev->prev_y = y;
    } else
        lips4v_set_cap(dev, x, y);

    if (download) {
        if (ccode % 128 == 0 && ccode == pdev->count) {
            /* 文字セット登録補助命令 */
            gs_snprintf(cset_sub, sizeof(cset_sub), "%c%dx%c", LIPS_DCS, ccode / 128, LIPS_ST);
            lputs(s, cset_sub);
            /* 文字セット登録命令 */
            gs_snprintf(cset,sizeof(cset),
                    "%c%d;1;0;0;3840;8;400;100;0;0;200;%d;%d;0;0;;;;;%d.p",
                    LIPS_CSI,
                    size + 9, cell_length,	/* Cell Width */
                    cell_length,	/* Cell Height */
                    (int)dev->x_pixels_per_inch);
            lputs(s, cset);
        } else {
            /* 1文字登録命令 */
            gs_snprintf(cset,sizeof(cset),
                    "%c%d;%d;8;%d.q", LIPS_CSI,
                    size + 9, ccode / 128, (int)dev->x_pixels_per_inch);
            lputs(s, cset);
        }

        /* ユーザ文字登録データ のヘッダ */
        sputc(s, ccode % 128);	/* charcter code */
        put_int(s, w);
        put_int(s, 0);
        put_int(s, h);
        put_int(s, 0);
        for (i = h - 1; i >= 0; --i) {
            put_bytes(s, data + i * raster, width_bytes);
        }
    }
    /* 文字セット・アサイン番号選択命令2 */
    if (pdev->current_font != ccode / 128) {
        gs_snprintf(cset_number, sizeof(cset_number), "%c%d%%v", LIPS_CSI, ccode / 128);
        lputs(s, cset_number);
        pdev->current_font = ccode / 128;
    }

    /* カラー */
    if (pdev->current_color != pdev->prev_color) {
        if (pdev->color_info.depth == 8) {
            sputc(s, LIPS_CSI);
            lputs(s, "?10;2;");
            gs_snprintf(text_color, sizeof(text_color), "%d",
                    (int)(pdev->color_info.max_gray - pdev->current_color));
        } else {
            int r = (pdev->current_color >> 16) * 1000.0 / 255.0;
            int g = ((pdev->current_color >> 8) & 0xff) * 1000.0 / 255.0;
            int b = (pdev->current_color & 0xff) * 1000.0 / 255.0;

            sputc(s, LIPS_CSI);
            lputs(s, "?10;;");
            gs_snprintf(text_color, sizeof(text_color), "%d;%d;%d", r, g, b);
        }
        lputs(s, text_color);
        lputs(s, "%p");
        pdev->prev_color = pdev->current_color;
    }
    /* 制御文字印字命令 */
    if (ccode % 128 == 0x00 ||
        (ccode % 128 >= 0x07 && ccode % 128 <= 0x0F) ||
        ccode % 128 == 0x1B) {
        sputc(s, LIPS_CSI);
        lputs(s, "1.v");
    }
    sputc(s, ccode % 128);

    if (download) {
        pdev->id_table[ccode] = id;
        if (pdev->count < max_cached_chars - 1)
            pdev->count++;
    }
    return 0;
}

static void
reverse_buffer(byte * buf, int Len)
{
    int i;

    for (i = 0; i < Len; i++)
        *(buf + i) = ~*(buf + i);
}

static void
lips4v_write_image_data(gx_device_vector * vdev, byte * buf, int tbyte,
                        int reverse)
{
    stream *s = gdev_vector_stream(vdev);
    byte *cbuf = gs_alloc_bytes(vdev->memory, tbyte * 3 / 2,
                                "lips4v_write_image_data(cbuf)");
    byte *cbuf_rle = gs_alloc_bytes(vdev->memory, tbyte * 3,
                                    "lips4v_write_image_data(cbuf_rle)");
    int Len, Len_rle;

    if (reverse)
        reverse_buffer(buf, tbyte);

    Len = lips_packbits_encode(buf, cbuf, tbyte);
    Len_rle = lips_rle_encode(buf, cbuf_rle, tbyte);

    if (Len > tbyte && Len_rle > tbyte) {
        /* Not compress */
        lputs(s, "0");
        sput_lips_int(s, tbyte);
        sputc(s, LIPS_IS2);

        put_bytes(s, buf, tbyte);
    } else if (Len > Len_rle) {
        /* Use RunLength encode */
        lputs(s, ":");
        sput_lips_int(s, Len_rle);
        sputc(s, LIPS_IS2);

        put_bytes(s, cbuf_rle, Len_rle);
    } else {
        /* Use PackBits encode */
        lputs(s, ";");
        sput_lips_int(s, Len);
        sputc(s, LIPS_IS2);

        put_bytes(s, cbuf, Len);
    }

    gs_free_object(vdev->memory, cbuf, "lips4v_write_image_data(cbuf)");
    gs_free_object(vdev->memory, cbuf_rle,
                   "lips4v_write_image_data(cbuf_rle)");
}

/* ---------------- Vector device implementation ---------------- */

static int
lips4v_beginpage(gx_device_vector * vdev)
{				/*
                                 * We can't use gdev_vector_stream here, because this may be called
                                 * from there before in_page is set.
                                 */
    gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;
    stream *s = vdev->strm;
    int dpi = vdev->x_pixels_per_inch;
    int width = pdev->MediaSize[0];
    int height = pdev->MediaSize[1];
    int paper_size, x0, y0;
    char dpi_char[6], unit[14];
    char page_header[8], l4vmono_page_header[7], l4vcolor_page_header[7];
    char duplex_char[6], tumble_char[6], toner_d[26], toner_s[5],

        nup_char[10];
    char username[6 + LIPS_USERNAME_MAX], feedmode[5], paper[16],

        faceup_char[256];
    bool dup = pdev->Duplex;
    int dupset = pdev->Duplex_set;
    bool tum = pdev->Tumble;

    /* ベクタ・モード移行命令 CSI &} は頁ごとに発行する */

    if (pdev->first_page) {
        if (pdev->pjl) {
            lputs(s, l4v_file_header1);
            if ((int)pdev->x_pixels_per_inch == 1200)
                lputs(s, "@PJL SET RESOLUTION = SUPERFINE\n");
            else if ((int)pdev->x_pixels_per_inch == 600)
                lputs(s, "@PJL SET RESOLUTION = FINE\n");
            else if ((int)pdev->x_pixels_per_inch == 300)
                lputs(s, "@PJL SET RESOLUTION = QUICK\n");
            lputs(s, l4v_file_header2);
            if (pdev->toner_density) {
                gs_snprintf(toner_d, sizeof(toner_d), "@PJL SET TONER-DENSITY=%d\n",
                        pdev->toner_density);
                lputs(s, toner_d);
            }
            if (pdev->toner_saving_set) {
                lputs(s, "@PJL SET TONER-SAVING=");
                if (pdev->toner_saving)
                    gs_snprintf(toner_s, sizeof(toner_s), "ON\n");
                else
                    gs_snprintf(toner_s, sizeof(toner_s), "OFF\n");
                lputs(s, toner_s);
            }
            lputs(s, l4v_file_header3);
        }
        lputs(s, l4v_file_header4);

        if (dpi > 9999)
            return_error(gs_error_rangecheck);

        /* set resolution (dpi) */
        gs_snprintf(dpi_char, sizeof(dpi_char), "%d", dpi);
        lputs(s, dpi_char);

        if (pdev->color_info.depth == 8)
            lputs(s, l4vmono_file_header);
        else
            lputs(s, l4vcolor_file_header);

        /* username */
        gs_snprintf(username, sizeof(username), "%c2y%s%c", LIPS_DCS, pdev->Username, LIPS_ST);
        lputs(s, username);
    }
    if (strcmp(pdev->mediaType, "PlainPaper") == 0) {
        sputc(s, LIPS_CSI);
        lputs(s, "20\'t");
    }
    else if (strcmp(pdev->mediaType, "OHP") == 0 ||
             strcmp(pdev->mediaType, "TransparencyFilm") == 0) {
        sputc(s, LIPS_CSI);
        lputs(s, "40\'t");	/* OHP mode (for LBP-2160) */
    }
    else if (strcmp(pdev->mediaType, "CardBoard") == 0) {
        sputc(s, LIPS_CSI);
        lputs(s, "30\'t");	/* CardBoard mode (for LBP-2160) */
    }
    else if (strcmp(pdev->mediaType, "GlossyFilm") == 0) {
        sputc(s, LIPS_CSI);
        lputs(s, "41\'t");	/* GlossyFilm mode (for LBP-2160) */
    }

    /* 給紙モード */
    if (pdev->ManualFeed ||
        (strcmp(pdev->mediaType, "PlainPaper") != 0
         && strcmp(pdev->mediaType, LIPS_MEDIATYPE_DEFAULT) != 0)) {
        /* Use ManualFeed */
        if (pdev->prev_feed_mode != 10) {
            gs_snprintf(feedmode, sizeof(feedmode), "%c10q", LIPS_CSI);
            lputs(s, feedmode);
            pdev->prev_feed_mode = 10;
        }
    } else {
        if (pdev->prev_feed_mode != pdev->cassetFeed) {
            gs_snprintf(feedmode, sizeof(feedmode), "%c%dq", LIPS_CSI, pdev->cassetFeed);
            lputs(s, feedmode);
            pdev->prev_feed_mode = pdev->cassetFeed;
        }
    }

    paper_size = lips_media_selection(width, height);

    /* 用紙サイズ */
    if (pdev->prev_paper_size != paper_size) {
        if (paper_size == USER_SIZE) {
            /* modified by shige 06/27 2003
            gs_snprintf(paper, sizeof(paper), "%c80;%d;%dp", LIPS_CSI, width * 10, height * 10); */
            /* modified by shige 11/09 2003
            gs_snprintf(paper, sizeof(paper), "%c80;%d;%dp", LIPS_CSI, height * 10, width * 10); */
            gs_snprintf(paper,sizeof(paper),  "%c80;%d;%dp", LIPS_CSI,
                    (height * 10 > LIPS_HEIGHT_MAX_720)?
                    LIPS_HEIGHT_MAX_720 : (height * 10),
                    (width * 10 > LIPS_WIDTH_MAX_720)?
                    LIPS_WIDTH_MAX_720 : (width * 10));
            lputs(s, paper);
        } else if (paper_size == USER_SIZE + LANDSCAPE) {
            /* modified by shige 06/27 2003
            gs_snprintf(paper, sizeof(paper), "%c81;%d;%dp", LIPS_CSI, height * 10, width * 10); */
            /* modified by shige 11/09 2003
            gs_snprintf(paper, sizeof(paper), "%c81;%d;%dp", LIPS_CSI, width * 10, height * 10); */
            gs_snprintf(paper,sizeof(paper),  "%c80;%d;%dp", LIPS_CSI,
                    (width * 10 > LIPS_HEIGHT_MAX_720)?
                    LIPS_HEIGHT_MAX_720 : (width * 10),
                    (height * 10 > LIPS_WIDTH_MAX_720)?
                    LIPS_WIDTH_MAX_720 : (height * 10));
            lputs(s, paper);
        } else {
            gs_snprintf(paper, sizeof(paper), "%c%dp", LIPS_CSI, paper_size);
            lputs(s, paper);
        }
    } else if (paper_size == USER_SIZE) {
        if (pdev->prev_paper_width != width ||
            pdev->prev_paper_height != height) {
                /* modified by shige 06/27 2003
                gs_snprintf(paper, sizeof(paper), "%c80;%d;%dp", LIPS_CSI, width * 10, height * 10); */
                /* modified by shige 11/09 2003
                gs_snprintf(paper, sizeof(paper), "%c80;%d;%dp", LIPS_CSI, height * 10, width * 10); */
                gs_snprintf(paper, sizeof(paper), "%c80;%d;%dp", LIPS_CSI,
                    (height * 10 > LIPS_HEIGHT_MAX_720)?
                    LIPS_HEIGHT_MAX_720 : (height * 10),
                    (width * 10 > LIPS_WIDTH_MAX_720)?
                    LIPS_WIDTH_MAX_720 : (width * 10));
                lputs(s, paper);
        }
    } else if (paper_size == USER_SIZE + LANDSCAPE) {
        if (pdev->prev_paper_width != width ||
            pdev->prev_paper_height != height) {
                /* modified by shige 06/27 2003
                gs_snprintf(paper, sizeof(paper), "%c81;%d;%dp", LIPS_CSI, height * 10, width * 10); */
                /* modified by shige 11/09 2003
                gs_snprintf(paper, sizeof(paper), "%c81;%d;%dp", LIPS_CSI, width * 10, height * 10); */
                gs_snprintf(paper, sizeof(paper), "%c80;%d;%dp", LIPS_CSI,
                    (width * 10 > LIPS_HEIGHT_MAX_720)?
                    LIPS_HEIGHT_MAX_720 : (width * 10),
                    (height * 10 > LIPS_WIDTH_MAX_720)?
                    LIPS_WIDTH_MAX_720 : (height * 10));
                lputs(s, paper);
        }
    }
    pdev->prev_paper_size = paper_size;
    pdev->prev_paper_width = width;
    pdev->prev_paper_height = height;

    if (pdev->faceup) {
        gs_snprintf(faceup_char, sizeof(faceup_char), "%c11;12;12~", LIPS_CSI);
        lputs(s, faceup_char);
    }
    /* N-up Printing Setting */
    if (pdev->first_page) {
        if (pdev->nup != 1) {
            gs_snprintf(nup_char, sizeof(nup_char), "%c%d1;;%do", LIPS_CSI, pdev->nup, paper_size);
            lputs(s, nup_char);
        }
    }
    /* Duplex Setting */
    if (dupset && dup) {
        if (pdev->prev_duplex_mode == 0 || pdev->prev_duplex_mode == 1) {
            gs_snprintf(duplex_char, sizeof(duplex_char), "%c2;#x", LIPS_CSI);	/* duplex */
            lputs(s, duplex_char);
            if (!tum) {
                /* long edge binding */
                if (pdev->prev_duplex_mode != 2) {
                    gs_snprintf(tumble_char, sizeof(tumble_char), "%c0;#w", LIPS_CSI);
                    lputs(s, tumble_char);
                }
                pdev->prev_duplex_mode = 2;
            } else {
                /* short edge binding */
                if (pdev->prev_duplex_mode != 3) {
                    gs_snprintf(tumble_char, sizeof(tumble_char), "%c2;#w", LIPS_CSI);
                    lputs(s, tumble_char);
                }
                pdev->prev_duplex_mode = 3;
            }
        }
    } else if (dupset && !dup) {
        if (pdev->prev_duplex_mode != 1) {
            gs_snprintf(duplex_char, sizeof(duplex_char), "%c0;#x", LIPS_CSI);	/* simplex */
            lputs(s, duplex_char);
        }
        pdev->prev_duplex_mode = 1;
    }
    sputc(s, LIPS_CSI);
    lputs(s, "?1;4;5;6;14l");
    sputc(s, LIPS_CSI);
    lputs(s, "?2;3;h");

    /* size unit (dpi) */
    sputc(s, LIPS_CSI);
    lputs(s, "11h");
    gs_snprintf(unit, sizeof(unit), "%c?7;%d I", LIPS_CSI, (int)pdev->x_pixels_per_inch);
    lputs(s, unit);
    gs_snprintf(page_header, sizeof(page_header), "%c[0&}#%c", LIPS_ESC, LIPS_IS2);
    lputs(s, page_header);	/* vector mode */

    lputs(s, "!0");		/* size unit (dpi) */
    sput_lips_int(s, dpi);
    lputs(s, "1");
    sputc(s, LIPS_IS2);

    if (pdev->color_info.depth == 8) {
        gs_snprintf(l4vmono_page_header, sizeof(l4vmono_page_header), "!13%c$%c", LIPS_IS2, LIPS_IS2);
        lputs(s, l4vmono_page_header);
    } else {
        gs_snprintf(l4vcolor_page_header, sizeof(l4vcolor_page_header), "!11%c$%c", LIPS_IS2, LIPS_IS2);
        lputs(s, l4vcolor_page_header);
    }

    lputs(s, "(00");
    sput_lips_int(s,
                  ((width - dev_l_margin(vdev) - dev_r_margin(vdev)) * dpi) /
                  72);
    sput_lips_int(s,
                  ((height - dev_b_margin(vdev) - dev_t_margin(vdev)) * dpi) /
                  72);
    sputc(s, LIPS_IS2);

    /* 原点移動命令 */
    x0 = (dev_l_margin(vdev) - 5. / MMETER_PER_INCH) * dpi;
    y0 = (dev_b_margin(vdev) - 5. / MMETER_PER_INCH) * dpi;

    if (x0 != 0 && y0 != 0) {
        lputs(s, "}\"");
        sput_lips_int(s, x0);
        sput_lips_int(s, y0);
        sputc(s, LIPS_IS2);
    }
    lputs(s, "I00");
    sputc(s, LIPS_IS2);
    lputs(s, "}F2");
    sputc(s, LIPS_IS2);
    lputs(s, "}H1");
    sputc(s, LIPS_IS2);
    lputs(s, "*0");
    sputc(s, LIPS_IS2);

    pdev->MaskState = 1;	/* 初期化: 透過 */
    pdev->linecap = 0;
    lputs(s, "}M");
    sput_lips_int(s, 3277);	/* 11 degree : 16383 * 2 / 10 */
    sputc(s, LIPS_IS2);
    lputs(s, "}I1");		/* non-zero winding rule is default */
    sputc(s,  LIPS_IS2);

    return 0;
}

static int
lips4v_setlinewidth(gx_device_vector * vdev, double width)
{
    stream *s = gdev_vector_stream(vdev);
    gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;

#if 0
    /* Scale を掛けているのは, Ghostscript 5.10/5.50 のバグのため */
    double xscale, yscale;

    xscale = fabs(igs->ctm.xx);
    yscale = fabs(igs->ctm.xy);

    if (xscale == 0 || yscale > xscale)	/* if portrait */
        width = ceil(width * yscale);
    else
        width = ceil(width * xscale);
#endif

    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }
    if (width < 1)
        width = 1;

    lputs(s, "F1");
    sput_lips_int(s, width);
    sputc(s, LIPS_IS2);

    return 0;
}

static int
lips4v_setlinecap(gx_device_vector * vdev, gs_line_cap cap)
{
    stream *s = gdev_vector_stream(vdev);
    gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;
    char c[6];
    int line_cap = 0;

    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }
    switch (cap) {
        default:
        case 0:
        case 3:
        line_cap = 0;		/* butt */
        break;
        case 1:
        line_cap = 1;		/* round */
        break;
        case 2:
        line_cap = 2;		/* square */
        break;
    }
    /* 線端形状指定命令 */
    gs_snprintf(c, sizeof(c), "}E%d%c", line_cap, LIPS_IS2);
    lputs(s, c);

    pdev->linecap = cap;

    return 0;
}

static int
lips4v_setlinejoin(gx_device_vector * vdev, gs_line_join join)
{
    stream *s = gdev_vector_stream(vdev);
    gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;

/* 線接続指定命令 */
    char c[5];
    int lips_join = 0;

    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }

    switch (join) {
        default:
        case 0:
        lips_join = 2;		/* miter */
        break;
        case 1:
        lips_join = 1;		/* round */
        break;
        case 2:
        lips_join = 3;		/* bevel */
        break;
        case 3:
        case 4:
        lips_join = 0;		/* none */
        break;
    }

    gs_snprintf(c, sizeof(c), "}F%d%c", lips_join, LIPS_IS2);
    lputs(s, c);

    return 0;
}

static int
lips4v_setmiterlimit(gx_device_vector * vdev, double limit)
{
    stream *s = gdev_vector_stream(vdev);
    gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;
    double lips_miterlimit;

    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }
    lips_miterlimit = (16383.0 * 2.0) / limit;

    lputs(s, "}M");
    sput_lips_int(s, lips_miterlimit);
    sputc(s, LIPS_IS2);

    return 0;
}

static int
lips4v_setfillcolor(gx_device_vector * vdev, const gs_gstate * pgs, const gx_drawing_color * pdc)
{

    if (!gx_dc_is_pure(pdc))
        return_error(gs_error_rangecheck);
    {
        stream *s = gdev_vector_stream(vdev);
        gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;
        gx_color_index color = gx_dc_pure_color(pdc);
        int drawing_color = 0;
        float r = 0.0F, g = 0.0F, b = 0.0F;

        if (vdev->color_info.depth == 8) {
            drawing_color = vdev->color_info.max_gray - color;
        } else {
            r = (color >> 16) * 1000.0 / 255.0;
            g = ((color >> 8) & 0xff) * 1000.0 / 255.0;
            b = (color & 0xff) * 1000.0 / 255.0;
        }

        if (pdev->TextMode) {
            sputc(s, LIPS_CSI);
            lputs(s, "&}");
            pdev->TextMode = FALSE;
        }
        pdev->current_color = color;

        if (color == gx_no_color_index) {
            lputs(s, "I0");
            sputc(s, LIPS_IS2);
        } else {
            lputs(s, "I1");
            sputc(s, LIPS_IS2);
        }

        /* 塗りつぶしカラー指定命令 */
        /* J {color} IS2 */
        lputs(s, "J");
        if (vdev->color_info.depth == 8) {
            sput_lips_int(s, drawing_color);
        } else {
            sput_lips_int(s, r);
            sput_lips_int(s, g);
            sput_lips_int(s, b);
        }
        sputc(s, LIPS_IS2);

        /* 単色イメージ・カラー指定命令 */
        /* }T {color} IS2 */
        lputs(s, "}T");
        if (vdev->color_info.depth == 8) {
            sput_lips_int(s, drawing_color);
        } else {
            sput_lips_int(s, r);
            sput_lips_int(s, g);
            sput_lips_int(s, b);
        }
        sputc(s, LIPS_IS2);
    }
    return 0;
}

static int
lips4v_setstrokecolor(gx_device_vector * vdev, const gs_gstate * pgs, const gx_drawing_color * pdc)
{
    if (!gx_dc_is_pure(pdc))
        return_error(gs_error_rangecheck);
    {
        stream *s = gdev_vector_stream(vdev);
        gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;
        gx_color_index color = gx_dc_pure_color(pdc);
        float r = 0.0F, g = 0.0F, b = 0.0F;

        if (vdev->color_info.depth == 24) {
            r = (color >> 16) * 1000 / 255.0;
            g = ((color >> 8) & 0xff) * 1000 / 255.0;
            b = (color & 0xff) * 1000 / 255.0;
        }

        if (pdev->TextMode) {
            sputc(s, LIPS_CSI);
            lputs(s, "&}");
            pdev->TextMode = FALSE;
        }
        /* ラインカラー指定命令 */
        /* G {color} IS2 */
        lputs(s, "G");
        if (vdev->color_info.depth == 8) {
            sput_lips_int(s, vdev->color_info.max_gray - color);
        } else {
            sput_lips_int(s, r);
            sput_lips_int(s, g);
            sput_lips_int(s, b);
        }
        sputc(s, LIPS_IS2);
    }
    return 0;
}

/* 線種指定命令 */
static int
lips4v_setdash(gx_device_vector * vdev, const float *pattern, uint count,
               double offset)
{
    stream *s = gdev_vector_stream(vdev);
    gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;
    int i;
#if 0
    float scale, xscale, yscale;
#endif

    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }
#if 0
    /* Scale を掛けているのは, Ghostscript 5.10/5.50 のバグのため */
    xscale = fabs(igs->ctm.xx);
    yscale = fabs(igs->ctm.xy);

    if (xscale == 0)		/* if portrait */
        scale = yscale;
    else
        scale = xscale;
#endif

    if (count == 0) {
        lputs(s, "E10");
        sputc(s, LIPS_IS2);
    } else {
        lputs(s, "}d");
        sputc(s, 0x2c);
        lputs(s, "1");
#if 0
        sput_lips_int(s, offset * scale / vdev->x_pixels_per_inch + 0.5);
#else
        sput_lips_int(s, offset);
#endif
        for (i = 0; i < count; ++i) {
            if (pdev->linecap == 1 && count == 2 && pattern[0] == 0) {
                if (i == 0) {
                    sput_lips_int(s, 1);
                } else {
#if 0
                    sput_lips_int(s,
                                  pattern[i] * scale /
                                  vdev->x_pixels_per_inch - 0.5);
#else
                    sput_lips_int(s, pattern[i] - 1);
#endif
                }
            } else {
#if 0
                sput_lips_int(s,
                              pattern[i] * scale / vdev->x_pixels_per_inch +
                              0.5);
#else
                sput_lips_int(s, pattern[i]);
#endif
            }
        }
        sputc(s, LIPS_IS2);
        lputs(s, "E1");
        sputc(s, 0x2c);
        lputs(s, "0");
        sputc(s, LIPS_IS2);
    }

    return 0;
}

/* パス平滑度指定 */
static int
lips4v_setflat(gx_device_vector * vdev, double flatness)
{
    stream *s = gdev_vector_stream(vdev);
    gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;

    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }
    lputs(s, "Pf");
    sput_lips_int(s, flatness);
    sputc(s, LIPS_IS2);

    return 0;
}

static int
lips4v_setlogop(gx_device_vector * vdev, gs_logical_operation_t lop,
                gs_logical_operation_t diff)
{
/****** SHOULD AT LEAST DETECT SET-0 & SET-1 ******/
    return 0;
}

/*--- added for Ghostscritp 8.15 ---*/
static int
lips4v_can_handle_hl_color(gx_device_vector * vdev, const gs_gstate * pgs1,
              const gx_drawing_color * pdc)
{
    return false; /* High level color is not implemented yet. */
}

static int
lips4v_beginpath(gx_device_vector * vdev, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vdev);
    gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;

    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }
    /* パス構築開始命令 */
    if (type & gx_path_type_clip) {
        lputs(s, "P(10");
        sputc(s, LIPS_IS2);
    } else {
        lputs(s, "P(00");
        sputc(s, LIPS_IS2);
    }

    return 0;
}

static int
lips4v_moveto(gx_device_vector * vdev, double x0, double y0, double x,
              double y, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vdev);

    /* サブパス開始命令 p1 */
    lputs(s, "p10");
    sput_lips_int(s, x);
    sput_lips_int(s, y);
    sputc(s, LIPS_IS2);

    return 0;
}

static int
lips4v_lineto(gx_device_vector * vdev, double x0, double y0, double x,
              double y, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vdev);
    gx_device_lips4v *const pdev = (gx_device_lips4v *) vdev;

    /* if round cap */
    if (pdev->linecap == 1) {
        if ((x0 == x) && (y0 == y))
            x += 1;
    }

    /* パス・ポリライン命令 */
    lputs(s, "p402");
    sput_lips_int(s, x);
    sput_lips_int(s, y);
    sputc(s, LIPS_IS2);

    return 0;
}

static int
lips4v_curveto(gx_device_vector * vdev, double x0, double y0,
               double x1, double y1, double x2, double y2, double x3,
               double y3, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vdev);

    /* パス・ポリライン命令 */
    lputs(s, "p404");
    sput_lips_int(s, x1);
    sput_lips_int(s, y1);
    sput_lips_int(s, x2);
    sput_lips_int(s, y2);
    sput_lips_int(s, x3);
    sput_lips_int(s, y3);
    sputc(s, LIPS_IS2);

    return 0;
}

static int
lips4v_closepath(gx_device_vector * vdev, double x, double y,
                 double x_start, double y_start, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vdev);

    lputs(s, "p0");
    sputc(s, LIPS_IS2);
    return 0;
}

static int
lips4v_endpath(gx_device_vector * vdev, gx_path_type_t type)
{
    stream *s = gdev_vector_stream(vdev);

    lputs(s, "P)");
    sputc(s, LIPS_IS2);
    if (type & gx_path_type_rule) {
        if (type & gx_path_type_even_odd) {
            lputs(s, "}I0");
            sputc(s, LIPS_IS2);
        } else {
            lputs(s, "}I1");
            sputc(s, LIPS_IS2);
        }
    }
    if (type & gx_path_type_fill) {
        if (type & gx_path_type_stroke) {
            lputs(s, "P&00");
            sputc(s, LIPS_IS2);
        } else {
            lputs(s, "PF00");
            sputc(s, LIPS_IS2);
        }
    }
    if (type & gx_path_type_stroke) {
        lputs(s, "PS00");
        sputc(s, LIPS_IS2);
    }
    if (type & gx_path_type_clip) {
        lputs(s, "PC10");
        sputc(s, LIPS_IS2);
    }
    return 0;
}

/* ---------------- Driver procedures ---------------- */

/* ------ Open/close/page ------ */

/* Open the device. */
static int
lips4v_open(gx_device * dev)
{
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;

    int code;

    code = lips4v_range_check(dev);
    if (code < 0)
        return code;

    vdev->v_memory = dev->memory;
/****** WRONG ******/
    vdev->vec_procs = &lips4v_vector_procs;

    code = gdev_vector_open_file_options(vdev, 512,
                        (VECTOR_OPEN_FILE_SEQUENTIAL|VECTOR_OPEN_FILE_BBOX));
    if (code < 0)
        return code;

    if (pdev->bbox_device != NULL) {
        if (pdev->bbox_device->memory == NULL)
            pdev->bbox_device->memory = gs_memory_stable(dev->memory);
    }

    gdev_vector_init(vdev);
    pdev->first_page = true;

    return 0;
}

/* Wrap up ("output") a page. */
static int
lips4v_output_page(gx_device * dev, int num_copies, int flush)
{
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    stream *s = gdev_vector_stream(vdev);
    char str[6];

    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }
    lputs(s, "%");
    sputc(s, LIPS_IS2);
    lputs(s, "}p");
    sputc(s, LIPS_IS2);

    if (num_copies > 255)
        num_copies = 255;
    if (pdev->prev_num_copies != num_copies) {
        gs_snprintf(str, sizeof(str), "%c%dv", LIPS_CSI, num_copies);
        lputs(s, str);
        pdev->prev_num_copies = num_copies;
    }
    sputc(s, LIPS_FF);
    sflush(s);
    vdev->in_page = false;
    pdev->first_page = false;
    gdev_vector_reset(vdev);
    return 0;
}

static int
lips4v_close(gx_device * dev)
{
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    gp_file *f = vdev->file;

    gp_fprintf(f, "%c0J%c", LIPS_DCS, LIPS_ST);
    if (pdev->pjl) {
        gp_fprintf(f, "%c%%-12345X@PJL SET LPARM : LIPS SW2 = OFF\n", LIPS_ESC);
        gp_fprintf(f,
                "%c%%-12345X%c%%-12345X@PJL EOJ\n"
                "%c%%-12345X", LIPS_ESC, LIPS_ESC, LIPS_ESC);
    }
    gdev_vector_close_file(vdev);

    return 0;
}

/* Close the device. */
/* Note that if this is being called as a result of finalization, */
/* the stream may no longer exist; but the file will still be open. */

/* ---------------- Get/put parameters ---------------- */

/* Get parameters. */
static int
lips4v_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    int code = gdev_vector_get_params(dev, plist);
    int ncode;
    gs_param_string usern;
    gs_param_string pmedia;

    if (code < 0)
        return code;

    if ((ncode = param_write_bool(plist, LIPS_OPTION_MANUALFEED,
                                  &pdev->ManualFeed)) < 0)
        code = ncode;

    if ((ncode = param_write_int(plist, LIPS_OPTION_CASSETFEED,
                                 &pdev->cassetFeed)) < 0)
        code = ncode;

    if ((ncode = param_write_bool(plist, LIPS_OPTION_DUPLEX_TUMBLE,
                                  &pdev->Tumble)) < 0)
        code = ncode;

    if ((ncode = param_write_int(plist, LIPS_OPTION_NUP, &pdev->nup)) < 0)
        code = ncode;

    if ((ncode = param_write_bool(plist, LIPS_OPTION_PJL, &pdev->pjl)) < 0)
        code = ncode;

    if ((ncode = param_write_int(plist, LIPS_OPTION_TONERDENSITY,
                                 &pdev->toner_density)) < 0)
        code = ncode;

    if (pdev->toner_saving_set >= 0 &&
        (code = (pdev->toner_saving_set ?
                 param_write_bool(plist, LIPS_OPTION_TONERSAVING,
                                  &pdev->
                                  toner_saving) : param_write_null(plist,
                                                                   LIPS_OPTION_TONERSAVING)))
        < 0)
        code = ncode;

    if (pdev->Duplex_set >= 0 &&
        (ncode = (pdev->Duplex_set ?
                  param_write_bool(plist, "Duplex", &pdev->Duplex) :
                  param_write_null(plist, "Duplex"))) < 0)
        code = ncode;

    if ((ncode = param_write_bool(plist, LIPS_OPTION_FONTDOWNLOAD,
                                  &pdev->FontDL)) < 0)
        code = ncode;

    if ((ncode = param_write_bool(plist, LIPS_OPTION_FACEUP,
                                  &pdev->faceup)) < 0) code = ncode;

    pmedia.data = (const byte *)pdev->mediaType,
        pmedia.size = strlen(pdev->mediaType), pmedia.persistent = false;

    if ((ncode = param_write_string(plist, LIPS_OPTION_MEDIATYPE,
                                    &pmedia)) < 0) code = ncode;

    if (code < 0)
        return code;

    usern.data = (const byte *)pdev->Username,
        usern.size = strlen(pdev->Username), usern.persistent = false;

    return param_write_string(plist, LIPS_OPTION_USER_NAME, &usern);
}

/* Put parameters. */
static int
lips4v_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    int ecode = 0;
    int code;
    gs_param_name param_name;
    gs_param_string pmedia;
    bool mf = pdev->ManualFeed;
    int cass = pdev->cassetFeed;
    gs_param_string usern;
    bool tum = pdev->Tumble;
    int nup = pdev->nup;
    bool pjl = pdev->pjl;
    int toner_density = pdev->toner_density;
    bool toner_saving = pdev->toner_saving;
    bool toner_saving_set = pdev->toner_saving_set;
    bool fontdl = pdev->FontDL;
    bool faceup = pdev->faceup;
    bool duplex;
    int duplex_set = -1;
    int old_bpp = dev->color_info.depth;
    int bpp = 0;

    if ((code = param_read_bool(plist,
                                (param_name = LIPS_OPTION_MANUALFEED),
                                &mf)) < 0) {
        param_signal_error(plist, param_name, ecode = code);
    }
    switch (code = param_read_int(plist,
                                  (param_name = LIPS_OPTION_CASSETFEED),
                                  &cass)) {
        case 0:
        if (cass < -1 || cass > 17 || (cass > 3 && cass < 10))
            ecode = gs_error_limitcheck;
        else
            break;
        goto casse;
        default:
        ecode = code;
      casse:param_signal_error(plist, param_name, ecode);
        /* Fall through. */
        case 1:
        break;
    }

    switch (code = param_read_string(plist,
                                     (param_name = LIPS_OPTION_MEDIATYPE),
                                     &pmedia)) {
        case 0:
        if (pmedia.size > LIPS_MEDIACHAR_MAX) {
            ecode = gs_error_limitcheck;
            goto pmediae;
        } else {   /* Check the validity of ``MediaType'' characters */
            if (strcmp((const char *)pmedia.data, "PlainPaper") != 0 &&
                strcmp((const char *)pmedia.data, "OHP") != 0 &&
                strcmp((const char *)pmedia.data, "TransparencyFilm") != 0 &&	/* same as OHP */
                strcmp((const char *)pmedia.data, "GlossyFilm") != 0 &&
                strcmp((const char *)pmedia.data, "CardBoard") != 0) {
                ecode = gs_error_rangecheck;
                goto pmediae;
            }
        }
        break;
        default:
        ecode = code;
      pmediae:param_signal_error(plist, param_name, ecode);
        /* Fall through. */
        case 1:
        pmedia.data = 0;
        break;
    }

    switch (code = param_read_string(plist,
                                     (param_name = LIPS_OPTION_USER_NAME),
                                     &usern)) {
        case 0:
        if (usern.size > LIPS_USERNAME_MAX) {
            ecode = gs_error_limitcheck;
            goto userne;
        } else {    /* Check the validity of ``User Name'' characters */
            int i;

            for (i = 0; i < usern.size; i++)
                if (usern.data[i] < 0x20 || usern.data[i] > 0x7e
                    /*
                       && usern.data[i] < 0xa0) ||
                       usern.data[i] > 0xfe
                     */
                    ) {
                    ecode = gs_error_rangecheck;
                    goto userne;
                }
        }
        break;
        default:
        ecode = code;
      userne:param_signal_error(plist, param_name, ecode);
        /* Fall through. */
        case 1:
        usern.data = 0;
        break;
    }

    if ((code = param_read_bool(plist,
                                (param_name = LIPS_OPTION_DUPLEX_TUMBLE),
                                &tum)) < 0)
        param_signal_error(plist, param_name, ecode = code);

    switch (code = param_read_int(plist,
                                  (param_name = LIPS_OPTION_NUP), &nup)) {
        case 0:
        if (nup != 1 && nup != 2 && nup != 4)
            ecode = gs_error_rangecheck;
        else
            break;
        goto nupe;
        default:
        ecode = code;
      nupe:param_signal_error(plist, param_name, ecode);
        /* Fall through. */
        case 1:
        break;
    }

    if ((code = param_read_bool(plist,
                                (param_name = LIPS_OPTION_PJL), &pjl)) < 0)
        param_signal_error(plist, param_name, ecode = code);

    switch (code = param_read_int(plist,
                                  (param_name = LIPS_OPTION_TONERDENSITY),
                                  &toner_density)) {
        case 0:
        if (toner_density < 0 || toner_density > 8)
            ecode = gs_error_rangecheck;
        else
            break;
        goto tden;
        default:
        ecode = code;
      tden:param_signal_error(plist, param_name, ecode);
        /* Fall through. */
        case 1:
        break;
    }

    if (pdev->toner_saving_set >= 0)
        switch (code =
                param_read_bool(plist, (param_name = LIPS_OPTION_TONERSAVING),
                                &toner_saving)) {
            case 0:
            toner_saving_set = 1;
            break;
            default:
            if ((code = param_read_null(plist, param_name)) == 0) {
                toner_saving_set = 0;
                break;
            }
            ecode = code;
            param_signal_error(plist, param_name, ecode);
            case 1:
            break;
        }

    if (pdev->Duplex_set >= 0)	/* i.e., Duplex is supported */
        switch (code = param_read_bool(plist, (param_name = "Duplex"),
                                       &duplex)) {
            case 0:
            duplex_set = 1;
            break;
            default:
            if ((code = param_read_null(plist, param_name)) == 0) {
                duplex_set = 0;
                break;
            }
            ecode = code;
            param_signal_error(plist, param_name, ecode);
            /* Fall through. */
            case 1:
            break;
        }
    if ((code = param_read_bool(plist,
                                (param_name = LIPS_OPTION_FONTDOWNLOAD),
                                &fontdl)) < 0)
        param_signal_error(plist, param_name, ecode = code);

    if ((code = param_read_bool(plist,
                                (param_name = LIPS_OPTION_FACEUP),
                                &faceup)) < 0) {
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
        /* Fall through. */
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
        dev_proc(pdev, map_rgb_color) =
            ((bpp == 8) ? gx_default_gray_map_rgb_color :
             gx_default_rgb_map_rgb_color);
        dev_proc(pdev, map_color_rgb) =
            ((bpp == 8) ? gx_default_gray_map_color_rgb :
             gx_default_rgb_map_color_rgb);
    }

    if (ecode < 0)
        return ecode;
    code = gdev_vector_put_params(dev, plist);
    if (code < 0)
        return code;

    pdev->ManualFeed = mf;
    pdev->cassetFeed = cass;
    pdev->Tumble = tum;
    pdev->nup = nup;
    pdev->pjl = pjl;
    pdev->toner_density = toner_density;
    pdev->toner_saving = toner_saving;
    pdev->toner_saving_set = toner_saving_set;
    pdev->FontDL = fontdl;
    pdev->faceup = faceup;

    if (duplex_set >= 0) {
        pdev->Duplex = duplex;
        pdev->Duplex_set = duplex_set;
    }
    if (pmedia.data != 0 &&
        bytes_compare(pmedia.data, pmedia.size,
                      (const byte *)pdev->mediaType, strlen(pdev->mediaType))
        ) {
        memcpy(pdev->mediaType, pmedia.data, pmedia.size);
        pdev->mediaType[pmedia.size] = 0;
    }
    if (usern.data != 0 &&
        bytes_compare(usern.data, usern.size,
                      (const byte *)pdev->Username, strlen(pdev->Username))
        ) {
        memcpy(pdev->Username, usern.data, usern.size);
        pdev->Username[usern.size] = 0;
    }
    if (bpp != 0 && bpp != old_bpp && pdev->is_open)
        return gs_closedevice(dev);
    return 0;
}

/* ---------------- Images ---------------- */

static int
lips4v_copy_mono(gx_device * dev, const byte * data,
                 int data_x, int raster, gx_bitmap_id id, int x, int y, int w,
                 int h, gx_color_index zero, gx_color_index one)
{
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    stream *s = gdev_vector_stream(vdev);
    int dpi = dev->x_pixels_per_inch;
    gx_drawing_color color;
    int code = 0;
    double r, g, b;

    if (id != gs_no_id && zero == gx_no_color_index &&
        one != gx_no_color_index && data_x == 0) {
        gx_drawing_color dcolor;

        color_set_pure(&dcolor, one);
        lips4v_setfillcolor(vdev, NULL, &dcolor);

        if (lips4v_copy_text_char(dev, data, raster, id, x, y, w, h) >= 0)
            return 0;
    }
    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }
    /*
       (*dev_proc(vdev->bbox_device, copy_mono))
       ((gx_device *)vdev->bbox_device, data, data_x, raster, id,
       x, y, w, h, zero, one);
     */
    if (zero == gx_no_color_index) {
        if (one == gx_no_color_index)
            return 0;
        /* one 色に染め、透過にする */
        if (pdev->MaskState != 1) {
            lputs(s, "}H1");
            sputc(s, LIPS_IS2);
            pdev->MaskState = 1;
        }
        if (pdev->color_info.depth == 8) {
            gx_color_index one_color = vdev->color_info.max_gray - one;

            lputs(s, "}T");
            sput_lips_int(s, one_color);
            sputc(s, LIPS_IS2);
        } else {
            r = (one >> 16) * 1000.0 / 255.0;
            g = ((one >> 8) & 0xff) * 1000.0 / 255.0;
            b = (one & 0xff) * 1000.0 / 255.0;
            lputs(s, "}T");
            sput_lips_int(s, r);
            sput_lips_int(s, g);
            sput_lips_int(s, b);
            sputc(s, LIPS_IS2);
        }
    } else if (one == gx_no_color_index)
        /* 1bit は透明 ビット反転・zero 色に染める */
    {
        gx_color_index zero_color = vdev->color_info.max_gray - zero;

        if (pdev->MaskState != 1) {
            lputs(s, "}H1");
            sputc(s, LIPS_IS2);
            pdev->MaskState = 1;
        }
        if (pdev->color_info.depth == 8) {
            lputs(s, "}T");
            sput_lips_int(s, zero_color);
            sputc(s, LIPS_IS2);
        } else {
            r = (zero >> 16) * 1000.0 / 255.0;
            g = ((zero >> 8) & 0xff) * 1000.0 / 255.0;
            b = (zero & 0xff) * 1000.0 / 255.0;
            lputs(s, "}T");
            sput_lips_int(s, r);
            sput_lips_int(s, g);
            sput_lips_int(s, b);
            sputc(s, LIPS_IS2);
        }
    } else if (one == vdev->white) {
        /* ビット反転 白塗り  zero 色に染める */
        gx_color_index zero_color = vdev->color_info.max_gray - zero;

        if (pdev->MaskState != 0) {
            lputs(s, "}H0");
            sputc(s, LIPS_IS2);
            pdev->MaskState = 0;
        }
        if (pdev->color_info.depth == 8) {
            lputs(s, "}T");
            sput_lips_int(s, zero_color);
            sputc(s, LIPS_IS2);
        } else {
            r = (zero >> 16) * 1000.0 / 255.0;
            g = ((zero >> 8) & 0xff) * 1000.0 / 255.0;
            b = (zero & 0xff) * 1000.0 / 255.0;
            lputs(s, "}T");
            sput_lips_int(s, r);
            sput_lips_int(s, g);
            sput_lips_int(s, b);
            sputc(s, LIPS_IS2);
        }
    } else {
        if (zero != gx_no_color_index) {
            code = (*dev_proc(dev, fill_rectangle)) (dev, x, y, w, h, zero);
            if (code < 0)
                return code;
        }
        if (pdev->MaskState != 1) {
            lputs(s, "}H1");
            sputc(s, LIPS_IS2);
            pdev->MaskState = 1;
        }
        color_set_pure(&color, one);

        code = gdev_vector_update_fill_color((gx_device_vector *) pdev,
                                             NULL, &color);
    }
    if (code < 0)
        return 0;
    lputs(s, "}P");
    sput_lips_int(s, x);	/* Position X */
    sput_lips_int(s, y);	/* Position Y */
    sput_lips_int(s, dpi * 100);
    sput_lips_int(s, dpi * 100);
    sput_lips_int(s, h);	/* Height */
    sput_lips_int(s, w);	/* Width */
    lputs(s, "100110");
    sputc(s, LIPS_IS2);

    lputs(s, "}Q11");

    {
        int i, j;
        uint width_bytes = (w + 7) >> 3;
        uint num_bytes = round_up(width_bytes, 4) * h;
        byte *buf = gs_alloc_bytes(vdev->memory, num_bytes,
                                   "lips4v_copy_mono(buf)");

        if (data_x % 8 == 0) {
            for (i = 0; i < h; ++i) {
                memcpy(buf + i * width_bytes,
                       data + (data_x >> 3) + i * raster, width_bytes);
            }
        } else {
            for (i = 0; i < h; ++i) {
                for (j = 0; j < width_bytes; j++) {
                    *(buf + i * width_bytes + j) =
                        *(data + (data_x >> 3) + i * raster +
                          j) << (data_x % 8) | *(data + (data_x >> 3) +
                                                 i * raster + j + 1) >> (8 -
                                                                         data_x
                                                                         % 8);
                }
            }
        }

        if (one == gx_no_color_index
            || (one == vdev->white
                && zero != gx_no_color_index)) lips4v_write_image_data(vdev,
                                                                       buf,
                                                                       num_bytes,
                                                                       TRUE);
        else
            lips4v_write_image_data(vdev, buf, num_bytes, FALSE);

        gs_free_object(vdev->memory, buf, "lips4v_copy_mono(buf)");
    }

    return 0;
}

/* Copy a color bitmap. */
static int
lips4v_copy_color(gx_device * dev,
                  const byte * data, int data_x, int raster, gx_bitmap_id id,
                  int x, int y, int w, int h)
{
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    gx_device_vector *const vdev = (gx_device_vector *) dev;

    stream *s = gdev_vector_stream(vdev);
    int depth = dev->color_info.depth;
    int dpi = dev->x_pixels_per_inch;
    int num_components = (depth < 24 ? 1 : 3);
    uint width_bytes = w * num_components;

    if (dev->color_info.depth == 8) {
        gx_drawing_color dcolor;

        /* LIPS IV ではグレースケールも単色イメージ・カラー指定命令に
           影響されるので黒色を指定しなければならない。 */
        color_set_pure(&dcolor, vdev->black);
        lips4v_setfillcolor(vdev, NULL, &dcolor);
    } else {
        if (pdev->TextMode) {
            sputc(s, LIPS_CSI);
            lputs(s, "&}");
            pdev->TextMode = FALSE;
        }
    }

    if (pdev->MaskState != 0) {
        lputs(s, "}H0");	/* 論理描画設定命令 */
        sputc(s, LIPS_IS2);
        pdev->MaskState = 0;
    }
    lputs(s, "}P");
    sput_lips_int(s, x);
    sput_lips_int(s, y);
    sput_lips_int(s, dpi * 100);
    sput_lips_int(s, dpi * 100);
    sput_lips_int(s, h);
    sput_lips_int(s, w);
    sput_lips_int(s, depth / num_components);
    sputc(s, depth < 24 ? '0' : ':');	/* 24 bit のとき点順次 */
    lputs(s, "0110");
    sputc(s, LIPS_IS2);

    {
        int i;
        uint num_bytes = width_bytes * h;
        byte *buf = gs_alloc_bytes(vdev->memory, num_bytes,
                                   "lips4v_copy_color(buf)");

        lputs(s, "}Q11");

        for (i = 0; i < h; ++i) {
            memcpy(buf + i * width_bytes,
                   data + ((data_x * depth) >> 3) + i * raster, width_bytes);
        }

        if (dev->color_info.depth == 8)
            lips4v_write_image_data(vdev, buf, num_bytes, TRUE);
        else
            lips4v_write_image_data(vdev, buf, num_bytes, FALSE);

        gs_free_object(vdev->memory, buf, "lips4v_copy_color(buf)");
    }

    return 0;
}

/* Fill a mask. */
static int
lips4v_fill_mask(gx_device * dev,
                 const byte * data, int data_x, int raster, gx_bitmap_id id,
                 int x, int y, int w, int h,
                 const gx_drawing_color * pdcolor, int depth,
                 gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    stream *s = gdev_vector_stream(vdev);
    int dpi = dev->x_pixels_per_inch;

    if (w <= 0 || h <= 0)
        return 0;
    if (depth > 1 ||
        gdev_vector_update_fill_color(vdev, NULL, pdcolor) < 0 ||
        gdev_vector_update_clip_path(vdev, pcpath) < 0 ||
        gdev_vector_update_log_op(vdev, lop) < 0)
        return gx_default_fill_mask(dev, data, data_x, raster, id,
                                    x, y, w, h, pdcolor, depth, lop, pcpath);
#if 1
    (*dev_proc(vdev->bbox_device, fill_mask))
        ((gx_device *) vdev->bbox_device, data, data_x, raster, id,
         x, y, w, h, pdcolor, depth, lop, pcpath);
#endif
    if (id != gs_no_id && data_x == 0) {
        if (lips4v_copy_text_char(dev, data, raster, id, x, y, w, h) >= 0)
            return 0;
    }
    if (pdev->TextMode) {
        sputc(s, LIPS_CSI);
        lputs(s, "&}");
        pdev->TextMode = FALSE;
    }
    /* 書きだし */
    if (pdev->MaskState != 1) {
        lputs(s, "}H1");	/* 論理描画設定命令 */
        sputc(s, LIPS_IS2);
        pdev->MaskState = 1;
    }
    lputs(s, "}P");
    sput_lips_int(s, x);
    sput_lips_int(s, y);
    sput_lips_int(s, dpi * 100);
    sput_lips_int(s, dpi * 100);
    sput_lips_int(s, h);
    sput_lips_int(s, w);
    lputs(s, "100110");
    sputc(s, LIPS_IS2);

    lputs(s, "}Q11");

    {
        int i;
        uint width_bytes = (w + 7) >> 3;
        uint num_bytes = round_up(width_bytes, 4) * h;
        byte *buf = gs_alloc_bytes(vdev->memory, num_bytes,
                                   "lips4v_fill_mask(buf)");

        /* This code seems suspect to me; we allocate a buffer
         * rounding each line up to a multiple of 4, and then
         * fill it without reference to this rounding. I suspect
         * that each line should be padded, rather than all the
         * data being crammed at the start, but I can't make that
         * change in the absence of any way to test this. I will
         * make do by adding the memset here so that any untouched
         * bytes are at least consistently set to 0 to avoid
         * indeterminisms (and valgrind errors). RJW */
        if (width_bytes * h < num_bytes) {
            memset(buf + width_bytes * h, 0, num_bytes - width_bytes * h);
        }
        for (i = 0; i < h; ++i) {
            memcpy(buf + i * width_bytes, data + (data_x >> 3) + i * raster,
                   width_bytes);
        }

        lips4v_write_image_data(vdev, buf, num_bytes, FALSE);

        gs_free_object(vdev->memory, buf, "lips4v_fill_mask(buf)");
    }

    return 0;
}

/* ---------------- High-level images ---------------- */

static image_enum_proc_plane_data(lips4v_image_plane_data);
static image_enum_proc_end_image(lips4v_image_end_image);
static const gx_image_enum_procs_t lips4v_image_enum_procs = {
    lips4v_image_plane_data, lips4v_image_end_image
};

/* Start processing an image. */
static int
lips4v_begin_typed_image(gx_device               *dev,
                   const gs_gstate               *pgs,
                   const gs_matrix               *pmat,
                   const gs_image_common_t       *pic,
                   const gs_int_rect             *prect,
                   const gx_drawing_color        *pdcolor,
                   const gx_clip_path            *pcpath,
                         gs_memory_t             *mem,
                         gx_image_enum_common_t **pinfo)
{
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    const gs_image_t *pim = (const gs_image_t *)pic;
    gdev_vector_image_enum_t *pie;
    const gs_color_space *pcs;
    gs_color_space_index index = 0;
    int num_components = 1;
    bool can_do;
    int code;

    pie = gs_alloc_struct(mem, gdev_vector_image_enum_t,
                          &st_vector_image_enum, "lips4v_begin_typed_image");
    if (pie == 0)
        return_error(gs_error_VMerror);
    pie->memory = mem;

    /* We can only cope with type 1 images here.*/
    if (pic->type->index != 1) {
        *pinfo = (gx_image_enum_common_t *) pie;
        goto fallback;
    }

    pcs = pim->ColorSpace;
    can_do = prect == NULL &&
        (pim->format == gs_image_format_chunky ||
         pim->format == gs_image_format_component_planar);

    code = gdev_vector_begin_image(vdev, pgs, pim, pim->format, prect,
                                   pdcolor, pcpath, mem,
                                   &lips4v_image_enum_procs, pie);
    if (code < 0)
        return code;
    *pinfo = (gx_image_enum_common_t *) pie;

    if (!pim->ImageMask) {
        index = gs_color_space_get_index(pcs);
        num_components = gs_color_space_num_components(pcs);

        if (pim->CombineWithColor)
            can_do = false;
        else {
            switch (index) {
                case gs_color_space_index_DeviceGray:
                if ((pim->Decode[0] != 0 || pim->Decode[1] != 1)
                    && (pim->Decode[0] != 1 || pim->Decode[1] != 0))
                    can_do = false;
                break;
                case gs_color_space_index_DeviceRGB:
                /* LIPS では RGB を反転することはできない */
                if (pim->Decode[0] != 0 || pim->Decode[1] != 1 ||
                    pim->Decode[2] != 0 || pim->Decode[3] != 1 ||
                    pim->Decode[4] != 0)
                    can_do = false;
                break;
                default:
                /*
                   LIPS では L*a*b* 形式のカラースペースが使えます。
                   CIEBasedABC を使って表現できないこともないのですが、
                   動作確認できないので低レベルの関数にまかせることに
                   しちゃいます。
                   それよりも CMYK のカラースペースが欲しい...
                 */
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
    else if (index == gs_color_space_index_DeviceGray) {
        gx_drawing_color dcolor;

        /* LIPS IV ではグレースケールも単色イメージ・カラー指定命令に
           影響されるので黒色を明示的に指定しなければならない。 */
        color_set_pure(&dcolor, vdev->black);
        lips4v_setfillcolor(vdev, NULL, &dcolor);
    }
    if (pim->ImageMask || (pim->BitsPerComponent == 1 && num_components == 1)) {
        if (pim->Decode[0] > pim->Decode[1])
            pdev->MaskReverse = 1;
        else
            pdev->MaskReverse = 0;
    }
    /* Write the image/colorimage/imagemask preamble. */
    {
        stream *s = gdev_vector_stream((gx_device_vector *) pdev);
        int ax, ay, bx, by, cx, cy, dx, dy;
        int tbyte;
        gs_matrix imat;
        int interpolate = 0;

        if (pdev->TextMode) {
            sputc(s, LIPS_CSI);
            lputs(s, "&}");
            pdev->TextMode = FALSE;
        }
        code = gs_matrix_invert(&pim->ImageMatrix, &imat);
        if (code < 0)
            return code;

        if (pmat == NULL)
            pmat = &ctm_only(pgs);
        gs_matrix_multiply(&imat, pmat, &imat);
        /*
           [xx xy yx yy tx ty]
           LIPS の座標系に変換を行なう。

           }U{Ax}{Ay}{Bx}{By}{Cx}{Cy}{pie->height}{pie->width}
           {pim->BitsPerComponent}{0}{0}{1} LIPS_IS2
         */

        /* }Q110{byte} LIPS_IS2 */
        ax = imat.tx;
        ay = imat.ty;
        bx = imat.xx * pim->Width + imat.yx * pim->Height + imat.tx;
        by = imat.xy * pim->Width + imat.yy * pim->Height + imat.ty;
        cx = imat.yx * pim->Height + imat.tx;
        cy = imat.yy * pim->Height + imat.ty;
        dx = imat.xx * pim->Width + imat.tx;
        dy = imat.xy * pim->Width + imat.ty;

        if (pim->ImageMask) {
            tbyte =
                (pie->width * pim->BitsPerComponent +
                 7) / 8 * num_components * pie->height;
            pdev->ncomp = 1;
            if (tbyte == 1) {
                /* LIPS IV では 1 dot のイメージを変形すると座標が狂うバグが
                   ある。よって 1 dot のイメージは矩形として処理する。 */
                pdev->OneBitMask = true;
                /* Draw Rectangle */
                lputs(s, "2");
                sput_lips_int(s, ax);
                sput_lips_int(s, ay);
                sput_lips_int(s, cx - ax);
                sput_lips_int(s, cy - ay);
                sput_lips_int(s, bx - cx);
                sput_lips_int(s, by - cy);
                sput_lips_int(s, dx - bx);
                sput_lips_int(s, dy - by);
                sputc(s, LIPS_IS2);
                return 0;
            } else {
                /* 描画論理設定命令 - 透過 */
                if (pdev->MaskState != 1) {
                    lputs(s, "}H1");
                    sputc(s, LIPS_IS2);
                    pdev->MaskState = 1;
                }
            }
        } else {
            /* 描画論理設定命令 - 白塗り */
            if (pdev->MaskState != 0) {
                lputs(s, "}H0");
                sputc(s, LIPS_IS2);
                pdev->MaskState = 0;
            }
            pdev->ncomp = num_components;
        }

        lputs(s, "}U");
        sput_lips_int(s, ax);
        sput_lips_int(s, ay);
        sput_lips_int(s, bx);
        sput_lips_int(s, by);
        sput_lips_int(s, cx);
        sput_lips_int(s, cy);
        sput_lips_int(s, pie->height);
        sput_lips_int(s, pie->width);
        sput_lips_int(s, pim->BitsPerComponent);

        if (pim->Interpolate) {
            if (pim->BitsPerComponent * pie->num_planes == 1)
                interpolate = 1;
            else
                interpolate = 3;
        }
        if (pim->ImageMask) {	/* 1bit のとき */
            lputs(s, "0");
        } else {
            if (index == gs_color_space_index_DeviceGray)
                lputs(s, "0");
            else {
                if (pim->format == gs_image_format_chunky)	/* RGBRGBRGB... */
                    sputc(s, 0x3a);
                else		/* RRR...GGG...BBB... */
                    sputc(s, 0x3b);
            }
        }
        if (interpolate > 0)
            sput_lips_int(s, interpolate);
        sputc(s, LIPS_IS2);
    }
    return 0;
}

/* Process the next piece of an image. */
static int
lips4v_image_plane_data(gx_image_enum_common_t * info,
                        const gx_image_plane_t * planes, int height,
                        int *rows_used)
{
    gx_device *dev = info->dev;
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    gdev_vector_image_enum_t *pie = (gdev_vector_image_enum_t *) info;

    stream *s = gdev_vector_stream((gx_device_vector *) pdev);
    int y;

    if (pdev->OneBitMask) {
        /*
           lputs(s, "\200");
         */
        pie->y += height;
        return 1;
    }
    if (pie->default_info)
        return gx_image_plane_data(pie->default_info, planes, height);
    gx_image_plane_data(pie->bbox_info, planes, height);
    {
        int plane;
        int width_bytes, tbyte;
        byte *buf;

        width_bytes =
            (pie->width * pie->bits_per_pixel / pdev->ncomp +
             7) / 8 * pdev->ncomp;
        tbyte = width_bytes * height;
        buf = gs_alloc_bytes(vdev->memory, tbyte, "lips4v_image_data(buf)");

        for (plane = 0; plane < pie->num_planes; ++plane)
            for (y = 0; y < height; ++y) {
                memcpy(buf + y * width_bytes,
                       planes[plane].data +
                       ((planes[plane].data_x * pie->bits_per_pixel) >> 3)
                       + y * planes[plane].raster, width_bytes);
            }

        lputs(s, "}Q10");

        if ((pie->bits_per_pixel > 1 && pdev->ncomp == 1) ||
            pdev->MaskReverse == 0) {
            lips4v_write_image_data(vdev, buf, tbyte, TRUE);
        } else
            lips4v_write_image_data(vdev, buf, tbyte, FALSE);

        gs_free_object(vdev->memory, buf, "lips4v_image_data(buf)");

    }

    return (pie->y += height) >= pie->height;
}

static int
lips4v_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    gx_device *dev = info->dev;
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_lips4v *const pdev = (gx_device_lips4v *) dev;
    gdev_vector_image_enum_t *pie = (gdev_vector_image_enum_t *) info;
    stream *s = gdev_vector_stream((gx_device_vector *) pdev);
    int code;

    if (pdev->OneBitMask)
        pdev->OneBitMask = false;
    else {
        lputs(s, "}Q1100");
        sputc(s, LIPS_IS2);	/* End of Image */
    }

    pdev->MaskReverse = -1;

    code = gdev_vector_end_image(vdev, (gdev_vector_image_enum_t *) pie,
                                 draw_last, pdev->white);
    return code;
}
