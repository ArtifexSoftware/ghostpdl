/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pldraw.c */
/* Common drawing routines for PCL5 and PCL XL */
#include "std.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxdevice.h"
#include "gzstate.h"
#include "pldraw.h"

/* Begin an image with parameters derived from a graphics state. */
int
pl_begin_image(gs_state *pgs, const gs_image_t *pim,
  void **pinfo)
{	gx_device *dev = pgs->device;

	return (*dev_proc(dev, begin_image))
	    (dev, (const gs_imager_state *)pgs, pim,
	     gs_image_format_chunky, NULL,
	     pgs->dev_color, pgs->clip_path, pgs->memory, pinfo);
}
