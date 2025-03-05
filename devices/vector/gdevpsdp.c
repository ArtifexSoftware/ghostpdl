/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* (Distiller) parameter handling for PostScript and PDF writers */
#include "string_.h"
#include "jpeglib_.h"		/* for sdct.h */
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"
#include "gxdevice.h"
#include "gsparamx.h"
#include "gdevpsdf.h"
#include "sbrotlix.h"
#include "strimpl.h"		/* for short-sighted compilers */
#include "scfx.h"
#include "sdct.h"
#include "slzwx.h"
#include "srlx.h"
#include "szlibx.h"
#include "gdevvec.h"

/* Define a (bogus) GC descriptor for gs_param_string. */
/* The only ones we use are GC-able and not persistent. */
gs_private_st_composite(st_gs_param_string, gs_param_string, "gs_param_string",
                        param_string_enum_ptrs, param_string_reloc_ptrs);
static
ENUM_PTRS_WITH(param_string_enum_ptrs, gs_param_string *pstr) return 0;
case 0: return ENUM_CONST_STRING(pstr);
ENUM_PTRS_END
static
RELOC_PTRS_WITH(param_string_reloc_ptrs, gs_param_string *pstr)
{
    gs_const_string str;

    str.data = pstr->data, str.size = pstr->size;
    RELOC_CONST_STRING_VAR(str);
    pstr->data = str.data;
}
RELOC_PTRS_END
gs_private_st_element(st_param_string_element, gs_param_string,
                      "gs_param_string[]", param_string_elt_enum_ptrs,
                      param_string_elt_reloc_ptrs, st_gs_param_string);

/* ---------------- Get/put Distiller parameters ---------------- */

/*
 * ColorConversionStrategy is supposed to affect output color space
 * according to the following table.  ****** NOT IMPLEMENTED YET ******

PS Input:  LeaveCU UseDIC           UseDICFI         sRGB
Gray art   Gray    CalGray/ICCBased Gray             Gray
Gray image Gray    CalGray/ICCBased CalGray/ICCBased Gray
RGB art    RGB     CalGray/ICCBased RGB              CalRGB/sRGB
RGB image  RGB     CalGray/ICCBased CalRGB/ICCBased  CalRGB/sRGB
CMYK art   CMYK    LAB/ICCBased     CMYK             CalRGB/sRGB
CMYK image CMYK    LAB/ICCBased     LAB/ICCBased     CalRGB/sRGB
CIE art    Cal/ICC Cal/ICC          Cal/ICC          CalRGB/sRGB
CIE image  Cal/ICC Cal/ICC          Cal/ICC          CalRGB/sRGB

 */

/*
 * The Always/NeverEmbed parameters are defined as being incremental.  Since
 * this isn't compatible with the general property of page devices that if
 * you do a currentpagedevice, doing a setpagedevice later will restore the
 * same state, we actually define the parameters in sets of 3:
 *	- AlwaysEmbed is used for incremental additions.
 *	- ~AlwaysEmbed is used for incremental deletions.
 *	- .AlwaysEmbed is used for the complete list.
 * and analogously for NeverEmbed.
 */

typedef struct psdf_image_filter_name_s {
    const char *pname;
    const stream_template *templat;
    psdf_version min_version;
} psdf_image_filter_name;

static const psdf_image_filter_name Poly_filters[] = {
    {"DCTEncode", &s_DCTE_template},
    {"FlateEncode", &s_zlibE_template, psdf_version_ll3},
    {"LZWEncode", &s_LZWE_template},
    {"BrotliEncode", &s_brotliE_template},
    {0, 0}
};

static const psdf_image_filter_name Mono_filters[] = {
    {"CCITTFaxEncode", &s_CFE_template},
    {"FlateEncode", &s_zlibE_template, psdf_version_ll3},
    {"LZWEncode", &s_LZWE_template},
    {"RunLengthEncode", &s_RLE_template},
    {"BrotliEncode", &s_brotliE_template},
    {0, 0}
};

typedef struct psdf_image_param_names_s {
    const char *ACSDict;	/* not used for mono */
    const char *Dict;
    const char *DownsampleType;
    float DownsampleThreshold_default;
    const psdf_image_filter_name *filter_names;
    const char *Filter;
    const char *AutoFilterStrategy;
    gs_param_item_t items[9];	/* AutoFilter (not used for mono), */
                                /* AntiAlias, */
                                /* Depth, Downsample, DownsampleThreshold, */
                                /* Encode, Resolution, AutoFilterStrategy, end marker */
} psdf_image_param_names_t;
#define pi(key, type, memb) { key, type, offset_of(psdf_image_params, memb) }
#define psdf_image_param_names(acs, aa, af, de, di, ds, dt, dst, dstd, e, f, fns, r, afs)\
    acs, di, dt, dstd, fns, f, afs, {\
      pi(af, gs_param_type_bool, AutoFilter),\
      pi(aa, gs_param_type_bool, AntiAlias),\
      pi(de, gs_param_type_int, Depth),\
      pi(ds, gs_param_type_bool, Downsample),\
      pi(dst, gs_param_type_float, DownsampleThreshold),\
      pi(e, gs_param_type_bool, Encode),\
      pi(r, gs_param_type_int, Resolution),\
      gs_param_item_end\
    }

static const psdf_image_param_names_t Color_names = {
    psdf_image_param_names(
        "ColorACSImageDict", "AntiAliasColorImages", "AutoFilterColorImages",
        "ColorImageDepth", "ColorImageDict",
        "DownsampleColorImages", "ColorImageDownsampleType",
        "ColorImageDownsampleThreshold", 1.5,
        "EncodeColorImages", "ColorImageFilter", Poly_filters,
        "ColorImageResolution", 0
    )
};
static const psdf_image_param_names_t Gray_names = {
    psdf_image_param_names(
        "GrayACSImageDict", "AntiAliasGrayImages", "AutoFilterGrayImages",
        "GrayImageDepth", "GrayImageDict",
        "DownsampleGrayImages", "GrayImageDownsampleType",
        "GrayImageDownsampleThreshold", 2.0,
        "EncodeGrayImages", "GrayImageFilter", Poly_filters,
        "GrayImageResolution", 0
    )
};
static const psdf_image_param_names_t Mono_names = {
    psdf_image_param_names(
        0, "AntiAliasMonoImages", 0,
        "MonoImageDepth", "MonoImageDict",
        "DownsampleMonoImages", "MonoImageDownsampleType",
        "MonoImageDownsampleThreshold", 2.0,
        "EncodeMonoImages", "MonoImageFilter", Mono_filters,
        "MonoImageResolution", 0
    )
};
static const psdf_image_param_names_t Color_names15 = {
    psdf_image_param_names(
        "ColorACSImageDict", "AntiAliasColorImages", "AutoFilterColorImages",
        "ColorImageDepth", "ColorImageDict",
        "DownsampleColorImages", "ColorImageDownsampleType",
        "ColorImageDownsampleThreshold", 1.5,
        "EncodeColorImages", "ColorImageFilter", Poly_filters,
        "ColorImageResolution", "ColorImageAutoFilterStrategy"
    )
};
static const psdf_image_param_names_t Gray_names15 = {
    psdf_image_param_names(
        "GrayACSImageDict", "AntiAliasGrayImages", "AutoFilterGrayImages",
        "GrayImageDepth", "GrayImageDict",
        "DownsampleGrayImages", "GrayImageDownsampleType",
        "GrayImageDownsampleThreshold", 2.0,
        "EncodeGrayImages", "GrayImageFilter", Poly_filters,
        "GrayImageResolution", "GrayImageAutoFilterStrategy"
    )
};
#undef pi
static const char *const AutoRotatePages_names[] = {
    psdf_arp_names, 0
};
static const char *const ColorConversionStrategy_names[] = {
    psdf_ccs_names, 0
};
static const char *const DownsampleType_names[] = {
    psdf_ds_names, 0
};
static const char *const AutoFilterStrategy_names[] = {
    psdf_afs_names, 0
};
static const char *const Binding_names[] = {
    psdf_binding_names, 0
};
static const char *const DefaultRenderingIntent_names[] = {
    psdf_ri_names, 0
};
static const char *const TransferFunctionInfo_names[] = {
    psdf_tfi_names, 0
};
static const char *const UCRandBGInfo_names[] = {
    psdf_ucrbg_names, 0
};
static const char *const CannotEmbedFontPolicy_names[] = {
    psdf_cefp_names, 0
};

static const gs_param_item_t psdf_param_items[] = {
#define pi(key, type, memb) { key, type, offset_of(psdf_distiller_params, memb) }

    /* General parameters */

    pi("ASCII85EncodePages", gs_param_type_bool, ASCII85EncodePages),
    /* (AutoRotatePages) */
    /* (Binding) */
    pi("CompressPages", gs_param_type_bool, CompressPages),
    /* (DefaultRenderingIntent) */
    pi("DetectBlends", gs_param_type_bool, DetectBlends),
    pi("DoThumbnails", gs_param_type_bool, DoThumbnails),
    pi("ImageMemory", gs_param_type_size_t, ImageMemory),
    /* (LockDistillerParams) */
    pi("LZWEncodePages", gs_param_type_bool, LZWEncodePages),
    pi("OPM", gs_param_type_int, OPM),
    pi("PreserveHalftoneInfo", gs_param_type_bool, PreserveHalftoneInfo),
    pi("PreserveOPIComments", gs_param_type_bool, PreserveOPIComments),
    pi("PreserveOverprintSettings", gs_param_type_bool, PreserveOverprintSettings),
    /* (TransferFunctionInfo) */
    /* (UCRandBGInfo) */
    pi("UseFlateCompression", gs_param_type_bool, UseFlateCompression),
    pi("UseBrotliCompression", gs_param_type_bool, UseBrotliCompression),

    /* Color image processing parameters */

    pi("ConvertCMYKImagesToRGB", gs_param_type_bool, ConvertCMYKImagesToRGB),
    pi("ConvertImagesToIndexed", gs_param_type_bool, ConvertImagesToIndexed),

    /* Font embedding parameters */

    /* (CannotEmbedFontPolicy) */
    pi("EmbedAllFonts", gs_param_type_bool, EmbedAllFonts),
    pi("MaxSubsetPct", gs_param_type_int, MaxSubsetPct),
    pi("SubsetFonts", gs_param_type_bool, SubsetFonts),
    pi("PassThroughJPEGImages", gs_param_type_bool, PassThroughJPEGImages),
    pi("PassThroughJPXImages", gs_param_type_bool, PassThroughJPXImages),
    pi("PSPageOptionsWrap", gs_param_type_bool, PSPageOptionsWrap),

#undef pi
    gs_param_item_end
};

/* -------- Get parameters -------- */

static int
psdf_write_name(gs_param_list *plist, const char *key, const char *str)
{
    gs_param_string pstr;

    param_string_from_string(pstr, str);
    return param_write_name(plist, key, &pstr);
}

static int
psdf_write_string_param(gs_param_list *plist, const char *key,
                        const gs_const_string *pstr)
{
    gs_param_string ps;

    ps.data = pstr->data;
    ps.size = pstr->size;
    ps.persistent = false;
    return param_write_string(plist, key, &ps);
}

/*
 * Get an image Dict parameter.  Note that we return a default (empty)
 * dictionary if the parameter has never been set.
 */
static int
psdf_get_image_dict_param(gs_param_list * plist, const gs_param_name pname,
                          gs_c_param_list *plvalue)
{
    gs_param_dict dict;
    int code;

    if (pname == 0)
        return 0;
    dict.size = 12;		/* enough for all param dicts we know about */
    if ((code = param_begin_write_dict(plist, pname, &dict, false)) < 0)
        return code;
    if (plvalue != 0) {
        gs_c_param_list_read(plvalue);
        code = param_list_copy(dict.list, (gs_param_list *)plvalue);
    }
    param_end_write_dict(plist, pname, &dict);
    return code;
}

/* Get a set of image-related parameters. */
static int
psdf_get_image_params(gs_param_list * plist,
          const psdf_image_param_names_t * pnames, psdf_image_params * params)
{
    /* Skip AutoFilter for mono images. */
    const gs_param_item_t *items =
        (pnames->items[0].key == 0 ? pnames->items + 1 : pnames->items);
    int code;

    /*
     * We must actually return a value for every parameter, so that
     * all parameter names will be recognized as settable by -d or -s
     * from the command line.
     */
    code = gs_param_write_items(plist, params, NULL, items);
    if (code < 0)
        return code;

    code = psdf_get_image_dict_param(plist, pnames->ACSDict, params->ACSDict);
    if (code < 0)
        return code;

           /* (AntiAlias) */
           /* (AutoFilter) */
           /* (Depth) */
    code = psdf_get_image_dict_param(plist, pnames->Dict, params->Dict);
    if (code < 0)
        return code;

           /* (Downsample) */
    code = psdf_write_name(plist, pnames->DownsampleType,
                DownsampleType_names[params->DownsampleType]);
    if (code < 0)
        return code;

           /* (DownsampleThreshold) */
           /* (Encode) */
    code = psdf_write_name(plist, pnames->Filter,
                                   (params->Filter == 0 ?
                                    pnames->filter_names[0].pname :
                                    params->Filter));
    if (code < 0)
        return code;

           /* (Resolution) */
    if (pnames->AutoFilterStrategy != 0)
        code = psdf_write_name(plist, pnames->AutoFilterStrategy,
                AutoFilterStrategy_names[params->AutoFilterStrategy]);
    if (code < 0)
        return code;

    return code;
}

/* Get a font embedding parameter. */
static int
psdf_get_embed_param(gs_param_list *plist, gs_param_name allpname,
                     const gs_param_string_array *psa)
{
    int code = param_write_name_array(plist, allpname, psa);

    if (code >= 0)
        code = param_write_name_array(plist, allpname + 1, psa);
    return code;
}

/* Transfer a collection of parameters. */
static const byte xfer_item_sizes[] = {
    GS_PARAM_TYPE_SIZES(0)
};
/* Get parameters. */
static
int gdev_psdf_get_image_param(gx_device_psdf *pdev, const psdf_image_param_names_t *image_names,
                              psdf_image_params * params, char *Param, gs_param_list * plist)
{
    const gs_param_item_t *pi;
    int code;

    for (pi = image_names->items; pi->key != 0; ++pi) {
        if (strcmp(pi->key, Param) == 0) {
            const char *key = pi->key;
            const void *pvalue = (const void *)((const char *)params + pi->offset);
            int size = xfer_item_sizes[pi->type];
            gs_param_typed_value typed;

            memcpy(&typed.value, pvalue, size);
            typed.type = pi->type;
            code = (*plist->procs->xmit_typed) (plist, key, &typed);
            return code;
        }
    }
    /* We only have an ACSDict for color image parameters */
    if (image_names->ACSDict) {
        if (strcmp(Param, image_names->ACSDict) == 0)
            return psdf_get_image_dict_param(plist, image_names->ACSDict, params->ACSDict);
    }
    if (strcmp(Param, image_names->Dict) == 0)
        return psdf_get_image_dict_param(plist, image_names->Dict, params->Dict);

    if (strcmp(Param, image_names->DownsampleType) == 0)
        return psdf_write_name(plist, image_names->DownsampleType,
                DownsampleType_names[params->DownsampleType]);
    if (strcmp(Param, image_names->Filter) == 0)
        return psdf_write_name(plist, image_names->Filter,
                                   (params->Filter == 0 ?
                                    image_names->filter_names[0].pname :
                                    params->Filter));
    return_error(gs_error_undefined);
}
int
gdev_psdf_get_param(gx_device *dev, char *Param, void *list)
{
    gx_device_psdf *pdev = (gx_device_psdf *) dev;
    const psdf_image_param_names_t *image_names;
    const gs_param_item_t *pi;
    gs_param_list * plist = (gs_param_list *)list;
    int code = 0;

    code = gdev_vector_get_param(dev, Param, list);
    if (code != gs_error_undefined)
        return code;

    /* General parameters first */
    for (pi = psdf_param_items; pi->key != 0; ++pi) {
        if (strcmp(pi->key, Param) == 0) {
            const char *key = pi->key;
            const void *pvalue = (const void *)((const char *)&pdev + pi->offset);
            int size = xfer_item_sizes[pi->type];
            gs_param_typed_value typed;

            memcpy(&typed.value, pvalue, size);
            typed.type = pi->type;
            code = (*plist->procs->xmit_typed) (plist, key, &typed);
            return code;
        }
    }

    /* Color image parameters */
    if (pdev->ParamCompatibilityLevel >= 1.5)
        image_names = &Color_names15;
    else
        image_names = &Color_names;

    code = gdev_psdf_get_image_param(pdev, image_names, &pdev->params.ColorImage, Param, plist);
    if (code != gs_error_undefined)
        return code;

    /* Grey image parameters */
    if (pdev->ParamCompatibilityLevel >= 1.5)
        image_names = &Gray_names15;
    else
        image_names = &Gray_names;

    code = gdev_psdf_get_image_param(pdev, image_names, &pdev->params.GrayImage, Param, plist);
    if (code != gs_error_undefined)
        return code;

    /* Mono image parameters */
    code = gdev_psdf_get_image_param(pdev, &Mono_names, &pdev->params.MonoImage, Param, plist);
    if (code != gs_error_undefined)
        return code;

    if (strcmp(Param, "AutoRotatePages") == 0) {
        return(psdf_write_name(plist, "AutoRotatePages",
                AutoRotatePages_names[(int)pdev->params.AutoRotatePages]));
    }
    if (strcmp(Param, "Binding") == 0) {
        return(psdf_write_name(plist, "Binding",
                Binding_names[(int)pdev->params.Binding]));
    }
    if (strcmp(Param, "DefaultRenderingIntent") == 0) {
        return(psdf_write_name(plist, "DefaultRenderingIntent",
                DefaultRenderingIntent_names[(int)pdev->params.DefaultRenderingIntent]));
    }
    if (strcmp(Param, "TransferFunctionInfo") == 0) {
        return(psdf_write_name(plist, "TransferFunctionInfo",
                TransferFunctionInfo_names[(int)pdev->params.TransferFunctionInfo]));
    }
    if (strcmp(Param, "UCRandBGInfo") == 0) {
        return(psdf_write_name(plist, "UCRandBGInfo",
                UCRandBGInfo_names[(int)pdev->params.UCRandBGInfo]));
    }
    if (strcmp(Param, "ColorConversionStrategy") == 0) {
        return(psdf_write_name(plist, "ColorConversionStrategy",
                ColorConversionStrategy_names[(int)pdev->params.ColorConversionStrategy]));
    }
    if (strcmp(Param, "CalCMYKProfile") == 0) {
        return(psdf_write_string_param(plist, "CalCMYKProfile",
                                        &pdev->params.CalCMYKProfile));
    }
    if (strcmp(Param, "CalGrayProfile") == 0) {
        return(psdf_write_string_param(plist, "CalGrayProfile",
                                        &pdev->params.CalGrayProfile));
    }
    if (strcmp(Param, "CalRGBProfile") == 0) {
        return(psdf_write_string_param(plist, "CalRGBProfile",
                                        &pdev->params.CalRGBProfile));
    }
    if (strcmp(Param, "sRGBProfile") == 0) {
        return(psdf_write_string_param(plist, "sRGBProfile",
                                        &pdev->params.sRGBProfile));
    }
    if (strcmp(Param, ".AlwaysOutline") == 0) {
        return(psdf_get_embed_param(plist, ".AlwaysOutline", &pdev->params.AlwaysOutline));
    }
    if (strcmp(Param, ".NeverOutline") == 0) {
        return(psdf_get_embed_param(plist, ".NeverOutline", &pdev->params.NeverOutline));
    }
    if (strcmp(Param, ".AlwaysEmbed") == 0) {
        return(psdf_get_embed_param(plist, ".AlwaysEmbed", &pdev->params.AlwaysEmbed));
    }
    if (strcmp(Param, ".NeverEmbed") == 0) {
        return(psdf_get_embed_param(plist, ".NeverEmbed", &pdev->params.NeverEmbed));
    }
    if (strcmp(Param, "CannotEmbedFontPolicy") == 0) {
        return(psdf_write_name(plist, "CannotEmbedFontPolicy",
                CannotEmbedFontPolicy_names[(int)pdev->params.CannotEmbedFontPolicy]));
    }
    return_error(gs_error_undefined);
}

int
gdev_psdf_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_psdf *pdev = (gx_device_psdf *) dev;
    int code = gdev_vector_get_params(dev, plist);
    if (code < 0)
        return code;

    code = gs_param_write_items(plist, &pdev->params, NULL, psdf_param_items);
    if (code < 0)
        return code;

    /* General parameters */

    code = psdf_write_name(plist, "AutoRotatePages",
                AutoRotatePages_names[(int)pdev->params.AutoRotatePages]);
    if (code < 0)
        return code;

    code = psdf_write_name(plist, "Binding",
                Binding_names[(int)pdev->params.Binding]);
    if (code < 0)
        return code;

    code = psdf_write_name(plist, "DefaultRenderingIntent",
                DefaultRenderingIntent_names[(int)pdev->params.DefaultRenderingIntent]);
    if (code < 0)
        return code;

    code = psdf_write_name(plist, "TransferFunctionInfo",
                TransferFunctionInfo_names[(int)pdev->params.TransferFunctionInfo]);
    if (code < 0)
        return code;

    code = psdf_write_name(plist, "UCRandBGInfo",
                UCRandBGInfo_names[(int)pdev->params.UCRandBGInfo]);
    if (code < 0)
        return code;

    /* Color sampled image parameters */

    code = psdf_get_image_params(plist,
                        (pdev->ParamCompatibilityLevel >= 1.5 ? &Color_names15 : &Color_names),
                        &pdev->params.ColorImage);
    if (code < 0)
        return code;

    code = psdf_write_name(plist, "ColorConversionStrategy",
                ColorConversionStrategy_names[(int)pdev->params.ColorConversionStrategy]);
    if (code < 0)
        return code;

    code = psdf_write_string_param(plist, "CalCMYKProfile",
                                        &pdev->params.CalCMYKProfile);
    if (code < 0)
        return code;

    code = psdf_write_string_param(plist, "CalGrayProfile",
                                        &pdev->params.CalGrayProfile);
    if (code < 0)
        return code;

    code = psdf_write_string_param(plist, "CalRGBProfile",
                                        &pdev->params.CalRGBProfile);
    if (code < 0)
        return code;

    code = psdf_write_string_param(plist, "sRGBProfile",
                                        &pdev->params.sRGBProfile);
    if (code < 0)
        return code;

    /* Gray sampled image parameters */

    code = psdf_get_image_params(plist,
                        (pdev->ParamCompatibilityLevel >= 1.5 ? &Gray_names15 : &Gray_names),
                        &pdev->params.GrayImage);
    if (code < 0)
        return code;

    /* Mono sampled image parameters */

    code = psdf_get_image_params(plist, &Mono_names, &pdev->params.MonoImage);
    if (code < 0)
        return code;

    /* Font outlining parameters */

    code = psdf_get_embed_param(plist, ".AlwaysOutline", &pdev->params.AlwaysOutline);
    if (code < 0)
        return code;

    code = psdf_get_embed_param(plist, ".NeverOutline", &pdev->params.NeverOutline);
    if (code < 0)
        return code;

    /* Font embedding parameters */

    code = psdf_get_embed_param(plist, ".AlwaysEmbed", &pdev->params.AlwaysEmbed);
    if (code < 0)
        return code;

    code = psdf_get_embed_param(plist, ".NeverEmbed", &pdev->params.NeverEmbed);
    if (code < 0)
        return code;

    code = param_write_string_array(plist, "PSPageOptions", &pdev->params.PSPageOptions);
    if (code < 0)
        return code;

    code = psdf_write_name(plist, "CannotEmbedFontPolicy",
                CannotEmbedFontPolicy_names[(int)pdev->params.CannotEmbedFontPolicy]);

    return code;
}

/* -------- Put parameters -------- */

extern stream_state_proc_put_params(s_CF_put_params, stream_CF_state);
extern stream_state_proc_put_params(s_DCTE_put_params, stream_DCT_state);
typedef stream_state_proc_put_params((*ss_put_params_t), stream_state);

static int
psdf_read_string_param(gs_param_list *plist, const char *key,
                       gs_const_string *pstr, gs_memory_t *mem, int ecode)
{
    gs_param_string ps;
    int code;

    switch (code = param_read_string(plist, key, &ps)) {
    case 0: {
        uint size = ps.size;
        byte *data = gs_alloc_string(mem, size, "psdf_read_string_param");

        if (data == 0)
            return_error(gs_error_VMerror);
        memcpy(data, ps.data, size);
        pstr->data = data;
        pstr->size = size;
        break;
    }
    default:
        ecode = code;
    case 1:
        break;
    }
    return ecode;
}

/*
 * The arguments and return value for psdf_put_enum are different because
 * we must cast the value both going in and coming out.
 */
static int
psdf_put_enum(gs_param_list *plist, const char *key, int value,
              const char *const pnames[], int *pecode)
{
    *pecode = param_put_enum(plist, key, &value, pnames, *pecode);
    return value;
}

static int
psdf_CF_put_params(gs_param_list * plist, stream_state * st)
{
    stream_CFE_state *const ss = (stream_CFE_state *) st;

    (*s_CFE_template.set_defaults) (st);
    ss->K = -1;
    ss->BlackIs1 = true;
    return s_CF_put_params(plist, (stream_CF_state *) ss);
}

static int
psdf_DCT_put_params(gs_param_list * plist, stream_state * st)
{
    return psdf_DCT_filter(plist, st, 8 /*nominal*/, 8 /*ibid.*/, 3 /*ibid.*/,
                           NULL);
}

/* Put [~](Always|Never)Embed parameters. */
/* Returns 0 = OK, 1 = no paramewter specified, <0 = error. */
static int
param_read_embed_array(gs_param_list * plist, gs_param_name pname,
                       gs_param_string_array * psa)
{
    int code;

    psa->data = 0, psa->size = 0;
    switch (code = param_read_name_array(plist, pname, psa)) {
        default:
            param_signal_error(plist, pname, code);
        case 0:
        case 1:
            break;
    }
    return code;
}
static bool
param_string_eq(const gs_param_string *ps1, const gs_param_string *ps2)
{
    return !bytes_compare(ps1->data, ps1->size, ps2->data, ps2->size);
}
static int
add_embed(gs_param_string_array *prsa, const gs_param_string_array *psa,
          gs_memory_t *mem)
{
    uint i;
    gs_param_string *const rdata =
        (gs_param_string *)prsa->data; /* break const */
    uint count = prsa->size;

    for (i = 0; i < psa->size; ++i) {
        uint j;

        for (j = 0; j < count; ++j)
            if (param_string_eq(&psa->data[i], &rdata[j]))
                    break;
        if (j == count) {
            uint size = psa->data[i].size;
            byte *data = gs_alloc_string(mem, size, "add_embed");

            if (data == 0)
                return_error(gs_error_VMerror);
            memcpy(data, psa->data[i].data, size);
            rdata[count].data = data;
            rdata[count].size = size;
            rdata[count].persistent = false;
            count++;
        }
    }
    prsa->size = count;
    return 0;
}
static void
delete_embed(gs_param_string_array *prsa, const gs_param_string_array *pnsa,
             gs_memory_t *mem)
{
    uint i;
    gs_param_string *const rdata =
        (gs_param_string *)prsa->data; /* break const */
    uint count = prsa->size;

    for (i = pnsa->size; i-- > 0;) {
        uint j;

        for (j = count; j-- > 0;)
            if (param_string_eq(&pnsa->data[i], &rdata[j]))
                break;
        if (j + 1 != 0) {
            gs_free_const_string(mem, rdata[j].data, rdata[j].size,
                                 "delete_embed");
            rdata[j] = rdata[--count];
        }
    }
    prsa->size = count;
}
static int merge_embed(gs_param_string_array * psa, gs_param_string_array * asa,
                     gs_memory_t *mem)
{
    gs_param_string_array rsa;
    gs_param_string *rdata;
    int code;

    rdata = gs_alloc_struct_array(mem, psa->size + asa->size,
                                  gs_param_string,
                                  &st_param_string_element,
                                  "psdf_put_embed_param(update)");
    if (rdata == 0)
        return_error(gs_error_VMerror);
    if (psa->size > 0)
        memcpy(rdata, psa->data, psa->size * sizeof(*psa->data));
    rsa.data = rdata;
    rsa.size = psa->size;
    rsa.persistent = false;
    code = add_embed(&rsa, asa, mem);
    if (code < 0) {
        gs_free_object(mem, rdata, "psdf_put_embed_param(update)");
        return code;
    }
    gs_free_const_object(mem, psa->data, "psdf_put_embed_param(free)");
    *psa = rsa;
    return 0;
}

static int
psdf_put_embed_param(gs_param_list * plist, gs_param_name notpname,
                     gs_param_name pname, gs_param_string_array * psa,
                     gs_memory_t *mem, int ecode)
{
    gs_param_name allpname = pname + 1;
    gs_param_string_array sa, nsa, asa;
    int code;

    mem = gs_memory_stable(mem);
    code  = param_read_embed_array(plist, pname, &sa);
    if (code < 0)
        return code;
    if (code == 0) {
        /* Optimize for sa == *psa. */
        int i;

        if (sa.size == psa->size) {
            for (i = 0; i < sa.size; ++i) {
                if (!param_string_eq(&sa.data[i], &psa->data[i]))
                    break;
     }
        } else
            i = -1;
        if (i == sa.size) {
            /* equal, no-op. */
        } else {
            delete_embed(psa, psa, mem);
            code = merge_embed(psa, &sa, mem);
            if (code < 0)
                return code;
        }
    }
    code = param_read_embed_array(plist, notpname, &nsa);
    if (code < 0)
        return code;
    if (nsa.data != 0)
        delete_embed(psa, &nsa, mem);
    code = param_read_embed_array(plist, allpname, &asa);
    if (code < 0)
        return code;
    if (asa.data != 0) {
        code = merge_embed(psa, &asa, mem);
        if (code < 0)
            return code;
    }
    if (psa->data)
        psa->data = gs_resize_object(mem, (gs_param_string *)psa->data, psa->size,
                                        "psdf_put_embed_param(resize)");
    return 0;
}

/* Put an image Dict parameter. */
static int
psdf_put_image_dict_param(gs_param_list * plist, const gs_param_name pname,
                          gs_c_param_list **pplvalue,
                          const stream_template * templat,
                          ss_put_params_t put_params, gs_memory_t * mem)
{
    gs_param_dict dict;
    gs_c_param_list *plvalue = *pplvalue;
    int code;

    mem = gs_memory_stable(mem);
    switch (code = param_begin_read_dict(plist, pname, &dict, false)) {
        default:
            param_signal_error(plist, pname, code);
            return code;
        case 1:
            return 0;
        case 0: {
            plvalue = gs_c_param_list_alloc(mem, pname);
            if (plvalue == 0)
                return_error(gs_error_VMerror);
            gs_c_param_list_write(plvalue, mem);
            code = param_list_copy((gs_param_list *)plvalue,
                                   dict.list);
            if (code < 0) {
                gs_c_param_list_release(plvalue);
                gs_free_object(mem, plvalue, pname);
                plvalue = *pplvalue;
            }
        }
        param_end_read_dict(plist, pname, &dict);
        break;
    }
    if (plvalue != *pplvalue) {
        if (*pplvalue)
            gs_c_param_list_release(*pplvalue);
        *pplvalue = plvalue;
    }
    return code;
}

/* Put a set of image-related parameters. */
static int
psdf_put_image_params(const gx_device_psdf * pdev, gs_param_list * plist,
                      const psdf_image_param_names_t * pnames,
                      psdf_image_params * params, int ecode)
{
    gs_param_string fs;
    /*
     * Since this procedure can be called before the device is open,
     * we must use pdev->memory rather than pdev->v_memory.
     */
    gs_memory_t *mem = gs_memory_stable(pdev->memory);
    gs_param_name pname;
    /* Skip AutoFilter for mono images. */
    const gs_param_item_t *items =
        (pnames->items[0].key == 0 ? pnames->items + 1 : pnames->items);
    int code = gs_param_read_items(plist, params, items, mem);
    if (code < 0)
        ecode = code;

    if ((pname = pnames->ACSDict) != 0) {
        code = psdf_put_image_dict_param(plist, pname, &params->ACSDict,
                                         &s_DCTE_template,
                                         psdf_DCT_put_params, mem);
        if (code < 0)
            ecode = code;
    }
    /* (AntiAlias) */
    /* (AutoFilter) */
    /* (Depth) */
    if ((pname = pnames->Dict) != 0) {
        const stream_template *templat;
        ss_put_params_t put_params;

        /* Hack to determine what kind of a Dict we want: */
        if (pnames->Dict[0] == 'M')
            templat = &s_CFE_template,
                put_params = psdf_CF_put_params;
        else
            templat = &s_DCTE_template,
                put_params = psdf_DCT_put_params;
        code = psdf_put_image_dict_param(plist, pname, &params->Dict,
                                         templat, put_params, mem);
        if (code < 0)
            ecode = code;
    }
    /* (Downsample) */
    params->DownsampleType = (enum psdf_downsample_type)
        psdf_put_enum(plist, pnames->DownsampleType,
                      (int)params->DownsampleType, DownsampleType_names,
                      &ecode);
    /* (DownsampleThreshold) */
    /* (Encode) */
    /* Process AutoFilterStrategy before Filter, because it sets defaults
       for the latter. */
    if (pnames->AutoFilterStrategy != NULL) {
        switch (code = param_read_string(plist, pnames->AutoFilterStrategy, &fs)) {
            case 0:
                {
                    const psdf_image_filter_name *pn = pnames->filter_names;
                    const char *param_name = 0;

                    if (gs_param_string_eq(&fs, "JPEG")) {
                        params->AutoFilterStrategy = af_Jpeg;
                        param_name = "DCTEncode";
                    } else {
                        if (gs_param_string_eq(&fs, "JPEG2000")) {
                            params->AutoFilterStrategy = af_Jpeg2000;
                            param_name = "JPXEncode";
                        } else {
                            ecode = gs_error_rangecheck;
                            goto ipe1;
                        }
                    }
                    while (pn->pname != 0 && !gs_param_string_eq(&fs, param_name))
                        pn++;
                    if (pn->pname != 0 && pn->min_version <= pdev->version) {
                        params->Filter = pn->pname;
                        params->filter_template = pn->templat;
                    }
                    break;
                }
            default:
                ecode = code;
            ipe1:param_signal_error(plist, pnames->AutoFilterStrategy, ecode);
            case 1:
                break;
        }
    }

    switch (code = param_read_string(plist, pnames->Filter, &fs)) {
        case 0:
            {
                const psdf_image_filter_name *pn = pnames->filter_names;

                while (pn->pname != 0 && !gs_param_string_eq(&fs, pn->pname))
                    pn++;
                if (pn->pname == 0 || pn->min_version > pdev->version) {
                    ecode = gs_error_rangecheck;
                    goto ipe;
                }
                params->Filter = pn->pname;
                params->filter_template = pn->templat;
                break;
            }
        default:
            ecode = code;
          ipe:param_signal_error(plist, pnames->Filter, ecode);
        case 1:
            break;
    }
    /* (Resolution) */
    if (ecode >= 0) {		/* Force parameters to acceptable values. */
        if (params->Resolution < 1)
            params->Resolution = 1;
        if (params->DownsampleThreshold < 1 ||
            params->DownsampleThreshold > 10)
            params->DownsampleThreshold = pnames->DownsampleThreshold_default;
        switch (params->Depth) {
            default:
                params->Depth = -1;
            case 1:
            case 2:
            case 4:
            case 8:
            case -1:
                break;
        }
    }
    return ecode;
}

/* This is a convenience routine. There doesn't seem to be any way to have a param_string_array
 * enumerated for garbage collection, and we have (currently) three members of the psdf_distiller_params
 * structure which store param_string_array. If the interpreter is using garbage collection then there
 * is the potential for the array, or its contents, to be relocated or freed while we are still
 * maintaining pointers to them, unless we enumerate the pointers.
 * Instead, we'll copy the string data from the interpreter, make our own param_string_array, and
 * manage the memory ourselves. This allows us to move the data into non-GC memory which is preferable
 * anyway.
 */
static int psdf_copy_param_string_array(gs_memory_t *mem, gs_param_list * plist, gs_param_string_array *sa, gs_param_string_array *da)
{
    if (sa->size > 0) {
        int ix;

        if (da->data != NULL) {
            for (ix = 0; ix < da->size;ix++)
                gs_free_object(mem->non_gc_memory, (byte *)da->data[ix].data, "freeing old string array copy");
            gs_free_object(mem->non_gc_memory, (byte *)da->data, "freeing old string array");
        }
        da->data = (const gs_param_string *)gs_alloc_bytes(mem->non_gc_memory, sa->size * sizeof(gs_param_string), "allocate new string array");
        if (da->data == NULL)
            return_error(gs_note_error(gs_error_VMerror));
        memset((byte *)da->data, 0x00, sa->size * sizeof(gs_param_string));
        da->size = sa->size;
        da->persistent = false;

        for(ix=0;ix < sa->size;ix++) {
            ((gs_param_string *)&da->data[ix])->data = gs_alloc_bytes(mem->non_gc_memory, sa->data[ix].size, "allocate new strings");
            if (da->data[ix].data == NULL)
                return_error(gs_note_error(gs_error_VMerror));
            memcpy((byte *)(da->data[ix].data), sa->data[ix].data, sa->data[ix].size);
            ((gs_param_string *)&da->data[ix])->size = sa->data[ix].size;
            ((gs_param_string *)&da->data[ix])->persistent = false;
        }
        gs_free_object(plist->memory, (byte *)sa->data, "freeing temporary param string array");
        sa->data = NULL;
        sa->size = 0;
    }
    return 0;
}

static int psdf_read_copy_param_string_array(gs_memory_t *mem, gs_param_list * plist, const char *Key, gs_param_string_array *da)
{
    gs_param_string_array sa;
    int code;

    code  = param_read_embed_array(plist, Key, &sa);
    if (code < 0)
        return code;

    if(sa.size)
        code = psdf_copy_param_string_array(mem, plist, &sa, da);

    return code;
}

/* Put parameters. */
int
gdev_psdf_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_psdf *pdev = (gx_device_psdf *) dev;
    gs_memory_t *mem =
        (pdev->v_memory ? pdev->v_memory : dev->memory);
    int ecode, code = 0;
    psdf_distiller_params params;

    params = pdev->params;

    /*
     * If LockDistillerParams was true and isn't being set to false,
     * ignore all other psdf parameters.  However, do not ignore the
     * standard device parameters.
     */
    ecode = code = param_read_bool(plist, "LockDistillerParams",
                                   &params.LockDistillerParams);

    if ((pdev->params.LockDistillerParams && params.LockDistillerParams)) {
        /* If we are not going to use the parameters we must still read them
         * in order to use up all the keys, otherwise, if we are being called
         * from .installpagedevice, it will error out on unconsumed keys. We
         * use a dummy params structure to read into, but we must make sure any
         * pointers are not copied from the real params structure, or they get
         * overwritten.
         */
        params.CalCMYKProfile.size = params.CalGrayProfile.size = params.CalRGBProfile.size = params.sRGBProfile.size = 0;
        params.CalCMYKProfile.data = 0;params.CalGrayProfile.data = params.CalRGBProfile.data = params.sRGBProfile.data = (byte *)0;

        params.ColorImage.ACSDict = params.ColorImage.Dict = 0;
        params.GrayImage.ACSDict = params.GrayImage.Dict = 0;
        params.MonoImage.ACSDict = params.MonoImage.Dict = 0;
        params.AlwaysOutline.data = params.NeverOutline.data = NULL;
        params.AlwaysOutline.size = params.NeverOutline.size = 0;
        params.AlwaysEmbed.data = params.NeverEmbed.data = 0;
        params.AlwaysEmbed.size = params.AlwaysEmbed.persistent = params.NeverEmbed.size = params.NeverEmbed.persistent = 0;
        params.PSPageOptions.data = NULL;
        params.PSPageOptions.size = 0;
    }

    /* General parameters. */

    code = gs_param_read_items(plist, &params, psdf_param_items, NULL);
    if (code < 0)
        return code;

    params.AutoRotatePages = (enum psdf_auto_rotate_pages)
        psdf_put_enum(plist, "AutoRotatePages", (int)params.AutoRotatePages,
                      AutoRotatePages_names, &ecode);
    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    params.Binding = (enum psdf_binding)
        psdf_put_enum(plist, "Binding", (int)params.Binding,
                      Binding_names, &ecode);
    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    params.DefaultRenderingIntent = (enum psdf_default_rendering_intent)
        psdf_put_enum(plist, "DefaultRenderingIntent",
                      (int)params.DefaultRenderingIntent,
                      DefaultRenderingIntent_names, &ecode);
    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    params.TransferFunctionInfo = (enum psdf_transfer_function_info)
        psdf_put_enum(plist, "TransferFunctionInfo",
                      (int)params.TransferFunctionInfo,
                      TransferFunctionInfo_names, &ecode);
    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    params.UCRandBGInfo = (enum psdf_ucr_and_bg_info)
        psdf_put_enum(plist, "UCRandBGInfo", (int)params.UCRandBGInfo,
                      UCRandBGInfo_names, &ecode);
    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    ecode = param_put_bool(plist, "UseFlateCompression",
                           &params.UseFlateCompression, ecode);

    /* Color sampled image parameters */

    ecode = psdf_put_image_params(pdev, plist,
                    (pdev->ParamCompatibilityLevel >= 1.5 ? &Color_names15 : &Color_names),
                                  &params.ColorImage, ecode);
    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    params.ColorConversionStrategy = (enum psdf_color_conversion_strategy)
        psdf_put_enum(plist, "ColorConversionStrategy",
                      (int)params.ColorConversionStrategy,
                      ColorConversionStrategy_names, &ecode);
    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    ecode = psdf_read_string_param(plist, "CalCMYKProfile",
                                   &params.CalCMYKProfile, mem, ecode);
    ecode = psdf_read_string_param(plist, "CalGrayProfile",
                                   &params.CalGrayProfile, mem, ecode);
    ecode = psdf_read_string_param(plist, "CalRGBProfile",
                                   &params.CalRGBProfile, mem, ecode);
    ecode = psdf_read_string_param(plist, "sRGBProfile",
                                   &params.sRGBProfile, mem, ecode);

    /* Gray sampled image parameters */

    ecode = psdf_put_image_params(pdev, plist,
                    (pdev->ParamCompatibilityLevel >= 1.5 ? &Gray_names15 : &Gray_names),
                                  &params.GrayImage, ecode);
    if (ecode < 0) {
        code = ecode;
        goto exit;
    }


    /* Mono sampled image parameters */

    ecode = psdf_put_image_params(pdev, plist, &Mono_names,
                                  &params.MonoImage, ecode);

    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    /* Font outlining parameters */

    ecode = psdf_put_embed_param(plist, "~AlwaysOutline", ".AlwaysOutline",
                                 &params.AlwaysOutline, mem, ecode);
    ecode = psdf_put_embed_param(plist, "~NeverOutline", ".NeverOutline",
                                 &params.NeverOutline, mem, ecode);

    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    /* Font embedding parameters */

    ecode = psdf_put_embed_param(plist, "~AlwaysEmbed", ".AlwaysEmbed",
                                 &params.AlwaysEmbed, mem, ecode);
    ecode = psdf_put_embed_param(plist, "~NeverEmbed", ".NeverEmbed",
                                 &params.NeverEmbed, mem, ecode);

    params.CannotEmbedFontPolicy = (enum psdf_cannot_embed_font_policy)
        psdf_put_enum(plist, "CannotEmbedFontPolicy",
                      (int)params.CannotEmbedFontPolicy,
                      CannotEmbedFontPolicy_names, &ecode);
    if (ecode < 0) {
        code = ecode;
        goto exit;
    }

    /* ps2write-specific output configuration options */
    code = psdf_read_string_param(plist, "PSDocOptions",
                                   (gs_const_string *)&params.PSDocOptions, mem, ecode);
    if (code < 0)
        goto exit;

    params.PSPageOptions.size = 0;
    params.PSPageOptions.data = NULL;
    code = psdf_read_copy_param_string_array(pdev->memory, plist, "PSPageOptions", &params.PSPageOptions);
    if (code < 0)
        goto exit;

    code = gdev_vector_put_params(dev, plist);

exit:
    if (!(pdev->params.LockDistillerParams && params.LockDistillerParams)) {
        /* Only update the device paramters if there was no error */
        /* Do not permit changes to pdev->Params.PSPageOptions, it doesn't make any sense */
        if (pdev->params.PSPageOptions.size != 0) {
            if (params.PSPageOptions.size != 0 && params.PSPageOptions.data != pdev->params.PSPageOptions.data) {
                int ix;

                for (ix = 0; ix < pdev->params.PSPageOptions.size;ix++)
                    gs_free_object(mem->non_gc_memory, (byte *)params.PSPageOptions.data[ix].data, "freeing old string array copy");
                gs_free_object(mem->non_gc_memory, (byte *)params.PSPageOptions.data, "freeing old string array");
            }
            params.PSPageOptions.data = pdev->params.PSPageOptions.data;
            params.PSPageOptions.size = pdev->params.PSPageOptions.size;
        }
        pdev->params = params;
    } else {
        /* We read a bunch of parameters and are now throwing them away. Either because there
         * was an error, or because the parameters were locked. We need to tidy up any memory
         * we allocated to hold these parameters.
         */
        gs_memory_t *stable_mem = gs_memory_stable(mem);

        if (params.PSPageOptions.data != NULL) {
            int ix;

            if (params.PSPageOptions.size != 0 && params.PSPageOptions.data != pdev->params.PSPageOptions.data) {
                for (ix = 0; ix < params.PSPageOptions.size;ix++)
                    gs_free_object(mem->non_gc_memory, (byte *)params.PSPageOptions.data[ix].data, "freeing dummy PSPageOptions");
                gs_free_object(mem->non_gc_memory, (byte *)params.PSPageOptions.data, "freeing dummy PSPageOptions");
            }
            params.PSPageOptions.data = NULL;
            params.PSPageOptions.size = 0;
        }
        if (params.CalCMYKProfile.data != 0)
            gs_free_string(stable_mem, (void *)params.CalCMYKProfile.data, params.CalCMYKProfile.size, "free dummy param CalCMYKProfile");
        if (params.CalGrayProfile.data != 0)
            gs_free_string(stable_mem, (void *)params.CalGrayProfile.data, params.CalGrayProfile.size, "free dummy param CalGrayProfile");
        if (params.CalRGBProfile.data != 0)
            gs_free_string(stable_mem, (void *)params.CalRGBProfile.data, params.CalRGBProfile.size, "free dummy param CalRGBProfile");
        if (params.sRGBProfile.data != 0)
            gs_free_string(stable_mem, (void *)params.sRGBProfile.data, params.sRGBProfile.size, "free dummy param sRGBProfile");
        if (params.ColorImage.ACSDict)
            gs_c_param_list_release(params.ColorImage.ACSDict);
        if (params.ColorImage.Dict)
            gs_c_param_list_release(params.ColorImage.Dict);
        if (params.GrayImage.ACSDict)
            gs_c_param_list_release(params.GrayImage.ACSDict);
        if (params.GrayImage.Dict)
            gs_c_param_list_release(params.GrayImage.Dict);
        if (params.MonoImage.ACSDict)
            gs_c_param_list_release(params.MonoImage.ACSDict);
        if (params.MonoImage.Dict)
            gs_c_param_list_release(params.MonoImage.Dict);
    }
    return code;
}
