/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* tiffgray device:  8-bit Gray uncompressed TIFF device
 * tiff32nc device:  32-bit CMYK uncompressed TIFF device
 * tiffsep device:   Generate individual TIFF gray files for each separation
 *                   as well as a 'composite' 32-bit CMYK for the page.
 * tiffsep1 device:  Generate individual TIFF 1-bit files for each separation
 * tiffscaled device:Mono TIFF device (error-diffused downscaled output from
 *                   8-bit Gray internal rendering)
 * tiffscaled8 device:Greyscale TIFF device (downscaled output from
 *                   8-bit Gray internal rendering)
 * tiffscaled24 device:24-bit RGB TIFF device (dithered downscaled output
 *                   from 24-bit RGB internal rendering)
 */

#include "stdint_.h"   /* for tiff.h */
#include "gdevtifs.h"
#include "gdevprn.h"
#include "gdevdevn.h"
#include "gsequivc.h"
#include "gxdht.h"
#include "gxiodev.h"
#include "stdio_.h"
#include "ctype_.h"
#include "gxgetbit.h"
#include "gdevppla.h"
#include "gxdownscale.h"

/*
 * Some of the code in this module is based upon the gdevtfnx.c module.
 * gdevtfnx.c has the following message:
 * Thanks to Alan Barclay <alan@escribe.co.uk> for donating the original
 * version of this code to Ghostscript.
 */

/* ------ The device descriptors ------ */

/* Default X and Y resolution */
#define X_DPI 72
#define Y_DPI 72

/* ------ The tiffgray device ------ */

static dev_proc_print_page(tiffgray_print_page);

static const gx_device_procs tiffgray_procs =
prn_color_params_procs(tiff_open, tiff_output_page, tiff_close,
                gx_default_gray_map_rgb_color, gx_default_gray_map_color_rgb,
                tiff_get_params, tiff_put_params);

const gx_device_tiff gs_tiffgray_device = {
    prn_device_body(gx_device_tiff, tiffgray_procs, "tiffgray",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    X_DPI, Y_DPI,
                    0, 0, 0, 0, /* Margins */
                    1, 8, 255, 0, 256, 0, tiffgray_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    TIFF_DEFAULT_DOWNSCALE,
    0, /* Adjust size */
    1  /* MinFeatureSize */
};

/* ------ The tiffscaled device ------ */

static dev_proc_print_page(tiffscaled_print_page);

static const gx_device_procs tiffscaled_procs =
prn_color_params_procs(tiff_open,
                       tiff_output_page,
                       tiff_close,
                       gx_default_gray_map_rgb_color,
                       gx_default_gray_map_color_rgb,
                       tiff_get_params_downscale,
                       tiff_put_params_downscale);

const gx_device_tiff gs_tiffscaled_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled_procs,
                    "tiffscaled",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    1,          /* num components */
                    8,          /* bits per sample */
                    255, 0, 256, 0,
                    tiffscaled_print_page),
    arch_is_big_endian,/* default to native endian (i.e. use big endian iff the platform is so */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    TIFF_DEFAULT_DOWNSCALE,
    0, /* Adjust size */
    1  /* MinFeatureSize */
};

/* ------ The tiffscaled8 device ------ */

static dev_proc_print_page(tiffscaled8_print_page);

static const gx_device_procs tiffscaled8_procs =
prn_color_params_procs(tiff_open,
                       tiff_output_page,
                       tiff_close,
                       gx_default_gray_map_rgb_color,
                       gx_default_gray_map_color_rgb,
                       tiff_get_params_downscale,
                       tiff_put_params_downscale);

const gx_device_tiff gs_tiffscaled8_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled8_procs,
                    "tiffscaled8",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    1,          /* num components */
                    8,          /* bits per sample */
                    255, 0, 256, 0,
                    tiffscaled8_print_page),
    arch_is_big_endian,/* default to native endian (i.e. use big endian iff the platform is so */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    TIFF_DEFAULT_DOWNSCALE,
    0, /* Adjust size */
    1  /* MinFeatureSize */
};

/* ------ The tiffscaled24 device ------ */

static dev_proc_print_page(tiffscaled24_print_page);

static const gx_device_procs tiffscaled24_procs =
prn_color_params_procs(tiff_open,
                       tiff_output_page,
                       tiff_close,
                       gx_default_rgb_map_rgb_color,
                       gx_default_rgb_map_color_rgb,
                       tiff_get_params_downscale,
                       tiff_put_params_downscale);

const gx_device_tiff gs_tiffscaled24_device = {
    prn_device_body(gx_device_tiff,
                    tiffscaled24_procs,
                    "tiffscaled24",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    3,          /* num components */
                    24,         /* bits per sample */
                    255, 255, 256, 256,
                    tiffscaled24_print_page),
    arch_is_big_endian,/* default to native endian (i.e. use big endian iff the platform is so */
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    TIFF_DEFAULT_DOWNSCALE,
    0, /* Adjust size */
    1  /* MinFeatureSize */
};


/* ------ Private functions ------ */

static void
tiff_set_gray_fields(gx_device_printer *pdev, TIFF *tif,
                     unsigned short bits_per_sample,
                     int compression,
                     long max_strip_size)
{
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);

    tiff_set_compression(pdev, tif, compression, max_strip_size);
}

static int
tiffgray_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    if (tfdev->Compression==COMPRESSION_NONE &&
        pdev->height > ((unsigned long) 0xFFFFFFFF - ftell(file))/(pdev->width)) /* note width is never 0 in print_page */
        return_error(gs_error_rangecheck);  /* this will overflow 32 bits */

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    tiff_set_gray_fields(pdev, tfdev->tif, 8, tfdev->Compression, tfdev->MaxStripSize);

    return tiff_print_page(pdev, tfdev->tif, 0);
}

static int
tiffscaled_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    tiff_set_gray_fields(pdev, tfdev->tif, 1, tfdev->Compression, tfdev->MaxStripSize);

    return tiff_downscale_and_print_page(pdev, tfdev->tif,
                                         tfdev->DownScaleFactor,
                                         tfdev->MinFeatureSize,
                                         tfdev->AdjustWidth,
                                         1, 1);
}

static int
tiffscaled8_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    tiff_set_gray_fields(pdev, tfdev->tif, 8, tfdev->Compression, tfdev->MaxStripSize);

    return tiff_downscale_and_print_page(pdev, tfdev->tif,
                                         tfdev->DownScaleFactor,
                                         tfdev->MinFeatureSize,
                                         tfdev->AdjustWidth,
                                         8, 1);
}

static void
tiff_set_rgb_fields(gx_device_tiff *tfdev)
{
    /* Put in a switch statement in case we want to have others */
    switch (tfdev->icc_struct->device_profile[0]->data_cs) {
        case gsRGB:
            TIFFSetField(tfdev->tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            break;
        case gsCIELAB:
            TIFFSetField(tfdev->tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ICCLAB);
            break;
        default:
            TIFFSetField(tfdev->tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            break;
    }
    TIFFSetField(tfdev->tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tfdev->tif, TIFFTAG_SAMPLESPERPIXEL, 3);

    tiff_set_compression((gx_device_printer *)tfdev, tfdev->tif,
                         tfdev->Compression, tfdev->MaxStripSize);
}


static int
tiffscaled24_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    TIFFSetField(tfdev->tif, TIFFTAG_BITSPERSAMPLE, 8);
    tiff_set_rgb_fields(tfdev);

    return tiff_downscale_and_print_page(pdev, tfdev->tif,
                                         tfdev->DownScaleFactor,
                                         tfdev->MinFeatureSize,
                                         tfdev->AdjustWidth,
                                         8, 3);
}

/* ------ The cmyk devices ------ */

static dev_proc_print_page(tiffcmyk_print_page);

#define cmyk_procs(p_map_color_rgb, p_map_cmyk_color)\
    tiff_open, NULL, NULL, tiff_output_page, tiff_close,\
    NULL, p_map_color_rgb, NULL, NULL, NULL, NULL, NULL, NULL,\
    tiff_get_params, tiff_put_params,\
    p_map_cmyk_color, NULL, NULL, NULL, gx_page_device_get_page_device

/* 8-bit-per-plane separated CMYK color. */

static const gx_device_procs tiffcmyk_procs = {
    cmyk_procs(cmyk_8bit_map_color_cmyk, cmyk_8bit_map_cmyk_color)
};

const gx_device_tiff gs_tiff32nc_device = {
    prn_device_body(gx_device_tiff, tiffcmyk_procs, "tiff32nc",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    X_DPI, Y_DPI,
                    0, 0, 0, 0, /* Margins */
                    4, 32, 255, 255, 256, 256, tiffcmyk_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    TIFF_DEFAULT_DOWNSCALE,
    0, /* Adjust size */
    1  /* MinFeatureSize */
};

/* 16-bit-per-plane separated CMYK color. */

static const gx_device_procs tiff64nc_procs = {
    cmyk_procs(cmyk_16bit_map_color_cmyk, cmyk_16bit_map_cmyk_color)
};

const gx_device_tiff gs_tiff64nc_device = {
    prn_device_body(gx_device_tiff, tiff64nc_procs, "tiff64nc",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    X_DPI, Y_DPI,
                    0, 0, 0, 0, /* Margins */
                    4, 64, 255, 255, 256, 256, tiffcmyk_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE,
    TIFF_DEFAULT_DOWNSCALE,
    0, /* Adjust size */
    1  /* MinFeatureSize */
};

/* ------ Private functions ------ */

static void
tiff_set_cmyk_fields(gx_device_printer *pdev, TIFF *tif,
                     short bits_per_sample,
                     uint16 compression,
                     long max_strip_size)
{
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
    TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);

    tiff_set_compression(pdev, tif, compression, max_strip_size);
}

static int
tiffcmyk_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code;

    if (tfdev->Compression==COMPRESSION_NONE &&
        pdev->height > ((unsigned long) 0xFFFFFFFF - ftell(file))/(pdev->width)) /* note width is never 0 in print_page */
        return_error(gs_error_rangecheck);  /* this will overflow 32 bits */

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
        return code;

    tiff_set_cmyk_fields(pdev,
                         tfdev->tif,
                         pdev->color_info.depth / pdev->color_info.num_components,
                         tfdev->Compression,
                         tfdev->MaxStripSize);

    return tiff_print_page(pdev, tfdev->tif, 0);
}

/* ----------  The tiffsep device ------------ */

#define NUM_CMYK_COMPONENTS 4
#define MAX_FILE_NAME_SIZE gp_file_name_sizeof
#define MAX_COLOR_VALUE 255             /* We are using 8 bits per colorant */

/* The device descriptor */
static dev_proc_open_device(tiffsep_prn_open);
static dev_proc_close_device(tiffsep_prn_close);
static dev_proc_get_params(tiffsep_get_params);
static dev_proc_put_params(tiffsep_put_params);
static dev_proc_print_page(tiffsep_print_page);
static dev_proc_get_color_mapping_procs(tiffsep_get_color_mapping_procs);
static dev_proc_get_color_comp_index(tiffsep_get_color_comp_index);
static dev_proc_encode_color(tiffsep_encode_color);
static dev_proc_decode_color(tiffsep_decode_color);
static dev_proc_update_spot_equivalent_colors(tiffsep_update_spot_equivalent_colors);
static dev_proc_ret_devn_params(tiffsep_ret_devn_params);
static dev_proc_open_device(tiffsep1_prn_open);
static dev_proc_close_device(tiffsep1_prn_close);
static dev_proc_put_params(tiffsep1_put_params);
static dev_proc_print_page(tiffsep1_print_page);
static dev_proc_fill_path(sep1_fill_path);

/* common to tiffsep and tiffsepo1 */
static dev_proc_print_page_copies(tiffseps_print_page_copies);
static dev_proc_output_page(tiffseps_output_page);

#define tiffsep_devices_common\
    gx_device_common;\
    gx_prn_device_common;\
        /* tiff state for separation files */\
    FILE *sep_file[GX_DEVICE_COLOR_MAX_COMPONENTS];\
    TIFF *tiff[GX_DEVICE_COLOR_MAX_COMPONENTS]; \
    bool  BigEndian;            /* true = big endian; false = little endian */\
    uint16 Compression;         /* for the separation files, same values as
                                   TIFFTAG_COMPRESSION */\
    bool close_files; \
    long MaxStripSize;\
    long DownScaleFactor;\
    long MinFeatureSize;\
    long BitsPerComponent;\
    int max_spots;\
    gs_devn_params devn_params;         /* DeviceN generated parameters */\
    equivalent_cmyk_color_params equiv_cmyk_colors

/*
 * A structure definition for a DeviceN type device
 */
typedef struct tiffsep_device_s {
    tiffsep_devices_common;
    FILE *comp_file;            /* Underlying file for tiff_comp */
    TIFF *tiff_comp;            /* tiff file for comp file */
    bool warning_given;
} tiffsep_device;

/* threshold array structure */
typedef struct threshold_array_s {
      int dheight, dwidth;
      byte *dstart;
} threshold_array_t;

typedef struct tiffsep1_device_s {
    tiffsep_devices_common;
    bool warning_given;
    threshold_array_t thresholds[GX_DEVICE_COLOR_MAX_COMPONENTS + 1]; /* one extra for Default */
    dev_t_proc_fill_path((*fill_path), gx_device); /* we forward to here */
} tiffsep1_device;

/* GC procedures */
static
ENUM_PTRS_WITH(tiffsep_device_enum_ptrs, tiffsep_device *pdev)
{
    if (index == 0)
        ENUM_RETURN(pdev->devn_params.compressed_color_list);
    index--;
    if (index == 0)
        ENUM_RETURN(pdev->devn_params.pdf14_compressed_color_list);
    index--;
    if (index < pdev->devn_params.separations.num_separations)
        ENUM_RETURN(pdev->devn_params.separations.names[index].data);
    ENUM_PREFIX(st_device_printer,
                    pdev->devn_params.separations.num_separations);
    return 0;
}
ENUM_PTRS_END

static RELOC_PTRS_WITH(tiffsep_device_reloc_ptrs, tiffsep_device *pdev)
{
    RELOC_PREFIX(st_device_printer);
    {
        int i;

        for (i = 0; i < pdev->devn_params.separations.num_separations; ++i) {
            RELOC_PTR(tiffsep_device, devn_params.separations.names[i].data);
        }
    }
    RELOC_PTR(tiffsep_device, devn_params.compressed_color_list);
    RELOC_PTR(tiffsep_device, devn_params.pdf14_compressed_color_list);
}
RELOC_PTRS_END

/* Even though tiffsep_device_finalize is the same as gx_device_finalize, */
/* we need to implement it separately because st_composite_final */
/* declares all 3 procedures as private. */
static void
tiffsep_device_finalize(const gs_memory_t *cmem, void *vpdev)
{
    tiffsep_device * const pdevn = (tiffsep_device *) vpdev;

    /* for safety */
    pdevn->close_files = true;

    /* We need to deallocate the compressed_color_list.
       and the names. */
    devn_free_params((gx_device*) vpdev);
    gx_device_finalize(cmem, vpdev);
}

gs_private_st_composite_final(st_tiffsep_device, tiffsep_device,
    "tiffsep_device", tiffsep_device_enum_ptrs, tiffsep_device_reloc_ptrs,
    tiffsep_device_finalize);

/*
 * Macro definition for tiffsep device procedures
 */
#define sep_device_procs(open, close, encode_color, decode_color, update_spot_colors,put_params, fill_path) \
{       open,\
        gx_default_get_initial_matrix,\
        NULL,                           /* sync_output */\
        tiffseps_output_page,               /* output_page */\
        close,                          /* close */\
        NULL,                           /* map_rgb_color - not used */\
        tiffsep_decode_color,           /* map_color_rgb */\
        NULL,                           /* fill_rectangle */\
        NULL,                           /* tile_rectangle */\
        NULL,                           /* copy_mono */\
        NULL,                           /* copy_color */\
        NULL,                           /* draw_line */\
        NULL,                           /* get_bits */\
        tiffsep_get_params,             /* get_params */\
        put_params,                     /* put_params */\
        NULL,                           /* map_cmyk_color - not used */\
        NULL,                           /* get_xfont_procs */\
        NULL,                           /* get_xfont_device */\
        NULL,                           /* map_rgb_alpha_color */\
        gx_page_device_get_page_device, /* get_page_device */\
        NULL,                           /* get_alpha_bits */\
        NULL,                           /* copy_alpha */\
        NULL,                           /* get_band */\
        NULL,                           /* copy_rop */\
        fill_path,                      /* fill_path */\
        NULL,                           /* stroke_path */\
        NULL,                           /* fill_mask */\
        NULL,                           /* fill_trapezoid */\
        NULL,                           /* fill_parallelogram */\
        NULL,                           /* fill_triangle */\
        NULL,                           /* draw_thin_line */\
        NULL,                           /* begin_image */\
        NULL,                           /* image_data */\
        NULL,                           /* end_image */\
        NULL,                           /* strip_tile_rectangle */\
        NULL,                           /* strip_copy_rop */\
        NULL,                           /* get_clipping_box */\
        NULL,                           /* begin_typed_image */\
        NULL,                           /* get_bits_rectangle */\
        NULL,                           /* map_color_rgb_alpha */\
        NULL,                           /* create_compositor */\
        NULL,                           /* get_hardware_params */\
        NULL,                           /* text_begin */\
        NULL,                           /* finish_copydevice */\
        NULL,                           /* begin_transparency_group */\
        NULL,                           /* end_transparency_group */\
        NULL,                           /* begin_transparency_mask */\
        NULL,                           /* end_transparency_mask */\
        NULL,                           /* discard_transparency_layer */\
        tiffsep_get_color_mapping_procs,/* get_color_mapping_procs */\
        tiffsep_get_color_comp_index,   /* get_color_comp_index */\
        encode_color,                   /* encode_color */\
        decode_color,                   /* decode_color */\
        NULL,                           /* pattern_manage */\
        NULL,                           /* fill_rectangle_hl_color */\
        NULL,                           /* include_color_space */\
        NULL,                           /* fill_linear_color_scanline */\
        NULL,                           /* fill_linear_color_trapezoid */\
        NULL,                           /* fill_linear_color_triangle */\
        update_spot_colors,             /* update_spot_equivalent_colors */\
        tiffsep_ret_devn_params         /* ret_devn_params */\
}


#define tiffsep_devices_body(dtype, procs, dname, ncomp, pol, depth, mg, mc, sl, cn, print_page, print_page_copies, compr)\
    std_device_full_body_type_extended(dtype, &procs, dname,\
          &st_tiffsep_device,\
          (int)((long)(DEFAULT_WIDTH_10THS) * (X_DPI) / 10),\
          (int)((long)(DEFAULT_HEIGHT_10THS) * (Y_DPI) / 10),\
          X_DPI, Y_DPI,\
          ncomp,                /* MaxComponents */\
          ncomp,                /* NumComp */\
          pol,                  /* Polarity */\
          depth, 0,             /* Depth, GrayIndex */\
          mg, mc,               /* MaxGray, MaxColor */\
          mg + 1, mc + 1,       /* DitherGray, DitherColor */\
          sl,                   /* Linear & Separable? */\
          cn,                   /* Process color model name */\
          0, 0,                 /* offsets */\
          0, 0, 0, 0            /* margins */\
        ),\
        prn_device_body_rest2_(print_page, print_page_copies, -1),\
        { 0 },                  /* tiff state for separation files */\
        { 0 },                  /* separation files */\
        arch_is_big_endian      /* true = big endian; false = little endian */,\
        compr                   /* COMPRESSION_* */,\
        true,                   /* close_files */ \
        TIFF_DEFAULT_STRIP_SIZE,/* MaxStripSize */\
        1,                      /* DownScaleFactor */\
        0,                      /* MinFeatureSize */\
        8,                      /* BitsPerComponent */\
        GS_SOFT_MAX_SPOTS	/* max_spots */

#define GCIB (ARCH_SIZEOF_GX_COLOR_INDEX * 8)

/*
 * TIFF devices with CMYK process color model and spot color support.
 */
static const gx_device_procs spot_cmyk_procs =
                sep_device_procs(tiffsep_prn_open, tiffsep_prn_close, tiffsep_encode_color, tiffsep_decode_color,
                                tiffsep_update_spot_equivalent_colors, tiffsep_put_params, NULL);

static const gx_device_procs spot1_cmyk_procs =
                sep_device_procs(tiffsep1_prn_open, tiffsep1_prn_close, tiffsep_encode_color, tiffsep_decode_color,
                                tiffsep_update_spot_equivalent_colors, tiffsep1_put_params, sep1_fill_path);

const tiffsep_device gs_tiffsep_device =
{
    tiffsep_devices_body(tiffsep_device, spot_cmyk_procs, "tiffsep", ARCH_SIZEOF_GX_COLOR_INDEX, GX_CINFO_POLARITY_SUBTRACTIVE, GCIB, MAX_COLOR_VALUE, MAX_COLOR_VALUE, GX_CINFO_SEP_LIN, "DeviceCMYK", tiffsep_print_page, tiffseps_print_page_copies, COMPRESSION_LZW),
    /* devn_params specific parameters */
    { 8,                        /* Not used - Bits per color */
      DeviceCMYKComponents,     /* Names of color model colorants */
      4,                        /* Number colorants for CMYK */
      0,                        /* MaxSeparations has not been specified */
      -1,                       /* PageSpotColors has not been specified */
      {0},                      /* SeparationNames */
      0,                        /* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 } /* Initial component SeparationOrder */
    },
    { true },                   /* equivalent CMYK colors for spot colors */
};

const tiffsep1_device gs_tiffsep1_device =
{
    tiffsep_devices_body(tiffsep1_device, spot1_cmyk_procs, "tiffsep1", ARCH_SIZEOF_GX_COLOR_INDEX, GX_CINFO_POLARITY_SUBTRACTIVE, GCIB, MAX_COLOR_VALUE, MAX_COLOR_VALUE, GX_CINFO_SEP_LIN, "DeviceCMYK", tiffsep1_print_page, tiffseps_print_page_copies, COMPRESSION_CCITTFAX4),
    /* devn_params specific parameters */
    { 8,                        /* Not used - Bits per color */
      DeviceCMYKComponents,     /* Names of color model colorants */
      4,                        /* Number colorants for CMYK */
      0,                        /* MaxSeparations has not been specified */
      -1,                       /* PageSpotColors has not been specified */
      {0},                      /* SeparationNames */
      0,                        /* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 } /* Initial component SeparationOrder */
    },
    { true },                   /* equivalent CMYK colors for spot colors */
    false,			/* warning_given */
    { {0} },                    /* threshold arrays */
    0,                          /* fill_path */
};

#undef NC
#undef SL
#undef ENCODE_COLOR
#undef DECODE_COLOR

static const uint32_t bit_order[32]={
#if arch_is_big_endian
        0x80000000, 0x40000000, 0x20000000, 0x10000000, 0x08000000, 0x04000000, 0x02000000, 0x01000000,
        0x00800000, 0x00400000, 0x00200000, 0x00100000, 0x00080000, 0x00040000, 0x00020000, 0x00010000,
        0x00008000, 0x00004000, 0x00002000, 0x00001000, 0x00000800, 0x00000400, 0x00000200, 0x00000100,
        0x00000080, 0x00000040, 0x00000020, 0x00000010, 0x00000008, 0x00000004, 0x00000002, 0x00000001
#else
        0x00000080, 0x00000040, 0x00000020, 0x00000010, 0x00000008, 0x00000004, 0x00000002, 0x00000001,
        0x00008000, 0x00004000, 0x00002000, 0x00001000, 0x00000800, 0x00000400, 0x00000200, 0x00000100,
        0x00800000, 0x00400000, 0x00200000, 0x00100000, 0x00080000, 0x00040000, 0x00020000, 0x00010000,
        0x80000000, 0x40000000, 0x20000000, 0x10000000, 0x08000000, 0x04000000, 0x02000000, 0x01000000
#endif
    };

/*
 * The following procedures are used to map the standard color spaces into
 * the color components for the tiffsep device.
 */
static void
tiffsep_gray_cs_to_cm(gx_device * dev, frac gray, frac out[])
{
    int * map = ((tiffsep_device *) dev)->devn_params.separation_order_map;

    gray_cs_to_devn_cm(dev, map, gray, out);
}

static void
tiffsep_rgb_cs_to_cm(gx_device * dev, const gs_imager_state *pis,
                                   frac r, frac g, frac b, frac out[])
{
    int * map = ((tiffsep_device *) dev)->devn_params.separation_order_map;

    rgb_cs_to_devn_cm(dev, map, pis, r, g, b, out);
}

static void
tiffsep_cmyk_cs_to_cm(gx_device * dev,
                frac c, frac m, frac y, frac k, frac out[])
{
    const gs_devn_params *devn = tiffsep_ret_devn_params(dev);
    const int *map = devn->separation_order_map;
    int j;

    if (devn->num_separation_order_names > 0) {
        /* This is to set only those that we are using */
        for (j = 0; j < devn->num_separation_order_names; j++) {
            switch (map[j]) {
                case 0 :
                    out[0] = c;
                    break;
                case 1:
                    out[1] = m;
                    break;
                case 2:
                    out[2] = y;
                    break;
                case 3:
                    out[3] = k;
                    break;
                default:
                    break;
            }
        }
    } else {
        cmyk_cs_to_devn_cm(dev, map, c, m, y, k, out);
    }
}

static const gx_cm_color_map_procs tiffsep_cm_procs = {
    tiffsep_gray_cs_to_cm,
    tiffsep_rgb_cs_to_cm,
    tiffsep_cmyk_cs_to_cm
};

/*
 * These are the handlers for returning the list of color space
 * to color model conversion routines.
 */
static const gx_cm_color_map_procs *
tiffsep_get_color_mapping_procs(const gx_device * dev)
{
    return &tiffsep_cm_procs;
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
 * With 32 bit gx_color_index values, we simply pack values.
 */
static gx_color_index
tiffsep_encode_color(gx_device *dev, const gx_color_value colors[])
{
    int bpc = ((tiffsep_device *)dev)->devn_params.bitspercomponent;
    gx_color_index color = 0;
    int i = 0;
    int ncomp = dev->color_info.num_components;
    COLROUND_VARS;

    COLROUND_SETUP(bpc);
    for (; i < ncomp; i++) {
        color <<= bpc;
        color |= COLROUND_ROUND(colors[i]);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 * With 32 bit gx_color_index values, we simply pack values.
 */
static int
tiffsep_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    int bpc = ((tiffsep_device *)dev)->devn_params.bitspercomponent;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    int mask = (1 << bpc) - 1;
    int i = 0;
    int ncomp = dev->color_info.num_components;

    for (; i < ncomp; i++) {
        out[ncomp - i - 1] = (gx_color_value) ((color & mask) << drop);
        color >>= bpc;
    }
    return 0;
}

/*
 *  Device proc for updating the equivalent CMYK color for spot colors.
 */
static int
tiffsep_update_spot_equivalent_colors(gx_device * dev, const gs_state * pgs)
{
    tiffsep_device * pdev = (tiffsep_device *)dev;

    update_spot_equivalent_cmyk_colors(dev, pgs,
                    &pdev->devn_params, &pdev->equiv_cmyk_colors);
    return 0;
}

/*
 *  Device proc for returning a pointer to DeviceN parameter structure
 */
static gs_devn_params *
tiffsep_ret_devn_params(gx_device * dev)
{
    tiffsep_device * pdev = (tiffsep_device *)dev;

    return &pdev->devn_params;
}

/* Get parameters.  We provide a default CRD. */
static int
tiffsep_get_params(gx_device * pdev, gs_param_list * plist)
{
    tiffsep_device * const pdevn = (tiffsep_device *) pdev;
    int code = gdev_prn_get_params(pdev, plist);
    int ecode = code;
    gs_param_string comprstr;

    if (code < 0)
        return code;

    code = devn_get_params(pdev, plist,
                &(((tiffsep_device *)pdev)->devn_params),
                &(((tiffsep_device *)pdev)->equiv_cmyk_colors));
    if (code < 0)
        return code;

    if ((code = param_write_bool(plist, "BigEndian", &pdevn->BigEndian)) < 0)
        ecode = code;
    if ((code = tiff_compression_param_string(&comprstr, pdevn->Compression)) < 0 ||
        (code = param_write_string(plist, "Compression", &comprstr)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "MaxStripSize", &pdevn->MaxStripSize)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "DownScaleFactor", &pdevn->DownScaleFactor)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "MinFeatureSize", &pdevn->MinFeatureSize)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "BitsPerComponent", &pdevn->BitsPerComponent)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "MaxSpots", &pdevn->max_spots)) < 0)
        ecode = code;

    return ecode;
}

/* Set parameters.  We allow setting the number of bits per component. */
static int
tiffsep_put_params(gx_device * pdev, gs_param_list * plist)
{
    tiffsep_device * const pdevn = (tiffsep_device *) pdev;
    int code;
    const char *param_name;
    gs_param_string comprstr;
    bool save_close_files = pdevn->close_files;
    long downscale = pdevn->DownScaleFactor;
    long mfs = pdevn->MinFeatureSize;
    long bpc = pdevn->BitsPerComponent;
    int max_spots = pdevn->max_spots;

    /* Read BigEndian option as bool */
    switch (code = param_read_bool(plist, (param_name = "BigEndian"), &pdevn->BigEndian)) {
        default:
            param_signal_error(plist, param_name, code);
            return code;
        case 0:
        case 1:
            break;
    }
    /* Read Compression */
    switch (code = param_read_string(plist, (param_name = "Compression"), &comprstr)) {
        case 0:
            if ((code = tiff_compression_id(&pdevn->Compression, &comprstr)) < 0 ||
                !tiff_compression_allowed(pdevn->Compression,
                                          pdevn->devn_params.bitspercomponent))
            {
                param_signal_error(plist, param_name, code);
                return code;
            }
            break;
        case 1:
            break;
        default:
            param_signal_error(plist, param_name, code);
            return code;
    }
    switch (code = param_read_long(plist, (param_name = "MaxStripSize"), &pdevn->MaxStripSize)) {
        case 0:
            /*
             * Strip must be large enough to accommodate a raster line.
             * If the max strip size is too small, we still write a single
             * line per strip rather than giving an error.
             */
            if (pdevn->MaxStripSize >= 0)
                break;
            code = gs_error_rangecheck;
        default:
            param_signal_error(plist, param_name, code);
            return code;
        case 1:
            break;
    }
    switch (code = param_read_long(plist,
                                   (param_name = "DownScaleFactor"),
                                   &downscale)) {
        case 0:
            if (downscale <= 0)
                downscale = 1;
            pdevn->DownScaleFactor = downscale;
            break;
        case 1:
            break;
        default:
            param_signal_error(plist, param_name, code);
            return code;
    }
    switch (code = param_read_long(plist, (param_name = "MinFeatureSize"), &mfs)) {
        case 0:
            if ((mfs >= 0) && (mfs <= 4)) {
                pdevn->MinFeatureSize = mfs;
                break;
            }
            code = gs_error_rangecheck;
        case 1:
            break;
        default:
            param_signal_error(plist, param_name, code);
            return code;
    }
    switch (code = param_read_long(plist, (param_name = "BitsPerComponent"), &bpc)) {
        case 0:
            if ((bpc == 1) || (bpc == 8)) {
                pdevn->BitsPerComponent = bpc;
                break;
            }
            code = gs_error_rangecheck;
        case 1:
            break;
        default:
            param_signal_error(plist, param_name, code);
            return code;
    }
    switch (code = param_read_int(plist, (param_name = "MaxSpots"), &max_spots)) {
        case 0:
            if ((max_spots >= 0) || (max_spots <= GS_CLIENT_COLOR_MAX_COMPONENTS-4)) {
                pdevn->max_spots = max_spots;
                break;
            }
            emprintf1(pdev->memory, "MaxSpots must be between 0 and %d\n",
                      GS_CLIENT_COLOR_MAX_COMPONENTS-4);
            code = gs_error_rangecheck;
        case 1:
            break;
        default:
            param_signal_error(plist, param_name, code);
            return code;
    }

    pdevn->close_files = false;

    code = devn_printer_put_params(pdev, plist,
                &(pdevn->devn_params), &(pdevn->equiv_cmyk_colors));

    pdevn->close_files = save_close_files;

    return(code);
}

static int
tiffsep1_put_params(gx_device * pdev, gs_param_list * plist)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *) pdev;
    int code;

    if ((code = tiffsep_put_params(pdev, plist)) < 0)
        return code;

    /* put_params may have changed the fill_path proc -- we need it set to ours */
    if (pdev->procs.fill_path != sep1_fill_path) {
        tfdev->fill_path = pdev->procs.fill_path;
        pdev->procs.fill_path = sep1_fill_path;
    }
    return code;

}

/* common print_page_copies method for both tiff separation devices */
static int
tiffseps_print_page_copies(gx_device_printer * pdev, FILE * prn_stream, int num_copies)
{
    int i = 1;
    int code = 0;
    (void) prn_stream;

    for (; i < num_copies; ++i) {
        code = (*pdev->printer_procs.print_page) (pdev, NULL);
        if (code < 0)
            return code;

        pdev->PageCount++;

    }
    /* Print the last (or only) page. */
    pdev->PageCount -= num_copies - 1;
    return (*pdev->printer_procs.print_page) (pdev, NULL);
}

int
tiffseps_output_page(gx_device *pdev, int num_copies, int flush)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int outcode = 0, closecode = 0, endcode;

    if (num_copies > 0 || !flush) {
        /* Print the accumulated page description. */
        outcode = (*ppdev->printer_procs.print_page_copies)(ppdev, ppdev->file, num_copies);
    }
    endcode = (ppdev->buffer_space && !ppdev->is_async_renderer ?
               clist_finish_page(pdev, flush) : 0);

    if (outcode < 0)
        return (outcode);
    if (closecode < 0)
        return (closecode);
    if (endcode < 0)
        return (endcode);

    endcode = gx_finish_output_page(pdev, num_copies, flush);
    return (endcode);
}

static void build_comp_to_sep_map(tiffsep_device *, short *);
static int number_output_separations(int, int, int, int);
static int create_separation_file_name(tiffsep_device *, char *, uint, int, bool);
static byte * threshold_from_order( gx_ht_order *, int *, int *, gs_memory_t *);
static int sep1_ht_order_to_thresholds(gx_device *pdev, const gs_imager_state *pis);
static void sep1_free_thresholds(tiffsep1_device *);
dev_proc_fill_path(clist_fill_path);

/* Open the tiffsep1 device.  This will now be using planar buffers so that
   we are not limited to 64 bit chunky */
int
tiffsep1_prn_open(gx_device * pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    tiffsep1_device *pdev_sep = (tiffsep1_device *) pdev;
    int code, k;

    /* With planar the depth can be more than 64.  Update the color
       info to reflect the proper depth and number of planes */
    pdev_sep->warning_given = false;
    if (pdev_sep->devn_params.page_spot_colors >= 0) {
        pdev->color_info.num_components =
            (pdev_sep->devn_params.page_spot_colors
                                 + pdev_sep->devn_params.num_std_colorant_names);
        if (pdev->color_info.num_components > pdev->color_info.max_components)
            pdev->color_info.num_components = pdev->color_info.max_components;
    } else {
        /* We do not know how many spots may occur on the page.
           For this reason we go ahead and allocate the maximum that we
           have available.  Note, lack of knowledge only occurs in the case
           of PS files.  With PDF we know a priori the number of spot
           colorants.  */
        int num_comp = pdev_sep->max_spots + 4; /* Spots + CMYK */
        if (num_comp > GS_CLIENT_COLOR_MAX_COMPONENTS)
            num_comp = GS_CLIENT_COLOR_MAX_COMPONENTS;
        pdev->color_info.num_components = num_comp;
        pdev->color_info.max_components = num_comp;
    }
    /* Push this to the max amount as a default if someone has not set it */
    if (pdev_sep->devn_params.num_separation_order_names == 0)
        for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
            pdev_sep->devn_params.separation_order_map[k] = k;
        }
    pdev->color_info.depth = pdev->color_info.num_components *
                             pdev_sep->devn_params.bitspercomponent;
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    code = gdev_prn_open_planar(pdev, true);
    ppdev->file = NULL;
    pdev->icc_struct->supports_devn = true;

    /* gdev_prn_open_planae may have changed the fill_path proc -- we need it set to ours */
    if (pdev->procs.fill_path != sep1_fill_path) {
        pdev_sep->fill_path = pdev->procs.fill_path;
        pdev->procs.fill_path = sep1_fill_path;
    }

    return code;
}

/* Close the tiffsep device */
int
tiffsep1_prn_close(gx_device * pdev)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *) pdev;
    int num_dev_comp = tfdev->color_info.num_components;
    int num_std_colorants = tfdev->devn_params.num_std_colorant_names;
    int num_order = tfdev->devn_params.num_separation_order_names;
    int num_spot = tfdev->devn_params.separations.num_separations;
    char name[MAX_FILE_NAME_SIZE];
    int code = gdev_prn_close(pdev);
    short map_comp_to_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int comp_num;
    int num_comp = number_output_separations(num_dev_comp, num_std_colorants,
                                        num_order, num_spot);
    const char *fmt;
    gs_parsed_file_name_t parsed;

    if (code < 0)
        return code;
    code = gx_parse_output_file_name(&parsed, &fmt, tfdev->fname,
                                     strlen(tfdev->fname), pdev->memory);
    if (code < 0)
        return code;

    /* If we are doing separate pages, delete the old default file */
    if (parsed.iodev == iodev_default(pdev->memory)) {          /* filename includes "%nnd" */
        if (fmt) {
            char compname[MAX_FILE_NAME_SIZE];
            long count1 = pdev->PageCount;

            while (*fmt != 'l' && *fmt != '%')
                --fmt;
            if (*fmt == 'l')
                sprintf(compname, parsed.fname, count1);
            else
                sprintf(compname, parsed.fname, (int)count1);
            parsed.iodev->procs.delete_file(parsed.iodev, compname);
        } else {
            parsed.iodev->procs.delete_file(parsed.iodev, tfdev->fname);
        }

    }

    if (tfdev->close_files) {
        build_comp_to_sep_map((tiffsep_device *)tfdev, map_comp_to_sep);
        /* Close the separation files */
        for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
            if (tfdev->sep_file[comp_num] != NULL) {
                int sep_num = map_comp_to_sep[comp_num];

                code = create_separation_file_name((tiffsep_device *)tfdev, name,
                                                    MAX_FILE_NAME_SIZE, sep_num, true);
                if (code < 0)
                    return code;
                code = gx_device_close_output_file(pdev, name, tfdev->sep_file[comp_num]);
                if (code < 0)
                    return code;
                tfdev->sep_file[comp_num] = NULL;
            }
            if (tfdev->tiff[comp_num]) {
                TIFFCleanup(tfdev->tiff[comp_num]);
                tfdev->tiff[comp_num] = NULL;
            }
        }
        }
    /* If we have thresholds, free them and clear the pointers */
    if( tfdev->thresholds[0].dstart != NULL) {
        sep1_free_thresholds(tfdev);
    }
    return 0;
}


static int
sep1_fill_path(gx_device * pdev, const gs_imager_state * pis,
                 gx_path * ppath, const gx_fill_params * params,
                 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *)pdev;

    /* If we haven't already converted the ht into thresholds, do it now */
    if( tfdev->thresholds[0].dstart == NULL) {
        int code = sep1_ht_order_to_thresholds(pdev, pis);

        if (code < 0)
            return code;
    }
    return (tfdev->fill_path)( pdev, pis, ppath, params, pdevc, pcpath);
}

/*
 * This routine will check to see if the color component name  match those
 * that are available amoung the current device's color components.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the colorant is not being used due to a SeparationOrder device parameter.
 * It returns a negative value if not found.
 */
static int
tiffsep_get_color_comp_index(gx_device * dev, const char * pname,
                                int name_size, int component_type)
{
    tiffsep_device * pdev = (tiffsep_device *) dev;
    int index;

    if (strncmp(pname, "None", name_size) == 0) return -1;
    index = devn_get_color_comp_index(dev,
                &(pdev->devn_params), &(pdev->equiv_cmyk_colors),
                pname, name_size, component_type, ENABLE_AUTO_SPOT_COLORS);
    /* This is a one shot deal.  That is it will simply post a notice once that
       some colorants will be converted due to a limit being reached.  It will
       not list names of colorants since then I would need to keep track of
       which ones I have already mentioned.  Also, if someone is fooling with
       num_order, then this warning is not given since they should know what
       is going on already */
    if (index < 0 && component_type == SEPARATION_NAME &&
        pdev->warning_given == false &&
        pdev->devn_params.num_separation_order_names == 0) {
        dlprintf("**** Max spot colorants reached.\n");
        dlprintf("**** Some colorants will be converted to equivalent CMYK values.\n");
        dlprintf("**** If this is a Postscript file, try using the -dMaxSpots= option.\n");
        pdev->warning_given = true;
    }
    return index;
}

/*
 * There can be a conflict if a separation name is used as part of the file
 * name for a separation output file.  PostScript and PDF do not restrict
 * the characters inside separation names.  However most operating systems
 * have some sort of restrictions.  For instance: /, \, and : have special
 * meaning under Windows.  This implies that there should be some sort of
 * escape sequence for special characters.  This routine exists as a place
 * to put the handling of that escaping.  However it is not actually
 * implemented. Instead we just map them to '_'.
 */
static void
copy_separation_name(tiffsep_device * pdev,
                char * buffer, int max_size, int sep_num)
{
    int sep_size = pdev->devn_params.separations.names[sep_num].size;
    int i;
    int restricted_chars[] = { '/', '\\', ':', 0 };

    /* If name is too long then clip it. */
    if (sep_size > max_size - 1)
        sep_size = max_size - 1;
    memcpy(buffer, pdev->devn_params.separations.names[sep_num].data,
                sep_size);
    /* Change some of the commonly known restricted characters to '_' */
    for (i=0; restricted_chars[i] != 0; i++) {
        char *p = buffer;

        while ((p=memchr(p, restricted_chars[i], sep_size - (p - buffer))) != NULL)
            *p = '_';
    }
    buffer[sep_size] = 0;       /* Terminate string */
}

/*
 * Determine the length of the base file name.  If the file name includes
 * the extension '.tif', then we remove it from the length of the file
 * name.
 */
static int
length_base_file_name(tiffsep_device * pdev)
{
    int base_filename_length = strlen(pdev->fname);

#define REMOVE_TIF_FROM_BASENAME 1
#if REMOVE_TIF_FROM_BASENAME
    if (base_filename_length > 4 &&
        pdev->fname[base_filename_length - 4] == '.'  &&
        toupper(pdev->fname[base_filename_length - 3]) == 'T'  &&
        toupper(pdev->fname[base_filename_length - 2]) == 'I'  &&
        toupper(pdev->fname[base_filename_length - 1]) == 'F')
        base_filename_length -= 4;
#endif
#undef REMOVE_TIF_FROM_BASENAME

    return base_filename_length;
}

/*
 * Create a name for a separation file.
 */
static int
create_separation_file_name(tiffsep_device * pdev, char * buffer,
                                uint max_size, int sep_num, bool use_sep_name)
{
    uint base_filename_length = length_base_file_name(pdev);

    /*
     * In most cases it is more convenient if we append '.tif' to the end
     * of the file name.
     */
#define APPEND_TIF_TO_NAME 1
#define SUFFIX_SIZE (4 * APPEND_TIF_TO_NAME)

    memcpy(buffer, pdev->fname, base_filename_length);
    buffer[base_filename_length++] = use_sep_name ? '(' : '.';
    buffer[base_filename_length] = 0;  /* terminate the string */

    if (sep_num < pdev->devn_params.num_std_colorant_names) {
        if (max_size < strlen(pdev->devn_params.std_colorant_names[sep_num]))
            return_error(gs_error_rangecheck);
        strcat(buffer, pdev->devn_params.std_colorant_names[sep_num]);
    }
    else {
        sep_num -= pdev->devn_params.num_std_colorant_names;
        if (use_sep_name) {
            copy_separation_name(pdev, buffer + base_filename_length,
                                max_size - SUFFIX_SIZE - 2, sep_num);
        } else {
                /* Max of 10 chars in %d format */
            if (max_size < base_filename_length + 11)
                return_error(gs_error_rangecheck);
            sprintf(buffer + base_filename_length, "s%d", sep_num);
        }
    }
    if (use_sep_name)
        strcat(buffer, ")");

#if APPEND_TIF_TO_NAME
    if (max_size < strlen(buffer) + SUFFIX_SIZE)
        return_error(gs_error_rangecheck);
    strcat(buffer, ".tif");
#endif
    return 0;
}

/*
 * Determine the number of output separations for the tiffsep device.
 *
 * There are several factors which affect the number of output separations
 * for the tiffsep device.
 *
 * Due to limitations on the size of a gx_color_index, we are limited to a
 * maximum of 8 colors per pass.  Thus the tiffsep device is set to 8
 * components.  However this is not usually the number of actual separation
 * files to be created.
 *
 * If the SeparationOrder parameter has been specified, then we use it to
 * select the number and which separation files are created.
 *
 * If the SeparationOrder parameter has not been specified, then we use the
 * nuber of process colors (CMYK) and the number of spot colors unless we
 * exceed the 8 component maximum for the device.
 *
 * Note:  Unlike most other devices, the tiffsep device will accept more than
 * four spot colors.  However the extra spot colors will not be imaged
 * unless they are selected by the SeparationOrder parameter.  (This does
 * allow the user to create more than 8 separations by a making multiple
 * passes and using the SeparationOrder parameter.)
*/
static int
number_output_separations(int num_dev_comp, int num_std_colorants,
                                        int num_order, int num_spot)
{
    int num_comp =  num_std_colorants + num_spot;

    if (num_comp > num_dev_comp)
        num_comp = num_dev_comp;
    if (num_order)
        num_comp = num_order;
    return num_comp;
}

/*
 * This routine creates a list to map the component number to a separation number.
 * Values less than 4 refer to the CMYK colorants.  Higher values refer to a
 * separation number.
 *
 * This is the inverse of the separation_order_map.
 */
static void
build_comp_to_sep_map(tiffsep_device * pdev, short * map_comp_to_sep)
{
    int num_sep = pdev->devn_params.separations.num_separations;
    int num_std_colorants = pdev->devn_params.num_std_colorant_names;
    int sep_num;
    int num_channels;

    /* since both proc colors and spot colors are packed in same encoded value we
       need to have this limit */

    num_channels =
        ( (num_std_colorants + num_sep) < (GX_DEVICE_COLOR_MAX_COMPONENTS) ? (num_std_colorants + num_sep) : (GX_DEVICE_COLOR_MAX_COMPONENTS) );

    for (sep_num = 0; sep_num < num_channels; sep_num++) {
        int comp_num = pdev->devn_params.separation_order_map[sep_num];

        if (comp_num >= 0 && comp_num < GX_DEVICE_COLOR_MAX_COMPONENTS)
            map_comp_to_sep[comp_num] = sep_num;
    }
}

/* Open the tiffsep device.  This will now be using planar buffers so that
   we are not limited to 64 bit chunky */
int
tiffsep_prn_open(gx_device * pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    tiffsep_device *pdev_sep = (tiffsep_device *) pdev;
    int code, k;

    /* With planar the depth can be more than 64.  Update the color
       info to reflect the proper depth and number of planes */
    pdev_sep->warning_given = false;
    if (pdev_sep->devn_params.page_spot_colors >= 0) {
        pdev->color_info.num_components =
            (pdev_sep->devn_params.page_spot_colors
                                 + pdev_sep->devn_params.num_std_colorant_names);
        if (pdev->color_info.num_components > pdev->color_info.max_components)
            pdev->color_info.num_components = pdev->color_info.max_components;
    } else {
        /* We do not know how many spots may occur on the page.
           For this reason we go ahead and allocate the maximum that we
           have available.  Note, lack of knowledge only occurs in the case
           of PS files.  With PDF we know a priori the number of spot
           colorants.  */
        int num_comp = pdev_sep->max_spots + 4; /* Spots + CMYK */
        if (num_comp > GS_CLIENT_COLOR_MAX_COMPONENTS)
            num_comp = GS_CLIENT_COLOR_MAX_COMPONENTS;
        pdev->color_info.num_components = num_comp;
        pdev->color_info.max_components = num_comp;
    }
    /* Push this to the max amount as a default if someone has not set it */
    if (pdev_sep->devn_params.num_separation_order_names == 0)
        for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
            pdev_sep->devn_params.separation_order_map[k] = k;
        }
    pdev->color_info.depth = pdev->color_info.num_components *
                             pdev_sep->devn_params.bitspercomponent;
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    code = gdev_prn_open_planar(pdev, true);
    ppdev->file = NULL;
    pdev->icc_struct->supports_devn = true;
    return code;
}

static int
tiffsep_close_sep_file(tiffsep_device *tfdev, const char *fn, int comp_num)
{
    int code;

    if (tfdev->tiff[comp_num]) {
        TIFFCleanup(tfdev->tiff[comp_num]);
        tfdev->tiff[comp_num] = NULL;
    }

    code = gx_device_close_output_file((gx_device *)tfdev,
                                       fn,
                                       tfdev->sep_file[comp_num]);
    tfdev->sep_file[comp_num] = NULL;
    tfdev->tiff[comp_num] = NULL;

    return code;
}

static int
tiffsep_close_comp_file(tiffsep_device *tfdev, const char *fn)
{
    int code;

    if (tfdev->tiff_comp) {
        TIFFCleanup(tfdev->tiff_comp);
        tfdev->tiff_comp = NULL;
    }

    code = gx_device_close_output_file((gx_device *)tfdev,
                                       fn,
                                       tfdev->comp_file);
    tfdev->comp_file = NULL;

    return code;
}

/* Close the tiffsep device */
int
tiffsep_prn_close(gx_device * pdev)
{
    tiffsep_device * const pdevn = (tiffsep_device *) pdev;
    int num_dev_comp = pdevn->color_info.num_components;
    int num_std_colorants = pdevn->devn_params.num_std_colorant_names;
    int num_order = pdevn->devn_params.num_separation_order_names;
    int num_spot = pdevn->devn_params.separations.num_separations;
    char name[MAX_FILE_NAME_SIZE];
    int code;
    int comp_num;
    int num_comp = number_output_separations(num_dev_comp, num_std_colorants,
                                        num_order, num_spot);

    if (pdevn->tiff_comp && pdevn->close_files) {
        TIFFCleanup(pdevn->tiff_comp);
        pdevn->tiff_comp = NULL;
    }
    code = gdev_prn_close(pdev);
    if (code < 0)
        return code;

    if (pdevn->close_files) {
        /* Close the separation files */
        for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
            if (pdevn->sep_file[comp_num] != NULL) {
                int sep_num = pdevn->devn_params.separation_order_map[comp_num];

                code = create_separation_file_name(pdevn, name,
                        MAX_FILE_NAME_SIZE, sep_num, true);
                if (code < 0)
                    return code;
                code = tiffsep_close_sep_file(pdevn, name, comp_num);
                if (code < 0)
                    return code;
            }
        }
    }

    return 0;
}

/*
 * Element for a map to convert colorants to a CMYK color.
 */
typedef struct cmyk_composite_map_s {
    frac c, m, y, k;
} cmyk_composite_map;

/*
 * Build the map to be used to create a CMYK equivalent to the current
 * device components.
 */
static void
build_cmyk_map(tiffsep_device * pdev, int num_comp,
               cmyk_composite_map * cmyk_map)
{
    int comp_num;

    for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
        int sep_num = pdev->devn_params.separation_order_map[comp_num];

        cmyk_map[comp_num].c = cmyk_map[comp_num].m =
            cmyk_map[comp_num].y = cmyk_map[comp_num].k = frac_0;
        /* The tiffsep device has 4 standard colors:  CMYK */
        if (sep_num < pdev->devn_params.num_std_colorant_names) {
            switch (sep_num) {
                case 0: cmyk_map[comp_num].c = frac_1; break;
                case 1: cmyk_map[comp_num].m = frac_1; break;
                case 2: cmyk_map[comp_num].y = frac_1; break;
                case 3: cmyk_map[comp_num].k = frac_1; break;
            }
        }
        else {
            sep_num -= pdev->devn_params.num_std_colorant_names;
            if (pdev->equiv_cmyk_colors.color[sep_num].color_info_valid) {
                cmyk_map[comp_num].c = pdev->equiv_cmyk_colors.color[sep_num].c;
                cmyk_map[comp_num].m = pdev->equiv_cmyk_colors.color[sep_num].m;
                cmyk_map[comp_num].y = pdev->equiv_cmyk_colors.color[sep_num].y;
                cmyk_map[comp_num].k = pdev->equiv_cmyk_colors.color[sep_num].k;
            }
        }
    }
}

/*
 * Build a CMYK equivalent to a raster line from planar buffer
 */
static void
build_cmyk_raster_line_fromplanar(gs_get_bits_params_t params, byte * dest,
                                  int width, int num_comp,
                                  cmyk_composite_map * cmyk_map, int num_order,
                                  tiffsep_device * const tfdev)
{
    int pixel, comp_num;
    uint temp, cyan, magenta, yellow, black;
    cmyk_composite_map * cmyk_map_entry;

    for (pixel = 0; pixel < width; pixel++) {
        cmyk_map_entry = cmyk_map;
        temp = *(params.data[tfdev->devn_params.separation_order_map[0]] + pixel);
        cyan = cmyk_map_entry->c * temp;
        magenta = cmyk_map_entry->m * temp;
        yellow = cmyk_map_entry->y * temp;
        black = cmyk_map_entry->k * temp;
        cmyk_map_entry++;
        for (comp_num = 1; comp_num < num_comp; comp_num++) {
            temp =
                *(params.data[tfdev->devn_params.separation_order_map[comp_num]] + pixel);
            cyan += cmyk_map_entry->c * temp;
            magenta += cmyk_map_entry->m * temp;
            yellow += cmyk_map_entry->y * temp;
            black += cmyk_map_entry->k * temp;
            cmyk_map_entry++;
        }
        cyan /= frac_1;
        magenta /= frac_1;
        yellow /= frac_1;
        black /= frac_1;
        if (cyan > MAX_COLOR_VALUE)
            cyan = MAX_COLOR_VALUE;
        if (magenta > MAX_COLOR_VALUE)
            magenta = MAX_COLOR_VALUE;
        if (yellow > MAX_COLOR_VALUE)
            yellow = MAX_COLOR_VALUE;
        if (black > MAX_COLOR_VALUE)
            black = MAX_COLOR_VALUE;
        *dest++ = cyan;
        *dest++ = magenta;
        *dest++ = yellow;
        *dest++ = black;
    }
}

static int
sep1_ht_order_to_thresholds(gx_device *pdev, const gs_imager_state *pis)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *)pdev;
    gs_memory_t *mem = pdev->memory;

    /* If we have thresholds, free them and clear the pointers */
    if( tfdev->thresholds[0].dstart != NULL) {
        sep1_free_thresholds(tfdev);
    } else {
        int nc, j;
        gx_ht_order *d_order;
        threshold_array_t *dptr;

        if (pis->dev_ht == NULL) {
            emprintf(mem, "sep1_order_to_thresholds: no dev_ht available\n");
            return_error(gs_error_rangecheck);  /* error condition */
        }
        nc = pis->dev_ht->num_comp;
        for( j=0; j<nc; j++ ) {
            d_order = &(pis->dev_ht->components[j].corder);
            dptr = &(tfdev->thresholds[j]);
            dptr->dstart = threshold_from_order( d_order, &(dptr->dwidth), &(dptr->dheight), mem);
            if( dptr->dstart == NULL ) {
                emprintf(mem,
                         "sep1_order_to_thresholds: conversion to thresholds failed.\n");
                return_error(gs_error_rangecheck);      /* error condition */
            }
        }
    }
    return 0;
}

static void
sep1_free_thresholds(tiffsep1_device *tfdev)
{
    int i;

    for (i=0; i < GX_DEVICE_COLOR_MAX_COMPONENTS + 1; i++) {
        threshold_array_t *dptr = &(tfdev->thresholds[i]);

        if (dptr->dstart != NULL) {
            gs_free(tfdev->memory, dptr->dstart, dptr->dwidth * dptr->dheight, 1,
                      "tiffsep1_threshold_array");
            dptr->dstart = NULL;
        }
    }
}

/************************************************************************/
/*      This routine generates a threshold matrix for use in            */
/*      the color dithering routine from the "order" info in            */
/*      the current graphics state.                                     */
/*                                                                      */
/************************************************************************/

static byte*
threshold_from_order( gx_ht_order *d_order, int *Width, int *Height, gs_memory_t *memory)
{
   int i, j, l, prev_l;
   unsigned char *thresh;
   gx_ht_bit *bits = (gx_ht_bit *)d_order->bit_data;
    int num_repeat, shift;
    int row_kk, col_kk, kk;

    /* We can have simple or complete orders.  Simple ones tile the threshold
       with shifts.   To handle those we simply loop over the number of
       repeats making sure to shift columns when we set our threshold values */
    num_repeat = d_order->full_height / d_order->height;
    shift = d_order->shift;

#ifdef DEBUG
if ( gs_debug_c('h') ) {
       dprintf2("   width=%d, height=%d,",
            d_order->width, d_order->height );
       dprintf2(" num_levels=%d, raster=%d\n",
            d_order->num_levels, d_order->raster );
}
#endif

   thresh = (byte *)gs_malloc(memory, d_order->width * d_order->full_height, 1,
                                  "tiffsep1_threshold_array");
   if( thresh == NULL ) {
#ifdef DEBUG
      emprintf(memory, "threshold_from_order, malloc failed\n");
      emprintf2(memory, "   width=%d, height=%d,",
                d_order->width, d_order->height );
      emprintf2(memory, " num_levels=%d, raster=%d\n",
                d_order->num_levels, d_order->raster );
#endif
        return thresh ;         /* error if allocation failed   */
   }
   for( i=0; i<d_order->num_bits; i++ )
      thresh[i] = 1;

   *Width  = d_order->width;
   *Height = d_order->full_height;

   prev_l = 0;
   l = 1;
   while( l < d_order->num_levels ) {
      if( d_order->levels[l] > d_order->levels[prev_l] ) {
         int t_level = (256*l)/d_order->num_levels;
         int row, col;
#ifdef DEBUG
         if ( gs_debug_c('h') )
            dprintf2("  level[%3d]=%3d\n", l, d_order->levels[l]);
#endif
         for( j=d_order->levels[prev_l]; j<d_order->levels[l]; j++) {
#ifdef DEBUG
            if ( gs_debug_c('h') )
               dprintf2("       bits.offset=%3d, bits.mask=%8x  ",
                   bits[j].offset, bits[j].mask);
#endif
            row = bits[j].offset / d_order->raster;
            for( col=0; col < (8*sizeof(ht_mask_t)); col++ ) {
               if( bits[j].mask & bit_order[col] )
                  break;
            }
            col += 8 * ( bits[j].offset - (row * d_order->raster) );
#ifdef DEBUG
            if ( gs_debug_c('h') )
               dprintf3("row=%2d, col=%2d, t_level=%3d\n",
                  row, col, t_level);
#endif
            if( col < (int)d_order->width ) {
                for (kk = 0; kk < num_repeat; kk++) {
                    row_kk = row + kk * d_order->height;
                    col_kk = col + kk * shift;
                    col_kk = col_kk % d_order->width;
                    *(thresh + col_kk + (row_kk * d_order->width)) = t_level;
                }
            }
         }
         prev_l = l;
      }
      l++;
   }

#ifdef DEBUG
   if (gs_debug_c('h')) {
      for( i=0; i<(int)d_order->height; i++ ) {
         dprintf1("threshold array row %3d= ", i);
         for( j=(int)d_order->width-1; j>=0; j-- )
            dprintf1("%3d ", *(thresh+j+(i*d_order->width)) );
         dprintf("\n");
      }
   }
#endif

   return thresh;
}

/*
 * Output the image data for the tiff separation (tiffsep) device.  The data
 * for the tiffsep device is written in separate planes to separate files.
 *
 * The DeviceN parameters (SeparationOrder, SeparationColorNames, and
 * MaxSeparations) are applied to the tiffsep device.
 */
static int
tiffsep_print_page(gx_device_printer * pdev, FILE * file)
{
    tiffsep_device * const tfdev = (tiffsep_device *)pdev;
    int num_std_colorants = tfdev->devn_params.num_std_colorant_names;
    int num_order = tfdev->devn_params.num_separation_order_names;
    int num_spot = tfdev->devn_params.separations.num_separations;
    int num_comp, comp_num, sep_num, code = 0, code1 = 0;
    cmyk_composite_map cmyk_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
    char name[MAX_FILE_NAME_SIZE];
    int base_filename_length = length_base_file_name(tfdev);
    int save_depth = pdev->color_info.depth;
    int save_numcomps = pdev->color_info.num_components;
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int plane_count = 0;  /* quite compiler */
    int factor = tfdev->DownScaleFactor;
    int mfs = tfdev->MinFeatureSize;
    int dst_bpc = tfdev->BitsPerComponent;
    gx_downscaler_t ds;
    int width = gx_downscaler_scale(tfdev->width, factor);
    int height = gx_downscaler_scale(tfdev->height, factor);

    /* Print the names of the spot colors */
    if (num_order == 0) {
        for (sep_num = 0; sep_num < num_spot; sep_num++) {
            copy_separation_name(tfdev, name,
                MAX_FILE_NAME_SIZE - base_filename_length - SUFFIX_SIZE, sep_num);
            dlprintf1("%%%%SeparationName: %s\n", name);
        }
    }

    /*
     * Check if the file name has a numeric format.  If so then we want to
     * create individual separation files for each page of the input.
    */
    code = gx_parse_output_file_name(&parsed, &fmt, tfdev->fname,
                                     strlen(tfdev->fname), pdev->memory);

    /* Write the page directory for the CMYK equivalent file. */
    if (dst_bpc == 8 && !tfdev->comp_file) {
        pdev->color_info.depth = 32;        /* Create directory for 32 bit cmyk */
        if (tfdev->Compression==COMPRESSION_NONE &&
            height > ((unsigned long) 0xFFFFFFFF - ftell(file))/(width*4)) { /* note width is never 0 in print_page */
            dprintf("CMYK composite file would be too large! Reduce resolution or enable compression.\n");
            return_error(gs_error_rangecheck);  /* this will overflow 32 bits */
        }

        code = gx_device_open_output_file((gx_device *)pdev, pdev->fname, true, true, &(tfdev->comp_file));
        if (code < 0)
            return code;

        tfdev->tiff_comp = tiff_from_filep(pdev->dname, tfdev->comp_file, tfdev->BigEndian);
        if (!tfdev->tiff_comp)
            return_error(gs_error_invalidfileaccess);

    }
    if (dst_bpc == 8) {
        code = tiff_set_fields_for_printer(pdev, tfdev->tiff_comp, factor, 0);
        if (dst_bpc == 1 && (tfdev->Compression==COMPRESSION_NONE ||
                             tfdev->Compression==COMPRESSION_CCITTRLE ||
                             tfdev->Compression==COMPRESSION_CCITTFAX3 ||
                             tfdev->Compression==COMPRESSION_CCITTFAX4))
          tiff_set_cmyk_fields(pdev, tfdev->tiff_comp, 1, tfdev->Compression, tfdev->MaxStripSize);
        else if (dst_bpc == 8 && (tfdev->Compression==COMPRESSION_NONE ||
                                  tfdev->Compression==COMPRESSION_LZW ||
                                  tfdev->Compression==COMPRESSION_PACKBITS))
          tiff_set_cmyk_fields(pdev, tfdev->tiff_comp, 8, tfdev->Compression, tfdev->MaxStripSize);
        else
          tiff_set_cmyk_fields(pdev, tfdev->tiff_comp, 8, COMPRESSION_LZW, tfdev->MaxStripSize);
}
    pdev->color_info.depth = save_depth;
    if (code < 0)
        return code;

    /* Set up the separation output files */
    num_comp = number_output_separations( tfdev->color_info.num_components,
                                        num_std_colorants, num_order, num_spot);

    if (!num_order && num_comp < num_std_colorants + num_spot) {
        dlprintf("Warning: skipping one or more colour separations, see: Devices.htm#TIFF\n");
    }

    for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
        int sep_num = tfdev->devn_params.separation_order_map[comp_num];

        code = create_separation_file_name(tfdev, name, MAX_FILE_NAME_SIZE,
                                            sep_num, true);
        if (code < 0)
            return code;

        /* Open the separation file, if not already open */
        if (tfdev->sep_file[comp_num] == NULL) {
            code = gx_device_open_output_file((gx_device *)pdev, name,
                    true, true, &(tfdev->sep_file[comp_num]));
            if (code < 0)
                return code;
            tfdev->tiff[comp_num] = tiff_from_filep(name,
                                                    tfdev->sep_file[comp_num],
                                                    tfdev->BigEndian);
            if (!tfdev->tiff[comp_num])
                return_error(gs_error_ioerror);
        }

        pdev->color_info.depth = dst_bpc;     /* Create files for 8 bit gray */
        pdev->color_info.num_components = 1;
        if (tfdev->Compression==COMPRESSION_NONE &&
            height*8/dst_bpc > ((unsigned long) 0xFFFFFFFF - ftell(file))/width) /* note width is never 0 in print_page */
            return_error(gs_error_rangecheck);  /* this will overflow 32 bits */

        code = tiff_set_fields_for_printer(pdev, tfdev->tiff[comp_num], factor, 0);
        tiff_set_gray_fields(pdev, tfdev->tiff[comp_num], dst_bpc, tfdev->Compression, tfdev->MaxStripSize);
        pdev->color_info.depth = save_depth;
        pdev->color_info.num_components = save_numcomps;
        if (code < 0)
            return code;
    }

    if (dst_bpc == 8)
        build_cmyk_map(tfdev, num_comp, cmyk_map);

    {
        int raster_plane = bitmap_raster(width * 8);
        byte *planes[GS_CLIENT_COLOR_MAX_COMPONENTS] = { 0 };
        int cmyk_raster = width * NUM_CMYK_COMPONENTS;
        int pixel, y;
        byte * sep_line;
        int plane_index;
        int offset_plane = 0;

        sep_line =
            gs_alloc_bytes(pdev->memory, cmyk_raster, "tiffsep_print_page");

        for (comp_num = 0; comp_num < num_comp; comp_num++ )
            TIFFCheckpointDirectory(tfdev->tiff[comp_num]);
        if (dst_bpc == 8)
            TIFFCheckpointDirectory(tfdev->tiff_comp);

        /* Write the page data. */
        {
            gs_get_bits_params_t params;

            /* Return planar data */
            params.options = (GB_RETURN_POINTER | GB_RETURN_COPY |
                 GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_RASTER_STANDARD |
                 GB_PACKING_PLANAR | GB_COLORS_NATIVE | GB_ALPHA_NONE);
            params.x_offset = 0;
            params.raster = bitmap_raster(width * pdev->color_info.depth);

            if (num_order > 0) {
                /* In this case, there was a specification for a separation
                   color order, which indicates what colorants we will
                   actually creat individual separation files for.  We need
                   to allocate for the standard colorants.  This is due to the
                   fact that even when we specify a single spot colorant, we
                   still create the composite CMYK output file. */
                for (comp_num = 0; comp_num < num_std_colorants; comp_num++) {
                    planes[comp_num] = gs_alloc_bytes(pdev->memory, raster_plane,
                                                      "tiffsep_print_page");
                    params.data[comp_num] = planes[comp_num];
                    if (params.data[comp_num] == NULL) {
                        code = gs_error_VMerror;
                        goto cleanup;
                    }
                }
                offset_plane = num_std_colorants;
                /* Now we need to make sure that we do not allocate extra
                   planes if any of the colorants in the order list are
                   one of the standard colorant names */
                plane_index = plane_count = num_std_colorants;
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    int temp_pos;

                    temp_pos = tfdev->devn_params.separation_order_map[comp_num];
                    if (temp_pos >= num_std_colorants) {
                        /* We have one that is not a standard colorant name
                           so allocate a new plane */
                        planes[plane_count] = gs_alloc_bytes(pdev->memory, raster_plane,
                                                        "tiffsep_print_page");
                        /* Assign the new plane to the appropriate position */
                        params.data[plane_index] = planes[plane_count];
                        if (params.data[plane_index] == NULL) {
                            code = gs_error_VMerror;
                            goto cleanup;
                        }
                        plane_count += 1;
                    } else {
                        /* Assign params.data with the appropriate std.
                           colorant plane position */
                        params.data[plane_index] = planes[temp_pos];
                    }
                    plane_index += 1;
                }
            } else {
                /* Sep color order number was not specified so just render all
                   the  planes that we can */
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    planes[comp_num] = gs_alloc_bytes(pdev->memory, raster_plane,
                                                    "tiffsep_print_page");
                    params.data[comp_num] = planes[comp_num];
                    if (params.data[comp_num] == NULL) {
                        code = gs_error_VMerror;
                        goto cleanup;
                    }
                }
            }
            code = gx_downscaler_init_planar(&ds, (gx_device *)pdev, &params,
                                             num_comp, factor, mfs, 8, dst_bpc);
            if (code < 0)
                goto cleanup;
            for (y = 0; y < height; ++y) {
                code = gx_downscaler_get_bits_rectangle(&ds, &params, y);
                if (code < 0)
                    goto cleanup;
                /* Write separation data (tiffgray format) */
                for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
                    byte *src;
                    byte *dest = sep_line;

                    if (num_order > 0) {
                        src = params.data[tfdev->devn_params.separation_order_map[comp_num]];
                    } else
                        src = params.data[comp_num];
                    for (pixel = 0; pixel < width; pixel++, dest++, src++)
                        *dest = MAX_COLOR_VALUE - *src;    /* Gray is additive */
                    TIFFWriteScanline(tfdev->tiff[comp_num], (tdata_t)sep_line, y, 0);
                }
                /* Write CMYK equivalent data (tiff32nc format) */
                if (dst_bpc == 8) {
                    build_cmyk_raster_line_fromplanar(params, sep_line, width,
                                                      num_comp, cmyk_map, num_order,
                                                      tfdev);
                    TIFFWriteScanline(tfdev->tiff_comp, (tdata_t)sep_line, y, 0);
                }
            }
cleanup:
            if (num_order > 0) {
                /* Free up the standard colorants if num_order was set.
                   In this process, we need to make sure that none of them
                   were the standard colorants.  plane_count should have
                   the sum of the std. colorants plus any non-standard
                   ones listed in separation color order */
                for (comp_num = 0; comp_num < plane_count; comp_num++) {
                    gs_free_object(pdev->memory, planes[comp_num],
                                                    "tiffsep_print_page");
                }
            } else {
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    gs_free_object(pdev->memory, planes[comp_num + offset_plane],
                                                    "tiffsep_print_page");
                }
            }
            gx_downscaler_fin(&ds);
            gs_free_object(pdev->memory, sep_line, "tiffsep_print_page");
        }
        code1 = code;
        for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
            TIFFWriteDirectory(tfdev->tiff[comp_num]);
            if (fmt) {
                int sep_num = tfdev->devn_params.separation_order_map[comp_num];

                code = create_separation_file_name(tfdev, name, MAX_FILE_NAME_SIZE, sep_num, false);
                if (code < 0) {
                    code1 = code;
                    continue;
                }
                code = tiffsep_close_sep_file(tfdev, name, comp_num);
                if (code < 0) {
                    code1 = code;
                }
            }
        }

        if (dst_bpc == 8)
            TIFFWriteDirectory(tfdev->tiff_comp);
        if (fmt) {
            code = tiffsep_close_comp_file(tfdev, pdev->fname);
        }
        if (code1 < 0) {
            code = code1;
        }
    }

    /* After page is printed free up the separation list since we may have
       different spots on the next page */
    free_separation_names(tfdev->memory, &(tfdev->devn_params.separations));
    tfdev->devn_params.num_separation_order_names = 0;
    return code;
}

/*
 * Output the image data for the tiff separation (tiffsep1) device.  The data
 * for the tiffsep1 device is written in separate planes to separate files.
 *
 * The DeviceN parameters (SeparationOrder, SeparationColorNames, and
 * MaxSeparations) are applied to the tiffsep device.
 */
static int
tiffsep1_print_page(gx_device_printer * pdev, FILE * file)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *)pdev;
    int num_std_colorants = tfdev->devn_params.num_std_colorant_names;
    int num_order = tfdev->devn_params.num_separation_order_names;
    int num_spot = tfdev->devn_params.separations.num_separations;
    int num_comp, comp_num, code = 0, code1 = 0;
    short map_comp_to_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    char name[MAX_FILE_NAME_SIZE];
    int save_depth = pdev->color_info.depth;
    int save_numcomps = pdev->color_info.num_components;
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int non_encodable_count = 0;

    if (tfdev->thresholds[0].dstart == NULL)
        return_error(gs_error_rangecheck);

    build_comp_to_sep_map((tiffsep_device *)tfdev, map_comp_to_sep);

    /*
     * Check if the file name has a numeric format.  If so then we want to
     * create individual separation files for each page of the input.
    */
    code = gx_parse_output_file_name(&parsed, &fmt, pdev->fname,
                                     strlen(pdev->fname), pdev->memory);

    /* If the output file is on disk and the name contains a page #, */
    /* then delete the previous file. */
    if (pdev->file != NULL && parsed.iodev == iodev_default(pdev->memory) && fmt) {
        char compname[MAX_FILE_NAME_SIZE];
        long count1 = pdev->PageCount;

        gx_device_close_output_file((gx_device *)pdev, pdev->fname, pdev->file);
        pdev->file = NULL;

        while (*fmt != 'l' && *fmt != '%')
            --fmt;
        if (*fmt == 'l')
            sprintf(compname, parsed.fname, count1);
        else
            sprintf(compname, parsed.fname, (int)count1);
        parsed.iodev->procs.delete_file(parsed.iodev, compname);
        /* we always need an open printer (it will get deleted in tiffsep1_prn_close */
        if ((code = gdev_prn_open_printer((gx_device *)pdev, 1)) < 0)
            return code;
    }

    /* Set up the separation output files */
    num_comp = number_output_separations( tfdev->color_info.num_components,
                                        num_std_colorants, num_order, num_spot);
    for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
        int sep_num = map_comp_to_sep[comp_num];

        code = create_separation_file_name((tiffsep_device *)tfdev, name,
                                        MAX_FILE_NAME_SIZE, sep_num, true);
        if (code < 0)
            return code;

        /* Open the separation file, if not already open */
        if (tfdev->sep_file[comp_num] == NULL) {
            code = gx_device_open_output_file((gx_device *)pdev, name,
                    true, true, &(tfdev->sep_file[comp_num]));
            if (code < 0)
                return code;
            tfdev->tiff[comp_num] = tiff_from_filep(name,
                                                    tfdev->sep_file[comp_num],
                                                    tfdev->BigEndian);
            if (!tfdev->tiff[comp_num])
                return_error(gs_error_ioerror);
        }

        pdev->color_info.depth = 8;     /* Create files for 8 bit gray */
        pdev->color_info.num_components = 1;
        code = tiff_set_fields_for_printer(pdev, tfdev->tiff[comp_num], 1, 0);
        tiff_set_gray_fields(pdev, tfdev->tiff[comp_num], 1, tfdev->Compression, tfdev->MaxStripSize);
        pdev->color_info.depth = save_depth;
        pdev->color_info.num_components = save_numcomps;
        if (code < 0)
            return code;

    }   /* end initialization of separation files */


    {   /* Get the expanded contone line, halftone and write out the dithered separations */
        int raster = gdev_prn_raster(pdev);
        byte *planes[GS_CLIENT_COLOR_MAX_COMPONENTS];
        int width = tfdev->width;
        int raster_plane = bitmap_raster(width * 8);
        int dithered_raster = ((7 + width) / 8) + ARCH_SIZEOF_LONG;
        int pixel, y;
        gs_get_bits_params_t params;
        gs_int_rect rect;
        /* the dithered_line is assumed to be 32-bit aligned by the alloc */
        uint32_t *dithered_line = (uint32_t *)gs_alloc_bytes(pdev->memory, dithered_raster,
                                "tiffsep1_print_page");

        memset(planes, 0, sizeof(*planes) * GS_CLIENT_COLOR_MAX_COMPONENTS);

        /* Return planar data */
        params.options = (GB_RETURN_POINTER | GB_RETURN_COPY |
             GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_RASTER_STANDARD |
             GB_PACKING_PLANAR | GB_COLORS_NATIVE | GB_ALPHA_NONE);
        params.x_offset = 0;
        params.raster = bitmap_raster(width * pdev->color_info.depth);

        code = 0;
        for (comp_num = 0; comp_num < num_comp; comp_num++) {
            planes[comp_num] = gs_alloc_bytes(pdev->memory, raster_plane,
                                            "tiffsep1_print_page");
            if (planes[comp_num] == NULL) {
                code = gs_error_VMerror;
                break;
            }
        }

        if (code < 0 || dithered_line == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }

        for (comp_num = 0; comp_num < num_comp; comp_num++ )
            TIFFCheckpointDirectory(tfdev->tiff[comp_num]);

        rect.p.x = 0;
        rect.q.x = pdev->width;
        /* Loop for the lines */
        for (y = 0; y < pdev->height; ++y) {
            rect.p.y = y;
            rect.q.y = y + 1;
            /* We have to reset the pointers since get_bits_rect will have moved them */
            for (comp_num = 0; comp_num < num_comp; comp_num++)
                params.data[comp_num] = planes[comp_num];
            code = (*dev_proc(pdev, get_bits_rectangle))((gx_device *)pdev, &rect, &params, NULL);
            if (code < 0)
                break;

            /* Dither the separation and write it out */
            for (comp_num = 0; comp_num < num_comp; comp_num++ ) {

/***** #define SKIP_HALFTONING_FOR_TIMING *****/ /* uncomment for timing test */
#ifndef SKIP_HALFTONING_FOR_TIMING

                /*
                 * Define 32-bit writes by default. Testing shows that while this is more
                 * complex code, it runs measurably and consistently faster than the more
                 * obvious 8-bit code. The 8-bit code is kept to help future optimization
                 * efforts determine what affects tight loop optimization. Subtracting the
                 * time when halftoning is skipped shows that the 32-bit halftoning is
                 * 27% faster.
                 */
#define USE_32_BIT_WRITES
                byte *thresh_line_base = tfdev->thresholds[comp_num].dstart +
                                    ((y % tfdev->thresholds[comp_num].dheight) *
                                        tfdev->thresholds[comp_num].dwidth) ;
                byte *thresh_ptr = thresh_line_base;
                byte *thresh_limit = thresh_ptr + tfdev->thresholds[comp_num].dwidth;
                byte *src = params.data[comp_num];
#ifdef USE_32_BIT_WRITES
                uint32_t *dest = dithered_line;
                uint32_t val = 0;
                const uint32_t *mask = &bit_order[0];
#else   /* example 8-bit code */
                byte *dest = dithered_line;
                byte val = 0;
                byte mask = 0x80;
#endif /* USE_32_BIT_WRITES */

                for (pixel = 0; pixel < width; pixel++, src++) {
#ifdef USE_32_BIT_WRITES
                    if (*src < *thresh_ptr++)
                        val |= *mask;
                    if (++mask == &(bit_order[32])) {
                        *dest++ = val;
                        val = 0;
                        mask = &bit_order[0];
                    }
#else   /* example 8-bit code */
                    if (*src < *thresh_ptr++)
                        val |= mask;
                    mask >>= 1;
                    if (mask == 0) {
                        *dest++ = val;
                        val = 0;
                        mask = 0x80;
                    }
#endif /* USE_32_BIT_WRITES */
                    if (thresh_ptr >= thresh_limit)
                        thresh_ptr = thresh_line_base;
                } /* end src pixel loop - collect last bits if any */
                /* the following relies on their being enough 'pad' in dithered_line */
#ifdef USE_32_BIT_WRITES
                if (mask != &bit_order[0]) {
                    *dest = val;
                }
#else   /* example 8-bit code */
                if (mask != 0x80) {
                    *dest = val;
                }
#endif /* USE_32_BIT_WRITES */
#endif /* SKIP_HALFTONING_FOR_TIMING */
                TIFFWriteScanline(tfdev->tiff[comp_num], (tdata_t)dithered_line, y, 0);
            } /* end component loop */
        }

        /* Update the strip data */
        code1 = 0;
        for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
            TIFFWriteDirectory(tfdev->tiff[comp_num]);

            if (fmt) {
                int sep_num = map_comp_to_sep[comp_num];

                code = create_separation_file_name((tiffsep_device *)tfdev, name, MAX_FILE_NAME_SIZE, sep_num, false);
                if (code < 0) {
                    code1 = code;
                    continue;
                }
                code = tiffsep_close_sep_file((tiffsep_device *)tfdev, name, comp_num);
                if (code < 0) {
                    code1 = code;
                }
            }
        }
        code = code1;

        /* free any allocations and exit with code */
cleanup:
        gs_free_object(pdev->memory, dithered_line, "tiffsep1_print_page");
        for (comp_num = 0; comp_num < num_comp; comp_num++) {
            gs_free_object(pdev->memory, planes[comp_num], "tiffsep1_print_page");
        }
    }
    /*
     * If we have any non encodable pixels then signal an error.
     */
    if (non_encodable_count) {
        dlprintf1("WARNING:  Non encodable pixels = %d\n", non_encodable_count);
        return_error(gs_error_rangecheck);
    }

    return code;
}
