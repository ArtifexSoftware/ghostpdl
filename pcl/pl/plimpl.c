/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* pcimpl.c - PCL5c & PCL/XL pl_interp_implementation_t descriptor */

#include "memory_.h"
#include "scommon.h"
#include "gxdevice.h"
#include "pltop.h"

#include "gsmemory.h" /* for gs_ptr_procs_t */

extern pl_interp_implementation_t pjl_implementation;

extern pl_interp_implementation_t pcl_implementation;

extern pl_interp_implementation_t pxl_implementation;

extern pl_interp_implementation_t xps_implementation;

extern pl_interp_implementation_t svg_implementation;

extern pl_interp_implementation_t ps_implementation;

extern pl_interp_implementation_t urf_implementation;

extern pl_interp_implementation_t img_implementation;

/* Zero-terminated list of pointers to implementations */
pl_interp_implementation_t *pdl_implementations[] = {
    &pjl_implementation,
#ifdef PCL_INCLUDED
    &pcl_implementation,
    &pxl_implementation,
#endif
#ifdef XPS_INCLUDED
    &xps_implementation,
#endif
#ifdef PSI_INCLUDED
    &ps_implementation,
#endif
#ifdef URF_INCLUDED
    &urf_implementation,
#endif
#ifdef IMG_INCLUDED
    &img_implementation,
#endif
    0
};

/* ---------------- Stubs ---------------- */

/* These are here for easier access to PSI_INCLUDED */
#ifndef PSI_INCLUDED
/* Stubs for GC */
const gs_ptr_procs_t ptr_struct_procs = { NULL, NULL, NULL };
const gs_ptr_procs_t ptr_string_procs = { NULL, NULL, NULL };
const gs_ptr_procs_t ptr_const_string_procs = { NULL, NULL, NULL };
const gs_ptr_procs_t ptr_name_index_procs = { NULL, NULL, NULL };
#endif
