/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


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
pl_interp_implementation_t const *const pdl_implementation[] = {
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
