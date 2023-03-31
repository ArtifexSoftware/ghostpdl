/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


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
int pcl_start_raster(uint src_width, uint src_height, pcl_state_t * pcs);

/* complete a raster (when exiting raster graphics mode) */
void pcl_complete_raster(pcl_state_t * pcs);

extern const pcl_init_t rtraster_init;

#endif /* rtraster_INCLUDED */
