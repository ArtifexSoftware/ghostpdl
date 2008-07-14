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

/* rtgmode.h - interface to PCL graphics (raster) mode */

#ifndef rtgmode_INCLUDED
#define rtgmode_INCLUDED

#include "rtrstst.h"
#include "pcstate.h"
#include "pcommand.h"

/*
 * Types of entry into graphics mode. Note that implicit entry is distinct
 * from any of the explicit modes.
 */
typedef enum {
    NO_SCALE_LEFT_MARG = 0,
    NO_SCALE_CUR_PT = 1,
    SCALE_LEFT_MARG = 2,
    SCALE_CUR_PTR = 3,
    IMPLICIT = 100
} pcl_gmode_entry_t;


/* enter raster graphics mode */
int     pcl_enter_graphics_mode(
    pcl_state_t *       pcs,
    pcl_gmode_entry_t   mode
);

/* exit raster graphics mode */
int pcl_end_graphics_mode(pcl_state_t * pcs);

extern  const pcl_init_t    rtgmode_init;
extern  const pcl_init_t    rtlrastr_init;

#endif			/* rtgmode_INCLUDED */
