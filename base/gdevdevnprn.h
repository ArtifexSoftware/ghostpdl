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

/* Include file for common DeviceN printer devices. */

#ifndef gdevdevnprn_INCLUDED
# define gdevdevnprn_INCLUDED

#include "gdevprn.h"
#include "gdevdevn.h"

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
dev_proc_ret_devn_params_const(gx_devn_prn_ret_devn_params_const);


#endif		/* ifndef gdevdevnprn_INCLUDED */
