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
 *     which the pcl_cap is maintained. If passing coordinates in the
 *     latter space, BE SURE TO SUBTRACT THE CURRENT TOP MARGIN.
 */
extern  void    pcl_set_cap_x(
    pcl_state_t *   pcs,
    coord           x,              /* position or distance */
    bool            relative,       /* x is distance (else position) */
    bool            use_margins     /* apply text margins */
);

extern  int     pcl_set_cap_y(
    pcl_state_t *   pcs,
    coord           y,                  /* position or distance */
    bool            relative,           /* y is distance (else position) */
    bool            use_margins,        /* apply text margins */
    bool            by_row              /* LF, half LF, or by row */
);

extern  void    pcl_do_CR( pcl_state_t * pcs );
extern  int     pcl_do_LF( pcl_state_t * pcs );
extern  void    pcl_home_cursor( pcl_state_t * pcs );

/* Get the HMI.  This may require recomputing it from the font. */
extern  coord   pcl_updated_hmi( pcl_state_t * pcs );

#define pcl_hmi(pcs)                                                    \
    ((pcs)->hmi_cp == HMI_DEFAULT ? pcl_updated_hmi(pcs) : (pcs)->hmi_cp)

extern  const pcl_init_t    pcursor_init;

#endif			/* pcursor_INCLUDED */
