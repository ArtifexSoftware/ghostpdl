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
/*$Id: */

/* metgstate.h */

/* a state that "tags" along with the graphics state */

#ifndef metgstate_INCLUDED
#  define metgstate_INCLUDED

#include "gstypes.h"
#include "gsstate.h"
#include "metsimple.h"

/* constructor */
void met_gstate_init(gs_state *pgs, gs_memory_t *mem);

/* operators */

ST_RscRefColor met_currentstrokecolor(gs_state *pgs);
void met_setstrokecolor(gs_state *pgs, const ST_RscRefColor color);

ST_RscRefColor met_currentfillcolor(gs_state *pgs);
void met_setfillcolor(gs_state *pgs, const ST_RscRefColor color);

ST_ZeroOne met_currentopacity(gs_state *pgs);
void met_setopacity(gs_state *pgs, const ST_ZeroOne opacity);

ST_FillRule met_currentfillrule(gs_state *pgs);
void met_setfillrule(gs_state *pgs, const ST_FillRule fill);

/* true if even-odd */
bool met_currenteofill(gs_state *pgs);

typedef enum {
    met_stroke_only, 
    met_fill_only, 
    met_stroke_and_fill,
    met_path_undefined
} met_path_t;

met_path_t met_currentpathtype(gs_state *pgs);

/* if set all paths should be closed */
void met_setclosepath(gs_state *pgs, bool close);
bool met_currentclosepath(gs_state *pgs);

#endif /* metgstate_INCLUDED */
