/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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
