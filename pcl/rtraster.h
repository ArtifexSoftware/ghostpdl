/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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
extern  int     pcl_start_raster(
    uint                src_width,
    uint                src_height,
    pcl_state_t *       pcs
);

/* complete a raster (when exiting raster graphics mode) */
extern  void    pcl_complete_raster( void );

extern  const pcl_init_t    rtraster_init;

#endif			/* rtraster_INCLUDED */
