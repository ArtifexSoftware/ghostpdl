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


#ifndef gxdownscale_INCLUDED
#define gxdownscale_INCLUDED

#include "ctype_.h"
#include "gsmemory.h"
#include "gstypes.h"
#include "gxdevcli.h"
#include "gxgetbit.h"

#include "claptrap.h"

/* Function to apply color management to a rectangle of data.
 *
 * dst/src point to arrays of pointers, 1 per component.
 * In the chunky case, only the first pointer is valid.
 * In the planar case, with n planes, the first n pointers
 * are valid.
 * w and h are the width and height of the rectangle of
 * data to be processed.
 * raster is the number of bytes to offset from the start
 * of one line to the next. This value should be ignored
 * in chunky cases.
 */
typedef int (gx_downscale_cm_fn)(void  *arg,
                                 byte **dst,
                                 byte **src,
                                 int    w,
                                 int    h,
                                 int    raster);

typedef struct gx_downscaler_ht_s
{
    int w;
    int h;
    int stride;
    int x_phase;
    int y_phase;
    byte *data;
} gx_downscaler_ht_t;

/* The following structure definitions should really be considered
 * private, and are exposed here only because it enables us to define
 * gx_downscaler_t's on the stack, thus avoiding mallocs.
 */

typedef struct gx_downscaler_s gx_downscaler_t;

/* Private function type for the core downscaler routines */
typedef void (gx_downscale_core)(gx_downscaler_t *ds,
                                 byte            *out_buffer,
                                 byte            *in_buffer,
                                 int              row,
                                 int              plane,
                                 int              span);

typedef struct gx_downscale_liner_s gx_downscale_liner;

struct gx_downscaler_s {
    gx_device            *dev;        /* Device */
    int                   width;      /* Width (pixels) */
    int                   awidth;     /* Adjusted width (pixels) */
    int                   span;       /* Num bytes in unscaled scanline */
    int                   factor;     /* Factor to downscale */
    byte                 *mfs_data;   /* MinFeatureSize data */
    int                   src_bpc;    /* Source bpc */
    int                   dst_bpc;    /* Destination bpc */
    int                  *errors;     /* Error diffusion table */
    byte                 *scaled_data;/* Downscaled data (only used for non
                                       * integer downscales). */
    int                   scaled_span;/* Num bytes in scaled scanline */
    gx_downscale_core    *down_core;  /* Core downscaling function */
    gs_get_bits_params_t  params;     /* Params if in planar mode */
    int                   num_comps;  /* Number of components as rendered */
    int                   num_planes; /* Number of planes if planar, 0 otherwise */

    gx_downscale_liner   *liner;      /* Source for line data */

    int                   early_cm;
    gx_downscale_cm_fn   *apply_cm;
    void                 *apply_cm_arg;
    int                   post_cm_num_comps;

    void                 *ets_config;
    gx_downscale_core    *ets_downscale;

    byte                 *pre_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    byte                 *post_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];

    gx_downscaler_ht_t   *ht;
    byte                 *htrow;
    byte                 *htrow_alloc;
    byte                 *inbuf;
    byte                 *inbuf_alloc;

    int                   do_skew_detection;
    int                   skew_detected;
    double                skew_angle;
};

/* The following structure is used to hold the configuration
 * parameters for the downscaler.
 */
typedef struct gx_downscaler_params_s
{
    int downscale_factor;
    int min_feature_size;
    int trap_w;
    int trap_h;
    int trap_order[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int ets;
    int do_skew_detection;
} gx_downscaler_params;

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
int gx_downscaler_init(gx_downscaler_t      *ds,
                       gx_device            *dev,
                       int                   src_bpc,
                       int                   dst_bpc,
                       int                   num_comps,
                 const gx_downscaler_params *params,
                       int                 (*adjust_width_proc)(int, int),
                       int                   adjust_width);

int gx_downscaler_init_cm(gx_downscaler_t      *ds,
                          gx_device            *dev,
                          int                   src_bpc,
                          int                   dst_bpc,
                          int                   num_comps,
                    const gx_downscaler_params *params,
                          int                 (*adjust_width_proc)(int, int),
                          int                   adjust_width,
                          gx_downscale_cm_fn   *apply_cm,
                          void                 *apply_cm_arg,
                          int                   post_cm_num_comps);

int gx_downscaler_init_cm_halftone(gx_downscaler_t      *ds,
                                   gx_device            *dev,
                                   int                   src_bpc,
                                   int                   dst_bpc,
                                   int                   num_comps,
                             const gx_downscaler_params *params,
                                   int                 (*adjust_width_proc)(int, int),
                                   int                   adjust_width,
                                   gx_downscale_cm_fn   *apply_cm,
                                   void                 *apply_cm_arg,
                                   int                   post_cm_num_comps,
                                   gx_downscaler_ht_t   *ht);

int gx_downscaler_init_planar(gx_downscaler_t      *ds,
                              gx_device            *dev,
                              int                   src_bpc,
                              int                   dst_bpc,
                              int                   num_comps,
                        const gx_downscaler_params *params,
                        const gs_get_bits_params_t *gb_params);

int gx_downscaler_init_planar_cm(gx_downscaler_t      *ds,
                                 gx_device            *dev,
                                 int                   src_bpc,
                                 int                   dst_bpc,
                                 int                   num_comps,
                           const gx_downscaler_params *params,
                           const gs_get_bits_params_t *gb_params,
                                 gx_downscale_cm_fn   *apply_cm,
                                 void                 *apply_cm_arg,
                                 int                   post_cm_num_comps);

int gx_downscaler_getbits(gx_downscaler_t *ds,
                          byte            *out_data,
                          int              row);

int gx_downscaler_get_bits_rectangle(gx_downscaler_t      *ds,
                                     gs_get_bits_params_t *params,
                                     int                   row);

/* Must only fin a device that has been inited (though you can safely
 * fin several times) */
void gx_downscaler_fin(gx_downscaler_t *ds);

void gx_downscaler_decode_factor(int factor, int *up, int *down);

int
gx_downscaler_scale(int width, int factor);

int
gx_downscaler_scale_rounded(int width, int factor);

int gx_downscaler_adjust_bandheight(int factor, int band_height);

/* Downscaling call to the process_page (downscaler also works on the
 * rendering threads and chains calls through to supplied options functions).
 */
int gx_downscaler_process_page(gx_device                 *dev,
                               gx_process_page_options_t *options,
                               int                        factor);

#define GX_DOWNSCALER_PARAMS_DEFAULTS \
{ 1, 0, 0, 0, { 3, 1, 0, 2 }, 0, 0 }

enum {
    GX_DOWNSCALER_PARAMS_MFS = 1,
    GX_DOWNSCALER_PARAMS_TRAP = 2,
    GX_DOWNSCALER_PARAMS_ETS = 4
};

int gx_downscaler_read_params(gs_param_list        *plist,
                              gx_downscaler_params *params,
                              int                   features);

int gx_downscaler_write_params(gs_param_list        *plist,
                               gx_downscaler_params *params,
                               int                   features);

/* A helper function for creating the post render ICC link.
 * This maps from the device space to the space given by the
 * post render profile entry in the device icc struct.
 * This should be destroyed using gsicc_free_link_dev.*/
int gx_downscaler_create_post_render_link(gx_device     *dev,
                                          gsicc_link_t **link);

/* A helper function for creating an ICC link. This maps
 * from the device space to the space given by the
 * supplied ICC profile.
 * This should be destroyed using gsicc_free_link_dev.*/
int
gx_downscaler_create_icc_link(gx_device      *dev,
                              gsicc_link_t  **link,
                              cmm_profile_t  *profile);

#endif
