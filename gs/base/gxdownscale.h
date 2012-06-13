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


#ifndef gxdownscale_INCLUDED
#define gxdownscale_INCLUDED

#include "ctype_.h"
#include "gsmemory.h"
#include "gstypes.h"
#include "gxdevcli.h"
#include "gxgetbit.h"

/* The following structure definitions should really be considered
 * private, and are exposed here only because it enables us to define
 * gx_downscaler_t's on the stack, thus avoiding mallocs.
 */

typedef struct gx_downscaler_s gx_downscaler_t;

typedef void (gx_downscale_core)(gx_downscaler_t *ds,
                                 byte            *out_buffer,
                                 byte            *in_buffer,
                                 int              row,
                                 int              plane,
                                 int              span);

struct gx_downscaler_s {
    gx_device            *dev;       /* Device */
    int                   width;     /* Width (pixels) */
    int                   awidth;    /* Adjusted width (pixels) */
    int                   span;      /* Num bytes in downscale buffer scanline */
    int                   factor;    /* Factor to downscale */
    byte                 *mfs_data;  /* MinFeatureSize data */
    int                   src_bpc;   /* Source bpc */
    int                  *errors;    /* Error diffusion table */
    byte                 *data;      /* Downscaling buffer */
    gx_downscale_core    *down_core; /* Core downscaling function */
    gs_get_bits_params_t  params;    /* Params if in planar mode */
    int                   num_planes;/* Number of planes if planar, 0 otherwise */
};

/* To use the downscaler:
 *
 *  + define a gx_downscaler_t on the stack.
 *  + initialise it with gx_downscaler_init or gx_downscaler_init_planar
 *  + repeatedly call gx_downscaler_get_lines (or
 *    gx_downscaler_copy_scan_lines) (for chunky mode) or
 *    gx_downscaler_get_bits_rectangle (for planar mode)
 *  + finalise with gx_downscaler_fin
 */
 
/* For chunky mode, currently only:
 *   src_bpc ==  8 && dst_bpc ==  1 && num_comps == 1
 *   src_bpc ==  8 && dst_bpc ==  8 && num_comps == 1
 *   src_bpc ==  8 && dst_bpc ==  8 && num_comps == 3
 *   src_bpc == 16 && dst_bpc == 16 && num_comps == 1
 * are supported. mfs is ignored for all except the first of these.
 * For planar mode, currently only:
 *   src_bpp ==  8 && dst_bpp ==  1
 *   src_bpp ==  8 && dst_bpp ==  8
 *   src_bpp == 16 && dst_bpp == 16
 * are supported.
 */
int gx_downscaler_init(gx_downscaler_t   *ds,
                       gx_device         *dev,
                       int                src_bpc,
                       int                dst_bpc,
                       int                num_comps,
                       int                factor,
                       int                mfs,
                       int              (*adjust_width_proc)(int, int),
                       int                adjust_width);

int gx_downscaler_init_planar(gx_downscaler_t      *ds,
                              gx_device            *dev,
                              gs_get_bits_params_t *params,
                              int                   num_comps,
                              int                   factor,
                              int                   mfs,
                              int                   src_bpc,
                              int                   dst_bpc);

int gx_downscaler_getbits(gx_downscaler_t *ds,
                          byte            *out_data,
                          int              row);

int gx_downscaler_get_bits_rectangle(gx_downscaler_t      *ds,
                                     gs_get_bits_params_t *params,
                                     int                   row);

/* Must only fin a device that has been inited (though you can safely
 * fin several times) */
void gx_downscaler_fin(gx_downscaler_t *ds);

/* A deliberate analogue to gdev_prn_copy_scan_lines, which despite being
 * deprecated is still massively popular. */
int
gx_downscaler_copy_scan_lines(gx_downscaler_t *ds, int y,
                              byte *str, uint size);

#endif
