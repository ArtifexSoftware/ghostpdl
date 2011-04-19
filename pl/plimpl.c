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

/* pcimpl.c - PCL5c & PCL/XL pl_interp_implementation_t descriptor */

#include "memory_.h"
#include "scommon.h"
#include "gxdevice.h"
#include "pltop.h"

extern pl_interp_implementation_t pcl_implementation;
extern pl_interp_implementation_t pxl_implementation;
extern pl_interp_implementation_t xps_implementation;
extern pl_interp_implementation_t svg_implementation;

#ifdef PSI_INCLUDED
extern pl_interp_implementation_t ps_implementation;
#endif

/* Zero-terminated list of pointers to implementations */
pl_interp_implementation_t const * const pdl_implementation[] = {
#ifdef XPS_INCLUDED
        &xps_implementation,
#endif
#ifdef SVG_INCLUDED
        &svg_implementation,
#endif
#ifdef PCL_INCLUDED
        &pcl_implementation,
        &pxl_implementation,
#endif
#ifdef PSI_INCLUDED
        &ps_implementation,
#endif
        0
};
