/* Copyright (C) 2002 artofcode LLC.  All rights reserved.
  
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
/* PhotoShop (PSD) export device, supporting DeviceN color models. */

#include "math_.h"
#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"
#include "gdevdcrd.h"
#include "gstypes.h"
#include "icc.h"
#include "gxdcconv.h"

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
private dev_proc_get_params(psd_get_params);
private dev_proc_put_params(psd_put_params);
private dev_proc_print_page(psd_print_page);
private dev_proc_map_color_rgb(psd_map_color_rgb);
private dev_proc_get_color_mapping_procs(get_spotrgb_color_mapping_procs);
#if 0
private dev_proc_get_color_mapping_procs(get_spotcmyk_color_mapping_procs);
#endif
private dev_proc_get_color_mapping_procs(get_psd_color_mapping_procs);
private dev_proc_get_color_comp_index(psd_get_color_comp_index);
private dev_proc_encode_color(psd_encode_color);
private dev_proc_decode_color(psd_decode_color);

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

/* This is redundant with color_info.cm_name. We may eliminate this
   typedef and use the latter string for everything. */
typedef enum {
    psd_DEVICE_GRAY,
    psd_DEVICE_RGB,
    psd_DEVICE_CMYK,
    psd_DEVICE_N
} psd_color_model;

/*
 * A structure definition for a DeviceN type device
 */
typedef struct psd_device_s {
    gx_device_common;
    gx_prn_device_common;

    /*        ... device-specific parameters ... */

    psd_color_model color_model;

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
    int num_std_colorant_names;	/* Number of names in list */

    /*
    * Separation names (if any).
    */
    gs_separation_names separation_names;

    /*
     * Separation Order (if specified).
     */
    gs_separation_names separation_order;

    /* ICC color profile objects, for color conversion. */
    char profile_rgb_fn[256];
    icmLuBase *lu_rgb;
    int lu_rgb_outn;

    char profile_cmyk_fn[256];
    icmLuBase *lu_cmyk;
    int lu_cmyk_outn;

    char profile_out_fn[256];
    icmLuBase *lu_out;
} psd_device;

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
	psd_map_color_rgb,		/* map_color_rgb */\
	NULL,				/* fill_rectangle */\
	NULL,				/* tile_rectangle */\
	NULL,				/* copy_mono */\
	NULL,				/* copy_color */\
	NULL,				/* draw_line */\
	NULL,				/* get_bits */\
	psd_get_params,		/* get_params */\
	psd_put_params,		/* put_params */\
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
	psd_get_color_comp_index,	/* get_color_comp_index */\
	psd_encode_color,		/* encode_color */\
	psd_decode_color		/* decode_color */\
}


private const fixed_colorant_names_list DeviceGrayComponents = {
	"Gray",
	0		/* List terminator */
};

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

const psd_device gs_psdrgb_device =
{   
    prn_device_body_extended(psd_device, spot_rgb_procs, "psdrgb",
	 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	 X_DPI, Y_DPI,		/* X and Y hardware resolution */
	 0, 0, 0, 0,		/* margins */
    	 GX_DEVICE_COLOR_MAX_COMPONENTS, 3,	/* MaxComponents, NumComp */
	 GX_CINFO_POLARITY_ADDITIVE,		/* Polarity */
	 24, 0,			/* Depth, Gray_index, */
	 255, 255, 1, 1,	/* MaxGray, MaxColor, DitherGray, DitherColor */
	 GX_CINFO_SEP_LIN,      /* Linear & Seperable */
	 "DeviceRGB",		/* Process color model name */
	 psd_print_page),	/* Printer page print routine */
    /* DeviceN device specific parameters */
    psd_DEVICE_RGB,		/* Color model */
    8,				/* Bits per color - must match ncomp, depth, etc. above */
    				/* Names of color model colorants */
    (const fixed_colorant_names_list *) &DeviceRGBComponents,
    3,				/* Number colorants for RGB */
    {0},			/* SeparationNames */
    {0}				/* SeparationOrder names */
};

private const gx_device_procs spot_cmyk_procs = device_procs(get_psd_color_mapping_procs);

const psd_device gs_psdcmyk_device =
{   
    prn_device_body_extended(psd_device, spot_cmyk_procs, "psdcmyk",
	 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	 X_DPI, Y_DPI,		/* X and Y hardware resolution */
	 0, 0, 0, 0,		/* margins */
    	 GX_DEVICE_COLOR_MAX_COMPONENTS, 4,	/* MaxComponents, NumComp */
	 GX_CINFO_POLARITY_SUBTRACTIVE,		/* Polarity */
	 32, 0,			/* Depth, Gray_index, */
	 255, 255, 1, 1,	/* MaxGray, MaxColor, DitherGray, DitherColor */
	 GX_CINFO_SEP_LIN,      /* Linear & Separable */
	 "DeviceCMYK",		/* Process color model name */
	 psd_print_page),	/* Printer page print routine */
    /* DeviceN device specific parameters */
    psd_DEVICE_CMYK,		/* Color model */
    8,				/* Bits per color - must match ncomp, depth, etc. above */
    				/* Names of color model colorants */
    (const fixed_colorant_names_list *) &DeviceCMYKComponents,
    4,				/* Number colorants for RGB */
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
    int i = ((psd_device *)dev)->separation_names.num_names;

    out[0] = out[1] = out[2] = gray;
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
}

private void
rgb_cs_to_spotrgb_cm(gx_device * dev, const gs_imager_state *pis,
				  frac r, frac g, frac b, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((psd_device *)dev)->separation_names.num_names;

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
    int i = ((psd_device *)dev)->separation_names.num_names;

    color_cmyk_to_rgb(c, m, y, k, NULL, out);
    for(; i>0; i--)			/* Clear spot colors */
        out[2 + i] = 0;
};

private void
gray_cs_to_spotcmyk_cm(gx_device * dev, frac gray, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    int i = ((psd_device *)dev)->separation_names.num_names;

    out[0] = out[1] = out[2] = 0;
    out[3] = frac_1 - gray;
    for(; i>0; i--)			/* Clear spot colors */
        out[3 + i] = 0;
}

private void
rgb_cs_to_spotcmyk_cm(gx_device * dev, const gs_imager_state *pis,
				   frac r, frac g, frac b, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    psd_device *xdev = (psd_device *)dev;
    int n = xdev->separation_names.num_names;
    int i;

    color_rgb_to_cmyk(r, g, b, pis, out);
    for(i = 0; i < n; i++)			/* Clear spot colors */
	out[4 + i] = 0;
}

private void
cmyk_cs_to_spotcmyk_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    psd_device *xdev = (psd_device *)dev;
    int n = xdev->separation_names.num_names;
    int i;

    out[0] = c;
    out[1] = m;
    out[2] = y;
    out[3] = k;
    for(i = 0; i < n; i++)			/* Clear spot colors */
	out[4 + i] = 0;
};

private void
cmyk_cs_to_spotn_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    psd_device *xdev = (psd_device *)dev;
    int n = xdev->separation_names.num_names;
    icmLuBase *luo = xdev->lu_cmyk;
    int i;

    if (luo != NULL) {
	double in[3];
	double tmp[MAX_CHAN];
	int outn = xdev->lu_cmyk_outn;

	in[0] = frac2float(c);
	in[1] = frac2float(m);
	in[2] = frac2float(y);
	in[3] = frac2float(k);
	luo->lookup(luo, tmp, in);
	for (i = 0; i < outn; i++)
	    out[i] = float2frac(tmp[i]);
	for (; i < n + 4; i++)
	    out[i] = 0;
    } else {
	/* If no profile given, assume CMYK */
	out[0] = c;
	out[1] = m;
	out[2] = y;
	out[3] = k;
	for(i = 0; i < n; i++)			/* Clear spot colors */
	    out[4 + i] = 0;
    }
};

private void
gray_cs_to_spotn_cm(gx_device * dev, frac gray, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */

    cmyk_cs_to_spotn_cm(dev, 0, 0, 0, (frac)(frac_1 - gray), out);
}

private void
rgb_cs_to_spotn_cm(gx_device * dev, const gs_imager_state *pis,
				   frac r, frac g, frac b, frac out[])
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    psd_device *xdev = (psd_device *)dev;
    int n = xdev->separation_names.num_names;
    icmLuBase *luo = xdev->lu_rgb;
    int i;

    if (luo != NULL) {
	double in[3];
	double tmp[MAX_CHAN];
	int outn = xdev->lu_rgb_outn;

	in[0] = frac2float(r);
	in[1] = frac2float(g);
	in[2] = frac2float(b);
	luo->lookup(luo, tmp, in);
	for (i = 0; i < outn; i++)
	    out[i] = float2frac(tmp[i]);
	for (; i < n + 4; i++)
	    out[i] = 0;
    } else {
	frac cmyk[4];

	color_rgb_to_cmyk(r, g, b, pis, cmyk);
	cmyk_cs_to_spotn_cm(dev, cmyk[0], cmyk[1], cmyk[2], cmyk[3],
			    out);
    }
}

private const gx_cm_color_map_procs spotRGB_procs = {
    gray_cs_to_spotrgb_cm, rgb_cs_to_spotrgb_cm, cmyk_cs_to_spotrgb_cm
};

private const gx_cm_color_map_procs spotCMYK_procs = {
    gray_cs_to_spotcmyk_cm, rgb_cs_to_spotcmyk_cm, cmyk_cs_to_spotcmyk_cm
};

private const gx_cm_color_map_procs spotN_procs = {
    gray_cs_to_spotn_cm, rgb_cs_to_spotn_cm, cmyk_cs_to_spotn_cm
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

#if 0
private const gx_cm_color_map_procs *
get_spotcmyk_color_mapping_procs(const gx_device * dev)
{
    return &spotCMYK_procs;
}
#endif

private const gx_cm_color_map_procs *
get_psd_color_mapping_procs(const gx_device * dev)
{
    const psd_device *xdev = (const psd_device *)dev;

    if (xdev->color_model == psd_DEVICE_RGB)
	return &spotRGB_procs;
    else if (xdev->color_model == psd_DEVICE_CMYK)
	return &spotCMYK_procs;
    else if (xdev->color_model == psd_DEVICE_N)
	return &spotN_procs;
    else
	return NULL;
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
 */
private gx_color_index
psd_encode_color(gx_device *dev, const gx_color_value colors[])
{
    int bpc = ((psd_device *)dev)->bitspercomponent;
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
psd_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    int bpc = ((psd_device *)dev)->bitspercomponent;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    int mask = (1 << bpc) - 1;
    int i = 0;
    int ncomp = dev->color_info.num_components;

    for (; i<ncomp; i++) {
        out[ncomp - i - 1] = (gx_color_value) ((color & mask) << drop);
	color >>= bpc;
    }
    return 0;
}

/*
 * Convert a gx_color_index to RGB.
 */
private int
psd_map_color_rgb(gx_device *dev, gx_color_index color, gx_color_value rgb[3])
{
    psd_device *xdev = (psd_device *)dev;

    if (xdev->color_model == psd_DEVICE_RGB)
	return psd_decode_color(dev, color, rgb);
    /* TODO: return reasonable values. */
    rgb[0] = 0;
    rgb[1] = 0;
    rgb[2] = 0;
    return 0;
}

private int
psd_open_profile(psd_device *xdev, char *profile_fn, icmLuBase **pluo,
		 int *poutn)
{
    icmFile *fp;
    icc *icco;
    icmLuBase *luo;

    dlprintf1("psd_open_profile %s\n", profile_fn);
    fp = new_icmFileStd_name(profile_fn, (char *)"rb");
    if (fp == NULL)
	return_error(gs_error_undefinedfilename);
    icco = new_icc();
    if (icco == NULL)
	return_error(gs_error_VMerror);
    if (icco->read(icco, fp, 0))
	return_error(gs_error_rangecheck);
    luo = icco->get_luobj(icco, icmFwd, icmDefaultIntent, icmSigDefaultData, icmLuOrdNorm);
    if (luo == NULL)
	return_error(gs_error_rangecheck);
    *pluo = luo;
    luo->spaces(luo, NULL, NULL, NULL, poutn, NULL, NULL, NULL, NULL);
    return 0;
}

private int
psd_open_profiles(psd_device *xdev)
{
    int code = 0;
    if (xdev->lu_out == NULL && xdev->profile_out_fn[0]) {
	code = psd_open_profile(xdev, xdev->profile_out_fn,
				    &xdev->lu_out, NULL);
    }
    if (code >= 0 && xdev->lu_rgb == NULL && xdev->profile_rgb_fn[0]) {
	code = psd_open_profile(xdev, xdev->profile_rgb_fn,
				&xdev->lu_rgb, &xdev->lu_rgb_outn);
    }
    if (code >= 0 && xdev->lu_cmyk == NULL && xdev->profile_cmyk_fn[0]) {
	code = psd_open_profile(xdev, xdev->profile_cmyk_fn,
				&xdev->lu_cmyk, &xdev->lu_cmyk_outn);
    }
    return code;
}

#define set_param_array(a, d, s)\
  (a.data = d, a.size = s, a.persistent = false);

/* Get parameters.  We provide a default CRD. */
private int
psd_get_params(gx_device * pdev, gs_param_list * plist)
{
    psd_device *xdev = (psd_device *)pdev;
    int code;
    bool seprs = false;
    gs_param_string_array scna;
    gs_param_string pos;
    gs_param_string prgbs;
    gs_param_string pcmyks;

    set_param_array(scna, NULL, 0);

    if ( (code = gdev_prn_get_params(pdev, plist)) < 0 ||
         (code = sample_device_crd_get_params(pdev, plist, "CRDDefault")) < 0 ||
	 (code = param_write_name_array(plist, "SeparationColorNames", &scna)) < 0 ||
	 (code = param_write_bool(plist, "Separations", &seprs)) < 0)
	return code;

    pos.data = (const byte *)xdev->profile_out_fn,
	pos.size = strlen(xdev->profile_out_fn),
	pos.persistent = false;
    code = param_write_string(plist, "ProfileOut", &pos);
    if (code < 0)
	return code;

    prgbs.data = (const byte *)xdev->profile_rgb_fn,
	prgbs.size = strlen(xdev->profile_rgb_fn),
	prgbs.persistent = false;
    code = param_write_string(plist, "ProfileRgb", &prgbs);

    pcmyks.data = (const byte *)xdev->profile_cmyk_fn,
	pcmyks.size = strlen(xdev->profile_cmyk_fn),
	pcmyks.persistent = false;
    code = param_write_string(plist, "ProfileCmyk", &prgbs);

    return code;
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
        fixed_colorant_name * plist = 
	    (fixed_colorant_name *) * pcomp_list;
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

private int
psd_param_read_fn(gs_param_list *plist, const char *name,
		  gs_param_string *pstr, int max_len)
{
    int code = param_read_string(plist, name, pstr);

    if (code == 0) {
	if (pstr->size >= max_len)
	    param_signal_error(plist, name, code = gs_error_rangecheck);
    } else {
	pstr->data = 0;
    }
    return code;
}

/* Compare a C string and a gs_param_string. */
static bool
param_string_eq(const gs_param_string *pcs, const char *str)
{
    return (strlen(str) == pcs->size &&
	    !strncmp(str, (const char *)pcs->data, pcs->size));
}

private int
psd_set_color_model(psd_device *xdev, psd_color_model color_model)
{
    xdev->color_model = color_model;
    if (color_model == psd_DEVICE_GRAY) {
	xdev->std_colorant_names =
	    (const fixed_colorant_names_list *) &DeviceGrayComponents;
	xdev->num_std_colorant_names = 1;
	xdev->color_info.cm_name = "DeviceGray";
	xdev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
    } else if (color_model == psd_DEVICE_RGB) {
	xdev->std_colorant_names =
	    (const fixed_colorant_names_list *) &DeviceRGBComponents;
	xdev->num_std_colorant_names = 3;
	xdev->color_info.cm_name = "DeviceRGB";
	xdev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
    } else if (color_model == psd_DEVICE_CMYK) {
	xdev->std_colorant_names =
	    (const fixed_colorant_names_list *) &DeviceCMYKComponents;
	xdev->num_std_colorant_names = 4;
	xdev->color_info.cm_name = "DeviceCMYK";
	xdev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else if (color_model == psd_DEVICE_N) {
	xdev->std_colorant_names =
	    (const fixed_colorant_names_list *) &DeviceCMYKComponents;
	xdev->num_std_colorant_names = 4;
	xdev->color_info.cm_name = "DeviceN";
	xdev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else {
	return -1;
    }

    return 0;
}

/* Set parameters.  We allow setting the number of bits per component. */
private int
psd_put_params(gx_device * pdev, gs_param_list * plist)
{
    psd_device * const pdevn = (psd_device *) pdev;
    gx_device_color_info save_info;
    gs_param_name param_name;
    int npcmcolors;
    int num_spot = pdevn->separation_names.num_names;
    int ecode = 0;
    int code;
    gs_param_string_array scna;
    gs_param_string po;
    gs_param_string prgb;
    gs_param_string pcmyk;
    gs_param_string pcm;
    psd_color_model color_model = pdevn->color_model;

    BEGIN_ARRAY_PARAM(param_read_name_array, "SeparationColorNames", scna, scna.size, scne) {
	break;
    } END_ARRAY_PARAM(scna, scne);

    code = psd_param_read_fn(plist, "ProfileOut", &po,
				 sizeof(pdevn->profile_out_fn));
    if (code >= 0)
	code = psd_param_read_fn(plist, "ProfileRgb", &prgb,
				 sizeof(pdevn->profile_rgb_fn));
    if (code >= 0)
	code = psd_param_read_fn(plist, "ProfileCmyk", &pcmyk,
				 sizeof(pdevn->profile_cmyk_fn));

    if (code >= 0)
	code = param_read_name(plist, "ProcessColorModel", &pcm);
    if (code == 0) {
	if (param_string_eq (&pcm, "DeviceGray"))
	    color_model = psd_DEVICE_GRAY;
	else if (param_string_eq (&pcm, "DeviceRGB"))
	    color_model = psd_DEVICE_RGB;
	else if (param_string_eq (&pcm, "DeviceCMYK"))
	    color_model = psd_DEVICE_CMYK;
	else if (param_string_eq (&pcm, "DeviceN"))
	    color_model = psd_DEVICE_N;
	else {
	    param_signal_error(plist, "ProcessColorModel",
			       code = gs_error_rangecheck);
	}
    }
    if (code < 0)
	ecode = code;

    /*
     * Save the color_info in case gdev_prn_put_params fails, and for
     * comparison.
     */
    save_info = pdevn->color_info;
    ecode = psd_set_color_model(pdevn, color_model);
    if (ecode == 0)
	ecode = gdev_prn_put_params(pdev, plist);
    if (ecode < 0) {
	pdevn->color_info = save_info;
	return ecode;
    }

    /* Separations are only valid with a subrtractive color model */
    if (pdev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE) {
        /*
         * Process the separation color names.  Remove any names that already match
         * the process color model colorant names for the device.
         */
        if (scna.data != 0) {
	    int i;
	    int num_names = scna.size;
	    const fixed_colorant_names_list * pcomp_names = 
	        ((psd_device *)pdev)->std_colorant_names;

	    for (i = num_spot = 0; i < num_names; i++) {
	        if (!check_process_color_names(pcomp_names, &scna.data[i]))
	            pdevn->separation_names.names[num_spot++] = &scna.data[i];
	    }
	    pdevn->separation_names.num_names = num_spot;
	    if (pdevn->is_open)
	        gs_closedevice(pdev);
        }

        npcmcolors = pdevn->num_std_colorant_names;
        pdevn->color_info.num_components = npcmcolors + num_spot;
        /* 
         * The DeviceN device can have zero components if nothing has been specified.
         * This causes some problems so force at least one component until something
         * is specified.
         */
        if (!pdevn->color_info.num_components)
	    pdevn->color_info.num_components = 1;
        pdevn->color_info.depth = bpc_to_depth(pdevn->color_info.num_components,
    						pdevn->bitspercomponent);
        if (pdevn->color_info.depth != save_info.depth) {
	    gs_closedevice(pdev);
        }
    }
    if (po.data != 0) {
	memcpy(pdevn->profile_out_fn, po.data, po.size);
	pdevn->profile_out_fn[po.size] = 0;
    }
    if (prgb.data != 0) {
	memcpy(pdevn->profile_rgb_fn, prgb.data, prgb.size);
	pdevn->profile_rgb_fn[prgb.size] = 0;
    }
    if (pcmyk.data != 0) {
	memcpy(pdevn->profile_cmyk_fn, pcmyk.data, pcmyk.size);
	pdevn->profile_cmyk_fn[pcmyk.size] = 0;
    }
    code = psd_open_profiles(pdevn);

    return code;
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
psd_get_color_comp_index(const gx_device * dev, const char * pname, int name_size, int src_index)
{
/* TO_DO_DEVICEN  This routine needs to include the effects of the SeparationOrder array */
    const fixed_colorant_names_list * list = ((const psd_device *)dev)->std_colorant_names;
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
	const gs_separation_names * separations = &((const psd_device *)dev)->separation_names;
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


/* ------ Private definitions ------ */

/* All two-byte quantities are stored MSB-first! */
#if arch_is_big_endian
#  define assign_u16(a,v) a = (v)
#  define assign_u32(a,v) a = (v)
#else
#  define assign_u16(a,v) a = ((v) >> 8) + ((v) << 8)
#  define assign_u32(a,v) a = (((v) >> 24) & 0xff) + (((v) >> 8) & 0xff00) + (((v) & 0xff00) << 8) + (((v) & 0xff) << 24)
#endif

typedef struct {
    FILE *f;

    int width;
    int height;
    int base_bytes_pp; /* almost always 3 (rgb) */
    int n_extra_channels;

    /* byte offset of image data */
    int image_data_off;
} psd_write_ctx;

private int
psd_setup(psd_write_ctx *xc, psd_device *dev)
{
    xc->base_bytes_pp = dev->color_info.num_components - dev->separation_names.num_names;
    xc->n_extra_channels = dev->separation_names.num_names;
    xc->width = dev->width;
    xc->height = dev->height;

    return 0;
}

private int
psd_write(psd_write_ctx *xc, const byte *buf, int size) {
    int code;

    code = fwrite(buf, 1, size, xc->f);
    if (code < 0)
	return code;
    return 0;
}

private int
psd_write_8(psd_write_ctx *xc, byte v)
{
    return psd_write(xc, (byte *)&v, 1);
}

private int
psd_write_16(psd_write_ctx *xc, bits16 v)
{
    bits16 buf;

    assign_u16(buf, v);
    return psd_write(xc, (byte *)&buf, 2);
}

private int
psd_write_32(psd_write_ctx *xc, bits32 v)
{
    bits32 buf;

    assign_u32(buf, v);
    return psd_write(xc, (byte *)&buf, 4);
}

private int
psd_write_header(psd_write_ctx *xc, psd_device *pdev)
{
    int code = 0;
    int n_extra_channels = xc->n_extra_channels;
    int bytes_pp = xc->base_bytes_pp + n_extra_channels;
    int chan_idx;
    int chan_names_len = 0;

    psd_write(xc, (const byte *)"8BPS", 4); /* Signature */
    psd_write_16(xc, 1); /* Version - Always equal to 1*/
    /* Reserved 6 Bytes - Must be zero */
    psd_write_32(xc, 0);
    psd_write_16(xc, 0);
    psd_write_16(xc, (bits16) bytes_pp); /* Channels (2 Bytes) - Supported range is 1 to 24 */
    psd_write_32(xc, xc->height); /* Rows */
    psd_write_32(xc, xc->width); /* Columns */
    psd_write_16(xc, 8); /* Depth - 1, 8 and 16 */
    psd_write_16(xc, (bits16) xc->base_bytes_pp); /* Mode - RGB=3, CMYK=4 */
    
    /* Color Mode Data */
    psd_write_32(xc, 0); 	/* No color mode data */

    /* Image Resources */

    /* Channel Names */
    for (chan_idx = 0; chan_idx < xc->n_extra_channels; chan_idx++) {
	const gs_param_string *separation_name =
	    pdev->separation_names.names[chan_idx];
	
	chan_names_len += (separation_name->size + 1);
    }    
    psd_write_32(xc, 12 + (chan_names_len + (chan_names_len % 2))
			+ (12 + (14*xc->n_extra_channels))
			+ 28); 
    psd_write(xc, (const byte *)"8BIM", 4);
    psd_write_16(xc, 1006); /* 0x03EE */
    psd_write_16(xc, 0); /* PString */
    psd_write_32(xc, chan_names_len + (chan_names_len % 2));
    for (chan_idx = 0; chan_idx < xc->n_extra_channels; chan_idx++) {
	const gs_param_string *separation_name =
	    pdev->separation_names.names[chan_idx];

	psd_write_8(xc, (byte) separation_name->size);
	psd_write(xc, separation_name->data, separation_name->size);
    }    
    if (chan_names_len % 2) 
	psd_write_8(xc, 0); /* pad */

    /* DisplayInfo - Colors for each spot channels */
    psd_write(xc, (const byte *)"8BIM", 4);
    psd_write_16(xc, 1007); /* 0x03EF */
    psd_write_16(xc, 0); /* PString */
    psd_write_32(xc, 14 * xc->n_extra_channels); /* Length */
    for (chan_idx = 0; chan_idx < xc->n_extra_channels; chan_idx++) {
	/*
	const gs_param_string *separation_name =
	    pdev->separation_names.names[chan_idx];
	psd_write_8(xc, separation_name->size);
	psd_write(xc, separation_name->data, separation_name->size);
	*/
	psd_write_16(xc, 02); /* CMYK */
	psd_write_16(xc, 65535); /* Cyan */
	psd_write_16(xc, 65535); /* Magenta */
	psd_write_16(xc, 65535); /* Yellow */
	psd_write_16(xc, 0); /* Black */
	psd_write_16(xc, 0); /* Opacity */
	psd_write_8(xc, 2); /* Don't know */
	psd_write_8(xc, 0); /* Padding - Always Zero */
    }

    /* Image resolution */
    psd_write(xc, (const byte *)"8BIM", 4);
    psd_write_16(xc, 1005); /* 0x03ED */
    psd_write_16(xc, 0); /* PString */
    psd_write_32(xc, 16); /* Length */
    		/* Resolution is specified as a fixed 16.16 bits */
    psd_write_32(xc, (int) (pdev->HWResolution[0] * 0x10000 + 0.5));
    psd_write_16(xc, 1);	/* width:  1 --> resolution is pixels per inch */
    psd_write_16(xc, 1);	/* width:  1 --> resolution is pixels per inch */
    psd_write_32(xc, (int) (pdev->HWResolution[1] * 0x10000 + 0.5));
    psd_write_16(xc, 1);	/* height:  1 --> resolution is pixels per inch */
    psd_write_16(xc, 1);	/* height:  1 --> resolution is pixels per inch */

    /* Layer and Mask information */
    psd_write_32(xc, 0); 	/* No layer or mask information */

    return code;
}

private void
psd_calib_row(psd_write_ctx *xc, byte **tile_data, const byte *row, 
		int channel, icmLuBase *luo)
{
    int base_bytes_pp = xc->base_bytes_pp;
    int n_extra_channels = xc->n_extra_channels;
    int channels = base_bytes_pp + n_extra_channels;
    int inn, outn;
    int x;
    double in[MAX_CHAN], out[MAX_CHAN];

    luo->spaces(luo, NULL, &inn, NULL, &outn, NULL, NULL, NULL, NULL);
	
    for (x = 0; x < xc->width; x++) {
	if (channel < outn) {
	    int plane_idx;

	    for (plane_idx = 0; plane_idx < inn; plane_idx++)
		in[plane_idx] = row[x*channels+plane_idx] * (1.0 / 255);	

	    (*tile_data)[x] = (int)(0.5 + 255 * out[channel]);
	    luo->lookup(luo, out, in);
	} else {
	    (*tile_data)[x] = 255 ^ row[x*channels+base_bytes_pp+channel];
	}
    }
}

private int
psd_write_image_data(psd_write_ctx *xc, gx_device_printer *pdev)
{
    int code = 0;
    int raster = gdev_prn_raster(pdev);
    int i, j;
    byte *line, *sep_line;
    int base_bytes_pp = xc->base_bytes_pp;
    int n_extra_channels = xc->n_extra_channels;
    int bytes_pp = base_bytes_pp + n_extra_channels;
    int chan_idx;
    psd_device *xdev = (psd_device *)pdev;
    icmLuBase *luo = xdev->lu_out;
    byte *row;

    /* Image Data Section */
    psd_write_16(xc, 0); /* Compression */

    line = gs_alloc_bytes(pdev->memory, raster, "psd_write_image_data");
    sep_line = gs_alloc_bytes(pdev->memory, xc->width, "psd_write_sep_line");

    for (chan_idx = 0; chan_idx < bytes_pp; chan_idx++) {
	for (j = 0; j < xc->height; ++j) {
	    code = gdev_prn_get_bits(pdev, j, line, &row);
	    if (luo == NULL) {
		for (i=0; i<xc->width; ++i) {
		    if (base_bytes_pp == 3) {
			/* RGB */
			sep_line[i] = row[i*bytes_pp + chan_idx];
		    } else {
			/* CMYK */
			sep_line[i] = 255 - row[i*bytes_pp + chan_idx];
		    }
		}
	    } else {
		psd_calib_row(xc, &sep_line, row, chan_idx, luo);
	    }	
	    psd_write(xc, sep_line, xc->width);
	}
    }

    gs_free_object(pdev->memory, sep_line, "psd_write_sep_line");
    gs_free_object(pdev->memory, line, "psd_write_image_data");
    return code;
}

static int
psd_print_page(gx_device_printer *pdev, FILE *file)
{
    psd_write_ctx xc;

    xc.f = file;

    psd_setup(&xc, (psd_device *)pdev);
    psd_write_header(&xc, (psd_device *)pdev);
    psd_write_image_data(&xc, pdev);

    return 0;
}
