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
/* Abstract types for compositing objects */

#ifndef gscompt_INCLUDED
#  define gscompt_INCLUDED

/*
 * Compositing is the next-to-last step in the rendering pipeline.
 * It occurs after color correction but before halftoning (if needed).
 *
 * gs_composite_t is the abstract superclass for compositing functions such
 * as RasterOp functions or alpha-based compositing.  Concrete subclasses
 * must provide a default implementation (presumably based on
 * get_bits_rectangle and copy_color) for devices that provide no optimized
 * implementation of their own.
 *
 * A client that wants to produce composited output asks the target device
 * to create an appropriate compositing device based on the target device
 * and the gs_composite_t (and possibly other elements of the imager state).
 * If the target device doesn't have its own implementation for the
 * requested function, format, and state, it passes the buck to the
 * gs_composite_t, which may make further tests for special cases before
 * creating and returning a compositing device that uses the default
 * implementation.
 */
typedef struct gs_composite_s gs_composite_t;

/*
 * To enable fast cache lookup and equality testing, compositing functions,
 * like halftones, black generation functions, etc., carry a unique ID (time
 * stamp).
 */
gs_id gs_composite_id(const gs_composite_t * pcte);

#endif /* gscompt_INCLUDED */
