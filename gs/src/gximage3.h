/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* ImageType 3 internal API */

#ifndef gximage3_INCLUDED
#  define gximage3_INCLUDED

#include "gsiparm3.h"
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
#define IMAGE3_MAKE_MID_PROC(proc)\
  int proc(P5(gx_device **pmidev, gx_device *dev, int width, int height,\
	      gs_memory_t *mem))
/*
 * Make the mask clip device -- the device that uses the mask image to
 * clip the (opaque) image data.
 * The device is closed and freed at the end of processing the image.
 */
#define IMAGE3_MAKE_MCD_PROC(proc)\
  int proc(P5(gx_device **pmcdev, gx_device *dev, gx_device *midev,\
	      const gs_int_point *origin, gs_memory_t *mem))
/*
 * Begin processing an ImageType 3 image, with the mask device creation
 * procedures as additional parameters.
 */
int gx_begin_image3_generic(P11(gx_device * dev,
				const gs_imager_state *pis,
				const gs_matrix *pmat,
				const gs_image_common_t *pic,
				const gs_int_rect *prect,
				const gx_drawing_color *pdcolor,
				const gx_clip_path *pcpath, gs_memory_t *mem,
				IMAGE3_MAKE_MID_PROC((*make_mid)),
				IMAGE3_MAKE_MCD_PROC((*make_mcd)),
				gx_image_enum_common_t **pinfo));

#endif /* gximage3_INCLUDED */
