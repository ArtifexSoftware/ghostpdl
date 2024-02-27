/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Common output syntax and parameters for PostScript and PDF writers */

#ifndef gdevpsdf_INCLUDED
#  define gdevpsdf_INCLUDED

/*
 * This file should really be named gdevpsd.h or gdevpsdx.h, but we're
 * keeping its current name for backward compatibility.
 */

#include "gdevvec.h"
#include "gsparam.h"
#include "strimpl.h"
#include "sa85x.h"
#include "scfx.h"
#include "spsdf.h"

extern const stream_template s_DCTE_template; /* don't want all of sdct.h */

/*
 * NOTE: the default filter for color and gray images should be DCTEncode.
 * We don't currently set this, because the management of the parameters
 * for this filter simply doesn't work.
 */

/* ---------------- Distiller parameters ---------------- */

/* Parameters for controlling distillation of images. */
typedef struct psdf_image_params_s {
    gs_c_param_list *ACSDict;	/* JPEG */
    bool AntiAlias;
    bool AutoFilter;
    int Depth;
    gs_c_param_list *Dict;	/* JPEG or CCITTFax */
    bool Downsample;
    float DownsampleThreshold;
    enum psdf_downsample_type {
        ds_Average,
        ds_Bicubic,
        ds_Subsample
    } DownsampleType;
#define psdf_ds_names\
        "Average", "Bicubic", "Subsample"
    bool Encode;
    const char *Filter;
    int Resolution;
    enum psdf_autofilter_type {
        af_Jpeg,
        af_Jpeg2000
    } AutoFilterStrategy;
#define psdf_afs_names\
        "JPEG", "JPEG2000"
    const stream_template *filter_template;
} psdf_image_params;

/* Complete distiller parameters. */
typedef struct psdf_distiller_params_s {

    /* General parameters */

    bool ASCII85EncodePages;
    enum psdf_auto_rotate_pages {
        arp_None,
        arp_All,
        arp_PageByPage
    } AutoRotatePages;
#define psdf_arp_names\
        "None", "All", "PageByPage"
    enum psdf_binding {
        binding_Left,
        binding_Right
    } Binding;
#define psdf_binding_names\
        "Left", "Right"
    bool CompressPages;
    enum psdf_default_rendering_intent {
        ri_Default,
        ri_Perceptual,
        ri_Saturation,
        ri_RelativeColorimetric,
        ri_AbsoluteColorimetric
    } DefaultRenderingIntent;
#define psdf_ri_names\
        "Default", "Perceptual", "Saturation", "RelativeColorimetric",\
        "AbsoluteColorimetric"
    bool DetectBlends;
    bool DoThumbnails;
    int64_t ImageMemory;
    bool LockDistillerParams;
    bool LZWEncodePages;
    int OPM;
    bool PreserveOPIComments;
    bool UseFlateCompression;

    /* Color processing parameters */

    gs_const_string CalCMYKProfile;
    gs_const_string CalGrayProfile;
    gs_const_string CalRGBProfile;
    gs_const_string sRGBProfile;
    enum psdf_color_conversion_strategy {
        ccs_LeaveColorUnchanged,
        ccs_UseDeviceIndependentColor,
        ccs_UseDeviceIndependentColorForImages,
        ccs_sRGB,
        ccs_CMYK,
        ccs_Gray,
        ccs_RGB,
        ccs_ByObjectType
    } ColorConversionStrategy;
#define psdf_ccs_names\
        "LeaveColorUnchanged", \
        "UseDeviceIndependentColor", "UseDeviceIndependentColorForImages",\
        "sRGB", "CMYK", "Gray", "RGB", "ByObjectType"

    bool PreserveHalftoneInfo;
    bool PreserveOverprintSettings;
    enum psdf_transfer_function_info {
        tfi_Preserve,
        tfi_Apply,
        tfi_Remove
    } TransferFunctionInfo;
#define psdf_tfi_names\
        "Preserve", "Apply", "Remove"
    enum psdf_ucr_and_bg_info {
        ucrbg_Preserve,
        ucrbg_Remove
    } UCRandBGInfo;
#define psdf_ucrbg_names\
        "Preserve", "Remove"

    /* Color sampled image parameters */

    psdf_image_params ColorImage;
    bool ConvertCMYKImagesToRGB;
    bool ConvertImagesToIndexed;

    /* Grayscale sampled image parameters */

    psdf_image_params GrayImage;

    /* Monochrome sampled image parameters */

    psdf_image_params MonoImage;

    /* Font outlining parameters */

    gs_param_string_array AlwaysOutline;
    gs_param_string_array NeverOutline;

    /* Font embedding parameters */

    gs_param_string_array AlwaysEmbed;
    gs_param_string_array NeverEmbed;
    enum psdf_cannot_embed_font_policy {
        cefp_OK,
        cefp_Warning,
        cefp_Error
    } CannotEmbedFontPolicy;
#define psdf_cefp_names\
        "OK", "Warning", "Error"
    bool EmbedAllFonts;
    int MaxSubsetPct;
    bool SubsetFonts;
    bool PassThroughJPEGImages;
    bool PassThroughJPXImages;
    gs_param_string PSDocOptions;
    gs_param_string_array PSPageOptions;
    bool PSPageOptionsWrap;
} psdf_distiller_params;

/* Declare templates for default image compression filters. */
extern const stream_template s_CFE_template;
extern const stream_template s_zlibE_template;

/* define some macros to use in setting up the default values of the pdfwrite device */
#define psdf_general_param_defaults(ascii)\
    ascii,	    /* ASCIIEncodePages */ \
    arp_None,	    /* Auto-RotatePages */ \
    binding_Left,   /* Binding */\
    1,		    /* CompressPages (true) */ \
    ri_Default,	    /* rendering intent */ \
    1,		    /* DetectBlends (true) */ \
    0,		    /* DoThumbnails (false) */ \
    500000,	    /* ImageMemory */ \
    0,		    /* LockDistillerParams (false) */ \
    0,		    /* LZWEncodePages (false) */ \
    0,		    /* Overprintmode (OPM) */ \
    0,		    /* PreserveOPIComments (false) */ \
    1,		    /* UseFlateCompression (true) */ \
        /* Color processing parameters */\
    {0},	    /* calCMYKProfile */ \
    {0},	    /* CalGrayProfile */ \
    {0},	    /* calRGBProfile */ \
    {0},	    /* sRGBProfile */ \
    ccs_LeaveColorUnchanged, /* ColorConversionStrategy */ \
    0,		    /* PreserveHalftoneInfo (false) */ \
    0,		    /* PreserveOverprintSettings (false) */ \
    tfi_Preserve,   /* TransferFunctionInfo */ \
    ucrbg_Remove    /* UCRandBGInfo */

#define psdf_color_image_param_defaults\
  { NULL,	    /* ACSDict (JPEG) */ \
    0,		    /* AntiAlias (false) */ \
    0,		    /* AutoFilter (false) */ \
    -1,		    /* Depth */ \
    NULL,	    /* Dict (JPEG or CCITTFax) */ \
    0,		    /* Downsample (false) */ \
    1.5,	    /* Donwsample threshold */ \
    ds_Bicubic,   /* Downsample type */ \
    1,		    /* Encode (true) */ \
    0,  /* compression biccufilter */ \
    150,	    /* Downsample resolution */ \
    af_Jpeg,	    /* AutoFilterStrategy */ \
    &s_zlibE_template	/* Filter stream template */ \
    },  0,	    /* ConvertCMYKImagesToRGB (false) */ \
    1		    /* ConvertImagesToIndexed (true) */

#define psdf_gray_image_param_defaults\
  { NULL,	    /* ACSDict (JPEG) */ \
    0,		    /* AntiAlias (false) */ \
    0,		    /* AutoFilter (false) */ \
    -1,		    /* Depth */ \
    NULL,	    /* Dict (JPEG or CCITTFax) */ \
    0,		    /* Downsample (false) */ \
    1.5,	    /* Donwsample threshold */ \
    ds_Subsample,   /* Downsample type */ \
    1,		    /* Encode (true) */ \
    0,  /* compression filter */ \
    150,	    /* Downsample resolution */ \
    af_Jpeg,	    /* AutoFilterStrategy */ \
    &s_zlibE_template	/* Filter stream template */ \
     }

#define psdf_mono_image_param_defaults\
  { NULL,	    /* ACSDict (JPEG) */ \
    0,		    /* AntiAlias (false) */ \
    0,		    /* AutoFilter (false) */ \
    -1,		    /* Depth */ \
    NULL,	    /* Dict (JPEG or CCITTFax) */ \
    0,		    /* Downsample (false) */ \
    2.0,	    /* Donwsample threshold */ \
    ds_Subsample,   /* Downsample type */ \
    1,		    /* Encode (true) */ \
    "CCITTFaxEncode",  /* compression filter */ \
    300,	    /* Downsample resolution */ \
    af_Jpeg,	    /* AutoFilterStrategy */ \
    &s_CFE_template	/* Filter stream template */ \
    }

#define psdf_font_param_defaults\
    {0},	    /* AlwaysOutline (array of names) */ \
    {0},	    /* NeverOutline (array of names) */ \
    {0},	    /* AlwaysEmbed (array of names) */ \
    {0},	    /* NeverEmbed (array of names) */ \
    cefp_Warning,   /* CannotEmbedFontPolicy */ \
    1,		    /* EmbedAllFonts (true) */ \
    100,	    /* Max Subset Percent */ \
    1		    /* Subset Fonts (true) */

#define psdf_JPEGPassThrough_param_defaults\
    1           /* PassThroughJPEGImages */
#define psdf_JPXPassThrough_param_defaults\
    1           /* PassThroughJPXImages */

#define psdf_PSOption_param_defaults\
    {0},        /* PSDocOptions */\
    {0}         /* PSPageOptions */

/* Define PostScript/PDF versions, corresponding roughly to Adobe versions. */
typedef enum {
    psdf_version_level1 = 1000,	/* Red Book Level 1 */
    psdf_version_level1_color = 1100,	/* Level 1 + colorimage + CMYK color */
    psdf_version_level2 = 2000,	/* Red Book Level 2 */
    psdf_version_level2_with_TT = 2010,	/* Adobe release 2010 with Type 42 fonts */
    psdf_version_level2_plus = 2017,	/* Adobe release 2017 */
    psdf_version_ll3 = 3010	/* LanguageLevel 3, release 3010 */
} psdf_version;

/* Define the extended device structure. */
#define gx_device_psdf_common\
        gx_device_vector_common;\
        psdf_version version;\
        bool binary_ok;		/* derived from ASCII85EncodePages */\
        bool HaveCFF;\
        bool HaveTrueTypes;\
        bool HaveCIDSystem;\
        double ParamCompatibilityLevel;\
        bool JPEG_PassThrough;\
        bool JPX_PassThrough;\
        psdf_distiller_params params

typedef struct gx_device_psdf_s {
    gx_device_psdf_common;
} gx_device_psdf;

#define psdf_initial_values(version, ascii)\
        vector_initial_values,\
        version,\
        !(ascii),\
        true,\
        true,\
        false,\
        1.3,\
        0,\
        0,\
         { psdf_general_param_defaults(ascii),\
           psdf_color_image_param_defaults,\
           psdf_gray_image_param_defaults,\
           psdf_mono_image_param_defaults,\
           psdf_font_param_defaults,\
           psdf_JPEGPassThrough_param_defaults,\
           psdf_JPXPassThrough_param_defaults,\
           psdf_PSOption_param_defaults\
         }
/* st_device_psdf is never instantiated per se, but we still need to */
/* extern its descriptor for the sake of subclasses. */
extern_st(st_device_psdf);
#define public_st_device_psdf()	/* in gdevpsdu.c */\
  BASIC_PTRS(device_psdf_ptrs) {\
    GC_OBJ_ELT2(gx_device_psdf, params.ColorImage.ACSDict,\
                params.ColorImage.Dict),\
    GC_CONST_STRING_ELT(gx_device_psdf, params.CalCMYKProfile),\
    GC_CONST_STRING_ELT(gx_device_psdf, params.CalGrayProfile),\
    GC_CONST_STRING_ELT(gx_device_psdf, params.CalRGBProfile),\
    GC_CONST_STRING_ELT(gx_device_psdf, params.sRGBProfile),\
    GC_OBJ_ELT2(gx_device_psdf, params.GrayImage.ACSDict,\
                params.GrayImage.Dict),\
    GC_OBJ_ELT2(gx_device_psdf, params.MonoImage.ACSDict,\
                params.MonoImage.Dict),\
    GC_OBJ_ELT2(gx_device_psdf, params.AlwaysOutline.data,\
                params.NeverOutline.data),\
    GC_OBJ_ELT2(gx_device_psdf, params.AlwaysEmbed.data,\
                params.NeverEmbed.data),\
    GC_CONST_STRING_ELT(gx_device_psdf, params.PSDocOptions)\
  };\
  gs_public_st_basic_super_final(st_device_psdf, gx_device_psdf,\
    "gx_device_psdf", device_psdf_ptrs, device_psdf_data,\
    &st_device_vector, 0, gx_device_finalize)
#define st_device_psdf_max_ptrs (st_device_vector_max_ptrs + 14)

/* Get/put parameters. */
int gdev_psdf_get_param(gx_device *dev, char *Param, void *list);
dev_proc_get_params(gdev_psdf_get_params);
dev_proc_put_params(gdev_psdf_put_params);

dev_proc_dev_spec_op(gdev_psdf_dev_spec_op);
/* ---------------- Vector implementation procedures ---------------- */

        /* gs_gstate */
int psdf_setlinewidth(gx_device_vector * vdev, double width);
int psdf_setlinecap(gx_device_vector * vdev, gs_line_cap cap);
int psdf_setlinejoin(gx_device_vector * vdev, gs_line_join join);
int psdf_setmiterlimit(gx_device_vector * vdev, double limit);
int psdf_setdash(gx_device_vector * vdev, const float *pattern,
                 uint count, double offset);
int psdf_setflat(gx_device_vector * vdev, double flatness);
int psdf_setlogop(gx_device_vector * vdev, gs_logical_operation_t lop,
                  gs_logical_operation_t diff);

        /* Paths */
#define psdf_dopath gdev_vector_dopath
int psdf_dorect(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1,
                fixed y1, gx_path_type_t type);
int psdf_beginpath(gx_device_vector * vdev, gx_path_type_t type);
int psdf_moveto(gx_device_vector * vdev, double x0, double y0,
                double x, double y, gx_path_type_t type);
int psdf_lineto(gx_device_vector * vdev, double x0, double y0,
                double x, double y, gx_path_type_t type);
int psdf_curveto(gx_device_vector * vdev, double x0, double y0,
                 double x1, double y1, double x2,
                 double y2, double x3, double y3, gx_path_type_t type);
int psdf_closepath(gx_device_vector * vdev, double x0, double y0,
                   double x_start, double y_start, gx_path_type_t type);

/* ---------------- Binary (image) data procedures ---------------- */

/* Define the structure for writing binary data. */
typedef struct psdf_binary_writer_s {
    gs_memory_t *memory;
    stream *target;		/* underlying stream */
    stream *strm;
    gx_device_psdf *dev;	/* may be unused */
    /* Due to recently introduced field cos_object_t::input_strm
     * the binary writer may be simplified significantly.
     * Keeping the old structure until we have time
     * for this optimization.
     */
} psdf_binary_writer;
extern_st(st_psdf_binary_writer);
#define public_st_psdf_binary_writer() /* in gdevpsdu.c */\
  gs_public_st_ptrs3(st_psdf_binary_writer, psdf_binary_writer,\
    "psdf_binary_writer", psdf_binary_writer_enum_ptrs,\
    psdf_binary_writer_reloc_ptrs, target, strm, dev)
#define psdf_binary_writer_max_ptrs 3

/* Begin writing binary data. */
int psdf_begin_binary(gx_device_psdf * pdev, psdf_binary_writer * pbw);

/* Add an encoding filter.  The client must have allocated the stream state, */
/* if any, using pdev->v_memory. */
int psdf_encode_binary(psdf_binary_writer * pbw,
                       const stream_template * template, stream_state * ss);

/* Add a 2-D CCITTFax encoding filter. */
/* Set EndOfBlock iff the stream is not ASCII85 encoded. */
int psdf_CFE_binary(psdf_binary_writer * pbw, int w, int h, bool invert);

/*
 * Acquire parameters, and optionally set up the filter for, a DCTEncode
 * filter.  This is a separate procedure so it can be used to validate
 * filter parameters when they are set, rather than waiting until they are
 * used.  pbw = NULL means just set up the stream state.
 */
int psdf_DCT_filter(gs_param_list *plist /* may be NULL */,
                    stream_state /*stream_DCTE_state*/ *st,
                    int Columns, int Rows, int Colors,
                    psdf_binary_writer *pbw /* may be NULL */);

/* Decive whether to convert an image to RGB. */
bool psdf_is_converting_image_to_RGB(const gx_device_psdf * pdev,
                const gs_gstate * pgs, const gs_pixel_image_t * pim);

/* Set up compression and downsampling filters for an image. */
/* Note that this may modify the image parameters. */
/* If pctm is NULL, downsampling is not used. */
/* pgs only provides UCR and BG information for CMYK => RGB conversion. */
int psdf_setup_image_filters(gx_device_psdf *pdev, psdf_binary_writer *pbw,
                             gs_pixel_image_t *pim, const gs_matrix *pctm,
                             const gs_gstate * pgs, bool lossless,
                             bool in_line);

int new_setup_image_filters(gx_device_psdf *pdev, psdf_binary_writer *pbw,
                             gs_pixel_image_t *pim, const gs_matrix *pctm,
                             const gs_gstate * pgs, bool lossless,
                             bool in_line, bool colour_conversion);

/* Set up compression filters for a lossless image, with no downsampling, */
/* no color space conversion, and only lossless filters. */
/* Note that this may modify the image parameters. */
int psdf_setup_lossless_filters(gx_device_psdf *pdev, psdf_binary_writer *pbw,
                                gs_pixel_image_t *pim, bool in_line);

int new_setup_lossless_filters(gx_device_psdf *pdev, psdf_binary_writer *pbw,
                                gs_pixel_image_t *pim, bool in_line, bool colour_conversion, const gs_matrix *pctm, gs_gstate * pgs);


int new_resize_input(psdf_binary_writer *pbw, int width, int num_comps, int bpc_in, int bpc_out);

/* Finish writing binary data. */
int psdf_end_binary(psdf_binary_writer * pbw);

/* Set up image compression chooser. */
int psdf_setup_compression_chooser(psdf_binary_writer *pbw,
                                   gx_device_psdf *pdev,
                                   int width, int height, int depth,
                                   int bits_per_sample);

/* Set up an "image to mask" filter. */
int psdf_setup_image_to_mask_filter(psdf_binary_writer *pbw, gx_device_psdf *pdev,
                                    int width, int height, int input_width,
                                    int depth, int bits_per_sample, uint *MaskColor);

/* Set up an image colors filter. */
int psdf_setup_image_colors_filter(psdf_binary_writer *pbw,
                                   gx_device_psdf *pdev,
                                   const gs_pixel_image_t *input_pim,
                                   gs_pixel_image_t * pim,
        const gs_gstate *pgs);

/* ---------------- Symbolic data printing ---------------- */

/* Backward compatibility definitions. */
#define psdf_write_string(s, str, size, print_ok)\
  s_write_ps_string(s, str, size, print_ok)
#define psdf_alloc_position_stream(ps, mem)\
  s_alloc_position_stream(ps, mem)
#define psdf_alloc_param_printer(pplist, ppp, s, mem)\
  s_alloc_param_printer(pplist, ppp, s, mem)
#define psdf_free_param_printer(plist)\
  s_free_param_printer(plist)

/* ---------------- Other procedures ---------------- */

/* Define the commands for setting the fill or stroke color. */
typedef struct psdf_set_color_commands_s {
    const char *setgray;
    const char *setrgbcolor;
    const char *setcmykcolor;
    const char *setcolorspace;
    const char *setcolor;
    const char *setcolorn;
} psdf_set_color_commands_t;
/* Define the standard color-setting commands (with PDF names). */
extern const psdf_set_color_commands_t
    psdf_set_fill_color_commands, psdf_set_stroke_color_commands;

/*
 * Adjust a gx_color_index to compensate for the fact that the bit pattern
 * of gx_color_index isn't representable.
 */
gx_color_index psdf_adjust_color_index(gx_device_vector *vdev,
                                       gx_color_index color);

/* Set the fill or stroke color. */
int psdf_set_color(gx_device_vector *vdev, const gx_drawing_color *pdc,
                   const psdf_set_color_commands_t *ppscc);
/* Round a double value to a specified precision. */
double psdf_round(double v, int precision, int radix);

/* stub to disable get_bits_rectangle */
dev_proc_get_bits_rectangle(psdf_get_bits_rectangle);

/* intercept and ignore overprint compositor creation */
dev_proc_composite(psdf_composite);

#endif /* gdevpsdf_INCLUDED */
