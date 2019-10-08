/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/* Default device parameters for Ghostscript library */
#include "memory_.h"		/* for memcpy */
#include "string_.h"		/* for strlen */
#include "gx.h"
#include "gserrors.h"
#include "gsdevice.h"		/* for prototypes */
#include "gsparam.h"
#include "gxdevice.h"
#include "gxfixed.h"
#include "gsicc_manage.h"

/* Define whether we accept PageSize as a synonym for MediaSize. */
/* This is for backward compatibility only. */
#define PAGESIZE_IS_MEDIASIZE

/* ================ Getting parameters ================ */

/* Forward references */
static bool param_HWColorMap(gx_device *, byte *);

/* Get the device parameters. */
int
gs_get_device_or_hw_params(gx_device * orig_dev, gs_param_list * plist,
                           bool is_hardware)
{
    /*
     * We must be prepared to copy the device if it is the read-only
     * prototype.
     */
    gx_device *dev;
    int code = 0;

    if (orig_dev->memory)
        dev = orig_dev;
    else {
        code = gs_copydevice(&dev, orig_dev, plist->memory);
        if (code < 0)
            return code;
    }
    gx_device_set_procs(dev);
    fill_dev_proc(dev, get_params, gx_default_get_params);
    fill_dev_proc(dev, get_page_device, gx_default_get_page_device);
    fill_dev_proc(dev, get_alpha_bits, gx_default_get_alpha_bits);
    if (is_hardware) {
        if (dev_proc(dev, get_hardware_params) != NULL)
            code = (*dev_proc(dev, get_hardware_params)) (dev, plist);
    } else {
        if (dev_proc(dev, get_params) != NULL)
            code = (*dev_proc(dev, get_params)) (dev, plist);
    }
    if (dev != orig_dev)
        gx_device_retain(dev, false);  /* frees the copy */
    return code;
}

int gx_default_get_param(gx_device *dev, char *Param, void *list)
{
    gs_param_list * plist = (gs_param_list *)list;
    int k, colors = dev->color_info.num_components;
    gs_param_string profile_array[NUM_DEVICE_PROFILES];
    gs_param_string postren_profile, blend_profile;
    gs_param_string proof_profile, link_profile, icc_colorants;
    gsicc_rendering_intents_t profile_intents[NUM_DEVICE_PROFILES];
    gsicc_blackptcomp_t blackptcomps[NUM_DEVICE_PROFILES];
    gsicc_blackpreserve_t blackpreserve[NUM_DEVICE_PROFILES];
    int color_accuracy = MAX_COLOR_ACCURACY;
    int depth = dev->color_info.depth;
    cmm_dev_profile_t *dev_profile;
    char null_str[1]={'\0'};
#define set_param_array(a, d, s)\
  (a.data = d, a.size = s, a.persistent = false);
    bool devicegraytok = true;  /* Default if device profile stuct not set */
    bool graydetection = false;
    bool usefastcolor = false;  /* set for unmanaged color */
    bool sim_overprint = true;  /* By default simulate overprinting (only valid with cmyk devices) */
    bool prebandthreshold = true, temp_bool = false;

    if(strcmp(Param, "OutputDevice") == 0){
        gs_param_string dns;
        param_string_from_string(dns, dev->dname);
        return param_write_name(plist, "OutputDevice", &dns);
    }
#ifdef PAGESIZE_IS_MEDIASIZE
    if (strcmp(Param, "PageSize") == 0) {
        gs_param_float_array msa;
        set_param_array(msa, dev->MediaSize, 2);
        return param_write_float_array(plist, "PageSize", &msa);
    }
#endif
    if (strcmp(Param, "ProcessColorModel") == 0) {
        const char *cms = get_process_color_model_name(dev);

        /* We might have an uninitialized device with */
        /* color_info.num_components = 0.... */
        if ((cms != NULL) && (*cms != '\0')) {
            gs_param_string pcms;
            param_string_from_string(pcms, cms);
            return param_write_name(plist, "ProcessColorModel", &pcms);
        }
    }
    if (strcmp(Param, "HWResolution") == 0) {
        gs_param_float_array hwra;
        set_param_array(hwra, dev->HWResolution, 2);
        return param_write_float_array(plist, "HWResolution", &hwra);
    }
    if (strcmp(Param, "ImagingBBox") == 0) {
        gs_param_float_array ibba;
        set_param_array(ibba, dev->ImagingBBox, 4);
        if (dev->ImagingBBox_set)
            return param_write_float_array(plist, "ImagingBBox", &ibba);
        else
            return param_write_null(plist, "ImagingBBox");
    }
    if (strcmp(Param, "Margins") == 0) {
        gs_param_float_array ma;
        set_param_array(ma, dev->Margins, 2);
        return param_write_float_array(plist, "Margins", &ma);
    }
    if (strcmp(Param, "MaxSeparations") == 0) {
        int max_sep = dev->color_info.max_components;
        return param_write_int(plist, "MaxSeparations", &max_sep);
    }
    if (strcmp(Param, "NumCopies") == 0) {
        if (dev->NumCopies_set < 0 || (*dev_proc(dev, get_page_device))(dev) == 0) {
            return_error(gs_error_undefined);
        } else {
            if (dev->NumCopies_set)
                return param_write_int(plist, "NumCopies", &dev->NumCopies);
            else
                return param_write_null(plist, "NumCopies");
        }
    }
    if (strcmp(Param, "SeparationColorNames") == 0) {
        gs_param_string_array scna;
        set_param_array(scna, NULL, 0);
        return param_write_name_array(plist, "SeparationColorNames", &scna);
    }
    if (strcmp(Param, "Separations") == 0) {
        bool seprs = false;
        return param_write_bool(plist, "Separations", &seprs);
    }
    if (strcmp(Param, "UseCIEColor") == 0) {
        return param_write_bool(plist, "UseCIEColor", &dev->UseCIEColor);
    }

    /* Non-standard parameters */
    if (strcmp(Param, "HWSize") == 0) {
        int HWSize[2];
        gs_param_int_array hwsa;

        HWSize[0] = dev->width;
        HWSize[1] = dev->height;
        set_param_array(hwsa, HWSize, 2);
        return param_write_int_array(plist, "HWSize", &hwsa);
    }
    if (strcmp(Param, ".HWMargins") == 0) {
        gs_param_float_array hwma;
        set_param_array(hwma, dev->HWMargins, 4);
        return param_write_float_array(plist, ".HWMargins", &hwma);
    }
    if (strcmp(Param, ".MediaSize") == 0) {
        gs_param_float_array msa;
        set_param_array(msa, dev->MediaSize, 2);
        return param_write_float_array(plist, ".MediaSize", &msa);
    }
    if (strcmp(Param, "Name") == 0) {
        gs_param_string dns;
        param_string_from_string(dns, dev->dname);
        return param_write_string(plist, "Name", &dns);
    }
    if (strcmp(Param, "Colors") == 0) {
        int colors = dev->color_info.num_components;
        return param_write_int(plist, "Colors", &colors);
    }
    if (strcmp(Param, "BitsPerPixel") == 0) {
        return param_write_int(plist, "BitsPerPixel", &depth);
    }
    if (strcmp(Param, "GrayValues") == 0) {
        int GrayValues = dev->color_info.max_gray + 1;
        return param_write_int(plist, "GrayValues", &GrayValues);
    }
    if (strcmp(Param, "PageCount") == 0) {
        return param_write_long(plist, "PageCount", &dev->PageCount);
    }
    if (strcmp(Param, ".IgnoreNumCopies") == 0) {
        return param_write_bool(plist, ".IgnoreNumCopies", &dev->IgnoreNumCopies);
    }
    if (strcmp(Param, "TextAlphaBits") == 0) {
        return param_write_int(plist, "TextAlphaBits",
                                &dev->color_info.anti_alias.text_bits);
    }
    if (strcmp(Param, "GraphicsAlphaBits") == 0) {
        return param_write_int(plist, "GraphicsAlphaBits",
                                &dev->color_info.anti_alias.graphics_bits);
    }
    if (strcmp(Param, "AntidropoutDownscaler") == 0) {
        return param_write_bool(plist, "AntidropoutDownscaler",
                                &dev->color_info.use_antidropout_downscaler);
    }
    if (strcmp(Param, ".LockSafetyParams") == 0) {
        return param_write_bool(plist, ".LockSafetyParams", &dev->LockSafetyParams);
    }
    if (strcmp(Param, "MaxPatternBitmap") == 0) {
        return param_write_int(plist, "MaxPatternBitmap", &dev->MaxPatternBitmap);
    }
    if (strcmp(Param, "PageUsesTransparency") == 0) {
        return param_write_bool(plist, "PageUsesTransparency", &dev->page_uses_transparency);
    }
    if (strcmp(Param, "MaxBitmap") == 0) {
        return param_write_long(plist, "MaxBitmap", &(dev->space_params.MaxBitmap));
    }
    if (strcmp(Param, "BandBufferSpace") == 0) {
        return param_write_long(plist, "BandBufferSpace", &dev->space_params.band.BandBufferSpace);
    }
    if (strcmp(Param, "BandHeight") == 0) {
        return param_write_int(plist, "BandHeight", &dev->space_params.band.BandHeight);
    }
    if (strcmp(Param, "BandWidth") == 0) {
        return param_write_int(plist, "BandWidth", &dev->space_params.band.BandWidth);
    }
    if (strcmp(Param, "BufferSpace") == 0) {
        return param_write_long(plist, "BufferSpace", &dev->space_params.BufferSpace);
    }
    if (strcmp(Param, "InterpolateControl") == 0) {
        int interpolate_control = dev->interpolate_control;
        return param_write_int(plist, "InterpolateControl", &interpolate_control);
    }
    if (strcmp(Param, "LeadingEdge") == 0) {
        if (dev->LeadingEdge & LEADINGEDGE_SET_MASK) {
            int leadingedge = dev->LeadingEdge & LEADINGEDGE_MASK;
            return param_write_int(plist, "LeadingEdge", &leadingedge);
        } else
            return param_write_null(plist, "LeadingEdge");
    }

    if (dev->color_info.num_components > 1) {
        int RGBValues = dev->color_info.max_color + 1;
        long ColorValues = (depth >= 32 ? -1 : 1L << depth); /* value can only be 32 bits */

        if (strcmp(Param, "RedValues") == 0) {
            return param_write_int(plist, "RedValues", &RGBValues);
        }
        if (strcmp(Param, "GreenValues") == 0) {
            return param_write_int(plist, "GreenValues", &RGBValues);
        }
        if (strcmp(Param, "BlueValues") == 0) {
            return param_write_int(plist, "BlueValues", &RGBValues);
        }
        if (strcmp(Param, "ColorValues") == 0) {
            return param_write_long(plist, "ColorValues", &ColorValues);
        }
    }
    if (strcmp(Param, "HWColorMap") == 0) {
        byte palette[3 << 8];

        if (param_HWColorMap(dev, palette)) {
            gs_param_string hwcms;

            hwcms.data = palette, hwcms.size = colors << depth,
                hwcms.persistent = false;
            return param_write_string(plist, "HWColorMap", &hwcms);
        }
    }

    /* ICC profiles */
    /* Check if the device profile is null.  If it is, then we need to
       go ahead and get it set up at this time.  If the proc is not
       set up yet then we are not going to do anything yet */
    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) != NULL) {
        int code;
        code = dev_proc(dev, get_profile)(dev,  &dev_profile);
        if (code < 0)
            return code;
        if (dev_profile == NULL) {
            code = gsicc_init_device_profile_struct(dev, NULL, 0);
            if (code < 0)
                return code;
            code = dev_proc(dev, get_profile)(dev,  &dev_profile);
            if (code < 0)
                return code;
        }
        for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
            if (dev_profile->device_profile[k] == NULL
                || dev_profile->device_profile[k]->name == NULL) {
                param_string_from_string(profile_array[k], null_str);
                profile_intents[k] = gsRINOTSPECIFIED;
                blackptcomps[k] = gsBPNOTSPECIFIED;
                blackpreserve[k] = gsBKPRESNOTSPECIFIED;
            } else {
                param_string_from_transient_string(profile_array[k],
                    dev_profile->device_profile[k]->name);
                profile_intents[k] = dev_profile->rendercond[k].rendering_intent;
                blackptcomps[k] = dev_profile->rendercond[k].black_point_comp;
                blackpreserve[k] = dev_profile->rendercond[k].preserve_black;
            }
        }
        if (dev_profile->blend_profile == NULL) {
            param_string_from_string(blend_profile, null_str);
        } else {
            param_string_from_transient_string(blend_profile,
                dev_profile->blend_profile->name);
        }
        if (dev_profile->postren_profile == NULL) {
            param_string_from_string(postren_profile, null_str);
        } else {
            param_string_from_transient_string(postren_profile,
                dev_profile->postren_profile->name);
        }
        if (dev_profile->proof_profile == NULL) {
            param_string_from_string(proof_profile, null_str);
        } else {
            param_string_from_transient_string(proof_profile,
                                     dev_profile->proof_profile->name);
        }
        if (dev_profile->link_profile == NULL) {
            param_string_from_string(link_profile, null_str);
        } else {
            param_string_from_transient_string(link_profile,
                                     dev_profile->link_profile->name);
        }
        devicegraytok = dev_profile->devicegraytok;
        graydetection = dev_profile->graydetection;
        usefastcolor = dev_profile->usefastcolor;
        sim_overprint = dev_profile->sim_overprint;
        prebandthreshold = dev_profile->prebandthreshold;
        /* With respect to Output profiles that have non-standard colorants,
           we rely upon the default profile to give us the colorants if they do
           exist. */
        if (dev_profile->spotnames == NULL) {
            param_string_from_string(icc_colorants, null_str);
        } else {
            char *colorant_names;

            colorant_names =
                gsicc_get_dev_icccolorants(dev_profile);
            if (colorant_names != NULL) {
                param_string_from_transient_string(icc_colorants, colorant_names);
            } else {
                param_string_from_string(icc_colorants, null_str);
            }
        }
    } else {
        for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
            param_string_from_string(profile_array[k], null_str);
            profile_intents[k] = gsRINOTSPECIFIED;
            blackptcomps[k] = gsBPNOTSPECIFIED;
            blackpreserve[k] = gsBKPRESNOTSPECIFIED;
        }
        param_string_from_string(blend_profile, null_str);
        param_string_from_string(postren_profile, null_str);
        param_string_from_string(proof_profile, null_str);
        param_string_from_string(link_profile, null_str);
        param_string_from_string(icc_colorants, null_str);
    }
    if (strcmp(Param, "DeviceGrayToK") == 0) {
        return param_write_bool(plist, "DeviceGrayToK", &devicegraytok);
    }
    if (strcmp(Param, "GrayDetection") == 0) {
        return param_write_bool(plist, "GrayDetection", &graydetection);
    }
    if (strcmp(Param, "UseFastColor") == 0) {
        return param_write_bool(plist, "UseFastColor", &usefastcolor);
    }
    if (strcmp(Param, "SimulateOverprint") == 0) {
        return param_write_bool(plist, "SimulateOverprint", &sim_overprint);
    }
    if (strcmp(Param, "PreBandThreshold") == 0) {
        return param_write_bool(plist, "PreBandThreshold", &prebandthreshold);
    }
    if (strcmp(Param, "PostRenderProfile") == 0) {
        return param_write_string(plist, "PostRenderProfile", &(postren_profile));
    }
    if (strcmp(Param, "BlendColorProfile") == 0) {
        return param_write_string(plist, "BlendColorProfile", &(blend_profile));
    }
    if (strcmp(Param, "ProofProfile") == 0) {
        return param_write_string(plist,"ProofProfile", &(proof_profile));
    }
    if (strcmp(Param, "DeviceLinkProfile") == 0) {
        return param_write_string(plist,"DeviceLinkProfile", &(link_profile));
    }
    if (strcmp(Param, "ICCOutputColors") == 0) {
        return param_write_string(plist,"ICCOutputColors", &(icc_colorants));
    }
    if (strcmp(Param, "OutputICCProfile") == 0) {
        return param_write_string(plist,"OutputICCProfile", &(profile_array[0]));
    }
    if (strcmp(Param, "GraphicICCProfile") == 0) {
        return param_write_string(plist,"GraphicICCProfile", &(profile_array[1]));
    }
    if (strcmp(Param, "ImageICCProfile") == 0) {
        return param_write_string(plist,"ImageICCProfile", &(profile_array[2]));
    }
    if (strcmp(Param, "TextICCProfile") == 0) {
        return param_write_string(plist,"TextICCProfile", &(profile_array[3]));
    }
    if (strcmp(Param, "ColorAccuracy") == 0) {
        return param_write_int(plist, "ColorAccuracy", (const int *)(&(color_accuracy)));
    }
    if (strcmp(Param, "RenderIntent") == 0) {
        return param_write_int(plist,"RenderIntent", (const int *) (&(profile_intents[0])));
    }
    if (strcmp(Param, "GraphicIntent") == 0) {
        return param_write_int(plist,"GraphicIntent", (const int *) &(profile_intents[1]));
    }
    if (strcmp(Param, "ImageIntent") == 0) {
        return param_write_int(plist,"ImageIntent", (const int *) &(profile_intents[2]));
    }
    if (strcmp(Param, "TextIntent") == 0) {
        return param_write_int(plist,"TextIntent", (const int *) &(profile_intents[3]));
    }
    if (strcmp(Param, "BlackPtComp") == 0) {
        return param_write_int(plist,"BlackPtComp", (const int *) (&(blackptcomps[0])));
    }
    if (strcmp(Param, "GraphicBlackPt") == 0) {
        return param_write_int(plist,"GraphicBlackPt", (const int *) &(blackptcomps[1]));
    }
    if (strcmp(Param, "ImageBlackPt") == 0) {
        return param_write_int(plist,"ImageBlackPt", (const int *) &(blackptcomps[2]));
    }
    if (strcmp(Param, "TextBlackPt") == 0) {
        return param_write_int(plist,"TextBlackPt", (const int *) &(blackptcomps[3]));
    }
    if (strcmp(Param, "KPreserve") == 0) {
        return param_write_int(plist,"KPreserve", (const int *) (&(blackpreserve[0])));
    }
    if (strcmp(Param, "GraphicKPreserve") == 0) {
        return param_write_int(plist,"GraphicKPreserve", (const int *) &(blackpreserve[1]));
    }
    if (strcmp(Param, "ImageKPreserve") == 0) {
        return param_write_int(plist,"ImageKPreserve", (const int *) &(blackpreserve[2]));
    }
    if (strcmp(Param, "TextKPreserve") == 0) {
        return param_write_int(plist,"TextKPreserve", (const int *) &(blackpreserve[3]));
    }
    if (strcmp(Param, "FirstPage") == 0) {
        return param_write_int(plist, "FirstPage", &dev->FirstPage);
    }
    if (strcmp(Param, "LastPage") == 0) {
        return param_write_int(plist, "LastPage", &dev->LastPage);
    }
    if (strcmp(Param, "DisablePageHandler") == 0) {
        temp_bool = dev->DisablePageHandler;
        return param_write_bool(plist, "DisablePageHandler", &temp_bool);
    }
    if (strcmp(Param, "PageList") == 0){
        gs_param_string pagelist;
        if (dev->PageList) {
            gdev_pagelist *p = (gdev_pagelist *)dev->PageList;
            param_string_from_string(pagelist, p->Pages);
        }
        else
            param_string_from_string(pagelist, null_str);
        return param_write_string(plist, "PageList", &pagelist);
    }
    if (strcmp(Param, "FILTERIMAGE") == 0) {
        temp_bool = dev->ObjectFilter & FILTERIMAGE;
        return param_write_bool(plist, "FILTERIMAGE", &temp_bool);
    }
    if (strcmp(Param, "FILTERTEXT") == 0) {
        temp_bool = dev->ObjectFilter & FILTERTEXT;
        return param_write_bool(plist, "FILTERTEXT", &temp_bool);
    }
    if (strcmp(Param, "FILTERVECTOR") == 0) {
        temp_bool = dev->ObjectFilter & FILTERVECTOR;
        return param_write_bool(plist, "FILTERVECTOR", &temp_bool);
    }

    return_error(gs_error_undefined);
}

/* Get standard parameters. */
int
gx_default_get_params(gx_device * dev, gs_param_list * plist)
{
    int code;

    /* Standard page device parameters: */

    bool seprs = false;
    gs_param_string dns, pcms, profile_array[NUM_DEVICE_PROFILES];
    gs_param_string blend_profile, postren_profile, pagelist;
    gs_param_string proof_profile, link_profile, icc_colorants;
    gsicc_rendering_intents_t profile_intents[NUM_DEVICE_PROFILES];
    gsicc_blackptcomp_t blackptcomps[NUM_DEVICE_PROFILES];
    gsicc_blackpreserve_t blackpreserve[NUM_DEVICE_PROFILES];
    bool devicegraytok = true;  /* Default if device profile stuct not set */
    bool graydetection = false;
    bool usefastcolor = false;  /* set for unmanaged color */
    bool sim_overprint = true;  /* By default simulate overprinting */
    bool prebandthreshold = true, temp_bool;
    int k;
    int color_accuracy = MAX_COLOR_ACCURACY;
    gs_param_float_array msa, ibba, hwra, ma;
    gs_param_string_array scna;
    char null_str[1]={'\0'};

#define set_param_array(a, d, s)\
  (a.data = d, a.size = s, a.persistent = false);

    /* Non-standard parameters: */
    int colors = dev->color_info.num_components;
    int mns = dev->color_info.max_components;
    int depth = dev->color_info.depth;
    int GrayValues = dev->color_info.max_gray + 1;
    int HWSize[2];
    gs_param_int_array hwsa;
    gs_param_float_array hwma;
    cmm_dev_profile_t *dev_profile;

    /* Fill in page device parameters. */

    param_string_from_string(dns, dev->dname);
    {
        const char *cms = get_process_color_model_name(dev);

        /* We might have an uninitialized device with */
        /* color_info.num_components = 0.... */
        if ((cms != NULL) && (*cms != '\0'))
            param_string_from_string(pcms, cms);
        else
            pcms.data = 0;
    }

    set_param_array(hwra, dev->HWResolution, 2);
    set_param_array(msa, dev->MediaSize, 2);
    set_param_array(ibba, dev->ImagingBBox, 4);
    set_param_array(ma, dev->Margins, 2);
    set_param_array(scna, NULL, 0);

    /* Fill in non-standard parameters. */
    HWSize[0] = dev->width;
    HWSize[1] = dev->height;
    set_param_array(hwsa, HWSize, 2);
    set_param_array(hwma, dev->HWMargins, 4);
    /* Check if the device profile is null.  If it is, then we need to
       go ahead and get it set up at this time.  If the proc is not
       set up yet then we are not going to do anything yet */
    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) != NULL) {
        code = dev_proc(dev, get_profile)(dev,  &dev_profile);
        if (code < 0)
            return code;

        if (dev_profile == NULL) {
            code = gsicc_init_device_profile_struct(dev, NULL, 0);
            if (code < 0)
                return code;
            code = dev_proc(dev, get_profile)(dev,  &dev_profile);
            if (code < 0)
                return code;
        }
        /* It is possible that the current device profile name is NULL if we
           have a pdf14 device in line with a transparency group that is in a
           color space specified from a source defined ICC profile. Check for
           that here to avoid any access violations.  Bug 692558 */
        for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
            if (dev_profile->device_profile[k] == NULL
                || dev_profile->device_profile[k]->name == NULL) {
                param_string_from_string(profile_array[k], null_str);
                profile_intents[k] = gsRINOTSPECIFIED;
                blackptcomps[k] = gsBPNOTSPECIFIED;
                blackpreserve[k] = gsBKPRESNOTSPECIFIED;
            } else {
                param_string_from_transient_string(profile_array[k],
                    dev_profile->device_profile[k]->name);
                profile_intents[k] = dev_profile->rendercond[k].rendering_intent;
                blackptcomps[k] = dev_profile->rendercond[k].black_point_comp;
                blackpreserve[k] = dev_profile->rendercond[k].preserve_black;
            }
        }
        /* The proof, link and post render profile */
        if (dev_profile->proof_profile == NULL) {
            param_string_from_string(proof_profile, null_str);
        } else {
            param_string_from_transient_string(proof_profile,
                                     dev_profile->proof_profile->name);
        }
        if (dev_profile->link_profile == NULL) {
            param_string_from_string(link_profile, null_str);
        } else {
            param_string_from_transient_string(link_profile,
                                     dev_profile->link_profile->name);
        }
        if (dev_profile->postren_profile == NULL) {
            param_string_from_string(postren_profile, null_str);
        } else {
            param_string_from_transient_string(postren_profile,
                dev_profile->postren_profile->name);
        }
        if (dev_profile->blend_profile == NULL) {
            param_string_from_string(blend_profile, null_str);
        } else {
            param_string_from_transient_string(blend_profile,
                dev_profile->blend_profile->name);
        }
        devicegraytok = dev_profile->devicegraytok;
        graydetection = dev_profile->graydetection;
        usefastcolor = dev_profile->usefastcolor;
        sim_overprint = dev_profile->sim_overprint;
        prebandthreshold = dev_profile->prebandthreshold;
        /* With respect to Output profiles that have non-standard colorants,
           we rely upon the default profile to give us the colorants if they do
           exist. */
        if (dev_profile->spotnames == NULL) {
            param_string_from_string(icc_colorants, null_str);
        } else {
            char *colorant_names;

            colorant_names =
                gsicc_get_dev_icccolorants(dev_profile);
            if (colorant_names != NULL) {
                param_string_from_transient_string(icc_colorants, colorant_names);
            } else {
                param_string_from_string(icc_colorants, null_str);
            }
        }
    } else {
        for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
            param_string_from_string(profile_array[k], null_str);
            profile_intents[k] = gsRINOTSPECIFIED;
            blackptcomps[k] = gsBPNOTSPECIFIED;
            blackpreserve[k] = gsBKPRESNOTSPECIFIED;
        }
        param_string_from_string(proof_profile, null_str);
        param_string_from_string(link_profile, null_str);
        param_string_from_string(icc_colorants, null_str);
        param_string_from_string(postren_profile, null_str);
        param_string_from_string(blend_profile, null_str);
    }
    /* Transmit the values. */
    /* Standard parameters */
    if (
        (code = param_write_name(plist, "OutputDevice", &dns)) < 0 ||
#ifdef PAGESIZE_IS_MEDIASIZE
        (code = param_write_float_array(plist, "PageSize", &msa)) < 0 ||
#endif
        (code = (pcms.data == 0 ? 0 :
                 param_write_name(plist, "ProcessColorModel", &pcms))) < 0 ||
        (code = param_write_float_array(plist, "HWResolution", &hwra)) < 0 ||
        (code = (dev->ImagingBBox_set ?
                 param_write_float_array(plist, "ImagingBBox", &ibba) :
                 param_write_null(plist, "ImagingBBox"))) < 0 ||
        (code = param_write_float_array(plist, "Margins", &ma)) < 0 ||
        (code = param_write_int(plist, "MaxSeparations", &mns)) < 0 ||
        (code = (dev->NumCopies_set < 0 ||
                 (*dev_proc(dev, get_page_device))(dev) == 0 ? 0:
                 dev->NumCopies_set ?
                 param_write_int(plist, "NumCopies", &dev->NumCopies) :
                 param_write_null(plist, "NumCopies"))) < 0 ||
        (code = param_write_name_array(plist, "SeparationColorNames", &scna)) < 0 ||
        (code = param_write_bool(plist, "Separations", &seprs)) < 0 ||
        (code = param_write_bool(plist, "UseCIEColor", &dev->UseCIEColor)) < 0 ||
        /* Non-standard parameters */
        /* Note:  if change is made in NUM_DEVICE_PROFILES we need to name
           that profile here for the device parameter on the command line */
        (code = param_write_bool(plist, "DeviceGrayToK", &devicegraytok)) < 0 ||
        (code = param_write_bool(plist, "GrayDetection", &graydetection)) < 0 ||
        (code = param_write_bool(plist, "UseFastColor", &usefastcolor)) < 0 ||
        (code = param_write_bool(plist, "SimulateOverprint", &sim_overprint)) < 0 ||
        (code = param_write_bool(plist, "PreBandThreshold", &prebandthreshold)) < 0 ||
        (code = param_write_string(plist,"OutputICCProfile", &(profile_array[0]))) < 0 ||
        (code = param_write_string(plist,"GraphicICCProfile", &(profile_array[1]))) < 0 ||
        (code = param_write_string(plist,"ImageICCProfile", &(profile_array[2]))) < 0 ||
        (code = param_write_string(plist,"TextICCProfile", &(profile_array[3]))) < 0 ||
        (code = param_write_string(plist,"ProofProfile", &(proof_profile))) < 0 ||
        (code = param_write_string(plist, "PostRenderProfile", &(postren_profile))) < 0 ||
        (code = param_write_string(plist, "BlendColorProfile", &(blend_profile))) < 0 ||
        (code = param_write_string(plist,"DeviceLinkProfile", &(link_profile))) < 0 ||
        (code = param_write_string(plist,"ICCOutputColors", &(icc_colorants))) < 0 ||
        (code = param_write_int(plist, "RenderIntent", (const int *)(&(profile_intents[0])))) < 0 ||
        (code = param_write_int(plist, "ColorAccuracy", (const int *)(&(color_accuracy)))) < 0 ||
        (code = param_write_int(plist,"GraphicIntent", (const int *) &(profile_intents[1]))) < 0 ||
        (code = param_write_int(plist,"ImageIntent", (const int *) &(profile_intents[2]))) < 0 ||
        (code = param_write_int(plist,"TextIntent", (const int *) &(profile_intents[3]))) < 0 ||
        (code = param_write_int(plist,"BlackPtComp", (const int *) (&(blackptcomps[0])))) < 0 ||
        (code = param_write_int(plist,"GraphicBlackPt", (const int *) &(blackptcomps[1]))) < 0 ||
        (code = param_write_int(plist,"ImageBlackPt", (const int *) &(blackptcomps[2]))) < 0 ||
        (code = param_write_int(plist,"TextBlackPt", (const int *) &(blackptcomps[3]))) < 0 ||
        (code = param_write_int(plist,"KPreserve", (const int *) (&(blackpreserve[0])))) < 0 ||
        (code = param_write_int(plist,"GraphicKPreserve", (const int *) &(blackpreserve[1]))) < 0 ||
        (code = param_write_int(plist,"ImageKPreserve", (const int *) &(blackpreserve[2]))) < 0 ||
        (code = param_write_int(plist,"TextKPreserve", (const int *) &(blackpreserve[3]))) < 0 ||
        (code = param_write_int_array(plist, "HWSize", &hwsa)) < 0 ||
        (code = param_write_float_array(plist, ".HWMargins", &hwma)) < 0 ||
        (code = param_write_float_array(plist, ".MediaSize", &msa)) < 0 ||
        (code = param_write_string(plist, "Name", &dns)) < 0 ||
        (code = param_write_int(plist, "Colors", &colors)) < 0 ||
        (code = param_write_int(plist, "BitsPerPixel", &depth)) < 0 ||
        (code = param_write_int(plist, "GrayValues", &GrayValues)) < 0 ||
        (code = param_write_long(plist, "PageCount", &dev->PageCount)) < 0 ||
        (code = param_write_bool(plist, ".IgnoreNumCopies", &dev->IgnoreNumCopies)) < 0 ||
        (code = param_write_int(plist, "TextAlphaBits",
                                &dev->color_info.anti_alias.text_bits)) < 0 ||
        (code = param_write_int(plist, "GraphicsAlphaBits",
                                &dev->color_info.anti_alias.graphics_bits)) < 0 ||
        (code = param_write_bool(plist, "AntidropoutDownscaler",
                                &dev->color_info.use_antidropout_downscaler)) < 0 ||
        (code = param_write_bool(plist, ".LockSafetyParams", &dev->LockSafetyParams)) < 0 ||
        (code = param_write_int(plist, "MaxPatternBitmap", &dev->MaxPatternBitmap)) < 0 ||
        (code = param_write_bool(plist, "PageUsesTransparency", &dev->page_uses_transparency)) < 0 ||
        (code = param_write_long(plist, "MaxBitmap", &(dev->space_params.MaxBitmap))) < 0 ||
        (code = param_write_long(plist, "BandBufferSpace", &dev->space_params.band.BandBufferSpace)) < 0 ||
        (code = param_write_int(plist, "BandHeight", &dev->space_params.band.BandHeight)) < 0 ||
        (code = param_write_int(plist, "BandWidth", &dev->space_params.band.BandWidth)) < 0 ||
        (code = param_write_long(plist, "BufferSpace", &dev->space_params.BufferSpace)) < 0 ||
        (code = param_write_int(plist, "InterpolateControl", &dev->interpolate_control)) < 0
        )
        return code;

    /* If LeadingEdge was set explicitly, report it here. */
    if (dev->LeadingEdge & LEADINGEDGE_SET_MASK) {
        int leadingedge = dev->LeadingEdge & LEADINGEDGE_MASK;
        code = param_write_int(plist, "LeadingEdge", &leadingedge);
    } else
        code = param_write_null(plist, "LeadingEdge");
    if (code < 0)
        return code;

    if ((code = param_write_int(plist, "FirstPage", &dev->FirstPage)) < 0)
        return code;
    if ((code = param_write_int(plist, "LastPage", &dev->LastPage)) < 0)
        return code;

    temp_bool = dev->DisablePageHandler;
    if ((code = param_write_bool(plist, "DisablePageHandler", &temp_bool)) < 0)
        return code;

    if (dev->PageList) {
        gdev_pagelist *p = (gdev_pagelist *)dev->PageList;
        param_string_from_string(pagelist, p->Pages);
    }
    else
        param_string_from_string(pagelist, null_str);
    if ((code = param_write_name(plist, "PageList", &pagelist)) < 0)
        return code;

    temp_bool = dev->ObjectFilter & FILTERIMAGE;
    if ((code = param_write_bool(plist, "FILTERIMAGE", &temp_bool)) < 0)
        return code;
    temp_bool = dev->ObjectFilter & FILTERTEXT;
    if ((code = param_write_bool(plist, "FILTERTEXT", &temp_bool)) < 0)
        return code;
    temp_bool = dev->ObjectFilter & FILTERVECTOR;
    if ((code = param_write_bool(plist, "FILTERVECTOR", &temp_bool)) < 0)
        return code;

    /* Fill in color information. */

    if (colors > 1) {
        int RGBValues = dev->color_info.max_color + 1;
        long ColorValues = (depth >= 32 ? -1 : 1L << depth); /* value can only be 32 bits */

        if ((code = param_write_int(plist, "RedValues", &RGBValues)) < 0 ||
            (code = param_write_int(plist, "GreenValues", &RGBValues)) < 0 ||
            (code = param_write_int(plist, "BlueValues", &RGBValues)) < 0 ||
            (code = param_write_long(plist, "ColorValues", &ColorValues)) < 0
            )
            return code;
    }
    if (param_requested(plist, "HWColorMap")) {
        byte palette[3 << 8];

        if (param_HWColorMap(dev, palette)) {
            gs_param_string hwcms;

            hwcms.data = palette, hwcms.size = colors << depth,
                hwcms.persistent = false;
            if ((code = param_write_string(plist, "HWColorMap", &hwcms)) < 0)
                return code;
        }
    }

    return 0;
}

/* Get the color map for a device.  Return true if there is one. */
static bool
param_HWColorMap(gx_device * dev, byte * palette /* 3 << 8 */ )
{
    int depth = dev->color_info.depth;
    int colors = dev->color_info.num_components;

    if (depth <= 8 && colors <= 3) {
        byte *p = palette;
        gx_color_value rgb[3];
        gx_color_index i;

        fill_dev_proc(dev, map_color_rgb, gx_default_map_color_rgb);
        for (i = 0; (i >> depth) == 0; i++) {
            int j;

            if ((*dev_proc(dev, map_color_rgb)) (dev, i, rgb) < 0)
                return false;
            for (j = 0; j < colors; j++)
                *p++ = gx_color_value_to_byte(rgb[j]);
        }
        return true;
    }
    return false;
}

/* Get hardware-detected parameters. Default action is no hardware params. */
int
gx_default_get_hardware_params(gx_device * dev, gs_param_list * plist)
{
    return 0;
}

/* ---------------- Input and output media ---------------- */

/* Finish defining input or output media. */
static int
finish_media(gs_param_list * mlist, gs_param_name key, const char *media_type)
{
    int code = 0;

    if (media_type != 0) {
        gs_param_string as;

        param_string_from_string(as, media_type);
        code = param_write_string(mlist, key, &as);
    }
    return code;
}

/* Define input media. */

const gdev_input_media_t gdev_input_media_default =
{
    gdev_input_media_default_values
};

int
gdev_begin_input_media(gs_param_list * mlist, gs_param_dict * pdict,
                       int count)
{
    pdict->size = count;
    return param_begin_write_dict(mlist, "InputAttributes", pdict, true);
}

int
gdev_write_input_media(int index, gs_param_dict * pdict,
                       const gdev_input_media_t * pim)
{
    char key[25];
    gs_param_dict mdict;
    int code;
    gs_param_string as;

    gs_sprintf(key, "%d", index);
    mdict.size = 4;
    code = param_begin_write_dict(pdict->list, key, &mdict, false);
    if (code < 0)
        return code;
    if ((pim->PageSize[0] != 0 && pim->PageSize[1] != 0) ||
        (pim->PageSize[2] != 0 && pim->PageSize[3] != 0)
        ) {
        gs_param_float_array psa;

        psa.data = pim->PageSize;
        psa.size =
            (pim->PageSize[0] == pim->PageSize[2] &&
             pim->PageSize[1] == pim->PageSize[3] ? 2 : 4);
        psa.persistent = false;
        code = param_write_float_array(mdict.list, "PageSize",
                                       &psa);
        if (code < 0)
            return code;
    }
    if (pim->MediaColor != 0) {
        param_string_from_string(as, pim->MediaColor);
        code = param_write_string(mdict.list, "MediaColor",
                                  &as);
        if (code < 0)
            return code;
    }
    if (pim->MediaWeight != 0) {
        /*
         * We do the following silly thing in order to avoid
         * having to work around the 'const' in the arg list.
         */
        float weight = pim->MediaWeight;

        code = param_write_float(mdict.list, "MediaWeight",
                                 &weight);
        if (code < 0)
            return code;
    }
    code = finish_media(mdict.list, "MediaType", pim->MediaType);
    if (code < 0)
        return code;
    return param_end_write_dict(pdict->list, key, &mdict);
}

int
gdev_write_input_page_size(int index, gs_param_dict * pdict,
                           double width_points, double height_points)
{
    gdev_input_media_t media;

    media.PageSize[0] = media.PageSize[2] = (float) width_points;
    media.PageSize[1] = media.PageSize[3] = (float) height_points;
    media.MediaColor = 0;
    media.MediaWeight = 0;
    media.MediaType = 0;
    return gdev_write_input_media(index, pdict, &media);
}

int
gdev_end_input_media(gs_param_list * mlist, gs_param_dict * pdict)
{
    return param_end_write_dict(mlist, "InputAttributes", pdict);
}

/* Define output media. */

const gdev_output_media_t gdev_output_media_default =
{
    gdev_output_media_default_values
};

int
gdev_begin_output_media(gs_param_list * mlist, gs_param_dict * pdict,
                        int count)
{
    pdict->size = count;
    return param_begin_write_dict(mlist, "OutputAttributes", pdict, true);
}

int
gdev_write_output_media(int index, gs_param_dict * pdict,
                        const gdev_output_media_t * pom)
{
    char key[25];
    gs_param_dict mdict;
    int code;

    gs_sprintf(key, "%d", index);
    mdict.size = 4;
    code = param_begin_write_dict(pdict->list, key, &mdict, false);
    if (code < 0)
        return code;
    code = finish_media(mdict.list, "OutputType", pom->OutputType);
    if (code < 0)
        return code;
    return param_end_write_dict(pdict->list, key, &mdict);
}

int
gdev_end_output_media(gs_param_list * mlist, gs_param_dict * pdict)
{
    return param_end_write_dict(mlist, "OutputAttributes", pdict);
}

/* ================ Putting parameters ================ */

/* Forward references */
static int param_normalize_anti_alias_bits( uint max_gray, int bits );
static int param_anti_alias_bits(gs_param_list *, gs_param_name, int *);
static int param_MediaSize(gs_param_list *, gs_param_name,
                            const float *, gs_param_float_array *);

static int param_check_bool(gs_param_list *, gs_param_name, bool, bool);
static int param_check_long(gs_param_list *, gs_param_name, long, bool);
#define param_check_int(plist, pname, ival, is_defined)\
  param_check_long(plist, pname, (long)(ival), is_defined)
static int param_check_bytes(gs_param_list *, gs_param_name, const byte *,
                              uint, bool);
#define param_check_string(plist, pname, str, is_defined)\
  param_check_bytes(plist, pname, (const byte *)(str), \
                    (is_defined) ? strlen(str) : 0, is_defined)

/* Set the device parameters. */
/* If the device was open and the put_params procedure closed it, */
/* return 1; otherwise, return 0 or an error code as usual. */
int
gs_putdeviceparams(gx_device * dev, gs_param_list * plist)
{
    bool was_open = dev->is_open;
    int code;

    gx_device_set_procs(dev);
    fill_dev_proc(dev, put_params, gx_default_put_params);
    fill_dev_proc(dev, get_alpha_bits, gx_default_get_alpha_bits);
    code = (*dev_proc(dev, put_params)) (dev, plist);
    return (code < 0 ? code : was_open && !dev->is_open ? 1 : code);
}

static int
gx_default_put_graydetection(bool graydetection, gx_device * dev)
{
    int code = 0;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        /* This is an odd case where the device has not yet fully been
           set up with its procedures yet.  We want to make sure that
           we catch this so we assume here that we are dealing with
           the target device.  For now allocate the profile structure
           but do not intialize the profile yet as the color info
           may not be fully set up at this time.  */
        if (dev->icc_struct == NULL) {
            /* Allocate at this time the structure */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
        }
        dev->icc_struct->graydetection = graydetection;
        dev->icc_struct->pageneutralcolor = graydetection;
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (profile_struct == NULL) {
            /* Create now  */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            profile_struct =  dev->icc_struct;
        }
        profile_struct->graydetection = graydetection;
        profile_struct->pageneutralcolor = graydetection;
    }
    return code;
}

static int
gx_default_put_graytok(bool graytok, gx_device * dev)
{
    int code = 0;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        /* This is an odd case where the device has not yet fully been
           set up with its procedures yet.  We want to make sure that
           we catch this so we assume here that we are dealing with
           the target device.  For now allocate the profile structure
           but do not intialize the profile yet as the color info
           may not be fully set up at this time.  */
        if (dev->icc_struct == NULL) {
            /* Allocate at this time the structure */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        dev->icc_struct->devicegraytok = graytok;
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (profile_struct == NULL) {
            /* Create now  */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            profile_struct =  dev->icc_struct;
            if (profile_struct == NULL)
                return_error(gs_error_VMerror);
        }
        profile_struct->devicegraytok = graytok;
    }
    return code;
}

static int
gx_default_put_prebandthreshold(bool prebandthreshold, gx_device * dev)
{
    int code = 0;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        /* This is an odd case where the device has not yet fully been
           set up with its procedures yet.  We want to make sure that
           we catch this so we assume here that we are dealing with
           the target device.  For now allocate the profile structure
           but do not intialize the profile yet as the color info
           may not be fully set up at this time.  */
        if (dev->icc_struct == NULL) {
            /* Allocate at this time the structure */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        dev->icc_struct->prebandthreshold = prebandthreshold;
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (profile_struct == NULL) {
            /* Create now  */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            profile_struct =  dev->icc_struct;
            if (profile_struct == NULL)
                return_error(gs_error_VMerror);
        }
        profile_struct->prebandthreshold = prebandthreshold;
    }
    return code;
}

static int
gx_default_put_usefastcolor(bool fastcolor, gx_device * dev)
{
    int code = 0;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        /* This is an odd case where the device has not yet fully been
           set up with its procedures yet.  We want to make sure that
           we catch this so we assume here that we are dealing with
           the target device.  For now allocate the profile structure
           but do not intialize the profile yet as the color info
           may not be fully set up at this time.  */
        if (dev->icc_struct == NULL) {
            /* Allocate at this time the structure */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        dev->icc_struct->usefastcolor = fastcolor;
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (profile_struct == NULL) {
            /* Create now  */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            profile_struct =  dev->icc_struct;
            if (profile_struct == NULL)
                return_error(gs_error_VMerror);
        }
        profile_struct->usefastcolor = fastcolor;
    }
    return code;
}

static int
gx_default_put_simulateoverprint(bool sim_overprint, gx_device * dev)
{
    int code = 0;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        /* This is an odd case where the device has not yet fully been
           set up with its procedures yet.  We want to make sure that
           we catch this so we assume here that we are dealing with
           the target device.  For now allocate the profile structure
           but do not intialize the profile yet as the color info
           may not be fully set up at this time.  */
        if (dev->icc_struct == NULL) {
            /* Allocate at this time the structure */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        dev->icc_struct->sim_overprint = sim_overprint;
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (profile_struct == NULL) {
            /* Create now  */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            profile_struct =  dev->icc_struct;
            if (profile_struct == NULL)
                return_error(gs_error_VMerror);
        }
        profile_struct->sim_overprint = sim_overprint;
    }
    return code;
}

static int
gx_default_put_intent(gsicc_rendering_intents_t icc_intent, gx_device * dev,
                   gsicc_profile_types_t index)
{
    int code;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        /* This is an odd case where the device has not yet fully been
           set up with its procedures yet.  We want to make sure that
           we catch this so we assume here that we are dealing with
           the target device */
        if (dev->icc_struct == NULL) {
            /* Intializes the device structure.  Not the profile though for index */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        code = gsicc_set_device_profile_intent(dev, icc_intent, index);
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (code < 0)
            return code;
        if (profile_struct == NULL) {
            /* Create now  */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        code = gsicc_set_device_profile_intent(dev, icc_intent, index);
    }
    return code;
}

static int
gx_default_put_blackpreserve(gsicc_blackpreserve_t blackpreserve, gx_device * dev,
                           gsicc_profile_types_t index)
{
    int code;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        /* This is an odd case where the device has not yet fully been
           set up with its procedures yet.  We want to make sure that
           we catch this so we assume here that we are dealing with
           the target device */
        if (dev->icc_struct == NULL) {
            /* Intializes the device structure.  Not the profile though for index */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        code = gsicc_set_device_blackpreserve(dev, blackpreserve, index);
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (code < 0)
            return code;
        if (profile_struct == NULL) {
            /* Create now  */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        code = gsicc_set_device_blackpreserve(dev, blackpreserve, index);
    }
    return code;
}




static int
gx_default_put_blackptcomp(gsicc_blackptcomp_t blackptcomp, gx_device * dev,
                           gsicc_profile_types_t index)
{
    int code;
    cmm_dev_profile_t *profile_struct;

    /* Although device methods should not be NULL, they are not completely filled in until
     * gx_device_fill_in_procs is called, and its possible for us to get here before this
     * happens, so we *must* make sure the method is not NULL before we use it.
     */
    if (dev_proc(dev, get_profile) == NULL) {
        /* This is an odd case where the device has not yet fully been
           set up with its procedures yet.  We want to make sure that
           we catch this so we assume here that we are dealing with
           the target device */
        if (dev->icc_struct == NULL) {
            /* Intializes the device structure.  Not the profile though for index */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        code = gsicc_set_device_blackptcomp(dev, blackptcomp, index);
    } else {
        code = dev_proc(dev, get_profile)(dev,  &profile_struct);
        if (code < 0)
            return code;
        if (profile_struct == NULL) {
            /* Create now  */
            dev->icc_struct = gsicc_new_device_profile_array(dev->memory);
            if (dev->icc_struct == NULL)
                return_error(gs_error_VMerror);
        }
        code = gsicc_set_device_blackptcomp(dev, blackptcomp, index);
    }
    return code;
}

static int
gx_default_put_icc_colorants(gs_param_string *colorants, gx_device * dev)
{
    char *tempstr;
    int code;

    if (colorants->size == 0) return 0;

    /* See below about this device fill proc */
    fill_dev_proc(dev, get_profile, gx_default_get_profile);
    tempstr = (char *) gs_alloc_bytes(dev->memory, colorants->size+1,
                                      "gx_default_put_icc_colorants");
    memcpy(tempstr, colorants->data, colorants->size);
    /* Set last position to NULL. */
    tempstr[colorants->size] = 0;
    code = gsicc_set_device_profile_colorants(dev, tempstr);
    gs_free_object(dev->memory, tempstr, "gx_default_put_icc_colorants");
    return code;
}

static int
gx_default_put_icc(gs_param_string *icc_pro, gx_device * dev,
                   gsicc_profile_types_t index)
{
    char *tempstr;
    int code = 0;

    if (icc_pro->size == 0) return 0;
    /* If this has not yet been set, then set it to the default.
       I don't like doing this here but if we are in here trying to
       set a profile for our device and if the proc for this has not
       yet been set, we are going to lose the chance to set the profile.
       Much like open, this proc should be set early on.  I leave that
       exercise to the device start-up experts */
    fill_dev_proc(dev, get_profile, gx_default_get_profile);
    if (icc_pro->size < gp_file_name_sizeof) {
        tempstr = (char *) gs_alloc_bytes(dev->memory, icc_pro->size+1,
                                          "gx_default_put_icc");
        if (tempstr == NULL)
            return_error(gs_error_VMerror);
        memcpy(tempstr, icc_pro->data, icc_pro->size);
        /* Set last position to NULL. */
        tempstr[icc_pro->size] = 0;
        code = gsicc_init_device_profile_struct(dev, tempstr, index);
        gs_free_object(dev->memory, tempstr, "gx_default_put_icc");
    }
    return code;
}

static void
rc_free_pages_list(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    gdev_pagelist *PageList = (gdev_pagelist *)ptr_in;

    if (PageList->rc.ref_count <= 1) {
        gs_free(mem->non_gc_memory, PageList->Pages, 1, PagesSize, "free page list");
        gs_free(mem->non_gc_memory, PageList, 1, sizeof(gdev_pagelist), "free structure to hold page list");
    }
}

/* Set standard parameters. */
/* Note that setting the size or resolution closes the device. */
/* Window devices that don't want this to happen must temporarily */
/* set is_open to false before calling gx_default_put_params, */
/* and then taking appropriate action afterwards. */
int
gx_default_put_params(gx_device * dev, gs_param_list * plist)
{
    int ecode = 0;
    int code;
    gs_param_name param_name;
    gs_param_float_array hwra;
    gs_param_int_array hwsa;
    gs_param_float_array msa;
    gs_param_float_array ma;
    gs_param_float_array hwma;
    gs_param_string_array scna;
    int nci = dev->NumCopies;
    int ncset = dev->NumCopies_set;
    bool ignc = dev->IgnoreNumCopies;
    bool ucc = dev->UseCIEColor;
    gs_param_string icc_pro;
    bool locksafe = dev->LockSafetyParams;
    gs_param_float_array ibba;
    bool ibbnull = false;
    int colors = dev->color_info.num_components;
    int depth = dev->color_info.depth;
    int GrayValues = dev->color_info.max_gray + 1;
    int RGBValues = dev->color_info.max_color + 1;
    long ColorValues = (depth >= 32 ? -1 : 1L << depth);
    int tab = dev->color_info.anti_alias.text_bits;
    int gab = dev->color_info.anti_alias.graphics_bits;
    int mpbm = dev->MaxPatternBitmap;
    int ic = dev->interpolate_control;
    bool page_uses_transparency = dev->page_uses_transparency;
    gdev_space_params sp = dev->space_params;
    gdev_space_params save_sp = dev->space_params;
    int rend_intent[NUM_DEVICE_PROFILES];
    int blackptcomp[NUM_DEVICE_PROFILES];
    int blackpreserve[NUM_DEVICE_PROFILES];
    gs_param_string cms, pagelist;
    int leadingedge = dev->LeadingEdge;
    int k;
    int color_accuracy;
    bool devicegraytok = true;
    bool graydetection = false;
    bool usefastcolor = false;
    bool sim_overprint = true;
    bool prebandthreshold = false;
    bool use_antidropout = dev->color_info.use_antidropout_downscaler;
    bool temp_bool;
    int  profile_types[NUM_DEVICE_PROFILES] = {gsDEFAULTPROFILE,
                                               gsGRAPHICPROFILE,
                                               gsIMAGEPROFILE,
                                               gsTEXTPROFILE};

    color_accuracy = gsicc_currentcoloraccuracy(dev->memory);
    if (dev->icc_struct != NULL) {
        for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
            rend_intent[k] = dev->icc_struct->rendercond[k].rendering_intent;
            blackptcomp[k] = dev->icc_struct->rendercond[k].black_point_comp;
            blackpreserve[k] = dev->icc_struct->rendercond[k].preserve_black;
        }
        graydetection = dev->icc_struct->graydetection;
        devicegraytok = dev->icc_struct->devicegraytok;
        usefastcolor = dev->icc_struct->usefastcolor;
        prebandthreshold = dev->icc_struct->prebandthreshold;
        sim_overprint = dev->icc_struct->sim_overprint;
    } else {
        for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
            rend_intent[k] = gsRINOTSPECIFIED;
            blackptcomp[k] = gsBPNOTSPECIFIED;
            blackpreserve[k] = gsBKPRESNOTSPECIFIED;
        }
    }

    /*
     * Template:
     *   BEGIN_ARRAY_PARAM(param_read_xxx_array, "pname", pxxa, size, pxxe) {
     *     ... check value if desired ...
     *     if (success)
     *       break;
     *     ... set ecode ...
     *   } END_ARRAY_PARAM(pxxa, pxxe);
     */

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

    /*
     * The actual value of LeadingEdge must be changed inside this routine,
     * so that we can detect that it has been changed. Thus, instead of a
     * device setting the value itself, it signals a request, which is
     * now executed.
     */
    if (leadingedge & LEADINGEDGE_REQ_BIT) {
        leadingedge = (leadingedge & LEADINGEDGE_SET_MASK) |
            ((leadingedge >> LEADINGEDGE_REQ_VAL_SHIFT) & LEADINGEDGE_MASK);
    }

    /*
     * The HWResolution, HWSize, and MediaSize parameters interact in
     * the following way:
     *      1. Setting HWResolution recomputes HWSize from MediaSize.
     *      2. Setting HWSize recomputes MediaSize from HWResolution.
     *      3. Setting MediaSize recomputes HWSize from HWResolution.
     * If more than one parameter is being set, we apply these rules
     * in the order 1, 2, 3.  This does the right thing in the most
     * common case of setting more than one parameter, namely,
     * setting both HWResolution and HWSize.
     *
     * Changing of LeadingEdge is treated exactly the same as a
     * change in HWResolution. In typical usage, MediaSize is
     * short-edge (MediaSize[0] < MediaSize[1]), so if LeadingEdge
     * is 1 or 3, then HWSize will become long-edge. For nonsquare
     * resolutions, HWResolution[0] always corresponds with width
     * (scan length), and [1] with height (number of scans).
     */

    BEGIN_ARRAY_PARAM(param_read_float_array, "HWResolution", hwra, 2, hwre) {
        if (hwra.data[0] <= 0 || hwra.data[1] <= 0)
            ecode = gs_note_error(gs_error_rangecheck);
        else
            break;
    } END_ARRAY_PARAM(hwra, hwre);
    BEGIN_ARRAY_PARAM(param_read_int_array, "HWSize", hwsa, 2, hwsa) {
        /* We need a special check to handle the nullpage device, */
        /* whose size is legitimately [0 0]. */
        if ((hwsa.data[0] <= 0 && hwsa.data[0] != dev->width) ||
            (hwsa.data[1] <= 0 && hwsa.data[1] != dev->height)
        )
            ecode = gs_note_error(gs_error_rangecheck);
#define max_coord (max_fixed / fixed_1)
#if max_coord < max_int
        else if (hwsa.data[0] > max_coord || hwsa.data[1] > max_coord)
            ecode = gs_note_error(gs_error_limitcheck);
#endif
#undef max_coord
        else
            break;
    } END_ARRAY_PARAM(hwsa, hwse);
    {
        int t;

        code = param_read_int(plist, "LeadingEdge", &t);
        if (code < 0) {
            if (param_read_null(plist, "LeadingEdge") == 0) {
                /* if param is null, clear explicitly-set flag */
                leadingedge &= ~LEADINGEDGE_SET_MASK;
            } else {
                ecode = code;
            }
        } else if (code == 0) {
            if (t < 0 || t > 3)
                param_signal_error(plist, "LeadingEdge",
                                   ecode = gs_error_rangecheck);
            else
                leadingedge = LEADINGEDGE_SET_MASK | t;
        }
    }
    {
        const float *res = (hwra.data == 0 ? dev->HWResolution : hwra.data);

#ifdef PAGESIZE_IS_MEDIASIZE
        const float *data;

        /* .MediaSize takes precedence over PageSize, so */
        /* we read PageSize first. */
        code = param_MediaSize(plist, "PageSize", res, &msa);
        if (code < 0)
            ecode = code;
        /* Prevent data from being set to 0 if PageSize is specified */
        /* but .MediaSize is not. */
        data = msa.data;
        code = param_MediaSize(plist, ".MediaSize", res, &msa);
        if (code < 0)
            ecode = code;
        else if (msa.data == 0)
            msa.data = data;
#else
        code = param_MediaSize(plist, ".MediaSize", res, &msa);
        if (code < 0)
            ecode = code;
#endif
    }

    BEGIN_ARRAY_PARAM(param_read_float_array, "Margins", ma, 2, me) {
        break;
    } END_ARRAY_PARAM(ma, me);
    BEGIN_ARRAY_PARAM(param_read_float_array, ".HWMargins", hwma, 4, hwme) {
        break;
    } END_ARRAY_PARAM(hwma, hwme);
    switch (code = param_read_bool(plist, (param_name = ".IgnoreNumCopies"), &ignc)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }
    if (dev->NumCopies_set >= 0 &&
        (*dev_proc(dev, get_page_device))(dev) != 0
        ) {
        switch (code = param_read_int(plist, (param_name = "NumCopies"), &nci)) {
            case 0:
                if (nci < 0)
                    ecode = gs_error_rangecheck;
                else {
                    ncset = 1;
                    break;
                }
                goto nce;
            default:
                if ((code = param_read_null(plist, param_name)) == 0) {
                    ncset = 0;
                    break;
                }
                ecode = code;	/* can't be 1 */
nce:
                param_signal_error(plist, param_name, ecode);
            case 1:
                break;
        }
    }
    /* Set the ICC output colors first */
    if ((code = param_read_string(plist, "ICCOutputColors", &icc_pro)) != 1) {
        if ((code = gx_default_put_icc_colorants(&icc_pro, dev)) < 0) {
            ecode = code;
            param_signal_error(plist, "ICCOutputColors", ecode);
        }
    }
    if ((code = param_read_string(plist, "DeviceLinkProfile", &icc_pro)) != 1) {
        if ((code = gx_default_put_icc(&icc_pro, dev, gsLINKPROFILE)) < 0) {
            ecode = code;
            param_signal_error(plist, "DeviceLinkProfile", ecode);
        }
    }
    if ((code = param_read_string(plist, "PostRenderProfile", &icc_pro)) != 1) {
        if ((code = gx_default_put_icc(&icc_pro, dev, gsPRPROFILE)) < 0) {
            ecode = code;
            param_signal_error(plist, "PostRenderProfile", ecode);
        }
    }
    if ((code = param_read_string(plist, "OutputICCProfile", &icc_pro)) != 1) {
        if ((code = gx_default_put_icc(&icc_pro, dev, gsDEFAULTPROFILE)) < 0) {
            ecode = code;
            param_signal_error(plist, "OutputICCProfile", ecode);
        }
    }
    /* Note, if a change is made to NUM_DEVICE_PROFILES we need to update
       this with the name of the profile */
    if ((code = param_read_string(plist, "GraphicICCProfile", &icc_pro)) != 1) {
        if ((code = gx_default_put_icc(&icc_pro, dev, gsGRAPHICPROFILE)) < 0) {
            ecode = code;
            param_signal_error(plist, "GraphicICCProfile", ecode);
        }
    }
    if ((code = param_read_string(plist, "ImageICCProfile", &icc_pro)) != 1) {
        if ((code = gx_default_put_icc(&icc_pro, dev, gsIMAGEPROFILE)) < 0) {
            ecode = code;
            param_signal_error(plist, "ImageICCProfile", ecode);
        }
    }
    if ((code = param_read_string(plist, "TextICCProfile", &icc_pro)) != 1) {
        if ((code = gx_default_put_icc(&icc_pro, dev, gsTEXTPROFILE)) < 0) {
            ecode = code;
            param_signal_error(plist, "TextICCProfile", ecode);
        }
    }
    if ((code = param_read_string(plist, "ProofProfile", &icc_pro)) != 1) {
        if ((code = gx_default_put_icc(&icc_pro, dev, gsPROOFPROFILE)) < 0) {
            ecode = code;
            param_signal_error(plist, "ProofProfile", ecode);
        }
    }
    if ((code = param_read_string(plist, "BlendColorProfile", &icc_pro)) != 1) {
        if ((code = gx_default_put_icc(&icc_pro, dev, gsBLENDPROFILE)) < 0) {
            ecode = code;
            param_signal_error(plist, "BlendColorProfile", ecode);
        }
    }
    if ((code = param_read_int(plist, (param_name = "RenderIntent"),
                                                    &(rend_intent[0]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "GraphicIntent"),
                                                    &(rend_intent[1]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "ImageIntent"),
                                                    &(rend_intent[2]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "TextIntent"),
                                                    &(rend_intent[3]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "BlackPtComp"),
                                                    &(blackptcomp[0]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "GraphicBlackPt"),
                                                    &(blackptcomp[1]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "ImageBlackPt"),
                                                    &(blackptcomp[2]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "TextBlackPt"),
                                                    &(blackptcomp[3]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "KPreserve"),
                                                    &(blackpreserve[0]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "GraphicKPreserve"),
                                                    &(blackpreserve[1]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "ImageKPreserve"),
                                                    &(blackpreserve[2]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "TextKPreserve"),
                                                    &(blackpreserve[3]))) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_int(plist, (param_name = "ColorAccuracy"),
                                                        &color_accuracy)) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_bool(plist, (param_name = "DeviceGrayToK"),
                                                        &devicegraytok)) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_bool(plist, (param_name = "GrayDetection"),
                                                        &graydetection)) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_bool(plist, (param_name = "UseFastColor"),
                                                        &usefastcolor)) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_bool(plist, (param_name = "SimulateOverprint"),
                                                        &sim_overprint)) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_bool(plist, (param_name = "PreBandThreshold"),
                                                        &prebandthreshold)) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_bool(plist, (param_name = "UseCIEColor"), &ucc)) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_anti_alias_bits(plist, "TextAlphaBits", &tab)) < 0)
        ecode = code;
    if ((code = param_anti_alias_bits(plist, "GraphicsAlphaBits", &gab)) < 0)
        ecode = code;
    if ((code = param_read_bool(plist, "AntidropoutDownscaler", &use_antidropout)) < 0)
        ecode = code;
    if ((code = param_read_int(plist, "MaxPatternBitmap", &mpbm)) < 0)
        ecode = code;
    if ((code = param_read_int(plist, "InterpolateControl", &ic)) < 0)
        ecode = code;
    if ((code = param_read_bool(plist, (param_name = "PageUsesTransparency"),
                                &page_uses_transparency)) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }
    if ((code = param_read_long(plist, "MaxBitmap", &sp.MaxBitmap)) < 0)
        ecode = code;

#define CHECK_PARAM_CASES(member, bad, label)\
    case 0:\
        if ((sp.params_are_read_only ? sp.member != save_sp.member : bad))\
            ecode = gs_error_rangecheck;\
        else\
            break;\
        goto label;\
    default:\
        ecode = code;\
label:\
        param_signal_error(plist, param_name, ecode);\
    case 1:\
        break

    switch (code = param_read_long(plist, (param_name = "BufferSpace"), &sp.BufferSpace)) {
        CHECK_PARAM_CASES(BufferSpace, sp.BufferSpace < 10000, bse);
    }

    switch (code = param_read_int(plist, (param_name = "BandWidth"), &sp.band.BandWidth)) {
        CHECK_PARAM_CASES(band.BandWidth, sp.band.BandWidth < 0, bwe);
    }

    switch (code = param_read_int(plist, (param_name = "BandHeight"), &sp.band.BandHeight)) {
        CHECK_PARAM_CASES(band.BandHeight, sp.band.BandHeight < 0, bhe);
    }

    switch (code = param_read_long(plist, (param_name = "BandBufferSpace"), &sp.band.BandBufferSpace)) {
        CHECK_PARAM_CASES(band.BandBufferSpace, sp.band.BandBufferSpace < 0, bbse);
    }


    switch (code = param_read_bool(plist, (param_name = ".LockSafetyParams"), &locksafe)) {
        case 0:
            if (dev->LockSafetyParams && !locksafe)
                code = gs_note_error(gs_error_invalidaccess);
            else
                break;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 1:
            break;
    }
    /* Ignore parameters that only have meaning for printers. */
#define IGNORE_INT_PARAM(pname)\
  { int igni;\
    switch ( code = param_read_int(plist, (param_name = pname), &igni) )\
      { default:\
          ecode = code;\
          param_signal_error(plist, param_name, ecode);\
        case 0:\
        case 1:\
          break;\
      }\
  }
    IGNORE_INT_PARAM("%MediaSource")
        IGNORE_INT_PARAM("%MediaDestination")
        switch (code = param_read_float_array(plist, (param_name = "ImagingBBox"), &ibba)) {
        case 0:
            if (ibba.size != 4 ||
                ibba.data[2] < ibba.data[0] || ibba.data[3] < ibba.data[1]
                )
                ecode = gs_note_error(gs_error_rangecheck);
            else
                break;
            goto ibbe;
        default:
            if ((code = param_read_null(plist, param_name)) == 0) {
                ibbnull = true;
                ibba.data = 0;
                break;
            }
            ecode = code;	/* can't be 1 */
          ibbe:param_signal_error(plist, param_name, ecode);
        case 1:
            ibba.data = 0;
            break;
    }

    /* Separation, DeviceN Color, and ProcessColorModel related parameters. */
    {
        const char * pcms = get_process_color_model_name(dev);
        /* the device should have set a process model name at this point */
        if ((code = param_check_string(plist, "ProcessColorModel", pcms, (pcms != NULL))) < 0)
            ecode = code;
    }
    IGNORE_INT_PARAM("MaxSeparations")
    if ((code = param_check_bool(plist, "Separations", false, true)) < 0)
        ecode = code;

    BEGIN_ARRAY_PARAM(param_read_name_array, "SeparationColorNames", scna, scna.size, scne) {
        break;
    } END_ARRAY_PARAM(scna, scne);

    /* Now check nominally read-only parameters. */
    if ((code = param_check_string(plist, "OutputDevice", dev->dname, true)) < 0)
        ecode = code;
    if ((code = param_check_string(plist, "Name", dev->dname, true)) < 0)
        ecode = code;
    if ((code = param_check_int(plist, "Colors", colors, true)) < 0)
        ecode = code;
    if ((code = param_check_int(plist, "BitsPerPixel", depth, true)) < 0)
        ecode = code;
    if ((code = param_check_int(plist, "GrayValues", GrayValues, true)) < 0)
        ecode = code;

    /* with saved-pages, PageCount can't be checked. No harm in letting it change */
    IGNORE_INT_PARAM("PageCount")

    if ((code = param_check_int(plist, "RedValues", RGBValues, true)) < 0)
        ecode = code;
    if ((code = param_check_int(plist, "GreenValues", RGBValues, true)) < 0)
        ecode = code;
    if ((code = param_check_int(plist, "BlueValues", RGBValues, true)) < 0)
        ecode = code;
    if ((code = param_check_long(plist, "ColorValues", ColorValues, true)) < 0)
        ecode = code;
    if (param_read_string(plist, "HWColorMap", &cms) != 1) {
        byte palette[3 << 8];

        if (param_HWColorMap(dev, palette))
            code = param_check_bytes(plist, "HWColorMap", palette,
                                     colors << depth, true);
        else
            code = param_check_bytes(plist, "HWColorMap", 0, 0, false);
        if (code < 0)
            ecode = code;
    }

    code = param_read_int(plist, "FirstPage", &dev->FirstPage);
    if (code < 0)
        ecode = code;

    code = param_read_int(plist,  "LastPage", &dev->LastPage);
    if (code < 0)
        ecode = code;

    code = param_read_bool(plist, "DisablePageHandler", &temp_bool);
    if (code < 0)
        ecode = code;
    if (code == 0)
        dev->DisablePageHandler = temp_bool;

    if ((code = param_read_string(plist, "PageList", &pagelist)) != 1 && pagelist.size > 0) {
        if (dev->PageList)
            rc_decrement(dev->PageList, "default put_params PageList");
        dev->PageList = (gdev_pagelist *)gs_alloc_bytes(dev->memory->non_gc_memory, sizeof(gdev_pagelist), "structure to hold page list");
        if (!dev->PageList)
            return gs_note_error(gs_error_VMerror);
        dev->PageList->Pages = (void *)gs_alloc_bytes(dev->memory->non_gc_memory, pagelist.size + 1, "String to hold page list");
        if (!dev->PageList->Pages){
            gs_free(dev->memory->non_gc_memory, dev->PageList, 1, sizeof(gdev_pagelist), "free structure to hold page list");
            dev->PageList = 0;
            return gs_note_error(gs_error_VMerror);
        }
        memset(dev->PageList->Pages, 0x00, pagelist.size + 1);
        memcpy(dev->PageList->Pages, pagelist.data, pagelist.size);
        dev->PageList->PagesSize = pagelist.size + 1;
        rc_init_free(dev->PageList, dev->memory->non_gc_memory, 1, rc_free_pages_list);
    }

    code = param_read_bool(plist, "FILTERIMAGE", &temp_bool);
    if (code < 0)
        ecode = code;
    if (code == 0) {
        if (temp_bool)
            dev->ObjectFilter |= FILTERIMAGE;
        else
            dev->ObjectFilter &= ~FILTERIMAGE;
    }

    code = param_read_bool(plist, "FILTERTEXT", &temp_bool);
    if (code < 0)
        ecode = code;
    if (code == 0) {
        if (temp_bool)
            dev->ObjectFilter |= FILTERTEXT;
        else
            dev->ObjectFilter &= ~FILTERTEXT;
    }

    code = param_read_bool(plist, "FILTERVECTOR", &temp_bool);
    if (code < 0)
        ecode = code;
    if (code == 0) {
        if (temp_bool)
            dev->ObjectFilter |= FILTERVECTOR;
        else
            dev->ObjectFilter &= ~FILTERVECTOR;
    }

    /* We must 'commit', in order to detect unknown parameters, */
    /* even if there were errors. */
    code = param_commit(plist);
    if (ecode < 0) {
        /* restore_page_device (zdevice2.c) will turn off LockSafetyParams, and relies on putparams
         * to put it back if we are restoring a device. The locksafe value is picked up above from the
         * device we are restoring to, and we *must* make sure it is preserved, even if setting the
         * params failed. Otherwise an attacker can use a failed grestore to reset LockSafetyParams.
         * See bug #699687.
         */
        dev->LockSafetyParams = locksafe;
        return ecode;
    }
    if (code < 0) {
        dev->LockSafetyParams = locksafe;
        return code;
    }

    /*
     * Now actually make the changes. Changing resolution, rotation
     * (through LeadingEdge) or page size requires closing the device,
     * but changing margins or ImagingBBox does not. In order not to
     * close and reopen the device unnecessarily, we check for
     * replacing the values with the same ones.
     */

    dev->color_info.use_antidropout_downscaler = use_antidropout;

    if (hwra.data != 0 &&
        (dev->HWResolution[0] != hwra.data[0] ||
         dev->HWResolution[1] != hwra.data[1])
        ) {
        if (dev->is_open)
            gs_closedevice(dev);
        gx_device_set_resolution(dev, hwra.data[0], hwra.data[1]);
    }
    if ((leadingedge & LEADINGEDGE_MASK) !=
        (dev->LeadingEdge & LEADINGEDGE_MASK)) {
        /* If the LeadingEdge_set flag changes but the value of LeadingEdge
           itself does not, don't close device and recompute page size. */
        dev->LeadingEdge = leadingedge;
        if (dev->is_open)
            gs_closedevice(dev);
        gx_device_set_resolution(dev, dev->HWResolution[0], dev->HWResolution[1]);
    }
    /* clear leadingedge request, preserve "set" flag */
    dev->LeadingEdge &= LEADINGEDGE_MASK;
    dev->LeadingEdge |= (leadingedge & LEADINGEDGE_SET_MASK);

    if (hwsa.data != 0 &&
        (dev->width != hwsa.data[0] ||
         dev->height != hwsa.data[1])
        ) {
        if (dev->is_open)
            gs_closedevice(dev);
        gx_device_set_width_height(dev, hwsa.data[0], hwsa.data[1]);
    }
    if (msa.data != 0 &&
        (dev->MediaSize[0] != msa.data[0] ||
         dev->MediaSize[1] != msa.data[1])
        ) {
        if (dev->is_open)
            gs_closedevice(dev);
        gx_device_set_page_size(dev, msa.data[0], msa.data[1]);
    }
    if (ma.data != 0) {
        dev->Margins[0] = ma.data[0];
        dev->Margins[1] = ma.data[1];
    }
    if (hwma.data != 0) {
        dev->HWMargins[0] = hwma.data[0];
        dev->HWMargins[1] = hwma.data[1];
        dev->HWMargins[2] = hwma.data[2];
        dev->HWMargins[3] = hwma.data[3];
    }
    dev->NumCopies = nci;
    dev->NumCopies_set = ncset;
    dev->IgnoreNumCopies = ignc;
    if (ibba.data != 0) {
        dev->ImagingBBox[0] = ibba.data[0];
        dev->ImagingBBox[1] = ibba.data[1];
        dev->ImagingBBox[2] = ibba.data[2];
        dev->ImagingBBox[3] = ibba.data[3];
        dev->ImagingBBox_set = true;
    } else if (ibbnull) {
        dev->ImagingBBox_set = false;
    }
    dev->UseCIEColor = ucc;
        dev->color_info.anti_alias.text_bits =
                param_normalize_anti_alias_bits(max(dev->color_info.max_gray,
                        dev->color_info.max_color), tab);
        dev->color_info.anti_alias.graphics_bits =
                param_normalize_anti_alias_bits(max(dev->color_info.max_gray,
                        dev->color_info.max_color), gab);
    dev->LockSafetyParams = locksafe;
    dev->MaxPatternBitmap = mpbm;
    dev->interpolate_control = ic;
    dev->space_params = sp;
    dev->page_uses_transparency = page_uses_transparency;
    gx_device_decache_colors(dev);

    /* Take care of the rendering intents and blackpts.  For those that
       are not set special, the default provides an override */
    if (dev->icc_struct != NULL) {
        /* Set the default object */
        code = gx_default_put_intent(rend_intent[0], dev, gsDEFAULTPROFILE);
        if (code < 0)
            return code;
        code = gx_default_put_blackptcomp(blackptcomp[0], dev, gsDEFAULTPROFILE);
        if (code < 0)
            return code;
        code = gx_default_put_blackpreserve(blackpreserve[0], dev, gsDEFAULTPROFILE);
        if (code < 0)
            return code;
        /* If the default was specified and not a specialized one (e.g. graphic
           image or text) then the special one will get set to the default.  */
        for (k = 1; k < NUM_DEVICE_PROFILES; k++) {
            if (rend_intent[0] != gsRINOTSPECIFIED &&
                rend_intent[k] == gsRINOTSPECIFIED) {
                code = gx_default_put_intent(rend_intent[0], dev, profile_types[k]);
            } else {
                code = gx_default_put_intent(rend_intent[k], dev, profile_types[k]);
            }
            if (code < 0)
                return code;
            if (blackptcomp[0] != gsBPNOTSPECIFIED &&
                blackptcomp[k] == gsBPNOTSPECIFIED) {
                code = gx_default_put_blackptcomp(blackptcomp[0], dev, profile_types[k]);
            } else {
                code = gx_default_put_blackptcomp(blackptcomp[k], dev, profile_types[k]);
            }
            if (code < 0)
                return code;
            if (blackpreserve[0] != gsBKPRESNOTSPECIFIED &&
                blackpreserve[k] == gsBKPRESNOTSPECIFIED) {
                code = gx_default_put_blackpreserve(blackpreserve[0], dev, profile_types[k]);
            } else {
                code = gx_default_put_blackpreserve(blackpreserve[k], dev, profile_types[k]);
            }
            if (code < 0)
                return code;
        }
    }
    gsicc_setcoloraccuracy(dev->memory, color_accuracy);
    code = gx_default_put_graytok(devicegraytok, dev);
    if (code < 0)
        return code;
    code = gx_default_put_usefastcolor(usefastcolor, dev);
    if (code < 0)
        return code;
    code = gx_default_put_simulateoverprint(sim_overprint, dev);
    if (code < 0)
        return code;
    code = gx_default_put_graydetection(graydetection, dev);
    if (code < 0)
        return code;
    return gx_default_put_prebandthreshold(prebandthreshold, dev);
}

void
gx_device_request_leadingedge(gx_device *dev, int le_req)
{
    dev->LeadingEdge = (dev->LeadingEdge & ~LEADINGEDGE_REQ_VAL) |
        ((le_req << LEADINGEDGE_REQ_VAL_SHIFT) & LEADINGEDGE_REQ_VAL) |
        LEADINGEDGE_REQ_BIT;
}

/* Limit the anti-alias bit values to the maximum legal value for the
 * current color depth.
 */
static int
param_normalize_anti_alias_bits( uint max_gray, int bits )
{
        int	max_bits = ilog2( max_gray + 1);

        return  (bits > max_bits ? max_bits : bits);
}

/* Read TextAlphaBits or GraphicsAlphaBits. */
static int
param_anti_alias_bits(gs_param_list * plist, gs_param_name param_name, int *pa)
{
    int code = param_read_int(plist, param_name, pa);

    switch (code) {
    case 0:
        switch (*pa) {
        case 1: case 2: case 4:
            return 0;
        default:
            code = gs_error_rangecheck;
        }
        /* fall through */
    default:
        param_signal_error(plist, param_name, code);
    case 1:
        ;
    }
    return code;
}

/* Read .MediaSize or, if supported as a synonym, PageSize. */
static int
param_MediaSize(gs_param_list * plist, gs_param_name pname,
                const float *res, gs_param_float_array * pa)
{
    gs_param_name param_name;
    int ecode = 0;
    int code;

    BEGIN_ARRAY_PARAM(param_read_float_array, pname, *pa, 2, mse) {
        float width_new = pa->data[0] * res[0] / 72;
        float height_new = pa->data[1] * res[1] / 72;

        if (width_new < 0 || height_new < 0)
            ecode = gs_note_error(gs_error_rangecheck);
#define max_coord (max_fixed / fixed_1)
#if max_coord < max_int
        else if (width_new > (long)max_coord || height_new > (long)max_coord)
            ecode = gs_note_error(gs_error_limitcheck);
#endif
#undef max_coord
        else
            break;
    } END_ARRAY_PARAM(*pa, mse);
    return ecode;
}

/* Check that a nominally read-only parameter is being set to */
/* its existing value. */
static int
param_check_bool(gs_param_list * plist, gs_param_name pname, bool value,
                 bool is_defined)
{
    int code;
    bool new_value;

    switch (code = param_read_bool(plist, pname, &new_value)) {
        case 0:
            if (is_defined && new_value == value)
                break;
            code = gs_note_error(gs_error_rangecheck);
            goto e;
        default:
            if (param_read_null(plist, pname) == 0)
                return 1;
          e:param_signal_error(plist, pname, code);
        case 1:
            ;
    }
    return code;
}
static int
param_check_long(gs_param_list * plist, gs_param_name pname, long value,
                 bool is_defined)
{
    int code;
    long new_value;

    switch (code = param_read_long(plist, pname, &new_value)) {
        case 0:
            if (is_defined && new_value == value)
                break;
            code = gs_note_error(gs_error_rangecheck);
            goto e;
        default:
            if (param_read_null(plist, pname) == 0)
                return 1;
          e:param_signal_error(plist, pname, code);
        case 1:
            ;
    }
    return code;
}
static int
param_check_bytes(gs_param_list * plist, gs_param_name pname, const byte * str,
                  uint size, bool is_defined)
{
    int code;
    gs_param_string new_value;

    switch (code = param_read_string(plist, pname, &new_value)) {
        case 0:
            if (is_defined && new_value.size == size &&
                !memcmp((const char *)str, (const char *)new_value.data,
                        size)
                )
                break;
            code = gs_note_error(gs_error_rangecheck);
            goto e;
        default:
            if (param_read_null(plist, pname) == 0)
                return 1;
          e:param_signal_error(plist, pname, code);
        case 1:
            ;
    }
    return code;
}
