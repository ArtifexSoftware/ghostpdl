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

/* gdevpsdf.h */
/* Common output syntax and parameters for PostScript and PDF writers */
#include "gdevvec.h"
/* We shouldn't need the following, but some picky compilers don't allow */
/* externs for undefined structure types. */
#include "strimpl.h"

/* ---------------- Distiller parameters ---------------- */

/* Parameters for controlling distillation of images. */
typedef struct psdf_image_params_s {
	/* ACSDict */
	bool AntiAlias;
	bool AutoFilter;
	int Depth;
	/* Dict */
	bool Downsample;
	enum psdf_downsample_type {
	  ds_Average,
	  ds_Subsample
	} DownsampleType;
	bool Encode;
	const char *Filter;
	int Resolution;
	const stream_template *filter_template;
} psdf_image_params;
#define psdf_image_param_defaults(f, ft)\
  /*ACSDict,*/ 0/*false*/, 0/*false*/, -1, /*Dict,*/ 0/*false*/,\
  ds_Subsample, 1/*true*/, f, 72, ft

/* Declare templates for default image compression filters. */
extern const stream_template s_CFE_template;

/* Complete distiller parameters. */
typedef struct psdf_distiller_params_s {

		/* General parameters */

	bool ASCII85EncodePages;
	enum psdf_auto_rotate_pages {
	  arp_None,
	  arp_All,
	  arp_PageByPage
	} AutoRotatePages;
	bool CompressPages;
	long ImageMemory;
	bool LZWEncodePages;
	bool PreserveHalftoneInfo;
	bool PreserveOPIComments;
	bool PreserveOverprintSettings;
	enum psdf_transfer_function_info {
	  tfi_Preserve,
	  tfi_Apply,
	  tfi_Remove
	} TransferFunctionInfo;
	enum psdf_ucr_and_bg_info {
	  ucrbg_Preserve,
	  ucrbg_Remove
	} UCRandBGInfo;
	bool UseFlateCompression;
#define psdf_general_param_defaults(ascii)\
  ascii, arp_None, 1/*true*/, 250000, 0/*false*/,\
  0/*false*/, 0/*false*/, 0/*false*/, tfi_Apply, ucrbg_Remove, 1/*true*/

		/* Color sampled image parameters */

	psdf_image_params ColorImage;
	enum psdf_color_conversion_strategy {
	  ccs_LeaveColorUnchanged,
	  ccs_UseDeviceDependentColor,
	  ccs_UseDeviceIndependentColor
	} ColorConversionStrategy;
	bool ConvertCMYKImagesToRGB;
	bool ConvertImagesToIndexed;
#define psdf_color_image_param_defaults\
  { psdf_image_param_defaults(0, 0) },\
  ccs_LeaveColorUnchanged, 1/*true*/, 0/*false*/

		/* Grayscale sampled image parameters */	  

	psdf_image_params GrayImage;
#define psdf_gray_image_param_defaults\
  { psdf_image_param_defaults(0, 0) }

		/* Monochrome sampled image parameters */

	psdf_image_params MonoImage;
#define psdf_mono_image_param_defaults\
  { psdf_image_param_defaults("CCITTFaxEncode", &s_CFE_template) }

		/* Font embedding parameters */

	gs_param_string_array AlwaysEmbed;
	gs_param_string_array NeverEmbed;
	bool EmbedAllFonts;
	bool SubsetFonts;
	int MaxSubsetPct;
#define psdf_font_param_defaults\
  	    { 0, 0, 1/*true*/ }, { 0, 0, 1/*true*/ },\
	   1/*true*/, 1/*true*/, 20

} psdf_distiller_params;

/* Define the extended device structure. */
#define gx_device_psdf_common\
	gx_device_vector_common;\
	psdf_distiller_params params
typedef struct gx_device_psdf_s {
	gx_device_psdf_common;
} gx_device_psdf;
#define psdf_initial_values(ascii)\
	vector_initial_values,\
	 { psdf_general_param_defaults(ascii),\
	   psdf_color_image_param_defaults,\
	   psdf_gray_image_param_defaults,\
	   psdf_mono_image_param_defaults,\
	   psdf_font_param_defaults\
	 }

/* st_device_psdf is never instantiated per se, but we still need to */
/* extern its descriptor for the sake of subclasses. */
extern_st(st_device_psdf);
#define public_st_device_psdf()	/* in gdevpsdf.c */\
  gs_public_st_suffix_add0_final(st_device_psdf, gx_device_psdf,\
    "gx_device_psdf", device_psdf_enum_ptrs,\
    device_psdf_reloc_ptrs, gx_device_finalize, st_device_vector)
#define st_device_psdf_max_ptrs (st_device_vector_max_ptrs)

/* Get/put parameters. */
dev_proc_get_params(gdev_psdf_get_params);
dev_proc_put_params(gdev_psdf_put_params);

/* Put a Boolean or integer parameter. */
int psdf_put_bool_param(P4(gs_param_list *plist, gs_param_name param_name,
			   bool *pval, int ecode));
int psdf_put_int_param(P4(gs_param_list *plist, gs_param_name param_name,
			  int *pval, int ecode));

/* ---------------- Vector implementation procedures ---------------- */

	/* Imager state */
int psdf_setlinewidth(P2(gx_device_vector *vdev, floatp width));
int psdf_setlinecap(P2(gx_device_vector *vdev, gs_line_cap cap));
int psdf_setlinejoin(P2(gx_device_vector *vdev, gs_line_join join));
int psdf_setmiterlimit(P2(gx_device_vector *vdev, floatp limit));
int psdf_setdash(P4(gx_device_vector *vdev, const float *pattern,
		    uint count, floatp offset));
int psdf_setflat(P2(gx_device_vector *vdev, floatp flatness));
int psdf_setlogop(P3(gx_device_vector *vdev, gs_logical_operation_t lop,
		     gs_logical_operation_t diff));
	/* Other state */
int psdf_setfillcolor(P2(gx_device_vector *vdev, const gx_drawing_color *pdc));
int psdf_setstrokecolor(P2(gx_device_vector *vdev, const gx_drawing_color *pdc));
	/* Paths */
#define psdf_dopath gdev_vector_dopath
int psdf_dorect(P6(gx_device_vector *vdev, fixed x0, fixed y0, fixed x1,
		   fixed y1, gx_path_type_t type));
int psdf_beginpath(P2(gx_device_vector *vdev, gx_path_type_t type));
int psdf_moveto(P6(gx_device_vector *vdev, floatp x0, floatp y0,
		   floatp x, floatp y, bool first));
int psdf_lineto(P5(gx_device_vector *vdev, floatp x0, floatp y0,
		   floatp x, floatp y));
int psdf_curveto(P9(gx_device_vector *vdev, floatp x0, floatp y0,
		    floatp x1, floatp y1, floatp x2,
		    floatp y2, floatp x3, floatp y3));
int psdf_closepath(P5(gx_device_vector *vdev, floatp x0, floatp y0,
		      floatp x_start, floatp y_start));

/* ---------------- Other procedures ---------------- */

/* Set the fill or stroke color.  rgs is "rg" or "RG". */
int psdf_set_color(P3(gx_device_vector *vdev, const gx_drawing_color *pdc,
		      const char *rgs));
