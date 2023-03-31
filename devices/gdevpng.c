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


/* PNG (Portable Network Graphics) Format.  Pronounced "ping". */
/* lpd 1999-09-24: changes PNG_NO_STDIO to PNG_NO_CONSOLE_IO for libpng
   versions 1.0.3 and later. */
/* lpd 1999-07-01: replaced remaining uses of gs_malloc and gs_free with
   gs_alloc_bytes and gs_free_object. */
/* lpd 1999-03-08: changed png.h to png_.h to allow compiling with only
   headers in /usr/include, no source code. */
/* lpd 1997-07-20: changed from using gs_malloc/png_xxx_int to png_create_xxx
 * for allocating structures, and from gs_free to png_write_destroy for
 * freeing them. */
/* lpd 1997-5-7: added PNG_LIBPNG_VER conditional for operand types of
 * dummy png_push_fill_buffer. */
/* lpd 1997-4-13: Added PNG_NO_STDIO to remove library access to stderr. */
/* lpd 1997-3-14: Added resolution (pHYs) to output. */
/* lpd 1996-6-24: Added #ifdef for compatibility with old libpng versions. */
/* lpd 1996-6-11: Edited to remove unnecessary color mapping code. */
/* lpd (L. Peter Deutsch) 1996-4-7: Modified for libpng 0.88. */
/* Original version by Russell Lang 1995-07-04 */

/* RJW: Include png header BEFORE the gs ones to avoid warnings. */
/*
 * libpng versions 1.0.3 and later allow disabling access to the stdxxx
 * files while retaining support for FILE * I/O.
 */
#define PNG_NO_CONSOLE_IO
/*
 * Earlier libpng versions require disabling FILE * I/O altogether.
 * This produces a compiler warning about no prototype for png_init_io.
 * The right thing will happen at link time, since the library itself
 * is compiled with stdio support.  Unfortunately, we can't do this
 * conditionally depending on PNG_LIBPNG_VER, because this is defined
 * in png.h.
 */
/*#define PNG_NO_STDIO*/
#include "png_.h"

#include "gdevprn.h"
#include "gdevmem.h"
#include "gdevpccm.h"
#include "gscdefs.h"
#include "gxdownscale.h"
#include "gxdevsop.h"
#include "gscms.h"

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

static dev_proc_print_page(png_print_page);
static dev_proc_print_page(png_print_page_monod);
static dev_proc_open_device(pngalpha_open);
static dev_proc_encode_color(pngalpha_encode_color);
static dev_proc_decode_color(pngalpha_decode_color);
static dev_proc_copy_alpha(pngalpha_copy_alpha);
static dev_proc_fillpage(pngalpha_fillpage);
static dev_proc_put_image(pngalpha_put_image);
static dev_proc_get_params(pngalpha_get_params);
static dev_proc_put_params(pngalpha_put_params);
static dev_proc_create_buf_device(pngalpha_create_buf_device);
static dev_proc_get_params(png_get_params_downscale);
static dev_proc_put_params(png_put_params_downscale);
static dev_proc_get_params(png_get_params_downscale_mfs);
static dev_proc_put_params(png_put_params_downscale_mfs);
static dev_proc_dev_spec_op(pngalpha_spec_op);

typedef struct gx_device_png_s gx_device_png;
struct gx_device_png_s {
    gx_device_common;
    gx_prn_device_common;
    gx_downscaler_params downscale;
};

/* Monochrome. */

const gx_device_png gs_pngmono_device =
{ /* The print_page proc is compatible with allowing bg printing */
  prn_device_body(gx_device_png, gdev_prn_initialize_device_procs_mono_bg, "pngmono",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           0, 0, 0, 0,		/* margins */
           1, 1, 1, 1, 2, 2, png_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};


/* 4-bit planar (EGA/VGA-style) color. */

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
png16_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, map_rgb_color, pc_4bit_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, pc_4bit_map_color_rgb);
    set_dev_proc(dev, encode_color, pc_4bit_map_rgb_color);
    set_dev_proc(dev, decode_color, pc_4bit_map_color_rgb);
}

const gx_device_png gs_png16_device = {
  prn_device_body(gx_device_png, png16_initialize_device_procs, "png16",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           0, 0, 0, 0,		/* margins */
           3, 4, 1, 1, 2, 2, png_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* 8-bit (SuperVGA-style) color. */
/* (Uses a fixed palette of 3,3,2 bits.) */

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
png256_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, map_rgb_color, pc_8bit_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, pc_8bit_map_color_rgb);
    set_dev_proc(dev, encode_color, pc_8bit_map_rgb_color);
    set_dev_proc(dev, decode_color, pc_8bit_map_color_rgb);
}

const gx_device_png gs_png256_device = {
  prn_device_body(gx_device_png, png256_initialize_device_procs, "png256",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           0, 0, 0, 0,		/* margins */
           3, 8, 5, 5, 6, 6, png_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* 8-bit gray */

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
pnggray_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray_bg(dev);

    set_dev_proc(dev, get_params, png_get_params_downscale);
    set_dev_proc(dev, put_params, png_put_params_downscale);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_png gs_pnggray_device =
{prn_device_body(gx_device_png, pnggray_initialize_device_procs, "pnggray",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 1, 8, 255, 0, 256, 0, png_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* Monochrome (with error diffusion) */

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
pngmonod_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray_bg(dev);

    set_dev_proc(dev, get_params, png_get_params_downscale_mfs);
    set_dev_proc(dev, put_params, png_put_params_downscale_mfs);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_png gs_pngmonod_device =
{prn_device_body(gx_device_png, pngmonod_initialize_device_procs, "pngmonod",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 1, 8, 255, 0, 256, 0, png_print_page_monod),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* 24-bit color. */

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
png16m_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb_bg(dev);

    set_dev_proc(dev, get_params, png_get_params_downscale);
    set_dev_proc(dev, put_params, png_put_params_downscale);

    /* The prn macros used in previous versions of the code leave
     * encode_color and decode_color set to NULL (which are then rewritten
     * by the system to the default. For compatibility we do the same. */
    set_dev_proc(dev, encode_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, gx_default_rgb_map_color_rgb);
}

const gx_device_png gs_png16m_device =
{prn_device_body(gx_device_png, png16m_initialize_device_procs, "png16m",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 3, 24, 255, 255, 256, 256, png_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* 48 bit color. */

static void
png48_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb_bg(dev);

    /* The prn macros used in previous versions of the code leave
     * encode_color and decode_color set to NULL (which are then rewritten
     * by the system to the default. For compatibility we do the same. */
    set_dev_proc(dev, encode_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, gx_default_rgb_map_color_rgb);
}

/* Since the print_page doesn't alter the device, this device can print in the background */
const gx_device_png gs_png48_device =
{prn_device_body(gx_device_png, png48_initialize_device_procs, "png48",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 3, 48, 0, 65535, 1, 65536, png_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* 32-bit RGBA */
/* pngalpha device is 32-bit RGBA, with the alpha channel
 * indicating pixel coverage, not true transparency.
 * Anti-aliasing is enabled by default.
 * An erasepage will erase to transparent, not white.
 * It is intended to be used for creating web graphics with
 * a transparent background.
 */
typedef struct gx_device_pngalpha_s gx_device_pngalpha;
struct gx_device_pngalpha_s {
    gx_device_common;
    gx_prn_device_common;
    gx_downscaler_params downscale;
    int background;
};

static void
pngalpha_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, open_device, pngalpha_open);
    set_dev_proc(dev, map_rgb_color, pngalpha_encode_color);
    set_dev_proc(dev, map_color_rgb, pngalpha_decode_color);
    set_dev_proc(dev, encode_color, pngalpha_encode_color);
    set_dev_proc(dev, decode_color, pngalpha_decode_color);
    set_dev_proc(dev, get_params, pngalpha_get_params);
    set_dev_proc(dev, put_params, pngalpha_put_params);
    set_dev_proc(dev, copy_alpha, pngalpha_copy_alpha);
    set_dev_proc(dev, get_color_mapping_procs, gx_default_DevRGB_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, gx_default_DevRGB_get_color_comp_index);
    set_dev_proc(dev, fillpage, pngalpha_fillpage);
    set_dev_proc(dev, put_image, pngalpha_put_image);
    set_dev_proc(dev, dev_spec_op, pngalpha_spec_op);
}

const gx_device_pngalpha gs_pngalpha_device = {
        std_device_part1_(gx_device_pngalpha,
                pngalpha_initialize_device_procs, "pngalpha",
                &st_device_printer, open_init_closed),
        /* color_info */
        {3 /* max components */,
         3 /* number components */,
         GX_CINFO_POLARITY_ADDITIVE /* polarity */,
         32 /* depth */,
         -1 /* gray index */,
         255 /* max gray */,
         255 /* max color */,
         256 /* dither grays */,
         256 /* dither colors */,
         { 4, 4 } /* antialias info text, graphics */,
         GX_CINFO_UNKNOWN_SEP_LIN /* separable_and_linear */,
         { 0 } /* component shift */,
         { 0 } /* component bits */,
         { 0 } /* component mask */,
         "DeviceRGB" /* process color name */,
         GX_CINFO_OPMSUPPORTED_UNKNOWN /* opmsupported */,
         0 /* process_cmps */,
         0 /* icc_locations */
        },
        std_device_part2_(
          (int)((float)(DEFAULT_WIDTH_10THS) * (X_DPI) / 10 + 0.5),
          (int)((float)(DEFAULT_HEIGHT_10THS) * (Y_DPI) / 10 + 0.5),
           X_DPI, Y_DPI),
        offset_margin_values(0, 0, 0, 0, 0, 0),
        std_device_part3_(),
        prn_device_body_rest_(png_print_page),
        GX_DOWNSCALER_PARAMS_DEFAULTS,
        0xffffff	/* white background */
};

const gx_device_pngalpha gs_png16malpha_device = {
        std_device_part1_(gx_device_pngalpha,
                pngalpha_initialize_device_procs, "png16malpha",
                &st_device_printer, open_init_closed),
        /* color_info */
        {3 /* max components */,
         3 /* number components */,
         GX_CINFO_POLARITY_ADDITIVE /* polarity */,
         32 /* depth */,
         -1 /* gray index */,
         255 /* max gray */,
         255 /* max color */,
         256 /* dither grays */,
         256 /* dither colors */,
         { 1, 1 } /* antialias info text, graphics */,
         GX_CINFO_UNKNOWN_SEP_LIN /* separable_and_linear */,
         { 0 } /* component shift */,
         { 0 } /* component bits */,
         { 0 } /* component mask */,
         "DeviceRGB" /* process color name */,
         GX_CINFO_OPMSUPPORTED_UNKNOWN /* opmsupported */,
         0 /* process_cmps */,
         0 /* icc_locations */
        },
        std_device_part2_(
          (int)((float)(DEFAULT_WIDTH_10THS) * (X_DPI) / 10 + 0.5),
          (int)((float)(DEFAULT_HEIGHT_10THS) * (Y_DPI) / 10 + 0.5),
           X_DPI, Y_DPI),
        offset_margin_values(0, 0, 0, 0, 0, 0),
        std_device_part3_(),
        prn_device_body_rest_(png_print_page),
        GX_DOWNSCALER_PARAMS_DEFAULTS,
        0xffffff	/* white background */
};

/* ------ Private definitions ------ */

static int
png_get_params_downscale(gx_device * dev, gs_param_list * plist)
{
    gx_device_png *pdev = (gx_device_png *)dev;
    int code, ecode;

    ecode = 0;
    if ((code = gx_downscaler_write_params(plist, &pdev->downscale, 0)) < 0)
        ecode = code;

    code = gdev_prn_get_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

static int
png_put_params_downscale(gx_device *dev, gs_param_list *plist)
{
    gx_device_png *pdev = (gx_device_png *)dev;
    int code, ecode;

    ecode = gx_downscaler_read_params(plist, &pdev->downscale, 0);

    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

static int
png_get_params_downscale_mfs(gx_device *dev, gs_param_list *plist)
{
    gx_device_png *pdev = (gx_device_png *)dev;
    int code, ecode;

    ecode = gx_downscaler_write_params(plist, &pdev->downscale,
                                      GX_DOWNSCALER_PARAMS_MFS);

    code = gdev_prn_get_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

static int
png_put_params_downscale_mfs(gx_device *dev, gs_param_list *plist)
{
    gx_device_png *pdev = (gx_device_png *)dev;
    int code, ecode;

    ecode = gx_downscaler_read_params(plist, &pdev->downscale,
                                      GX_DOWNSCALER_PARAMS_MFS);

    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

#define PNG_MEM_ALIGN 16
static png_voidp
gdevpng_malloc(png_structp png, png_size_t size)
{
    gs_memory_t *mem = png_get_mem_ptr(png);
    uchar *unaligned;
    uchar *aligned;

    if (size == 0)
        return NULL;
    unaligned = gs_alloc_bytes(mem, size + PNG_MEM_ALIGN, "libpng");
    if (unaligned == NULL)
        return NULL;

    aligned = (uchar *)((intptr_t)(unaligned + PNG_MEM_ALIGN) & ~(PNG_MEM_ALIGN - 1));
    aligned[-1] = (uchar)(aligned - unaligned);

    return aligned;
}

static void
gdevpng_free(png_structp png, png_voidp ptr)
{
    gs_memory_t *mem = png_get_mem_ptr(png);
    uchar *aligned = ptr;
    if (aligned == NULL)
        return;
    gs_free_object(mem, aligned - aligned[-1], "libpng");
}

static void
my_png_write(png_struct *png, png_bytep buf, png_size_t size)
{
    gp_file *file = png_get_io_ptr(png);

    (void)gp_fwrite(buf, 1, size, file);
}

static void
my_png_flush(png_struct *png)
{
    gp_file *file = png_get_io_ptr(png);

    (void)gp_fflush(file);
}

/* Write out a page in PNG format. */
/* This routine is used for all formats. */
static int
do_png_print_page(gx_device_png * pdev, gp_file * file, bool monod)
{
    gs_memory_t *mem = pdev->memory;
    int raster = gdev_prn_raster(pdev);
    gx_downscaler_t ds;

    /* PNG structures */
    byte *row = gs_alloc_bytes(mem, raster, "png raster buffer");
    png_struct *png_ptr =
        png_create_write_struct_2(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL, pdev->memory, gdevpng_malloc, gdevpng_free);
    png_info *info_ptr = png_create_info_struct(png_ptr);
    int depth = pdev->color_info.depth;
    int y;
    int code;			/* return code */
    char software_key[80];
    char software_text[256];
    png_text text_png;
    int dst_bpc, src_bpc;
    bool errdiff = 0;

    bool invert = false, endian_swap = false, bg_needed = false;
    png_byte bit_depth = 0;
    png_byte color_type = 0;
    png_uint_32 x_pixels_per_unit;
    png_uint_32 y_pixels_per_unit;
    png_byte phys_unit_type;
    png_color_16 background;
    png_uint_32 width, height;
#if PNG_LIBPNG_VER_MINOR >= 5
    png_color palette[256];
#endif
    png_color *palettep;
    png_uint_16 num_palette;
    png_uint_32 valid = 0;
    int upfactor, downfactor;

    /* Sanity check params */
    if (pdev->downscale.downscale_factor < 1)
        pdev->downscale.downscale_factor = 1;
    if (pdev->downscale.min_feature_size < 1)
        pdev->downscale.min_feature_size = 1;
    else if (pdev->downscale.min_feature_size > 2)
        pdev->downscale.min_feature_size = 2;

    /* Slightly nasty, but it saves us duplicating this entire routine. */
    if (monod) {
        errdiff = 1;
        depth = 1;
    }

    if (row == 0 || png_ptr == 0 || info_ptr == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    /* set error handling */
#if PNG_LIBPNG_VER_MINOR >= 5
    code = setjmp(png_jmpbuf(png_ptr));
#else
    code = setjmp(png_ptr->jmpbuf);
#endif
    if (code) {
        /* If we get here, we had a problem reading the file */
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    code = 0;			/* for normal path */
    /* set up the output control */
    png_set_write_fn(png_ptr, file, my_png_write, my_png_flush);

    /* set the file information here */
    gx_downscaler_decode_factor(pdev->downscale.downscale_factor,
                                &upfactor, &downfactor);
    /* resolution is in pixels per meter vs. dpi */
    x_pixels_per_unit =
        (png_uint_32) (pdev->HWResolution[0] * upfactor * (100.0 / 2.54) / downfactor + 0.5);
    y_pixels_per_unit =
        (png_uint_32) (pdev->HWResolution[1] * upfactor * (100.0 / 2.54) / downfactor + 0.5);

    phys_unit_type = PNG_RESOLUTION_METER;
    valid |= PNG_INFO_pHYs;

    switch (depth) {
        case 32:
            bit_depth = 8;
            color_type = PNG_COLOR_TYPE_RGB_ALPHA;
            invert = true;

            {   gx_device_pngalpha *ppdev = (gx_device_pngalpha *)pdev;
                background.index = 0;
                background.red =   (ppdev->background >> 16) & 0xff;
                background.green = (ppdev->background >> 8)  & 0xff;
                background.blue =  (ppdev->background)       & 0xff;
                background.gray = 0;
                bg_needed = true;
            }
            errdiff = 1;
            break;
        case 48:
            bit_depth = 16;
            color_type = PNG_COLOR_TYPE_RGB;
#if defined(ARCH_IS_BIG_ENDIAN) && (!ARCH_IS_BIG_ENDIAN)
            endian_swap = true;
#endif
            break;
        case 24:
            bit_depth = 8;
            color_type = PNG_COLOR_TYPE_RGB;
            errdiff = 1;
            break;
        case 8:
            bit_depth = 8;
            if (gx_device_has_color(pdev)) {
                color_type = PNG_COLOR_TYPE_PALETTE;
                errdiff = 0;
            } else {
                color_type = PNG_COLOR_TYPE_GRAY;
                errdiff = 1;
            }
            break;
        case 4:
            bit_depth = 4;
            color_type = PNG_COLOR_TYPE_PALETTE;
            break;
        case 1:
            bit_depth = 1;
            color_type = PNG_COLOR_TYPE_GRAY;
            /* invert monochrome pixels */
            if (!monod) {
                invert = true;
            }
            break;
    }

    /* set the palette if there is one */
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        int i;
        int num_colors = 1 << depth;
        gx_color_value rgb[3];

#if PNG_LIBPNG_VER_MINOR >= 5
        palettep = palette;
#else
        palettep =
            (void *)gs_alloc_bytes(mem, 256 * sizeof(png_color),
                                   "png palette");
        if (palettep == 0) {
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }
#endif
        num_palette = num_colors;
        valid |= PNG_INFO_PLTE;
        for (i = 0; i < num_colors; i++) {
            (*dev_proc(pdev, map_color_rgb)) ((gx_device *) pdev,
                                              (gx_color_index) i, rgb);
            palettep[i].red = gx_color_value_to_byte(rgb[0]);
            palettep[i].green = gx_color_value_to_byte(rgb[1]);
            palettep[i].blue = gx_color_value_to_byte(rgb[2]);
        }
    }
    else {
        palettep = NULL;
        num_palette = 0;
    }
    /* add comment */
#ifdef CLUSTER
    strncpy(software_key, "GPL Ghostscript", sizeof(software_key));
    strncpy(software_text, "GPL Ghostscript", sizeof(software_text));
#else
    strncpy(software_key, "Software", sizeof(software_key));
    {
        int major = (int)(gs_revision / 1000);
        int minor = (int)(gs_revision - (major * 1000)) / 10;
        int patch = gs_revision % 10;

        gs_snprintf(software_text, sizeof(software_text), "%s %d.%02d.%d", gs_product, major, minor, patch);
    }
#endif
    text_png.compression = -1;	/* uncompressed */
    text_png.key = software_key;
    text_png.text = software_text;
    text_png.text_length = strlen(software_text);

    dst_bpc = bit_depth;
    src_bpc = dst_bpc;
    if (errdiff)
        src_bpc = 8;
    else
        pdev->downscale.downscale_factor = upfactor = downfactor = 1;
    width = pdev->width * upfactor / downfactor;
    height = pdev->height * upfactor / downfactor;

#if PNG_LIBPNG_VER_MINOR >= 5
    png_set_pHYs(png_ptr, info_ptr,
                 x_pixels_per_unit, y_pixels_per_unit, phys_unit_type);

    png_set_IHDR(png_ptr, info_ptr,
                 width, height, bit_depth,
                 color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    if (palettep)
        png_set_PLTE(png_ptr, info_ptr, palettep, num_palette);

    png_set_text(png_ptr, info_ptr, &text_png, 1);

    if (pdev->icc_struct != NULL && pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] != NULL) {
        cmm_profile_t *icc_profile = pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
        if (icc_profile->hash_is_valid && icc_profile->hashcode == ARTIFEX_sRGB_HASH) {
            /* sRGB case. Just use the tag */
            png_set_sRGB(png_ptr, info_ptr, PNG_sRGB_INTENT_RELATIVE);
        } else {
            /* PNG can only be RGB or gray.  No CIELAB :(  */
            if (icc_profile->data_cs == gsRGB || icc_profile->data_cs == gsGRAY) {
                if (icc_profile->num_comps == pdev->color_info.num_components &&
                    !(pdev->icc_struct->usefastcolor)) {
                    png_set_iCCP(png_ptr, info_ptr, icc_profile->name,
                        PNG_COMPRESSION_TYPE_DEFAULT, icc_profile->buffer,
                        icc_profile->buffer_size);
                }
            }
        }
    }
#else
    info_ptr->bit_depth = bit_depth;
    info_ptr->color_type = color_type;
    info_ptr->width = width;
    info_ptr->height = height;
    info_ptr->x_pixels_per_unit = x_pixels_per_unit;
    info_ptr->y_pixels_per_unit = y_pixels_per_unit;
    info_ptr->phys_unit_type = phys_unit_type;
    info_ptr->palette = palettep;
    info_ptr->num_palette = num_palette;
    info_ptr->valid |= valid;
    info_ptr->text = &text_png;
    info_ptr->num_text = 1;
    /* Set up the ICC information */
    if (pdev->icc_struct != NULL && pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] != NULL) {
        cmm_profile_t *icc_profile = pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
        if (icc_profile->hash_is_valid && icc_profile->hashcode == ARTIFEX_sRGB_HASH) {
            /* sRGB case. Just use the tag */
            png_set_sRGB(png_ptr, info_ptr, PNG_sRGB_INTENT_RELATIVE);
        } else {
            /* PNG can only be RGB or gray.  No CIELAB :(  */
            if (icc_profile->data_cs == gsRGB || icc_profile->data_cs == gsGRAY) {
                if (icc_profile->num_comps == pdev->color_info.num_components &&
                    !(pdev->icc_struct->usefastcolor)) {
                    info_ptr->iccp_name = icc_profile->name;
                    info_ptr->iccp_profile = icc_profile->buffer;
                    info_ptr->iccp_proflen = icc_profile->buffer_size;
                    info_ptr->valid |= PNG_INFO_iCCP;
                }
            }
        }
    }
#endif
    if (invert) {
        if (depth == 32)
            png_set_invert_alpha(png_ptr);
        else
            png_set_invert_mono(png_ptr);
    }
    if (bg_needed) {
        png_set_bKGD(png_ptr, info_ptr, &background);
    }
#if defined(ARCH_IS_BIG_ENDIAN) && (!ARCH_IS_BIG_ENDIAN)
    if (endian_swap) {
        png_set_swap(png_ptr);
    }
#endif

    /* write the file information */
    png_write_info(png_ptr, info_ptr);

#if PNG_LIBPNG_VER_MINOR >= 5
#else
    /* don't write the comments twice */
    info_ptr->num_text = 0;
    info_ptr->text = NULL;
#endif

    /* For simplicity of code, we always go through the downscaler. For
     * non-supported depths, it will pass through with minimal performance
     * hit. So ensure that we only trigger downscales when we need them.
     */
    code = gx_downscaler_init(&ds, (gx_device *)pdev, src_bpc, dst_bpc,
                              depth/dst_bpc, &pdev->downscale, NULL, 0);
    if (code >= 0)
    {
#ifdef CLUSTER
        int bitlen = width*dst_bpc;
        int end = bitlen>>3;
        int mask = 255>>(bitlen&7);
        if (bitlen & 7)
            mask = ~mask;
        else
            end--;
#endif
        /* Write the contents of the image. */
        for (y = 0; y < height; y++) {
            gx_downscaler_getbits(&ds, row, y);
#ifdef CLUSTER
            row[end] &= mask;
#endif
            png_write_rows(png_ptr, &row, 1);
        }
        gx_downscaler_fin(&ds);
    }

    /* write the rest of the file */
    png_write_end(png_ptr, info_ptr);

#if PNG_LIBPNG_VER_MINOR >= 5
#else
    /* if you alloced the palette, free it here */
    gs_free_object(mem, palettep, "png palette");
#endif

  done:
    /* free the structures */
    png_destroy_write_struct(&png_ptr, &info_ptr);
    gs_free_object(mem, row, "png raster buffer");

    return code;
}

static int
png_print_page(gx_device_printer * pdev, gp_file * file)
{
    return do_png_print_page((gx_device_png *)pdev, file, 0);
}

static int
png_print_page_monod(gx_device_printer * pdev, gp_file * file)
{
    return do_png_print_page((gx_device_png *)pdev, file, 1);
}

#if PNG_LIBPNG_VER_MINOR < 5

/*
 * Patch around a static reference to a never-used procedure.
 * This could be avoided if we were willing to edit pngconf.h to
 *      #undef PNG_PROGRESSIVE_READ_SUPPORTED
 */
#ifdef PNG_PROGRESSIVE_READ_SUPPORTED
#  if PNG_LIBPNG_VER >= 95
#    define PPFB_LENGTH_T png_size_t
#  else
#    define PPFB_LENGTH_T png_uint_32
#  endif
void
png_push_fill_buffer(png_structp, png_bytep, PPFB_LENGTH_T);
void
png_push_fill_buffer(png_structp png_ptr, png_bytep buffer,
                     PPFB_LENGTH_T length)
{
}
#endif
#endif

static int
pngalpha_open(gx_device * pdev)
{
    gx_device_pngalpha *ppdev = (gx_device_pngalpha *)pdev;
    int code;
    /* We replace create_buf_device so we can replace copy_alpha
     * for memory device, but not clist. We also replace the fillpage
     * proc with our own to fill with transparent.
     */
    ppdev->printer_procs.buf_procs.create_buf_device =
        pngalpha_create_buf_device;
    code = gdev_prn_open(pdev);
    return code;
}

static int
pngalpha_create_buf_device(gx_device **pbdev, gx_device *target, int y,
   const gx_render_plane_t *render_plane, gs_memory_t *mem,
   gx_color_usage_t *color_usage)
{
    gx_device_printer *ptarget;
    int code = gx_default_create_buf_device(pbdev, target, y,
        render_plane, mem, color_usage);
    /* Now set copy_alpha to one that handles RGBA */

    /* this is really pretty nasty. The pngalpha device is going to replace
     * the device methods in the memory rendering device with some of its own.
     * To me this seems fraught with peril, its making a lot of assumptions
     * about the compatibility of the devices!
     * This, of course, totally breaks device chaining, but since the memory
     * device wasn't going to pass on the intermediate method calls to the
     * 'terminating' device, we can work around it here. We simply descend
     * the chain of devices to the terminating device, and pull the methods
     * we need directly from that device. I don't know why we are using
     * 'orig_procs' either, but its safe to do so because this is only
     * done here for the PNG device, and we know that this is a gx_device_prn
     * based device.
     */
    while (target->child != NULL)
        target = target->child;

    ptarget= (gx_device_printer *)target;
    set_dev_proc(*pbdev, copy_alpha, ptarget->orig_procs.copy_alpha);
    set_dev_proc(*pbdev, dev_spec_op, ptarget->orig_procs.dev_spec_op);
    set_dev_proc(*pbdev, fillpage, pngalpha_fillpage);
    return code;
}

static int
pngalpha_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_pngalpha *ppdev = (gx_device_pngalpha *)pdev;
    int background;
    int code, ecode;

    /* BackgroundColor in format 16#RRGGBB is used for bKGD chunk */
    switch(code = param_read_int(plist, "BackgroundColor", &background)) {
        case 0:
            ppdev->background = background & 0xffffff;
            break;
        case 1:		/* not found */
            code = 0;
            break;
        default:
            param_signal_error(plist, "BackgroundColor", code);
            break;
    }

    if ((ecode = gx_downscaler_read_params(plist, &ppdev->downscale, 0)) < 0)
        code = ecode;

    if (code == 0) {
        code = gdev_prn_put_params(pdev, plist);
    }

    return code;
}

/* Get device parameters */
static int
pngalpha_get_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_pngalpha *ppdev = (gx_device_pngalpha *)pdev;
    int code = gdev_prn_get_params(pdev, plist);
    int ecode;
    if (code >= 0)
        code = param_write_int(plist, "BackgroundColor",
                                &(ppdev->background));
    ecode = 0;
    if ((ecode = gx_downscaler_write_params(plist, &ppdev->downscale, 0)) < 0)
        code = ecode;

    return code;
}

/* RGB mapping for 32-bit RGBA color devices */

static gx_color_index
pngalpha_encode_color(gx_device * dev, const gx_color_value cv[])
{
    /* low 7 are alpha, stored inverted to avoid white/opaque
     * being 0xffffffff which is also gx_no_color_index.
     * So 0xff is transparent and 0x00 is opaque.
     * We always return opaque colors (bits 0-7 = 0).
     * Return value is 0xRRGGBB00.
     */
    return
        ((uint) gx_color_value_to_byte(cv[2]) << 8) +
        ((ulong) gx_color_value_to_byte(cv[1]) << 16) +
        ((ulong) gx_color_value_to_byte(cv[0]) << 24);
}

/* Map a color index to a r-g-b color. */
static int
pngalpha_decode_color(gx_device * dev, gx_color_index color,
                             gx_color_value prgb[])
{
    prgb[0] = gx_color_value_from_byte((color >> 24) & 0xff);
    prgb[1] = gx_color_value_from_byte((color >> 16) & 0xff);
    prgb[2] = gx_color_value_from_byte((color >> 8)  & 0xff);
    return 0;
}

/* fill the page fills with transparent */
static int
pngalpha_fillpage(gx_device *dev, gs_gstate * pgs, gx_device_color *pdevc)
{
    return (*dev_proc(dev, fill_rectangle))(dev, 0, 0, dev->width, dev->height,  0xffffffff);
}

/* Handle the RGBA planes from the PDF 1.4 compositor */
static int
pngalpha_put_image (gx_device *pdev, gx_device *mdev, const byte **buffers, int num_chan, int xstart,
              int ystart, int width, int height, int row_stride,
              int alpha_plane_index, int tag_plane_index)
{
    gx_device_memory *pmemdev = (gx_device_memory *)mdev;
    byte *buffer_prn;
    int yend = ystart + height;
    int xend = xstart + width;
    int x, y;
    int src_position, des_position;

    if (num_chan != 3 || alpha_plane_index <= 0)
            return_error(gs_error_unknownerror);        /* can't handle these cases */

    /* Now we need to convert the 4 channels (RGBA) planar into what   */
    /* the do_png_print_page expects -- chunky inverted data. For that */
    /* we need to find the underlying gx_device_memory buffer for the  */
    /* data (similar to bit_put_image, and borrwed from there).        */
    /* Drill down to get the appropriate memory buffer pointer */
    buffer_prn = pmemdev->base;
    /* Now go ahead and process the planes into chunky as the memory device needs */
    for ( y = ystart; y < yend; y++ ) {
        src_position = (y - ystart) * row_stride;
        des_position = y * pmemdev->raster + xstart * 4;
        for ( x = xstart; x < xend; x++ ) {
            buffer_prn[des_position++] =  buffers[0][src_position];
            buffer_prn[des_position++] =  buffers[1][src_position];
            buffer_prn[des_position++] =  buffers[2][src_position];
            /* Alpha data in low bits. Note that Alpha is inverted. */
            buffer_prn[des_position++] = (255 - buffers[alpha_plane_index][src_position]);
            src_position += 1;
        }
    }
    return height;        /* we used all of the data */
}

/* Implementation for 32-bit RGBA in a memory buffer */
/* Derived from gx_default_copy_alpha, but now maintains alpha channel. */
static int
pngalpha_copy_alpha(gx_device * dev, const byte * data, int data_x,
           int raster, gx_bitmap_id id, int x, int y, int width, int height,
                      gx_color_index color, int depth)
{				/* This might be called with depth = 1.... */
    if (depth == 1)
        return (*dev_proc(dev, copy_mono)) (dev, data, data_x, raster, id,
                                            x, y, width, height,
                                            gx_no_color_index, color);
    /*
     * Simulate alpha by weighted averaging of RGB values.
     * This is very slow, but functionally correct.
     */
    {
        const byte *row;
        gs_memory_t *mem = dev->memory;
        int bpp = dev->color_info.depth;
        int ncomps = dev->color_info.num_components;
        uint in_size = gx_device_raster(dev, false);
        byte *lin;
        uint out_size;
        byte *lout;
        int code = 0;
        gx_color_value color_cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
        int ry;
        gs_int_rect rect;
        gs_get_bits_params_t params;

        fit_copy(dev, data, data_x, raster, id, x, y, width, height);
        row = data;
        out_size = bitmap_raster(width * bpp);
        lin = gs_alloc_bytes(mem, in_size, "copy_alpha(lin)");
        lout = gs_alloc_bytes(mem, out_size, "copy_alpha(lout)");
        if (lin == 0 || lout == 0) {
            code = gs_note_error(gs_error_VMerror);
            goto out;
        }
        (*dev_proc(dev, decode_color)) (dev, color, color_cv);
        rect.p.x = 0;
        rect.q.x = dev->width;
        params.x_offset = 0;
        params.raster = bitmap_raster(dev->width * dev->color_info.depth);
        for (ry = y; ry < y + height; row += raster, ++ry) {
            byte *line;
            int sx, rx;

            byte *l_dptr = lout;
            int l_dbit = 0;
            byte l_dbyte = ((l_dbit) ? (byte)(*(l_dptr) & (0xff00 >> (l_dbit))) : 0);
            int l_xprev = x;

            rect.p.y = ry;
            rect.q.y = ry+1;

            params.options = (GB_ALIGN_ANY |
                              (GB_RETURN_COPY | GB_RETURN_POINTER) |
                              GB_OFFSET_0 |
                              GB_RASTER_STANDARD | GB_PACKING_CHUNKY |
                              GB_COLORS_NATIVE | GB_ALPHA_NONE);
            params.data[0] = lin;
            code = (*dev_proc(dev, get_bits_rectangle))(dev, &rect, &params);
            if (code < 0)
                break;
            line = params.data[0];
            for (sx = data_x, rx = x; sx < data_x + width; ++sx, ++rx) {
                gx_color_index previous = gx_no_color_index;
                gx_color_index composite;
                uint32_t alpha2, alpha;

                switch(depth)
                {
                case 2:	/* map 0 - 3 to 0 - 255 */
                    alpha = ((row[sx >> 2] >> ((3 - (sx & 3)) << 1)) & 3) * 85;
                    break;
                case 4:
                    alpha2 = row[sx >> 1];
                    alpha = (sx & 1 ? alpha2 & 0xf : alpha2 >> 4) * 17;
                    break;
                case 8:
                    alpha = row[sx];
                    break;
                default:
		  return_error(gs_error_rangecheck);
                }
                if (alpha == 255) {	/* Just write the new color. */
                    composite = color;
                } else {
                    if (previous == gx_no_color_index) {	/* Extract the old color. */
                        const byte *src = line + (rx * (bpp >> 3));
                        previous = 0;
                        previous += (gx_color_index) * src++ << 24;
                        previous += (gx_color_index) * src++ << 16;
                        previous += (gx_color_index) * src++ << 8;
                        previous += *src++;
                    }
                    if (alpha == 0) {	/* Just write the old color. */
                        composite = previous;
                    } else {	/* Blend values. */
                        gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
                        int i;
                        uint32_t old_coverage;
                        uint32_t new_coverage;

                        (*dev_proc(dev, decode_color)) (dev, previous, cv);
                        /* decode color doesn't give us coverage */
                        cv[3] = previous & 0xff;
                        old_coverage = 255 - cv[3];
                        new_coverage =
                            (255 * alpha + old_coverage * (255 - alpha)) / 255;
                        for (i=0; i<ncomps; i++)
                            cv[i] = min(((255 * alpha * color_cv[i]) +
                                         (old_coverage * (255 - alpha ) * cv[i]))
                                        / (new_coverage * 255), gx_max_color_value);
                        composite =
                            (*dev_proc(dev, encode_color)) (dev, cv);
                        /* encode color doesn't include coverage */
                        composite |= (255 - new_coverage) & 0xff;

                        /* composite can never be gx_no_color_index
                         * because pixel is never completely transparent
                         * (low byte != 0xff).
                         */
                    }
                }
                if (sizeof(composite) > 4) {
                    if (sample_store_next64(composite, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                        return_error(gs_error_rangecheck);
                }
                else {
                    if (sample_store_next32(composite, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                        return_error(gs_error_rangecheck);
                }
            }
            if ( rx > l_xprev ) {
                sample_store_flush(l_dptr, l_dbit, l_dbyte);
                code = (*dev_proc(dev, copy_color))
                  (dev, lout, l_xprev - x, raster,
                   gx_no_bitmap_id, l_xprev, ry, rx - l_xprev, 1);
                if (code < 0)
                    return code;
            }
        }
      out:gs_free_object(mem, lout, "copy_alpha(lout)");
        gs_free_object(mem, lin, "copy_alpha(lin)");
        return code;
    }
}

static int
pngalpha_spec_op(gx_device* pdev, int dso, void* ptr, int size)
{
    switch (dso)
    {
    case gxdso_supports_alpha:
        return 1;
    }

    return gdev_prn_dev_spec_op(pdev, dso, ptr, size);
}
