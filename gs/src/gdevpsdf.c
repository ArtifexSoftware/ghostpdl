/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gdevpsdf.c */
/* Common output syntax and parameters for PostScript and PDF writers */
#include "stdio_.h"
#include "string_.h"
#include "gserror.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gstypes.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gdevpsdf.h"
#include "gdevpstr.h"
#include "strimpl.h"		/* for short-sighted compilers */
#include "scfx.h"
#include "slzwx.h"
#include "srlx.h"

/* Structure descriptor */
public_st_device_psdf();

/* ---------------- Get/put Distiller parameters ---------------- */

typedef struct psdf_image_filter_name_s {
  const char *pname;
  const stream_template *template;
} psdf_image_filter_name;
typedef struct psdf_image_param_names_s {
  const char *ACSDict;		/* not used for mono */
  const char *AntiAlias;
  const char *AutoFilter;	/* not used for mono */
  const char *Depth;
  const char *Dict;
  const char *Downsample;
  const char *DownsampleType;
  const char *Encode;
  const char *Filter;
  const char *Resolution;
} psdf_image_param_names;
private const psdf_image_param_names Color_names = {
  "ColorACSImageDict", "AntiAliasColorImages", "AutoFilterColorImages",
  "ColorImageDepth", "ColorImageDict",
  "DownsampleColorImages", "ColorImageDownsampleType", "EncodeColorImages",
  "ColorImageFilter", "ColorImageResolution"
};
private const psdf_image_filter_name Poly_filters[] = {
  /*{"DCTEncode", &s_DCTE_template},*/
  {"LZWEncode", &s_LZWE_template},
  {0, 0}
};
private const psdf_image_param_names Gray_names = {
  "GrayACSImageDict", "AntiAliasGrayImages", "AutoFilterGrayImages",
  "GrayImageDepth", "GrayImageDict",
  "DownsampleGrayImages", "GrayImageDownsampleType", "EncodeGrayImages",
  "GrayImageFilter", "GrayImageResolution"
};
private const psdf_image_param_names Mono_names = {
  0, "AntiAliasMonoImages", 0,
  "MonoImageDepth", "MonoImageDict",
  "DownsampleMonoImages", "MonoImageDownsampleType", "EncodeMonoImages",
  "MonoImageFilter", "MonoImageResolution"
};
private const psdf_image_filter_name Mono_filters[] = {
  {"CCITTFaxEncode", &s_CFE_template},
  {"LZWEncode", &s_LZWE_template},
  {"RunLengthEncode", &s_RLE_template},
  {0, 0}
};
private const char *AutoRotatePages_names[] = {
  "None", "All", "PageByPage", 0
};
private const char *ColorConversionStrategy_names[] = {
  "LeaveColorUnchanged", "UseDeviceDependentColor",
  "UseDeviceIndependentColor", 0
};
private const char *DownsampleType_names[] = {
  "Average", "Subsample", 0
};
private const char *TransferFunctionInfo_names[] = {
  "Preserve", "Apply", "Remove", 0
};
private const char *UCRandBGInfo_names[] = {
  "Preserve", "Remove", 0
};

/* -------- Get parameters -------- */

/* Get a set of image-related parameters. */
private int
psdf_get_image_params(gs_param_list *plist,
  const psdf_image_param_names *pnames, psdf_image_params *params)
{	int code;
	gs_param_string dsts, fs;

	param_string_from_string(dsts,
		DownsampleType_names[params->DownsampleType]);
	if (
	     /* ACSDict */
	     (code = param_write_bool(plist, pnames->AntiAlias,
				      &params->AntiAlias)) < 0 ||
	     (pnames->AutoFilter != 0 &&
	      (code = param_write_bool(plist, pnames->AutoFilter,
				       &params->AutoFilter)) < 0) ||
	     (code = param_write_int(plist, pnames->Depth,
				     &params->Depth)) < 0 ||
	     /* Dict */
	     (code = param_write_bool(plist, pnames->Downsample,
				      &params->Downsample)) < 0 ||
	     (code = param_write_name(plist, pnames->DownsampleType,
				      &dsts)) < 0 ||
	     (code = param_write_bool(plist, pnames->Encode,
				      &params->Encode)) < 0 ||
	     (code = (params->Filter == 0 ? 0 :
		      (param_string_from_string(fs, params->Filter),
		       param_write_name(plist, pnames->Filter, &fs)))) < 0 ||
	     (code = param_write_int(plist, pnames->Resolution,
				     &params->Resolution)) < 0
	   )
	  ;
	return code;
}

/* Get parameters. */
int
gdev_psdf_get_params(gx_device *dev, gs_param_list *plist)
{	gx_device_psdf *pdev = (gx_device_psdf *)dev;
	int code = gdev_vector_get_params(dev, plist);
	gs_param_string arps, ccss, tfis, ucrbgis;

	if ( code < 0 )
	  return code;
	param_string_from_string(arps,
	  AutoRotatePages_names[(int)pdev->params.AutoRotatePages]);
	param_string_from_string(ccss,
	  ColorConversionStrategy_names[(int)pdev->params.ColorConversionStrategy]);
	param_string_from_string(tfis,
	  TransferFunctionInfo_names[(int)pdev->params.TransferFunctionInfo]);
	param_string_from_string(ucrbgis,
	  UCRandBGInfo_names[(int)pdev->params.UCRandBGInfo]);
	if (
			/* General parameters */

	     (code = param_write_bool(plist, "ASCII85EncodePages",
				      &pdev->params.ASCII85EncodePages)) < 0 ||
	     (code = param_write_name(plist, "AutoRotatePages",
				      &arps)) < 0 ||
	     (code = param_write_bool(plist, "CompressPages",
				      &pdev->params.CompressPages)) < 0 ||
	     (code = param_write_long(plist, "ImageMemory",
				      &pdev->params.ImageMemory)) < 0 ||
	     (code = param_write_bool(plist, "LZWEncodePages",
				      &pdev->params.LZWEncodePages)) < 0 ||
	     (code = param_write_bool(plist, "PreserveHalftoneInfo",
				      &pdev->params.PreserveHalftoneInfo)) < 0 ||
	     (code = param_write_bool(plist, "PreserveOPIComments",
				      &pdev->params.PreserveOPIComments)) < 0 ||
	     (code = param_write_bool(plist, "PreserveOverprintSettings",
				      &pdev->params.PreserveOverprintSettings)) < 0 ||
	     (code = param_write_name(plist, "TransferFunctionInfo", &tfis)) < 0 ||
	     (code = param_write_name(plist, "UCRandBGInfo", &ucrbgis)) < 0 ||
	     (code = param_write_bool(plist, "UseFlateCompression",
				      &pdev->params.UseFlateCompression)) < 0 ||

			/* Color sampled image parameters */

	     (code = psdf_get_image_params(plist, &Color_names, &pdev->params.ColorImage)) < 0 ||
	     (code = param_write_name(plist, "ColorConversionStrategy",
				      &ccss)) < 0 ||
	     (code = param_write_bool(plist, "ConvertCMYKImagesToRGB",
				      &pdev->params.ConvertCMYKImagesToRGB)) < 0 ||
	     (code = param_write_bool(plist, "ConvertImagesToIndexed",
				      &pdev->params.ConvertImagesToIndexed)) < 0 ||

			/* Gray sampled image parameters */

	     (code = psdf_get_image_params(plist, &Gray_names, &pdev->params.GrayImage)) < 0 ||

			/* Mono sampled image parameters */

	     (code = psdf_get_image_params(plist, &Mono_names, &pdev->params.MonoImage)) < 0 ||

			/* Font embedding parameters */

	     (code = param_write_name_array(plist, "AlwaysEmbed", &pdev->params.AlwaysEmbed)) < 0 ||
	     (code = param_write_name_array(plist, "NeverEmbed", &pdev->params.NeverEmbed)) < 0 ||
	     (code = param_write_bool(plist, "EmbedAllFonts", &pdev->params.EmbedAllFonts)) < 0 ||
	     (code = param_write_bool(plist, "SubsetFonts", &pdev->params.SubsetFonts)) < 0 ||
	     (code = param_write_int(plist, "MaxSubsetPct", &pdev->params.MaxSubsetPct)) < 0
	   )
	  ;
	return code;
}

/* -------- Put parameters -------- */

/* Compare a C string and a gs_param_string. */
bool
psdf_key_eq(const gs_param_string *pcs, const char *str)
{	return (strlen(str) == pcs->size &&
		!strncmp(str, (const char *)pcs->data, pcs->size));
}

/* Get an enumerated value. */
private int
psdf_put_enum_param(gs_param_list *plist, gs_param_name param_name, int *pvalue,
  const char **pnames, int ecode)
{	gs_param_string ens;
	int code = param_read_name(plist, param_name, &ens);

	switch ( code )
	  {
	  case 1:
	    return ecode;
	  case 0:
	    { int i;
	      for ( i = 0; pnames[i] != 0; ++i )
		if ( psdf_key_eq(&ens, pnames[i]) )
		  { *pvalue = i;
		    return 0;
		  }
	    }
	    code = gs_error_rangecheck;
	  default:
	    ecode = code;
	    param_signal_error(plist, param_name, code);
	  }
	return code;
}

/* Put a Boolean or integer parameter. */
int
psdf_put_bool_param(gs_param_list *plist, gs_param_name param_name,
  bool *pval, int ecode)
{	int code;

	switch ( code = param_read_bool(plist, param_name, pval) )
	{
	default:
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
		break;
	}
	return ecode;
}
int
psdf_put_int_param(gs_param_list *plist, gs_param_name param_name,
  int *pval, int ecode)
{	int code;

	switch ( code = param_read_int(plist, param_name, pval) )
	{
	default:
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
		break;
	}
	return ecode;
}

/* Put [~](Always|Never)Embed parameters. */
private int
psdf_put_embed_param(gs_param_list *plist, gs_param_name notpname,
  gs_param_string_array *psa, int ecode)
{	gs_param_name pname = notpname + 1;
	int code;
	gs_param_string_array nsa;

	/***** Storage management is incomplete ******/
	/***** Doesn't do incremental add/delete ******/
	switch ( code = param_read_name_array(plist, pname, psa) )
	  {
	  default:
		ecode = code;
		param_signal_error(plist, pname, ecode);
	  case 0:
	  case 1:
		break;
	  }
	switch ( code = param_read_name_array(plist, notpname, &nsa) )
	  {
	  default:
		ecode = code;
		param_signal_error(plist, notpname, ecode);
	  case 0:
	  case 1:
		break;
	  }
	return ecode;
}

/* Put a set of image-related parameters. */
private int
psdf_put_image_params(gs_param_list *plist, const psdf_image_param_names *pnames,
  const psdf_image_filter_name *pifn, psdf_image_params *params, int ecode)
{	gs_param_string fs;
	int dsti = params->DownsampleType;
	int code;

	if ( pnames->ACSDict )
	  { /* ACSDict */
	  }
	ecode = psdf_put_bool_param(plist, pnames->AntiAlias,
				   &params->AntiAlias, ecode);
	if ( pnames->AutoFilter )
	  ecode = psdf_put_bool_param(plist, pnames->AutoFilter,
				     &params->AutoFilter, ecode);
	ecode = psdf_put_int_param(plist, pnames->Depth,
				  &params->Depth, ecode);
	/* Dict */
	ecode = psdf_put_bool_param(plist, pnames->Downsample,
				   &params->Downsample, ecode);
	if ( (ecode = psdf_put_enum_param(plist, pnames->DownsampleType,
					 &dsti, DownsampleType_names,
					 ecode)) >= 0
	   )
	  params->DownsampleType = (enum psdf_downsample_type)dsti;
	ecode = psdf_put_bool_param(plist, pnames->Encode,
				   &params->Encode, ecode);
	switch ( code = param_read_string(plist, pnames->Filter, &fs) )
	  {
	  case 0:
	    {	const psdf_image_filter_name *pn = pifn;
		while ( pn->pname != 0 && !psdf_key_eq(&fs, pn->pname) )
		  pn++;
		if ( pn->pname == 0 )
		  { ecode = gs_error_rangecheck;
		    goto ipe;
		  }
		params->Filter = pn->pname;
		params->filter_template = pn->template;
		break;
	    }
	  default:
		ecode = code;
ipe:		param_signal_error(plist, pnames->Filter, ecode);
	  case 1:
		break;
	  }
	ecode = psdf_put_int_param(plist, pnames->Resolution,
				  &params->Resolution, ecode);
	if ( ecode >= 0 )
	  { /* Force parameters to acceptable values. */
	    if ( params->Resolution < 1 )
	      params->Resolution = 1;
	    switch ( params->Depth )
	      {
	      default:
		params->Depth = -1;
	      case 1: case 2: case 4: case 8:
	      case -1:
		break;
	      }
	  }
	return ecode;
}

/* Put parameters. */
int
gdev_psdf_put_params(gx_device *dev, gs_param_list *plist)
{	gx_device_psdf *pdev = (gx_device_psdf *)dev;
	int ecode = 0;
	int code;
	gs_param_name param_name;
	psdf_distiller_params params;

		/* General parameters. */

	params = pdev->params;

	ecode = psdf_put_bool_param(plist, "ASCII85EncodePages",
				   &params.ASCII85EncodePages, ecode);
	{ int arpi = params.AutoRotatePages;
	  ecode = psdf_put_enum_param(plist, "AutoRotatePages", &arpi,
				      AutoRotatePages_names, ecode);
	  params.AutoRotatePages = (enum psdf_auto_rotate_pages)arpi;
	}
	ecode = psdf_put_bool_param(plist, "CompressPages",
				   &params.CompressPages, ecode);
	switch ( code = param_read_long(plist, (param_name = "ImageMemory"), &params.ImageMemory) )
	{
	default:
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
		break;
	}
	ecode = psdf_put_bool_param(plist, "LZWEncodePages",
				   &params.LZWEncodePages, ecode);
	ecode = psdf_put_bool_param(plist, "PreserveHalftoneInfo",
				   &params.PreserveHalftoneInfo, ecode);
	ecode = psdf_put_bool_param(plist, "PreserveOPIComments",
				   &params.PreserveOPIComments, ecode);
	ecode = psdf_put_bool_param(plist, "PreserveOverprintSettings",
				   &params.PreserveOverprintSettings, ecode);
	{ int tfii = params.TransferFunctionInfo;
	  ecode = psdf_put_enum_param(plist, "TransferFunctionInfo", &tfii,
				     TransferFunctionInfo_names, ecode);
	  params.TransferFunctionInfo = (enum psdf_transfer_function_info)tfii;
	}
	{ int ucrbgi = params.UCRandBGInfo;
	  ecode = psdf_put_enum_param(plist, "UCRandBGInfo", &ucrbgi,
				     UCRandBGInfo_names, ecode);
	  params.UCRandBGInfo = (enum psdf_ucr_and_bg_info)ucrbgi;
	}
	ecode = psdf_put_bool_param(plist, "UseFlateCompression",
				   &params.UseFlateCompression, ecode);

		/* Color sampled image parameters */

	ecode = psdf_put_image_params(plist, &Color_names, Poly_filters,
				     &params.ColorImage, ecode);
	{ int ccsi = params.ColorConversionStrategy;
	  ecode = psdf_put_enum_param(plist, "ColorConversionStrategy", &ccsi,
				     ColorConversionStrategy_names, ecode);
	  params.ColorConversionStrategy =
	    (enum psdf_color_conversion_strategy)ccsi;
	}
	ecode = psdf_put_bool_param(plist, "ConvertCMYKImagesToRGB",
				   &params.ConvertCMYKImagesToRGB, ecode);
	ecode = psdf_put_bool_param(plist, "ConvertImagesToIndexed",
				   &params.ConvertImagesToIndexed, ecode);

		/* Gray sampled image parameters */

	ecode = psdf_put_image_params(plist, &Gray_names, Poly_filters,
				     &params.GrayImage, ecode);

		/* Mono sampled image parameters */

	ecode = psdf_put_image_params(plist, &Mono_names, Mono_filters,
				     &params.MonoImage, ecode);

			/* Font embedding parameters */

	ecode = psdf_put_embed_param(plist, "~AlwaysEmbed",
				    &params.AlwaysEmbed, ecode);
	ecode = psdf_put_embed_param(plist, "~NeverEmbed",
				    &params.NeverEmbed, ecode);
	ecode = psdf_put_bool_param(plist, "EmbedAllFonts",
				   &params.EmbedAllFonts, ecode);
	ecode = psdf_put_bool_param(plist, "SubsetFonts",
				   &params.SubsetFonts, ecode);
	ecode = psdf_put_int_param(plist, "MaxSubsetPct",
				  &params.MaxSubsetPct, ecode);

	if ( ecode < 0 )
	  return ecode;
	code = gdev_vector_put_params(dev, plist);
	if ( code < 0 )
	  return code;

	pdev->params = params;		/* OK to update now */
	return 0;
}

/* ---------------- Utilities ---------------- */

int
psdf_set_color(gx_device_vector *vdev, const gx_drawing_color *pdc,
  const char *rgs)
{	if ( !gx_dc_is_pure(pdc) )
	  return_error(gs_error_rangecheck);
	{ stream *s = gdev_vector_stream(vdev);
	  gx_color_index color = gx_dc_pure_color(pdc);
	  float r = (color >> 16) / 255.0;
	  float g = ((color >> 8) & 0xff) / 255.0;
	  float b = (color & 0xff) / 255.0;

	  if ( r == g && g == b )
	    pprintg1(s, "%g", r), pprints1(s, " %s\n", rgs + 1);
	  else
	    pprintg3(s, "%g %g %g", r, g, b), pprints1(s, " %s\n", rgs);
	}
	return 0;
}

/* ---------------- Vector implementation procedures ---------------- */

int
psdf_setlinewidth(gx_device_vector *vdev, floatp width)
{	pprintg1(gdev_vector_stream(vdev), "%g w\n", width);
	return 0;
}

int
psdf_setlinecap(gx_device_vector *vdev, gs_line_cap cap)
{	pprintd1(gdev_vector_stream(vdev), "%d J\n", cap);
	return 0;
}

int
psdf_setlinejoin(gx_device_vector *vdev, gs_line_join join)
{	pprintd1(gdev_vector_stream(vdev), "%d j\n", join);
	return 0;
}

int
psdf_setmiterlimit(gx_device_vector *vdev, floatp limit)
{	pprintg1(gdev_vector_stream(vdev), "%g M\n", limit);
	return 0;
}

int
psdf_setdash(gx_device_vector *vdev, const float *pattern, uint count,
  floatp offset)
{	stream *s = gdev_vector_stream(vdev);
	int i;

	pputs(s, "[ ");
	for ( i = 0; i < count; ++i )
	  pprintg1(s, "%g ", pattern[i]);
	pprintg1(s, "] %g d\n", offset);
	return 0;
}

int
psdf_setflat(gx_device_vector *vdev, floatp flatness)
{	pprintg1(gdev_vector_stream(vdev), "%g i\n", flatness);
	return 0;
}

int
psdf_setlogop(gx_device_vector *vdev, gs_logical_operation_t lop,
  gs_logical_operation_t diff)
{	/****** SHOULD AT LEAST DETECT SET-0 & SET-1 ******/
	return 0;
}

int
psdf_setfillcolor(gx_device_vector *vdev, const gx_drawing_color *pdc)
{	return psdf_set_color(vdev, pdc, "rg");
}

int
psdf_setstrokecolor(gx_device_vector *vdev, const gx_drawing_color *pdc)
{	return psdf_set_color(vdev, pdc, "RG");
}

int
psdf_dorect(gx_device_vector *vdev, fixed x0, fixed y0, fixed x1, fixed y1,
  gx_path_type_t type)
{	int code = (*vdev_proc(vdev, beginpath))(vdev, type);
	if ( code < 0 )
	  return code;
	pprintg4(gdev_vector_stream(vdev), "%g %g %g %g re\n",
		 fixed2float(x0), fixed2float(y0),
		 fixed2float(x1 - x0), fixed2float(y1 - y0));
	return (*vdev_proc(vdev, endpath))(vdev, type);
}

int
psdf_beginpath(gx_device_vector *vdev, gx_path_type_t type)
{	return 0;
}

int
psdf_moveto(gx_device_vector *vdev, floatp x0, floatp y0, floatp x, floatp y,
  bool first)
{	pprintg2(gdev_vector_stream(vdev), "%g %g m\n", x, y);
	return 0;
}

int
psdf_lineto(gx_device_vector *vdev, floatp xy, floatp y0, floatp x, floatp y)
{	pprintg2(gdev_vector_stream(vdev), "%g %g l\n", x, y);
	return 0;
}

int
psdf_curveto(gx_device_vector *vdev, floatp x0, floatp y0,
  floatp x1, floatp y1, floatp x2, floatp y2, floatp x3, floatp y3)
{	if ( x1 == x0 && y1 == y0 )
	  pprintg4(gdev_vector_stream(vdev), "%g %g %g %g v\n",
		   x2, y2, x3, y3);
	else if ( x3 == x2 && y3 == y2 )
	  pprintg4(gdev_vector_stream(vdev), "%g %g %g %g y\n",
		   x1, y1, x2, y2);
	else
	  pprintg6(gdev_vector_stream(vdev), "%g %g %g %g %g %g c\n",
		   x1, y1, x2, y2, x3, y3);
	return 0;
}

int
psdf_closepath(gx_device_vector *vdev, floatp x0, floatp y0,
  floatp x_start, floatp y_start)
{	pputs(gdev_vector_stream(vdev), "h\n");
	return 0;
}

/* endpath is deliberately omitted. */
