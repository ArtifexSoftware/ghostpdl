/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* tiffgray device:  8-bit Gray uncompressed TIFF device */
/* tiff32nc device:  32-bit CMYK uncompressed TIFF device */
/* tiffsep device: Generate individual TIFF gray files for each separation */
/*                 as well as a 'composite' 32-bit CMYK for the page.      */
/* tiffsep1 device: Generate individual TIFF 1-bit files for each separation. */

#include "gdevtifs.h"
#include "gdevprn.h"
#include "gdevdevn.h"
#include "gsequivc.h"
#include "gxdht.h"
#include "gxiodev.h"
#include "stdio_.h"
#include "ctype_.h"

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
		    0, 0, 0, 0,	/* Margins */
		    1, 8, 255, 0, 256, 0, tiffgray_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE
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

    if (pdev->height > (max_long - ftell(file))/(pdev->width)) /* note width is never 0 in print_page */
	return_error(gs_error_rangecheck);  /* this will overflow max_long */

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
	return code;

    tiff_set_gray_fields(pdev, tfdev->tif, 8, tfdev->Compression, tfdev->MaxStripSize);

    return tiff_print_page(pdev, tfdev->tif);
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
		    0, 0, 0, 0,	/* Margins */
		    4, 32, 255, 255, 256, 256, tiffcmyk_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE
};

/* 16-bit-per-plane separated CMYK color. */

static const gx_device_procs tiff64nc_procs = {
    cmyk_procs(cmyk_16bit_map_color_cmyk, cmyk_16bit_map_cmyk_color)
};

const gx_device_tiff gs_tiff64nc_device = {
    prn_device_body(gx_device_tiff, tiff64nc_procs, "tiff64nc",
		    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		    X_DPI, Y_DPI,
		    0, 0, 0, 0,	/* Margins */
		    4, 64, 255, 255, 256, 256, tiffcmyk_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_NONE,
    TIFF_DEFAULT_STRIP_SIZE
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

    if (pdev->height > (max_long - ftell(file))/(pdev->width)) /* note width is never 0 in print_page */
	return_error(gs_error_rangecheck);  /* this will overflow max_long */

    code = gdev_tiff_begin_page(tfdev, file);
    if (code < 0)
	return code;

    tiff_set_cmyk_fields(pdev,
			 tfdev->tif,
			 pdev->color_info.depth / pdev->color_info.num_components,
			 tfdev->Compression,
			 tfdev->MaxStripSize);

    return tiff_print_page(pdev, tfdev->tif);
}

/* ----------  The tiffsep device ------------ */

#define NUM_CMYK_COMPONENTS 4
#define MAX_FILE_NAME_SIZE gp_file_name_sizeof
#define MAX_COLOR_VALUE	255		/* We are using 8 bits per colorant */

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
static dev_proc_encode_color(tiffsep_encode_compressed_color);
static dev_proc_decode_color(tiffsep_decode_compressed_color);
static dev_proc_update_spot_equivalent_colors(tiffsep_update_spot_equivalent_colors);
static dev_proc_ret_devn_params(tiffsep_ret_devn_params);
static dev_proc_open_device(tiffsep1_prn_open);
static dev_proc_close_device(tiffsep1_prn_close);
static dev_proc_put_params(tiffsep1_put_params);
static dev_proc_print_page(tiffsep1_print_page);
static dev_proc_fill_path(sep1_fill_path);

#define tiffsep_devices_common\
    gx_device_common;\
    gx_prn_device_common;\
	/* tiff state for separation files */\
    FILE *sep_file[GX_DEVICE_COLOR_MAX_COMPONENTS];\
    TIFF *tiff[GX_DEVICE_COLOR_MAX_COMPONENTS]; \
    bool  BigEndian;            /* true = big endian; false = little endian */\
    uint16 Compression;		/* for the separation files, same values as
				   TIFFTAG_COMPRESSION */\
    long MaxStripSize;\
    gs_devn_params devn_params;		/* DeviceN generated parameters */\
    equivalent_cmyk_color_params equiv_cmyk_colors

/*
 * A structure definition for a DeviceN type device
 */
typedef struct tiffsep_device_s {
    tiffsep_devices_common;
    TIFF *tiff_comp;		/* tiff file for comp file */
} tiffsep_device;

/* threshold array structure */
typedef struct threshold_array_s {
      int dheight, dwidth;
      byte *dstart;
} threshold_array_t;

typedef struct tiffsep1_device_s {
    tiffsep_devices_common;
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
}
RELOC_PTRS_END

/* Even though tiffsep_device_finalize is the same as gx_device_finalize, */
/* we need to implement it separately because st_composite_final */
/* declares all 3 procedures as private. */
static void
tiffsep_device_finalize(void *vpdev)
{
    gx_device_finalize(vpdev);
}

gs_private_st_composite_final(st_tiffsep_device, tiffsep_device,
    "tiffsep_device", tiffsep_device_enum_ptrs, tiffsep_device_reloc_ptrs,
    tiffsep_device_finalize);

/*
 * Macro definition for tiffsep device procedures
 */
#define sep_device_procs(open, close, encode_color, decode_color, update_spot_colors,put_params, fill_path) \
{	open,\
	gx_default_get_initial_matrix,\
	NULL,				/* sync_output */\
	gdev_prn_output_page,		/* output_page */\
	close,				/* close */\
	NULL,				/* map_rgb_color - not used */\
	tiffsep_decode_color,		/* map_color_rgb */\
	NULL,				/* fill_rectangle */\
	NULL,				/* tile_rectangle */\
	NULL,				/* copy_mono */\
	NULL,				/* copy_color */\
	NULL,				/* draw_line */\
	NULL,				/* get_bits */\
	tiffsep_get_params,		/* get_params */\
	put_params,			/* put_params */\
	NULL,				/* map_cmyk_color - not used */\
	NULL,				/* get_xfont_procs */\
	NULL,				/* get_xfont_device */\
	NULL,				/* map_rgb_alpha_color */\
	gx_page_device_get_page_device,	/* get_page_device */\
	NULL,				/* get_alpha_bits */\
	NULL,				/* copy_alpha */\
	NULL,				/* get_band */\
	NULL,				/* copy_rop */\
	fill_path,			/* fill_path */\
	NULL,				/* stroke_path */\
	NULL,				/* fill_mask */\
	NULL,				/* fill_trapezoid */\
	NULL,				/* fill_parallelogram */\
	NULL,				/* fill_triangle */\
	NULL,				/* draw_thin_line */\
	NULL,				/* begin_image */\
	NULL,				/* image_data */\
	NULL,				/* end_image */\
	NULL,				/* strip_tile_rectangle */\
	NULL,				/* strip_copy_rop */\
	NULL,				/* get_clipping_box */\
	NULL,				/* begin_typed_image */\
	NULL,				/* get_bits_rectangle */\
	NULL,				/* map_color_rgb_alpha */\
	NULL,				/* create_compositor */\
	NULL,				/* get_hardware_params */\
	NULL,				/* text_begin */\
	NULL,				/* finish_copydevice */\
	NULL,				/* begin_transparency_group */\
	NULL,				/* end_transparency_group */\
	NULL,				/* begin_transparency_mask */\
	NULL,				/* end_transparency_mask */\
	NULL,				/* discard_transparency_layer */\
	tiffsep_get_color_mapping_procs,/* get_color_mapping_procs */\
	tiffsep_get_color_comp_index,	/* get_color_comp_index */\
	encode_color,			/* encode_color */\
	decode_color,			/* decode_color */\
	NULL,				/* pattern_manage */\
	NULL,				/* fill_rectangle_hl_color */\
	NULL,				/* include_color_space */\
	NULL,				/* fill_linear_color_scanline */\
	NULL,				/* fill_linear_color_trapezoid */\
	NULL,				/* fill_linear_color_triangle */\
	update_spot_colors,		/* update_spot_equivalent_colors */\
	tiffsep_ret_devn_params		/* ret_devn_params */\
}


#define tiffsep_devices_body(dtype, procs, dname, ncomp, pol, depth, mg, mc, sl, cn, print_page, compr)\
    std_device_full_body_type_extended(dtype, &procs, dname,\
	  &st_tiffsep_device,\
	  (int)((long)(DEFAULT_WIDTH_10THS) * (X_DPI) / 10),\
	  (int)((long)(DEFAULT_HEIGHT_10THS) * (Y_DPI) / 10),\
	  X_DPI, Y_DPI,\
    	  ncomp,		/* MaxComponents */\
	  ncomp,		/* NumComp */\
	  pol,			/* Polarity */\
	  depth, 0,		/* Depth, GrayIndex */\
	  mg, mc,		/* MaxGray, MaxColor */\
	  mg + 1, mc + 1,	/* DitherGray, DitherColor */\
	  sl,			/* Linear & Separable? */\
	  cn,			/* Process color model name */\
	  0, 0,			/* offsets */\
	  0, 0, 0, 0		/* margins */\
	),\
	prn_device_body_rest_(print_page),\
	{ 0 },			/* tiff state for separation files */\
	{ 0 },			/* separation files */\
	arch_is_big_endian      /* true = big endian; false = little endian */,\
	compr			/* COMPRESSION_* */,\
	TIFF_DEFAULT_STRIP_SIZE	/* MaxStripSize */

/*
 * Select the default number of components based upon the number of bits
 * that we have in a gx_color_index.  If we have 64 bits then we can compress
 * the colorant data.  This allows us to handle more colorants.  However the
 * compressed encoding is not separable.  If we do not have 64 bits then we
 * use a simple non-compressable encoding.
 */
#if USE_COMPRESSED_ENCODING
#   define NC GX_DEVICE_COLOR_MAX_COMPONENTS 
#   define SL GX_CINFO_SEP_LIN_NONE
#   define ENCODE_COLOR tiffsep_encode_compressed_color
#   define DECODE_COLOR tiffsep_decode_compressed_color
#else
#   define NC ARCH_SIZEOF_GX_COLOR_INDEX
#   define SL GX_CINFO_SEP_LIN
#   define ENCODE_COLOR tiffsep_encode_color
#   define DECODE_COLOR tiffsep_decode_color
#endif
#define GCIB (ARCH_SIZEOF_GX_COLOR_INDEX * 8)

/*
 * TIFF devices with CMYK process color model and spot color support.
 */
static const gx_device_procs spot_cmyk_procs =
		sep_device_procs(tiffsep_prn_open, tiffsep_prn_close, ENCODE_COLOR, DECODE_COLOR,
				tiffsep_update_spot_equivalent_colors, tiffsep_put_params, NULL);

static const gx_device_procs spot1_cmyk_procs =
		sep_device_procs(tiffsep1_prn_open, tiffsep1_prn_close, ENCODE_COLOR, DECODE_COLOR,
				tiffsep_update_spot_equivalent_colors, tiffsep1_put_params, sep1_fill_path);

const tiffsep_device gs_tiffsep_device =
{   
    tiffsep_devices_body(tiffsep_device, spot_cmyk_procs, "tiffsep", NC, GX_CINFO_POLARITY_SUBTRACTIVE, GCIB, MAX_COLOR_VALUE, MAX_COLOR_VALUE, SL, "DeviceCMYK", tiffsep_print_page, COMPRESSION_LZW),
    /* devn_params specific parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    { true },			/* equivalent CMYK colors for spot colors */
};

const tiffsep1_device gs_tiffsep1_device =
{   
    tiffsep_devices_body(tiffsep1_device, spot1_cmyk_procs, "tiffsep1", NC, GX_CINFO_POLARITY_SUBTRACTIVE, GCIB, MAX_COLOR_VALUE, MAX_COLOR_VALUE, SL, "DeviceCMYK", tiffsep1_print_page, COMPRESSION_CCITTFAX4),
    /* devn_params specific parameters */
    { 1,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    { true },			/* equivalent CMYK colors for spot colors */
    { {0} },			/* threshold arrays */
    0, 				/* fill_path */
};

#undef NC
#undef SL
#undef ENCODE_COLOR
#undef DECODE_COLOR

static const uint32_t bit_order[32]={
#if arch_is_big_endian
	0x80000000, 0x40000000, 0x20000000, 0x10000000,	0x08000000, 0x04000000, 0x02000000, 0x01000000,
	0x00800000, 0x00400000, 0x00200000, 0x00100000,	0x00080000, 0x00040000, 0x00020000, 0x00010000,
	0x00008000, 0x00004000, 0x00002000, 0x00001000,	0x00000800, 0x00000400, 0x00000200, 0x00000100,
	0x00000080, 0x00000040, 0x00000020, 0x00000010,	0x00000008, 0x00000004, 0x00000002, 0x00000001 
#else
	0x00000080, 0x00000040, 0x00000020, 0x00000010,	0x00000008, 0x00000004, 0x00000002, 0x00000001, 
	0x00008000, 0x00004000, 0x00002000, 0x00001000,	0x00000800, 0x00000400, 0x00000200, 0x00000100,
	0x00800000, 0x00400000, 0x00200000, 0x00100000,	0x00080000, 0x00040000, 0x00020000, 0x00010000,
	0x80000000, 0x40000000, 0x20000000, 0x10000000,	0x08000000, 0x04000000, 0x02000000, 0x01000000 
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
    int * map = ((tiffsep_device *) dev)->devn_params.separation_order_map;

    cmyk_cs_to_devn_cm(dev, map, c, m, y, k, out);
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
 * With 64 bit gx_color_index values, we compress the colorant values.  This
 * allows us to handle more than 8 colorants.
 */
static gx_color_index
tiffsep_encode_compressed_color(gx_device *dev, const gx_color_value colors[])
{
    return devn_encode_compressed_color(dev, colors, &(((tiffsep_device *)dev)->devn_params));
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 * With 64 bit gx_color_index values, we compress the colorant values.  This
 * allows us to handle more than 8 colorants.
 */
static int
tiffsep_decode_compressed_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    return devn_decode_compressed_color(dev, color, out,
		    &(((tiffsep_device *)dev)->devn_params));
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
 * With 32 bit gx_color_index values, we simply pack values.
 */
static gx_color_index
tiffsep_encode_color(gx_device *dev, const gx_color_value colors[])
{
    int bpc = ((tiffsep_device *)dev)->devn_params.bitspercomponent;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_index color = 0;
    int i = 0;
    int ncomp = dev->color_info.num_components;

    for (; i < ncomp; i++) {
	color <<= bpc;
        color |= (colors[i] >> drop);
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
    
    return devn_printer_put_params(pdev, plist,
		&(pdevn->devn_params), &(pdevn->equiv_cmyk_colors));
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
static void build_comp_to_sep_map(tiffsep_device *, short *);
static int number_output_separations(int, int, int, int);
static int create_separation_file_name(tiffsep_device *, char *, uint, int, bool);
static byte * threshold_from_order( gx_ht_order *, int *, int *, gs_memory_t *);
static int sep1_ht_order_to_thresholds(gx_device *pdev, const gs_imager_state *pis);
static void sep1_free_thresholds(tiffsep1_device *);
dev_proc_fill_path(clist_fill_path);

/* Open the tiffsep device */
/* Save the fill_hl_color and fill_path procs around the gdev_prn_open */
int
tiffsep1_prn_open(gx_device * pdev)
{
    tiffsep1_device * const tfdev = (tiffsep1_device *)pdev;
    int code = gdev_prn_open(pdev);

#if !USE_COMPRESSED_ENCODING
    /*
     * If we are using the compressed encoding scheme, then set the separable
     * and linear info.
     */
    set_linear_color_bits_mask_shift(pdev);
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
#endif
    /* gdev_prn_open may have changed the fill_path proc -- we need it set to ours */
    if (pdev->procs.fill_path != sep1_fill_path) {
        tfdev->fill_path = pdev->procs.fill_path;
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
    code = gx_parse_output_file_name(&parsed, &fmt,
		    			tfdev->fname, strlen(tfdev->fname));
    if (code < 0)
	return code;

    /* If we are doing separate pages, delete the old default file */
    if (parsed.iodev == iodev_default) {		/* filename includes "%nnd" */
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

    /*
     * We allow more spot colors than we can image.  This allows the user
     * to obtain separations for more than our max of 8 by doing multiple
     * passes.
     */
    return devn_get_color_comp_index(dev,
		&(pdev->devn_params), &(pdev->equiv_cmyk_colors),
		pname, name_size, component_type, ALLOW_EXTRA_SPOT_COLORS);
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
    buffer[sep_size] = 0;	/* Terminate string */
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

/* Open the tiffsep device */
int
tiffsep_prn_open(gx_device * pdev)
{
    int code = gdev_prn_open(pdev);

#if !USE_COMPRESSED_ENCODING
    /*
     * If we are using the compressed encoding scheme, then set the separable
     * and linear info.
     */
    set_linear_color_bits_mask_shift(pdev);
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
#endif
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
    short map_comp_to_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int comp_num;
    int num_comp = number_output_separations(num_dev_comp, num_std_colorants,
					num_order, num_spot);

    if (pdevn->tiff_comp) {
	TIFFCleanup(pdevn->tiff_comp);
	pdevn->tiff_comp = NULL;
    }
    code = gdev_prn_close(pdev);
    if (code < 0)
	return code;

    build_comp_to_sep_map(pdevn, map_comp_to_sep);
    /* Close the separation files */
    for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
        if (pdevn->sep_file[comp_num] != NULL) {
	    int sep_num = map_comp_to_sep[comp_num];

	    code = create_separation_file_name(pdevn, name,
	            MAX_FILE_NAME_SIZE, sep_num, false);
	    if (code < 0)
		return code;
            code = tiffsep_close_sep_file(pdevn, name, comp_num);
	    if (code < 0)
		return code;
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
	short *map_comp_to_sep, cmyk_composite_map * cmyk_map)
{
    int comp_num;

    for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
	int sep_num = map_comp_to_sep[comp_num];

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
 * Build a CMYK equivalent to a raster line.
 */
static void
build_cmyk_raster_line(byte * src, byte * dest, int width,
	int num_comp, cmyk_composite_map * cmyk_map)
{
    int pixel, comp_num;
    uint temp, cyan, magenta, yellow, black;
    cmyk_composite_map * cmyk_map_entry;

    for (pixel = 0; pixel < width; pixel++) {
	cmyk_map_entry = cmyk_map;
	temp = *src++;
	cyan = cmyk_map_entry->c * temp;
	magenta = cmyk_map_entry->m * temp;
	yellow = cmyk_map_entry->y * temp;
	black = cmyk_map_entry->k * temp;
	cmyk_map_entry++;
	for (comp_num = 1; comp_num < num_comp; comp_num++) {
	    temp = *src++;
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
	    eprintf("sep1_order_to_thresholds: no dev_ht available\n");
	    return_error(gs_error_rangecheck);	/* error condition */
	}
	nc = pis->dev_ht->num_comp;
	for( j=0; j<nc; j++ ) {
	    d_order = &(pis->dev_ht->components[j].corder);
	    dptr = &(tfdev->thresholds[j]);
	    dptr->dstart = threshold_from_order( d_order, &(dptr->dwidth), &(dptr->dheight), mem);
	    if( dptr->dstart == NULL ) {
		eprintf("sep1_order_to_thresholds: conversion to thresholds failed.\n");
		return_error(gs_error_rangecheck);	/* error condition */
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
/*	This routine generates a threshold matrix for use in		*/
/*	the color dithering routine from the "order" info in		*/
/*	the current graphics state.					*/
/*									*/
/************************************************************************/

static byte*
threshold_from_order( gx_ht_order *d_order, int *Width, int *Height, gs_memory_t *memory)
{
   int i, j, l, prev_l;
   unsigned char *thresh;
   gx_ht_bit *bits = (gx_ht_bit *)d_order->bit_data;

#ifdef DEBUG
if ( gs_debug_c('h') ) {
       dprintf2("   width=%d, height=%d,",
	    d_order->width, d_order->height );
       dprintf2(" num_levels=%d, raster=%d\n",
	    d_order->num_levels, d_order->raster );
}
#endif

   thresh = (byte *)gs_malloc(memory, d_order->num_bits, 1,
                                  "tiffsep1_threshold_array");
   if( thresh == NULL ) {
#ifdef DEBUG
      eprintf("threshold_from_order, malloc failed\n");
      eprintf2("   width=%d, height=%d,",
	      d_order->width, d_order->height );
      eprintf2(" num_levels=%d, raster=%d\n",
	      d_order->num_levels, d_order->raster );
#endif
 	return thresh ;		/* error if allocation failed	*/
   }
   for( i=0; i<d_order->num_bits; i++ )
      thresh[i] = 1;

   *Width  = d_order->width;
   *Height = d_order->height;

   prev_l = 0;
   l = 1;
   while( l < d_order->num_levels ) {
      if( d_order->levels[l] > d_order->levels[prev_l] ) {
	 int t_level = (256*l)/d_order->num_levels;
	 int row, col;
#ifdef DEBUG
	 if ( gs_debug_c('h') )
	    dprintf2("	level[%3d]=%3d\n", l, d_order->levels[l]);
#endif
	 for( j=d_order->levels[prev_l]; j<d_order->levels[l]; j++) {
#ifdef DEBUG
	    if ( gs_debug_c('h') )
	       dprintf2("	bits.offset=%3d, bits.mask=%8x	",
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
	    if( col < (int)d_order->width )
	       *(thresh+col+(row * d_order->width)) = t_level;
	 }
	 prev_l = l;
      }
      l++;
   }

#ifdef DEBUG
   if ( gs_debug_c('h') ) {
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
    int num_comp, comp_num, sep_num, code = 0;
    short map_comp_to_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    cmyk_composite_map cmyk_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
    char name[MAX_FILE_NAME_SIZE];
    int base_filename_length = length_base_file_name(tfdev);
    int save_depth = pdev->color_info.depth;
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int non_encodable_count = 0;

    build_comp_to_sep_map(tfdev, map_comp_to_sep);

    /* Print the names of the spot colors */
    for (sep_num = 0; sep_num < num_spot; sep_num++) {
        copy_separation_name(tfdev, name, 
	    MAX_FILE_NAME_SIZE - base_filename_length - SUFFIX_SIZE, sep_num);
	dlprintf1("%%%%SeparationName: %s\n", name);
    }

    /*
     * Check if the file name has a numeric format.  If so then we want to
     * create individual separation files for each page of the input.
    */
    code = gx_parse_output_file_name(&parsed, &fmt,
		    			tfdev->fname, strlen(tfdev->fname));
    
    /* Write the page directory for the CMYK equivalent file. */
    pdev->color_info.depth = 32;	/* Create directory for 32 bit cmyk */
    if (pdev->height > (max_long - ftell(file))/(pdev->width*4)) /* note width is never 0 in print_page */
	return_error(gs_error_rangecheck);  /* this will overflow max_long */

    if (gdev_prn_file_is_new(pdev)) {
	tfdev->tiff_comp = tiff_from_filep(pdev->dname, file, tfdev->BigEndian);
	if (!tfdev->tiff_comp)
	    return_error(gs_error_invalidfileaccess);
    }
    code = tiff_set_fields_for_printer(pdev, tfdev->tiff_comp);
    tiff_set_cmyk_fields(pdev, tfdev->tiff_comp, 8, COMPRESSION_NONE, tfdev->MaxStripSize);
    pdev->color_info.depth = save_depth;
    if (code < 0)
	return code;

    /* Set up the separation output files */
    num_comp = number_output_separations( tfdev->color_info.num_components,
					num_std_colorants, num_order, num_spot);
    for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
	int sep_num = map_comp_to_sep[comp_num];
	
	code = create_separation_file_name(tfdev, name, MAX_FILE_NAME_SIZE,
					    sep_num, false);
	if (code < 0)
	    return code;
	/*
	 * Close the old separation file if we are creating individual files
	 * for each page.
	 */
	if (tfdev->sep_file[comp_num] != NULL && fmt != NULL) {
            code = tiffsep_close_sep_file(tfdev, name, comp_num);
	    if (code < 0)
		return code;
	}
	/* Open the separation file, if not already open */
	if (tfdev->sep_file[comp_num] == NULL) {
	    code = gx_device_open_output_file((gx_device *)pdev, name,
		    true, false, &(tfdev->sep_file[comp_num]));
	    if (code < 0)
	        return code;
            tfdev->tiff[comp_num] = tiff_from_filep(name,
						    tfdev->sep_file[comp_num],
						    tfdev->BigEndian);
	    if (!tfdev->tiff[comp_num])
		return_error(gs_error_ioerror);
	}

	pdev->color_info.depth = 8;	/* Create files for 8 bit gray */
	if (pdev->height > (max_long - ftell(file))/(pdev->width)) /* note width is never 0 in print_page */
	    return_error(gs_error_rangecheck);  /* this will overflow max_long */

	code = tiff_set_fields_for_printer(pdev, tfdev->tiff[comp_num]);
	tiff_set_gray_fields(pdev, tfdev->tiff[comp_num], 8, tfdev->Compression, tfdev->MaxStripSize);
	pdev->color_info.depth = save_depth;
        if (code < 0)
	    return code;
    }

    build_cmyk_map(tfdev, num_comp, map_comp_to_sep, cmyk_map);

    {
        int raster = gdev_prn_raster(pdev);
	int width = tfdev->width;
	int cmyk_raster = width * NUM_CMYK_COMPONENTS;
	int pixel, y;
	byte * line = gs_alloc_bytes(pdev->memory, raster, "tiffsep_print_page");
	byte * unpacked = gs_alloc_bytes(pdev->memory, width * num_comp,
							"tiffsep_print_page");
	byte * sep_line;
	byte * row;

	if (line == NULL || unpacked == NULL)
	    return_error(gs_error_VMerror);
	sep_line =
	    gs_alloc_bytes(pdev->memory, cmyk_raster, "tiffsep_print_page");
	if (sep_line == NULL) {
	    gs_free_object(pdev->memory, line, "tiffsep_print_page");
	    return_error(gs_error_VMerror);
	}

        /* Write the page data. */
	for (y = 0; y < pdev->height; ++y) {
	    code = gdev_prn_get_bits(pdev, y, line, &row);
	    if (code < 0)
		break;
	    /* Unpack the encoded color info */
	    non_encodable_count += devn_unpack_row((gx_device *)pdev, num_comp,
			    &(tfdev->devn_params), width, row, unpacked);
	    /* Write separation data (tiffgray format) */
            for (comp_num = 0; comp_num < num_comp; comp_num++ ) {
		byte * src = unpacked + comp_num;
		byte * dest = sep_line;
		
		for (pixel = 0; pixel < width; pixel++, dest++, src += num_comp)
		    *dest = MAX_COLOR_VALUE - *src;    /* Gray is additive */
		TIFFWriteScanline(tfdev->tiff[comp_num], (tdata_t)sep_line, y, 0);
	    }
	    /* Write CMYK equivalent data (tiff32nc format) */
	    build_cmyk_raster_line(unpacked, sep_line,
			    		width, num_comp, cmyk_map);
	    TIFFWriteScanline(tfdev->tiff_comp, (tdata_t)sep_line, y, 0);
	}
	for (comp_num = 0; comp_num < num_comp; comp_num++ )
	    TIFFWriteDirectory(tfdev->tiff[comp_num]);
	TIFFWriteDirectory(tfdev->tiff_comp);

	gs_free_object(pdev->memory, line, "tiffsep_print_page");
	gs_free_object(pdev->memory, sep_line, "tiffsep_print_page");
    }

#if defined(DEBUG) && 0
    print_compressed_color_list(tfdev->devn_params.compressed_color_list,
		    			max(16, num_comp));
#endif

    /*
     * If we have any non encodable pixels then signal an error.
     */
    if (non_encodable_count) {
        dlprintf1("WARNING:  Non encodable pixels = %d\n", non_encodable_count);
	return_error(gs_error_rangecheck);
    }

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
    int num_comp, comp_num, code = 0;
    short map_comp_to_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    char name[MAX_FILE_NAME_SIZE];
    int save_depth = pdev->color_info.depth;
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
    code = gx_parse_output_file_name(&parsed, &fmt,
		    			pdev->fname, strlen(pdev->fname));

    /* If the output file is on disk and the name contains a page #, */
    /* then delete the previous file. */
    if (pdev->file != NULL && parsed.iodev == iodev_default && fmt) {
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
	/*
	 * Close the old separation file if we are creating individual files
	 * for each page.
	 */
	if (tfdev->sep_file[comp_num] != NULL && fmt != NULL) {
	    code = gx_device_close_output_file((const gx_device *)tfdev, name,
			    		tfdev->sep_file[comp_num]);
	    if (code < 0)
		return code;
	    tfdev->sep_file[comp_num] = NULL;
	    if (tfdev->tiff[comp_num]) {
		TIFFCleanup(tfdev->tiff[comp_num]);
		tfdev->tiff[comp_num] = NULL;
	    }
	}
	/* Open the separation file, if not already open */
	if (tfdev->sep_file[comp_num] == NULL) {
	    code = gx_device_open_output_file((gx_device *)pdev, name,
		    true, false, &(tfdev->sep_file[comp_num]));
	    if (code < 0)
	        return code;
	    tfdev->tiff[comp_num] = tiff_from_filep(name,
						    tfdev->sep_file[comp_num],
						    tfdev->BigEndian);
	    if (!tfdev->tiff[comp_num])
		return_error(gs_error_ioerror);
	}

	pdev->color_info.depth = 8;	/* Create files for 8 bit gray */
	code = tiff_set_fields_for_printer(pdev, tfdev->tiff[comp_num]);
	tiff_set_gray_fields(pdev, tfdev->tiff[comp_num], 1, tfdev->Compression, tfdev->MaxStripSize);
	pdev->color_info.depth = save_depth;
        if (code < 0)
	    return code;

    }	/* end initialization of separation files */


    {	/* Get the expanded contone line, halftone and write out the dithered separations */
        int raster = gdev_prn_raster(pdev);
	int width = tfdev->width;
	int dithered_raster = ((7 + width) / 8) + ARCH_SIZEOF_LONG;
	int pixel, y;
	byte *line = gs_alloc_bytes(pdev->memory, raster, "tiffsep1_print_page");
	byte *unpacked = gs_alloc_bytes(pdev->memory, width * num_comp,
							"tiffsep1_print_page");
	/* the dithered_line is assumed to be 32-bit aligned by the alloc */
	uint32_t *dithered_line = (uint32_t *)gs_alloc_bytes(pdev->memory, dithered_raster,
				"tiffsep1_print_page");
	byte *row;

	if (line == NULL || unpacked == NULL || dithered_line == NULL)
	    return_error(gs_error_VMerror);

        /* Loop for the lines */
	for (y = 0; y < pdev->height; ++y) {
	    code = gdev_prn_get_bits(pdev, y, line, &row);
	    if (code < 0)
		break;
	    /* Unpack the encoded color info */
	    non_encodable_count += devn_unpack_row((gx_device *)pdev, num_comp,
			    &(tfdev->devn_params), width, row, unpacked);
	    /* Dither the separation and write it out (tiffpack format) */
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
 *
 * The compressed color encoding has a much more significant impact on
 * the performance. On a 33 file test suite with spot colors, uncompressed
 * encoding (which is limited to CMYK + 4 spot colors) ran almost twice
 * the speed of compressed color encoding.
 */
#define USE_32_BIT_WRITES
		byte *thresh_line_base = tfdev->thresholds[comp_num].dstart +
				    ((y % tfdev->thresholds[comp_num].dheight) *
					tfdev->thresholds[comp_num].dwidth) ;
		byte *thresh_ptr = thresh_line_base;
		byte *thresh_limit = thresh_ptr + tfdev->thresholds[comp_num].dwidth;
		byte *src = unpacked + comp_num;
#ifdef USE_32_BIT_WRITES
		uint32_t *dest = dithered_line;
		uint32_t val = 0;
		uint32_t *mask = &bit_order[0];
#else	/* example 8-bit code */
		byte *dest = dithered_line;
		byte val = 0;
		byte mask = 0x80;
#endif /* USE_32_BIT_WRITES */
		
		for (pixel = 0; pixel < width; pixel++, src += num_comp) {
#ifdef USE_32_BIT_WRITES
		    if (*src < *thresh_ptr++)
		        val |= *mask;
		    if (++mask == &(bit_order[32])) {
			*dest++ = val;
			val = 0;
			mask = &bit_order[0];
		    }
#else	/* example 8-bit code */
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
#else	/* example 8-bit code */
		if (mask != 0x80) {
		    *dest = val;
		}
#endif /* USE_32_BIT_WRITES */
#endif /* SKIP_HALFTONING_FOR_TIMING */
		TIFFWriteScanline(tfdev->tiff[comp_num], (tdata_t)dithered_line, y, 0);
	    } /* end component loop */
	}
	/* Update the strip data */
        for (comp_num = 0; comp_num < num_comp; comp_num++ )
	    TIFFWriteDirectory(tfdev->tiff[comp_num]);
	gs_free_object(pdev->memory, line, "tiffsep1_print_page");
	gs_free_object(pdev->memory, dithered_line, "tiffsep1_print_page");
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
