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
/* Definitions for planar rendering */

#ifndef gxrplane_INCLUDED
#  define gxrplane_INCLUDED

#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;
#endif

/*
 * Define the parameters for extracting a single plane from chunky pixels.
 * This structure should be considered opaque, and should only be
 * initialized with the procedure.
 */
typedef struct gx_render_plane_s {
    int depth;
    int shift;			/* bit position of l.s.b. from low end */
    int index;			/* index within multi-screen halftone */
} gx_render_plane_t;

/*
 * Initialize a rendering plane specification for a device.  Note that it is
 * up to the device to decide which bits constitute a given plane identified
 * by index.  (Currently this is done with a fixed procedure, but eventually
 * it will be made a property of the device somehow, perhaps in the
 * color_info.)
 */
int gx_render_plane_init(gx_render_plane_t *render_plane,
			 const gx_device *dev, int index);

#endif /* gxrplane_INCLUDED */
