/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* ImageType 3x internal API */

#ifndef gximag3x_INCLUDED
#  define gximag3x_INCLUDED

#include "gsipar3x.h"
#include "gxiparam.h"

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
#define IMAGE3X_MAKE_MID_PROC(proc)\
  int proc(P6(gx_device **pmidev, gx_device *dev, int width, int height,\
	      int depth, gs_memory_t *mem))
typedef IMAGE3X_MAKE_MID_PROC((*image3x_make_mid_proc_t));

/*
 * Make the mask clip device -- the device that uses the mask image to
 * clip the (opaque) image data -- and its enumerator.
 * The device is closed and freed at the end of processing the image.
 */
#define IMAGE3X_MAKE_MCDE_PROC(proc)\
  int proc(P14(/* The initial arguments are those of begin_typed_image. */\
	       gx_device *dev,\
	       const gs_imager_state *pis,\
	       const gs_matrix *pmat,\
	       const gs_image_common_t *pic,\
	       const gs_int_rect *prect,\
	       const gx_drawing_color *pdcolor,\
	       const gx_clip_path *pcpath, gs_memory_t *mem,\
	       gx_image_enum_common_t **pinfo,\
	       /* The following arguments are added. */\
	       gx_device **pmcdev, gx_device *midev[2],\
	       gx_image_enum_common_t *pminfo[2],\
	       const gs_int_point origin[2],\
	       const gs_image3x_t *pim))
typedef IMAGE3X_MAKE_MCDE_PROC((*image3x_make_mcde_proc_t));

/*
 * Begin processing an ImageType 3x image, with the mask device creation
 * procedures as additional parameters.
 */
int gx_begin_image3x_generic(P11(gx_device * dev,
				 const gs_imager_state *pis,
				 const gs_matrix *pmat,
				 const gs_image_common_t *pic,
				 const gs_int_rect *prect,
				 const gx_drawing_color *pdcolor,
				 const gx_clip_path *pcpath, gs_memory_t *mem,
				 IMAGE3X_MAKE_MID_PROC((*make_mid)),
				 IMAGE3X_MAKE_MCDE_PROC((*make_mcde)),
				 gx_image_enum_common_t **pinfo));

#endif /* gximag3x_INCLUDED */
