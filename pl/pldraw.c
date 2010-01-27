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
/*$Id$ */

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

	if ( pim->ImageMask | pim->CombineWithColor )
	  gx_set_dev_color(pgs);
	return (*dev_proc(dev, begin_image))
	    (dev, (const gs_imager_state *)pgs, pim,
	     gs_image_format_chunky, (const gs_int_rect *)0,
	     gs_currentdevicecolor_inline(pgs), pgs->clip_path, pgs->memory,
	     (gx_image_enum_common_t **)pinfo);
}
