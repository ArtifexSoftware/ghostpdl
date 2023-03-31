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


/* gsicc handling for direct color replacement. */

#include "std.h"
#include "string_.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "scommon.h"
#include "strmio.h"
#include "gx.h"
#include "gxgstate.h"
#include "gxcspace.h"
#include "gsicc_cms.h"
#include "gsicc_cache.h"

/* A link structure for our replace color transform */
typedef struct rcm_link_s {
    byte num_out;
    byte num_in;
    gsicc_colorbuffer_t data_cs_in;
    gs_memory_t *memory;
    gx_cm_color_map_procs cm_procs;  /* for the demo */
    const gx_device *cmdev;
    void *context;  /* For a table and or a set of procs */
} rcm_link_t;

static void gsicc_rcm_transform_general(const gx_device *dev, gsicc_link_t *icclink,
                                         void *inputcolor, void *outputcolor,
                                         int num_bytes_in, int num_bytes_out);

/* Functions that should be optimized later to do planar/chunky with
   color conversions.  Just putting in something that should work
   right now */
static void
gsicc_rcm_planar_to_planar(const gx_device *dev, gsicc_link_t *icclink,
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
        outputpos[k] = out_buffer_ptr + k * output_buff_desc->plane_stride;
    }
    /* Note to self.  We currently only do this in the transparency buffer
       case which has byte representation so just stepping through
       plane_stride is ok at this time.  */
    for (k = 0; k < input_buff_desc->plane_stride ; k++) {
        for (j = 0; j < input_buff_desc->num_chan; j++) {
            in_color[j] = *(inputpos[j]);
            inputpos[j] += input_buff_desc->bytes_per_chan;
        }
        gsicc_rcm_transform_general(dev, icclink, (void*) &(in_color[0]),
                                         (void*) &(out_color[0]), 1, 1);
        for (j = 0; j < output_buff_desc->num_chan; j++) {
            *(outputpos[j]) = out_color[j];
            outputpos[j] += output_buff_desc->bytes_per_chan;
        }
    }
}

/* This is not really used yet */
static void
gsicc_rcm_planar_to_chunky(const gx_device *dev, gsicc_link_t *icclink,
                                  gsicc_bufferdesc_t *input_buff_desc,
                                  gsicc_bufferdesc_t *output_buff_desc,
                                  void *inputbuffer, void *outputbuffer)
{


}

/* This is used with the fast thresholding code when doing -dUseFastColor
   and going out to a planar device */
static void
gsicc_rcm_chunky_to_planar(const gx_device *dev, gsicc_link_t *icclink,
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
                gsicc_rcm_transform_general(dev, icclink, (void*) inputcolor,
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
                gsicc_rcm_transform_general(dev, icclink, (void*) inputcolor,
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
gsicc_rcm_chunky_to_chunky(const gx_device *dev, gsicc_link_t *icclink,
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
            gsicc_rcm_transform_general(dev, icclink, (void*) inputcolor,
                                         (void*) outputcolor, num_bytes_in,
                                          num_bytes_out);
            inputcolor += pixel_in_step;
            outputcolor += pixel_out_step;
        }
        inputpos += input_buff_desc->row_stride;
        outputpos += output_buff_desc->row_stride;
    }
}

/* Transform an entire buffer using replacement method */
static int
gsicc_rcm_transform_color_buffer(gx_device *dev, gsicc_link_t *icclink,
                                  gsicc_bufferdesc_t *input_buff_desc,
                                  gsicc_bufferdesc_t *output_buff_desc,
                                  void *inputbuffer, void *outputbuffer)
{
    /* Since we have to do the mappings to and from frac colors we will for
       now just call the gsicc_rcm_transform_color as we step through the
       buffers.  This process can be significantly sped up */

    if (input_buff_desc->is_planar) {
        if (output_buff_desc->is_planar) {
            gsicc_rcm_planar_to_planar(dev, icclink, input_buff_desc,
                                        output_buff_desc, inputbuffer,
                                        outputbuffer);
        } else {
            gsicc_rcm_planar_to_chunky(dev, icclink, input_buff_desc,
                                        output_buff_desc, inputbuffer,
                                        outputbuffer);
        }
    } else {
        if (output_buff_desc->is_planar) {
            gsicc_rcm_chunky_to_planar(dev, icclink, input_buff_desc,
                                        output_buff_desc, inputbuffer,
                                        outputbuffer);
        } else {
            gsicc_rcm_chunky_to_chunky(dev, icclink, input_buff_desc,
                                        output_buff_desc, inputbuffer,
                                        outputbuffer);
        }
    }
    return 0;
}

/* Shared function between the single and buffer conversions.  This is where
   we do the actual replacement.   For now, we make the replacement a
   negative to show the effect of what using color replacement.  We also use
   the device procs to map to the device value.  */
static void
gsicc_rcm_transform_general(const gx_device *dev, gsicc_link_t *icclink,
                             void *inputcolor, void *outputcolor,
                             int num_bytes_in, int num_bytes_out)
{
    /* Input data is either single byte or 2 byte color values.  */
    rcm_link_t *link = (rcm_link_t*) icclink->link_handle;
    byte num_in = link->num_in;
    byte num_out = link->num_out;
    frac frac_in[4];
    frac frac_out[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int k;

    /* Make the negative for the demo.... */
    if (num_bytes_in == 2) {
        unsigned short *data = (unsigned short *) inputcolor;
        for (k = 0; k < num_in; k++) {
            frac_in[k] = frac_1 - ushort2frac(data[k]);
        }
    } else {
        byte *data = (byte *) inputcolor;
        for (k = 0; k < num_in; k++) {
            frac_in[k] = frac_1 - byte2frac(data[k]);
        }
    }
    /* Use the device procedure */
    switch (num_in) {
        case 1:
            (link->cm_procs.map_gray)(link->cmdev, frac_in[0], frac_out);
            break;
        case 3:
            (link->cm_procs.map_rgb)(link->cmdev, NULL, frac_in[0], frac_in[1],
                                 frac_in[2], frac_out);
            break;
        case 4:
            (link->cm_procs.map_cmyk)(link->cmdev, frac_in[0], frac_in[1], frac_in[2],
                                 frac_in[3], frac_out);
            break;
        default:
            memset(&(frac_out[0]), 0, sizeof(frac_out));
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
static int
gsicc_rcm_transform_color(gx_device *dev, gsicc_link_t *icclink, void *inputcolor,
                           void *outputcolor, int num_bytes)
{
    gsicc_rcm_transform_general(dev, icclink, inputcolor, outputcolor,
                                 num_bytes, num_bytes);
    return 0;
}

static void
gsicc_rcm_freelink(gsicc_link_t *icclink)
{
    rcm_link_t *rcm_link = (rcm_link_t*) icclink->link_handle;
    if (rcm_link != NULL)
        gs_free_object(rcm_link->memory, rcm_link, "gsicc_rcm_freelink");
    icclink->link_handle = NULL;
}

/* Get the replacement color management link.  It basically needs to store
   the number of components for the source so that we know what we are
   coming from (e.g. RGB, CMYK, Gray) */
gsicc_link_t*
gsicc_rcm_get_link(const gs_gstate *pgs, gx_device *dev,
                   gsicc_colorbuffer_t data_cs)
{
    gsicc_link_t *result;
    gsicc_hashlink_t hash;
    rcm_link_t *rcm_link;
    gs_memory_t *mem;
    const gx_cm_color_map_procs * cm_procs;
    const gx_device *cmdev;
    bool pageneutralcolor = false;
    cmm_dev_profile_t *dev_profile;
    int code;

    if (dev == NULL)
        return NULL;

    mem = dev->memory->non_gc_memory;
    /* Need to check if we need to monitor for color */
    code = dev_proc(dev, get_profile)(dev,  &dev_profile);
    if (code < 0)
        return NULL;
    if (dev_profile != NULL) {
        pageneutralcolor = dev_profile->pageneutralcolor;
    }

    cm_procs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);

    hash.rend_hash = gsCMM_REPLACE;
    hash.des_hash = dev->color_info.num_components;
    hash.src_hash = data_cs;
    hash.link_hashcode = data_cs + hash.des_hash * 256 + hash.rend_hash * 4096;

    /* Check the cache for a hit. */
    result = gsicc_findcachelink(hash, pgs->icc_link_cache, false, false);
    if (result != NULL) {
        return result;
    }
    /* If not, then lets create a new one.  This may actually return a link if
       another thread has already created it while we were trying to do so */
    if (gsicc_alloc_link_entry(pgs->icc_link_cache, &result, hash, false, false))
        return result;

    if (result == NULL)
        return result;

    /* Now compute the link contents */
    /* We (this thread) owns this link, so we can update it */
    result->procs.map_buffer = gsicc_rcm_transform_color_buffer;
    result->procs.map_color = gsicc_rcm_transform_color;
    result->procs.free_link = gsicc_rcm_freelink;
    result->hashcode = hash;
    result->is_identity = false;
    rcm_link = (rcm_link_t *) gs_alloc_bytes(mem, sizeof(rcm_link_t),
                                               "gsicc_rcm_get_link");
    if (rcm_link == NULL)
        return NULL;
    result->link_handle = (void*) rcm_link;
    rcm_link->memory = mem;
    rcm_link->num_out = min(dev->color_info.num_components,
                             GS_CLIENT_COLOR_MAX_COMPONENTS);
    rcm_link->data_cs_in = data_cs;
    rcm_link->cm_procs.map_cmyk = cm_procs->map_cmyk;
    rcm_link->cm_procs.map_rgb = cm_procs->map_rgb;
    rcm_link->cm_procs.map_gray = cm_procs->map_gray;
    rcm_link->cmdev = cmdev;

    switch (data_cs) {
        case gsGRAY:
            rcm_link->num_in = 1;
            break;
        case gsRGB:
        case gsCIELAB:
            rcm_link->num_in = 3;
            break;
        case gsCMYK:
            rcm_link->num_in = 4;
            break;
        default:
            result->procs.free_link(result);
            return NULL;
    }
    /* Likely set if we have something like a table or procs */
    rcm_link->context = NULL;

    result->num_input = rcm_link->num_in;
    result->num_output = rcm_link->num_out;
    result->link_handle = rcm_link;
    result->hashcode.link_hashcode = hash.link_hashcode;
    result->hashcode.des_hash = hash.des_hash;
    result->hashcode.src_hash = hash.src_hash;
    result->hashcode.rend_hash = hash.rend_hash;
    result->includes_softproof = false;
    result->includes_devlink = false;
    result->is_identity = false;  /* Always do replacement for demo */

    /* Set up for monitoring non gray color spaces */
    if (pageneutralcolor && data_cs != gsGRAY)
        gsicc_mcm_set_link(result);

    result->valid = true;
    /* Now release any tasks/threads waiting for these contents by unlocking it */
    gx_monitor_leave(result->lock);	/* done with updating, let everyone run */

    return result;
}
