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

/* gsicc handling for monitoring colors.  Used for detecting gray only pages */


#include <stdlib.h> /* abs() */

#include "std.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxdevcli.h"
#include "gxcspace.h"
#include "gsicc_cms.h"
#include "gsicc_cache.h"
#include "gxcvalue.h"
#include "gxdevsop.h"
#include "gdevp14.h"
#include "string_.h"

static int gsicc_mcm_transform_general(gx_device *dev, gsicc_link_t *icclink,
                                       void *inputcolor, void *outputcolor,
                                       int num_bytes_in, int num_bytes_out);

/* Functions that should be optimized later to do planar/chunky with
   color conversions.  Just putting in something that should work
   right now */
static int
gsicc_mcm_planar_to_planar(gx_device *dev, gsicc_link_t *icclink,
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
    int code;

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
        code = gsicc_mcm_transform_general(dev, icclink,
                                           (void*) &(in_color[0]),
                                           (void*) &(out_color[0]), 1, 1);
        if (code < 0)
            return code;
        for (j = 0; j < output_buff_desc->num_chan; j++) {
            *(outputpos[j]) = out_color[j];
            outputpos[j] += output_buff_desc->bytes_per_chan;
        }
    }
    return 0;
}

/* This is not really used yet */
static int
gsicc_mcm_planar_to_chunky(gx_device *dev, gsicc_link_t *icclink,
                                  gsicc_bufferdesc_t *input_buff_desc,
                                  gsicc_bufferdesc_t *output_buff_desc,
                                  void *inputbuffer, void *outputbuffer)
{
    return 0;
}

/* This is used with the fast thresholding code when doing -dUseFastColor
   and going out to a planar device */
static int
gsicc_mcm_chunky_to_planar(gx_device *dev, gsicc_link_t *icclink,
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
    int code;

    /* Do row by row. */
    for (k = 0; k < input_buff_desc->num_rows ; k++) {
        inputcolor = inputpos;
        output_loc = outputpos;

        /* split the 2 byte 1 byte case here to avoid decision in inner loop */
        if (output_buff_desc->bytes_per_chan == 1) {
            for (j = 0; j < input_buff_desc->pixels_per_row; j++) {
                code = gsicc_mcm_transform_general(dev, icclink,
                                                   (void*) inputcolor,
                                                   (void*) &(outputcolor[0]),
                                                   num_bytes_in,
                                                   num_bytes_out);
                if (code < 0)
                    return code;
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
                code = gsicc_mcm_transform_general(dev, icclink,
                                                   (void*) inputcolor,
                                                   (void*) &(outputcolor[0]),
                                                   num_bytes_in,
                                                   num_bytes_out);
                if (code < 0)
                    return code;
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
    return 0;
}

static int
gsicc_mcm_chunky_to_chunky(gx_device *dev, gsicc_link_t *icclink,
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
    int code;

    /* Do row by row. */
    for (k = 0; k < input_buff_desc->num_rows ; k++) {
        inputcolor = inputpos;
        outputcolor = outputpos;
        for (j = 0; j < input_buff_desc->pixels_per_row; j++) {
            code = gsicc_mcm_transform_general(dev, icclink,
                                               (void*) inputcolor,
                                               (void*) outputcolor,
                                               num_bytes_in,
                                               num_bytes_out);
            if (code < 0)
                return code;
            inputcolor += pixel_in_step;
            outputcolor += pixel_out_step;
        }
        inputpos += input_buff_desc->row_stride;
        outputpos += output_buff_desc->row_stride;
    }
    return 0;
}

/* Transform an entire buffer monitoring and transforming the colors */
static int
gsicc_mcm_transform_color_buffer(gx_device *dev, gsicc_link_t *icclink,
                                  gsicc_bufferdesc_t *input_buff_desc,
                                  gsicc_bufferdesc_t *output_buff_desc,
                                  void *inputbuffer, void *outputbuffer)
{
    if (input_buff_desc->is_planar) {
        if (output_buff_desc->is_planar) {
            return gsicc_mcm_planar_to_planar(dev, icclink, input_buff_desc,
                                              output_buff_desc, inputbuffer,
                                              outputbuffer);
        } else {
            return gsicc_mcm_planar_to_chunky(dev, icclink, input_buff_desc,
                                              output_buff_desc, inputbuffer,
                                              outputbuffer);
        }
    } else {
        if (output_buff_desc->is_planar) {
            return gsicc_mcm_chunky_to_planar(dev, icclink, input_buff_desc,
                                              output_buff_desc, inputbuffer,
                                              outputbuffer);
        } else {
            return gsicc_mcm_chunky_to_chunky(dev, icclink, input_buff_desc,
                                              output_buff_desc, inputbuffer,
                                              outputbuffer);
        }
    }
}

/* This is where we do the monitoring and the conversion if needed */
static int
gsicc_mcm_transform_general(gx_device *dev, gsicc_link_t *icclink,
                             void *inputcolor, void *outputcolor,
                             int num_bytes_in, int num_bytes_out)
{
    bool is_neutral = false;
    unsigned short outputcolor_cm[GX_DEVICE_COLOR_MAX_COMPONENTS];
    void *outputcolor_cm_ptr = (void*) outputcolor_cm;
    cmm_dev_profile_t *dev_profile;
    int code;
    int k;

    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;

    /* Monitor only if gray detection is still true */
    if (dev_profile->pageneutralcolor)
        is_neutral = icclink->procs.is_color(inputcolor, num_bytes_in);

    /* Color found turn off gray detection */
    if (!is_neutral)
        dev_profile->pageneutralcolor = false;

    /* Reset all links so that they will no longer monitor.  This one will finish
       the buffer but we will not have any additional ones */
    if (!dev_profile->pageneutralcolor)
    {
        code = gsicc_mcm_end_monitor(icclink->icc_link_cache, dev);
        if (code < 0)
            return code;
    }

    /* Now apply the color transform using the original color procs, but don't
       do this if we had the identity.  We also have to worry about 8 and 16 bit
       depth */
    if (icclink->hashcode.des_hash == icclink->hashcode.src_hash) {
        if (num_bytes_in == num_bytes_out) {
            /* The easy case */
            memcpy(outputcolor, inputcolor, (size_t)num_bytes_in * icclink->num_input);
        } else {
            if (num_bytes_in == 2) {
                unsigned short *in_ptr = (unsigned short*) inputcolor;
                byte *out_ptr = (byte*) outputcolor;
                for (k = 0; k < icclink->num_input; k++) {
                    out_ptr[k] = gx_color_value_to_byte(in_ptr[k]);
                }
            } else {
                byte *in_ptr = (byte*) inputcolor;
                unsigned short *out_ptr = (unsigned short*) outputcolor;
                for (k = 0; k < icclink->num_input; k++) {
                    out_ptr[k] = gx_color_value_to_byte(in_ptr[k]);
                }
            }
        }
    } else {
        if (num_bytes_in == num_bytes_out) {
            icclink->orig_procs.map_color(dev, icclink, inputcolor, outputcolor,
                                          num_bytes_in);
        } else {
            icclink->orig_procs.map_color(dev, icclink, inputcolor, outputcolor_cm_ptr,
                                          num_bytes_in);
            if (num_bytes_in == 2) {
                unsigned short *in_ptr = (unsigned short*) outputcolor_cm_ptr;
                byte *out_ptr = (byte*) outputcolor;
                for (k = 0; k < icclink->num_input; k++) {
                    out_ptr[k] = gx_color_value_to_byte(in_ptr[k]);
                }
            } else {
                byte *in_ptr = (byte*) outputcolor_cm_ptr;
                unsigned short *out_ptr = (unsigned short*) outputcolor;
                for (k = 0; k < icclink->num_input; k++) {
                    out_ptr[k] = gx_color_value_to_byte(in_ptr[k]);
                }
            }
        }
    }
    return 0;
}

/* Monitor for color */
static int
gsicc_mcm_transform_color(gx_device *dev, gsicc_link_t *icclink, void *inputcolor,
                           void *outputcolor, int num_bytes)
{
    return gsicc_mcm_transform_general(dev, icclink, inputcolor, outputcolor,
                                       num_bytes, num_bytes);
}

bool gsicc_mcm_monitor_rgb(void *inputcolor, int num_bytes)
{
    if (num_bytes == 1) {
        byte *rgb_val = (byte*) inputcolor;
        int rg_diff = (int) abs((int) rgb_val[0] - (int) rgb_val[1]);
        int rb_diff = (int) abs((int) rgb_val[0] - (int) rgb_val[2]);
        int bg_diff = (int) abs((int) rgb_val[1] - (int) rgb_val[2]);
        return (rg_diff < DEV_NEUTRAL_8 && rb_diff < DEV_NEUTRAL_8
                && bg_diff < DEV_NEUTRAL_8);
    } else {
        unsigned short *rgb_val = (unsigned short*) inputcolor;
        int rg_diff = (int) abs((int) rgb_val[0] - (int) rgb_val[1]);
        int rb_diff = (int) abs((int) rgb_val[0] - (int) rgb_val[2]);
        int bg_diff = (int) abs((int) rgb_val[1] - (int) rgb_val[2]);
        return (rg_diff < DEV_NEUTRAL_16 && rb_diff < DEV_NEUTRAL_16
                && bg_diff < DEV_NEUTRAL_16);
    }
}

bool gsicc_mcm_monitor_cmyk(void *inputcolor, int num_bytes)
{
#ifdef PESSIMISTIC_CMYK_NEUTRAL
    if (num_bytes == 1) {
        byte *cmyk = (byte*) inputcolor;
        return (cmyk[0] < DEV_NEUTRAL_8 && cmyk[1] < DEV_NEUTRAL_8
                && cmyk[2] < DEV_NEUTRAL_8);
    } else {
        unsigned short *cmyk = (unsigned short*) inputcolor;
        return (cmyk[0] < DEV_NEUTRAL_16 && cmyk[1] < DEV_NEUTRAL_16
                && cmyk[2] < DEV_NEUTRAL_16);
    }
#else /* The code below is more similar to RGB in that C = M = Y (or close) */
      /* will be treated as neutral.                                        */
    if (num_bytes == 1) {
        byte *cmyk = (byte*) inputcolor;
        int cm_diff = (int) abs((int) cmyk[0] - (int) cmyk[1]);
        int cy_diff = (int) abs((int) cmyk[0] - (int) cmyk[2]);
        int my_diff = (int) abs((int) cmyk[1] - (int) cmyk[2]);
        return (cm_diff < DEV_NEUTRAL_8 && cy_diff < DEV_NEUTRAL_8
                && my_diff < DEV_NEUTRAL_8);
    } else {
        unsigned short *cmyk = (unsigned short*) inputcolor;
        int cm_diff = (int) abs((int) cmyk[0] - (int) cmyk[1]);
        int cy_diff = (int) abs((int) cmyk[0] - (int) cmyk[2]);
        int my_diff = (int) abs((int) cmyk[1] - (int) cmyk[2]);
        return (cm_diff < DEV_NEUTRAL_16 && cy_diff < DEV_NEUTRAL_16
                && my_diff < DEV_NEUTRAL_16);
    }
#endif
}

bool gsicc_mcm_monitor_lab(void *inputcolor, int num_bytes)
{
    if (num_bytes == 1) {
        byte *lab = (byte*) inputcolor;
        int diffa = (int) abs((int) lab[1] - (int) 0x80);
        int diffb = (int) abs((int) lab[2] - (int) 0x80);
        return (diffa < AB_NEUTRAL_8 && diffb < AB_NEUTRAL_8);
    } else {
        unsigned short *lab = (unsigned short*) inputcolor;
        int diffa = (int) abs((int) lab[1] - (int) 0x8000);
        int diffb = (int) abs((int) lab[2] - (int) 0x8000);
        return (diffa < AB_NEUTRAL_16 && diffb < AB_NEUTRAL_16);
    }
}

/* Set the link up to monitor */
void
gsicc_mcm_set_link(gsicc_link_t* link)
{
    link->orig_procs = link->procs;
    link->is_monitored = true;
    link->is_identity = false;

    link->procs.map_buffer = gsicc_mcm_transform_color_buffer;
    link->procs.map_color = gsicc_mcm_transform_color;

    switch (link->data_cs) {
        case gsRGB:
            link->procs.is_color = gsicc_mcm_monitor_rgb;
            break;
        case gsCIELAB:
            link->procs.is_color = gsicc_mcm_monitor_lab;
            break;
        case gsCMYK:
            link->procs.is_color = gsicc_mcm_monitor_cmyk;
            break;
        default:
            break;
    }
}

/* This gets rid of the monitoring */
int
gsicc_mcm_end_monitor(gsicc_link_cache_t *cache, gx_device *dev)
{
    gx_monitor_t *lock = cache->lock;
    gsicc_link_t *curr;
    int code;
    cmm_dev_profile_t *dev_profile;


    /* Get the device profile */
    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;
    dev_profile->pageneutralcolor = false;
    /* If this device is a pdf14 device, then we may need to take care of the
       profile in the target device also.  This is a special case since the
       pdf14 device has its own profile different from the target device */
    if (dev_proc(dev, dev_spec_op)(dev, gxdso_is_pdf14_device, NULL, 0) > 0) {
        gs_pdf14_device_color_mon_set(dev, false);
    }

    /* Lock the cache as we remove monitoring from the links */
    gx_monitor_enter(lock);
    curr = cache->head;
    while (curr != NULL ) {
        if (curr->is_monitored) {
            curr->procs = curr->orig_procs;
            if (curr->hashcode.des_hash == curr->hashcode.src_hash)
                curr->is_identity = true;
            curr->is_monitored = false;
        }
        /* Now release any tasks/threads waiting for these contents */
        gx_monitor_leave(curr->lock);
        curr = curr->next;
    }
    gx_monitor_leave(lock);	/* done with updating, let everyone run */
    return 0;
}

/* Conversely to the above, this gets restores monitoring, needed after
 * monitoring was turned off above (for the next page)
*/
int
gsicc_mcm_begin_monitor(gsicc_link_cache_t *cache, gx_device *dev)
{
    gx_monitor_t *lock = cache->lock;
    gsicc_link_t *curr;
    int code;
    cmm_dev_profile_t *dev_profile;

    /* Get the device profile */
    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;
    dev_profile->pageneutralcolor = true;
    /* If this device is a pdf14 device, then we may need to take care of the
       profile in the target device also.  This is a special case since the
       pdf14 device has its own profile different from the target device */
    if (dev_proc(dev, dev_spec_op)(dev, gxdso_is_pdf14_device, NULL, 0) > 0) {
        gs_pdf14_device_color_mon_set(dev, true);
    }

    /* Lock the cache as we remove monitoring from the links */
    gx_monitor_enter(lock);

    curr = cache->head;
    while (curr != NULL ) {
        if (curr->data_cs != gsGRAY) {
            gsicc_mcm_set_link(curr);
            /* Now release any tasks/threads waiting for these contents */
            gx_monitor_leave(curr->lock);
        }
        curr = curr->next;
    }
    gx_monitor_leave(lock);	/* done with updating, let everyone run */
    return 0;
}
