/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* State and interface definitions for clipping path accumulator */
/* Requires gxdevice.h, gzcpath.h */

#ifndef gzacpath_INCLUDED
#  define gzacpath_INCLUDED

/*
 * Device for accumulating a rectangle list.  This device can clip
 * the list being accumulated with a clipping rectangle on the fly:
 * we use this to clip clipping paths to band boundaries when
 * rendering a band list.
 */
typedef struct gx_device_cpath_accum_s {
    gx_device_common;
    gs_memory_t *list_memory;
    gs_int_rect clip_box;
    gs_int_rect bbox;
    gx_clip_list list;
} gx_device_cpath_accum;

/* Start accumulating a clipping path. */
void gx_cpath_accum_begin(P2(gx_device_cpath_accum * padev, gs_memory_t * mem));

/* Set the accumulator's clipping box. */
void gx_cpath_accum_set_cbox(P2(gx_device_cpath_accum * padev,
				const gs_fixed_rect * pbox));

/* Finish accumulating a clipping path. */
/* Note that this releases the old contents of the clipping path. */
int gx_cpath_accum_end(P2(const gx_device_cpath_accum * padev,
			  gx_clip_path * pcpath));

/* Discard an accumulator in case of error. */
void gx_cpath_accum_discard(P1(gx_device_cpath_accum * padev));

#endif /* gzacpath_INCLUDED */
