/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcdraw.h - Interface to PCL5 drawing utilities */

#ifndef pcdraw_INCLUDED
#define pcdraw_INCLUDED

#include "pcstate.h"

/* compatibility function */
int pcl_set_ctm(P2(pcl_state_t * pcs, bool print_direction));

/* set CTM and clip rectangle for drawing PCL object */
int pcl_set_graphics_state(P1(pcl_state_t * pcs));

/* set the current drawing color */
int pcl_set_drawing_color(P4(
    pcl_state_t *           pcs,
    pcl_pattern_source_t    type,
    int                     pcl_id,
    bool                    for_image
));

#endif  	/* pcdraw_INCLUDED */
