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


/* pcfsel.h */
/* Interface to PCL5 / HP-GL/2 font selection */

#ifndef pcfsel_INCLUDED
#  define pcfsel_INCLUDED

#include "pcstate.h"

/* Recompute the font from the parameters if necessary. */
/* This is used by both PCL and HP-GL/2. */
int pcl_reselect_font(pcl_font_selection_t * pfs, const pcl_state_t * pcs,
                      bool intenal_only);

/*
 * Select a font by ID, updating the selection parameters.  Return 0
 * normally, 1 if no font was found, or an error code.  The pcl_state_t is
 * used only for the font and symbol set dictionaries.
 */
int pcl_select_font_by_id(pcl_font_selection_t * pfs, uint id,
                          pcl_state_t * pcs);

/* set font parameters after an id selection */
void
pcl_set_id_parameters(const pcl_state_t * pcs,
                      pcl_font_selection_t * pfs, pl_font_t * fp, uint id);

#endif /* pcfsel_INCLUDED */
