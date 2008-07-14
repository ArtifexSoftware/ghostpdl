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

/* pginit.h - Interface to initialize/reset procedures in pginit.c */

#ifndef pginit_INCLUDED
#define pginit_INCLUDED

#include "gx.h"
#include "pcstate.h"
#include "pcommand.h"
#include "pgmand.h"

/* Reset a set of font parameters to their default values. */
void hpgl_default_font_params( pcl_font_selection_t * pfs );

/* Reset all the fill patterns to solid fill. */
void hpgl_default_all_fill_patterns( hpgl_state_t * pgls );

/* Reset (parts of) the HP-GL/2 state. */
void hpgl_do_reset( pcl_state_t * pcs, pcl_reset_type_t type );

#endif  	/* pginit_INCLUDED */
