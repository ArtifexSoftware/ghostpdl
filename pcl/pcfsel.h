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

/* pcfsel.h */
/* Interface to PCL5 / HP-GL/2 font selection */

#ifndef pcfsel_INCLUDED
#  define pcfsel_INCLUDED

#include "pcstate.h"


/* Recompute the font from the parameters if necessary. */
/* This is used by both PCL and HP-GL/2. */
int pcl_reselect_font(pcl_font_selection_t *pfs, const pcl_state_t *pcs, bool intenal_only);

/*
 * Select a font by ID, updating the selection parameters.  Return 0
 * normally, 1 if no font was found, or an error code.  The pcl_state_t is
 * used only for the font and symbol set dictionaries.
 */
int pcl_select_font_by_id(pcl_font_selection_t *pfs, uint id,
                          pcl_state_t *pcs);

/* set font parameters after an id selection */
void
pcl_set_id_parameters(const pcl_state_t *pcs, 
		      pcl_font_selection_t *pfs, pl_font_t *fp, uint id);


#endif				/* pcfsel_INCLUDED */
