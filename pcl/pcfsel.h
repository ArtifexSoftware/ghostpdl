/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcfsel.h */
/* Interface to PCL5 / HP-GL/2 font selection */

#ifndef pcfsel_INCLUDED
#  define pcfsel_INCLUDED

#include "pcstate.h"


/* Recompute the font from the parameters if necessary. */
/* This is used by both PCL and HP-GL/2. */
int pcl_reselect_font(P2(pcl_font_selection_t *pfs, const pcl_state_t *pcs));

/* fallback if a glyph is not found in a font.  This select the best
   from the parameters, however, the font is chosen only from the
   collection of fonts that actually have the desired character code */
int pcl_reselect_substitute_font(P3(pcl_font_selection_t *pfs,
				    const pcl_state_t *pcs,
				    const uint chr));
/*
 * Select a font by ID, updating the selection parameters.  Return 0
 * normally, 1 if no font was found, or an error code.  The pcl_state_t is
 * used only for the font and symbol set dictionaries.
 */
int pcl_select_font_by_id(P3(pcl_font_selection_t *pfs, uint id,
			     pcl_state_t *pcs));

/* set font parameters after an id selection */
void
pcl_set_id_parameters(P4(const pcl_state_t *pcs, 
		      pcl_font_selection_t *pfs, pl_font_t *fp, uint id));


#endif				/* pcfsel_INCLUDED */
