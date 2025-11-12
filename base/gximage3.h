/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* ImageType 3 internal API */

#ifndef gximage3_INCLUDED
#  define gximage3_INCLUDED

#include "gsiparm3.h"
#include "gxiparam.h"

/*
 * We implement ImageType 3 images by interposing a mask clipper in
 * front of an ordinary ImageType 1 image.  Note that we build up the
 * mask row-by-row as we are processing the image.
 *
 * We export a generalized form of the begin_image procedure for use by
 * the PDF and PostScript writers.
 */
typedef struct gx_image3_enum_s {
    gx_image_enum_common;
    gx_device *mdev;		/* gx_device_memory in default impl. */
    gx_device *pcdev;		/* gx_device_mask_clip in default impl. */
    gx_image_enum_common_t *mask_info;
    gx_image_enum_common_t *pixel_info;
    gs_image3_interleave_type_t InterleaveType;
    int num_components;		/* (not counting mask) */
    int bpc;			/* BitsPerComponent */
    int mask_width, mask_height, mask_full_height;
    int pixel_width, pixel_height, pixel_full_height;
    byte *mask_data;		/* (if chunky) */
    byte *pixel_data;		/* (if chunky) */
    /* The following are the only members that change dynamically. */
    int mask_y;
    int pixel_y;
    int mask_skip;		/* # of mask rows to skip, see below */
} gx_image3_enum_t;

/*
 * The machinery for splitting an ImageType3 image into pixel and mask data
 * is used both for imaging per se and for writing high-level output.
 * We implement this by making the procedures for setting up the mask image
 * and clipping devices virtual.
 */

/*
 * Make the mask image device -- the device that processes mask bits.
 * The device is closed and freed at the end of processing the image.
 */
#define IMAGE3_MAKE_MID_PROC(proc)\
  int proc(gx_device **pmidev, gx_device *dev, int width, int height,\
           gs_memory_t *mem)
typedef IMAGE3_MAKE_MID_PROC((*image3_make_mid_proc_t));

/*
 * Make the mask clip device -- the device that uses the mask image to
 * clip the (opaque) image data -- and its enumerator.
 * The device is closed and freed at the end of processing the image.
 */
#define IMAGE3_MAKE_MCDE_PROC(proc)\
  int proc(/* The initial arguments are those of begin_typed_image. */\
               gx_device *dev,\
           const gs_gstate *pgs,\
           const gs_matrix *pmat,\
           const gs_image_common_t *pic,\
           const gs_int_rect *prect,\
           const gx_drawing_color *pdcolor,\
           const gx_clip_path *pcpath, gs_memory_t *mem,\
           gx_image_enum_common_t **pinfo,\
           /* The following arguments are added. */\
           gx_device **pmcdev, gx_device *midev,\
           gx_image_enum_common_t *pminfo,\
           const gs_int_point *origin)
typedef IMAGE3_MAKE_MCDE_PROC((*image3_make_mcde_proc_t));

/*
 * Begin processing an ImageType 3 image, with the mask device creation
 * procedures as additional parameters.
 */
int gx_begin_image3_generic(gx_device * dev,
                            const gs_gstate *pgs,
                            const gs_matrix *pmat,
                            const gs_image_common_t *pic,
                            const gs_int_rect *prect,
                            const gx_drawing_color *pdcolor,
                            const gx_clip_path *pcpath, gs_memory_t *mem,
                            IMAGE3_MAKE_MID_PROC((*make_mid)),
                            IMAGE3_MAKE_MCDE_PROC((*make_mcde)),
                            gx_image_enum_common_t **pinfo);

#endif /* gximage3_INCLUDED */
