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

/* pcursor.h - interface to the PCL cursor positioning code */

#ifndef pcursor_INCLUDED
#define pcursor_INCLUDED

#include "gx.h"
#include "pcstate.h"
#include "pcommand.h"

/*
 * Default values for HMI and VMI. The use of -1 for HMI indicates "not set".
 */
#define HMI_DEFAULT -1L
#define VMI_DEFAULT inch2coord(8.0 / 48)

/*
 * Horizontal and vertical cursor movement routines. x and y are in
 * centipoints.
 *
 * NB: absolute vertical positions passed to pcl_set_cap_y are in full
 *     page direction space, not the "pseudo" page directions space in
 *     which the pcs->cap is maintained. If passing coordinates in the
 *     latter space, BE SURE TO SUBTRACT THE CURRENT TOP MARGIN.
 */
void pcl_set_cap_x(
    pcl_state_t *   pcs,
    coord           x,              /* position or distance */
    bool            relative,       /* x is distance (else position) */
    bool            use_margins     /* apply text margins */
);

int pcl_set_cap_y(
    pcl_state_t *   pcs,
    coord           y,                  /* position or distance */
    bool            relative,           /* y is distance (else position) */
    bool            use_margins,        /* apply text margins */
    bool            by_row,             /* LF, half LF */
    bool            by_row_command      /* ESC & a <rows> R special case. */
);

void    pcl_do_CR(pcl_state_t * pcs);
int     pcl_do_FF(pcl_state_t * pcs);
int     pcl_do_LF(pcl_state_t * pcs);
void    pcl_home_cursor(pcl_state_t * pcs);

/* Get the HMI.  This may require recomputing it from the font. */
coord   pcl_updated_hmi(pcl_state_t * pcs);

#define pcl_hmi(pcs)                                                    \
    ((pcs)->hmi_cp == HMI_DEFAULT ? pcl_updated_hmi(pcs) : (pcs)->hmi_cp)

extern  const pcl_init_t    pcursor_init;

#endif			/* pcursor_INCLUDED */
