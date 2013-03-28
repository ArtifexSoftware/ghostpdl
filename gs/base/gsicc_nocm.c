/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* gsicc handling for unmanaged color. */

#include "std.h"
#include "string_.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "scommon.h"
#include "strmio.h"
#include "gx.h"
#include "gxistate.h"
#include "gxcspace.h"
#include "gsicc_cms.h"
#include "gsicc_cache.h"

/* A link structure for our non-cm color transform */
typedef struct nocm_link_s {
    /* device (or default) procs to do the transformation */
    gx_cm_color_map_procs cm_procs;
    /* Since RGB to CMYK requires BG and UCR, we need to have the 
       imager state available */
    gs_imager_state *pis;
    byte num_in;
    byte num_out;
    gs_memory_t *memory;
} nocm_link_t;

static void gsicc_nocm_transform_general(gx_device *dev, gsicc_link_t *icclink, 
                                         void *inputcolor, void *outputcolor, 
                                         int num_bytes_in, int num_bytes_out);

/* Functions that should be optimized later to do planar/chunky with 
   color conversions.  Just putting in something that should work 
   right now */

/* At most, we have 4 input and 4 output ptrs.  Since this is used only in
   DeviceGray, DeviceRGB and DeviceCMYK cases */
static void
gsicc_nocm_planar_to_planar(gx_device *dev, gsicc_link_t *icclink,
                                  gsicc_bufferdesc_t *input_buff_desc,
                                  gsicc_bufferdesc_t *output_buff_desc,
                                  void *inputbuffer, void *outputbuffer)
{
    int k, j;
    byte *inputpos[4];
    byte *outputpos[4];
    byte *in_buffer_ptr = (byte *) inputbuffer;
    byte *out_buffer_ptr = (byte *) outputbuffer;
    byte in_color[4], out_color[4];

    for (k = 0; k < input_buff_desc->num_chan; k++) {
        inputpos[k] = in_buffer_ptr + k * input_buff_desc->plane_stride;
    }
    for (k = 0; k < output_buff_desc->num_chan; k++) {
        outputpos[k] = out_buffer_ptr + k * input_buff_desc->plane_stride;
    }
    /* Note to self.  We currently only do this in the transparency buffer
       case which has byte representation so just stepping through 
       plane_stride is ok at this time.  */
    for (k = 0; k < input_buff_desc->plane_stride ; k++) {
        for (j = 0; j < input_buff_desc->num_chan; j++) {
            in_color[j] = *(inputpos[j]);
            inputpos[j] += input_buff_desc->bytes_per_chan;
        }
        gsicc_nocm_transform_general(dev, icclink, (void*) &(in_color[0]), 
                                         (void*) &(out_color[0]), 1, 1);
        for (j = 0; j < output_buff_desc->num_chan; j++) {
            *(outputpos[j]) = out_color[j];
            outputpos[j] += output_buff_desc->bytes_per_chan;
        }
    }
}

/* This is not really used yet */
static void
gsicc_nocm_planar_to_chunky(gx_device *dev, gsicc_link_t *icclink,
                                  gsicc_bufferdesc_t *input_buff_desc,
                                  gsicc_bufferdesc_t *output_buff_desc,
                                  void *inputbuffer, void *outputbuffer)
{


}

/* This is used with the fast thresholding code when doing -dUseFastColor
   and going out to a planar device */
static void
gsicc_nocm_chunky_to_planar(gx_device *dev, gsicc_link_t *icclink,
                                  gsicc_bufferdesc_t *input_buff_desc,
                                  gsicc_bufferdesc_t *output_buff_desc,
                                  void *inputbuffer, void *outputbuffer)
{
    int k, j, m;
    byte *inputpos = (byte *) inputbuffer;
    byte *outputpos = (byte *) outputbuffer;
    byte *output_loc;
    byte *inputcolor;
    byte outputcolor[8];  /* 8 since we have max 4 colorants and 2 bytes/colorant */
    unsigned short *pos_in_short, *pos_out_short;
    int num_bytes_in = input_buff_desc->bytes_per_chan;
    int num_bytes_out = output_buff_desc->bytes_per_chan;
    int pixel_in_step = num_bytes_in * input_buff_desc->num_chan;
    int plane_stride = output_buff_desc->plane_stride;

    /* Do row by row. */
    for (k = 0; k < input_buff_desc->num_rows ; k++) {
        inputcolor = inputpos;
        output_loc = outputpos;

        /* split the 2 byte 1 byte case here to avoid decision in inner loop */
        if (output_buff_desc->bytes_per_chan == 1) {
            for (j = 0; j < input_buff_desc->pixels_per_row; j++) {
                gsicc_nocm_transform_general(dev, icclink, (void*) inputcolor, 
                                             (void*) &(outputcolor[0]), num_bytes_in, 
                                              num_bytes_out);
                /* Stuff the output in the proper planar location */
                for (m = 0; m < output_buff_desc->num_chan; m++) {
                    *(output_loc + m * plane_stride + j) = outputcolor[m];
                }
                inputcolor += pixel_in_step;
            }
            inputpos += input_buff_desc->row_stride;
            outputpos += output_buff_desc->row_stride;        
        } else {
            for (j = 0; j < input_buff_desc->pixels_per_row; j++) {
                gsicc_nocm_transform_general(dev, icclink, (void*) inputcolor, 
                                             (void*) &(outputcolor[0]), num_bytes_in, 
                                              num_bytes_out);
                /* Stuff the output in the proper planar location */
                pos_in_short = (unsigned short*) &(outputcolor[0]);
                pos_out_short = (unsigned short*) (output_loc);
                for (m = 0; m < output_buff_desc->num_chan; m++) {
                    *(pos_out_short + m * plane_stride + j) = pos_in_short[m];
                }
                inputcolor += pixel_in_step;
            }
            inputpos += input_buff_desc->row_stride;
            outputpos += output_buff_desc->row_stride;
        }
    }
}

static void
gsicc_nocm_chunky_to_chunky(gx_device *dev, gsicc_link_t *icclink,
                                  gsicc_bufferdesc_t *input_buff_desc,
                                  gsicc_bufferdesc_t *output_buff_desc,
                                  void *inputbuffer, void *outputbuffer)
{
    int k, j;
    byte *inputpos = (byte *) inputbuffer;
    byte *outputpos = (byte *) outputbuffer;
    byte *inputcolor, *outputcolor;
    int num_bytes_in = input_buff_desc->bytes_per_chan;
    int num_bytes_out = output_buff_desc->bytes_per_chan;
    int pixel_in_step = num_bytes_in * input_buff_desc->num_chan;
    int pixel_out_step = num_bytes_out * output_buff_desc->num_chan;

    /* Do row by row. */
    for (k = 0; k < input_buff_desc->num_rows ; k++) {
        inputcolor = inputpos;
        outputcolor = outputpos;
        for (j = 0; j < input_buff_desc->pixels_per_row; j++) {
            gsicc_nocm_transform_general(dev, icclink, (void*) inputcolor, 
                                         (void*) outputcolor, num_bytes_in, 
                                          num_bytes_out);
            inputcolor += pixel_in_step;
            outputcolor += pixel_out_step;
        }
        inputpos += input_buff_desc->row_stride;
        outputpos += output_buff_desc->row_stride;
    }
}

/* Transform an entire buffer using the generic (non color managed) 
   transformations */
static void
gsicc_nocm_transform_color_buffer(gx_device *dev, gsicc_link_t *icclink,
                                  gsicc_bufferdesc_t *input_buff_desc,
                                  gsicc_bufferdesc_t *output_buff_desc,
                                  void *inputbuffer, void *outputbuffer)
{
    /* Since we have to do the mappings to and from frac colors we will for
       now just call the gsicc_nocm_transform_color as we step through the
       buffers.  This process can be significantly sped up */
    
    if (input_buff_desc->is_planar) {
        if (output_buff_desc->is_planar) {
            gsicc_nocm_planar_to_planar(dev, icclink, input_buff_desc, 
                                        output_buff_desc, inputbuffer, 
                                        outputbuffer);    
        } else {
            gsicc_nocm_planar_to_chunky(dev, icclink, input_buff_desc, 
                                        output_buff_desc, inputbuffer, 
                                        outputbuffer);    
        }
    } else {
        if (output_buff_desc->is_planar) {
            gsicc_nocm_chunky_to_planar(dev, icclink, input_buff_desc, 
                                        output_buff_desc, inputbuffer, 
                                        outputbuffer);    
        } else {
            gsicc_nocm_chunky_to_chunky(dev, icclink, input_buff_desc, 
                                        output_buff_desc, inputbuffer, 
                                        outputbuffer);    
        }
    }
    return;
}

/* Shared function between the single and buffer conversions */
static void
gsicc_nocm_transform_general(gx_device *dev, gsicc_link_t *icclink, 
                             void *inputcolor, void *outputcolor, 
                             int num_bytes_in, int num_bytes_out)
{
    /* Input data is either single byte or 2 byte color values.  The 
       color mapping procs work on frac values so we have to sandwich 
       the transformation between to and from frac conversions.  We are only
       doing at most 4 source colors here */
    nocm_link_t *link = (nocm_link_t*) icclink->link_handle;
    byte num_in = link->num_in;
    byte num_out = link->num_out;
    frac frac_in[4];
    frac frac_out[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int k;

    if (num_bytes_in == 2) {
        unsigned short *data = (unsigned short *) inputcolor;
        for (k = 0; k < num_in; k++) {
            frac_in[k] = ushort2frac(data[k]);
        }
    } else {
        byte *data = (byte *) inputcolor;
        for (k = 0; k < num_in; k++) {
            frac_in[k] = byte2frac(data[k]);
        }
    }
    /* Use the device procedure */
    switch (num_in) {
        case 1:
            (link->cm_procs.map_gray)(dev, frac_in[0], frac_out);
            break;
        case 3:
            (link->cm_procs.map_rgb)(dev, link->pis, frac_in[0], frac_in[1],
                                 frac_in[2], frac_out);
            break;
        case 4:
            (link->cm_procs.map_cmyk)(dev, frac_in[0], frac_in[1], frac_in[2], 
                                 frac_in[3], frac_out);            
            break;
        default:
            break;
    }   
    if (num_bytes_out == 2) {
        unsigned short *data = (unsigned short *) outputcolor;
        for (k = 0; k < num_out; k++) {
            data[k] = frac2ushort(frac_out[k]);
        }
    } else { 
        byte *data = (byte *) outputcolor;
        for (k = 0; k < num_out; k++) {
            data[k] = frac2byte(frac_out[k]);
        }
    }
    return;
}

/* Transform a single color using the generic (non color managed) 
   transformations */
static void
gsicc_nocm_transform_color(gx_device *dev, gsicc_link_t *icclink, void *inputcolor,
                           void *outputcolor, int num_bytes)
{

    gsicc_nocm_transform_general(dev, icclink, inputcolor, outputcolor, 
                                 num_bytes, num_bytes);
}

static void
gsicc_nocm_freelink(gsicc_link_t *icclink)
{
    nocm_link_t *nocm_link = (nocm_link_t*) icclink->link_handle;

    if (nocm_link->pis != NULL) {
        if (nocm_link->pis->black_generation != NULL) {
            gs_free_object(nocm_link->memory, nocm_link->pis->black_generation, 
                           "gsicc_nocm_freelink");
        }
        if (nocm_link->pis->undercolor_removal != NULL) {
            gs_free_object(nocm_link->memory, nocm_link->pis->undercolor_removal, 
                           "gsicc_nocm_freelink");
        }
        gs_free_object(nocm_link->memory, nocm_link->pis, "gsicc_nocm_freelink");
    }
    gs_free_object(nocm_link->memory, nocm_link, "gsicc_nocm_freelink");
}

/* Since this is the only occurence of this object we are not going to 
   fool aroung with reference counting and closure functions.  When 
   the link is destroyed, we will simply free the bytes */
static gx_transfer_map*
gsicc_nocm_copy_curve(gx_transfer_map *in_map, gs_memory_t *mem)
{
    gx_transfer_map *out_map;

    if (in_map == NULL) {
        return NULL;
    } else {
        out_map = (gx_transfer_map*) gs_alloc_bytes(mem, sizeof(gx_transfer_map), 
                            "gsicc_nocm_copy_curve");
        out_map->proc = in_map->proc;
        memcpy(&(out_map->values[0]), &(in_map->values[0]), 
                sizeof(frac) * transfer_map_size);
        out_map->id = gs_no_id;
        return out_map;
    }
}

/* Get the link, which is the mapping procedure in this non color managed 
   transformation case. */
gsicc_link_t*
gsicc_nocm_get_link(const gs_imager_state *pis, gx_device *dev, 
                    gs_color_space_index src_index)
{
    gsicc_link_t *result;
    gsicc_hashlink_t hash;
    nocm_link_t *nocm_link;
    gs_memory_t *mem = pis->memory->non_gc_memory;
    const gx_cm_color_map_procs * cm_procs;

    /* If the cm_procs are forwarding due to the overprint device or other
       odd thing, drill down now and get the proper ones */
    if (fwd_uses_fwd_cmap_procs(dev)) {
        cm_procs = fwd_get_target_cmap_procs(dev);
    } else {
        cm_procs = dev_proc(dev, get_color_mapping_procs)(dev);
    }
    /* We will add this to the link cache so that we can avoid the issue
       of black_generation and undercolor removal being GC values.
       Since the link is not GC we would need to copy the contents over
       each time a link was requested.  This could be costly if we had 
       a lot of link requests.  */
    hash.rend_hash = gsCMM_NONE;
    hash.des_hash = dev->color_info.num_components;
    hash.src_hash = src_index;
    hash.link_hashcode = src_index + hash.des_hash * 256 + hash.rend_hash * 4096;

    /* Check the cache for a hit. */
    result = gsicc_findcachelink(hash, pis->icc_link_cache, false, false);
    if (result != NULL) {
        return result;
    }
    /* If not, then lets create a new one.  This may actually return a link if 
       another thread has already created it while we were trying to do so */ 
    if (gsicc_alloc_link_entry(pis->icc_link_cache, &result, hash, false, false)) 
        return result;

    if (result == NULL)
        return NULL;

    /* Now compute the link contents */
    result->procs.map_buffer = gsicc_nocm_transform_color_buffer;
    result->procs.map_color = gsicc_nocm_transform_color;
    result->procs.free_link = gsicc_nocm_freelink;
    result->hashcode = hash;
    nocm_link = (nocm_link_t *) gs_alloc_bytes(mem, sizeof(nocm_link_t),
                                               "gsicc_nocm_get_link");
    result->link_handle = (void*) nocm_link;
    nocm_link->memory = mem;
    /* Create a dummy imager state and populate the ucr/bg values.  This
       is the only part that we need */ 
    if (pis == NULL || 
       (pis->black_generation == NULL && pis->undercolor_removal == NULL)) {
        nocm_link->pis = NULL;
    } else {
        nocm_link->pis = (gs_imager_state*) 
                          gs_alloc_bytes(mem, sizeof(gs_imager_state), 
                                         "gsicc_nocm_get_link");
        nocm_link->pis->black_generation = (gx_transfer_map*)
                            gsicc_nocm_copy_curve(pis->black_generation, mem); 
        nocm_link->pis->undercolor_removal = (gx_transfer_map*)
                            gsicc_nocm_copy_curve(pis->undercolor_removal, mem); 
    }
    nocm_link->num_out = min(dev->color_info.num_components, 
                             GS_CLIENT_COLOR_MAX_COMPONENTS);
    nocm_link->cm_procs.map_cmyk = cm_procs->map_cmyk;
    nocm_link->cm_procs.map_rgb = cm_procs->map_rgb;
    nocm_link->cm_procs.map_gray = cm_procs->map_gray;
    nocm_link->num_in = src_index;
    if (result != NULL) {
        gsicc_set_link_data(result, nocm_link, hash,
                            pis->icc_link_cache->lock, false, false);
    }
    return result;
}
