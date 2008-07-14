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

/* rtraster.h - interface to raster rendering code */

#ifndef rtraster_INCLUDED
#define rtraster_INCLUDED

#include "pcstate.h"
#include "pcommand.h"

/*
 * Create a PCL raster object (on entering raster graphics mode).
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_start_raster(
    uint                src_width,
    uint                src_height,
    pcl_state_t *       pcs
);

/* complete a raster (when exiting raster graphics mode) */
void pcl_complete_raster(pcl_state_t *pcs);

extern  const pcl_init_t    rtraster_init;

#endif			/* rtraster_INCLUDED */
