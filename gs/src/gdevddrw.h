/* Copyright (C) 2002 artofcode LLC. All rights reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/* $Id$ */
/* Prototypes of some polygon and image drawing procedures */

#ifndef gdevddrw_INCLUDED
#  define gdevddrw_INCLUDED

#if DROPOUT_PREVENTION

enum fill_trap_flags {
    ftf_peak0 = 1,
    ftf_peak1 = 2,
    ftf_pseudo_rasterization = 4
};

int
gx_fill_trapezoid_narrow(gx_device * dev, const gs_fixed_edge * left,
    const gs_fixed_edge * right, fixed ybot, fixed ytop, int flags,
    const gx_device_color * pdevc, gs_logical_operation_t lop);
#endif

#endif /* gdevddrw_INCLUDED */

