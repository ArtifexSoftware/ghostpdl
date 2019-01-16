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


/* pcursor.h - interface to the PCL cursor positioning code */

#ifndef pcursor_INCLUDED
#define pcursor_INCLUDED

#include "gx.h"
#include "pcstate.h"
#include "pcommand.h"

/*
 * Default values for HMI and VMI. The use of -1 for HMI indicates "not set".
 */

coord pcl_vmi_default(pcl_state_t * pcs);

#define HMI_DEFAULT -1L
#define VMI_DEFAULT pcl_vmi_default(pcs)

/*
 * Horizontal and vertical cursor movement routines. x and y are in
 * centipoints.
 *
 * NB: absolute vertical positions passed to pcl_set_cap_y are in full
 *     page direction space, not the "pseudo" page directions space in
 *     which the pcs->cap is maintained. If passing coordinates in the
 *     latter space, BE SURE TO SUBTRACT THE CURRENT TOP MARGIN.
 */
void pcl_set_cap_x(pcl_state_t * pcs, coord x,  /* position or distance */
                   bool relative,       /* x is distance (else position) */
                   bool use_margins     /* apply text margins */
    );

int pcl_set_cap_y(pcl_state_t * pcs, coord y,   /* position or distance */
                  bool relative,        /* y is distance (else position) */
                  bool use_margins,     /* apply text margins */
                  bool by_row,  /* LF, half LF */
                  bool by_row_command   /* ESC & a <rows> R special case. */
    );

void pcl_do_CR(pcl_state_t * pcs);

int pcl_do_FF(pcl_state_t * pcs);

int pcl_do_LF(pcl_state_t * pcs);

void pcl_home_cursor(pcl_state_t * pcs);

/* Get the HMI.  This may require recomputing it from the font. */
int pcl_updated_hmi(pcl_state_t * pcs);

int pcl_update_hmi_cp(pcl_state_t * pcs);

extern const pcl_init_t pcursor_init;

#endif /* pcursor_INCLUDED */
