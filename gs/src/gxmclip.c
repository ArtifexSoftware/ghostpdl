/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.

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


/* Mask clipping support */
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxmclip.h"

/* Initialize a mask clipping device. */
int
gx_mask_clip_initialize(gx_device_mask_clip * cdev,
		  const gx_device_mask_clip * proto, const gx_bitmap * bits,
			gx_device * tdev, int tx, int ty)
{
    int buffer_width = bits->size.x;
    int buffer_height =
    tile_clip_buffer_size / (bits->raster + sizeof(byte *));

    gx_device_init((gx_device *) cdev, (const gx_device *)proto,
		   NULL, true);
    cdev->width = tdev->width;
    cdev->height = tdev->height;
    cdev->color_info = tdev->color_info;
    cdev->target = tdev;
    cdev->phase.x = -tx;
    cdev->phase.y = -ty;
    if (buffer_height > bits->size.y)
	buffer_height = bits->size.y;
    gs_make_mem_mono_device(&cdev->mdev, 0, 0);
    for (;;) {
	if (buffer_height <= 0) {	/*
					 * The tile is too wide to buffer even one scan line.
					 * We could do copy_mono in chunks, but for now, we punt.
					 */
	    cdev->mdev.base = 0;
	    return 0;
	}
	cdev->mdev.width = buffer_width;
	cdev->mdev.height = buffer_height;
	if (gdev_mem_bitmap_size(&cdev->mdev) <= tile_clip_buffer_size)
	    break;
	buffer_height--;
    }
    cdev->mdev.base = cdev->buffer.bytes;
    return (*dev_proc(&cdev->mdev, open_device)) ((gx_device *) & cdev->mdev);
}
