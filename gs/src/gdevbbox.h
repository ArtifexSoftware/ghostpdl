/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*Id: gdevbbox.h */
/* Interface to bounding box device */
/* Requires gxdevice.h */

/*
 * This device keeps track of the per-page bounding box, and also optionally
 * forwards all drawing commands to a target.  It can be used either as a
 * free-standing device or as a component (e.g., by the EPS writer).
 *
 * One way to use a bounding box device is simply to include bbox.dev in the
 * value of DEVICE_DEVSn in the makefile.  This produces a free-standing
 * device named 'bbox' that can be selected in the usual way (-sDEVICE=bbox)
 * and that prints out the bounding box at each showpage or copypage without
 * doing any drawing.
 *
 * The other way to use a bounding box device is from C code as a component
 * in a device pipeline.  To set up a bounding box device that doesn't do
 * any drawing:
 *	gx_device_bbox *bdev =
 *	  gs_alloc_struct_immovable(some_memory,
 *				    gx_device_bbox, &st_device_bbox,
 *				    "some identifying string for debugging");
 *	gx_device_bbox_init(bdev, NULL);
 * Non-drawing bounding box devices have an "infinite" page size.
 *
 * To set up a bounding box device that draws to another device tdev:
 *	gx_device_bbox *bdev =
 *	  gs_alloc_struct_immovable(some_memory,
 *				    gx_device_bbox, &st_device_bbox,
 *				    "some identifying string for debugging");
 *	gx_device_bbox_init(bdev, tdev);
 * Bounding box devices that draw to a real device appear to have the
 * same page size as that device.
 *
 * To intercept the end-of-page to call a routine eop of your own, after
 * setting up the device:
 *	dev_proc_output_page(eop);	-- declare a prototype for eop
 *	...
 *	set_dev_proc(bdev, output_page, eop);
 *	...
 *	int eop(gx_device *dev, int num_copies, int flush)
 *	{	gs_rect bbox;
 *		gx_device_bbox_bbox((gx_device_bbox *)dev, &bbox);
 *		<< do whatever you want >>
 *		return gx_forward_output_page(dev, num_copies, flush);
 *	}
 */
#define gx_device_bbox_common\
	gx_device_forward_common;\
	bool free_standing;\
	/* In order to handle compositors, we provide a separate pointer */\
	/* to the bbox device instance that holds the actual box. */\
	gx_device_bbox *box_device;\
	/* The following are updated dynamically. */\
	gs_fixed_rect bbox;\
	gx_color_index white
typedef struct gx_device_bbox_s gx_device_bbox;
struct gx_device_bbox_s {
	gx_device_bbox_common;
};
extern_st(st_device_bbox);
#define public_st_device_bbox()	/* in gdevbbox.c */\
  gs_public_st_suffix_add1_final(st_device_bbox, gx_device_bbox,\
    "gx_device_bbox", device_bbox_enum_ptrs, device_bbox_reloc_ptrs,\
    gx_device_finalize, st_device_forward, box_device)

/* Initialize a bounding box device. */
void gx_device_bbox_init(P2(gx_device_bbox *dev, gx_device *target));

/* Read back the bounding box in 1/72" units. */
void gx_device_bbox_bbox(P2(gx_device_bbox *dev, gs_rect *pbbox));
