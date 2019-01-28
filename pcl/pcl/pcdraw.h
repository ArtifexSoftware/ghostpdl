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


/* pcdraw.h - Interface to PCL5 drawing utilities */

#ifndef pcdraw_INCLUDED
#define pcdraw_INCLUDED

#include "pcstate.h"

/* compatibility function */
int pcl_set_ctm(pcl_state_t * pcs, bool print_direction);

/* set CTM and clip rectangle for drawing PCL object */
int pcl_set_graphics_state(pcl_state_t * pcs);

/* set the current drawing color */
int pcl_set_drawing_color(pcl_state_t * pcs,
                          pcl_pattern_source_t type,
                          int pcl_id, bool for_image);

#endif /* pcdraw_INCLUDED */
