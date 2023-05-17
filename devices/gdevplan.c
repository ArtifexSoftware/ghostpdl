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
#include "gxdownscale.h"
#include "gsicc_cache.h"
#include "gxdevsop.h"

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

typedef struct {
    gx_device_common;
    gx_prn_device_common;
    gx_downscaler_params downscale;
    gsicc_link_t *icclink;
} gx_device_plan;

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
static dev_proc_dev_spec_op(plan_spec_op);

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

static int
plan_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_plan *pdev = (gx_device_plan *)dev;
    int code = gdev_prn_get_params(dev, plist);

    if (code < 0)
        return code;

    return gx_downscaler_write_params(plist, &pdev->downscale, 0);
}

static int
plan_put_params(gx_device *dev, gs_param_list * plist)
{
    gx_device_plan *pdev = (gx_device_plan *)dev;
    int code;

    code = gx_downscaler_read_params(plist, &pdev->downscale, 0);
    if (code < 0)
        return code;

    return gdev_prn_put_params(dev, plist);
}

static void
plan_base_initialize_device_procs(gx_device *dev,
                     dev_proc_map_color_rgb(map_color_rgb),
                     dev_proc_encode_color(encode_color),
                     dev_proc_decode_color(decode_color))
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, open_device, plan_open);
    set_dev_proc(dev, close_device, plan_close);
    set_dev_proc(dev, map_color_rgb, map_color_rgb);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
    set_dev_proc(dev, encode_color, encode_color);
    set_dev_proc(dev, decode_color, decode_color);
    set_dev_proc(dev, get_params, plan_get_params);
    set_dev_proc(dev, put_params, plan_put_params);
}

static void
planm_initialize_device_procs(gx_device *dev)
{
    plan_base_initialize_device_procs(dev,
                                      gdev_prn_map_color_rgb,
                                      gdev_prn_map_rgb_color,
                                      gdev_prn_map_color_rgb);
}

static void
plang_initialize_device_procs(gx_device *dev)
{
    plan_base_initialize_device_procs(dev,
                                      plang_decode_color,
                                      plang_encode_color,
                                      plang_decode_color);
    set_dev_proc(dev, dev_spec_op, plan_spec_op);
}

static void
plan_initialize_device_procs(gx_device *dev)
{
    plan_base_initialize_device_procs(dev,
                                      plan_decode_color,
                                      gx_default_rgb_map_rgb_color,
                                      plan_decode_color);
    set_dev_proc(dev, dev_spec_op, plan_spec_op);
}

static void
planc_initialize_device_procs(gx_device *dev)
{
    plan_base_initialize_device_procs(dev,
                                      planc_map_color_rgb,
                                      planc_encode_color,
                                      planc_decode_color);
    set_dev_proc(dev, dev_spec_op, plan_spec_op);
}

static void
plank_initialize_device_procs(gx_device *dev)
{
    plan_base_initialize_device_procs(dev,
                                      planc_map_color_rgb,
                                      planc_encode_color,
                                      planc_decode_color);
}

static void
planr_initialize_device_procs(gx_device *dev)
{
    plan_base_initialize_device_procs(dev,
                                      plan_decode_color,
                                      gx_default_rgb_map_rgb_color,
                                      plan_decode_color);
}

/* Macro for generating device descriptors. */
#define plan_prn_device(init, dev_name, num_comp, depth, max_gray, max_rgb, print_page) \
{       prn_device_body(gx_device_plan, init, dev_name,\
        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, X_DPI, Y_DPI,\
        0, 0, 0, 0,\
        num_comp, depth, max_gray, max_rgb, max_gray + 1, max_rgb + 1,\
        print_page),\
        GX_DOWNSCALER_PARAMS_DEFAULTS\
}

/* The device descriptors themselves */
const gx_device_plan gs_plan_device =
  plan_prn_device(plan_initialize_device_procs, "plan",
                  3, 24, 255, 255, plan_print_page);
const gx_device_plan gs_plang_device =
  plan_prn_device(plang_initialize_device_procs, "plang",
                  1, 8, 255, 0, plang_print_page);
const gx_device_plan gs_planm_device =
  plan_prn_device(planm_initialize_device_procs, "planm",
                  1, 1, 1, 0, planm_print_page);
const gx_device_plan gs_plank_device =
  plan_prn_device(plank_initialize_device_procs, "plank",
                  4, 4, 1, 1, plank_print_page);
const gx_device_plan gs_planc_device =
  plan_prn_device(planc_initialize_device_procs, "planc",
                  4, 32, 255, 255, planc_print_page);
const gx_device_plan gs_planr_device =
  plan_prn_device(planr_initialize_device_procs, "planr",
                  3, 3, 1, 1, planr_print_page);

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
    code = gdev_prn_open_planar(pdev, pdev->color_info.num_components);
    if (code < 0)
        return code;
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    set_linear_color_bits_mask_shift(pdev);

    return code;
}

static int
plan_close(gx_device *pdev)
{
    gx_device_plan *dev = (gx_device_plan *)pdev;

#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plan_close\n");
#endif

    gsicc_free_link_dev(dev->icclink);
    dev->icclink = NULL;
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
                   gx_color_value prgb[])
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
                  gx_color_value prgb[])
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
                   gx_color_value prgb[])
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

static int
plan_spec_op(gx_device *dev, int op, void *data, int datasize)
{
    if (op == gxdso_supports_iccpostrender) {
        int bpc = dev->color_info.depth / dev->color_info.num_components;
        return bpc == 8;
    }
    if (op == gxdso_skip_icc_component_validation) {
        int bpc = dev->color_info.depth / dev->color_info.num_components;
        return bpc == 8;
    }
    return gdev_prn_dev_spec_op(dev, op, data, datasize);
}

/* ------ Internal routines ------ */

static int post_cm(void  *arg,
                   byte **dst,
                   byte **src,
                   int    w,
                   int    h,
                   int    raster)
{
    gsicc_bufferdesc_t input_buffer_desc, output_buffer_desc;
    gsicc_link_t *icclink = (gsicc_link_t*)arg;

    gsicc_init_buffer(&input_buffer_desc, icclink->num_input, 1, false,
        false, true, src[1] - src[0], raster, h, w);
    gsicc_init_buffer(&output_buffer_desc, icclink->num_output, 1, false,
        false, true, dst[1] - dst[0], raster, h, w);
    icclink->procs.map_buffer(NULL, icclink, &input_buffer_desc, &output_buffer_desc,
        src[0], dst[0]);
    return 0;
}

/* Print a page using a given row printing routine. */
static int
plan_print_page_loop(gx_device_printer *pdev, int log2bits, int numComps,
                     gp_file *pstream)
{
    gx_device_plan *dev = (gx_device_plan *)pdev;
    int i, lnum;
    int code = 0;
    gs_get_bits_options_t options;
    gx_downscaler_t ds;
    gs_get_bits_params_t params;
    int post_cm_comps;
    byte *planes[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int factor = dev->downscale.downscale_factor;
    int width = gx_downscaler_scale(pdev->width, factor);
    int height = gx_downscaler_scale(pdev->height, factor);
    int raster_plane = bitmap_raster(width << log2bits);

#ifdef DEBUG_DUMP
    dump_row row_proc = NULL;
    int output_is_nul = !strncmp(pdev->fname, "nul:", min(strlen(pdev->fname), 4)) ||
        !strncmp(pdev->fname, "/dev/null", min(strlen(pdev->fname), 9));
#endif

    if (gdev_prn_file_is_new(pdev)) {
        code = gx_downscaler_create_post_render_link((gx_device *)pdev,
                                                     &dev->icclink);
        if (code < 0)
            return code;
    }

    options = GB_ALIGN_ANY |
              GB_RETURN_POINTER |
              GB_RETURN_COPY |
              GB_OFFSET_0 |
              GB_RASTER_STANDARD |
              GB_COLORS_NATIVE |
              GB_ALPHA_NONE;
    if (numComps == 1)
        options |= GB_PACKING_CHUNKY;
    else
        options |= GB_PACKING_PLANAR;

    memset(&params, 0, sizeof(params));
    params.options = options;
    params.x_offset = 0;
    post_cm_comps = dev->icclink ? dev->icclink->num_output : numComps;

    planes[0] = gs_alloc_bytes(pdev->memory, raster_plane * post_cm_comps,
                               "plan_print_page_loop");
    if (planes[0] == NULL)
        return_error(gs_error_VMerror);
    for (i = 1; i < post_cm_comps; i++) {
        planes[i] = planes[i-1] + raster_plane;
        params.data[i] = planes[i];
    }

    code = gx_downscaler_init_planar_cm(&ds,
                                        (gx_device *)dev,
                                        1<<log2bits, /* src_bpc */
                                        1<<log2bits, /* dst_bpc */
                                        numComps,
                                        &dev->downscale,
                                        &params,
                                        dev->icclink ? post_cm : NULL,
                                        dev->icclink,
                                        post_cm_comps);
    if (code < 0)
        goto fail;

#ifdef DEBUG_DUMP
    if (!output_is_nul)
        row_proc = dump_start(width, height, post_cm_comps, log2bits, pstream);
#endif

    for (lnum = 0; lnum < height; lnum++) {
        for (i = 0; i < post_cm_comps; i++)
            params.data[i] = planes[i];
        code = gx_downscaler_get_bits_rectangle(&ds, &params, lnum);
        if (code < 0)
            break;
#ifdef DEBUG_DUMP
        if (row_proc)
            (*row_proc)(width, params.data, pstream);
#endif
    }

    gx_downscaler_fin(&ds);
fail:
    gs_free_object(pdev->memory, planes[0], "plan_print_page_loop");
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
