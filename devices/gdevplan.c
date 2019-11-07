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

/* PLANar devices */
#include "gdevprn.h"
#include "gscdefs.h"
#include "gscspace.h" /* For pnm_begin_typed_image(..) */
#include "gxgetbit.h"
#include "gxlum.h"
#include "gxiparam.h" /* For pnm_begin_typed_image(..) */
#include "gdevmpla.h"
#include "gdevplnx.h"
#include "gdevppla.h"
#include "gdevmem.h"

/* This file defines 5 different devices:
 *
 *  plan   24 bit RGB (8 bits per channel)
 *  plang   8 bit Grayscale
 *  planm   1 bit Monochrome
 *  planc  32 bit CMYK (8 bits per channel)
 *  plank   4 bit CMYK (1 bit per channel)
 *  planr   3 bit RGB (1 bit per channel)
 */

/* Define DEBUG_PRINT to enable some debugging printfs. */
#undef DEBUG_PRINT

/* Define DEBUG_DUMP to dump the data to the output stream. */
#define DEBUG_DUMP

/* Define HT_RAW_DUMP to store the output as a raw CMYK buffer with the
   data size packed into the file name.  Photoshop does not handle pam
   cmyk properly so we resort to this for debugging */
#define HT_RAW_DUMP

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 600
#define Y_DPI 600

/* For all but mono, we need our own color mapping and alpha procedures. */
static dev_proc_decode_color(plan_decode_color);
static dev_proc_encode_color(plang_encode_color);
static dev_proc_decode_color(plang_decode_color);
static dev_proc_encode_color(planc_encode_color);
static dev_proc_decode_color(planc_decode_color);
static dev_proc_map_color_rgb(planc_map_color_rgb);

static dev_proc_open_device(plan_open);
static dev_proc_close_device(plan_close);

/* And of course we need our own print-page routines. */
static dev_proc_print_page(plan_print_page);

static int plan_print_page(gx_device_printer * pdev, gp_file * pstream);
static int planm_print_page(gx_device_printer * pdev, gp_file * pstream);
static int plang_print_page(gx_device_printer * pdev, gp_file * pstream);
static int planc_print_page(gx_device_printer * pdev, gp_file * pstream);
static int plank_print_page(gx_device_printer * pdev, gp_file * pstream);
static int planr_print_page(gx_device_printer * pdev, gp_file * pstream);

/* The device procedures */

/* See gdevprn.h for the template for the following. */
#define pgpm_procs(p_color_rgb, encode_color, decode_color) {\
        plan_open,\
        NULL, /* get_initial_matrix */ \
        NULL, /* sync output */ \
        /* Since the print_page doesn't alter the device, this device can print in the background */\
        gdev_prn_bg_output_page, \
        plan_close,\
        NULL, /* map_rgb_color */ \
        p_color_rgb, /* map_color_rgb */ \
        NULL, /* fill_rectangle */ \
        NULL, /* tile_rectangle */ \
        NULL, /* copy_mono */ \
        NULL, /* copy_color */ \
        NULL, /* draw_line */ \
        NULL, /* get_bits */ \
        gdev_prn_get_params, \
        gdev_prn_put_params,\
        NULL, /* map_cmyk_color */ \
        NULL, /* get_xfont_procs */ \
        NULL, /* get_xfont_device */ \
        NULL, /* map_rgb_alpha_color */ \
        gx_page_device_get_page_device, \
        NULL,   /* get_alpha_bits */\
        NULL,   /* copy_alpha */\
        NULL,   /* get_band */\
        NULL,   /* copy_rop */\
        NULL,   /* fill_path */\
        NULL,   /* stroke_path */\
        NULL,   /* fill_mask */\
        NULL,   /* fill_trapezoid */\
        NULL,   /* fill_parallelogram */\
        NULL,   /* fill_triangle */\
        NULL,   /* draw_thin_line */\
        NULL,   /* begin_image */\
        NULL,   /* image_data */\
        NULL,   /* end_image */\
        NULL,   /* strip_tile_rectangle */\
        NULL,   /* strip_copy_rop */\
        NULL,   /* get_clipping_box */\
        NULL,   /* begin_typed_image */\
        NULL,   /* get_bits_rectangle */\
        NULL,   /* map_color_rgb_alpha */\
        NULL,   /* create_compositor */\
        NULL,   /* get_hardware_params */\
        NULL,   /* text_begin */\
        NULL,   /* finish_copydevice */\
        NULL,   /* begin_transparency_group */\
        NULL,   /* end_transparency_group */\
        NULL,   /* begin_transparency_mask */\
        NULL,   /* end_transparency_mask */\
        NULL,   /* discard_transparency_layer */\
        NULL,   /* get_color_mapping_procs */\
        NULL,   /* get_color_comp_index */\
        encode_color, /* encode_color */\
        decode_color, /* decode_color */\
        NULL,   /* pattern_manage */\
        NULL,   /* fill_rectangle_hl_color */\
        NULL,   /* include_color_space */\
        NULL,   /* fill_linear_color_scanline */\
        NULL,   /* fill_linear_color_trapezoid */\
        NULL,   /* fill_linear_color_triangle */\
        NULL,	/* update spot */\
        NULL,   /* DevN params */\
        NULL,   /* fill page */\
        NULL,   /* push_transparency_state */\
        NULL,   /* pop_transparency_state */\
        NULL,   /* put_image */\
        NULL    /* dev_spec_op */\
}

static const gx_device_procs planm_procs =
  pgpm_procs(gdev_prn_map_color_rgb, gdev_prn_map_rgb_color, gdev_prn_map_color_rgb);
static const gx_device_procs plang_procs =
  pgpm_procs(plang_decode_color, plang_encode_color, plang_decode_color);
static const gx_device_procs plan_procs =
  pgpm_procs(plan_decode_color, gx_default_rgb_map_rgb_color, plan_decode_color);
static const gx_device_procs planc_procs =
  pgpm_procs(planc_map_color_rgb, planc_encode_color, planc_decode_color);
static const gx_device_procs plank_procs =
  pgpm_procs(planc_map_color_rgb, planc_encode_color, planc_decode_color);
static const gx_device_procs planr_procs =
  pgpm_procs(plan_decode_color, gx_default_rgb_map_rgb_color, plan_decode_color);

/* Macro for generating device descriptors. */
#define plan_prn_device(procs, dev_name, num_comp, depth, max_gray, max_rgb, print_page) \
{       prn_device_body(gx_device_printer, procs, dev_name,\
        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, X_DPI, Y_DPI,\
        0, 0, 0, 0,\
        num_comp, depth, max_gray, max_rgb, max_gray + 1, max_rgb + 1,\
        print_page)\
}

/* The device descriptors themselves */
const gx_device_printer gs_plan_device =
  plan_prn_device(plan_procs, "plan", 3, 24, 255, 255, plan_print_page);
const gx_device_printer gs_plang_device =
  plan_prn_device(plang_procs, "plang", 1, 8, 255, 0, plang_print_page);
const gx_device_printer gs_planm_device =
  plan_prn_device(planm_procs, "planm", 1, 1, 1, 0, planm_print_page);
const gx_device_printer gs_plank_device =
  plan_prn_device(plank_procs, "plank", 4, 4, 1, 1, plank_print_page);
const gx_device_printer gs_planc_device =
  plan_prn_device(planc_procs, "planc", 4, 32, 255, 255, planc_print_page);
const gx_device_printer gs_planr_device =
  plan_prn_device(planr_procs, "planr", 3, 3, 1, 1, planr_print_page);

/* ------ Initialization ------ */

#ifdef DEBUG_DUMP
static void dump_row_ppm(int w, byte **data, gp_file *dump_file)
{
    byte *r = data[0];
    byte *g = data[1];
    byte *b = data[2];

    if (dump_file == NULL)
        return;
    while (w--) {
        gp_fputc(*r++, dump_file);
        gp_fputc(*g++, dump_file);
        gp_fputc(*b++, dump_file);
    }
}

static void dump_row_pnmk(int w, byte **data, gp_file *dump_file)
{
    byte *r = data[0];
    byte *g = data[1];
    byte *b = data[2];
    byte *k = data[3];

    if (dump_file == NULL)
        return;
    while (w) {
        byte C = *r++;
        byte M = *g++;
        byte Y = *b++;
        byte K = *k++;
        int s;
        for (s=7; s>=0; s--) {
            gp_fputc(255*((C>>s)&1), dump_file);
            gp_fputc(255*((M>>s)&1), dump_file);
            gp_fputc(255*((Y>>s)&1), dump_file);
            gp_fputc(255*((K>>s)&1), dump_file);
            w--;
            if (w == 0) break;
        }
    }
}

static void dump_row_pnmc(int w, byte **data, gp_file *dump_file)
{
    byte *r = data[0];
    byte *g = data[1];
    byte *b = data[2];
    byte *k = data[3];

    if (dump_file == NULL)
        return;
    while (w--) {
        gp_fputc(*r++, dump_file);
        gp_fputc(*g++, dump_file);
        gp_fputc(*b++, dump_file);
        gp_fputc(*k++, dump_file);
    }
}

static void dump_row_pbm(int w, byte **data, gp_file *dump_file)
{
    byte *r = data[0];
#ifdef CLUSTER
    int end = w>>3;
    byte mask = 255>>(w&7);
    if (w & 7)
        mask = ~mask;
    else
        end--;
#else
    byte mask = 255;
#endif

    if (dump_file == NULL)
        return;
    if (w == 0)
        return;
    w = (w+7)>>3;
    while (--w) {
        gp_fputc(*r++, dump_file);
    }
    gp_fputc(mask & *r, dump_file);
}

static void dump_row_pgm(int w, byte **data, gp_file *dump_file)
{
    byte *r = data[0];

    if (dump_file == NULL)
        return;
    while (w--) {
        gp_fputc(*r++, dump_file);
    }
}

static void dump_row_pnmr(int w, byte **data, gp_file *dump_file)
{
    byte *r = data[0];
    byte *g = data[1];
    byte *b = data[2];

    if (dump_file == NULL)
        return;
    while (w) {
        byte R = *r++;
        byte G = *g++;
        byte B = *b++;
        int s;
        for (s=7; s>=0; s--) {
            gp_fputc(255*((R>>s)&1), dump_file);
            gp_fputc(255*((G>>s)&1), dump_file);
            gp_fputc(255*((B>>s)&1), dump_file);
            w--;
            if (w == 0) break;
        }
    }
}

typedef void (*dump_row)(int w, byte **planes, gp_file *file);

static dump_row dump_start(int w, int h, int num_comps, int log2bits,
                           gp_file *dump_file)
{
    dump_row row_proc = NULL;
    if ((num_comps == 3) && (log2bits == 3)) {
        row_proc = dump_row_ppm;
    } else if ((num_comps == 1) && (log2bits == 0)) {
        row_proc = dump_row_pbm;
    } else if ((num_comps == 1) && (log2bits == 3)) {
        row_proc = dump_row_pgm;
    } else if ((num_comps == 4) && (log2bits == 0)) {
        row_proc = dump_row_pnmk;
    } else if ((num_comps == 4) && (log2bits == 3)) {
        row_proc = dump_row_pnmc;
    } else if ((num_comps == 3) && (log2bits == 0)) {
        row_proc = dump_row_pnmr;
    } else
        return NULL;
    if (dump_file == NULL)
        return row_proc;
    if (num_comps == 3) {
        if (log2bits == 0)
            gp_fprintf(dump_file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 3\n"
                    "MAXVAL 255\nTUPLTYPE RGB\n# Image generated by %s\nENDHDR\n", w, h, gs_product);
        else
            gp_fprintf(dump_file, "P6 %d %d 255\n", w, h);
    } else if (num_comps == 4) {
        if (log2bits == 0)
            gp_fprintf(dump_file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\n"
                    "MAXVAL 255\nTUPLTYPE CMYK\n# Image generated by %s\nENDHDR\n", w, h, gs_product);

        else
            gp_fprintf(dump_file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\n"
                    "MAXVAL 255\nTUPLTYPE CMYK\n# Image generated by %s\nENDHDR\n", w, h, gs_product);
    } else if (log2bits == 0)
        gp_fprintf(dump_file, "P4 %d %d\n", w, h);
    else
        gp_fprintf(dump_file, "P5 %d %d 255\n", w, h);
    return row_proc;
}
#endif

/*
 * Define a special open procedure to ensure we use a planar device.
 */
static int
plan_open(gx_device * pdev)
{
    int code;

#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plan_open\n");
#endif
    code = gdev_prn_open_planar(pdev, 1);
    if (code < 0)
        return code;
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    set_linear_color_bits_mask_shift(pdev);

    return code;
}

static int
plan_close(gx_device *pdev)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plan_close\n");
#endif

    return gdev_prn_close(pdev);
}

/* ------ Color mapping routines ------ */

/* Map an RGB color to a gray value. */
static gx_color_index
plang_encode_color(gx_device * pdev, const gx_color_value cv[])
{                               /* We round the value rather than truncating it. */
    gx_color_value gray;
    gx_color_value r, g, b;

    r = cv[0]; g = cv[0]; b = cv[0];
    gray = ((r * (ulong) lum_red_weight) +
     (g * (ulong) lum_green_weight) +
     (b * (ulong) lum_blue_weight) +
     (lum_all_weights / 2)) / lum_all_weights
    * pdev->color_info.max_gray / gx_max_color_value;

    return gray;
}

/* Map a gray value back to an RGB color. */
static int
plang_decode_color(gx_device * dev, gx_color_index color,
                   gx_color_value prgb[3])
{
    gx_color_value gray =
    color * gx_max_color_value / dev->color_info.max_gray;

    prgb[0] = gray;
    prgb[1] = gray;
    prgb[2] = gray;
    return 0;
}

/* Map an rgb color tuple back to an RGB color. */
static int
plan_decode_color(gx_device * dev, gx_color_index color,
                  gx_color_value prgb[3])
{
    uint bitspercolor = dev->color_info.depth / 3;
    uint colormask = (1 << bitspercolor) - 1;
    uint max_rgb = dev->color_info.max_color;

    prgb[0] = ((color >> (bitspercolor * 2)) & colormask) *
        (ulong) gx_max_color_value / max_rgb;
    prgb[1] = ((color >> bitspercolor) & colormask) *
        (ulong) gx_max_color_value / max_rgb;
    prgb[2] = (color & colormask) *
        (ulong) gx_max_color_value / max_rgb;
    return 0;
}

/* Map a cmyk color tuple back to an gs color. */
static int
planc_decode_color(gx_device * dev, gx_color_index color,
                   gx_color_value prgb[4])
{
    uint bitspercolor = dev->color_info.depth / 4;
    uint colormask = (1 << bitspercolor) - 1;
    uint c, m, y, k;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / colormask))

    k = color & colormask;
    color >>= bitspercolor;
    y = color & colormask;
    color >>= bitspercolor;
    m = color & colormask;
    c = color >> bitspercolor;
    prgb[0] = cvalue(c);
    prgb[1] = cvalue(m);
    prgb[2] = cvalue(y);
    prgb[3] = cvalue(k);
    return 0;
}

/* Map a cmyk color back to an rgb tuple. */
static int
planc_map_color_rgb(gx_device * dev, gx_color_index color,
                    gx_color_value prgb[3])
{
    uint bitspercolor = dev->color_info.depth / 4;
    uint colormask = (1 << bitspercolor) - 1;
    uint c, m, y, k;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / colormask))

    k = color & colormask;
    color >>= bitspercolor;
    y = color & colormask;
    color >>= bitspercolor;
    m = color & colormask;
    c = color >> bitspercolor;
    k = colormask - k;
    prgb[0] = cvalue((colormask - c) * k / colormask);
    prgb[1] = cvalue((colormask - m) * k / colormask);
    prgb[2] = cvalue((colormask - y) * k / colormask);
    return 0;
}

/* Map CMYK to color. */
static gx_color_index
planc_encode_color(gx_device * dev, const gx_color_value cv[])
{
    int bpc = dev->color_info.depth / 4;
    gx_color_index color;
    COLROUND_VARS;

    COLROUND_SETUP(bpc);
    color = ((((((COLROUND_ROUND(cv[0]) << bpc) +
                 COLROUND_ROUND(cv[1])) << bpc) +
               COLROUND_ROUND(cv[2])) << bpc) +
             COLROUND_ROUND(cv[3]));

    return (color == gx_no_color_index ? color ^ 1 : color);
}

/* ------ Internal routines ------ */

/* Print a page using a given row printing routine. */
static int
plan_print_page_loop(gx_device_printer * pdev, int log2bits, int numComps,
                     gp_file *pstream)
{
    int lnum;
    int code = 0;
    gs_get_bits_options_t options;
#ifdef DEBUG_DUMP
    dump_row row_proc = NULL;
    int output_is_nul = !strncmp(pdev->fname, "nul:", min(strlen(pdev->fname), 4)) ||
        !strncmp(pdev->fname, "/dev/null", min(strlen(pdev->fname), 9));

    if (!output_is_nul)
        row_proc = dump_start(pdev->width, pdev->height, numComps, log2bits, pstream);
#endif
    options = GB_ALIGN_ANY |
              GB_RETURN_POINTER |
              GB_OFFSET_0 |
              GB_RASTER_STANDARD |
              GB_COLORS_NATIVE |
              GB_ALPHA_NONE;
    if (numComps == 1)
        options |= GB_PACKING_CHUNKY;
    else
        options |= GB_PACKING_PLANAR;
    for (lnum = 0; lnum < pdev->height; lnum++) {
        gs_int_rect *unread, rect;
        gs_get_bits_params_t params;

        rect.p.x = 0;
        rect.p.y = lnum;
        rect.q.x = pdev->width;
        rect.q.y = lnum+1;
        memset(&params, 0, sizeof(params));
        params.options = options;
        params.x_offset = 0;
        code = (*dev_proc(pdev, get_bits_rectangle))((gx_device *)pdev, &rect, &params,&unread);
        if (code < 0)
            break;
#ifdef DEBUG_DUMP
        if (row_proc)
            (*row_proc)(pdev->width, params.data, pstream);
#endif
    }
    return (code < 0 ? code : 0);
}

/* ------ Individual page printing routines ------ */

/* Print a monobit page. */
static int
planm_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "planm_print_page\n");
#endif
    return plan_print_page_loop(pdev, 0, 1, pstream);
}

/* Print a gray-mapped page. */
static int
plang_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plang_print_page\n");
#endif
    return plan_print_page_loop(pdev, 3, 1, pstream);
}

/* Print a color-mapped page. */
static int
plan_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "planc_print_page\n");
#endif
    return plan_print_page_loop(pdev, 3, 3, pstream);
}

/* Print a 1 bit CMYK page. */
static int
plank_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plank_print_page\n");
#endif
    return plan_print_page_loop(pdev, 0, 4, pstream);
}

/* Print an 8bpc CMYK page. */
static int
planc_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "planc_print_page\n");
#endif
    return plan_print_page_loop(pdev, 3, 4, pstream);
}

/* Print a color-mapped page (rgb, 1 bpc). */
static int
planr_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "planr_print_page\n");
#endif
    return plan_print_page_loop(pdev, 0, 3, pstream);
}
