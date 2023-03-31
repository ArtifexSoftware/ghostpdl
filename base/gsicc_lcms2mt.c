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


/* gsicc interface to LittleCMS2-MT */

#include "memory_.h"
#include "lcms2mt.h"
#include "lcms2mt_plugin.h"
#include "gslibctx.h"
#include "gserrors.h"
#include "gp.h"
#include "gsicc_cms.h"
#include "gxdevice.h"

#ifdef WITH_CAL
#include "cal.h"
#endif

#define USE_LCMS2_LOCKING

#ifdef USE_LCMS2_LOCKING
#include "gxsync.h"
#endif

#define DUMP_CMS_BUFFER 0
#define DEBUG_LCMS_MEM 0
#define LCMS_BYTES_MASK T_BYTES(-1)	/* leaves only mask for the BYTES (currently 7) */
#define LCMS_ENDIAN16_MASK T_ENDIAN16(-1) /* similarly, for ENDIAN16 bit */

#define gsicc_link_flags(hasalpha, planarIN, planarOUT, endianswapIN, endianswapOUT, bytesIN, bytesOUT) \
    ((hasalpha != 0) << 2 | \
     (planarIN != 0) << 5 | (planarOUT != 0) << 4 | \
     (endianswapIN != 0) << 3 | (endianswapOUT != 0) << 2 | \
     (bytesIN == 1) << 1 | (bytesOUT == 1))

typedef struct gsicc_lcms2mt_link_list_s {
    int flags;
    cmsHTRANSFORM *hTransform;
    struct gsicc_lcms2mt_link_list_s *next;
} gsicc_lcms2mt_link_list_t;

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
    gs_memory_t *mem = (gs_memory_t *)cmsGetContextUserData(id);

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
    gs_memory_t *mem = (gs_memory_t *)cmsGetContextUserData(id);
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
    gs_memory_t *mem = (gs_memory_t *)cmsGetContextUserData(id);
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
        LCMS_VERSION,
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
        LCMS_VERSION,
        cmsPluginMutexSig,
        NULL
    },
    gs_lcms2_createMutex,
    gs_lcms2_destroyMutex,
    gs_lcms2_lockMutex,
    gs_lcms2_unlockMutex
};

#endif

static int
gscms_get_accuracy(gs_memory_t *mem)
{
    gs_lib_ctx_t *ctx = gs_lib_ctx_get_interp_instance(mem);

    switch (ctx->icc_color_accuracy) {
    case 0:
        return cmsFLAGS_LOWRESPRECALC;
    case 1:
        return 0;
    case 2:
    default:
        return cmsFLAGS_HIGHRESPRECALC;
    }
}

/* Get the number of channels for the profile.
  Input count */
int
gscms_get_input_channel_count(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsColorSpaceSignature colorspace;
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    colorspace = cmsGetColorSpace(ctx, profile);
    return cmsChannelsOf(ctx, colorspace);
}

/* Get the number of output channels for the profile */
int
gscms_get_output_channel_count(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsColorSpaceSignature colorspace;
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    colorspace = cmsGetPCS(ctx, profile);
    return cmsChannelsOf(ctx, colorspace);
}

/* Get the number of colorant names in the clrt tag */
int
gscms_get_numberclrtnames(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsNAMEDCOLORLIST *lcms_names;
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    lcms_names = (cmsNAMEDCOLORLIST *)cmsReadTag(ctx, profile,
        cmsSigColorantTableTag);
    return cmsNamedColorCount(ctx, lcms_names);
}

/* Get the nth colorant name in the clrt tag */
char*
gscms_get_clrtname(gcmmhprofile_t profile, int colorcount, gs_memory_t *memory)
{
    cmsNAMEDCOLORLIST *lcms_names;
    char name[256];
    char *buf;
    int length;
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    lcms_names = (cmsNAMEDCOLORLIST *)cmsReadTag(ctx, profile,
                                                 cmsSigColorantTableTag);
    if (colorcount >= cmsNamedColorCount(ctx, lcms_names)) return(NULL);
    if (cmsNamedColorInfo(ctx, lcms_names, colorcount, name, NULL, NULL, NULL,
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
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    return cmsGetDeviceClass(ctx, profile) == cmsSigLinkClass;
}

/* Needed for v2 profile creation */
int
gscms_get_device_class(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    return cmsGetDeviceClass(ctx, profile);
}

/* Check if the profile is a input type */
bool
gscms_is_input(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    return cmsGetDeviceClass(ctx, profile) == cmsSigInputClass;
}

/* Get the device space associated with this profile */
gsicc_colorbuffer_t
gscms_get_profile_data_space(gcmmhprofile_t profile, gs_memory_t *memory)
{
    cmsColorSpaceSignature colorspace;
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    colorspace = cmsGetColorSpace(ctx, profile);
    switch (colorspace) {
        case cmsSigXYZData:
            return gsCIEXYZ;
        case cmsSigLabData:
            return gsCIELAB;
        case cmsSigRgbData:
            return gsRGB;
        case cmsSigGrayData:
            return gsGRAY;
        case cmsSigCmykData:
            return gsCMYK;
        default:
            return gsNCHANNEL;
    }
}

/* Get ICC Profile handle from buffer */
gcmmhprofile_t
gscms_get_profile_handle_mem(unsigned char *buffer, unsigned int input_size,
    gs_memory_t *mem)
{
    cmsContext ctx = gs_lib_ctx_get_cms_context(mem);

    cmsSetLogErrorHandler(ctx, gscms_error);
    return cmsOpenProfileFromMem(ctx,buffer,input_size);
}

/* Get ICC Profile handle from file ptr */
gcmmhprofile_t
gscms_get_profile_handle_file(const char *filename, gs_memory_t *mem)
{
    cmsContext ctx = gs_lib_ctx_get_cms_context(mem);

    return cmsOpenProfileFromFile(ctx, filename, "r");
}

/* Transform an entire buffer */
int
gscms_transform_color_buffer(gx_device *dev, gsicc_link_t *icclink,
                             gsicc_bufferdesc_t *input_buff_desc,
                             gsicc_bufferdesc_t *output_buff_desc,
                             void *inputbuffer, void *outputbuffer)
{
    gsicc_lcms2mt_link_list_t *link_handle = (gsicc_lcms2mt_link_list_t *)(icclink->link_handle);
    cmsHTRANSFORM hTransform = (cmsHTRANSFORM)link_handle->hTransform;
    cmsUInt32Number dwInputFormat, dwOutputFormat, num_src_lcms, num_des_lcms;
    int  hasalpha, planarIN, planarOUT, numbytesIN, numbytesOUT, swap_endianIN, swap_endianOUT;
    int needed_flags = 0;
    unsigned char *inputpos, *outputpos;
    cmsContext ctx = gs_lib_ctx_get_cms_context(icclink->memory);

#if DUMP_CMS_BUFFER
    gp_file *fid_in, *fid_out;
#endif
    /* Although little CMS does  make assumptions about data types in its
       transformations we can change it after the fact by cloning from any
       other transform. We always create [0] which is no_alpha, chunky IN/OUT,
       no endian swap IN/OUT, 2-bytes_per_component IN/OUT. */
    /* Set us to the proper output type */
    /* Note, we could speed this up by passing back the encoded data type
        to the caller so that we could avoid having to go through this
        computation each time if they are doing multiple calls to this
        operation, but this is working on a buffer at a time. */

    /* Now set if we have planar, num bytes, endian case, and alpha data to skip */
    /* Planar -- pdf14 case for example */
    planarIN = input_buff_desc->is_planar;
    planarOUT = output_buff_desc->is_planar;

    /* 8 or 16 byte input and output */
    numbytesIN = input_buff_desc->bytes_per_chan;
    numbytesOUT = output_buff_desc->bytes_per_chan;
    if (numbytesIN > 2 || numbytesOUT > 2)
        return_error(gs_error_rangecheck);	/* TODO: we don't support float */

    /* endian */
    swap_endianIN = input_buff_desc->endian_swap;
    swap_endianOUT = output_buff_desc->endian_swap;

    /* alpha, which is passed through unmolested */
    /* TODO:  Right now we always must have alpha last */
    /* This is really only going to be an issue when we have interleaved alpha data */
    hasalpha = input_buff_desc->has_alpha;

    needed_flags = gsicc_link_flags(hasalpha, planarIN, planarOUT,
                                    swap_endianIN, swap_endianOUT,
                                    numbytesIN, numbytesOUT);
    while (link_handle->flags != needed_flags) {
        if (link_handle->next == NULL) {
            hTransform = NULL;
            break;
        } else {
            link_handle = link_handle->next;
            hTransform = link_handle->hTransform;
        }
    }
    if (hTransform == NULL) {
        /* the variant we want wasn't present, clone it from the last on the list */
        gsicc_lcms2mt_link_list_t *new_link_handle =
            (gsicc_lcms2mt_link_list_t *)gs_alloc_bytes(icclink->memory->non_gc_memory,
                                                         sizeof(gsicc_lcms2mt_link_list_t),
                                                         "gscms_transform_color_buffer");
        if (new_link_handle == NULL) {
            return_error(gs_error_VMerror);
        }
        new_link_handle->next = NULL;		/* new end of list */
        new_link_handle->flags = needed_flags;
        hTransform = link_handle->hTransform;	/* doesn't really matter which we start with */
        /* Color space MUST be the same */
        dwInputFormat = COLORSPACE_SH(T_COLORSPACE(cmsGetTransformInputFormat(ctx, hTransform)));
        dwOutputFormat = COLORSPACE_SH(T_COLORSPACE(cmsGetTransformOutputFormat(ctx, hTransform)));
        /* number of channels.  This should not really be changing! */
        num_src_lcms = T_CHANNELS(cmsGetTransformInputFormat(ctx, hTransform));
        num_des_lcms = T_CHANNELS(cmsGetTransformOutputFormat(ctx, hTransform));
        if (num_src_lcms != input_buff_desc->num_chan ||
            num_des_lcms != output_buff_desc->num_chan) {
            /* We can't transform this. Someone is doing something odd */
            return_error(gs_error_unknownerror);
        }
        dwInputFormat = dwInputFormat | CHANNELS_SH(num_src_lcms);
        dwOutputFormat = dwOutputFormat | CHANNELS_SH(num_des_lcms);
        /* set the remaining parameters, alpha, planar, num_bytes */
        dwInputFormat = dwInputFormat | EXTRA_SH(hasalpha);
        dwOutputFormat = dwOutputFormat | EXTRA_SH(hasalpha);
        dwInputFormat = dwInputFormat | PLANAR_SH(planarIN);
        dwOutputFormat = dwOutputFormat | PLANAR_SH(planarOUT);
        dwInputFormat = dwInputFormat | ENDIAN16_SH(swap_endianIN);
        dwOutputFormat = dwOutputFormat | ENDIAN16_SH(swap_endianOUT);
        dwInputFormat = dwInputFormat | BYTES_SH(numbytesIN);
        dwOutputFormat = dwOutputFormat | BYTES_SH(numbytesOUT);

        hTransform = cmsCloneTransformChangingFormats(ctx, hTransform, dwInputFormat, dwOutputFormat);
        if (hTransform == NULL)
            return_error(gs_error_unknownerror);
        /* Now we have a new hTransform to add to the list, BUT some other thread */
        /* may have been working in the same one. Lock, check again and add to    */
        /* the (potentially new) end of the list */
        gx_monitor_enter(icclink->lock);
        while (link_handle->next != NULL) {
            if (link_handle->flags == needed_flags) {
                /* OOPS. Someone else did it while we were building it */
                cmsDeleteTransform(ctx, hTransform);
                hTransform = link_handle->hTransform;
                new_link_handle = NULL;
                break;
            }
            link_handle = link_handle->next;
        }
        gx_monitor_leave(icclink->lock);
        if (new_link_handle != NULL) {
            new_link_handle->hTransform = hTransform;
            link_handle->next = new_link_handle;		/* link to end of list */
        }
    }

    inputpos = (byte *) inputbuffer;
    outputpos = (byte *) outputbuffer;
    cmsDoTransformLineStride(ctx, hTransform,
        inputpos, outputpos, input_buff_desc->pixels_per_row,
        input_buff_desc->num_rows, input_buff_desc->row_stride,
        output_buff_desc->row_stride, input_buff_desc->plane_stride,
        output_buff_desc->plane_stride);

#if DUMP_CMS_BUFFER
    fid_in = gp_fopen(icclink->memory,"CM_Input.raw","ab");
    fid_out = gp_fopen(icclink->memory,"CM_Output.raw","ab");
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
    gsicc_lcms2mt_link_list_t *link_handle = (gsicc_lcms2mt_link_list_t *)(icclink->link_handle);
    cmsHTRANSFORM hTransform = (cmsHTRANSFORM)link_handle->hTransform;
    cmsUInt32Number dwInputFormat, dwOutputFormat;
    cmsContext ctx = gs_lib_ctx_get_cms_context(icclink->memory);
    int big_endianIN, big_endianOUT, needed_flags;

    /* For a single color, we are going to use the link as it is
       with the exception of taking care of the word size. */
    if (num_bytes > 2)
        return_error(gs_error_rangecheck);	/* TODO: we don't support float */

    dwInputFormat = cmsGetTransformInputFormat(ctx, hTransform);
    big_endianIN = T_ENDIAN16(dwInputFormat);
    dwOutputFormat = cmsGetTransformOutputFormat(ctx, hTransform);
    big_endianOUT = T_ENDIAN16(dwOutputFormat);

    needed_flags = gsicc_link_flags(0, 0, 0,	/* alpha and planar not used for single color */
                                    big_endianIN, big_endianOUT,
                                    num_bytes, num_bytes);
    while (link_handle->flags != needed_flags) {
        if (link_handle->next == NULL) {
            hTransform = NULL;
            break;
        } else {
            link_handle = link_handle->next;
            hTransform = link_handle->hTransform;
        }
    }
    if (hTransform == NULL) {
        gsicc_lcms2mt_link_list_t *new_link_handle =
            (gsicc_lcms2mt_link_list_t *)gs_alloc_bytes(icclink->memory->non_gc_memory,
                                                         sizeof(gsicc_lcms2mt_link_list_t),
                                                         "gscms_transform_color_buffer");
        if (new_link_handle == NULL) {
            return_error(gs_error_VMerror);
        }
        new_link_handle->next = NULL;		/* new end of list */
        new_link_handle->flags = needed_flags;
        hTransform = link_handle->hTransform;

        /* the variant we want wasn't present, clone it from the HEAD (no alpha, not planar) */
        dwInputFormat = COLORSPACE_SH(T_COLORSPACE(cmsGetTransformInputFormat(ctx, hTransform)));
        dwOutputFormat = COLORSPACE_SH(T_COLORSPACE(cmsGetTransformOutputFormat(ctx, hTransform)));
        dwInputFormat = dwInputFormat | CHANNELS_SH(T_CHANNELS(cmsGetTransformInputFormat(ctx, hTransform)));
        dwOutputFormat = dwOutputFormat | CHANNELS_SH(T_CHANNELS(cmsGetTransformOutputFormat(ctx, hTransform)));
        dwInputFormat = dwInputFormat | ENDIAN16_SH(big_endianIN);
        dwOutputFormat = dwOutputFormat | ENDIAN16_SH(big_endianOUT);
        dwInputFormat = dwInputFormat | BYTES_SH(num_bytes);
        dwOutputFormat = dwOutputFormat | BYTES_SH(num_bytes);

        /* Get the transform with the settings we need */
        hTransform = cmsCloneTransformChangingFormats(ctx, hTransform, dwInputFormat, dwOutputFormat);

        if (hTransform == NULL)
            return_error(gs_error_unknownerror);

        /* Now we have a new hTransform to add to the list, BUT some other thread */
        /* may have been working in the same one. Lock, check again and add to    */
        /* the (potentially new) end of the list */
        gx_monitor_enter(icclink->lock);
        while (link_handle->next != NULL) {
            if (link_handle->flags == needed_flags) {
                /* OOPS. Someone else did it while we were building it */
                cmsDeleteTransform(ctx, hTransform);
                hTransform = link_handle->hTransform;
                new_link_handle = NULL;
                break;
            }
            link_handle = link_handle->next;
        }
        gx_monitor_leave(icclink->lock);
        if (new_link_handle != NULL) {
            new_link_handle->hTransform = hTransform;
            link_handle->next = new_link_handle;		/* link to end of list */
        }
    }

    /* Do conversion */
    cmsDoTransform(ctx, hTransform, inputcolor, outputcolor, 1);

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
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);
    gsicc_lcms2mt_link_list_t *link_handle = (gsicc_lcms2mt_link_list_t *)(link);
    cmsHTRANSFORM hTransform = (cmsHTRANSFORM)link_handle->hTransform;

    *num_inputs = T_CHANNELS(cmsGetTransformInputFormat(ctx, hTransform));
    *num_outputs = T_CHANNELS(cmsGetTransformOutputFormat(ctx, hTransform));
}

/* Get the link from the CMS. TODO:  Add error checking */
gcmmhlink_t
gscms_get_link(gcmmhprofile_t  lcms_srchandle, gcmmhprofile_t lcms_deshandle,
               gsicc_rendering_param_t *rendering_params, int cmm_flags,
               gs_memory_t *memory)
{
    cmsUInt32Number src_data_type,des_data_type;
    cmsColorSpaceSignature src_color_space,des_color_space;
    int src_nChannels,des_nChannels;
    int lcms_src_color_space, lcms_des_color_space;
    unsigned int flag;
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);
    gsicc_lcms2mt_link_list_t *link_handle;

    /* Check for case of request for a transfrom from a device link profile
       in that case, the destination profile is NULL */

    /* First handle all the source stuff */
    src_color_space  = cmsGetColorSpace(ctx, lcms_srchandle);
    lcms_src_color_space = _cmsLCMScolorSpace(ctx, src_color_space);

    /* littlecms returns -1 for types it does not (but should) understand */
    if (lcms_src_color_space < 0)
        lcms_src_color_space = 0;
    src_nChannels = cmsChannelsOf(ctx, src_color_space);

    /* For now, just do single byte data, interleaved.  We can change this
      when we use the transformation. */
    src_data_type = (COLORSPACE_SH(lcms_src_color_space)|
                        CHANNELS_SH(src_nChannels)|BYTES_SH(2));

    if (lcms_deshandle != NULL) {
        des_color_space  = cmsGetColorSpace(ctx, lcms_deshandle);
    } else {
        /* We must have a device link profile. Use it's PCS space. */
        des_color_space = cmsGetPCS(ctx, lcms_srchandle);
    }
    lcms_des_color_space = _cmsLCMScolorSpace(ctx, des_color_space);
    if (lcms_des_color_space < 0)
        lcms_des_color_space = 0;
    des_nChannels = cmsChannelsOf(ctx, des_color_space);
    des_data_type = (COLORSPACE_SH(lcms_des_color_space)|
                        CHANNELS_SH(des_nChannels)|BYTES_SH(2));

    /* Set up the flags */
    flag = gscms_get_accuracy(memory);
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
    link_handle = (gsicc_lcms2mt_link_list_t *)gs_alloc_bytes(memory->non_gc_memory,
                                                         sizeof(gsicc_lcms2mt_link_list_t),
                                                         "gscms_transform_color_buffer");
    if (link_handle == NULL)
        return NULL;
    link_handle->hTransform = cmsCreateTransform(ctx, lcms_srchandle, src_data_type,
                                                    lcms_deshandle, des_data_type,
                                                    rendering_params->rendering_intent,
                                                    flag | cmm_flags);
    if (link_handle->hTransform == NULL) {
        int k;

        /* Add a bit of robustness here. Some profiles are ill-formed and
           do not have all the intents.  If we failed due to a missing
           intent, lets go ahead and try each and see if we can get something
           that works. */
        for (k = 0; k <= gsABSOLUTECOLORIMETRIC; k++) {
            link_handle->hTransform = cmsCreateTransform(ctx, lcms_srchandle, src_data_type,
                lcms_deshandle, des_data_type, k, flag | cmm_flags);
            if (link_handle->hTransform != NULL)
                break;
        }

        if (link_handle->hTransform == NULL) {
            gs_free_object(memory, link_handle, "gscms_get_link");
            return NULL;
        }
    }

    link_handle->next = NULL;
    link_handle->flags = gsicc_link_flags(0, 0, 0, 0, 0,    /* no alpha, not planar, no endian swap */
                                          sizeof(gx_color_value), sizeof(gx_color_value));
    return link_handle;
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
    gsicc_lcms2mt_link_list_t *link_handle;

    link_handle = (gsicc_lcms2mt_link_list_t *)gs_alloc_bytes(memory->non_gc_memory,
                                                         sizeof(gsicc_lcms2mt_link_list_t),
                                                         "gscms_transform_color_buffer");
    if (link_handle == NULL)
         return NULL;
    link_handle->next = NULL;
    link_handle->flags = gsicc_link_flags(0, 0, 0, 0, 0,    /* no alpha, not planar, no endian swap */
                                          sizeof(gx_color_value), sizeof(gx_color_value));
    /* Check if the rendering intent is something other than relative colorimetric
       and if we have a proofing profile.  In this case we need to create the
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

        link_handle = gscms_get_link(lcms_srchandle, lcms_proofhandle,
            rendering_params, cmm_flags, memory);
        if (link_handle->hTransform == NULL) {
            gs_free_object(memory, link_handle, "gscms_get_link_proof_devlink");
            return NULL;
        }

        /* Now mash that to a device link profile */
        flag = gscms_get_accuracy(memory);
        if (rendering_params->black_point_comp == gsBLACKPTCOMP_ON ||
            rendering_params->black_point_comp == gsBLACKPTCOMP_ON_OR) {
            flag = (flag | cmsFLAGS_BLACKPOINTCOMPENSATION);
        }
        src_to_proof = cmsTransform2DeviceLink(ctx, link_handle->hTransform, 3.4, flag);
        cmsDeleteTransform(ctx, link_handle->hTransform);

        src_color_space  = cmsGetColorSpace(ctx, src_to_proof);
        lcms_src_color_space = _cmsLCMScolorSpace(ctx, src_color_space);

        /* littlecms returns -1 for types it does not (but should) understand */
        if (lcms_src_color_space < 0)
            lcms_src_color_space = 0;
        src_nChannels = cmsChannelsOf(ctx, src_color_space);

        /* For now, just do single byte data, interleaved.  We can change this
          when we use the transformation. */
        src_data_type = (COLORSPACE_SH(lcms_src_color_space)|
                            CHANNELS_SH(src_nChannels)|BYTES_SH(2));
        if (lcms_devlinkhandle == NULL) {
            des_color_space = cmsGetColorSpace(ctx, lcms_deshandle);
        } else {
            des_color_space = cmsGetPCS(ctx, lcms_devlinkhandle);
        }
        lcms_des_color_space = _cmsLCMScolorSpace(ctx, des_color_space);
        if (lcms_des_color_space < 0) lcms_des_color_space = 0;
        des_nChannels = cmsChannelsOf(ctx, des_color_space);
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
        flag = gscms_get_accuracy(memory);
        if (rendering_params->black_point_comp == gsBLACKPTCOMP_ON
            || rendering_params->black_point_comp == gsBLACKPTCOMP_ON_OR) {
            flag = (flag | cmsFLAGS_BLACKPOINTCOMPENSATION);
        }

        /* Use relative colorimetric here */
        link_handle->hTransform = cmsCreateMultiprofileTransform(ctx,
                    hProfiles, nProfiles, src_data_type, des_data_type,
                    gsRELATIVECOLORIMETRIC, flag);
        cmsCloseProfile(ctx, src_to_proof);
    } else {
       /* First handle all the source stuff */
        src_color_space  = cmsGetColorSpace(ctx, lcms_srchandle);
        lcms_src_color_space = _cmsLCMScolorSpace(ctx, src_color_space);

        /* littlecms returns -1 for types it does not (but should) understand */
        if (lcms_src_color_space < 0)
            lcms_src_color_space = 0;
        src_nChannels = cmsChannelsOf(ctx, src_color_space);

        /* For now, just do single byte data, interleaved.  We can change this
          when we use the transformation. */
        src_data_type = (COLORSPACE_SH(lcms_src_color_space)|
                            CHANNELS_SH(src_nChannels)|BYTES_SH(2));
        if (lcms_devlinkhandle == NULL) {
            if (src_dev_link) {
                des_color_space = cmsGetPCS(ctx, lcms_srchandle);
            } else {
                des_color_space = cmsGetColorSpace(ctx, lcms_deshandle);
            }
        } else {
            des_color_space = cmsGetPCS(ctx, lcms_devlinkhandle);
        }
        lcms_des_color_space = _cmsLCMScolorSpace(ctx, des_color_space);
        if (lcms_des_color_space < 0) lcms_des_color_space = 0;
        des_nChannels = cmsChannelsOf(ctx, des_color_space);
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
        flag = gscms_get_accuracy(memory);
        if (rendering_params->black_point_comp == gsBLACKPTCOMP_ON
            || rendering_params->black_point_comp == gsBLACKPTCOMP_ON_OR) {
            flag = (flag | cmsFLAGS_BLACKPOINTCOMPENSATION);
        }
        link_handle->hTransform =  cmsCreateMultiprofileTransform(ctx,
                                       hProfiles, nProfiles, src_data_type,
                                       des_data_type, rendering_params->rendering_intent, flag);
    }
    if (link_handle->hTransform == NULL) {
        gs_free_object(memory, link_handle, "gscms_get_link_proof_devlink");
        return NULL;
    }
    return link_handle;
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
    cmsPlugin(ctx, (void *)&gs_cms_mutexhandler);
#endif

#ifdef WITH_CAL
    cmsPlugin(ctx, cal_cms_extensions2());
#endif

    cmsSetLogErrorHandler(ctx, gscms_error);

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
    cmsContext ctx = gs_lib_ctx_get_cms_context(icclink->memory);
    gsicc_lcms2mt_link_list_t *link_handle = (gsicc_lcms2mt_link_list_t *)(icclink->link_handle);

    while (link_handle != NULL) {
        gsicc_lcms2mt_link_list_t *next_handle;
        cmsDeleteTransform(ctx, link_handle->hTransform);
        next_handle = link_handle->next;
        gs_free_object(icclink->memory->non_gc_memory, link_handle, "gscms_release_link");
        link_handle = next_handle;
    }
    icclink->link_handle = NULL;
}

/* Have the CMS release the profile handle */
void
gscms_release_profile(void *profile, gs_memory_t *memory)
{
    cmsHPROFILE profile_handle;
    cmsContext ctx = gs_lib_ctx_get_cms_context(memory);

    profile_handle = (cmsHPROFILE) profile;
    cmsCloseProfile(ctx, profile_handle);
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
                            const char* ColorName, gx_color_value device_values[])
{
    gsicc_lcms2mt_link_list_t *link_handle = (gsicc_lcms2mt_link_list_t *)(icclink->link_handle);
    cmsHTRANSFORM hTransform = (cmsHTRANSFORM)link_handle->hTransform;
    unsigned short *deviceptr = device_values;
    int index;
    cmsContext ctx = gs_lib_ctx_get_cms_context(icclink->memory);

    /* Check if name is present in the profile */
    /* If it is not return -1.  Caller will need to use an alternate method */
    if((index = cmsNamedColorIndex(ctx, hTransform, ColorName)) < 0)
        return -1;

    /* Get the device value. */
/* FIXME: This looks WRONG. Doesn't it need to use tint_value??? */
/*        Also, the hTransform from link_handle is 2-byte IN, *NOT* float */
   cmsDoTransform(ctx, hTransform,&index,deviceptr,1);
   return 0;
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
    cmsHTRANSFORM hTransform, hTransformNew;
    cmsUInt32Number dwOutputFormat;
    cmsUInt32Number lcms_proof_flag;
    int number_colors;
    cmsContext ctx = gs_lib_ctx_get_cms_context(icclink->memory);
    gsicc_lcms2mt_link_list_t *link_handle;

    icclink->link_handle = NULL;		/* in case of failure */
    /* NOTE:  We need to add a test here to check that we even HAVE
    device values in here and NOT just CIELAB values */
    if ( lcms_proofhandle != NULL ){
        lcms_proof_flag = cmsFLAGS_GAMUTCHECK | cmsFLAGS_SOFTPROOFING;
    } else {
        lcms_proof_flag = 0;
    }

    /* Create the transform */
    /* ToDo:  Adjust rendering intent */
    hTransform = cmsCreateProofingTransform(ctx,
                                            lcms_srchandle, TYPE_NAMED_COLOR_INDEX,
                                            lcms_deshandle, TYPE_CMYK_8,
                                            lcms_proofhandle,INTENT_PERCEPTUAL,
                                            INTENT_ABSOLUTE_COLORIMETRIC,
                                            lcms_proof_flag);
    if (hTransform == NULL)
        return;					/* bail */

    /* In littleCMS there is no easy way to find out the size of the device
        space returned by the named color profile until after the transform is made.
        Hence we adjust our output format after creating the transform.  It is
        set to CMYK8 initially. */
    number_colors = cmsNamedColorCount(ctx, cmsGetNamedColorList(hTransform));

    /* NOTE: Output size of gx_color_value with no color space type check */
    dwOutputFormat =  (CHANNELS_SH(number_colors)|BYTES_SH(sizeof(gx_color_value)));

    /* Change the formatters */
    hTransformNew  = cmsCloneTransformChangingFormats(ctx, hTransform,TYPE_NAMED_COLOR_INDEX,dwOutputFormat);
    cmsDeleteTransform(ctx, hTransform);	/* release the original after cloning */
    if (hTransformNew == NULL)
        return;					/* bail */
    link_handle = (gsicc_lcms2mt_link_list_t *)gs_alloc_bytes(icclink->memory->non_gc_memory,
                                                         sizeof(gsicc_lcms2mt_link_list_t),
                                                         "gscms_transform_color_buffer");
    if (link_handle == NULL) {
        cmsDeleteTransform(ctx, hTransformNew);
        return;					/* bail */
    }
    link_handle->flags = gsicc_link_flags(0, 0, 0, 0, 0,    /* no alpha, not planar, no endian swap */
                                          sizeof(gx_color_value), sizeof(gx_color_value));
    link_handle->hTransform = hTransformNew;
    link_handle->next = NULL;
    icclink->link_handle = link_handle;

    cmsCloseProfile(ctx, lcms_srchandle);
    if(lcms_deshandle)
        cmsCloseProfile(ctx, lcms_deshandle);
    if(lcms_proofhandle)
        cmsCloseProfile(ctx, lcms_proofhandle);
    return;
}

bool
gscms_is_threadsafe(void)
{
    return true;              /* threads work correctly */
}
