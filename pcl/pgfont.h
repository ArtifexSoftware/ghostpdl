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

/* pgfont.h */
/* Definitions for HP-GL/2 stick and arc fonts */

#ifndef pgfont_INCLUDED
#  define pgfont_INCLUDED

#include "pgmand.h"

/* Fill in stick/arc font boilerplate. */
void hpgl_fill_in_stick_font(gs_font_base *pfont, long unique_id);
void hpgl_fill_in_arc_font(gs_font_base *pfont, long unique_id);
void hpgl_initialize_stick_fonts(hpgl_state_t *pcs);

#endif                       /* pgfont_INCLUDED */
