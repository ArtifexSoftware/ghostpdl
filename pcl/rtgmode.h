/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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
int     pcl_enter_graphics_mode(P2(
    pcl_state_t *       pcs,
    pcl_gmode_entry_t   mode
));

/* exit raster graphics mode */
int pcl_end_graphics_mode(P1(pcl_state_t * pcs));

extern  const pcl_init_t    rtgmode_init;
extern  const pcl_init_t    rtlrastr_init;

#endif			/* rtgmode_INCLUDED */
