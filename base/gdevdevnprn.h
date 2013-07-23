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

/* Include file for common DeviceN printer devices. */

#ifndef gdevdevnprn_INCLUDED
# define gdevdevnprn_INCLUDED

/*
 * DevN based printer devices all start with the same device fields.
 */
#define gx_devn_prn_device_common\
    gx_device_common;\
    gx_prn_device_common;\
    gs_devn_params devn_params;\
    equivalent_cmyk_color_params equiv_cmyk_colors

/*
 * A structure definition for a DeviceN printer type device
 */
typedef struct gx_devn_prn_device_s {
    gx_devn_prn_device_common;
} gx_devn_prn_device;

/*
 * Garbage collection structure descriptor and finalizer.
 */
extern_st(st_gx_devn_prn_device);
struct_proc_finalize(gx_devn_prn_device_finalize);

/*
 * Basic device procedures for instances of the above device.
 */
dev_proc_get_params(gx_devn_prn_get_params);
dev_proc_put_params(gx_devn_prn_put_params);
dev_proc_get_color_comp_index(gx_devn_prn_get_color_comp_index);
dev_proc_get_color_mapping_procs(gx_devn_prn_get_color_mapping_procs);
dev_proc_encode_color(gx_devn_prn_encode_color);
dev_proc_decode_color(gx_devn_prn_decode_color);
dev_proc_update_spot_equivalent_colors(gx_devn_prn_update_spot_equivalent_colors);
dev_proc_ret_devn_params(gx_devn_prn_ret_devn_params);

/*
 * The PSD device provides psd output functions for gx_devn_prn_devices.
 */
typedef struct {
    FILE *f;

    int width;
    int height;
    int base_bytes_pp;	/* almost always 3 (rgb) or 4 (CMYK) */
    int n_extra_channels;
    int num_channels;	/* base_bytes_pp + any spot colors that are imaged */
    /* Map output channel number to original separation number. */
    int chnl_to_orig_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    /* Map output channel number to gx_color_index position. */
    int chnl_to_position[GX_DEVICE_COLOR_MAX_COMPONENTS];

    /* byte offset of image data */
    int image_data_off;
} psd_write_ctx;

int psd_setup(psd_write_ctx *xc, gx_devn_prn_device *dev, FILE *file, int w, int h);
int psd_write_header(psd_write_ctx *xc, gx_devn_prn_device *pdev);

int psd_write(psd_write_ctx *xc, const byte *buf, int size);
int psd_write_8(psd_write_ctx *xc, byte v);
int psd_write_16(psd_write_ctx *xc, bits16 v);
int psd_write_32(psd_write_ctx *xc, bits32 v);

#endif		/* ifndef gdevdevnprn_INCLUDED */
