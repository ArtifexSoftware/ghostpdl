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


/* gsicc interface to littleCMS */

#include "memory_.h"
#include "lcms2.h"
#include "lcms2_plugin.h"
#include "gslibctx.h"
#include "gserrors.h"
#include "gp.h"
#include "gsicc_cms.h"
#include "gxdevice.h"

#define USE_LCMS2_LOCKING

#ifdef USE_LCMS2_LOCKING
#include "gxsync.h"
#endif

#define DUMP_CMS_BUFFER 0
#define DEBUG_LCMS_MEM 0
#define LCMS_BYTES_MASK 0x7

typedef struct gsicc_lcms2_link_s gsicc_lcms2_link_t;

/* Only provide warning about issues in lcms if debug build */
static void
gscms_error(cmsContext       ContextID,
            cmsUInt32Number  error_code,
            const char      *error_text)
{
#ifdef DEBUG
    gs_warn1("cmm error : %s",error_text);
#endif
}

static
void *gs_lcms2_malloc(cmsContext id, unsigned int size)
{
    void *ptr;
#if !(defined(SHARE_LCMS) && SHARE_LCMS==1)
    gs_memory_t *mem = (gs_memory_t *)cmsGetContextUserData(id);
#endif

#if defined(SHARE_LCMS) && SHARE_LCMS==1
    ptr = malloc(size);
#else
    ptr = gs_alloc_bytes(mem, size, "lcms");
#endif

#if DEBUG_LCMS_MEM
    gs_warn2("lcms malloc (%d) at 0x%x",size,ptr);
#endif
    return ptr;
}

static
void gs_lcms2_free(cmsContext id, void *ptr)
{
#if !(defined(SHARE_LCMS) && SHARE_LCMS==1)
    gs_memory_t *mem = (gs_memory_t *)cmsGetContextUserData(id);
#endif

    if (ptr != NULL) {
#if DEBUG_LCMS_MEM
        gs_warn1("lcms free at 0x%x",ptr);
#endif

#if defined(SHARE_LCMS) && SHARE_LCMS==1
        free(ptr);
#else
        gs_free_object(mem, ptr, "lcms");
#endif
    }
}

static
void *gs_lcms2_realloc(cmsContext id, void *ptr, unsigned int size)
{
#if !(defined(SHARE_LCMS) && SHARE_LCMS==1)
    gs_memory_t *mem = (gs_memory_t *)cmsGetContextUserData(id);
#endif
    void *ptr2;

    if (ptr == 0)
        return gs_lcms2_malloc(id, size);
    if (size == 0)
    {
        gs_lcms2_free(id, ptr);
        return NULL;
    }
#if defined(SHARE_LCMS) && SHARE_LCMS==1
    ptr2 = realloc(ptr, size);
#else
    ptr2 = gs_resize_object(mem, ptr, size, "lcms");
#endif

#if DEBUG_LCMS_MEM
    gs_warn3("lcms realloc (%x,%d) at 0x%x",ptr,size,ptr2);
#endif
    return ptr2;
}

static cmsPluginMemHandler gs_cms_memhandler =
{
    {
        cmsPluginMagicNumber,
        2000,
        cmsPluginMemHandlerSig,
        NULL
    },
    gs_lcms2_malloc,
    gs_lcms2_free,
    gs_lcms2_realloc,
    NULL,
    NULL,
    NULL,
};

#ifdef USE_LCMS2_LOCKING

static
void *gs_lcms2_createMutex(cmsContext id)
{
    gs_memory_t *mem = (gs_memory_t *)cmsGetContextUserData(id);

    return gx_monitor_label(gx_monitor_alloc(mem), "lcms2");
}

static
void gs_lcms2_destroyMutex(cmsContext id, void* mtx)
{
    gx_monitor_free((gx_monitor_t *)mtx);
}

static
cmsBool gs_lcms2_lockMutex(cmsContext id, void* mtx)
{
    return !gx_monitor_enter((gx_monitor_t *)mtx);
}

static
void gs_lcms2_unlockMutex(cmsContext id, void* mtx)
{
    gx_monitor_leave((gx_monitor_t *)mtx);
}

static cmsPluginMutex gs_cms_mutexhandler =
{
    {
        cmsPluginMagicNumber,
        2060,
        cmsPluginMutexSig,
        NULL
    },
    gs_lcms2_createMutex,
    gs_lcms2_destroyMutex,
    gs_lcms2_lockMutex,
    gs_lcms2_unlockMutex
};

#endif

/* Get the number of channels for the profile.
  Input count */
int
gscms_get_input_channel_count(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsColorSpaceSignature colorspace;

    colorspace = cmsGetColorSpace(profile);
    return(cmsChannelsOf(colorspace));
}

/* Get the number of output channels for the profile */
int
gscms_get_output_channel_count(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsColorSpaceSignature colorspace;

    colorspace = cmsGetPCS(profile);
    return(cmsChannelsOf(colorspace));
}

/* Get the number of colorant names in the clrt tag */
int
gscms_get_numberclrtnames(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsNAMEDCOLORLIST *lcms_names;

    lcms_names = (cmsNAMEDCOLORLIST *)cmsReadTag(profile,
                                                 cmsSigColorantTableTag);
    return cmsNamedColorCount(lcms_names);
}

/* Get the nth colorant name in the clrt tag */
char*
gscms_get_clrtname(gcmmhprofile_t profile, int colorcount, gs_memory_t *memory)
{
    cmsNAMEDCOLORLIST *lcms_names;
    char name[256];
    char *buf;
    int length;

    lcms_names = (cmsNAMEDCOLORLIST *)cmsReadTag(profile,
                                                 cmsSigColorantTableTag);
    if (colorcount >= cmsNamedColorCount(lcms_names)) return(NULL);
    if (cmsNamedColorInfo(lcms_names,
                          colorcount,
                          name,
                          NULL,
                          NULL,
                          NULL,
                          NULL) == 0)
        return NULL;
    length = strlen(name);
    buf = (char*) gs_alloc_bytes(memory, length + 1, "gscms_get_clrtname");
    if (buf)
        strcpy(buf, name);
    return buf;
}

/* Check if the profile is a device link type */
bool
gscms_is_device_link(gcmmhprofile_t profile, gs_memory_t *memory)
{
    return cmsGetDeviceClass(profile) == cmsSigLinkClass;
}

/* Needed for v2 profile creation */
int
gscms_get_device_class(gcmmhprofile_t profile, gs_memory_t *memory)
{
    return cmsGetDeviceClass(profile);
}

/* Check if the profile is a input type */
bool
gscms_is_input(gcmmhprofile_t profile, gs_memory_t *memory)
{
    return (cmsGetDeviceClass(profile) == cmsSigInputClass);
}

/* Get the device space associated with this profile */
gsicc_colorbuffer_t
gscms_get_profile_data_space(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsColorSpaceSignature colorspace;

    colorspace = cmsGetColorSpace(profile);
    switch( colorspace ) {
        case cmsSigXYZData:
            return(gsCIEXYZ);
        case cmsSigLabData:
            return(gsCIELAB);
        case cmsSigRgbData:
            return(gsRGB);
        case cmsSigGrayData:
            return(gsGRAY);
        case cmsSigCmykData:
            return(gsCMYK);
        default:
            return(gsNCHANNEL);
    }
}

/* Get ICC Profile handle from buffer */
gcmmhprofile_t
gscms_get_profile_handle_mem(unsigned char *buffer,
                             unsigned int input_size, gs_memory_t *mem)
{
    cmsContext ctx = gs_lib_ctx_get_cms_context(mem);

    cmsSetLogErrorHandlerTHR(ctx, gscms_error);
    return(cmsOpenProfileFromMemTHR(ctx,buffer,input_size));
}

/* Get ICC Profile handle from file ptr */
gcmmhprofile_t
gscms_get_profile_handle_file(const char *filename, gs_memory_t *mem)
{
    cmsContext ctx = gs_lib_ctx_get_cms_context(mem);

    return(cmsOpenProfileFromFileTHR(ctx, filename, "r"));
}

/* Transform an entire buffer */
int
gscms_transform_color_buffer(gx_device *dev, gsicc_link_t *icclink,
                             gsicc_bufferdesc_t *input_buff_desc,
                             gsicc_bufferdesc_t *output_buff_desc,
                             void *inputbuffer,
                             void *outputbuffer)
{
    cmsHTRANSFORM hTransform = (cmsHTRANSFORM)icclink->link_handle;
    cmsUInt32Number dwInputFormat, dwOutputFormat, num_src_lcms, num_des_lcms;
    int planar,numbytes, swap_endian, hasalpha, k;
    unsigned char *inputpos, *outputpos;
#if DUMP_CMS_BUFFER
    gp_file *fid_in, *fid_out;
#endif
    /* Although little CMS does  make assumptions about data types in its
       transformations you can change it after the fact.  */
    /* Set us to the proper output type */
    /* Note, we could speed this up by passing back the encoded data type
        to the caller so that we could avoid having to go through this
        computation each time if they are doing multiple calls to this
        operation */
    /* Color space MUST be the same */
    dwInputFormat = COLORSPACE_SH(T_COLORSPACE(cmsGetTransformInputFormat(hTransform)));
    dwOutputFormat = COLORSPACE_SH(T_COLORSPACE(cmsGetTransformOutputFormat(hTransform)));

    /* Now set if we have planar, num bytes, endian case, and alpha data to skip */
    /* Planar -- pdf14 case for example */
    planar = input_buff_desc->is_planar;
    dwInputFormat = dwInputFormat | PLANAR_SH(planar);
    planar = output_buff_desc->is_planar;
    dwOutputFormat = dwOutputFormat | PLANAR_SH(planar);

    /* 8 or 16 byte input and output */
    numbytes = input_buff_desc->bytes_per_chan;
    if (numbytes>2) numbytes = 0;  /* littleCMS encodes float with 0 ToDO. */
    dwInputFormat = dwInputFormat | BYTES_SH(numbytes);
    numbytes = output_buff_desc->bytes_per_chan;
    if (numbytes>2) numbytes = 0;
    dwOutputFormat = dwOutputFormat | BYTES_SH(numbytes);

    /* endian */
    swap_endian = input_buff_desc->endian_swap;
    dwInputFormat = dwInputFormat | ENDIAN16_SH(swap_endian);
    swap_endian = output_buff_desc->endian_swap;
    dwOutputFormat = dwOutputFormat | ENDIAN16_SH(swap_endian);

    /* number of channels.  This should not really be changing! */
    num_src_lcms = T_CHANNELS(cmsGetTransformInputFormat(hTransform));
    num_des_lcms = T_CHANNELS(cmsGetTransformOutputFormat(hTransform));
    if (num_src_lcms != input_buff_desc->num_chan ||
        num_des_lcms != output_buff_desc->num_chan) {
        /* We can't transform this. Someone is doing something odd */
        return_error(gs_error_unknownerror);
    }
    dwInputFormat = dwInputFormat | CHANNELS_SH(num_src_lcms);
    dwOutputFormat = dwOutputFormat | CHANNELS_SH(num_des_lcms);

    /* alpha, which is passed through unmolested */
    /* ToDo:  Right now we always must have alpha last */
    /* This is really only going to be an issue when we have
       interleaved alpha data */
    hasalpha = input_buff_desc->has_alpha;
    dwInputFormat = dwInputFormat | EXTRA_SH(hasalpha);
    dwOutputFormat = dwOutputFormat | EXTRA_SH(hasalpha);

    /* Change the formatters */
    cmsChangeBuffersFormat(hTransform,dwInputFormat,dwOutputFormat);

    /* littleCMS knows nothing about word boundarys.  As such, we need to do
       this row by row adjusting for our stride.  Output buffer must already
       be allocated. ToDo:  Check issues with plane and row stride and word
       boundry */
    inputpos = (byte *) inputbuffer;
    outputpos = (byte *) outputbuffer;
    if(input_buff_desc->is_planar){
        /* Determine if we can do this in one operation or if we have to break
           it up.  Essentially if the width * height = plane_stride then yes.  If
           we are doing some subsection of a plane then no. */
        if (input_buff_desc->num_rows * input_buff_desc->pixels_per_row ==
            input_buff_desc->plane_stride  &&
            output_buff_desc->num_rows * output_buff_desc->pixels_per_row ==
            output_buff_desc->plane_stride) {
            /* Do entire buffer.*/
            cmsDoTransform(hTransform, inputpos, outputpos,
                           input_buff_desc->plane_stride);
        } else {
            /* We have to do this row by row, with memory transfers */
            byte *temp_des, *temp_src;
            int source_size = input_buff_desc->bytes_per_chan *
                              input_buff_desc->pixels_per_row;

            int des_size = output_buff_desc->bytes_per_chan *
                           output_buff_desc->pixels_per_row;
            int y, i;

            temp_src = (byte*)gs_alloc_bytes(dev->memory->non_gc_memory,
                                              source_size * input_buff_desc->num_chan,
                                              "gscms_transform_color_buffer");
            if (temp_src == NULL)
                return_error(gs_error_VMerror);
            temp_des = (byte*) gs_alloc_bytes(dev->memory->non_gc_memory,
                                              des_size * output_buff_desc->num_chan,
                                              "gscms_transform_color_buffer");
            if (temp_des == NULL)
                return_error(gs_error_VMerror);
            for (y = 0; y < input_buff_desc->num_rows; y++) {
                byte *src_cm = temp_src;
                byte *src_buff = inputpos;
                byte *des_cm = temp_des;
                byte *des_buff = outputpos;

                /* Put into planar temp buffer */
                for (i = 0; i < input_buff_desc->num_chan; i ++) {
                    memcpy(src_cm, src_buff, source_size);
                    src_cm += source_size;
                    src_buff += input_buff_desc->plane_stride;
                }
                /* Transform */
                cmsDoTransform(hTransform, temp_src, temp_des,
                               input_buff_desc->pixels_per_row);
                /* Get out of temp planar buffer */
                for (i = 0; i < output_buff_desc->num_chan; i ++) {
                    memcpy(des_buff, des_cm, des_size);
                    des_cm += des_size;
                    des_buff += output_buff_desc->plane_stride;
                }
                inputpos += input_buff_desc->row_stride;
                outputpos += output_buff_desc->row_stride;
            }
            gs_free_object(dev->memory->non_gc_memory, temp_src,
                           "gscms_transform_color_buffer");
            gs_free_object(dev->memory->non_gc_memory, temp_des,
                           "gscms_transform_color_buffer");
        }
    } else {
        /* Do row by row. */
        for(k = 0; k < input_buff_desc->num_rows ; k++){
            cmsDoTransform(hTransform, inputpos, outputpos,
                           input_buff_desc->pixels_per_row);
            inputpos += input_buff_desc->row_stride;
            outputpos += output_buff_desc->row_stride;
        }
    }
#if DUMP_CMS_BUFFER
    fid_in = gp_fopen(dev->memory,"CM_Input.raw","ab");
    fid_out = gp_fopen(dev->memory,"CM_Output.raw","ab");
    fwrite((unsigned char*) inputbuffer,sizeof(unsigned char),
                            input_buff_desc->row_stride,fid_in);
    fwrite((unsigned char*) outputbuffer,sizeof(unsigned char),
                            output_buff_desc->row_stride,fid_out);
    fclose(fid_in);
    fclose(fid_out);
#endif
    return 0;
}

/* Transform a single color. We assume we have passed to us the proper number
   of elements of size gx_device_color. It is up to the caller to make sure
   the proper allocations for the colors are there. */
int
gscms_transform_color(gx_device *dev, gsicc_link_t *icclink, void *inputcolor,
                             void *outputcolor, int num_bytes)
{
    return gscms_transform_color_const(dev, icclink, inputcolor, outputcolor, num_bytes);
}

int
gscms_transform_color_const(const gx_device *dev, gsicc_link_t *icclink, void *inputcolor,
                             void *outputcolor, int num_bytes)
{
    cmsHTRANSFORM hTransform = (cmsHTRANSFORM)icclink->link_handle;
    cmsUInt32Number dwInputFormat,dwOutputFormat;

    /* For a single color, we are going to use the link as it is
       with the exception of taking care of the word size. */
    /* numbytes = sizeof(gx_color_value); */
    if (num_bytes>2) num_bytes = 0;  /* littleCMS encodes float with 0 ToDO. */
    dwInputFormat = cmsGetTransformInputFormat(hTransform);
    dwOutputFormat = cmsGetTransformOutputFormat(hTransform);
    dwInputFormat = (dwInputFormat & (~LCMS_BYTES_MASK))  | BYTES_SH(num_bytes);
    dwOutputFormat = (dwOutputFormat & (~LCMS_BYTES_MASK)) | BYTES_SH(num_bytes);
    /* Change the formatters */
    cmsChangeBuffersFormat(hTransform,dwInputFormat,dwOutputFormat);
    /* Do conversion */
    cmsDoTransform(hTransform,inputcolor,outputcolor,1);

    return 0;
}

/* Get the flag to avoid having to the cmm do any white fix up, it such a flag
   exists for the cmm */
int
gscms_avoid_white_fix_flag(gs_memory_t *memory)
{
    return cmsFLAGS_NOWHITEONWHITEFIXUP;
}

void
gscms_get_link_dim(gcmmhlink_t link, int *num_inputs, int *num_outputs,
    gs_memory_t *memory)
{
    *num_inputs = T_CHANNELS(cmsGetTransformInputFormat(link));
    *num_outputs = T_CHANNELS(cmsGetTransformOutputFormat(link));
}

/* Get the link from the CMS. TODO:  Add error checking */
gcmmhlink_t
gscms_get_link(gcmmhprofile_t  lcms_srchandle,
               gcmmhprofile_t lcms_deshandle,
               gsicc_rendering_param_t *rendering_params, int cmm_flags,
               gs_memory_t *memory)
{
    cmsUInt32Number src_data_type,des_data_type;
    cmsColorSpaceSignature src_color_space,des_color_space;
    int src_nChannels,des_nChannels;
    int lcms_src_color_space, lcms_des_color_space;
    unsigned int flag;
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    /* Check for case of request for a transfrom from a device link profile
       in that case, the destination profile is NULL */

   /* First handle all the source stuff */
    src_color_space  = cmsGetColorSpace(lcms_srchandle);
    lcms_src_color_space = _cmsLCMScolorSpace(src_color_space);
    /* littlecms returns -1 for types it does not (but should) understand */
    if (lcms_src_color_space < 0) lcms_src_color_space = 0;
    src_nChannels = cmsChannelsOf(src_color_space);
    /* For now, just do single byte data, interleaved.  We can change this
      when we use the transformation. */
    src_data_type = (COLORSPACE_SH(lcms_src_color_space)|
                        CHANNELS_SH(src_nChannels)|BYTES_SH(2));

    if (lcms_deshandle != NULL) {
        des_color_space  = cmsGetColorSpace(lcms_deshandle);
    } else {
        /* We must have a device link profile. Use it's PCS space. */
        des_color_space = cmsGetPCS(lcms_srchandle);
    }
    lcms_des_color_space = _cmsLCMScolorSpace(des_color_space);
    if (lcms_des_color_space < 0) lcms_des_color_space = 0;
    des_nChannels = cmsChannelsOf(des_color_space);
    des_data_type = (COLORSPACE_SH(lcms_des_color_space)|
                        CHANNELS_SH(des_nChannels)|BYTES_SH(2));

    /* Set up the flags */
    flag = cmsFLAGS_HIGHRESPRECALC;
    if (rendering_params->black_point_comp == gsBLACKPTCOMP_ON
        || rendering_params->black_point_comp == gsBLACKPTCOMP_ON_OR) {
        flag = (flag | cmsFLAGS_BLACKPOINTCOMPENSATION);
    }
    if (rendering_params->preserve_black == gsBLACKPRESERVE_KONLY) {
        switch (rendering_params->rendering_intent) {
            case INTENT_PERCEPTUAL:
                rendering_params->rendering_intent = INTENT_PRESERVE_K_ONLY_PERCEPTUAL;
                break;
            case INTENT_RELATIVE_COLORIMETRIC:
                rendering_params->rendering_intent = INTENT_PRESERVE_K_ONLY_RELATIVE_COLORIMETRIC;
                break;
            case INTENT_SATURATION:
                rendering_params->rendering_intent = INTENT_PRESERVE_K_ONLY_SATURATION;
                break;
            default:
                break;
        }
    }
    if (rendering_params->preserve_black == gsBLACKPRESERVE_KPLANE) {
        switch (rendering_params->rendering_intent) {
            case INTENT_PERCEPTUAL:
                rendering_params->rendering_intent = INTENT_PRESERVE_K_PLANE_PERCEPTUAL;
                break;
            case INTENT_RELATIVE_COLORIMETRIC:
                rendering_params->rendering_intent = INTENT_PRESERVE_K_PLANE_RELATIVE_COLORIMETRIC;
                break;
            case INTENT_SATURATION:
                rendering_params->rendering_intent = INTENT_PRESERVE_K_PLANE_SATURATION;
                break;
            default:
                break;
        }
    }
    /* Create the link */
    return cmsCreateTransformTHR(ctx,
               lcms_srchandle, src_data_type,
               lcms_deshandle, des_data_type,
               rendering_params->rendering_intent, flag | cmm_flags);
    /* cmsFLAGS_HIGHRESPRECALC)  cmsFLAGS_NOTPRECALC  cmsFLAGS_LOWRESPRECALC*/
}

/* Get the link from the CMS, but include proofing and/or a device link
   profile.  Note also, that the source may be a device link profile, in
   which case we will not have a destination profile but could still have
   a proof profile or an additional device link profile */
gcmmhlink_t
gscms_get_link_proof_devlink(gcmmhprofile_t lcms_srchandle,
                             gcmmhprofile_t lcms_proofhandle,
                             gcmmhprofile_t lcms_deshandle,
                             gcmmhprofile_t lcms_devlinkhandle,
                             gsicc_rendering_param_t *rendering_params,
                             bool src_dev_link, int cmm_flags,
                             gs_memory_t *memory)
{
    cmsUInt32Number src_data_type,des_data_type;
    cmsColorSpaceSignature src_color_space,des_color_space;
    int src_nChannels,des_nChannels;
    int lcms_src_color_space, lcms_des_color_space;
    cmsHPROFILE hProfiles[5];
    int nProfiles = 0;
    unsigned int flag;
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    /* Check if the rendering intent is something other than relative colorimetric
       and  if we have a proofing profile.  In this case we need to create the
       combined profile a bit different.  LCMS does not allow us to use different
       intents in the cmsCreateMultiprofileTransform transform.  Also, don't even
       think about doing this if someone has snuck in a source based device link
       profile into the mix */
    if (lcms_proofhandle != NULL &&
        rendering_params->rendering_intent != gsRELATIVECOLORIMETRIC &&
        !src_dev_link) {
        /* First handle the source to proof profile with its particular intent as
           a device link profile */
        cmsHPROFILE src_to_proof;
        cmsHTRANSFORM temptransform;

        temptransform = gscms_get_link(lcms_srchandle, lcms_proofhandle,
                                      rendering_params, cmm_flags, memory);
        /* Now mash that to a device link profile */
        flag = cmsFLAGS_HIGHRESPRECALC;
        if (rendering_params->black_point_comp == gsBLACKPTCOMP_ON ||
            rendering_params->black_point_comp == gsBLACKPTCOMP_ON_OR) {
            flag = (flag | cmsFLAGS_BLACKPOINTCOMPENSATION);
        }
        src_to_proof = cmsTransform2DeviceLink(temptransform, 3.4, flag);
        /* Free up the link handle */
        cmsDeleteTransform(temptransform);
        src_color_space  = cmsGetColorSpace(src_to_proof);
        lcms_src_color_space = _cmsLCMScolorSpace(src_color_space);
        /* littlecms returns -1 for types it does not (but should) understand */
        if (lcms_src_color_space < 0) lcms_src_color_space = 0;
        src_nChannels = cmsChannelsOf(src_color_space);
        /* For now, just do single byte data, interleaved.  We can change this
          when we use the transformation. */
        src_data_type = (COLORSPACE_SH(lcms_src_color_space)|
                            CHANNELS_SH(src_nChannels)|BYTES_SH(2));
        if (lcms_devlinkhandle == NULL) {
            des_color_space = cmsGetColorSpace(lcms_deshandle);
        } else {
            des_color_space = cmsGetPCS(lcms_devlinkhandle);
        }
        lcms_des_color_space = _cmsLCMScolorSpace(des_color_space);
        if (lcms_des_color_space < 0) lcms_des_color_space = 0;
        des_nChannels = cmsChannelsOf(des_color_space);
        des_data_type = (COLORSPACE_SH(lcms_des_color_space)|
                            CHANNELS_SH(des_nChannels)|BYTES_SH(2));
        /* Now, we need to go back through the proofing profile, to the
           destination and then to the device link profile if there was one. */
        hProfiles[nProfiles++] = src_to_proof;  /* Src to proof with special intent */
        hProfiles[nProfiles++] = lcms_proofhandle; /* Proof to CIELAB */
        if (lcms_deshandle != NULL) {
            hProfiles[nProfiles++] = lcms_deshandle;  /* Our destination */
        }
        /* The output device link profile */
        if (lcms_devlinkhandle != NULL) {
            hProfiles[nProfiles++] = lcms_devlinkhandle;
        }
        flag = cmsFLAGS_HIGHRESPRECALC;
        if (rendering_params->black_point_comp == gsBLACKPTCOMP_ON
            || rendering_params->black_point_comp == gsBLACKPTCOMP_ON_OR) {
            flag = (flag | cmsFLAGS_BLACKPOINTCOMPENSATION);
        }
        /* Use relative colorimetric here */
        temptransform = cmsCreateMultiprofileTransformTHR(ctx,
                    hProfiles, nProfiles, src_data_type,
                    des_data_type, gsRELATIVECOLORIMETRIC, flag);
        cmsCloseProfile(src_to_proof);
        return temptransform;
    } else {
       /* First handle all the source stuff */
        src_color_space  = cmsGetColorSpace(lcms_srchandle);
        lcms_src_color_space = _cmsLCMScolorSpace(src_color_space);
        /* littlecms returns -1 for types it does not (but should) understand */
        if (lcms_src_color_space < 0) lcms_src_color_space = 0;
        src_nChannels = cmsChannelsOf(src_color_space);
        /* For now, just do single byte data, interleaved.  We can change this
          when we use the transformation. */
        src_data_type = (COLORSPACE_SH(lcms_src_color_space)|
                            CHANNELS_SH(src_nChannels)|BYTES_SH(2));
        if (lcms_devlinkhandle == NULL) {
            if (src_dev_link) {
                des_color_space = cmsGetPCS(lcms_srchandle);
            } else {
                des_color_space = cmsGetColorSpace(lcms_deshandle);
            }
        } else {
            des_color_space = cmsGetPCS(lcms_devlinkhandle);
        }
        lcms_des_color_space = _cmsLCMScolorSpace(des_color_space);
        if (lcms_des_color_space < 0) lcms_des_color_space = 0;
        des_nChannels = cmsChannelsOf(des_color_space);
        des_data_type = (COLORSPACE_SH(lcms_des_color_space)|
                            CHANNELS_SH(des_nChannels)|BYTES_SH(2));
        /* lcms proofing transform has a clunky API and can't include the device
           link profile if we have both. So use cmsCreateMultiprofileTransform
           instead and round trip the proofing profile. */
        hProfiles[nProfiles++] = lcms_srchandle;
        /* Note if source is device link, we cannot do any proofing */
        if (lcms_proofhandle != NULL && !src_dev_link) {
            hProfiles[nProfiles++] = lcms_proofhandle;
            hProfiles[nProfiles++] = lcms_proofhandle;
        }
        /* This should be NULL if we have a source device link */
        if (lcms_deshandle != NULL) {
            hProfiles[nProfiles++] = lcms_deshandle;
        }
        /* Someone could have a device link at the output, giving us possibly two
           device link profiles to smash together */
        if (lcms_devlinkhandle != NULL) {
            hProfiles[nProfiles++] = lcms_devlinkhandle;
        }
        flag = cmsFLAGS_HIGHRESPRECALC;
        if (rendering_params->black_point_comp == gsBLACKPTCOMP_ON
            || rendering_params->black_point_comp == gsBLACKPTCOMP_ON_OR) {
            flag = (flag | cmsFLAGS_BLACKPOINTCOMPENSATION);
        }
        return cmsCreateMultiprofileTransformTHR(ctx,
                    hProfiles, nProfiles, src_data_type,
                    des_data_type, rendering_params->rendering_intent, flag);
    }
}

/* Do any initialization if needed to the CMS */
void *
gscms_create(gs_memory_t *memory)
{
    cmsContext ctx;

    /* Set our own error handling function */
    ctx = cmsCreateContext((void *)&gs_cms_memhandler, memory);
    if (ctx == NULL)
        return NULL;

#ifdef USE_LCMS2_LOCKING
    cmsPluginTHR(ctx, (void *)&gs_cms_mutexhandler);
#endif

    cmsSetLogErrorHandlerTHR(ctx, gscms_error);

    return ctx;
}

/* Do any clean up when done with the CMS if needed */
void
gscms_destroy(void *cmsContext_)
{
    cmsContext ctx = (cmsContext)cmsContext_;
    if (ctx == NULL)
        return;

    cmsDeleteContext(ctx);
}

/* Have the CMS release the link */
void
gscms_release_link(gsicc_link_t *icclink)
{
    if (icclink->link_handle != NULL )
        cmsDeleteTransform(icclink->link_handle);

    icclink->link_handle = NULL;
}

/* Have the CMS release the profile handle */
void
gscms_release_profile(void *profile, gs_memory_t *memory)
{
    cmsHPROFILE profile_handle;

    profile_handle = (cmsHPROFILE) profile;
    cmsCloseProfile(profile_handle);
}

/* Named color, color management */
/* Get a device value for the named color.  Since there exist named color
   ICC profiles and littleCMS supports them, we will use
   that format in this example.  However it should be noted
   that this object need not be an ICC named color profile
   but can be a proprietary type table. Some CMMs do not
   support named color profiles.  In that case, or if
   the named color is not found, the caller should use an alternate
   tint transform or other method. If a proprietary
   format (nonICC) is being used, the operators given below must
   be implemented. In every case that I can imagine, this should be
   straight forward.  Note that we allow the passage of a tint
   value also.  Currently the ICC named color profile does not provide
   tint related information, only a value for 100% coverage.
   It is provided here for use in proprietary
   methods, which may be able to provide the desired effect.  We will
   at the current time apply a direct tint operation to the returned
   device value.
   Right now I don't see any reason to have the named color profile
   ever return CIELAB. It will either return device values directly or
   it will return values defined by the output device profile */

int
gscms_transform_named_color(gsicc_link_t *icclink,  float tint_value,
                            const char* ColorName, gx_color_value device_values[] )
{
    cmsHPROFILE hTransform = icclink->link_handle;
    unsigned short *deviceptr = device_values;
    int index;

    /* Check if name is present in the profile */
    /* If it is not return -1.  Caller will need to use an alternate method */
    if((index = cmsNamedColorIndex(hTransform, ColorName)) < 0) return -1;
    /* Get the device value. */
   cmsDoTransform(hTransform,&index,deviceptr,1);
   return(0);
}

/* Create a link to return device values inside the named color profile or link
   it with a destination profile and potentially a proofing profile.  If the
   output_colorspace and the proof_color space are NULL, then we will be
   returning the device values that are contained in the named color profile.
   i.e. in namedcolor_information.  Note that an ICC named color profile
    need NOT contain the device values but must contain the CIELAB values. */
void
gscms_get_name2device_link(gsicc_link_t *icclink,
                           gcmmhprofile_t  lcms_srchandle,
                           gcmmhprofile_t lcms_deshandle,
                           gcmmhprofile_t lcms_proofhandle,
                           gsicc_rendering_param_t *rendering_params)
{
    cmsHTRANSFORM hTransform;
    cmsUInt32Number dwOutputFormat;
    cmsUInt32Number lcms_proof_flag;
    int number_colors;
    cmsContext ctx = gs_lib_ctx_get_cms_context(icclink->memory);

    /* NOTE:  We need to add a test here to check that we even HAVE
    device values in here and NOT just CIELAB values */
    if ( lcms_proofhandle != NULL ){
        lcms_proof_flag = cmsFLAGS_GAMUTCHECK | cmsFLAGS_SOFTPROOFING;
    } else {
        lcms_proof_flag = 0;
    }
    /* Create the transform */
    /* ToDo:  Adjust rendering intent */
    hTransform = cmsCreateProofingTransformTHR(ctx,
                                            lcms_srchandle, TYPE_NAMED_COLOR_INDEX,
                                            lcms_deshandle, TYPE_CMYK_8,
                                            lcms_proofhandle,INTENT_PERCEPTUAL,
                                            INTENT_ABSOLUTE_COLORIMETRIC,
                                            lcms_proof_flag);
    /* In littleCMS there is no easy way to find out the size of the device
        space returned by the named color profile until after the transform is made.
        Hence we adjust our output format after creating the transform.  It is
        set to CMYK8 initially. */
    number_colors = cmsNamedColorCount(cmsGetNamedColorList(hTransform));
    /* NOTE: Output size of gx_color_value with no color space type check */
    dwOutputFormat =  (CHANNELS_SH(number_colors)|BYTES_SH(sizeof(gx_color_value)));
    /* Change the formatters */
    cmsChangeBuffersFormat(hTransform,TYPE_NAMED_COLOR_INDEX,dwOutputFormat);
    icclink->link_handle = hTransform;
    cmsCloseProfile(lcms_srchandle);
    if(lcms_deshandle) cmsCloseProfile(lcms_deshandle);
    if(lcms_proofhandle) cmsCloseProfile(lcms_proofhandle);
    return;
}

bool
gscms_is_threadsafe(void)
{
    return false;
}
