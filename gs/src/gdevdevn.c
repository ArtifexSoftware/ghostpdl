/* Copyright (C) 1991, 1995 to 1999 Aladdin Enterprises. 2001 ArtifexSoftware Inc.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/*$Id$ */
/* Example DeviceN process color model devices. */
#include "math_.h"
#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"
#include "gdevdcrd.h"
#include "gstypes.h"
#include "gxdcconv.h"

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
private dev_proc_get_params(devicen_get_params);
private dev_proc_put_params(devicen_put_params);
private dev_proc_print_page(devicen_print_page);
private dev_proc_get_color_mapping_procs(get_spotrgb_color_mapping_procs);
private dev_proc_get_color_mapping_procs(get_spotcmyk_color_mapping_procs);
private dev_proc_get_color_mapping_procs(get_devicen_color_mapping_procs);
private dev_proc_get_color_comp_index(devicen_get_color_comp_index);
private dev_proc_encode_color(devicen_encode_color);
private dev_proc_decode_color(devicen_decode_color);

/*
 * Type definitions associated with the fixed color model names.
 */
typedef const char * fixed_colorant_name;
typedef fixed_colorant_name fixed_colorant_names_list[];

/*
 * Structure for holding SeparationNames and SeparationOrder elements.
 */
typedef struct gs_separation_names_s {
    int num_names;
    const gs_param_string * names[GX_DEVICE_COLOR_MAX_COMPONENTS];
} gs_separation_names;

/*
 * A structure definition for a DeviceN type device
 */
typedef struct devicen_device_s {
    gx_device_common;
    gx_prn_device_common;

    /*        ... device-specific parameters ... */

    /*
     * Bits per component (device colorant).  Currently only 1 and 8 are
     * supported.
     */
    int bitspercomponent;

    /*
     * Pointer to the colorant names for the color model.  This will be
     * null if we have DeviceN type device.  The actual possible colorant
     * names are those in this list plus those in the separation_names
     * list (below).
     */
    const fixed_colorant_names_list * std_colorant_names;
    const int num_std_colorant_names;	/* Number of names in list */

    /*
    * Separation names (if any).
    */
    gs_separation_names separation_names;

    /*
     * Separation Order (if specified).
     */
    gs_separation_names separation_order;

} devicen_device;

/*
 * Macro definition for DeviceN procedures
 */
#define device_procs(get_color_mapping_procs)\
{	gdev_prn_open,\
	gx_default_get_initial_matrix,\
	NULL,				/* sync_output */\
	gdev_prn_output_page,		/* output_page */\
	gdev_prn_close,			/* close */\
	NULL,				/* map_rgb_color - not used */\
	NULL,				/* map_color_rgb - not used */\
	NULL,				/* fill_rectangle */\
	NULL,				/* tile_rectangle */\
	NULL,				/* copy_mono */\
	NULL,				/* copy_color */\
	NULL,				/* draw_line */\
	NULL,				/* get_bits */\
	devicen_get_params,		/* get_params */\
	devicen_put_params,		/* put_params */\
	NULL,				/* map_cmyk_color - not used */\
	NULL,				/* get_xfont_procs */\
	NULL,				/* get_xfont_device */\
	NULL,				/* map_rgb_alpha_color */\
	gx_page_device_get_page_device,	/* get_page_device */\
	NULL,				/* get_alpha_bits */\
	NULL,				/* copy_alpha */\
	NULL,				/* get_band */\
	NULL,				/* copy_rop */\
	NULL,				/* fill_path */\
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
	get_color_mapping_procs,	/* get_color_mapping_procs */\
	devicen_get_color_comp_index,	/* get_color_comp_index */\
	devicen_encode_color,		/* encode_color */\
	devicen_decode_color		/* decode_color */\
}


private const fixed_colorant_names_list DeviceRGBComponents = {
	"Red",
	"Green",
	"Blue",
	0		/* List terminator */
};

private const fixed_colorant_names_list DeviceCMYKComponents = {
	"Cyan",
	"Magenta",
	"Yellow",
	"Black",
	0		/* List terminator */
};


/*
 * Example device with RGB and spot color support
 */
private const gx_device_procs spot_rgb_procs = device_procs(get_spotrgb_color_mapping_procs);

const devicen_device gs_spotrgb_device =
{   
    prn_device_body_extended(devicen_device, spot_rgb_procs, "spotrgb",
	 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	 X_DPI, Y_DPI,		/* X and Y hardware resolution */
	 0, 0, 0, 0,		/* margins */
    	 GX_DEVICE_COLOR_MAX_COMPONENTS, 3,	/* MaxComponents, NumComp */
	 GX_CINFO_POLARITY_ADDITIVE,		/* Polarity */
	 24, 0,			/* Depth, Gray_index, */
	 255, 255, 256, 256,	/* MaxGray, MaxColor, DitherGray, DitherColor */
	 GX_CINFO_SEP_LIN,      /* Linear & Separable */
	 "DeviceN",		/* Process color model name */
	 devicen_print_page),	/* Printer page print routine */
    /* DeviceN device specific parameters */
    8,				/* Bits per color - must match ncomp, depth, etc. above */
    (&DeviceRGBComponents),	/* Names of color model colorants */
    3,				/* Number colorants for RGB */
    {0},			/* SeparationNames */
    {0}				/* SeparationOrder names */
};

/*
 * Example device with CMYK and spot color support
 */
private const gx_device_procs spot_cmyk_procs = device_procs(get_spotcmyk_color_mapping_procs);

const devicen_device gs_spotcmyk_device =
{   
    prn_device_body_extended(devicen_device, spot_cmyk_procs, "spotcmyk",
	 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	 X_DPI, Y_DPI,		/* X and Y hardware resolution */
	 0, 0, 0, 0,		/* margins */
    	 GX_DEVICE_COLOR_MAX_COMPONENTS, 4,	/* MaxComponents, NumComp */
	 GX_CINFO_POLARITY_SUBTRACTIVE,		/* Polarity */
	 4, 0,			/* Depth, Gray_index, */
	 1, 1, 2, 2,		/* MaxGray, MaxColor, DitherGray, DitherColor */
	 GX_CINFO_SEP_LIN,      /* Linear & Separable */
	 "DeviceN",		/* Process color model name */
	 devicen_print_page),	/* Printer page print routine */

    /* DeviceN device specific parameters */
    1,				/* Bits per color - must match ncomp, depth, etc. above */
    (&DeviceCMYKComponents),	/* Names of color model colorants */
    4,				/* Number colorants for CMYK */
    {0},			/* SeparationNames */
    {0}				/* SeparationOrder names */
};

/*
 * Example DeviceN color device
 */
private const gx_device_procs devicen_procs = device_procs(get_devicen_color_mapping_procs);

const devicen_device gs_devicen_device =
{   
    prn_device_body_extended(devicen_device, devicen_procs, "devicen",
	 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	 X_DPI, Y_DPI,		/* X and Y hardware resolution */
	 0, 0, 0, 0,		/* margins */
	 GX_DEVICE_COLOR_MAX_COMPONENTS, 1,	/* MaxComponents, NumComp */
				/* Note: We start with at least one component */
	 GX_CINFO_POLARITY_ADDITIVE,		/* Polarity */
	 8, 0,			/* Depth, Gray_index, */
	 255, 255, 1, 1,	/* MaxGray, MaxColor, DitherGray, DitherColor */
	 GX_CINFO_SEP_LIN,      /* Linear & Seperable */
	 "DeviceN",		/* Process color model name */
				/* Note: We start with at least one component */
	 devicen_print_page),	/* Printer page print routine */
    /* DeviceN device specific parameters */
    8,				/* Bits per color - must match ncomp, depth, etc. above */
    NULL		,	/* No names for standard DeviceN color model */
    0,				/* No standarad colorants for DeviceN */
    {0},			/* SeparationNames */
    {0}				/* SeparationOrder names */
};


/*
 * The following procedures are used to map the standard color spaces into
 * the color components for the spotrgb device.
 */
private void
gray_cs_to_spotrgb_cm(gx_device * dev, frac gray, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((devicen_device *)dev)->separation_names.num_names;

    out[0] = out[1] = out[2] = gray;
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
}

private void
rgb_cs_to_spotrgb_cm(gx_device * dev, const gs_imager_state *pis,
				  frac r, frac g, frac b, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((devicen_device *)dev)->separation_names.num_names;

    out[0] = r;
    out[1] = g;
    out[2] = b;
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
}

private void
cmyk_cs_to_spotrgb_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((devicen_device *)dev)->separation_names.num_names;

    color_cmyk_to_rgb(c, m, y, k, NULL, out);
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
};

private void
gray_cs_to_spotcmyk_cm(gx_device * dev, frac gray, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((devicen_device *)dev)->separation_names.num_names;

    out[0] = out[1] = out[2] = frac_0;
    out[3] = frac_1 - gray;
    for(; i>0; i--)			/* Clear spot colors */
        out[3 + i] = 0;
}

private void
rgb_cs_to_spotcmyk_cm(gx_device * dev, const gs_imager_state *pis,
				   frac r, frac g, frac b, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((devicen_device *)dev)->separation_names.num_names;

    color_rgb_to_cmyk(r, g, b, pis, out);
    for(; i>0; i--)			/* Clear spot colors */
        out[3 + i] = 0;
}

private void
cmyk_cs_to_spotcmyk_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((devicen_device *)dev)->separation_names.num_names;

    out[0] = c;
    out[1] = m;
    out[2] = y;
    out[3] = k;
    for(; i>0; i--)			/* Clear spot colors */
        out[3 + i] = 0;
};

private const gx_cm_color_map_procs spotRGB_procs = {
    gray_cs_to_spotrgb_cm, rgb_cs_to_spotrgb_cm, cmyk_cs_to_spotrgb_cm
};

private const gx_cm_color_map_procs spotCMYK_procs = {
    gray_cs_to_spotcmyk_cm, rgb_cs_to_spotcmyk_cm, cmyk_cs_to_spotcmyk_cm
};

/*
 * These are the handlers for returning the list of color space
 * to color model conversion routines.
 */
private const gx_cm_color_map_procs *
get_spotrgb_color_mapping_procs(const gx_device * dev)
{
    return &spotRGB_procs;
}

private const gx_cm_color_map_procs *
get_spotcmyk_color_mapping_procs(const gx_device * dev)
{
    return &spotCMYK_procs;
}


private const gx_cm_color_map_procs *
get_devicen_color_mapping_procs(const gx_device * dev)
{
    return &spotRGB_procs;		/* For now - the DeviceN device = spotrgb */
}


/*
 * Encode a list of colorant values into a gx_color_index_value.
 */
private gx_color_index
devicen_encode_color(gx_device *dev, const gx_color_value colors[])
{
    int bpc = ((devicen_device *)dev)->bitspercomponent;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_index color = 0;
    int i = 0;
    int ncomp = dev->color_info.num_components;

    for (; i<ncomp; i++) {
	color <<= bpc;
        color |= (colors[i] >> drop);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 */
private int
devicen_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    int bpc = ((devicen_device *)dev)->bitspercomponent;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    int mask = (1 << bpc) - 1;
    int i = 0;
    int ncomp = dev->color_info.num_components;

    for (; i<ncomp; i++) {
        out[ncomp - i - 1] = (color & mask) << drop;
	color >>= bpc;
    }
    return 0;
}

/*
 * This routine will extract a specified set of bits from a buffer and pack
 * them into a given buffer.
 *
 * Parameters:
 *   source - The source of the data
 *   dest - The destination for the data
 *   depth - The size of the bits per pixel - must be a multiple of 8
 *   first_bit - The location of the first data bit (LSB).
 *   bit_width - The number of bits to be extracted.
 *   npixel - The number of pixels.
 *
 * Returns:
 *   Length of the output line (in bytes)
 *   Data in dest.
 */
private int
repack_data(byte * source, byte * dest, int depth, int first_bit,
		int bit_width, int npixel)
{
    int in_nbyte = depth >> 3;		/* Number of bytes per input pixel */
    int out_nbyte = bit_width >> 3;	/* Number of bytes per output pixel */
    gx_color_index mask = 1;
    gx_color_index data;
    int i, j, length = 0;
    byte temp;
    byte * out = dest;
    int in_bit_start = 8 - depth;
    int out_bit_start = 8 - bit_width;
    int in_byte_loc = in_bit_start, out_byte_loc = out_bit_start;

    mask = (mask << bit_width) - 1;
    for (i=0; i<npixel; i++) {
        /* Get the pixel data */
	if (!in_nbyte) {		/* Multiple pixels per byte */
	    data = *source;
	    data >>= in_byte_loc;
	    in_byte_loc -= depth;
	    if (in_byte_loc < 0) {	/* If finished with byte */
	        in_byte_loc = in_bit_start;
		source++;
	    }
	}
	else {				/* One or more bytes per pixel */
	    data = *source++;
	    for (j=1; j<in_nbyte; j++)
	        data = (data << 8) + *source++;
	}
	data >>= first_bit;
	data &= mask;

	/* Put the output data */
	if (!out_nbyte) {		/* Multiple pixels per byte */
	    temp = *out & ~(mask << out_byte_loc);
	    *out = temp | (data << out_byte_loc);
	    out_byte_loc -= bit_width;
	    if (out_byte_loc < 0) {	/* If finished with byte */
	        out_byte_loc = out_bit_start;
		out++;
	    }
	}
	else {				/* One or more bytes per pixel */
	    *out++ = data >> ((out_nbyte - 1) * 8);
	    for (j=1; j<out_nbyte; j++) {
	        *out++ = data >> ((out_nbyte - 1 - j) * 8);
	    }
	}
    }
    /* Return the number of bytes in the destination buffer. */
    if (out_byte_loc != out_bit_start) { 	/* If partially filled last byte */
	*out++ = *out & ((~0) << out_byte_loc);	/* Mask unused part of last byte */
    }
    length = out - dest;
    return length;
}

private void write_pcx_file(gx_device_printer * pdev, char * filename, int ncomp,
			    int bpc, int pcmlinelength);
/* 
 * This is an example print page routine for a DeviceN device.  This routine
 * will handle a DeviceN, a CMYK with spot colors, or an RGB with spot colors
 * process color model.
 *
 * This routine creates several output files.  If the process color model is
 * RGB or CMYK then a bit image file is created which contains the data for the
 * process color model data.  This data is put into the given file stream.
 * I.e. into the output file specified by the user.  This file is not created
 * for the DeviceN process color model.  A separate bit image file is created 
 * is created for the data for each of the given spot colors.  The names for
 * these files are created by taking the given output file name and appending
 * "sn" (where n is the spot color number 0 to ...) to the output file name.
 * The results are unknown if the output file is stdout etc.
 *
 * After the bit image files are created, then a set of PCX format files are
 * created from the bit image files.  This files have a ".pcx" appended to the
 * end of the files.  Thus a CMYK process color model with two spot colors
 * would end up with a total of six files being created.  (xxx, xxxs0, xxxs1,
 * xxx.pcx, xxxs0.pcx, and xxxs1.pcx).
 *
 * I do not assume that any users will actually want to create all of these
 * different files.  However I wanted to show an example of how each of the
 * spot * colorants could be unpacked from the process color model colorants.
 * The bit images files are any easy to show this without the complication of
 * trying to put the data into a specific format.  However I do not have a
 * tool which will display the bit image data directly so I needed to convert
 * it to a form which I can view.  Thus the PCX format files are being created.
 * Note:  The PCX implementation is not complete.  There are many (most)
 * of bis per pixel and number of colorants that are not supported.
 */
private int
devicen_print_page(gx_device_printer * pdev, FILE * prn_stream)
{
    int line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    byte *in = gs_alloc_bytes(pdev->memory, line_size, "devicen_print_page(in)");
    byte *buf = gs_alloc_bytes(pdev->memory, line_size + 3, "devicen_print_page(buf)");
    const devicen_device * pdevn = (devicen_device *) pdev;
    int npcmcolors = pdevn->num_std_colorant_names;
    int ncomp = pdevn->color_info.num_components;
    int depth = pdevn->color_info.depth;
    int nspot = pdevn->separation_names.num_names;
    int bpc = pdevn->bitspercomponent;
    int lnum = 0, bottom = pdev->height;
    int width = pdev->width;
    FILE * spot_file[GX_DEVICE_COLOR_MAX_COMPONENTS] = {0};
    int i, code = 0;
    int first_bit;
    int pcmlinelength, linelength[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte *data;
    char spotname[gp_file_name_sizeof];

    if (in == NULL || buf == NULL) {
	code = gs_error_VMerror;
	goto prn_done;
    }

    /* Open the output files for the spot colors */
    for(i=0; i<nspot; i++) {
	sprintf(spotname, "%ss%d", pdevn->fname, i);
        spot_file[i] = fopen(spotname, "wb");
	if (spot_file[i] == NULL) {
	    code = gs_error_VMerror;
	    goto prn_done;
	}
    }

    if (!npcmcolors)
	npcmcolors = nspot; /* Temp hack */

    /* Now create the output bit image files */
    for (; lnum < bottom; ++lnum) {
	gdev_prn_get_bits(pdev, lnum, in, &data);
        /* Now put the pcm data into the output file */
	if (npcmcolors) {
	    first_bit = bpc * (ncomp - npcmcolors);
	    pcmlinelength = repack_data(data, buf, depth, first_bit, bpc * npcmcolors, width);
	    fwrite(buf, 1, pcmlinelength, prn_stream);
	}
	/* Put spot color data into the output files */
        for (i=0; i<nspot; i++) {
	    first_bit = bpc * (nspot - 1 - i);
	    linelength[i] = repack_data(data, buf, depth, first_bit, bpc, width);
	    fwrite(buf, 1, linelength[i], spot_file[i]);
        }
    }

    /* Close the bit image files */
    for(i=0; i<nspot; i++) {
        fclose(spot_file[i]);
	spot_file[i] = NULL;
    }

    /* Now convert the bit image files into PCX files */
    if (npcmcolors)
	write_pcx_file(pdev, (char *) &pdevn->fname, npcmcolors, bpc, pcmlinelength);
    for(i=0; i<nspot; i++) {
	sprintf(spotname, "%ss%d", pdevn->fname, i);
	write_pcx_file(pdev, spotname, 1, bpc, linelength[i]);
    }


    /* Clean up and exit */
  prn_done:
    for(i=0; i<nspot; i++) {
	if (spot_file[i] != NULL)
            fclose(spot_file[i]);
    }
    if (in != NULL)
        gs_free_object(pdev->memory, in, "devicen_print_page(in)");
    if (buf != NULL)
        gs_free_object(pdev->memory, buf, "devicen_print_page(buf)");
    return code;
}

#define set_param_array(a, d, s)\
  (a.data = d, a.size = s, a.persistent = false);

/* Get parameters.  We provide a default CRD. */
private int
devicen_get_params(gx_device * pdev, gs_param_list * plist)
{
    int code;
    bool seprs = false;
    gs_param_string_array scna;

    set_param_array(scna, NULL, 0);

    if ( (code = gdev_prn_get_params(pdev, plist)) < 0 ||
         (code = sample_device_crd_get_params(pdev, plist, "CRDDefault")) < 0 ||
	 (code = param_write_name_array(plist, "SeparationColorNames", &scna)) < 0 ||
	 (code = param_write_bool(plist, "Separations", &seprs)) < 0)
	return code;

    return 0;
}
#undef set_param_array

#define compare_color_names(name, name_size, str, str_size) \
    (name_size == str_size && \
	(strncmp((const char *)name, (const char *)str, name_size) == 0))

/*
 * This routine will check if a name matches any item in a list of process model
 * color component names.
 */
private bool
check_process_color_names(const fixed_colorant_names_list * pcomp_list,
			  const gs_param_string * pstring)
{
    if (pcomp_list) {
        const fixed_colorant_name * plist = *pcomp_list;
        uint size = pstring->size;
    
	while( *plist) {
	    if (compare_color_names(*plist, strlen(*plist), pstring->data, size)) {
		return true;
	    }
	    plist++;
	}
    }
    return false;
}

/*
 * This utility routine calculates the number of bits required to store
 * color information.  In general the values are rounded up to an even
 * byte boundary except those cases in which mulitple pixels can evenly
 * into a single byte.
 *
 * The parameter are:
 *   ncomp - The number of components (colorants) for the device.  Valid
 * 	values are 1 to GX_DEVICE_COLOR_MAX_COMPONENTS
 *   bpc - The number of bits per component.  Valid values are 1, 2, 4, 5,
 *	and 8.
 * Input values are not tested for validity.
 */
static int
bpc_to_depth(int ncomp, int bpc)
{
    static const byte depths[4][8] = {
	{1, 2, 0, 4, 8, 0, 0, 8},
	{2, 4, 0, 8, 16, 0, 0, 16},
	{4, 8, 0, 16, 16, 0, 0, 24},
	{4, 8, 0, 16, 32, 0, 0, 32}
    };

    if (ncomp <=4 && bpc <= 8)
        return depths[ncomp -1][bpc-1];
    else
    	return (ncomp * bpc + 7) & 0xf8;
}

#define BEGIN_ARRAY_PARAM(pread, pname, pa, psize, e)\
    BEGIN\
    switch (code = pread(plist, (param_name = pname), &(pa))) {\
      case 0:\
	if ((pa).size != psize) {\
	  ecode = gs_note_error(gs_error_rangecheck);\
	  (pa).data = 0;	/* mark as not filled */\
	} else
#define END_ARRAY_PARAM(pa, e)\
	goto e;\
      default:\
	ecode = code;\
e:	param_signal_error(plist, param_name, ecode);\
      case 1:\
	(pa).data = 0;		/* mark as not filled */\
    }\
    END

/* Set parameters.  We allow setting the number of bits per component. */
private int
devicen_put_params(gx_device * pdev, gs_param_list * plist)
{
    devicen_device * const pdevn = (devicen_device *) pdev;
    gx_device_color_info save_info;
    gs_param_name param_name;
    int npcmcolors = pdevn->num_std_colorant_names;
    int bpc = pdevn->bitspercomponent;
    int num_spot = pdevn->separation_names.num_names;
    int v;
    int ecode = 0;
    int code;
    const char *vname;
    gs_param_string_array scna;

    BEGIN_ARRAY_PARAM(param_read_name_array, "SeparationColorNames", scna, scna.size, scne) {
	break;
    } END_ARRAY_PARAM(scna, scne);


    if ((code = param_read_int(plist, (vname = "GrayValues"), &v)) != 1 ||
	(code = param_read_int(plist, (vname = "RedValues"), &v)) != 1 ||
	(code = param_read_int(plist, (vname = "GreenValues"), &v)) != 1 ||
	(code = param_read_int(plist, (vname = "BlueValues"), &v)) != 1
	) {
	if (code < 0)
	    ecode = code;
	else
	    switch (v) {
		case   2: bpc = 1; break;
		case   4: bpc = 2; break;
		case  16: bpc = 4; break;
		case  32: bpc = 5; break;
		case 256: bpc = 8; break;
		default:
		    param_signal_error(plist, vname,
				       ecode = gs_error_rangecheck);
	    }
    }

    /*
     * Save the color_info in case gdev_prn_put_params fails, and for
     * comparison.
     */
    save_info = pdevn->color_info;
    /*
     * Process the separation color names.  Remove any names that already match
     * the process color model colorant names for the device.
     */
    if (scna.data != 0) {
	int i;
	int num_names = scna.size;
	const fixed_colorant_names_list * pcomp_names = 
	    ((devicen_device *)pdev)->std_colorant_names;

	for (i = num_spot = 0; i < num_names; i++) {
	    if (!check_process_color_names(pcomp_names, &scna.data[i]))
	        pdevn->separation_names.names[num_spot++] = &scna.data[i];
	}
	pdevn->separation_names.num_names = num_spot;
	if (pdevn->is_open)
	    gs_closedevice(pdev);
    }
    pdevn->bitspercomponent = bpc;
    pdevn->color_info.num_components = npcmcolors + num_spot;
    /* 
     * The DeviceN device can have zero components if nothing has been specified.
     * This causes some problems so force at least one component until something
     * is specified.
     */
    if (!pdevn->color_info.num_components)
	pdevn->color_info.num_components = 1;
    pdevn->color_info.depth = bpc_to_depth(pdevn->color_info.num_components, bpc);

    pdevn->color_info.max_gray = pdevn->color_info.max_color =
	(pdevn->color_info.dither_grays =
	 pdevn->color_info.dither_colors =
	 (1 << bpc)) - 1;
    ecode = gdev_prn_put_params(pdev, plist);
    if (ecode < 0) {
	pdevn->color_info = save_info;
	return ecode;
    }
    if (pdevn->color_info.depth != save_info.depth) {
	gs_closedevice(pdev);
    }
    return 0;
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
 * number if the name is found.  It returns a negative value if not found.
 */
private int
devicen_get_color_comp_index(const gx_device * dev, const char * pname, int name_size, int src_index)
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    const fixed_colorant_names_list * list = ((const devicen_device *)dev)->std_colorant_names;
    const fixed_colorant_name * pcolor = *list;
    int color_component_number = 0;
    int i;

    /* Check if the component is in the implied list. */
    if (pcolor) {
	while( *pcolor) {
	    if (compare_color_names(pname, name_size, *pcolor, strlen(*pcolor)))
		return color_component_number;
	    pcolor++;
	    color_component_number++;
	}
    }

    /* Check if the component is in the separation names list. */
    {
	const gs_separation_names * separations = &((const devicen_device *)dev)->separation_names;
	int num_spot = separations->num_names;

	for (i=0; i<num_spot; i++) {
	    if (compare_color_names((const char *)separations->names[i]->data,
		  separations->names[i]->size, pname, name_size)) {
		return color_component_number;
	    }
	    color_component_number++;
	}
    }

    return -1;
}


/*
 * We are using the PCX output format.  This is done for simplicity.
 * Much of the following code was copied from gdevpcx.c.
 */

/* ------ Private definitions ------ */

/* All two-byte quantities are stored LSB-first! */
#if arch_is_big_endian
#  define assign_ushort(a,v) a = ((v) >> 8) + ((v) << 8)
#else
#  define assign_ushort(a,v) a = (v)
#endif

typedef struct pcx_header_s {
    byte manuf;			/* always 0x0a */
    byte version;
#define version_2_5			0
#define version_2_8_with_palette	2
#define version_2_8_without_palette	3
#define version_3_0 /* with palette */	5
    byte encoding;		/* 1=RLE */
    byte bpp;			/* bits per pixel per plane */
    ushort x1;			/* X of upper left corner */
    ushort y1;			/* Y of upper left corner */
    ushort x2;			/* x1 + width - 1 */
    ushort y2;			/* y1 + height - 1 */
    ushort hres;		/* horz. resolution (dots per inch) */
    ushort vres;		/* vert. resolution (dots per inch) */
    byte palette[16 * 3];	/* color palette */
    byte reserved;
    byte nplanes;		/* number of color planes */
    ushort bpl;			/* number of bytes per line (uncompressed) */
    ushort palinfo;
#define palinfo_color	1
#define palinfo_gray	2
    byte xtra[58];		/* fill out header to 128 bytes */
} pcx_header;

/* Define the prototype header. */
private const pcx_header pcx_header_prototype =
{
    10,				/* manuf */
    0,				/* version (variable) */
    1,				/* encoding */
    0,				/* bpp (variable) */
    00, 00,			/* x1, y1 */
    00, 00,			/* x2, y2 (variable) */
    00, 00,			/* hres, vres (variable) */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* palette (variable) */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    0,				/* reserved */
    0,				/* nplanes (variable) */
    00,				/* bpl (variable) */
    00,				/* palinfo (variable) */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* xtra */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};


/* Forward declarations */
private void pcx_write_rle(const byte *, const byte *, int, FILE *);
private int pcx_write_page(gx_device_printer * pdev, FILE * infile,
    int linesize, FILE * outfile, pcx_header * phdr, bool planar, int depth);

static const byte pcx_cmyk_palette[16 * 3] =
{
    0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x0f, 0x0f, 0x00,
    0xff, 0x00, 0xff, 0x0f, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x0f, 0x00, 0x00,
    0x00, 0xff, 0xff, 0x00, 0x0f, 0x0f, 0x00, 0xff, 0x00, 0x00, 0x0f, 0x00,
    0x00, 0x00, 0xff, 0x00, 0x00, 0x0f, 0x1f, 0x1f, 0x1f, 0x0f, 0x0f, 0x0f,
};

static const byte pcx_ega_palette[16 * 3] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0x00, 0xaa, 0xaa,
    0xaa, 0x00, 0x00, 0xaa, 0x00, 0xaa, 0xaa, 0xaa, 0x00, 0xaa, 0xaa, 0xaa,
    0x55, 0x55, 0x55, 0x55, 0x55, 0xff, 0x55, 0xff, 0x55, 0x55, 0xff, 0xff,
    0xff, 0x55, 0x55, 0xff, 0x55, 0xff, 0xff, 0xff, 0x55, 0xff, 0xff, 0xff
};


/*
 * This routine will set up the revision and palatte for the output
 * file.
 *
 * Please note that this routine does not currently handle all possible
 * combinations of bits and planes.
 *
 * Input parameters:
 *   pdev - Pointer to device data structure
 *   file - output file
 *   header - The header structure to hold the data.
 *   bits_per_plane - The number of bits per plane.
 *   num_planes - The number of planes.
 */
private bool
setup_pcx_header(gx_device_printer * pdev, pcx_header * phdr, int num_planes, int bits_per_plane)
{
    bool planar;
 
    *phdr = pcx_header_prototype;
    phdr->bpp = bits_per_plane;
    phdr->nplanes = num_planes;

    switch (num_planes) {
        case 1:
	    switch (bits_per_plane) {
	        case 1:
    			phdr->version = version_2_8_with_palette;
    			assign_ushort(phdr->palinfo, palinfo_gray);
    			memcpy((byte *) phdr->palette, "\000\000\000\377\377\377", 6);
			planar = false;
			break;
		case 2:				/* Not defined */
			break;
		case 4:	
    			phdr->version = version_2_8_with_palette;
    			memcpy((byte *) phdr->palette, pcx_ega_palette, sizeof(pcx_ega_palette));
			planar = true;
			break;
		case 5:				/* Not defined */
			break;
		case 8:
    			phdr->version = version_3_0;
    			assign_ushort(phdr->palinfo, palinfo_gray);
			planar = false;
			break;
		case 16:			/* Not defined */
			break;
	    }
	    break;
	case 2:
	    switch (bits_per_plane) {
	        case 1:				/* Not defined */
			break;
		case 2:				/* Not defined */
			break;
		case 4:				/* Not defined */
			break;
		case 5:				/* Not defined */
			break;
		case 8:				/* Not defined */
			break;
		case 16:			/* Not defined */
			break;
	    }
	    break;
	case 3:
	    switch (bits_per_plane) {
	        case 1:				/* Not defined */
			break;
		case 2:				/* Not defined */
			break;
		case 4:				/* Not defined */
			break;
		case 5:				/* Not defined */
			break;
		case 8:
    			phdr->version = version_3_0;
    			assign_ushort(phdr->palinfo, palinfo_color);
			planar = true;
			break;
		case 16:			/* Not defined */
			break;
	    }
	    break;
	case 4:
	    switch (bits_per_plane) {
	        case 1:
    			phdr->version = 2;
    			memcpy((byte *) phdr->palette, pcx_cmyk_palette,
	   			sizeof(pcx_cmyk_palette));
			planar = false;
			phdr->bpp = 4;
			phdr->nplanes = 1;
			break;
		case 2:				/* Not defined */
			break;
		case 4:				/* Not defined */
			break;
		case 5:				/* Not defined */
			break;
		case 8:				/* Not defined */
			break;
		case 16:			/* Not defined */
			break;
	    }
	    break;
    }
    return planar;
}

/* Write a palette on a file. */
private int
pc_write_mono_palette(gx_device * dev, uint max_index, FILE * file)
{
    uint i, c;
    gx_color_value rgb[3];

    for (i = 0; i < max_index; i++) {
	rgb[0] = rgb[1] = rgb[2] = i << 8;
	for (c = 0; c < 3; c++) {
	    byte b = gx_color_value_to_byte(rgb[c]);

	    fputc(b, file);
	}
    }
    return 0;
}
/*
 * This routine will send any output data required at the end of a file
 * for a particular combination of planes and bits per plane.
 *
 * Please note that most combinations do not require anything at the end
 * of a data file.
 *
 * Input parameters:
 *   pdev - Pointer to device data structure
 *   file - output file
 *   header - The header structure to hold the data.
 *   bits_per_plane - The number of bits per plane.
 *   num_planes - The number of planes.
 */
private int
finish_pcx_file(gx_device_printer * pdev, FILE * file, pcx_header * header, int num_planes, int bits_per_plane)
{
    switch (num_planes) {
        case 1:
	    switch (bits_per_plane) {
	        case 1:				/* Do nothing */
			break;
		case 2:				/* Not defined */
			break;
		case 4:				/* Do nothing */
			break;
		case 5:				/* Not defined */
			break;
		case 8:
			fputc(0x0c, file);
			return pc_write_mono_palette((gx_device *) pdev, 256, file);
		case 16:			/* Not defined */
			break;
	    }
	    break;
	case 2:
	    switch (bits_per_plane) {
	        case 1:				/* Not defined */
			break;
		case 2:				/* Not defined */
			break;
		case 4:				/* Not defined */
			break;
		case 5:				/* Not defined */
			break;
		case 8:				/* Not defined */
			break;
		case 16:			/* Not defined */
			break;
	    }
	    break;
	case 3:
	    switch (bits_per_plane) {
	        case 1:				/* Not defined */
			break;
		case 2:				/* Not defined */
			break;
		case 4:				/* Not defined */
			break;
		case 5:				/* Not defined */
			break;
		case 8:				/* Do nothing */
			break;
		case 16:			/* Not defined */
			break;
	    }
	    break;
	case 4:
	    switch (bits_per_plane) {
	        case 1:				/* Do nothing */
			break;
		case 2:				/* Not defined */
			break;
		case 4:				/* Not defined */
			break;
		case 5:				/* Not defined */
			break;
		case 8:				/* Not defined */
			break;
		case 16:			/* Not defined */
			break;
	    }
	    break;
    }
    return 0;
}

/* Send the page to the printer. */
private void
write_pcx_file(gx_device_printer * pdev, char * filename, int ncomp,
			    int bpc, int linesize)
{
    pcx_header header;
    int code;
    bool planar;
    char outname[gp_file_name_sizeof];
    FILE * in;
    FILE * out;
    int depth = bpc_to_depth(ncomp, bpc);

    in = fopen(filename, "rb");
    sprintf(outname, "%s.pcx", filename);
    out = fopen(outname, "wb");

    planar = setup_pcx_header(pdev, &header, ncomp, bpc);
    code = pcx_write_page(pdev, in, linesize, out, &header, planar, depth);
    if (code >= 0)
        code = finish_pcx_file(pdev, out, &header, ncomp, bpc);

    fclose(in);
    fclose(out);
}

/* Write out a page in PCX format. */
/* This routine is used for all formats. */
/* The caller has set header->bpp, nplanes, and palette. */
private int
pcx_write_page(gx_device_printer * pdev, FILE * infile, int linesize, FILE * outfile,
	       pcx_header * phdr, bool planar, int depth)
{
    int raster = linesize;
    uint rsize = ROUND_UP((pdev->width * phdr->bpp + 7) >> 3, 2);	/* PCX format requires even */
    int height = pdev->height;
    uint lsize = raster + rsize;
    byte *line = gs_alloc_bytes(pdev->memory, lsize, "pcx file buffer");
    byte *plane = line + raster;
    int y;
    int code = 0;		/* return code */

    if (line == 0)		/* can't allocate line buffer */
	return_error(gs_error_VMerror);

    /* Fill in the other variable entries in the header struct. */

    assign_ushort(phdr->x2, pdev->width - 1);
    assign_ushort(phdr->y2, height - 1);
    assign_ushort(phdr->hres, (int)pdev->x_pixels_per_inch);
    assign_ushort(phdr->vres, (int)pdev->y_pixels_per_inch);
    assign_ushort(phdr->bpl, (planar || depth == 1 ? rsize :
			      raster + (raster & 1)));

    /* Write the header. */

    if (fwrite((const char *)phdr, 1, 128, outfile) < 128) {
	code = gs_error_ioerror;
	goto pcx_done;
    }
    /* Write the contents of the image. */
    for (y = 0; y < height; y++) {
	byte *row = line;
	byte *end;

	code = fread(line, sizeof(byte), linesize, infile);
	if (code < 0)
	    break;
	end = row + raster;
	if (!planar) {		/* Just write the bits. */
	    if (raster & 1) {	/* Round to even, with predictable padding. */
		*end = end[-1];
		++end;
	    }
	    pcx_write_rle(row, end, 1, outfile);
	} else
	    switch (depth) {

		case 4:
		    {
			byte *pend = plane + rsize;
			int shift;

			for (shift = 0; shift < 4; shift++) {
			    register byte *from, *to;
			    register int bright = 1 << shift;
			    register int bleft = bright << 4;

			    for (from = row, to = plane;
				 from < end; from += 4
				) {
				*to++ =
				    (from[0] & bleft ? 0x80 : 0) |
				    (from[0] & bright ? 0x40 : 0) |
				    (from[1] & bleft ? 0x20 : 0) |
				    (from[1] & bright ? 0x10 : 0) |
				    (from[2] & bleft ? 0x08 : 0) |
				    (from[2] & bright ? 0x04 : 0) |
				    (from[3] & bleft ? 0x02 : 0) |
				    (from[3] & bright ? 0x01 : 0);
			    }
			    /* We might be one byte short of rsize. */
			    if (to < pend)
				*to = to[-1];
			    pcx_write_rle(plane, pend, 1, outfile);
			}
		    }
		    break;

		case 24:
		    {
			int pnum;

			for (pnum = 0; pnum < 3; ++pnum) {
			    pcx_write_rle(row + pnum, row + raster, 3, outfile);
			    if (pdev->width & 1)
				fputc(0, outfile);		/* pad to even */
			}
		    }
		    break;

		default:
		    code = gs_note_error(gs_error_rangecheck);
		    goto pcx_done;

	    }
	code = 0;
    }

  pcx_done:
    gs_free_object(pdev->memory, line, "pcx file buffer");

    return code;
}

/* ------ Internal routines ------ */

/* Write one line in PCX run-length-encoded format. */
private void
pcx_write_rle(const byte * from, const byte * end, int step, FILE * file)
{  /*
    * The PCX format theoretically allows encoding runs of 63
    * identical bytes, but some readers can't handle repetition
    * counts greater than 15.
    */
#define MAX_RUN_COUNT 15
    int max_run = step * MAX_RUN_COUNT;

    while (from < end) {
	byte data = *from;

	from += step;
	if (data != *from || from == end) {
	    if (data >= 0xc0)
		putc(0xc1, file);
	} else {
	    const byte *start = from;

	    while ((from < end) && (*from == data))
		from += step;
	    /* Now (from - start) / step + 1 is the run length. */
	    while (from - start >= max_run) {
		putc(0xc0 + MAX_RUN_COUNT, file);
		putc(data, file);
		start += max_run;
	    }
	    if (from > start || data >= 0xc0)
		putc((from - start) / step + 0xc1, file);
	}
	putc(data, file);
    }
#undef MAX_RUN_COUNT
}
