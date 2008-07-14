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

/* pcdraw.h - Interface to PCL5 drawing utilities */

#ifndef pcdraw_INCLUDED
#define pcdraw_INCLUDED

#include "pcstate.h"

/* compatibility function */
int pcl_set_ctm(pcl_state_t * pcs, bool print_direction);

/* set CTM and clip rectangle for drawing PCL object */
int pcl_set_graphics_state(pcl_state_t * pcs);

/* set the current drawing color */
int pcl_set_drawing_color(
    pcl_state_t *           pcs,
    pcl_pattern_source_t    type,
    int                     pcl_id,
    bool                    for_image
);

#endif  	/* pcdraw_INCLUDED */
