/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* pcdraw.h - Interface to PCL5 drawing utilities */

#ifndef pcdraw_INCLUDED
#define pcdraw_INCLUDED

#include "pcstate.h"

/* compatibility function */
extern  int     pcl_set_ctm( pcl_state_t * pcs, bool print_direction );

/* set CTM and clip rectangle for drawing PCL object */
extern  int     pcl_set_graphics_state( pcl_state_t * pcs );

/* set the current drawing color */
extern  int     pcl_set_drawing_color(
    pcl_state_t *           pcs,
    pcl_pattern_source_t    type,
    int                     pcl_id,
    bool                    for_image
);

#endif  	/* pcdraw_INCLUDED */
